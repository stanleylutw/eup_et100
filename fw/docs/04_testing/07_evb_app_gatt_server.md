# DEV1.4 App GATT Server 測試紀錄

日期：2026-10-21；Branch：v0.7.0_evb_app_gatt_server；版本：0.7.0-dev。
裝置名稱：**EUP_AIoT_HUB_FR**；UART3：9600/8N1。

## 變更前 Checkpoint

依 Stanley 要求，先將 sensor GATT Client／HTML disconnect 修正／D4 LED 提交為 `db4f2f7032904705a87f91bb55ed8035fb1d9e0d`，推至 origin/v0.6.0_evb_gatt_client，遠端 SHA 只讀核對一致。未推 main／tag／company。PCB 原始資料與混有既有編輯的硬體 README 未納入該 commit。

Stanley 明確同意新分支後開始實作；查到 App 對原定 EUP_AIoT_FR 的 AES 分類衝突後，停在燒錄前請示。Stanley 選擇改名 EUP_AIoT_HUB_FR、不改 App；本版依此決策完成，未修改 sibling repos 或 SDK，新增 SDK patch 0。

## Host Tests

```bash
bash fw/tools/test_evb_serial_host.sh
```

七組 native tests 均使用 `-Wall -Wextra -Werror -fsanitize=address,undefined` 通過，完整 log `/tmp/et100_app_gatt_host_tests.log`。

| 測試 | 驗證內容 |
|---|---|
| Serial | GCHK 0.7.0-dev、token／切包／overflow、GTIME／CID 保存與失敗 |
| BLE reporter | 既有 filter、payload／XOR、排程／節流／去重、5,000 AD fuzz |
| Scan lifecycle | 同步／非同步啟動錯誤、callback publication、backoff／timeout |
| Sensor GATT | discovery／CCCD／AES／command／config、HTML cancel、late link／owner drain／scan restore |
| LED | 待機／資料／sensor link 優先與時間 wrap |
| App manager | v5 清單／selected vector、LE byte order、八筆容量、選取／清除、五秒 TTL／tick=0／wrap、共用 AD metadata、address type 隔離 |
| App SDK adapter | 真實 SDK types、UUID／ADV 名稱與長度、CCCD／MTU、不截斷通知、read capacity／offset／blob 快照、同步 completion／send error backoff、App／sensor conidx 隔離、斷線重新廣播 |

測試初稿將 attribute count 誤寫 11，實際 service＋四組 declaration/value＋CCCD 共 10；修正 mock 期望後通過，未改 production table 來迎合錯誤測試。Host callbacks 為 mock，沒有證明真實 SDK 排程、iOS discovery、RF 並行或 heap 餘量。

## SDK 邊界查證

SDK headers／example 與只讀反組譯確認：server READ_REQ 的 msg_len 為 SDK 配置的容量，offset 為讀取起點，return 為實際長度；callback 中 bounded copy 後回長度。WRITE callback return 被 SDK 忽略，沒有對外 ATT error setter，所以非法 selection／CCCD 只忽略、不假稱會回 ATT error。`server_send_event` 先 copy payload 再 enqueue。未修改 SDK／lib／工具。

先前聲稱「App 只看 UUID 所以完全不需考慮名稱」不完整；App 確實以 UUID 啟用 manager，但在 discovery 前也以 EUP_AIoT 前綴／HUB 子字串分類 sensor AES。名稱改為含 HUB 才避免該衝突。App manager schema 實際以 `CoreBluetoothManager.swift` 四個 parser／selection writer 為準核對，尚非真實 App 通過證據。

## ARM Build／Image

`bash fw/tools/build_evb_serial.sh` 通過；完整 log `/tmp/et100_app_gatt_build.log`。既有 RWX LOAD segment warning 保留，未為此修改 SDK。

| 項目 | 數據 |
|---|---|
| ELF text/data/bss | 240,744／35,768／66,528 bytes |
| data+bss | 102,296 bytes，不是 runtime heap 可用量 |
| Project.bin | 276,520 bytes |
| Project_burn.bin | 284,712 bytes，含 8,192-byte header 區 |
| CRC32（raw） | 0xDC2F028F，獨立 zlib 比對 PASS |
| Burn SHA-256 | `effbd9aad13fb8c134ee9f39f475413e8287f78a7bab4e4f2e3007ef526921ec` |
| Header／payload | FR509x magic、TLV length=0、exec_offset=0x2000、length、payload 完全比對 PASS |
| FlashEnv 邊界 | 映像小於 offset 800 KiB；GTIME／CID／auto keys 不變 |
| 名稱／版本 | binary 含 EUP_AIoT_HUB_FR 與 0.7.0-dev |

沒有新增 RTOS task，App poll 沿用 serial task。新增 table／read snapshot 等 static RAM 配置；SDK 動態 service／connection／notify allocation、serial 4 KiB stack high-water、50 KiB RTOS heap 餘量均未實測。

## EVB 燒錄

依既有分工授權，已將本版 burn image 寫入 `0x08000000..0x08045827`（70 sectors，37.8 秒）；逐 byte readback PASS。SYSRESETREQ 後 resumed，PC=0x1FFE2030 位於 PRAM ram_code；這不是 App／UART 已啟動成功的證明。完整 log `/tmp/et100_app_gatt_flash.log`。Codex 未操作 UART／HTML／手機。新分支功能未 commit／push；先前 checkpoint 已推 origin。

## Stanley 實機驗收

1. 手機 App 掃描名稱 **EUP_AIoT_HUB_FR**，連線後應看到 manager status、sensor list／select 頁，不應啟動 sensor AES 失敗斷線流程。舊連線的 OS GATT cache 可能需斷開、重新掃描；必要時以 nRF Connect 核對四個 8D2F UUID。
2. 確認 sensor 廣播 CID 符合目前 SCID；App 列表應顯示 EUP v1/v2，最多八筆。未知格式可出現在 UART Type 1，但不列入 App manager。
3. 點選 sensor，讀所選 raw ADV、RSSI／FW／資料；開啟 UUID05 notify 後應更新。確認 MTU／完整資料不截斷，與 UART Type 1 的內容應可對照，但兩者封裝不同。
4. 停止 sensor 廣播超過五秒，清單應移除／清空 selection；恢復後可以重新選取。App 連線本身不應因此斷開。
5. 斷開 App，重新掃描可再次發現／連線；CCCD 不應跨新連線保留。再確認 UART GCHK=0.7.0-dev、GTIME／CID 保存、Type 1、sensor GATT connect／AP／disconnect／DC 未退化。
6. App 保持連線時由 HTML 對 sensor 連線，驗證雙角色（MCU peripheral＋central）是否可同時運作。Sensor GATT 期間 scanner 暫停，App table 過五秒消失是既定 TTL，不等於 App link 壞掉；斷開 sensor 後確認掃描回復。原廠 library 是否穩定支援所有並行情境仍須實測。
7. D3 app 啟動後熄滅；D4 保持前版邏輯，只依 sensor GATT link／廣播活動，不以 App link 代表 AES 成功。按鍵未修改。

請匯出 App／HTML logs，分別確認 service discovery、UUID 值、通知、重新廣播、取消／斷線及同時連線。Codex 不操作 UART／HTML／手機，不把 build 或 readback 當 App 驗收 PASS。

## Stanley 驗收回覆（2026-10-21）

燒入 EUP_AIoT_HUB_FR／0.7.0-dev 並提供 App 連線／清單／選取／Notify／重連步驟後，Stanley 回覆「yes, it works.」。據此記錄 App 基本連線與 manager 使用路徑獲使用者確認可用；未取得逐筆 App log，不對每個步驟獨立判為 PASS。App 與 sensor GATT 同時連線、長時間穩定性、heap／stack、通知壓力及所有錯誤路徑仍未驗收。此回覆不改變未 commit／push 新功能的狀態。

## 限制

本版是 Gateway manager wire-compatible 子集，不包含 App 透過 Gateway 連 sensor／改設定／OTA、安全 provisioning、MAC whitelist／MAXRPT、所有未知 payload。TTL 5 秒、通知 latest-only／上限 10 Hz；通知過載或 SDK 不回 completion 的復原、OS read-blob／MTU 實際行為需測。安全性沿用開發版設定，未強制加密／AES，不宣稱量產可用。未經獨立 reviewer，本次為自我檢查＋自動測試。
