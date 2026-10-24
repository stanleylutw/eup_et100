# DEV1.4 Gateway-style GATT Client

Version: v0.3
日期：2026-10-24；現行韌體：0.9.3-dev（命令表保留 0.6 baseline）
狀態：host tests／ARM build 通過；真實 sensor GATT 驗收待 Stanley 操作。

## 範圍

本節描述 v0.6 baseline：UART3 為 9600/8N1、ASCII CRLF 與四字元 token，支援單一 central session、手動／自動連線、EUP AES challenge 認證、metadata 讀取與設定。當時沒有 Sensor OTA、MCUmgr、package staging、GLIST 或 MAXRPT。現行 v0.8.10 的 GLIST／SLIST、GMAXRPT／SMAXRPT 見 [serial 協定](01_evb_serial_gchk.md)，後續 OTA 見各專用協定；本節不是現行功能缺口清單。SDK 新增 patch 0；採專案 source 與 build overlay。

## 命令

下列請求均需加上 `;<token>\r\n`，MAC 為 MSB-first 12 位 hex。

| 請求 | 行為 |
|---|---|
| `BLEGW$STYPESWITCH;1;<MAC>` | HTML 格式；亦接受 `1:<MAC>`。IDLE 才接受；等待目標 connectable ADV，再以其 address type 連線 |
| `BLEGW$STYPESWITCH;0;` | HTML 格式（保留空 MAC 欄位）；亦接受 `0`。要求取消／斷線；ACK 不代表已完成，須等待 DC 與 GTYPESWITCH=0 |
| `BLEGW$GTYPESWITCH` | IDLE 回 0；其餘回 `1:<MAC>`，包括清理中 |
| `BLEGW$SAUTOGATT;<MAC>;1` | 將 MAC 加入持久化自動連線清單，上限 8 筆 |
| `BLEGW$SAUTOGATT;<MAC>;0` | 移除該 MAC；不取消既有 session |
| `BLEGW$GAUTOLIST` | 回傳已保存 MAC，以分號分隔 |
| `BLEGW$GSENSORINFO;<MAC>` | 讀最後一個目標的 metadata cache，不是即時在線檢查 |
| `BLEGW$GSENSORCMD;<MAC>;<CMD>` | READY 才接受；CMD 為兩碼 hex，例如 01 或 7F |
| `BLEGW$SSENSORCFG;<MAC>;<TAG>;<VALUE_HEX>` | READY 才接受；組成 command 12 的單一 TLV，value 1-30 bytes |

一般回覆為 `AT$PD=BLEGW$<CMD>;<valid>;<fields>;<token>\r\n`，無 XOR。GCHK 版本 0.6.0-dev，state 0=IDLE、1=準備／認證／清理、2=已認證 READY。

STYPESWITCH 回覆正規化為 `...;1;0;<token>` 或 `...;1;1:<UPPERCASE_MAC>;<token>`，不回帶取消請求的空 MAC 欄位。多餘欄位／非空的取消 MAC 欄位不接受。2026-10-21 首版只接受 `0`／`1:<MAC>`，因此 HTML 的 `0;;token` 回 valid=0 而未執行斷線；本次修正此相容性問題，不改 HTML 或 SDK。

GSENSORINFO fields 為 `<MAC>;<auto>;<firmware>;<battery_hex>;<capabilities_hex>;<age_seconds>`；沒有資料回 valid=0。只有最近目標一份 RAM cache，不是 Gateway 的八個 metadata cache。

GSENSORCMD fields 為 `<MAC>;<CMD>;<sensor_status>;<payload_hex>`；大於 64 bytes 時分段加 `;pN/M`，最多接收 320 bytes。valid=1 表示收到有效協定回覆，不保證 sensor_status=00。已接受命令的逾時回 valid=0、原 MAC/CMD 與空 payload；未接受／格式錯／忙碌回 valid=0、`no_session`。SSENSORCFG 保留原參數，只有 sensor status=00 才 valid=1。設定的 tag/value 必須依 sensor 規格，不自行定義。

## GATT 與認證

- Service `E9E00000-4F4C-4F47-4943-455550000001`；RX suffix 0001、Write Command；TX suffix 0002、Notify。要求 MTU 247，實際至少 35，並完成 discovery／CCCD。
- 以 command 10 取得 tag 01／16-byte nonce，沿用 Gateway auth helper 與 TinyCrypt AES 算 token，再以 command 11 送 tag 01／16-byte token。認證材料不輸出 log。
- 只有 sensor 回覆 status=00 才發 AP。自 MASTER_CONNECT 起，包含 discovery 在內總認證期限 3 秒；本版單次認證，不含完整 Gateway retry policy。
- 認證後依序 best-effort 讀 command 01-08 metadata；使用者命令優先於下一筆自動讀取。成功解析的即時 01-08 回覆也會更新 cache。
- 手動等待 ADV 上限 90 秒，匹配後單次連線上限 10 秒。最後成功交換 30 秒後 idle timeout；HTML 15 秒 7F PING 可維持 session，韌體不自行 PING。

## Type 8 與生命週期

`AT$PD=G1;8;<event>;<MAC>...*<XOR>\r\n`；事件 WS／CS／AS／AP／AF／TO／CF／DC。MR 為 `G1;8;MR;<MAC>;<CMD>;<status>;<hex>[;pN/M]`。XOR 與 Type 1 相同，涵蓋 `AT$PD=` 之後、星號之前。只有 AP 代表應用層認證完成，UART ACK／BLE link 均不等於 AP。

單一 RAM owner 依序經 WAIT_ADV／CONNECTING／SETUP／AUTH／READY／DRAIN；連線前確認掃描停止。取消後保留 owner，直到 initiating／link／scan-stop 均確認結束，才排入 DC 並釋放。晚到 MASTER 立即清理；舊 generation 的 AP/MR 不再送出。取消／斷線每秒重試；超過六秒仍未確認會回 CF，但不假裝 IDLE。

未取消連線時，SDK 的零 status CONN_END 可能早於 MASTER，不能直接視為 initiating 已消失。**v0.9.2 修正**：`gap_conn_stop()` 回成功後，CONN_END 不論 status 都完成取消；SDK 呼叫內同步送達的 END 以 sequence counter 處理。已接受 stop 但無 END 時，滿 2 秒且未連線也可清 initiating；重複 stop 成功不重設計時。

取消 owner 釋放後有 2 秒 quiet，阻擋 relay／reserve／auto／手動新連線；遲來 MASTER 不交給新 session，而沿 DRAIN 清理。未成功取消的零 status 不套用上述釋放捷徑。drain reason 原前綴不變，新增 `e=<conn_end_seen>,st=<conn_end_status>,sr=<stop_requested>`；wire event 格式／UUID 不改。實機時序與重啟驗收仍由 Stanley 執行，見 [23 報告](../04_testing/23_gatt_stop_release.md)。

已知 Minor：relay retry backoff 1200 ms 短於 quiet 2000 ms，quiet 內的 `-EBUSY` 可能消耗一次 attempt；原 review 接受延後修正，本版沒有修改 job retry，也沒有新增 BLE stack／整機自動 reset。

### 掃描生命週期（v0.9.3）

成功 ADV_START 後才允許 scanner。IDLE／WAIT_ADV 要求 interval 40／window 30；一般手動／auto GATT 認證完成、非 exclusive READY 才要求 connected profile：interval 40／window 10（25 ms／6.25 ms），維持 general discovery、LE 1M、duration=0、duplicate filtering 關閉。exclusive Sensor relay／probe 不啟用 connected scan。cleanup 切回 normal，原本自動工作冷卻五秒及取消 quiet 不變。

- 每次一般 session 最多送一筆 connected start；API 拒絕、非零 START 或中途 END 後，本 session 不自動重試。掃描失敗不輸出 CF、不主動斷開一般 GATT。
- SDK API 回 0 只表示接受請求；收到成功 START 才算 ACTIVE。profile 切換先 stop 並等 END，pending 期間不重複排入操作。
- START 等待三秒無事件，先嘗試 stop 同步，不直接重送 start；stop 拒絕最多每秒重試。stop 接受但三秒無 END 時保留不確定的 pending 並等待事件，不盲目重啟；這不阻塞 GATT ownership 清理，也沒有新增 stack／整機 reset。
- `et100_scan_status`：FF 未要求、FE 等 START、FD 同步啟動拒絕、0 事件確認 active、`0x200|status` START 失敗、`0x100|status` END；FC 等 STOP、FA 缺 END、F9 stop API 拒絕、FB 為 start timeout 進入同步前的暫態。`et100_scan_diag` 提供 phase、desired／issued／active window、計數與最近錯誤，皆為 RAM 診斷，不改線上格式。

其他 Beacon 的接收與 Type-1 仍依 CID／MAC／GTIME／MAXRPT，不取代目前 GATT owner；SMP 的 UART 停報規則另見 [04](04_smp_serial_ota.md)。Host fake／ARM build 已驗證程式策略，Controller 射頻並行、接收率與 GATT 延遲尚待 Stanley 實機驗收，見 [24](../04_testing/24_gatt_scan_resume.md)。

## 保存與資源

FlashEnv 新 key `0x00030004`，100 bytes：`45 41 01 00`＋八個 MSB-first MAC（48 bytes）＋該 48 bytes 的反碼。寫入／讀回一致才更新 RAM；同值不重寫。保留 GTIME 30002／CID 30003；session 不保存。開機載入清單後，匹配 ADV 可自動連線。

GATT task stack 1536 words（6 KiB）、serial stack 1024 words（4 KiB），另有 16×264 bytes 輸出 queue，皆消耗既有 RTOS heap。worker 保留四個 queue slots 給 UART 回覆；UART dispatch 不阻塞等 queue。嚴重滿載可丟回覆並進入安全清理，host 必須逐筆等待 token，不可無限制 burst。實機 heap／stack high-water 尚未量測。

SDK callback 的 handle／msg_len 依 headers 與 mock 驗證，不能代替真實 BLE stack 時序、sensor 認證與設定驗收。測試見 [GATT 紀錄](../04_testing/05_evb_gatt_client.md)，第三方來源見 [TinyCrypt](../../third_party/tinycrypt/README.md)。
