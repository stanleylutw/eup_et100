# REVIEW_REPORT — v0.9.1

v0.9.0 的 review 原文保留於 commit `2455a03`（`git show 2455a03:fw/REVIEW_REPORT.md`）；v0.9.0 Step 5 由 Stanley 判定 PASS（2026-10-23，未經 Claude 逐項 review）。

## Step 5 Claude review（2026-10-23）— 結論：**PASS**（無 Critical／Major）

Branch `v0.9.1_relay_handoff`，base `2455a03`。

**Claude 自行重跑**：`fw/tests/test_relay_handoff.sh`、`test_evb_serial_host.sh`、`test_sensorflow_nano.sh`、`test_inst_status_host.sh`、`tools/tests/*.cjs` 全部 PASS；App 獨立重 build PASS（僅既有 RWX warning）。

### Part A（上傳後立即交接）— 符合 D1–D5

- `ota_worker_relay_begin()` 改看 `smp_in_flight`、所有 transport `slots[].full`、`delete_queued`、`ota.relay` 與發布的 `upload_open`／`upload_changed_ms`；保護期以 `ota.owner_ms`（只有變更型請求經 `claim()` 更新）計算，唯讀查詢不延長。
- `publish_upload()` 只在 worker task 讀 manager 內部欄位，跨 task 經 critical section 發布（符合 §1 設計）。
- `ota_worker_active()` 未改（Type-1 暫停／OTABAUD 回 9600 不受影響）。
- `run_job()`：上傳階段 `-ENOTCONN` 經 `upload_error_hint=CONNECT_FAILED` → `FAILED／CONNECT_FAILED`；驗證階段新增對應分支。

### Part B（連線失敗清理）— 符合 D7、D8、D10；D9 依指示保留 TODO

- cleanup 改為 15 s 上限＋GATT worker 在 `DRAIN && !session_running` 時背景 `drain_poll()`；**未強制清除** `initiating`／`connected`。
- 無併發風險：relay job 經 `idle_hook` 在 **GATT worker task 內**執行（`sensor_ota_job.c` 設定 `et100_gatt_set_idle_hook`），exclusive session 與 DRAIN 背景重試不會同時執行。
- `et100_gatt_unreserve()` 在 DRAIN 時延後到 `release_owner()` 成功才清除，DRAIN 期間 `reserve` 回 BUSY。
- 診斷事件附 `i/c/s/stop/disc` 並每 10 s 至多一次；SYS BLE 設 ERROR，釋放後由下一個 scan／adv／link 事件重新計算（非永久）。
- `test_gatt_client.c` 覆蓋：正常 timeout 清理、stop 失敗／事件缺失（DRAIN 保留、reserve BUSY、unreserve 延後）、診斷節流、D9 未改（status 0 仍保留 owner）、延遲 MASTER_CONNECT 先斷線再釋放。

### Part C（HTML）

- `SENSOROTA;0;…;BUSY`：PACKAGE_CHECK 維持 PENDING、顯示「裝置忙碌，稍後自動重試」，30 s 內重試。
- CF `connect_timeout`／`disconnect_drain_timeout` 以**前綴**比對，且只在 MAC 與目前 relay session 相符時套用。

### Minor（不阻擋）

| # | 內容 | 處理 |
|---|---|---|
| m1 | 測試報告編號與既有 `21_sensor_ota_handoff_analysis.md` 重複（施工單本身寫錯） | Step 7 已改名為 `22_relay_handoff.md` 並更新施工單引用 |
| m2 | CF reason 由 `disconnect_drain_timeout` 擴充為帶旗標字串；只做完全相等比對的舊工具會漏判 | 06 relay 協定已註明「請以前綴比對」 |
| m3 | `session()` 移除「進入時 DRAIN 直接 cleanup」；依賴 `et100_gatt_exclusive()` 只在 IDLE 時進入、worker 以 `drain_poll()` 處理 DRAIN | 可接受，已確認呼叫路徑 |
| m4 | cleanup 逾時（仍 DRAIN）時 job 不再做其餘重試 | 符合 D7 意圖（資源未釋放不得開新 session） |

### 待辦（不屬本版）

- **D9**：`initiating` 清除條件，待 Stanley M0 SWD 取證（讀 `initiating`、`connected`、`scan_stop_pending`、`connection_end_seen`、`connection_end_status`、`state`）。
- **D11**：清理永久失敗的自動復原（BLE／整機 reset）待 Stanley 決策。
