# REVIEW_REPORT — v0.8.11 交付 Claude review

## Step 5 Claude review（2026-10-23）— 結論：**PASS**（M1 已於 Step 6 修正並複驗）

**Step 6 複驗**：`evb_installer.mk` 預設 `ET100_INSTALLER_DEBUG ?= 1`（source 預設仍 0）；Claude 獨立重 build installer 與交付檔逐 byte 相同，`LOADER WINDOW`／`INST COMPLETE`／`WDT 60s`／`E0 JUMP` 字串皆存在；18 報告新指紋（正常 `0882d97a…`、badcand `e675e139…`）與檔案一致；host／HTML 測試全部 PASS。Minor m1–m5 不阻擋，記錄如下（m1 待 Stanley 決定）。

**重跑驗證（Claude 自行執行）**：`test_inst_status_host.sh`、`test_evb_serial_host.sh`、`tools/tests/*.cjs` 全部 PASS；ARM installer／App 於獨立目錄重新 build PASS（僅既有 RWX warning），**installer `Project.bin` 與 Codex 產物逐 byte 相同**；18 報告四份產物 SHA-256 與實際檔案一致。

### Step 6b（m1，Stanley 決定後追加）— ✅ 已實作並複驗

**複驗**：`boot_health.c` 僅在 TRIAL，或 CONFIRMED＋FAILED／ABORTED 且 txn 未回報時產生 `APP`；`ET100_FDB_INST_REPORTED_KEY`（0x00030008，新 key）在 `serial_send` 完成後才於 serial task 寫入，寫入失敗下次重送；host／SMP／UI 測試 PASS；App 重 build 與交付檔僅差打包階段回填的 EUPFWV1 CRC 4 bytes，18 報告指紋與檔案一致。


App `APP` 行改依 07 S4：只在 boot state `TRIAL`，或 `CONFIRMED`＋`last_update` 為 `FAILED`／`ABORTED` 且該 txn 尚未回報時送一次；回報後以 App 端持久旗標（FlashDB 新 key，於 `et100_fdb_keys.h` 登記，不得重用既有 key）記住 txn；試運行中送出 `CONFIRMED` 即視為已回報；旗標寫入失敗允許下次重送一次。一般開機不輸出。需新增 host 測試：一般開機無輸出、TRIAL 送、還原後第一次送且第二次不送、旗標寫入失敗重送一次。

**Codex Step 6b 實作完成，待 Claude 複驗**：

- boot health 依 S4 篩選 APP；terminal APP／CONFIRMED 的完整 hash 隨行保存，serial task 在 `serial_send()` 返回後呼叫 `et100_boot_line_sent()`，才寫新的 FlashDB key `0x00030008`（32-byte hash）。TRIAL APP 不標為結果已回報；不改 boot record／EIR1。
- 正常與 NO_CONFIRM host 均覆蓋一般開機靜默、TRIAL、FAILED／ABORTED 首次送／再次不送、CONFIRMED 送後記錄、寫入失敗後重送、完整 hash 比對、錯長度記錄及 dequeue 後未送即 reset。既有設定 key 與 auto list 不變。
- `test_inst_status_host.sh`、`test_evb_serial_host.sh`、`test_et100_smp.cjs` PASS；正常 App／NO_CONFIRM App build／DFU PASS（僅既有 RWX warning）。兩份 App 最新尺寸／SHA-256 已更新 [18 §3.1](docs/04_testing/18_installer_status.md)。Installer 指紋不變。
- 本輪沒有硬體操作，未 commit；沒有修 m2–m5 或更動 SDK／linker／B-info 保護。狀態行仍是 best-effort，旗標代表已嘗試透過 UART 送出，不是 host 收到的 ACK。

### Major

| # | 問題 | 位置 | 修正建議 |
|---|---|---|---|
| M1 | EVB installer 預設 `ET100_INSTALLER_DEBUG=0`，`printf` 全部靜音：`LOADER WINDOW`、`INST COMPLETE`、`TRIAL WDT 60s`、`E0 JUMP` 不再輸出。`fw/installer/load_image.py`（L52、L72–73）與 `fw/installer/trial_host.py`（L46、L75、L85–90）等待這些字串 → **開發用 loader（免 SWD 載入）與 rollback 實機測試工具失效**，屬既有功能退化。07 §9 原意為「量產預設關閉、開發版保留」 | `fw/installer/main.c` `_write()`；`fw/targets/evb_installer.mk` | EVB 開發 target 預設開啟（例如 `evb_installer.mk` 加 `ET100_INSTALLER_DEBUG ?= 1` 進 CFLAGS），量產 target 才為 0；或至少 `LOADER WINDOW` 改用 `status_write` 輸出。修正後 badcand／正常 installer 重 build 並更新 18 報告指紋 |

### Step 6 M1 修正（Codex，待 Claude 複驗）

- `evb_installer.mk` 新增 `ET100_INSTALLER_DEBUG ?= 1` 並加入 CFLAGS；不改 `main.c` 的 fallback 0。量產保留 0，EVB 可明確用 make `ET100_INSTALLER_DEBUG=0` 覆寫，已 dry-run 確認。
- 正常／badcand Installer 重建 PASS（只有既有 RWX warning）；兩份映像都含 LOADER WINDOW／INST COMPLETE／WDT 60s enabled／E0 JUMP，header／CRC／B-info gap 驗證 PASS。
- `test_inst_status_host.sh` 重跑 PASS（42＋687 次中斷案例）；新尺寸／SHA-256 見 [18 §3.1](docs/04_testing/18_installer_status.md)。App ZIP 未變，沒有硬體操作；尚未宣稱 loader／trial 實機 PASS。
- 本輪只修 M1，Minor 保留由 Stanley／Claude 決定。

### Minor（不阻擋，記錄）

| # | 問題 | 建議 |
|---|---|---|
| m1 | App 在**每次開機**（含一般 CONFIRMED）都在 9600 送 `APP` 行，等於對 RS232_0 另一端（客戶車機）每次開機主動輸出；與 07 A.8／Codex 先前 Q8 建議「一般開機不輸出」方向不一致（07 §5.2 目前文字允許） | **Stanley 決定（2026-10-23）：只在試運行或有未回報結果時才送** → 07 S4／§5.2／A.8 已更新，列為 Step 6 修正項（見下方 Step 6b） |
| m2 | installer 跳轉前固定等 300 ms（`nor_qspi1_delay_us(300000)`），一般開機（無 INST 輸出）也付出 | 僅在 `inst_active` 時等待 |
| m3 | OTA worker 啟動即 `app_nor_prepare()`（每次開機初始化並授權 NOR 寫入），先前為首次 OTA 時才準備 | 可接受（只讀 EIR1）；記錄行為變更 |
| m4 | HALT 時若無碼則填 `IC_VERIFY_FAIL`，可能誤標原因 | 可接受；或新增 `UNKNOWN` 碼（需同步 07 §7） |
| m5 | 流程：Codex 已修改 07／06／04／平台計畫／狀態文件（comm.md：MD 由 Claude 在 Step 7 維護） | Step 7 由 Claude 逐段重讀確認，不另要求回退 |

### 已確認通過的重點

- **回報與決策分離**：`status_write()` 每行有 50 ms 上限、失敗只丟棄；EIR1 寫入失敗不影響安裝；記錄讀不到視為 0 次。
- **O3**：`inst_attempt()` 第 4 次開機（前 3 次未完成）PENDING→ABORTED、BACKUP_READY→ROLLBACK_REQUIRED；ROLLBACK_REQUIRED 無上限；I/O／驗證錯誤仍 HALT。
- **rollback 原因**：`transaction_prepare()` 由記錄判定 TRIAL_TIMEOUT(9)／TRIAL_RESET(10)，不受先前 `status_record` 覆寫影響。
- **120 s 期限**：`trial_started` 只在首次 poll 設定、不受主迴圈重計；逾時後 `erase_state`／`program_state` 拒寫，擋住晚到的 SMP Confirm；最多 1 s 等記錄與事件送出後 reset。
- **NOR 所有權**：App 端 EIR1 讀寫只在 OTA worker；`GOTASTAT`、BLE `0x09` 只讀 RAM 快照，BLE callback 不碰 NOR。
- **HTML**：INST session 與 180 s 期限在送 reset **之前**建立；txn 取上傳檔完整 SHA-256 前 4 bytes（與 installer boot state hash 一致）；`HALT` 立即失敗；30 s 無事件回 SMP；成功仍需完整 hash／版本／confirmed。
- boot record 格式、SDK、linker、0x08001000 保護未變；試運行維持 1 次。

---


日期：2026-10-23；branch：`v0.8.11_installer_status`；base `df4158d`。

**狀態：Codex 實作與 host／build 完成，未經獨立 review，未做實機測試。** Stanley 負責實機；本輪沒有 UART／SWD／燒錄／實體 NOR 寫入，尚未 commit／merge v0.8.11。

## 完成範圍

- INST v1：共用 CRC16／整行格式、10% 節流、穩定結果碼；Installer 各階段輸出、115200 BOOT→9600、bounded TX、依實際進度餵狗。
- O2：NOR `0x460000–0x461FFF` 雙 sector EIR1 記錄，Logs 移至 `0x462000`；不占 journal。
- O3：同完整交易 hash／持久狀態三次中斷，下一次 PENDING 放棄、BACKUP_READY 還原；ROLLBACK_REQUIRED 無次數上限。boot record 格式不變。
- App：APP／CONFIRMED／TRIAL_TIMEOUT、120 s 絕對期限、逾時禁止晚到的 Confirm 寫 boot state；結果保存只透過 OTA worker。
- GOTASTAT 與 BLE `0x09` 唯讀快照；HTML INST parser／CRC／txn 關聯，漏事件回 SMP，180 s 總期限與完整 hash／版本／confirmed 成功門檻保留。共用 Log 尺寸不變。
- SDK／linker／0x08001000 保護未修改；沒有新增簽章驗證。

## 驗證

| 檢查 | 結果 |
|---|---|
| `bash fw/tools/test_inst_status_host.sh` | PASS；42 次 EIR1 中斷注入＋687 次交易 torn-operation；正常及 NO_CONFIRM health、三次策略、Candidate 讀取錯誤注入 |
| `bash fw/tools/test_evb_serial_host.sh` | PASS；含既有 BLE／SMP／relay、GOTASTAT／0x09／health regression |
| `node tools/tests/test_et100_smp.cjs` | PASS；INST／舊版回退／HALT／ABORTED／rollback／180 s |
| `node tools/tests/test_et100_ui.cjs` | PASS；mock Serial、桌機／手機截圖、共用 Log 高度與既有 Gateway／Sensor regression |
| `test_flash_protection.py` | 3 PASS，未接硬體 |
| `check_plan_vs_code.py` | 3 PASS |
| ARM 正常 Installer／App、NO_CONFIRM App、badcand Installer | PASS；只有既有 RWX linker warning |
| 產物結構檢查 | ROM header／CRC／B-info 檔案內容、App 非零 metadata CRC／MCUboot SHA／ZIP 一致 PASS |

完整 logs、產物路徑／hash、Stanley 四項測試步驟見 [18 報告](docs/04_testing/18_installer_status.md)。Host injection 不是實體斷電驗證；不得將本表改寫成 EVB PASS。

## 請 Claude 重點檢查

1. O3 中斷計數與新交易／換持久狀態重算、三次後分支；結果記錄遺失視為零，不把診斷當安裝成功權威。
2. EIR1 新 bank body／commit／readback 順序、舊 bank 保留、NOR 單一 owner 與結果 mailbox；App 正常啟動會初始化 NOR 讀取診斷，但 Installer 正常啟動不需要 NOR。
3. App boot-state cache 與 worker SMP Test／Confirm 刷新、120 s 對 erase／program 的期限檢查、最多 1 s 診斷等待；SDK pmu_init 的 WDT 盲窗仍存在。
4. Installer WDT／各失敗碼、bounded UART、LDR1 READY／ACK 相容性、BOOT 300 ms handoff；正常 CONFIRMED 不送 INST 行（EVB 開發 target 仍有工具所需人讀 log），HALT 仍可見。
5. HTML 未知協定版本／錯 txn／漏 HELLO、BOOT、APP；HALT 留在 115200、成功只看 SMP 完整證據；rollback 原因查不到時不臆測。

## 限制與交接

- **未實測** WDT 最慢 erase、堆疊、UART 交接時間／漏行、四項 EVB 場景；先 review 再交 Stanley。
- 120 s TRIAL 加安裝／還原可能超過 HTML 180 s；畫面屆時只能報結果未知，後續查詢，不延長既有期限或假報失敗。
- `0x09` 跨 repo 分配、產品 UART／AW9523、車機是否接收主動行仍為開放議題；不改其他 repo。
- NO_CONFIRM 與 badcand 是獨立測試檔，不可出貨；badcand 為讀取注入，並非實體 NOR 損毀。
- 新增 common 模組、測試、script、18 報告目前為未追蹤檔，review／後續提交須一併納入，不能只看 `git diff` 的已追蹤檔。
