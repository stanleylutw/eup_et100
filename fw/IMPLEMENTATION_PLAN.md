# IMPLEMENTATION_PLAN — v0.9.1 Sensor relay：上傳後立即交接＋連線失敗清理

> 狀態：**已核准，交 Codex 實作**（2026-10-23）。v0.9.0 施工單保留於 commit `2455a03`（`git show 2455a03:fw/IMPLEMENTATION_PLAN.md`）。v0.9.0 Step 5 由 Stanley 判定 PASS（2026-10-23，未經 Claude 逐項 review）。
> 分工（comm.md 標準規則）：Claude 寫施工單／review，**Codex 實作**，**Stanley 實機測試**。

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.9.1_relay_handoff

Base branch: v0.9.0_system_status（2455a03）

版號依據：只改 relay 啟動判斷、GATT 清理與 HTML 顯示，**不改任何協定格式**（沿用既有 state／error 名稱，不新增），屬 patch。

範圍：**Part A** 上傳完成後 BUSY（問題 1）、**Part B** 連線逾時後永遠 CONNECTING（問題 2）、**Part C** HTML 顯示。交接分析見 [21 分析](docs/04_testing/21_sensor_ota_handoff_analysis.md)。

## 0. Part A — 上傳完成後 BUSY：問題與決策

**現象**（Stanley 2026-10-23）：RS232 SMP 把 Sensor 套件存入 NOR 成功後，立即啟動 GATT OTA（relay）顯示失敗。

**已確認事實（Claude 讀碼）**：

| # | 事實 | 位置 |
|---|---|---|
| F1 | `sensor_ota_job_start()` 最後呼叫 `ota_worker_relay_begin()`，失敗回 `-EBUSY` → UART `SENSOROTA;0;<code>;BUSY;tok` | `fw/src/sensor_ota_job.c` `sensor_ota_job_start()` |
| F2 | `ota_worker_relay_begin()` 條件為 `!delete_queued && !ota_worker_active()` | `fw/src/ota_worker.c` |
| F3 | `ota_worker_active()` = 30 s 內有**任何** SMP 請求（含 state 查詢、echo）；每個請求處理完都重設 `last_activity` | `fw/src/ota_worker.c` |
| F4 | 套件驗證成功後 `sensor_finish()` 設 `m->session=false`；上傳新套件時記錄先變 INVALID；relay 只接受 VALID | `fw/src/ota_manager.c`、`sensor_ota_job_start()` |
| F5 | relay 進行中 `claim()` 對所有變更型 SMP 請求與刪除回 `SMP_EBUSY`（`m->relay`） | `fw/src/ota_manager.c` `claim()` |
| F6 | NOR 讀寫（含 relay 的 package read）全部經 OTA worker 排隊 | `fw/src/ota_worker.c` `read_request` |
| F7 | HTML 收到 `SENSOROTA;0;…;BUSY` 時設 `state=FAILED`、`PACKAGE_CHECK=FAIL`，顯示「啟動 Sensor OTA 失敗」 | `tools/rs232_tester.html`（約 L5540–5550） |
| F8 | `ota_worker_active()` 另用於：SMP 期間暫停 Type-1 主動上報、`OTABAUD` 30 s 回 9600 的判斷（serial task） | `fw/src/serial_gchk.c` |

**結論**：30 s 計時在 relay 判斷中是多餘的保護；資料一致性已由 F4／F5／F6 的**狀態**保證。

**決策**：

| # | 決策 |
|---|---|
| D1 | relay 啟動改為**狀態判斷**：worker 沒有待處理／處理中的 SMP 請求、沒有排隊中的刪除、OTA manager **沒有未完成且仍在保護期內的上傳 session** → 立即允許 |
| D2 | 未完成的上傳 session（含 ET-100 自身更新 image 0）在最後一次變更請求後 `OTA_OWNER_HOLD_MS`（30 s）內仍拒絕（BUSY）；套件驗證成功（session 結束）**不再等待** |
| D3 | `ota_worker_active()` 的其他用途（F8）**不變** |
| D4 | 協定不變：`SENSOROTA` 失敗仍回 `BUSY`，不新增細分原因 |
| D5 | HTML：`BUSY` 不是失敗。顯示「裝置忙碌，稍後自動重試」，`PACKAGE_CHECK` 維持 PENDING；每 2 s 自動重送 `SENSOROTA`，最多 30 s，仍 BUSY 才顯示「裝置忙碌，請稍後再試」（不寫成套件錯誤） |

## 0B. Part B — 連線逾時後永遠 CONNECTING：問題與決策

**現象**（21 分析 §2）：`CF connect_timeout` → 6 s 後 `CF disconnect_drain_timeout` → `GSENSOROTA`／`GSENSOROTAFLOW` 持續回 `CONNECTING`，直到使用者斷線。

**已確認事實（Claude 讀碼）**：

| # | 事實 | 位置 |
|---|---|---|
| G1 | `session()` 連線等待 ≤ 10 s；失敗 `CF connect_timeout` → `cleanup` | `fw/src/gatt_client.c` `session()` |
| G2 | `cleanup` 為**無期限** `for(;;)`：只有 `release_owner()` 成功才離開；每秒呼叫 `gap_conn_stop()`／`gap_disconnect()`，**忽略回傳值**；6 s 只送一次 `disconnect_drain_timeout` | 同上 |
| G3 | `release_owner()` 需 `!initiating && !connected && !scan_stop_pending` 且 DC 行成功放入 output queue | `release_owner()` |
| G4 | `GAP_EVT_CONN_END`：只有 `conn_end_status != 0 || connected` 才清 `initiating`（註解：成功的 action 可能先於 MASTER_CONNECT） | `et100_gatt_gap()` |
| G5 | `et100_gatt_exclusive()` 在呼叫端 task 同步跑 `session()`；cleanup 不結束 → `run_job()` 無法 `finish_job()` → job／workflow 停在 CONNECTING | `gatt_client.c`、`sensor_ota_job.c` `run_job()` |
| G6 | 協定已有 `FAILED` state 與 `CONNECT_FAILED` error（可沿用，不需新增） | `sensor_ota_job.c` name 表、06 relay 協定 |
| G7 | SDK `gap_api.h` 未說明 `gap_conn_stop()` 取消成功時 `GAP_EVT_CONN_END` 的 status 值 | SDK header |

**根因假設（未證實，需 M0 證據）**：

- **H-B1（優先）**：`gap_conn_stop()` 取消成功時 SDK 送出 `CONN_END` status 0；依 G4 `initiating` 不會被清除 → `release_owner()` 永遠失敗。
- H-B2：`gap_conn_stop()` 回錯誤（例如已不在 initiating）而未產生任何事件，`initiating` 永遠為 1。
- H-B3：`scan_stop_pending` 或 output queue 滿阻擋 `release_owner()`。
- H-B4：延遲到達的 `MASTER_CONNECT` 使 `connected=1`，而 `gap_disconnect()` 失敗。

**決策**：

| # | 決策 |
|---|---|
| D6 | **M0 先取證**（Stanley，實機，唯讀）：重現卡住時以 SWD 讀 `gatt_client.c` 靜態變數 `initiating`、`connected`、`scan_stop_pending`、`connection_end_seen`、`connection_end_status`、`state`，判定 H-B1～B4。Codex 在 21 報告附讀取指令（`arm-none-eabi-nm` 取位址、pyocd 唯讀讀取） |
| D7 | **結果與清理分離**：連線失敗時**立即**讓 job 得到結果（沿用既有重試；最後一次仍失敗 → `FAILED`／`CONNECT_FAILED`），不再等 cleanup 結束。資源（GATT owner、`reserved`）**仍由 gatt_client 保留**直到確認釋放；期間新的 `SENSOROTA` 回既有 `BUSY` |
| D8 | **cleanup 有總期限**：`session()` 內 cleanup 最多 15 s；逾時則 session 回傳（job 得到失敗結果），gatt_client 維持非 IDLE（DRAIN），由既有 poll 路徑繼續嘗試 `release_owner()`，**不強制清除** `initiating`／`connected` |
| D9 | **依 M0 證據修正清除條件**：若證實 H-B1，`gap_conn_stop()` 回 `NO_ERR` 後收到 status 0 的 `CONN_END` 且 200 ms 內無 `MASTER_CONNECT`，才清 `initiating`；延遲到達的 `MASTER_CONNECT` 必須以目前 target／generation 比對，不得被下一個 session 接收。M0 未取得前，只實作 D7／D8／D10，D9 列待辦 |
| D10 | 記錄 `gap_conn_stop()`／`gap_disconnect()` 回傳值；`disconnect_drain_timeout` 之後每 10 s 至多一次附帶旗標的診斷事件（例如 `CF;…;drain:i=1,c=0,s=0`）**只在 G1 Type 8 的 reason 文字尾端**附加，HTML 以前綴比對原因 |
| D11 | **永久清理失敗的復原**（自動 BLE reset／整機 reset）**不在本版**；列 Stanley 決策。本版行為：保持 BUSY，SYS（v0.9.0）BLE 項回 ERROR，需人工重開機 |

## 1. 設計（Part A）

跨 task 一致性：`ota_worker_relay_begin()` 在 serial task 執行，OTA manager 欄位由 worker task 修改。**不直接跨 task 讀 manager 內部欄位**，改由 worker 發布一個狀態：

```text
worker 每處理完一個 SMP 請求（以及 delete／sensor_finish 之後）：
    taskENTER_CRITICAL
    upload_hold_until = ota_manager_upload_open(&ota) ? now + OTA_OWNER_HOLD_MS : 0
    taskEXIT_CRITICAL
worker 取出 slot 開始處理前：smp_in_flight = true；處理完並發布後：smp_in_flight = false

ota_worker_relay_begin()（critical section 內）：
    允許 ⇔ !delete_queued && !smp_in_flight && 沒有 slot.full
           && (upload_hold_until == 0 || now >= upload_hold_until)
    允許時 ota_manager_set_relay(true)（現有）
```

- `ota_manager_upload_open(m)`（新增，manager 內部）：`m->session` 為真即回 true（ET-100 image 0 上傳完成後 session 仍保留，維持 D2 行為；Sensor 套件 `sensor_finish()` 成功後為 false）。
- 計時用 tick 差值比較，處理 wrap（同 `claim()` 寫法）。
- relay 結束（`ota_worker_relay_end()`）行為不變。

## 1B. 設計（Part B）

```text
session():  ... connect fail → cleanup
cleanup:    最多 15 s 嘗試 release_owner()（記錄 stop/disconnect 回傳值）
            成功 → state=IDLE（現行）
            逾時 → 保留 state=DRAIN 與 owner，session 回傳 -ENOTCONN（不清旗標）
poll 路徑：  state==DRAIN 且無 session 執行中 → 定期 release_owner()（含 stop/disconnect 重試，1 s 間隔）
run_job():  拿到失敗即依既有重試／finish_job(FAILED, CONNECT_FAILED)，釋放 relay NOR lock
新 SENSOROTA：et100_gatt_state()!=0 → 既有 BUSY（HTML 依 D5 顯示忙碌並重試）
```

## 2. 檔案範圍

| 檔案 | 內容 |
|---|---|
| `fw/src/ota_worker.c／.h` | `smp_in_flight`、`upload_hold_until` 發布；`ota_worker_relay_begin()` 改用 §1 條件；`ota_worker_active()` 不改 |
| `fw/src/ota_manager.c／.h` | 新增 `ota_manager_upload_open()`（唯讀） |
| `fw/tests/`（`test_smp.c` 或新 `test_relay_handoff.c`） | host 測試：見 §3 |
| `tools/rs232_tester.html` | D5：BUSY 自動重試與文案；啟動失敗時清除樂觀設定的 ACTIVE 步驟；`CONNECT_FAILED` 顯示「連線失敗」；CF 事件只套用到目前 OTA 的 target／session；收到 drain 診斷顯示「BLE 清理中」 |
| `tools/tests/test_et100_ui.cjs`（或新測試） | mock：BUSY→重試→成功；持續 BUSY 30 s→「裝置忙碌」且 PACKAGE_CHECK 不是 FAIL |
| `fw/src/gatt_client.c` | Part B：cleanup 總期限、DRAIN 背景重試、回傳值記錄與診斷事件（D8／D10）；D9 依 M0 證據 |
| `fw/src/sensor_ota_job.c` | Part B：連線失敗後 job 正常結束為 `FAILED`／`CONNECT_FAILED`，釋放 relay lock |
| `fw/tests/test_gatt_client*.c`（或既有 gatt host test） | Part B host 測試：見 §3 |
| `fw/targets/evb_serial.mk` | 版本 `0.9.1-dev` |
| `fw/docs/04_testing/22_relay_handoff.md`（新） | host／build 證據與 Stanley 實機步驟（不預填 PASS） |

**不做**：協定格式變更、OTABAUD／Type-1 暫停的 30 s 邏輯、SDK／linker／installer。

## 3. 驗收

**Host（必須）**：

1. Sensor 套件最後一包驗證成功後，**同一 tick** 呼叫 `ota_worker_relay_begin()` → 允許。
2. 驗證成功後再送 SMP state 查詢（非變更型）→ 仍允許（不再因查詢重設 30 s）。
3. 上傳進行中（session 開啟、最後變更 < 30 s）→ 拒絕；逾 30 s 未再上傳 → 允許，且因記錄 INVALID，`sensor_ota_job_start()` 回套件錯誤而非 BUSY。
4. worker 有待處理 slot 或正在處理 SMP 請求 → 拒絕。
5. 刪除排隊中 → 拒絕。
6. ET-100 image 0 上傳後 30 s 內 → 拒絕；之後允許。
7. relay 進行中：SMP upload／test／confirm／delete 皆 `SMP_EBUSY`（既有行為回歸）。
8. `ota_worker_active()` 行為不變（Type-1 暫停、OTABAUD 回 9600 的既有測試 PASS）。

**Host（Part B）**：

9. connect timeout＋正常清理（CONN_END status≠0）→ job 結束 FAILED／CONNECT_FAILED，gatt 回 IDLE。
10. `gap_conn_stop()` 回錯誤、CONN_END 缺失 → 15 s 後 session 回傳、job FAILED；gatt 保持 DRAIN，新 SENSOROTA 回 BUSY；之後事件到達 → 背景釋放 → IDLE。
11. 延遲 `MASTER_CONNECT`（cleanup 期間或之後）→ 被斷線處理，不被下一個 session 接收。
12. `scan_stop_pending` 未清、output queue 滿（backpressure）→ 不釋放、不誤報 IDLE。
13. 取消期間與清理期間重複啟動 → BUSY，不產生第二個 session。

**HTML**：mock BUSY→重試成功；持續 BUSY→「裝置忙碌」，`PACKAGE_CHECK` 非 FAIL；啟動失敗不殘留 ACTIVE；`CONNECT_FAILED` 顯示連線失敗；其他 target 的 CF 不套用。

**EVB（Stanley）**：
- M0：重現 connect timeout 卡住時先 SWD 讀旗標（D6），結果回報 Claude 決定 D9。
- RS232 上傳套件完成 → 立即按 GATT OTA → 直接開始；上傳中途按 → 顯示忙碌並自動重試。
- Sensor 關機（製造 connect timeout）→ 最終顯示 `FAILED／CONNECT_FAILED`，不永久 CONNECTING；Sensor 開機後可再次開始。

## 4. 交付

Codex 完成後停下，交 Claude Step 5 review；不 commit、不燒錄。Step 7 由 Claude 更新 06 relay 協定（補「上傳完成後可立即啟動」、連線失敗結果與清理中行為）、平台計畫、狀態文件、21 分析文件（標記已處理項與 D9／D11 待決）。
