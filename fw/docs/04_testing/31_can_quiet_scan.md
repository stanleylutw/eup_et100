# v0.9.8 CAN 靜默掃描與版本來源驗證

日期：2026-10-28；分支 `v0.9.8_can_quiet_scan`，base `v0.9.7_can_j1939`／`f9977ba`。

**Claude Step 5 review：PASS**（Critical 0／Major 0／Minor 1 已修，見 `fw/REVIEW_REPORT.md`）。
本輪為 Codex Step 4；不 commit、不燒錄、不存取 EVB／UART／DAP，完成後交 Claude Step 5 review。

## 1. 修改與邊界

- `fw/src/can_task.c`：task 端新增 `rx_since_invalidate`，初始化為 false；每次交付 `AppGateway_Frame()` 後設 true。速率 apply 成功／失敗與 BUS_OFF 共用 helper，只有處理過新訊框才 invalidate，完成後清旗標。SYS12 狀態更新不受旗標影響。
- 初始化仍由原樣移植的 `AppGateway_Init()` 清空並請求首次快照；`AppGateway_Configure()` 的設定快照照舊。未修改 bitrate 掃描節奏、TTL、Type2／3／4 的週期或 SMP gate。
- `fw/targets/evb_serial.mk`：唯一人工版號為 `ET100_VERSION_FLAGS` 中的 `EUP_FIRMWARE_VERSION="0.9.8-dev"`。Make 從實際 flag 推導 major／minor／patch，C 以公式 `major*100+minor*10+patch` 產生 `ET100_CAN_VERSION`。一般回覆、佇列滿即時失敗回覆與 Type0 共用此常數；minor／patch ≥10 在編譯期拒絕。
- `fw/src/can/`、SDK、F407 repo、HTML、FlashDB schema 均未修改。施工單與既有 REVIEW_REPORT 是工作開始前的變更，未由本輪改寫；永久平台文件由 Claude Step 7 處理。

## 2. Host 驗證

| 項目 | 驗證內容 | 結果 |
|---|---|---|
| 靜默自動掃描 | 0～30000 ms，每50 ms step；61 次 apply，glue invalidate 0 次；Type1 1 筆、Type2 1 筆、Type3／4 各3筆 | PASS |
| 收到資料後靜默 | 三幀鎖定250k；Type1 TTL 仍於3秒到期；5秒開始重掃、首次實際換速時 invalidate 1次，後續掃描到30秒不再增加 Type1 | PASS |
| 清空後重新收資料 | 新訊框重新設旗標；下一次硬體 apply 失敗仍清空一次，再次失敗不重複；SYS12=6 | PASS |
| BUS_OFF | 有資料時清空一次；無新資料的恢復與再次 BUS_OFF 不再 invalidate；保留 SYS12 4→5→2 | PASS |
| 設定快照 | 靜默時成功設定 GTIME 仍立即要求 Type1 快照，未以靜默抑制設定行為 | PASS |
| 版本 | 解析韌體字串與 CAN 常數比對；一般 GVERSION／佇列滿失敗回覆／Type0 均驗證98；Make 覆寫為1.2.3-dev時元件同步變為1／2／3 | PASS |
| 非法版號 | minor=10、patch=10 分別編譯失敗，並確認對應 `_Static_assert` 訊息 | PASS |
| 原移植與既有斷言 | 17份來源hash一致；四階段56筆demo、golden wires、parser fuzz、ring／epoch／設定保存、MON恆開、零TX、INIT卡住fixture保留 | PASS |

測試以 wrapper 計數 glue 對真實 `AppGateway_Invalidate()` 的呼叫，仍使用原移植解碼器產生並計數實際輸出；未替換解碼或快照行為。
既有 SMP resume 的 Type1 斷言保留，改為恢復後注入真實訊框觸發變更，不再依賴已修除的靜默掃描副作用。

執行命令：

```bash
bash fw/tools/test_can_host.sh
bash fw/tools/test_evb_serial_host.sh
bash fw/tools/test_sensorflow_nano.sh
bash fw/tools/test_inst_status_host.sh
for test in tools/tests/*.cjs; do node "$test" || exit; done
bash fw/tools/build_evb_serial.sh
```

完整 host、Sensor FLOW nano、installer status 測試與13支JS測試全部 PASS。Sensor FLOW nano 為 CPU／RAM-only，不存取硬體。瀏覽器測試在沙箱內啟動 Chrome 時 SIGABRT／EPERM，改於沙箱外執行既有 mocked Web Serial 測試後通過，不連接實體 serial port。

暫存 logs：`/tmp/et100_quiet_can.log`、`/tmp/et100_quiet_host.log`、`/tmp/et100_quiet_nano.log`、`/tmp/et100_quiet_inst.log`、`/tmp/et100_quiet_js.log`、`/tmp/et100_quiet_build.log`、`/tmp/et100_quiet_nm.log`。

### v0.9.8 m1：移除 ripgrep 依賴的回歸驗證

`fw/tools/test_can_host.sh` 的非法版號錯誤訊息檢查已由 `rg -q` 改為 macOS 內建的 `grep -q`，保留原搜尋字串與失敗判定。

於乾淨環境確認 Bash 為 `3.2.57(1)-release`、PATH 為 `/usr/bin:/bin:/usr/sbin:/sbin`，且 `command -v rg` 找不到指令，再依序執行：

```bash
env -i HOME="$HOME" PATH=/usr/bin:/bin:/usr/sbin:/sbin bash fw/tools/test_can_host.sh
env -i HOME="$HOME" PATH=/usr/bin:/bin:/usr/sbin:/sbin bash fw/tools/test_evb_serial_host.sh
```

| 測試 | 結果 | Log |
|---|---|---|
| CAN host（無 rg） | PASS，exit 0；包含17份來源hash、56筆demo、quiet CAN、SDK adapter、版本比對與minor／patch=10編譯失敗檢查 | `/tmp/et100_quiet_no_rg_can.log` |
| 完整 serial host（無 rg） | PASS，exit 0；包含既有回歸及腳本尾端呼叫的CAN host | `/tmp/et100_quiet_no_rg_host.log` |

首次於沙箱內執行時，Make 無法建立暫存檔；改於沙箱外使用相同乾淨環境重跑後，兩支腳本均通過。本輪僅修改測試工具與本節紀錄，不存取硬體、不燒錄、不 commit；交 Claude review 確認 m1。

## 3. ARM build 與產物

正式 M33 build／unsigned development DFU packaging PASS，版本 `0.9.8-dev`。僅原有 linker RWX LOAD segment warning，無新 warning。
`arm-none-eabi-nm` 確認 `can_add_tx_*`、`can_init`、`can_enter_bus_monitoring_mode` 均未連結。

| text B | data B | bss B | total B |
|---:|---:|---:|---:|
| 294864 | 35856 | 93312 | 424032 |

產物位置 `fw/build_serial/output/`；未燒錄：

| 檔案 | 大小 B | SHA-256 |
|---|---:|---|
| Project.bin | 330728 | `bf45bb3e31ac1cdc425bba35d694e0d1acffb7d6417317d13ae5441ae52ac156` |
| et100.signed.bin | 331280 | `b72ffecf6bbd3a1950b44dedc56413e66c107443d19008cf978434fc13ae0116` |
| dfu_application.zip | 206892 | `01921ab86d2021acedd77ff47ca1f0f2f8962d044688dfef527cbda6a80c4664` |

## 4. Stanley 實機驗收（全部待測）

1. 自行燒錄 v0.9.8；`CAN$GVERSION;<tok>` 回 `V1:98`，設定自動速率並啟動 F407 S2 demo。
2. F407 reset或拔CAN線，記錄TTL到期與開始掃描的邊界；失效／首次清空後不再每約0.56秒重複空值Type1。Type2／3／4仍依週期送缺值。接回後鎖定並恢復四階段資料。
3. 比對 SYS12 3→2→3、BUS_OFF／恢復狀態，確認不因省略重複 invalidate 遺失狀態更新。
4. 開機300秒確認Type0版本98；SMP上傳期間CAN主動回報暫停、完成後恢復；斷電重開後設定保留且能自動鎖定。

以上未預填硬體 PASS；本輪不取代 Claude Step 5 review。

## 5. Stanley 實機結果（2026-10-28 14:50–14:52，`rs232_log_20261028_145239.txt`，Claude 判讀）

| 項目 | 證據 | 結果 |
|---|---|---|
| 版本 | `AT$PD=CAN$GVERSION;1;V1:98;ZQP5*4D` | ✅ |
| 重開機設定保留＋冷啟動自動鎖定 | v0.9.7 設的 `CAN$SBITRATE;0` 經重燒／重開後 `GBITRATE` 仍回 `0`，SYS12=3、Type1 四階段正常 | ✅ |
| 靜默不重送（M1） | 3 次斷訊（14:51:11、14:51:59、14:52:11）：每次只有 TTL 失效的 Type1（1–2 筆）＋約 3 s 後 1 筆 rescan invalidate 快照，之後到接回前（最長 24 s）**無 Type1**；v0.9.7 同情境為每 0.56 s 一筆 | ✅ |
| SYS12 | 每次斷訊 `12,2`、接回 `12,3` | ✅ |
| 接回自動鎖回 | 14:51:38、14:52:01、14:52:34 恢復 Type1，短斷訊（約 2 s）不觸發 rescan | ✅ |
| Type2／3／4 | 靜默期間照週期輸出空值／`FF` 補缺秒，恢復後混合樣本正確 | ✅ |

觀察（非缺陷）：rescan invalidate 的那 1 筆 `AT$PD=67;1;000000;;;*0B` 與 TTL 失效的上一筆內容相同，每個資料 epoch 最多 1 筆，有界。

仍待測：開機 300 s Type0（版本 98）、SMP 上傳期間 CAN 暫停／完成後恢復。
