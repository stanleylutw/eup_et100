# v0.9.2 Step 7 文件補同步與 UI 回歸

日期：2026-10-24；分支 `v0.9.2_gatt_stop_release`，文件起點 `aa9d5d6`。
Stanley 要求先提交 v0.9.3 文件，再回本分支補 Step 7、commit、merge main，最後返回 v0.9.3；不 push。本輪只有 Markdown 變更，不改 firmware／HTML／tests／SDK／Installer，也沒有 UART／SWD／USB／EVB 操作。

## 1. 補同步內容

1. GATT 通用協定原本仍寫零 status CONN_END 可能讓 owner 永久 DRAIN。補入 `76bb3df` 已實作的 accepted stop／任意 status END、同步事件 sequence、2 秒無事件後備／quiet、延遲 MASTER 清理及診斷欄位。未取消的零 status 不套用快速釋放。
2. 補記 `4c67d9d` 的 UI：Dash 第一／預設頁、圖示側欄與 tooltip、ET-100 系統／Gateway 唯讀資訊、33 項中文狀態與 SCH 數字區塊／產品主板 PDF、ET-100 隱藏舊遙測、LTE 精簡表格、GSM 選埠與連線分離，以及 Flash／NOR 視覺一致性。
3. 平台計畫保留 v0.9.2 版號，新增 Revision 與現行行為說明；舊 baseline 明確標示為歷史，不猜目前板況。bring-up 儀表板與 SYS UI 文件加上對應引用。
4. 保留原版 retry Minor：1200 ms backoff 短於 2000 ms quiet，BUSY 可能消耗 attempt。沒有修 firmware 或宣稱此問題已消失。

## 2. 本輪重跑

五組皆使用 mock Web Serial，不開啟實體 port；Chrome 在 sandbox 內啟動失敗（SIGABRT／EPERM），同一腳本經核准於 sandbox 外執行後通過，未修改腳本或安裝套件。

| 命令 | 本輪結果 |
|---|---|
| `node tools/tests/test_dash_info_ui.cjs` | PASS；ASCII 系統／設定資訊、NOR 狀態、MAC 清單、查詢逾時、斷線、非 ET-100 篩選、responsive light／dark |
| `node tools/tests/test_sidebar_nav_ui.cjs` | PASS；六個 view、active state、tooltip／Escape、桌機／手機幾何、無控制區溢出；腳本亦驗證 GSM 選埠不 open、連線／中斷及 LTE 表格 |
| `node tools/tests/test_system_status_ui.cjs` | PASS；33 項、query token／CRC／delta／未知碼、boot／offline／timeout、firmware gate、固定狀態／SCH 區塊及 responsive |
| `node tools/tests/test_et100_transfer_ui.cjs` | PASS；Candidate／Sensor ACK 進度、INST 備份／安裝／還原、SKIP／缺 START／terminal、流動端點與 mobile layout |
| `node tools/tests/test_et100_ui.cjs` | PASS；模擬自體 OTA／Sensor 存 NOR、套件比對／下載、relay gate／start／cancel／success／metadata／EMPTY、NOR→Sensor 圖與 AT 欄位寬度 |
| `python3 fw/tools/check_plan_vs_code.py` | PASS；3 個 CHECK 錨點相符 |
| `git diff --check` | PASS |

UI 測試使用 mock 回覆與既有 fixture／產物，沒有重新 build 本版 App；測試通過不代表燒入／運行了 v0.9.2，也不作本版 firmware hash 或射頻接收率證據。原 firmware 測試／M0 模型仍見 [23 報告](23_gatt_stop_release.md)，本輪不重寫其歷史量測。

## 3. 文件／程式一致性

- 逐項對照 `gatt_client.c` 的 accepted-stop、END sequence、fallback／quiet 與連線入口；`ota_worker.c ACTIVE_MS=30000`、serial `SMP_IDLE_MS=30000` 保持原值。一般 session 未新增 connected scan。
- 搜尋平台、協定、測試與工具 Markdown 的 CONN_END／quiet／Dash／GSM／SMP 30 秒描述。歷史測試報告保留當時語境；缺漏補到現行 GATT／SYS、平台計畫與本報告，不把舊報告改寫成新實測。
- 已補 UI 描述不是新周邊能力：型號／SCH 是設計參考，PDF 為產品主板、非 EVB。NOR 新版／舊版備份沒有可靠查詢來源時維持「—」，不冒用 GATT metadata。
- 沒有 Kconfig／預設值改動；原 v0.9.2 有 firmware GATT 生命周期變更，故補平台與通用協定；UI 有操作行為改變，但 payload／UUID／分區／容量不變。README 建置步驟與 comm 流程不改；Revision History 作本次 changelog。

## 4. 邊界與狀態

本版一般 GATT 連線仍停掃，SMP 主動回報保護仍 30 秒，SYS 非 9600 時仍暫停。未帶入 v0.9.3 的 connected scan、3 秒回報緩衝、半包週期性 expiry 或新 TX 仲裁。

本輪為 Codex 文件補同步與 host 回歸，不冒充獨立 Claude UI review；既有 firmware Claude review 不延伸涵蓋 `4c67d9d` 的後續 UI。實機 UART、GATT／RF、Sensor OTA 與 GSM 選埠的真實設備驗收仍由 Stanley 負責；文件 merge 不代表硬體驗收通過。
