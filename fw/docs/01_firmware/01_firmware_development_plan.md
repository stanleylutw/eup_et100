# ET-100 Firmware Development Plan — DRAFT

> **2026-10-02 Codex 補充審查**：原草案保留。可實作的責任分工與 baseline 見 [架構草案](02_firmware_architecture_draft.md)、[硬體覆蓋／衝突矩陣](03_hardware_coverage_matrix.md)、[分階段執行計畫](04_development_execution_draft.md)。本檔的 task priority 10/11、固定 OTA partitions、部分 pin/rail 假設及記憶體預算不可直接作為施工設定；有衝突時先依補充文件的 gate 查證，並經批准後實作。

**Version**：v0.2 DRAFT
**Last updated**：2026-10-21
**Author**：Claude Code + Stanley (Eupfin)
**Status**：Draft — subject to revision as Quectel/Freqchip answers come back from [100_questions_for_quectel V1.4](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md)

> **本檔定位**：ET-100 韌體**開發計畫 + 架構草案**，承接 [00 Platform Plan](../00_project/00_et100_firmware_platform_plan.md) 的 WHY/WHAT，寫 HOW。
> **與相鄰文件分工**：
> - [fr306x_reference/05 ET-100 開發對照](../../../docs/03_hardware/fr306x_reference/05_et100_development_reference.md)：bring-up 順序 + 各功能 check list（**本檔不重複**）
> - [sch_analysis/](../../../docs/03_hardware/sch_analysis/)：HW 真實樣貌（本檔引用，不複製）
> - 本檔：**4 層架構、FreeRTOS task 拓撲、驅動矩陣、記憶體預算、階段產出、build matrix**

> **2026-10-21 OTA 決策**：Stanley 採完整 2 MB 邏輯規劃、外部 NOR Candidate／Rollback／Sensor slot、LTE／BLE／RS232 三入口；不採晶片內 A/B。[OTA 儲存規格](06_ota_storage_design.md) 為本議題的唯一詳細契約；仍須 Q43-5／Q48-6 與實測，沒有 SDK／linker／FlashDB 修改授權。舊草案將 header 當原廠 bootloader、雙扣 header／假定 ROM 會從 NOR 安裝的說法由本版更正。

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
| 18 | 韌體更新（**LTE + BLE + RS232 三入口，共用一套 core**，另支援 sensor relay，見 §2.0a）| 應用層 | 🔴 高 |
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

### 2.0a OTA 設計：三入口 + 共用一套 core（更新決策 2026-10-21）

ET-100 走 **「LTE／FTP、BLE、RS232 三個入口，共用一套更新核心」**。RS232 是有線更新，正常 9600、協商後 115200／8N1；需求及取消 Factory recovery／改留 sensor slot 均由 Stanley 在本對話明確指定，不是 Codex 擅自增加。

```text
LTE：伺服器 → EG800Q → UART ──┐
BLE：手機 App → BLE GATT ─────┼─▶  OTA Manager  ─▶  NOR Candidate／Sensor slot
RS232：有線 host → UART ──────┘           │
                                           ▼
                                  驗證 → 安裝 → 啟動確認
```

**transport 介面現況**：

| 介面 | 可行性與現況 | 參考 |
|---|---|---|
| LTE OTA | EG800Q 負責網路下載，FR3068E-C 接收並儲存。原廠有 FTP(S)／HTTP(S) 文件；專案尚未完成這條流程 | Quectel FTP(S) 文件 |
| BLE App OTA | SDK 已有 OTA GATT service（`service_ota.c`）支援資料寫入與通知回覆；手機 App 需實作對應協定；尚未實機驗證 | [`vendor/fr30xxc_sdk__202411/examples/common/ota/service_ota.c:59`](../../vendor/fr30xxc_sdk__202411/examples/common/ota/service_ota.c) |
| RS232 update | 正常 9600 協商後切 115200／8N1；binary chunk／offset／CRC／ACK／續傳，取消／逾時回正常速率；尚未實作 | [OTA 儲存規格 §6](06_ota_storage_design.md#6-三個-transport兩種-target) |

**三條路徑必須共用**：

- 同一份更新映像與版本／型號資訊
- 同一套 Flash 暫存、hash／簽章驗證與安裝流程
- 同一套斷點續傳、進度回報與斷電恢復狀態
- **同時只允許一個 update session**（LTE/BLE/RS232、ET100/sensor target 均互斥），不交錯覆寫資料；內建 state 是安裝／boot 唯一權威，NOR journal 只管下載

**使用情境分工**（不是 primary/secondary 階層，是**對等** transport）：

- LTE：適合**遠端車隊更新**（fleet rollout）
- BLE：適合**現場維修**；也可讓 App 使用預先下載的檔案，在無 LTE 網路時更新
- RS232：適合有線維修／受控更新；不因此把所有 RS232 ports 都當 OTA console

**資源／吞吐量尚待驗證**：96 MHz 下的並行負載、BLE 吞吐量、UART 流控、Flash 寫入需實測，不能因選外部 staging 就斷言沒有 MCU 資源限制。

**OTA 檔案實際存放**（詳見 §5.3a）：
- 下載時：外部 NOR 的 ET100 Candidate 2 MB 或 Sensor slot 256 KB；ET100 Rollback 另留 2 MB
- 安裝時：**Eupfin 自有 installer** 從 NOR staging 讀 → copy 到內建 App slot（§5.1）；不是原廠 ROM 負責 NOR copy
- 內建 Flash **不做 App A/B**；新候選為 header 8 KB＋installer 120 KB＋App 1792 KB＋Settings 112 KB＋boot state 16 KB，總 2 MB；現行 1016 KB linker／FlashEnv 尚未變更

**真正的最大未知**（非三個傳輸入口本身）：
- **Eupfin 自有 installer** 的安全安裝實作：NOR → 內部 App copy、驗簽、先備份再 erase、斷電恢復；不能假定原廠 ROM 會執行這條流程
- **Q43-5 只確認 ROM／installer 邊界**：ROM header code_length／CRC 範圍、options 與啟動自有 installer 的規則，不是等待原廠提供 NOR copy 功能
- 斷電恢復（任何階段 power loss 都要可救；外部 staging 寫壞不影響 live app）
- **rollback** 條件與機制（external staging 保留上一版 image 可救援）

**開發順序**：host model／NOR → installer／斷電復原 → RS232 → BLE → LTE → sensor relay。需先證實 installer 可獨立恢復，不能拿 transport 下載成功代替安全安裝驗收。

對應補充文件：
- [`06_ota_storage_design.md`](06_ota_storage_design.md)：候選地址、state 權威、rollback／VTOR 與 ROM gates
- [`02_firmware_architecture_draft.md`](02_firmware_architecture_draft.md) §services：ota_manager 統一 session／下載／驗證／install request，三入口不各自改 boot state
- [`04_development_execution_draft.md`](04_development_execution_draft.md) M5：三入口、驗簽、NOR staging、自有 installer 與 boot recovery

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

現行 runtime 配置仍依 SDK linker 1016 KB；新產品候選以完整 **2 MB** 規劃，兩者分開列示。[08 實測報告](../04_testing/08_evb_internal_flash_size.md)記錄 EP-C EVB 2 MB 容量及上半區 sector erase／program，相關[唯讀 probe](../../tools/evb/flash_size_probe.py)／[sector 工具](../../tools/evb/flash_sector_test.py)已恢復、本輪未執行。產品 E-C 完整 map 與 runtime writes 仍待 Q48-6；Q43-5 只確認 ROM／installer 邊界。
SRAM 使用 Phase 1a-ext 已擴到 **512 KiB**。

### 5.1 內建 MCU Flash 候選（總 2 MB）— **單一 App＋NOR staging**

> **設計決策 2026-10-21**：延續原 NOR staging 方向，取消內部 A/B 候選；自有 installer 從外部 NOR 安裝，不宣稱原廠 mask ROM 已支援 NOR 安裝。詳細契約見 [06 §2／§4／§5](06_ota_storage_design.md)。

| 區域 | 起始 | 結束（含） | KB |
|---|---|---|---:|
| FR509x header | 0x08000000 | 0x08001FFF | 8 |
| Installer | 0x08002000 | 0x0801FFFF | 120 |
| **App slot** | **0x08020000** | **0x081DFFFF** | **1792** |
| Settings／KV | 0x081E0000 | 0x081FBFFF | 112 |
| Boot／Update state | 0x081FC000 | 0x081FFFFF | 16 |
| **總計** | 0x08000000 | 0x081FFFFF | **2048** |

Header＋installer 合計 128 KB，App 上限包含 App 驗證／封裝開銷。ROM header 的 code_length／CRC 只涵蓋 installer 的方案須 Q43-5 確認；一般 OTA 不重寫 header。Installer 自帶 NOR／Flash driver 與驗簽，安裝途中不可依賴 App。App relocation／VTOR／睡眠還原須驗證。

內建 state 是安裝／trial／確認／rollback 唯一權威；NOR journal 只管下載。正常 confirmed boot 不初始化 NOR。16 KB state 暫分雙份 8 KB banks（各兩個 4 KB sectors），須驗證 record／GC／wear。Settings 留內部 112 KB；telemetry 與候選／rollback／sensor package 放外部 NOR。

**現行 FlashDB 風險**：FlashEnv 仍在 0x080C8000、12 KB，與 SDK linker app window 重疊。現有 build guard 禁止 burn image 越過 offset 800 KB。新 App slot 會覆蓋舊 FlashEnv，因此 schema／bonding／設定的可重入遷移是前置 gate；本輪不改 FAL／linker，不可把候選表當已落地分區。

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

### 5.3a 外部 SPI NOR Flash 預算（XM25QH256D，總 32 MiB）

> 詳見 [`02_firmware_architecture_draft.md`](02_firmware_architecture_draft.md) §7。本節是 01 計畫的對應投影。

| 區域 | NOR offset（起始..結束，含） | 容量 |
|---|---|---:|
| Download journal／metadata | 0x000000..0x01FFFF | 128 KB |
| ET100 Candidate | 0x020000..0x21FFFF | 2 MB |
| ET100 Rollback | 0x220000..0x41FFFF | 2 MB |
| Sensor OTA | 0x420000..0x45FFFF | 256 KB |
| Logs／其他資料 | 0x460000..0x1FFFFFF | 27.625 MB |

**Candidate／Rollback 設計意圖**：
- Candidate 2 MB 須容納完整包裝，但內部 App slot 仍限 1792 KB（含 App image overhead），不能按 2 MB 目的區安裝
- 安裝前把內部 confirmed App 複製到 Rollback 並讀回驗 hash；已有相同 hash 的實際有效備份可跳過重寫；trial App 不可取代 confirmed 備份
- Sensor 實體 slot 256 KB，但 package 必須 **≤225280 B（含 256 B header）**，沿用 Gateway 上限，不接受放得下但不相容的檔案
- Logs／其他內部仍需切 24 MB telemetry、diagnostics 與 wear reserve；容量不等於已驗證 30 天保存
- 寫入 external 不干擾 live app 執行（SPI 另一條 bus）
- Download 階段隨便 abort / resume，不碰 live app
- Install 階段由自有 installer 讀 NOR → write internal → verify → trial／confirm；一般 confirmed boot 不讀 NOR，完整安全條件見 06

**關鍵依賴**：
- **自有 installer 實作**：由 Eupfin installer 負責 NOR → 內部 App copy／驗證／rollback，不要求原廠 ROM 具有 NOR install 功能；[Q43-5](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md) 僅確認 ROM header／CRC／options／啟動 installer 邊界，不能代替自有 installer 開發與斷電測試
- XM25QH256D 的 **16 MiB 邊界**（32-bit addressing vs 24-bit）實測，避免 alias（Codex 02 §7 已標為待驗）
- 外部 NOR／內部 state 的 erase endurance 與 metadata 寫入頻率依 exact part 規格及磨耗模型驗證；不以泛稱 typical 100k 或每月更新估計取代 journal／GC 分析

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

下圖前段依 NOR installer 候選修正；後段 T 時間與 board init 仍是早期草案估值，不是實測時限。Installer 的 confirmed fast path 不啟 NOR；待安裝／rollback 路徑見 06。

```text
T=0ms   Hard reset / PWR_IN 到位
          ↓
ROM boot（範圍／options 待 Q43-5）：
          - 讀 0x08000000 的 FR509x header（保留區 8 KB，不是 Flash-resident 原廠 bootloader）
          - 依 header 驗 installer 載入映像並啟動 @0x08002000
          ↓
自有 installer（候選、未實作）：
          - 讀內部唯一 boot state；confirmed 路徑不初始化 NOR
          - 待安裝／rollback 才啟 NOR，先備份 confirmed App 並驗證，再允許 erase
          - 驗內部 App／vectors，設 VTOR=0x08020000／MSP 後跳 App
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
| 3.5 | 更新 framework（三入口共用 core；NOR staging／單 App／簽章／先備份後 erase／installer 斷電恢復；先 RS232、再 BLE／LTE／sensor relay，見 §2.0a／06）| 原估 10 天，須依 boot gates 重新估時 |
| 3.6 | Config Service（KVDB + BLE/RS232 config UI） | 3 天 |
| **3 總計** | | **~29 天**（含等 EUP spec 的 blocker）|

### Phase 4 — Application Services（高階業務邏輯）

| # | 工作 | 時間 |
|:-:|---|:-:|
| 4.1 | Positioning Service（GNSS tracking + fix quality + AGPS）| 3 天 |
| 4.2 | Communication Service（LTE + BLE + RS232 三路）— **LTE modem_service 詳細設計見 [`05_modem_service_design.md`](05_modem_service_design.md)**（state machine、auto APN、PDP、TCP buffer mode、URC parser、PSM 等）| 4 天 |
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
| OTA installer + format + safe install + power-fail recovery + rollback | Q43-5／Q48-6 | 三入口需求已定，但 wire contract 未凍結；ROM header 範圍、VTOR、RAM-code、FlashEnv 遷移及斷電恢復仍阻擋實作 |
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
4. **Log policy**：位置已選外部 NOR Logs／其他區；record bytes／事件配額／30 天保存／滿容量與 wear 分流仍待定，不占內部 App／boot state。
5. **更新入口已確認 2026-10-21**：LTE／FTP、BLE／App、RS232 三入口共用核心，見 §2.0a／06；RS232 更新 115200／8N1。取消 Factory recovery、Sensor slot 256 KB／package ≤225280 B 已確認。
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
- Quectel 回覆 100_questions V1.4 的 P0 / P0-FW 區（主要 Q48 memory map、Q42 secure boot、Q43 ROM／installer 邊界）
- EUP 協定考古有結果
- Freqchip EVB 到手開始 Phase 1b
- Codex / Antigravity 執行各 Phase 時發現本計畫假設有誤

**本檔 commit 之後**：
1. 依 ADR 02 的 patch 政策、comm.md 的 workflow，開始跑 Phase 2a 的「無需 EVB 可做」子任務（§7.2a）
2. 新增 `fw/docs/01_firmware/02_event_bus_design.md`（§3.2 的 detailed spec）
3. 新增 `fw/docs/01_firmware/03_peripheral_pinmap.md`（§4 的 full pin assignment matrix，等 Q48 memory map 回來才能定稿）
