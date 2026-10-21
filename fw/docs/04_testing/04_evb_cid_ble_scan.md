# DEV1.4 CID／BLE scan／Type 1 驗證

日期：2026-10-21
分支：`v0.5.0_evb_cid_ble_scan`
版本：`0.5.0-dev`

## 已完成 host 驗證

AddressSanitizer／UndefinedBehaviorSanitizer：

- Serial regression：GCHK／GTIME、token、拆包／黏包／超長輸入；CID 預設、四欄格式、hex 正規化、全空 filter、mock 保存／reload／失敗不更新 RAM，PASS。
- Reporter：CID／Service UUID 過濾、錯誤 AD 長度、MAC byte order、RSSI、Gateway 原始 XOR vector `*3F`、同包 name 附加、RSSI 排序、一秒節流、新資料才回報、EUP burst dedup／window expiry、毫秒 clock wrap、cache／batch 上限，PASS。
- 5,000 組 bounded AD fuzz inputs，sanitizers 無報錯。不是 radio fuzz 或完整 SDK HCI 測試。

Logs：`/tmp/et100_cid_host_tests.log`、`/tmp/et100_ble_reporter_host_tests.log`。

## Build／image

- ARM GCC build PASS；只有既有 RWX LOAD segment warning。完整 log `/tmp/et100_cid_scan_build.log`。
- text 228,304、data 35,280、bss 65,336 bytes；靜態 SRAM 100,616 bytes，為 linker 512 KiB 的 19.2%。runtime stack／heap 餘量未量測。
- raw 263,592 bytes，burn 271,784 bytes；FR509x header／exec offset／length／payload／獨立 CRC32／FlashEnv 邊界檢查 PASS。
- CRC32 `0x895A2561`。
- SHA-256 `57a98b59c328b1a6268f66145a8f4354c5bc25b58e98fa331e0b363b4a33a405`。
- SDK 新增 patch 0；專案 overlay 替換 main／AT／BLE source，新增 reporter object。既有 GTIME key／schema 不變，CID 另用 user key 0x00030003。

## Flash／硬體

已依授權 program 到 `0x08000000`，67 sectors、38.1 秒，逐 byte readback PASS。SYSRESETREQ 後 PC `0x080063EE`、SP `0x20013490`，已 resume，PC 位於 app flash。log `/tmp/et100_cid_scan_flash.log`。依既定分工，Codex 未操作 UART／HTML；實際 radio／sensor 驗收由 Stanley 執行，不能由 flash verify 推論掃描已通。

## Stanley 實測 — 16:54 至 16:56

來源：Stanley 提供 `/Users/stanley/Downloads/rs232_log_20261021_165642.txt`，108 行。此為 HTML export，未保存逐 byte UART 時序或 radio sniffer capture。

| 檢查 | 實測結果 |
|---|---|
| Firmware／baud | GCHK 為 0.5.0-dev，9600 bps 連線 |
| Command response | 23 筆 TX、23 筆對應回覆；無缺漏或 token／command mismatch |
| Supported commands | GCHK 4、GCID 3、GGTIME 4、SCID 1，均 valid=1 |
| Current settings | GCID 0639；GGTIME 3 秒；SCID 設回 0639 後成功 ACK |
| Type 1 | 55 筆，分布在 105.975 秒的接收窗口；全部可解析 |
| Payload／filter | 55 筆皆以 3906 開頭，即 ID 0x0639；16 筆 26-byte、39 筆 16-byte payload |
| MAC | C3000061A509：16 筆；EEACB6F5627E：4 筆；FADB30E9301F：19 筆；F1962B1B7D6F：16 筆 |
| XOR checksum | 55/55 獨立重算一致，0 error |
| RSSI | -99 至 -34 dBm，wire field 格式有效 |
| Type 1 間隔 | 最短 1.118 秒、中位 1.166 秒；未見低於一秒的兩筆回報 |
| Query turnaround | HTML 顯示 TX→RSP 48-142 ms，中位 62 ms；含 UART／host 時間，非 MCU 純處理延遲 |

未支援命令（GLIST、GMAXRPT、GAUTOLIST、GSOTAPKG、GTYPESWITCH、SAUTOGATT）均 valid=0，符合本次範圍；SAUTOGATT 點選失敗不代表 scan／Type 1 失效。

有一筆相同 MAC＋完整 payload 在 2.299 秒後再出現，已超過設定的 1500 ms dedup window，符合目前去重契約，不能判為 checksum 或 duplicate-window bug。HTML 接收時間不足以證明 MCU clock 精準度、完整 GTIME snapshot／RSSI 排序或在 air interface 上的 burst 去重。

**驗收結論**：基本 real BLE scan→Type 1→HTML 路徑、當前 0639 設定的 query／set 與 command/report 共存 PASS。log 只把 SCID 設回相同 0639，沒有不匹配／雙 ID／清空 filter 的對照，也沒有 reset marker；故負向過濾、CID 持久性、reset reconnect、全部 sensor payload fidelity 與長時間穩定性仍未驗收。

## Stanley 實測 — CID 1234 對照（17:00 export）

來源：`/Users/stanley/Downloads/rs232_log_20261021_170012.txt`，17 行。

- 16:59:40.262：SCID 1234 valid=1，token 6U10 正確。
- 16:59:48.964：GCID valid=1，讀回 1234；16:59:50.789 GGTIME 讀回 3。
- 改成 1234 後約 30 秒未見 Type 1，與先前 0639 sensors 被排除的預期一致；沒有同步 air capture，無法排除當時 sensor 停止廣播。
- 17:00:10.860：SCID 恢復 0639 valid=1，token RE2F 正確；log 到此結束，尚未看到恢復後的 Type 1。
- 7 筆命令都有正確 command/token 回覆，GLIST／GMAXRPT／GTYPESWITCH valid=0 為預期。
- GCHK 前有一行 `�`。可能與 reset 階段 ROM 使用不同 baud 有關，但 log 沒有 reset marker，不能據此認定曾按 RST，亦不能獨立宣稱 CID 持久性 PASS。需 Stanley 確認 16:59:40 至 16:59:48 間的實際 reset 操作。

此樣本新增負向 CID 過濾的支持證據；恢復回報與 reset 持久性結論仍待補充。未修改或重新燒錄 firmware。

**Stanley 後續確認（2026-10-21）**：針對上述兩筆操作之間是否按 RST，Stanley 明確回覆「yes everything ok」。結合 reset 後 GCID=1234、GGTIME=3 的原始 log，CID 與 GTIME 的手動 RST 後保存驗收 **PASS**。Stanley 亦回報整體操作正常；0639 恢復後的 Type 1 未包含在此 export，保留為使用者確認，未另造收包數據。此結果不延伸為突然斷電／FlashDB GC／長時間穩定性驗收。

## Stanley 手動步驟

維持 HTML 9600/8N1；每筆命令含 CRLF，HTML 可自動產生 token。

| 步驟 | 請求／操作 | 預期 |
|---|---|---|
| 1 | `BLEGW$GCHK;V001` | 版本 0.5.0-dev |
| 2 | `BLEGW$GCID;C001` | 首次預設 `...;1;0639;;;;C001` |
| 3 | `BLEGW$SGTIME;5;T001` | 成功；原先保存的 GTIME 在此之前應仍可查回 |
| 4 | 開啟 EUP sensor，等待其廣播周期＋最多 GTIME | `AT$PD=G1;1;<真實MAC>;<3906...>;<RSSI>*<XOR>`，HTML 顯示資料 |
| 5 | `BLEGW$SCID;0639;0DFC;;;C002` | 回覆 uppercase；兩種 ID 皆可通過 |
| 6 | `BLEGW$SCID;1234;;;;C003` | EUP 0639 不再回報，已排隊的舊 filter 資料清除 |
| 7 | `BLEGW$SCID;;;;;C004` | 不過濾 CID，仍只回報含 MFG／Service Data 的 ADV |
| 8 | 恢復 `BLEGW$SCID;0639;;;;C005`；按 RST 再查 GCID／GGTIME | CID 0639 與 GTIME 5 仍保留 |
| 9 | `BLEGW$SCID;ZZZZ;;;;E001` | valid=0，再查 GCID 原值不變 |

若 sensor 是每 30 秒廣播一次，GTIME 5 不表示每 5 秒必定有新報告。沒有新封包不重送舊資料。若沒有 Type 1，先確認 ID／sensor 在廣播，再檢查 SWD symbols `et100_scan_status`、`et100_scan_reports`、`et100_scan_accepted`；不可把 host tests 當成 radio 已通。

## 已知限制／待驗證

只支援 legacy LE 1M ADV；SDK general discovery 的 non-discoverable 處理、scan＋peripheral advertising／GATT 並行、實際 RSSI／payload／checksum、CID reset 保存、密集 RF、斷電／GC、runtime stack high-water、throughput／低功耗都尚待硬體驗收。沒有 MAC whitelist、MAXRPT setting、realtime XYZ batch 或 OTA；每周期上限採 min(GTIME,15)，無完整 Gateway 功能等價宣稱。32-entry cache 滿時會淘汰最久未見者，serial 忙碌或周期切換可能捨棄未送出的舊 batch。

## 重跑 host tests

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DET100_SERIAL_HOST_TEST -Ifw/src fw/src/serial_gchk.c \
  fw/tests/test_serial_gchk.c -o /tmp/et100_serial_test
/tmp/et100_serial_test
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DET100_SERIAL_HOST_TEST -Ifw/src fw/src/ble_reporter.c \
  fw/tests/test_ble_reporter.c -o /tmp/et100_ble_reporter_test
/tmp/et100_ble_reporter_test
```
