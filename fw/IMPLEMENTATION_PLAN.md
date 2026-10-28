# IMPLEMENTATION_PLAN — v0.9.7 CAN J1939（V1.9 CAN box 功能移植到 ET-100）

> 狀態：**已核准，交 Codex 實作**（Stanley 2026-10-28：版號 v0.9.7、同意 D1–D6）。
> 分工（comm.md 標準規則）：Claude 寫施工單／review，**Codex 實作**，**Stanley 實機測試**。

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.9.7_can_j1939

Base branch: main（c91cdd2）

版號：依 comm.md 新增對外介面應升 minor，**Stanley 決定維持只遞增 z**（v0.9.7）。

## 0. 決策（Stanley 2026-10-28）

| # | 決策 |
|---|---|
| D1 | **規格沿用 V1.9**：`eup_gateway/docs/02_protocol/J1939轉譯器通訊協定_V1.9.md` 與 F407 的專案解釋（D3–D16，見 `~/Firmware/EUP/eup_canbus_32f407/docs/01_firmware/01_ota_smp_rtos_design.md`「v0.2.6 J1939 V1.9 Step 1」）。輸出 `AT$PD=67;<type>;…*XX`（Type 0–4）、指令 `CAN$`（S／G TYPESWITCH、GTIME、CARTYPE、BITRATE，GVERSION），HTML「CAN 指令」分頁不改 |
| D2 | **只聽**：CAN controller 一律 **bus monitoring mode**（不送 ACK、不送任何 frame、不送 J1939 request）。與 F407 不同（F407 鎖定速率後轉 NORMAL），本專案任何情況都不轉 NORMAL |
| D3 | **移植 F407 HAL-free 模組並保持內容不變**，連同 host 測試與測試向量；HAL／RTOS 相關部分以 FR3068 SDK 重寫 |
| D4 | 第一版輸出走 RS232（EVB：UART3），與 `BLEGW$` 共用 TX mutex；LTE 上傳、本地儲存不在本版 |
| D5 | 開發環境：EVB CAN3＋F407 DK 當 J1939 訊號源；bus 上需有會 ACK 的正常節點（見 §4） |
| D6 | ET-100 實板（MCAN1 PB6／PB7、AW9523 P0_2 VIO／P1_6 STB、R0521 Q49）**不在本版**，另開分支 |

## 1. 參考來源（F407 repo，基準 commit `a730897`）

| F407 檔案 | 狀態 | ET-100 處理 |
|---|---|---|
| `app/j1939.c／.h`（116 行） | 無 HAL／RTOS | **原樣移植** |
| `app/app_can_protocol.c／.h`（331 行） | 無 HAL／RTOS | **原樣移植**（`CanProtocol_Parse／Reply／Candidate`、`CanConfig_Valid`） |
| `app/app_can_bitrate.c／.h`（167 行） | 無 HAL／RTOS | **原樣移植**；`CanRateApply_t(kbps, silent)` 的 `silent` 在 ET-100 一律以 bus monitoring 實作（D2） |
| `app/app_gateway.c／.h`（593 行）、`app/j1939_sheet_tables_gen.h` | 無 HAL／RTOS | **原樣移植**（Type 0–4 組行、TTL、checksum、`AppGateway_Frame／Poll／Wire／BootLine`） |
| `app/app_demo_j1939_frames.h` | 資料 | 移植為 **host 測試資料**（不進 App build） |
| `docs/02_protocol/j1939_v19_rules.csv`、`tests/host/fixtures/j1939_v19_vectors.json` | 規則／向量 | 複製到 `fw/docs/02_protocol/`、`fw/tests/fixtures/` |
| `tests/host/test_gateway.c`、`test_can_protocol.c`、`test_can_bitrate.c`、`test_demo_frames.c` | host 測試 | 移植並納入 host 腳本；fakes 依需要調整 include，**斷言不得放寬** |
| `app/app_can_settings.c`、`app_canmon.c`、`app_cmd.c` | HAL／RTOS／NOR | **不移植**，以 §2 新寫 |

規則：移植檔案開頭加來源註記（repo、檔案、commit `a730897`）；內容修改僅限 include 路徑與型別轉接，若必須改邏輯先提問。

## 2. ET-100 新增部分

```text
CAN3 IRQ (PA10 RX / PA11 TX, GPIO_FUNCTION_5)
  → ISR 讀 RX FIFO0 → 固定大小 ring（64 frames，溢位計數，不配 heap）
  → can task（新 task，優先序同 serial，stack 依實測）
       AppGateway_Frame(id, data, dlc, now) ＋ CanBitrate_Observe(now, good)
       每 50 ms AppGateway_Poll(now)、CanBitrate_Poll(now)
  → 輸出 callback：完整一行交給 serial task 以既有 TX mutex 送出（不得在 ISR 或 critical section 內送）
serial_gchk.c：行首 "CAN$" → CanProtocol_Parse／Candidate／Reply（含 checksum，回覆附 checksum）
設定：FlashDB 新 key `ET100_FDB_CAN_KEY 0x00030009`（登記於 et100_fdb_keys.h），保存 TYPESWITCH／GTIME／BITRATE／CARTYPE；
      寫入後讀回驗證才回成功；記錄無效用 F407 預設（1:1:1:1、30、0、1）
```

| 項目 | 規格 |
|---|---|
| Controller | CAN3（`CAN3_BASE`），classic CAN 2.0B（不開 FD／BRS），只收 29-bit extended（標準 frame 以 filter 拒收） |
| 速率 | 250／500 kbps；sample point 落在 80–87.5 %（J1939-11），以 SDK 時脈實算並在 30 報告記錄 prescaler／segment |
| 模式 | `can_enter_bus_monitoring_mode()` 永遠開啟；auto bitrate 掃描與鎖定沿用 `app_can_bitrate`（只是不轉 NORMAL） |
| 錯誤 | bus-off／error passive 由中斷與 `can_get_*_error_counter()` 觀察；bus-off 依 M_CAN 程序恢復；計數器可由 `CAN$` 以外的診斷讀取（例如 SYS） |
| SYS | 第 12 項 CAN（08 協定）：初始化中 1、監聽 2、收到有效 frame 3、BUS_OFF 4、恢復中 5、初始化失敗 6 |
| Type0 | App 開機 300 s 後送一次；數字版本 `major*100+minor*10+patch` → 0.9.7 = `97`；`CAN$GVERSION` 回 `V1:97` |
| 與 SMP 共存 | SMP（UART／BLE）進行中暫停 Type 1–4 主動輸出（沿用既有暫停條件，不在本版改暫停規則）；暫停期間資料照常解碼、TTL 照常計算，恢復後依 V1.9 週期輸出，不補送 |
| 與 BLE 回報共存 | `AT$PD=67;` 與既有 `AT$PD=G1;`／`BLEGW$` 輸出同一條 UART，一律整行、經 TX mutex |
| EVB 腳位 | PA10＝CAN_RX、PA11＝CAN_TX，經 **J2 跳線**接 U3 TJA1044；TJA1044 STB（符號 pin 8）在 EVB 上的接法請先查 `FR3068E-C_Device_DEV1.4.SchDoc` 確認（若需 GPIO 控制，納入初始化並在 30 報告註明） |

## 3. 檔案範圍

| 檔案 | 內容 |
|---|---|
| `fw/src/can/j1939.c／.h`、`app_can_protocol.c／.h`、`app_can_bitrate.c／.h`、`app_gateway.c／.h`、`j1939_sheet_tables_gen.h`（新，移植） | §1 |
| `fw/src/can_task.c／.h`（新） | §2：SDK CAN3 初始化、bus monitoring、ISR ring、task、bitrate apply callback、錯誤／bus-off、SYS 12 provider、設定載入 |
| `fw/src/serial_gchk.c／.h` | `CAN$` 行分流與回覆；CAN 輸出整行送出；SMP 期間暫停 |
| `fw/src/et100_fdb_keys.h` | `ET100_FDB_CAN_KEY 0x00030009` |
| `fw/src/evb_serial_main.c` | 啟動 can task |
| `fw/targets/evb_serial.mk` | 新檔案、版本 `0.9.7-dev` |
| `fw/tests/`（移植 4 支＋新增 `test_can_task.c`）、`fw/tests/fixtures/j1939_v19_vectors.json`、`fw/tools/test_can_host.sh`（新，並納入 `test_evb_serial_host.sh`） | §5 |
| `fw/docs/02_protocol/09_can_j1939.md`（新）、`fw/docs/02_protocol/j1939_v19_rules.csv`（複製） | ET-100 的 V1.9 實作說明（引用 V1.9 原文與 F407 解釋，列出 D2 差異） |
| `fw/docs/04_testing/30_can_j1939.md`（新） | host／build 證據、bit timing、Stanley 實機步驟（不預填 PASS） |

**不做**：ET-100 實板 MCAN1／AW9523、任何 CAN 發送、J1939 request／address claim、LTE 上傳、本地儲存、SMP 暫停規則修改、HTML 修改、SDK 修改。

## 4. 實機環境（Stanley）

```text
F407 DK（v0.2.7，CAN1 送 S2 demo、CAN2 為正常節點負責 ACK）
   CANH／CANL ──────────── EVB J2 跳線 PA10/PA11 → U3 TJA1044 → CANH／CANL
兩端各一個 120 Ω 終端（EVB 用跳線、F407 DK 依其硬體）；GND 共地
F407 先送 CAN$SBITRATE;1;<tok>（固定 250k → CAN2 NORMAL 會 ACK），再按 S2
```

## 5. 驗收

**Host（必須）**：

1. 移植的 F407 測試（gateway／protocol／bitrate／demo frames）在 ET-100 host 環境全部 PASS，斷言與 F407 相同。
2. **對照測試**：F407 S2 四階段 56 筆 payload 送入 ET-100 的 `AppGateway_Frame()`，Type1／Type2 整行與 Type3／Type4 樣本與 F407 預期**逐字相同**。
3. `CAN$` 解析：checksum 有／無、錯誤 checksum、各 S／G 指令、失敗 payload 與 F407 相同；設定 FlashDB 保存、讀回驗證、記錄無效用預設值。
4. `can_task`：ring 溢位計數、只收 extended、bitrate 自動掃描鎖定與 5 s 無幀重掃、bus-off 進入／恢復 → SYS 12 狀態轉換、**任何路徑都不呼叫發送 API**（fake 斷言 tx 次數為 0、monitoring 永遠開啟）。
5. SMP 進行中 Type 1–4 不輸出、恢復後照週期輸出；Type0 300 s 一次。
6. 既有 host／nano／JS 測試全部 PASS；ARM build 無新 warning。

**EVB（Stanley）**：
- 依 §4 接線，`CAN$SBITRATE;1` 下 Type1 每 5 s 一筆並依四階段變化（`0;251;1` → `0;126;0` → `0;130;0` → `0;124;0`，與 F407 實機一致）；Type2／3／4 依 TYPESWITCH 輸出。
- `CAN$SBITRATE;0`（自動）在有 ACK 節點時可鎖定 250k；拔掉 CAN 線 → TTL 到期欄位失效、5 s 後重掃；接回恢復。
- HTML「CAN 指令」分頁可操作 ET-100（S／G 指令、Type 顯示）。
- 同時進行 SMP 上傳時 CAN 輸出暫停、上傳完成後恢復；上傳不受影響。
- 重開機後設定保留。

## 6. 交付

Codex 完成後停下，交 Claude Step 5 review；不 commit、不燒錄。Step 7 由 Claude 更新 09 協定、08 SYS（CAN 項已實作）、平台計畫、`docs/00_project/11_project_bring_up_status.md`、01 開發計畫的 D06／CAN 項目。
