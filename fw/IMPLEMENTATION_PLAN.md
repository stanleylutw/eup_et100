# IMPLEMENTATION_PLAN — v0.9.2 GATT 取消連線後正確釋放（D9）

> 狀態：**已核准，交 Codex 實作**（2026-10-23）。v0.9.1 施工單保留於 commit `29d30b8`（`git show 29d30b8:fw/IMPLEMENTATION_PLAN.md`）。
> 分工（comm.md 標準規則）：Claude 寫施工單／review，**Codex 實作**，**Stanley 實機測試**。

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.9.2_gatt_stop_release

Base branch: v0.9.1_relay_handoff（29d30b8）

版號依據：只改 `gatt_client.c` 的連線取消完成判斷與診斷文字尾端欄位，協定格式不變（CF reason 仍以 `disconnect_drain_timeout` 開頭），屬 patch。

## 0. 證據與根因

**M0 實機證據（Stanley，2026-10-23，DAP 唯讀，連續 5 次一致；`/tmp/et100_m0_readonly_20261023.json`，板上 App 與本機映像相符）**：

```text
state=6(DRAIN)  initiating=1  connected=0  connection_end_seen=1  connection_end_status=0
gap_conn_stop() 回傳 0（每秒重試）  reserved=1  unreserve_pending=1
```

RS232 診斷同步顯示 `disconnect_drain_timeout:i=1,c=0,s=0,stop=0,disc=-1` 持續 3 分鐘以上，期間無任何 MASTER_CONNECT。

**根因（確認，H-B1）**：`gap_conn_stop()` 取消成功後 SDK 送出 **status 0** 的 `GAP_EVT_CONN_END`；`et100_gatt_gap()` 只在 `conn_end_status != 0 || connected` 時清除 `initiating`（`fw/src/gatt_client.c` `GAP_EVT_CONN_END` 分支，原註解：成功的 action 可能先於 MASTER_CONNECT）→ `release_owner()` 永遠失敗 → ownership 不釋放、`SENSOROTA` 永遠 BUSY，需重開機。

**仍未證實**：快照不能證明該 status 0 事件是哪一次 `gap_conn_stop()` 產生（或為原 `gap_conn_start()` 的結束）；因此修法必須包含**延遲連線防護**。

**不需修正（澄清）**：先前懷疑 job 在 15 s 清理後仍停在 CONNECTING。依事件時序回推，清理約自 21:45:27.9 開始，15 s 上限約 21:45:42.9 結束，與 21:45:43.2 的取消幾乎同時（取消優先 → CANCELLED）；屬設計內行為，不列入本版。

## 1. 決策

| # | 決策 |
|---|---|
| D9a | 新增旗標 `stop_requested`：`gap_conn_stop()` 回 `NO_ERR` 時設為 1（僅在 `initiating` 時呼叫，現行）。收到 `GAP_EVT_CONN_END` 時，**若 `stop_requested`，不論 status 一律清除 `initiating`** 並清 `stop_requested`；未曾 stop 的 status 0 維持現行規則（保留 owner，等待 MASTER_CONNECT） |
| D9b | 後備：`stop_requested` 且 `!connected`，2 s 內沒有 `CONN_END` 也沒有 MASTER_CONNECT → 清除 `initiating`（M0 顯示事件會到，此為防 SDK 漏送） |
| D9c | **延遲連線防護**：因取消而釋放（D9a／D9b）後設定 **2 s quiet 期**；quiet 期內 `et100_gatt_exclusive()`、`et100_gatt_reserve()`、auto-connect 一律不開始新 session（`exclusive` 回 `-EBUSY`、reserve 回 `-EBUSY`）。quiet 期內若收到 MASTER_CONNECT：沿用現行「IDLE 收到連線 → DRAIN 斷線」路徑處理，**不得被下一個 session 接收** |
| D9d | 診斷：`disconnect_drain_timeout` reason 尾端追加 `,e=<connection_end_seen>,st=<connection_end_status>,sr=<stop_requested>`（前綴不變，06 協定已規定前綴比對） |
| D9e | 不改：15 s 清理上限、背景重試、`unreserve` 延後、BUSY／HTML 行為、`ota_worker` 交接規則 |
| D11 | 清理永久失敗的自動復原：本版**不做**（D9 後預期極少發生），仍列 Stanley 待決 |

## 2. 檔案範圍

| 檔案 | 內容 |
|---|---|
| `fw/src/gatt_client.c` | D9a–D9d：`stop_requested`、CONN_END 處理、2 s 後備、quiet 期、診斷欄位；`stop_requested` 於 `gap_conn_start()` 前與釋放時清除 |
| `fw/tests/test_gatt_client.c` | §3 Host 1–7 |
| `fw/tests/test_sensor_ota.c`（或既有 job 層測試） | §3 Host 8（job 層重現 M0 情境） |
| `fw/targets/evb_serial.mk` | 版本 `0.9.2-dev` |
| `fw/docs/04_testing/23_gatt_stop_release.md`（新） | host／build 證據、M0 摘要、Stanley 實機步驟（不預填 PASS） |

**不做**：SDK 修改、協定格式變更、HTML 修改（現有前綴比對已相容）、D11。

## 3. 驗收

**Host（fake SDK 須能重現 M0：`gap_conn_stop()` 回 0、隨後 CONN_END status 0、無 MASTER_CONNECT）**：

1. connect timeout → stop OK → CONN_END status 0 → `initiating` 清除 → `release_owner()` 成功 → IDLE（重現 M0 並證明已修）。
2. stop OK → CONN_END status ≠ 0 → 同樣釋放（回歸）。
3. **未呼叫 stop** 時收到 status 0 的 CONN_END → `initiating` 保留；隨後 MASTER_CONNECT → 正常連線（現行「action 先於 connect」行為不變）。
4. stop OK → 2 s 內無 CONN_END、無連線 → D9b 後備清除並釋放。
5. 釋放後 quiet 期內：`et100_gatt_exclusive()`／`reserve()` 回 `-EBUSY`；quiet 期過後可開始。
6. quiet 期內遲到的 MASTER_CONNECT → 進入 DRAIN 並斷線，**不**成為下一個 session 的連線；斷線後回 IDLE。
7. 診斷字串含 `e=1,st=0,sr=1` 並維持 `disconnect_drain_timeout` 前綴。
8. Job 層：M0 情境下 relay job 結束為 `FAILED／CONNECT_FAILED`（或取消時 `CANCELLED`），且 GATT 回 IDLE、`reserved`／`unreserve_pending` 清除後，新的 `SENSOROTA` 可再次開始（quiet 期後）。

另跑既有 `test_relay_handoff.sh`、`test_evb_serial_host.sh`、`test_sensorflow_nano.sh`、`tools/tests/*.cjs`，全部 PASS。

**EVB（Stanley）**：Sensor 關機／遠離造成 connect timeout → job 結束為 `FAILED／CONNECT_FAILED`，**數秒內** GATT 回 IDLE（不再出現持續的 `disconnect_drain_timeout`）；Sensor 開機後可直接再次開始 GATT OTA 並完成。

## 4. 交付

Codex 完成後停下，交 Claude Step 5 review；不 commit、不燒錄。Step 7 由 Claude 更新 06 relay 協定（第 7 點：取消連線後正常釋放、診斷新欄位）、21 分析文件（D9 已處理）、平台計畫、狀態文件。
