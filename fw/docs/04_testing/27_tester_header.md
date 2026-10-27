# v0.9.4 Tester 共用兩行頂欄

日期：2026-10-24；分支 `v0.9.4_tester_header`，base `9ed327e`。
Stanley 確認固定分區／兩行頂欄並授權實作。只改 HTML、UI 測試及本次文件；未改 firmware／SDK／線上協定／OTA core，未存取 serial hardware／EVB。2026-10-27 另授權提交、合併 main 與推送 origin；Git 實際結果以提交及遠端記錄為準。

## 1. 版面

- 淺灰色共用頂欄置於整個 workspace 上方（依後續回饋由深灰改為淺灰，文字／圖示同步改深色），固定 42px（依回饋由 64px 經 52px 縮減），移除底部邊框及垂直分區線，不再分成左右兩個 header。桌機身分區固定 350px，與 sidebar 對齊；透明 Logo 使用 38px 顯示框，垂直置中，裝置／韌體各一行，字級 13／12px，韌體用灰色。
- 四個固定 grid 軌道：裝置資訊、Section 2 預留空白、Section 3 Log 工具、連線／明暗控制。Section 2 目前空白；Dash 的 Section 3 留空，但不移動其他軌道。
- BLE／CAN／OTA／GATT 共用既有 UART Log 的筆數／匯出／清除；AT 頁則在同一位置顯示獨立的 AT 筆數／匯出／清除，不混用資料來源。匯出／清除使用既有 Lucide subset 圖示及 tooltip。
- BLE／CAN filters 與 Log 高度按鈕移到 Log 內工具列；AT filter／search／pause 保留於 AT Log 工具列。原事件處理與 serial／OTA 行為不變。連線／中斷仍是既有主 serial 連線；AT 的 GSM 選埠／連線留在原功能區，沒有暗中共用 port。
- Gateway Dash 不再重複顯示舊深灰摘要 banner；依最新要求，ET100 與其他 Gateway 均顯示 CAN／Sensor 區。ET100 系統狀態／CAN／感測裝置各有右上角收摺圖示，初始系統狀態收起、CAN／裝置展開；收摺不清資料、不停止查詢，新資料與切頁不重設狀態，展開重繪圖表。狀態與統計資料解析未刪除，Section 2 不自動塞入統計。
- 全頁捲軸統一 5px、透明軌道／轉角與圓角灰色滑塊，修正 Dashboard 遺漏樣式而顯示瀏覽器粗白軌道；明暗各自設定滑塊／hover 色，保留導覽圖示區隱藏捲軸與非 WebKit thin fallback。SYS UI 驗證 Dashboard／側欄／主 Log／AT／OTA 捲軸樣式及 Dashboard 仍可捲動，sidebar 導覽回歸 PASS。
- Logo 最新呈現：深色以原圖加顯示尺寸固定 1px 純白輪廓，涵蓋黑色與紅色；原填色及透明背景保留。SVG filter 將 SourceAlpha 膨脹 1px，以白色置於原圖下方，不加底板或矩形外框。淺色不啟用 filter；mock browser 驗證 SourceAlpha／1px／白色設定、手動／系統深色與淺色覆蓋，以及桌機／手機截圖。
- 設定齒輪固定於左側導覽底部；依後續回饋取消對話框，改在左側標準功能區顯示「紀錄／關於」，無遮罩／關閉按鈕，關於使用側欄共用表格。保留右側當前視圖與 Log 工具，按原頁籤可切回。紀錄上限 1,000／4,000／10,000／20,000（預設 4,000）與連線清 Log（預設關閉），使用本機 localStorage；關於顯示工具名稱、版本、更新時間。原六頁仍可捲動選取，不改字級或主版面。一般 RX／TX Log／AT DOM 與 AT 保留陣列同步限量，AT 篩選／匯出不能取回淘汰資料；新連線成功才清 Log，不清 telemetry，AT 第二通道不重清。
- 新增 `test_tester_settings_ui.cjs`：PASS 六頁往返設定／選中狀態／保留右側視圖、明暗／手機邊界、儲存／重載／壞 JSON／儲存失敗降級、RX／TX 即時與持續裁切、AT 匯出與篩選淘汰、新 session／雙通道／失敗 open 清除規則。既有 header、sidebar、SYS、Dash info、OTA transfer mock UI 回歸均 PASS。本輪無 serial／EVB 存取。
- 資訊側欄簡化：所有頁面 section 移除整寬底線，以原有留白分區，表格行線／輸入框／工作區分隔線保持不變；頂欄測試逐頁驗證側欄 section 無底線，SYS UI 回歸驗證兩種主題的表格仍有 1px 行線。
- 深色可讀性：系統資訊／Gateway 設定表格使用中性灰底、較亮文字與分隔線，刷新按鈕使用中灰底。後續依 Stanley 指示，將共用中性色階套用所有頁面的 BLE／CAN／OTA／GATT／AT 表格，保留成功／警告／錯誤色與淺色配色；Logo 移除先前底板，恢復透明原圖，不改品牌色與頂欄配置。SYS UI 測試驗證色碼、項目／內容文字對比至少 4.5:1；頂欄測試加入跨頁元件樣本，驗證各表格標題／內容底色、文字與狀態色。等待按鈕背景轉場完成後再取樣。
- 後續視覺修正：功能側欄採淡灰 `#F7F8FA`／深色 `#202124`，右側採白色／`#191919`，兩區增加 1px 細分隔線（淺色 `#E1E4E8`／深色 `#34363B`）。手機上下排列時改底線，頂欄／圖示列仍無邊框；回歸測試新增背景差異與分隔線方向／色碼斷言。
- 2026-10-26 外框微調：頂欄與圖示列使用同色背景，內容左上角 24px 圓角，移除 sidebar／圖示列垂直分隔線，選中圖示改中性圓角底色。淺色為淺灰／白，深色為深灰／近黑；保留透明 EUP Logo、型號、韌體與所有固定軌道。頂欄測試新增兩種主題的背景一致性、內容對比與圓角／無邊框斷言。
- 裝置名稱／韌體超長時省略並保留完整 title；筆數有固定寬度，不推動工具或連線按鈕。斷線後清除顯示為「—」。工具版本仍依 HTML lastModified 產生，可於 EUP tooltip 查看，不占固定區域。
- 900／700px breakpoint 只調整固定軌道與工具顯示方式，窄螢幕縮短空白區、匯出／清除保留圖示，無第三行；手機 workspace 的側欄／功能內容改為上下排列以避免主功能只剩極窄寬度。沒有 viewport font scaling。

## 2. 本輪測試

七組 mock browser 與一組離線 SMP 測試：

```bash
node tools/tests/test_tester_header_ui.cjs
node tools/tests/test_tester_settings_ui.cjs
node tools/tests/test_sidebar_nav_ui.cjs
node tools/tests/test_dash_info_ui.cjs
node tools/tests/test_system_status_ui.cjs
node tools/tests/test_et100_transfer_ui.cjs
node tools/tests/test_et100_ui.cjs
node tools/tests/test_et100_smp.cjs
git diff --check
```

Chrome 測試經核准於 sandbox 外執行，Web Serial 完全模擬，未請求／開啟實體 USB port。SMP 測試使用既有 fixture／映像，不是本輪 firmware build 或 EVB 更新。

新增頂欄測試涵蓋 1920／1440／1024／768／390／360px、六個頁面切換、42px 高度、無底部邊框／垂直分區線、桌機 350px 對齊、四軌道的 x／y／width／height 固定、空白區、有／無 Log、按鈕／Logo 邊界、重複 ID、主 serial 與 AT 的獨立 counter／匯出／清除、長名稱／韌體／超長筆數、斷線重設與明暗切換。

`test_system_status_ui.cjs` 依最新 Dashboard 要求調整 ET100 CAN／Sensor 可見性與三區收摺預設，保留 ET100 33 項、CRC／token／unknown／offline／timeout 協定斷言。新增 CAN 車速／RPM／油量、裝置韌體／變化量 mock 資料、收摺期間更新及 canvas 非空像素檢查，以及各尺寸收摺按鈕邊界。nRF 原 CAN／Sensor 顯示仍驗證，未以 UI 測試聲稱硬體 CAN 或 RF 通過。

## 3. 視覺檢查與限制

2026-10-27 提交前 Step 7 完整性搜尋已檢查 docs／fw/docs／fw/README.md／comm.md 與本次施工單，重讀平台 UI 歷史、SYS 協定 UI 說明與 v0.9.2 報告；歷史紀錄保留當時狀態。本次 Kconfig／預設值、firmware 行為、payload／GATT／UUID／OTA 語意三項皆無變動，只有瀏覽器工具內部行為，故不升平台版本或改協定文件。`python3 fw/tools/check_plan_vs_code.py` 三個錨點通過；無重新命名的資產引用需遷移。

Dashboard 感測裝置標頭及上緣改用 BLE 頁同一狀態 helper 與橘／灰色變數，取消 EUP／Minew／LIGO 固定廠牌色。篩選與近期計數同步使用相同 fresh 判斷。`test_system_status_ui.cjs` 新增明暗模式、運轉／待機／休眠、GTIME×2 邊界，以及 30 秒模式 2999／3000／65000／65001 ms 案例，驗證標頭／上緣顏色、亮色／淡化與近期計數一致；mock browser PASS。

- 已查看正常桌機頂欄、桌機長名稱 light 與手機 dark 截圖，未見頂欄重疊或連線操作被擠出；原 sidebar／Dash／SYS／OTA 圖表的桌機／手機回歸通過。
- 暫存截圖：`/tmp/et100_header_normal_bar.png`、`/tmp/et100_header_normal_desktop.png`，以及 `/tmp/et100_header_{light,dark}_{1440,390}.png`；不是提交的產品資產。
- 本輪沒有 firmware build／量測、沒有獨立 Claude review。真實瀏覽器選埠、實體 serial／OTA 與使用者視覺驗收仍由 Stanley 執行；mock PASS 不代表硬體驗收。
