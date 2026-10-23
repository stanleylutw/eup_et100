# 19 — Sensor flow 格式與 Serial 交接

日期：2026-10-23；branch：`v0.8.12_sensorflow_format`；base：`1015337`。

Codex 依 Stanley 授權代寫施工單並實作；**host／ARM nano／build 完成；Claude Step 5 review PASS（M1 測試腳本 `rg`→`grep` 已修正並複驗；負向對照：舊碼在 nano 模擬下 Invalid memory read）；未做本版 EVB 測試**。沒有 UART／SWD／燒錄／NOR 實體寫入，未 commit／merge／push。

## 1. 問題與證據界線

- production `GSENSOROTAFLOW` 原用 `%012llX`；實際 App 為 `--specs=nano.specs`。EVB 0.8.11 回覆出現字面 `lX`、尾端 token 亂碼。修正保留 48-bit／12 hex 欄位，改 `%04lX%08lX`，分別傳入高 16／低 32 bit 的 unsigned long。
- 上傳後 GSOTAPKG 無回覆、舊 token 回覆的根因**仍未確認**。恢復 9600 不證明重開機（韌體有 30 s fallback），USB Log 時間不證明 bridge 重送。
- 最近 log `rs232_log_20261023_150637.txt` 與 `151207.txt` 顯示：刪除後完整 Sensor 上傳也能成功，完成後 GSOTAPKG 約 28 ms；另有固定 FLOW 亂碼及一次舊 token／通用失敗回覆。一般 GSOTAPKG 為 RAM 快照，不能把 60 s HTML timeout 當正常 NOR 寫入時間。
- 格式錯誤有無造成 HardFault／重啟尚無實機暫存器證據。本輪不宣稱已修好所有無回覆／舊 token 問題。

## 2. 改動

1. `sensor_ota_job.c`：只改 FLOW hex 格式與對應參數，不更動封包欄位數、workflow、token 或 UART 協定。
2. `evb_serial.mk`：App 版本 0.8.12-dev，GCHK／metadata 由既有版本來源更新；Installer 保持 0.8.11，不重建。
3. HTML：以 port 身分＋記錄的開啟 baud＋readable／writable／reader／writer 判斷是否維持現有連線。同 baud 不 close/open、不另送 CRLF；真正 reopen 後送單獨 CRLF 清理 ASCII 殘留。這是緩解，不是對 bridge 根因的判定。初次連線及舊 Gateway 切 baud 只增加被動 baud 記錄，不改既有協定／傳輸。未改 timeout、套件驗證或 180 s 安裝成功判定。
4. 新增 ARM nano 測試工具：連結同一 toolchain 的 `libc_nano.a`，執行包含實際 `sensor_ota_job.c` 的 fake Sensor suite，不用 PC libc 冒充 nano。專案 venv 安裝 `unicorn==2.1.4`、`pyelftools==0.32`，經 Stanley 明確同意；不改系統 Python、不 sudo、不改 SDK。

### 既有未提交 UI 修改

開分支前 `tools/rs232_tester.html`／`test_et100_ui.cjs` 已有 Stanley 核准的未提交圖表調整：GATT 共用字級、填滿 Flash 區、Log 高度不變、文字縮放補償、說明 11px 灰色、容量小數一位、NOR 版本（套件大小）、隱藏套件上限文字。**本輪保留這些修改，未回退；它們不是 UART 修正的必要條件。** Review／後續 commit 時須區分，不將 UI 改動算成間歇問題的根因修復。

## 3. 測試

| 測試 | 結果／覆蓋 |
|---|---|
| `bash fw/tools/test_evb_serial_host.sh` | PASS；FLOW 零／低位／高 16-bit／全 48-bit／遮蔽額外高位／精確 token／buffer 截斷；完整 relay／SMP／Sensor store／serial regression |
| `bash fw/tools/test_sensorflow_nano.sh` | PASS；ARM Cortex-M33 Thumb、hard-float ABI、nano.specs＋nosys.specs，production UART formatter＋fake Sensor 全套執行，assert 不能省略；map 確認 libc_nano.a |
| `node tools/tests/test_et100_ui.cjs` | PASS；50 次同 baud helper 不 reopen、真實 9600→115200→9600、只在真 reopen 送 CRLF、連續 NOR 查詢、失敗 open 後恢復、完整 mock 上傳及既有桌機／手機圖表 regression |
| `node tools/tests/test_et100_smp.cjs` | PASS；SMP／INST／Sensor package／180 s regression |
| ARM App＋DFU | PASS；只有既有 RWX linker warning，未新增 C warning |
| 產物檢查 | MCUboot 0.8.12.0、非零 metadata CRC、MCUboot SHA-256、ZIP 內外映像一致；沒有簽章驗證 |

Nano 測試的 syscall／heap 僅存在測試 ELF，不連入 App；Unicorn 僅執行 CPU／RAM，不驗證 UART 硬體、IRQ、FreeRTOS 排程或 NOR。Mac 沙箱內 JIT 可能觸發 Illegal instruction，本輪在獲准的 host 執行測試成功，不能把此錯誤誤寫為 EVB fault。script 產生的測試 ELF／map 在 `/tmp/et100-nano.*`，不是燒錄檔。

Logs：`/tmp/et100_v0.8.12_host.log`、`/tmp/et100_v0.8.12_nano.log`、`/tmp/et100_v0.8.12_app_build.log`。

## 4. 產物

| 檔案 | bytes | SHA-256 |
|---|---:|---|
| `fw/build_serial/output/dfu_application.zip` | 197,361 | `8aa03abcd528e4cb33f0561bc35170fe0f6446d79b6cfcf22dc9d5836f8bd5d9` |
| `fw/build_serial/output/et100.signed.bin` | 318,568 | `2ad1116758d4825dac9278031df67b8f1d6829acffefefff4590f225770cdaa7` |

App ELF text／data／bss = 282200／35808／85480 bytes。ZIP 含時間戳，重打包可能改變 ZIP hash。正常／badcand Installer 與 NO_CONFIRM App 未修改、未重建；不能拿 0.8.111 測試檔當成本版修正。

## 5. Stanley 實機步驟（待執行）

1. 用新版 HTML 將 0.8.12 ZIP 經 SMP 更新到 EVB；**不需重新燒 Installer**。確認 GCHK／GFWINFO 0.8.12-dev、slot0 0.8.12.0 confirmed=true，保存完整 log。
2. 在相同連線下連續查 FLOW 數十次，idle 應有 `...;00000000;000000000000;<本次 token>`；執行過 relay 的 packed bits 仍為 12 位 uppercase hex。不得出現字面 lX、亂碼或錯 token。
3. 保留 auto list／B-info 規則；上傳 Sensor 套件、確認 VALID，再做同檔重傳與必要時刪除後完整上傳。每次檢查上傳後第一筆 GSOTAPKG、回 9600、後續多次查詢；避免同時跑另一個 Serial monitor。
4. 若再無回覆，**先保留當時 port、baud 與完整 log，不直接重連／reset／重傳**。記錄最後 TX／ACK、版本與 token。唯讀 SWD 讀 PC、CFSR／HFSR／BFAR 仍由 Stanley 另行核准／執行；9600 恢復本身不足以證明重啟。
5. 必要時比較「維持連線查詢」與刻意 reopen 的 A/B，配合 UART RX 邏輯分析儀，才能區分 host、bridge、parser 殘留；不以緩解後未重現當根因證明。

## 6. Review／開放項目

- 新增 emulator 依賴／runner／support／script 未追蹤檔也須 review；只看 tracked diff 會漏掉。
- HTML baud 是 host 目前開啟設定，不是假裝知道裝置的瞬間實際 baud；非預期 reset／30 s fallback 仍可能造成兩端不一致，不能靠 cache 證明裝置活著。
- 本輪不加 RX 計數器、不自動 retry 60 s 查詢、不做相同 App 上傳前 hash 最佳化、不變更韌體交接設計。若間歇故障仍重現，需新的證據與施工單。
