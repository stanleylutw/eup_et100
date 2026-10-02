# Sheet 11 — IO Expander + Level Shift + 外部中斷 + 1-Wire + ACC + 電源控制

**PDF Page**：11 / 15
**Ref Designator Prefix**：11xx
**圖面標題**：MCU_LTE 電平轉換；外部 ACC 中斷輸入檢測；外部 IO×3 中斷輸入檢測；外部 DC 電源輸入檢測；MCU_IO 擴展；一線溫度數據採集；對外 ACC 信號輸出；外部電源輸入開關控制
**對應提案書章節**：[02_客戶需求.md §P05 4× DI / 1× DO / 1-Wire 溫度 ×4](../../00_project/02_客戶需求.md)

---

## 1. 功能定位

**本 sheet 是全板最雜的一頁**，功能分 8 個子塊：

1. **MCU↔LTE 電平轉換**：3V3 ↔ 1V8，4 條（UART TX、UART RX、LTE_WAKE_MCU、MCU_WAKE_LTE），全用 2SC4617 BJT 單端 shifter
2. **IO expander**：AW9523BTQR I2C 擴展 16 個 GPIO
3. **外部 ACC 中斷輸入**：BJT 偵測 car ACC（點火信號）
4. **外部 IO×3 中斷輸入**：3 路數位 input 偵測
5. **1-Wire 溫度**：DS18B20 介面，含 fuse + BJT buffer
6. **對外 ACC 輸出**：BJT 推 ACC_OUT 給外設
7. **外部 DC 電源輸入檢測**：BJT 偵測 PWR_IN 存在
8. **外部電源輸入開關控制**：PNM523T201ED PFET 切 PWR_IN

---

## 2. 關鍵元件

### 2a. IO Expander

| Ref | 型號 | 功能 |
|---|---|---|
| U1101 | AW9523BTQR | I2C 16-ch IO expander, 3.3V, 100mA max per pin |

**AW9523 pin 分配**（從 SCH 讀出）：

| AW9523 Pin | Net | 方向 | 用途 |
|:-:|---|:-:|---|
| P1_0 | MCU_RS232_EN1 | out | Sheet 5 第 1 顆 RS232 EN |
| P1_1 | MCU_RS232_EN2 | out | Sheet 5 第 2 顆 RS232 EN |
| P1_2 | MCU_RS232_EN0 | out | Sheet 5 第 3 顆 RS232 EN |
| P1_3 | GNSS_LDO_EN | out | Sheet 12 GNSS 子板 LDO EN |
| P0_5 | NFC_1V8_EN | out | Sheet 7 U0704 EN |
| P1_4 | VDD_3V3_EN | out | Sheet 7 U0702 EN |
| P0_7 | BUZZER_3V3_EN | out | Sheet 7 U0703 EN |
| P0_6 | VDD_PP_3V3_EN | out | Sheet 5 RS232_3V3 power switch EN |
| P0_3 | PWR_SW_CTRL | out | 本 sheet Q1108 PFET gate |
| P0_2 | DC_DET | in | 本 sheet DC 偵測 |
| P0_4 | GNSS_VBAT_EN | out | Sheet 12 Q1201 GNSS backup PMOS |
| P1_5 | MCU_WAKE_LTE_3V3 | out | 本 sheet 經 BJT shift → MCU_WAKE_LTE 1V8 |
| P1_6 | MCU_CHA_EN | out | Sheet 8 charger EN（鏡像備援） |
| P1_7 | ACC_INT_OUT | out | 本 sheet BJT 推對外 ACC 輸出 |
| P1_8 | MCU_CAN_STB | out | Sheet 5 CAN STB |
| AD1/AD0 | GND | — | I2C 位址設定（都接 GND → 位址 0x58 或類似，查 datasheet） |
| INTN | EXP_INTN | out | → Sheet 1 MCU 中斷通知任一 input 變化 |
| RSTN | EXP_RST | in | ← Sheet 1 MCU reset AW9523 |
| SCL/SDA | EXP_I2C5_SCL/SDA | — | ↔ Sheet 1 MCU |

> **注意**：pin 編號 P0_X / P1_X 用的是 AW9523 的 datasheet 命名；SCH 上寫的是 `P0_X` / `P1_X` 格式，實際對應 I2C register bit。階段 B 查 AW9523 datasheet 核對 register map。

### 2b. 電平轉換（UART & WAKE）

4 組 2SC4617 BJT shifter，拓撲均為「open-collector + pull-up」單端轉換：
- **MCU_LTE_TXD_3V3 → UART_TXD_1V8**：Q1102 base 接 MCU_LTE_TXD_3V3，collector 接 UART_TXD_1V8，上拉 4.7K 到 LTE_EXT_1V8
- **UART_RXD_1V8 → MCU_LTE_RXD_3V3**：Q1104 base 接 UART_RXD_1V8，collector 接 MCU_LTE_RXD_3V3，上拉 4.7K 到 MCU_3V3
- **LTE_WAKE_MCU（1V8）→ LTE_WAKE_MCU_3V3**：Q1115 相同拓撲
- **MCU_WAKE_LTE_3V3 → MCU_WAKE_LTE（1V8）**：Q1106 相同拓撲

每組有 1nF (C110x) + 4.7K pull-up。

### 2c. 外部 IO 中斷檢測

每路相同拓撲，`ACC_INT_IN` / `IO1_INT_IN` / `IO2_INT_IN` / `IO3_INT_IN`：

```
外部 >5V 訊號 → R_in 43K → 分壓到 6.8K → BJT 2SC4617 base
                                           → collector 接 100K pull-up @ MCU_3V3
                                           → 送到 MCU PA14~15 / 其他 GPIO
```

分壓 43K+6.8K 把外部 36V 降到 ~5V 左右（在 BJT Vbe 限內），BJT 當 inverter buffer，MCU 看到的是反相邏輯。4 路都一樣（Q1101/Q1103/Q1105/Q1107）。

### 2d. 1-Wire 溫度採集

```
1_Wire_Temp_IN_CON (外接 DS18B20 上行)
    ↓
F1 TLC-FSMD005_24 fuse (over-current)
    ↓
PESD TVS → GND (clamp)
    ↓
1_Wire_Temp_IN (線上)
    ↓
分兩路：
  a) → Q1116 2SC4617（經 pull-up 4.7K，推 1_Wire_Temp_IN_MCU 到 Sheet 1）
  b) ← Q1117（1_Wire_Temp_OUT_MCU 經 BJT 推 1-Wire bus，雙向通訊 half-duplex）
```

1-Wire 用 BJT 做半雙工 bus driver（DS18B20 parasitic power 或 external power 皆支援）。

### 2e. 對外 ACC 輸出

```
MCU GPIO ACC_INT_OUT (via AW9523 P1_7) → Q1110 2SC4617 base
                                        → collector → VDD_5V_PP 經 4.7K pull-up
                                        → ACC_INT_OUT_CON (去 J0901 pin 14/16/18/22)
```

**ACC_INT_OUT 在 J0901 占了 4 支 pin**（14/16/18/22）—— 這是對外提供 ACC 訊號給 4 個外部 accessory 用（例如 RS232 設備的喚醒線都需要 ACC）。

### 2f. 外部電源偵測

```
PWR_IN_9-36V → R1143 43K → 6.8K 分壓 → Q1109 2SC4617 base
                                       → collector → 100K pull-up @ MCU_3V3
                                       → DC_DET (到 AW9523 P0_2)
```

MCU 透過 I2C 讀 AW9523 input register 即可知道外部 DC 是否存在，用於偵測 ignition / 掉電。

### 2g. 外部電源開關

```
PWR_IN_9-36V → Source of Q1108 PNM523T201ED (P-MOSFET)
                                             Gate 經 R1134 470R → 由 PWR_SW_CTRL(AW9523 P0_3) 控
                                             Drain → VDD_PP_9V (下游)
```

**用途**：讓 MCU 可以軟關掉下游所有 9-36V rail —— 但問題是，如果這條 switch off，Sheet 6 的一級 DCDC 也斷了，MCU 自己也會掉電。**所以這條 switch 不是給 MCU 自己用，而是只切「VDD_PP_9V」這條特定分支**，但從目前 netlist 看不到 VDD_PP_9V 下游接到哪 —— **階段 B 必查**。可能是給外接 RS232 設備的「對外 9-36V 供電」用。

---

## 3. 電源輸入

| Rail | 用途 |
|---|---|
| MCU_3V3 | AW9523 VCC + 所有 BJT pull-up |
| VCC_1V8 | 外部 IO 中斷檢測 BJT 的 3V3 pull-up 另一端部分 |
| LTE_EXT_1V8 | UART TXD/RXD 1V8 側 pull-up |
| VDD_5V_PP | 對外 ACC_INT_OUT 推力源 |
| PWR_IN_9-36V | Q1108 PFET 源電壓 |

---

## 4. 介面（I/O Nets）

太多，重點：所有 ACC/IO/1-Wire/RS232 的 CON（connector 側）net → 連 Sheet 9 J0901；控制 net（EN/CTRL）→ 連 Sheet 1 MCU 或由 AW9523 代管。詳見階段 B 的 [`91_net_inventory.md`](91_net_inventory.md)。

---

## 5. 設計細節

- **為什麼用 I2C IO expander 而不直接用 MCU GPIO？**
  - 省 MCU pin：16 條 EN/CTRL/STB 線用 2 條 I2C 控制
  - 邊緣掉線保護：I2C 掛掉時 AW9523 default output 可設計成「所有可切 rail off」，比 MCU GPIO reset 時的三態更可控
  - **代價**：I2C latency，低延遲訊號不能用（例如 CAN STB 原本要求 ns 級，掛 I2C 要 ms 級 → **CAN waking from standby 會慢**，若 bus 第一幀就要收可能漏接）
- **BJT 做 UART level shift 的問題**：
  - 速度限制：Rise time 由 pull-up × stray C 決定，4.7K + 10pF → τ=47ns → 10% 到 90% 約 100ns，9600 bps（周期 104µs）綽綽有餘，115200 bps（周期 8.7µs）也 OK，**超過 1 Mbps 就會波形變差**
  - 反相邏輯：BJT 當 common-emitter 是反相，所以 MCU 端的 TX 高 → shift 後是低，軟體要用「反相 UART」或靠 UART peripheral 的 polarity invert 設定；**韌體要特別處理**
  - 相比 IC level shifter（TXS0108、SN74LVC2T45）成本便宜，但走線一多就煩
- **外部 IO 偵測的 43K+6.8K 分壓**：36V × 6.8/(43+6.8) ≈ 4.92V 到 BJT base，Vbe 開 0.7V 後 collector 拉低。**9V 時**：9 × 6.8/49.8 ≈ 1.23V > Vbe，可偵測到；**5V 時**：5 × 6.8/49.8 ≈ 0.68V，接近 Vbe 門檻，**可能偵測不穩定** → 若客戶接 5V 感測器要特別驗證。
- **1-Wire fuse + TVS**：FSMD005 500mA fuse 保護短路，PESD TVS 防 ESD。1-Wire 線一拉就 >10m，EMC/ESD 必有。

---

## 6. 疑點 / Review Notes

- [ ] **🟠 CAN STB 經 I2C 控**：bus 首幀響應延遲，CAN 標準要求 standby 喚醒 <10µs 時無法達成；若 J1939 診斷服務要求嚴格可能卡。
- [ ] **🟠 I2C 掛掉 → AW9523 POR default**：datasheet 查預設 output 是否 = 0；若為 0，開機瞬間所有可切 rail off，MCU 要能撐到軟體把 EN 拉高（<100ms）。
- [ ] **📘 BJT UART shift 軟體反相**：韌體必須在 UART 設定裡開 TX/RX polarity invert，否則 LTE AT command 全讀亂碼。
- [ ] **📘 Q1108 PWR_SW_CTRL 下游不明**：VDD_PP_9V net 從哪消費？從現有圖看沒找到下游，可能是給未來擴充或外接設備預留的「對外 9-36V 轉出」。**階段 B 必查 netlist**。
- [ ] **📘 ACC_INT_OUT 占 J0901 4 支 pin**：對應提案書 P12 的 RS232_1/2 / CAN / RS232_0 四組 4-pin 子連接器各帶一條 ACC 輸出。設計意圖對，但 4 條都接到同一 BJT collector → 任何一個外設短路到 GND 就會把 4 條 ACC 同時拉死。**應該要每條獨立 fuse 或獨立 BJT**。
- [ ] **📘 外部 IO 偵測對 5V 訊號 marginal**：提案書 P05 寫 ">5V 觸發" 剛好在門檻，若實測不穩可能要調 R1143 等的分壓比。
- [ ] **DNP**：R1140、R1157 這類 pull-up/down 有 NM，須階段 B 列。

---

## 7. 參考

- SCH PDF：Sheet 11
- 相關 sheet：Sheet 1（MCU I2C 控 AW9523）、Sheet 2（LTE UART / WAKE）、Sheet 5（RS232 EN）、Sheet 7（LDO EN）、Sheet 8（CHA_EN）、Sheet 9（外部 CON 線路）、Sheet 12（GNSS EN）
- 提案書章節：[02_P05 4×DI/1×DO/1-Wire ×4](../../00_project/02_客戶需求.md)
- Datasheet：Awinic AW9523B（階段 B）、PNM523T201ED、2SC4617
