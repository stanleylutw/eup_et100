# IMPLEMENTATION_PLAN — v0.9.3 一般 GATT 連線期間恢復掃描

## Branch

分支：`v0.9.3_gatt_scan_resume`；base：`main`（`a04e0d6`）。
2026-10-24 Stanley 明確指定 v0.9.3，並授權 Codex 代寫施工單及實作。
初始 Step 4 只做 host 測試與 App build，不操作 EVB／SWD／燒錄；原先不 commit，後續依 Stanley 明確授權已提交 `77ba471`。本輪 Step 7 留在原分支，只同步文件、不代為 commit。
Claude 後續獨立 review、Stanley 實機驗收；host PASS 不代表射頻並行能力已證實。

## 1. 已查證與未查證

- SDK `fr30xxc_sdk__202411` central 範例在連線前停止掃描、斷線後恢復；ET-100 沿用此策略。
- 目前 Host library 的 `gap_scan_start()` 排入請求後回 0，必須依 `GAP_EVT_SCAN_START` 判定成功。
- SDK 未明文禁止既有 GATT link 與 scan 並行；實際 Controller 的並行行為、穩定性與覆蓋率未查證。
- Type-1 原有 CID／MAC 過濾、去重、GTIME／MAXRPT 不改；SMP 主動回報暫停規則依下方 Part B 的追加授權修改。

## 2. 修改範圍（白名單）

- `fw/IMPLEMENTATION_PLAN.md`（本施工單）
- `fw/src/ble_reporter.c`、`fw/src/ble_reporter.h`（掃描 profile／生命週期與 RAM 診斷）
- `fw/src/gatt_client.c`（一般 READY 啟用與 cleanup 恢復）
- `fw/tests/test_scan_lifecycle.c`、`fw/tests/test_gatt_client.c`（fake SDK 回歸）
- `fw/targets/evb_serial.mk`（App 版本 `0.9.3-dev`）
- `fw/docs/04_testing/24_gatt_scan_resume.md`（結果、指紋與 Stanley 測試步驟）
- Part B 追加：`fw/src/serial_gchk.c`、`fw/src/ota_worker.c`、`fw/src/ota_worker.h`、`fw/src/smp_ble.c`、`fw/src/smp_ble.h`。
- Part B 測試：`fw/tests/test_relay_handoff.c`、`fw/tests/test_smp.c`、`fw/tests/test_smp_ble_activity.c`、`fw/tools/test_evb_serial_host.sh`。
- Part B 報告：`fw/docs/04_testing/25_smp_report_resume.md`。
- M1 修正追加：`fw/src/serial_report_tx.h`、`fw/tests/test_serial_report_tx.c`（共用單筆 TX 仲裁與真實 SYS／reporter 回歸）。
- Step 7 文件追加授權（2026-10-24）：`docs/00_project/11_project_bring_up_status.md`、`fw/docs/00_project/00_et100_firmware_platform_plan.md`、`fw/docs/02_protocol/01_evb_serial_gchk.md`、`fw/docs/02_protocol/02_evb_gatt_client.md`、`fw/docs/02_protocol/04_smp_serial_ota.md`、`fw/docs/02_protocol/06_sensor_ota_relay.md`、`fw/docs/02_protocol/08_system_status.md`、`fw/REVIEW_REPORT.md`；本施工單同步記錄授權與結果。

不改 SDK、Installer、HTML、GATT UUID、UART payload、FlashDB／boot record 或其他 repo。

## 3. 實作決策

1. 一般 GATT 認證成功、非 exclusive owner 才切換 connected profile：interval 40／window 10（25 ms／6.25 ms，要求占空比 25%）。維持 GEN_DISC、1M、dup_filt_pol=0、duration=0。
2. IDLE／WAIT_ADV 使用原本 interval 40／window 30；exclusive Sensor OTA 不啟用 connected scan。
3. 每次一般 GATT session 最多送一筆 connected scan start；明確失敗或中途 SCAN_END 後不在本 session 自動重試，避免 radio／SDK 請求風暴。同步拒絕、非零 START status、缺事件分開保留診斷。
4. 掃描狀態以事件確認；START_PENDING／STOP_PENDING 不重複排入請求。開始等待 3 s 無事件時，記 timeout 並先送 stop 嘗試同步，不能直接重送 start。stop 接受後 3 s 無 END 時保留不確定狀態，等待事件，不盲目啟動新 profile。
5. 更換 profile 必須先停止既有 scan 並等 END；SDK 呼叫前先發布 pending 狀態，允許 fake SDK 同步 callback。正常 API 呼叫在 critical section 外。
6. cleanup 切回 normal profile；掃描復原不改 GATT DRAIN ownership／取消 quiet 期，也不阻塞連線清理。低占空比掃描失敗不輸出 CF 或主動斷線。
7. 既有 `et100_scan_status` 仍以 0 代表事件已確認 active；新增具名 RAM 診斷：phase、desired／issued／active window、start／stop 計數與錯誤／timeout。沒有新增線上協定欄位。

## 4. 驗證

- 真實 scan adapter＋fake SDK：同步 callback、延遲成功、同步拒絕、非零 status、缺 START／END、profile 切換、tick wrap、重複事件、重複呼叫節流。
- 在 connected profile 注入其他 Beacon，驗證實際 reporter 接收與 Type-1 payload／checksum／過濾規則；不是只驗證 start 函式被呼叫。
- GATT fake：一般認證成功才啟用；認證失敗不啟用；手動／auto 共用 session；exclusive 不啟用；cleanup 恢復 normal；維持 v0.9.2 M0／late-connect／quiet 回歸。
- `bash fw/tools/test_evb_serial_host.sh`、relay handoff、system status、installer status、nano 測試及正常 App build。
- 記錄 App／DFU SHA-256；Stanley 驗證 SCAN_START 成功、其他 Beacon ADV／Type-1 增加、GATT 指令延遲／斷線率、斷線恢復與 Sensor OTA 不受影響。

## Part B — Step 4 追加施工單：SMP 結束後恢復主動回報

2026-10-24 Stanley 授權同一分支追加實作，保留 Part A 的現有變更；不 commit、不操作 EVB。

1. Beacon Type-1／SYS 暫停改為實際 SMP 忙碌＋3 秒靜默緩衝，取代原本回報專用的 30 秒活動判斷。UART 收到 SMP byte、BLE 收到 SMP VALUE write、完整請求排入 worker 均記錄活動。
2. queued／processing／整包 response sending 任一存在就保持暫停。回覆成功、失敗或送出失敗，都在 sender 返回後重新計 3 秒；不能在分段／多行回覆之間解除。回覆傳送狀態獨立於既有 NOR relay ownership，避免改變 v0.9.1 的接管條件。
3. UART 半行／多行半包依既有 200 ms／2 s 逾時，新增每輪 serial task 檢查。BLE 半包使用原本 2 s，新增週期性清除與 receiving 查詢；斷線清除既有重組狀態。busy 查詢與 callback 更新使用 critical section，不能以缺少下一包當作永久 busy。
4. UART／BLE 任一收包、worker 忙碌或最後活動／回覆結束未滿 3 秒，均暫停 UART 主動回報。這不等於停止 BLE 掃描，也不聲稱降低射頻負載。
5. OTABAUD 的 UART SMP idle 30 秒與 NOR／relay upload hold／mutating guard 不改。SYS 不再因目前非 9600 baud 而額外暫停，和 Type-1 共用上述安全緩衝，恢復時使用目前 baud。ASCII 查詢不重啟 SMP 緩衝；SMP 唯讀查詢只延長新的 3 秒緩衝。
6. 已開始送出的 UART 行不截斷；保留 TX mutex，不增加協定欄位、不改 HTML、SDK、Installer、FlashDB、UUID。Type-1 的 GTIME／MAXRPT 排程不變，因此 3 秒是解除暫停，不保證每台 Beacon 都在 3 秒內出現。

### Part B 驗證

- 真實 worker＋fake manager：serial／BLE queued、處理超過 3 秒、回覆分段超過 3 秒時仍暫停；回覆完成後 2999 ms 暫停、3000 ms 恢復；新請求重新計時；無回覆／失敗回覆、tick wrap、NOR 接管回歸。
- 真實 BLE adapter＋fake SDK：第一段就暫停、半包 2 秒逾時週期性解除、斷線清除、分段 notification 回覆期間持續暫停。
- 真實 serial decoder：半行／半包無新 byte 時的週期性逾時與 tick wrap；多行 SMP 回覆的每行 sender 檢查暫停。
- 重跑 serial host、relay handoff、正常 App build；記錄新版產物指紋。115200＋Beacon 連續上傳、拔線、完成後持續查詢、OTABAUD 30 秒相容性由 Stanley 實機驗收，不以 host 測試宣稱第三方 serial client 相容性已通過。

### M1 修正與重新 review

2026-10-24 Stanley 授權修正後再 review。前輪發現：送 SYS 時可能收到新的 SMP，原迴圈卻沿用送 SYS 前的 `reports_paused`，繼續送 Beacon。

- SYS／Beacon 各自以非阻塞方式取得現有 TX mutex，再重新檢查 SMP、半包及 RX ring／overflow；不共用舊快照。鎖忙碌或 gate 關閉時不呼叫 producer，避免消耗尚未送出的 SYS／reporter 狀態。
- gate 通過後視為仲裁取得一筆完整行的發送權；該 producer 最多送一行，不可在持鎖時再次取得 TX mutex。開始仲裁後的新 SMP 允許該行完成，但下一行必須重新取得發送權並檢查最新狀態。不在 critical section 裡做 UART 輸出。
- 以小型共用 header 固定「try lock → fresh gate → producer → unlock」順序，production 與 host 使用同一仲裁實作；SYS 暫停仍保留原有 full-snapshot coalescing 行為。
- 測試真實 SYS／reporter：取得 TX 時注入 SMP、不消耗待送資料；SYS 送出期間注入 SMP，下一筆 Beacon 被阻擋；worker busy／RX pending／half-packet 分別阻擋；SMP 解除後可正常送出。保留原先長回覆／3 秒／NOR 回歸，重跑 build 後再 review。

## Step 7 文件同步

2026-10-24 Stanley 要求留在同一分支，程式已於 `77ba471` 提交；本輪僅更新永久文件及 review 紀錄，供後續另一筆 commit，不 amend、不 merge、不 push。

- 平台計畫／Revision History 與 bring-up 狀態同步至 v0.9.3；一般 GATT connected scan 與 SMP 回報排程分開描述。
- 同步 serial／GATT／SMP／relay／SYS 的行為與 timeout 定義，保留線上格式、UUID、儲存 schema、OTABAUD 30 秒與 NOR owner 保護不變。
- `REVIEW_REPORT.md` 如實記錄 Codex 自我複查、已修 M1、Minor 整合測試缺口及待驗收項目，不沿用 v0.9.2 的 Claude PASS 當作本版 review。
- 無新增量測，不改既有歷史測試數字；最新產物與測試結果仍引用 24／25 報告。無建置／安裝步驟或流程變更，不改 README／comm.md；本 repo 無獨立 Changelog，本次版本記錄集中於平台計畫 Revision History。
- 完整性搜尋涵蓋 `docs/`、`fw/docs/`、`fw/README.md` 與兩份 comm；舊測試報告保留當時版本語境，不回填為本版實測。25 報告僅釐清 SYS 暫停後仍補 Type=10 的敘述，不新增量測。
- `python3 fw/tools/check_plan_vs_code.py` 的 3 個 CHECK 全通過；掃描 profile、3 秒 report gate、兩個 30 秒保護另逐項對照 source。`git diff --check` 通過。全部改動為 Markdown，未重跑 firmware build 或接觸硬體。
