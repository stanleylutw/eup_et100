# v0.9.3 Part B — SMP 結束後恢復主動回報

日期：2026-10-24。分支：`v0.9.3_gatt_scan_resume`，base `a04e0d6`。
Stanley 授權同分支補施工單與 Step 4 實作。本報告不是獨立 review；未操作 EVB／UART／SWD、未燒錄、未 commit。

## 1. 規則與邊界

- UART 每個 SMP byte、BLE 每個 SMP VALUE write，以及 worker submit 都記錄活動。ASCII 查詢不延長 SMP 緩衝。
- UART 半行／半包、BLE 半包、worker queued／processing／整包 response sending 時暫停 Beacon Type-1 與 SYS 主動回報。回覆送完後再靜默 3000 ms；成功、錯誤或 sender 失敗返回都適用。
- 新請求重新延長暫停；沒有新請求時，2999 ms 仍暫停，3000 ms 可恢復。若收包／處理／傳送仍忙碌，不得因計時到期而恢復。
- `response_in_flight` 跨越整包 UART 多行編碼／BLE notifications，而不只跨一行 TX mutex；最後活動時間在 sender 返回後更新。sender 返回不代表 PC 已確認收到。
- M1 修正後，SYS 與 Beacon 各自經過「非阻塞取得 TX mutex → 檢查最新 gate → 產生並送出最多一行 → 釋放 mutex」。不沿用前一行的暫停快照；新增 UART RX ring／overflow 檢查。仲裁取得的一行允許完成，後續行必須重新檢查。
- TX 忙碌或 gate 暫停時不消耗 producer 狀態，保留待送 boot／SYS／Beacon；SYS 暫停時仍保留 full-snapshot coalescing。正常命令／SMP sender 沿用原有阻塞 TX mutex；主動回報 producer 使用已持鎖的 writer，避免重複取得同一把鎖。不在 critical section 內傳送 UART。
- Serial task 每輪呼叫既有 serial decoder expiry，並輪詢 BLE reassembly expiry；無下一個 byte／write 也能清除半包。UART 200 ms 半行／2 s 半包與 BLE 2 s 半包門檻不變。異常半包的 3 秒從最後收到資料計算，不額外疊加 3 秒。
- SYS 不再因 baud 非 9600 額外停報；與 Type-1 共用回報門檻，使用目前 baud。SYS 原有 ASCII 輸入未完成、Sensor relay busy 等其他限制保留。
- OTABAUD 仍依 UART SMP 最後活動後 30 秒回 9600，沒有縮短為 3 秒；NOR upload hold／relay handoff 的 v0.9.1 條件不改。回覆傳送旗標只用於回報保護，不加入 NOR 接管判斷。
- 沒有為此呼叫停掃描、沒有更動 Part A 一般 GATT 的 connected scan profile。UART 停報不等於 BLE 停掃，也不能宣稱改善 BLE 射頻負載。
- 3 秒代表解除暫停；個別 Beacon 出現仍受 GTIME／MAXRPT／CID／MAC／RSSI 排程影響，沒有新 Beacon 不捏造資料。SYS 保留暫停後補完整快照的 coalescing 行為，即使狀態未變仍可能送 Type=10；不是新增狀態變更。此敘述於 Step 7 對照程式釐清，未改既有測試數字。
- 沒改 HTML、SDK、Installer、UUID、線上格式、FlashDB 或其他 repo。

## 2. 本輪驗證

下列命令 PASS：

```bash
bash fw/tools/build_evb_serial.sh
bash fw/tools/test_evb_serial_host.sh
bash fw/tests/test_relay_handoff.sh
bash fw/tools/test_inst_status_host.sh
git diff --check
```

- Serial host suite 使用 ASan／UBSan，包含新增真實 BLE SMP adapter 測試、真實 serial decoder 的週期性半包逾時與 wrap，以及既有 GATT／scan／system status／SMP manager 回歸。
- BLE adapter：第一段／每段即記活動、完整請求才 submit、無下一包的週期性 expiry、斷線清除、tick wrap、notification 分段、送出拒絕與中途斷線。
- Worker fixture：serial／BLE 排隊超過 3 秒仍暫停、處理超過 3 秒仍暫停；使用真實 serial encoder 產生多行回覆，每行模擬超過 3 秒仍不得解除；完整回覆後核對 2999／3000 ms 邊界。
- 覆蓋錯誤回覆、無回覆、連續 20 筆唯讀請求與 timer wrap；NOR 接管仍依既有規則，唯讀活動不延長未完成上傳的 hold。
- M1 新增 production 共用仲裁＋真實 SYS／reporter 回歸：取得 TX 時注入 SMP、SYS 傳送時注入 SMP、TX busy、RX pending、half packet 均能延後下一筆回報，不消耗待送狀態；解除後 boot／full SYS snapshot／Type-1 正常送出。
- 既有 Installer 687 個 power-cut 案例等回歸通過。ARM build 僅有既有 RWX linker warning；SDK 的 `program target with file ...` 是 post-process 提示，不代表燒錄。
- 本輪未重跑 nano formatter 專用測試，其 formatter 程式碼未修改；Part A 的通過紀錄另見 `24_gatt_scan_resume.md`。

Host fake 不證明第三方工具與實體 UART／BLE 的相容性；下列項目仍待 Stanley 實機驗收。

## 3. 最新產物指紋

App `0.9.3-dev`，包含 Part A＋Part B；此表取代 Part A 報告的歷史產物作為本輪待驗收映像。

| 檔案 | Bytes | SHA-256 |
|---|---:|---|
| `fw/build_serial/output/Project.bin` | 321464 | `a7c740794bf4b356f94f4464d376a4994dcca0591fa4ff57164bcdbda9d45413` |
| `fw/build_serial/output/App_mcuboot.bin` | 322016 | `997ed74cc2ae30467fbd4f2320886ce95f688a22fbb7d83ab1f0c3cc03c5810d` |
| `fw/build_serial/output/dfu_application.zip` | 199922 | `4f5a9a539e161ce1409307f16f960616ae307f15312690b3f056687880c15cb5` |

沿用 hash-only prototype 映像，檔名不代表已有數位簽章驗證。Installer 不需為本變更重建／重刷。

## 4. Stanley 實機驗收（待執行）

1. 以 HTML 及 `fw/tools/evb/smp_serial_ota.py`（smpclient）分別執行 115200 Sensor 套件連續上傳，同時準備符合過濾條件的 Beacon。封包間隔小於 3 秒時，不應插入主動 Type-1／SYS 行；最終回覆送完後約 3 秒解除暫停。
2. 回報恢復時間以 GTIME 設為 1 秒、適當 MAXRPT 的單一 Beacon 觀察，避免排程造成誤判；SYS 在解除暫停後可依節流補 Type=10 完整快照，不把它誤當新的狀態變更。
3. 上傳一半拔線／終止工具，重新連線時確認半包可逾時清除、可讀取 GCHK、Beacon 回報可恢復，並可續傳。不要為本測試刪除已上傳資料。
4. 上傳完成後持續每秒 SMP 查詢：主動回報應保持暫停；停止查詢後 3 秒解除。改用 ASCII 查詢則不應延長 SMP 緩衝。
5. 保持 serial 115200，確認停止 SMP 後 3 秒可在目前 baud 接收主動行，30 秒 fallback 前不提早改 baud；最後 UART SMP 後 30 秒仍按既有規則回 9600。HTML 主動切回 baud 的流程另行對照，不與裝置 fallback 混淆。
6. BLE SMP 上傳與半包中斷同樣驗證；UART 不應在 worker 回覆分段期間恢復。完成後驗證 Sensor relay 可立即依既有狀態接管，不被回報專用 3 秒緩衝拒絕。
7. 重跑 Part A 一般 GATT 連線＋其他 Beacon 掃描、exclusive Sensor OTA 與取消清理。保留完整 UART log 與最終韌體版本，供後續 Step 5 review。

## 5. M1 修正後 Codex 重新 review

2026-10-24，Stanley 授權修正並重新 review；本節是 Codex 自我複查，不冒充獨立 Claude review。

- 前輪 Major M1：SYS 傳送期間有新 SMP 排入，但舊 `reports_paused` 快照仍允許送 Beacon。已改為每筆回報取得 TX 後各自檢查，並有 `test_serial_report_tx.c` 的事件交錯回歸。
- 重查 producer 最多一行、TX mutex 不重入、等待失敗不消耗回報、critical section 不做 UART I/O、SMP queued／reply gate 與 NOR ownership 獨立，以及原本 30 秒 OTABAUD 條件未改。
- 修正後重跑正常 App build、完整 serial host ASan／UBSan、relay handoff 與 diff check 均 PASS；未發現其他可確認的 Critical／Major。
- 先前 Part A 的 Minor 測試缺口仍存在：GATT session 測試使用假的 scan adapter，尚未與真實 scan 狀態機做完整整合測試。M1 測試是共用仲裁＋真實 SYS／reporter 的 host fixture，不是整個 UART RTOS task／中斷排程的硬體證明。
- EVB 射頻並行、115200 第三方 client、實體 UART 時序與 OTABAUD 30 秒驗收仍待 Stanley。沒有進行硬體操作、commit、merge 或 push。
