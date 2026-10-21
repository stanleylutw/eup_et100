# EVB App GATT Manager

Version: v0.1
日期：2026-10-21；韌體：0.7.0-dev；名稱：**EUP_AIoT_HUB_FR**。

## 角色與相容性

手機 App 為 central，EVB 為 peripheral／GATT Server；同時保留 EVB 作 central 的 sensor GATT Client。App 的 sensor 選取只是查看已掃描資料，不會發起 sensor 連線／認證／設定，也不是 OTA relay。

原定 EUP_AIoT_FR 被現有 App 的 `isNormalSensorConnection` 規則誤判為 sensor（EUP_AIoT 開頭且無 HUB），會因缺少 EUP AES service 而斷線。Stanley 明確選擇 EUP_AIoT_HUB_FR，不修改 App。App discovery 依 manager service UUID 啟用管理頁，名稱含 HUB 可避開 sensor AES。這是 code-level 相容性檢查，實際 iOS／Android 尚待測試。

此版本取代 EVB 原 FFF0 demo service；原 vendor OTA service 保留但不宣稱可用，不屬本次測試／產品 OTA。UART3 仍 9600/8N1，既有 GTIME／CID／auto-GATT FlashDB keys 保留。GCHK 的 0／1／2 仍為 sensor session 狀態，不因 App 連線改變；D4 仍依 sensor link／符合廣播活動顯示，App 單獨連線不觸發 GATT 快閃。

## 廣播與 UUID

Legacy connectable、LE 1M、100 ms 廣播。ADV 內完整名稱 EUP_AIoT_HUB_FR（15 bytes；AD 共 17 bytes）；scan response 內完整 128-bit manager service UUID（AD 共 18 bytes），符合 SDK 28／31-byte 限制。GAP Device Name 回 15 bytes，不包含 NUL。需要 active scan 取得 service UUID；不承諾被動掃描僅憑 ADV service filter 能找到。

Service：`8D2F0001-9F8A-4F6F-9A7B-7D5F4F455550`。

| Characteristic（其餘 UUID 同上） | Properties | 格式 |
|---|---|---|
| 8D2F0002 | Read | Gateway status，16 bytes |
| 8D2F0003 | Read | Device list v5，4＋16×count bytes，最大 132 |
| 8D2F0004 | Write Request | 選取／清除，2／4／10 bytes |
| 8D2F0005 | Read＋Notify | Selected ADV，6 或 23＋raw_length，最大 54 bytes |

多 byte 整數為 little-endian；address 為 SDK／Zephyr wire little-endian 6 bytes，不是 UART 顯示的 MSB-first 字串。

## Binary 格式

Status：byte 0 version=1；1 mode=0（沿用 Gateway 管理介面）；2 count；3 selected index（未選 FF）；4-7 uptime seconds；8-11 list sequence；12-15 event sequence。uptime 依 32-bit RTOS tick，約 49.7 天 wrap，非永久累積時鐘。

List header：`05 <count> 10 <list_seq低8位>`。每筆 16 bytes：index、address type、AdvA[6]、signed RSSI、payload version、role mode、role state、FW major/minor/patch、flags。index 0-7，不保證八個 slots 永久對應同一 sensor。

選取寫入：`01 01 00 <index>`，或 `01 01 01 <addr_type> <AdvA_LE[6]>`。清除：`01 02`。選取不存在／逾時 slot、錯長度／版本／offset／operation 不改 selection。清單及選取只在 RAM，開機或成功 CID 變更會清空。

Selected 未選：`01 FF 00 00 00 00`。有選取：byte 0 version=1、1 index、2-5 entry sequence、6 RSSI、7 addr_type、8-13 AdvA_LE、14 legacy adv_type、15 status=FF unknown、16 battery=FF unknown、17 flags、18-21 last_seen_ms、22 raw_length、23 起為原始 ADV AD structures（最多 31 bytes）。不是 UART Type 1 的 manufacturer/service payload＋附加 name；App 沿用 AD parser。SDK 非連線不可掃描類型 08 映射為 Gateway／Bluetooth legacy type 03。

flags：bit0 valid、bit1 selected、bit2 timeout、bit6 identity ready；其餘校正／電量 flags 未推測填入。實際 battery／motion 可由 App 解析 raw ADV，FF header 不代表 sensor battery 無效。

## 資料來源與 TTL

App 與 UART 經同一 AD 邊界解析、同一四欄 CID/UUID filter，先 0xFF 再 0x16。Type 1 格式、GTIME、burst 去重／節流不變。App table 只列已辨識 EUP v1（19 bytes／version nibble 0）或 v2（16 bytes／nibble 1）的資料，不把其他符合 CID 的未知格式假裝成 EUP。一般 sensor 仍可出現在 UART Type 1 而不出現在 App 清單。

App table 最多八筆，UART cache 仍為 32。已滿且未逾時時不替換活躍 entry；超過最後 ADV 5000 ms 才 timeout／可重用，並清除該 selection。從 tick=0 收到的 entry 也會正常逾時。v1 可依非零 payload identity 找回同一 entry，v2 以 AdvA＋type 辨識，避免不同 address type 誤合併。清單保留 wire-compatible v5 格式，不複製 Gateway 的邊界 bug。

Sensor GATT owner 期間 scanner 依原設計暫停，超過五秒沒有新 ADV 的 App entry 會消失；這不是 App BLE link 斷線。Scanner 恢復收到新資料後 entry 可重建。

## Read／Notify／連線生命週期

- 單 App link；第二個 peripheral-role peer 會要求斷線。Sensor master-role link 不受影響。
- READ_REQ 使用 SDK msg_len 容量／offset，copy 不超出 buffer；offset=0 建立快照，後續同 attribute read-blob 使用同一份內容，避免掃描更新造成長讀撕裂。若中間讀其他 attribute，舊 blob 不再提供。
- 以 MTU exchange 要求 247；實際通知需 MTU≥3＋該筆完整 selected 長度。MTU 23 時不發截斷／無法解析的 selected value，仍可 read-blob。MTU 更新後重新嘗試完整通知。
- UUID05 CCCD 僅接受 0000／0001。最多一筆 in-flight notification；在 serial task poll 驅動，至少 100 ms 間隔，只送最新快照，不保證逐包送出。成功／失敗 completion 會釋放 pending；同步錯誤或 completion 錯誤可重試，SDK 永不回 completion 時會等到斷線重設，不無限制灌送。
- SDK 只讀反組譯確認 `server_send_event` 會複製 payload 後入 queue，沒有等待 callback；短 critical section 內發布 pending 並送出，避免 conidx 重用期間發到另一個 peer。實際 host heap、callback 時序與壓力仍待測。
- App 斷線清除 CCCD、MTU／read snapshot／pending，恢復廣播。Sensor 斷線不重啟 App 廣播，拒絕第二個 peer 的斷線也不干擾第一個 App。

## 限制與安全性

SDK 的 WRITE callback return 不控制 ATT status；無應用層錯誤回覆 API。不合法選取／CCCD 不改狀態並增加 `et100_app_invalid_writes`，但手機可能收到成功 ATT Write Response；需讀回 status／selected 驗證，不能聲稱 error path 完全等同 Gateway。`et100_app_notify_errors` 為 SWD 診斷 counter，不增加 UART 欄位。

沿用原 SDK Just Works security 設定，manager characteristics 未新增 AES／授權／強制 encryption。這是現場開發診斷介面，不宣稱產品安全 provisioning。App 不提供 sensor 操作、LTE、TCP、韌體更新入口；未安裝工具或修改 SDK／App／Gateway repo。

來源：Gateway `src/main.c`／`device_table.c`；App `CoreBluetoothManager.swift` 的 UUID、parseHubGatewayStatus／parseHubDeviceList／parseHubSelectedBeaconData／selectHubDevice。測試與真實驗收狀態見 [測試紀錄](../04_testing/07_evb_app_gatt_server.md)。
