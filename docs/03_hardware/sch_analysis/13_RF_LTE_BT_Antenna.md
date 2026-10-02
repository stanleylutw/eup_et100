# Sheet 13 — LTE 天線 matching + **LTE Antenna Tuner** + BT 天線 matching

**PDF Page**：13 / 15
**Ref Designator Prefix**：13xx
**圖面標題**：LTE ANT；LTE Tuner RF；BT ANT；鋼片天線（×2）
**對應提案書章節**：[05_技術方案_天線與外觀.md §P33 LTE 天線 §P34 BT 天線](../../00_project/05_技術方案_天線與外觀.md)、[10_風險與談判清單.md §6 內置天線效能](../../00_project/10_風險與談判清單.md)

> **🔴 本頁有兩個未在提案書中出現的硬體**：
> 1. **MXD8544AE antenna tuner (U1301)** — 可切 4 條 RF path 的阻抗匹配開關；由 LTE 模組的 `SDR_GRFC_1/2` 兩線控制。
> 2. **RAS2442A35-MC BT SPDT switch (U1302)** — BT 側也放了可切換路徑。
>
> 這兩個 IC **都不在提案書 V1.8 P31 關鍵零件表裡**，是實作層加碼。

---

## 1. 功能定位

**LTE 與 BT 兩套天線的匹配網路與訊號路由**：

- **LTE 路徑**：EG800Q-EU `ANT_MAIN` pin 36（50Ω）→ π-matching network → J1301 耦合器（test point bypass compatible design）→ U1301 MXD8544AE antenna tuner → 鋼片天線（steel sheet antenna）
- **BT 路徑**：FR3068E-C pin 3 `ANT` (50Ω，透過 Sheet 1 R0101) → 匹配網路 → U1302 RAS2442A35-MC SPDT switch → J1302 耦合器 → 鋼片天線

兩條 RF 路徑都走獨立鋼片天線，由機構組裝（提案書 §P32 說 LTE 貼下殼、BT 貼下殼）。

---

## 2. 關鍵元件

### 2a. LTE RF Path

| Ref | 型號 | 功能 | DNP? | 備註 |
|---|---|---|:-:|---|
| C1301 | 100pF 25V 0603 | ANT_MAIN DC blocking | | |
| R1305 | 0R 1/20W 0603 | DC jumper | | |
| C1303/C1304 | NM_100pF | **NM** | 匹配位預留 |  |
| L1301 | NM-4.3nH | **NM** | 匹配電感預留 | |
| D1301 | TEA10201V05A0 | TVS | | 低電容 RF TVS |
| C1307 | NM-3.0pF | **NM** | | |
| **J1301** | **818011998** | **耦合器 / coupler** | | 「LTE TEST POINT BYPASS Compatible Design」注記 |
| R1306/R1307 | 0R（CO-PAD 位） | | **可切成 NM-0R 做 test point tap** | |
| R1308 | 0R | | | |
| C1305 | NM-100pF | **NM** | | |
| L1302 | NM-8.2nH | **NM** | | |
| **U1301** | **MXD8544AE** | **4-path antenna tuner / aperture tuner** | | 由 SDR_GRFC_1/2 控；RF1/2/3/4 四條 RF 輸入 |
| R1309~R1312 | 0R 1/20W × 4 | 四條 RF path 的 jumper | | |
| C1302 | NM-0.5pF | **NM** | tuner RF 旁並聯電容 | |
| C1306 | 100pF 25V 0603 | VDD 旁 cap | | |
| C1308 | NM-0.5pF | **NM** | | |
| C1309 | NM-100pF | **NM** | | |
| R1313 | 0R | | LTE_EXT_1V8 → U1301 VDD | |

### 2b. BT RF Path

| Ref | 型號 | 功能 | DNP? | 備註 |
|---|---|---|:-:|---|
| C1310 | 33pF 50V 0603 | ANT DC blocking | | |
| R1318 | 0R | | | |
| C1312/C1313 | NM-0.5pF / NM-3.0pF | **NM** | | |
| R1319 | 0R（CO-PAD）| | | |
| **U1302** | **RAS2442A35-MC** | **SPDT RF switch**（PIN 1=TRx 共用，PIN 2/4=分支 Ant） | | 可能是 BT TX/RX 切換或天線切換（規格待查） |
| L1304/L1305 | NM-18nH 匹配電感 | **NM** | | |
| C1314/C1315 | NM-0.5pF/NM-3.0pF | **NM** | | |
| R1320/R1321 | 0R（CO-PAD）| | | |
| J1302 | 818011998 | 耦合器 | | 和 LTE 側同型號 |
| D1302 | TEA10201V05A0 | TVS | | |
| R1322~R1324 | 0R（部分 NM） | 匹配 jumper 群 | | |
| **HOLE1301~1304**：**LTE 鋼片天線的 PTH 固定孔** | | 4 處金屬彈片 pad | | 參照「钢片天线」註記 |
| **HOLE1305/1306**：BT 鋼片天線 PTH | | 2 處 | | |

---

## 3. 電源輸入

| Rail | 來源 | V | 用途 |
|---|---|:-:|---|
| LTE_EXT_1V8 | ← Sheet 2 EG800Q-EU VDD_EXT 經 Sheet 11 分路 | 1.8V | U1301 MXD8544AE tuner 內部邏輯 VDD |

BT switch U1302 的 VDD 可能從 MCU_3V3 或無獨立 VDD（RF switch 常用 bias 經 RF port），**待查 datasheet**。

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 | 用途 |
|---|:-:|---|---|
| 50ohm_MAIN_ANT | in | ← Sheet 2 pin 36 ANT_MAIN | LTE RF |
| SDR_GRFC_1 | in | ← Sheet 2 pin 104 | tuner band select bit 0 |
| SDR_GRFC_2 | in | ← Sheet 2 pin 67 | tuner band select bit 1 |
| 50ohm_BT_ANT | in | ← Sheet 1 pin 3（經 R0101 50Ω） | BT RF |
| LTE_EXT_1V8 | in | ← Sheet 2 | tuner VDD |

---

## 5. 設計細節

- **「LTE TEST POINT BYPASS Compatible Design」**：J1301/J1302 是 directional coupler，用於工廠量產 RF 測試時可以把訊號從 coupler port 取出送到測試儀，正常運作時 coupler 幾乎不影響主路（~0.5dB 插損）。設計意圖：量產線一條板子只插一個同軸針就能測 TRP/TIS。
- **MXD8544AE antenna tuner 的實際用途**：
  - **不是** band 擴展（module baseband 已固定支援 B1/3/5/7/8/20/28）
  - **是** 針對不同頻段切換天線 matching network，提升 return loss / efficiency
  - 典型用法：700MHz 低頻用一組 L/C、2600MHz 高頻用另一組 → 單一天線在寬頻段都能用
  - 本板 RF1-4 四條 path 可能對應：{Band 20, Band 8, Band 3+28, Band 1+7} 或類似分組 → **要與 Quectel 確認 band 分配**
- **大量 NM 元件**：L1301/L1302/C1303/C1304/C1305/C1306/C1307... 全是 NM —— **matching 元件在 EVT 調試前先不裝**，實測天線後再決定要加哪些 LC；這是 RF 設計常態。
- **「CO-PAD」注記**：某些 0R 標 CO-PAD（coupling pad），代表 PCB 上有 shunt pad 可以改並聯元件；是**預留 matching 位**而非功能零件。
- **「Close to ANT」**：L1301、L1302 旁邊都有「Close to ANT」dashed box —— 這些 LC 必須靠近鋼片天線放，距離走線影響 Q 值。
- **BT 鋼片天線**：僅 2 個固定孔（HOLE1305/1306），BT 天線小尺寸（30×5mm per 提案書 P34），比 LTE（13×65mm per P33）小很多。
- **PCB 走線為 50Ω**：50ohm_MAIN_ANT_1 ~ _7、50ohm_Bypass_LTE_testpoint、50ohm_BT_ANT_0 ~ _11，所有 RF 走線在 netlist 用「50ohm_」prefix 標示，PCB layout 要用 controlled impedance。

---

## 6. 疑點 / Review Notes

- [ ] **🔴 CRITICAL：antenna tuner 不能補 band**：MXD8544AE 不改變 baseband 支援的 band，**EG800Q-EU 不支援 B40 的事實不會因 tuner 存在而改變**。這點在 [99_risks_vs_proposal_v1.8.md](99_risks_vs_proposal_v1.8.md) 要特別標明。
- [ ] **🟠 tuner 的 band 分配表未提供**：SDR_GRFC_1/2 的 00/01/10/11 分別切到哪組 matching，SCH 未標明，需 Quectel 文件說明。
- [ ] **🟠 BT switch U1302 的用途不明**：單一 BT 天線的話，SPDT switch 可能用於：(a) TX/RX 切換（經典 BT 收發分路）、(b) 兩個天線 diversity、(c) BT/BLE 內部 chain 切換；本板只接一條鋼片天線 → **(c) 或 chain 選擇可能性高**。待 FR3068E-C BT datasheet 查。
- [ ] **🟠 LTE 天線效率提案書 P33**：600-960MHz 20-40%，1700-2700MHz 45-55% —— 對 B5/B8（低頻主力）效率偏低。tuner 應能把低頻 return loss 改善，但效率上限仍受天線機構決定。**EVT 必測 TRP/TIS 並與提案書數字比對**。
- [ ] **📘 所有 NM 都是調試預留**：EVT 的 RF tuning session 會逐一填值；量產 BOM 以 EVT 固化後為準。
- [ ] **📘 鋼片天線 vs FPC 天線**：提案書 P33 寫「FPC 排線天線」，SCH 寫「鋼片天線」—— **用詞不一致，實際用鋼片**（FPC 需另外彎折固定，鋼片直接彈片扣）。
- [ ] **📘 單天線 LTE main-only**：沒有 AUX antenna，意味著不做 LTE Rx diversity → 弱訊號下 downlink 效能會比有 diversity 低 ~2-3dB。EG800Q-EU 是 Cat-1 bis 簡化版本，可能本來就無 diversity 支援，待確認。

---

## 7. 參考

- SCH PDF：Sheet 13
- 相關 sheet：Sheet 2（LTE module + SDR_GRFC 控制）、Sheet 1（BT ANT pin 3）
- 提案書章節：[05_P32-P34 天線布局 LTE/BT](../../00_project/05_技術方案_天線與外觀.md)、[10_P6 內置天線效能](../../00_project/10_風險與談判清單.md)
- Datasheet：Maxscend MXD8544AE antenna tuner、Rafael RAS2442A35-MC（階段 B 必查）
