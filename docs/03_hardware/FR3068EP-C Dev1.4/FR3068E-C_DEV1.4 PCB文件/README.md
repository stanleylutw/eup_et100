# FR3068E-C DEV1.4 PCB 工程文件總覽與開發指南

> **評估板型號**：Freqchip FR3068E-C DEV1.4（FR3068EP-C）  
> **轉換來源**：Altium Designer 原始工程檔案（3 份 SchDoc + 1 份 PcbDoc）  
> **分析日期**：2026-10-21  

## 1. 檔案索引導覽

| 文件名稱 | 來源工程檔 | 說明與重點內容 |
| :--- | :--- | :--- |
| [FR3068E-C_MCU_DEV1.4.md](FR3068E-C_MCU_DEV1.4.md) | `FR3068E-C_MCU_DEV1.4.SchDoc` | **核心 MCU 頁面**：U1 (QFN80) 完整 81 腳網表、晶振、RF 匹配天線、LED D3/D4、按鍵 K1～K4、PSRAM、類比音訊、64 顆元件 BOM |
| [FR3068E-C_Device_DEV1.4.md](FR3068E-C_Device_DEV1.4.md) | `FR3068E-C_Device_DEV1.4.SchDoc` | **周邊介面頁面**：5V/3.3V 電源、TJA1044 CAN FD、SP3485 RS485、HT6872 功放、J7 (24P)/J8 (36P) 擴充排針完整映射、102 顆元件 BOM |
| [FR3068E-C_Debug_DEV1.4.md](FR3068E-C_Debug_DEV1.4.md) | `FR3068E-C_Debug_DEV1.4.SchDoc` | **板載偵錯頁面**：FR8003A DAPLink MCU、SWD/VCOM J9 跳線設定（PB10/PB11/PB4/PB5）、24 顆元件 BOM |
| [FR3068E-C_DEV1.4_PCB.md](FR3068E-C_DEV1.4_PCB.md) | `FR3068E-C_DEV1.4.PcbDoc` | **PCB Layout 實體佈局**：2 層板疊層、190 顆元件坐標與層別、181 組網路走線規則與 RF 50Ω 佈線 |

## 2. 核心人機與除錯腳位速查表（Quick Reference）

| 項目 | 位號 | MCU 引腳 | 晶片 Pin 名稱 | 有效電平 | 控制／設定方式 |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **LED 1** | **D3** | **Pin 72** | **`PA6`** | **高電平亮（High）** | GPIOA_Pin_6 輸出 High 點亮，串 1kΩ 電阻 |
| **LED 2** | **D4** | **Pin 73** | **`PA7`** | **高電平亮（High）** | GPIOA_Pin_7 輸出 High 點亮，串 1kΩ 電阻 |
| **按鍵 1** | **K1** | **Pin 66** | **`PP0`** | **低電平有效（Low）** | PMU Pin 0，直通 GND，韌體需開內部上拉（Pull-Up） |
| **按鍵 2** | **K2** | **Pin 68** | **`PP2`** | **低電平有效（Low）** | PMU Pin 2，直通 GND，韌體需開內部上拉（Pull-Up） |
| **按鍵 3** | **K3** | **Pin 67** | **`PP3`** | **低電平有效（Low）** | PMU Pin 3，直通 GND，韌體需開內部上拉（Pull-Up） |
| **復位鍵** | **K4** | **Pin 58** | **`RSTN`** | **硬體低復位** | 直通 GND，手動重啟晶片 |
| **SWD CLK** | J9 | **Pin 21** | **`PB10`** | — | J9 Pin 1-2 短接帽，連至 DAPLink SWCLK |
| **SWD DIO** | J9 | **Pin 22** | **`PB11`** | — | J9 Pin 3-4 短接帽，連至 DAPLink SWDIO |
| **Debug RX** | J9 | **Pin 15** | **`PB4`** | — | J9 Pin 5-6 短接帽，MCU RX <- DAPLink TX |
| **Debug TX** | J9 | **Pin 16** | **`PB5`** | — | J9 Pin 7-8 短接帽，MCU TX -> DAPLink RX |

## 3. 全板元件統計彙整（Total EVB BOM Summary）

- **原理圖總元件數**：`64` (MCU) + `102` (Device) + `24` (Debug) = **190 顆**
- **PCB 實體放置元件數**：**190 顆**（100% 完全匹配，無多餘或遺漏元件）
- **主要 IC 統計**：
  1. `U1`：Freqchip FR3068E-C（QFN80，BLE 5.4 + Cortex-M33 MCU）
  2. `U3`：NXP TJA1044GT/3Z（CAN FD 收發器）
  3. `U5`：Exar SP3485EN-L/TR（RS485 收發器）
  4. `U6`：AMS LM1117-3.3V（3.3V LDO 穩壓器）
  5. `U7`：Heroic HT6872（4.7W 單聲道 D 類音訊功放）
  6. `U8`：MT3540-F25（升壓 DC-DC 轉換器）
  7. `U9`：Freqchip FR8003A_QFN20（板載 DAPLink 偵錯主控）
  8. `U10`：Torex 662K（XC6206 3.3V LDO 穩壓器）
  9. `U11`：XT25Q128 / PSRAM（16MB 外部記憶體）
