# Sheet 14 — NFC 天線 matching + 外接 NFC 天線座

**PDF Page**：14 / 15
**Ref Designator Prefix**：14xx
**圖面標題**：（無綠色標題框，但從 J1401 標籤與 netlist 連到 NFC 可推斷）
**對應提案書章節**：[05_技術方案_天線與外觀.md §P35 NFC 天線](../../00_project/05_技術方案_天線與外觀.md)

> **圖面紅字 3 條設計註記**：
> 1. 「750PF 没有，暂用 68PF+680PF，5%，100V，0603 封装」
> 2. 「56/68/39PF，1%精度的，100V，只有 0603 的封装」
> 3. 「电阻都是 0201 封装，与原厂确认」
> 4. 「天线扣线不能超 5mm，否则滤波电容 68PF/39PF 需要靠近天线放」

---

## 1. 功能定位

**PN7160 差分 TX/RX ↔ NFC 外接天線（40×30mm，貼在上殼內側，透過 pin 到 NFC 天線座 J1401）** 的匹配網路：
- **TX 路**：PN7160 TX1/TX2 差分 → L1401/L1402 160nH 匹配電感 → π 型 LC filter（68pF/680pF 替代 750pF、39pF 等）→ J1401 pin 1/2
- **RX 路**：PN7160 RXP/RXN 差分 ← C1401/C1414 耦合電容 ← 線上 R1402/R1405（2.2K 到 TX 路取 bias）
- **J1402**：保留的第二 NFC 天線座（未用）

---

## 2. 關鍵元件

### 2a. TX 路（左半）

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| L1401 | 160nH 1% | — | NFC_TX1 匹配電感 | | 13.56MHz 阻抗調整 |
| L1402 | 160nH 5% | — | NFC_TX2 匹配電感 | | |
| C1404 | 68pF 1% 100V | 0603 | TX1 旁 shunt | | |
| C1405 | 680pF 5% 100V | 0603 | TX1 旁 shunt | | **註：替代 750pF 的組合** |
| C1402 | NM_56pF 1% 100V | 0603 | **NM** TX1 shunt | | 預留調整位 |
| C1403 | 68pF 1% 100V | 0603 | TX1 series | | |
| C1406 | 68pF 1% 100V | 0603 | TX1 series（第二段）| | |
| C1407 | 39pF 1% 100V | 0603 | TX1 shunt（第二段）| | |
| C1408 | 68pF 1% 100V | 0603 | TX2 shunt | | |
| C1411 | 680pF 5% 100V | 0603 | TX2 shunt | | 替代 750pF |
| C1413 | NM_56pF 1% 100V | 0603 | **NM** | | |
| C1409 | 68pF 1% 100V | 0603 | TX2 series | | |
| C1410 | 39pF 1% 100V | 0603 | TX2 shunt | | |
| C1412 | 68pF 1% 100V | 0603 | TX2 shunt（跨 TX1/TX2 平衡）| | |
| R1403/R1404 | 0R 1/20W | 0603 | 到 J1401 的 jumper | | |

### 2b. RX 路（右半）

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| C1401 | 1.0nF 10% 100V | — | RXN 到 TX1 的耦合（取 field 感應）| | |
| C1414 | 1.0nF 10% 100V | — | RXP 到 TX2 的耦合 | | |
| R1401 | NM 0R 1/16W | 0603 | RXN 線 jumper | **NM** | |
| R1402 | 2.2K 1% 1/20W | 0603 | RX bias 分壓 | | |
| R1405 | 2.2K 1% 1/20W | 0603 | 同上 | | |
| R1406 | NM 0R 1/16W | 0603 | RXP 線 jumper | **NM** | |

### 2c. 連接器

| Ref | 型號 | 說明 |
|---|---|---|
| J1401 | 3.2×0.8×1896（彈片座）| NFC 天線主連接（3 pin：SIG + GND×2）|
| J1402 | 81800A286（彈片座）| 保留的第二 NFC 天線座，SIG + GND2 pin |

---

## 3. 電源輸入

本頁無獨立電源 rail —— NFC 天線端由 PN7160 的 TX1/TX2 driver 電流驅動，功率來自 Sheet 10 的 NFC_VDD。

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 |
|---|:-:|---|
| NFC_TX1 | in | ← Sheet 10 PN7160 pin 21 |
| NFC_TX2 | in | ← Sheet 10 PN7160 pin 19 |
| NFC_RXN | out | → Sheet 10 PN7160 pin 15 |
| NFC_RXP | out | → Sheet 10 PN7160 pin 16 |
| J1401 pin 1 (SIG) | out | → NFC 天線 pin 1 |
| J1401 pin 2 (SIG+) | out | → NFC 天線 pin 2 |
| J1401 pin 3 (GND) | — | GND |

---

## 5. 設計細節

- **典型 NFC matching 拓撲**：PN7160 reader IC 的 recommended matching 是「EMC filter → matching network（兩段 LC → ）→ antenna coil」，L 和 C 的 datasheet 建議值隨目標天線 Q 和自感而定。本板用 160nH + 68pF/680pF 組合，是朝「低 Q + 寬頻」設計，犧牲一些 field strength 換 robustness（金屬附近、各種卡類相容性）。
- **750pF → 68pF + 680pF 替代**：並聯等效 748pF，與 750pF 誤差 0.3%，實務上無影響；料件庫存問題不影響 RF 效能。
- **56pF NM 位置**：預留 fine-tune 用，EVT 期間 field tester 若發現阻抗偏移可加 56pF 平衡。
- **TX1/TX2 差分 push-pull**：PN7160 TX1/TX2 180° 相位，相當於跨天線的 full-wave 驅動（相對 single-ended +6dB field strength）。
- **RX 路耦合**：RXN/RXP 不是獨立線到天線，而是**從 TX 路耦合**（C1401/C1414）；PN7160 reader IC 的 RX 是監聽 TX 路上的卡片 load modulation（反向耦合進 PN7160 的 RX amp）。這是 reader-only mode 的典型拓撲（不做 P2P）。
- **「天線扣線不能超 5mm」layout 要求**：如果天線到 J1401 的扣線（spring finger 的連接段）超過 5mm，則 TX2 shunt 的 68pF/39pF 電容必須靠近天線放，否則 trace 自感會把 matching 調壞 → **PCB layout 階段必落實**。

---

## 6. 疑點 / Review Notes

- [ ] **🟠 750pF 替代**：等效 748pF 在數學上 OK，但實際元件 ESL (equivalent series inductance)：68pF 0603 ~500pH、680pF 0603 ~800pH，並聯後 ~300pH；750pF 單顆 ~700pH。替代後 ESL 下降 → 在 13.56MHz 阻抗略變，EVT 要驗。
- [ ] **🟠 Matching 設計未經實測調整**：圖面大量 NM/預留，表示 EVT 必須做 complete RF tuning session，拿 VNA 掃 S11 / smith chart 調值。
- [ ] **🟠 NFC 天線貼上殼**（提案書 P32），**40×30mm 面積**（P35）—— 若機構設計變更（例如縮小上殼）天線尺寸也要變，matching 要重做。SOW 要寫清天線尺寸凍結點。
- [ ] **📘 J1402 保留**：為什麼有兩個 NFC 天線座？可能是為同一塊板支援兩種機構（不同殼型），只裝其中一個 J1401 或 J1402；另一個 NM。要與機構團隊確認。
- [ ] **📘 PN7160 Antenna Design Guide**：NXP 有專門的設計指南 AN11955，階段 B 要對照本 sheet 實際元件值。
- [ ] **📘 讀距與金屬距離**：NFC 天線貼上殼，距離塑膠殼內的 PCBA 和金屬屏蔽罩距離要 ≥3mm 以上，否則 Q 值被金屬吸收下降 → 讀距縮短。機構評審時必須確認。
- [ ] **📘 歐規 vs 標準**：13.553-13.567MHz 範圍（提案書 P07），本板走標準 13.56MHz，CE/MIC/NBTC 認證對 ERP ≤4.5mW ERP（P07 標），本 matching 若 field strength 過強要降 TX power（PN7160 register 可調）。

---

## 7. 參考

- SCH PDF：Sheet 14
- 相關 sheet：Sheet 10（PN7160 TX1/TX2/RXP/RXN）
- 提案書章節：[05_P35 NFC 天線 40×30mm](../../00_project/05_技術方案_天線與外觀.md)、[02_P07 13.56MHz / 4.5mW ERP](../../00_project/02_客戶需求.md)
- Datasheet / App note：NXP PN7160 Antenna Design Guide（AN11955 或對應版本）
