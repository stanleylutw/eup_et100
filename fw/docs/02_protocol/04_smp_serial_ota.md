# 04 — 更新協定：SMP over serial（v0.8.5）與 BLE（v0.8.6）

Version: v0.9.3；最後更新：2026-10-24 Codex（Stanley 授權 Step 7）。本版只有回報排程變更，wire format 不變，實機相容性待驗收。

日期：2026-10-22；分支 `v0.8.5_ota_manager_smp_serial`；實作：Claude Code（comm.md 備用規則）。
適用：ET-100 RS232 更新入口；EVB 開發用 **UART3**（DAPLink VCOM），與 GCHK ASCII 協定共用同一條 UART。

本協定就是 Zephyr／NCS 的 **MCUmgr SMP serial transport**，所以 `smpclient`、`mcumgr`、AuTerm 等現成工具可直接使用。之後 `tools/rs232_tester.html` 依本文件實作（Web Serial＋base64＋CBOR）。BLE 入口（v0.8.6）使用同一套 SMP 指令，只是 transport 不同。

## 1. 連線參數

| 項目 | 值 |
|---|---|
| 預設 | 9600 8N1，無 flow control |
| 更新用 | 115200 8N1（以 `OTABAUD` 切換，§4） |
| 與 GCHK 共存 | SMP 每行以 `0x06 0x09`（新封包）或 `0x04 0x14`（續行）開頭；ASCII 指令（`BLEGW$…\r\n`）照常處理 |
| SMP 期間 | 暫停 Type-1／SYS 主動回報：收包、排隊、處理與整包回覆期間＋最後活動／回覆完成後 3 秒；細節見 §1.1 |

### 1.1 主動回報保護（v0.9.3）

- UART 每個 SMP byte、BLE 每個 SMP VALUE write 均記活動，包含尚未組成完整請求的資料；worker queued／processing／整包 response sending 任一存在就暫停 UART 主動回報。成功、失敗或 sender 送出失敗返回後，都從回覆結束重新計 3 秒；新請求重新延長，不在封包間幾毫秒的空檔恢復。
- 3 秒是解除回報暫停的條件，不是 SMP 必要等待，也不代表 PC 已收到成功回覆。Type-1 恢復後仍依 GTIME／MAXRPT 排程，沒有新 Beacon 不捏造資料；SYS 則依既有節流補最新完整快照，即使狀態未變也可能送 Type=10，之後才恢復變更通知。
- UART 半行 200 ms、半包 2 秒及 BLE 半包 2 秒的既有 expiry 會被週期性檢查，無下一個 byte／write 也能清除。異常半包的緩衝從最後 RX 計算，不額外疊加 3 秒；未完成的 NOR 上傳／續傳資料不因此刪除。
- 每筆主動行先非阻塞取得 TX mutex，再查 SMP／RX ring／half-packet gate，通過後才產生該行；SYS 送出後的 Beacon 要重新檢查，不能沿用舊快照。已取得發送權的一行可完整送完，下一行可延後且不消耗待送狀態。mutex 不只保護回報：worker 的整包回覆旗標跨越所有 UART 多行／BLE notifications。
- UART／BLE SMP 都會暫停 UART Type-1／SYS；這不呼叫 BLE 停掃，不保證減少掃描射頻負載。SYS 不因目前為 115200 而額外停報，使用目前 baud。
- **分開保留**：OTABAUD 仍依 UART SMP idle **30 秒**回 9600（§4）；跨 transport 的 NOR mutating owner 30 秒與 relay 狀態 gate 不改。SMP 唯讀查詢延長的是回報用 3 秒，不延長未完成上傳的 mutating owner；ASCII 查詢不延長 SMP 緩衝。

驗證與產物見 [25 報告](../04_testing/25_smp_report_resume.md)。Host／ARM 通過不等於第三方 client／115200 實體 UART／BLE 並行已驗收；工具若兩筆請求間隔超過 3 秒，應能區分主動文字行與 SMP framing。

## 2. Framing（與 Zephyr／smpclient 相同）

```text
raw   = be16(len(packet) + 2) | packet | be16(CRC16-XMODEM(packet))
lines = base64(raw) 切成每行 ≤ 124 字元（4 的倍數）
第 1 行：0x06 0x09 + base64 + '\n'；之後各行：0x04 0x14 + base64 + '\n'
```

- CRC16-XMODEM：poly 0x1021、init 0x0000，只算 `packet`（不含長度欄位）。`"123456789"` → `0x31C3`。
- 每行 base64 可獨立解碼（只有最後一行可能有 `=` padding）。
- 裝置可接收的 decoded `packet` 最大 **1024 bytes**（`mcumgr params` 回報 `buf_size=1024`）；每行最多 256 個 base64 字元。
- 舊的未完成行遇到新的 `0x06`／`0x04` 會被丟棄（client 中途被關掉也不會卡住下一次連線）。

## 3. SMP 封包與支援的指令

SMP header 8 bytes（big-endian）：`op/ver(1) | flags(1) | len(2) | group(2) | seq(1) | id(1)`，後接 CBOR map。`op`：0 讀、2 寫；回應為 op+1，`seq`／`id`／版本照抄。v1、v2 都接受。

| Group | ID | op | 用途 | Request | Response |
|---|---|---|---|---|---|
| OS 0 | 0 | 寫 | echo | `{"d": text}` | `{"r": text}` |
| OS 0 | 5 | 寫 | reset | `{}` | `{}`，送完後裝置重開 |
| OS 0 | 6 | 讀 | mcumgr params | `{}` | `{"buf_size":1024,"buf_count":1}` |
| OS 0 | 8 | 讀 | bootloader info（v0.8.6） | `{}`／`{"query":"mode"}` | `{"bootloader":"MCUboot"}`／`{"mode":3,"no-downgrade":false}` |
| IMG 1 | 0 | 讀 | image state | `{}` | `{"images":[…],"splitStatus":0}` |
| IMG 1 | 0 | 寫 | test／confirm | `{"hash": bytes32, "confirm": bool}` | 同 state read |
| IMG 1 | 1 | 寫 | upload | 見 §3.1 | `{"rc":0,"off":N}` |
| 其他 | — | — | — | — | `{"rc":8}`（ENOTSUP） |

錯誤一律以 `{"rc": n}` 回覆：3 EINVAL、5 ENOENT、6 EBADSTATE、8 ENOTSUP、9 ECORRUPT、1 EUNKNOWN。

### 3.1 Upload

- 第一包（`off=0`）必須帶：`image=0`、`len`（整個檔案大小，560 B–1.75 MB）、**`sha`（整個檔案的 SHA-256，32 bytes，必填）**、`data`。
- 之後每包：`off`、`data`。裝置回 `off` = **本次連線已接收**的下一個位移（可能含最多 4 KB 仍在 RAM、尚未寫入 NOR 的資料）；client 一律照回傳的 `off` 繼續送。
- `off` 不等於期望值時，裝置不收資料、直接回期望值（client 會自動重送）。
- **續傳**：兩種位置要分清楚——`off` 是「本次接收位置」；「可恢復位置」是已寫入 NOR 並 commit 的連續 4 KB 倍數。斷線、client 重開或裝置重開後，client 重新送第一包（同 `len`／`sha`），裝置回**可恢復位置**，client 從那裡續傳（RAM 裡未 commit 的部分會重送）。
- 同一份檔案若已完整收到，第一包直接回 `off=len`。
- `upgrade=true`（MCUmgr：只允許升級）：第一包（off 0）的 MCUboot 版本（major.minor.revision，同 Zephyr 預設不比 build number）必須**大於**執行中版本，否則回 EBADSTATE（同 Zephyr `CURRENT_VERSION_IS_NEWER` 的 legacy rc）；非 bool 回 EINVAL。未帶或 false 不檢查版本。（v0.8.5–v0.8.7 初版誤解為「永久安裝」而回 ENOTSUP，v0.8.7 已依規格修正。）
- 需求：目前 boot state 為已確認（有 PENDING 或 TRIAL 時回 EBADSTATE）。
- **最後一包與第一次 state read 可能需要數秒**（裝置要驗整份檔案 hash、MCUboot TLV、EUPFWV1 metadata）：client timeout 建議 ≥ 30 秒。

### 3.1a Sensor package（`image=1`，v0.8.8）

- 第一包 `image=1`：上傳 **Gateway Relay Header v1 sensor package**（256 B header＋原始 sensor signed image，格式與 `eup_gateway/include/sensor_ota_package.h` 相同），`len` 288 B–225280 B，`sha` 必填；`image` 其他值回 EINVAL；裝置無 Settings KVDB 時回 ENOTSUP。
- 寫入 NOR `0x420000`（256 KB sensor 區），與 ET-100 Candidate 共用同一套 journal／`off`／續傳規則（重開機後重送第一包可從 4 KB commit 位置續傳）。
- 第一包時持久記錄改為 **INVALID**（generation+1）；**最後一包**時裝置驗 journal SHA-256、Relay header（magic／CRC／長度／字串）、image SHA-256（header 0x90）、MCUboot SHA-256 TLV（header 0xB0），全過才記 **VALID** 並回 `{"rc":0,"off":len}`；失敗回 ECORRUPT（9），記錄維持 INVALID。最後一包約需 4 秒。
- sensor package **不會**出現在 image state 的 slot 1，也不能 test／confirm；`upgrade` 旗標不適用（同 gateway 允許降版）。
- 同時只有一個上傳 session：image 0 與 image 1 互相取代 journal session（已 VALID 的 package 不受影響，以持久記錄為準）；跨 transport 仍是 30 秒單一 owner（EBUSY）。
- 查詢／刪除用 RS232 `BLEGW$GSOTAPKG`／`BLEGW$DSOTAPKG`，格式與 eup_gateway `03_rs232_protocol.md` §9.8 相同：

```text
BLEGW$GSOTAPKG;<tok> → AT$PD=BLEGW$GSOTAPKG;1;VALID;<gen>;<pkg_len>;<img_len>;<product>;<board>;<soc>;<fw>;<upload_sha256 64 hex>;<build_id|->;<tok>
                     → AT$PD=BLEGW$GSOTAPKG;1;EMPTY|INVALID;<gen>;-;-;-;-;-;-;-;-;<tok>
                     → AT$PD=BLEGW$GSOTAPKG;0;UNSUPPORTED|BUSY|READ_FAILED;<tok>
BLEGW$DSOTAPKG;<tok> → （擦除完成後，約 2 秒）AT$PD=BLEGW$DSOTAPKG;1;OK;<tok>
                     → AT$PD=BLEGW$DSOTAPKG;0;BUSY|UNSUPPORTED|DELETE_FAILED;<tok>
```

刪除期間 `GSOTAPKG` 回 BUSY；刪除後 EMPTY、generation 歸 0（同 gateway）。查詢只讀內建 Settings 的記錄（VALID 時保存驗證過的 256 B header），不讀 NOR。

### 3.2 Image state

```text
images[0] = { image:0, slot:0, version:"0.8.5.0", hash:<MCUboot TLV SHA-256>, bootable, pending:false,
              confirmed:<running 已確認；TRIAL 時為 false>, active:true, permanent:false }
images[1] = { slot:1, version, hash, bootable:true, pending:<已 test／confirm>, confirmed:false, active:false,
              permanent:<以 confirm 排入時為 true，至重開機> }
```

- `hash` 是 **MCUboot image hash**（image 內 SHA-256 TLV 的值），與 nRF Connect Device Manager 從檔案算出的值相同。上傳用的 `sha` 是**整個檔案**的 SHA-256，兩者不同。
- slot 1 只在 Candidate 下載完整、通過 hash／MCUboot／EUPFWV1（FR3068、CRC 非 0）驗證後才列出；與 slot 0 相同的 image 不列。
- `version` 為 `major.minor.revision.build`（MCUboot header）。

### 3.3 Test／Confirm／Reset

| 動作 | Request | 結果 |
|---|---|---|
| test | `{"hash": slot1.hash, "confirm": false}` | boot state → PENDING；重複送同一 hash 冪等 |
| reset | OS reset | installer 備份目前 App、安裝 Candidate，新 App 以 TRIAL 開機 |
| confirm | `{"hash": slot0.hash, "confirm": true}` | TRIAL → CONFIRMED（新 App 自檢通過也會自動確認，見 06 §5） |

`confirm:true` 搭配 slot 1 的 hash（nRF iOS／Android 函式庫預設的「Confirm only」模式）：**不做永久安裝**，與 test 相同排入 PENDING，回報 `pending:true, permanent:true`；重開機後一樣經 installer 備份、TRIAL、自檢 30 秒自動確認，失敗則 rollback。只在 boot state 為 CONFIRMED 時接受，否則 EBADSTATE。（v0.8.7 前回 ENOTSUP，App 在最後一步失敗。）

## 4. 切換 baud：`BLEGW$OTABAUD`（EUP 擴充）

SMP 本身沒有 baud 協商，因此以 GCHK 風格的 ASCII 指令切換（在 9600 下送）：

```text
Host  → BLEGW$OTABAUD;115200;<token4>\r\n
ET100 → AT$PD=BLEGW$OTABAUD;1;115200;<token4>\r\n     （回應送完才切換）
雙方切 115200 → 執行 SMP
最後一個 SMP 封包後 30 秒（或切換後 30 秒內沒有 SMP）→ App 自動回 9600；App 重開機也回 9600。Installer 階段則為 115200，不能把它當作 App 已啟動。
```

只接受 `9600`、`115200`，其他回 `;0;`。token 為 4 個英數字元（同 GCHK）。

## 5. 完整流程範例

```text
[9600]   BLEGW$OTABAUD;115200;OB01        → AT$PD=BLEGW$OTABAUD;1;115200;OB01
[115200] OS params read                   → buf_size=1024
         IMG upload off=0 len sha data…   → off=N … 直到 off=len
         IMG state read                   → slot1 = 新版、bootable
         IMG state write hash=slot1,confirm=false → slot1 pending
         OS reset                         → 裝置重開、installer 安裝、新 App TRIAL
[9600]   （約 30 秒後）IMG state read     → slot0 = 新版、confirmed
```

EVB 實測：293 KB，115200 約 53 秒、9600 約 8 分鐘（見 [13 測試報告](../04_testing/13_evb_smp_serial.md)）。

### v0.8.11 INST 交接

HTML 在送 OS reset 前掛上 INST listener，維持 115200 收 Installer 狀態；收到已知 INST v1、txn 匹配的 BOOT;…;9600 才切 9600。3 s 沒觀測到 HELLO／BOOT，或 HELLO 後 30 s 無事件時，退回 9600 的 SMP 查詢；不能把漏行當作已成功或斷言舊 Installer。保留 reset 起算的 180 s 總期限與完整版本／hash／confirmed 驗證；INST CONFIRMED 僅作提示，不能替代映像比對。未知協定主版本只記錄，不依事件切 baud。

App 新增 `BLEGW$GOTASTAT;<tok>`，供查詢實際 boot state 與最後放棄／還原結果；它不含完整映像 hash，不能單獨判定更新成功。試運行 120 s 後 reset，加上安裝／還原時間可能超出 HTML 的 180 s，此時顯示結果未知，稍後再查詢，不擅自增加等待期限。格式及錯誤碼見 [07 INST](07_installer_status.md)；本輪實機由 Stanley 執行。

## 6. BLE transport（v0.8.6）

同一套 SMP 指令（§3）也可經 BLE 使用，nRF Connect Device Manager 可直接更新。

| 項目 | 值 |
|---|---|
| Service | `8D53DC1D-1DB7-4CD3-868B-8A527460AA84`（Nordic MCUmgr SMP） |
| Characteristic | `DA2E7828-FBCE-4E01-AE9E-261174997C48`：write without response／write＋notify |
| 封包 | SMP 封包直接寫入（**無** base64／CRC framing）；一個封包可拆成多次 write，依 SMP header 長度重組（上限 1024 B，2 秒未收齊丟棄） |
| 回應 | notify，每段 ≤ MTU−3（裝置請求 MTU 247） |
| 開放條件 | **只在開發版**（`ET100_SMP_BLE_OPEN`）註冊；無認證，見 06 §6.1 安全聲明 |

- **bootloader info（OS 8）必須回應**：nRF Connect Device Manager 依 `mode` 決定流程。回 `mode=3`（swap with revert）時走「Test → Reset → Confirm」；不回應時 App 會停在「Reset into Firmware Loader required」。
- **單一更新 owner**：UART 與 BLE 同時只能一方更新。一方最後一次 upload／test／confirm 後 30 秒內，另一方的這些指令回 `EBUSY`（rc 10）；state read 不受限。超過 30 秒後另一方可接手，同一份 image 從已寫入的位置續傳。
- EVB 實測：296 KB 約 47–51 秒（5.8–6.3 kB/s）。

## 7. 參考工具

- `fw/tools/evb/smp_serial_ota.py`（以 `smpclient` 實作）：`state`、`echo`、`upload <zip|bin> [--test] [--reset]`、`test <hash>`、`confirm`、`reset`、`--baud 115200`。
- 安裝：`fw/tools/requirements-smp.txt`（`--no-deps`）。
