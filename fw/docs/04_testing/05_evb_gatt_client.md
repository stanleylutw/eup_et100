# DEV1.4 GATT Client 測試紀錄

日期：2026-10-21；Branch：v0.6.0_evb_gatt_client；版本：0.6.0-dev

## 結果範圍

Host sanitizer tests、ARM build、header／CRC／payload／FlashEnv 邊界與 EVB program/readback/reset 通過。真實 sensor GATT 認證、讀取、設定與 scan 恢復尚待 Stanley HTML 驗收，不能視為完整 Gateway 等價。

## Host 與 review

- `test_gatt_client.c`：公開 AES 向量、真實 SDK types、MTU／discovery／CCCD、auth 成功／失敗、讀取／設定、取消／晚到連線、零 status drain、auto list reload／保存失敗、輸出順序／過期事件、UART 滿 queue 不阻塞 RX。
- `test_scan_lifecycle.c`：同步 callback publication、非同步／同步啟動失敗、backoff、pending timeout、restart。
- `test_serial_gchk.c`：token／切包／連包／overflow、GTIME／CID 邊界與保存失敗。
- `test_ble_reporter.c`：AD 邊界、CID/UUID、RSSI／XOR、排程／去重、clock wrap、5,000 AD fuzz。
- 皆以 `-Wall -Wextra -Werror -fsanitize=address,undefined` 執行通過。完整結果 `/tmp/et100_gatt_host_tests.log`。
- 獨立 reviewer 多輪指出的 lifecycle／cancel／scan restore／queue-order 問題已修正；最後無確認的 Critical／Major。Mock 不模擬真實 RTOS 排程、堆疊壓力或 radio 時序，不替代硬體驗收。auth helper 與 Gateway 原檔比對一致，不輸出認證材料。

GATT test 編譯方式：

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ifw/tests/gatt_fakes -Ifw/src -Ifw/third_party/tinycrypt/include \
  -Ifw/vendor/fr30xxc_sdk__202411/components/btdm/include \
  fw/tests/test_gatt_client.c fw/src/eup_aiot_uuid_client.c \
  fw/src/eup_aiot_metadata.c fw/src/eup_aiot_auth.c \
  fw/third_party/tinycrypt/source/aes_encrypt.c \
  fw/third_party/tinycrypt/source/utils.c -o /tmp/et100_gatt_test
/tmp/et100_gatt_test
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ifw/tests/gatt_fakes -Ifw/src \
  -Ifw/vendor/fr30xxc_sdk__202411/components/btdm/include \
  fw/tests/test_scan_lifecycle.c -o /tmp/et100_scan_lifecycle_test
/tmp/et100_scan_lifecycle_test
```

既有 serial／reporter 命令見 [serial 規格](../02_protocol/01_evb_serial_gchk.md) 與 [scan 紀錄](04_evb_cid_ble_scan.md)。

## Build 與燒錄

`bash fw/tools/build_evb_serial.sh` 成功，log `/tmp/et100_gatt_build.log`。SDK 新 patch 0；原有 linker／96 MHz／FR509x post-process 保留。仍有既有 RWX LOAD segment linker warning，未為此改 SDK。

| 項目 | 結果 |
|---|---|
| ELF text / data / bss | 239,568 / 35,288 / 66,304 bytes |
| data+bss | 101,592 bytes；不是 runtime heap 可用量 |
| Project.bin | 274,864 bytes |
| Project_burn.bin | 283,056 bytes，含 8,192-byte header 區 |
| CRC32（raw payload） | 0x70A77898，獨立 zlib 比對通過 |
| Burn SHA-256 | `59b2b727f50d9db566e39a72e848c1117371e92f7a4514e788b398f6ffeaa5c5` |
| 影像檢查 | FR509x magic／TLV length=0／exec_offset=0x2000／code_length／payload 完全相符，未碰 offset 800 KiB FlashEnv |
| EVB 燒錄 | 0x08000000..0x080451AF，70 sectors，37.7 秒；逐 byte readback PASS |
| Reset | SYSRESETREQ 後 resumed，PC 在 PRAM；尚未由 UART 證明 app／GATT 啟動 |

燒錄 log `/tmp/et100_gatt_flash.log`。沿用 Stanley 已授權的修改／build／flash 分工；Codex 未操作 UART／HTML。原 921600 baseline target 保留。新增 GATT task 6 KiB、serial task 4 KiB stack、16×264-byte queue 從既有 50 KiB RTOS heap 配置，尚須量 high-water。

## Stanley 手動驗收

1. HTML 以 9600 連 DAPLink VCOM，清除 reset 初期亂碼。GCHK 應顯示 0.6.0-dev；先確認 GTIME／CID 舊設定與 Type 1 未退化。
2. GATT 頁選真正的 EUP sensor MAC，先手動連線：`BLEGW$STYPESWITCH;1:<MAC>;<token>`。等待 sensor 可連線廣播窗口，觀察 WS → CS → AS → AP；ACK 不等於認證成功。AF／CF／TO 請保留完整 log。
3. AP 後讀 firmware（GSENSORCMD 01）、PING（7F）與 GSENSORINFO，檢查 MR／回覆 token。GSENSORINFO 是 cache，不能單獨證明仍在線。
4. 設定測試僅使用 HTML 已有且 sensor 規格明定的項目；SSENSORCFG 回 valid=1 後再讀回，必要時斷線後確認生效。不任意嘗試未定義 tag。
5. STYPESWITCH 0，等待 DC，再查 GTYPESWITCH=0；確認 Type 1 掃描回報恢復。連線中取消、離線與錯誤認證也需確認不會卡死／假 AP。
6. SAUTOGATT 加一個 MAC，查 GAUTOLIST，按 RST 後查清單保存，匹配 ADV 後應自動連線。session 不保存，但 auto 可很快重連，不能要求 reset 後查詢一定為 0。移除 auto flag 不取消既有 session，須另送 STYPESWITCH 0。
7. 輸出 HTML 完整 log 供解析，確認 Type 8 XOR、各 token、sensor_status、取消後無舊 AP/MR、斷線後 scan 恢復。所有命令逐筆等待回覆。

## 已知限制

### HTML 斷線格式修正（2026-10-21）

Stanley 回報 GATT 連線正常；17:46 log 顯示 HTML 送 `BLEGW$STYPESWITCH;0;;KKM8`，舊 parser 回 valid=0，之後 7F PING 仍成功。這是命令遭拒，不是已接受 cancel 後 SDK disconnect 失敗。SAUTOGATT flag=0 只移除自動連線設定，不中止既有 session。

修正接受 `0;`／`0`，連線接受 `1;<MAC>`／`1:<MAC>`，回覆採 Gateway 的 canonical 格式。加入 HTML 空欄取消、分號連線、小寫 MAC 正規化與 malformed 拒絕測試；成功 session 的 mock cleanup 現在經真正 UART handler 的 `0;` 路徑執行，確認 DC 與 scan 恢復。Firmware 仍為 0.6.0-dev，以本次 hash／log 辨識修正版。Stanley 須重按 HTML Disconnect，確認 valid=1 → DC → GTYPESWITCH=0，而非僅看按鈕狀態。若 auto list 尚有該 MAC，須先移除避免自動重連。

本次 build log：`/tmp/et100_gatt_disconnect_build.log`；flash log：`/tmp/et100_gatt_disconnect_flash.log`。上表保留首版映像數據，修正版結果另列於下。

修正版四組 host tests 通過（`/tmp/et100_gatt_disconnect_host_tests.log`）；ARM build 與獨立 header／CRC／payload／FlashEnv 邊界檢查通過。ELF text/data/bss=239,632/35,288/66,304 bytes；raw=274,928 bytes，burn=283,120 bytes；CRC32=0x721CAEF2；burn SHA-256=`9581dcf1924022380ff72d4c55e77ea86ce4c8c75fedcf57a7417ce114abec5e`。硬體取消／DC／scan 恢復仍待 Stanley 實測。

修正版已寫入 EVB（70 sectors，37.5 秒），逐 byte readback PASS；SYSRESETREQ 後 resumed，PC=0x080060D8 位於 app flash。未執行 UART／HTML 自動測試，未 commit／push。

單一 session／單一目標 metadata cache；手動每次工作只有一次 connect、一次 auth；認證三秒包含 discovery。模糊零 status CONN_END 若無後續確認，可能保留 DRAIN 直到 RST，不假釋放 owner。持續輸出過載可丟 ACK 並安全清理。真實 SDK callback 時序、掃描／廣播／GATT 並行、FlashDB GC／斷電、長時間與 heap／stack 壓力尚待測試。完整行為見 [GATT 規格](../02_protocol/02_evb_gatt_client.md)。
