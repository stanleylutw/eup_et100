# 技術問題清單 — ET-100 Schematic Review

**對象 / To**：Quectel ODM 專案團隊
**提案 / Project**：Eupfin Tracker Device ODM Service (ET-100)
**SCH 版本審查 / SCH Reviewed**：QEM800QQA-FYN01AA_MB_SCH_V1.1_20260923_1630
**提案書對照 / Proposal Ref**：Eupfin_Tracker_Product_Proposal V1.8 (2026.08.28)
**提出者 / From**：EUPFIN Technology Co., Ltd.
**日期 / Date**：2026-10-02
**版本 / Rev**：V1.3（Markdown；HTML／PDF 尚未同步）

**修訂紀錄 / Revision History**：
| Rev | Date | Change |
|:-:|:-:|---|
| V1.0 | 2026-10-01 | Initial release (38 題) |
| V1.1 | 2026-10-01 | ① 修正 MCU vendor name: Fibocom → **Freqchip (富芮坤)** ② 新增 P0-FW 8 題（Q39-Q46）針對 Eupfin 自行開發韌體的前提條件 |
| V1.2 | 2026-10-02 | 依 Freqchip SDK 公開翻查結果：① **撤回 Q39、Q41、Q46**（SDK 公開可自取、GCC 支援齊、EVB 公開賣）② **新增 Q47（SVD 檔）、Q48（SRAM 容量矛盾）** ③ 修正 Q43、Q45 背景 |
| V1.3 | 2026-10-02 | 依 FR306x v0.4.9 修正 reserved pin、安全能力、OTA 與 Q48 memory map／revision／AEC 資格；保留問題編號。HTML／PDF 尚未同步，重產後才能寄送 |

> 規格來源：[FR306x v0.4.9 開發參考](../fr306x_reference/README.md)。此版只更新 Markdown；同名 HTML／PDF 不是本版內容。

---

## 一、說明 / Scope

本清單彙整 Eupfin 技術團隊對 ET-100 Schematic V1.1 完成內部 review 後，須請 Quectel 書面回覆的技術疑問。問題按**阻擋程度**分四級：

| 分級 | 定義 | 期望回覆時間 |
|:-:|---|---|
| **P0** | 阻擋 SOW 簽署 | 本次會議內口頭 + 10 工作天內書面 |
| **P1** | 阻擋 EVT 樣品 tape-out | 2 週內 |
| **P2** | 阻擋 DVT 樣品驗收 | 1 個月內 |
| **P3** | 資訊性 / 可與 EVT 報告一併回覆 | 隨 EVT 報告 |

每題後方附「**Quectel 回覆**」空欄，請直接 inline 填寫。

---

## P0 — 阻擋 SOW 簽署（10 題）

### Q01-P0 — LTE 模組 Band 覆蓋 vs 目標市場

**背景**：EG800Q-EU 支援 FDD B1/3/5/7/8/20/28，**不支援 TDD B40**。但 ET-100 目標市場包含：
- 泰國（AIS/DTAC 主用 B40 TDD）
- 馬來西亞（Maxis/Celcom 主用 B40 TDD）
- 印尼（Telkomsel/Indosat 主用 B40 TDD）
- 越南（主 FDD B1/3/5/7/8 — EG800Q-EU 覆蓋 ✓）
- 台灣（部分 TDD B38/41，FDD B1/3/8/28 — 部分覆蓋）

**問題 Q01-1**：Quectel 是否可提供 EG800Q-EU 在上述 5 國、逐電信商（Chunghwa / FarEasTone / Taiwan Mobile / Viettel / Vinaphone / MobiFone / Maxis / Celcom / U Mobile / AIS / DTAC / True / Telkomsel / Indosat / XL）的 band mapping 表？

**問題 Q01-2**：若需支援 TH/MY/ID 的 TDD B40，建議的替代方案是什麼？EG800Q-**GL** 版（B34/38/39/40/41 TDD）是否 pin-to-pin 相容 EU 版？Project stage？Lead time？價差？

**問題 Q01-3**：EG800Q-EU 為純 LTE Cat-1 bis，**無 2G/3G fallback**。客戶提案書 P04 原寫「2G/3G/4G 頻段」。若某區域 LTE 覆蓋不足時，裝置行為為何（entering emergency only / full loss of connectivity）？是否需加 2G/3G module 做備援？

**Quectel 回覆**：


---

### Q02-P0 — 量產時程

**背景**：客戶需求 P04 寫 MP = 2026 Q4；Quectel 提案 P47 寫 T0+9 個月 = 2027.03.31。差距 6 個月。

**問題 Q02-1**：若 T0 = 2026.10（SOW 簽署當月），Quectel 保證的 MP 日期是？是否含認證時間？

**問題 Q02-2**：若客戶堅持 2026 Q4 MP，需並行哪些 workstream？額外費用？是否可做 pre-cert 版本給 pilot 客戶？

**問題 Q02-3**：認證（NCC 8 週、BSMI 8 週、MIC 6-8 週、CE 4-6 週、NBTC 6-8 週、SIRIM 4-6 週）是**並行**或**序列**跑？

**Quectel 回覆**：


---

### Q03-P0 — 電池連接器 Pin 序

**背景**：SCH Sheet 8 J0801 WF15003-01207 連接器，圖面紅圈註記「电池 pin 序？」。pin 1/2/3 對應 B+/NTC/B- 的順序未定。

**問題 Q03-1**：J0801 的 pin 1、pin 2、pin 3 分別接電池的哪一極？

**問題 Q03-2**：建議的電池廠料號為何？UN38.3 認證誰持證？NTC 熱敏電阻規格（B 值、R25°C）？

**問題 Q03-3**：Sheet 8 的 R0806 VBAT_NTC pull-up 標為 NM，意思是出貨不裝 NTC 嗎？若量產保留 NM，UN38.3 / 1.2M 落摔 / MSDS 認證如何通過（通常需溫度保護）？

**Quectel 回覆**：


---

### Q04-P0 — 認證主體 / 持證方

**背景**：SCH 無法涉及，但 SOW 簽署前必定義。提案書 P49 標示 MIC「需實際進口商持證」、SIRIM「需實際進口商持證」、P53 又寫「需客戶確認是否由 Quectel 負責認證」。

**問題 Q04-1**：每國認證（NCC、BSMI、MIC/QCVN、SIRIM、NBTC、CE）的 **持證方 / 進口商 / 送測主體 / 樣品承擔 / 重測責任** 分別是？

**問題 Q04-2**：QCVN55 在提案書 P04 要求但 P49 認證表中消失（被 QCVN86 替代）—— QCVN55 vs QCVN86 的差異？是否確實可替代？

**問題 Q04-3**：CE (RED) 認證範圍排除「outdoor safety 測試 + SAR 測試」—— 車用是否可以不做 SAR？歐盟實務是否允許？

**問題 Q04-4**：泰國 DLT 認證誰負責？

**Quectel 回覆**：


---

### Q05-P0 — 機構 BOM 料號不齊

**背景**：SCH Sheet 9 圖面紅字「缺少定位孔（PTH）、屏蔽罩、PCBA、標籤紙、點膠位、夾具定位點（DET）的料號，目前不對」。

**問題 Q05-1**：下列料號何時可確定？
- H0901/H0904/H0905 屏蔽罩 ×3（BUCK/Charger/NFC）
- PCB0901 PCBA 絲印標籤 (QEM885EZA-CN01_PBT6_V1.2_PCB_TM_R1 是最終版？)
- LAB0901 標籤紙 (FC5851U_Label_GDL_R2)
- TP0916/TP0917 DET 夾具定位點
- HOLE0901-0906 PTH 定位柱

**問題 Q05-2**：客戶 BOM 要以哪個版本為最終 freeze 點？

**Quectel 回覆**：


---

### Q06-P0 — DCDC 型號不一致

**背景**：提案書 P25 寫 **JW5293**，SCH Sheet 6 用 **JW5357MSOTBWTR**。兩者都是 JoulWatt synchronous buck，但不同型號。

**問題 Q06-1**：最終量產用哪個？

**問題 Q06-2**：若 JW5357，提供 datasheet（本次 review 僅取得 bstelec 第三方版本）。

**Quectel 回覆**：


---

### Q07-P0 — JW5357 FB Divider 可能錯誤 🔴

**背景**：
- JW5357 datasheet Vref = **0.6V typ**
- SCH Sheet 6 標 R0614 = 10K、R0615 = **160K**
- 公式 Vout = 0.6 × (160/10 + 1) = **10.2V**
- 但設計意圖為 4V（圖面標題「5V 轉 4V 電源 DCDC」）
- 若量產板真燒出 10.2V，JW5357 Vin 也會爆（max 18V 但壓倒 output cap），下游所有 LDO 失效

**問題 Q07-1**：請確認 **R0615 的實際料號值**。是否應為 56K / 57.6K（配 10K 得 4V）？

**問題 Q07-2**：若 SCH 標籤錯誤，請提供修正後的 SCH V1.2 版本。

**Quectel 回覆**：


---

### Q08-P0 — LTE_VBAT BJT Vce 問題 🔴

**背景**：
- Sheet 2 Q0232 2SC4617 BJT 當 LTE_VBAT 開關
- EG800Q-EU VBAT min = **3.3V** (datasheet V1.0)
- VDD_LDO_4V 典型值：主電時 3.6V、電池時 3.4V
- 2SC4617 Vce(sat) @ Ic=1.5A ≥ 0.8V
- → LTE_VBAT 最差 = 3.4V - 0.8V = **2.6V < 3.3V 下限 700mV**
- 預期行為：LTE Tx 突波時 EG800Q brown-out / network detach

**問題 Q08-1**：Quectel 是否實測過此 BJT 配置下 LTE_VBAT 在 Tx burst 的實際壓降？

**問題 Q08-2**：建議將 Q0232 改為低 Rdson PMOS（例如 YJL2305A，Sheet 5/12 已在用）：
- Rdson ~50mΩ @ Vgs=2.5V → @ 1.5A 僅 75mV 損耗 → LTE_VBAT = 3.33V ✓
- 是否同意變更？BOM / layout 影響？

**Quectel 回覆**：


---

### Q09-P0 — Antenna Tuner Band 分配

**背景**：Sheet 13 MXD8544AE antenna tuner 由 EG800Q-EU 的 SDR_GRFC_1/2 兩線控制，4 state 選擇 4 條 RF matching path。提案書未提此 tuner。

**問題 Q09-1**：請提供 tuner 控制 state (00/01/10/11) 對應的 band matching 分配表。

**問題 Q09-2**：Tuner 開啟後 LTE 各 band 的 TRP/TIS 預估值？特別是提案書 P33 指出 600-960MHz 效率僅 20-40% 的改善幅度？

**問題 Q09-3**：Tuner 控制時機（LTE module 內建演算法自動切，還是韌體層面手動配置）？

**Quectel 回覆**：


---

### Q10-P0 — GNSS 子板獨立 SCH

**背景**：Sheet 12 僅畫主板到 GNSS 子板的 FPC 介面；AG3352Q 本體、天線、LNA/SAW 等皆在 GNSS 子板獨立 SCH。本次 review PDF 不含子板。

**問題 Q10**：請提供 GNSS 子板獨立 SCH + layout 概覽（含天線）。

**Quectel 回覆**：


---

## P1 — 阻擋 EVT 樣品 tape-out（11 題）

### Q11-P1 — ACC_INT_OUT 5 支共用 Driver

**背景**：Sheet 11 Q1110 2SC4617 單一 BJT collector 對外推 5 支 ACC_INT_OUT（J0901 pin 14/16/18/22/26 給 4 組 RS232/CAN 子線束 + 1 支備援）。任一下游短路到 GND 會把 5 支全部拉死。

**問題 Q11-1**：建議改為每支獨立 fuse 或獨立 driver，Quectel 是否同意？

**問題 Q11-2**：若維持現狀，每支 pin 的最大對外負載電流規範是多少？韌體要如何偵測/保護？

**Quectel 回覆**：


---

### Q12-P1 — CAN STB 經 I2C IO Expander 的延遲

**背景**：Sheet 5 SIT1042 的 STB pin 由 Sheet 11 AW9523 的 P1_8 控制（經 I2C bus）。wake from standby 的延遲 =（I2C transaction 時間）典型 1-5ms。J1939 診斷服務要求 STB low 到 bus 可用 <10µs，可能漏接首幀。

**問題 Q12**：建議 MCU_CAN_STB 由 MCU GPIO 直控、不經 AW9523，可行？

**Quectel 回覆**：


---

### Q13-P1 — 一級 DCDC 熱裕量

**背景**：
- 車內 ambient 85°C 要求（提案書 P06）
- U0601 SGM61630 @ Vin 36V、Vout 5V、Iout 1.5A（LTE Tx + 充電 + NFC 同時）→ loss ~1.4W
- SOIC-8-EP 熱阻 60°C/W → Tj = 85 + 1.4×60 = **169°C 超 Tj_max 125°C**

**問題 Q13-1**：Quectel 是否做過 85°C 持續滿載 thermal simulation？Max continuous Iout 預估？

**問題 Q13-2**：layout 的 ground copper 面積 vs. 散熱預估？是否需 heat spreader 或下接 GND via array？

**問題 Q13-3**：高溫情境下是否要降 LTE Tx duty cycle、charger 電流以保 Tj？

**Quectel 回覆**：


---

### Q14-P1 — Charger VIN 來源

**背景**：Sheet 8 U0801 YX4066 VIN 標 `VDD_5V`。但 Sheet 6 的 ORing 將 VDD_5V 分成 _MCU (3A) 和 _PP (1A)。VDD_5V 可能指 ORing 前原節點、_MCU 或 _PP。

**問題 Q14-1**：Charger VIN 實際接到哪一條 rail？

**問題 Q14-2**：若接 _PP（1A rail）：Charger 充電 1A + CAN 100mA + NFC 讀卡 200mA + RS232 50mA = **~1.35A**，超過 1A 預算。預期行為？

**Quectel 回覆**：


---

### Q15-P1 — 命名錯誤：VDD_PP_3V3 實際 ≈ 5V

**背景**：Sheet 5 Q0501 PMOS 從 VDD_5V_PP (5V) 開關到下游 UM3221 VCC。輸出 net 命名為 `VDD_PP_3V3`，但實際載 5V 而非 3.3V。

**問題 Q15**：請修正 net name（例如 `VDD_PP_SW` 或 `VDD_RS232_5V`）避免後續 Firmware/Test team 誤讀。

**Quectel 回覆**：


---

### Q16-P1 — Sheet 6 Title Block 錯誤

**背景**：Sheet 6 的 title block 顯示「SHEET 15 OF 15」，應為「SHEET 6 OF 15」。EDA 匯出資料可能對不上。

**問題 Q16**：請修正並重發 SCH V1.2。

**Quectel 回覆**：


---

### Q17-P1 — Sheet 2 TP0208 LTE_USB_BOOT Pull-down

**背景**：Sheet 2 LTE_USB_BOOT 由 TP0208 引出，未見 pull-down 到 GND 的 R。EG800Q 內部若 pull-up 到 1V8，開機瞬間可能被誤判為 download mode。

**問題 Q17**：TP0208 是否需加 pull-down 100K 到 GND？

**Quectel 回覆**：


---

### Q18-P1 — Snubber C0603 數值

**背景**：Sheet 6 SW node 標 C0603 = 22nF/50V snubber。若真為 22nF，500kHz 下純 C snubber 損耗 = ½ × C × V² × f ≈ 7W，會直接燒 PCB。

**問題 Q18-1**：C0603 實際是 22nF 還是 22**p**F？

**問題 Q18-2**：若為 snubber，是否需配 series R（典型 RC snubber）？

**Quectel 回覆**：


---

### Q19-P1 — Snubber Reference Design 功能

**背景**：舊 SCH 分析記錄 pin 12 RESV0 接 R0110 1M 到 GND；v0.4.9 確認 pin 名稱，但未規定外部上下拉。請核對阻值、DNP、silicon revision 與 hardware guide，不能僅因 reserved 名稱判定接法安全。

**問題 Q19**：R0110 1M 下接的 意圖是否符合 Freqchip reference design？若 datasheet 建議 NC 則移除。

**Quectel 回覆**：


---

### Q20-P1 — AG3352Q 實際料號 / Datasheet

**背景**：提案書 P21 寫 AG3352Q（Airoha），本次 review 在公開管道僅取得簡述。GNSS 子板 SCH 未提供（見 Q10）。

**問題 Q20-1**：AG3352Q 的 VDD 範圍（min/typ/max）、current @ various mode 的 datasheet 完整版？

**問題 Q20-2**：AG3352Q 的 TIME_PULSE pin（1PPS）是否接到 FPC J1201 pin 5 GNSS_DATA_IN_EINT？

**Quectel 回覆**：


---

### Q21-P1 — PWRKEY 驅動方式

**背景**：EG800Q HW Design 要求 PWRKEY 用 **open-collector** 驅動，拉低 ≥500ms 開機。SCH Sheet 2 中 PWRKEY 由 MCU 直接驅動或經 BJT 不明確。

**問題 Q21**：請確認 PWRKEY 的實際 drive 架構。若是 MCU GPIO 推挽直 drive，VIL 可能無法達 EG800Q 的 0.5V 要求。

**Quectel 回覆**：


---

## P2 — 阻擋 DVT 樣品驗收（10 題）

### Q22-P2 — 功耗表重出

**背景**：提案書 P14 功耗評估表有多處矛盾：
- discharge efficiency 標 70% vs 下方算式用 80%
- LTE Start up / Searching network 的 Active duration 都寫 **0 秒**
- 1554mWh × 0.7 / 371.8mW = 2.92h；× 0.8 = 3.34h，**都不是標的 4.2h**

**問題 Q22**：請重出功耗評估表，含：
- (a) 統一 discharge efficiency
- (b) LTE start-up / search / weak-signal 三種情境的實測 Active duration
- (c) 三組回報頻率情境（15s / 60s / 300s）的續航
- (d) GNSS hot/warm/cold start 分別耗電
- (e) 電池 300 cycles 後衰減

**Quectel 回覆**：


---

### Q23-P2 — 30 天本地儲存驗收規格

**背景**：
- 32MB flash / 240 筆/小時 × 24 × 30 = 172,800 筆
- 單筆上限 ~115-140 bytes（扣 wear leveling 後更少）
- CAN J1939 payload 可達 50-100B，若全存必爆

**問題 Q23**：請定義：
- (a) 單筆 payload 最大 size
- (b) 壓縮方式（有無）
- (c) 循環覆寫策略
- (d) 斷網重傳 queue 順序
- (e) wear leveling 演算法與預期 cycle 壽命
- (f) CAN 全存 vs filtered 的策略

**Quectel 回覆**：


---

### Q24-P2 — EVT 天線交付物清單

**問題 Q24**：EVT 必交下列報告：
- (a) 各頻段 TRP/TIS（conducted + radiated）× tuner 各 state
- (b) GNSS CN0 + TTFF（cold/warm/hot）vs 商用參考機
- (c) NFC 讀距（MIFARE 1K、ICODE SLI、ISO14443A/B 四種卡）
- (d) BT RSSI @ 1m / 5m / 10m
- (e) 天線間 isolation
- (f) **整機裝殼固定於車體後**的實車 OTA 測試

Quectel 是否同意納入 EVT 交付範圍？

**Quectel 回覆**：


---

### Q25-P2 — NFC 匹配電容替代料 ESL 影響

**背景**：Sheet 14 圖面註記「750pF 没有，暂用 68pF + 680pF, 5%, 100V, 0603」。

**問題 Q25**：68+680pF 等效 748pF 數學 OK，但 ESL 不同。EVT 必測實際讀距，若 degrade 需換 single 750pF 料。Quectel 是否接受此規格變更條件？

**Quectel 回覆**：


---

### Q26-P2 — 反接保護

**背景**：Sheet 9 板端無 series PFET 或 schottky 做 polarity protection。若線束端 ETL 失效，9-36V 接反可能損壞 Sheet 6 DCDC input FET。

**問題 Q26-1**：Quectel 是否假設反接保護完全依賴線束端？

**問題 Q26-2**：是否建議板端加 PFET-based reverse protection？

**Quectel 回覆**：


---

### Q27-P2 — Load Dump TVS Clamp Voltage

**背景**：Sheet 9 D0901 PSBDAF60V3 TVS = 60V Vrwm。ISO 7637-2 Pulse 5A (load dump) 要求 clamp 到 86V max，60V TVS 的 Vclamp 可能 >75V，接近標準上限。

**問題 Q27**：Quectel 是否模擬過 ISO 7637-2 Pulse 5A 下 D0901 的實測 clamp？是否需升到 70V-80V TVS？

**Quectel 回覆**：


---

### Q28-P2 — RS232 Pin 24 共用 RXD0/RXD1 疑慮

**背景**：J0901 pin 24 在 SCH 中似乎被 RS232_RXD0 與 RS232_RXD1 共用（也可能是標籤重複讀誤）。

**問題 Q28**：請提供 J0901 完整 pin 分配表（含每 pin 對應的 RS232 子線束）。

**Quectel 回覆**：


---

### Q29-P2 — EG800Q-EU USB Bootloader 安全

**背景**：Sheet 2 USB debug port 可透過 USB 燒 LTE 韌體。量產出廠後若有人開殼接 USB 可能可以重刷 LTE 韌體。

**問題 Q29**：EG800Q-EU 是否有 fuse 選項禁用 USB bootloader？若有，是否要在量產時啟用？

**Quectel 回覆**：


---

### Q30-P2 — SIM 熱插拔壽命

**背景**：V1.8 客戶需求強調 SIM 翻蓋式 + 熱插拔。nano-SIM 金手指壽命典型 ~1000 次 insertion/removal。

**問題 Q30**：翻蓋 SIM holder MUP-C783-1 的 insertion life cycle spec？若車隊換手頻繁可能不夠。

**Quectel 回覆**：


---

### Q31-P2 — Reserved Pin 核對

**背景**：EG800Q-EU 108 pin 中 >40 支 RESERVED。本 SCH 全部 NC，階段 B 判斷符合 reference design。

**問題 Q31**：請提供 EG800Q-EU reference design，確保 RESERVED pin 處理（NC vs pull-down vs GND）全部符合。

**Quectel 回覆**：


---

## P0-FW — Eupfin 自行開發韌體前提條件（V1.2：**5 題必答** + 3 題降級）

**背景**：依 2026-10 新達成共識，**Eupfin 將與 Quectel 平行開發自家韌體（customer-fork firmware）**。Quectel 不交付 FW source 給 Eupfin。V1.2 更新：Eupfin 已自行取得 SDK 並確認 GCC 可行後，Q39/Q41/Q46 降為 P1-P3；但 **Q40/Q42/Q43/Q44/Q45/Q47/Q48 仍是 P0 必答**（已改番號為 Q42/Q43/Q44/Q45 + 新 Q47/Q48，共 7 題）。

### Q39-P0-FW — Freqchip FR3068E-C BSP Source / Library 取得 ~~[WITHDRAWN V1.2]~~

**V1.2 撤回原因**：Eupfin 已於 2026-10-02 透過社群公開管道（gitee.com/qinyunti/fr3068-e-c-micropython）取得 **fr30xxc_sdk__202411**，含完整 driver source、GCC Makefile 範例、以及 BLE binary library（`libbtdm_host.a`，標準 GNU ar archive 可 link）。暫時不需要 Quectel 牽線。

**但以下仍需 Quectel 書面確認（降級為 P1）**：

**問題 Q39-1 (改 P1)**：社群 bundled 版本（fr30xxc_sdk__202411）與 Quectel 內部用的 SDK 是否同一版本？若不同，差異為何？

**問題 Q39-2 (改 P1)**：BLE stack `libbtdm_host.a` 的 **Bluetooth Qualification（BQB QDID）** 由誰持有？Eupfin 以此 stack 出的量產韌體能否沿用現有 QDID、或需重新認證？

**Quectel 回覆**：


---

### Q40-P0-FW — EVT 樣品 Flash 空白 / JTAG unlock

**問題 Q40-1**：EVT 樣品板出貨時是否 flash **完全空白**（而非預燒 Quectel 版本的 FW）？

**問題 Q40-2**：若預燒 Quectel FW，Eupfin 要如何清掉並燒自家 FW？Quectel 是否提供 unlock / mass erase 程序？

**問題 Q40-3**：SWD / JTAG debug 介面在 EVT 板上是否永久可用（量產板是否 lock debug）？

**Quectel 回覆**：


---

### Q41-P0-FW — 燒錄工具 / Programming Flow ~~[WITHDRAWN V1.2]~~

**V1.2 撤回原因**：SDK 內附 Keil FLM（`components/tools/keil/FR30xx.FLM`）+ JLinkDevices.xml，JLink 原生支援。另有原廠 `FreqChip_Download` serial 燒錄工具。開發期燒錄已確認：J-Link / DAPLink + OpenOCD 皆可。

**降級為 P2，僅需答一題**：

**問題 Q41 (改 P2)**：量產產線用的 ATE 燒錄工具與開發期一致嗎？若有專屬治具，是否會影響 Eupfin 自家 FW 燒錄流程？

**Quectel 回覆**：


---

### Q42-P0-FW — Bootloader / Secure Boot / Signing Key

**背景**：v0.4.9 p.8 列 2Kb Efuse、AES128/192/256、TRNG，但未定義 secure boot／簽章驗證／debug lock／key provisioning。這些硬體模組不足以證明 ROM 已實作 secure boot；若板上 boot 流程要求受信任簽章，Eupfin 必須取得可用的簽署與量產授權流程。

**問題 Q42-1**：Quectel 的 FW 是否啟用 secure boot？signing key 由誰持有？

**問題 Q42-2**：Eupfin 版 FW 是否能共用同一條 signing key？若否，Eupfin 能否持有獨立 key？

**問題 Q42-3**：若 secure boot 啟用，Efuse 燒錄時機（EVT / DVT / PVT / MP？）？是否可選擇不啟用？

**問題 Q42-4**：MP 樣品預設狀態是否「secure boot off，可燒自家 FW」？

**Quectel 回覆**：


---

### Q43-P0-FW — OTA Framework / Dual-Bank

**背景**：提案書 P45 列 OTA 為軟體功能。v0.4.9 p.7 列 FR3068E-C **2 MB Flash**；SDK linker 僅配置 **1016 KiB app window @0x08002000**，不是晶片全部容量或 bootloader 大小的證明。SDK BLE 範例已有 OTA service 與 post_process.py 封裝產物，但板上 bootloader 接受格式、image swap 與 rollback 尚未驗證。雙 bank 的單 image 上限須扣 boot／metadata／config／erase alignment 後計算，不是需大於半個 Flash；也不能直接按 2 MB 擴大分區。

**問題 Q43-1**：Freqchip SDK 是否提供現成的 OTA framework（bootloader + image swap + rollback）？若有，位於 SDK 哪個目錄？

**問題 Q43-2**：建議的 flash partition layout（bootloader / app A / app B / config / log）？是否有 reference partition table？

**問題 Q43-3**：OTA image signature verification + rollback protection 的實作建議？Quectel 自家 FW 的 OTA 流程圖？

**問題 Q43-4**：Over LTE 的 OTA（HTTPS download）vs over BLE 的 OTA（Nordic DFU 類似）—— 兩種都要支援嗎？

**Quectel 回覆**：


---

### Q44-P0-FW — Antenna Tuner MXD8544AE Control Protocol

**背景**：Sheet 13 MXD8544AE 由 LTE 模組 SDR_GRFC_1/2 控制（見 Q09）。但若 Eupfin FW 要自己管理 band select、power saving，必須理解控制時序。

**問題 Q44-1**：SDR_GRFC_1/2 是 LTE module 內部自動切（Qualcomm QCX216 modem firmware 管理），還是需要 host MCU 透過 AT 命令設定？

**問題 Q44-2**：若自動管理，Eupfin FW 需做任何事嗎？

**問題 Q44-3**：若需手動管理，提供 AT command 清單 + 時序圖。

**Quectel 回覆**：


---

### Q45-P0-FW — Quectel FW Behavior Reference

**背景**：Quectel 不交付 source，但 Eupfin FW 須產出與 Quectel FW 行為相似的輸出（相同 BLE adv、相同 LED 行為、相同 OTA 流程、相同 cloud protocol）才能相容既有 cloud server / mobile app。此需求的優先序比 Q39-Q44 更關鍵 —— SDK 公開只解決「如何寫 code」，但不解決「寫什麼 behavior」。

**問題 Q45-1**：Quectel FW 的以下行為 spec 能否以文件形式提供：
- (a) BLE advertising packet 格式與頻率
- (b) 開機 LED 自測序列（Driver / Memory / GPS / Net 的 blink pattern）
- (c) 蜂鳴器告警 pattern 對應事件
- (d) OTA validation / rollback 行為
- (e) G-Sensor event thresholds（典型 shock / tilt / motion）
- (f) 低功耗模式 state machine（entry / exit 條件）

**問題 Q45-2**：Quectel FW 的 cloud protocol 是否使用「EUP 終端設備通訊協定」？若是，提供 spec 文件。

**Quectel 回覆**：


---

### Q46-P0-FW — Freqchip FR3068E-C EVB 直接購買 ~~[WITHDRAWN V1.2]~~

**V1.2 撤回原因**：Freqchip FR3068x-C 低功耗開發板已於 mbb.eet-china.com 等公開評測，Eupfin 可自行採購，無需 Quectel 介入。

**降級為 P3，僅保留一題做 reference**：

**問題 Q46 (改 P3)**：Quectel 自家 FR3068 開發所用的 EVB 型號是否為 Freqchip 官方 FR3068E-C EVB V1.0？若有變體或客製版本，請告知差異。

**Quectel 回覆**：


---

### Q47-P0-FW — FR3068E-C SVD 檔（Peripheral Register View）

**背景**：Freqchip SDK 公開版無 `.svd` 檔，debug 時 VS Code Cortex-Debug 的 "Peripheral Register View" 無法顯示 register 名稱與 bitfield，只能靠 `fr30xx.h` 的 struct typedef 手對地址。對 bring-up 階段 debug 效率影響大。

**問題 Q47-1**：原廠（Freqchip 或 Quectel 內部）是否有 FR3068E-C 的 SVD 檔？若有，提供給 Eupfin。

**問題 Q47-2**：若無，Freqchip 是否有 Peripheral Description Database（PDB）可以用腳本轉成 SVD？或 Register View GUI 工具？

**Quectel 回覆**：


---

### Q48-P0-FW — FR3068E-C Memory Map／SDK 適配／料號 Revision 確認

**背景**：
- 提案書 P19 的 Flash 描述不一致；原廠 v0.4.9 p.7 明列 **2 MB Flash、512 KB SRAM、2 組 CAN、QFN80 9×9 mm，AEC-Q100「否」**。
- SDK 202411 的 `ldscript.ld`／`ldscript_3068e.ld` 配置 **Flash 1016 KiB @0x08002000、SRAM 256 KiB @0x20000000、PRAM 128 KiB @0x1FFE0000**；與總容量的 bank／reserved 對應未確認，不能自行相加或擴大。
- v0.4.9 p.30 記載 v0.4.4 修改 FR3068E-C pin；舊 SCH 分析與新版有多處腳號差異，尚不能判定原理圖或晶片版本有錯。

**問題 Q48-1**：請提供板上 exact part／silicon revision 對應的完整 Flash／SRAM memory map（bank、reserved、ROM／boot、cache／BT 使用區），說明 2 MB／512 KB 如何對應 SDK window，並提供適配的 SDK／linker／startup 版本。

**問題 Q48-2**：PRAM（128 KiB @0x1FFE0000）與 512 KB SRAM 的關係為何？哪些區域可給 heap／data／RAM code／DMA，哪些有 retention、cache coherency 或存取限制？

**問題 Q48-3**：現有配置下 FreeRTOS heap／task stack、BLE buffer 與 app 的建議預算為何？啟用其他 bank 需哪些初始化與 linker 設定，如何驗證安全？

**問題 Q48-4**：請核對 SCH symbol／PCB footprint 與 board-matched pin-change errata，提供 full pinmux、逐 GPIO voltage bank、SWD／VTref、reset defaults、1V8／3V3 sequencing 與 unpowered IO 規範。

**問題 Q48-5**：請確認實際採購料號與 AEC-Q100 資格；若要求車規，提供 exact part 的 qualification 資料並說明與 v0.4.9 p.7「否」的差異，不能以系列 Grade 2 或工作溫度代替證明。

**Quectel 回覆**：


---

## P3 — 資訊性 / EVT 報告時回覆（7 題）

### Q32-P3 — 系統框圖的 EG800Q-GL/EU 註記

**背景**：Sheet 15 reference block diagram 寫「EG800Q-GL/EU」，Sheet 2 BOM 是 EU。

**問題 Q32**：若未來換 GL 版（Q01 相關），SCH 需做哪些修改？

**Quectel 回覆**：


---

### Q33-P3 — AW9523 I2C 位址

**問題 Q33**：AW9523 的 AD0/AD1 pin 都接 GND，I2C 位址 = ?（0x58？請提供最終位址）

**Quectel 回覆**：


---

### Q34-P3 — PN7160 I2C 位址

**問題 Q34**：PN7160 ADR0/ADR1 pin 的接法 → I2C 位址？

**Quectel 回覆**：


---

### Q35-P3 — SC7U22TR Datasheet

**問題 Q35**：請提供 G-Sensor SC7U22TR 的 datasheet（原廠為 Silan Microelectronics？賽昇？），含：
- VDD / VDDIO 範圍
- Typical active / sleep current
- I2C 位址
- FIFO 深度

**Quectel 回覆**：


---

### Q36-P3 — RAS2442A35-MC BT Switch 用途

**背景**：Sheet 13 RAS2442A35-MC SPDT RF switch 用在 BT 側。本板只接一條鋼片 BT 天線，SPDT switch 的實際用途不明。

**問題 Q36**：此 switch 是用於 BT TX/RX chain select、antenna diversity、還是其他？

**Quectel 回覆**：


---

### Q37-P3 — 1V8 UART Debug Bridge 建議料

**背景**：Sheet 2 DBG_TXD/RXD 為 1V8 level。工廠需使用 1V8 UART-USB bridge。

**問題 Q37**：Quectel 建議的工廠 debug tool 料號？

**Quectel 回覆**：


---

### Q38-P3 — Shielding Cover 的 Opening Points

**背景**：Sheet 9 標 3 顆 shielding cover（BUCK/Charger/NFC）。EMC 測試時可能需開蓋量測。

**問題 Q38**：shielding cover 是焊死還是夾持式？EMC 工程師是否可打開再焊回？

**Quectel 回覆**：


---

## 二、附件 / Attachments

- 本次 review 底稿：QEM800QQA-FYN01AA_MB_SCH_V1.1_20260923_1630.pdf
- Eupfin 完整 review MD：[eup_et100/docs/03_hardware/sch_analysis/](.) （含 15 份 Sheet 分析、power tree、net inventory、risk cross-reference）

---

## 三、Priority Summary Table / 優先級彙總

| 等級 | 題數 | 回覆時限 | 阻擋 |
|:-:|:-:|---|---|
| P0 | 10 | SOW 簽署前 | SOW |
| **P0-FW** | **7**（Q40/Q42-Q45/Q47/Q48） | **SOW 簽署前** | **Eupfin FW 專案關鍵能力** |
| P1 | 11 + Q39（降級） = **12** | 2 週內 | EVT 板 tape-out |
| P2 | 10 + Q41（降級） = **11** | 1 個月內 | DVT 樣品 |
| P3 | 7 + Q46（降級） = **8** | 隨 EVT 報告 | — |
| **合計** | **48** | | |

---

## 四、回覆方式 / How to Reply

請在每題「Quectel 回覆」欄下方 inline 填寫，並於封面標註回覆版本（V1.1、V1.2…）。
如個別題目需要 attachment（datasheet、simulation report、reference design PDF），請於該題標註「見 Attachment X」並附於信件。

有任何題目需 Eupfin 補充前提或釐清，請直接寫於該題下方「Eupfin 補充」欄即可。

---

**END OF DOCUMENT**
