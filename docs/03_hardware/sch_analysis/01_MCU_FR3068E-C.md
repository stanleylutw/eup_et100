# Sheet 1 — MCU（FR3068E-C）+ Reset + JLINK + SPI Flash

**PDF Page**：1 / 15
**Ref Designator Prefix**：01xx
**圖面標題**：MCU_JLINK 下載；MCU_RESET 開關；FLASH
**對應提案書章節**：[04_技術方案_關鍵零件.md §P19 MCU Selection FR3068E-C](../../00_project/04_技術方案_關鍵零件.md)、[05_技術方案_天線與外觀.md §P30 FLASH XM25QH256D](../../00_project/05_技術方案_天線與外觀.md)

> **2026-10-02 v0.4.9 校正**：本文件的 SCH net／pin 表保留先前讀圖紀錄，尚未重新逐腳核對原理圖，不能當成可直接接線的 pinmux。原廠訂購表為 2 MB Flash、512 KB SRAM、2 組 CAN、QFN80 9×9 mm、AEC-Q100「否」。新版多處 physical pin 與舊表不同，且 p.30 記載 v0.4.4 曾修改 FR3068E-C pin。差異見 [開發對照](../fr306x_reference/05_et100_development_reference.md)，完整原廠腳表見 [封裝與 pinout](../fr306x_reference/02_packages_and_pinout.md)。需 Quectel 提供 board-matched silicon revision、symbol／footprint、full pinmux 與 IO voltage-bank 表後才可確認板上配置。

---

## 1. 功能定位

**系統主控**。FR3068E-C 掛所有周邊：
- **I2C**：Sensor_I2C0（G-Sensor）、NFC_I2C3（NFC 控制）、EXP_I2C5（AW9523 IO expander）
- **UART**：UART0（RS232_0 115200 DB9）、UART1（RS232_1 9600）、UART2（RS232_2 9600）、UART3（CAN 9600 用但實際接 CAN 只用 TX/RX，待確認）、UART4（LTE 115200 for 軟體調試 + police 讀取版本資訊）、UART5（GNSS）
- **SPI**：SPIMX2_1_HOST → XM25QH256D 外部 Flash
- **BLE**：內建 BT core（Sheet 13 外接 BT 天線）
- **CAN FD**：MCU_CAN_RXD/TXD/STB → Sheet 5 SIT1042
- **多路 GPIO**：LED1~4_CTRL、BUFFER_CTRL（蜂鳴器 PWM）、LTE_WAKE / MCU_WAKE_LTE / MCU_LTE_RESET、CHA_EN / CHA_CHRG / CHA_FULL、外部 IO1~3 中斷、ACC_INT_IN / ACC_INT_OUT、1-Wire 溫度
- **ADC**：VBAT_ADC、oil_ADC、1-Wire 等多路

> 原理圖 symbol 的腳位排列不代表實體封裝或 IO 電壓域；原廠 FR3068E-C 封裝為 QFN80，不是 LQFP80。

---

## 2. 關鍵元件

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| U0101 | FR3068E-C | QFN80 9×9 mm（板上 footprint 待核） | 主 MCU | | CM33 最高 156 MHz + 32-bit RISC @48 MHz（BT core）；2 MB Flash + 512 KB SRAM；2 組 CAN FD；BT 5.3；v0.4.9 p.7 AEC-Q100 為「否」 |
| X0101 | SX20Y024000BC1T009 | — | **24MHz TCXO** | | 給 MCU + BT core，±2% 50V spec（圖面 10pF 負載電容） |
| U0103 | XM25QH256DXIQT | USON/SOP8 | 256Mb (32MB) SPI NOR Flash | | 3.3V operation，deep power-down 0.2µA |
| S0101 | TS30654EB2B01 | SMD switch | Reset 按鈕 | | 按下拉低 MCU_RST |
| D0101 | NM_RB521CM-30T2R | SOD-323 | MCU_3V3 保護 | **NM** | 預留反灌 blocking diode 位置 |
| D0104 | WS05K6-RHI | — | MCU_3V3 TVS | | ESD 保護 |
| D0105/D0106 | PESNK402-03 | SOD-523 | SWCLK / SWDIO TVS | | JLINK pin ESD |
| D0107 | PESNK402-03 | SOD-523 | MCU_RST TVS | | |
| R0101 | 50 ohm | 0402 | BT_ANT 匹配電阻 | | pin 3 (ANT) → 50Ω → Sheet 13 BT ANT 匹配網路 |
| C0101/02 | 10pF 50V | 0402 | TCXO 負載電容 | | |
| C0117 (R0119/R0117) | 4.7K | 0402 | SPI Flash /HOLD、/WP pull-up | | 把 Flash 三線（CS/MISO/MOSI/CLK）外的保留腳位固定 |

**I2C bus pull-up**：
- EXP_I2C5 (SDA/SCL) → **4.7K @ MCU_3V3** × 2
- Sensor_I2C0 (SDA/SCL) → **4.7K @ VCC_1V8** × 2（1V8 域 I2C）
- NFC_I2C3 → 由 Sheet 10 側負責 pull-up

### Pin 分區

> 以下分區是舊 SCH 分析的暫存描述，不是 v0.4.9 確認的 voltage-bank map。腳號、電源輸入／輸出角色及 net 連接均待回查；尤其新版 VDD_IO3V3 為 pin 14、VDD_IO1V8 為 pin 36。不得依此表決定 SWD VTref 或外部電源接法。

- **AVDD_1V2**（pin 1, 2）：MCU 核心電源 1.2V 由 MCU 內部 LDO 從 3V3 下來，外部只需 decoupling（22µF+100nF）
- **ANT**（pin 3）：BT 天線輸出，透過 R0101 50Ω → Sheet 13
- **1V8 域 IO**（PB12-PB15 / PC0-PC7，pin 21-32）：外接 NFC I2C（I2C3）、JLINK（SWCLK/SWDIO）、UART4 debug（115200）
- **3V3 域 IO**（其餘 pin）：所有其他周邊
- **BT crystal / clock**：BT_SFB / BT_BSW / BT_JUDO / BT_VCC（pin 72-75）配 TCXO、L0101 2.2µH filter、C0113 2.2µF
- **GNSS_DATA_IN_EINT**（pin 68）：GNSS 子板中斷進 MCU
- **PWM / Buzzer**：BUFFER_CTRL（pin 51 PD15）
- **ADC**：VBAT_NTC（pin 55 PP7）、VBAT_ADC_IN（pin 54 PP6）、LTE_WAKE_MCU_3V3（pin 53 PP5）、oil_ADC_IN（pin 52 PP4）

---

## 3. 電源輸入

| Rail | 來源 | 用途 |
|---|---|---|
| MCU_3V3 | Sheet 7 U0701 MCU_GNSS_3V3 → 本 sheet 經 D0101(NM)/D0104 直接命名為 MCU_3V3 | 主 IO 電壓 |
| AVDD_1V2 | MCU 內部 LDO | 核心 |
| IO3V3_LDO | MCU 內部 LDO | 3V3 域 IO 參考 |
| IO1V8_LDO / VCC_1V8 | 來自 Sheet 7 NFC_1V8 | 1V8 域 IO 參考 |
| VDD_IO3V3 | 來自 MCU_3V3 | IO rail |

---

## 4. 介面（I/O Nets）主要清單

因為 MCU 連接超過 60 條 net，僅列重點（完整表留階段 B）：

| 類別 | Nets | 對外 Sheet |
|---|---|---|
| LTE | MCU_LTE_TXD_3V3 / RXD_3V3 / RESET / LTE_WAKE_MCU / MCU_WAKE_LTE | Sheet 2, 11 |
| GNSS | GNSS_TXD / GNSS_RXD（UART5）、GNSS_DATA_IN_EINT、GNSS_LDO_EN、GNSS_VBAT_EN | Sheet 12 |
| NFC | NFC_I2C3_SDA / SCL、MCU_WAKE_NFC、NFC_CLK_REQ、NFC_VEN、NFC_IRQ、NFC_DWL_REQ | Sheet 10 |
| G-Sensor | Sensor_I2C0_SDA / SCL、G-Sensor_INT | Sheet 3 |
| IO Expander | EXP_I2C5_SDA / SCL、EXP_RST、EXP_INTN | Sheet 11 |
| CAN | MCU_CAN_TXD / RXD / STB | Sheet 5 |
| RS232 | MCU_RS232_TXD0/1/2、RXD0/1/2、EN1/EN2/EN0 | Sheet 5 |
| LED | LED1~4_CTRL | Sheet 4 |
| 蜂鳴器 | BUFFER_CTRL | Sheet 4 |
| Charger | MCU_CHA_EN、CHA_CHRG、CHA_FULL | Sheet 8 |
| SPI Flash | SPI_FLASH_RST / CS / MISO / MOSI / CLK | 本 sheet U0103 |
| JLINK | SWCLK、SWDIO | 本 sheet TP0105/06/07 |
| 1-Wire | 1_Wire_Temp_IN_MCU、1_Wire_Temp_OUT_MCU | Sheet 11 |
| ACC | ACC_INT_IN、ACC_INT_OUT | Sheet 11 |
| ADC | oil_ADC_IN、VBAT_ADC_IN、VBAT_NTC、LTE_WAKE_MCU_3V3 | Sheet 11 / 8 |
| BT | ANT（pin 3） → BT 匹配 → 鋼片天線 | Sheet 13 |

---

## 5. 設計細節

- **BT_ANT pin 3 緊鄰 24MHz TCXO**：圖面特別註記「Pin76 与 Pin2 之间走线需尽量远离晶振，如果从芯片底部（焊盘内侧）布线，该走线与晶振焊盘距离不小于 0.2mm；如果采用四层板设计，可考虑过孔穿到背面走线。」—— **layout 注意事項**，PCB 校核時必查。
- **1V8 與 3V3 域分區待確認**：舊筆記把 PB12-PB15/PC0-PC7 歸為 1V8，v0.4.9 未提供逐 GPIO 的 voltage-bank map 或 SWD pinmux。需確認 target VTref，不能直接斷言 JLINK 為 1V8 或其餘 pin 全為 3V3。
- **TCXO 選用**：SX20Y024000BC1T009 是 24MHz ±2ppm TCXO，給 BT core 需求的高精度；MCU core clock 由內部 PLL 從 24MHz 倍頻到 156MHz。
- **UART4 專門給 software debug**：圖面註記「UART4 115200bps 专用于软件调试，客户现场：Police 读取版本信息」—— 這是**出廠後保留給警方（或客戶服務人員）讀版本/log 的介面**，走 Sheet 2 LTE 的 debug UART 線分流，實際接到 LTE_DBG 的 1V8 TP0201/02/03。
- **UART3 標示 CAN、UART1 標示 CAN**：圖面左側標籤「UART3_9600bps」「CAN」「UART2_9600bps」—— UART3 腳位（PB4, PB5）實際接 MCU_RS232_RXD2/TXD2 給 Sheet 5 第二顆 UM3221；CAN 腳位（PB7, PB6）接 MCU_CAN_RXD/TXD。**圖面標籤有誤導**，實際以 net name 為準。

---

## 6. 疑點 / Review Notes

- [ ] **BT ANT 與 24MHz TCXO 的隔離**：layout 要求 ≥0.2mm，實際 PCBA 必核。
- [ ] **1V8 / 3V3 混用的 power sequencing**：FR3068E-C datasheet 要查 1V8/3V3 的 power-on 順序（通常 3V3 先、1V8 後或同時）。目前 MCU_GNSS_3V3 常開、NFC_1V8 由 AW9523 開關 → **開機瞬間 1V8 可能延遲幾 ms** → PB12-PB15 這幾支腳是否有保護機制？
- [ ] **SPI Flash /WP、/HOLD 都 4.7K pull-up**：代表韌體不用 hardware WP 功能；若要 secure boot 要在量產階段才啟用。
- [ ] **MCU 的 RESV0 pin 12**：舊完整表記錄 R0110 1M 到 GND；先前此處的 R0101 10K 與該表矛盾，撤回。v0.4.9 確認名稱為 RESV0，但未規定外部上下拉；阻值、DNP 與接法須回查 SCH／原廠 hardware guide。
- [ ] **TCXO 的電源**：圖面上 TCXO 沒看到獨立 LDO，由 MCU_3V3 直接供電 —— 24MHz TCXO 本身 PSRR 應該夠，但 BT 收發時 3V3 ripple 若大可能影響頻偏，EVT 必測。
- [ ] **DNP**：D0101 NM、R0119 NM（SPI Flash 旁某顆）、R0101 旁的 R0110 1M；階段 B 逐一列出。
- [ ] **Reset／低電壓條件**：v0.4.9 p.26 列 POWON_VTH 2.9 V、BOR_VTH 2.4 V，5 us < TR < 10 ms、TD > 20 ms。建議 MCU_VCC／BT_VCC 工作範圍為 2.9-3.6 V；不能把 BOR 門檻當最低工作電壓，threshold tolerance／RSTN pulse width 仍未提供。
- [ ] **JLINK TP0105/06/07 無 10-pin 標準座**：代表量產板只保留 TP 點，開發期需用夾具探針 —— 這是常見量產做法，debug 不便但 BOM 省錢。

---

## 7. 參考

- SCH PDF：Sheet 1
- 相關 sheet：所有其他 sheet（MCU 是中心）
- 提案書章節：[04_P19 FR3068E-C](../../00_project/04_技術方案_關鍵零件.md)、[05_P30 FLASH XM25QH256D](../../00_project/05_技術方案_天線與外觀.md)
- Datasheet：Freqchip FR3068E-C、XMC XM25QH256D

---

## 階段 B — 完整 Pin Map、Decoupling、TCXO Layout、I2C 分析

### B.1 舊 SCH 80-pin 讀圖紀錄（未依新版逐腳驗證）

> 保留此表作為待核對紀錄，不作為原廠 pinout。例：舊表 PB4/PB5 = pin 14/15，新版為 15/16；舊表 PC1/PC2 = pin 27/28，新版為 26/29；舊表 PD15 = pin 50，新版為 52。不得直接搬移 net、SWD 或 board config。

**MC1 (CM33) 域** — 3.3V IO：

| Pin | SCH 標籤 | Net | 功能 |
|:-:|---|---|---|
| 1 | NC_1 | — | — |
| 2 | AVDD_1V2 | AVDD_1V2 | CM33 核心 analog 1.2V（內部 LDO 從 3V3）|
| 3 | ANT | 50OHM_BT_ANT | BT 天線輸出（經 R0101 50Ω → Sheet 13）|
| 4 | PA10 | CHA_CHRG | ← Sheet 8 charger status |
| 5 | PA11 | CHA_FULL | ← Sheet 8 charger status |
| 6 | PA12 | MCU_LTE_RXD_3V3 | ← Sheet 11 shifter ← Sheet 2 LTE TX |
| 7 | PA13 | MCU_LTE_TXD_3V3 | → Sheet 11 shifter → Sheet 2 LTE RX |
| 8 | PA14 | IO3_INT_IN | ← Sheet 11 外部 IO3 中斷 |
| 9 | PA15 | IO2_INT_IN | ← Sheet 11 外部 IO2 中斷 |
| 10 | PB0 / BT_PA4 | NC/BT internal | BT core 相關 |
| 11 | PB1 / BT_PA7 | NC/BT internal | BT core 相關 |
| 12 | RESV0 | R0110 1M to GND | Freqchip spec reserved pin |
| 13 | VDD_IO3V3 | MCU_3V3 | 3V3 IO rail |
| 14 | PB4 | MCU_RS232_RXD2 | ← Sheet 5 U0502 R1OUT |
| 15 | PB5 | MCU_RS232_TXD2 | → Sheet 5 U0502 T1IN |
| 16 | PB6 | MCU_CAN_RXD | ← Sheet 5 SIT1042 |
| 17 | PB7 | MCU_CAN_TXD | → Sheet 5 SIT1042 |
| 18 | PB8 | MCU_RS232_TXD1 | → Sheet 5 U0501 |
| 19 | PB9 | MCU_RS232_RXD1 | ← Sheet 5 U0501 |
| 20 | PB10/BT_PA14 | — | BT RF |
| 21 | PB11 | — | 1V8 域起點 |

**1V8 域** — PB12-PB15, PC0-PC7（Pin 21-32）:

| Pin | SCH 標籤 | Net | 功能 |
|:-:|---|---|---|
| 22 | PB12 | NFC_I2C3_SDA | ↔ Sheet 10 PN7160 |
| 23 | PB13 | NFC_I2C3_SCL | ↔ Sheet 10 |
| 24 | PB14 / BT_PA17 | — | BT RF |
| 25 | PB15 | EXP_RST | → Sheet 11 AW9523 RSTN |
| 26 | PC0 | VDD0 | 1V8 domain IO rail (NFC_1V8 經 pin 36?)|
| 27 | PC1 | SWCLK | JLINK |
| 28 | PC2 | SWDIO | JLINK |
| 29 | PC3 | NFC_CLK_REQ | ← Sheet 10 PN7160 CLK_REQ |
| 30 | PC4 | NFC_VEN | → Sheet 10 PN7160 VEN |
| 31 | PC5 | NFC_IRQ | ← Sheet 10 |
| 32 | PC6 | NFC_DWL_REQ | → Sheet 10 download mode |
| 33 | PC7 | VCC_1V8 | 1V8 IO rail from NFC_1V8 |
| 34 | VDD_IO2_1 | NFC_1V8 | 1V8 IO 分 rail |

**回到 3V3 域** — PD0-PD15, PP4-PP7:

| Pin | SCH 標籤 | Net | 功能 |
|:-:|---|---|---|
| 35 | PD0 / IO1_INT_IN | IO1_INT_IN | ← Sheet 11 |
| 36 | PD1 | MCU_WAKE_LTE_3V3 | → Sheet 11 shifter |
| 37 | PD2 | MCU_LTE_RESET | → Sheet 11 → Sheet 2 LTE RESET |
| 38 | PD3 | EXP_INTN | ← Sheet 11 AW9523 INTN |
| 39 | PD4 | GNSS_RXD | → Sheet 12 J1201 pin 4 |
| 40 | PD5 | GNSS_TXD | ← Sheet 12 J1201 pin 3 |
| 41-42 | PD6/PD7 | EXP_I2C5_SCL/SDA | ↔ Sheet 11 AW9523 |
| 43 | PD8 | SPI_FLASH_CLK | → U0103 |
| 44 | PD9 | SPI_FLASH_CS | → U0103 /CS |
| 45 | PD10 | SPI_FLASH_MOSI | → U0103 |
| 46 | PD11 | SPI_FLASH_MISO | ← U0103 |
| 47 | PD12 | SPI_FLASH_RST | → U0103 /HOLD/RESET |
| 48 | PD13 | 1_Wire_Temp_OUT_MCU | → Sheet 11 |
| 49 | PD14 | 1_Wire_Temp_IN_MCU | ← Sheet 11 |
| 50 | PD15 | BUFFER_CTRL | → Sheet 4 蜂鳴器 PWM |
| 51 | PP4 | oil_ADC_IN | ← Sheet 11 油量 ADC |
| 52 | PP5 | LTE_WAKE_MCU_3V3 | ← Sheet 11 shifter |
| 53 | PP6 | VBAT_ADC_IN | ← Sheet 8 電池電壓 ADC |
| 54 | PP7 | VBAT_NTC | ← Sheet 8 電池溫度 ADC |
| 55 | PD16 (BUFFER_CTRL) | — | 可能是 LED CTRL |
| ... | | | |
| 59 | IO3V3_LDO | IO3V3_LDO | 3V3 IO rail(內部 LDO 輸出)|
| 60 | NA | NC | |

**TCXO / BT clock** — Pin 71-78（典型 Freqchip 排布）:

| Pin | SCH 標籤 | Net |
|:-:|---|---|
| 71 | SYS_SPB | BT system clock |
| 72 | DVDD_IO2_1 | 1V8 domain supply |
| 73 | MCU_VCC | MCU 核心電源 |
| 74 | SYS_SFB | BT system freq buffer |
| 75 | DVDD_1V2 | 1V2 core |
| 76 | GNSS_VCC | GNSS wake signal (data in EINT) |
| 77 | GNSS_DATA_IN_EINT | ← Sheet 12 J1201 pin 5 |
| 78 | LTE_WAKE_MCU | ← Sheet 2 LTE_WAKE_MCU |
| 79 | EDU_CTRL | 四色灯 CTRL 群 |
| 80 | ED1_CTRL | LED1_CTRL → Sheet 4 |

> **註**：QFN80 physical pin 必須依原廠指定的視圖與 pin-1 標記核對，不能用順／逆時排列猜測。此表尚待重新讀取 SCH symbol 並對照板上 revision。

### B.2 Decoupling 網路驗證

**舊 decoupling 比較紀錄（非 v0.4.9 已確認規範）**：v0.4.9 未提供以下每 rail 的電容組合；需取得原廠 hardware guide，核對內部 LDO／Buck 的穩定性、ESR、layout 與實際 BOM。

| Rail | 舊筆記建議（來源待核） | SCH 實作（待回查） | 舊判斷（撤回，待驗證） |
|---|---|---|---|
| AVDD_1V2 | 1µF + 100nF | C0119 22µF + 100nF | **多裝不會錯**，裕量充足 |
| MCU_3V3 (VDD_IO3V3) | 10µF + 100nF × 2 | C0117 2.2µF + 100nF | **略少**，但 LDO 常開 Vout 穩定 |
| VCC_1V8 (NFC_1V8) | 1µF + 100nF | C0113 2.2µF + ... | OK |
| BT core supply | 10µF + 100nF 多路 | 多顆 2.2µF + 100nF | OK |

**原先「整體 decoupling 符合 reference design」結論撤回**；電容不是越大必然越安全。v0.4.9 p.28 明列 BT_Buck 10 µH（Isat ≥80 mA）、SYS_Buck 2.2 µH（Isat ≥300 mA），兩者 SRF ≥10 MHz、DCR ≤1 Ω；須先確認相關 net／元件再對照，不能套用到所有電感。

### B.3 TCXO Layout 要求（已標在 SCH）

圖面中文註記：**「Pin76 与 Pin2 之间走线需尽量远离晶振，如果从芯片底部（焊盘内侧）布线，该走线与晶振焊盘距离不小于 0.2mm；如果采用四层板设计，可考虑过孔穿到背面走线。」**

**解讀**：
- Pin 76 在 v0.4.9 為 BT_BFB，不是 GNSS_VCC；原圖註記是否對應同一 silicon revision 待確認
- Pin 2 = AVDD_1V2（核心 1V2 電源）
- 這條線不能靠近 TCXO 焊盤 **0.2mm 內**，否則 24MHz 時脈耦合進電源 → BT phase noise 惡化
- 4 層板建議從背面走線（遠離 TCXO）

**4 層板 stackup 推測**（提案書 §P15 標 4 層通孔 PCB）：
- Top：SIG + components
- GND1
- Power + SIG routing (Vcc 平面 + 短距信號)
- Bottom：SIG + BGA fanout
- **這條敏感線應走 bottom layer** ✓

### B.4 BT RF Path 阻抗

從 pin 3 ANT → R0101 50Ω → Sheet 13 BT matching network → 鋼片天線

**50Ω trace**：
- 4 層板 microstrip typical：Top layer trace 0.254mm 寬 + 0.2mm 到 GND1 層 → ~50Ω 典型（視板材 Dk 4.3）
- 長度：Sheet 1 到 Sheet 13 跨 package，走線 ~15-30mm 典型
- Insertion loss @ 2.4GHz, 30mm microstrip ≈ 0.1-0.2 dB（可忽略）

**R0101 「50 ohm」註記**：
- 這是**串聯電阻**，不是 trace 阻抗標籤
- 設計意圖：若 BT RF path 阻抗不匹配，可改這顆 R 做 matching
- 預設 50Ω，EVT 可改 0Ω short / 其他值調整

### B.5 I2C Bus Loading 分析

**3 條獨立 I2C bus**：

| Bus | 電壓域 | Slave | Pull-up | 速度 | 總線電容估算 |
|---|:-:|---|:-:|:-:|:-:|
| EXP_I2C5 | 3.3V | AW9523 (1 slave) | 4.7K × 2 | 400kHz | ~30pF (trace + slave Cin) |
| Sensor_I2C0 | 1.8V | SC7U22 (1 slave) | 4.7K × 2 | 400kHz | ~30pF |
| NFC_I2C3 | 1.8V | PN7160 (1 slave) | 4.7K × 2 @ NFC_1V8 | 400kHz | ~30pF |

**Rise time check**：τ = R × C = 4.7K × 30pF = **141ns**
- 400kHz I2C 一個 bit = 2.5µs，rise time 應 <300ns → 141ns ✓

**所有 bus 都 OK**，不需提升 pull-up 到 2.2K 或改 1kHz 以下速度。

### B.6 SPI Flash 效能

**XM25QH256DXIQT** @ 3.3V：
- Max SPI clock：**104 MHz** (standard SPI); **80 MHz** (quad SPI)
- Read: 50 MHz standard, 80 MHz quad
- Write/Erase: block erase ~400ms, page program ~1ms per 256B

**本板 SPI Flash 用 standard SPI**（CS/MISO/MOSI/CLK 四線 + /HOLD /WP pull-up）
- 32MB / 256B page = **131,072 pages**
- 全板寫入時間：131,072 × 1ms = **131 秒**（約 2 分鐘，若從空白寫滿）
- Erase (4KB sector): 8192 sectors × 50ms = **~7 分鐘**

**對 30 天 log 的意涵**（提案書 P05 要求）：
- 每筆 ~128B，一天 240 × 24 = 5760 筆 = **~720KB/day**
- 32MB / 720KB/day = **44 天**（理論，不含 wear leveling + FS overhead）
- 實務 20-24MB 可用（扣掉 FS、bad block）→ **~27-33 天**
- **剛好達標 30 天**，但無裕量 → 若 CAN J1939 payload >128B 就會 miss target

### B.7 疑點更新

| 原疑點 | Stage B 結論 |
|---|---|
| BT ANT 與 24MHz TCXO 隔離 | ✓ SCH 已標 ≥0.2mm；PCB layout 必核 |
| 1V8/3V3 power sequencing | **待確認**：v0.4.9 未定義分域 sequencing、GPIO reset defaults 或 unpowered IO injection；撤回「預設高阻、無風險」結論 |
| SPI Flash /WP /HOLD 4.7K | 舊 SCH 記錄為 pull-up；不代表無法使用其他 write protection，更不能由此推導 secure boot／fuse 鎖流程 |
| RESV0 pin 12 接 1M to GND | 舊 SCH 紀錄，原廠未規定此外部接法；需確認，不能先判 OK 或自行移除 |
| BOR／工作電壓 | p.26 BOR 2.4 V、p.27 建議供電下限 2.9 V 是不同條件；低於 2.9 V 已無工作保證，不代表必然觸發 BOR。電池 EOL 需量測晶片 pin 電壓與 reset |

Sources:
- [FR306x v0.4.9 開發參考](../fr306x_reference/README.md)：本次校正的原廠來源；未列事項保持待確認。
- Freqchip FR306x Technical Reference（公開版）、XMC XM25QH256D datasheet
