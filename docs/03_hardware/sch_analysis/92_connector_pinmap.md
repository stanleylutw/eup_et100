# 對外連接器 Pinmap 彙整

**來源**：綜合 Sheet 2（SIM 與 LTE module I/F）、Sheet 3（SIM 卡）、Sheet 5（RS232/CAN）、Sheet 8（電池）、Sheet 9（26pin 主連接器）、Sheet 12（GNSS 子板 FPC）、Sheet 13（LTE/BT RF 耦合器）、Sheet 14（NFC 天線）。

> 階段 A 彙整；量產線束規範以 Quectel 正式線束圖為準。

> **2026-10-02 SWD 校正**：debug TP 網名是舊 SCH 記錄，不是已確認的 SWD pinmux／VTref。v0.4.9 未提供 SWD alternate-function 或逐 pin voltage-bank map；需核對 board revision、TP 到 GPIO 的 routing 及 debug domain 電壓，不能只憑 MCU_3V3 TP 或舊 1V8 域描述決定 debugger VTref。見 [開發對照](../fr306x_reference/05_et100_development_reference.md)。

---

## 1. J0901 — 外部 26pin 主連接器（Sheet 9）

**料號**：C2012RD21307T0102BD（2.0mm pitch，一字排 2×13）

| Pin | 訊號 | 色 | 規格 | SCH net | 對應模組 |
|:-:|---|:-:|---|---|---|
| 1  | PWR_IN (B+)         | 紅 | 9-36V, 線束 1A fuse  | PWR_IN            | Sheet 6 DCDC |
| 2  | GND                  | 黑 | — | GND | — |
| 3  | ACC（點火）         | 藍 | >5V 觸發 | ACC_INT_IN_CON | Sheet 11 Q1101 |
| 4  | ADC（油量）          | 綠 | 0-33V | oil_ADC_IN_CON | Sheet 11 → MCU ADC |
| 5  | 1-Wire 溫度          | 白 | DS18B20 ×4 | 1_Wire_Temp_IN_CON | Sheet 11 Q1116/Q1117 |
| 6  | GND                  | 黑 | — | GND | — |
| 7  | Input1               | 青 | >5V | IO1_INT_IN_CON | Sheet 11 Q1103 |
| 8  | Input2               | 紫 | >5V | IO2_INT_IN_CON | Sheet 11 Q1105 |
| 9  | Input3               | 棕 | >5V | IO3_INT_IN_CON | Sheet 11 Q1107 |
| 10 | Output（負輸出）     | 灰 | 200mA | PWR_SW_CTRL_CON | Sheet 11 Q1108 PFET |
| 11 | RS232_1 TX           | 黃 | 9600 | RS232_TXD1 | Sheet 5 U0501 |
| 12 | GND                  | 黑 | — | GND | — |
| 13 | RS232_2 TX           | 黃 | 9600 | RS232_TXD2 | Sheet 5 U0502 |
| 14 | ACC_OUT              | 紅 | — | ACC_INT_OUT_CON | Sheet 11 Q1110 collector（共用 ×5）|
| 15 | RS232_2 RX           | 綠 | 9600 | RS232_RXD2 | Sheet 5 U0502 |
| 16 | ACC_OUT              | 紅 | — | ACC_INT_OUT_CON | 共用（同 pin 14） |
| 17 | GND                  | 黑 | — | GND | — |
| 18 | ACC_OUT              | 紅 | — | ACC_INT_OUT_CON | 共用 |
| 19 | CAN_H                | 黃 | J1939 | CAN_H | Sheet 5 SIT1042 經 F0501 fuse |
| 20 | CAN_L                | 綠 | J1939 | CAN_L | Sheet 5 SIT1042 經 F0502 fuse |
| 21 | GND                  | 黑 | — | GND | — |
| 22 | ACC_OUT              | 紅 | — | ACC_INT_OUT_CON | 共用 |
| 23 | RS232_0 TX           | 黃 | 115200（原 DB9） | RS232_TXD0 | Sheet 5 U0503 |
| 24 | RS232_0/1 RX（待釐清） | 綠 | | RS232_RXD0 / RXD1？ | Sheet 5 |
| 25 | GND                  | 黑 | — | GND | — |
| 26 | ACC_OUT              | 紅 | — | ACC_INT_OUT_CON | 共用 |

**對外的 5 條子線束**（提案書 P41）：
1. 10-pin wire：P1-P10 → 溫度 + 電源 + 數位 I/O（線長 50cm）
2. CAN 4-pin wire：P17-P19 + ACC（線長 30cm）
3. RS232_1 4-pin wire：P11/P12/P14/P17 等（線長 30cm）
4. RS232_2 4-pin wire：P13/P15/P16/P18（線長 30cm）
5. RS232_0 4-pin wire：P23/P24/P25/P26（線長 30cm）

**待釐清**：
- Pin 24 到底對應 RXD0 還是 RXD1（SCH 標籤重複）
- 5 條 ACC_OUT 線束是否該改為獨立 driver 以避免共用 fault

---

## 2. J0801 — 電池連接器（Sheet 8）

**料號**：WF15003-01207（3-pin JST 相容）

| Pin | 推測 | 狀態 |
|:-:|---|---|
| 1 | B+（+極） | **🔴 未確認** |
| 2 | NTC（溫度感測，與 pull-up 配合） | **🔴 未確認** |
| 3 | B-（GND） | **🔴 未確認** |

**提醒**：SCH 圖面紅圈「电池 pin 序？」—— EVT 樣品前必須與電池廠書面確認。接錯可能燒電池或炸 charger。

---

## 3. J0301 — SIM 卡座（Sheet 3）

**料號**：MUP-C783-1（翻蓋式 nano-SIM）

| Pin | ISO 7816 | SCH net | 備註 |
|:-:|---|---|---|
| C1 | VCC | SIM_VCC | ← Sheet 2 USIM_VDD（1.8V/3V class 自動切） |
| C2 | RST | SIM_RST | ← Sheet 2 USIM_RST |
| C3 | CLK | SIM_CLK | ← Sheet 2 USIM_CLK |
| C5 | GND | GND | 7 支 GND pad 其一 |
| C6 | VPP | — | NC（nano-SIM 不用） |
| C7 | I/O | SIM_IO | ↔ Sheet 2 USIM_DATA |
| SW | DETECTION | SIM_DET | → Sheet 2 USIM_DET（翻蓋機械開關） |

5 TP（TP0301-TP0305）給 SIM 五條訊號線量測。

---

## 4. J1201 — GNSS 子板 FPC 連接器（Sheet 12）

**料號**：TF31-8S-0.5SH(800)（0.5mm pitch 8-pin FPC）

| Pin | Net | 方向（主板視角） | 用途 |
|:-:|---|:-:|---|
| 1 | GND | — | — |
| 2 | GNSS_VBAT | out | 備援 VBAT（給 GNSS chip 的 VBAT_BACKUP） |
| 3 | GNSS_TXD | in | ← GNSS NMEA UART TX |
| 4 | GNSS_RXD | out | → GNSS UART RX |
| 5 | GNSS_DATA_IN_EINT | in | ← 1PPS / data ready 中斷 |
| 6 | GNSS_LDO_EN | out | → GNSS 子板 LDO EN |
| 7 | GNSS_3V3 | out | → GNSS 子板主 3V3 |
| 8 | GND | — | — |

6 TP（TP1201-TP1206）給六條訊號線量測。

---

## 5. J1301 / J1302 — RF 耦合器（Sheet 13）

| Ref | 用途 | 說明 |
|---|---|---|
| J1301 | LTE 主路耦合器 | 818011998 directional coupler，4-pin（IN/OUT/CP1/CP2），正常時 IN→OUT 幾乎無損，CP 端輸出耦合訊號到工廠測試儀 |
| J1302 | BT 主路耦合器 | 同型號 |

**非線束連接器**，是工廠量產測試用的 RF probe tap。

---

## 6. J1401 / J1402 — NFC 天線座（Sheet 14）

| Ref | 用途 | 料號 |
|---|---|---|
| J1401 | NFC 天線主連接座 | 3.2×0.8×1896（彈片式 3-pin：SIG1 / SIG2 / GND） |
| J1402 | NFC 天線預留座 | 81800A286（2-pin：SIG / GND2） |

NFC 天線本體（40×30mm，提案書 P35）貼在上殼內側，透過彈片與 J1401 連接。J1402 保留給可能的機構變體。

---

## 7. HOLE1301~HOLE1306 — LTE / BT 鋼片天線 PTH（Sheet 13）

| Ref | 用途 |
|---|---|
| HOLE1301 ~ HOLE1304 | LTE 鋼片天線 4 固定孔 |
| HOLE1305 / HOLE1306 | BT 鋼片天線 2 固定孔 |

鋼片天線透過 PTH + spring finger 連接到 PCB RF trace，非線束連接器。

---

## 8. 定位 PTH（Sheet 9）

**料號**：pth_cir1_80_3_50（機械定位孔）

| Ref | 用途 |
|---|---|
| HOLE0901 ~ HOLE0906 | 6 支 PCBA 到殼體的定位柱 |

TP0916 / TP0917 標為 DET（夾具定位點），用於量產測試治具的對位。

---

## 9. Debug / 量產測試 TP 清單（粗分）

| Sheet | TP 編號 | 用途 |
|:-:|---|---|
| 1 | TP0104/105/106/107/108 | MCU_3V3、SWCLK、SWDIO、空、MCU_RST |
| 2 | TP0201-TP0214 | LTE debug UART / RESET / BOOT / USB / STATUS / SIM_DET |
| 3 | TP0301-TP0305 | SIM 五線 |
| 4 | TP0401 | LED_3V3 |
| 5 | TP0501 | VDD_PP_3V3 (RS232 power switch output) |
| 6 | TP0601-TP0604 | PWR_IN / VDD_5V / VDD_5V_MCU / VDD_4V 四 rail test point |
| 7 | TP0701-TP0704 | 四顆 LDO output |
| 8 | TP0801-TP0803 | 充電器 VIN / VBAT / 電池接點 |
| 9 | TP0901-TP0917 | 外部 26pin 全部 17 條訊號 |
| 10 | TP1001 | NFC_VDD |
| 11 | — | 無獨立 TP（訊號走到 Sheet 1 的 TP 群） |
| 12 | TP1201-TP1206 | GNSS 6 條訊號 |
| 13 | TP0201 等（LTE 相關已在 Sheet 2） | — |
| 14 | 無 | NFC 天線走 J1401 直接量 |

**總計 ~50 個 TP**，量產 ICT 覆蓋率應該充足。
