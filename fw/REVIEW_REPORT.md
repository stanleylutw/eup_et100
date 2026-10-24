# REVIEW_REPORT — v0.9.3_gatt_scan_resume

日期：2026-10-24。程式 commit：`77ba471`，base：`a04e0d6`。
Stanley 授權 Codex 實作、修正後重新 review，並在同分支同步 Step 7 文件。本紀錄是 **Codex 自我複查，不是獨立 Claude review**。前版 Claude review 不視為涵蓋本版；v0.9.2 歷史仍可由 `git show 4c67d9d:fw/REVIEW_REPORT.md` 查閱。

## 範圍與結論

- Part A：一般已認證、非 exclusive GATT 的 connected scan、事件確認／profile 切換／RAM 診斷；exclusive OTA 不啟用。
- Part B：SMP 實際忙碌＋3 秒回報緩衝、UART／BLE 週期性半包 expiry；OTABAUD 30 秒與 NOR／relay ownership 不改。
- M1 修正後未發現其他可確認的 Critical／Major；保留 1 項 Minor 測試缺口。獨立 review 及 Stanley 實機驗收仍未完成，不能宣稱 RF 並行／第三方 serial client 已通過。

## Findings

| 等級／編號 | 狀態 | 內容 |
|---|---|---|
| Major M1 | 已修正並有回歸 | 原 serial loop 在送 SYS 後沿用舊 `reports_paused`，期間收到新 SMP 仍可能送 Beacon。`serial_report_tx.h` 固定每筆先非阻塞取得 TX mutex，再查最新 gate，通過才呼叫 producer；SYS／Beacon 各自仲裁，被延後者不先消耗狀態。已取得發送權的一行可完成，下一行重新檢查。 |
| Minor m1 | 保留 | GATT session 測試仍使用假的 scan adapter，尚未將真實 GATT 與真實 scan 狀態機串接測試完整連線／停掃／connected scan／斷線復原。既有分開測試與 host PASS 不能代替此整合或射頻驗收。 |

## 驗證與限制

- M1 修正後重跑正常 App build、完整 serial host ASan／UBSan、relay handoff、Installer host 與 `git diff --check` 均 PASS。ARM build 僅既有 RWX warning，不代表燒錄。
- `test_serial_report_tx.c` 使用 production 共用仲裁與真實 SYS／reporter，覆蓋取得 TX 時注入 SMP、SYS 傳送時注入 SMP、TX busy／RX pending／half packet、不消耗待送狀態及解除後送出；這不是完整 UART RTOS／IRQ 實機測試。
- Worker／BLE adapter 分開驗證長時間處理／多行回覆保護、2999／3000 ms 邊界、半包 expiry、斷線與 tick wrap；既有 relay／M0／quiet 回歸保留。
- 最新映像指紋、完整測試紀錄與 Stanley 步驟集中於 [24 掃描報告](docs/04_testing/24_gatt_scan_resume.md)、[25 回報恢復報告](docs/04_testing/25_smp_report_resume.md)，此處不複製量測或 hash。nano formatter 本輪未改，原 Part A 通過紀錄仍見 24。
- 未操作 EVB／UART／SWD，未證實 Controller 的 GATT＋scan 並行接收率、延遲或 115200 實體 UART 相容性。未新增自動 stack／整機 reset。

## Step 7

同分支文件同步至 v0.9.3：平台計畫／Revision History、bring-up 狀態與 serial／GATT／SMP／relay／SYS 協定。沒有新增量測、沒有改 code 或 wire format；獨立 review／實機驗收保持 pending。這批文件由 Stanley 後續另行 commit，不 merge／push，也不提前清空施工單與 review 歷史。
