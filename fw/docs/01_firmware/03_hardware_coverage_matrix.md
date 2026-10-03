# ET-100 硬體覆蓋與驗證矩陣

日期：2026-10-02。版本：v0.1 DRAFT。搭配 [架構](02_firmware_architecture_draft.md)／[執行計畫](04_development_execution_draft.md)。這是開發 reference，不是最終 pin assignment。

來源：[客戶需求 P04/P05](../../../docs/00_project/02_客戶需求.md)、[P10-P14 系統設計](../../../docs/00_project/03_技術方案_系統設計.md)、[P44/P45 軟體與線束](../../../docs/00_project/06_技術方案_線束與軟體.md)、[SCH 索引](../../../docs/03_hardware/sch_analysis/00_索引.md)、[Datasheet gaps](../../../docs/03_hardware/fr306x_reference/06_gaps_and_revision_history.md)。舊產品 P06-P08 的數值不自動成為 ET-100 驗收條件。

## 1. 全功能清單

狀態統一：`HOST` 僅 host example；`PLAN` 未實作；`BLOCKED` 特定板級行為需先確認。所有功能目前都沒有 ET-100 實機 PASS。階段代號見執行計畫。

| ID | 功能／來源 Sheet | Logical interface／owner | 前置確認 | 驗收證據／階段 |
|---|---|---|---|---|
| H01 | MCU clock/reset/memory / 1 | SDK system + board init；HOST | Revision、SWD/VTref、map、wait states | Cold boot、core frequency、upper-bank／retention、reset cause；M1 |
| H02 | AW9523 / 11 | `I2C_EXPANDER` + power owner；PLAN | Address、16 IO 對應、POR direction/value | ID／readback、reset、輸出 safe state、bus fault；M2 |
| H03 | Rail/PMU / 6/7/15 | Power service；BLOCKED | Shared domains、backfeed、wake/retention | Rails waveforms、quiesce/resume、故障安全、sleep current；M2/M6 |
| H04 | 32 MB 外部 NOR / 1 | `FLASH_EXTERNAL` + storage；PLAN | Exact ID、geometry、addressing、routing、safe scratch | Page/sector、16 MiB boundary、highest sector、power interruption；M2/M4 |
| H05 | LTE EG800Q / 2 | `UART_LTE` + modem；PLAN | Exact variant/firmware、power/reset/PWRKEY、polarity/flow control | AT/URC/binary transfer、弱網重連、bounded recovery；M3 |
| H06 | SIM 插拔 / 3 | Modem-owned USIM，MCU 經 AT/URC；PLAN | DET route/polarity、hot-plug 支援配置 | 無卡、PIN、插拔、重新註网、不 crash；M3 |
| H07 | GNSS AG3352Q 子板 / 12 | `UART_GNSS` + position；PLAN | 子板 SCH、baud、enable/backup、EINT meaning | Valid/invalid NMEA、cold/warm/hot fix、age/quality；M3 |
| H08 | BLE 設定/telemetry / 1/13 | SDK host + command router；HOST | App protocol、UUID、security、RF switch | Adv/connect、授權讀寫、reconnect、並行負載；M1/M4 |
| H09 | PN7160 NFC / 10/14 | `I2C_NFC` + NFC host transport；PLAN | Variant/firmware、host library、address/IRQ/VEN、target cards | Target card matrix、presence/remove、read errors、driver session；M3 |
| H10 | SC7U22 6-axis / 3 | `I2C_SENSOR` + IO worker；BLOCKED | Datasheet/register map、INT、FIFO、rail retention | 六軸校驗、motion/shock repeatability、wake、queue overflow；M3/M6 |
| H11 | CAN/J1939 / 5 | `CAN_VEHICLE` + CAN worker；BLOCKED | Controller/pinmux、STB routing、VIO、termination、PGNs | Loopback/listen-only、filters、bus-off、known PGN vectors；M3/M4 |
| H12 | RS232_0 / 5/9 | `UART_RS232_0` + line driver；PLAN | RX/TX routing；115200 候選 | Loopback/framing、EN、資料量、與 console 無衝突；M3 |
| H13 | RS232_1 / 5/9 | `UART_RS232_1`；BLOCKED | J0901 RXD0/1 conflict；9600 候選 | 獨立雙向收發、不 cross-talk；M3 |
| H14 | RS232_2 / 5/9 | `UART_RS232_2`；PLAN | Pinmux、EN；9600 候選 | 獨立雙向收發及 sleep 恢復；M3 |
| H15 | ACC + DI1..3 / 9/11 | 4 logical inputs + vehicle；PLAN | Levels/polarity、interrupt/wake、debounce | 外部 fixture、threshold/hysteresis、bounce、event timestamp；M2/M3 |
| H16 | 1 個外部 DO / 9/11 | Safe output service；BLOCKED | 負輸出 vs PFET switched positive branch 衝突 | Dummy load、reset/boot/sleep/fault default；未確認前禁止啟動；M2 |
| H17 | ACC_OUT fanout / 9/11 | 一個 logical output；BLOCKED | Connector pin 數／共用 collector、電流、polarity | Approved dummy loads、共同切換及故障隔離限制；M2 |
| H18 | 油量 analog input / 1/9/11 | ADC adapter + calibration；BLOCKED | External 0-33V 與 MCU range/divider、Vref/acquisition | 限流外部 fixture校正、誤差、filter、open/short；M3 |
| H19 | VBAT ADC / 1/8 | Battery monitor；PLAN | Divider/source impedance、ADC settling、voltage limits | 與 DMM 對照、loaded battery、low-voltage state；M3/M6 |
| H20 | NTC / 8 | Optional battery temperature；BLOCKED | NM 是否裝件、R25/B、charger thermal safety | 不存在則 unavailable； populated後 calibration/fault；M3 |
| H21 | 4 個 1-Wire probes / 9/11 | Timer + separated RX/TX + IO worker；BLOCKED | Inversion、probe type、parasite/strong pull-up、cable | 4 ROM IDs、CRC、disconnect、長線及 concurrent BLE；M3 |
| H22 | 4 LED / 4 | Pattern scheduler + GPIO；PLAN | Physical color/order/polarity、rail、behavior spec | 各燈 mapping、state patterns、nonblocking、sleep；M2/M4 |
| H23 | Buzzer / 4 | Cadence scheduler + enable；PLAN | Active buzzer BOM、drive、rail delay | On/off patterns、power transitions、無阻塞；M2 |
| H24 | Charger + battery backup / 7/8 | Power/battery service；BLOCKED | Battery pinout/chemistry、CHRG/FULL truth table、NTC、rail safety | 專用 battery fixture、安全充電、主電掉線／恢復；M3/M6 |
| H25 | RTC/time sync / MCU/12 | Time service；PLAN | RTC calibration/retention；EINT 非已證明 PPS | Monotonic、UTC valid/invalid、跨 reboot/clock correction；M1/M4 |
| H26 | Watchdog/debug/diagnostics / MCU | Health + diagnostic transport；PLAN | SDK watchdog/sleep、approved debug interface | 卡死注入、reset reason、rate limits、production permissions；M1/M6 |
| H27 | LTE RF tuner / 13 | Modem control，不預設 MCU GPIO；BLOCKED | GRFC/AT ownership、band-state map | Vendor procedure + RF report，不能靠 tuning 補 unsupported bands；M6 |
| H28 | BT switch / 13 | SDK RF config 或 approved board binding；BLOCKED | Switch purpose/control/polarity | Approved RF procedure + BLE radiated test；M1/M6 |
| H29 | NFC antenna / 14 | NFC service diagnostic hook；PLAN | Matching、卡種/距離目標、enclosure | 裝殼 card matrix／RF tests；不是新增 GPIO driver；M3/M6 |
| H30 | Reset/button/test points / 1/9 | Board/production diagnostics；BLOCKED | Reset route；button 是否真的 populated、ATE fixture | Reset/recovery、authorized self-test、version/ID readout；M1/M6 |

跨硬體功能：雙 transport OTA、EUP protocol、remote config、30 天 logging、重傳、alert/driver-session 見架構與 M4/M5。LTE 的 USB test points 不自動成為 MCU USB 產品介面；SD／audio／display／PSRAM 不因 SDK 有 driver 就加入 scope。

## 2. 不能照抄的文件矛盾

| ID | 原記錄／問題 | Draft 處理與解除方式 |
|---|---|---|
| B01 | 5 UART / 3 I2C 規格與筆記 UART0..5、I2C0/3/5 labels | 先 logical binding；取得 full pinmux／exact revision，不推定第六 UART 或 I2C5 硬體 |
| B02 | AW9523「16 IO」卻列 `P1_8` 為 CAN_STB | 表格無法直接映射完整有效 bit；取得 netlist 與 exact pin table，不能 encode 成 bit 8 |
| B03 | 客戶 J0901 pin10 負輸出；筆記卻描述 Q1108 PFET positive switching | 輸出預設不啟動；Quectel 書面确认實際 circuit/load/polarity |
| B04 | J0901 RXD0/RXD1、ACC_OUT 4/5 pin 數前後不同 | 連接器／線束 continuity mapping；共享 net 只算一個 channel |
| B05 | 舊 physical pin table 與 datasheet v0.4.9 多處差異 | 按 silicon revision/SCH symbol/PCB 核對，不直接拿新 pin 表重接 SWD |
| B06 | `NFC_1V8`／`VDD_3V3` 與 MCU／IMU 共用；筆記關掉 rail 又要求 motion wake | 建立 rail dependency；不能關掉 wake sensor 的供電 |
| B07 | `VDD_PP_3V3` 真實電壓／CAN VIO 未定；DCDC divider 記錄矛盾 | 限流上電量測、BOM與正式SCH確認；FW不能補救不符電氣額定 |
| B08 | NTC 為 NM；LTE wake signal 位在 PP pin 卻被列成 ADC channel | 不啟用缺件 sensor；先判斷 signal role，digital wake 不當 analog measurement |
| B09 | GNSS EINT 被推定為 1PPS；LTE BJT shift 被推定必須 UART invert | 取得子板／level-shift topology、示波器量測，不先選 polarity 或 PPS precision |
| B10 | SDK NOR 範例使用 EVB GPIO、quad 與三-byte address | Board-specific adapter + full-capacity boundary test；不能直接重用稱 32 MB 支援 |
| B11 | 原草案 task priority 10/11；SDK max=10 | 保留 SDK host/RPMsg，以 0..9 合法值規劃並做 latency/resource 測量 |
| B12 | 原草案固定 A/B 400 KiB、boot/CRC 分區、automatic rollback | 未取得正式 boot/map 不定址；目前 linker 1016 KiB 是 app window，不是 partition 授權 |
| B13 | 原草案把 .bss、RTOS heap/task stack/BT buffers 重複加總 | Baseline與新增 reserved bytes分開；以 map＋allocator runtime ledger 計算 |
| B14 | 文件稱 CMake 成功、RTT 可用、release .conf 已存在 | 目前確證為 SDK Makefile；repo CMake/linker/startup parity 與 RTT 都另設 gate |
| B15 | PWM、NFC IRQ/register、charger fault truth table 的筆記推論 | 只做 transport／logical interface 規劃；driver 細節等 exact datasheet/sample |

這些是資料／規劃衝突，不是本次已證明的 silicon 或 PCB defect。Q48 包含 memory、revision、pinmux 與資格詢問；Q49/Q50 尚不能當成既有正式清單項目引用，本次不改對外問題清單／HTML/PDF。

## 3. 證據登錄格式

每個 Hxx 保持一筆狀態：`NOT_STARTED / HOST_PASS / EVB_PASS / EVT_PASS / FAILED / BLOCKED`。PASS 必須記錄 board/BOM revision、SDK＋patch manifest、build/image hash、fixture、steps、expected/actual、測量值、log位置、reviewer。EVB_PASS 不自動升級成 EVT_PASS。

進度以這張矩陣與實際證據更新；「SDK 有 driver」「能編譯」「有時收到資料」都不是產品功能驗收。外部輸出／充電／CAN transmit／擦除測試必須先確認 safety scope 和 fixture。
