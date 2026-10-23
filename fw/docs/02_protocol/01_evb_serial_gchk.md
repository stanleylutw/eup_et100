# DEV1.4 UART3 — Gateway 查詢、CID 與 BLE 回報子集

Version: v0.8.10
日期：2026-10-21
狀態：GCHK、基本 BLE scan／Type 1、CID 切換對照與 CID／GTIME 手動 RST 後保存已由 Stanley 驗證；詳細證據及限制見測試紀錄

## 用途與接線

0.7.0-dev 新增手機 App GATT Server，名稱 EUP_AIoT_HUB_FR，見 [App GATT 規格](03_evb_app_gatt_server.md)。原 FFF0 demo service 已被 manager service 取代；UART／sensor GATT Client 行為保留，版本辨識更新如下。

0.6.0-dev 新增 Gateway-style GATT client，命令、Type 8 與限制見 [GATT 規格](02_evb_gatt_client.md)。此前 0.5.0-dev 的 scan／CID 實測證據保留；新 GATT 硬體驗收尚待執行。

HTML tester 為 command initiator，DEV1.4 為 responder。使用同一 DAPLink VCOM，PB5 為 MCU TX、PB4 為 MCU RX；四個 DEBUG jumper 保留。UART3：9600 bps、8 data bits、no parity、1 stop bit、no flow control。這條路徑驗證 UART 協定，尚未驗證 ET-100 UM3221 RS232 電平轉換電路。

這個 target 的 GCC `_write` 丟棄 vendor printf；UART3 僅由 serial task 傳送 response。ROM reset 階段仍可能輸出其固有 baud 的資料，應在 app 啟動後清空 HTML log 再測。原 SDK BLE example target 仍為 921600，不因本 target 改變。

## GCHK

請求必須為 printable ASCII，以 CRLF 結尾，token 必須是 4 個 ASCII 英數字元：

```text
BLEGW$GCHK;A1b2\r\n
AT$PD=BLEGW$GCHK;1;<identity>;Freqchip;DEV1.4_fr3068ep-c;0.7.0-dev;<state>;A1b2\r\n
```

- `identity`：依 SDK `app_btdm_init()` 相同演算法讀 unique ID 並建立 random static BLE identity，以 MSB-first 12 位 uppercase hex 顯示。不是 LTE IMEI；並非已驗證的工廠唯一性保證。
- `Freqchip` 為 EVB MCU vendor；板型明確標示 FR3068EP-C，不冒用量產 E-C 或 nRF 板型。
- `0.7.0-dev` 是 App GATT Server 版開發識別，非 release tag。
- `state=0` 為 IDLE，1 為準備／認證／清理，2 為已認證 READY。
- token 原樣回傳，包含大小寫。Query response 依 Gateway 現行實作，不含 XOR checksum；本版沒有 Type 0 heartbeat 或其他主動回報。

合法 `BLEGW$<CMD>;...;<token>` 的未支援命令、或 GCHK 多餘參數，回覆 `AT$PD=BLEGW$<CMD>;0;<token>\r\n`。不產生設定成功或 OTA handshake。命令名稱限制 1-24 個 uppercase 英數字元。prefix、token、CRLF 不合法或含 binary bytes 的行會丟棄。

## GTIME 查詢與設定（0.4.0-dev）

```text
BLEGW$GGTIME;Q001\r\n
AT$PD=BLEGW$GGTIME;1;15;Q001\r\n
BLEGW$SGTIME;30;S001\r\n
AT$PD=BLEGW$SGTIME;1;30;S001\r\n
BLEGW$GGTIME;Q002\r\n
AT$PD=BLEGW$GGTIME;1;30;Q002\r\n
```

值為十進位整數 1-1800 秒，預設 15。拒絕 0、1801、負數、加號、空白、小數、非數字與多餘參數。SGTIME 回覆保留原參數字串與 token；非法值或儲存失敗回覆 `AT$PD=BLEGW$SGTIME;0;<原參數>;<token>`。缺少 seconds 時原參數為空字串。設定失敗不更新目前 RAM 值。GGTIME 多餘參數沿用一般 malformed-query 失敗回覆。

FlashDB 沿用 SDK `FlashEnv`：internal Flash offset `0xC8000`（800 KiB）、長度 12 KiB；與目前映像無重疊，build script 會檢查映像末端。user key `0x00030002` 與 SDK random seed／BT keys 分開。record 為 8 bytes：`45 54 01 00 <seconds LE16> <seconds 兩個 bytes 的 bitwise complement>`；schema 1。

FlashDB init 失敗時 GTIME query/set 皆回覆 valid=0；query 仍列出 default 15。缺少、不合法或不支援 schema 的 record 使用 default 15，但不在開機時自行改寫；下一次有效 SGTIME 可保存。保存經 `flashdb_set` 成功且讀回完全一致，才更新 RAM 並回覆 1；相同 durable record 不重寫以減少磨耗。若 Flash 操作中途出錯，保留最後確認的 RAM 值，但不保證下次 boot 一定取回舊值。突然斷電及 FlashDB GC／故障恢復仍待硬體測試。

0.4.0-dev 的 GTIME 只保存設定；從 0.5.0-dev 開始，GTIME 控制 Type 1 snapshot 周期，成功改值後從當下重新計時。v0.8.10 新增 MAXRPT，要求 GTIME ≥ MAXRPT；GTIME 範圍仍為 1–1800 秒。

### v0.8.10 白名單與回報限制

- `BLEGW$GLIST;<token>` 回 `AT$PD=BLEGW$GLIST;<valid>;<MAC1>;…;<MAC8>;<token>`。
- `BLEGW$SLIST;<MAC1>;…;<MAC8>;<token>` 設定最多八筆顯示順序 MAC（12 位 hex），回應八個正規化大寫欄位；空欄／全零表示空位，尾端空位可省略，多餘欄位拒絕。
- `BLEGW$GMAXRPT;<token>` 回 `AT$PD=BLEGW$GMAXRPT;<valid>;<count>;<token>`。
- `BLEGW$SMAXRPT;<count>;<token>` 回 `AT$PD=BLEGW$SMAXRPT;<valid>;<count>;<token>`。
- 必須符合 `非空白名單筆數 ≤ MAXRPT ≤ min(GTIME,15)`，MAXRPT 最小 1、預設 3；舊 GTIME 小於 3 時預設配合降至 GTIME。
- 與 Gateway 相同：CID 或 MAC 命中即可；兩份清單都空才不過濾。匹配 CID 的 payload 優先於僅匹配 MAC 的 payload。
- 設定經 FlashDB `0x00030007` 保存為 102-byte 版本／互補值記錄，寫入及讀回成功才更新執行中設定；設定與 BLE callback 原子同步。舊 key 不變；損毀或不存在則使用預設。
- HTML 沿用既有指令控制；新增三類獨立選檔記憶、ET-100 build 下拉讀檔及中文記憶體圖。圖表自行等比例縮放，不更動共用 Log 高度。
- Host／ARM／DFU／瀏覽器模擬測試通過；白名單與 MAXRPT 的 EVB 保存／過濾仍待 Stanley 驗收。

## GCID／SCID（0.5.0-dev）

```text
BLEGW$GCID;C001\r\n
AT$PD=BLEGW$GCID;1;0639;;;;C001\r\n
BLEGW$SCID;0639;0dfc;;;C002\r\n
AT$PD=BLEGW$SCID;1;0639;0DFC;;;C002\r\n
BLEGW$SCID;;;;;C003\r\n
AT$PD=BLEGW$SCID;1;;;;;C003\r\n
```

SCID 固定四個欄位；每欄為空或恰好四碼 hex，回覆正規化為 uppercase。空欄與 0000 為 unused；四欄全空時不過濾 CID，但仍需要有效的 Manufacturer／Service Data。預設 `0639;;;`。設定 0639 比對廣播內 little-endian bytes `39 06`；0DFC 比對 `FC 0D`。MAC whitelist 未實作。

FlashEnv key `0x00030003`，與既有 GTIME key 分離。record schema 1：`45 43 01 00` + 四個 LE16 CID（8 bytes）+ 該 8 bytes 的 bitwise complement，共 20 bytes。init 失敗時 query/set valid=0；record 缺失／不合法時用 default，不在 boot 自動寫入。寫入與讀回一致後才更新 RAM，重複相同 record 不重寫。成功 SCID 清空 cache 與排隊的舊 filter 結果，重新計時；保留 GTIME。

儲存失敗回覆 valid=0 並列出嘗試設定的正規化 CID；格式不合法時回覆 valid=0 與四個空欄。兩者均保留原 RAM 設定。這個錯誤欄位政策與 Gateway 對某些不合法輸入的部分 echo 不完全相同；有效請求格式一致。全部命令回帶原 token，不含 checksum。

## BLE scan 與 Type 1（0.5.0-dev）

沿用 SDK central example 的 general discovery active scan，LE 1M、interval 40 × 0.625 ms = 25 ms、window 30 × 0.625 ms = 18.75 ms、duration 0、duplicate filter 關閉。SDK 對 observer mode 有額外 host 過濾註解，故未採 observer。原 peripheral 廣播成功後啟動掃描；並行能力與實際 scan event 須由 EVB 驗收。

僅接收 legacy advertising，丟棄 SCAN_RSP／extended／periodic reports。AD structures 必須通過長度檢查。找出符合 filter 的第一個 0xFF payload，否則使用符合的第一個 0x16 payload；payload 至少含兩個 ID bytes。原始 payload byte order 不變；同一 ADV 若含第一個非空 0x08／0x09 name，最多附加 12 bytes name 的完整 AD structure，總 payload 上限 31 bytes（末端 name 可被截斷）。

```text
AT$PD=G1;1;<MAC>;<hex_payload>;<RSSI>*<XOR>\r\n
AT$PD=G1;1;AABBCCDDEEFF;FFFF010AB80B01EA0011;0050*3F\r\n
```

MAC 是 link-layer advertiser address，MSB-first uppercase；不是從 sensor payload 取出。RSSI 格式為符號位（負 0、非負 1）＋三碼幅值，例如 -50 為 0050、+20 為 1020。RSSI 不在 -127..+20 者丟棄。XOR 範圍為 `AT$PD=` 後、`*` 前的 ASCII，輸出兩碼 uppercase hex。

- Cache 32 entries，以 address type＋MAC 辨識，覆蓋最新 payload／RSSI；滿時淘汰最久未見者。
- 每 GTIME snapshot 依 RSSI 強至弱選出最多 min(GTIME,15) 筆；超出者丟棄 pending flag。
- UART 同一 task 逐筆送出，相鄰 Type 1 至少一秒；query response 可插在 Type 1 之間，不會插進單一 frame。
- 沒有新符合封包便不重送舊值。若 sensor 每 30 秒才廣播，設 GTIME=5 不會每 5 秒捏造新資料。
- EUP v1／v2 的長度、version nibble、packet-info offset 沿用 Gateway parser 判斷；同 MAC 已 snapshot 的相同 version＋packet_info 在 1500 ms 內只更新 RSSI/last-seen，避免五包 burst 跨周期重複。未採用 Gateway 的 realtime XYZ batch。
- CID 更新清空 cache／batch；GTIME 更新取消舊 batch、保留 pending cache並重設周期。host 應等待回覆再送下一命令。

0.5.0-dev 不自動重試掃描；0.6.0-dev 在 IDLE／WAIT_ADV 驅動恢復，至少一秒 backoff、pending 三秒逾時。可經 SWD 查看 `et100_scan_status`（FF 未要求、FE 等待、FD 同步錯誤、0 成功、0x200|status 非同步啟動錯誤、0x100|status 結束）與 reports／accepted counters。這些不是新 UART 欄位。GATT 期間掃描暫停，owner 釋放後恢復；並行及所有 sensor 類型仍待實測。

## 接收與資源

ISR 只把 byte 放入 512-byte ring（可存 511 bytes），task 每 10 ms 空閒輪詢並處理完整行。serial task stack 為 1024 words（4 KiB，由 RTOS heap 配置），供 FlashDB 呼叫使用；stack high-water 待驗證。每行最多 127 個 printable bytes；超長或 ring overflow 時丟棄至下一個 LF，再恢復解析。0.6.0-dev 先處理一輪 RX，再送 bounded GATT queue 與 Type 1；GATT dispatch 不阻塞等待 queue。最終 UART transmit 仍為 blocking，大量命令或 Flash erase/GC 仍可能超過 ring，host 應逐筆送命令並等待對應 token。新增 queue／GATT stack 資源見 GATT 規格。

未啟用原範例 AT# memory read/write 指令。BLE peripheral profile 保留於專案自有 source，但此新 target 的 BLE regression 需要另驗。未提供 baud runtime 切換；目前可保存 GTIME、CID 與 GATT auto list，session 僅在 RAM。

## Build 與測試

```bash
bash fw/tools/build_evb_serial.sh 2>&1 | tee /tmp/et100_serial_build.log
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DET100_SERIAL_HOST_TEST -Ifw/src fw/src/serial_gchk.c \
  fw/tests/test_serial_gchk.c -o /tmp/et100_serial_test
/tmp/et100_serial_test
```

獨立產物：`fw/build_serial/output/Project_burn.bin`。燒錄時需明確指定 `--image`，原工具 default 仍指向 921600 baseline。須取得 Stanley 燒錄授權後執行：

```bash
python3 fw/tools/evb/fr3068_flash.py --image fw/build_serial/output/Project_burn.bin
```

HTML 使用本 repo 的 `tools/rs232_tester.html`，一般指令為 9600；SMP 上傳可切 115200。v0.8.10 支援 GLIST／SLIST、GMAXRPT／SMAXRPT；GATT、App SMP 與 Sensor relay 分別見其協定文件。ET-100 不使用 nRF Gateway 的更新方式。

Stanley 已使用 HTML 確認 GCHK 與 5 個未支援命令的失敗回覆，見 [實測紀錄](../04_testing/02_evb_serial_gchk.md)。擴充驗收仍包含：GCHK token 100 次一致、長時間輸出無 vendor log、reset 後可重連、BLE 原有廣播／連線不退化。Host tests 不代替這些硬體結果。依 Stanley 指示，UART／HTML 後續測試由 Stanley 操作，Codex 未執行自動 UART script。

GTIME 實測見 [保存紀錄](../04_testing/03_evb_gtime_settings.md)；最新 CID／scan 驗收見 [測試紀錄](../04_testing/04_evb_cid_ble_scan.md)。

## 來源

- `eup_gateway/docs/02_protocol/03_rs232_protocol.md` v3.3：9600/8N1、frame/token 定義。
- `eup_gateway/src/rs232/rs232_cmd_handler.c`：GCHK 欄位次序。
- `eup_gateway/src/rs232/rs232_engine.c`：query response 只加 CRLF。
- `eup_gateway/tools/rs232_tester.html`：9600 連線、parseGchkResponse 與自動 query。
