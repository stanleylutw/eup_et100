# FR306x 型號、架構與周邊

來源：[FR306x v0.4.9 PDF](../QT0013201514_FR306x技术规格书_v0.4.9.pdf)，p.5-10。返回 [索引](README.md)。此處描述原廠宣稱的能力，並不表示所有功能已在 ET-100 韌體啟用或測試。

## 1. 概述與協定

來源 p.5：FR306x 是低功耗、高安全性的無線 MCU，整合 BR／EDR／BLE 收發器與控制器、CAN FD，面向工業及汽車電子。系列概述提到 AEC-Q100 Grade 2；各料號是否符合資格須以 p.7 訂購表分別判讀。

| 模式 | 資料速率 | 調變／編碼 |
|---|---|---|
| BR | 1 Mbps | GFSK |
| EDR | 2 Mbps | π/4-DQPSK |
| EDR | 3 Mbps | 8DPSK |
| BLE LE 1M | 1 Mbps | GFSK |
| BLE LE 2M | 2 Mbps | GFSK |
| BLE coded S2 | 500 kbps | 來源稱 LE S2；coded PHY |
| BLE coded S8 | 125 kbps | 來源稱 LE S8；coded PHY |

宣稱符合 Bluetooth 5.3，可分別開關不同模式，支援多主、多從、多連線。未提供最大連線數、同時角色組合、MTU、吞吐量、profile 清單或 qualification ID。AUTOSAR／SAE J1939 的支援宣稱不等於本 SDK 已附完整可量產軟體。

## 2. 完整訂購表

來源 p.7；所有列的 MC1(CM33) 最高主頻均為 156 MHz，工作溫度均 -40 至 +105 °C，濕敏等級均 MSL3。

| 型號 | Flash | SRAM | GPIO | CAN 數 | 封裝 mm | 包裝／卷數量 | AEC-Q100 |
|---|---|---|---|---|---|---|---|
| FR3066D-C | 1 MB | 256 KB | 29 | 2 | QFN48 6 × 6 | 編帶，5000 顆／卷 | 否 |
| FR3066DQ-C | 1 MB | 256 KB | 29 | 2 | QFN48 6 × 6 | 編帶，5000 顆／卷 | 是 |
| FR3066EQ-D | 2 MB | 512 KB | 29 | 3 | QFN48 6 × 6 | 編帶，5000 顆／卷 | 是 |
| FR3068E-C | 2 MB | 512 KB | 56 | 2 | QFN80 9 × 9 | 編帶，3000 顆／卷 | 否 |
| FR3068EP-C | 2 MB | 512 KB | 56 | 2 | QFN80 9 × 9 | 編帶，3000 顆／卷 | 否 |
| FR3068E-D | 2 MB | 512 KB | 56 | 4 | QFN80 9 × 9 | 編帶，3000 顆／卷 | 否 |

FR3068EP-C 另外內建 2 MB PSRAM（p.5，僅該型號）；p.7 沒有獨立 PSRAM 欄。`D`、`Q`、`P` 等字母不應自行推導車規資格、silicon revision 或記憶體映射。

![訂購表原頁](assets/pdf_page_07.png)

## 3. CPU 與記憶體

| 區塊 | 原廠規格 | 來源 |
|---|---|---|
| 應用主核 MC1 | ARM Cortex-M33，最高 156 MHz | p.5、7-8 |
| 運算指令 | FPU、DSP 指令集 | p.5、8 |
| Bluetooth 協處理器 | 32-bit RISC，48 MHz，獨立 SRAM，執行 Bluetooth 協定 | p.5、8 |
| 主核 SRAM | 系列最大 512 KB，CM33 獨享 | p.5 |
| 內建 Flash | 系列最大 2 MB，用於使用者程式 | p.5、8 |
| Cache | 32 KB | p.5、8 |
| PSRAM | 2 MB，only FR3068EP-C | p.5 |
| QSPI／OPI bus | 時鐘 78 MHz | p.5；QSPI 上限亦見 p.9 |

未提供 SRAM bank、PRAM／DRAM 分配、Flash erase geometry、位址範圍、cache coherency、MPU／TrustZone 設定、boot ROM 區域或 PSRAM memory map。這些不能由「512 KB SRAM」反推。

## 4. 功能方塊圖的全部區塊

來源 p.8，圖 1-1。圖題內寫 `FR306x-C SOC`，但文件適用整個系列；图中 4× CAN 等是系列上限，非全部料號共有。

| 類別 | 圖中列示 |
|---|---|
| System | 6×32-bit Timer；2× DMA；2× Watchdog（Interrupt、Window）；PMU |
| MCU core | ARM MC1(CM33)；FPU；DSP |
| Hardware accelerator | IIR filter；FFT／IFFT 128、256、512 |
| Security | CRC；AES-128／192／256；Efuse 2 Kb；TRNG |
| Analog | 12-bit ADC，9 channels |
| Memory | Up to 2 MByte Flash；32 KB Cache；up to 512 KB SRAM |
| Connectivity | 8080／RGB888／RGB565 parallel interface；OSPIM×2；SPIM×2；SPIS×2；QSPIS×1；UART×5；SDMMC×1；PDM×2；I2S×2；USB 2.0 OTG FS；CAN FD×4；I2C×3 |
| GPIOs | Up to 56 GPIO；2× PWM／16 channels（保留原圖寫法） |
| Bluetooth core | 32-bit RISC；BT＋BLE 5.3 dual baseband；BT＋BLE 5.3 dual-mode radio |

`Efuse(2Kb)` 保留原單位，不能寫成 2 KB。DMA stream／channel 數、TRNG 品質、FFT 精度、IIR 係數格式及 watchdog 時間基準均未說明。

![功能方塊圖原頁](assets/pdf_page_08.png)

## 5. 周邊介面完整清單

來源 p.5、p.8-10；保留摘要與方塊圖不同的寫法。

| 介面 | 規格／數量 | 補充及限制 |
|---|---|---|
| UART | 5×，帶 flow control | 可用於 debug 與 AT；未列 UART 編號與 pinmux |
| I2C | 3× | 可接 EEPROM、FM 接收器及一般設備；未列速度或 slave 支援 |
| SPI master | 2×，支援 QSPI／OSPI | p.8 另列 OSPIM×2、SPIM×2，不自行相加為 4 個獨立 master |
| SPI slave | 2× | p.8 另列 QSPIS×1；instance 與共用關係未說明 |
| 外部 Flash | 1／2／4-bit SPI Flash | QSPI serial clock 最大 78 MHz；未列 JEDEC 支援表 |
| I2S | 2× | sample rate／word size／clock source 未列 |
| PDM | 最多 4-ch 數位音訊 | p.8 圖列 PDM×2；instance 与 channel 不混為一談 |
| Parallel display | RGB565／RGB888；圖另列 8080 | 未列解析度、pixel clock、DMA timing |
| SDIO／SDMMC／eMMC | SDIO 3.0、eMMC 4.5.1；1／4／8-bit | p.10 寫 eMMC 4.5；圖列 SDMMC×1 |
| USB | USB-OTG；圖明列 USB 2.0 OTG FS | 不宣稱 High Speed、PHY pin 或特定 USB class |
| PWM | 摘要最多 31× PWM | 方塊圖寫 2× PWM／16 channels；可引出數需確認 |
| ADC | 12-bit SAR，最多 9 channels | p.5 指 FR3068E-C／EP-C／E-D 9ch，FR3066DQ-C／EQ-D 7ch；未明寫 FR3066D-C 的通道數 |
| GPIO | 最多 56，QFN48 型號 29 | pin 表只給 GPIO 名稱，沒有 alternate function index |
| Timer | 多路；圖列 6×32-bit | 中斷／capture／prescaler／clock source 未列 |
| Watchdog | 圖列 interrupt／window 兩種 | 用於追蹤異常；reset 範圍與設定未列 |

## 6. Bluetooth 收發器及控制器

來源 p.8-9。完整 RF 數值及條件見 [04](04_rf_characteristics.md)。

收發器具備內建 band-pass filter、數位 RF demodulator、即時 RSSI、快速 AGC，以改善靈敏度、channel rejection 與 dynamic range。摘要 TX 範圍 -20 至 +10 dBm；EDR TX 表另列典型 +7 dBm，不混用。

控制器支援：Broadcaster、Central、Observer、Peripheral；Advertising、Data、Control packets；AES／CCM 加密；CRC／Whitening 位元流處理；跳頻計算；協定 idle 期間 baseband power-down。

## 7. CAN FD 控制器

來源 p.5、7、9。

- 符合 CAN 2.0 Part A／B、ISO 11898-1。
- CAN FD 收發資料長度最多 64 bytes。
- 宣稱支援 AUTOSAR、SAE J1939。
- 改進的 RX filter。
- 2 個可配置 RX FIFO。
- 最多 64 個 dedicated RX buffer。
- 最多 32 個 dedicated TX buffer。
- 可配置 TX FIFO、TX queue、TX event FIFO。

上述資源是否為每個 CAN instance、message RAM 如何分配、最大 nominal／data bit rate、timestamp／interrupt／DMA、Bosch M_CAN 版本均未定義。FR3068E-C 為 2 組 CAN，不能套用系列 4 組上限。

## 8. PMU 與封裝

來源 p.10：內建 power-on reset、高效 switching supply（輸入 2.9-3.6 V、輸出可程式化）、內部數位／RF／analog LDO、軟體 shutdown、硬體 wake-up、低電壓偵測。

來源封裝為 QFN48 6×6 mm、QFN80 9×9 mm。pin、供電、reset 與尺寸分別見 [02](02_packages_and_pinout.md)、[03](03_electrical_power_and_clock.md)。

## 9. 原廠列出的應用領域

來源 p.6：汽車安全進入、無鑰匙進入、無線電池管理；工業定位、智慧建築與監控、門禁、工廠自動化。這些是應用例，不是特定型號的功能認證。
