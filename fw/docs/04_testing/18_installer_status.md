# 18 — INST v1／試運行期限交付

日期：2026-10-23；branch：`v0.8.11_installer_status`；base：`df4158d`。

**Codex 實作／host／build；Claude Step 5 review PASS（M1 已修正並複驗），S4／Step 6b m1 本輪追加實作、待複驗；Stanley 實機測試待執行。** 本輪沒有開 UART、SWD、燒錄或 NOR 實體寫入，不標示 EVB PASS。協定見 [07](../02_protocol/07_installer_status.md)。

## 1. 政策與範圍

- O2：0x01F000 為完整 journal 的一部分，不可占用；NOR 0x460000–0x461FFF 為 EIR1 雙 4 KB sector，Logs 從 0x462000 起。commit 使用獨立 8-byte program 單位，新記錄讀回前不擦有效舊 bank。
- O3：同一完整 hash／同持久狀態連續三次中斷後，下一次 PENDING→ABORTED；BACKUP_READY→ROLLBACK_REQUIRED；回滾無上限。實際 I/O／驗證錯仍 HALT；CRC 錯／讀不到計數視為零。
- boot record v2 100 bytes、SDK、linker、0x08001000 保護不變；CRC／SHA-only 原型，沒有新增簽章驗證。
- Installer UART 115200；EVB 開發 target 預設 `ET100_INSTALLER_DEBUG=1`，保留 LOADER WINDOW／INST COMPLETE／TRIAL WDT 60s／E0 JUMP，供既有 loader／trial 工具使用。source 的量產預設維持 0，亦可明確以 make `ET100_INSTALLER_DEBUG=0` 覆寫。有界 NOR init／授權後開 8 s WDT，只以完成的 I/O／hash 讀取進展餵狗。正常 CONFIRMED 開機不碰 NOR；HALT 關閉 WDT，避免 fail-closed 變成無限 reset。
- App 自檢仍需 30 s 健康；120 s 期限獨立計時，逾時停止餵狗，給事件／worker 最多 1 s 後 reset。SDK pmu_init 的既有 WDT 盲窗未消除。
- App boot-state erase／program 在原有 critical section 內檢查期限，避免 reset 等待期間晚到的 SMP Confirm 改變回滾決定；host 同時覆蓋 health poll 尚未執行但期限已到的情形。
- NOR 記錄失敗不冒充成功。若最後 ABORTED／ROLLED_BACK 已由 boot state 證實，App 能回 code 12（診斷原因不可用），不臆測實際失敗源。
- S4：一般 App 開機不送 APP；TRIAL 送一次，CONFIRMED＋FAILED／ABORTED 僅未回報交易送一次。新 FlashDB key `ET100_FDB_INST_REPORTED_KEY=0x00030008` 保存已回報的完整 32-byte hash，與 auto list／boot health／report controls 等既有 key 分開。serial task 在送出 terminal APP／CONFIRMED 並返回後才保存；TRIAL APP 不記為終局已回報。保存失敗下次開機可重送，與 boot record／NOR EIR1 無關。

## 2. Host 與 build

| 項目 | 證據／狀態 |
|---|---|
| 共用 INST | CRC 31C3／2926／8208、長度／字元、10% 節流、最大映像輸出預算；`test_inst_status.c` |
| EIR1 | 42 次讀取／擦除／program／commit 斷電注入，舊或新完整記錄可讀、重新保存成功，區外 bytes 不變 |
| 交易／boot state | 687 次 backup／TRIAL／confirm／rollback torn-operation 注入；NOR init 各狀態、三次策略、壞 rollback、boot state 損毀；`test_trial_rollback.c` |
| App health | 正常確認、BLE／STORAGE／LOOP／WRITE 逾時、NO_CONFIRM、停止餵狗、GOTASTAT 快照刷新／缺失診斷；fake tick／reset，不是實測 120 s |
| S4／Step 6b | 正常／NONE 開機無 APP、TRIAL 送、還原／放棄首次送且再次開機不送、CONFIRMED 送後才記錄、旗標寫失敗重送、同前綴不同完整 hash、旗標長度無效、dequeue 後未送即 reset 不記錄；正常與 NO_CONFIRM host 皆 PASS |
| RS232／BLE | GOTASTAT 正常／無記錄／未就緒；0x09 TLV state／result／code／txn／完整檔案 hash；不在 callback 讀 NOR |
| HTML | CRC／錯版本／錯 txn／新欄位、HELLO／BOOT 遺失、HALT、ABORTED／還原查詢，完整 SMP 流程／180 s／Sensor regression；瀏覽器只用 mock Serial |
| 正常 Installer／App | ARM build／DFU；最後結果與檔案尺寸以本節 logs／輸出目錄為準 |
| 測試變體 | NO_CONFIRM App（0.8.111-dev）與 badcand Installer（0.8.11-badcand）；與正常版分目錄 |
| B-info 工具 | `test_flash_protection.py` 三項 PASS；沒有實體 before／after hash，本輪不可宣稱板上 B-info 已驗證 |

Logs：`/tmp/et100_v0.8.11_inst_host.log`、`/tmp/et100_v0.8.11_host.log`、`/tmp/et100_v0.8.11_installer_build.log`、`/tmp/et100_v0.8.11_app_build.log`、`/tmp/et100_v0.8.11_no_confirm_build.log`、`/tmp/et100_v0.8.11_bad_candidate_build.log`。ARM 的 RWX linker warning 為既有警告，不標成無 warning。

Step 6 M1：正常／badcand Installer 已以 EVB 預設 DEBUG=1 重建；兩份 raw image 均確認含工具所需字串，ROM header／CRC／B-info gap 通過。DEBUG=0 的 make dry-run 確認編譯旗標可覆寫（不是量產硬體驗收）；`test_inst_status_host.sh` 重跑 PASS。沒有修改 App 或重打包 App ZIP。

Step 6b m1：S4 已實作；`test_inst_status_host.sh`、`test_evb_serial_host.sh`、`test_et100_smp.cjs` 重跑 PASS；正常／NO_CONFIRM App 重建並重新打包，最新指紋如下。Installer／badcand 與 Step 6 M1 相同，沒有重建或改寫。本輪未做 EVB 測試。

## 3. 測試檔案（先交 Claude review，不代表允許出貨）

| 用途 | 路徑 |
|---|---|
| 正常 Installer，SWD | `fw/build_installer/output/Project_burn.bin` |
| 正常 App 0.8.11-dev，HTML／SMP | `fw/build_serial/output/dfu_application.zip`（同目錄 et100.signed.bin） |
| 不自我確認的 App 0.8.111-dev | `fw/build_serial_no_confirm/output/dfu_application.zip` |
| Candidate 讀取錯誤注入 Installer | `fw/build_installer_bad_candidate/output/Project_burn.bin` |

**NO_CONFIRM 與 badcand 僅供測試，測後還原正常 Installer／App。** badcand 只在 Candidate 讀取時反轉映像 offset 520 的 byte，不改寫實體 NOR，也不改 Rollback 來源。此驗收證明來源損毀的拒絕路徑，不宣稱做過物理 NOR 損壞測試。

### 3.1 本輪產物指紋

| 檔案 | 大小（bytes） | SHA-256 |
|---|---:|---|
| 正常 Installer `Project_burn.bin` | 51,768（raw 43,576） | `0882d97a53f86720940f7914f3b4d9df5eeb00bbb4bd53d5a7014c9b1faf0533` |
| 正常 App ZIP | 197,357 | `5233ba8f7ce232d26ba472d8c25278238c8754188d9148dc5db4bc4f904f60b7` |
| 正常 `et100.signed.bin` | 318,568 | `7039b54161e72587ceec7beec7ef913d2bc969bfe80df7aaa9f7421946a3d7e4` |
| NO_CONFIRM App ZIP | 197,251 | `2754e93bc5a2f6542b42befd0b205b617b4100533bdf40aaf50f11bf5520fd2a` |
| NO_CONFIRM `et100.signed.bin` | 318,352 | `ec95e24458c022adc42690df597075a4f8400485510ba4c2e2f637a26f88c832` |
| badcand Installer `Project_burn.bin` | 51,832（raw 43,640） | `e675e1394281b186ad7a39cf499743590a19ccbad36455fa809e5c10b7b78370` |

兩份 Installer 均驗證 ROM header 的長度／CRC、payload 一致、B-info 為全 00；只是檔案檢查，不是板上比對。兩份 App 均驗證非零 metadata CRC、MCUboot header／SHA-256／版本及 ZIP 內外映像一致；未驗數位簽章。正常 App ELF text/data/bss = 282200/35808/85480，NO_CONFIRM = 281984/35808/85480；正常 Installer = 39240/4336/9048，badcand = 39304/4336/9048。ZIP 重打包時間戳會影響 ZIP hash，映像 hash 才是交易識別。

## 4. Stanley 的四項 EVB 測試

### 前置

先完成 Claude review。關閉會占用 VCOM 的其他程式；保存目前 GCHK／GAUTOLIST／設定與 B-info hash。由 Stanley 使用既有保護工具部署正常 Installer：

```sh
fw/.venv/bin/python fw/tools/evb/fr3068_flash.py --image fw/build_installer/output/Project_burn.bin
```

工具必須跳過整個 B-info sector，要求映像 B-info bytes 與板上原有內容一致、驗證 before／after hash。若不一致就停止，不能繞過 guard。Installer 只有約數十 KB，不應擦除 App／Settings。先確認目前 App 仍可 GCHK；HTML 維持共用 Log 高度，只有圖表自行縮放。

### 1. 正常更新

以 HTML ET-100 載入正常 0.8.11 ZIP，從原本已確認的舊版更新。預期 115200 收 HELLO／START／CHECK／BACKUP（或 SKIP）／ERASE／WRITE／VERIFY／RESULT INSTALLED／BOOT；HTML 切 9600，再看新版 slot0 confirmed=false→true。成功仍由完整 hash／版本／confirmed 證明。確認 GOTASTAT 為 CONFIRMED，設定／GAUTOLIST、B-info hash 不變。

### 2. 120 s 未確認後還原

以正常 Installer、已確認的正常 0.8.11 App 為基準，HTML 選 NO_CONFIRM 的 0.8.111 ZIP。新 App 進 TRIAL 後約 120 s 應送 TRIAL_TIMEOUT，最遲再等待 1 s 主動 reset，Installer 還原原本 0.8.11。HTML 在 9600 聽不到回滾的即時 115200 行，需待舊 App 回來查 GOTASTAT：CONFIRMED／ROLLED_BACK／通常 code 9；SMP 的完整舊 hash／version 應吻合。

**初次安裝＋120 s＋回滾可能超出 HTML 180 s**：畫面顯示「結果未知」不等於失敗或成功；不要當下反覆重送，等裝置回來再查詢。這輪不擅自延長 180 s。若診斷記錄無法保存，code 12 可回報已還原但原因未知。

### 3. Candidate 驗證失敗 → ABORTED

先確認正常 0.8.11 App 已 confirmed。由 Stanley 以同一保護工具將 Installer 換成 `fw/build_installer_bad_candidate/output/Project_burn.bin`，不動 App／Settings。HTML 上傳 NO_CONFIRM 的 0.8.111 ZIP（它本身必須正常，不能先破壞 ZIP 讓 HTML 擋下）。預期 HELLO version 為 `0.8.11-badcand`，CHECK FAIL、RESULT ABORTED code 2，BOOT CONFIRMED；原 0.8.11 App 應仍可執行，GOTASTAT ABORTED，B-info／清單不變。

**測完必須由 Stanley 換回正常 Installer**，否則之後所有 Candidate 都會被注入錯誤而拒絕。

### 4. TRIAL 期間斷電 → 還原

確認已換回正常 Installer、原 0.8.11 App confirmed。再次更新 NO_CONFIRM 0.8.111；看到新版 slot0 confirmed=false／TRIAL 後、120 s 以前，由 Stanley 斷電再上電。預期一次 TRIAL 即還原，不重試新版；舊 0.8.11 confirmed，GOTASTAT ROLLED_BACK（通常 code 10），完整舊 hash／清單／B-info hash 不變。

## 5. Review 重點與限制

- boot record 沒有加欄位；NOR 記錄只是診斷與 O3 計數，不用來猜 App 安裝成功。三次政策有 host 覆蓋，不等於做過三次實體斷電。
- 結果雙 sector／計數、實際 erase 最慢時間、8 s WDT、UART TEMT／300 ms host 交接、RTOS stack／worker 排程仍待實機。App 事件 mailbox 是單行，可被較重要的新事件取代；host 不能依賴每一行必達。
- GOTASTAT 不含完整 hash，不能獨自證明成功；0x09 的 hash 是完整 signed-file hash，SMP 是 MCUboot TLV hash，不可混用。txn 前 8 hex 只作關聯，計數比對完整 32 B。
- 0x09 的跨 repo 分配（O5）、產品 UART／AW9523（O1）、車機接收主動行（O4）仍未完成；本輪只有 DEV1.4 實作，不修改 Gateway／App repo。
- Installer 版本須由 SWD 部署，App ZIP 無法更新它；只更新 App 時 HTML 會沿用舊 Installer 回退，不會出現本版安裝進度。
