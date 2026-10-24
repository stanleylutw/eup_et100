# IMPLEMENTATION_PLAN — v0.9.2 Step 7 文件補同步

## Branch

2026-10-24 Stanley 明確要求回到既有 `v0.9.2_gatt_stop_release`（base `aa9d5d6`），補 Step 7、commit、merge main，再返回 `v0.9.3_gatt_scan_resume`。不另開分支、不 push、不操作 EVB。原 firmware 施工單保留於 `4c67d9d`。

## 範圍

僅修改 `docs/00_project/11_project_bring_up_status.md`、`fw/docs/00_project/00_et100_firmware_platform_plan.md`、`fw/docs/02_protocol/02_evb_gatt_client.md`、`fw/docs/02_protocol/08_system_status.md`、`fw/docs/04_testing/26_v092_step7_backfill.md`、`fw/IMPLEMENTATION_PLAN.md`、`fw/REVIEW_REPORT.md`。

- GATT 協定補上 v0.9.2 已實作的 accepted-stop／CONN_END 任意 status、2 秒後備與 quiet、診斷欄位；保留未取消零 status 的原始限制與已知 retry Minor。
- 平台與狀態文件記錄 `4c67d9d` 的 Dash 預設頁／圖示導覽、系統與 Gateway 唯讀資訊、33 項狀態與 SCH PDF、GSM 選埠／連線分離及 Flash UI 一致性。不改 wire format。
- 重跑該提交的五組 mock browser suites，結果集中在 26 報告，不當硬體證據；常數 checker、完整性搜尋與 diff check 另驗。
- 不帶入 v0.9.3 connected scan、3 秒 SMP 回報或新 TX 仲裁；v0.9.2 原 SMP 30 秒／非 9600 SYS 暫停規則保持不變。SDK／Installer／HTML／source／test 不修改。
- 先 commit 完整文件與工作紀錄，再依 comm.md 用另一筆 commit 將 task plan／review 還原成可追溯佔位符；最後 `--no-ff` merge main，返回 v0.9.3，不自動合併 main 回 v0.9.3。

## Step 7 檢查

- Kconfig／預設值：沒有，本次純文件；原 firmware 與 UI 沒有新增 Kconfig。
- Firmware 行為：原 v0.9.2 有取消連線與 timeout／quiet 改動，故補 GATT 章節；本輪不改 code。
- Payload／GATT／OTA：原版 GATT 生命周期及 UI 有變更，需文件同步；線上格式與 UUID 不變。
- README 建置／安裝與 comm 流程不變，不改；無獨立 Changelog，版本增補寫入平台 Revision History。
