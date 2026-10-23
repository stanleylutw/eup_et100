# REVIEW_REPORT — v0.9.2

v0.9.1 的 review 原文保留於 commit `29d30b8`（`git show 29d30b8:fw/REVIEW_REPORT.md`）。

## Step 5 Claude review（2026-10-23）— 結論：**PASS**（無 Critical／Major，1 Minor）

Branch `v0.9.2_gatt_stop_release`，base `29d30b8`。

**Claude 自行重跑**：`test_evb_serial_host.sh`、`fw/tests/test_relay_handoff.sh`、`test_sensorflow_nano.sh`、`test_inst_status_host.sh`、`tools/tests/*.cjs` 全部 PASS；App 獨立重 build PASS（僅既有 RWX warning）。

### 符合施工單

- **D9a**：`gap_conn_stop()` 回 `NO_ERR` 後設 `stop_requested`；`GAP_EVT_CONN_END` 在 `stop_requested` 時不論 status 清除 `initiating`。另以 `connection_end_sequence` 處理「CONN_END 在 SDK 呼叫內同步送達、早於回傳值」的情況。未下 stop 的 status 0 維持原規則。
- **D9b**：`drain_poll()` 中 `stop_requested && !connected` 滿 2 s 清除；重複的成功 stop 不重設計時（測試確認）。
- **D9c**：取消釋放後 2 s quiet 期，`et100_gatt_exclusive()`、`et100_gatt_reserve()`、auto-connect、手動 `et100_gatt_uart` 連線皆被擋（手動入口為施工單外的合理補強）；quiet 期內遲到的 MASTER_CONNECT 經既有 IDLE→DRAIN 路徑斷線。
- **D9d**：診斷 reason 尾端 `,e=,st=,sr=`，前綴不變。
- Host fake SDK 重現 M0（stop 回 0 → 之後 END(0) → 無 MASTER_CONNECT），並涵蓋同步 END、無事件後備、延遲連線、job 層 FAILED／CANCELLED → 釋放 → quiet 期 BUSY → 成功重啟。

### Minor

| # | 內容 | 建議 |
|---|---|---|
| m1 | relay job 連線失敗後的重試：`SENSOR_OTA_RETRY_BACKOFF_MS`（1.2 s）短於 quiet 期（2 s），下一次 `et100_gatt_exclusive()` 會回 `-EBUSY` 並消耗一次 attempt；實際有效連線嘗試由 3 次變 2 次（最終錯誤碼仍為最後一次真實連線的結果） | 後續小改：`run_job()` 遇 quiet 期 `-EBUSY` 時等待至 quiet 結束且不計 attempt，或 backoff 改為 ≥ 2 s。不阻擋本版 |

### 待辦（不屬本版）

- **D11**：清理永久失敗的自動復原（BLE／整機 reset）仍待 Stanley 決策（D9 後預期極少觸發）。
