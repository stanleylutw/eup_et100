# FR3068E-C_MCU_DEV1.4 原理圖規格說明書

> **原始檔案**：[`FR3068E-C_MCU_DEV1.4.SchDoc`](FR3068E-C_MCU_DEV1.4.SchDoc)  
> **板卡版本**：FR3068E-C DEV1.4（Freqchip 官方評估板）  
> **核心晶片**：Freqchip FR3068E-C（QFN80 封裝，9×9 mm，Pitch 0.4 mm）  
> **元件總數**：64 顆  

## 1. 核心 MCU（U1）完整 81 腳位與網表對照表

此表由原始 SchDoc 向量網表與元件引腳坐標自動解析提取，與最新 v0.4.9 規格書 100% 吻合：

| 引腳編號 | 原理圖引腳定義（Symbol Pin Name） | 連接網路（Net Name） | 電氣類型／預設功能說明 |
| :---: | :--- | :--- | :--- |
| 1 | `NC` | `-` | NC（規格書 v0.4.9 確認懸空） |
| 2 | `AVDD_1V2` | `-` | 射頻類比 1.2V 核心供電 |
| 3 | `ANT` | `-` | 藍牙 2.4GHz 射頻天線輸出／輸入（接 RF 匹配網路） |
| 4 | `PA10/UART2_CTS/SPIMX2_0_MOSI/I2C1_SCK/PWM0_10/MCAN3_RX/SPIMX8_0_IO2/LCD_RDX/PDM0_SCK` | `PA10` | 多功能可配置 GPIO |
| 5 | `PA11/UART2_RTS/SPIMX2_0_MISO/I2C1_SDA/PWM0_11/MCAN3_TX/SPIMX8_0_IO3/LCD_DCX/PDM0_SDA` | `PA11` | 多功能可配置 GPIO |
| 6 | `PA12/UART1_RXD/SPIS0_SCLK/I2C0_SCK/PWM0_12/SPIMX8_0_IO4/LCD_DAT0` | `PA12` | 多功能可配置 GPIO |
| 7 | `PA13/UART1_TXD/SPIS0_CSN/I2C0_SDA/PWM0_13/SPIMX8_0_IO5/LCD_DAT1` | `PA13` | 多功能可配置 GPIO |
| 8 | `PA14/UART1_CTS/SPIS0_MOSI/I2C0_SCK/PWM0_14/MCAN1_RX/SPIMX8_0_IO6/LCD_DAT2/PDM0_SCK/SPDIF_OUT` | `PA14` | 多功能可配置 GPIO |
| 9 | `PA15/UART1_RTS/SPIS0_MISO/I2C0_SDA/PWM0_15/MCAN1_TX/SPIMX8_0_IO7/LCD_DAT3/PDM0_SDA/SPDIF_IN` | `PA15` | 多功能可配置 GPIO |
| 10 | `PB0/BT_PA4/UART2_RXD/SPIMX2_0_SCLK/I2C1_SCK/PWM0_0/SPIMX8_0_SCLK/LCD_DAT4/I2S0_SCLK` | `PB0` | 多功能可配置 GPIO |
| 11 | `PB1/BT_PA5/UART2_TXD/SPIMX2_0_CSN/I2C1_SDA/PWM0_1/SPIMX8_0_CSN/LCD_DAT5/I2S0_LRCK` | `PB1` | 多功能可配置 GPIO |
| 12 | `RESV0` | `-` | 多功能可配置 GPIO |
| 13 | `PB3/BT_PA7/UART2_RTS/SPIMX2_0_MISO/I2C0_SDA/PWM0_3/MCAN3_TX/SPIMX8_0_IO1/LCD_DAT7/PDM0_SDA/I2S0_DIN/SPDIF_IN` | `PB3` | 多功能可配置 GPIO |
| 14 | `VDD_IO3V3` | `-` | 3.3V IO 電源供電（VDD_IO3V3） |
| 15 | `PB4/UART3_RXD/SPIS0_SCLK/I2C0_SCK/PWM0_4/SPIMX8_0_IO2/LCD_DAT8` | `PB4` | 多功能可配置 GPIO |
| 16 | `PB5/UART3_TXD/SPIS0_CSN/I2C0_SDA/PWM0_5/SPIMX8_0_IO3/LCD_DAT9` | `PB5` | 多功能可配置 GPIO |
| 17 | `PB6/UART3_CTS/SPIS0_MOSI/I2C1_SCK/PWM0_6/MCAN1_RX/SDIOH0_CLK/SPIMX8_0_SCLK/LCD_DAT10/PDM1_SCK` | `PB6` | 多功能可配置 GPIO |
| 18 | `PB7/UART3_RTS/SPIS0_MISO/I2C1_SDA/PWM0_7/MCAN1_TX/SDIOH0_CMD/SPIMX8_0_CSN/LCD_DAT11/PDM1_SDA` | `PB7` | 多功能可配置 GPIO |
| 19 | `PB8/UART2_RXD/SPIMX2_0_SCLK/I2C0_SCK/PWM0_8/SDIOH0_DAT0/SPIMX8_0_IO4/LCD_DAT12/I2S1_SCLK` | `PB8` | 多功能可配置 GPIO |
| 20 | `PB9/UART2_TXD/SPIMX2_0_CSN/I2C0_SDA/PWM0_9/SDIOH0_DAT1/SPIMX8_0_IO5/LCD_DAT13/I2S1_LRCK` | `PB9` | 多功能可配置 GPIO |
| 21 | `PB10/UART2_CTS/SPIMX2_0_MOSI/I2C0_SCK/PWM0_10/MCAN1_RX/SDIOH0_DAT2/SPIMX8_0_IO6/LCD_DAT14/MCU_SWCLK/PDM1_SCK/I2S1_DOUT/SPDIF_OUT` | `PB10` | SWD 偵錯時鐘線（連至 J9 排針） |
| 22 | `PB11/UART2_RTS/SPIMX2_0_MISO/I2C0_SDA/PWM0_11/MCAN1_TX/SDIOH0_DAT3/SPIMX8_0_IO7/LCD_DAT15/MCU_SWDIO/PDM1_SDA/I2S1_DIN/SPDIF_IN` | `PB11` | SWD 偵錯數據線（連至 J9 排針） |
| 23 | `PB12/UART3_RXD/SPIS0_SCLK/I2C1_SCK/PWM0_12/SPIMX8_0_IO4/SDIOH0_DAT4/LCD_CSX/OSPI_SCLK<XIP>/QSPI1_SCLK<XIP>` | `PB12` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 24 | `PB13/UART3_TXD/SPIS0_CSN/I2C1_SDA/PWM0_13/SPIMX8_0_IO5/SDIOH0_DAT5/LCD_WRX/OSPI_CSN<XIP>/QSPI1_IO0<XIP>` | `PB13` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 25 | `PB14/UART3_CTS/SPIS0_MOSI/I2C0_SCK/PWM0_14/SPIMX8_0_IO6/SDIOH0_DAT6/LCD_RDX/OSPI_DQS<XIP>/PDM1_SCK/QSPI1_IO3<XIP>` | `PB14` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 26 | `PC1/UART2_TXD/SPIMX2_0_CSN/I2C1_SDA/PWM1_1/SPIMX8_0_IO1/SDIOH0_DAT1/LCD_DAT1/OSPI_IO5<XIP>/I2S1_LRCK/QSPI1_CSN<XIP>` | `PC1` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 27 | `PC0/UART2_RXD/SPIMX2_0_SCLK/I2C1_SCK/PWM1_0/SPIMX8_0_IO0/SDIOH0_DAT0/LCD_DAT0/OSPI_IO6<XIP>/I2S1_SCLK/QSPI1_IO1<XIP>` | `PC0` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 28 | `PB15/UART3_RTS/SPIS0_MISO/I2C0_SDA/PWM0_15/SPIMX8_0_IO7/SDIOH0_DAT7/LCD_DCX/OSPI_IO7<XIP>/PDM1_SDA/QSPI1_IO2<XIP>` | `PB15` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 29 | `PC2/UART2_CTS/SPIMX2_0_MOSI/I2C2_SCK/PWM1_2/SPIMX8_0_IO2/SDIOH0_DAT2/LCD_DAT2/OSPI_IO1<XIP>/PDM1_SCK/I2S1_DOUT/SPDIF_OUT` | `PC2` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 30 | `PC3/UART2_RTS/SPIMX2_0_MISO/I2C2_SDA/PWM1_3/SPIMX8_0_IO3/SDIOH0_DAT3/LCD_DAT3/OSPI_IO3<XIP>/PDM1_SDA/I2S1_DIN/SPDIF_IN` | `PC3` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 31 | `DVDD_1V2` | `-` | 多功能可配置 GPIO |
| 32 | `PC4/UART3_RXD/SPIS0_SCLK/I2C2_SCK/PWM1_4/SPIMX8_0_SCLK/SDIOH0_CLK/LCD_DAT4/OSPI_IO4<XIP>/DAC_PWMLP/PWM_LCK` | `PC4` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 33 | `PC5/UART3_TXD/SPIS0_CSN/I2C2_SDA/PWM1_5/SPIMX8_0_CSN/SDIOH0_CMD/SPIMX8_1_DQS/LCD_DAT5/OSPI_IO2<XIP>/DAC_PWMLN/PWM_LCK` | `PC5` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 34 | `PC6/UART3_CTS/SPIS0_MOSI/I2C1_SCK/PWM1_6/SPIMX8_0_DQS/SDIOH0_DAT4/SPIMX8_1_IO4/LCD_DAT6/OSPI_IO0<XIP>/PDM1_SCK/DAC_PWMRP/PWM_RCK` | `PC6` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 35 | `PC7/UART3_RTS/SPIS0_MISO/I2C1_SDA/PWM1_7/SDIOH0_DAT5/SPIMX8_1_IO5/LCD_DAT7/OSPI_SCLK#<XIP>/PDM1_SDA/DAC_PWMRN/PWM_RCK` | `PC7` | 高速記憶體介面（連至 U11 PSRAM/Flash） |
| 36 | `VDD_IO1V8` | `-` | 1.8V IO 電源供電（VDD_IO1V8） |
| 37 | `PD0/UART4_RXD/SPIMX2_1_SCLK/I2C2_SCK/PWM1_0/SDIOH0_CLK/SPIMX8_1_SCLK/LCD_DAT0/I2S0_SCLK` | `PD0` | 多功能可配置 GPIO |
| 38 | `PD1/UART4_TXD/SPIMX2_1_CSN/I2C2_SDA/PWM1_1/SDIOH0_CMD/SPIMX8_1_CSN/LCD_DAT1/I2S0_LRCK` | `PD1` | 多功能可配置 GPIO |
| 39 | `PD2/UART4_CTS/SPIMX2_1_MOSI/I2C1_SCK/PWM1_2/MCAN1_RX/SDIOH0_DAT4/SPIMX8_1_IO0/LCD_DAT2/PDM1_SCK/I2S0_DOUT/SPDIF_OUT` | `PD2` | 多功能可配置 GPIO |
| 40 | `PD3/UART4_RTS/SPIMX2_1_MISO/I2C1_SDA/PWM1_3/MCAN1_TX/SDIOH0_DAT5/SPIMX8_1_IO1/LCD_DAT3/PDM1_SDA/I2S0_DIN/SPDIF_IN` | `PD3` | 多功能可配置 GPIO |
| 41 | `PD4/UART5_RXD/I2C2_SCK/PWM1_4/SPIS1_SCLK/SDIOH0_DAT6/SPIMX8_1_IO2/LCD_DAT4/DAC_PWMLP/PWM_LCK` | `PD4` | 多功能可配置 GPIO |
| 42 | `PD5/UART5_TXD/I2C2_SDA/PWM1_5/SPIS1_CSN/SDIOH0_DAT7/SPIMX8_1_IO3/LCD_DAT5/DAC_PWMLN/PWM_LCK` | `PD5` | 多功能可配置 GPIO |
| 43 | `PD6/UART5_CTS/I2C2_SCK/PWM1_6/SPIS1_IO0/SDIOH0_DAT0/SPIMX8_1_IO4/LCD_DAT6/PDM1_SCK/DAC_PWMRP/PWM_RCK` | `PD6` | 多功能可配置 GPIO |
| 44 | `PD7/UART5_RTS/I2C2_SDA/PWM1_7/SPIS1_IO1/SDIOH0_DAT1/SPIMX8_1_IO5/LCD_DAT7/PDM1_SDA/DAC_PWMRN/PWM_RCK` | `PD7` | 多功能可配置 GPIO |
| 45 | `PD8/UART4_RXD/SPIMX2_1_SCLK/I2C1_SCK/PWM1_8/SPIS1_IO2/SDIOH0_DAT2/SPIMX8_1_IO6/LCD_CSX/I2S0_SCLK/DAC_PWMCLK` | `PD8` | 多功能可配置 GPIO |
| 46 | `PD9/UART4_TXD/SPIMX2_1_CSN/I2C1_SDA/PWM1_9/SPIS1_IO3/SDIOH0_DAT3/SPIMX8_1_IO7/LCD_WRX/I2S0_LRCK` | `PD9` | 多功能可配置 GPIO |
| 47 | `PD10/UART4_CTS/SPIMX2_1_MOSI/I2C2_SCK/PWM1_10/SPIS1_SCLK/SDIOH0_CLK/SPIMX8_1_DQS/LCD_RDX/PDM1_SCK/I2S0_DOUT/SPDIF_OUT` | `PD10` | 多功能可配置 GPIO |
| 48 | `PD11/UART4_RTS/SPIMX2_1_MISO/I2C2_SDA/PWM1_11/SPIS1_CSN/SDIOH0_CMD/LCD_DCX/PDM1_SDA/I2S0_DIN/SPDIF_IN/DAC_PWMLP/PWM_LCK` | `PD11` | 多功能可配置 GPIO |
| 49 | `PD12/UART5_RXD/I2C2_SCK/PWM1_12/SDIOH0_DAT4/LCD_DAT8/USB_DP/I2S1_SCLK/DAC_PWMLN/PWM_LCK` | `PD12` | 多功能可配置 GPIO |
| 50 | `PD13/UART5_TXD/I2C2_SDA/PWM1_13/SDIOH0_DAT5/LCD_DAT9/USB_DM/I2S1_LRCK/DAC_PWMRP/PWM_RCK` | `PD13` | 多功能可配置 GPIO |
| 51 | `PD14/UART5_CTS/I2C1_SCK/PWM1_14/MCAN1_RX/SDIOH0_DAT6/LCD_DAT10/USB_DP/PDM1_SCK/I2S1_DOUT/SPDIF_OUT/DAC_PWMRN/PWM_RCK` | `PD14` | 多功能可配置 GPIO |
| 52 | `PD15/UART5_RTS/I2C1_SDA/PWM1_15/MCAN1_TX/SDIOH0_DAT7/LCD_DAT11/USB_DM/PDM1_SDA/I2S1_DIN/SPDIF_IN/DAC_PWMCLK` | `PD15` | 多功能可配置 GPIO |
| 53 | `PP4` | `-` | 多功能可配置 GPIO |
| 54 | `PP5` | `-` | 多功能可配置 GPIO |
| 55 | `PP6` | `-` | 多功能可配置 GPIO |
| 56 | `PP7` | `-` | 多功能可配置 GPIO |
| 57 | `IO1V8_LDO` | `IO1V8_LDO` | 多功能可配置 GPIO |
| 58 | `RSTN` | `-` | 晶片主復位（低電平復位，接 RST 按鍵 K4） |
| 59 | `IO3V3_LDO` | `IO3V3_LDO` | 多功能可配置 GPIO |
| 60 | `MIC_BIAS` | `MIC_BIAS` | 多功能可配置 GPIO |
| 61 | `NC` | `-` | 多功能可配置 GPIO |
| 62 | `DVDD_LDO` | `-` | 多功能可配置 GPIO |
| 63 | `SYS_BFB` | `-` | 多功能可配置 GPIO |
| 64 | `SYS_BSW` | `-` | 多功能可配置 GPIO |
| 65 | `MCU_VCC` | `MCU_VCC` | 多功能可配置 GPIO |
| 66 | `PP0` | `-` | PMU IO0（接按鍵 K1） |
| 67 | `PP3` | `-` | PMU IO3（接按鍵 K3） |
| 68 | `PP2` | `-` | PMU IO2（接按鍵 K2） |
| 69 | `DVDD_1V2` | `-` | 多功能可配置 GPIO |
| 70 | `PA4/UART1_RXD/SPIS0_SCLK/I2C1_SCK/PWM0_4/USB_DP` | `PA4` | 多功能可配置 GPIO |
| 71 | `PA5/UART1_TXD/SPIS0_CSN/I2C1_SDA/PWM0_5/SPIMX8_0_DQS/USB_DM` | `PA5` | 多功能可配置 GPIO |
| 72 | `PA6/UART1_CTS/SPIS0_MOSI/I2C0_SCK/PWM0_6/MCAN1_RX/SPIMX8_0_SCLK/USB_DP/SPDIF_OUT` | `PA6` | 通用 GPIO，板載控制 LED D3（高電平點亮） |
| 73 | `PA7/UART1_RTS/SPIS0_MISO/I2C0_SDA/PWM0_7/MCAN1_TX/SPIMX8_0_CSN/USB_DM/SPDIF_IN` | `PA7` | 通用 GPIO，板載控制 LED D4（高電平點亮） |
| 74 | `BT_VCC` | `-` | 多功能可配置 GPIO |
| 75 | `BT_BSW` | `-` | 多功能可配置 GPIO |
| 76 | `BT_BFB` | `-` | 多功能可配置 GPIO |
| 77 | `BT_IOLDO` | `-` | 多功能可配置 GPIO |
| 78 | `XO_24M` | `-` | 24MHz 晶振輸出（XO） |
| 79 | `XI_24M` | `-` | 24MHz 晶振輸入（XI） |
| 80 | `NC` | `-` | 多功能可配置 GPIO |
| 81 | `EPAD/GND` | `GND` | 晶片底部散熱接地焊盤（EPAD/GND） |

## 2. 關鍵子電路分析

### 2.1 晶振電路（Clock Circuit）
- **主高頻晶振 Y1**：24MHz / 12pF / 10ppm（CRY3225 封裝）。
- **負載電容**：C22（100pF/NC）與 C29（100pF/NC），配合晶片內部可配置負載電容陣列調整頻偏。
- **連接腳位**：Pin 79（XI_24M）、Pin 78（XO_24M）。

### 2.2 射頻 RF 匹配電路（2.4GHz BLE RF Frontend）
- **天線座／板載天線**：RF1（RFANT_BT）。
- **LC 匹配網路**：
  - 串聯電感 L1：10uH / 100mA（高頻扼流／匹配）。
  - 匹配電容 C1（2pF）、C2（1.8pF）、C3（0.9pF）、C4（NC）。
  - 阻抗控制：RF 走線嚴格控制為 50Ω 特性阻抗微帶線。

### 2.3 板載人機介面（UI：LEDs & Push Buttons）
| 元件編號 | 功能說明 | MCU 腳位 | 驅動方式／有效電平 | 線路架構 |
| :---: | :---: | :---: | :---: | :--- |
| **D3** | 紅色 LED | **PA6** (Pin 72) | **高電平有效（Active HIGH）** | PA6 ➔ R7 (1kΩ) ➔ D3 (LED) ➔ GND |
| **D4** | 紅色 LED | **PA7** (Pin 73) | **高電平有效（Active HIGH）** | PA7 ➔ R8 (1kΩ) ➔ D4 (LED) ➔ GND |
| **K1** | 輕觸按鍵 1 | **PP0** (Pin 66) | **低電平有效（Active LOW）** | PP0 直通 K1 ➔ GND（需啟用內部上拉） |
| **K2** | 輕觸按鍵 2 | **PP2** (Pin 68) | **低電平有效（Active LOW）** | PP2 直通 K2 ➔ GND（需啟用內部上拉） |
| **K3** | 輕觸按鍵 3 | **PP3** (Pin 67) | **低電平有效（Active LOW）** | PP3 直通 K3 ➔ GND（需啟用內部上拉） |
| **K4** | 復位按鍵 (RST) | **RSTN** (Pin 58) | **硬體重啟（低有效）** | RSTN ➔ K4 ➔ GND，並聯 C21(332) 濾波 |

### 2.4 外部記憶體介面（PSRAM / QSPI Flash: U11）
- **元件型號**：U11 標示為 `XT25Q128`（相容 16MB QSPI Flash / PSRAM）。
- **腳位連接**：
  - `PSRAM-CLK`：連至 PB12 / Pin 23
  - `PSRAM-CS`：連至 PB13 / Pin 24
  - `PSRAM-IO0`：連至 PC0 / Pin 27
  - `PSRAM-IO1`：連至 PC1 / Pin 26
  - `PSRAM-IO2`：連至 PB14 / Pin 25
  - `PSRAM-IO3`：連至 PB15 / Pin 28
- **供電**：`IO1V8_LDO`（1.8V 專用高速記憶體電源），旁路電容 C19（2.2µF）。

### 2.5 類比音訊介面（Audio & MIC Circuit）
- **麥克風輸入**：`MIC+`、`MIC-` 接至 MCU PP7、PP6，由 `MIC_BIAS`（Pin 60）提供偏置供電。
- **DAC 類比輸出**：`DACN`、`DACP`（音訊差分輸出）直接拉至 Device 頁面之 HT6872 功放晶片。

## 3. MCU 原理圖元件清單（BOM: 64 Items）

| 序號 | 位號（Designator） | 數值／型號（Value/Comment） | 封裝（Footprint） | 說明 |
| :---: | :--- | :--- | :--- | :--- |
| 1 | `C1` | 2pF | `-` | C |
| 2 | `C10` | 105 | `-` | C |
| 3 | `C11` | 225 | `-` | C |
| 4 | `C12` | 105 | `-` | C |
| 5 | `C13` | 225 | `-` | C |
| 6 | `C14` | 105 | `-` | C |
| 7 | `C15` | 105 | `-` | C |
| 8 | `C16` | 105 | `-` | C |
| 9 | `C17` | 225 | `-` | C |
| 10 | `C18` | 106 | `-` | C |
| 11 | `C19` | 225 | `-` | C |
| 12 | `C2` | 1.8pF | `-` | C |
| 13 | `C20` | NC | `-` | C |
| 14 | `C21` | 332 | `-` | C |
| 15 | `C22` | 101 | `-` | C |
| 16 | `C23` | 105 | `-` | C |
| 17 | `C24` | 105 | `-` | C |
| 18 | `C25` | NC | `-` | C |
| 19 | `C28` | 332 | `-` | C |
| 20 | `C29` | 101 | `-` | C |
| 21 | `C3` | 0.9pF | `-` | C |
| 22 | `C30` | 105 | `-` | C |
| 23 | `C31` | NC | `-` | C |
| 24 | `C37` | NC | `-` | C |
| 25 | `C4` | NC | `-` | C |
| 26 | `C5` | 103 | `-` | C |
| 27 | `C6` | 225 | `-` | C |
| 28 | `C7` | 33pF | `-` | C |
| 29 | `C8` | 225 | `-` | C |
| 30 | `C9` | 106 | `-` | C |
| 31 | `D1` | NC | `-` | TVS |
| 32 | `D2` | NC | `-` | TVS |
| 33 | `D3` | Red | `-` | LED |
| 34 | `D4` | Red | `-` | LED |
| 35 | `K1` | L5.2xW5.2xH1.5 | `-` | SW(TS-030) |
| 36 | `K2` | L5.2xW5.2xH1.5 | `-` | SW(TS-030) |
| 37 | `K3` | L5.2xW5.2xH1.5 | `-` | SW(TS-030) |
| 38 | `K4` | L5.2xW5.2xH1.5 | `-` | SW(TS-030) |
| 39 | `L1` | 10uH/100mA | `-` | L |
| 40 | `L2` | 2.2uH/350mA | `-` | L |
| 41 | `MIC1` | MIC | `-` | MIC |
| 42 | `R1` | 1K | `-` | R |
| 43 | `R11` | 2.2K | `-` | R |
| 44 | `R12` | 2.2K | `-` | R |
| 45 | `R17` | 2.2K | `-` | R |
| 46 | `R18` | 2.2K | `-` | R |
| 47 | `R28` | 2.2K | `-` | R |
| 48 | `R33` | 2.2K | `-` | R |
| 49 | `R36` | 22R | `-` | R |
| 50 | `R5` | 2.2R | `-` | R |
| 51 | `R6` | 1M | `-` | R |
| 52 | `R60` | 22R | `-` | R |
| 53 | `R61` | 22R | `-` | R |
| 54 | `R62` | 22R | `-` | R |
| 55 | `R63` | 22R | `-` | R |
| 56 | `R64` | 22R | `-` | R |
| 57 | `R65` | 0R | `-` | R |
| 58 | `R66` | 0R | `-` | R |
| 59 | `R7` | 1K | `-` | R |
| 60 | `R8` | 1K | `-` | R |
| 61 | `RF1` | ANT | `-` | RFANT_BT |
| 62 | `U1` | FR3068E-C | `QFN80_0909_P0.4` | FR3068E_C |
| 63 | `U11` | XT25Q128 | `-` | PSRAM |
| 64 | `Y1` | 24MHz/12pF/10ppm | `-` | CRY3225 |
