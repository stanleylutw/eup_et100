# v0.9.3 一般 GATT 連線期間恢復掃描

日期：2026-10-24。分支：`v0.9.3_gatt_scan_resume`；base：`a04e0d6`。
Stanley 授權 Codex 代寫施工單、實作及 host／build 驗證。未操作 EVB、SWD、UART、燒錄或 commit。
本文件不是獨立 Claude review；實機驗收仍由 Stanley 執行。
同分支後續追加的 SMP 回報恢復規則與最新產物指紋，見 `25_smp_report_resume.md`；下列指紋保留為 Part A 驗證時的歷史產物。

## 1. 行為

- 手動及 auto-GATT 共用一般 session：認證成功進入 READY 後啟用 connected scan。
- 掃描維持 GEN_DISC、1M、重複封包不過濾、duration=0。interval=40（25 ms）、window=10（6.25 ms），要求占空比 25%；不是保證實際空中接收率。
- 每次一般 session 最多送一筆 connected start；API 拒絕、START 非零或中途 END 後不在同一 session 重試。GATT 照常處理命令，不將 scan 失敗報成 CF。
- cleanup 回 normal profile（40／30，要求 75%）。若 low scan 仍 active，先 stop，等 END，再 start normal；不能重複排入 pending 操作。
- 開始 3 s 無 START：記 timeout，先嘗試 stop 同步，不直接重送 start。stop 立即拒絕時最多每秒重試；stop 回 0 卻 3 s 沒有 END 時停止重送，等待 END。此時掃描結果不確定，不能當成功；GATT ownership 清理不等待這個 scan 狀態。
- 即使 stop 等待期間收到遲來 START，也不能覆寫 STOP_PENDING。同步 callback 在 SDK API 回傳之前到達也可處理。
- Sensor OTA／probe 的 exclusive session 不啟用 connected profile；Part A 未改 SMP／OTA Type-1 暫停條件，Part B 的新條件另見 `25_smp_report_resume.md`。
- CID／MAC、GTIME、MAXRPT、payload、checksum、GATT UUID、FlashDB／boot record 不變；Installer 與 SDK 未修改。

## 2. Host 測試

下列命令均 PASS：

```bash
bash fw/tools/build_evb_serial.sh
bash fw/tools/test_evb_serial_host.sh
bash fw/tests/test_relay_handoff.sh
bash fw/tools/test_inst_status_host.sh
bash fw/tools/test_sensorflow_nano.sh
```

- build：正常 App `0.9.3-dev`；只有既有 LOAD segment RWX linker warning。腳本出現的 `program target with file ...` 是 SDK post-process 提示，不是已燒錄；腳本未連接硬體。
- `test_evb_serial_host.sh` 以 ASan／UBSan 與真實 SDK GAP 型別執行；包含 system-status 測試。
- scan lifecycle：事件確認、同步／非同步失敗、缺 START／END、遲來／重複事件、profile 切換、stop 拒絕節流、tick wrap；實際 reporter 收到其他 Beacon 後產生 Type-1，核對 MAC、payload、XOR，並驗證 CID／scan-response 過濾不變。
- GATT fake：手動／auto 正常 session 在認證成功後啟用、其他 connectable Beacon 不替換 owner、掃描失敗仍能完成 GATT 操作、認證失敗不啟用、exclusive 不啟用、cleanup 回 normal。
- v0.9.2 M0 模擬（stop 回 0 → END status 0 → 無 MASTER_CONNECT）、quiet、late-connect 與 bounded drain 回歸通過。
- installer power-cut matrix 687 個案例及結果記錄的 torn-write 測試通過。
- Nano：沙箱內 Unicorn 曾以 Illegal instruction 結束；同一腳本在核准的沙箱外 host 環境通過。未安裝／更新套件。

Host fake 只證明程式策略與 callback 處理，不證明 Controller 支援射頻並行。

## 3. 產物指紋

| 檔案（repo 相對路徑） | Bytes | SHA-256 |
|---|---:|---|
| `fw/build_serial/output/Project.bin` | 321016 | `fba8079918ed2b73aba709d6b136217120cd77ef403745742e6e224747b8d412` |
| `fw/build_serial/output/App_mcuboot.bin` | 321568 | `04fb0edddfe037331db827ca769f6390e44245e147963e5348f2bb9688e0ff91` |
| `fw/build_serial/output/dfu_application.zip` | 199632 | `0507a6961a65ecc40231b785de381ccacaaa3a8196e6822c01bdf35d6b37d588` |

`App_mcuboot.bin` 與 `et100.signed.bin` 是相同 MCUboot hash-only 映像；檔名不代表數位簽章驗證已完成。開發用 BLE SMP 開放設定沿用原設定。Installer 不必為此功能重建／重刷。

## 4. RAM 診斷（供 Stanley 視需要讀取）

全域 `et100_scan_diag`：

| 欄位 | 意義 |
|---|---|
| `phase` | 0 STOPPED、1 START_PENDING、2 ACTIVE、3 STOP_PENDING、4 UNCERTAIN |
| `desired_window` | 目前政策：10 connected／30 normal |
| `issued_window` | 最近 start 請求使用的 window |
| `active_window` | 最近 START 成功確認的 window；END 或開始失敗清為 0 |
| `start_requests`／`start_success` | 送出次數／START 成功事件數，不可把兩者混為一談 |
| `start_failed`／`start_timeouts` | SDK 立即拒絕或 START 失敗／開始缺事件次數 |
| `stop_requests`／`stop_failed`／`stop_timeouts` | 停止請求／立即拒絕／等待 END 逾時次數 |
| `last_start_rc`／`last_stop_rc` | 最近 SDK API 同步回傳 |
| `last_start_status`／`last_end_status` | 最近 GAP 事件的 status |
| `unexpected_events` | 在非預期 phase 收到 START 的次數 |

`et100_scan_status` 保留 0=已確認 ACTIVE、0xfe=開始等待、0xfd=開始 API 拒絕、0x200\|status=START 失敗、0x100\|status=END。
新增 0xfc=停止等待、0xfa=缺 END、0xf9=stop API 拒絕；0xfb 是開始逾時進入同步前的暫態。
`active_window` 是最近事件的紀錄；STOP_PENDING／UNCERTAIN 不能只靠該欄非零推論仍在掃描。
既有 `et100_scan_reports`／`et100_scan_accepted` 分別是通過基本封包類型檢查後的 ADV 數與過濾接受數，不是空中全部封包數。
沒有新增 RS232 查詢指令或新增線上欄位。

## 5. Stanley 實機驗收（尚未執行）

1. 透過現有 ET-100 OTA 頁上傳上述 `dfu_application.zip`，等待 `0.9.3` 已確認。準備 GATT 目標 A、持續廣播且符合 CID／MAC 規則的 B／C；先記錄未連線時 Type-1 基準。GTIME／MAXRPT 不應讓 B／C 永遠被 RSSI 較強裝置排除。
2. 一般 GATT 手動連 A，等 AP。確認 GATT 命令可回覆，同時在連線仍存活時，持續收到 B／C 的新 Type-1 payload／token，不把連線前快取誤當成新掃描。
3. 如需讀 RAM，由 Stanley 自行取得 `et100_scan_diag` 與原有 ADV counters：應有 START success、phase ACTIVE、active_window=10，reports／accepted 隨 B／C 廣播增加。若只看到 start_rc=0 而沒有成功事件／新 ADV，不算通過。記錄拒絕 status／缺事件證據，不先推論硬體不支援。
4. 以既有保活／命令維持 A 的 session，對照未開 connected scan 的基準觀察 GATT 指令延遲、timeout／斷線、UART 回報負載；再取消／斷線，確認 normal window=30 恢復。重跑 auto-GATT，並確認 B／C 不會取代目前 owner。
5. 重跑 Sensor OTA：等待廣播、GATT／AES、SMP 傳輸、取消與失敗清理，不應啟用 window=10。再回一般 GATT，確認低占空比掃描可重新啟用。

若 SDK 拒絕並行或出現缺 END，不要自行把 pending 當完成；保留完整 RS232 log 與診斷。此版沒有新增 BLE stack／整機自動 reset。所有實機項目仍為待驗收。
