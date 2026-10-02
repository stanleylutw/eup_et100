# Sheet 2 — LTE Module（EG800Q-EU）+ USIM 介面 + Debug/Reset/Boot/USB TP

**PDF Page**：2 / 15
**Ref Designator Prefix**：02xx
**圖面標題**：LTE_DBG；LTE_RESET；LTE_BOOT；LTE_USB
**對應提案書章節**：[04_技術方案_關鍵零件.md §P20 EG800Q-EU](../../00_project/04_技術方案_關鍵零件.md)、[10_風險與談判清單.md §1 LTE 模組區域覆蓋不足](../../00_project/10_風險與談判清單.md)

> **🔴 P0 風險**：EG800Q-EU 的 band（B1/3/5/7/8/20/28）**不包含東南亞市場主流的 TDD B40**（泰國/馬來西亞/印尼），且無 2G/3G fallback。此風險在 Sheet 13 看到 MXD8544AE antenna tuner 後有新變數：**tuner 可切 4 條 RF path**，理論上可針對不同 band 做天線阻抗 retune，但 **module baseband 本身不支援 B40 → tuner 幫不了**。詳見 [99_risks_vs_proposal_v1.8.md](99_risks_vs_proposal_v1.8.md)。

---

## 1. 功能定位

**4G LTE Cat-1 bis 聯網主模組**。負責：
- 蜂巢資料回傳（HTTPS / MQTT / TCP）
- SIM 卡 I/F
- GPS 輔助定位（AGPS，雖主要 GNSS 在 Sheet 12）
- OTA download（經 LTE 下載韌體）
- USB debug port（量產測試 / 工廠燒 SIM 時用）

EG800Q-EU chipset = QCX216（Qualcomm QCX216，前身 MDM9205 相關）。

---

## 2. 關鍵元件

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| U0201 (A/B/C/D) | **EG800Q-EU** | 108-pin LCC/LGA 17.7×15.8×2.4mm | LTE Cat-1 bis module | | 分成 4 個 SCH symbol block（A/B/C/D）畫便於 pin 分組；實際是**一顆** |
| Q0232 | 2SC4617 | SOT-23 | LTE_VBAT 開關 BJT | | 由 MCU `LTE_VBAT_EN` 控 base，拉高開 VBAT 給 LTE |
| L0231 | YJL2305A（or similar P-MOSFET） | SOT-23 | 可能是給 LTE_VBAT 的 soft-start PMOS | | 需 EDA 匯出確認（圖面不清） |
| C0231 / C0232 | 100nF | 0402 | VDD_LDO_4V 入 LTE_VBAT 的 decoupling | | 在 PMOS 兩端各一顆 |
| R0202 | 4.7K | 0402 | PWRKEY pull-up | | EG800Q 開機信號；低平脈衝啟動 |
| C0211 | 100nF | 0402 | STATUS pin bypass | | |
| D0211 | PESD... | SOD-523 | STATUS pin ESD | | |
| D0201 | PESDxC2V0X8MF | — | VBAT 線上 TVS | | |
| D0204/05/06 | NM_PESD... | SOD-523 | **NM** RESET_N / NET_STATUS / STATUS TVS | **NM** | 預留 ESD 位 |
| D0207 | PESD... | SOD-523 | USB_BOOT TVS | | |
| D0208 | PESD... | SOD-523 | USB_VBUS TVS | | |
| D0209/10 | PESD... | SOD-523 | USB_DP/DM TVS | | |
| D0206 | TVS | — | USB_VBUS clamp | | 16V |
| D0203/04 | PESNK402-03 | — | DBG_TXD/RXD TVS | | 1V8 debug UART ESD |
| R0231/32 | 100K / 0R | 0402 | Q0232 base/collector 偏壓 | | |

**108-pin module 的訊號分組**（SCH symbol 分 A/B/C/D 四 part 畫）：
- **A part**（左上）：GND、RESERVED×多、ADC0/ADC1、USIM、UART、PCM、AUX_RXD/TXD、STATUS、VDD_EXT、MAIN_RTS、SIM_DET、NET_STATUS、RESET_N、PWRKEY
- **B part**（左下）：USB_VBUS、USB_DM、USB_DP、SDR_GRFC_1/2（給 Sheet 13 antenna tuner 控制）、I2C_SCL/SDA（EG800Q 自己的 sensor hub）、GRFC2、更多 RESERVED
- **C part**（中）：PSM_INT、PSM_IND、USIM_DET、USB_BOOT、更多 RESERVED
- **D part**（右）：全是 GND pins（88-93）

### 關鍵 pin 功能

| Pin | Net | 用途 |
|:-:|---|---|
| 7 | PWRKEY | 開機信號（低平脈衝 >500ms） |
| 14 | RESET_N | 強制 reset（低平脈衝） |
| 15 | NET_STATUS | 網路狀態 LED/訊號 → MCU |
| 16 | MAIN_RXD（UART） | ← MCU TX |
| 17 | MAIN_TXD（UART） | → MCU RX |
| 24 | VDD_EXT | 1.8V output 給外部（SIM VCC source / UART level shift reference） |
| 25 | STATUS | 開機 / 關機狀態 → MCU |
| 32 | PCM_CLK / 33 PCM_SYNC / 34 PCM_DIN / 35 PCM_DOUT | PCM audio（不用，RESERVED） |
| 36 | ANT_MAIN | LTE 主天線（50Ω → Sheet 13） |
| 59 USB_DP / 60 USB_DM / 61 USB_VBUS | USB 2.0 FS debug port | |
| 65 I2C_SDA / 66 I2C_SCL | 給 LTE 自己 host sensor（本板未用） | |
| 67 GRFC2 / 104 GRFC_C_1（名字為 SDR_GRFC_2 / _1 到 Sheet 13）| 給外部 antenna tuner 的控制訊號 | |
| 78 USIM_DET | SIM 插拔偵測 | |

---

## 3. 電源輸入

| Rail | 來源 | V | I | 備註 |
|---|---|:-:|:-:|---|
| LTE_VBAT | ← Sheet 7 VDD_LDO_4V 經 Q0232 BJT | 3.5-4V | 1.5A | **主電源** |
| USIM_VCC | ← Sheet 2 pin 11 USIM_VDD（1.8V/3V class B，由 LTE 內部切換） | 1.8/3V | 50mA | → Sheet 3 SIM 卡 |
| VDD_EXT | ← pin 24 LTE 內部 LDO 輸出 | 1.8V | 50mA | → LTE_EXT_1V8 給 Sheet 11 level shift、Sheet 13 tuner |

> **LTE_VBAT 由 BJT 切電源的問題**：2SC4617 Vce(sat) 典型 0.2V @ 300mA，若 LTE 突波 1.5A 時 Vce(sat) 升到 0.5V，LTE_VBAT 從 4V 掉到 3.5V；EG800Q-EU spec 要求 VBAT 3.4-4.2V，仍在範圍但很緊。**改用低 Rdson PMOS 會更穩**，但目前設計用 BJT 可能是為了成本。

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 | 用途 |
|---|:-:|---|---|
| LTE_VBAT_EN | in | ← Sheet 11 MCU GPIO | LTE 電源開關控制 |
| MCU_LTE_TXD_3V3 → UART_TXD_1V8 | in | ← Sheet 11 level shift | UART TX（經 1V8↔3V3 shifter） |
| MCU_LTE_RXD_3V3 ← UART_RXD_1V8 | out | → Sheet 11 level shift | UART RX |
| PWRKEY | in | ← Sheet 11（可能經 GPIO） | 開機 pulse |
| LTE_RESET_N | in | ← Sheet 11 MCU_LTE_RESET 經 R0201 series | reset |
| LTE_STATUS | out | → Sheet 1 MCU | 開機狀態 |
| LTE_NET_STATUS | out | → Sheet 1 MCU | 網路狀態 |
| LTE_WAKE_MCU | out | → Sheet 11 level shift → Sheet 1 LTE_WAKE_MCU_3V3 | PSM 喚醒 MCU |
| MCU_WAKE_LTE | in | ← Sheet 11 level shift ← Sheet 1 | MCU 喚醒 LTE |
| USIM_DATA/CLK/RST | — | ↔ Sheet 3 SIM 卡 | SIM 介面（1.8V/3V） |
| SIM_DET | in | ← Sheet 3 SIM 插拔偵測 | |
| LTE_USB_BOOT | in | ← test point TP0208/09 | 工廠量產經 USB 燒 firmware 時使用 |
| USB_VBUS/DP/DM | ↔ | TP0210/11/12/13 | USB debug port |
| DBG_TXD_1V8 / DBG_RXD_1V8 | ↔ | TP0201/02/03 | LTE AT command debug UART（1V8 level，不是 USB） |
| 50ohm_MAIN_ANT | out | → Sheet 13 LTE 天線 matching | 主射頻 |
| SDR_GRFC_1 / SDR_GRFC_2 | out | → Sheet 13 U1301 MXD8544AE tuner control | **antenna tuner band select 兩個控制線** |

---

## 5. 設計細節

- **module 分 4 symbol 畫**（A/B/C/D）：是 EDA 軟體（Allegro / Altium）對大 pin count package 的常見拆法，不是獨立器件。pin 1-44 在 A、45-66 在 B、73-86 在 C、89-109 在 D。
- **1V8 UART**：LTE 的 UART 是 1.8V level，MCU 是 3.3V level（在 3V3 域 pins）→ 需要 level shifter。本板的 shift 不用 IC，而是 Sheet 11 的 2SC4617 BJT 單端 shifter（三支：MCU_LTE_TXD、MCU_LTE_RXD、LTE_WAKE_MCU、MCU_WAKE_LTE）。
- **USB 2.0 FS debug port**：工廠用 USB 可燒 LTE 韌體（`USB_BOOT` + `RESET_N` 配合進入 download mode）；USB VBUS 5V 由 test point 供給。**量產出廠後 USB 不接**（殼內 TP）。
- **SIM 熱插拔支援**：V1.8 客戶需求紅字強調「翻蓋式固定、nano-sim、熱插拔」，SIM_DET pin 連 Sheet 3 SIM holder 的機械開關 → MCU 讀到 SIM 插拔就可以重新初始化 USIM 介面。
- **antenna tuner 控制**：`SDR_GRFC_1/2` 兩線 → 4 種組合，對應 Sheet 13 U1301 MXD8544AE 的 RF1/RF2/RF3/RF4 四條 band path。這是**提案書沒提的加碼硬體**。

---

## 6. 疑點 / Review Notes

- [ ] **🔴 CRITICAL (業務面)**：EG800Q-EU band 不含 TDD B40，SE 亞三國（TH/MY/ID）主要 LTE band 覆蓋不足；見 [10_風險與談判清單.md §1](../../00_project/10_風險與談判清單.md)。
- [ ] **antenna tuner (Sheet 13 U1301) 的角色**：此 tuner 是**阻抗匹配可切，不是 band 擴展**。tuner 只能幫天線在不同頻段上有較佳 return loss，但 module baseband 不支援的 band（例如 B40），tuner 也沒用。**不要用 tuner 的存在來緩解 band 問題**。
- [ ] **LTE_VBAT 用 BJT 開關**：Vce(sat) 損耗問題，突波時可能跌破 LTE 下限；若 EVT 看到 reset / brownout，必須改 PMOS。
- [ ] **PWRKEY sequence**：datasheet 典型要求 PWRKEY 低電壓 >500ms 才開機；MCU GPIO 要能撐 500ms 的拉低時間。
- [ ] **USIM 熱插拔**：SIM holder 若是翻蓋式，MCU 用 SIM_DET 偵測後要有軟體 debounce；物理上 SIM pins 的 ESD 已有 Sheet 3 TVS 保護，但**金手指磨損** 經多次插拔會是機械壽命問題。
- [ ] **DNP**：D0204/05/06（RESET_N / NET_STATUS / STATUS 的 TVS）全 NM —— 省料。若 EVT 看到這些線上 ESD 損壞再補。
- [ ] **USB bootloader 保護**：量產出廠後若有人開殼接 USB 可能可以重刷 LTE 韌體；是否要在 LTE 側禁用 USB bootloader（Quectel 可能有 fuse 選項），**要與 Quectel 確認**。
- [ ] **DBG_TXD/RXD 1V8**：工廠拿 UART-USB bridge 要支援 1.8V level（CP2102 / FT232 的 3V3 版不行），需用 1V8 版本的 bridge；量產測試治具要配。
- [ ] **GRFC2 和 SDR_GRFC_1 命名混亂**：Pin 104 = GRFC2，到 Sheet 13 時叫 `SDR_GRFC_2`；Pin 67（實際標 SDR_GRFC_1）在 B part 畫，標籤跳行容易看錯。
- [ ] **Reserved pins 太多**：EG800Q-EU 的 108 pin 中有 >40 支 RESERVED。這些 pin 大多內部短接 GND 或懸空，SCH 已遵循 Quectel reference design 全部標 RESERVED 不接，階段 B 要與 Quectel reference design 文件 pin-by-pin 核對。

---

## 7. 參考

- SCH PDF：Sheet 2
- 相關 sheet：Sheet 3（SIM）、Sheet 11（level shift / enable）、Sheet 13（RF ANT + tuner）、Sheet 7（LTE_VBAT 經 D0702 ORing 進 VDD_LDO_4V）
- 提案書章節：[04_P20 EG800Q-EU](../../00_project/04_技術方案_關鍵零件.md)、[10_P1 LTE 選型風險](../../00_project/10_風險與談判清單.md)
- Datasheet：Quectel EG800Q series HW Design V1.0/V1.1、QCX216 chipset

---

## 階段 B — VBAT 餘裕核算、PWRKEY 時序、Antenna Tuner band 分配

### B.1 VBAT 餘裕核算 — **🔴 嚴重問題確認**

**Datasheet 事實**：
- EG800Q-EU VBAT 範圍 **3.3V ~ 4.3V**（min 3.3V，不是 Stage A 寫的 3.4V）
- Nominal 3.8V

**本板路徑**：VDD_LDO_4V → Q0232 2SC4617 BJT → LTE_VBAT

**Q0232 2SC4617 Vce(sat) 查核**（Toshiba 2SC4617 datasheet）：
- Vce(sat) @ Ic=100mA: **0.15V** typ
- Vce(sat) @ Ic=500mA: **0.3V** typ
- Vce(sat) @ Ic=1A: **0.6V** typ
- Vce(sat) @ Ic=1.5A: **≥0.8V** 典型（接近飽和區邊緣，不保證線性）

**各情境 LTE_VBAT 實際電壓**：

| 情境 | VDD_LDO_4V | LTE Ic | Q0232 Vce(sat) | LTE_VBAT | EG800Q 下限 3.3V | 結論 |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| 主電 + LTE idle | 3.6V | 50mA | 0.1V | 3.5V | ✓ | OK |
| 主電 + LTE 註網搜網 | 3.6V | 500mA | 0.3V | 3.3V | ✓ (剛好) | **緊** |
| 主電 + LTE Tx burst (CAT1 full power) | 3.6V | 1A | 0.6V | **3.0V** | ❌ | **低於下限** |
| 主電 + LTE Tx peak | 3.6V | 1.5A | 0.8V | **2.8V** | ❌ | **嚴重 undervoltage** |
| 電池 + LTE idle | 3.4V | 50mA | 0.1V | 3.3V | ✓ (剛好) | 緊 |
| 電池 + LTE Tx | 3.4V | 1A | 0.6V | **2.8V** | ❌ | **嚴重** |

**結論**：**Q0232 2SC4617 BJT 做 LTE_VBAT 開關不夠**。LTE Tx 瞬態時 VBAT 跌破 3.3V 下限 → EG800Q-EU 可能自動 reset / brownout / detach from network。

**可能緩解**：
- Base 驅動加強（Ib 大 → β 增 → Vce 降），但 2SC4617 max Ic=1.5A 已經是上限
- **真正解法**：改用 PMOS（低 Rdson < 100mΩ → @ 1.5A Vds 僅 150mV → LTE_VBAT = 3.4V ✓）
  - 建議料號：YJL2305A（Sheet 5/12 已在用）、AO3401、SI2301 等
  - **BOM cost 增加很小**（PMOS 0.5-1 分 vs BJT 0.3 分）
- 或 PMOS + Charge pump 做 soft-start，防 startup inrush

### B.2 PWRKEY 時序

**EG800Q-EU HW Design 要求**：
- Turn-ON pulse: **≥500ms** low pulse on PWRKEY
- Turn-OFF pulse: **≥650ms** low
- VBAT must be stable **≥30ms before** driving PWRKEY low
- Recommended driver: **open-collector**

**本板實作**（Sheet 2）：
- R0202 4.7K pull-up to VDD_LDO_4V (或 LTE_VBAT？待查)
- PWRKEY 由 MCU GPIO 直接 drive？或經 BJT open-collector？—— 從 Sheet 2 畫面看不到直接 PWRKEY 的 BJT，可能是 MCU GPIO 推挽直接 drive（**不符合 datasheet 推薦**）
- MCU GPIO VIL 一般 0.3×VDD = 1V；EG800Q PWRKEY VIL_max 0.5V → **MCU 直 drive 可能無法把 PWRKEY 拉夠低**

**行動**：
- EDA 匯出確認 PWRKEY 走線（有無 BJT 中介）
- 若 MCU 直 drive，需驗 EVT 板實際 LTE 能否開機
- 若 fail，須加 BJT open-collector driver（成本 1 顆 BJT + 1 電阻）

### B.3 Antenna Tuner MXD8544AE Band 分配

**Datasheet 查核受限**（Maxscend 文件取得困難），但可從功能類別推論：
- MXD8544AE 是 **SP4T RF switch / aperture tuner**，4 input RF path 可切；
- 由 2 條控制線（本板 SDR_GRFC_1、SDR_GRFC_2）解碼成 4 state

**4 state 典型應用**（LTE 全頻段 EU 版 B1/3/5/7/8/20/28）：
| State (G2 G1) | 可能的 band 分組 | 原因 |
|:-:|---|---|
| 00 | B20 (791-862 MHz 低頻 digital dividend) | 低頻需要大電感 matching |
| 01 | B8 (880-960 MHz) + B5 (824-894 MHz) | 中低頻 |
| 10 | B1 (1920-2170 MHz) + B3 (1710-1880 MHz) | 中頻 |
| 11 | B7 (2500-2690 MHz) | 高頻 |

> **這只是推論**；實際分配要 Quectel 提供 reference design 文件。

**對 band 覆蓋的意義**：tuner 無法擴展支援的 band，但可**改善各 band 的 TRP/TIS**。對提案書 P33 寫的「600-960MHz 效率 20-40%」，若 tuner 能把低頻 return loss 從 -5dB 改善到 -10dB，TRP 可提升 2-3dB → 弱訊號區收發距離增加約 25-40%。

### B.4 SIM 熱插拔 Debounce 需求

**SIM_DET 行為**：
- 翻蓋機械開關：合上時 → DETECTION_SWITCH 短路到 GND → SIM_DET = 低
- EG800Q-EU USIM_DET pin：datasheet 預設 **high=卡在**（logic 可透過 AT 命令反向）
- 本板 R0302 51K pull-up 到 VDD_3V3 → 翻蓋打開時 SIM_DET = 高 → EG800Q 判為「卡拔出」

**韌體 debounce 要求**：
- 機械開關 bounce 典型 10-50ms
- 若無 debounce，一次插拔可能觸發多次 insert/remove event
- EG800Q 的 USIM detect 內部有 debounce（典型 50-100ms），但本板的 SIM_DET net 也接到 MCU 嗎？從 Sheet 2 看沒有直接到 MCU 的 trace → **SIM 熱插拔完全由 LTE 模組處理，MCU 不介入**
- LTE 處理機制：發送 URC `+CPIN: NOT READY` → 等插入後 `+CPIN: READY`

### B.5 USB Debug Port 使用模式

**三種進入 USB mode 的方式**：
1. **正常模式**：USB 可用於 AT command、diag log（出廠後保留給警方/服務人員，見提案書 UART4 註記）
2. **Download mode**：
   - 條件：`LTE_USB_BOOT` pin 拉高 + `LTE_RESET_N` reset
   - 用途：工廠量產燒 LTE 韌體
3. **Fault recovery**：若 flash corrupt，可以 force USB download 救援

**本板 TP0208/09 配置**（Sheet 2）：
- TP0208 = LTE_USB_BOOT TP
- TP0209 = 另一 TP
- **沒看到 pull-down to GND default** → 浮空可能會被 LTE 內部 pull-up 拉高（進 download mode）→ **開機就卡 download mode** 風險

**待確認**：TP0208 若無 R 接地，EVT 板可能開機狀態異常。

### B.6 1V8 UART 的 Reserved pin 檢查

EG800Q-EU 的 108 pin 中 >40 支 RESERVED。對照 Quectel reference design 關鍵檢查：
- RESERVED6 (pin 8), RESERVED7 (pin 27), RESERVED45 (pin 86)：**要確認是否要 NC**，部分 reference design 要求 pull-down
- GND pins 1, 10, 14 等：全部接 GND ✓
- 本 SCH 全部標 `RESERVED` 懸空 → 符合 reference design 要求（Quectel reference 允許全 NC）

**行動**：階段 B 用 Quectel reference design PDF 做 pin-by-pin 完整比對（本次不展開，EVT 前補做）。

### B.7 疑點更新

| 原疑點 | Stage B 結論 |
|---|---|
| LTE_VBAT 用 BJT 開關 | **🔴 確認嚴重**：Vce(sat) 導致 Tx burst 時 VBAT 跌到 2.8V, 低於 3.3V 下限 1 整個 Volt。必須改 PMOS |
| PWRKEY sequence | 需 ≥500ms 低平、open-collector drive；若 MCU GPIO 直 drive 可能 VIL marginal |
| LTE_EXT_1V8 給 shift / tuner / SIM | ✓ |
| Antenna tuner band 分配 | **待 Quectel 提供**；推測 00→B20、01→B5/8、10→B1/3、11→B7 |

Sources:
- [Quectel EG800Q-EU Hardware Design V1.0](https://quectel.com/content/uploads/2024/02/Quectel_EG800Q-EU_Hardware_Design_V1.0.pdf)
- [Quectel EG800Q Series Hardware Design V1.1](https://quectel.com/content/uploads/2024/04/Quectel_EG800Q_Series_Hardware_Design_V1.1.pdf)
