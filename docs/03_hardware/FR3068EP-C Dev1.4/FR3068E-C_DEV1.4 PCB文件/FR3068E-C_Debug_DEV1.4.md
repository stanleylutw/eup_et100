# FR3068E-C_Debug_DEV1.4 板載偵錯器規格說明書

> **原始檔案**：[`FR3068E-C_Debug_DEV1.4.SchDoc`](FR3068E-C_Debug_DEV1.4.SchDoc)  
> **板卡版本**：FR3068E-C DEV1.4（Freqchip 官方評估板）  
> **核心功能**：板載 DAPLink / CMSIS-DAP 韌體偵錯器與虛擬串列埠（VCP UART）  
> **元件總數**：24 顆  

## 1. 偵錯器架構概述

- **主控 MCU**：U9 採用 Freqchip 自家 **`FR8003A_QFN20`** 微控制器，預燒 CMSIS-DAP / DAPLink 韌體。
- **USB 介面**：J10（Type-C 16-Pin 介面，專供 DAPLink 連線電腦）。
- **時鐘源**：Y2 為 24MHz / 12pF / 10ppm 晶振（CRY3225 封裝），配合 C60/C61 負載電容。
- **獨立供電**：U10 為 **`662K`**（XC6206 3.3V LDO），將 USB 5V `VIN` 轉為 3.3V 提供 U9 運作。
- **狀態指示燈**：
  - `D9`（紅色 LED）：DAPLink 運作／枚舉指示燈。
  - `D10`（紅色 LED）：SWD / UART 資料傳輸指示燈。

## 2. 偵錯跳線開關 J9（2.54mm 4Px2 Jumper Block）

J9 負責將 DAPLink 輸出訊號與目標晶片 FR3068E-C 的引腳物理對接。出廠預設附帶跳線帽：

| 排針位置 | MCU 目標腳位 | DAPLink 訊號 | 訊號方向 | 功能說明 |
| :---: | :---: | :---: | :---: | :--- |
| J9 Pin 1-2 | **`PB10`** | **SWCLK** | DAPLink ➔ MCU | SWD 偵錯時鐘線 |
| J9 Pin 3-4 | **`PB11`** | **SWDIO** | 雙向（Bidirectional） | SWD 偵錯數據線 |
| J9 Pin 5-6 | **`PB4`** | **DAP_TX** | DAPLink ➔ MCU | 虛擬串列埠：MCU 接收端（MCU RX） |
| J9 Pin 7-8 | **`PB5`** | **DAP_RX** | MCU ➔ DAPLink | 虛擬串列埠：MCU 發送端（MCU TX） |

> [!IMPORTANT]
> **注意**：板載 DAPLink 原理圖中**沒有連接 nRESET 訊號線**至 MCU RSTN（Pin 58）。
> 因此 pyOCD 的 `under-reset` 復位連接模式無法透過軟體自動觸發，若 MCU 韌體卡死導致 SWD 失聯，需配合手動按壓 RST 按鍵（K4）進行救援燒錄。

## 3. 偵錯原理圖元件清單（BOM: 24 Items）

| 序號 | 位號（Designator） | 數值／型號（Value/Comment） | 封裝（Footprint） | 說明 |
| :---: | :--- | :--- | :--- | :--- |
| 1 | `C54` | 225 | `-` | C |
| 2 | `C55` | 225 | `-` | C |
| 3 | `C56` | 225 | `-` | C |
| 4 | `C57` | 104 | `-` | C |
| 5 | `C58` | 106 | `-` | C |
| 6 | `C59` | 104 | `-` | C |
| 7 | `C60` | 10pF | `-` | C |
| 8 | `C61` | 10pF | `-` | C |
| 9 | `D10` | Red | `-` | LED |
| 10 | `D9` | Red | `-` | LED |
| 11 | `J10` | TYPE-C-16PIN | `-` | TYPE-C-16PIN |
| 12 | `J9` | 2.54mm_4Px2 | `-` | 2.54mm_4Px2 |
| 13 | `R13` | 5.1K | `-` | R |
| 14 | `R50` | 1K | `-` | R |
| 15 | `R51` | 1K | `-` | R |
| 16 | `R52` | 100R | `-` | R |
| 17 | `R53` | 100R | `-` | R |
| 18 | `R54` | 100R | `-` | R |
| 19 | `R55` | 100R | `-` | R |
| 20 | `R56` | 1.5K | `-` | R |
| 21 | `R9` | 5.1K | `-` | R |
| 22 | `U10` | 662K | `-` | 662K |
| 23 | `U9` | FR8003A_QFN20 | `-` | FR8003A_QFN20 |
| 24 | `Y2` | 24MHz/12pF/10ppm | `-` | CRY3225 |
