# IMPLEMENTATION_PLAN — v0.9.0 系統／周邊狀態

## Branch

`v0.9.0_system_status`，base `v0.8.12_sensorflow_format`（5cd3bfa）。Stanley 已明確授權 Codex 代寫施工單、完成韌體與 HTML；Claude 後續 review，Stanley 負責實機。不自行 commit／merge／push，不存取 UART／SWD／EVB。

## 規格與決策

依 [08 SYS](docs/02_protocol/08_system_status.md)：`ET100$SYS;Type;Index,Status;...*CRC`，33 項、320-byte 上限、CRC-16/XMODEM；Type 10 完整快照、11 變更。查詢 GSYSSTAT 使用 token 包裝，只讀 RAM。新增介面跨模組，依 comm.md 升 minor 至 0.9.0-dev。

- 只接現有證據：SYSTEM（serial task 執行）、SETTINGS（FlashDB init）、NOR（唯一 OTA worker 的 prepare 結果）、BLE（實際 GAP scan／adv／link 事件）、RS232_0（本 target UART3 已開啟）、WATCHDOG（PMU enable bit）。RESET 無可靠來源維持 UNKNOWN；其餘預設 UNSUPPORTED，不猜測 NOT_FITTED。
- BLE 仍依核准表 1–6，兩種以上實際角色為 MULTI_ROLE；不擅自擴充手機／Sensor 子欄位。初始化／角色證據在本次開機有效，事件錯誤回 ERROR；失敗 scan 不影響仍有效的其他角色。
- 不用歷史 FAILED 推測本次 rollback。首輪 boot snapshot 為 TRIAL 才通知 Type 2，否則 Type 0；不改 Installer 或 boot state。
- serial task 送 SYS，既有 TX mutex 保護；無新 task、無 heap queue。每 500 ms 最多一行，5 秒收集後送全表，bulk 期間暫停，恢復補全表。pending 為固定 33-byte 已報值與目前值，不累積事件。
- 第一版 provider 是初始化／事件狀態，不宣稱持續硬體掃描、全 Flash 驗證、RF 或線束診斷。尚無新資料的產品 provider 不冒充 READY。
- HTML 在 Dash 增加獨立、未框成卡片的狀態表，完整 English Name／狀態名稱；不改共用 log、OTA 圖或 Gateway／Sensor 流程。不自動在 bulk 中查詢；只有已識別 FR3068 且版本 >=0.9.0 才開查詢。

## 檔案範圍

1. 新增 `fw/src/system_status.c/.h`：固定碼、快照、格式、節流；`fw/tests/test_system_status.c` 與 `fw/tools/test_system_status_host.sh`。
2. `fw/src/serial_gchk.c/.h`：GSYSSTAT parser／RAM callback、serial task SYS 排程；`fw/tests/test_serial_gchk.c`。
3. `fw/src/evb_serial_main.c`、`evb_serial_ble.c`、`ble_reporter.c`、`ota_worker.c`：既有 provider hook，不新增硬體操作。對應 `fw/tests/test_scan_lifecycle.c`、`test_app_gatt_server.c` 的 BLE event host test。
4. `fw/targets/evb_serial.mk`、`fw/tools/test_evb_serial_host.sh`：新模組／版本／host regression。
5. `tools/rs232_tester.html`、新增 `tools/et100_system_status.js`、`tools/tests/test_system_status.cjs`、`tools/tests/test_system_status_ui.cjs`；必要時修改既有 ET100 UI tests，保留既有功能。
6. 本施工單、`fw/REVIEW_REPORT.md`、08 協定、平台計畫與 `docs/00_project/11_project_bring_up_status.md`；新增 `fw/docs/04_testing/20_system_status.md`，記錄 host／ARM／DFU 指紋及 Stanley 實機步驟，不預填 EVB PASS。

## 驗收與停止點

### Stanley 追加授權：OTA 資料通道視覺化

同一分支完成 HTML 顯示：ET-100 RS232 上傳到 Candidate／Sensor 區、INST BACKUP 的 Flash App→NOR Rollback、INST WRITE 的 Candidate／Rollback→Flash App。只有已驗證的事件／ACK 能啟動動畫；ERASE／VERIFY／等待／錯誤不模擬資料搬移。遺失 START 時不推測 WRITE 來源；BACKUP SKIP 不動畫。最終決定：GATT 保留 Bluetooth，RS232／內部搬移用 FileText，固定四個、每趟四秒。只改 `tools/rs232_tester.html`、`tools/tests/test_et100_ui.cjs`、新增 `tools/tests/test_et100_transfer_ui.cjs` 及本施工單／20 報告，不改流程／協定／韌體／Log 或分區尺寸。

### Stanley 追加授權：提交與分析交接（2026-10-23）

Stanley 要求提交本分支並整理給 Claude 分析。新增 `fw/docs/04_testing/21_sensor_ota_handoff_analysis.md`，記錄 BUSY／PACKAGE_INVALID 誤標與 GATT cleanup 卡住的證據、待驗假設及建議；本次不修復這些行為，不 merge／push，不清除仍供 reviewer 使用的施工單／review。

Host 驗證全 33 項、全部碼、CRC、精確格式與邊界、boot type、bulk 暫停恢復、合併／節流、查詢 token／壞輸入。JS 與 C 向量一致；Playwright mock 驗證 Dash table、部分更新、未知碼、CRC 錯、重連／boot 清除、timeout、舊韌體及 Gateway 不探測，桌機／手機無溢出。原 serial／SMP／relay／HTML 回歸；正常 ARM App build／DFU；完成停待 review。禁止 SDK／linker／Installer／boot record／NVS key 改動，0x08001000 保護不變。

---

# v0.8.12 原施工單（歷史）

## 本輪授權與分支

Stanley 授權 Codex 代寫施工單並實作；branch `v0.8.12_sensorflow_format`，base `v0.8.11_installer_status`（1015337）。不 merge、不 commit、不燒錄、不 SWD、不改 SDK／linker／Installer／boot record／B-info。

兩份既有未提交檔案 `tools/rs232_tester.html`、`tools/tests/test_et100_ui.cjs` 含 Stanley 核准的圖表 UI 修改，原樣保留；本輪只在其上增加 Serial 修正／測試，交付時明記兩類 diff。v0.8.11 原施工單保留於下方，並非本輪擴大授權。

## 已知與未證實

- GSENSOROTAFLOW 的 `%012llX` 與實際 nano printf 不相容：改為 `%04lX%08lX`，高 16／低 32 bit，仍為 12 個 uppercase hex，token 不變。
- 上傳後查詢無回覆、舊 token 回覆的根因尚未證實；9600 恢復不證明曾重開機，USB 時間戳不能證明 DAPLink 重送。
- 同鮑率重開 port 是可減少的交接，不宣稱修正所有間歇故障。真正 reopen 後先送 CRLF 是清 parser 的緩解，不是根因證據；不延長 timeout、不跳過套件驗證。

## 本輪檔案範圍

1. `fw/src/sensor_ota_job.c`、`fw/tests/test_sensor_ota.c`：單點格式修正；零／高位／全 48-bit／token 與截斷測試，既有 relay regression。
2. `fw/targets/evb_serial.mk`：版本 0.8.12-dev；不改 SDK Makefile，不重建 Installer。
3. `fw/tests/nano_test_support.c`、`fw/tools/test_sensorflow_nano.sh`、`fw/tools/run_nano_test.py`、`fw/tools/requirements-nano-test.txt`：ARM nano 真實 libc 測試 ELF，以 Unicorn 執行 production UART formatter／fake Sensor host suite；依賴只安裝在 `fw/.venv`（Stanley 已核准），記錄固定版本；不改系統 Python／sudo。
4. `tools/rs232_tester.html`、`tools/tests/test_et100_ui.cjs`、必要時 `test_et100_smp.cjs`：依實際開啟的 port／baud／reader／writer 決定是否需 reopen；同 baud 不重開，真正 reopen 才送 CRLF；reset 後仍必須立即回 9600，失敗恢復仍可操作。不改 Gateway／Sensor 協定與既有 UI。
5. 本施工單、`fw/REVIEW_REPORT.md`、新增 `fw/docs/04_testing/19_sensorflow_serial.md`：host／nano／ARM／DFU 證據、log 與指紋、未解假設及 Stanley 的最小實機步驟。不替 v0.8.11 補填未做的 EVB 測試。

## 驗收與停止點

Host relay／SMP／serial 全通過；nano 執行測試驗證 12 位輸出與尾端 token；HTML mock 驗證重複查詢不 reopen、9600→115200→9600 真正切換、CRLF 不被當指令、上傳後查詢及既有 UI regression；ARM App／DFU build PASS，保留既有 RWX warning 說明。交付 `fw/build_serial/output/dfu_application.zip`／fingerprint，由 Stanley 實機部署與留完整 log；本輪完成即停待 review，不碰硬體。

---

# v0.8.11 原施工單（歷史參考）

## Branch

`v0.8.11_installer_status`，base `main`（df4158d），Stanley 聊天授權建立及實作。v0.8.10 任務 commit `1f16f0d`；獨立 docs commit `9e31239`。

## 依據與決策

規格：[07 INST DRAFT v2](docs/02_protocol/07_installer_status.md)。Codex 實作、Claude review、Stanley 實機驗收；不燒錄、不 SWD、不修改 SDK／boot record／linker／0x08001000。

- O2：journal 完整使用 32 個 sector，0x01F000 衝突；結果／中斷計數移至 NOR 0x460000–0x461FFF，雙 4 KB sector、CRC／commit、新完整記錄先於舊記錄回收。Logs 從 0x462000 起。
- O3（Stanley 核准）：完整 hash＋同 boot state 連續三次未完成開機嘗試後，下一次 PENDING→ABORTED、BACKUP_READY→ROLLBACK_REQUIRED、ROLLBACK_REQUIRED 不限次數。換狀態／交易重算；記錄無效視為零。實際 I/O／驗證錯誤仍 HALT。boot record 不變；NOR 記錄失敗不構成「安裝成功」證據。
- 試運行一次，120 s 絕對期限不受 loop 健康重計影響。App 在正常 task／TX mutex 送整行，NOR 存取只經 OTA worker。
- INST 固定 v1、CRC 向量 2926／8208／31C3、每階段 10% 節流；未知版本不觸發切速率或成功。HTML 保留 reset 起 180 s 總期限，漏事件回 SMP，成功必須完整 hash＋版本＋confirmed。

## 範圍與實作

1. `fw/common/inst_status.c/.h`：共用格式、節流、穩定碼、雙 sector 記錄與中斷策略；host tests。
2. `fw/installer/main.c`、`transaction.c`、`install.c/.h`：事件、結果、各失敗路徑及依進度餵狗，不改 install_hook 測試契約；bounded UART。必要時 `candidate.c` hash 進度 hook。
3. `fw/src/boot_health.c/.h`、`serial_gchk.c/.h`、`ota_worker.c/.h`：App 事件／120 s／NOR owner／GOTASTAT。
4. `fw/src/eup_meta_protocol.c/.h`、`eup_meta_service.c`：0x09 唯讀快照，不在 callback 讀 NOR；指令分配仍記為本專案待跨 repo 確認，不修改 Gateway。
5. `fw/targets/evb_installer.mk`、`evb_serial.mk`、`fw/tools/test_evb_serial_host.sh`、新增 `fw/tools/test_inst_status_host.sh`：納入模組／版本／測試。
6. `fw/tests/` 相應 installer／boot health／metadata／serial／fake NOR tests；`tools/rs232_tester.html`、`tools/tests/test_et100_*.cjs`：INST 旁路、進度、115200→9600、舊版／漏事件回退；共用 Log 高度不得變。
7. 本施工單、REVIEW_REPORT、07／06、相關平台／狀態／protocol 文件；新增 `fw/docs/04_testing/18_installer_status.md`，寫清楚 host／build 證據與 Stanley 四項實機步驟，不預填 PASS。

## 驗收與交付

Host 覆蓋正常／續作／ABORTED／ROLLED_BACK／TRIAL_TIMEOUT／HALT／狀態損毀／各狀態 NOR init 失敗、輸出預算、雙 sector 中斷、三次策略與漏 HELLO／BOOT／APP。ARM installer／App build 與 DFU；另外提供 NO_CONFIRM App 測試 zip，保持正常版輸出可辨識。不增加試運行次數。

交付 Installer 與 App 路徑、logs、正常／120 s 未確認／Candidate 損毀／TRIAL 斷電四項步驟；由 Claude review 後停止，不自行 commit／merge v0.8.11。
