# REVIEW_REPORT — v0.9.5 rs232_tester 支援 STM32F407

- Reviewer：Claude Code（eup_canbus_32f407 session），2026-10-27（Step 5，獨立 review）
- 對象：`v0.9.5_tester_f407` working tree（base `main` `6b68c69`，未 commit）
- 依據：[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)
- 前一份（v0.9.3）review：`git show 08d90c5:fw/REVIEW_REPORT.md`

## 結論

**Critical 0／Major 0／Minor 1**，另 1 項建議。可進入 Step 7；實機 T3–T5、T7、T8 待 Stanley（T4／T5 需 eup_canbus_32f407 階段 C CP2 韌體）。

## Reviewer 自行驗證

| 項目 | 方法 | 結果 |
|---|---|---|
| 範圍 | `git status`／`git diff --stat` | ✅ 只改 `tools/rs232_tester.html`；新增 `tools/tests/test_f407_ota.cjs`、`fw/docs/04_testing/28_tester_f407.md`；`fw/IMPLEMENTATION_PLAN.md` 內容與交付時相同（Codex 未改） |
| 全部 tester 測試 | `node tools/tests/*.cjs`（13 支） | ✅ 全 PASS（11 支既有回歸 + fixture + 新 F407 測試） |
| F407 測試涵蓋 | `test_f407_ota.cjs` | ✅ 合成映像／ZIP、身分、大小與 SP 邊界、F407 INST 向量（`…WRITE…*5311`、`…HELLO…*6AD5`）；Playwright＋mock Web Serial 7 種情境：usb、poll-only、event-only、rollback-reboot、timeout、cancel、rs232；含 UID 不符的 port 被拒絕 |

## 程式審查

| 項目 | 判斷 |
|---|---|
| 裝置設定表 `profiles` | ✅ ET-100／F407 兩筆，`validate()`、`loadFile()`、`parseInst()`、`identify()`、`syncOtaSelfTarget()` 全部查表；ET-100 值與原寫死值一致 |
| 檔案／裝置比對 `compatible()` | ✅ 檔案 profile 與裝置 profile 同 `key`，且 project／hardware／soc 三者相符；錯誤訊息列出雙方身分 |
| ZIP 上限 | ✅ 依 manifest board／soc 選 profile 再套 `maxSize`；BIN 先以最大值粗檢、再以 profile 細檢 |
| INST | ✅ 依前綴選 profile，只接受目前裝置；CRC 範圍從行首起 |
| USB 判斷 | ✅ `getInfo()` `0483:5740`；`reopen()` 在 F407 USB 模式為 no-op |
| USB 重連 | ✅ reset 前掛 `connect`／`disconnect` 監聽並記錄 GCHK UID；`getPorts()` 輪詢＋事件兩路；**舊 port 真的消失前不會重開舊 App**；重開後送 GCHK 比對 UID，不符則關閉繼續等；所有 Web Serial 原生操作以同一 180 s 期限包住；取消／逾時清理監聽與 port |
| 斷線不清空 | ✅ `readLoop()` 在預期的 USB 斷線期間不觸發一般中斷流程 |
| Sensor 功能 | ✅ F407 停用 Sensor OTA／relay 按鈕並顯示原因 |
| 記憶體圖 | ✅ F407 顯示 512 KB 內建 Flash 與 8 MB NOR，進度容量改讀分區 `data-bytes`，App 上限 384 KB；ET-100 圖不變 |
| Dash | ✅ F407 的 GCHK 觸發 GFWINFO 以取得 project／soc 後才選 profile |

## Issues

### Minor

| # | 位置 | 問題 | 建議 |
|---|---|---|---|
| m1 | `rs232_tester.html` F407 `nor` 分區表 | 少了施工單 §3.4 的 **「App 已回報旗標」0x222000–0x223FFF（8 KB）**；`Logs／預留` 由 0x222000 起算（應為 0x224000）。只影響圖示，不影響 OTA | 補一列 App 回報旗標（0x222000、0x2000），`Logs／預留` 改從 0x224000 起；測試加檢查 |

### 建議（非缺陷）

- F407 時隱藏了「選擇 ET-100 build」捷徑，只能選本機檔案。可另加 F407 捷徑指向 `eup_canbus_32f407/build/Release/dfu_application.zip`（需另行授權 build root 權限，非本施工單範圍）。

## 未驗證（需實機，Stanley）

| # | 項目 | 前提 |
|---|---|---|
| T3 | F407 身分與記憶體圖 | 目前 F407 板（階段 B 韌體即可） |
| T4 | F407 USB OTA 自動重連 | eup_canbus_32f407 階段 C CP2 |
| T5 | F407 rollback | 同上（`APP_TRIAL_FAIL=1` DFU） |
| T7 | ET-100 回歸 | ET-100 EVB |
| T8 | F407 RS232（DB9） | USB-RS232 線；MAX232 @115200 未驗證（F407 Q6） |

Host／mock browser 不代替實機驗收；merge／push 不將待驗收項目視為通過。

---

## Re-review（2026-10-27，F407 build 捷徑＋m1）

| 項目 | 判斷 |
|---|---|
| m1 | ✅ F407 NOR 圖新增「App 已回報旗標」0x222000（8 KB），`Logs／預留` 改由 0x224000 起；`test_f407_ota.cjs` 檢查完整分區表 |
| F407 build 捷徑 | ✅ 選單新增 `F407 Release`（`build/Release/dfu_application.zip`）、`F407 Debug`（`build/Debug/dfu_application.zip`）；只在 F407 時顯示；專案根目錄以獨立 IndexedDB key `eup-f407-root` 保存，與 ET-100 `eup-et100-root` 隔離；權限被拒或檔案不存在時清除記錄並提示重新選擇 `eup_canbus_32f407` 根目錄；保留「本機檔案…」 |
| ET-100 回歸 | ✅ `test_et100_ui.cjs` 只改為計算可見選單項目，並新增「ET-100 時 F407 捷徑隱藏」斷言 |
| 施工單 | Codex 在 `IMPLEMENTATION_PLAN.md` 末尾追加一行「後續核准加入 F407 build 捷徑」決策紀錄，屬範圍變更紀錄，可接受 |
| 測試 | ✅ `node tools/tests/*.cjs` 13 支全 PASS；F407 測試涵蓋資料夾隔離、權限與遺失檔案回復 |

**結論：Critical 0／Major 0／Minor 0 → 進入 Step 7。** 實機 T3–T5、T7、T8 仍待 Stanley。

注意：F407 捷徑讀的是 `build/<preset>/dfu_application.zip`，此檔要等 eup_canbus_32f407 **階段 C CP1** 完成後才會由 build 產生；在那之前選捷徑會出現「找不到」提示，屬正常。
