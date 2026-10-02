# Sheet 6 — 一級 DCDC 9-36V→5V + 二級 DCDC 5V→4V

**PDF Page**：6 / 15
**Ref Designator Prefix**：06xx
**圖面標題**：9~36V 轉 5V 電源 DCDC；5V 轉 4V 電源 DCDC
**對應提案書章節**：[03_技術方案_系統設計.md §P11 電源方案細節](../../00_project/03_技術方案_系統設計.md)、[04_技術方案_關鍵零件.md §P24 SGM61630A §P25 JW5293](../../00_project/04_技術方案_關鍵零件.md)

> **提案書註**：P25 寫 LTE 模組用 **JW5293**，實際 SCH 用 **JW5357**（也是 JW SOT 系列 synchronous buck）。型號不一致，見 §6。

---

## 1. 功能定位

**全板最上游電源**。把車上 9-36V 直流經兩級 DCDC 轉成下游所有電源的 input：
- 第一級 → VDD_5V（經 ORing 分 _MCU 3A 給下游二級 DCDC，_PP 1A 給 Charger / RS232 / NFC analog / CAN）
- 第二級 → VDD_4V（進 Sheet 7 四顆 LDO 的 VDD_LDO_4V 總匯流排）

---

## 2. 關鍵元件

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| U0601 | SGM61630AXPS8GTR | SOIC-8 EP | 一級 Buck：Vin 4.3-60V, 3A, 500kHz, HS-FET 140mΩ, Iq 50µA | | FB divider 57.6k/10k → Vout=0.75×(57.6/10+1)=**5.07V**；圖面標 `fsw=500kHz` |
| U0602 | JW5357MSOTBWTR | SOT583/MSOT | 二級 Buck：Vin 2.7-6V, 3A, 1.5MHz | | FB 160k/10k → Vout 設 **4.0V**（提案書 P11 稱「3.8V_Module」實際 4V） |
| L0601 | 10µH 0.068Ω 4.5A | — | 一級儲能電感 | | 500kHz 配 10µH 合理 |
| L0602 | 4.7µH 0.4Ω 3.2A | — | 二級儲能電感 | | 1.5MHz 配 4.7µH 合理 |
| D0601 | PSBDAF40V3 | SMA/SOD | ORing / blocking diode → VDD_5V_MCU | | 40V 3A SBD |
| D0602 | PSBDAF40V3 | SMA/SOD | ORing / blocking diode → VDD_5V_PP | | 同上 |
| R0607 | 10K 1% | 0402 | U0601 FB 下臂 | | |
| R0606 | 57.6K 1% | 0402 | U0601 FB 上臂 | | 搭 10K 得 5.07V |
| R0614 | 10K 1% | 0402 | U0602 FB 下臂 | | |
| R0615 | 160K 1% | 0402 | U0602 FB 上臂 | | 搭 10K 得 4V（典型 JW5357 Vref 0.6V 的 ratio，需查 datasheet 確認） |
| TP0601 | — | — | PWR_IN test point | | 「預留磁珠調試」 |
| TP0602 | — | — | VDD_5V output test point | | 「預留磁珠調試」 |
| TP0603 | — | — | VDD_5V_MCU input to U0602 | | |
| TP0604 | — | — | VDD_4V output | | 「預留磁珠調試」 |
| C0603 | 22nF 50V | 0603 | Snubber（HS-FET SW 節點對 GND） | | 500kHz 高速節點加 snubber，EMC 保守做法 |

**Input decoupling**：VIN 側 4× 10nF（C0603/04/05/06）+ 3× 4.7µF（C0607/08/09）+ 100nF（C0610）—— 高/低頻 bypass 層疊，典型 60V input 設計。

---

## 3. 電源輸入 / 輸出

| Rail | 來源 | Vout | 電流（圖註） | 保護 | 消費 |
|---|---|:-:|:-:|---|---|
| PWR_IN_9-36V | Sheet 9 經 F0901 fuse + D0901 TVS | 9-36V | 2A | fuse + TVS | 本 sheet U0601 |
| VDD_5V | U0601 Buck | 5.07V | 3A（inductor 4.5A margin） | DCDC 內建 OCP/OTP/UVLO | ORing 到 _MCU/_PP |
| VDD_5V_MCU | VDD_5V ─ D0601 | ~4.8V | 3A | SBD blocking | U0602 input |
| VDD_5V_PP | VDD_5V ─ D0602 | ~4.8V | 1A | SBD blocking | Sheet 5 RS232_3V3 switch、Sheet 8 Charger VIN、Sheet 10 NFC_VDD、Sheet 11 ACC_INT_OUT |
| VDD_4V | U0602 Buck | 4.0V | 3A | DCDC 內建 | 進 Sheet 7 VDD_LDO_4V、Sheet 2 LTE_VBAT 開關、Sheet 12 GNSS_VBAT 開關 |

對照 [`90_power_tree.md`](90_power_tree.md)。

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 | 用途 |
|---|:-:|---|---|
| PWR_IN_9-36V | in | ← Sheet 9（D0901 後） | 車用 9-36V |
| VDD_5V | out | → 本 sheet 內 ORing | 一級輸出 |
| VDD_5V_MCU | out | → 本 sheet U0602 input；→ Sheet 8 Charger VIN（**not** _PP，見 §6 疑點）| |
| VDD_5V_PP | out | → Sheet 5、Sheet 8、Sheet 10、Sheet 11 | 1A 給外設 |
| VDD_4V | out | → Sheet 7（LDO 群）、Sheet 2（LTE_VBAT）、Sheet 12（GNSS_VBAT） | |

> **注意**：Sheet 8 charger (U0801 VIN) 標示由 VDD_5V 餵，SCH 走線實際是 `[6, 8] VDD_5V` → 待階段 B 確認是接到 ORing 前的 VDD_5V 還是 _PP 側。

---

## 5. 設計細節

- **兩級 DCDC 的理由**：9-36V 一次降到 4V 效率不如先降 5V 再降 4V；且 5V rail 本身要給 CAN/RS232/充電器用，必須單獨存在。
- **ORing 分路**（而非單純並聯）：D0601/D0602 把一條 5V 分成兩個 fault domain —— _PP 側外部負載 short 時 blocking diode 可以保護 MCU 側不被拉死。但代價是 Vf 損耗 ~0.3V @ 1A（PSBDAF40V3）。
- **「預留磁珠調試」**：TP0601/02/04 處預留磁珠位置但目前短路，EMC 測試時可改磁珠抑制共模雜訊。
- **Snubber C0603 22nF/50V**：直接並在 SW 節點對 GND，常見為 50mΩ 左右的 RC snubber，但本圖僅見 C（R 可能併在 PCB 拉線上或省略），**EMC 測試後可能需補 R**。
- **二級 DCDC Vout 4V 而非提案書的「3.8V_Module」**：JW5357 配 160k/10k FB → Vout=0.6×(160/10+1)=**10.2V ???** —— 這個算式**對不上**，實際 Vref 不是 0.6V 或 FB divider 讀錯，**待階段 B 查 JW5357 datasheet 的 Vref**。可能是 0.6V 內部參考 + 不同 ratio。若圖註標的 3A 下實際 Vout 真是 4V，那 Vref 應是 0.6V 搭 160k/10k 合不出，或 ratio 是 160/(160+某值)。**此處先打星號**，階段 B 必查。

---

## 6. 疑點 / Review Notes

- [ ] **Title block 錯誤**：SIZE A2 / DATE 2026-09-23 / SHEET **15 OF 15** —— 應為 SHEET 6 OF 15。圖框模板未改。
- [ ] **提案書 P25 寫 JW5293，SCH 實際 JW5357**：型號不一致。JW5357 與 JW5293 都是 JoulWatt synchronous buck 系列，但 Vin 範圍、Iout、package 不完全相同。**要向 Quectel 書面確認**哪個是正確料號，並對 datasheet 核 Vout 設定。
- [ ] **二級 Vout 計算對不上**：R0614=10K / R0615=160K 的 FB divider，若 Vref 0.6V → Vout=0.6×17=10.2V；若 Vref 0.235V → 4V。待 datasheet。
- [ ] **一級 ORing 選用 SBD（而非理想二極體 IC）**：Vf 損耗在 1A 時約 0.3W 散熱、2A 時更高；對 VDD_5V_PP 的 1A 標註尚可，但若 LTE 搜網 500mA 突波 + NFC 讀卡 200mA 同時，瞬態電流可能超 1A，**bulk cap 要夠**。
- [ ] **DNP 元件**：圖面未見明顯 NM 標註（除了 R0601 旁邊的 NM_SMA2W2CA） —— 階段 B 細讀所有 R06xx/C06xx 標註。
- [ ] **Vin feed-forward / loop compensation**：SGM61630 用 RT/SYNC pin（R0608 49.9K 設 fsw），compensation 由內部 Type-II 自動；JW5357 類似。這對 500mA load step 的穩定性 OK，但給 LTE 突波（500mA → 1A 快速）要看 bode plot，階段 B 補。
- [ ] **熱**：一級 3A × (36V-5V)/36V × (1-η) ≈ 3A × 86% × 15% loss = ~1.4W @ worst；SOIC-8 EP 熱阻 60°C/W → 85°C 升 ~85°C，裕量緊，**EVT 要做 85°C 滿載 thermal scan**。

---

## 7. 參考

- SCH PDF：Sheet 6
- 相關 sheet：Sheet 7（LDO 下游）、Sheet 8（charger 從 VDD_5V_PP 取電）、Sheet 9（PWR_IN 上游）
- 提案書章節：[03_P11 電源框圖](../../00_project/03_技術方案_系統設計.md)、[04_P24 SGM61630A](../../00_project/04_技術方案_關鍵零件.md)、[04_P25 JW5293（實際 JW5357）](../../00_project/04_技術方案_關鍵零件.md)
- Datasheet：SGM Micro SGM61630（Vref 0.75V 已驗證）、JoulWatt JW5357（Vref 0.6V 已驗證）

---

## 階段 B — Datasheet 核對、Vout 核算、熱 / 效率預算、Loop 分析

> 本節基於以下 datasheet 查核事實：
> - **SGM61630**：FB regulated to **0.75V**（SG Micro datasheet V ）；OVP 110%、SW on when FB<107% Vref；Iq 50µA；max Vin 60V
> - **JW5357**：FB regulated to **0.6V** typ (594-606mV @25°C, 588-612mV -40~125°C)；I² control；max Vin 18V；3A

### B.1 Vout 核算（一級 SGM61630）

**SCH 直接寫公式**：Sheet 6 圖面文字「Vout = (57.6/10+1) × 0.75 = 5.07V」。核對：
- R0606 = 57.6K ±1%（FB top）
- R0607 = 10K ±1%（FB bottom）
- Vref = 0.75V ✓
- **Vout = 0.75 × (57.6/10 + 1) = 0.75 × 6.76 = 5.07V ✓**

Tolerance 分析（最差 case）：
- R0606 56.98K ~ 58.22K
- R0607 9.9K ~ 10.1K
- Vref 0.735V ~ 0.765V（datasheet typ ±2%）
- Vout 最低：0.735 × (56.98/10.1 + 1) = **4.88V**
- Vout 最高：0.765 × (58.22/9.9 + 1) = **5.27V**
- **變動範圍 ±4%**，對下游 DCDC（JW5357 要求 Vin 4.5-18V）足夠裕量。

### B.2 ⚠️ Vout 核算（二級 JW5357）— **SCH 標示矛盾**

**SCH 寫**：R0614=10K、R0615=160K、Vout 設計目標 4V。

**計算**：Vout = Vref × (R_top/R_bot + 1) = 0.6 × (160/10 + 1) = **10.2V ❌**

**矛盾**：10.2V 不是設計意圖的 4V，而且 10.2V > JW5357 Vin max 18V 在實際運作下也不會真出 10.2V（受限於 input 5V），實際可能 SCH 標示錯誤。

**反推實際值**：Vout = 4V, Vref = 0.6V → R_top/R_bot = (4/0.6) − 1 = 5.667
- 若 R_bot=10K，R_top 應為 56.67K（標準值 56K 或 57.6K）
- 若 R_bot=1K，R_top 應為 5.667K

**最可能解釋**：R0615 實際值可能為 **56K 或 57.6K**（與一級 FB 相同），SCH 圖面標籤「160K」為繪製 / 字型渲染可能錯誤。本頁小字無法 100% 判讀。

**行動**：**Quectel 必須書面確認 R0615 實際料號值**。若真為 160K → Vout 錯誤 → EVT 板燒出來電壓錯誤 → 下游所有 LDO 失效 → 整板不開機。

### B.3 熱預算（Worst-case Thermal Budget）

**假設**：Vin_max = 36V（卡車 24V 系統 jump-start）、Vout = 5V、DCDC η = 85% 典型（SGM61630 datasheet @ Vin 24V Iout 1A ~88%）

| 情境 | Iout | Pout | η | Pin | Ploss | Tj rise* | 評估 |
|---|:-:|:-:|:-:|:-:|:-:|:-:|---|
| LTE idle only | 100mA | 0.5W | 85% | 0.59W | 0.09W | 5°C | OK |
| LTE active (continuous Tx max) | 1.5A | 7.5W | 88% | 8.52W | 1.02W | 60°C | **緊** |
| LTE 突波 500ms | 2A peak | 10W | 85% | 11.8W | 1.76W | 100°C | **transient OK**, continuous NG |
| 全系統滿載（LTE Tx + NFC read + CAN + charging） | 2.5A peak | 12.5W | 85% | 14.7W | 2.21W | 130°C | **超 Tj 125°C 上限** |

*熱阻假設：SOIC-8 Exposed Pad with proper PCB ground copper = **~60°C/W**（Datasheet typical）

**結論**：
- 持續 1.5A @ 36V 下 Tj 升 60°C，若 ambient = 85°C（提案書 operating temp 範圍）→ Tj = 145°C **超 Tj_max 125°C**
- 85°C ambient + LTE Tx 不應超過 1A 持續（更保守 0.8A）
- Charger (1A) + LTE active (500mA avg) + NFC (200mA) = 1.7A @ 36V → **邊緣風險**
- **EVT 必測**：85°C ambient、全系統滿載下 U0601 殼溫；若超 100°C 需加散熱銅箔面積或降 Vin（24V 系統基線）

### B.4 效率曲線預估

SGM61630 典型效率曲線（Datasheet）：
- Vin 12V, Vout 5V, Iout 0.5A → η ≈ 92%
- Vin 24V, Vout 5V, Iout 1A → η ≈ 88%
- Vin 36V, Vout 5V, Iout 1A → η ≈ 85%
- Vin 36V, Vout 5V, Iout 100mA → η ≈ 75%（light load）
- Vin 36V, Vout 5V, Iout 10mA → η ≈ 60%（very light load）

**light load 效率低影響**：待機 10mA 下 η 60% → Pin = (5×0.01)/0.6 = 83mW → 24V 系統取 3.5mA，36V 系統取 2.3mA。**平均功耗估算**（提案書 P14 標 Standby 18mW）這個 Pin 已遠超，所以提案書 P14 功耗表的「Standby 18mW」**可能只算下游 LDO 的 Iq，沒算 DCDC 的 light-load loss** → 待機功耗估算低估。

### B.5 Loop Stability（SGM61630 一級）

- **內部補償**：SGM61630 current-mode control，內部 Type-II compensation
- **外部補償**：C0650 33pF 並聯在 R0606（FB top R）→ 這是 feedforward cap，加零點（zero）補償 output cap 的極點
- Zero 位置：fz = 1/(2π × R0606 × C0650) = 1/(2π × 57.6k × 33p) ≈ **83.7 kHz**
- 500kHz switching → bandwidth 典型 ~50kHz，zero 83.7kHz 在 crossover 之後 → 幫助相位裕度 ✓

**Load step response 預估**：
- 100mA → 1A step（LTE search 開啟瞬間）
- Output cap 總量 Sheet 6 標 ~95µF（4.7µF ×4 + 100nF ×3 + 22µF ×1，約略估計）
- ΔV_sag ≈ ΔI × ESR + ΔI × tresponse / Cout
- 若 ceramics ESR 50mΩ 總並聯 ~15mΩ、tresponse ~20µs
- ΔV ≈ 0.9A × 15mΩ + 0.9A × 20µs / 95µF = 13.5mV + 189mV = **~200mV sag**
- 5V - 200mV = 4.8V → 仍 > JW5357 Vin min 4.5V ✓

### B.6 Snubber C0603

- SCH 標 22nF/50V，無 series R
- SW node ringing 典型 50-200MHz（寄生 L 1-10nH × C 10-50pF）
- 純 C 吸收能量：E = ½ × C × V² = ½ × 22n × 36² = **14.3µJ per cycle**
- 500kHz × 14.3µJ = **7.1W 損耗**（嚴重）！

**結論**：C0603 不可能是純 snubber，否則 7W 損耗燒 PCB。更可能是：
- (a) 標錯單位（應該是 **22pF** 不是 22nF）
- (b) 本不是 snubber 而是 BOOT cap（SGM61630 BOOT pin 需 100nF-1µF，但 BOOT cap 另見）
- (c) 是 EMI filter 到 GND

**行動**：階段 B 要用 EDA 匯出確認 C0603 實際 placement 與值。

### B.7 EN UVLO

- R0608 旁 R0605 構成 Vin→EN 分壓
- SGM61630 EN threshold typ 1.2V
- 設計目標：Vin < 某值時 EN=OFF（Vin UVLO）
- SCH 標 R0608=160K、R0609=49.9K；若為 EN 分壓，Vin_UVLO = 1.2 × (R_top/R_bot + 1)
  - 若 R_top=160K / R_bot=49.9K → UVLO = 1.2 × (160/49.9 + 1) = **5.05V**
  - 低於 5V 時 DCDC 關閉，防止進一步放電 → 車上 jump 失敗保護合理
- **R0605 和 R0608 的實際接線（哪個是 top / bottom）需 EDA 匯出確認**

### B.8 Input / Output Cap 容量驗證

**Input side**（高頻 bypass 到 bulk）：
- 標 C0603/04/05/06 10nF ×4 + C0607/08/09 4.7µF ×3 + C0610 100nF = **~14.2µF 總**
- 500kHz、ΔI_in RMS ≈ 1A × √(D(1-D)) ≈ 0.5A @ D=0.14（Vin 36V, Vout 5V）
- Cin ripple：ΔV = ΔI_rms / (2πfC) = 0.5 / (2π × 500k × 14.2µ) = 11.2mV ✓

**Output side**：
- 標 ~95µF 類似上述估計
- Output ripple：ΔV = ΔI_L / (8fC) 低 ESR 下約 1-10mV ✓

### B.9 疑點更新（原 §6 的升級）

| 原疑點 | Stage B 結論 |
|---|---|
| JW5357 Vout 計算對不上 | **Vref=0.6V 確認；R0615 若真為 160K → Vout=10.2V 錯誤；若為 56K/57.6K → 4V ✓**。SCH 標籤要 Quectel 書面確認 |
| 熱 1.4W @ worst, 85°C ambient | **確認**：85°C ambient + 1.5A 持續 → Tj 145°C 超 spec。必測 |
| SBD ORing Vf 損耗 | **確認**：PSBDAF40V3 @ 1A Vf 0.4V @85°C；0.4W per diode 散熱 |
| Snubber C0603 22nF | **新發現**：若真 22nF 會燒掉 7W；更可能是標錯或功能不同，必查 |
| EN compensation 行為 | SGM61630 internal Type-II + external feedforward cap 已補零點 ✓ |

---

Sources:
- [SGM61630 60V, 3A Buck Converter with 50μA IQ Datasheet (SG Micro)](https://www.sg-micro.com/rect/assets/2e4aa3c1-fd17-4b20-ac75-892cfe5f2e56/SGM61630.pdf)
- [JW5357/JW5357M 18V/3A Sync Step-Down Converter](http://www.bstelec.com/uploads/soft/240829/JW5357.pdf)
