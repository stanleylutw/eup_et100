# 23 — v0.9.2 GATT 取消連線後釋放（D9）

日期：2026-10-23。分支 `v0.9.2_gatt_stop_release`，base `v0.9.1_relay_handoff`（29d30b8）。

**Codex 實作／host／ARM build 完成；Claude Step 5 review PASS（1 Minor：quiet 期內重試會消耗一次 attempt）；EVB 驗收由 Stanley 執行。未 commit／merge／push，本輪未開 UART／SWD、未存取或燒錄 EVB。** 原 v0.9.1 的 M0 是先前另行授權的唯讀取證，不是本版硬體驗收。

## 1. M0 與本輪決策

先前 `/tmp/et100_m0_readonly_20261023.json`：板上 App 與 v0.9.1 映像完整相符，CPU 取證前後 RUNNING；連續五次讀到 DRAIN／initiating=1／connected=0／connection_end_seen=1／connection_end_status=0／stop rc=0／reserved=1／unreserve_pending=1。這確認實際卡住的旗標組合，但單靠快照不能識別是哪次 stop 或原始 connect action 產生該 END。

本輪依施工單 D9a–D9d，新增「已接受取消」的證據，再處理零狀態完成事件。Stanley 另核准：

1. quiet 期除 exclusive／reserve／auto 外，也阻擋手動 STYPESWITCH 啟動，避免旁路開始新 session。
2. 驗收 7 的 `e=1,st=0,sr=1` 是診斷格式 fixture；正常 M0 路徑應在 6 秒診斷前釋放，不要求不可能同時成立的正常狀態。

## 2. 實作

- `gap_conn_stop()` 回 NO_ERR 才設定 `stop_requested`；收到 END 後不論 status 清 initiating／stop_requested。未接受 stop 的零狀態 END 仍保留 initiating，允許後續正常 MASTER_CONNECT。
- `connection_end_sequence` 區分 SDK call 前的舊事件與 call 期間的新事件。若 SDK 在回傳值可見前就同步送出 END，回 NO_ERR 後仍完成釋放；不因尚未設定 stop_requested 而漏掉。SDK call 不放入 critical section，避免等待 callback 時關閉中斷。
- 若無 END 或 MASTER_CONNECT，從**第一次成功 stop** 起 2 秒後備清除；每秒重試不延長該期限。stop 回錯誤不啟動後備。MASTER_CONNECT 會停止後備並沿 DRAIN 斷線，不能把已建立的 link 當不存在。
- accepted stop 對應的 owner 真正釋放時開始 2 秒 quiet；exclusive／reserve／auto／手動入口都檢查，原 5 秒 auto cooldown 仍保留。quiet 中的 MASTER_CONNECT 沿原 IDLE→DRAIN 路徑斷線，不交給新 session。quiet／後備計時都測試 uint32_t 回繞。
- CF reason 保留 `disconnect_drain_timeout` 前綴，追加 `e`／`st`／`sr`。只增加文字尾端，沒有新 state／error／欄位結構；event／reason 本地 buffer 增加，容納診斷內容，不改 UART queue 大小。
- 15 秒清理上限、背景 worker、延後 unreserve、NOR handoff 與 HTML 全部不改。D11 自動 BLE／整機 reset **未實作**。

2 秒 quiet 是核准的有界防護，不宣稱可保證任意長延遲的 SDK 事件都不再到達；更長延遲與實際 SDK 語意仍由 Stanley 留實機 log 驗證。

## 3. 測試證據

### Production GATT＋fake SDK

`fw/tests/test_gatt_client.c` 直接包含 production `gatt_client.c`，SDK API 型別仍用原 SDK header。

| 案例 | 結果 |
|---|---|
| M0：連線 timeout，stop 回 0，**回傳後**於下一次排程送 END(status=0)，不送 MASTER_CONNECT | PASS；fake 送事件前斷言 stop_requested／initiating 已設，之後清除、回 IDLE；stop 只呼叫一次，0 個 connect，未產生持續 drain timeout |
| stop 回 0，回傳後 END(status≠0) | PASS；正常釋放並 quiet |
| 未呼叫 stop 的 END(status=0)，隨後 MASTER_CONNECT | PASS；先保留 initiating，再正常連線、驗證、執行 exclusive run |
| stop 成功但完全沒有 END／link | PASS；2 秒後備釋放，多次 stop 不重設期限 |
| stop 回錯誤、無完成事件 | PASS；保留 v0.9.1 的 15 秒 DRAIN／reservation 保護，不強制放行 |
| quiet 內四種入口 | PASS；exclusive／reserve／auto／手動皆拒絕；auto fixture 暫停舊 cooldown 以隔離 quiet 條件；1999 ms 拒絕、2000 ms 可開始 |
| quiet 內遲到 MASTER_CONNECT | PASS；DRAIN 斷線、未驗證，未交給下一個 session |
| SDK call 內同步 END(status=0)，之後才 return NO_ERR | PASS；不漏完成、不留下 stop_requested |
| 診斷 fixture | PASS；前綴及 `e=1,st=0,sr=1` 皆存在；fixture 人工安排另一個未釋放條件，不宣稱正常 M0 會拖到 6 秒 |
| 原 scan pending／output queue backpressure／晚到事件／reservation／cancel，以及計時回繞 | PASS |

### Job 層

`fw/tests/test_sensor_ota.c` 執行 production Sensor job，GATT 邊界維持原有 fixture，新增 stop 回 0 後 END(status=0)、無 link 的模型。確認 FAILED／CONNECT_FAILED 或取消時 CANCELLED，gatt IDLE、reservation／pending unreserve 清除；quiet 內新 SENSOROTA 回 BUSY，期滿後新工作能開始並成功。

這是**兩層分開的 host 測試**：production GATT 的旗標／SDK 事件由上表驗證；job 層使用 GATT fake 驗證終止／重啟契約，不冒充完整真實 SDK end-to-end 或 EVB 測試。

注意既有 job backoff 為 1200 ms，短於 quiet 2000 ms，內部重試可能得到既有 BUSY 並消耗一次 attempt；本輪不修改 production job／重試策略。fixture 覆蓋此情況後仍以 CONNECT_FAILED 結束，並可在 quiet 後重啟，供 reviewer 核對。

### 回歸命令

| 命令 | 結果 |
|---|---|
| `bash fw/tools/test_evb_serial_host.sh` | PASS；全部原 serial／GATT／Sensor／SYS／SMP 與本輪新增案例；ASan／UBSan |
| `bash fw/tests/test_relay_handoff.sh` | PASS；v0.9.1 worker handoff 不變 |
| `bash fw/tools/test_sensorflow_nano.sh` | PASS；真 ARM newlib-nano formatter／fake Sensor job suite，含本輪 job fixture；Unicorn JIT 在沙箱外執行，未碰 EVB |
| `tools/tests/*.cjs`，逐一以 Node 執行 | ALL CJS PASS；Sensor、SMP／INST／180 秒、ET100 UI／transfer、relay handoff UI、SYS codec／UI；Chrome 只用 mock Serial |
| `python3 fw/tests/test_dfu_package.py` | PASS；版本 0.9.2-dev、ZIP／manifest／metadata CRC |
| `bash fw/tools/build_evb_serial.sh` | PASS；App／DFU，僅既有 RWX linker warning |
| `git diff --check` | PASS |

Logs：`/tmp/et100_v0.9.2_host.log`、`/tmp/et100_v0.9.2_build.log`、`/tmp/et100_v0.9.2_cjs.log`。不修改 HTML、SDK、Installer、OTA worker 或協定文件；06／21／平台／狀態文件留待 Claude Step 7。

## 4. 交付指紋

| 路徑 | bytes | SHA-256 |
|---|---:|---|
| `fw/build_serial/output/dfu_application.zip` | 199300 | `9158c093084e844ad0ff35c6f63863c3c51c0d129082da31d1f3a5ef020c6098` |
| `fw/build_serial/output/et100.signed.bin` | 321056 | `72a4129d2fcf57c547d34e50eb4fab75d04094f3262f5968f7f8ef77e4157652` |

ELF：`fw/build_serial/Objects/Project.elf`；text／data／bss = **284688／35808／85624** bytes，與 v0.9.1 相比新增 352 text／24 bss bytes。版本 0.9.2-dev；只需 App 更新，Installer 不需重燒。

仍為 hash-only 開發原型，不能當正式簽章安全性。metadata 的原型 git_branch／build_id 等歷史固定欄位不在 scope，請以 firmware version／指紋判斷。ZIP 重打包會改時間戳與指紋。

## 5. Stanley EVB 驗收（待執行）

1. Claude review 後，以正常 DFU 更新 0.9.2；確認完整 hash／版本／confirmed。保留目前設定與 B-info，不需重燒 Installer。
2. 在收到可連線廣播、已發起 connect 後讓 Sensor 遠離／關機，以製造 connect timeout；從一開始就關機可能只測到 WAIT_ADV_TIMEOUT。
3. 保存完整 log：stop 成功且收到 END 應迅速回 IDLE；若漏 END，約 2 秒後備釋放。工作可依原策略重試，最終應 FAILED／CONNECT_FAILED 或使用者取消時 CANCELLED，不再因本次 M0 組合永久 DRAIN。
4. 釋放後 2 秒內新啟動會 BUSY（HTML 沿用既有重試），期滿後 Sensor 回到可連線環境應能再次 OTA。quiet 內的遲到連線必須斷線、不進入新工作。
5. 若仍看到持續 drain timeout，保留新增 `e/st/sr` 及 `i/c/s/stop/disc`，先回報給 Claude／Codex；勿把不同版本 ELF 位址套到板上讀取。D11 未做，不預期自動重啟。

本輪沒有執行以上硬體步驟，所有 EVB 結果仍待 Stanley。
