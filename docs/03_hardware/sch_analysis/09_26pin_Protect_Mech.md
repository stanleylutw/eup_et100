# Sheet 9 — 外部 26pin 連接器 + DC 輸入保護 + ESD 陣列 + 機構

**PDF Page**：9 / 15
**Ref Designator Prefix**：09xx
**圖面標題**：外部 26pin 連接器；DC 輸入限流；測試點預留；缺少定位孔（PTH）、屏蔽罩、PCBA、標籤紙、點膠位、夾具定位點（DET）的料號，目前不對
**對應提案書章節**：[03_技術方案_系統設計.md §P12 Pin Definition](../../00_project/03_技術方案_系統設計.md)、[06_技術方案_線束與軟體.md §P41-P43 線束](../../00_project/06_技術方案_線束與軟體.md)

> **🟠 圖面紅字警告**：缺少定位孔（PTH）、屏蔽罩、PCBA、標籤紙、點膠位、夾具定位點（DET）的料號，目前不對 —— 這是 Quectel 自己標的 known-open-item。

---

## 1. 功能定位

**對外介面的匯流點**。所有通往線束的訊號都在此匯合：
- **J0901**：26-pin 2.0mm pitch 主連接器，對應提案書 P12 的完整 pin 定義（PWR + GND + ACC + ADC + 1-Wire + 3× Input + 1× Output + 3× RS232 + CAN + 4× ACC_OUT）
- **DC 輸入保護**：F0901 2A SMD fuse + D0901 PSBDAF60V3 TVS
- **TVS 陣列**：17 條外部 IO 線各配 TVS 防 ESD / 電源突波
- **17 個測試點**（TP0901~TP0917）
- **機構元件**：3 個屏蔽罩（BUCK / Charger / NFC）、PCBA、標籤、DET 定位孔、6 個 PTH 定位柱

---

## 2. 關鍵元件

### 2a. 連接器

| Ref | 型號 | 功能 | 備註 |
|---|---|---|---|
| J0901 | C2012RD21307T0102BD | **26-pin 2.0mm pitch 水平連接器** | 對應提案書 P12 pin 定義（複製於下方）|

**J0901 pin 定義**（SCH 標籤 + 提案書 P12 對照）：

| Pin | 訊號 | 色碼 | 規格 | SCH 中 net name |
|:-:|---|:-:|---|---|
| 1  | PWR_IN (B+) | 紅 | 1A fuse | PWR_IN |
| 2  | GND | 黑 | — | GND |
| 3  | ACC（點火） | 藍 | >5V 觸發 | ACC_INT_IN_CON |
| 4  | ADC（0-33V） | 綠 | 油量感測 | oil_ADC_IN_CON |
| 5  | 1-Wire 溫度 | 白 | DS18B20 ×4 | 1_Wire_Temp_IN_CON |
| 6  | GND | 黑 | — | GND |
| 7  | Input1 | 青 | >5V 觸發 | IO1_INT_IN_CON |
| 8  | Input2 | 紫 | | IO2_INT_IN_CON |
| 9  | Input3 | 棕 | | IO3_INT_IN_CON |
| 10 | Output | 灰 | 負輸出 | PWR_SW_CTRL_CON（200mA 標註）|
| 11 | RS232_1 TX | 黃 | 9600 | RS232_TXD1 |
| 12 | GND | 黑 | — | GND |
| 13 | RS232_2 TX | 黃 | 9600 | RS232_TXD2 |
| 14 | ACC_OUT | 紅 | | ACC_INT_OUT_CON |
| 15 | RS232_2 RX | 綠 | | RS232_RXD2 |
| 16 | ACC_OUT | 紅 | | ACC_INT_OUT_CON（同 pin 14）|
| 17 | GND | 黑 | — | GND |
| 18 | ACC_OUT | 紅 | | ACC_INT_OUT_CON |
| 19 | CAN_H | 黃 | J1939 | CAN_H |
| 20 | CAN_L | 綠 | | CAN_L |
| 21 | GND | 黑 | — | GND |
| 22 | ACC_OUT | 紅 | | ACC_INT_OUT_CON |
| 23 | RS232_0 TX | 黃 | 115200 | RS232_TXD0 |
| 24 | RS232_0/1 RX | 綠 | | RS232_RXD0 / RXD1（待釐清）|
| 25 | GND | 黑 | — | GND |
| 26 | ACC_OUT | 紅 | | ACC_INT_OUT_CON |

**4 支 ACC_OUT**：pin 14/16/18/22/26 共 5 支（圖面標註 4 組 ACC_OUT 接到各 RS232/CAN 子接頭的 4-pin wire 中的 ACC 線），全部來自 Sheet 11 的 Q1110 BJT 單一 collector —— 共用一個驅動點，任何一條短路到 GND 都會把全部拉死（見 [Sheet 11](11_IOExp_LevelShift.md) §6 疑點）。

### 2b. DC 輸入保護

| Ref | 型號 | 功能 | DNP? |
|---|---|---|:-:|
| F0901 | 1206WJF200A072V | 2A SMD fuse | |
| R0901 | NM 0R 1/8W | 預留 jumper 位 | **NM** |
| D0901 | PSBDAF60V3 | **60V TVS**（Vrwm 60V, Vclamp ~75V） | |
| D0910 | NM_SMD30KCA | **NM** | 預留更強 TVS | **NM** |

**保護鏈**：PWR_IN → F0901 2A fuse → D0901 TVS 60V clamp → PWR_IN_9-36V 到 Sheet 6 DCDC

- Fuse 2A @ 36V 輸入時最大功率 72W；DCDC 效率 85% → 36V/5V 輸出比約 7:1，5V 側若最大 5A 輸入側才 ~0.8A → 2A fuse 裕量充足
- 60V TVS 應付車用 load dump 典型 ~75V peak，但是 ISO 7637-2 Pulse 5A（load dump）要求 clamp 到 86V max → **60V TVS 可能 marginal**，EMC/EFT 測試可能要升到 70V TVS

### 2c. ESD TVS 陣列（右半區域）

每條外部 IO 線都配 TVS，分成兩區畫：
- **左區**（PWR_IN、ACC_INT_IN、1_Wire_Temp_IN、IO1/IO2/IO3_INT_IN、PWR_SW_CTRL、RS232_TXD1、RS232_TXD2、RS232_TXD0 共 10 條）
- **右區**（oil_ADC_IN、IO2_INT_IN、PWR_SW_CTRL、RS232_RXD1、ACC_INT_OUT ×4、RS232_RXD2、RS232_RXD0 共 10 條）

每顆 TVS 一端接 net 一端接 GND，型號多樣：`SMD40KCA`、`PESDNC2F30V8N`、`PESDNC2F28V8N`、`PTVS6C3N15VUH`、`PESDNC2FD5V8` 等（依信號型別/電壓分級）。

**總共 17 個 TP**（TP0901~TP0917），每條外部 IO 都有 debug 探針點。

### 2d. 機構元件（紅色虛線框內，**料號待補**）

| Ref | 說明 | 料號狀態 |
|---|---|---|
| H0901 / H0904 / H0905 | LG69T-TE-A_V1.2 屏蔽罩 ×3 | ⚠️ 標 TGH0901 的 NM-TB3164J；料號可能不對 |
| | 分別罩住 BUCK（Sheet 6 DCDC）、Charger IC（Sheet 8）、NFC（Sheet 10）| |
| PCB0901 | PCBA 本身的 silk 標籤 | QEM885EZA-CN01_PBT6_V1.2_PCB_TM_R1 |
| LAB0901 | 標籤紙 | FC5851U_Label_GDL_R2 |
| TP0916 / TP0917 | DET 定位點 | |
| HOLE0901~HOLE0906 | 定位柱 PTH ×6 | pth_cir1_80_3_50（機械孔） |

---

## 3. 電源輸入

| Rail | 來源 | V | I 上限 | 用途 |
|---|---|:-:|:-:|---|
| PWR_IN (線束) | 車用電瓶 | 9-36V | 1A（線束端 fuse） | 經 F0901 2A fuse 進板 |
| PWR_IN_9-36V | 本板 F0901 下 | 9-36V | 2A | → Sheet 6 DCDC |

---

## 4. 介面（I/O Nets）

| 類別 | Net 群 | 連接到 |
|---|---|---|
| 電源 | PWR_IN、GND | 車上電瓶 |
| 數位輸入 | ACC_INT_IN_CON、IO1/IO2/IO3_INT_IN_CON | Sheet 11 BJT 偵測 |
| 類比輸入 | oil_ADC_IN_CON | Sheet 11 → MCU ADC |
| 1-Wire | 1_Wire_Temp_IN_CON | Sheet 11 |
| 數位輸出 | PWR_SW_CTRL_CON（Output） | Sheet 11 Q1108 PFET |
| ACC 輸出（4+ pin） | ACC_INT_OUT_CON ×5 pin | Sheet 11 Q1110 BJT collector |
| RS232 | RS232_TXD0/1/2、RXD0/1/2 | Sheet 5 UM3221 ×3 |
| CAN | CAN_H、CAN_L | Sheet 5 SIT1042 經 F0501/F0502 fuse |

---

## 5. 設計細節

- **線束 end 1A fuse + 板端 2A fuse 兩段保護**：線束端快熔（1A），板端慢熔（2A）—— 線束短路優先熔線束端（易換），板端是最後防線。符合汽車電子標準做法。
- **TVS 分級選型**：
  - PWR_IN：60V TVS 應付 load dump
  - ACC/IO/ADC：28V TVS 應付 24V 系統 load dump + 30V 尖峰
  - 數位 I/O 輸出 / RS232：15V-30V TVS
  - 1-Wire：高速訊號用低電容 TVS
- **測試點 17 個**：量產每條外部線都可用飛針測，充分支援 ICT。
- **屏蔽罩 ×3**：BUCK（Sheet 6 DCDC 的高頻 EMI 源）、Charger IC（Sheet 8，比較少 EMI 但避免被 LTE 干擾）、NFC（Sheet 10，13.56MHz 不想輻射到 LTE 低頻）—— 三個 EMI/EMC 敏感區各罩一個 cover 是標準做法。
- **ACC_INT_OUT 共 5 支 pin**：對應提案書 §P41 的線束設計，每條 RS232 / CAN 子線束都帶一條 ACC 給外接設備喚醒用。

---

## 6. 疑點 / Review Notes

- [ ] **🔴 CRITICAL（Quectel 自標）**：機構 BOM 料號未定（PTH / 屏蔽罩 / PCBA / 標籤 / DET）—— EVT 樣品可用現有料號湊，量產前必須鎖定。
- [ ] **🟠 Load dump TVS 60V 可能不夠**：ISO 7637-2 Pulse 5A/5B 的 clamp 要求可能超 60V TVS 的 Vclamp，實際 EMC 測試驗證。
- [ ] **🟠 5 支 ACC_INT_OUT 共用 BJT collector**：見 [Sheet 11](11_IOExp_LevelShift.md) §6；一條短路全掉。
- [ ] **🟠 pin 24 可能被 RS232_0 和 RS232_1 的 RXD 共用**：對照提案書 P12 應該是 pin 23-26 給 RS232_0；但 SCH 看 RXD1 也標 pin 24 → 要查 netlist。
- [ ] **📘 Fuse 2A @ 36V = 72W 瞬時能承受，熔斷時間要查 datasheet**：car 冷啟動 inrush 可能數 A 瞬時，fuse 不能太快熔。
- [ ] **📘 1A 線束端 fuse 位置**：提案書 §P42 寫在 10-pin adapter cable 的 P1/P3 兩條帶 1A fuse —— 本 SCH 板端只畫 2A fuse，1A fuse 在線束，兩者配合。
- [ ] **📘 大量 DNP TVS**：圖面右區 TVS 陣列有部分 NM，階段 B 要列清楚哪些實際裝、哪些預留。
- [ ] **📘 無反接保護**：9-36V 若接反會直接過 fuse → TVS clamp → DCDC 的 input cap；若反接 36V 可能 damage DCDC input FET。**沒看到 series schottky 或 PFET 反接保護**，階段 B 必問 Quectel 是否故意省略（依賴線束端做）。

---

## 7. 參考

- SCH PDF：Sheet 9
- 相關 sheet：Sheet 5（RS232/CAN）、Sheet 6（PWR_IN 下游）、Sheet 11（所有外部 IO 的 BJT 偵測/驅動）
- 提案書章節：[03_P12 Pin Definition](../../00_project/03_技術方案_系統設計.md)、[06_P41-P43 線束設計](../../00_project/06_技術方案_線束與軟體.md)
