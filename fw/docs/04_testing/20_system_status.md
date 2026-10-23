# 20 — 系統／周邊狀態 SYS

日期：2026-10-23。分支 `v0.9.0_system_status`，base `5cd3bfa`。

Stanley 明確授權 Codex 代寫施工單並實作韌體＋HTML。**Host／mock browser／ARM build 完成，待獨立 review、Stanley 實機驗收。2026-10-23 Stanley 要求提交本分支；未 merge／push，Codex 未操作 UART／SWD／EVB。** 新回報的 Sensor OTA 問題見 [21 分析交接](21_sensor_ota_handoff_analysis.md)，尚未修復。

## 1. 交付範圍

- [08 協定](../02_protocol/08_system_status.md)：`ET100$SYS;Type;Index,Status;...*CRC`，33 項完整狀態表；移除先前提案的 Boot ID／Revision／Age 等欄位。
- `system_status.c/.h`：固定 RAM 快照、CRC-16/XMODEM、320-byte 上限、2 Hz 主動節流與合併；無新 task、無動態 queue。
- `GSYSSTAT`：合法四位大寫／數字 token，成功回覆完整 33 項，只讀 RAM；格式錯誤／未就緒回協定錯誤，不發硬體探測。
- 開機 SYS Type 0／已確認 TRIAL Type 2；五秒收集後 Type 10，變動 Type 11。SMP、relay、RX 半行、非 9600 時暫停，恢復補全表。既有 INST 及 OTA 成功條件不變。
- HTML Dash 的獨立表格：Index／英文名稱／狀態碼與英文狀態；有刷新、CRC／token／未知碼處理、boot／斷線清除、查詢三秒逾時。FR3068 版本 >=0.9.0 才主動查詢，避免對 nRF／舊版亂探測。有效主動 SYS 亦可識別此介面。
- Stanley 後續指定的 UI 名稱已統一為「中文名稱 英文／型號」，全部 33 項套用；只改顯示 label，不改 protocol name／Index／Status 或版面。SYS JS／mock UI 已重跑，桌機／手機截圖檢查通過；本次名稱調整不需重建 App。

## 2. 真實來源與未完成項目

| 項目 | 來源 | 限制 |
|---|---|---|
| SYSTEM | serial task 執行 | 不是整機自檢 PASS |
| SETTINGS | 已有 FlashDB init 結果 | 不新增 test key，不驗證所有 key |
| NOR | 唯一 OTA worker 的原有 prepare | 成功只表示初始化／寫入授權檢查完成，不是全容量或後續所有 I/O 掃描 |
| BLE | 實際 GAP scan／adv／master/slave link 事件 | MULTI_ROLE 無法區分手機／Sensor 細節；不改原 GATT 行為 |
| RS232_0 | DEV1.4 UART3 接收啟用 | 邏輯 transport，非產品 RS232 電氣或對端存在證明 |
| WATCHDOG | PMU enable bit | ARMED 不是故意觸發 watchdog 的實機驗證 |
| RESET | UNKNOWN | 無核准 SoC reset cause binding，不猜上電／WDT／rollback |
| 其餘 | UNSUPPORTED | 33 項有 registry 不代表產品驅動已完成；未擅自推定 NOT_FITTED |

Boot Type 3 不能由歷史 last_update=FAILED 猜測，第一版不輸出；其他 reset Type 同樣保留但無可靠來源不使用。現有 provider 的初始化／事件證據在本次開機有效，沒有新增產品週期採樣或 STALE timer。

## 3. 測試結果

| 命令 | 結果與覆蓋 |
|---|---|
| `bash fw/tools/test_system_status_host.sh` | PASS；33 項／合法碼、精確 token／CRC、最大碼／buffer 邊界、boot、BLE 角色、節流、合併、bulk 暫停恢復、tick 回繞 |
| `bash fw/tools/test_evb_serial_host.sh` | PASS；新增 GSYSSTAT parser／SDK GAP SYS callbacks；原 serial、report controls、BLE scan／GATT、App manager、boot health、Sensor relay、SMP／Sensor package 回歸全部通過 |
| `bash fw/tools/test_inst_status_host.sh` | PASS；INST、trial／rollback／中斷矩陣及 S4 不回歸 |
| `node tools/tests/test_system_status.cjs` | PASS；實際編譯 C formatter 產生回覆，由 JS 解碼；全部表／文件 CRC 向量、full／delta、未知碼與壞輸入 |
| `node tools/tests/test_system_status_ui.cjs` | PASS；33 項、查詢／舊 token／CRC、SMP Decoder 分段 SYS、boot／offline／timeout、版本 gate、1400／1920／390px 無表格溢出 |
| `node tools/tests/test_et100_ui.cjs` | PASS；舊 OTA／Sensor／relay、圖表、AT 與桌機／手機 regression |
| `node tools/tests/test_et100_smp.cjs` | PASS；SMP／INST／180 秒確認流程 |
| `node tools/tests/test_et100_sensor.cjs` | PASS；Sensor image 1 完整／續傳／錯誤／取消，不發 Test/reset |
| `python3 fw/tests/test_dfu_package.py` | PASS；ZIP／manifest、非零 metadata CRC、現有 scanner 唯讀相容 |
| `python3 fw/tools/check_plan_vs_code.py` | PASS；3 個既有 OTA 容量／chunk 斷言（不是 SYS 全功能驗證） |
| ARM App／DFU | PASS；只有既有 RWX linker warning，沒有新增 C warning |

Logs：`/tmp/et100_v0.9.0_host.log`、`/tmp/et100_v0.9.0_build.log`。截圖：`/tmp/et100_system_status_desktop.png`、`/tmp/et100_system_status_mobile.png`，Codex 已檢視，無新增表格重疊／溢出；不改共用 Log 或 OTA 分區圖尺寸。

## 4. 資源與安全

- App ELF text／data／bss = **283776／35808／85560** bytes。
- SYS 現值／已報值各 33 bytes，serial task 增加 321-byte 輸出 buffer；無新 FreeRTOS task／heap queue。
- ARM objdump 的 serial_task 靜態 stack frame 為 836-byte local allocation＋36-byte saved registers＝872 bytes。原 task 配置仍為 1024 words（4096 bytes）；此數字不包含巢狀呼叫／IRQ／libc，不是 runtime high-water 證明，實機並行負載仍待測。
- 不改 SDK／linker／Installer／boot record／NVS key，0x08001000 保護不變；沒有實體 NOR 測試寫入。build log 的燒錄路徑提示不是實際燒錄。
- Package 仍是 hash-only 原型，不宣稱簽章驗證。EUPFWV1 firmware version 為 0.9.0-dev；既有原型 build_id／git_branch 靜態欄位未在此 scope 改寫，不能用其歷史文字識別本次 branch，以版本與下列 SHA-256 為準。

## 5. 產物指紋

| 路徑 | bytes | SHA-256 |
|---|---:|---|
| `fw/build_serial/output/dfu_application.zip` | 198597 | `ab74274a1384dea22d02504a0744849283635a6b648ae668521a5645c63ca7ca` |
| `fw/build_serial/output/et100.signed.bin` | 320144 | `49313e8e533b3bb28b32d00fc78737acddb9f6fd91258c7cddf57b0d858c8d76` |

本版只需 App SMP 更新；**不需重新燒 Installer**。ZIP 重打包可能改 timestamp／hash。

## 6. Stanley 實機驗收（待執行）

1. 保存目前 auto-connect 清單與必要設定，依既有規則唯讀確認 B-info hash。用新版 HTML 的 ET-100 OTA 上傳本版 DFU，確認 slot0 0.9.0.0／完整 hash／confirmed。不要把收到 SYS 當更新成功。
2. 開 Dash，應列 33 項；SYSTEM／SETTINGS／NOR／UART／WDT 使用真實狀態，未接入的 EG800／GNSS／NFC 等顯示 UNSUPPORTED，不顯示 READY。
3. 手動 reset 後保存完整 log：無可靠 reset cause 為 Type 0；若錯過開機通知，重新讀取仍能取得 Type 10。TRIAL 開機 Type 2 的 SYS 不應打斷 INST 的 115200→9600 交接。
4. BLE 掃描／廣播／手機或 Sensor 連線／斷線，核對 04 的角色狀態，兩種以上為 MULTI_ROLE；不能要求此版辨識手機與 Sensor 子角色。
5. 同 baud 多次重新讀取，不應重開 port；檢查回覆 token／CRC、33 項唯一且升冪。斷開 USB 後不得保留綠色狀態。
6. 再做一次 App 或 Sensor NOR 上傳／relay：bulk 期間無新增 SYS 洗畫面，結束及 30 秒活躍窗口到期後補 Type 10；GCHK、Type-1、OTA 確認仍正常。保存完整 log；確認設定／auto list、B-info hash 不變。

本輪 Codex 未執行以上任何硬體步驟。若狀態錯誤先保留 raw SYS／命令／版本與場景，不直接以 HTML 顯示推論硬體故障。

## 7. Review 重點

33 項表／共用碼對齊、320-byte 邊界、serial callback 的 RAM-only 契約、GAP 多 link 更新、短 critical section、bulk 暫停／恢復、GSYSSTAT token 與 HTML 快照／事件競態、舊韌體不探測，以及 runtime stack／UART 並行待驗界線。新增未追蹤模組／測試亦須納入 review；目前未經獨立 review。

## 8. Stanley 追加：二進位資料通道 UI

僅 HTML／測試，App DFU 指紋不變，不需重建或重新燒錄。沿用既有曲線動畫；最終 GATT 使用 Bluetooth 圖示，RS232／內部搬移使用 FileText；ET-100 的動態路徑新增如下：

| 證據 | 路徑／動畫 |
|---|---|
| 已收到 SMP ACK，且上傳未滿 | 檔案 → NOR Candidate 或 Sensor OTA（依目標） |
| INST BACKUP done/total | Flash App → NOR Rollback；SKIP／完成不動畫 |
| INST START INSTALL／RESUME 後 WRITE | NOR Candidate → Flash App |
| 明確 START ROLLBACK 後 WRITE | NOR Rollback → Flash App；一般 9600 流程若收不到此事件不推測 |
| ERASE／VERIFY／等待／terminal／錯誤 | 不播放資料搬移；等待以原文字／狀態表呈現 |

只使用 Client 已驗證版本／CRC／txn 的 INST callback。若 START 遺失，WRITE 不猜來源；不修改接收鮑率、180 秒期限或更新成功判定。確認完成 App 區標綠；Sensor 套件成功比對 VALID 才有 stored 標示。

桌機曲線端點對齊相關分區中心，經縮放座標補償；手機直向堆疊時只跨 panel 間隙並突出目標區，避免動畫穿過分區文字。活動路徑原靜態說明暫時隱藏，右側事件進度保留。只改 overlay／outline，不改 Log 高度、panel 尺寸或 OTA 行為。

`node tools/tests/test_et100_transfer_ui.cjs` PASS：模擬 ACK／INST 的上傳、反向備份、寫入／還原、SKIP／缺 START／terminal、縮放端點、CSS 位移、桌機與手機及 layout 不變。原 `test_et100_ui.cjs`、`test_et100_smp.cjs` PASS。截圖 `/tmp/et100_transfer_{upload,backup,install,mobile}.png`，桌機已檢視活動文字無重疊；新增 mobile 路徑亦經 mock 回歸，不是實機驗收。

### 後續 UI 調整（Stanley 指定）

- 通道靜態文字隱藏，保留原空間；資料圖示使用綠色 FileText，固定四個、每趟四秒。連續 ACK 只更新進度，不再移除／重新加入動畫，避免圖示反覆從起點重跑。
- ET-100 與 Device→RS232 共用同一分區高度；隱藏地址／固定說明／舊狀態細節，名稱置中、容量在右。Installer 標籤為 Bootloader。兩個 4 KB、16 KB boot state、8 KB installer result 以 32px layout track 精簡顯示；容量與位址配置完全不改，空間留給主程式及日誌預留。
- 備份／寫入／還原區內新增**動態操作進度**（不恢復固定說明）：INST BACKUP／WRITE 的已完成 bytes／總 bytes／百分比，灰色為此映像占分區容量，綠色為裝置回報已搬移部分。完成保存 100% 供驗證階段查看，不代表映像已 confirmed；新 START／HELLO／新操作清除先前進度。缺 START 時 WRITE 不猜 Candidate／Rollback，也不顯示猜測的目的地進度。
- `test_et100_transfer_ui.cjs` 新增連續 ACK 不重啟、四個圖示／4s／綠色、BACKUP／WRITE／restore bytes 與 fill／100%／缺 START 不顯示等回歸；`test_et100_ui.cjs` 新增簡化標籤、兩頁 NOR 等高、compact rows 與 Bootloader 名稱。全部 PASS；備份／寫入截圖已檢視，不改韌體或既有 DFU 指紋，尚未實機驗收。
- Stanley 最終決定隱藏操作 bytes／百分比，只保留真實進度填色；新版使用 OTA 按鈕綠色 `#34a853`，Rollback 舊版、備份圖示／標籤使用深綠 `#176f54`。選取／活動／完成框線為 3px，依路徑顏色顯示。標籤在分區名稱文字的右上方：「備份」、「安裝中」（明確還原為「還原中」）、黃色「檢查中」，背景 2 秒呼吸、文字不閃爍；成功顯示不閃爍的「更新成功」。reduced-motion 停止動畫。測試檢查標籤與名稱／容量無重疊，原 OTA UI 與動畫回歸 PASS。
