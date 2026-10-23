# 07 — Installer 安裝狀態協定（INST v1）與試運行期限

| 項目 | 內容 |
|---|---|
| 狀態 | **INST v1 已實作於 v0.8.11**（2026-10-23）：Claude review PASS；**Stanley EVB 實機驗收待執行**；未凍結為量產介面（實機通過且簽章 Q42 定案後再凍結） |
| 實作版本 | v0.8.11（`v0.8.11_installer_status`）；測試步驟見 [18 報告](../04_testing/18_installer_status.md) |
| 適用 | installer（0x08002000）、App（`boot_health`）、`tools/rs232_tester.html`、自家 App（BLE，只查結果） |
| 相關 | [04 SMP serial OTA](04_smp_serial_ota.md)、[05 裝置資訊](05_device_metadata.md)、06 OTA storage design §5 |

> **永久介面警告**：installer 不能經 OTA 更新（只能 SWD）。本協定一旦進入量產 installer 即無法修改；未來的 App／HTML 必須永遠支援 INST v1。

## 1. Stanley 決策（2026-10-23）

| # | 決策 |
|---|---|
| S1 | 加入**試運行期限**：新 App 在期限內未自我確認 → 主動 reset → installer rollback |
| S2 | **試運行維持 1 次**（同 MCUboot test 語意）：試運行期間任何 reset（含斷電）都會 rollback，不做重試次數 |
| S3 | installer／App 以單向狀態行回報安裝流程；RS232 即時、BLE 只查最終結果 |
| S4 | App 的 `APP` 行**只在試運行，或有未回報的終局結果時**才送；一般開機不輸出（2026-10-23，Review m1） |

## 2. 實際流程（含本版變更）

```text
App:  SMP upload → NOR Candidate(0x020000) → 驗證 → test/confirm → PENDING → reset
installer:
  ① 讀 boot state（讀失敗／損毀 → HALT，停機）
  ② TRIAL（上次試運行未確認）→ ROLLBACK_REQUIRED
  ③ PENDING: NOR 初始化 → 驗 Candidate → 備份目前 App 到 NOR 0x220000（已有效則略過）
             → BACKUP_READY → 擦寫內建 App → 驗證 → TRIAL → 跳 App
     NOR 初始化失敗或 Candidate／備份失敗 → CONFIRMED＋ABORTED（保留原 App）
  ④ BACKUP_READY（斷電續作）→ 重新驗來源 → 從頭擦寫 → 驗證 → TRIAL
  ⑤ ROLLBACK_REQUIRED → 由 0x220000 擦寫回舊 App → 驗證 → CONFIRMED＋FAILED
     ④⑤ NOR 初始化失敗 → HALT
  ⑥ 跳轉前開 60 s watchdog（現況）
App（TRIAL）:
  自檢條件（FlashDB 可寫讀、BLE 已廣播、主迴圈連續 30 s）全部成立 → boot_state_confirm → CONFIRMED
  【S1 新增】TRIAL 開始起 120 s 仍未確認 → 輸出 TRIAL_TIMEOUT → 停止餵狗並 NVIC reset → ②⑤ rollback
```

**期限 120 s 的理由**：確認條件最短 30 s；BLE 啟動、FlashDB 初始化與主迴圈中斷重計（> 2 s 即重算）需留餘裕；仍遠短於「卡在試運行」的現況（無限）。數值為建議，施工時可依實測調整。

**installer 安裝期間的 watchdog**：安裝開始啟用 8 s WDT，依實際進度（完成擦寫、驗證讀取）餵狗，不以狀態行送出當健康證據。計數不推測 reset 原因：同一完整交易 hash、同一持久 boot state 連續三次未完成重啟（含斷電／手動 reset）後，下一次開機 PENDING 安全 ABORTED；BACKUP_READY 轉 ROLLBACK_REQUIRED 並驗證、還原 NOR 備份；ROLLBACK_REQUIRED 不限次數。真正 I/O／驗證錯誤仍 HALT。計數只存在結果記錄，讀不到／CRC 錯視為零，不改 boot record。此政策由 Stanley 於聊天明確核准（O3）。

## 3. 實體層

| 項目 | EVB（DEV1.4） | ET-100 實板 |
|---|---|---|
| installer 輸出 | UART3（DAPLink VCOM，即 HTML 所連 port），115200 8N1 | 必須是 HTML 所連的 RS232_0（J0901）。installer 需經 AW9523（I2C5）開 `VDD_PP_3V3_EN`（P0_2）與 `MCU_RS232_EN0`（P1_2），I2C 必須有 timeout、只改必要位元；**狀態輸出初始化失敗不得阻止有效 App 開機**（§10 O1） |
| App 輸出 | 9600 8N1（UART3） | 9600（RS232_0，待確認） |

## 4. 行格式

```text
ET100$INST;<ver>;<src>;<event>;<txn>[;<field>…]*<crc>\r\n
```

| 部分 | 規則 |
|---|---|
| 前綴 | `ET100$INST;`，區分大小寫 |
| `<ver>` | 主版本，十進位；本文件 `1`。同一主版本內只能**在行尾附加**可選欄位或新增事件 |
| `<src>` | `L` installer、`A` App |
| `<event>` | §5 |
| `<txn>` | **更新交易 ID** = boot record `hash`（被安裝／試運行的完整檔案 SHA-256）前 8 個大寫 hex。整個更新生命週期（安裝、續作、試運行、rollback）不變；無交易時為 `-`。**不使用 boot record `sequence`**（每次寫入都會變） |
| `<field>` | 可列印 ASCII 0x20–0x7E，不含 `;`／`*`；數值十進位、≤ 10 位；字串欄位 ≤ 31 字元 |
| `*<crc>` | **CRC-16/XMODEM**（poly 0x1021、init 0x0000、不反射、xorout 0x0000），計算範圍為行首 `E` 起至 `*` 前一個字元；4 位大寫 hex。只用於濾除雜訊，**不是認證** |
| 長度 | 整行含 `\r\n` ≤ 120 bytes |

**測試向量**：
```text
123456789                                           → 31C3（XMODEM 標準 check value）
ET100$INST;1;L;WRITE;8755CE49;262144;314752         → *2926
ET100$INST;1;L;HELLO;8755CE49;0.8.11;PENDING        → *8208
```

**Host 解析**：前綴不符 → 忽略（可能是除錯行）；CRC 錯 → 丟棄；`<ver>` 不支援 → 只記錄與提示，**不得**據此切 baud 或判定成功，退回 SMP 查詢；未知事件或多出的欄位 → 忽略。

## 5. 事件

### 5.1 installer（`src=L`，115200）

| 事件 | 欄位（`txn` 之後） | 說明 |
|---|---|---|
| `HELLO` | `<installer_ver>;<state>` | 只在 boot state 為 `PENDING`／`BACKUP_READY`／`TRIAL`／`ROLLBACK_REQUIRED`／`CORRUPT` 時送；一般 `CONFIRMED`／`NONE` 開機**不送**（量產車機不受干擾） |
| `START` | `<mode>;<length>` | `mode` = `INSTALL`／`RESUME`（BACKUP_READY 續作）／`ROLLBACK`；`length` 為目標長度 |
| `CHECK` | `<OK\|FAIL>` | 來源 image 驗證；同一交易可能出現多次 |
| `BACKUP` | `<done>;<total>` 或 `SKIP` | `total` = 舊 App 長度 |
| `ERASE` | `<done>;<total>` | `total` = 目標長度向上對齊 4 KB |
| `WRITE` | `<done>;<total>` | `total` = 目標長度 |
| `VERIFY` | `<OK\|FAIL>` | 內建 App 寫入後驗證 |
| `RESULT` | `<INSTALLED\|ABORTED\|ROLLED_BACK>;<code>` | **對應的 boot state 持久寫入成功後**才送 |
| `BOOT` | `<TRIAL\|CONFIRMED\|NONE>;<baud>` | UART TEMT 後即將跳轉；`baud` = App 端速率（9600）。**只是交接通知，不代表成功** |
| `HALT` | `<stage>;<code>` | fail closed 停機（含 `HELLO;…;CORRUPT` 之後的 `HALT;STATE_READ`） |

**進度規則**：`BACKUP`／`ERASE`／`WRITE` 每階段在 0、每 10%、結束時送（每階段 ≤ 12 行，與 image 大小無關）。續作或重跑時進度**可歸零**；host 不得假設單調遞增跨越 `START`。

### 5.2 App（`src=A`，9600，經既有 TX mutex 整行送出，不得在 callback／critical section 內送）

| 事件 | 欄位 | 說明 |
|---|---|---|
| `APP` | `<TRIAL\|CONFIRMED\|NONE>;<fw_version>;<last_result>` | 開機後 UART 就緒時**最多送一次，且只在以下情況（S4）**：① boot state 為 `TRIAL`；② boot state 為 `CONFIRMED` 且 `last_update` 為 `FAILED`／`ABORTED`（還原或放棄），而該交易的結果**尚未回報過**。回報後以 App 端持久旗標記住該交易（例如 FlashDB 新 key 存已回報的 txn），之後一般開機不再送；旗標寫入失敗時允許下次開機重送一次。試運行中已送出 `CONFIRMED` 的交易視為已回報。`last_result` 取自 §6 結果記錄（`-` 無記錄） |
| `CONFIRMED` | — | 自我確認寫入成功 |
| `TRIAL_TIMEOUT` | `<elapsed_s>;<reason>` | S1：期限到仍未確認，即將 reset；`reason` = 最先未滿足的條件（`STORAGE`／`BLE`／`LOOP`／`WRITE`） |

主動行可能漏收（HTML 切 baud 期間）；host 一律可用 §6 查詢補回。

### 5.3 序列

```text
正常：   L HELLO;PENDING → START;INSTALL → CHECK;OK → BACKUP… → ERASE… → WRITE… → VERIFY;OK
         → RESULT;INSTALLED;0 → BOOT;TRIAL;9600 → A APP;TRIAL → A CONFIRMED
期限逾時：… A APP;TRIAL → A TRIAL_TIMEOUT;120;BLE →（reset）→ L HELLO;TRIAL → START;ROLLBACK
         → ERASE… → WRITE… → VERIFY;OK → RESULT;ROLLED_BACK;9 → BOOT;CONFIRMED;9600 → A APP;CONFIRMED;…;ROLLED_BACK
當機／斷電：（WDT 或斷電 reset，無 TRIAL_TIMEOUT）→ 同上 rollback
放棄：   L HELLO;PENDING → START;INSTALL → CHECK;FAIL → RESULT;ABORTED;<code> → BOOT;CONFIRMED;9600
停機：   L HELLO;-;CORRUPT → HALT;STATE_READ;<code>
```

**HTML 注意**：rollback 時 HTML 已在 9600，聽不到 installer 的 115200 行 → **不提供 rollback 即時進度**，等舊 App 回來後以 `APP`／§6 查詢得知 `ROLLED_BACK`。看到 `HELLO;TRIAL` 只表示「準備還原」，不是「已還原」。

## 6. 結果記錄與查詢

- **boot record 不變**（維持現有 v2 100 bytes／CRC／commit 位置；舊 installer 可讀）。
- 另設**安裝結果／中斷計數記錄**：NOR **0x460000–0x461FFF**，兩個 4 KB sector 輪替，新記錄 CRC／commit／讀回成功前保留舊 sector。`0x01F000` 確實被 journal 32-sector 輪替使用，不得挪用。記錄不是 App 選擇／已安裝的權威；只有 O3 核准的中斷計數用途會影響放棄／還原策略。格式含 `EIR1`、版本、序號、完整交易 hash、boot state、嘗試次數、result／code／stage、installer 版本與 CRC32／commit；細節由共用模組定義。讀不到／CRC 錯視為無記錄、零次。App 寫入仍由 OTA worker 單獨擁有 NOR，不在 BLE callback／critical section 操作 NOR。
- **RS232**：`BLEGW$GOTASTAT;<tok>` → `AT$PD=BLEGW$GOTASTAT;1;<state>;<txn>;<result>;<code>;<fw_version>;<tok>`。
- **BLE**：EUP AIoT Communication Service 新增唯讀 **`0x09 GET_OTA_STATUS`**（gateway 規格 0x09 目前未使用；0x02 已預留勿用；**需與 gateway 規格擁有者確認分配**）。TLV：`0x01 state`、`0x02 result`、`0x03 code`、`0x04 txn`、`0x05 running image SHA-256（32 B）`、`0x06 fw_version`；未知 tag 由 App 略過。

### 6.1 v0.8.11 wire 定義

- `GOTASTAT;0;<tok>` 代表 boot／映像快照尚未就緒；沒有 NOR 結果記錄時回 `txn=-;result=-;code=0`，不把缺失記錄當成功。state／result 使用下列 enum 的英文名稱。
- BLE tags：state／result 為 uint8；code 為 uint32 LE；txn 為 8 位大寫 hex ASCII 或 `-`；running hash 為完整 signed-file SHA-256（與 SMP 的 MCUboot TLV hash 不同）；fw_version 為 UTF-8 字串。不就緒回 protocol INTERNAL_ERROR，不讀 NOR。
- state：NONE=0、PENDING=1、CONFIRMED=2、BACKUP_READY=3、TRIAL=4、ROLLBACK_REQUIRED=5。result：NONE=0（文字 `-`）、RUNNING=1、INSTALLED=2、ABORTED=3、ROLLED_BACK=4、TRIAL_TIMEOUT=5、HALT=6、CONFIRMED=7。
- EIR1 記錄為 128 bytes，整數均 LE：magic@0、ver=1@4、sequence@8、完整 txn hash@12（32 B）、state@44、attempts@48、result@52、code@56、stage@60、installer_ver@64（16-byte NUL 字串）、保留@80–111、CRC32@112（涵蓋 0–111）、保留@116–119、commit `0x54494D43`@120、保留@124–127。保留 bytes 寫零；最後 8 bytes 獨立 program，避免與資料共用同一 STIG 8-byte PP 單位。
- stage：NONE=0、NOR_INIT=1、STATE_READ=2、STATE_WRITE=3、CHECK=4、BACKUP=5、ERASE=6、WRITE=7、VERIFY=8、BOOT=9。序號採 32-bit wrap-aware 比較，同序異記錄視為無效。
- 若 NOR 不可用，不能保證保存 HALT／ABORTED 原因；App 可由 boot state 的 CONFIRMED＋last_update 推導已放棄／已還原，回 code 12，不能推測 NOR／來源／備份是哪一項錯誤。完整成功仍需 SMP／BLE 完整 hash 與 confirmed，GOTASTAT 單獨沒有完整 hash，不能獨自證明成功。
- App 只在 OTA worker 存取 NOR；BLE／RS232 讀快照。SMP Test／Confirm 後刷新 boot 快照，避免回報開機時的過期狀態。

## 7. 結果碼（INST code，穩定介面）

| code | 名稱 | 說明 |
|---:|---|---|
| 0 | OK | |
| 1 | NOR_INIT | NOR 初始化／授權失敗 |
| 2 | SRC_INVALID | 來源 image 驗證失敗（Candidate 或 Rollback） |
| 3 | BACKUP_FAIL | 備份寫入或驗證失敗 |
| 4 | ERASE_FAIL | 內建 Flash 擦除失敗 |
| 5 | WRITE_FAIL | 內建 Flash 寫入或來源讀取失敗 |
| 6 | VERIFY_FAIL | 寫入後驗證失敗 |
| 7 | STATE_WRITE | boot state 寫入失敗 |
| 8 | STATE_READ | boot state 讀取失敗或損毀 |
| 9 | TRIAL_TIMEOUT | 試運行期限到未確認（S1） |
| 10 | TRIAL_RESET | 試運行期間 reset（當機、WDT 或斷電；installer 無法區分） |
| 11 | RETRY_LIMIT | 同交易／同持久狀態已中斷三次，依 O3 放棄或轉回滾 |
| 12 | RESULT_UNAVAILABLE | boot state 可證明 ABORTED／ROLLED_BACK，但 NOR 診斷原因不可讀、損毀或未成功保存；不推測原始錯誤 |

內部原始錯誤（image／NOR／journal 各自的負值）**不直接輸出**；除錯時可附加在行尾可選欄位 `<domain>:<raw>`。既有碼不得改義，只能新增。

## 8. Host（HTML）行為

1. **送 reset 前**就掛好 INST 旁路 parser，維持 115200 被動接收。
2. 3 s 內未收到 `HELLO`：只能判定「未觀測到」（可能是舊 installer 或未安裝），退回現行 SMP 流程。
3. 總期限維持 **reset 起 180 s**；任一階段 30 s 無事件只提示「無回應」，不另加期限。
4. 收到 `BOOT` → 依 `baud` 切換 → 以現有 5–10 s SMP／`GOTASTAT` 輪詢補足漏收的 `APP`／`CONFIRMED`。
5. **成功**：完整 hash／版本與上傳檔案相符且 confirmed（SMP image state 或 `GOTASTAT`）。**還原**：`ROLLED_BACK` 且舊 image confirmed。結果未知時不得顯示成功。
6. 收到 `HALT` → 立即顯示失敗與 code。

實作補充：BOOT 行送完、TEMT 清空後保留 300 ms 交接窗口；UART 等待上限不拖住有效 App。HELLO 遺失但收到有效且 txn 匹配的 BOOT 仍可切 9600；HELLO 已收到但 BOOT 遺失時，30 s 無事件後切回 9600，以 SMP 補查，不延長 180 s。未知主版本不可觸發切換；新欄位可略過。試運行逾時、回滾皆可能使整體超過 180 s，此時顯示結果未知，待 App 回來後另查 GOTASTAT／SMP，不自動增加總期限。

## 9. 輸出限制

- 每次交易輸出 ≤ 約 6 KB（4 階段 × 12 行 × ≤ 120 B），115200 下 < 0.6 s。
- UART 送出失敗或逾時就丟棄該行，**不得阻塞安裝**。
- 量產 installer 預設**不輸出人讀除錯行**（開發版保留）。INST 行本身也是對 RS232_0 另一端的主動輸出，需確認客戶車機可忽略（§10 O4）。
- 現有接收入口（`ET100-LOAD`、`ET100-WDT`）屬開發功能，與本協定無關；量產是否移除另行決策。

v0.8.11：Installer source 的人讀 printf 預設關閉（量產為 0）；EVB 開發 target `evb_installer.mk` 預設 `ET100_INSTALLER_DEBUG=1`，保留既有 loader／trial 工具需要的文字，可明確覆寫為 0。LDR1 的 READY／ACK 仍獨立送出。INST UART 寫入採 DWT＋有限迴圈，整行最多等待 50 ms；不以送 log 餵狗。8 s WDT 在有界 NOR init／授權完成後啟用，擦寫／驗證讀取完成才餵狗；HALT 停止 WDT 避免錯誤重啟迴圈。App 120 s 逾時停止餵狗，留最多 1 s 給 worker 保存與 UART 狀態，然後 reset。ROLLED_BACK 的 code 9／10／11 是還原原因，不表示還原失敗。

App boot-state 寫入入口同樣檢查絕對期限（交易原有 critical section 內），逾時後拒絕 erase／program；因此等待 reset 的最多 1 s 期間，晚到的 SMP Confirm 不能再把 TRIAL 改成 CONFIRMED。期限前已完成確認的正常 App 不受此限制。

## 10. 施工前需確認（open）

| # | 事項 |
|---|---|
| O1 | ET-100 實板 RS232_0 的 MCU UART／pinmux 與 installer 內 AW9523 初始化（實板到手前 EVB 先用 UART3） |
| O2 | **已決定**：journal 使用 0x000000–0x01FFFF；結果雙 sector 改用 0x460000–0x461FFF，日誌／預留從 0x462000 起 |
| O3 | **已決定**：同交易／同持久狀態連續三次未完成重啟後，PENDING→ABORTED、BACKUP_READY→ROLLBACK_REQUIRED；回滾不限次數。真實 I/O／驗證錯誤維持 HALT；結果記錄失效視為零次 |
| O4 | 客戶車機是否容忍 RS232_0 上的 INST 主動輸出（只在更新／還原時出現） |
| O5 | `0x09 GET_OTA_STATUS` 分配需與 gateway 協定擁有者確認 |

## 11. 施工單草案（v0.8.11，**Codex 實作、Claude review、Stanley 實機測試**；開工時搬入 `fw/IMPLEMENTATION_PLAN.md`）

| 項目 | 檔案 |
|---|---|
| INST 行產生（格式、CRC、節流）host 可測模組 | `fw/common/inst_status.c／.h`（installer 與 App 共用） |
| installer 事件輸出、結果碼轉換、結果記錄寫入、安裝期間 WDT | `fw/installer/main.c`、`transaction.c`、`install.c` |
| App：`APP`／`CONFIRMED`／`TRIAL_TIMEOUT`、**120 s 試運行期限**、結果記錄讀取 | `fw/src/boot_health.c`、`serial_gchk.c` |
| RS232 `GOTASTAT`、BLE `0x09 GET_OTA_STATUS` | `fw/src/serial_gchk.c`、`eup_meta_protocol.c` |
| HTML INST parser 與狀態機 | `tools/rs232_tester.html`、`tools/tests/` |

**驗收（精簡）**：
1. Host：格式／CRC 測試向量、節流、所有序列（正常、續作、ABORTED、ROLLED_BACK、TRIAL_TIMEOUT、HALT、狀態損毀、各狀態 NOR 初始化失敗）、最大 image 的輸出上限、漏收 `HELLO`／`BOOT`／`APP` 時的 host 行為。
2. EVB：正常更新（HTML 顯示各階段並成功）、`ET100_TEST_NO_CONFIRM` 觸發 120 s 期限 → rollback、損毀 Candidate → ABORTED、試運行中斷電 → rollback。
3. **EVB 實機測試（含 installer SWD 燒錄）由 Stanley 執行**；實作者只做 host 測試與 build，提供測試步驟與測試用 zip。B 資訊段不變。

## 附錄 A — RS232 OTA 完整範例（各情境實際行）

條件：EVB 由 0.8.7-dev 更新到 0.8.71-dev，檔案 298184 B，交易 ID `8755CE49`。`L` = installer（115200）、`A` = App（9600）、`→` = HTML 送出、`←` = 裝置回應。行尾 CRC 依 §4 實際計算，可直接當 host test 向量（行尾 `\r\n` 省略）。進度行只列代表性幾行（實際為 0、每 10%、100%）。

### A.1 正常更新

```text
① 更新前檢查（9600）
→ BLEGW$GCHK;A001
← AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.7-dev;0;A001
→ BLEGW$GFWINFO;A002
← AT$PD=BLEGW$GFWINFO;1;EUP_ET100;ET100_EVB_DEV1.4;0.8.7-dev;fr3068e_c;A002      ← 比對檔案硬體
→ BLEGW$OTABAUD;115200;A003
← AT$PD=BLEGW$OTABAUD;1;115200;A003

② 上傳（115200，SMP，約 53 s）
SMP upload off=0…298184 → 驗證 → SMP test（或 confirm）→ PENDING → SMP reset
（HTML 送 reset 前已掛好 INST parser，維持 115200）

③ installer（115200）
ET100$INST;1;L;HELLO;8755CE49;0.8.11;PENDING*8208
ET100$INST;1;L;START;8755CE49;INSTALL;298184*3944
ET100$INST;1;L;CHECK;8755CE49;OK*19AA
ET100$INST;1;L;BACKUP;8755CE49;0;298184*7B47
ET100$INST;1;L;BACKUP;8755CE49;29818;298184*2A52
ET100$INST;1;L;BACKUP;8755CE49;298184;298184*1289
   （NOR 已有相同備份時改為：ET100$INST;1;L;BACKUP;8755CE49;SKIP*7249）
ET100$INST;1;L;ERASE;8755CE49;0;299008*7530
ET100$INST;1;L;ERASE;8755CE49;299008;299008*7C22
ET100$INST;1;L;WRITE;8755CE49;0;298184*C19E
ET100$INST;1;L;WRITE;8755CE49;149092;298184*2725
ET100$INST;1;L;WRITE;8755CE49;298184;298184*216D
ET100$INST;1;L;VERIFY;8755CE49;OK*829A
ET100$INST;1;L;RESULT;8755CE49;INSTALLED;0*FE9A          ← 安裝完成（不等於更新成功）
ET100$INST;1;L;BOOT;8755CE49;TRIAL;9600*6287             ← HTML 切 9600

④ 新 App 試運行（9600）
ET100$INST;1;A;APP;8755CE49;TRIAL;0.8.71-dev;-*E5F3
（約 30 s 自檢）
ET100$INST;1;A;CONFIRMED;8755CE49*A7F6                   ← 更新成功
→ BLEGW$GOTASTAT;A004                                    （漏收時補查）
← AT$PD=BLEGW$GOTASTAT;1;CONFIRMED;8755CE49;INSTALLED;0;0.8.71-dev;A004
```

### A.2 試運行 120 s 逾時 → 還原

```text
ET100$INST;1;A;APP;8755CE49;TRIAL;0.8.71-dev;-*E5F3
ET100$INST;1;A;TRIAL_TIMEOUT;8755CE49;120;BLE*13FD       ← 原因：BLE 未廣播；App 主動 reset
（以下 115200，HTML 已在 9600 收不到，僅 log）
ET100$INST;1;L;HELLO;8755CE49;0.8.11;TRIAL*C23F
ET100$INST;1;L;START;8755CE49;ROLLBACK;298184*2D15
… ERASE／WRITE／VERIFY …
ET100$INST;1;L;RESULT;8755CE49;ROLLED_BACK;9*D450         ← 9 = TRIAL_TIMEOUT
ET100$INST;1;L;BOOT;8755CE49;CONFIRMED;9600*EFC3
（9600）
ET100$INST;1;A;APP;8755CE49;CONFIRMED;0.8.7-dev;ROLLED_BACK*42C6
```

### A.3 試運行中當機或斷電 → 還原

同 A.2，但沒有 `TRIAL_TIMEOUT` 行，結果碼為 10：

```text
ET100$INST;1;L;RESULT;8755CE49;ROLLED_BACK;10*6403        ← 10 = TRIAL_RESET（當機／WDT／斷電）
```

### A.4 NOR 內新檔損毀 → 放棄，保留舊版

```text
ET100$INST;1;L;HELLO;8755CE49;0.8.11;PENDING*8208
ET100$INST;1;L;START;8755CE49;INSTALL;298184*3944
ET100$INST;1;L;CHECK;8755CE49;FAIL*A7E1
ET100$INST;1;L;RESULT;8755CE49;ABORTED;2*48CE             ← 2 = SRC_INVALID，內建 App 未動
ET100$INST;1;L;BOOT;8755CE49;CONFIRMED;9600*EFC3
```

### A.5 寫入內建 App 時斷電 → 下次開機續作

```text
ET100$INST;1;L;HELLO;8755CE49;0.8.11;BACKUP_READY*0E15
ET100$INST;1;L;START;8755CE49;RESUME;298184*1948
ET100$INST;1;L;CHECK;8755CE49;OK*19AA
ET100$INST;1;L;ERASE;8755CE49;0;299008*7530               ← 進度歸零：重新擦寫，不是接續
…（之後同 A.1 ③）
```

### A.6 同一狀態連續中斷 3 次（O3）

```text
PENDING（內建 App 未動）：
ET100$INST;1;L;RESULT;8755CE49;ABORTED;11*74ED            ← 11 = RETRY_LIMIT，開舊版
BACKUP_READY（內建 App 可能已部分覆寫）→ 改走還原：
ET100$INST;1;L;START;8755CE49;ROLLBACK;298184*2D15
…
ET100$INST;1;L;RESULT;8755CE49;ROLLED_BACK;11*7422
ROLLBACK_REQUIRED：不設上限，持續重試還原
```

### A.7 不可恢復錯誤 → 停機（需人工處理）

```text
ET100$INST;1;L;HELLO;-;0.8.11;CORRUPT*F308                ← boot state 損毀
ET100$INST;1;L;HALT;-;STATE_READ;8*2723

ET100$INST;1;L;VERIFY;8755CE49;FAIL*55A3                  ← 寫入後驗證失敗（硬體問題）
ET100$INST;1;L;HALT;8755CE49;VERIFY;6*AA3C
```

### A.8 一般開機

boot state 為 `CONFIRMED`（無更新）：installer **不輸出任何 `ET100$INST` 行**；App 也不送 `APP`（S4）。還原／放棄後的**第一次**開機 App 送一次 `APP;CONFIRMED;…;ROLLED_BACK|ABORTED`，之後開機皆無輸出。
