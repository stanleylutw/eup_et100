# ET-100 韌體架構草案

日期：2026-10-21。版本：v0.2。狀態：架構草案；本輪只更新 NOR OTA 規格，尚未實作 installer／更新傳輸。

承接 [原開發草案](01_firmware_development_plan.md) 與 [平台計畫](../00_project/00_et100_firmware_platform_plan.md)。硬體逐項對照見 [覆蓋矩陣](03_hardware_coverage_matrix.md)，工作順序見 [執行計畫](04_development_execution_draft.md)。本文件不是 SDK 修改授權或施工單。

OTA／Flash 的詳細決策以 [06 儲存規格](06_ota_storage_design.md) 為準：單一內部 App＋外部 Candidate／Rollback／Sensor slot，三入口共用管理器；不採 stash 內部 A/B。新 map 是候選，現行 firmware memory layout 不變。

## 1. 依據與邊界

來源優先序不是單純「新文件覆蓋舊文件」：晶片額定能力看 exact-part datasheet；板級接線看符合 silicon revision 的正式 SCH/BOM/netlist；SDK 行為看實際 source/build；產品行為看已批准的需求／協定。衝突必須確認，不能自行選一個答案寫進 driver。

| 項目 | 目前可確認 | 仍未確認 |
|---|---|---|
| MCU | FR3068E-C，M33 最高 156 MHz；datasheet 列 2 MB Flash、512 KB SRAM | 板上 revision、完整 memory map、pinmux、IO domain |
| Host baseline | Patched SDK BLE example 原生 Makefile clean build 成功 | ET-100 韌體及 repo CMake 的完整 RTOS/BLE 等價 build |
| Clock | Example 目標核心 96 MHz，156 MHz guard 已加入 | 硬體頻率、Flash timing、並行負載與實際功耗 |
| 記憶體 | Linker：Flash 1016 KiB、SRAM 512 KiB、PRAM 128 KiB | Upper SRAM bank／retention；PRAM 與 datasheet SRAM 的關係 |
| 硬體功能 | 需求與舊 SCH 筆記可建立 logical function 清單 | 不能將筆記中的 UART/I2C 數字直接視為 controller ID |
| OTA | SDK 有 BLE OTA 與 image metadata 程式 | ROM/bootloader、可用 partitions、簽章及 rollback 保證 |

最新 host baseline：[PATCHES.md](../../vendor/PATCHES.md) 的 Phase 1a-ext 與 [Bring-up Category 8](../../../docs/00_project/11_project_bring_up_status.md)。以 bytes 為計量依據；1 KiB = 1024 bytes，1 MiB = 1048576 bytes。

## 2. 結構與責任

保留既有四層概念，不強制所有呼叫都經過 pub/sub。控制操作用明確 API；非同步狀態用 bounded queue／notification。

```text
Application：tracking / driver session / vehicle data / alerts / command routing
    |
Services：modem / positioning / NFC / storage / OTA / config / power / health
    |
Devices：EG800Q / AG3352Q / PN7160 / SC7U22 / AW9523 / NOR / 1-Wire / line IO
    |
HAL adapters + board profile：Freqchip SDK GPIO/UART/I2C/SPI/CAN/ADC/Timer/PMU
```

| 模組 | 唯一 owner 與輸出 | 不應負責 |
|---|---|---|
| `modem_service` | LTE UART、AT transaction、URC、SIM/network/socket/download 狀態 | 任意 task 直接向 modem 發 AT |
| `position_service` | NMEA stream、fix quality、age、GNSS 控制 | 把無效或過期座標當新定位 |
| `nfc_service` | Host protocol session、card discovery、card data | UID 自動等同已授權駕駛 |
| `vehicle_service` | CAN 白名單資料、DI debounce、ADC 校正、探頭讀值 | 任意 CAN 全流量永久保存 |
| `storage_service` | 設定／記錄／OTA staging 的分區與寫入排程 | Driver 自行擦寫其他分區 |
| `ota_manager` | 三入口 update owner、下載／驗證、install request；內部 boot state 為安裝唯一權威 | LTE/BLE/RS232 各自改 boot state，或用 NOR journal 決定安裝 |
| `power_service` | Rail dependency、wake policy、sleep 協調 | 各 task 自行開關共用電源 |
| `health_service` | Heartbeat、reset reason、故障分級、watchdog 授權 | 無條件 pet watchdog 掩蓋卡死 |
| `app_controller` | Tracking／alert／driver session 的業務狀態 | 包辦所有 peripheral polling |

裝置 API 至少有 init、start/stop、狀態、timeout、錯誤碼與 recovery；不存在的功能回報 unsupported，不回傳假成功。

## 3. 建議程式目錄

下面是後續施工目標，不代表目前已有這些檔案。先完成 build baseline，再逐模組建立，避免一次產生大量空殼。

```text
fw/src/
  main.c                       SDK-compatible startup handoff
  app/                         tracking, alerts, driver session, command router
  services/                    modem, position, vehicle, storage, ota, power, health
  drivers/                     chip/protocol-specific adapters
  hal/                         thin Freqchip peripheral wrappers
fw/include/                    matching public headers
fw/boards/
  freqchip_evb/                 verified EVB wiring/profile
  et100_evt/                    verified revision-specific wiring/profile
fw/configs/                    actual build-system options, not assumed .conf parser
fw/tests/
  host/                        parsers/state machines/storage fault models
  hil/                         board bring-up and integration procedures
```

`fw/src/main.c` 目前只是 placeholder。不能把 vendor example 的 boot／heap／BLE 初始化全部換成上面目錄後就宣稱移植完成。CAN controller adapter 與 J1939 decoder 分開；PN7160 transport 與 NFC host protocol 分開；NOR driver 與 record format 分開。

## 4. Board Profile 與開機

Board profile 使用 logical roles：`UART_LTE`、`UART_GNSS`、`UART_RS232_0..2`、`I2C_EXPANDER`、`I2C_SENSOR`、`I2C_NFC`、`FLASH_EXTERNAL`、`CAN_VEHICLE`。待確認角色不得填入猜測的 controller/pin。EVB 與 ET-100 profile 分開，debug console 也必須有獨立 routing 決策。

每個 binding 記錄：board revision、net、GPIO/controller、alternate function、physical pin、IO voltage、active polarity、reset default、rail、IRQ/DMA、來源及驗證日期。Address 使用明確 7-bit I2C notation；不能把 schematic bus suffix 當 address 或 SDK instance。

開機狀態機：

```text
RESET -> SDK_INIT -> BOARD_SAFE -> RTOS_READY -> STORAGE_READY
      -> DEVICE_PROBE -> CORE_READY -> NETWORK_READY (optional)
```

- 沿用已驗證的 SDK startup、RAM-code copy、heap 與 BLE/RPMsg 初始化順序；不重複啟動 scheduler／controller task。
- `BOARD_SAFE` 先確保輸出安全值與未供電 IO 不 backfeed，再初始化 AW9523 bootstrap bus。POR defaults 必須重新按 exact device 文件及板子確認。
- 由 power owner 開必要 rail，等待有依據的 settle time，再 probe 下游裝置；失敗可進 degraded mode，不無限 retry。
- `CORE_READY` 表示本地記錄與必要硬體就緒；不等於 LTE 已註網／cloud 登入，也不等於 GNSS 已定位。
- 不訂「500 ms 全功能 ready」。PWRKEY、modem registration、GNSS cold start 的時序另依 exact module firmware/spec 測試。
- 不因 rail 名稱含 NFC 就關閉它：舊筆記指出 `NFC_1V8` 可能同時供 MCU IO domain 與 IMU。

## 5. RTOS 與資料流

實際 SDK `app_config.h`：`FREERTOS_MAX_PRIORITY=10`，host priority 5、RPMsg 6、app 2、monitor 1；`FreeRTOSConfig.h` 引用該上限，因此合法 task priority 為 0..9。原草案 10／11 不能直接採用。既有 host/RPMsg stack constants 為 2048，呼叫端的單位與 runtime watermark 還需逐項確認，不能改稱另一組「SDK typical」。

初版採下列 execution contexts，不是一個 function 一個 task：

| Context | 工作 | 排程原則 |
|---|---|---|
| SDK host／RPMsg／timer／idle | 原有 Bluetooth 與 RTOS 路徑 | 保留配置，量測後才調整 |
| Modem worker | 唯一 AT owner、URC、binary data framing | Bounded receive buffers、transaction deadline |
| GNSS worker | 持續 stream receive／parse | 不被 LTE registration 阻塞 |
| IO worker | IMU／NFC deferred IRQ、ADC、LED、1-Wire 狀態機 | I2C 操作在 task；長 conversion 非 busy-wait |
| CAN worker | ISR 收 frame 後 filter／decode | 獨立 bounded queue；overflow 可觀測 |
| Storage worker | Record／metadata／staging operations | 背景 erase；控制 OTA 對 live logging 的影響 |
| App supervisor | Business events、power/health 協調 | 不執行長阻塞 bus transaction |

OTA 初期可在 storage worker 分塊推進，不必新增 task。NFC 吞吐或 latency 實測需要時再拆 IO worker。新 task 的 priority／stack 以測量決定，不先占用 SDK 的高優先序。

IPC contracts：事件含來源、sequence、monotonic timestamp、validity；UTC time 另有 quality/age。ISR 僅 capture／queue／notify，遵守 FreeRTOS interrupt priority 限制。Small values copy；大 payload 用 fixed buffer pool，明定 producer/consumer ownership、release、timeout 與 queue-full 行為。

Critical alerts 與一般 telemetry 分級。CAN／UART raw stream 不走廣播 event bus；OTA payload 不重複 copy 到每個 subscriber。每個 queue 都有 capacity、high-water mark、drop counter 與 backpressure policy。Bus mutex 不得持有到等待 LTE/cloud response；定義固定 lock order，避免 power/storage/bus deadlock。

## 6. 通訊與功能服務

- LTE：區分 command response、URC、prompt 與定長 binary payload；下載內容即使含 CR/LF 或 `OK` 也不能當 AT response。處理 timeout、SIM 拔插、註網失敗、PDP/socket 斷線及 bounded recovery。RTS/CTS 是否接出未定，不假設可用硬體流控。
- GNSS：NMEA checksum、fragmentation、fix validity、staleness、UTC quality；EINT 未證明為 1PPS，初版不依赖它提供精確同步。AGPS/EPO 是後續選配，不先承諾。
- NFC：取得 PN7160 exact variant/firmware 對應的 NCI/host library 與 sample，再建立 transport；不照舊筆記猜「IRQ status register」。卡片種類、UID／卡上資料／driver authorization 需分開驗收。
- IMU：取得 SC7U22 register map、WHO_AM_I、interrupt/FIFO 與低功耗設定；不直接套其他 IMU 的 0x6A/0x6B 位址或暫存器。Motion wake 必须保留 sensor 所需 rail。
- RS232：三個 logical ports 獨立 baud/framing/config；初版依需求候選 115200／9600／9600，exact routing 先確認。Configuration、透明轉發或周邊 protocol 分開，不預設三路皆為 console。
- CAN：以 MCU CAN controller + SIT1042 transceiver 組成；不是 UART-to-CAN。先 bench loopback/listen-only，再依 PGN/SPN 白名單實作 J1939。Vehicle transmit 另需批准；bus-off recovery 限次，不持續重啟干擾車輛。
- 1-Wire：最多四顆 probe，ROM search/CRC、nonblocking conversion、離線／斷線辨識。分離 RX/TX BJT buffer 的極性需量測；parasite power／strong pull-up 不由「有 data+GND」推定已支援。
- ADC／battery：raw counts、校正 mV、工程單位分開；確認 reference/range/settling 後才乘分壓比。沒有 NTC 時回 unavailable；電壓估算不得標為精確 SoC。CHRG/FULL 的 fault 解碼按 charger 文件，不猜四態。
- LED／buzzer：Pattern scheduler，不用 task 內 delay 阻塞；依提案 P13 與 Q45 收斂產品行為。舊 SCH 稱自激 buzzer，先規劃 on/off cadence，不保證 PWM 可改音調。

## 7. 儲存與 30 天記錄

由 storage owner 管理 erase-aligned partitions；優先評估 SDK 已有 FAL/FlashDB，先驗證斷電與板級 NOR adapter，再決定 TSDB 或專用 append-only ring。沒有需求不新增 FatFS／大型通用 framework。

Record schema 至少含 version、length、sequence、timestamp/quality、position validity、selected vehicle/sensor values、CRC 與 commit marker。Pending upload／ACK checkpoint 與 physical erase 分離；遇重複 ACK、掉電或重新連線不能漏資料。Server 需支持明確去重／ACK 契約，不能由 client 單方面保證 exactly-once。

以下是 **32 MB ET100 NOR 的候選預算**，不是已部署 partition table；相對 offset 詳見 06 §3：

| 用途 | 初版預算 |
|---|---:|
| Download journal／metadata | 128 KB，只管下載 |
| ET100 Candidate | 2 MB |
| ET100 Rollback | 2 MB，安裝前建立並驗證 confirmed App 備份 |
| Sensor OTA | 256 KB slot；package 上限 225280 B（含 256 B header） |
| Logs／其他資料 | 27.625 MB，內部分配 telemetry／diagnostics／wear reserve |

主要 Settings 改為內建 112 KB，boot state 內建 16 KB；舊 1 MB config／journal 合併預算取消。24 MB telemetry 為 Logs／其他的子預算，不再與 27.625 MB 重複相加。EVB 只有 16 MB／1.8 V NOR，能測 offset 0x460000 前的 OTA 區，但不能驗證 ET100 的 32 MB／4-byte addressing。

全部 record overhead 都計入單筆 bytes；事件額外計算。30 天、15 秒一筆 = 172800 筆：64 bytes 為 10.55 MiB，128 bytes 為 21.09 MiB，256 bytes 為 42.19 MiB。24 MiB 對 128-byte periodic records 僅餘 2.91 MiB，還要扣 sector overhead／未滿 sector／事件。不得宣稱 CAN raw logs 也能保留 30 天；policy 需定義已 ACK／未 ACK、滿容量及重要告警的保存次序。

SDK `IC_W25Qxx.c` 現有 read/program 路徑送三個 address bytes；`ext_flash.c` 使用 EVB GPIO、開 quad，部分 protection/chip-erase API 為空。這不是 ET-100 32 MiB NOR 已可用的證據。需驗證符合 XM25QH256D 的 addressing method，測 16 MiB 邊界上下與最高 sector 不 alias；任何 destructive test 只用批准的 scratch partition。

## 8. LTE／BLE／RS232 更新

三入口均為 Stanley 已確認需求，不是互相替代；取消 Factory recovery、改留 sensor slot 亦已確認。RS232 正常 9600、更新模式 115200／8N1。

```text
LTE download adapter -----+
BLE app transfer adapter -+-> OTA manager -> Candidate / Sensor slot -> verification
RS232 binary adapter -----+                        +-> installer -> trial / confirm / rollback
                                                   +-> sensor BLE DFU relay
```

共用 API 概念：`begin(manifest, source)`、`write(session, offset, data)`、`query_progress()`、`finish()`、`cancel()`。名稱與 wire format 尚未定稿。Manifest 規劃含 product/board compatibility、version、image length/hash、signature/key ID；所有 offset/length 做 overflow 與 partition bounds 檢查。

```text
下載（NOR journal）：IDLE -> RECEIVING -> RECEIVED / download validated
安裝（內部 state）：INSTALL_PENDING -> BACKUP_READY -> INSTALLING -> TRIAL -> CONFIRMED
恢復（內部 state）：ROLLBACK_REQUIRED -> ROLLING_BACK -> CONFIRMED / RECOVERY_FAULT
```

- 只允許一個 writer/session，三入口與兩種 target 均互斥；NOR journal 綁定 manifest/hash，只管下載／續傳，安裝／trial／確認／rollback 唯一權威是內部 state。
- LTE／FTP 是已確認需求，FTPS／HTTPS 可另建議而不能直接取代；exact modem firmware 支援、binary download／resume 行為另查官方 module 文件。
- BLE App 需匹配 Freqchip 或自訂 protocol；不是 Nordic DFU 自動相容。MTU/chunk/ACK/flow-control/security/reconnect 要與 App 一起定義及測試。
- Transport encryption、CRC、hash 與 image signature 是不同保護。簽章驗證／key provisioning 列為正式 release gate；沒有 secure boot 證據時不得宣稱 root-of-trust／anti-rollback 已達成。
- SDK `app_otas_get_storage_address()` 從現有 A image 尾端向上 4 KiB 對齊求 B address；不是固定 400 KiB A/B。外部 staging 不代表 bootloader 能直接從外部 Flash install。
- Factory `Project_burn.bin` 與 OTA payload format 分開確認。保留現有 Patch #5，不推翻 chip type／封裝流程。
- 已選單一內部 App＋NOR staging，不選晶片內 A/B；實作仍須驗 ROM header 只覆蓋 installer、獨立 installer 可用／備份先於 erase／state journal／掉電恢復。App @0x08020000 的 VTOR 與 RAM-code 必須一併處理。
- 一般 confirmed boot 不初始化 NOR；只有待安裝／安裝恢復或 rollback 才由 installer 啟 NOR。安裝中無可執行 App 時，installer 自帶 NOR／Flash／驗簽及必要硬體初始化，不能回呼 App。
- 開發順序為 NOR／installer／斷電復原 → RS232 → BLE → LTE → sensor relay；沒有 boot gate 不交付 field-safe 更新。
- App 中驗簽也不能替代不可繞過的 boot verification。正式版本不得保留可任意寫記憶體／boot region 的範例 OTA/debug 指令。

## 9. 資源與可靠度

| Latest example 指標 | Bytes | Linker 使用率 |
|---|---:|---:|
| Raw Flash image | 260376 | 25.0% / 1016 KiB |
| Static SRAM | 97232 | 18.5% / 512 KiB |
| RAM-code PRAM | 15048 | 11.5% / 128 KiB |
| Factory burn image | 268568 | 封裝檔長度，不當作 app runtime allocation |

SRAM baseline 已包括 `rt_heap` 50 KiB、BT `.dram_section` 30784 bytes、`_user_heap` 4096 bytes；不能再把三者全加一次。動態 task stacks 若由既有 RTOS heap 分配，也不能同时算 heap reservation 與每個 allocation。

暫定工程目標：新增 reserved SRAM 相對 baseline 不超過 128 KiB，約 223 KiB total；PRAM 不先放新 DMA/CAN buffers；Flash 必須先符合已批准 image window／OTA方案，而非預設剩餘 750 KiB 全給 app。這些是 review budgets，不是實測 ET-100 numbers；crypto/NFC/protocol 納入後以 map/stack/heap 水位修訂。

Health checklist：allocation failure、queue overflow、storage busy timeout、watchdog task health、reset reason、bus recovery、modem restart budget、OTA lock、record loss counter。Crash information 用小型可恢复記錄，HardFault 中不做阻塞 I2C/SPI dump。敏感 config／SIM identifiers 不寫入一般 log；正式 build 保留 clock/bounds/signature guards，不因 `NDEBUG` 消失。

Sleep 使用 prepare/quiesce/commit/resume 協調：停止新 transaction、持久化必要 metadata、等待 bounded completion、確認 wake source 所需 rail／retention，再睡眠。OTA install 與 flash erase 階段可 veto sleep。Motion wake 模式不能把 IMU VDD/VDDIO 關掉；備援模式下 CAN/NFC/RS232 的 rail 不一定存在，需 capability-aware degraded state。

## 10. 審查前需要決策

1. Formal EUP protocol、server ACK 與 App BLE protocol owner/version。
2. Logging interval、maximum serialized record bytes、event peak rate、滿容量 policy。
3. Board-matched bootloader、partition table、programmer access、signing/key ownership。
4. Safe DO electrical behavior、CAN STB 真正 routing、五個 UART／三條 I2C 的合法 pinmux。
5. 電池安全、NTC 是否 populated、motion wake 模式與 retention 範圍。

這些未答覆不阻擋 host parser／state-machine tests，但阻擋相應 hardware writes 或 production claims。
