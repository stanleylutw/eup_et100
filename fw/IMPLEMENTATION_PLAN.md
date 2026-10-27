# IMPLEMENTATION_PLAN — v0.9.6 F407 layout v2／Installer USB

日期：2026-10-28；依 Stanley 本輪明確需求實作。分支 `v0.9.6_f407_installer_usb`，base `v0.9.5_tester_f407`／`3f02a8d`，不合併／提交／推送，不操作硬體。

## 本輪範圍與決策

- 僅修改 `tools/rs232_tester.html`、`tools/tests/test_f407_ota.cjs`、本施工單，新增 `fw/docs/04_testing/29_f407_installer_usb.md`。其他 repo／firmware／SDK／ET-100 wire format 不改。
- F407 圖改為 layout v2：Installer 48 KB、Boot state A `0x0800C000`／16 KB、B `0x08010000`／16 KB、Sector 4 其餘 `0x08014000`／48 KB 不可使用、App 384 KB；移除 F407 Settings，NOR 配置保持 v0.9.5。
- USB 重連核對 UID 後，以 GCHK fw 的 `-inst` 區分 Installer。Installer 僅收 INST 進度，不查 SMP；BOOT 後預期再斷線，重連 App 再查 SMP／GOTASTAT。INST 在 UID 確認前暫存，錯 UID 的事件不得改變 OTA 狀態；HALT 立即失敗，安靜／未回 App 受總期限限制。
- 頂欄維持原 App 版本，Installer 身分與 GFWINFO 不寫入 App metadata cache，也不作檔案相容性依據。手動查詢若正連 Installer，明確拒絕，不發 SMP。
- 採建議的固定期限方案：F407 profile 為 240 秒，ET-100 180 秒不變；不因 BOOT／多次重連重設期限。
- F407 EUPFWV1 offset 278 的 flags bit0 必須為 1；CRC／SHA 驗證仍保留。ET-100 不檢查此旗標。
- mock 驗證安裝 USB→Installer→App、TRIAL_TIMEOUT→rollback Installer→舊 App（超過 180 秒仍成功）、無旗標載入拒絕、HALT／總期限、錯 UID／錯 txn、百分比與 layout v2 圖，重跑既有 tester 測試。實機驗收只由 Stanley 操作。

規格已唯讀核對 F407 `common/flash_layout.h`、設計 §4.1／§8.1、`installer/usb_link.c`、`comm.md` 與 `docs/REVIEW_REPORT.md`。F407 實機紀錄為來源 repo 的既有結果，不能計作本次 HTML 實機驗收。

---

## 前一輪 v0.9.5 歷史施工單（已提交 3f02a8d；下文舊期限／layout 由上文覆蓋）

> 撰寫：Claude Code，2026-10-27（於 eup_canbus_32f407 session 起草，2026-10-27 移入本 repo）。
> 草稿來源：`~/Firmware/EUP/eup_canbus_32f407/docs/handoff/et100_v0.9.5_tester_f407_IMPLEMENTATION_PLAN.md`。

## Branch

```bash
cd ~/Firmware/EUP/eup_et100
git checkout main
git checkout -b v0.9.5_tester_f407
```

**Base：`main`（`6b68c69`，已含 `v0.9.4_tester_header` merge）。** 不 commit／merge／push，除非 Stanley 另行授權。

版號：只改 `tools/` 的 host 工具、不改 ET-100 firmware／協定 → patch +1。

---

## 1. 目標

讓同一份 `tools/rs232_tester.html` 能對 **eup_canbus_32f407（STM32F407）** 做 SMP OTA，流程、畫面與 ET-100 相同（上傳 → NOR → 備份 → installer 寫入 → TRIAL → 確認／rollback），並支援 F407 的 **USB CDC** 連線（reset 後自動重連）。ET-100 既有行為**完全不變**。

F407 韌體規格來源（唯讀）：
- `~/Firmware/EUP/eup_canbus_32f407/docs/01_firmware/01_ota_smp_rtos_design.md` §6（身分）、§9（tester 修改點）
- `~/Firmware/EUP/eup_canbus_32f407/comm.md`「`BLEGW$` 協定」「SMP serial OTA」
- `~/Firmware/EUP/eup_canbus_32f407/docs/IMPLEMENTATION_PLAN.md`（階段 C：installer、INST、GOTASTAT）

## 2. 前提（Claude Code 於 main `6b68c69` 確認：下列寫死值與函式皆存在，`getInfo()`／`connect` 監聽為 0 處）

| 位置 | 現況（寫死 ET-100） |
|---|---|
| `validate()`（約 181.2k 字元處） | 大小 `≤ 0x1c0000`；`project!=='EUP_ET100'`、`!hardware.startsWith('ET100_EVB_')`；manifest `board!=='dev14_fr3068ep_c'`、`soc!=='fr3068e_c'`；SP `≤ 0x20080000`；PC 起點 `0x08020200` |
| `loadFile()` | manifest 內 image `≤ 0x1c0000` |
| `parseInst()` | regex `^ET100\$INST;…` |
| `identify()` | GFWINFO 比對 `file.manifest?.soc \|\| 'fr3068e_c'` |
| `syncOtaSelfTarget()` | `/fr3068/i.test(hwVer) \|\| project==='EUP_ET100'` → `et100`，否則 `gateway`（nRF ZVERO） |
| 刪除／儲存 sensor package | 限 `project==='EUP_ET100' && soc==='fr3068e_c'` → F407 自然排除（**保持**） |
| reset 後流程 | `request(2,0,5)` → `waitInstaller()`（115200 收 INST）→ `reopen(9600)` → 每 5 s 查 image state；180 s 總期限 |
| Web Serial | 只有 `requestPort()`；**沒有** `navigator.serial` `connect`／`disconnect` 監聽，沒有 `getInfo()` |

## 3. 規格

### 3.1 裝置設定表（取代所有寫死值）

新增單一常數表，`validate()`、`loadFile()`、`identify()`、`syncOtaSelfTarget()`、`parseInst()` 全部查表：

| 欄位 | ET-100 | F407 |
|---|---|---|
| `key` | `et100` | `f407` |
| `project` | `EUP_ET100` | `eup_canbus_32f407` |
| `hardware` 規則 | `startsWith('ET100_EVB_')` | `=== 'EUP_stm32f407_dk'` |
| `soc` | `fr3068e_c` | `stm32f407` |
| manifest `board` | `dev14_fr3068ep_c` | `stm32f407_dk` |
| 映像大小上限 | `0x1c0000` | `0x60000` |
| SP 範圍 | `0x20000000 < sp ≤ 0x20080000` | `0x20000000 < sp ≤ 0x20020000` |
| PC 起點 | `0x08020200` | `0x08020200` |
| INST 前綴 | `ET100$INST;` | `F407$INST;` |
| Sensor package／relay | 支援 | **不支援**（相關按鈕隱藏或停用） |
| 預設 transport | RS232 | 依 §3.3 判斷 USB 或 RS232 |

- 裝置判斷：`GFWINFO` 的 `project`＋`soc` 對表；判斷不出 → 視為 nRF gateway（現行 ZVERO 路徑，不變）。
- 檔案判斷：EUPFWV1 `project`／`hardware` ＋ manifest `board`／`soc` 必須與**同一筆**設定相符，且與已連線裝置相同 `key`，否則拒絕（錯誤訊息列出裝置與檔案的 project／hardware／soc）。
- OTA 面板 F407 沿用 ET-100 面板（`otaMode === 'et100'` 的 UI），標題與按鈕文字改為依裝置顯示「ET-100」或「F407」。

### 3.2 INST 解析

- `parseInst()` 接受設定表中所有前綴；CRC-16/XMODEM 範圍為「行首第一個字元起至 `*` 前」。
- 解析結果帶 `device` 欄位；只接受與目前裝置相符的前綴。
- 測試向量（F407 前綴，由本工作計算後寫入測試）：與 ET-100 07 §4 相同內容，僅前綴改為 `F407$INST;`。

### 3.3 USB CDC 模式（F407）

- 連線後以 `port.getInfo()` 判斷：`usbVendorId === 0x0483 && usbProductId === 0x5740` → **USB 模式**；其他（含 USB-RS232 轉接線）→ RS232 模式。
- USB 模式：
  - `OTABAUD` 照送（F407 回 `;1;` 但不切換）；`reopen(115200)`／`reopen(9600)` 改為**不關閉 port、不改 baud**。
  - 送 OS reset 後 port 會斷線（F407 installer 沒有 USB）：
    1. 不呼叫 `waitInstaller()`，畫面顯示「安裝中（USB 模式無即時進度）」。
    2. 監聽 `navigator.serial` 的 `connect` 事件，並每 1 s 呼叫 `navigator.serial.getPorts()`；找到 VID/PID 相同的 port 就 `open()`。
    3. **Web Serial 無法讀 USB 序號**：重新開啟後先送 `BLEGW$GCHK`，確認 identity（UID 24 hex）與 reset 前相同才繼續；不同 → 關閉並繼續等待。
    4. 之後沿用現有流程：每 5 s 查 SMP image state 與 `GOTASTAT`，同樣的 180 s 總期限與成功／還原判定。
  - 斷線期間的 read loop 錯誤不可觸發一般的「連線中斷」清除流程（OTA 狀態、Log 保留）。
- RS232 模式（ET-100、或 F407 經 DB9）：完全沿用現行流程（115200 收 INST → `BOOT` 後切 9600）。

### 3.4 F407 硬體與 Flash／NOR 配置（OTA 視覺化必須依裝置切換）

現行 tester 的 ET-100 OTA 視覺化寫死 ET-100 配置：`et100FlashPartitions`（ROM A／B、Installer 0x08002000 120 KB、App 0x08020000 1792 KB、Settings 0x081E0000、Boot state 0x081FC000）、NOR 分區函式（Journal、Candidate 2 MB、Rollback 2 MB、Sensor OTA、Installer 結果 0x460000）、進度條容量（Rollback 2 MB、App 1792 KB）與「外部 NOR · 16 MB（EVB）」等文字。**F407 若沿用會顯示錯誤的記憶體圖**，必須改為依裝置設定表選擇。

**F407 硬體摘要**（來源：`eup_canbus_32f407/docs/03_hardware/STM32F407VET6_DK_hardware.md`）

| 項目 | 值 |
|---|---|
| MCU | STM32F407VET6（Cortex-M4F 168 MHz，Flash 512 KB，SRAM 128 KB + CCM 64 KB） |
| 外部 NOR | W25Q64（**8 MB**，JEDEC `EF 40 17`；電路圖標 W25Q128，實機為 W25Q64），SPI2 |
| OTA 介面 | USB CDC（U8，VID/PID `0483:5740`）、USART1 經 MAX232 到 DB9（RS232） |

**F407 內部 Flash（512 KB）** — 取代 `et100FlashPartitions`：

| 名稱 | 位址 | 大小 | 說明 |
|---|---|---|---|
| Installer | 0x08000000 | 32 KB | 驗證、安裝與失敗復原（只能 SWD 更新） |
| Boot state A | 0x08008000 | 16 KB | 記錄安裝、試運行與確認狀態 |
| Boot state B | 0x0800C000 | 16 KB | 同上（雙 bank 輪替） |
| Settings（保留） | 0x08010000 | 64 KB | 預留 |
| App | 0x08020000 | 384 KB | 目前執行的韌體（MCUboot header 0x200，向量表 0x08020200） |

**F407 外部 NOR（8 MB）** — 取代 ET-100 NOR 分區：

| 名稱 | 位址 | 大小 | 說明 |
|---|---|---|---|
| Download journal | 0x000000 | 128 KB | 保存下載進度，支援斷點續傳 |
| Candidate | 0x020000 | 1 MB | 接收並驗證待更新韌體（實際上限 384 KB） |
| Rollback | 0x120000 | 1 MB | 更新失敗時還原 |
| Installer 結果（EIR1） | 0x220000 | 8 KB | 安裝結果與中斷計數 |
| App 已回報旗標 | 0x222000 | 8 KB | 記錄已回報的 `APP` 結果 |
| Logs／預留 | 0x224000 | 至 0x7FEFFF | 預留 |
| NOR 自測 | 0x7FF000 | 4 KB | 開發用 `nor test` |

- F407 沒有 Sensor OTA 分區；視覺化不顯示該列。
- 進度條容量：F407 的 App 與 Rollback 進度以**實際映像大小**（≤ 384 KB）計算，不使用 ET-100 的 1792 KB／2 MB 常數。
- 標題文字依裝置顯示（例如「F407 Flash 與 NOR OTA 視覺化」「外部 NOR · 8 MB」）。
- USB 模式沒有 INST 即時進度：視覺化只顯示「上傳 → 安裝中 → 結果」，不顯示備份／擦寫的百分比。

### 3.5 不做

- 不改 ET-100 firmware、協定文件、nRF gateway ZVERO 路徑。
- 不加 F407 sensor package。
- 不改 Dashboard／CAN 頁的 F407 顯示（另立工作）。

## 4. 修改範圍

| 檔案 | 動作 |
|---|---|
| `tools/rs232_tester.html` | §3 |
| `tools/tests/test_f407_ota.cjs` | 新增：設定表、F407 Flash／NOR 分區表與視覺化標題（不得出現 ET-100 位址／容量）、`validate()`（F407 正確／錯 project／錯 board／過大／SP 超界）、`parseInst()`（F407 前綴、CRC、錯前綴）、USB 模式重連（模擬 `getPorts()`／`connect`、identity 不符時拒絕）、180 s 期限 |
| `tools/tests/test_et100_smp.cjs`、`test_et100_ui.cjs` 等既有測試 | 只在介面改名時同步更新；**ET-100 既有案例必須全部照舊通過** |
| `fw/IMPLEMENTATION_PLAN.md`、`fw/docs/04_testing/28_tester_f407.md` | 本施工單、測試紀錄 |

## 5. 驗證

| # | 項目 | 方法 | 通過條件 |
|---|---|---|---|
| T1 | 既有測試 | `node tools/tests/*.cjs` | ET-100 相關全部 PASS（回歸） |
| T2 | 新測試 | `node tools/tests/test_f407_ota.cjs` | 全 PASS |
| T3 | F407 身分（實機，Stanley） | Chrome 連 F407 USB（`/dev/cu.usbmodem*`） | 頂欄顯示 `EUP_stm32f407_dk`／`0.2.x`；OTA 面板顯示 F407；記憶體圖為 §3.4 的 512 KB／8 MB 配置 |
| T4 | F407 USB OTA（實機，Stanley；需 eup_canbus_32f407 **階段 C CP2 完成**） | 載入 F407 `dfu_application.zip` → 開始 OTA | 上傳 → reset → 自動重連 → 30–40 s 內顯示成功；新版 confirmed |
| T5 | F407 rollback（實機，Stanley） | 載入 `APP_TRIAL_FAIL=1` 的 DFU | 約 120 s 後顯示「已還原」，舊版 confirmed |
| T6 | 錯誤檔案（實機或模擬） | F407 連線時載入 ET-100 DFU，或反之 | 載入時即拒絕，訊息列出雙方身分 |
| T7 | ET-100 回歸（實機，Stanley，有 EVB 時） | ET-100 DFU OTA | 行為與 v0.9.4 相同 |
| T8 | F407 RS232（實機，Stanley，需 USB-RS232 線） | 經 DB9 連 F407 | 與 ET-100 相同流程（含 INST 進度） |

## 6. 時程與相依

- 可與 eup_canbus_32f407 階段 C **平行**：T1–T3、T6 不需 C 完成；T4、T5、T8 需要 C CP2 完成的韌體。
- F407 端若在階段 C review 時修改了 INST 或 GOTASTAT 格式，需同步回本施工單。

## 7. 實作前確認（2026-10-27 Stanley 核准）

- 後續核准加入 F407 build 捷徑：選單提供 `build/Release/dfu_application.zip` 與 `build/Debug/dfu_application.zip`；以獨立 `eup-f407-root` 記住使用者授權的 F407 專案根目錄，保留本機選檔及 ET-100 原捷徑。測試資料夾隔離、權限與遺失檔案回復；範圍仍為本施工單 HTML／新測試／測試報告。

- F407 不沿用 ET-100 的容量／分區圖：依 F407 設計文件 §4 顯示內部 Flash 512 KB（Installer 32 KB、Boot state A／B 各 16 KB、Settings 64 KB、App 384 KB）及 W25Q64 NOR 8 MB（Journal 128 KB、Candidate／Rollback 各 1 MB、結果紀錄 8 KB、預留與末尾 4 KB 自測區）。ET-100 圖表保持原樣。此項僅修改本表已列的 HTML／新測試／測試紀錄。
- 實作與驗證紀錄集中於 `fw/docs/04_testing/28_tester_f407.md`；F407 階段 C CP2 及所有實機驗收仍由 Stanley 執行，不以 mock 結果代替。
