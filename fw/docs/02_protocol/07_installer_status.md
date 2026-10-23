# 07 — Installer 安裝狀態協定（INST v1）與試運行期限

| 項目 | 內容 |
|---|---|
| 狀態 | **DRAFT v2**（2026-10-23，Claude）：v1 草案經 Codex 分析後修訂；**未凍結、未實作** |
| 目標版本 | v0.8.11（排在 v0.8.10 之後） |
| 適用 | installer（0x08002000）、App（`boot_health`）、`tools/rs232_tester.html`、自家 App（BLE，只查結果） |
| 相關 | [04 SMP serial OTA](04_smp_serial_ota.md)、[05 裝置資訊](05_device_metadata.md)、06 OTA storage design §5 |

> **永久介面警告**：installer 不能經 OTA 更新（只能 SWD）。本協定一旦進入量產 installer 即無法修改；未來的 App／HTML 必須永遠支援 INST v1。

## 1. Stanley 決策（2026-10-23）

| # | 決策 |
|---|---|
| S1 | 加入**試運行期限**：新 App 在期限內未自我確認 → 主動 reset → installer rollback |
| S2 | **試運行維持 1 次**（同 MCUboot test 語意）：試運行期間任何 reset（含斷電）都會 rollback，不做重試次數 |
| S3 | installer／App 以單向狀態行回報安裝流程；RS232 即時、BLE 只查最終結果 |

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

**installer 安裝期間的 watchdog**（Codex Q9，本版納入）：安裝開始即啟用 WDT，**依實際進度**（每完成一個 4 KB 擦寫／每 64 KB hash）餵狗，timeout 需涵蓋最慢單次操作（4 KB erase 上限 750 ms）的數倍，建議 8 s。WDT reset 後依持久狀態續作（③→PENDING 重跑、④ 重擦寫）。為避免決定性錯誤造成無限重啟，同一狀態連續 WDT reset 的處理留待施工單定義（§10 O3）。

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
| `APP` | `<TRIAL\|CONFIRMED\|NONE>;<fw_version>;<last_result>` | 開機後 UART 就緒時送一次；`last_result` 取自 §6 結果記錄（`-` 無記錄） |
| `CONFIRMED` | — | 自我確認寫入成功 |
| `TRIAL_TIMEOUT` | `<elapsed_s>;<reason>` | S1：期限到仍未確認，即將 reset；`reason` = 最先未滿足的條件（`STORAGE`／`BLE`／`LOOP`／`WRITE`） |

主動行可能漏收（HTML 切 baud 期間）；host 一律可用 §6 查詢補回。

### 5.3 序列

```text
正常：   L HELLO;PENDING → START;INSTALL → CHECK;OK → BACKUP… → ERASE… → WRITE… → VERIFY;OK
         → RESULT;INSTALLED;0 → BOOT;TRIAL;9600 → A APP;TRIAL → A CONFIRMED
期限逾時：… A APP;TRIAL → A TRIAL_TIMEOUT;120;BLE →（reset）→ L HELLO;TRIAL → START;ROLLBACK
         → ERASE… → WRITE… → VERIFY;OK → RESULT;ROLLED_BACK;0 → BOOT;CONFIRMED;9600 → A APP;CONFIRMED;…;ROLLED_BACK
當機／斷電：（WDT 或斷電 reset，無 TRIAL_TIMEOUT）→ 同上 rollback
放棄：   L HELLO;PENDING → START;INSTALL → CHECK;FAIL → RESULT;ABORTED;<code> → BOOT;CONFIRMED;9600
停機：   L HELLO;-;CORRUPT → HALT;STATE_READ;<code>
```

**HTML 注意**：rollback 時 HTML 已在 9600，聽不到 installer 的 115200 行 → **不提供 rollback 即時進度**，等舊 App 回來後以 `APP`／§6 查詢得知 `ROLLED_BACK`。看到 `HELLO;TRIAL` 只表示「準備還原」，不是「已還原」。

## 6. 結果記錄與查詢

- **boot record 不變**（維持現有 v2 100 bytes／CRC／commit 位置；舊 installer 可讀）。
- 另設**安裝結果記錄**（非開機權威，只供診斷顯示）：NOR **0x01F000**（journal 區最後一個 4 KB sector，需確認 journal 未使用，§10 O2），格式 `magic "EIR1" | ver | txn(4) | result | code | stage | installer_ver | crc32`，由 installer 在 `RESULT`／`HALT` 時寫入、App 的 `TRIAL_TIMEOUT` 也寫入。讀不到或 CRC 錯 → 視為無記錄。
- **RS232**：`BLEGW$GOTASTAT;<tok>` → `AT$PD=BLEGW$GOTASTAT;1;<state>;<txn>;<result>;<code>;<fw_version>;<tok>`。
- **BLE**：EUP AIoT Communication Service 新增唯讀 **`0x09 GET_OTA_STATUS`**（gateway 規格 0x09 目前未使用；0x02 已預留勿用；**需與 gateway 規格擁有者確認分配**）。TLV：`0x01 state`、`0x02 result`、`0x03 code`、`0x04 txn`、`0x05 running image SHA-256（32 B）`、`0x06 fw_version`；未知 tag 由 App 略過。

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

內部原始錯誤（image／NOR／journal 各自的負值）**不直接輸出**；除錯時可附加在行尾可選欄位 `<domain>:<raw>`。既有碼不得改義，只能新增。

## 8. Host（HTML）行為

1. **送 reset 前**就掛好 INST 旁路 parser，維持 115200 被動接收。
2. 3 s 內未收到 `HELLO`：只能判定「未觀測到」（可能是舊 installer 或未安裝），退回現行 SMP 流程。
3. 總期限維持 **reset 起 180 s**；任一階段 30 s 無事件只提示「無回應」，不另加期限。
4. 收到 `BOOT` → 依 `baud` 切換 → 以現有 5–10 s SMP／`GOTASTAT` 輪詢補足漏收的 `APP`／`CONFIRMED`。
5. **成功**：完整 hash／版本與上傳檔案相符且 confirmed（SMP image state 或 `GOTASTAT`）。**還原**：`ROLLED_BACK` 且舊 image confirmed。結果未知時不得顯示成功。
6. 收到 `HALT` → 立即顯示失敗與 code。

## 9. 輸出限制

- 每次交易輸出 ≤ 約 6 KB（4 階段 × 12 行 × ≤ 120 B），115200 下 < 0.6 s。
- UART 送出失敗或逾時就丟棄該行，**不得阻塞安裝**。
- 量產 installer 預設**不輸出人讀除錯行**（開發版保留）。INST 行本身也是對 RS232_0 另一端的主動輸出，需確認客戶車機可忽略（§10 O4）。
- 現有接收入口（`ET100-LOAD`、`ET100-WDT`）屬開發功能，與本協定無關；量產是否移除另行決策。

## 10. 施工前需確認（open）

| # | 事項 |
|---|---|
| O1 | ET-100 實板 RS232_0 的 MCU UART／pinmux 與 installer 內 AW9523 初始化（實板到手前 EVB 先用 UART3） |
| O2 | NOR 0x01F000 是否未被 download journal 使用；或改放其他保留位置 |
| O3 | installer 安裝期間 WDT 連續 reset 的上限與處理（避免無限重啟；達上限時 HALT 或 ABORTED） |
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
