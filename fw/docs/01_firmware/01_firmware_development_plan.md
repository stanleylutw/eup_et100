# ET-100 Firmware Development Plan — DRAFT

**Version**：v0.1 DRAFT
**Last updated**：2026-10-02
**Author**：Claude Code + Stanley (Eupfin)
**Status**：Draft — subject to revision as Quectel/Freqchip answers come back from [100_questions_for_quectel V1.3](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md)

> **本檔定位**：ET-100 韌體**開發計畫 + 架構草案**，承接 [00 Platform Plan](../00_project/00_et100_firmware_platform_plan.md) 的 WHY/WHAT，寫 HOW。
> **與相鄰文件分工**：
> - [fr306x_reference/05 ET-100 開發對照](../../../docs/03_hardware/fr306x_reference/05_et100_development_reference.md)：bring-up 順序 + 各功能 check list（**本檔不重複**）
> - [sch_analysis/](../../../docs/03_hardware/sch_analysis/)：HW 真實樣貌（本檔引用，不複製）
> - 本檔：**4 層架構、FreeRTOS task 拓撲、驅動矩陣、記憶體預算、階段產出、build matrix**

---

## 1. Scope & Deliverable 總綱

### 1.1 Hardware Function 覆蓋目標

本韌體**必須驅動**以下所有硬體功能（來源：[提案書 §P05 客戶需求](../../../docs/00_project/02_客戶需求.md)、[§P45 軟體清單](../../../docs/00_project/06_技術方案_線束與軟體.md)、[sch_analysis 全套](../../../docs/03_hardware/sch_analysis/00_索引.md)）：

| # | 硬體 Function | 對應 SCH Sheet | 驅動複雜度 |
|:-:|---|:-:|:-:|
| 1 | GNSS 定位（AG3352Q NMEA） | 12 | 🟡 中 |
| 2 | LTE 聯網（EG800Q-EU AT） | 2 | 🔴 高 |
| 3 | BLE 5.3 設定/OTA（FR3068E-C 內建） | 1 | 🟡 中（SDK 提供 stack）|
| 4 | NFC/RFID 駕駛識別（PN7160 I²C） | 10 | 🟡 中 |
| 5 | G-Sensor 運動偵測（SC7U22 I²C） | 3 | 🟢 低 |
| 6 | CAN J1939 車輛資料（SIT1042 + MCU CAN FD） | 5 | 🔴 高 |
| 7 | 4× DI 外部中斷偵測（ACC + Input1-3） | 11 | 🟢 低 |
| 8 | 1× DO 外部負輸出（Output pin） | 11 | 🟢 低 |
| 9 | 類比輸入 ADC（油量 + VBAT + VBAT_NTC + PWR_IN） | 1、8、11 | 🟢 低 |
| 10 | 1-Wire 溫度 4× DS18B20 | 11 | 🟡 中（bit-banging timing） |
| 11 | 3× RS232（UM3221 ×3） | 5 | 🟢 低 |
| 12 | 4× LED（DRV/MEM/GPS/NET） | 4 | 🟢 低 |
| 13 | 蜂鳴器 PWM | 4 | 🟢 低 |
| 14 | 外部 Flash 32 MB SPI NOR（XM25QH256D） | 1 | 🟡 中 |
| 15 | IO Expander AW9523 I²C（16 ch） | 11 | 🟢 低 |
| 16 | 備援電池充電（YX4066HDN8AR） | 8 | 🟢 低（讀 CHRG/FULL） |
| 17 | 電源管理 PMU 進出 sleep | FR3068E-C 內建 | 🔴 高 |
| 18 | OTA 韌體更新（over BLE + over LTE） | 應用層 | 🔴 高 |
| 19 | EUP 終端設備通訊協定（cloud comm） | 應用層 | 🔴 高（spec 未定）|
| 20 | 30 天 log + 斷網重傳 | FlashDB + 外部 Flash | 🔴 高 |

### 1.2 不在 Scope（out of scope）

- **BT A2DP/AVRCP/HFG audio profiles**：libbtdm_host.a 包含但 ET-100 不用
- **FatFS**：若不接 SD card 可省（評估階段後決定）
- **LVGL**：ET-100 無 LCD，SDK 的 lvgl module 不編進去
- **MicroPython**：不走 scripting 路線
- **AUTOSAR**：datasheet 寫系列支援但 Eupfin 不採用

---

## 2. 4 層架構總覽

```text
┌──────────────────────────────────────────────────────────────────────────┐
│  APPLICATION LAYER                                                       │
│                                                                          │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐   │
│  │ Positioning  │ │ Communication│ │  Driver ID   │ │   Vehicle    │   │
│  │   Service    │ │   Service    │ │   Service    │ │ Data Service │   │
│  │ (GNSS track) │ │(LTE/BLE/RS232│ │  (NFC auth)  │ │ (CAN/1-Wire/ │   │
│  │              │ │   bidir)     │ │              │ │  ADC/DI/DO)  │   │
│  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘   │
│                                                                          │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐                     │
│  │    Alert     │ │     OTA      │ │    Config    │                     │
│  │   Service    │ │   Service    │ │   Service    │                     │
│  │(ACC/shock/   │ │ (BLE DFU +   │ │ (EUP proto + │                     │
│  │ power loss)  │ │ LTE download)│ │ BLE/RS232 UI)│                     │
│  └──────────────┘ └──────────────┘ └──────────────┘                     │
├──────────────────────────────────────────────────────────────────────────┤
│  FRAMEWORK LAYER                                                         │
│                                                                          │
│  ┌─────────────────┐ ┌──────────────┐ ┌──────────────┐ ┌───────────┐   │
│  │ FreeRTOS Kernel │ │  Event Bus   │ │  PM / Sleep  │ │  Time Sync│   │
│  │ (SDK provided)  │ │ (pub/sub)    │ │   Policy     │ │(GNSS 1PPS │   │
│  │                 │ │              │ │              │ │ + LTE NTP)│   │
│  └─────────────────┘ └──────────────┘ └──────────────┘ └───────────┘   │
│                                                                          │
│  ┌─────────────────┐ ┌──────────────┐ ┌──────────────┐ ┌───────────┐   │
│  │  EUP Protocol   │ │ Log/Storage  │ │  Peripheral  │ │  Watchdog │   │
│  │     Stack       │ │(FlashDB TSDB │ │  Bus Manager │ │ + Crash   │   │
│  │(spec 待確認)    │ │   + KVDB)    │ │ (I²C/UART)   │ │   Dump    │   │
│  └─────────────────┘ └──────────────┘ └──────────────┘ └───────────┘   │
├──────────────────────────────────────────────────────────────────────────┤
│  DRIVER LAYER                                                            │
│                                                                          │
│  ┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────────┐    │
│  │   LTE   ││  GNSS   ││   NFC   ││G-Sensor ││IO Exp.  ││  CAN    │    │
│  │EG800Q-EU││ AG3352Q ││ PN7160  ││ SC7U22  ││ AW9523  ││ J1939   │    │
│  │  AT eng ││  NMEA   ││  I²C    ││  I²C    ││  I²C    ││SIT1042  │    │
│  └─────────┘└─────────┘└─────────┘└─────────┘└─────────┘└─────────┘    │
│                                                                          │
│  ┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────────┐┌─────────┐    │
│  │  SPI    ││ 1-Wire  ││ 3xRS232 ││ Buzzer  ││   LED   ││ Battery │    │
│  │  Flash  ││DS18B20  ││ (UART)  ││  (PWM)  ││ (GPIO)  ││ Charger │    │
│  │ 32MB    ││         ││         ││         ││         ││ monitor │    │
│  └─────────┘└─────────┘└─────────┘└─────────┘└─────────┘└─────────┘    │
│                                                                          │
│  ┌─────────┐                                                             │
│  │   BLE   │  ← libbtdm_host.a (SDK binary, 317 .o)                     │
│  │ 5.3 BR/ │                                                             │
│  │EDR/BLE  │                                                             │
│  └─────────┘                                                             │
├──────────────────────────────────────────────────────────────────────────┤
│  HAL LAYER (Freqchip SDK components/drivers/peripheral/)                 │
│                                                                          │
│  GPIO | UART×5 | I²C×3 | SPI×2 | QSPI | CAN FD×2 | ADC 12-bit 9ch       │
│  PWM×2(16ch) | RTC | PMU | Flash XIP | Watchdog | Cache (32KB)          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.1 分層原則

- **Application** 只跟 **Framework** 對話（走 event bus），不直接呼叫 driver
- **Framework** 可用 Driver 的 API，但不碰 HAL register
- **Driver** 封裝 HAL；統一的 handle/callback 介面
- **HAL** = SDK 原廠 driver + Eupfin 必要的 override（例：patch #7b runtime clock guard）

---

## 3. FreeRTOS Task 拓撲

### 3.1 Task 清單（含優先序 + stack size 預估）

**FreeRTOS priority**：0（idle）~ configMAX_PRIORITIES-1。越高越優先。
**Stack size**：以 words 為單位（1 word = 4 bytes）；數字為 **初版估計**，EVB 到之後用 `uxTaskGetStackHighWaterMark()` 微調。

| # | Task | Priority | Stack (words) | 執行週期 | 職責 |
|:-:|---|:-:|:-:|---|---|
| 1 | `app_ble_task` | 11（最高） | 1024 | 事件驅動 | BLE adv/connect/GATT/DFU；SDK 內部） |
| 2 | `app_bt_controller_task` | 10 | 1024 | 事件驅動 | BT controller；SDK 內部 |
| 3 | `app_can_task` | 9 | 512 | 事件驅動 | CAN J1939 收發（低延遲） |
| 4 | `app_lte_task` | 8 | 1536 | 事件驅動 | LTE AT engine + modem state machine |
| 5 | `app_proto_task` | 7 | 1024 | 事件驅動 | EUP protocol encode/decode/route |
| 6 | `app_gnss_task` | 6 | 768 | ~1 Hz | NMEA parse + fix state |
| 7 | `app_nfc_task` | 5 | 768 | ~5 Hz polling | PN7160 polling loop + card parse |
| 8 | `app_sensor_task` | 5 | 512 | 10 Hz / event | G-Sensor + ADC + 1-Wire + DI |
| 9 | `app_io_task` | 5 | 512 | 事件驅動 | LED/Buzzer/DO/IO expander control |
| 10 | `app_log_task` | 4 | 768 | 事件驅動 | 寫 flash + 排程重傳 |
| 11 | `app_pm_task` | 3 | 384 | 事件驅動 | sleep policy + wake source management |
| 12 | `app_main_task` | 2 | 1024 | 1 Hz | 統籌、state machine、timer |
| 13 | `monitor_task`（RTT only） | 1 | 2048 | 2 s | FreeRTOS task stat 印出（RTT debug 用） |
| | `IDLE_TASK` | 0 | 256 | — | FreeRTOS 內建 |

**總 stack 預算**：~12 KB words = **~48 KB** 給 task stacks。
BLE/BT controller 的 1024 words 是 SDK typical；若 SDK 另有要求以 SDK 為準。

### 3.2 IPC 模式 — Event Bus（pub/sub）

為避免 N×N 的 queue 爆炸，採**單一 event bus**（+ 每 task 一個 inbox queue）：

```c
typedef enum {
    EVT_GNSS_FIX_UPDATED,      // 來源：app_gnss_task
    EVT_LTE_NETWORK_UP,        // 來源：app_lte_task
    EVT_LTE_NETWORK_DOWN,
    EVT_LTE_RX_PACKET,         // 收到 cloud 下行
    EVT_CAN_FRAME_RECEIVED,
    EVT_NFC_CARD_PRESENT,
    EVT_NFC_CARD_UID,
    EVT_ACC_RISING,            // ACC 高電平 (ignition on)
    EVT_ACC_FALLING,           // ACC 低電平 (ignition off)
    EVT_DIN_EDGE,              // 外部 Input1/2/3 緣觸發
    EVT_IMU_SHOCK,             // G-Sensor 衝擊
    EVT_IMU_MOTION,            // G-Sensor 運動
    EVT_IMU_STATIONARY,        // G-Sensor 靜止超時
    EVT_POWER_LOSS,            // 外部 DC 掉線
    EVT_POWER_RESTORED,
    EVT_BATTERY_LOW,
    EVT_TEMPERATURE_READ,      // 1-Wire 讀到
    EVT_PROTO_CMD,             // Cloud/BLE/RS232 下來的指令
    EVT_OTA_START,
    EVT_OTA_PROGRESS,
    EVT_OTA_DONE,
    EVT_BLE_CONNECTED,
    EVT_BLE_DISCONNECTED,
    EVT_SLEEP_REQUEST,         // PM 要求各 task 進 low power
    EVT_WAKE_FROM_SLEEP,
    // ...
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    uint32_t timestamp;        // RTC-synced tick
    uint16_t source_task_id;
    uint16_t payload_len;
    union {
        gnss_fix_t fix;
        lte_rx_t   rx;
        can_frame_t frame;
        // ...
    } data;
} app_event_t;

// API
void event_bus_publish(const app_event_t *evt);
int  event_bus_subscribe(app_event_type_t type, QueueHandle_t inbox);
```

**好處**：
- 新增 task 不用改既有 task
- 事件 log 容易（publish 時可同時寫 TSDB）
- Mock/test 友善（可注入 fake event）

**缺點**：
- 多一層間接；latency 加幾 µs（對 CAN 影響最大，CAN task 直接 queue bypass bus）

### 3.3 Timer 使用

- **FreeRTOS Software Timers**：給非精確事件（例：每分鐘回報）
- **FR3068E-C 硬體 Timer**：給精確定時（例：GNSS 1PPS 比對、PWM、1-Wire bit timing）

---

## 4. 周邊驅動矩陣（完整）

| # | Driver | HAL instance | Pins (推測) | 控制訊號 | 當前狀態 | 阻擋項 |
|:-:|---|---|---|---|:-:|---|
| D01 | GNSS NMEA | UART5 | PD4 TX / PD5 RX | GNSS_LDO_EN、GNSS_VBAT_EN（經 AW9523） | ⬜ | 需子板 SCH + AG3352Q datasheet |
| D02 | LTE AT engine | UART4（1V8 域） | 經 level shift（Sheet 11）| LTE_VBAT_EN、PWRKEY、RESET、WAKE_MCU | ⬜ | Q08（BJT→PMOS）、Q21（PWRKEY driver）|
| D03 | NFC PN7160 | I²C3（1V8 域） | PB12/PB13 | NFC_1V8_EN、VEN、IRQ、DWL_REQ、WAKE | ⬜ | Sheet 14 NFC antenna matching 實測 |
| D04 | G-Sensor SC7U22 | I²C0（1V8 域） | — | INT1 wake | ⬜ | datasheet（Q35）|
| D05 | IO Expander AW9523 | I²C5（3V3 域） | PD6/PD7 | RSTN、INTN | ⬜ | 設 16 ch 功能表 |
| D06 | CAN J1939 | CAN FD #1 | PB7 TX / PB6 RX | MCU_CAN_STB（經 AW9523）| ⬜ | 二組 CAN pinmux + bit timing（Q07/Q50 相關）|
| D07 | SPI Flash XM25QH256D | SPIM（SPIMX2_1） | PD8/9/10/11 + /CS PD9 | /HOLD、/WP pull-up | ⬜ | — |
| D08 | 1-Wire DS18B20 | GPIO + Timer | 經 Sheet 11 BJT buffer | 1_Wire_Temp_IN/OUT | ⬜ | 自寫 bit-bang（SDK 無） |
| D09 | 3× RS232 | UART1/2/3 | PB4/5/8/9 等 | MCU_RS232_EN1/2/0（經 AW9523） | ⬜ | EN sequence |
| D10 | Buzzer | GPIO 或 PWM | PD15 (BUFFER_CTRL) | BUZZER_3V3_EN | ⬜ | 單音或可調 PWM 擇一 |
| D11 | 4× LED | GPIO ×4 | LED1-4_CTRL | VDD_3V3_EN | ⬜ | 顏色 ↔ CTRL 編號對照 |
| D12 | Battery Charger status | GPIO + ADC | PA10 CHRG、PA11 FULL、ADC VBAT/NTC | MCU_CHA_EN | ⬜ | Q03 battery pin 序 |
| D13 | ADC (4 ch) | ADC | PP4 oil、PP5 WAKE、PP6 VBAT、PP7 NTC | — | ⬜ | ADC Vref 校正 |
| D14 | DI (ACC + 3× external) | GPIO EXTI | PA14/15、PD0 等 | 經 Sheet 11 BJT 偵測 | ⬜ | 5V 邊緣門檻（Sheet 11 §6） |
| D15 | DO (external negative output) | GPIO | PWR_SW_CTRL（經 AW9523）| Q1108 PFET | ⬜ | — |
| D16 | ACC_OUT ×5 pins | GPIO | ACC_INT_OUT（經 AW9523）| Q1110 BJT | ⬜ | 共用 collector 的 fault isolation |
| D17 | PMU / Sleep | SDK PMU | — | — | ⬜ | SDK sleep API 選定 |
| D18 | BLE Stack | libbtdm_host.a | ANT pin 3（經 Sheet 13 BT ANT） | — | ✅ SDK | Advertising + GATT service 自訂 |
| D19 | Watchdog | IWDT | — | — | ⬜ | 超時值定（典型 3-10 s）|

**圖例**：⬜ 未實作、🟡 bring-up 中、🟢 已 bring-up、✅ 可用

---

## 5. 記憶體預算（初版）

Flash 容量**依現有 SDK linker 1016 KiB 計算**（非 datasheet 2 MB，等 Quectel 回 Q48 memory map 再擴）。
SRAM 使用 Phase 1a-ext 已擴到 **512 KiB**。

### 5.1 Flash 預算（總 1016 KiB，單位 KiB）

| 區段 | 預算 | 累計 | 用途 |
|---|:-:|:-:|---|
| Bootloader | 若 Freqchip 原廠 bootloader 占 0-8 KiB（0x08000000-0x08002000） | — | 不在 linker 範圍 |
| Reserved (header/CRC) | 8 | 8 | post_process.py 填 header |
| **App A** | **~400** | 408 | ET-100 主韌體 slot A |
| **App B** | **~400** | 808 | OTA 升級用 slot B |
| **Config partition** | 32 | 840 | KVDB（NVS 類設定、SIM profile、cloud endpoint）|
| **Log partition** | 128 | 968 | TSDB（30 天 log 的 flash cache；主 log 走外部 32 MB SPI Flash）|
| Reserve | 48 | 1016 | 增長緩衝 |

**單一 App slot ~400 KiB**：
- Phase 1a 的 BLE example 占 260 KiB，Eupfin app 預估：
  - 保留 LTE/NFC/CAN/GNSS driver + EUP protocol + log + OTA → 預估 **320-380 KiB**
  - **margin 20-80 KiB**，EVT 階段若超要考慮 app slot 擴大或移到外部 flash

### 5.2 SRAM 預算（總 512 KiB，單位 KiB）

| 區段 | 預算 | 累計 | 用途 |
|---|:-:|:-:|---|
| .data | 36 | 36 | 初始化全域（BLE stack state 含） |
| .bss | 62 | 98 | 未初始化全域 |
| FreeRTOS task stacks | 48 | 146 | 詳見 §3.1 table |
| FreeRTOS heap_6 | 64 | 210 | pvPortMalloc |
| BLE buffer pool | 32 | 242 | ACL/HCI/L2CAP 緩衝 |
| CAN FD RX FIFO | 8 | 250 | 二組 CAN × 32 frame × ~70 B |
| GNSS NMEA buffer | 4 | 254 | 1-2 秒的 sentence |
| LTE AT buffer | 8 | 262 | AT command + URC 緩衝 |
| EUP protocol buffer | 16 | 278 | 編解碼 |
| 1-Wire + ADC + log stage | 8 | 286 | 感測 + log cache |
| newlib malloc heap | 4 | 290 | printf, string ops |
| **已用小計** | **290** | | **≈ 57%** |
| **Reserve** | **222** | 512 | **~43% 給未來擴充** |

### 5.3 PRAM 預算（總 128 KiB）

| 區段 | 預算 | 用途 |
|---|:-:|---|
| .ram_code_front | <1 | 低功耗 critical path |
| .ram_code | ~15 | XIP sensitive（timer ISR / sleep enter） |
| 外部 Flash DMA buffer | 16 | 寫 flash 時的 DMA ping-pong |
| CAN FD mailbox RAM | 可選 8 | 若 SDK 支援 message RAM 在 PRAM |
| **已用小計** | **~40** | **31%** |
| **Reserve** | **~88** | ~69% |

---

## 6. 開機序列（Power-on → Ready）

```text
T=0ms   Hard reset / PWR_IN 到位
          ↓
T=0-5ms Freqchip ROM bootloader：
          - 讀 flash 0x08000000-0x08002000 的 bootloader
          - 驗 app slot header (CRC + magic 0x51525251)
          - 跳到 0x08002000 的 Reset_Handler
          ↓
T=5-10ms SDK startup (startup_fr30xx.S)：
          - set SP = _estack
          - call SystemInit()
          - copy .ram_code_front / .ram_code / .data to RAM
          - zero .bss
          - call main()
          ↓
T=10-20ms app_hw_init()：
          - hw_clock_init() — PLL 設 96 MHz（patch #7a）
          - hw_xip_flash_init(true)
          - GPIO init (UART4 debug pin only)
          - UART4 init @ 921600 for debug
          - PMU init
          ↓
T=20-30ms FreeRTOS kernel start：
          - 建 app_main_task、app_pm_task、app_log_task
          - vTaskStartScheduler()
          ↓
T=30ms  app_main_task 開始跑：
          步驟 1：I²C bus EXP_I2C5 init → AW9523 reset + 全部 output = LOW
          步驟 2：I²C 開 VDD_3V3_EN、NFC_1V8_EN、BUZZER_3V3_EN、GNSS_VBAT_EN
          步驟 3：等 100ms LDO 穩壓
          步驟 4：LED 自測序列（Driver 黃 → Memory 綠 → GPS 紅 → Net 藍 各 100ms）
          步驟 5：Buzzer 短 beep（50ms）確認 audio 作動
          ↓
T=130ms app_main_task 續跑：
          步驟 6：I²C bus Sensor_I2C0 init → G-Sensor SC7U22 bring-up
          步驟 7：I²C bus NFC_I2C3 init → PN7160 reset + firmware check
          步驟 8：SPI init → XM25QH256D ID 讀取 + FlashDB mount
          步驟 9：建立 app_sensor_task、app_io_task
          ↓
T=300ms app_main_task 續跑：
          步驟 10：UART5 init → 開 GNSS LDO EN → 建 app_gnss_task
          步驟 11：UART4 init（PA12/13 經 Sheet 11 shift）→ LTE_VBAT_EN 開 → PWRKEY 500ms 低平 → 建 app_lte_task
          步驟 12：CAN FD init → 建 app_can_task
          步驟 13：UART1/2/3 init（三顆 UM3221）→ 開 EN
          步驟 14：BLE stack start → advertising 開跑
          步驟 15：建 app_proto_task、app_ble_task (SDK 內部)
          ↓
T=500ms 系統 READY：
          - LED 進入正常指示模式
          - Monitor task (RTT only) 開始週期性報告
          - 開始接收 cloud / BLE / RS232 下行指令
```

**關鍵時序約束**：
- **T=10ms Patch #7b 的 156 MHz guard** 在 `System_MCU_clock_Config()` 內已就位
- **T=20ms AW9523 開 LDO** 到 **T=30ms LDO 穩定**：100ms margin 給 WR0338 LDO 的 soft start (<1ms typical, 100ms 保守)
- **T=300ms LTE PWRKEY**：datasheet 要 ≥500ms low pulse，與 VBAT_EN 之間要 ≥30ms（Q21）
- **T=500ms**：Hot start 情境下的 ready time；cold start LTE 註網另算 10-60s

---

## 7. 開發階段 Roadmap

**原則**：每階段都有可展示的里程碑 + 可寫 REVIEW_REPORT。**不在 EVB 可做的任務盡量提前**。

### Phase 1a / 1a-ext — Toolchain 驗證（**已完成** 2026-10-02）

- ARM GCC 14.3.1 + CMake 可 build Freqchip SDK
- `Project_burn.bin` 可產出
- 5 → 7 處 vendor SDK patch 文件化
- 詳見 [REVIEW_REPORT.md](../../REVIEW_REPORT.md)

### Phase 1b — Freqchip EVB 到貨驗證（**等 EVB**，無 EVB 無法做）

| # | 工作 | 預期時間 |
|:-:|---|:-:|
| 1b.1 | 燒 `Project_burn.bin` 到 EVB → 看 BLE 廣播 | 1 天 |
| 1b.2 | J-Link SWD 連線 debug，驗證 Cortex-Debug 可用 | 1 天 |
| 1b.3 | UART3 921600 看 FreeRTOS printf 輸出 | 0.5 天 |
| 1b.4 | 寫最小 blink LED on EVB（GPIO 第一步） | 1 天 |
| 1b.5 | UART echo test（驗 UART driver） | 1 天 |
| 1b.6 | I²C scanner on EVB（驗 I²C master） | 1 天 |
| **1b 總計** | | **~5 天** |

### Phase 2a — HAL 包裝（可部分**平行**跑，不全需 EVB）

| # | 工作 | 可否無 EVB | 時間 |
|:-:|---|:-:|:-:|
| 2a.1 | 建 fw/src/ 與 fw/include/ 的目錄骨架 | ✅ 無需 EVB | 0.5 天 |
| 2a.2 | 寫 Event Bus（§3.2） | ✅ 無需 EVB | 2 天 |
| 2a.3 | GPIO HAL wrapper | ⚠ 寫代碼無需 EVB，測試需要 | 1 天 |
| 2a.4 | UART HAL wrapper + ring buffer | ⚠ 同上 | 2 天 |
| 2a.5 | I²C HAL wrapper + bus manager | ⚠ 同上 | 2 天 |
| 2a.6 | SPI/QSPI HAL wrapper | ⚠ 同上 | 1 天 |
| 2a.7 | ADC HAL wrapper | ⚠ 同上 | 1 天 |
| 2a.8 | CAN FD HAL wrapper | ⚠ 同上 | 2 天 |
| 2a.9 | PWM HAL（給 buzzer） | ⚠ 同上 | 0.5 天 |
| 2a.10 | Timer HAL（給 1-Wire bit timing） | ⚠ 同上 | 1 天 |
| **2a 總計** | | | **~13 天** |

**平行度**：2a.1、2a.2 完全可在 EVB 到之前做；其餘 code 可寫但 unit test 要等 EVB。

### Phase 2b — Device Driver（周邊 chip 的驅動）

| # | Driver | 需要 EVB/EVT？ | 時間 |
|:-:|---|:-:|:-:|
| 2b.1 | SPI Flash XM25QH256D | EVB（SDK 有 driver，驗證即可）| 1 天 |
| 2b.2 | G-Sensor SC7U22 | EVB（I²C slave 可接 EVB）| 2 天 |
| 2b.3 | IO Expander AW9523 | EVB（I²C slave 可接 EVB） | 1 天 |
| 2b.4 | PN7160 NFC | EVB（I²C slave 可接 EVB） | 3 天 |
| 2b.5 | 1-Wire DS18B20 | EVB（external DS18B20 可接）| 2 天 |
| 2b.6 | LTE AT engine (EG800Q-EU) | **EVT**（EVB 不含 LTE module）| 5 天 |
| 2b.7 | GNSS AG3352Q NMEA | **EVT**（EVB 不含 GNSS 子板）| 2 天 |
| 2b.8 | CAN J1939 protocol stack | **EVT** 或外接 CAN transceiver 板 | 5 天 |
| **2b 總計** | | | **~21 天** |

**EVB-era vs EVT-era 分野**：2b.1-2b.5 可用「EVB + 接 slave module」做（成本可接受）；2b.6-2b.8 需 ET-100 板上實體。

### Phase 3 — Framework & Services

| # | 工作 | 時間 |
|:-:|---|:-:|
| 3.1 | Power Management policy（進出 sleep、wake source）| 3 天 |
| 3.2 | Time Sync（GNSS 1PPS 校正 + LTE NTP backup） | 2 天 |
| 3.3 | Log/Storage（FlashDB TSDB + 外部 SPI Flash）| 4 天 |
| 3.4 | EUP Protocol Stack（**等 EUP spec**）| 10 天 |
| 3.5 | OTA framework（dual-bank + rollback + sign verify） | 7 天 |
| 3.6 | Config Service（KVDB + BLE/RS232 config UI） | 3 天 |
| **3 總計** | | **~29 天**（含等 EUP spec 的 blocker）|

### Phase 4 — Application Services（高階業務邏輯）

| # | 工作 | 時間 |
|:-:|---|:-:|
| 4.1 | Positioning Service（GNSS tracking + fix quality + AGPS）| 3 天 |
| 4.2 | Communication Service（LTE primary + BLE secondary + RS232 tertiary）| 4 天 |
| 4.3 | Driver ID Service（NFC auth + session management） | 2 天 |
| 4.4 | Vehicle Data Service（CAN J1939 PGN + 1-Wire temp + ADC）| 4 天 |
| 4.5 | Alert Service（ACC/motion/power loss/shock/geofence） | 3 天 |
| **4 總計** | | **~16 天** |

### Phase 5 — 整合測試 + 認證準備

| # | 工作 | 時間 |
|:-:|---|:-:|
| 5.1 | 並行壓力測試 | 3 天 |
| 5.2 | 低溫 (-40°C) / 高溫 (85°C) functional test | 3 天 |
| 5.3 | 電源失效 + 掉電 + brown-out 復原測試 | 2 天 |
| 5.4 | 功耗量測 PPK2（15s/60s/300s 回報三組 scenario）| 3 天 |
| 5.5 | 認證測試輔助（NCC/BSMI/MIC/SIRIM/NBTC/CE） | 5+ 天 |
| **5 總計** | | **~16+ 天** |

### 總工期估算（純 FW 單人 + AI）

| Phase | 時間 |
|---|:-:|
| Phase 1a + 1a-ext | ✅ 完成 |
| Phase 1b | ~5 天 |
| Phase 2a | ~13 天（可與 1b 平行一半）|
| Phase 2b | ~21 天 |
| Phase 3 | ~29 天（含 blocker buffer）|
| Phase 4 | ~16 天 |
| Phase 5 | ~16+ 天 |
| **純 FW 工時合計** | **~100 工作天 ≈ 5 個月**（solo + AI、不含阻擋等待） |

對照提案書 [§P47 Quectel ODM 時程](../../../docs/00_project/07_專案時程.md)：T0+6 月 DVT、T0+9 月 MP。**本 FW 開發需跟上 DVT 的 6 個月窗口**，純 FW 5 個月時間剛好可行，但無 slack。

---

## 8. Build Matrix

| Build Target | 目的 | Clock | 配置檔 |
|---|---|:-:|---|
| `build_debug` | 開發期常用，含 printf、assert、full debug symbol | 96 MHz | `configs/debug/prj_debug.conf` |
| `build_rtt_debug` | SEGGER RTT 無侵入 debug（取代 UART printf） | 96 MHz | `configs/debug/prj_rtt_debug.conf` |
| `build_release` | 正式燒錄版本，-Os、assert off、不含 symbol | 96 MHz | `configs/release/prj_release.conf` |
| `build_measure` | 功耗量測專用，所有周邊預設 off | 96 MHz 可切 48 MHz | `configs/debug/prj_measure.conf` |
| `build_boot` | 單獨 build bootloader（Phase 3 以後）| 96 MHz | `configs/boot/prj_boot.conf` |

**對應 comm.md 快捷指令**：
- `#b` → `cmake --build build_debug`（預設）
- `#rttb` → `cmake --build build_rtt_debug`
- `#release vX.Y.Z` → 跑 `tools/release.sh` 產 `release/vX.Y.Z/`

---

## 9. 外部依賴（Blockers）

本計畫執行進度受以下外部答覆影響，依優先序：

### 必答才能開始（阻擋 Phase 2b-3）

| 項目 | 來源 | 影響 |
|---|---|---|
| EVB 到手 | Freqchip 直購 / Quectel 借 | Phase 1b 全部 |
| Freqchip FR3068E-C memory map | Q48 | SRAM/Flash 分配 § 5 |
| Freqchip signed BSP source | Q39 | BLE QDID 歸屬 |
| Secure boot signing key | Q42 | 量產板能否燒自寫 FW |
| OTA bootloader + format | Q43 | § 3.5 OTA Service 設計 |
| **EUP 終端設備通訊協定 spec** | Eupfin 內部考古 | § 3.4、§ 4.2 全部 |
| Quectel FW behavior spec | Q45 | § 4 相容性 |

### 可邊做邊等（阻擋 Phase 4）

| 項目 | 來源 | 影響 |
|---|---|---|
| EVT 樣品板 | Quectel SOW 後 T0+3 月 | § Phase 2b 的 LTE/GNSS/CAN 要等 |
| LTE antenna tuner control protocol | Q09、Q44 | § 4.2 LTE 效能 tuning |
| Clock 156 vs 192 MHz 真相 | Q50 | 若 datasheet 錯/對有不同策略 |
| SVD 檔 | Q47 | debug 效率（不致命） |

---

## 10. 設計未定（Open Questions）

本計畫中以下設計**尚未定稿**，記錄下來等資訊充分後再決定：

1. **CAN J1939 哪個 instance**：FR3068E-C 有 2 組 CAN FD，先用哪個？第二組預留嗎？
2. **1-Wire bit-bang vs SDK**：SDK 無 1-Wire driver，自寫 bit-bang 需精確 µs timing；是否值得走 PWM/Timer 的 hw 加速？
3. **BLE adv name 格式**：`ET-100-<serial>` 還是 `EUP-ET100-<MAC>`？與既有 Eupfin app 相容性？
4. **Log 寫哪**：內建 2MB Flash 的 log partition（128 KiB）vs 外部 32MB SPI Flash？分流策略？
5. **OTA source**：LTE 下載為主、BLE DFU 為備？還是雙模都要？
6. **Config endpoint**：BLE + RS232 哪個是正式 UI？
7. **UART 分配**：UART0-UART5 共 6 條，但本板只用 5 條（LTE/GNSS/RS232×3），UART0 給 Debug console 還是備用？
8. **Watchdog policy**：3s 短 timeout vs 30s 長 timeout；FreeRTOS task-aware watchdog 自寫 or SDK 原生？

---

## 11. Code 結構（提案）

```text
fw/
├── src/
│   ├── app/                  # Application Layer
│   │   ├── app_main.c         # 統籌 state machine
│   │   ├── app_positioning.c
│   │   ├── app_comm.c
│   │   ├── app_driver_id.c
│   │   ├── app_vehicle.c
│   │   ├── app_alert.c
│   │   └── app_ota.c
│   ├── framework/            # Framework Layer
│   │   ├── event_bus.c
│   │   ├── pm_policy.c
│   │   ├── time_sync.c
│   │   ├── eup_protocol.c
│   │   ├── log_storage.c
│   │   └── watchdog.c
│   ├── driver/               # Driver Layer
│   │   ├── drv_lte.c          # EG800Q-EU
│   │   ├── drv_gnss.c         # AG3352Q NMEA
│   │   ├── drv_nfc.c          # PN7160
│   │   ├── drv_imu.c          # SC7U22
│   │   ├── drv_io_ext.c       # AW9523
│   │   ├── drv_can.c          # SIT1042 + J1939
│   │   ├── drv_spi_flash.c    # XM25QH256D
│   │   ├── drv_onewire.c      # DS18B20
│   │   ├── drv_rs232.c
│   │   ├── drv_buzzer.c
│   │   ├── drv_led.c
│   │   └── drv_battery.c
│   ├── hal/                  # HAL (SDK wrapper)
│   │   ├── hal_gpio.c
│   │   ├── hal_uart.c
│   │   ├── hal_i2c.c
│   │   ├── hal_spi.c
│   │   ├── hal_can.c
│   │   ├── hal_adc.c
│   │   ├── hal_pwm.c
│   │   ├── hal_timer.c
│   │   └── hal_pmu.c
│   └── main.c                # entry → app_hw_init() → vTaskStartScheduler()
├── include/
│   ├── app/
│   ├── framework/
│   ├── driver/
│   ├── hal/
│   └── et100_config.h         # project-wide config
├── boards/
│   ├── freqchip_evb/          # Freqchip EVB 的 pin 配置
│   └── et100_evt/             # ET-100 EVT 的 pin 配置
└── configs/
    ├── debug/
    ├── release/
    └── measure/
```

---

## 12. 下一步

**本檔是 Draft v0.1**，等以下事件之一觸發 v0.2：
- Quectel 回覆 100_questions V1.3 的 P0 / P0-FW 區（主要 Q48 memory map、Q42 secure boot、Q43 OTA）
- EUP 協定考古有結果
- Freqchip EVB 到手開始 Phase 1b
- Codex / Antigravity 執行各 Phase 時發現本計畫假設有誤

**本檔 commit 之後**：
1. 依 ADR 02 的 patch 政策、comm.md 的 workflow，開始跑 Phase 2a 的「無需 EVB 可做」子任務（§7.2a）
2. 新增 `fw/docs/01_firmware/02_event_bus_design.md`（§3.2 的 detailed spec）
3. 新增 `fw/docs/01_firmware/03_peripheral_pinmap.md`（§4 的 full pin assignment matrix，等 Q48 memory map 回來才能定稿）
