# IMPLEMENTATION_PLAN — v0.9.8 CAN 靜默掃描輸出修正＋版本號單一來源

> 狀態：**已核准，交 Codex 實作**（Stanley 2026-10-28：「let codex to improve it in v0.9.8」）。
> 分工（comm.md 標準規則）：Claude 寫施工單／review，**Codex 實作**，**Stanley 實機測試**。
> v0.9.7 施工單原文見 commit `f9977ba`（`git show f9977ba:fw/IMPLEMENTATION_PLAN.md`）。

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.9.8_can_quiet_scan

Base branch: `v0.9.7_can_j1939`（`f9977ba`，尚未 merge 到 main；工作目錄中未提交的 `fw/REVIEW_REPORT.md`「實機發現」與本施工單一併帶入新分支）

## 0. 背景與決策

| # | 內容 |
|---|---|
| D1 | **M1（實機 Major）**：Stanley 14:28 F407 reset（bus 靜默約 12 s，BITRATE=0 自動）時，`AT$PD=67;1;000000;;;*0B` 每 0.56 s 重複 17 筆。原因：`fw/src/can_task.c` `apply()` 每次掃描換速率都呼叫 `AppGateway_Invalidate()`，後者 `RequestSnapshot()` 強制一筆 Type1。F407 `a730897` `app_canmon.c:74` 同樣（繼承缺陷，F407 是否修由 Stanley 另決） |
| D2 | **修正只在 ET-100 glue**（`can_task.c`）；`fw/src/can/` 移植檔、SDK 不改（沿用 v0.9.7 D3） |
| D3 | **m1（v0.9.7 Minor）一併處理**：本版號變成 0.9.8，CAN 數字版本必須是 `98`；`97` 寫死 3 處改為單一來源 |
| D4 | 版本只遞增 z（Stanley 慣例）；其他 CAN 行為、V1.9 協定、HTML 不變 |

## 1. M1 修正規格

- `can_task.c` 新增 task 端旗標 `rx_since_invalidate`（`initialize()` 設 false）。
- `et100_can_step()` 每處理一筆 frame（呼叫 `AppGateway_Frame()` 時）設為 true。
- 原本呼叫 `AppGateway_Invalidate()` 的位置（`apply()` 成功／失敗兩條路徑、bus-off 進入）改成共用 helper：**只有旗標為 true 才呼叫 `AppGateway_Invalidate()` 並清旗標**；旗標為 false 時欄位自上次 invalidate 起從未被填入，已是全無效，略過不會讓舊樣本跨 epoch。
- 不得改 `CanBitrate_*` 掃描節奏、TTL、Type2／3／4 週期、`AppGateway_Configure()` 的快照（設定變更仍立即送一筆，屬 F407 原行為）。
- 注意 SYS12 狀態（2／4／5／6）仍照原邏輯更新，與是否 invalidate 無關。

## 2. 版本號單一來源（m1）

- 由 `EUP_FIRMWARE_VERSION`（`fw/targets/evb_serial.mk`，改為 `"0.9.8-dev"`）在編譯期或啟動時推導 `major*100+minor*10+patch`；若推導複雜，可在 mk 另加 `-DET100_CAN_VERSION=98`，但**必須只有一處人工維護**，並加 host 斷言：解析 `EUP_FIRMWARE_VERSION` 得到的值與 CAN 版本相同。
- `can_task.c` 3 處（`CAN$` 佇列滿回覆、命令回覆、Type0 `AppGateway_BootLine`）改用該常數。
- host 測試的 `97` 斷言改由同一常數產生期望字串，不得刪除斷言。
- 規則：minor 或 patch ≥ 10 時無法以此公式表示 → 編譯期 `_Static_assert` 擋下。

## 3. 檔案範圍

| 檔案 | 內容 |
|---|---|
| `fw/src/can_task.c` | §1、§2 |
| `fw/targets/evb_serial.mk` | `0.9.8-dev`（及必要的版本 flag） |
| `fw/tests/test_can_task.c`、`fw/tests/test_can_sdk.c`、`fw/tools/test_can_host.sh`（若需 flag） | §4 |
| `fw/docs/04_testing/31_can_quiet_scan.md`（新） | host／build 證據、Stanley 實機步驟（不預填 PASS） |
| `fw/docs/02_protocol/09_can_j1939.md` | 註明「靜默掃描期間不重複輸出 Type1」與版本來源 |

**不做**：改 `fw/src/can/`、改 SDK、改 F407 repo、改 HTML、改 SMP 暫停規則、ET-100 實板 MCAN1。

## 4. 驗收

**Host（必須）**

1. 自動模式、零 frame 持續 30 s（涵蓋多次掃描換速率）：Type1 **最多 1 筆**；Type2／3／4 仍依週期輸出無效值。
2. 收到 frame 後 → 靜默 → 掃描換速率：invalidate **恰好 1 次**（欄位清空並送 1 筆 Type1），之後不再重複。
3. bus-off 前有資料 → invalidate；bus-off 前無資料 → 不重複送。
4. 既有 v0.9.7 斷言（demo 56 筆、零 TX、MON 恆開、INIT 卡住 fixture、SYS12 轉換）全部保留且 PASS。
5. `CAN$GVERSION` 回 `V1:98`、Type0 版本 `98`，期望值來自單一常數。
6. `test_can_host.sh`、`test_evb_serial_host.sh`、`test_sensorflow_nano.sh`、`test_inst_status_host.sh`、`tools/tests/*.cjs` 全 PASS；`build_evb_serial.sh` 無新 warning；`nm` 確認 `can_add_tx_*`／`can_init`／`can_enter_bus_monitoring_mode` 未連結。

**EVB（Stanley）**

- 自動模式下 F407 reset 或拔 CAN 線：Type1 只在失效時出 1 筆空值，靜默期間不再每 0.56 s 重複；接回後自動鎖回並恢復四階段輸出。
- `CAN$GVERSION` → `V1:98`。
- v0.9.7 尚未測的項目一併在本版測：開機 300 s Type0（版本 98）、SMP 上傳期間 CAN 暫停／完成後恢復、EVB 斷電重開後 BITRATE 等設定保留且自動鎖定。

## 5. 交付

Codex 完成後停下，交 Claude Step 5 review；不 commit、不燒錄。Step 7 由 Claude 更新平台計畫版本、30／31 報告連結。
