# SCH V1.1 對照 提案書 V1.8 風險清單

> **2026-10-02 MCU 新依據**：原廠 v0.4.9 訂購表列 FR3068E-C AEC-Q100「否」，需核對車規要求與 exact part 資格；另有 2 MB／512 KB 與 SDK window 的 mapping、pin-change revision、SWD／IO voltage bank、sequencing 尚待確認。原有「SCH 已實作」不代表這些新差異已解除。詳見 [開發對照](../fr306x_reference/05_et100_development_reference.md) 及 [Q48 V1.3](100_questions_for_quectel.md)。

**目的**：把 [10_風險與談判清單.md](../../00_project/10_風險與談判清單.md) 的 9 項逐一對照 SCH V1.1，標註**硬體層面是否已回應**、**還需要什麼**。

> 分類：✅ SCH 已實作 | 🟡 SCH 有相關設計但未完整解決 | 🔴 SCH 無法解決（需業務/認證/韌體層面）| 🆕 SCH 本身新增的 open issue

---

## 風險對照表

### P0 — 必須在 Kick-off 前解決

#### 1. LTE 模組區域覆蓋不足 🔴

**提案書問題**：EG800Q-EU 不支援 TDD B40（泰馬印越主要 band）；無 2G/3G fallback。

**SCH 層面觀察**：
- [Sheet 2](02_LTE_EG800Q-EU.md)：U0201 確實是 EG800Q-EU
- [Sheet 13](13_RF_LTE_BT_Antenna.md)：新增 **MXD8544AE antenna tuner**，提案書未提

**結論**：🔴 **SCH 無法解決 band 覆蓋問題**。tuner 只改天線阻抗匹配，無法擴展 baseband 支援的 band。此風險**只能在業務面解決**（換 GL 版、或接受失去 TDD B40 市場）。
- 若最終換 EG800Q-GL，SCH 需最小改動（同 QCX216 chipset，大概率 pin-to-pin 相容；需 Quectel 書面確認）
- Sheet 15 的 reference 電源樹已寫「EG800Q-GL/EU」—— 顯示 Quectel 內部已預期此改動
- antenna tuner 存在只是加分項（低頻效率補救），不緩解 band

**行動**：Quectel 書面提供 EG800Q-EU → GL 的 pin 相容性文件；確認 tuner band 分配表（SDR_GRFC_1/2 四組對應哪些 band 的 matching）。

---

#### 2. 量產時程矛盾 🔴（非 SCH 範疇）

**提案書問題**：客戶 2026 Q4 需 MP vs Quectel 2027.03.31 目標 → 6 個月落差。

**SCH 層面觀察**：V1.1 定稿於 2026-09-23，是 Kick-off 前的設計基線。EVT 建板尚未開始。

**結論**：🔴 時程議題非 SCH 層面，但 SCH 的狀態印證時程壓力：許多機構 BOM（Sheet 9 標 "缺少料號"）、電池 pin 序未定、tuner band 分配未定 → **還不夠成熟到立刻送 EVT**，再修 1-2 輪 schematic 更實際。

---

#### 3. 認證表與需求表不一致 🔴（非 SCH 範疇）

**結論**：SCH 無法涉及認證主體 / 樣品數 / SAR 等問題，純業務合約面。

---

### P1 — 影響設計與 BOM 的關鍵疑慮

#### 4. 備援電池續航與功耗算法 🟡

**提案書問題**：P14 表自我矛盾（70% vs 80% discharge efficiency、LTE start/search 0 秒），4.2 小時續航算式對不上。

**SCH 層面觀察**：
- [Sheet 8](08_BatteryCharge.md)：電池 J0801（WF15003-01207）確認存在，但 **pin 序未定**
- [Sheet 7](07_PowerLDO_3V3_1V8.md)：ORing 設計正確（主電掉線 → 電池接管 LDO 群）
- [Sheet 2](02_LTE_EG800Q-EU.md)：LTE_VBAT 可切（Q0232 BJT）→ 不用時完全關 LTE
- [Sheet 11](11_IOExp_LevelShift.md)：AW9523 IO expander 可關多條 rail（NFC/VDD_3V3/BUZZER）→ context-aware 省電有硬體支援

**結論**：🟡 **SCH 已提供「低功耗模式」的硬體基礎**（多條可切 rail）；提案書的「平均 371.8mW、4.2h」算的是**最差狀況（全部開啟）**，實際用 deep sleep + LTE PSM + GNSS backup 可大幅延長。但：
- **SCH 無法回答「續航到底多久」—— 要實測**
- 電池 pin 序未定 → EVT 前風險
- LTE_VBAT 用 BJT 開關（Vce 損耗）→ 比 PMOS 浪費
- D0701/D0702 ORing 的 Vf 讓 VBAT 到 LDO input 掉 0.3V → 電池放電末期（3.5V）時 3V3 rail 吃緊

**行動**：
- EVT 前完成電池 pin 序
- EVT 階段實測「熄火後」續航：全靜音 / 15s 回報 / 60s 回報三組 scenario
- 考慮 LTE_VBAT 改 PMOS

---

#### 5. 30 天本地儲存規格 🟡

**提案書問題**：256M-bit (32MB) flash vs 240 筆/小時 × 24 × 30 = 172,800 筆 → 單筆 ~115-140 bytes，CAN 全存必爆。

**SCH 層面觀察**：
- [Sheet 1](01_MCU_FR3068E-C.md)：U0103 XM25QH256DXIQT 確認 **256Mb = 32MB** —— SCH 符合提案書
- SPI 四線（CS/MISO/MOSI/CLK）+ 兩線保留（/HOLD、/WP）全接 4.7K pull-up → **沒用 hardware WP、無 secure boot**，正常
- Deep power-down 0.2µA（datasheet 標）

**結論**：🟡 **SCH 層面只能證明 flash 容量 = 32MB**，無法回答 payload / wear leveling / 循環覆寫策略。這些是**韌體 + 規格文件**層面，SCH 做對了容量選型。

**行動**：韌體 SOW 要明定 payload byte 上限（<128B 建議）、是否壓縮、FIFO/LIFO 覆寫、CAN frame filter white-list。

---

#### 6. 內置天線效能不足且無實測交付 🟡

**提案書問題**：LTE 600-960MHz 效率 20-40%，4 天線擠一殼內互擾風險。

**SCH 層面觀察**：
- [Sheet 13](13_RF_LTE_BT_Antenna.md)：**新增 MXD8544AE antenna tuner** → 可針對各頻段切換 matching → **低頻效率可能改善**（SDR_GRFC_1/2 兩線選 4 條 matching path）
- Sheet 13 大量 NM / CO-PAD 位 → RF 完全靠 EVT tuning
- [Sheet 14](14_RF_NFC_Antenna.md)：NFC 匹配用 68pF+680pF 替代 750pF → **預計 EVT 必測讀距**
- Sheet 13 BT 側多出 RAS2442A35-MC SPDT switch → 可能是 chain select，待查

**結論**：🟡 **SCH 給了 RF tuning 的充足設計空間**（matching 預留 / tuner / BT switch），但沒做出任何實測承諾。「EVT 必測天線 TRP/TIS」仍是必要條件。

**行動**：Sheet 13/14 的 EVT 要包含：
- 各頻段 TRP/TIS（conducted + radiated、含不同 tuner state）
- GNSS CN0 + TTFF（熱/暖/冷）
- NFC 讀距（4 類卡）
- BT RSSI
- 天線間 isolation
- **整機裝殼固定於車體後**的實車 OTA

---

### P2 — 影響長期維護的合約條款

#### 7. 軟體 SOW 條目太泛 🔴（非 SCH 範疇）
#### 8. 交付物與 IP 歸屬 🔴（非 SCH 範疇）
#### 9. NRE 分期付款與失敗條款 🔴（非 SCH 範疇）

這三項純業務合約，SCH 無法涉及。

---

## 🆕 SCH 本身引入的新 open issues

從 13 份 Sheet MD 彙整 SCH 層面新發現，**這些是提案書沒提、SCH 看到的新問題**：

### 🆕 N1. 電池連接器 pin 序未定（🔴 CRITICAL）
Sheet 8 圖面紅圈「电池 pin 序？」。EVT 前必須鎖定；接錯燒料件。
**責任**：Quectel + 電池廠

### 🆕 N2. 機構 BOM 料號不齊（🔴 Quectel 自標）
Sheet 9 圖面紅字「缺少定位孔（PTH）、屏蔽罩、PCBA、標籤紙、點膠位、夾具定位點（DET）的料號，目前不對」。
**責任**：Quectel

### 🆕 N3. Sheet 6 title block 錯誤（🟢 低）
Sheet 6 顯示 "SHEET 15 OF 15"，應為 "SHEET 6 OF 15"。圖框模板沒改。EDA 匯出資料可能對不上。

### 🆕 N4. Antenna tuner band 分配未提供（🟠）
Sheet 13 U1301 MXD8544AE 4 條 RF path 分別對應哪些 band 的 matching，SCH 沒標。
**責任**：Quectel 補 band 分配表。

### 🆕 N5. NFC 匹配電容用 68pF+680pF 替代 750pF（🟠）
Sheet 14 圖面紅字。等效 748pF 數學 OK，但 ESL 略變，EVT 必測讀距。
**責任**：EVT 測試 + 必要時換料。

### 🆕 N6. VDD_PP_3V3 命名錯誤（🟢 低）
Sheet 5 net 命名 `VDD_PP_3V3` 但實際載 5V（RS232 chip VCC）。影響閱讀不影響功能。
**責任**：Quectel 改 net name。

### 🆕 N7. 5 支 ACC_INT_OUT 共用一個 BJT collector（🟠）
Sheet 11 Q1110 單一 BJT 推 5 支對外 ACC_OUT，任一短路到 GND 全掉。
**行動**：改獨立 driver 或獨立 fuse。

### 🆕 N8. CAN STB 經 I2C IO expander 控制，有延遲（🟠）
Sheet 11；CAN wake from standby 延遲 ms 級，J1939 診斷服務可能漏首幀。
**行動**：MCU 直接控 STB，不經 AW9523。

### 🆕 N9. LTE_VBAT 用 BJT 而非 PMOS（🟠）
Sheet 2 Q0232 2SC4617；突波時 Vce 損耗讓 LTE_VBAT 跌破 EG800Q spec 下限（3.4V）邊緣。
**行動**：改 YJL2305A 或類似 PMOS。

### 🆕 N10. GNSS 子板主電源經 D1201 掉 Vf 0.3V（🟠）
Sheet 12；MCU_GNSS_3V3 3.3V - 0.3V = 3V 給子板，可能在 AG3352Q 的 VDD min 邊緣。
**行動**：確認 AG3352Q min VDD；若不符，改 ideal diode 或 Si-ORing。

### 🆕 N11. 無反接保護（🟠）
Sheet 9；板端無 series PFET 或 schottky 做 polarity protection。若接反可能損壞 DCDC input FET。
**行動**：確認依賴線束端做反接保護；或加板端 PFET。

### 🆕 N12. BJT UART level shift 需韌體反相（🟢 標記）
Sheet 11；MCU UART peripheral 必須設 TX/RX polarity invert，否則 LTE AT command 全亂碼。
**行動**：韌體開發須知。

### 🆕 N13. AW9523 預設 output 行為未確認（🟠）
Sheet 11；若 POR default = all low，開機瞬間所有可切 rail off，MCU 要撐到 I2C 寫完 EN。
**行動**：階段 B 查 AW9523 datasheet。

### 🆕 N14. GNSS 子板獨立 SCH 不在本 PDF（🟠）
Sheet 12 只畫主板端介面；子板上 AG3352Q 本體、天線、LNA、SAW 等 SCH 需另向 Quectel 索取才能完整 review。
**行動**：向 Quectel 要 GNSS 子板 SCH。

### 🆕 N15. BT switch U1302 RAS2442A35-MC 用途不明（🟡）
Sheet 13；FR3068E-C BT 內部若無 TX/RX switching 需求，此 SPDT 可能是 chain select 或 antenna diversity 預留。
**行動**：階段 B 查 FR3068E-C BT 架構 + RAS2442 datasheet。

### 🆕 N16. 提案書 vs SCH 的 DCDC 型號不一致（🟡）
提案書 P25 寫 JW5293，SCH Sheet 6 用 JW5357。都是 JoulWatt sync buck，但不同型號。
**行動**：Quectel 書面確認用哪個；對 datasheet 核 Vout 設定。

### 🆕 N17. 功能框圖 Sheet 15 寫 EG800Q-GL/EU（🟢 低）
Sheet 15 reference block diagram 保留 GL 選項；Sheet 2 BOM 確定是 EU。若未來換 GL 要同步改。

---

## 彙整：可以簽 SOW 前應完成的 SCH 層面清單

| # | 項目 | 來源 | 責任 |
|:-:|---|---|---|
| 1 | 電池 pin 序鎖定 | [N1](99_risks_vs_proposal_v1.8.md) | Quectel + 電池廠 |
| 2 | 機構 BOM 料號補齊 | [N2](99_risks_vs_proposal_v1.8.md) | Quectel |
| 3 | Antenna tuner band 分配表 | [N4](99_risks_vs_proposal_v1.8.md) | Quectel |
| 4 | DCDC 型號確認（JW5293 vs JW5357） | [N16](99_risks_vs_proposal_v1.8.md) | Quectel |
| 5 | AG3352Q 子板獨立 SCH 提供 | [N14](99_risks_vs_proposal_v1.8.md) | Quectel |
| 6 | AG3352Q min VDD 查核 → D1201 設計調整 | [N10](99_risks_vs_proposal_v1.8.md) | Quectel + 階段 B |
| 7 | AW9523 POR default 狀態確認 | [N13](99_risks_vs_proposal_v1.8.md) | 階段 B |
| 8 | ACC_OUT 共用 BJT 問題改善 | [N7](99_risks_vs_proposal_v1.8.md) | Quectel |
| 9 | CAN STB 直連 MCU 不經 AW9523 | [N8](99_risks_vs_proposal_v1.8.md) | Quectel |
| 10 | LTE_VBAT BJT → PMOS 評估 | [N9](99_risks_vs_proposal_v1.8.md) | Quectel |
| 11 | 反接保護確認 | [N11](99_risks_vs_proposal_v1.8.md) | Quectel |
| 12 | NFC 替代電容 ESL 影響實測 | [N5](99_risks_vs_proposal_v1.8.md) | EVT |

**時間敏感度**：#1、#2、#5 應該在 Kick-off 前就要有；#3-#4、#8-#10 可在 EVT build 前；其餘 EVT 期間解決。
