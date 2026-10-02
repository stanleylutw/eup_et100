# 全系統電源樹（Power Tree）

**來源**：綜合 Sheet 6（主 DCDC）、Sheet 7（LDO 群）、Sheet 8（充電 / 電池）、Sheet 2（LTE_VBAT 開關）、Sheet 11（PWR_SW_CTRL）、Sheet 12（GNSS_VBAT 開關）、Sheet 15（參考 power tree 圖）。
**依據的 SCH 版本**：V1.1 / 2026-09-23。

> **2026-10-02 MCU 規格註記**：此樹保留舊 SCH rail／net 記錄，不代表 v0.4.9 已確認各 pin 的供電角色或分域 sequencing。MCU_VCC／BT_VCC 建議 2.9-3.6 V，BOR 2.4 V；IO LDO output 與 VDD_IO input 不應混稱或自行外接，須依 board-matched hardware guide 核對。1V8 關閉時的 backfeed／unpowered IO 仍待確認，見 [電氣條件](../fr306x_reference/03_electrical_power_and_clock.md)。

> 這是**階段 A** 的骨架；電流預算、efficiency、thermal 計算、decoupling 審查進入階段 B 再補。本頁用來讓後續 13 份 Sheet MD 都能引用同一棵電源樹，避免每頁各寫一遍。

---

## 1. 全圖（文字版樹狀）

```text
26pin J0901 pin1 PWR_IN (9-36V, 保險絲 1A @ 線束端)
  │
  ├─▶ F0901 (SMD fuse 2A, 1206WJF200A072V)  ← Sheet 9 DC input current limit
  │    │
  │    ├─▶ D0901 (TVS PSBDAF60V3, 60V clamp)  ─▶ GND  (板端二級過壓保護)
  │    │
  │    └─▶ PWR_IN_9-36V  ──▶ [Sheet 11 Q1108 PNM523T201ED PFET]
  │                           │  Gate 由 MCU PWR_SW_CTRL 控制
  │                           │  用途：MCU 可切斷下游所有電源（軟關機 / 低功耗）
  │                           ▼
  │                       VDD_PP_9V (orange, "9V" 命名是歷史殘留，實際 = PWR_IN_9-36V 直通)
  │
  │                            [暫缺：Q1108 下游分支到何處？需 Sheet 11 細讀 — 階段 B]
  │
  └─▶ 無限流直通 PWR_IN  ─▶ 多處 TP0601 偵測點

PWR_IN_9-36V (= 板上 Vbatt_in)
  │
  ├─▶ U0601 SGM61630AXPS8GTR  (Sheet 6, 第一級 DCDC)
  │     Vin 4.3V-60V, 3A, fsw=500kHz, 140mΩ HS FET, 50uA Iq
  │     FB=57.6k/10k+1, Vout ≈ 5.07V
  │     ├─ L0601 10uH 0.068Ω 4.5A
  │     └─▶ VDD_5V (TP0602)  ──┬──▶ D0601 (PSBDAF40V3 ORing diode) → VDD_5V_MCU (3A rail for 二級 DCDC)
  │                             └──▶ D0602 (PSBDAF40V3 ORing diode) → VDD_5V_PP (1A rail: RS232 / NFC_VDD / CAN / Charger)
  │
  ├─ (ADC1 偵測 PWR_IN 電壓 → MCU)
  │
  ↓

VDD_5V_MCU (3A)
  │
  └─▶ U0602 JW5357MSOTBWTR  (Sheet 6, 第二級 DCDC)
        Vin 2.7-6V, 3A, 1.5MHz
        FB → Vout = 4V, 3A (output 經 R0640 磁珠 → VDD_4V)
        │
        ↓

VDD_4V (3A, 經 D0701/D0702 ORing)
  │
  ├─ D0701 (PSBDAF40V3)  ← VDD_4V 直通進 LDO 群
  ├─ D0702 (PSBDAF40V3)  ← VBAT (備援電池, 經 Sheet 8 charger 下接)
  │                        在 VDD_4V 處與主路 OR，實現 **主電源掉線 → 電池接管**
  │
  ▼  (合流為 VDD_LDO_4V)

VDD_LDO_4V  (供下列 4 顆 WR0338 系列 LDO，見 Sheet 7)
  │
  ├─▶ U0701 WR0338-33A50R → **MCU_GNSS_3V3** (300mA, 常開, EN 直接拉高)
  │       └─▶ Sheet 1: MCU 的 IO3V3_LDO / MCU 3V3；Sheet 12: GNSS 子板 GNSS_3V3
  │
  ├─▶ U0702 WR0338-33A50R → **VDD_3V3**     (300mA, VDD_3V3_EN 由 Sheet 11 IO expander 控制)
  │       └─▶ Sheet 3 G-Sensor_3V3；Sheet 4 LED_3V3；Sheet 5 RS232 enable 上拉；Sheet 7 下游共用
  │
  ├─▶ U0703 WR0338-33A50R → **BUZZER_3V3**  (300mA, BUZZER_3V3_EN 由 IO expander 控制)
  │       └─▶ Sheet 4 蜂鳴器驅動
  │
  └─▶ U0704 WR0338-18A50R → **NFC_1V8**     (300mA, NFC_1V8_EN 由 IO expander 控制)
          └─▶ Sheet 10 PN7160 VDD_PAD；Sheet 3 G-Sensor_1V8（I2C pull-up）；Sheet 1 MCU VCC_1V8

VDD_5V_PP (1A)
  │
  ├─▶ Sheet 8 U0801 YX4066HDN8AR charger VIN → **VBAT**（充 600mAh 鋰電, CHA_EN 由 MCU 控制）
  │                                              VBAT_NTC / VBAT_ADC_IN 偵測溫度與電壓
  ├─▶ Sheet 5 RS232_3V3 power switch 輸入（經 YJL2305A PMOS Q0502）
  ├─▶ Sheet 10 R1001 → NFC_VDD（PN7160 TX/analog VDD）
  ├─▶ Sheet 5 CAN_5V（U0504 SIT1042 VCC）
  └─▶ Sheet 11 ACC_INT_OUT 的對外推送電源

VDD_LDO_4V
  │
  ├─▶ Sheet 2 Q0232 2SC4617 ─▶ **LTE_VBAT**（EG800Q-EU 主電, LTE_VBAT_EN 由 MCU 控制）
  │       Note: 用 BJT 切電源，Vce 壓降 ~0.2-0.5V，LTE 實際入電 ~3.5-3.8V；
  │       需在階段 B 驗 EG800Q-EU 的 VBAT 容忍範圍（spec 3.4-4.2V）
  │
  └─▶ Sheet 12 Q1201 YJL2305A P-MOSFET ─▶ **GNSS_VBAT**（GNSS 子板備援 RTC/星曆記憶, GNSS_VBAT_EN 控制）

Sheet 2 LTE_EXT_1V8（來自 EG800Q-EU 的 VDD_EXT）
  │
  ├─▶ Sheet 3 SIM VCC 路徑的 pull-up
  ├─▶ Sheet 11 UART_TXD_1V8 / UART_RXD_1V8 / LTE_WAKE_* 的 level-shift 高側電壓
  └─▶ Sheet 13 LTE antenna tuner U1301 MXD8544AE 的 VDD（RF 控制邏輯）
```

---

## 2. Rail 清單

| Rail | 來源 | Vout | 電流上限（圖註） | Enable | 典型消費端 |
|---|---|:-:|:-:|---|---|
| PWR_IN_9-36V   | 外部 9-36V → F0901 fuse → TVS | 9-36V | 2A | 常開（硬體保險絲） | DCDC 第一級 U0601、ADC1 偵測 |
| VDD_PP_9V      | PWR_IN_9-36V 經 Q1108 PFET  | 9-36V | 200mA | PWR_SW_CTRL (MCU) | 待確認下游（階段 B） |
| VDD_5V         | U0601 DCDC                 | 5.07V | 3A | 常開 | 兩路 ORing 分 _MCU 與 _PP |
| VDD_5V_MCU     | VDD_5V ─ D0601             | 5V    | 3A | 常開 | U0602 DCDC 二級 input |
| VDD_5V_PP      | VDD_5V ─ D0602             | 5V    | 1A | 常開 | Charger、CAN、NFC analog、RS232 switch |
| VDD_4V         | U0602 DCDC                 | 4V    | 3A | 常開 | 經 D0701 ORing 進 VDD_LDO_4V |
| VBAT           | 600mAh Li-ion 經 Charger    | ~3.5-4.2V | 1A | 充電由 MCU_CHA_EN | 經 D0702 ORing 進 VDD_LDO_4V |
| VDD_LDO_4V     | VDD_4V ∥ VBAT ORing        | 3.5-4V | 3A | 常開 | 4 顆 WR0338 LDO、LTE_VBAT 開關、GNSS_VBAT 開關 |
| LTE_VBAT       | VDD_LDO_4V 經 Q0232 BJT     | ~3.5-3.8V | 1.5A | LTE_VBAT_EN (MCU) | **EG800Q-EU 主電** |
| GNSS_VBAT      | VDD_LDO_4V 經 Q1201 PMOS    | ~3.8-4V | 300mA | GNSS_VBAT_EN (via Sheet 11) | GNSS 子板 (RTC/星曆保持) |
| MCU_GNSS_3V3   | U0701 WR0338-33A50R          | 3.3V | 300mA | 常開 | MCU IO3V3_LDO + GNSS_3V3 |
| VDD_3V3        | U0702 WR0338-33A50R          | 3.3V | 300mA | VDD_3V3_EN (IO expander) | G-Sensor、LED、RS232 enable |
| BUZZER_3V3     | U0703 WR0338-33A50R          | 3.3V | 300mA | BUZZER_3V3_EN (IO expander) | 蜂鳴器驅動 |
| NFC_1V8        | U0704 WR0338-18A50R          | 1.8V | 300mA | NFC_1V8_EN (IO expander) | PN7160 VDD_PAD、G-Sensor 1V8、MCU 1V8 域 |
| NFC_VDD        | VDD_5V_PP 直通 R1001         | ~5V | 1A | 常開 | PN7160 VDD_A / VDD_D / VDD_TX / VDD_UP |
| LTE_EXT_1V8    | EG800Q-EU VDD_EXT 輸出        | 1.8V | 50mA（SIM 用）| 由 LTE module 內部 | SIM、UART level shift 高側、LTE tuner VDD |

---

## 3. Enable 控制網

```text
MCU GPIO ─▶ PWR_SW_CTRL     ─▶ Q1108 PFET  ─▶ 切斷/接通 PWR_IN 下游分支
MCU GPIO ─▶ MCU_LTE_VBAT_EN ─▶ Q0232 BJT   ─▶ LTE_VBAT (LTE 開關機)
MCU GPIO ─▶ MCU_CHA_EN      ─▶ U0801 Charger EN  ─▶ 電池充電啟/停

MCU I2C ─▶ U1101 AW9523 IO expander ─┬─▶ VDD_3V3_EN
                                      ├─▶ BUZZER_3V3_EN
                                      ├─▶ NFC_1V8_EN
                                      ├─▶ GNSS_LDO_EN   （GNSS 子板 VDD 的 LDO EN）
                                      ├─▶ GNSS_VBAT_EN  （GNSS 備援）
                                      ├─▶ MCU_WAKE_LTE_3V3 （LTE 喚醒）
                                      ├─▶ MCU_CHA_EN   （鏡像備援路徑）
                                      ├─▶ MCU_CAN_STB  （CAN 待機）
                                      └─▶ ACC_INT_OUT  （對外 ACC 輸出）
```

**設計意圖**：主要電源 rails 由 I2C IO expander 開關，MCU 可以把平常用不到的 rail 全部關掉（例如：停車 >N 分鐘 → 關 BUZZER_3V3、關 NFC_1V8、關 LTE_VBAT、保留 GNSS_VBAT 熱啟動星曆），達成 context-aware 省電。但同時代表 **I2C bus 掛掉就全掉電**，應確認 AW9523 的 POR default 是「全部 output = enable」還是「disable」—— 這會決定開機瞬間行為，**待階段 B 查 AW9523 datasheet**。

---

## 4. ORing / 掉電接管順序

```text
Normal:
  PWR_IN_9-36V OK → DCDC → VDD_5V → VDD_4V → D0701 → VDD_LDO_4V ≈ 4.0V
  VBAT (3.8V) 經 D0702, 因 VDD_4V 較高, D0702 反向偏壓, 不供電
  → 電池不放電, 由 Charger 充飽

主電源掉線 (車熄火切電 or 保險絲斷):
  VDD_5V_PP → Charger VIN 消失 → 停止充電
  VDD_5V → VDD_4V 掉, D0701 反向
  VBAT 透過 D0702 承接 VDD_LDO_4V (壓降一顆 shottky ≈ 0.3V)
  → LDO 群仍工作: MCU/GNSS/VDD_3V3/NFC_1V8 保持
  → 但 VDD_5V_PP 側 (CAN/RS232/NFC_VDD) 全掉 → 只能靠 LTE_VBAT 回報
```

**待驗（階段 B）**：
- D0701/D0702 的 Vf（PSBDAF40V3 datasheet）→ 計算 VDD_LDO_4V 在電池供電時的實際電壓 → 是否仍 >LDO dropout（WR0338 dropout 170mV @100mA）→ 否則 3V3 rail 會崩
- 掉電瞬間 VDD_5V_PP 側負載（NFC 讀卡動作中？）會不會把 bulk cap 的能量回灌 → charger 需要有 reverse-blocking
- 600mAh 搭 371.8mW 平均功耗（提案書 P14）實際續航 → 已在 [10_風險與談判清單.md §4](../../00_project/10_風險與談判清單.md) 標為必重算

---

## 5. 與 Sheet 15 參考 power tree 的差異

Sheet 15 的 reference power tree 圖是**簡化版**，與實際接線的差異：

| 項目 | Sheet 15 reference | 實際 SCH |
|---|---|---|
| LDO 數量 | 畫 2 顆（1.8V + 3.3V） | 實際 4 顆（3 顆 3.3V + 1 顆 1.8V） |
| 3.3V 分路 | 畫一條線到 MCU/GNSS/FLASH/GSENSOR/BUZZER/LED/LevelShift | 實際分成兩條 rail：MCU_GNSS_3V3 常開、VDD_3V3 可切 |
| LTE_VBAT | 直接畫 DCDC 3.8V → EG800Q | 中間有 Q0232 BJT 開關，MCU 可切 LTE 電源 |
| GNSS_VBAT | 未標示 | 實際有 Q1201 YJL2305A 可切 |
| Antenna tuner | 未標示 | 實際 Sheet 13 有 MXD8544AE |

**結論**：Sheet 15 僅為 reference overview，實際以 Sheet 6/7/8/11/12 為準。
