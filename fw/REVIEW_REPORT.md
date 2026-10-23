# REVIEW_REPORT — v0.8.12

## Step 5 Claude review（2026-10-23）— 結論：**PASS**（M1 已於 Step 6 修正並複驗：`grep -q` 版本在一般 bash 下 PASS）

**Claude 自行重跑**：`test_evb_serial_host.sh`、`test_inst_status_host.sh`、`tools/tests/test_et100_{smp,ui,sensor}.cjs` 全部 PASS；App 獨立重 build PASS（僅既有 RWX warning），二進位不再含 `%012llX`；交付 ZIP 內 image 含新格式 `%04lX%08lX`。

**韌體修正正確**：`(step_results>>32)&0xFFFF` → `%04lX`、低 32 bit → `%08lX`，仍為 12 位大寫 hex，欄位數與 token 不變（未改協定）。

**ARM newlib-nano 測試有效（含負向對照）**：以 `grep -q` 取代 `rg -q` 後執行，修正後 PASS；**同一測試改用 HEAD 舊碼（`%012llX`）→ `nano emulation failed … Invalid memory read (UC_ERR_READ_UNMAPPED)`**，證明舊碼在 nano 下會把錯位參數當指標解參考。（模擬器記憶體配置≠FR3068，**不等於證明 EVB 上曾 HardFault**，但確認該 bug 有 fault 的可能，而不只是亂碼。）

### Major

| # | 問題 | 修正建議 |
|---|---|---|
| M1 | `fw/tools/test_sensorflow_nano.sh` 用 `rg -q` 檢查 map；非互動 bash 下 `rg` 不在 PATH（本機即如此）→ 腳本誤報 `nano libc was not linked` 並失敗，主要回歸測試在一般環境不可用 | 改用 `grep -q 'libc_nano.a'`（或先檢查工具存在並給明確錯誤） |

### Minor

| # | 問題 | 建議 |
|---|---|---|
| m1 | `et100Reopen()` 在 baud 相同時直接 return，但呼叫端仍無條件 `decoder.reset()`，可能丟棄正在接收中的半行 | 可接受；或只在真正 reopen 時 reset |
| m2 | reopen 後立即送 `\r\n`：若裝置尚未切到同一 baud，該 CRLF 會變成雜訊 byte，使 ASCII parser 進入 discard 直到下一個 `\n`，可能吃掉下一筆 ASCII 查詢（SMP 不受影響，因 SMP 解碼器先處理） | 目前 OTABAUD ACK 後裝置即切速、HTML close/open 需數十 ms，風險低；記錄觀察 |
| m3 | 流程：plan／REVIEW_REPORT／19 報告由 Codex 撰寫（Stanley 授權，因 Claude token 不足） | 已知例外，Step 7 由 Claude 重讀確認 |
| m4 | 異常一（無回覆）、異常三（舊 token）根因仍未證實；本版只修已確認的格式問題並降低 reopen | 實機重測時保留完整 log；再發生無回覆時先以 SWD 讀 PC／CFSR／HFSR，不要先重連 |

---


## v0.8.12 Step 4 Codex 交付

Branch `v0.8.12_sensorflow_format`，base `1015337`。Stanley 因 Claude token 不足授權 Codex 代寫 plan／實作；**未經獨立 review、尚未實機驗收、未 commit／merge／push**。v0.8.11 review 原文保留於下方，並非本版 PASS。

- 已修：production FLOW `%012llX` → `%04lX%08lX`（高 16／低 32 unsigned long），12 uppercase hex／token／欄位數不變；App 0.8.12-dev。
- HTML：同一 port／baud 且 streams 完整時不 reopen；真正 reopen 後 CRLF 清 ASCII 殘留；新接線與既有 Gateway 切 baud 只記錄 baud。這是減少交接的緩解，不宣稱確認所有舊 token／無回覆根因。
- Host、ARM 真 nano printf 執行、HTML mock Serial、SMP、ARM App／DFU PASS；只有既有 RWX warning。Unicorn 2.1.4／pyelftools 0.32 已依授權裝在專案 venv，無系統 Python／SDK 變更。
- Nano 測試連入 production `sensor_ota_job.c`，執行零／高位／全 48-bit、mask、token／截斷及 fake Sensor relay suite；不是只對 PC libc 測同一字串。沒有 EVB／UART／SWD 操作。
- 新增測試工具、完整證據／指紋與 Stanley 步驟見 [19 報告](docs/04_testing/19_sensorflow_serial.md)。`fw/build_serial/output/dfu_application.zip` 為本版 0.8.12；Installer 未重建，0.8.11 測試產物報告為歷史紀錄。

### 請獨立 reviewer 重點檢查

1. 12 位 hex 精確長度、高位遮罩、ABI 型別與 token；nano runner 確實執行相同 production UART formatter／assertions，沒有用 PC libc 代替。
2. HTML baud cache 在首次 open、其他 Gateway 切 baud、reopen 失敗／重新連線時是否正確；same-baud 不丟 reader／writer，reset 後仍立即回 9600。
3. CRLF 只在真 reopen 送出，不當根因證明；保留 token 驗證、套件 SHA／metadata 驗證及既有 timeout，沒有假報成功。
4. 兩份 HTML 檔包含先前 Stanley 核准的未提交 UI 改動，本輪原樣保留並加測；Review／後續 commit 要區分 UI 與 UART 修正。

### 未解問題

上傳後第一筆查詢偶發無回覆、舊 token／通用失敗回覆仍未取得實機原因。HardFault、bridge 重送、接收殘留、parser 黏接都不能寫成既定事實。待 Stanley 用新版重測後判定，不能把 60 s timeout 當 NOR 寫入速度。

---

# v0.8.11 原 review（歷史）

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
