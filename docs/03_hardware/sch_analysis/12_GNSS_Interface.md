# Sheet 12 — GNSS 子板介面（主板端）

**PDF Page**：12 / 15
**Ref Designator Prefix**：12xx
**圖面標題**：小板 GNSS 接口
**對應提案書章節**：[04_技術方案_關鍵零件.md §P21 AG3352Q](../../00_project/04_技術方案_關鍵零件.md)、[03_P17 GNSS 子板 30×45×1.6 mm](../../00_project/03_技術方案_系統設計.md)

> 本頁只畫**主板到 GNSS 子板的連接介面**；GNSS 芯片本身（AG3352Q）、陶瓷天線、子板 LNA/SAW 等在獨立子板 schematic（**不在本 PDF 內**）。

---

## 1. 功能定位

**主板 ↔ GNSS 子板**的連接介面：
- 透過 8-pin FPC 連接器（J1201 TF31-8S-0.5SH(800)）傳：VBAT、GND、UART TXD/RXD、DATA_IN_EINT 中斷、LDO_EN 控制、3V3 主電源
- 主電源 `MCU_GNSS_3V3` 從 Sheet 7 常開 LDO 進來，經 D1201 series diode → GNSS_3V3
- 備援 `GNSS_VBAT` 由 Sheet 7 VDD_LDO_4V 經 Q1201 YJL2305A P-MOSFET 開關；用 Q1202 2SC4617 做 PMOS gate driver（GNSS_VBAT_EN 來自 Sheet 11 AW9523 P0_4）
- 所有訊號線上有 TVS 保護陣列（6 顆 PESD / 全 NM 但有腳位預留）

---

## 2. 關鍵元件

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| J1201 | TF31-8S-0.5SH(800) | 8-pin FPC 0.5mm pitch | 到 GNSS 子板的 FPC conn | | |
| D1201 | （未明確標型號）series Shottky | SOD | MCU_GNSS_3V3 → GNSS_3V3 blocking | | 1A，Vf 0.3V |
| R1204 | 0R 1/16W | 0402 | MCU_GNSS_3V3 series | | 可改磁珠 |
| R1202 | NM 0R 1/10W | 0402 | VDD_LDO_4V → GNSS_VBAT 直通旁路 | **NM** | 若將來不想要 EN 控制可短路 |
| Q1201 | YJL2305A | SOT-23 | **P-MOSFET** 開關 for GNSS_VBAT | | 低 Rdson，比 BJT 損耗小 |
| Q1202 | 2SC4617 | SOT-23 | NPN BJT，PMOS gate 驅動 | | 配 R1203 0R、R1205 1K、R1204 100K |
| R1206 | NM_RB521CM-30T2R | SOD | 預留 reverse-blocking diode 位 | **NM** | |
| C1201 | 100nF | 0402 | PMOS 旁 bypass | | |
| C1202/C1203 | 100nF ×2 | 0402 | VDD_LDO_4V / GNSS_VBAT bypass | | |
| D1202~D1207 | **NM** PESDxC2V0D5/PESDNK402-03 | SOD-523 | **TVS 陣列 for 6 條 GNSS 訊號**（GNSS_VBAT / TXD / RXD / DATA_IN_EINT / LDO_EN / 3V3）| **全 NM** | ESD 保護預留位 |
| TP1201~TP1206 | — | TP | 6 條訊號的 debug 探針 | | |

### J1201 FPC pin 定義

| Pin | Net | 方向（主板視角） | 用途 |
|:-:|---|:-:|---|
| 1 | GND | — | GND |
| 2 | GNSS_VBAT | out | 備援電池模式 VBAT（給子板上 GNSS chip 的 VBAT_BACKUP） |
| 3 | GNSS_TXD | in | ← GNSS 子板的 UART TX（NMEA 輸出） |
| 4 | GNSS_RXD | out | → GNSS 子板的 UART RX（config / aiding） |
| 5 | GNSS_DATA_IN_EINT | in | ← GNSS 子板的中斷（1PPS 或 data ready） |
| 6 | GNSS_LDO_EN | out | → GNSS 子板的 LDO EN（控制子板主 VDD）|
| 7 | GNSS_3V3 | out | → GNSS 子板主 3V3 電源 |
| 8 | GND | — | GND |

---

## 3. 電源輸入

| Rail | 來源 | V | Enable | 用途 |
|---|---|:-:|---|---|
| MCU_GNSS_3V3 | ← Sheet 7 U0701 常開 | 3.3V | 常開 | → D1201 → GNSS_3V3 到子板 |
| GNSS_3V3 | MCU_GNSS_3V3 經 D1201 | ~3.0V (Vf 0.3V 後) | — | 子板主 VDD |
| VDD_LDO_4V | ← Sheet 7 ORing（主電 + 電池） | 3.5-4V | — | 經 Q1201 PMOS 到 GNSS_VBAT |
| GNSS_VBAT | VDD_LDO_4V 經 Q1201 PMOS | ~3.5-4V | GNSS_VBAT_EN（Sheet 11 P0_4） | 子板 GNSS chip 的 VBAT_BACKUP（保星曆 / RTC） |

**兩電源控制邏輯**：
- 常開 `GNSS_3V3` + 可切 `GNSS_LDO_EN`（給子板自己的 LDO）→ 可以關閉子板的 GNSS 主功能但保持 MCU 可以 I/O
- 可切 `GNSS_VBAT` → 關 VBAT 等於清掉子板 GNSS chip 的星曆記憶，下次開機要 cold start（30s+）

**節能策略**：
- Deep sleep：關 GNSS_LDO_EN（主 VDD off）、保 GNSS_VBAT（保星曆 → 下次 warm start <5s）
- Full off：關兩個 EN，下次 cold start

---

## 4. 介面（I/O Nets）

| Net | 方向 | 源 / 目的 |
|---|:-:|---|
| GNSS_VBAT_EN | in | ← Sheet 11 AW9523 P0_4 |
| GNSS_LDO_EN | out | → J1201 pin 6（給 GNSS 子板的 LDO EN）；此 net 的源端也是 Sheet 11 P1_3 |
| GNSS_TXD | in | ← J1201 pin 3 → Sheet 1 MCU UART5 RXD |
| GNSS_RXD | out | ← Sheet 1 MCU UART5 TXD → J1201 pin 4 |
| GNSS_DATA_IN_EINT | in | ← J1201 pin 5 → Sheet 1 MCU GPIO |
| MCU_GNSS_3V3 | in | ← Sheet 7 U0701 |
| VDD_LDO_4V | in | ← Sheet 7 ORing node |

---

## 5. 設計細節

- **為什麼 MCU_GNSS_3V3 常開？** MCU 本身是 3V3（這條 rail 和 MCU 共用），GNSS 子板不用時這條 3V3 仍需要給 MCU，所以 LDO 常開。子板上的 GNSS chip 另有 `GNSS_LDO_EN` 控制自己的 LDO，MCU 可以關掉子板 chip 而不影響自己。
- **D1201 series diode**：阻止 GNSS 子板的 VBAT_BACKUP 電源反灌回主板 MCU_GNSS_3V3 bus；Vf 0.3V → 子板實際 3V 左右（3.3V - 0.3V），**AG3352Q 的 VDD typical 3.3V ±5%，3V 已在下限**，階段 B 要查 AG3352Q datasheet 的 min VDD。
- **GNSS_VBAT 用 PMOS 而非 BJT**：相對 Sheet 2 的 LTE_VBAT（用 BJT）這裡用 PMOS，電流損耗更小（GNSS VBAT 典型 <1mA 給 RTC/星曆 → 相比 LTE 1.5A，差異不大，但設計一致性上該統一都用 PMOS）。
- **全 6 顆 TVS NM**：ESD 保護位預留但沒裝 —— 因為 GNSS 子板是**內部連接**（主板 ↔ 殼內子板），不走外部線束，ESD 風險低。EVT ESD 測試若失敗再補；量產通常保持 NM 省料。
- **沒有 1PPS 輸出線**：J1201 的 GNSS_DATA_IN_EINT 可能承擔 1PPS + NMEA UART ready 兩種角色；階段 B 查 AG3352Q 的 EINT pin 定義。

---

## 6. 疑點 / Review Notes

- [ ] **🟠 GNSS_3V3 經 D1201 Vf 0.3V 後只有 3V**：若 AG3352Q 的 VDD min 要求 3.0V，剛好邊緣；若 >3.0V 就不符合 spec。**階段 B 必查 AG3352Q datasheet**。可能的改善：用 ideal diode controller 或改 Si-ORing MOSFET。
- [ ] **📘 FPC 連接器選用 0.5mm pitch**：8-pin TF31 系列，I/O 不多但 FPC 彈性好；量產組裝時 FPC 排線的彎折半徑要列入 ME 規範。
- [ ] **📘 GNSS_LDO_EN 的 default 行為**：AW9523 reset 時 output 預設狀態 → GNSS_LDO_EN 低 → 子板 LDO off → GNSS 不工作。MCU 開機後要主動開 EN。
- [ ] **📘 子板上 GNSS chip 的消費**：提案書 P14 功耗表寫 72mA operating、0.3mA standby；但[10_風險與談判清單.md §4](../../00_project/10_風險與談判清單.md) 指 LTE 的 start up / search Active duration 都寫 0 秒 → 疑功耗表有誤。GNSS 的 4 秒熱啟動（P14 寫 4s）實際也需要驗證。
- [ ] **📘 GNSS 子板 schematic 不在本 PDF**：要向 Quectel 要子板獨立 SCH 才能完整做 review。
- [ ] **DNP**：R1202、R1206、D1202-D1207 全 NM，正常。

---

## 7. 參考

- SCH PDF：Sheet 12（主板端）；GNSS 子板 SCH 需另向 Quectel 索取
- 相關 sheet：Sheet 1（UART5）、Sheet 7（電源）、Sheet 11（EN 控制）
- 提案書章節：[04_P21 AG3352Q](../../00_project/04_技術方案_關鍵零件.md)、[03_P16-P18 GNSS 子板布局](../../00_project/03_技術方案_系統設計.md)
- Datasheet：Airoha AG3352Q（主模組 2.8V-4.3V input；chipset 1.8V/3.3V dual option）

---

## 階段 B — VDD-Vf 分析、Enable Sequence、Cold/Warm/Hot Start

### B.1 ✅ VDD-Vf 分析 — Stage A 擔心過度

**Datasheet 查核**：AG3352Q module input voltage range = **2.8V ~ 4.3V**

**重新評估 GNSS_3V3 = 3.0V（經 D1201 Vf 0.3V 後）**：
- 3.0V > 2.8V (min) → **符合 spec，有 0.2V 餘裕**
- Stage A 的擔心（「3V 已在下限邊緣」）**過度謹慎**；實際 margin 是夠的

**但補充注意**：
- VDD_LDO_4V 若在電池放電末期（見 [Sheet 7 B.1](07_PowerLDO_3V3_1V8.md)）下降到 3.2V，MCU_GNSS_3V3 LDO dropout 進入 → MCU_GNSS_3V3 可能從 3.3V 降到 ~3.0V
- 再經 D1201 Vf 0.3V → GNSS_3V3 = **2.7V < 2.8V min ❌**
- **連鎖效應**：電池放電末期時 GNSS 子板會先 brown-out，MCU 本身可能還能撐

**改善方向**：
- 若要電池模式下 GNSS 繼續可用：MCU_GNSS_3V3 LDO 要有更大 margin（換 LDO 到 2.5V-5.5V input + 更低 dropout）
- 或改用 ideal diode 代 D1201 → Vf 減到 30mV → GNSS_3V3 = 2.97V 持平

### B.2 Enable Sequence 詳解

**POR 默認**（基於 Sheet 7 B.4 的 AW9523 預設 LOW 行為）：
- `GNSS_LDO_EN` = LOW → GNSS 子板主 LDO 關閉
- `GNSS_VBAT_EN` = LOW → Q1202 OFF → Q1201 PMOS gate 由 R1204 100K 拉到 source → PMOS OFF → GNSS_VBAT = 0
- `MCU_GNSS_3V3` = ON（上游常開）→ 但 D1201 下游 GNSS_3V3 到子板，若子板 LDO EN=0，子板的 GNSS chip 其實未上電

**正常開機序列**（韌體負責）：
1. MCU 開機 → I2C 開 `GNSS_VBAT_EN` → 建立 GNSS_VBAT（保存星曆通道）
2. 等 10ms（VBAT 穩定）
3. I2C 開 `GNSS_LDO_EN` → 子板主 LDO 建立 → AG3352Q 開機（典型 100-500ms 到 first NMEA）
4. UART5 開始收 GNSS NMEA
5. 等 cold/warm/hot fix

### B.3 Cold / Warm / Hot Start 時間與功耗

**AG3352Q 典型效能**（提案書 P14 數字 + Airoha datasheet typ）：

| 狀態 | 條件 | TTFF 典型 | Operating current @ 3V3 |
|---|---|:-:|:-:|
| Cold start | 無星曆、無時間、無位置 | **35s** | ~72mA（提案書 P14）|
| Warm start | 有星曆 + 時間（<4hr 過期） | **5s** | ~72mA |
| Hot start | 有定位 + 星曆 fresh | **<1s** | ~72mA |
| Tracking | 已 fix | 持續 | ~45mA（典型，省點） |
| Standby (VBAT 保留) | VDD off, VBAT on | — | ~15µA |
| Full off | VBAT + VDD off | — | 0 |

**30 秒功耗**（cold start，提案書 P14 寫 4 秒 operating = **熱啟動**假設）：
- 若實際熱啟動（有星曆）：4s × 72mA = 288mA·s
- 若 **冷啟動**（無星曆，第一次開機或 VBAT 曾斷）：35s × 72mA = **2520mA·s** → 比熱啟動多 **8.75x 功耗**

**節能策略**：
- **絕對不要關 GNSS_VBAT**，否則每次 wake 都 cold start
- 深睡只關 `GNSS_LDO_EN`，保 `GNSS_VBAT_EN` → 下次 wake 是 warm start (5s)

### B.4 1PPS / EINT 訊號

**GNSS_DATA_IN_EINT**（J1201 pin 5 → MCU）：
- 多用途 pin：可能是 1PPS、也可能是 UART 資料到位中斷
- AG3352Q datasheet 顯示 `TIME_PULSE` pin 預設輸出 1PPS，可透過 NMEA 命令配置
- 本板 EINT 命名暗示作「中斷」用，而非 1PPS 計時

**行動**：階段 EVT 要確認 GNSS 子板上 AG3352Q 的 EINT pin 是接 TIME_PULSE 還是 WAKEUP interrupt。這個分辨會影響：
- 若是 1PPS → MCU 可做精確時間同步（±100ns 精度）
- 若是中斷 → 僅用於 wake MCU

### B.5 Q1201 PMOS 開關損耗

**YJL2305A** typical：
- Rdson @ Vgs=2.5V: ~**50mΩ**
- Rdson @ Vgs=4V: ~30mΩ

**GNSS_VBAT 典型 standby 電流 = 15µA**：
- Vds = 15µA × 50mΩ = **0.75µV**（可忽略）
- → GNSS_VBAT ≈ VDD_LDO_4V 幾乎無損

比 Sheet 2 的 2SC4617 BJT 高明太多（BJT Vce(sat) 100mV 損耗 vs PMOS 完全無損）—— 這進一步證明 **Sheet 2 應該學 Sheet 12 改 PMOS**。

### B.6 疑點更新

| 原疑點 | Stage B 結論 |
|---|---|
| GNSS_3V3 經 D1201 Vf 0.3V 後 3V marginal | **✅ 其實 OK**：AG3352Q min 2.8V, 3.0V 有 0.2V margin |
| 電池放電末期 GNSS_3V3 跌破 2.8V | **🟠 新發現**：電池 EOL 時 GNSS 會先 brown-out |
| GNSS 子板獨立 SCH 不在本 PDF | 仍待 Quectel 提供 |
| EINT = 1PPS or wake interrupt | **待 EVT 確認** |

Sources:
- [AG3352 series - Airoha Technology product page](https://www.everythingrf.com/products/gps-gnss-modules/airoha-technology/667-2688-ag3352-series)
