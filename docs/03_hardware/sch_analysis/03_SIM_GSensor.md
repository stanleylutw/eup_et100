# Sheet 3 — 翻蓋 SIM 卡座 + G-Sensor

**PDF Page**：3 / 15
**Ref Designator Prefix**：03xx
**圖面標題**：翻蓋 SIM 卡；G-Sensor
**對應提案書章節**：[02_客戶需求.md §P04 SIM 翻蓋式熱插拔 V1.8 紅字](../../00_project/02_客戶需求.md)、[04_P28 G+Sensor SC7U22](../../00_project/04_技術方案_關鍵零件.md)

---

## 1. 功能定位

兩個小塊：
- **翻蓋 SIM 卡座**：接 LTE 模組（Sheet 2）的 USIM 介面，加 ESD 保護陣列、series resistor。V1.8 客戶需求強調翻蓋式、熱插拔。
- **G-Sensor**：SC7U22TR 6 軸 IMU（提案書 §P28）—— 3 軸加速度 + 3 軸陀螺儀，I2C 給 MCU。用於撞擊偵測、傾斜、靜止判斷（stop/go）、G 力事件觸發。

---

## 2. 關鍵元件

### 2a. SIM

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| J0301 | MUP-C783-1 | 翻蓋式 | nano-SIM 卡座，含 DETECTION_SWITCH | | 7 pin + SW |
| R0301~R0304 | 68R | 0201 | SIM VCC/RST/CLK/IO series damping | | EMC + 保護短路電流 |
| D0303~D0306 | PESDNC2FD5V8 ×4 | SOD-523 | SIM 四條線 ESD 保護 | | |
| C0301 | 100nF | 0402 | SIM_VCC 本地 bypass | | |
| C0302/03/04 | 33nF | 0402 | SIM_RST/CLK/IO 線上濾波 | | |
| R0301 | 15K | 0603 (1/20W) | SIM_IO pull-up（可能是給熱插拔偵測的邏輯） | | |
| R0302 | 51K | 0603 (1/20W) | DETECTION_SWITCH pull-up | | 配 SIM_DET → LTE 模組偵測插拔 |
| TP0301~TP0305 | — | TP | SIM 五條線的 debug 探針點 | | |

### 2b. G-Sensor

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| U0301 | **SC7U22TR** | LGA-14 2.5×3.0mm | 6-axis IMU（3-axis accel + 3-axis gyro） | | ±2/4/8/16g, ±125/250/500/1000/2000 dps |
| R0307 | 0R (1/16W) | — | G-Sensor_1V8 input series | | |
| R0308 | 0R | — | G-Sensor_3V3 input series | | 分兩路電源：1V8 digital, 3V3 analog |
| R0313 | NM 4.7K | — | SCX/SDX（SPI 選用）| **NM** | 本板用 I2C 模式，SPI 腳位接 pull-up disable |
| R0313/R0314 | 4.7K / NM 4.7K | 0402 | I2C 相關 pull-up | | 1V8 I2C bus |
| C0305 | 100nF | — | VDD_3V3 bypass | | |
| C0306 | 100nF | — | VDDIO bypass | | |

**SC7U22 pin 功能**：
| Pin | Name | 用途 |
|:-:|---|---|
| 1 | SDO | SPI MISO / I2C LSB of addr |
| 2 | ASDX | SPI data in / I2C SDA |
| 3 | ASCX | SPI CLK / I2C SCL |
| 4 | INT1 | 中斷 1 → G-Sensor_INT 到 MCU |
| 5 | VDDIO | 1V8（NFC_1V8） |
| 6/7 | GND | |
| 8 | VDD | 3V3 (G-Sensor_3V3) |
| 9 | INT2 | 保留 |
| 10 | OCSB | 不用 |
| 11 | OSDO | 不用 |
| 12 | CSB | I2C/SPI 模式選擇 pin（接 4.7K pull-up → I2C 模式） |
| 13 | SCX | 本板走 I2C → 接 Sensor_I2C0_SCL |
| 14 | SDX | 本板走 I2C → 接 Sensor_I2C0_SDA |

---

## 3. 電源輸入

| Rail | 來源 | V | 消費 |
|---|---|:-:|---|
| LTE_EXT_1V8 | ← Sheet 2 VDD_EXT | 1.8V | SIM 卡 VCC（50mA 標註） |
| VCC_1V8 | ← Sheet 7 NFC_1V8 | 1.8V | SIM pull-ups (R0301 15K)、G-Sensor VDDIO、G-Sensor I2C pull-ups |
| VDD_3V3 | ← Sheet 7 U0702 | 3.3V | G-Sensor VDD（analog + core）、SIM DETECTION_SWITCH pull-up (R0302 51K) |

**SIM 卡 VCC 的 1V8/3V class**：由 LTE 模組（EG800Q-EU）自動切換 class B（1.8V）/ class A（3V）—— 本 sheet SCH 看到 SIM_VCC = `LTE_EXT_1V8`，實際上 LTE 內部會把 VDD_EXT 經切換電路給 USIM_VDD。

---

## 4. 介面（I/O Nets）

### SIM
| Net | 方向 | 源 / 目的 |
|---|:-:|---|
| SIM_VCC | in | ← Sheet 2 USIM_VDD |
| SIM_RST | in | ← Sheet 2 USIM_RST |
| SIM_CLK | in | ← Sheet 2 USIM_CLK |
| SIM_IO | ↔ | ↔ Sheet 2 USIM_DATA |
| SIM_DET | out | → Sheet 2 USIM_DET（SIM 插拔偵測）|

### G-Sensor
| Net | 方向 | 源 / 目的 |
|---|:-:|---|
| Sensor_I2C0_SDA | ↔ | Sheet 1 MCU（1V8 域） |
| Sensor_I2C0_SCL | ↔ | Sheet 1 MCU（1V8 域） |
| G-Sensor_INT | out | → Sheet 1 MCU（事件中斷） |

---

## 5. 設計細節

- **SIM 線上 68R series + 33nF filter**：68R 限制短路電流（SIM 短路 to GND 時最大 ~26mA @ 1.8V），加 33nF 做低通濾波（fc = 1/(2π×68×33nF) ≈ 70kHz），對 SIM 標準 clock（max 4MHz）略高但有助於 ESD 突波的 slew rate 限制。
- **SIM_DET 用 DETECTION_SWITCH**：翻蓋式 SIM holder 機械彈片，蓋上時短路到 GND（或開路，看型號）。R0302 51K pull-up 到 VDD_3V3，LTE 側 USIM_DET 讀低時 = SIM 已插入。**V1.8 要求熱插拔**，MCU/LTE 要能處理：SIM_DET edge → LTE 重新初始化 USIM → 若無卡則通知 MCU。
- **G-Sensor 兩電源 (3V3 + 1V8)**：IMU 常見配置 —— analog 核心用 3V3 保證精度，I2C/SPI 介面用 1V8 配 MCU 的 1V8 域。
- **G-Sensor_INT 一條線**：SC7U22 有 INT1/INT2 兩路，本板只拉 INT1；代表韌體需設 INT1 承擔多種事件（motion、tap、free-fall、orientation 共用一條線），用 interrupt source register 分辨。

---

## 6. 疑點 / Review Notes

- [ ] **🟠 SIM 熱插拔金手指壽命**：翻蓋式比抽屜式耐插拔，但 nano-SIM 金手指規格 insertion/removal ~1000 次（典型 Molex spec）；若車隊使用場景（司機換手頻繁），壽命可能不夠；若是「設定一次後固定」就 OK。**V1.8 強調熱插拔但沒說多久插拔一次 → 要在 SOW 澄清應用場景**。
- [ ] **📘 G-Sensor_INT 只接一支**：事件類型分辨要靠 SW 讀 register；若多事件同時觸發可能漏處理，一般韌體有事件 queue 機制可以吸收。
- [ ] **📘 I2C bus 分配**：
  - Sensor_I2C0（1V8 域）：G-Sensor + ？其他感測器（本 sheet 僅 G-Sensor；如果以後要加更多 sensor 可擴展）
  - NFC_I2C3（1V8 域）：NFC（Sheet 10）
  - EXP_I2C5（3V3 域）：IO expander AW9523（Sheet 11）
  - 分三條獨立 bus 很浪費 MCU 資源，但各 bus 速度 / 電壓 / 中斷獨立性比較好；本設計選擇對（避開 1V8/3V3 混用的 risk）。
- [ ] **📘 SC7U22 datasheet 來源**：SC7U22 為「賽昇科技 / Silan Microelectronics」或類似國產 IMU，階段 B 要找 datasheet 核：
  - typical active current（提案書 P14 用 30mA 估算，SC7U22 典型可能 <5mA → **功耗預算有鬆動空間**）
  - I2C 位址（可能 0x6A/6B，由 SDO/SA0 決定）
  - FIFO 深度（決定 MCU 讀取頻率）
- [ ] **DNP**：R0313 SPI 相關 pull-up 可能 NM（本板走 I2C），需核。
- [ ] **1V8 域 I2C pull-up 位置**：pull-up 到 VCC_1V8，與 NFC I2C 同 1V8 來源但不同 bus；若 NFC_1V8_EN 關閉時 I2C pull-up 也沒了，但 G-Sensor VDDIO 也同時沒電，整體 bus 下電，MCU 側不會看到異常 pulse —— OK。

---

## 7. 參考

- SCH PDF：Sheet 3
- 相關 sheet：Sheet 2（SIM 到 LTE）、Sheet 1（I2C + 中斷）、Sheet 7（NFC_1V8、VDD_3V3）
- 提案書章節：[02_P04 SIM 熱插拔要求](../../00_project/02_客戶需求.md)、[04_P28 SC7U22](../../00_project/04_技術方案_關鍵零件.md)
- Datasheet：SC7U22（階段 B 待尋）
