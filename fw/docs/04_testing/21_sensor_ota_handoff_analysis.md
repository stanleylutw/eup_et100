# 21 — Sensor NOR／GATT OTA 異常分析交接

日期：2026-10-23。分支：`v0.9.0_system_status`。

本文件供 Claude 分析及規劃修正；**不是修復完成報告**。

> **處理狀態（2026-10-23）**：§1 BUSY 交接與 HTML 誤標、§2 結果與清理分離／清理期限／診斷事件已於 **v0.9.1** 實作（見 [22 報告](22_relay_handoff.md)）。§2 `initiating` 清除條件（施工單 D9）待 Stanley M0 SWD 取證；清理永久失敗的自動復原（D11）待決策。Stanley 要求保存目前分支並提交；本輪沒有修改下述互斥、GATT 清理或 HTML 錯誤處理。沒有操作 EVB／UART／SWD，實機由 Stanley 負責。v0.9.0 SYS 與 UI 仍待獨立 review。

## 1. NOR 上傳成功後，啟動 GATT OTA 得到 BUSY

Stanley 提供的畫面：

```text
19:11:44.580 TX  BLEGW$SENSOROTA;EEACB6F5627E;CXRM
19:11:44.669 RX  AT$PD=BLEGW$SENSOROTA;0;4;BUSY;CXRM
```

HTML 同時顯示 PACKAGE_CHECK 失敗、`PACKAGE_INVALID`，WAIT_ADV 仍為活動狀態。

### 已確認

- `fw/src/ota_worker.c`：`ACTIVE_MS=30000`；每個 SMP 請求提交及處理完成都更新 `last_activity`，不限上傳封包。`ota_worker_relay_begin()` 在 `ota_worker_active()` 為真或刪除待處理時拒絕交接。
- `fw/src/sensor_ota_job.c`：`sensor_ota_job_start()` 也會因已有 job、pending start、GATT 非 IDLE、reserve 失敗而回 EBUSY。因此僅憑該畫面不能確認命中哪一個 BUSY 分支。
- 這 30 秒是軟體活動窗口，不是 NOR 寫入後的硬體等待需求。成功上傳後也沒有立即結束活動窗口。
- `tools/rs232_tester.html`：SENSOROTA 啟動拒絕分支無條件將 PACKAGE_CHECK 標為 FAIL；其說明固定為 PACKAGE_INVALID。BUSY 被錯誤顯示為套件無效。

### 請 Claude 評估

1. 將「SMP transport 最近有活動」與「NOR upload／驗證／排隊工作仍占用」分開；完成且無待處理請求時可安全交接，未完成／中斷上傳保留逾時保護。
2. 不能只將 ACTIVE_MS 改小或移除：需檢查 UART／BLE 兩種 SMP transport、worker slot、已接受但尚未處理的封包、delete、relay reservation，以及新上傳與 relay 啟動的競態。
3. 不要順便改動 serial 30 秒回到 9600、Type-1／SYS 暫停規則，除非施工單明確列入並說明必要性。
4. HTML BUSY 顯示「裝置忙碌，請稍後重試」，不得宣告套件損毀；啟動失敗也應清除樂觀設定的 ACTIVE 步驟，不保留互相矛盾的狀態。

## 2. 連線失敗後，OTA 一直回 CONNECTING

證據來源：Stanley 貼上的完整文字；本機原檔為 `/Users/stanley/.codex/attachments/04580141-bc50-4fcc-81b7-afc9717cbcf3/貼上的文字.txt`。關鍵摘錄：

```text
19:20:16.804 SOTA;STEP;WAIT_ADV;PASS
19:20:16.873 SOTA;STEP;CONNECTING;ACTIVE
19:20:16.977 G1;8;CS;EEACB6F5627E;
19:20:27.772 G1;8;CF;EEACB6F5627E;connect_timeout
19:20:28.288 GSENSOROTA;1;CONNECTING;EEACB6F5627E;9;0;167055;23837;0;NONE;5DQF
19:20:34.551 G1;8;CF;EEACB6F5627E;disconnect_drain_timeout
19:20:44.556 GSENSOROTA;1;CONNECTING;EEACB6F5627E;9;0;167055;38214;0;NONE;8ZT1
19:20:44.693 GSENSOROTAFLOW;1;CONNECTING;CONNECTING;ACTIVE;NONE;00000000;000000000052;4B7O
19:20:46.104 使用者中斷連線
```

### 已確認的程式路徑

1. `fw/src/gatt_client.c` 的 `session()` 連線等待最多 10 秒；失敗輸出 CF/connect_timeout，進入 cleanup，內部 state 改為 DRAIN。
2. cleanup 每秒嘗試 `gap_conn_stop()`／`gap_disconnect()`，目前忽略回傳值。超過 6 秒只輸出一次 CF/disconnect_drain_timeout，迴圈繼續，沒有總期限。
3. `release_owner()` 必須確認 `!initiating && !connected && !scan_stop_pending`，且 DC 成功入輸出 queue，才回 IDLE。
4. `et100_gatt_exclusive()` 同步呼叫 session，只有 cleanup 結束後才返回結果。`sensor_ota_job.c` 的 `run_job()` 在此之前無法完成重試判斷或 `finish_job()`，故 job／workflow 仍停在 CONNECTING／ACTIVE／NONE。
5. HTML 收到的兩種 OTA 查詢都仍是 CONNECTING，因此持續顯示連線中；不能只把 HTML 當成唯一根因。

### 尚未確認

- 哪個條件阻止 release_owner：initiating、connected、scan_stop_pending，或輸出 queue。
- SDK 取消操作的實際回傳值、GAP_EVT_CONN_END／DISCONNECT 是否遺失或狀態值不符預期。
- `connect_timeout` 不一定代表完整等滿 10 秒；程式也會因失敗的 connection-end 事件提早離開等待，現有文字未區分。
- 不能由此 log 證明 Sensor 韌體錯誤、NOR 套件損毀、SDK 缺陷、HardFault 或重開機。

### 請 Claude 評估的修正方向

- 將對外「操作結果」和內部「資源清理狀態」分離：連線失敗立即記錄真實原因／失敗步驟，清理仍保留 ownership；清理超時必須可觀察，不再永久宣稱 CONNECTING。
- 決定如何在現有協定相容性下表達清理中／清理超時；不要未經規格決策直接新增 state 或錯誤碼。需核對 `06_sensor_ota_relay.md`、workflow 表、HTML 狀態與舊工具。
- 檢查 GAP start/end、stop 回傳值與延遲 MASTER_CONNECT 的全部路徑，避免 late connection 被當成下一個 session。
- 不得為了讓畫面恢復就強制清掉 initiating／connected、釋放 NOR 或允許第二個 session；自動整機 reset／BLE reset 需另行決策。
- HTML 可顯示「連線逾時，清理中」及清理失敗，但只有確認資源釋放後才允許再次開始。CF 需與正確 target／目前工作對應，不可把其他 GATT session 的事件套到 OTA。

## 3. 建議測試與後續工作

Host 測試至少包含：成功上傳立即交接、未完成／中斷上傳拒絕、新 SMP 封包與 relay 啟動競態、雙 transport 排隊、delete 競態、BUSY 不誤標 PACKAGE_INVALID、connect timeout＋正常清理、stop 失敗／完成事件缺失、延遲連線成功、scan stop 未完成、輸出 queue backpressure、取消期間與清理期間重複啟動。

請 Claude 先確認根因範圍、協定相容性與 recovery 策略，再提出下一分支施工單；本次 commit 不表示已修以上問題，不宜把目前分支當已完成硬體驗收而直接釋出。Stanley 後續保留完整起始／上傳／切換／失敗 log，實機觀察由 Stanley 執行。

## 4. 本分支已實作內容與界線

- v0.9.0 SYS／GSYSSTAT、33 項周邊 RAM 狀態及 Dash 解碼，詳見 08 協定與 20 測試報告。
- ET-100／Device→RS232 NOR 與內建 Flash 圖、實際 ACK／INST 驅動的資料流、進度填色及階段標籤；GATT 資料流恢復 Bluetooth 圖示，RS232／內部搬移維持 FileText。
- 沒有實作本文件所列的 BUSY／錯誤映射／GATT cleanup 修復。
