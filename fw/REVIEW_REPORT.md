# REVIEW_REPORT — v0.9.6 F407 layout v2／Installer USB

- Reviewer：Claude Code（eup_canbus_32f407 session），2026-10-28（Step 5，獨立 review）
- 對象：`v0.9.6_f407_installer_usb` working tree（base `v0.9.5_tester_f407` `3f02a8d`，未 commit）
- 依據：[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)；F407 規格 `~/Firmware/EUP/eup_canbus_32f407`（`v0.2.5_installer_usb`）
- 前一份（v0.9.5）review：`git show 3f02a8d:fw/REVIEW_REPORT.md`

## 結論

**Critical 0／Major 0／Minor 1**（測試擬真度，見文末「完整 Step 5 補充」）→ 可進入 Step 7。實機驗收（Chrome + F407 v0.2.5）待 Stanley。

## Reviewer 自行驗證

| 項目 | 方法 | 結果 |
|---|---|---|
| 範圍 | `git status`／`git diff --stat` | ✅ 只改 `tools/rs232_tester.html`、`tools/tests/test_f407_ota.cjs`、施工單；新增 `fw/docs/04_testing/29_f407_installer_usb.md` |
| 全部 tester 測試 | `node tools/tests/*.cjs`（13 支） | ✅ 全 PASS |
| F407 情境 | `test_f407_ota.cjs` | ✅ 11 種：usb、poll-only、event-only、rollback-reboot、timeout、cancel、rs232、**installer-usb、installer-rollback、installer-halt、installer-timeout** |

## 對照修改清單

| # | 項目 | 判斷 |
|---|---|---|
| 1 | Flash 圖 layout v2 | ✅ Installer 0x08000000／48 KB（註明 layout 描述 0x08000200）、Boot state A 0x0800C000、B 0x08010000、「Sector 4 其餘」0x08014000／48 KB 不可使用、App 384 KB；移除 Settings；NOR 不變 |
| 2a | Installer 階段辨識與進度 | ✅ 重連後 GCHK fw 結尾 `-inst` → `phase='installer'`；BACKUP／ERASE／WRITE 百分比顯示於狀態與路徑標籤 |
| 2a | Installer 階段不送 SMP | ✅ `et100Reconnect()` 在 installer 階段只等待、不返回，因此 Client 的 SMP 輪詢暫停 |
| 2b | BOOT 後再次斷線重連 | ✅ 斷線 → 重新尋找 port → 以 GCHK UID 核對 → `phase='app'` 才返回，照原流程以 SMP state／GOTASTAT 判定 |
| 2b | UID 確認前的 INST | ✅ 暫存最多 64 行，UID 確認後才重放；錯 UID 的事件不改 OTA 狀態 |
| 2b | HALT | ✅ 任何階段收到 HALT 立即失敗並顯示 code |
| 2c／2d | Installer 身分不污染 App 資訊 | ✅ installer 的 GCHK 不更新頂欄版本；GFWINFO（`-inst`）不寫入 metadata cache、不作相容性依據；手動查詢遇到 installer 會明確拒絕 |
| 3 | 期限 | ✅ F407 `timeoutMs` 240 s、ET-100 維持 180 s；逾時訊息依 profile 顯示 |
| 4 | layout 旗標 | ✅ F407 profile 檢查 EUPFWV1 offset 278 bit0，0 → 載入即拒絕並說明需 v0.2.5 以上；ET-100 不檢查 |
| 5 | 測試 | ✅ 見上表 |

## 對照 F407 實機行為（eup_canbus_32f407 v0.2.5 實測）

- installer USB 出現時間約 0.5 s、送 HELLO 前會等 host 開 port 並回 GCHK → 與 tester「重連即送 GCHK」相容。
- 正常安裝 reset→confirmed 約 54–60 s；rollback 約 165 s → 均在 240 s 內。

## 未驗證（需 Stanley 實機）

| 項目 | DFU |
|---|---|
| USB 正常升級，畫面顯示 installer 進度、二次重連、成功 | `eup_canbus_32f407/build/dfu_test/ok_0.2.6/dfu_application.zip` |
| USB rollback，畫面顯示還原、結果 `ROLLED_BACK` | `eup_canbus_32f407/build/dfu_test/rollback_0.2.7/dfu_application.zip` |
| ET-100 回歸 | ET-100 EVB |

## Merge 注意

`v0.9.6_f407_installer_usb` 建在**尚未 merge 的** `v0.9.5_tester_f407` 上。建議只 merge v0.9.6（已包含 v0.9.5 的 commit），merge 前依 comm.md 做一次「清空任務文件」commit。

---

## 完整 Step 5 補充（2026-10-28）

第一輪只 review 了 `rs232_tester.html`。本輪補看 `tools/tests/test_f407_ota.cjs` diff（124 行）、`fw/IMPLEMENTATION_PLAN.md` 全文、`fw/docs/04_testing/29_f407_installer_usb.md`，並把 mock 與 F407 v0.2.5 實機 INST 紀錄（`eup_canbus_32f407/docs/REVIEW_REPORT.md`）逐項對照。

### mock 與實機對照

| 行為 | 實機（F407 v0.2.5） | mock | 判斷 |
|---|---|---|---|
| Installer GCHK | `…;0.2.5-inst;0;<tok>`，UID 同 App | 同 | ✅ |
| HELLO 時機 | 回完 GCHK 之後才送 | GCHK 回覆後立即送 | ✅ |
| 正常安裝序列 | HELLO;PENDING → START;INSTALL → CHECK → BACKUP… → CHECK → ERASE… → WRITE… → VERIFY → RESULT;INSTALLED;0 → BOOT;TRIAL;9600 → 斷線 | HELLO → START → CHECK → BACKUP/ERASE/WRITE（各 1 行 50%）→ VERIFY → RESULT → BOOT → 斷線 | ✅（進度行數簡化，不影響邏輯） |
| 還原序列 | App `A;TRIAL_TIMEOUT;120;STORAGE` → 斷線 → installer **`HELLO;…;TRIAL`** → `START;ROLLBACK` → … → `RESULT;ROLLED_BACK;9` → `BOOT;CONFIRMED;9600` | `TRIAL_TIMEOUT` → rollback installer 送 **`HELLO;0.2.5;PENDING`** → `START;ROLLBACK` → … | ⚠️ 見 m1 |
| Installer 不回 SMP | 不回 | 收到 SMP 即丟例外，且測試斷言零 SMP | ✅ |
| 干擾行 | — | 錯 UID 的 HALT、錯 txn 的 HALT、未請求的 GFWINFO | ✅ 驗證過濾正確 |
| 時間 | 安裝約 21–27 s；還原約 165 s | 用虛擬時鐘；rollback 情境斷言 180–240 s 之間 | ✅ |

### HTML 邊界再確認

- installer 階段的斷線偵測有三條路：`disconnect` 事件、read loop 例外（`et100UsbExpectedLoss`）、每輪 `getPorts()` 列舉不到舊 port。installer 分支之前就先做列舉，因此即使事件遺失也能偵測 → ✅。
- installer 階段 `et100Reconnect()` 每 1 s 檢查 HALT 與期限，不會卡死 → ✅。
- 第二次（還原）installer 重連時清除 `hello`／`boot` 旗標 → ✅。

### 施工單與測試報告

- 施工單範圍、決策與實作一致；29 報告如實標示「未獨立 review、未實機」。
- 小落差：29 報告寫「Stanley 仍需驗收 Debug DFU」，實際提供的是 Release 測試檔（`build/dfu_test/ok_0.2.6`、`rollback_0.2.7`）；不影響驗收內容。

### Minor

| # | 位置 | 問題 | 建議 |
|---|---|---|---|
| m1 | `test_f407_ota.cjs` installer-rollback 情境 | 還原用的 installer mock 送 `HELLO;0.2.5;PENDING`，實機是 `HELLO;…;TRIAL`。tester 目前不使用 HELLO 的 state 欄位，**不影響功能**，但測試擬真度不足，日後若 tester 依 state 分支就測不到 | 改為 `HELLO;0.2.5;TRIAL`；可加一行 BACKUP;SKIP 以外的 ERASE/WRITE（實機還原無 BACKUP） |

**結論維持：Critical 0／Major 0，可進入 Step 7；m1 可順手修或延後。**

**Stanley 決定（2026-10-28）：m1 延後**，不在 v0.9.6 修正；記錄於此，日後 tester 若依 HELLO state 分支時一併處理。

Host／mock browser 不代替實機驗收；merge／push 不將待驗收項目視為通過。
