# v0.9.6 F407 layout v2 與 Installer USB

日期：2026-10-28。分支 `v0.9.6_f407_installer_usb`，base `3f02a8d`。

## 1. 實作

- 唯讀核對 F407 repo 的設計 §4.1／v0.2.5 Installer USB、小節 comm.md、`common/flash_layout.h`、`installer/usb_link.c` 及既有 review；不更動該 repo、不操作硬體。
- 內建 Flash 圖依 layout v2 顯示 Installer 48 KB（layout 描述 `0x08000200`）、Boot state A／B 各 16 KB（`0x0800C000`／`0x08010000`）、Sector 4 其餘 48 KB（不可使用）及 App 384 KB。F407 不再顯示 Settings；NOR 8 MB 各區不改，ET-100 圖不變。
- F407 驗證 metadata CRC 後檢查 offset 278 的 flags bit0；未設旗標時在載入就拒絕並顯示 layout v2 提示。其他 flags 可同時存在，ET-100 不要求 bit0。
- F407 profile 的確認期限固定 240 秒，從 Test ACK 後開始，含 reset、所有 Installer／App 重連及 rollback。ET-100 固定 180 秒；BOOT／多次重連不延長期限。
- USB 每次接回先核對 GCHK UID，再以 fw `-inst` 判斷 Installer 或 App；Installer 階段不發任何 SMP／GOTASTAT 輪詢。收到 BOOT 後等待預期斷線，再找同 UID App；App 回來後才查 SMP state／GOTASTAT 決定成功或還原。
- UID 驗證前最多暫存 64 筆有效 F407 INST 行，驗證成功才重播；不同 UID 的暫存事件丟棄。既有 CRC／device／txn／version 規則不變。支援 BACKUP／ERASE／WRITE 步驟百分比及原備份／安裝／還原圖示；HALT 立即失敗，無事件或漏 BOOT 仍受總期限限制。
- 保留頂欄原 App 版本，Installer 版本呈現於步驟與 Log；Installer GCHK／GFWINFO 不更新 App 身分 cache。手動 identify 遇 Installer GFWINFO 即停止，不作映像相容性判斷或 SMP state 查詢。
- 舊版沒有 USB Installer 的 App→App 重連路徑仍保留，但 F407 檔案現在必須 layout v2；裝置應先由 Stanley SWD 工廠燒錄 v0.2.5。

## 2. 驗證與限制

- 新合成映像測試：F407 bit0=0 拒絕、bit0=1 接受、其他 bit 共存；ET-100 bit0=0 仍接受。瀏覽器驗證無旗標檔案載入拒絕並停用開始鍵。
- USB mock 增加 Installer 正常流程、超過 180 秒的 TRIAL_TIMEOUT／rollback、Installer HALT、Installer 靜默直到 240 秒。對每個 Installer port 斷言零 SMP；檢查進度、頂欄／metadata 不含 `-inst`、錯 UID 的 HALT 不生效、監聽器清理。原 7 個情境保持，包括 USB 純輪詢／純事件、RS232、取消、舊路徑 rollback 與逾時。
- 測試命令：`node tools/tests/test_f407_ota.cjs`（或加 `--offline`）；其他既有 `tools/tests/test_*.cjs` 逐支執行；`git diff --check`。Chrome 只使用 mock Web Serial，不開實體 port。
- 結果：F407 11 個 mock browser 情境全部 PASS，既有 11 支 tester 測試全部 PASS；metadata 旗標／手動查詢 Installer 零 SMP 的離線測試 PASS，plan/code 三個常數錨點及 diff check PASS。已查看更新後桌機 Flash 圖（`/tmp/f407_ota_desktop.png`），48 KB Installer、Boot state A／B、不可用 Sector 4 剩餘區與 384 KB App 顯示正確；測試亦保留手機深色無水平溢出斷言。模擬 rollback 超過 180 秒、於 240 秒內由舊版 confirmed＋交易吻合的 GOTASTAT 判定已還原；不是實機時間量測。
- 尚未獨立 Step 5 review、未作本輪實機驗收。F407 來源 repo 的約 54–60 秒正常安裝／約 165 秒 rollback 為既有 firmware 證據，本報告不重複宣稱 HTML 實測。Stanley 仍需驗收 Debug DFU 的 Installer 進度→App 成功及 APP_TRIAL_FAIL=1 的還原。
- 未提交／合併／推送，未修改 ET-100 firmware、SDK、F407 repo 或 Dashboard／CAN 頁。
