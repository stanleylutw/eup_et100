# Sheet 7 — 4V→3V3 LDO ×3 + 4V→1V8 LDO

**PDF Page**：7 / 15
**Ref Designator Prefix**：07xx
**圖面標題**：4V 轉 3V3 電源 LDO；4V 轉 1V8 電源 LDO
**對應提案書章節**：[04_技術方案_關鍵零件.md §P26 WR0338 系列](../../00_project/04_技術方案_關鍵零件.md)

---

## 1. 功能定位

**下游所有數位電源 rail 的供應**。從 Sheet 6 的 VDD_4V（經 Sheet 8 電池 ORing 後叫 `VDD_LDO_4V`）分四條：
- **MCU_GNSS_3V3** → 常開，給 MCU + GNSS 模組
- **VDD_3V3** → 可切，給 G-Sensor、LED、RS232 EN pull-up
- **BUZZER_3V3** → 可切，給蜂鳴器
- **NFC_1V8** → 可切，給 NFC 數位介面與 1V8 域 I2C/UART

分 rail 的目的是「不用時可以關掉」—— enable 由 Sheet 11 的 AW9523 IO expander 控制。

---

## 2. 關鍵元件

| Ref | 型號 | 封裝 | Vout | Iout | EN 來源 | 用途 |
|---|---|---|:-:|:-:|---|---|
| U0701 | WR0338-33A50R | SOT23-5 | 3.3V | 300mA | **常開**（EN 經 R0701=1K 接 input 側） | MCU_GNSS_3V3 → Sheet 1 MCU 的 IO3V3_LDO、Sheet 12 GNSS 子板 |
| U0702 | WR0338-33A50R | SOT23-5 | 3.3V | 300mA | `VDD_3V3_EN`（Sheet 11 AW9523 P1_4） | VDD_3V3 → Sheet 3 G-Sensor、Sheet 4 LED、Sheet 5 RS232 EN pull-up |
| U0703 | WR0338-33A50R | SOT23-5 | 3.3V | 300mA | `BUZZER_3V3_EN`（Sheet 11 AW9523 P0_7） | BUZZER_3V3 → Sheet 4 蜂鳴器 |
| U0704 | WR0338-18A50R | SOT23-5 | 1.8V | 300mA | `NFC_1V8_EN`（Sheet 11 AW9523 P0_5） | NFC_1V8 → Sheet 10 PN7160 VDD_PAD、Sheet 1 MCU 1V8 域、Sheet 3 G-Sensor I2C pull-up |
| D0701 | PSBDAF40V3 | SMA | — | 1A | — | VDD_4V → VDD_LDO_4V 的 ORing 端 |
| D0702 | PSBDAF40V3 | SMA | — | 1A | — | VBAT → VDD_LDO_4V 的 ORing 端（電池接管路徑）|
| R0701/02/04/06 | 1K | 0402 | — | — | — | 各 LDO 的 EN pull-up（配 EN 線上 100K pull-down） |
| R0703/05/07 | 100K | 0402 | — | — | — | 各 EN pin 的 pull-down（確保 default off） |

**Decoupling pattern**（每顆 LDO 相同）：
- Input：22µF（低頻）+ 1µF（中頻）+ 100nF（高頻）× 3 顆並聯
- Output：22µF + 1µF + 100nF × 3

這個 input/output 都 3 顆並聯的疊 cap 作法對 WR0338 有點超規（datasheet 建議 1µF），**可能是為了應付瞬態 load step 預留**。

---

## 3. 電源輸入 / 輸出

| Rail | 來源 | Vout | Iout 上限 | Enable | 消費 |
|---|---|:-:|:-:|---|---|
| VDD_LDO_4V | Sheet 6 VDD_4V ∥ Sheet 8 VBAT（D0701/D0702 ORing） | 3.7-4V | 3A（受 Sheet 6 DCDC 限） | — | 本 sheet 4 顆 LDO + Sheet 2 LTE_VBAT 開關 + Sheet 12 GNSS_VBAT 開關 |
| MCU_GNSS_3V3 | U0701 | 3.3V | 300mA | 常開 | Sheet 1 MCU、Sheet 12 GNSS |
| VDD_3V3 | U0702 | 3.3V | 300mA | `VDD_3V3_EN` | Sheet 3/4/5 |
| BUZZER_3V3 | U0703 | 3.3V | 300mA | `BUZZER_3V3_EN` | Sheet 4 |
| NFC_1V8 | U0704 | 1.8V | 300mA | `NFC_1V8_EN` | Sheet 10 PN7160 (VDD_PAD only, 非主 VDD)、Sheet 1 MCU 1V8 域、Sheet 3 Sensor I2C 側 |

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 |
|---|:-:|---|
| VDD_LDO_4V | in | ← Sheet 6 D0701 + Sheet 8 D0702 ORing |
| MCU_GNSS_3V3 | out | → Sheet 1、Sheet 12 |
| VDD_3V3_EN | in | ← Sheet 11 (AW9523 P1_4) |
| VDD_3V3 | out | → Sheet 3、Sheet 4、Sheet 5 |
| BUZZER_3V3_EN | in | ← Sheet 11 (AW9523 P0_7) |
| BUZZER_3V3 | out | → Sheet 4 |
| NFC_1V8_EN | in | ← Sheet 11 (AW9523 P0_5) |
| NFC_1V8 | out | → Sheet 10、Sheet 1、Sheet 3 |

---

## 5. 設計細節

- **MCU_GNSS_3V3 常開**的理由：MCU 開機就要跑；GNSS 若用 EASY/EPO 軌道預測，RTC 與星曆要靠 VBAT 保持，但主 VDD 的開關由 `GNSS_LDO_EN` 控制在 Sheet 12（本 sheet MCU_GNSS_3V3 進 GNSS 前還有一顆 D1201 series diode + 另一條 EN）。
- **EN pull-down 100K**：確保 AW9523 reset / I2C 掛掉時所有可切 rail 預設 off。這對開機瞬間 power sequencing 友善，**但要注意 MCU 開機後必須先設好 AW9523 的 P1_4/P0_5/P0_7 output = 1 才會有 3V3/1V8 下游電**。開機初始化順序：
  1. VDD_LDO_4V 已有（硬體）
  2. MCU_GNSS_3V3 已有 → MCU 跑起來
  3. MCU 用 I2C 設 AW9523 → 開 VDD_3V3、BUZZER_3V3、NFC_1V8
- **四顆 LDO 都用 WR0338 系列**：3V3 版 `-33A50R`、1V8 版 `-18A50R`，PSRR 75dB @1kHz 對 LTE 突波的 ripple suppression 足夠。dropout 170mV @100mA，4V input - 3.3V output = 700mV 裕量充足；1V8 更不用擔心。
- **VDD_LDO_4V ORing**：D0701 來自 DCDC 二級、D0702 來自備援電池。正常時 VDD_4V ≈ 4V > VBAT ≈ 3.8V → D0702 反偏不供電；主電掉線 → VDD_4V 掉 → D0702 導通，電池供電。此處是「主電源掉線接管」的關鍵節點。
- **D0701/D0702 的 Vf 損耗**：PSBDAF40V3 大致 0.3V @ 1A，所以：
  - 正常：VDD_LDO_4V = 4V - 0.3V = 3.7V
  - 電池供電：VDD_LDO_4V = 3.8V - 0.3V = 3.5V
  - 3.5V 給 WR0338-33 做 3.3V → dropout 200mV，仍可（WR0338 dropout 170mV @100mA，所以可運作）
  - 若 3V3 rail 總電流大（例如 LED 全開 + RS232 全開 + G-Sensor active），dropout 會上升 → **邊緣案例**，階段 B 要算最壞 case

---

## 6. 疑點 / Review Notes

- [ ] **4 顆同款 LDO 的靜態電流累加**：WR0338 Iq typ 3µA，4 顆 = 12µA；但 **可切的 3 顆關掉時 shutdown current <1µA each**，低功耗模式下只剩 MCU_GNSS_3V3 的 3µA。
- [ ] **電池供電時 3V3 的裕量**：見 §5 最後一項，VDD_LDO_4V = 3.5V 時 WR0338-33 已貼近 dropout。**實測必測**：電池放電到 3.5V（放電末期）時 MCU_GNSS_3V3 是否還是 3.3V；若已掉到 3.2V，MCU 的 power-fail detector 可能誤觸發。
- [ ] **NFC_1V8 只給 PN7160 的 VDD_PAD（數位 I/O 參考）**：PN7160 的主 VDD 由 NFC_VDD（5V）供給 —— 這個 rail 分工是對的（PN7160 數位介面 1V8，TX/analog 5V），但要確認 VDD_PAD 的 current <300mA 預算；PN7160 datasheet 典型 <10mA，OK。
- [ ] **DNP 檢查**：圖面各 LDO 的輸入/輸出 cap 疊 3 顆，若量產階段有一顆標 NM 要注意；本 sheet 初看都 populated，待階段 B 細讀。
- [ ] **TP0701~0704**：每顆 LDO output 都有 TP，debug 與量產測試友善。
- [ ] **沒有 Power Good 回授 MCU**：WR0338 無 PG pin，MCU 無法直接知道 LDO 健康；只能用 ADC 偵測 VDD_3V3 的實際電壓（但本 SCH 看不出有這條 ADC 回授，階段 B 查）。

---

## 7. 參考

- SCH PDF：Sheet 7
- 相關 sheet：Sheet 6（VDD_4V 上游）、Sheet 8（VBAT ORing）、Sheet 11（EN 控制源）、Sheet 1/3/4/5/10/12（下游消費）
- 提案書章節：[04_P26 WR0338 系列](../../00_project/04_技術方案_關鍵零件.md)

---

## 階段 B — Dropout / PSRR / 電流預算 / EN Sequencing

### B.1 Dropout 分析（Battery EOL Scenario）

WR0338 系列 datasheet（提案書 P26 整理）：
- Dropout: **170mV @ Iout=100mA**
- Dropout 隨 Iout 增加而增加（典型 LDO 行為）；高電流時可能 350mV（@300mA）

**本板關鍵場景 — 電池放電末期**：

| 情境 | VDD_LDO_4V 實際電壓 | WR0338 @100mA dropout | WR0338-33 Vout 保持 ≥ 3.3V 的 Vin 下限 | 結論 |
|---|:-:|:-:|:-:|---|
| Normal (主電)       | 4.0V − 0.4V (D0701 Vf) = **3.6V** | 170mV | 3.47V | ✓ 大量 margin |
| Battery full (4.2V) | 4.2V − 0.4V (D0702 Vf) = **3.8V** | 170mV | 3.47V | ✓ |
| Battery nominal (3.8V) | 3.8V − 0.4V = **3.4V** | 170mV | 3.47V | **⚠️ 僅 70mV margin！** |
| Battery EOL (3.3V) | 3.3V − 0.4V = **2.9V** | — | — | **❌ Vin < Vout, dropout 進入** |
| Battery deep EOL (3.0V) | 3.0V − 0.4V = **2.6V** | — | — | 低於 MCU 建議供電下限；實際 MCU pin 還需扣 LDO 壓降，是否觸發 BOR 待測 |

**結論**：
- **主電模式**：Margin 充足
- **電池 nominal 以上**：Margin 緊但可用
- **電池放電末期**：LDO dropout 會降低 MCU rail，但 3.1 V 不是已知 BOR 門檻；實際電壓需依負載、diode Vf、LDO dropout 與溫度量測，不由電池電壓直接推定 reset 或剩餘電量。

> **2026-10-02 原廠規格校正**：[FR306x v0.4.9 電氣條件](../fr306x_reference/03_electrical_power_and_clock.md) p.26-27 列 MCU_VCC／BT_VCC 為 2.9-3.6 V、POWON_VTH 2.9 V、BOR_VTH 2.4 V，5 us < TR < 10 ms、TD > 20 ms。低於 2.9 V 即不在建議工作範圍，不等於必然 reset；也不能把 2.4 V 當作可正常運作下限。

**實際續航上限**：600mAh 電池可用容量 ~450mAh（扣掉 nominal 以下的 ~25%），而**非**提案書 P14 標的「600mAh × 70% = 420mAh」。差別不大，但說明 **ORing diode Vf 0.4V 吃掉的裕量比提案書算的多**。

**改善方向**（EVT 後若實測有問題）：
- 改 Shottky Vf 0.3V → 0.2V（LSM115AJ、SS14 等，耐壓足夠即可）
- 或改 ideal diode controller（LTC4411 等）→ Vf ~30mV，margin 大增，但 cost +

### B.2 PSRR vs DCDC Switching Ripple

WR0338 PSRR = 75dB @ 1kHz
- DCDC 二級 JW5357 switching @ 1.5MHz
- WR0338 PSRR 在 1MHz 典型降到 40-50dB（LDO 共性）

**計算**：JW5357 output ripple ~10mV @ 1.5MHz
- WR0338 1.5MHz PSRR 假設 45dB → attenuation 178x
- Output ripple on 3V3 rail = 10mV / 178 = **0.056mV** → ✓ 遠低於 MCU/GNSS/RFID 可容忍 ripple

**但是：light-load scenario**：
- 若 DCDC 進入 PFM 模式（<100mA 時），switching freq 下降到 10kHz-100kHz
- 100kHz 時 WR0338 PSRR ~65dB → attenuation 1778x
- Ripple 10mV / 1778 = 5.6µV ✓

無論 heavy/light load，PSRR 都足夠。**實際瓶頸是負載瞬態，不是 ripple**。

### B.3 LDO 電流預算（最差狀況）

**MCU_GNSS_3V3**（U0701，常開）：
| 消費 | 最大電流 |
|---|:-:|
| MCU (FR3068E-C) active 預算假設，非原廠 typical | 50mA |
| MCU peak (BT TX + flash write) 預算假設，非原廠 max | ~80mA |
| GNSS 子板 (AG3352Q active) | 72mA (提案書 P14) |
| 其他 pull-ups 總和 | ~5mA |
| **合計** | **~160mA peak** |

300mA LDO 下估算 160mA → **53% 使用率**，但不能套用僅 @100mA 的 170mV dropout 保證此負載可運作。上述 MCU 50/80mA 為保守預算假設，並非 v0.4.9 保證值；原廠的 MC1、BT 收發與 sleep 數值各有條件，不能直接相加為整機 worst case。須量測並更新電流、dropout 與熱預算。

**VDD_3V3**（U0702，可切）：
| 消費 | 最大電流 |
|---|:-:|
| G-Sensor (SC7U22 active) | 3mA |
| LED ×4 全開 | 25mA |
| RS232 EN pull-up ×3 | ~2mA |
| 其他 pull-up | ~5mA |
| **合計** | **~35mA** |

**低使用率** ✓

**BUZZER_3V3**（U0703，可切）：
| 消費 | 最大電流 |
|---|:-:|
| GHB530 buzzer active | ~50mA |
| **合計** | **~50mA** |

✓

**NFC_1V8**（U0704，可切）：
| 消費 | 最大電流 |
|---|:-:|
| PN7160 VDD_PAD (1V8 I/O) | ~5mA |
| G-Sensor 1V8 VDDIO | ~1mA |
| MCU 1V8 域 IO | ~5mA |
| SIM pull-up | ~1mA |
| **合計** | **~12mA** |

**超低使用率**，可考慮調降 LDO 容量以節省成本（但不是本案 blocker）。

### B.4 Enable Sequencing — AW9523 POR 行為更新

> **Datasheet 查核**：AW9523B POR 預設所有 16 pin = GPIO output、state 由 AD0/AD1 決定。本板 **AD0/AD1 都接 GND → 預設全部 LOW**。

**開機流程**：
1. **PWR_IN 加電（9-36V）** → DCDC 建立 VDD_5V → VDD_4V → 經 D0701 → VDD_LDO_4V
2. **U0701 MCU_GNSS_3V3 立即工作**（EN 直接拉高常開）→ **MCU 開機**
3. **AW9523 開機**（VCC = MCU_3V3）→ POR 預設所有 output = LOW
4. **MCU 把 EXP_RST 拉高（釋放 reset）**
5. **MCU I2C 寫 AW9523 register** → 開 VDD_3V3_EN / BUZZER_3V3_EN / NFC_1V8_EN / GNSS_LDO_EN
6. 下游 LDO 啟動（soft-start ~100µs 典型 WR0338）
7. 下游周邊初始化（PN7160、G-Sensor、GNSS 等）

**開機瞬間安全性**：
- 所有可切 rail OFF 到 MCU 寫 I2C 為止（~10-100ms 典型 Zephyr/RTOS 開機時間）
- 這段期間：PN7160、G-Sensor 全無電 → 無異常 I2C activity
- **設計意圖正確** ✓

**節能循環（Deep Sleep → Wake）**：
```text
Deep Sleep:
  MCU I2C 寫 AW9523 → VDD_3V3_EN=0, BUZZER_3V3_EN=0, NFC_1V8_EN=0, LTE_VBAT_EN=0
  → 僅 MCU_GNSS_3V3 + GNSS_VBAT(保星曆) 保留
  → 總功耗 ≈ MCU sleep (~5µA) + 4 顆 LDO Iq (~12µA) + GNSS 子板 standby (~0.3mA)
  → 約 **0.33mA @ 3.6V = 1.2mW** ✓

Wake-on-motion:
  G-Sensor_INT 拉低 → MCU wake → 讀 G-Sensor → 判斷為真實事件
  → I2C 開 LTE_VBAT_EN、VDD_3V3_EN
  → 等 LTE 開機、拿定位、回報
  → 回 sleep
```

**關鍵**：MCU_GNSS_3V3 常開但 GNSS chip 本身的 LDO 由 `GNSS_LDO_EN` 關閉 → MCU 用自己的 3V3，GNSS 子板的 chip 不耗電，只保 VBAT (RTC + 星曆)。

### B.5 疑點更新

| 原疑點 | Stage B 結論 |
|---|---|
| 電池供電時 3V3 的裕量 | **確認**：電池 3.6V 以下已進 dropout；MCU brownout 風險真實存在 |
| NFC_1V8 給 PN7160 VDD_PAD | **確認**：PN7160 datasheet VDD_PAD typ 10mA；WR0338 300mA 足夠 overshoot 10x+ |
| 沒有 Power Good 回授 MCU | **確認**：WR0338 無 PG；MCU 可用 ADC 讀 3V3 bus（但目前 SCH 無此 ADC trace） |
| 4 顆同款 LDO Iq 累加 | **確認**：WR0338 Iq 3µA ×4 = 12µA；深睡下微乎其微 |
| AW9523 POR default | **✅ 已查核**：預設全 LOW，安全開機行為 |

Sources:
- [AW9523B Datasheet V1.1.1 May 2016](https://cdn-shop.adafruit.com/product-files/4886/AW9523+English+Datasheet.pdf)
