# v0.9.5 Tester 支援 F407

日期：2026-10-27。分支：`v0.9.5_tester_f407`，base `main`／`6b68c69`。

## 1. 範圍與行為

- 僅變更 `tools/rs232_tester.html`、新增 `tools/tests/test_f407_ota.cjs`，以及本施工單／報告；後續 build 捷徑使 `tools/tests/test_et100_ui.cjs` 的選單斷言同步改為只計可見項目，並驗證 F407 選項在 ET-100 隱藏。未修改其他 repo、ET-100 firmware、SDK、協定文件、Dashboard／CAN 顯示；未操作硬體、未提交／合併／推送。
- 單一裝置表列 ET-100／F407 的 project、hardware 規則、soc、board、大小／SP／PC 限制、INST 前綴與 Sensor 支援。檔案內 metadata 與 ZIP manifest 必須命中同一設定；連線身分不符在載入或更新前拒絕，訊息列出裝置及檔案欄位。BIN 的 soc 由已驗證的 project／hardware 設定取得。
- F407 GCHK 硬體字串只觸發 GFWINFO 查詢；以 GFWINFO 的 project＋soc 選擇 F407 SMP 模式。ET-100 保留既有 GCHK FR3068 路由與 pre-0.8.7 無 GFWINFO 的相容流程；未知裝置仍走原 Gateway 路徑。F407 不新增 Sensor package／relay，停用 Device OTA 選擇並略過進入 OTA 頁的自動 Sensor 查詢。
- F407 OTA 沿用原 SMP 上傳／Test／reset／image state 與 GOTASTAT 成功或還原判定；標題、版本資訊、選檔按鈕與 Log 使用 F407 名稱。後續依 Stanley 指示加入 Release／Debug build 捷徑（`build/Release/dfu_application.zip`／`build/Debug/dfu_application.zip`），保留本機選檔，不將 ET-100 build 路徑提供給 F407。F407 根目錄以獨立 `eup-f407-root` 儲存／授權，兩個 preset 共用該目錄；找不到檔案清除此筆記憶並提示重新選擇專案根目錄，不支援資料夾 API 時提示使用本機檔案。
- `parseInst()` 接受兩個設定表前綴，結果含 `device`；Client 僅採用當前裝置的 INST。CRC 自行首第一字元計算，未變動 ET-100 既有規則。
- USB 以 VID/PID `0483:5740` 且 F407 profile 判斷。OTABAUD 指令照送，原連線的 reopen 不 close／改 baud。reset 前建立有界重連 session，USB 斷線不觸發一般離線清除；顯示「安裝中（USB 模式無即時進度）」。connect 事件與每秒 getPorts 輪詢尋找候選，重新開啟後先核對 GCHK 24 hex UID。錯誤 UID 關閉且不送 SMP；相符後才查 image state。若 TRIAL 失敗再次拔除 USB，仍可在相同期限內重連。
- 沿用 Test ACK 後起算的 180 秒總期限；USB 探索／開埠／身分查詢受剩餘期限限制，不因重連重設。結束移除監聽器，手動中斷取消重連，遲來事件不能重啟。原生 open 遲完成時清理，不接管新 session。保留 Log，不因自動重連再次套用「連線後清除 Log」。
- F407 經 USB-RS232 轉接線則維持 9600 → 115200／INST → 9600；ET-100 同樣維持原流程。
- Stanley 核准另依 F407 設計 §4 呈現 512 KB 內部 Flash／8 MB NOR，Candidate／Rollback 各 1 MB，App 上限 384 KB；不顯示 Sensor 分區。進度填色使用各分區實際容量。ET-100 分區數值與版面保持原樣。

## 2. INST 測試向量

下列與 ET-100 INST §4 內容相同，只換 F407 前綴，CRC-16/XMODEM 從 `F` 起算：

```text
F407$INST;1;L;WRITE;8755CE49;262144;314752*5311
F407$INST;1;L;HELLO;8755CE49;0.8.11;PENDING*6AD5
```

## 3. 測試

- 新離線測試：合成 MCUboot／EUPFWV1 映像與 ZIP，不依賴 F407 build 產物。驗證正確映像、錯 project／board／soc、384 KB 上限、SP 上下界／對齊、ET-100 與 F407 雙向拒絕、前綴／CRC／錯裝置 INST 隔離。
- 新 mock browser：七個情境全部 PASS（USB 自動重連、純輪詢、純 connect 事件、TRIAL 後第二次 USB 重連並由 GOTASTAT 確認 rollback、180 秒結果未知、手動斷線、F407 RS232 INST）；涵蓋錯 UID 不送 SMP、USB 不改 baud、無 Sensor 查詢、兩種 Flash 圖表容量／起點、Log 保留、事件移除及遲來事件不恢復連線。USB 重連後每輪同時查 image state／GOTASTAT，輪詢期間手動斷線立即結束。
- 既有回歸：`tools/tests/test_*.cjs` 中除新 F407 測試外的 11 支腳本全部通過（Dash、Sensor SMP、ET-100 SMP／UI／傳輸圖、relay、導覽、SYS、頂欄、設定）。後續 build 捷徑僅同步 ET-100 UI 的選單可見性斷言，再重跑 F407／ET-100 UI。原測試使用既有本機 ET-100 fixture；非本次 firmware build 或重新硬體驗收。
- `git diff --check` 檢查格式；Chrome 測試在 sandbox 外執行，Web Serial 完全模擬。新測試命令：`node tools/tests/test_f407_ota.cjs`；離線部分可用 `--offline`。已查看桌機與手機深色截圖 `/tmp/f407_ota_desktop.png`、`/tmp/f407_ota_mobile_dark.png`；F407 標題、USB 路徑、分區容量可見，手機沿用既有垂直捲動版面，未增加水平溢出。

## 4. 限制與待驗收

- Step 5 Minor m1 修正：F407 NOR 補列「App 已回報旗標」`0x222000`／8 KB，Logs／預留改由 `0x224000` 起至 `0x7FEFFF`。新增完整 NOR 各列名稱／起點／大小、連續無重疊及總容量 8 MB 斷言；僅改圖示，不改 OTA。F407 build 選檔捷徑原為建議，後續由 Stanley 明確授權實作；新測試涵蓋實際點擊 Release／Debug、目錄快取與恢復、ET-100 路徑／儲存鍵隔離、檔案缺失清理及 API 不支援提示。

- 此報告不宣稱 F407 C CP2 已完成；T3–T5／T7–T8 的 USB 實體列舉、安裝時間、TRIAL／rollback、RS232 INST 與 ET-100 實機回歸均待 Stanley。
- 不讀取 USB 序號，以 GCHK UID 核對；不新增 USB 權限提示，自動重連僅使用已授權 port 或瀏覽器 connect 事件提供的 port。
- 未做 firmware build／燒錄／EVB／SWD。已嘗試代理唯讀審查，但代理因額度限制終止，未產生 review 結論；不得計為 PASS。本輪測試不能取代 Claude 的 Step 5 或 Stanley 實機驗收。
