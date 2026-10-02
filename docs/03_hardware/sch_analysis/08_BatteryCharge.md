# Sheet 8 — 備援電池充電 + 電池連接器

**PDF Page**：8 / 15
**Ref Designator Prefix**：08xx
**圖面標題**：外部 DC 充電至電池；電池連接器
**對應提案書章節**：[03_技術方案_系統設計.md §P14 Power Consumption Evaluation](../../00_project/03_技術方案_系統設計.md)、[02_客戶需求.md §P07 150mAh → V1.8 更新為 600mAh](../../00_project/02_客戶需求.md)

> **提案書 V1.8 已把備援電池從 150mAh 升到 600mAh**（HHS 402025 → 新型號待確認），但 [10_風險與談判清單.md §4](../../00_project/10_風險與談判清單.md) 標了「續航 4.2 小時」疑點 —— **功耗算式與 discharge efficiency 本身有誤**，與本 sheet 無直接關係，但本 sheet 的充電電流設定會影響實測續航。

---

## 1. 功能定位

**備援電池管理**。車上主電源掉線（熄火切電、保險絲斷、車禍）時：
1. 由本 sheet 的 Li-ion charger IC（U0801 YX4066HDN8AR，TP4066 相容線性充電器）維持電池充飽
2. 掉電瞬間，VBAT 經 [`Sheet 7 D0702`](07_PowerLDO_3V3_1V8.md) ORing diode 接管 VDD_LDO_4V bus
3. MCU + GNSS + LTE 繼續工作，用 LTE 回報「斷電告警」

MCU 可透過 `CHRG`、`FULL` 訊號知道充電狀態；用 `CHA_EN` 控制充電啟停；用 `VBAT_ADC_IN` 讀電池電壓；用 `VBAT_NTC`（**NM，目前未裝熱敏電阻**）保留量測電池溫度。

---

## 2. 關鍵元件

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| U0801 | YX4066HDN8AR | DFN-8 | Li-ion 線性充電器（TP4066 相容） | | 典型 500mA max 充電，可用 ISET 調 |
| J0801 | WF15003-01207 | 3-pin JST | 電池連接器 | | **pin 1/2/3 對應 B+/NTC/B- 未定**（圖面紅圈） |
| R0808 | 2K | 0402 | ISET → 充電電流設定 | | TP4066: Icharge = 1200V/R_ISET，2K → 600mA；需查 YX4066 datasheet 核算 |
| R0805 | 0R | 0402 | CHA_EN → charger EN pin | | 直通，由 MCU 控制 |
| R0806 | NM 100K | 0402 | VBAT_NTC pull-up | **NM** | 預留 NTC 熱敏電阻 case |
| D0803 | PESDxxV3B | SOD | VBAT 線上 TVS | | |
| R0807 | 100K 1% | 0402 | VBAT 分壓上臂 → ADC | | |
| R0809 | 100K 1% | 0402 | VBAT 分壓下臂 → ADC | | 1:1 分壓 → VBAT_ADC_IN = VBAT/2，MCU ADC 讀後 ×2 得實際電池電壓 |
| R0803 | 0R 1/10W | 0402 | VBAT 到連接器 | | |
| R0810/11/12 | 4.7K | 0402 | MCU_3V3 pull-up on CHA_EN / CHA_CHRG / CHA_FULL | | open-drain output 的 pull-up |
| R0804 | 4.99R | 0402 | VIN decoupling（U0801 input） | | 1A 下壓降 5mV，功率 5mW，忽略 |

---

## 3. 電源輸入 / 輸出

| Rail | 來源 | V | I 上限 | 用途 |
|---|---|:-:|:-:|---|
| VDD_5V_MCU（或 _PP，待確認） | ← Sheet 6 | 5V | 1A | U0801 VIN 充電器輸入 |
| VBAT | U0801 BAT output ↔ J0801 pin（待定） | 3.0-4.2V | 1A | 給 Sheet 7 D0702 ORing、本 sheet 分壓偵測 |

> **圖面標示**：U0801 VIN 標 `VDD_5V`（Sheet 6 的 ORing 前原節點）。SCH 的交叉引用 `[6, 8]` 代表這條 net 存在於 Sheet 6 與 8，但 Sheet 6 的 ORing 把 VDD_5V 切成 _MCU / _PP 兩路，此處要走 trace 才能確認接到哪一側（或甚至接 ORing 前原節點）。**階段 B 要用 netlist 確認**。

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 | 用途 |
|---|:-:|---|---|
| VDD_5V | in | ← Sheet 6 | 充電器 input |
| VBAT | in/out | ↔ J0801 battery connector；→ Sheet 7 D0702；→ 本 sheet 分壓 | |
| CHA_EN | in | ← Sheet 11 MCU_CHA_EN | 由 MCU 控制充電 ON/OFF |
| CHA_CHRG | out | → Sheet 1 MCU PA10 | 充電中指示（open-drain，充電時拉低） |
| CHA_FULL | out | → Sheet 1 MCU PA11 | 充飽指示（open-drain） |
| VBAT_ADC_IN | out | → Sheet 1 MCU PP6（VBAT_ADC_IN） | ADC 讀電池電壓 |
| VBAT_NTC | out | → Sheet 1 MCU PP7 | 電池溫度（**NM**，目前讀到 100K pull-up 的空值） |

---

## 5. 設計細節

- **線性充電器 vs. switching**：600mAh 電池、500mA 充電 → VIN - VBAT 壓差最差 5V-3V=2V → P_dissipation = 500mA × 2V = 1W，DFN-8 package 熱阻 ~45°C/W → 85°C 升 45°C → 殼內 85°C。**充電期間在高溫（85°C 車內）會觸發 charger 內部 OTP 降流**，這是 feature 不是 bug，但要在韌體層面接受「充電慢」。
- **ISET=2K**（典型值）：若 YX4066 用 Icharge = K/R_ISET，K 待 datasheet 查；若 K=1200V 則 Icharge=600mA（1C 給 600mAh）。1C 充電對 Li-ion 安全上限內。
- **CHRG/FULL 兩線式充電指示**：TP4066 慣例，CHRG 充電中拉低、FULL 充飽拉低。MCU 可同時讀以區分「充電中 / 已充飽 / 無電池 / 故障」四態。
- **VBAT 分壓 100K/100K**：等效源阻抗 50K；FR306x v0.4.9 只列 12-bit SAR ADC／通道數，未提供 input range、reference、sampling time 或允許源阻抗，不能宣稱原廠要求 <10K。需取得 ADC analog specs 並驗證 settling；平均多個 sample 不保證消除 acquisition 誤差。見 [待確認規格](../fr306x_reference/06_gaps_and_revision_history.md)。
- **NM VBAT_NTC**：若要支援提案書 §P07 的「電池 UN38.3 認證、1.2M 落摔」，熱敏電阻會是 BMS 必備以符合安規，**量產前應該要補上**。目前 NM 是為樣品省料。

---

## 6. 疑點 / Review Notes

- [ ] **🔴 CRITICAL: 電池連接器 pin 序未定**（圖面紅圈「电池 pin 序？」）：J0801 WF15003-01207 的 pin 1/2/3 誰是 B+、GND、NTC 尚未決定。接錯會直接燒電池或炸 BMS。**EVT 樣品之前必須與電池廠確認**。
- [ ] **🟠 VBAT_NTC NM**：目前沒裝熱敏電阻 → MCU 讀到 100K pull-up 的空值。若量產保留 NM，等於沒有電池溫度保護，**UN38.3 認證可能卡關**。
- [ ] **🟠 充電電流 ISET=2K 但 YX4066 的 K 常數未知**：若 K 不是 1200V，實際充電電流與 600mA 預期不同。階段 B 查 datasheet。
- [ ] **🟠 Charger VIN 來源不明**：圖面 `VDD_5V` 可能是 ORing 前的 VDD_5V 原節點、或 _MCU、或 _PP。接到 _PP 的話，充電器與其他 _PP 負載（NFC/CAN/RS232）共享 1A 預算 → 充電 + NFC 讀卡時可能 current limit。接到 _MCU 的話對 _MCU 側二級 DCDC 的 input 挑戰較大。**要在 netlist 層面確認**。
- [ ] **📘 高溫車內充電行為**：85°C 車內充電線性充電器會降流，實際有效充電時間可能遠低於計算值。
- [ ] **📘 無電池偵測**：如何判斷電池不在？YX4066 內部有 BAT disconnect detection，但訊號是否接到 MCU？若無，MCU 只能用「VBAT_ADC 讀到接近 VIN」間接推斷。
- [ ] **📘 掉電瞬間反灌**：主電源切斷瞬間，VDD_5V 側 bulk cap 會透過充電器反灌回 VBAT；YX4066 的 reverse blocking 行為 datasheet 要確認。

---

## 7. 參考

- SCH PDF：Sheet 8
- 相關 sheet：Sheet 6（VDD_5V 來源）、Sheet 7（VBAT ORing 下游）、Sheet 1（MCU 讀 ADC / 控制 CHA_EN / 讀 CHRG/FULL）、Sheet 11（CHA_EN 轉由 IO expander 鏡像控制）
- 提案書章節：[03_P14 功耗評估（4.2 小時續航爭議）](../../00_project/03_技術方案_系統設計.md)、[02_P07 原 150mAh](../../00_project/02_客戶需求.md)、[10_風險與談判清單.md §4](../../00_project/10_風險與談判清單.md)
- Datasheet：YX4066HDN8AR（階段 B 找正式 datasheet；TP4066 系列原廠為 TPower / NanJing TopPower）
