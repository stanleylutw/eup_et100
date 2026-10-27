# IMPLEMENTATION_PLAN — v0.9.4 共用兩行頂欄

## Branch

2026-10-24 Stanley 授權直接實作 UI，並指定 `v0.9.4_tester_header`；base `main`（`9ed327e`）。不改 firmware／SDK／協定，不操作 EVB。2026-10-27 Stanley 另授權 git commit、merge main 與 push origin。

## 範圍與規格

- 僅修改 `tools/rs232_tester.html`、`tools/tests/test_tester_header_ui.cjs`、`tools/tests/test_system_status_ui.cjs`、`fw/IMPLEMENTATION_PLAN.md`、`fw/docs/04_testing/27_tester_header.md`；依使用者後續要求新增 `tools/assets/eup-logo.png`，以提供的原始 PNG 取代頂欄 EUP 文字。設定功能擴充另新增 `tools/tests/test_tester_settings_ui.cjs`。
- 淺灰色全寬共用頂欄取代 sidebar summary 與右側 Header；依後續回饋由深灰改為淺灰，文字／圖示改深色。高度由 64px 經 52px 縮至 42px，移除底部邊框及垂直分區線但保留固定軌道。Logo 使用 38px 顯示框以容納於頂欄內。左側 EUP 垂直置中，裝置／韌體兩行，第二行小一階且灰色。桌機裝置區與 sidebar 350px 對齊。
- Section 2 暫空，Section 3 固定軌道：無 Log 時留空，有 Log 時顯示筆數／匯出／清除；最右固定連線／中斷、明暗切換。不讓字長、筆數與切頁改變起始位置。斷線清除舊裝置資訊。
- 保留 BLE／CAN filter、Log 高度、AT filter／pause／search 功能，移到對應 Log 工具列；AT 的筆數／匯出／清除改用頂欄固定區域。原 Gateway Dash banner 不重複出現；依最新要求，ET-100 亦恢復顯示 CAN／Sensor 遙測。
- 窄螢幕使用固定 breakpoint 軌道，縮短空白區、以圖示保留 Log 動作及 tooltip；不採 viewport font scaling，不讓頂欄增加第三行或遮住連線操作。既有連線／OTA 狀態與 event handler 保留。

## 驗證

捲軸一致性（Stanley 回報 Dashboard 出現粗白色捲軸）：將既有局部規則改為全頁共用的 5px 捲軸、透明軌道／轉角及圓角灰色滑塊，依明暗主題切換滑塊與 hover 色；包含 Dashboard、Log、AT、OTA 與選單。保留圖示列捲動區隱藏捲軸規則，非 WebKit 瀏覽器使用標準 thin／透明軌道 fallback。不改區域尺寸或捲動功能。

ET100 Dashboard 區塊（Stanley 最新要求）：恢復 ET100 的 CAN 車輛資料與感測裝置區，不新增韌體／CAN driver。ET100 系統狀態、CAN、感測裝置各在標題右側加入收摺圖示與 aria-expanded／tooltip；初始僅系統狀態收起，另外兩區展開。收摺只隱藏內容，不清資料／停止查詢；新資料與頁籤往返不改目前收摺狀態，展開後重繪裝置圖表。三區仍以既有 parser／cache 為來源；nRF 保持 CAN／Sensor 顯示，ET100 系統區仍按裝置類型顯示。

深色 Logo 描邊（Stanley 最新要求）：改為顯示尺寸固定 1px 純白輪廓，涵蓋黑色與紅色。使用原始 `eup-logo.png`，SVG filter 以 SourceAlpha 向外膨脹 1px，再疊回原圖；紅黃／黑色填色／透明背景保留，無底板／矩形外框，不使用額外描邊 PNG。深色與系統深色偏好啟用 filter，手動淺色維持原圖；38px Logo／42px 頂欄不變。頂欄測試涵蓋描邊來源／半徑／顏色、主題切換及手動覆蓋系統偏好。

2026-10-26 設定面板（Stanley 指定功能）：左側圖示列底部固定齒輪，原六頁圖示在窄螢幕獨立捲動。依後續回饋移除對話框／遮罩／關閉按鈕，改在左側功能區顯示「紀錄／關於」兩個標準 section；關於使用既有側欄表格樣式。齒輪具選中狀態，切設定只換左側，右側現有 Log／資料／OTA 視圖保持；再按原頁籤可切回。紀錄提供保留上限 1,000／4,000／10,000／20,000 筆（預設 4,000）及連線後清除 Log（預設關閉）；以 `eup.tester.settings.v1` 儲存在本機瀏覽器，無法儲存時提示、仍於本次生效。一般 RX／TX Log DOM 與 AT Log 的 DOM／保留陣列／匯出均受限；降低上限立即裁切，篩選不復活舊資料。僅新 session 成功連線時清該連線 Log，不清 telemetry／設定，不因 AT 第二通道連上再次清除；失敗 open 保留舊紀錄。關於只顯示工具名稱、既有版本與頁面更新時間。不加字級／行距／縮放／裝置設定或硬體操作。

資訊側欄減少線條（Stanley 核准）：所有頁面的側欄 section 移除整寬底線（由系統資訊／Gateway 設定擴展至其餘頁面），保留表格內 1px 行線、輸入框、工作區分隔線與既有區塊留白；明暗模式皆適用。

深色可讀性修正（Stanley 核准）：系統資訊與 Gateway 設定的項目底色 `#292B30`、內容底色 `#202124`、文字 `#B8BDC7`／`#ECEEF2`、表格分隔線 `#3C4048`、標題 `#D5D9E0`，刷新按鈕採中灰底。後續依指示套用共用深色中性色階至 BLE／CAN／OTA／GATT／AT 表格，保留狀態色與淺色配色。移除先前 Logo 淺灰底板，恢復透明 Logo，42px 頂欄／型號／韌體位置不變。新增跨頁表格元件、狀態色、文字對比至少 4.5:1 與主題色碼驗證。

後續視覺修正（Stanley 核准）：左側功能區改為 `#F7F8FA`，右側維持白色，兩區之間加 `1px #E1E4E8` 分隔線。深色使用 `#202124` 左側、`#191919` 右側及 `#34363B` 分隔線。窄螢幕上下排列時改為底部分隔線；圖示列與頂欄維持無邊框，不加陰影或外層卡片。

2026-10-26 Stanley 同意參考附圖調整外框：頂欄與 48px 圖示列共用背景，內容區左上角 24px 圓角，移除 sidebar／圖示列垂直分隔線，選中圖示使用中性圓角底色。淺色為淺灰外框／白內容，深色為深灰外框／近黑內容；保留 EUP PNG、裝置型號、韌體、42px 頂欄及桌機 350px sidebar。不改協定或控制項行為。

Playwright mock Web Serial：六頁切換、無／有 Log、UART／AT 不混用筆數或匯出／清除、連線／斷線、長裝置／版本／大量計數、明暗切換；桌機與手機檢查區塊位置、固定高度、文字溢出、截圖與 page errors。重跑既有 sidebar、Dash、SYS、OTA UI 測試；不以 mock 宣稱硬體通過。

Dashboard 感測裝置卡片共用 BLE 頁 `sensorListAppearance`：橘色／灰色標頭與上緣使用相同 `--bar-active`／`--bar-sleep`，移除廠牌固定配色。資料新鮮度、篩選與計數亦使用同一判斷；30 秒模式前 3 秒亮橘，65 秒內保留正常文字，超時淡化。以 mock 測試明暗模式、運轉／待機／休眠與 3 秒／GTIME×2／65 秒邊界；不存取硬體。
