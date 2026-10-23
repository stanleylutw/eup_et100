# 22 — v0.9.1 Sensor relay 交接與連線清理

日期：2026-10-23。分支 `v0.9.1_relay_handoff`，base `v0.9.0_system_status`（2455a03）。

**Codex 實作／host／ARM build 完成；Claude Step 5 review PASS；Stanley 負責實機（含 M0 SWD 取證）。未 commit／merge／push，未操作 UART／SWD／EVB。** 本文件依本輪施工單範圍新增；06 協定、平台與狀態文件、舊 21 分析留待 Claude Step 7 更新。

## 1. 核准決策與實作

Stanley 確認施工單兩處歧義：**清理最多等 15 秒後回傳 job；未完成上傳的保護期只由變更請求延長，查詢不延長。**

### Part A：上傳後交接

- manager 新增唯讀 `ota_manager_upload_open()`。worker 在 SMP 處理完成／刪除後發布 session 是否開啟與 `owner_ms` 快照；不從 serial task 直接讀 manager 的 session 欄位。
- relay 必須沒有 queued／in-flight SMP、排隊或執行中的 delete、尚在 30 秒保護內的 open upload，也不能重複取得 relay。條件與 relay 取得在同一 critical section 判斷。
- Sensor 套件成功驗證後 session 關閉，可同 tick 交接。未完成上傳及 image 0 仍維持 owner hold；SMP state／echo 等查詢不更新 manager 的 owner timestamp。
- `ota_worker_active()`、Type-1／SYS 暫停、OTABAUD 回 9600 的原本 30 秒規則不改。新 SMP 封包若在 relay 取得後才抵達，仍由 manager 原有 relay mutating guard 回 BUSY。

### Part B：有期限清理，保留 ownership

- cleanup 最多同步等待 15 秒；每秒重試 stop／disconnect。超時回傳原失敗結果；若本來成功但資源仍未釋放，回 `-ENOTCONN`，不能當成正常結束。
- gatt worker 繼續在 DRAIN 執行背景清理，不強制清除 initiating／connected／scan_stop_pending。relay job 可結束 FAILED／CONNECT_FAILED、釋放 NOR relay lock，但 reservation 的解除延後到真正 `release_owner()` 成功。
- DRAIN 期間新 SENSOROTA／reserve 仍回 BUSY；延遲 MASTER_CONNECT 仍在原 DRAIN 內斷線，不開第二個 session。輸出 queue 無法接受 DC 時不回 IDLE。
- 原始 CF/connect_timeout 保留；6 秒清理超時後回報 reason `disconnect_drain_timeout:i=...,c=...,s=...,stop=...,disc=...`，後續最多每 10 秒一次。只擴充既有 Type 8 reason 文字，不改欄位、state、error 名稱。`stop`／`disc` 為最近 SDK 回傳值，初始 -1 表示尚未嘗試。
- 清理超時／仍在 DRAIN 時 SYS BLE 回 ERROR。無自動 BLE reset 或整機 reset。

**D9 TODO（待 M0）**：原 `CONN_END` 只有 `status != 0 || connected` 才清 initiating 的條件完全保留。沒有加入 200 ms 推測性清除，也沒有宣稱修好 SDK 取消連線的根因。D11 的自動復原策略仍待 Stanley 決定。持續 DRAIN 時只能等待有效完成事件或由 Stanley 人工重啟。

### Part C：HTML

- BUSY 後每 2 秒重送啟動，最長 30 秒；期間 PACKAGE_CHECK 為 PENDING，顯示「裝置忙碌，稍後自動重試」。逾時顯示「裝置忙碌，請稍後再試」，不是 PACKAGE_INVALID。
- 每次重試有新 token，拒絕不匹配回覆；成功／取消／斷線會清 timer。非套件啟動錯誤也清掉樂觀 ACTIVE，不誤標套件。
- 只把目前已接受 OTA target 的 CF／DC 套到 relay 畫面；reason 以前綴識別。連線逾時顯示「BLE 清理中」，有清理超時文字；job 結束後在 DC 前禁止重新開始。其他 target 或無活動 OTA 的 CF 不套用。
- 既有 GATT Bluetooth 圖示、RS232 FileText、分區樣式及共用 log 高度不變。

## 2. 測試

| 命令 | 結果／範圍 |
|---|---|
| `bash fw/tests/test_relay_handoff.sh` | PASS；直接執行 production worker，manager／SMP 為 fixture：同 tick 完成、完成後查詢、未完成 hold／查詢不延長、兩 transport slots、in-flight、delete、重複 relay、clock wrap、原 active window 不變 |
| `bash fw/tools/test_evb_serial_host.sh` | PASS；真實 manager Sensor 完成後 upload_open=false／查詢不更新 owner；既有 relay mutating guards；新增 GATT 15 秒、stop 失敗、zero-status 不清 initiating、延遲連線、背景 worker 恢復、scan pending／queue backpressure／reservation；job FAILED／CONNECT_FAILED 與新啟動 BUSY；全部原有 serial／BLE／SYS／SMP 回歸 |
| `bash fw/tools/test_sensorflow_nano.sh` | PASS；真 ARM newlib-nano formatter 與 Sensor job suite。沙箱內 Unicorn 曾 SIGILL；允許沙箱外 JIT 後通過，不涉及 EVB |
| `bash fw/tools/test_inst_status_host.sh` | PASS；原 INST／trial／rollback 回歸 |
| `node tools/tests/test_relay_handoff_ui.cjs` | PASS；BUSY→重試成功、持續 BUSY 30 秒、取消、BAD_MAC 不誤標套件、不同 target CF 忽略、FAILED／清理鎖／DC 恢復 |
| `node tools/tests/test_et100_ui.cjs` | PASS；既有 mock Web Serial／ET-100／Sensor relay／版面回歸 |
| `node tools/tests/test_et100_transfer_ui.cjs` | PASS；原 ACK／INST 搬移、GATT Bluetooth、填色／標籤與桌機手機 |
| `node tools/tests/test_et100_smp.cjs` | PASS；原 SMP、INST 與 180 秒確認流程 |
| `node tools/tests/test_et100_sensor.cjs` | PASS；Sensor package upload／resume／錯誤／取消，不送 Test/reset |
| `python3 fw/tests/test_dfu_package.py` | PASS；ZIP／manifest、版本及 metadata CRC |
| `bash fw/tools/build_evb_serial.sh` | PASS；0.9.1-dev App／DFU，僅既有 RWX warning，無燒錄 |
| `git diff --check` | PASS |

新 worker fixture 不冒充真實 NOR 驗證；真 package／manager 路徑由原 `test_smp` 測試覆蓋。Host 不能確認 SDK 實機事件語意／電氣連線／D9 根因，EVB 不填 PASS。

截圖：`/tmp/et100_relay_busy_ui.png`、`/tmp/et100_relay_cleanup_ui.png`（mock，非實機）。
最後重跑 logs：`/tmp/et100_v0.9.1_host.log`、`/tmp/et100_v0.9.1_build.log`。錯誤提示在收合模式亦可見；右側摘要使用短文案「BLE 清理中」，保留既有分區圖高度。

## 3. 交付產物

只更新 App，不重建 Installer。

| 路徑 | bytes | SHA-256 |
|---|---:|---|
| `fw/build_serial/output/dfu_application.zip` | 199070 | `cc18ac7e594ff03a2a64fee1fd14654f8cc6e95443450ad71806ae96d2dc5fe0` |
| `fw/build_serial/output/et100.signed.bin` | 320704 | `e19a591a6a687b710354f6cc74eacfc396cfb7e3ba4eb5d3ad7e78f32ede00d2` |

ELF：`fw/build_serial/Objects/Project.elf`，text／data／bss = 284336／35808／85600 bytes。版本 0.9.1-dev；原型 metadata 的固定 git_branch／build_id 等歷史欄位不在本輪 scope，請以版本與 SHA-256 判別。仍是 hash-only 原型，不宣稱 production 簽章安全性。重打包 ZIP 可能改變指紋。

## 4. M0：Stanley 唯讀取證（Codex 不執行）

**先保存板上實際執行版本的 ELF**，不要用不同版本或後來重 build 的地址。若要取舊 0.9.0 的 M0，必須使用該次 0.9.0 的 ELF，不能拿本版符號地址讀舊版板子。

在卡住時不要先重新連接／reset；保存完整 serial log。以下由 Stanley 執行，沿用專案 `swd_probe.py` 的 generic cortex_m attach，不載入 FLM、不 halt、不 reset、不寫 Flash 或 SRAM：

```bash
cd /Users/stanley/Firmware/EUP/eup_et100
export M0_ELF=fw/build_serial/Objects/Project.elf
export M0_NM="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-nm"
"$M0_NM" -S --defined-only "$M0_ELF" | grep -E ' (state|initiating|connected|scan_stop_pending|connection_end_seen|connection_end_status)$'
fw/.venv/bin/python - <<'PY'
import os, subprocess, time
from pyocd.core.helpers import ConnectHelper

wanted = {"state", "initiating", "connected", "scan_stop_pending",
          "connection_end_seen", "connection_end_status"}
symbols = {}
for line in subprocess.check_output(
        [os.environ["M0_NM"], "-S", "--defined-only", os.environ["M0_ELF"]], text=True).splitlines():
    fields = line.split()
    if len(fields) == 4 and fields[3] in wanted:
        name = fields[3]
        if name in symbols:
            raise SystemExit("ambiguous symbol: " + name)
        symbols[name] = (int(fields[0], 16), int(fields[1], 16))
if set(symbols) != wanted or any(n not in (1, 4) for _, n in symbols.values()):
    raise SystemExit("missing/unexpected symbol sizes; check matching ELF")
session = ConnectHelper.session_with_chosen_probe(
    target_override="cortex_m", options={"connect_mode": "attach", "frequency": 1000000,
                                        "resume_on_disconnect": False})
if session is None:
    raise SystemExit("no selected probe")
with session:
    target = session.board.target
    for sample in range(3):
        print("sample", sample, "monotonic", time.monotonic())
        for name, (address, size) in sorted(symbols.items()):
            value = target.read8(address) if size == 1 else target.read32(address)
            print(f"{name}: @0x{address:08X} size={size} value=0x{value:X}")
        time.sleep(0.1)
PY
```

state：0 IDLE／1 WAIT_ADV／2 CONNECTING／3 SETUP／4 AUTH／5 READY／6 DRAIN。`state` 與 `connection_end_status` 在本版 ELF 為 1 byte，其餘表列旗標為 4 bytes，所以不可一律 read32。讀取時 CPU 仍在執行，樣本不是 atomic；回報三份樣本與 exact ELF／DFU 指紋。若 pyocd 未安裝或 attach 失敗，先回報錯誤，不改用 rescue／flash 工具。SWD attach 本身仍需接觸 debug 硬體，可能影響時序，不能當零干擾證據。

**目前沒有 M0 結果，D9 仍 TODO。** 即使讀到 initiating=1、CONN_END status=0，也需結合事件順序／stop 回傳值排除延遲 MASTER_CONNECT，再由 Claude／Stanley 決定修法。

## 5. Stanley 實機步驟（待執行）

1. 經 review 後用正常 App DFU 更新至 0.9.1，確認完整 hash／版本／confirmed；保留既有設定與 B-info 保護。Installer 不需更動。
2. RS232 上傳 Sensor 套件成功，確認 VALID，立即啟動 GATT OTA；不應只因最近 SMP 活動而等待 30 秒。若另一個 GATT session／排隊工作占用仍可 BUSY。
3. 未完成上傳／其他工作 BUSY 時，核對每 2 秒重試與最多 30 秒停止；取消／斷線不應繼續送啟動。
4. 在已收到可連線廣播、開始連線後讓 Sensor 無法完成連線，觀察 connect_timeout。正常清理可按既有策略重試；若清理卡住，最多再等 15 秒後 job 結束 FAILED／CONNECT_FAILED，按鈕顯示清理中且不能開始第二個工作。
5. 若持續 DRAIN，先做 M0，保留 CF 診斷。D9 尚未實作，不能保證 Sensor 再開機即可恢復；需要有效完成事件才回 IDLE，否則由 Stanley 取證後人工重啟。Sensor 從一開始就關機可能只得到 WAIT_ADV_TIMEOUT，不是 connect timeout 測試。
6. 確認正常上傳／重試／取消／既有 INST、共用 log／GATT Bluetooth 圖示及 SYS 未回歸；完整 log 交 Claude review。
