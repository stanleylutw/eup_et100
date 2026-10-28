# REVIEW_REPORT — v0.9.7

v0.9.6 的 review 原文保留於 commit `2b602ba`（`git show 2b602ba:fw/REVIEW_REPORT.md`）。

## Step 5 Claude review（2026-10-28）— 結論：**PASS**（Critical 0／Major 0／Minor 1）

Branch `v0.9.7_can_j1939`，base `c91cdd2`。

**Claude 自行驗證**

| 項目 | 方法 | 結果 |
|---|---|---|
| 原樣移植 | 對 F407 `git show a730897:app/<file>` 逐檔 `diff` | ✅ 9 份核心檔只多第 1 行來源註記，實作 byte 相同 |
| CAN host 測試 | `fw/tools/test_can_host.sh` | ✅ demo 四階段 Type1／2 整行與 Type3／4 樣本逐字相符、56 筆 payload 經正式 `app_gateway`；glue、SDK adapter（MON 恆開、零 TX）PASS |
| 回歸 | `test_evb_serial_host.sh`、`test_sensorflow_nano.sh`、`test_inst_status_host.sh`、`test_relay_handoff.sh`、`tools/tests/*.cjs` | ✅ 全部 PASS |
| ARM build | 獨立目錄重 build | ✅ 無新 warning（僅既有 RWX） |
| 只聽（D2） | `arm-none-eabi-nm`／`objdump` | ✅ `can_add_tx_*`、`can_init` 未被連結，呼叫點 0；`can3_line0_irq` 為強符號（≠ `Default_Handler`） |

**程式審查重點**

- `et100_can_hw_apply()`：INIT／CCE 中設 timing、`__CAN_BUS_MONITOR_MODE_ENABLE`、classic（FD／BRS 關）、自動重送關；RAM 只配 1 個 extended filter＋FIFO0 64，無 TX buffer；刻意不呼叫會清 MON 的 `can_init()`；回傳值以 `CCCtrl.MON` 確認。
- Bit timing：MCAN clock 48 MHz／2 = 24 MHz，prescaler 4（250k）／2（500k），1+20+3 TQ → **SP 87.5 %**（J1939-11）；SDK 段欄位 raw+1 的換算與 SDK demo 的速率註解一致。
- `apply()` 忽略可攜模組的 `silent` 旗標，鎖定後仍維持 MON（D2）；同速率重新 apply 不清 FIFO，避免丟掉鎖定用的 frame。
- ISR → 64 筆 SPSC ring（ISR 只增 head、task 於 critical 取 tail），溢位計數並視為錯誤觀察；只收 extended、拒 remote／FD。
- `CAN$` 由 serial parser 分流；佇列滿時立即回失敗 payload，不遺失回覆；設定 FlashDB `0x00030009` 寫入後讀回比對才回成功。
- SMP 期間 Type1–4 不送、照常解碼／TTL，不補送；Type0 遇暫停延後；所有輸出經 TX mutex 整行送出。
- EVB TJA1044 STB：原理圖證據為 R26 10 kΩ 下拉（常態 normal），無需 GPIO；實際裝配待 Stanley 確認。

### Minor

| # | 內容 | 建議 |
|---|---|---|
| m1 | 數字版本 `97` 在 `fw/src/can_task.c` 寫死 3 處（`CAN$` 回覆、Type0）；下次改版若漏改，`GVERSION`／Type0 會回報舊版本 | 改為單一常數，最好由 `EUP_FIRMWARE_VERSION` 或 build 參數推導（major*100+minor*10+patch），並加 host 測試比對 |

### 未驗證（需 Stanley 實機，步驟見 30 報告）

實板 CAN clock、實際 monitor-only 行為、F407 DK＋EVB 的 ACK 拓撲與終端、Type1 四階段輸出、自動偵測、拔線／接回、HTML「CAN 指令」分頁、SMP 共存、重開機保留設定、stack high-water。

## Step 5 re-review（2026-10-28，第 2 輪）— 結論：**PASS**（Critical 0／Major 0／Minor 1，m1 仍開）

觸發：首輪 review 後 `can_task.c`、`tests/test_can_sdk.c`、`tests/can_fakes/fr30xx.h`、30 報告 §5 有變更（Stanley 授權的 DAP 唯讀除錯：板上 CAN3 停在 INIT=1／MON=1，但舊程式只看 MON 就回成功、SYS12=2）。

**變更審查**

- `et100_can_hw_apply()`：SDK `can_message_ram_init()` 結束時會 `__CAN_INIT_STOP`（已對 `vendor/fr30xxc_sdk__202411/.../driver_can.c:570-572` 確認）；新程式先 bounded wait INIT=0，再重進 INIT／CCE 寫 filter；最後退出後 bounded wait INIT=0，並要求 **INIT=0、CCE=0、MON=1、FDOE=0、BRSE=0** 才回成功／開 NVIC。✅ 修正了「首輪 review 我接受的 `CCCtrl.MON` 單一判斷」這個盲點。
- 移除 `can_enter_bus_monitoring_mode()`（無等待的 INIT 進出）；MON 只在 INIT／CCE 期間設定，D2 不變。✅
- 失敗路徑（含 RAM init 負值錯誤碼轉 uint32 > 2056）回 0 → SYS12=6，由既有 500 ms 重試處理，NVIC 保持關閉，不會進 NORMAL。✅
- fake 重現 INIT 卡住狀態，斷言不得回成功／不得開 IRQ、SYS12=6，解除後重試成功；fake 的舊 helper 故意留 INIT=1 並斷言不被呼叫。✅ 斷言未放寬。
- F407 移植檔與 SDK 原檔未改。✅

**Claude 自行重跑**

| 項目 | 結果 |
|---|---|
| `test_can_host.sh` | ✅ PASS（demo 56 筆、glue、SDK adapter 含 INIT 卡住 fixture） |
| 回歸：`test_evb_serial_host.sh`、`test_sensorflow_nano.sh`、`test_inst_status_host.sh`、`tools/tests/*.cjs`（13 支） | ✅ 全部 PASS |
| ARM build `build_evb_serial.sh` | ✅ 僅既有 RWX warning |
| `nm`／`objdump` | ✅ `can_add_tx_*`、`can_init`、`can_enter_bus_monitoring_mode` 皆未連結，呼叫點 0；`can3_line0_irq` 0x08034d18 ≠ `Default_Handler` |

**仍開**：m1（`97` 寫死 3 處）。**實機待驗**：此修正只在 fake 上證明；需 Stanley 重燒後以 DAP 或 SYS12 確認 INIT=0／MON=1，再跑 §4 F407 DK 四階段。
