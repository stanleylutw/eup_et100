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

## 實機發現（Stanley 2026-10-28 14:28，F407 reset＝bus 靜默約 12 s，BITRATE=0 自動）

| # | 等級 | 內容 |
|---|---|---|
| M1 | Major | bus 靜默且自動掃描時，`AT$PD=67;1;000000;;;*0B` 每 **0.56 s** 重複輸出（14:28:06.98→16.56 共 17 筆）。原因：`can_task.c` `apply()` 每次掃描換速率都呼叫 `AppGateway_Invalidate()` → `RequestSnapshot()` 強制一筆 Type1。F407 `a730897` `app_canmon.c:74` `apply_rate()` 同樣行為（繼承缺陷）。車輛熄火、CAN 靜默時會無限期約 1.8 行／秒佔用 RS232（未來 LTE 更嚴重） |

**建議修正（只改 ET-100 glue，不改移植模組，符合 D3）**：`can_task.c` 加 `rx_since_invalidate` 旗標，ISR 收到 frame 的 task 端處理（`AppGateway_Frame`）時設為 true；`apply()`（成功與失敗兩條路徑）及 bus-off 只在旗標為 true 時才 `AppGateway_Invalidate()` 並清旗標。欄位在上次 invalidate 後未收過資料就已是全無效，略過不會讓舊樣本跨 epoch。`initialize()` 的首次 apply 不受影響（`AppGateway_Init` 已清空）。
host 測試：自動模式、零 frame 持續 30 s → Type1 最多 1 筆（失效當下），Type2／3／4 仍依週期；收到 frame 後換速率仍 invalidate（既有斷言保留）。F407 專案另行通報同一問題。

同次 log 其餘行為 ✅：TTL 到期 Type1 欄位清空、SYS12 3→2（`ET100$SYS;11;12,2`）、恢復後 SYS12=3、自動鎖回 250k、Type3／4 以 `FFFFFFFFFF`／`FFFFF` 補缺秒樣本。

---

# v0.9.8 Step 5 Claude review（2026-10-28）— 結論：**PASS**（Critical 0／Major 0／Minor 1 已修）

Branch `v0.9.8_can_quiet_scan`，base `v0.9.7_can_j1939`／`f9977ba`。

**程式審查**

- `invalidate_received()`：只有 `rx_since_invalidate` 為 true 才 `AppGateway_Invalidate()`；旗標在 task 端 `AppGateway_Frame()` 後設定，`apply()` 成功／失敗與 bus-off 三處共用。✅ 符合施工單 §1，`fw/src/can/` 與 SDK 未改。
- 版本：`evb_serial.mk` 由 `ET100_VERSION_FLAGS` 解析 major／minor／patch（含命令列覆寫），`can_task.c` `_Static_assert` 擋 ≥10，3 處 `97` 全改 `ET100_CAN_VERSION`。✅ 只有一處人工維護。
- 測試：30 s 靜默掃描 61 次 apply、invalidate 0、Type1 1 筆；有資料 epoch 只清一次；失敗路徑與 bus-off 重複錯誤不重送；設定快照仍即時；佇列滿失敗回覆也用同一版本。✅ 既有斷言保留。

**Claude 自行重跑**

| 項目 | 結果 |
|---|---|
| `test_can_host.sh`、`test_evb_serial_host.sh` | ⚠️ 乾淨 bash 下 **exit 127**（見 m1）；以 grep 代替 rg 後 ✅ PASS（含 quiet CAN、版本 0.9.8-dev、minor／patch=10 編譯失敗） |
| `test_sensorflow_nano.sh`、`test_inst_status_host.sh`、`tools/tests/*.cjs`（13 支） | ✅ PASS |
| ARM build | ✅ 僅既有 RWX；flags `MAJOR=0 MINOR=9 PATCH=8`；ELF 內 `0.9.8-dev` |
| `nm` | ✅ `can_add_tx_*`／`can_init`／`can_enter_bus_monitoring_mode` 未連結 |

### Minor

| # | 內容 | 修正 |
|---|---|---|
| m1 | `fw/tools/test_can_host.sh:56` 使用 `rg -q`；Stanley 的 macOS bash 沒有 ripgrep（`command -v rg` 無），`set -e` 下整支腳本與包含它的 `test_evb_serial_host.sh` 失敗 | 改 `grep -q`（其他 fw/tools 腳本皆不依賴 rg） ✅ 已修（Claude 於無 rg 乾淨 bash 重跑兩支 exit 0） |

**實機待驗（Stanley，步驟見 31 報告）**：靜默／拔線只出 1 筆空 Type1、接回自動鎖回、`V1:98`、Type0、SMP 暫停、斷電重開設定保留。
