# 跨 Sheet Net Inventory

> **2026-10-02 規格註記**：本表保留舊 SCH net 記錄，未重新核對新版 physical pin／alternate function。v0.4.9 列 5 組 UART、3 組 I2C；UART0~5、I2C0/3/5 網名不代表硬體 instance 數量。SWCLK／SWDIO、GPIO voltage bank 與各 peripheral routing 均待 board-matched pinmux 確認，不直接按新版腳號搬移 net。見 [開發對照](../fr306x_reference/05_et100_development_reference.md)。

**用途**：所有命名 net 的 source / sink / 穿越 sheet 對照表，特別聚焦跨 sheet 訊號。排序：電源 → 控制 → 通訊 → 中斷 → 類比 → 外部 I/O。

**圖例**：
- `→`：訊號方向（source → sink）
- `↔`：雙向（I2C、SPI data）
- `in`：進該 sheet 的訊號
- `out`：出該 sheet 的訊號

---

## 1. 電源 Rails（14 條主 rail）

| Net | Source Sheet | Sink Sheet(s) | V | I 上限 | 可切？ | 控制訊號 |
|---|:-:|:-:|:-:|:-:|:-:|---|
| PWR_IN                | 9 (J0901 pin 1 ← 車電)      | 9 經 F0901 → PWR_IN_9-36V | 9-36V | 1A fuse | 無 | — |
| PWR_IN_9-36V          | 9 (F0901 output)             | 6 (U0601 Vin), 11 (Q1108 source) | 9-36V | 2A | 無 | — |
| VDD_PP_9V             | 11 (Q1108 PFET drain)        | 外部 J0901 pin 10（Output 腳）| 9-36V | 200mA | ✓ | PWR_SW_CTRL (via AW9523 P0_3) |
| VDD_5V                | 6 (U0601 output)             | 6 ORing → _MCU/_PP           | 5.07V | 3A | 無 | — |
| VDD_5V_MCU            | 6 (D0601 output)             | 6 (U0602 Vin), 8 (Charger VIN?) | ~4.8V | 3A | 無 | — |
| VDD_5V_PP             | 6 (D0602 output)             | 5 (RS232 switch), 5 (CAN_5V), 8 (Charger?), 10 (NFC_VDD via R1001), 11 (ACC_INT_OUT) | ~4.8V | 1A | 無 | — |
| VDD_4V                | 6 (U0602 output)             | 7 (D0701 input)              | 4V (疑) | 3A | 無 | — |
| VBAT                  | 8 (U0801 BAT, J0801 pin)     | 7 (D0702 input), 8 (ADC divider) | 3.0-4.2V | 1A | — | — |
| VDD_LDO_4V            | 7 (D0701 ∥ D0702 ORing node) | 7 (4 顆 LDO VIN), 2 (Q0232 BJT), 12 (Q1201 PMOS) | 3.4-3.7V | 3A | 無 | — |
| LTE_VBAT              | 2 (Q0232 BJT emitter)        | 2 (EG800Q-EU VBAT pins)      | ~3.0-3.5V | 1.5A | ✓ | LTE_VBAT_EN (MCU GPIO) |
| GNSS_VBAT             | 12 (Q1201 PMOS drain)        | 12 (J1201 pin 2)              | ~3.6-3.9V | 300mA | ✓ | GNSS_VBAT_EN (AW9523 P0_4) |
| MCU_GNSS_3V3          | 7 (U0701 output)             | 1 (MCU 3V3 rails), 12 (D1201 input) | 3.3V | 300mA | 無 (常開) | — |
| GNSS_3V3              | 12 (D1201 cathode)           | 12 (J1201 pin 7)              | ~3.0V | 300mA | 無（但子板 LDO EN 可切）| GNSS_LDO_EN (AW9523 P1_3) |
| VDD_3V3               | 7 (U0702 output)             | 3 (G-Sensor 3V3), 4 (LED_3V3 via R0401), 5 (RS232 EN pull-up), 8 (Charger status pull-up) | 3.3V | 300mA | ✓ | VDD_3V3_EN (AW9523 P1_4) |
| VDD_PP_3V3            | 5 (Q0501 PMOS drain)         | 5 (3 顆 UM3221 VCC)           | ~5V (命名誤) | 300mA | ✓ | VDD_PP_3V3_EN (AW9523 P0_6) |
| BUZZER_3V3            | 7 (U0703 output)             | 4 (U0401 buzzer VCC)          | 3.3V | 100mA | ✓ | BUZZER_3V3_EN (AW9523 P0_7) |
| NFC_1V8               | 7 (U0704 output)             | 1 (MCU 1V8 域), 3 (G-Sensor 1V8), 10 (PN7160 VDD_PAD) | 1.8V | 300mA | ✓ | NFC_1V8_EN (AW9523 P0_5) |
| NFC_VDD               | 10 (via R1001 ← VDD_5V_PP)   | 10 (PN7160 VDD_A/D/UP/TX/VBAT) | ~5V | 1A | 無 | — |
| LED_3V3               | 4 (R0401 ← VDD_3V3)          | 4 (4 顆 LED anode)            | 3.3V | 25mA | 由 VDD_3V3_EN 間接控 | — |
| CAN_5V                | 5 (R0521 ← VDD_5V_PP)        | 5 (U0504 VCC)                 | ~5V | 100mA | 無 | — |
| CAN_VIO_3V3           | 5 (VDD_PP_3V3 的 pull-up)    | 5 (U0504 VIO)                 | ~5V | 10mA | 由 VDD_PP_3V3_EN 間接控 | — |
| LTE_EXT_1V8           | 2 (EG800Q-EU VDD_EXT output) | 3 (SIM VCC path), 11 (UART shift 高側), 13 (U1301 tuner VDD) | 1.8V | 50mA | 由 LTE 開機間接控 | — |

---

## 2. 控制訊號（AW9523 IO Expander 的 16 pin）

| AW9523 Pin | Net | Source | Sink | 用途 |
|:-:|---|:-:|---|---|
| P0_2 | DC_DET          | Sheet 11 Q1109 collector | AW9523 input | 外部 DC 存在偵測 |
| P0_3 | PWR_SW_CTRL     | AW9523 output | Sheet 11 Q1108 base/gate 側 | Output pin 控制 |
| P0_4 | GNSS_VBAT_EN    | AW9523 output | Sheet 12 Q1202 base       | GNSS 備援電 |
| P0_5 | NFC_1V8_EN      | AW9523 output | Sheet 7 U0704 EN           | NFC 1V8 LDO |
| P0_6 | VDD_PP_3V3_EN   | AW9523 output | Sheet 5 Q0502 base         | RS232 power switch |
| P0_7 | BUZZER_3V3_EN   | AW9523 output | Sheet 7 U0703 EN           | 蜂鳴器 LDO |
| P1_0 | MCU_RS232_EN1   | AW9523 output | Sheet 5 U0501 EN           | RS232 第 1 顆 EN |
| P1_1 | MCU_RS232_EN2   | AW9523 output | Sheet 5 U0502 EN           | RS232 第 2 顆 EN |
| P1_2 | MCU_RS232_EN0   | AW9523 output | Sheet 5 U0503 EN           | RS232 第 3 顆 EN |
| P1_3 | GNSS_LDO_EN     | AW9523 output | Sheet 12 J1201 pin 6 → 子板 LDO EN | GNSS 子板主 VDD |
| P1_4 | VDD_3V3_EN      | AW9523 output | Sheet 7 U0702 EN           | 3V3 LDO |
| P1_5 | MCU_WAKE_LTE_3V3| AW9523 output | Sheet 11 Q1106 base        | 經 shift 到 LTE WAKE |
| P1_6 | MCU_CHA_EN      | AW9523 output | Sheet 8 U0801 EN           | 充電器啟停 |
| P1_7 | ACC_INT_OUT     | AW9523 output | Sheet 11 Q1110 base        | 對外 ACC 輸出 |
| P1_8 | MCU_CAN_STB     | AW9523 output | Sheet 5 U0504 STB          | CAN 待機 |
| INTN | EXP_INTN        | AW9523 output | Sheet 1 MCU GPIO (PD3)     | AW9523 中斷通知 |
| RSTN | EXP_RST         | Sheet 1 MCU GPIO (PB15)    | AW9523 input               | AW9523 reset |

---

## 3. 直接由 MCU 控制的訊號（不經 AW9523）

| Net | Source MCU Pin | Sink Sheet(s) | 用途 |
|---|:-:|---|---|
| LTE_VBAT_EN      | MCU (未標 pin) | Sheet 2 Q0232 base         | LTE 電源開關 |
| MCU_LTE_RESET    | MCU PD2 | Sheet 11 → Sheet 2 LTE RESET_N | LTE reset |
| MCU_WAKE_NFC     | MCU | Sheet 10 PN7160 WKUP_REQ   | NFC 喚醒 |
| NFC_VEN          | MCU PC4 | Sheet 10 PN7160 VEN         | NFC enable |
| NFC_DWL_REQ      | MCU PC6 | Sheet 10 PN7160 DWL_REQ     | NFC firmware download |
| LED1_CTRL ~ LED4_CTRL | MCU GPIO ×4 | Sheet 4 DTC1143ZE base ×4 | 4 顆 LED |
| BUFFER_CTRL      | MCU PD15 | Sheet 4 Q1203 base         | 蜂鳴器 PWM |
| SPI_FLASH_CS/CLK/MOSI/MISO/RST | MCU PD8-PD12 | Sheet 1 U0103 | SPI Flash |
| SWCLK / SWDIO    | MCU PC1/PC2 | Sheet 1 TP0105/TP0106       | JLINK |

---

## 4. UART / 通訊

| UART# | MCU TX pin | MCU RX pin | 對端 | Baud | 電壓域 |
|:-:|:-:|:-:|---|:-:|:-:|
| UART0 | PB (RS232_TXD0 ← Sheet 1 → Sheet 5 U0503 T1IN) | Sheet 5 U0503 R1OUT → MCU | 外部 RS232_0 (via J0901 pin 23/24) | 115200 | 3V3 |
| UART1 | PB8 → Sheet 5 U0501 T1IN | Sheet 5 U0501 R1OUT → PB9 | 外部 RS232_1 (via J0901 pin 11/24?) | 9600 | 3V3 |
| UART2 | PB5 → Sheet 5 U0502 T1IN | Sheet 5 U0502 R1OUT → PB4 | 外部 RS232_2 (via J0901 pin 13/15) | 9600 | 3V3 |
| UART3 | — | — | CAN 共用 pin（實際用 PB6/PB7 給 CAN）| — | 3V3 |
| UART4 | PA13 → Sheet 11 shift → Sheet 2 LTE | Sheet 2 LTE → Sheet 11 shift → PA12 | LTE AT command | 115200 | **1V8 ↔ 3V3** |
| UART5 | PD4 → Sheet 12 J1201 pin 4 | Sheet 12 J1201 pin 3 → PD5 | GNSS NMEA | 115200 | 3V3 |

**跨電壓域 UART（UART4）**：
- MCU 3V3 ← → Sheet 11 2SC4617 BJT ← → 1V8 → Sheet 2 EG800Q UART
- **BJT 反相邏輯 → 韌體 UART peripheral 必須設 TX/RX polarity invert**

---

## 5. I2C Bus

| Bus Name | 電壓域 | SCL / SDA MCU Pin | Slaves |
|---|:-:|:-:|---|
| EXP_I2C5 | 3V3 | PD6/PD7 | Sheet 11 U1101 AW9523 (0x58 預設)|
| Sensor_I2C0 | 1V8 | — | Sheet 3 U0301 SC7U22 (0x6A/6B)|
| NFC_I2C3 | 1V8 | PB12/PB13 | Sheet 10 U1001 PN7160 (0x28/29/2A/2B)|

---

## 6. 外部 I/O（Sheet 9 J0901 ↔ Sheet 11 偵測 / 驅動）

| J0901 Pin | 外部功能 | Sheet 9 net name | Sheet 11 處理 | 到 MCU 的 net |
|:-:|---|---|---|---|
| 1 | PWR_IN      | PWR_IN             | 無（直通 Sheet 9→6）| — |
| 3 | ACC         | ACC_INT_IN_CON     | Q1101 BJT 偵測     | ACC_INT_IN (MCU GPIO) |
| 4 | 油量 ADC    | oil_ADC_IN_CON     | R 分壓             | oil_ADC_IN (MCU ADC PP4) |
| 5 | 1-Wire      | 1_Wire_Temp_IN_CON | F1 fuse + Q1116/Q1117 BJT bidir buffer | 1_Wire_Temp_IN_MCU / OUT_MCU (MCU PD13/14) |
| 7-9 | Input1-3  | IO1/2/3_INT_IN_CON | Q1103/05/07 BJT 偵測 | IO1/2/3_INT_IN (MCU GPIO) |
| 10 | Output      | PWR_SW_CTRL_CON    | Q1108 PFET open-drain | PWR_SW_CTRL (AW9523) |
| 14/16/18/22/26 | ACC_OUT ×5 | ACC_INT_OUT_CON | Q1110 BJT 單一 collector 共用 | ACC_INT_OUT (AW9523) |
| 11 | RS232_1 TX  | RS232_TXD1         | — (純外部) | 經 Sheet 5 U0501 到 MCU PB8 |
| 13 | RS232_2 TX  | RS232_TXD2         | — | 經 Sheet 5 U0502 到 MCU PB5 |
| 15 | RS232_2 RX  | RS232_RXD2         | — | 經 Sheet 5 到 MCU PB4 |
| 19/20 | CAN_H/L   | CAN_H / CAN_L      | — (直接到 Sheet 5 SIT1042) | MCU_CAN_RXD/TXD (PB6/PB7) |
| 23/24 | RS232_0 TX/RX | RS232_TXD0/RXD0 | — | 經 Sheet 5 U0503 到 MCU |

---

## 7. ADC 類比輸入（進 MCU ADC）

| MCU ADC Pin | Net | 來源 | 用途 |
|:-:|---|---|---|
| PP4 | oil_ADC_IN      | Sheet 11 R 分壓 ← J0901 pin 4 | 油量感測器 |
| PP5 | LTE_WAKE_MCU_3V3 | Sheet 11 shifter ← LTE_WAKE_MCU (1V8) | 可能是 ADC 讀訊號強度 / 或單純 GPIO wake |
| PP6 | VBAT_ADC_IN     | Sheet 8 分壓 100K/100K ← VBAT | 電池電壓偵測 |
| PP7 | VBAT_NTC        | Sheet 8 ← NTC（NM 預留）  | 電池溫度 |

---

## 8. RF 訊號

| Net | 源 | 目的 | 阻抗 |
|---|---|---|:-:|
| 50OHM_BT_ANT | Sheet 1 pin 3 ANT via R0101 50Ω | Sheet 13 BT matching | 50Ω microstrip |
| 50ohm_MAIN_ANT | Sheet 2 pin 36 ANT_MAIN | Sheet 13 LTE matching → U1301 tuner → 鋼片天線 | 50Ω |
| 50OHM_LTE_TX_RF1~4 | Sheet 13 U1301 MXD8544AE 4 condenser | Sheet 13 matching | 50Ω each |
| SDR_GRFC_1 / SDR_GRFC_2 | Sheet 2 EG800Q (pin 67/104) | Sheet 13 U1301 control | 數位 1V8 控制 |
| NFC_TX1 / NFC_TX2 | Sheet 10 PN7160 pin 21/19 | Sheet 14 matching → J1401 → 天線 | 差分 |
| NFC_RXP / NFC_RXN | Sheet 14 ← C1414/C1401 (RX 耦合) | Sheet 10 PN7160 pin 16/15 | 差分 |

---

## 9. USB (LTE Debug)

| Net | Source | Sink | 用途 |
|---|---|---|---|
| USB_VBUS | Sheet 2 TP0210 | Sheet 2 EG800Q pin 61 | USB 5V input |
| USB_DP | Sheet 2 TP0211 | Sheet 2 EG800Q pin 59 | USB D+ |
| USB_DM | Sheet 2 TP0212 | Sheet 2 EG800Q pin 60 | USB D- |
| LTE_USB_BOOT | Sheet 2 TP0208/09 | Sheet 2 EG800Q pin 82 | Download mode trigger |

---

## 10. Net Source/Sink Cross-Reference 總結

**跨 >3 sheet 的「公用 net」**：
- MCU_3V3：Sheet 1 (source via internal LDO 或 MCU_GNSS_3V3) → Sheet 8/11/其他 pull-up 用
- GND：所有 sheet
- VDD_LDO_4V：Sheet 7 → Sheet 2, 7, 12
- LTE_EXT_1V8：Sheet 2 → Sheet 3, 11, 13
- NFC_1V8：Sheet 7 → Sheet 1, 3, 10

**最長 trace net（可能影響 EMC/SI）**：
- `GNSS_RXD / TXD`：MCU → Sheet 1 → Sheet 12 → FPC → GNSS 子板。總長可能 >50mm，但 UART 9600-115200 bps 無瓶頸
- `50ohm_MAIN_ANT`：LTE → 多段 matching → tuner → 鋼片天線。需 50Ω 控制與 shielding
- `MCU_CAN_RXD / TXD / H / L`：MCU → Sheet 5 → J0901 → 車上線束。EMC 關鍵
