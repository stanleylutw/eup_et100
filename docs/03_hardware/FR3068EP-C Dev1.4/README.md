# FR3068EP-C DEV1.4 EVB Bring-up

**日期**：2026-10-21
**狀態**：Phase 1b 基本燒錄、啟動、SWD 與 BLE GATT 已實機驗證

## 1. 開發板與 ET-100 差異

| 項目 | FR3068EP-C DEV1.4 EVB | ET-100 |
|---|---|---|
| MCU | FR3068EP-C，含 2 MB PSRAM | FR3068E-C，不可依賴 EVB PSRAM |
| 外部 NOR Flash | GigaDevice GD25LQ128EWIG，16 MB、1.8 V | XM25QH256D，32 MB、3.3 V、需 4-byte addressing |
| Debug UART | PB5=MCU TX、PB4=MCU RX，接板載 DAPLink VCOM | PB4/PB5 配給 RS232_2，產品 debug UART 必須另選 routing |
| SWD | PB10=SWCLK、PB11=SWDIO | 同為 PB10/PB11 |
| Reset | 板上 RST 按鍵；DAPLink 沒有 nRESET 線 | 依 ET-100 debug connector 實際 wiring |
| 車載介面 | 板上有 CAN、RS485 transceiver 與端子 | 依 ET-100 原理圖 |

EVB 上的額外 PSRAM、外部 Flash 與周邊只供 bring-up 使用；ET-100 firmware 的 memory budget 與 driver 不得依賴 EVB 專有資源。

## 2. 照片

| 檔案 | 內容 |
|---|---|
| [IMG_5516.jpeg](IMG_5516.jpeg) | EVB 正面全貌、DAPLink、擴充排針、CAN/RS485 與按鍵 |
| [IMG_5517.jpeg](IMG_5517.jpeg) | DEBUG jumper 區近照：PB5/RX、PB4/TX、PB10/SWDC、PB11/SWDO |
| [IMG_5518.jpeg](IMG_5518.jpeg) | GD25LQ128EWIG 外部 Flash 近照 |
| [IMG_5519.jpeg](IMG_5519.jpeg) | EVB 背面 |

照片在納入 repo 前已移除 EXIF/GPS metadata。

## 3. DEBUG Jumper

板上四列 DEBUG jumper 的絲印與用途：

| MCU pin | DAPLink 端 | 用途 |
|---|---|---|
| PB5 | RX | UART：MCU TX -> DAPLink RX |
| PB4 | TX | UART：MCU RX <- DAPLink TX |
| PB10 | SWDC | SWCLK |
| PB11 | SWDO | SWDIO |

出廠只有兩個 jumper cap，原本插在 PB5/PB4 UART。初次 SWD bring-up 時把兩個 cap 移到 PB10/PB11；之後已補兩個 jumper cap，四個位置同時接通。2026-10-21 實測確認同一條 DAPLink USB 可同時使用 CMSIS-DAP SWD 與 921600 bps VCOM UART。

板載 DAPLink **沒有 nRESET 線**，所以 pyOCD `under-reset` 模式不可用。若 image 讓 debug bus 卡死，必須用 `rescue_and_flash.py` 在手動釋放 RST 後的短窗口抓住 CPU。

## 4. 已確認的介面

- DAPLink VCOM：`/dev/cu.usbmodem2024_2425_62851`（裝置名稱可能因主機重插而改變）
- ROM UART：115200 8N1；reset 時輸出 `freqchip`
- SWD DP IDCODE：`0x0BE12477`（DPv2）
- AP：AHB5-AP
- CPUID：`0x410FD214`，Cortex-M33 r0p4
- Internal flash loader：SDK `components/tools/keil/FR30xx.FLM`
- Internal flash base：`0x08000000`
- Flash sector/page：4 KiB / 256 B

## 5. 工具準備

從 repo root 執行：

```bash
python3 -m pip install --user pyserial pyocd
python3 fw/tools/evb/fr3068_flash.py --help
```

工具預設會由 script 位置反推 repo root，不含使用者 home directory 的硬編碼路徑。完整參數見 [`fw/tools/evb/README.md`](../../../fw/tools/evb/README.md)。

## 6. Build 與 Image 安全檢查

```bash
cd fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC
make
```

本板已驗證的 boot image 是整份 `Project_burn.bin` 寫到 `0x08000000`。燒錄前必須確認 FR509x header layout：word 7 與 word 11 是 `0x51525251`，word 8 是 `0x00000000` 的 `tlv_length`。

```bash
python3 - <<'PY'
from pathlib import Path
import struct

p = Path("fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC/output/Project_burn.bin")
d = p.read_bytes()
for index in (7, 8, 9, 11):
    print(f"word{index}: 0x{struct.unpack_from('<I', d, index * 4)[0]:08X}")
PY
```

`CHIP_TYPE_FR509x` 在此是 post-process 的 header layout selector，不代表 MCU 型號是 FR509x。先前改成 FR303x 會讓 options 欄位錯位，實機已證實可能導致 ROM flash access 卡死。

## 7. 正常燒錄

確認 jumper 已接 PB10/PB11 後：

```bash
python3 fw/tools/evb/fr3068_flash.py
```

腳本會：

1. 驗證 image 是已知 FR509x header layout。
2. 使用 SDK `FR30xx.FLM` sector erase/program。
3. 對 `0x08000000` 起的 image 做逐 byte readback。
4. 清除 `DEMCR.VC_CORERESET`，執行 SYSRESETREQ。
5. 重新 attach，短暫 halt 讀 PC/SP 後 resume。

本次實測 66 sectors，program 約 35 秒；實際時間依 probe 與 USB 狀態而異。

## 8. 救援燒錄

只在正常 SWD attach 因壞 image 持續回 WAIT ACK 時使用：

```bash
python3 fw/tools/evb/rescue_and_flash.py
```

看到 `[catch] armed` 後，**按住再放開板上 RST 一次**。腳本會送 DPv2 dormant-to-SWD sequence，在 reset release 後的短窗口設定 halt/vector catch，保持同一 probe session，接著燒錄、verify、清 vector catch、reset 並檢查 PC。

救援期間不可拔除 USB、重啟腳本或讓其他 pyOCD process 同時占用 probe。

## 9. UART 與唯讀診斷

UART jumper 接回 PB5/PB4 後：

```bash
python3 fw/tools/evb/uart_listen.py /dev/cu.usbmodem2024_2425_62851 115200 20
python3 fw/tools/evb/uart_listen.py /dev/cu.usbmodem2024_2425_62851 921600 20
```

SWD 唯讀檢查：

```bash
python3 fw/tools/evb/swd_probe.py
python3 fw/tools/evb/swd_retry.py
```

2026-10-21 並行測試方式：先開 921600 bps capture，再由 CMSIS-DAP SWD 送 `SYSRESETREQ`。SWD reset 成功，UART 同時收到 748 bytes，printable ratio 95%，包含：

```text
start running
FR5090: ble_simple_peripheral.
HCI is ready.
app_ble_init
adv start :0x00
```

開頭少量非 printable bytes 出現在 reset 邊界；後續 application log 完整可讀，不影響 921600 bps 驗證結論。

## 10. Phase 1b 實測結果

- `Project_burn.bin` 以 FR509x header 寫入 `0x08000000`，readback byte-for-byte PASS。
- Reset 後 PC 位於 PRAM `0x1FFE3A90` 的 `__RAM_CODE` idle 路徑，app 正常執行。
- 同一條 DAPLink USB 在 SWD software reset 期間同步收到 921600 bps UART application log。
- 手機 nRF Connect 可連線 `30xx_Ble_Periphreal`。
- 可看到 GATT service `0xFFF0`、characteristic `0xFFF3`（Read/Write/Notify）與預期 128-bit characteristic。
- Patch #7a 的 96 MHz 設定可開機與建立 BLE 連線；尚未做頻率實測或長時間穩定性驗證。
- Patch #8 讓 development build 保持不進 sleep，以維持 SWD 可達性；此行為不代表 ET-100 production power policy。

## 11. 待辦

**最新板上版本：0.7.0-dev／EUP_AIoT_HUB_FR**。已燒入手機 App GATT Manager（四個 8D2F characteristic），program／逐 byte readback PASS，App 實機連線／清單／notify 與 sensor 並行待 Stanley 驗收，見 [App GATT 紀錄](../../../fw/docs/04_testing/07_evb_app_gatt_server.md)。UART 保持 9600，D4／D3 與按鍵不變；下文 0.6.0-dev 為歷史進度，不再是目前板上版本。未修改 SDK 或 App。

2026-10-21 依 Stanley 指定，最新 serial target 將 **D4／PA7** 作 Gateway LED，**D3／PA6** 在 app 初始化後固定低電位關閉。D4 待機常亮、符合廣播資料時 200 ms 切換、sensor GATT link 存在時 100 ms 切換；不是 AES 認證指示。未改 K1-K3／PMU pull。實體亮滅與 regression 待 Stanley 驗收，最新 build／flash 證據見 [LED 紀錄](../../../fw/docs/04_testing/06_evb_gateway_led.md)。

2026-10-21 已新增獨立 UART3 **9600 bps** serial target 並燒入 EVB。目前板上為 `0.6.0-dev`，新增 Gateway-style GATT client（manual／auto、AES、metadata／設定、Type 8）；host／build／program-readback 通過，真實 sensor 驗收待 Stanley 執行，見 [GATT 紀錄](../../../fw/docs/04_testing/05_evb_gatt_client.md)。前版 0.5.0-dev 的 GCID／SCID 保存、general discovery scan 與 Type 1 GTIME/RSSI/XOR 回報已由 Stanley HTML log 確認 4 個真實 MAC、55 筆 Type 1 且 XOR 全通過，並確認 CID 切換與 CID／GTIME 手動 RST 後保存正常；新版本仍須 regression。斷電／GC／長時間測試仍未驗收。此 target 不輸出 921600 vendor log；上方 921600 指令適用原 BLE baseline。規格見 [serial 協定](../../../fw/docs/02_protocol/01_evb_serial_gchk.md)，歷史證據見 [CID／scan 紀錄](../../../fw/docs/04_testing/04_evb_cid_ble_scan.md)。

- 2026-10-21 **D3/D4 已由原理圖確認**：D3 連接 **PA6** (Pin 72)，D4 連接 **PA7** (Pin 73)，均串聯 1 kΩ 限流電阻接地，**高電平有效（Active HIGH）**。
- 2026-10-21 **K1-K3 已由原理圖確認**：K1 連接 **PP0** (Pin 66)，K2 連接 **PP2** (Pin 68)，K3 連接 **PP3** (Pin 67)，直通 GND，板載**無外部上拉電阻**。先前電位跳動係因未開內部上拉（Floating）；韌體設定 PMU 啟用 Internal Pull-Up 即可作為穩定**低電平有效（Active LOW）**按鍵。
- **官方 EVB 完整工程解析文件已建立**：
  - [PCB 與原理圖工程索引 (README.md)](FR3068E-C_DEV1.4%20PCB文件/README.md)
  - [MCU 核心電路規格書 (FR3068E-C_MCU_DEV1.4.md)](FR3068E-C_DEV1.4%20PCB文件/FR3068E-C_MCU_DEV1.4.md)
  - [周邊與介面包規格書 (FR3068E-C_Device_DEV1.4.md)](FR3068E-C_DEV1.4%20PCB文件/FR3068E-C_Device_DEV1.4.md)
  - [板載偵錯器規格書 (FR3068E-C_Debug_DEV1.4.md)](FR3068E-C_DEV1.4%20PCB文件/FR3068E-C_Debug_DEV1.4.md)
  - [PCB Layout 實體工程規格書 (FR3068E-C_DEV1.4_PCB.md)](FR3068E-C_DEV1.4%20PCB文件/FR3068E-C_DEV1.4_PCB.md)
- 做 96 MHz 下 BLE 長時間連線與 throughput 測試。
- 驗證 Patch #7b runtime guard 在 EVB 不誤觸發。
- 依獨立 `IMPLEMENTATION_PLAN.md` 進行 GPIO interrupt、LED drive、CAN、I2C bring-up。

