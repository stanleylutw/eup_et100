# ET-100 Firmware Platform Plan

Version: v0.5
Last updated: 2026-10-21 Codex

## Revision History

| Version | Date Time | Summary | Who | FW Branch |
|---|---|---|---|---|
| v0.1 | 2026-10-02 10:00:00 | Initial skeleton — project kick-off，Phase 0 scaffold only | Stanley | main |
| v0.2 | 2026-10-02 | 依 FR306x v0.4.9 同步 MCU 規格；區分晶片總容量與 SDK 配置，未修改 firmware | Codex | 未執行 git 操作 |
| v0.3 | 2026-10-21 | 新增 DEV1.4 獨立 9600 UART3 target 與 GCHK 相容子集；program/readback、Stanley HTML GCHK 及未支援命令回覆通過 | Codex | v0.3.0_evb_serial_gchk |
| v0.4 | 2026-10-21 | 新增 GTIME query/set、FlashDB key/schema、寫入後讀回及錯誤處理；9600 target 更新為 0.4.0-dev，硬體保存測試由 Stanley 執行 | Codex | v0.4.0_evb_gtime_settings |
| v0.5 | 2026-10-21 | 新增 GCID／SCID 保存、BLE scan 與 Type 1；實測 4 MAC、55 筆 Type 1／XOR 全通過，CID 切換及 CID／GTIME 手動 reset 保存獲 Stanley 確認 | Codex | v0.5.0_evb_cid_ble_scan |

## 1. 專案定位

- **產品**：EUP ET-100 Vehicle Tracker（車輛追蹤器）
- **目標市場**：Taiwan、Vietnam、Malaysia、Thailand、Indonesia 車隊管理
- **硬體**：Quectel ODM 設計（QEM800QQA-FYN01AA SCH V1.1）
- **韌體**：Eupfin 自主開發（本計畫）
- **分工**：Quectel 做自己版本的 FW 跑在同一塊 HW 上；Eupfin 自寫韌體有獨立 IP、cloud 相容性

## 2. 硬體概要（WHY + WHAT）

### 2.1 核心
- **MCU**：Freqchip FR3068E-C（Cortex-M33 最高 156 MHz + 32-bit RISC @48 MHz BT core，2 MB Flash、512 KB SRAM，BT5.3 BR/EDR/BLE，2 組 CAN FD，56 GPIO，QFN80 9×9 mm）。v0.4.9 p.7 AEC-Q100 欄為「否」。
- **目前 SDK 配置**：Flash app window 1016 KiB @0x08002000、SRAM 256 KiB @0x20000000、PRAM 128 KiB @0x1FFE0000；未確認與晶片總容量的 bank／reserved 對應，不直接擴大配置。
- **規格來源**：[FR306x v0.4.9 開發參考](../../../docs/03_hardware/fr306x_reference/README.md)；physical pin、pinmux、IO voltage bank 與板上 revision 須核對後才能移植 driver。
- **LTE**：Quectel EG800Q-EU（Cat-1 bis，UART AT command 介面）
- **GNSS**：Airoha AG3352Q（UART NMEA，GPS/GLONASS/Galileo/BeiDou/QZSS/SBAS）
- **NFC/RFID**：NXP PN7160（I2C 介面，MIFARE/ISO14443/15693/FeliCa）
- **IMU**：SC7U22（6 軸 accel+gyro，I2C）
- **外部 IO 擴展**：AW9523B I2C IO expander（16 pin）
- **外部 Flash**：XMC XM25QH256D 32MB SPI NOR
- **電源管理**：9-36V 車電 → 多路 DCDC/LDO + 600mAh 備援電池 + 充電器

### 2.2 對外介面
- 26-pin 主連接器（PWR、ACC、ADC、1-Wire、4×DI、1×DO、3×RS232、CAN、4×ACC_OUT）
- 4 顆 LED（Driver / Memory / GPS / Net）
- 1 顆蜂鳴器
- NFC 讀卡區（上殼內側 40×30mm 天線）
- 翻蓋 nano-SIM 卡座
- Debug port（SWD / UART）

詳見 [../../../docs/03_hardware/sch_analysis/](../../../docs/03_hardware/sch_analysis/)。

## 3. 韌體架構（4 層）

```text
┌─────────────────────────────────────────────────────────┐
│ Application                                             │
│   GNSS Tracking | LTE Comm | BLE/RFID | CAN Acquisition │
│   Status & Alert | Driver ID | Power Policy             │
├─────────────────────────────────────────────────────────┤
│ Framework (RTOS kernel + services)                      │
│   FreeRTOS | Protocol Stack (EUP) | OTA | PM | Peripheral Bus │
├─────────────────────────────────────────────────────────┤
│ Driver (SDK + custom)                                   │
│   LTE AT | GNSS NMEA | PN7160 I2C | AW9523 I2C          │
│   SC7U22 I2C | SPI Flash | CAN J1939 | BLE Stack (vendor binary) │
├─────────────────────────────────────────────────────────┤
│ HAL (Freqchip SDK: components/drivers/peripheral/)      │
│   GPIO | UART | I2C | SPI | CAN | PWM | RTC | ADC       │
└─────────────────────────────────────────────────────────┘
```

## 4. Toolchain / Build

DEV1.4 已新增專案自有 UART3 serial target：`bash fw/tools/build_evb_serial.sh`，獨立產物在 `fw/build_serial/output/`。透過 build overlay 替換 example main／AT／BLE source，不修改 SDK。UART3 使用 9600/8N1，GCC vendor printf 被丟棄；原 SDK target 保留 921600 log。GCHK 回覆 EVB 板型與 SDK-derived BLE identity；state 0 為此相容層 idle。GTIME 沿用 key `0x00030002`（預設 15，1-1800 秒）；0.5.0-dev 新增 CID key `0x00030003`（四欄、預設 0639），均以 FlashDB 寫入與讀回確認後才更新 RAM。Stanley 已確認前版 GTIME 保存可用。

0.5.0-dev 使用 general discovery active scan（LE 1M、25 ms interval／18.75 ms window），只處理 legacy ADV。以 CID/UUID 過濾 0xFF／0x16，快取 32 個 address type＋MAC，依 GTIME 及 RSSI 回報最多 min(GTIME,15) 筆 Type 1、每筆間隔一秒，支援 EUP v1/v2 1500 ms burst 去重。原 peripheral profile 保留，radio 並行仍待 EVB 驗收。未實作 MAC whitelist、MAXRPT setting、GATT central session、realtime XYZ batch 或 OTA。規格見 [serial 協定](../02_protocol/01_evb_serial_gchk.md)，最新證據見 [CID／scan 測試紀錄](../04_testing/04_evb_cid_ble_scan.md)。

- **Compiler**：ARM GNU Toolchain `arm-none-eabi-gcc` 13+
- **Build**：CMake + Ninja（可相容 SDK 原生 Makefile）
- **IDE**：VS Code + C/C++ 擴充 + Cortex-Debug
- **Debug**：J-Link 或 DAPLink + OpenOCD
- **Flash**：JLinkExe、OpenOCD、或原廠 FreqChip_Download（serial）
- **CI/CD**：GitHub Actions（待建）

→ 決策依據：[`../99_decisions/01_toolchain_gcc.md`](../99_decisions/01_toolchain_gcc.md)

## 5. 開發階段（粗略）

| Phase | 目標 | 時間 | 依賴 |
|:-:|---|---|---|
| **0** | Project skeleton + SDK fetch + CMake 骨架 | **已完成 2026-10-02** | 無 |
| **1** | 向 Freqchip 購 EVB，跑 SDK blink / BLE periphreal 範例 | T0+2 週 | EVB 到貨、Q39-Q46 回覆 |
| **2** | 建 HAL 抽象層、GPIO / UART / I2C / SPI driver bring-up | T0+2 月 | Phase 1 完成 |
| **3** | 周邊 bring-up：GNSS NMEA、LTE AT、PN7160、G-Sensor、AW9523、SPI Flash | T0+4 月 | EVT board 到貨 |
| **4** | 應用層：EUP 協定、OTA、CAN J1939、低功耗策略 | T0+6 月 | Phase 3 完成 |
| **5** | 整合測試、認證測試、量產前驗證 | T0+9 月 | DVT/PVT board |

> T0 = SOW 簽訂日。目前尚未 T0。

## 6. 與 Quectel FW 的 Compatibility 要求

Eupfin FW 需與既有 cloud server + mobile app 相容：
- BLE advertising packet 格式（待 Quectel 提供 spec，Q45）
- Cloud server protocol（EUP 終端設備通訊協定，待考古或新定義）
- OTA image format（待 Quectel 提供 spec）
- G-Sensor event threshold（待 Quectel 提供）

## 7. 已知風險（高層次）

| # | 風險 | 影響 | Mitigation |
|---|---|---|---|
| R1 | Freqchip BSP 正式授權管道未定 | 無法通過 BT BQB 認證 | 走社群 bundled SDK + 加 Quectel 牽線（Q39）|
| R2 | Secure boot signing key 不在 Eupfin | 量產板燒不進自家 FW | SOW 寫明可選不啟 secure boot，或 Eupfin 持有獨立 key（Q42）|
| R3 | Datasheet 2 MB Flash／512 KB SRAM 與 SDK 可用區的映射未確認 | 擴大 linker 可能覆蓋保留區；應用與 OTA budget 未定 | 取得 board-matched memory map、PRAM／SRAM bank 分配與適配 SDK（Q48）；先按現有 linker 預算 |
| R4 | EG800Q-EU TDD B40 不支援 → TH/MY/ID 市場受限 | 業務面問題，非 FW | 業務層決定（Q01）|
| R5 | Solo + AI 開發速度上限 | Phase 時程壓力 | 強化 Claude Code / Codex / Antigravity 分工（comm.md）|

詳盡 open issues：[../../../docs/03_hardware/sch_analysis/99_risks_vs_proposal_v1.8.md](../../../docs/03_hardware/sch_analysis/99_risks_vs_proposal_v1.8.md)

## 8. 下階段

- 等 Quectel 回覆 100_questions V1.2 的 P0-FW 區（Q40/Q42/Q43/Q44/Q45/Q47/Q48）
- 並行跑 T1 EUP 協定考古、T2 建 git repo、T3 FR3068 技術資料收集
- 購 Freqchip FR3068E-C EVB（**不用等 Quectel**）
