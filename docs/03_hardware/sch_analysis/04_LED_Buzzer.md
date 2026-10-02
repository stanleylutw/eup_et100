# Sheet 4 — 4× LED 指示燈 + 蜂鳴器

**PDF Page**：4 / 15
**Ref Designator Prefix**：04xx
**圖面標題**：指示燈 ×4；蜂鳴器
**對應提案書章節**：[02_客戶需求.md §P05 燈號 + 蜂鳴器](../../00_project/02_客戶需求.md)、[03_技術方案_系統設計.md §P13 LED Status](../../00_project/03_技術方案_系統設計.md)

---

## 1. 功能定位

**使用者可見的狀態輸出**：
- **4 顆 LED**：Driver（黃）、GPS（紅）、Net（藍）、Power/Memory（綠）—— 提案書 P13 定義恆亮/熄滅/閃爍四態
- **1 顆蜂鳴器**：事件告警（闖入、拔卡、斷電、OTA 完成等）

LED 全部用 DTC1143ZE 預偏置數位三極體（內建 base-emitter resistor），MCU GPIO 直接驅動；蜂鳴器用一顆 2SC4617 BJT 接 GHB530 自激蜂鳴器。

---

## 2. 關鍵元件

### 2a. LED ×4

| LED Ref | 顏色 | 典型 VF @ 5mA | 限流電阻 | 驅動 BJT | MCU 控制訊號 |
|:-:|:-:|:-:|:-:|:-:|:-:|
| D1303 | 黃 | 1.9V | R0403 240Ω | Q0402 DTC1143ZE | LED1_CTRL |
| D0401 | 黃綠 | 1.85V | R0402 240Ω | Q0401 DTC1143ZE | LED2_CTRL |
| D0404 | 紅 | 1.9V | R0406 240Ω | Q0404 DTC1143ZE | LED3_CTRL |
| D0405 | 藍 | 2.65V | R0407 82Ω | Q0405 DTC1143ZE | LED4_CTRL |

**電流計算（典型）**：
- 黃 / 黃綠 / 紅：(3.3V - 1.9V - Vce_sat 0.1V) / 240Ω ≈ **5.4mA** → datasheet 典型標 VF @ 5mA 合理
- 藍：(3.3V - 2.65V - 0.1V) / 82Ω ≈ **6.7mA** → 比其他略高（補償藍光效率）

**LED_3V3 rail**：
- R0401 0R / 1/16W series → 100nF (C0401) bypass → 直接接 VDD_3V3
- 100mA 標註（4 顆 LED 全開 ~25mA，裕量充足）
- **TP0401** 測試點

### 2b. 蜂鳴器

| Ref | 型號 | 功能 | 備註 |
|---|---|---|---|
| U0401 | **GHB530** | 自激電磁式蜂鳴器 | 典型工作頻率 2.7-4kHz；需 DC 電壓驅動；內建震盪電路 |
| Q1203 | 2SC4617 | 蜂鳴器驅動 NPN | base 經 R0409 4.7K series + R0408 47K pull-down |
| D0403 | SRR8528S-30 | **飛輪二極體（flyback）** | 吸收蜂鳴器 inductive kick |
| R0410 | 3R 1/16W | 蜂鳴器電流限制 | 保護 BJT 不過流 |
| C0402 | 100nF 16V | BUZZER_3V3 bypass | |

**驅動邏輯**：
```
MCU GPIO BUFFER_CTRL → R0409 → Q1203 base
                               Q1203 collector → BUZZER_3V3 經 R0410 3R → U0401 蜂鳴器 → GND
                               flyback D0403 跨 蜂鳴器 兩端
```

- BUFFER_CTRL 高 → Q1203 ON → 蜂鳴器通電 → 內部震盪發聲
- BUFFER_CTRL 低 → Q1203 OFF → 蜂鳴器斷電 → 不響
- **BUFFER_CTRL 可以 PWM**（圖面註記 "pwm"）→ 控制斷續鳴叫節奏，但不改變音調（音調由蜂鳴器自激頻率決定）

---

## 3. 電源輸入

| Rail | 來源 | V | I | 用途 |
|---|---|:-:|:-:|---|
| LED_3V3 | ← Sheet 7 VDD_3V3（直接共用）| 3.3V | ~25mA max（4 LED 全開）| LED anode |
| BUZZER_3V3 | ← Sheet 7 U0703（可切）| 3.3V | ~100mA | 蜂鳴器驅動 |

**為什麼 BUZZER 要獨立 rail**：
1. 蜂鳴器 inductive 負載，開關瞬間產生 ~100V 感應尖峰，用獨立 rail + flyback 保護避免污染共用 3V3
2. 省電模式可完全關掉（BUZZER_3V3_EN 由 AW9523 控）—— 平時 Iq 甚至連 LDO 都不耗

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 |
|---|:-:|---|
| LED1_CTRL | in | ← Sheet 1 MCU GPIO |
| LED2_CTRL | in | ← Sheet 1 MCU GPIO |
| LED3_CTRL | in | ← Sheet 1 MCU GPIO |
| LED4_CTRL | in | ← Sheet 1 MCU GPIO |
| BUFFER_CTRL | in | ← Sheet 1 MCU GPIO（PWM 可用）|

---

## 5. 設計細節

- **DTC1143ZE 預偏置**：內建 R1 (B-E) 4.7kΩ、R2 (base series) 4.7kΩ → MCU GPIO 直接驅動 base 不用外加 base resistor。Vbe 門檻 ~0.7V，MCU 3.3V 下 Ib ≈ (3.3-0.7)/4.7k ≈ 0.55mA，β ≥ 100 → Ic 可承 55mA（遠大於 LED 5mA 需求）。
- **提案書 P13 的 LED 顏色對應**：
  - V1.8 P13 定義 **Driver 黃 / GPS 紅 / Net 藍 / Power-Memory 綠**
  - SCH 畫出 **黃 / 黃綠 / 紅 / 藍**
  - 「黃綠」= 綠（通常是 540nm 黃綠光），即 Power/Memory
  - 四色對照：LED1_CTRL=黃=Driver, LED2_CTRL=黃綠=Power/Memory, LED3_CTRL=紅=GPS, LED4_CTRL=藍=Net
  - **對應關係**要在韌體層確定；若 LED 位置排序與 CTRL 編號不一致會誤導使用者，EVT 要實測對照
- **藍 LED 82Ω**：藍光 VF 高 → 相同 3V3 下電流會變小，所以降 R 補償。常見設計做法。
- **蜂鳴器 R0410 3R**：很小的 series resistor，主要用途是**限制短路電流**（蜂鳴器 open 時無效，shorted 時 Ic 不會超 1A）；對正常振盪的 impedance 幾乎無影響。
- **飛輪 D0403 SRR8528S-30**：30V shottky，吸收蜂鳴器 off 瞬間的 inductive kick。
- **自激 vs 他激蜂鳴器**：GHB530 內建震盪電路 → DC 驅動即可發聲，單音；若要多音（告警 vs 提示用不同音調）要改他激蜂鳴器 + PWM 從 MCU 調頻率 → **本設計只能靠 PWM 斷續節奏區分事件**。

---

## 6. 疑點 / Review Notes

- [ ] **📘 LED 顏色 → CTRL 編號對照**：韌體要確認 LED1=Driver / LED2=Power/Memory / LED3=GPS / LED4=Net（與提案書 P13 一致），開機自測 LED 全亮確認位置正確。
- [ ] **📘 蜂鳴器只能單音**：若客戶 UX 需求有「不同事件用不同音」的差異化，GHB530 無法達成。要早點確認；若需多音，SOW 要改規格換他激蜂鳴器。
- [ ] **📘 LED 的夜間太亮問題**：3.3V @ 5mA 的 LED 在夜間車內可能太刺眼 → 可用 PWM dimming（LED_CTRL 用 PWM 代替 DC）—— 但現在 LED1-4 經 DTC1143ZE 已經是數位控制，PWM 到 base 時 BJT 跟得上（<1µs 開關時間），OK。但要 MCU GPIO 支援 PWM，階段 B 查 FR3068E-C 的 PWM 可配通道。
- [ ] **📘 Power/Memory LED 常亮代表什麼**：提案書 P13 寫「恆亮=開機且 Memory 正常；熄滅=關機/休眠；閃爍=開機但 Memory 異常」—— 這條邏輯要韌體層面做 self-test 確認 flash 可讀。
- [ ] **📘 省電模式 LED 行為**：Deep sleep 時 LED 全熄還是 breathing？與 UI 設計要求有關。
- [ ] **📘 蜂鳴器 BUZZER_3V3_EN 的開關延遲**：EN 經 I2C IO expander 控制 → 開蜂鳴器要先 I2C 寫 AW9523 → BUZZER_3V3_EN 拉高 → LDO 建壓 → BUFFER_CTRL 高 → 蜂鳴器響。整段延遲可能 10-50ms，對「立即告警」場景邊緣可接受；若要 ms 級響應，BUZZER_3V3 應改常開。

---

## 7. 參考

- SCH PDF：Sheet 4
- 相關 sheet：Sheet 1（CTRL 訊號源）、Sheet 7（LED_3V3 / BUZZER_3V3）、Sheet 11（AW9523 控 BUZZER EN）
- 提案書章節：[02_P05 燈號 + 蜂鳴器需求](../../00_project/02_客戶需求.md)、[03_P13 LED Status 定義](../../00_project/03_技術方案_系統設計.md)
- Datasheet：Rohm/等 DTC1143ZE、GHB530（自激蜂鳴器 datasheet 階段 B 查）
