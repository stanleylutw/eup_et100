# ET-100 開發對照與待驗證清單

整理日期：2026-10-02。datasheet 來源：[FR306x v0.4.9](../QT0013201514_FR306x技术规格书_v0.4.9.pdf)。返回 [索引](README.md)。本文件清楚區分 datasheet、現有 SDK 配置、舊硬體分析與開發建議；沒有在本次整理中修改 source、linker、SDK 或原理圖。

## 1. 型號與 SDK 配置不能混為一談

| 項目 | Datasheet 的 FR3068E-C | 現有資料／配置 | 處理方式 |
|---|---|---|---|
| Flash | p.7：2 MB | SDK `ldscript.ld` 與 `ldscript_3068e.ld` 為 1016K app window，自 0x08002000 起 | 記錄差異；確認晶片 revision、boot 區、完整 memory map，不直接擴大 LENGTH |
| SRAM | p.7：512 KB | SDK SRAM 256K，base 0x20000000；另有 PRAM 128K，base 0x1FFE0000 | 來源未說兩者与 512 KB 的對應及其餘 bank，不能自行相加或直接改成 512K |
| Cache | p.5／8：32 KB | SDK XIP／cache 行為另需查 implementation | 不從總 Flash 容量推導 cache coherency |
| PSRAM | 只有 FR3068EP-C 列 2 MB | ET-100 名稱為 FR3068E-C | 不能視為普通 E-C 已有 PSRAM |
| CPU | p.7：最高 156 MHz | `app_config.h` 列出最高 240 MHz 的通用 SDK 選項 | SDK menu 的選項不是 FR3068E-C clock rating |
| 封裝 | p.7／12：QFN80 9×9 mm | 舊硬體分析曾寫 LQFP80 | 回查 PCB footprint、BOM、原理圖 symbol 与實際雷雕 |
| AEC-Q100 | p.7：否 | 舊硬體分析曾寫 Grade 2 | 取得 exact part 的資格資料，不沿用系列摘要 |
| CAN | p.7：2 組 CAN FD | 原理圖另有外部 CAN transceiver | 確認每 instance 的 pinmux、clock、bus routing；不能當成系列 4 組 |

本機配置來源：

- [實際 Makefile](../../../fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC/Makefile)，`LD := .../ldscript.ld`。
- [ldscript.ld](../../../fw/vendor/fr30xxc_sdk__202411/components/tools/gcc/ldscript.ld)，`FLASH = 1016K`、`PRAM = 128K`、`SRAM = 256K`。
- [ldscript_3068e.ld](../../../fw/vendor/fr30xxc_sdk__202411/components/tools/gcc/ldscript_3068e.ld)，有相同三段配置。
- [SDK app_config.h](../../../fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/Inc/app_config.h)，`SYSTEM_CLOCK_SEL` 通用選項。
- [舊 MCU 硬體分析](../sch_analysis/01_MCU_FR3068E-C.md)，來源性質是先前原理圖分析，非新版 datasheet。

SDK 名稱為 `fr30xxc_sdk__202411`，日期早於 datasheet v0.4.9；這是版本風險線索，不足以證明哪個配置錯。p.30 v0.4.4 明列「修改 FR3068E-C pin」，因此板上 silicon／文件 revision 必須確認。

## 2. 舊硬體筆記的 physical pin 差異

以下只比較舊 Markdown 與 v0.4.9，不判定原理圖本身一定錯，也不建立新的 ET-100 net map。

| 對象 | 舊筆記列示 | Datasheet v0.4.9 p.12／16-18 |
|---|---|---|
| PB1 別名 | PB1/BT_PA7 | pin 11 PB1/BT_PA5 |
| pin 13 | VDD_IO3V3 | PB3/BT_PA7 |
| VDD_IO3V3 | pin 13 | pin 14 |
| PB4／PB5 | pin 14／15 | pin 15／16 |
| PB6／PB7 | pin 16／17 | pin 17／18 |
| PB8／PB9 | pin 18／19 | pin 19／20 |
| PB10／PB11 | pin 20／21 | pin 21／22 |
| PB12／PB13／PB14 | pin 22／23／24 | pin 23／24／25 |
| PB15 | pin 25 | pin 28 |
| pin 26 | PC0／VDD0 | PC1 |
| pin 27 | PC1／SWCLK | PC0；來源未標 SWCLK |
| pin 28 | PC2／SWDIO | PB15；來源未標 SWDIO |
| PC2／PC3 | pin 28／29 | pin 29／30 |
| pin 31 | PC5 | DVDD_1V2 |
| PC4／PC5／PC6／PC7 | pin 30／31／32／33 | pin 32／33／34／35 |
| pin 34 | VDD_IO2_1 | PC6 |
| VDD_IO1V8 | 舊表位置及命名混雜 | pin 36 |
| PD0／PD1 | pin 35／36 | pin 37／38 |
| PD15 | pin 50，部分段落寫 pin 51 | pin 52 |
| PP4／PP5／PP6／PP7 | 舊完整表 pin 51／52／53／54 | pin 53／54／55／56 |

完整 80 pin 權威摘錄見 [02](02_packages_and_pinout.md)。確認程序：先核對原理圖 PDF 的 symbol pin number，再對照 PCB pin、BOM exact part、供應商 silicon revision／pin-change errata，最後才改 board config。不能用這張「差異表」直接改 SWD 接線。

## 3. 韌體功能可用性分層

| 層級 | 本次文件能支持什麼 | 仍需什麼 |
|---|---|---|
| 晶片能力 | M33、FPU／DSP、BLE／BR／EDR、CAN FD、周邊數量 | exact silicon／errata |
| SDK build | 既有 patched example 可 host build，非本次重建 | ABI、功能配置、reproducible patches |
| Boot／燒錄 | datasheet 只有 reset／power 條件 | boot ROM／header／CRC／地址／燒錄工具文件 |
| IO 接線 | physical pin 名稱與電氣門檻 | alternate-function map、voltage bank map、board net mapping |
| Runtime | 原廠宣稱支援各功能 | EVB／ET-100 實機 bring-up、latency、power、reliability |

不能從本 datasheet 推出 `post_process.py` 的 FR303x／FR509x header 選擇。也不能用它證明某份 BIN「一定能／不能 boot」。相關差異應在 bootloader reference／EVB 驗證中收斂。

## 4. Bring-up 建議順序

以下為開發建議，不是 datasheet 原文。

1. 確認 BOM exact part、silicon revision、QFN footprint、EPAD、原理圖 pinout 及供電架構。
2. 量測 MCU_VCC／BT_VCC、IO domains、reset、24 MHz oscillator；驗證 cold start、brownout、快速 power-cycle。
3. 取得 debug pinmux／VTref／SWD 文件，先確保 debugger voltage 与 IO domain 相容。
4. 確认 SDK matching revision、Flash ID、SRAM／PRAM bank、linker／startup 配對、燒錄地址及 boot metadata。
5. 跑最小 GPIO／UART；UART3 的範例接腳與板上的 RS232／CAN／LTE routing 必須先核對，不能直接移植。
6. 啟動 FreeRTOS，驗證 tick、heap、stack watermark、interrupt priority、sleep／wake。
7. 驗證 BLE 名稱／廣播／GATT／連線／掉線重連，再驗證安全性與 OTA；host build 不代表 OTA header 正確。
8. 分別啟動 CAN／I2C／SPI／ADC／GNSS／LTE 等 board 周邊，最後做並行壓力、功耗與電源失效測試。

## 5. 各功能的開發檢查表

### CPU／clock／memory

- [ ] 主頻不超過 FR3068E-C 的 156 MHz rating；確認 PLL source、divider、Flash wait state、實際核心頻率。
- [ ] 取得完整 memory map；確認 512 KB 的 bank 分配與 PRAM 关系。
- [ ] linker／startup 的 `.data`、`.bss`、RAM code、cache、heap、stack 與保留區一致。
- [ ] DMA buffer alignment、cache coherency、睡眠 retention 的 bank 可用性有正式規格。
- [ ] 不以 linker 空間剩餘量直接等同 malloc 可用量。

### Power／IO／reset

- [ ] MCU_VCC／BT_VCC 維持 2.9-3.6 V recommended range。
- [ ] cold start TR 滿足 5 us < TR < 10 ms；power-cycle 低於 2.4 V 後 TD > 20 ms。
- [ ] 1.8 V／3.3 V domain map、sequencing、unpowered IO／backfeed 有明確依據。
- [ ] BT 10 µH 與 SYS 2.2 µH Buck 電感的 Isat／SRF／DCR 都符合 p.28。
- [ ] PORT_PMU／GPIO 的 pull 與 drive 依 domain 配置；不要讓內部 pull 干擾 ADC。
- [ ] RSTN、RESV0、NC、NA、EPAD 與 decoupling 按原廠 hardware guide 處理。

### BLE／RF

- [ ] 精確 PHY／TX power／interval；區分 IC 摘要和板級量測。
- [ ] 使用載板／外殼後重新驗證天線及 co-existence，尤其 LTE 與 switching regulator 干擾。
- [ ] SDK connection／bond／MTU／buffer 上限實測，不由 Bluetooth 5.3 標籤推定。
- [ ] sleep retention 範圍与 RTC／GPIO wake 實測；128K deep-sleep 數值為 TBD。

### CAN FD

- [ ] 2 組 CAN 的 pinmux／clock／message RAM 與 transceiver routing 明確。
- [ ] nominal／data bit timing、sample point、oscillator tolerance、TX delay compensation 按 reference manual 設定。
- [ ] DLC 對應最大 64-byte payload；測 FIFO、filter、bus-off、error counter、重試及恢復。
- [ ] 宣稱支援 J1939／AUTOSAR 不等於相關軟體、license 或測試已交付。

### 外部 Flash、ADC、UART／I2C

- [ ] 外部 SPI NOR 的容量与內建 2 MB Flash 分開規劃；不能把兩者合併成連續程式空間。
- [ ] QSPI 最高 78 MHz 仍須受實際 Flash、layout、dummy cycle 與 voltage／temperature 限制。
- [ ] ADC reference／input range／sample time／channel mapping 另取文件；12-bit 不代表量測精度。
- [ ] 5× UART、3× I2C 的 instance 與 GPIO mapping 另確認；板上網名不代表有 6 個硬體 UART／I2C。
- [ ] I2C speed／rise time／pull-up、UART flow control 與 baud error 按 board 與 peripheral 規格驗證。

## 6. 容量規劃注意事項

本次 datasheet 新依據為 FR3068E-C 2 MB／512 KB，但目前 build 的實際配置仍是 1016 KiB Flash window、256 KiB SRAM 加独立 PRAM window。應同時保留「資料表總容量」與「本次 linker 可用區」兩套數字。

不能據此宣稱剩餘容量已足夠 dual-image OTA、30 天 log、LTE protocol 或 NFC stack。分區需要 bootloader、A／B images、metadata、bond／KV、log、erase block 對齊、wear 與斷電恢復 budget。若 log 使用板上外部 SPI Flash，需另外核對該元件容量、格式與擦寫壽命。

## 7. 可直接交給原廠的確認清單

| 優先級 | 要求 | 理由／來源 |
|---|---|---|
| P0 | 提供目前板上 FR3068E-C revision 對應 datasheet／pin-change errata | p.30 v0.4.4 改 pin，舊筆記多處腳號不同 |
| P0 | 提供 2 MB Flash／512 KB SRAM 的完整位址、bank、PRAM／cache／reserved map | p.7 與 SDK 202411 linker 差異 |
| P0 | 提供 full pinmux、IO voltage-bank、reset defaults、SWD／VTref 表 | p.15-18 只有 GPIO 名稱 |
| P0 | 提供 board-matched boot image format、chip-family selector、ROM／Flash boot 流程與燒錄 guide | datasheet 未定義 header／CRC |
| P0 | 提供 1.8 V／3.3 V domain sequencing、RSTN timing、RESV0 和 unpowered IO 規範 | p.26 只有系統 VCC 時序 |
| P1 | 確認 exact part 的 AEC-Q100 狀態／證書及 temperature grade | p.5 系列概述與 p.7 型號資格不同 |
| P1 | 提供 CAN reference manual、bit-rate 上限、clock／message RAM 資料 | p.9 能力列表不含 register／timing |
| P1 | 提供 ADC analog specs、sleep retention map／current、Buck／晶體 application notes | p.27 有 TBD，p.28 只有元件摘要 |

這張表只是內部查詢清單，本次沒有對外傳送。
