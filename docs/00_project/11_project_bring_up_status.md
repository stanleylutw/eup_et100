# ET-100 Project Bring-up — 已完成項目總覽

**起算日期**：2026-10-01
**最後更新**：2026-10-21
**維護者**：Stanley（Eupfin） + Claude Code + Codex

> 本檔是跨 HW + FW 的專案層級狀態儀表板。詳細技術文件連結分散在 [`docs/03_hardware/sch_analysis/`](../03_hardware/sch_analysis/) 與 [`fw/`](../../fw/)。

> **2026-10-02 規格同步**：FR3068E-C 原廠 v0.4.9 為 2 MB Flash、512 KB SRAM、QFN80、2 組 CAN、AEC-Q100「否」；BT core 是 32-bit RISC @48 MHz。現有 SDK linker 配置另計，host build 不代表已在硬體上運行。來源：[FR306x 開發參考](../03_hardware/fr306x_reference/README.md)。

---

## 📘 Category 1 — 專案背景理解

| 項目 | 產出 | 狀態 |
|---|---|:-:|
| 讀完 Quectel V1.8 提案書 57 頁 | 10 份章節摘要 [docs/00_project/](.) | ✅ |
| 識別風險 → 寫 pre-SOW 清單 | [10_風險與談判清單.md](10_風險與談判清單.md) 9 項 | ✅ |
| 建立 workspace comm.md 工作流程 | [`../../comm.md`](../../comm.md) + [`../../fw/comm.md`](../../fw/comm.md) | ✅ |

---

## 🔌 Category 2 — Schematic Review（硬體）

| 項目 | 產出 | 狀態 |
|---|---|:-:|
| 15 頁 SCH 全頁掃描分類 | [sch_analysis/00_索引.md](../03_hardware/sch_analysis/00_索引.md) | ✅ |
| 單頁 MD 模板 | [sch_analysis/_TEMPLATE.md](../03_hardware/sch_analysis/_TEMPLATE.md) | ✅ |
| 13 份 sheet MD（Stage A） | `01_MCU` ~ `14_RF_NFC` | ✅ |
| 6 份關鍵頁升級 Stage B（datasheet + 計算） | Sheet 1, 2, 6, 7, 10, 12 | ✅ |
| 全系統電源樹（15 條 rail） | [sch_analysis/90_power_tree.md](../03_hardware/sch_analysis/90_power_tree.md) | ✅ |
| 跨 sheet net 對照表 | [sch_analysis/91_net_inventory.md](../03_hardware/sch_analysis/91_net_inventory.md) | ✅ |
| 對外連接器 pinmap | [sch_analysis/92_connector_pinmap.md](../03_hardware/sch_analysis/92_connector_pinmap.md) | ✅ |
| 提案書 9 項風險 + SCH 新 17 項對照 | [sch_analysis/99_risks_vs_proposal_v1.8.md](../03_hardware/sch_analysis/99_risks_vs_proposal_v1.8.md) | ✅ |
| **總計**：20 份 MD，~3700 行 | | ✅ |

---

## 📤 Category 3 — 寄給 Quectel 的技術問題清單

| 項目 | 產出 | 狀態 |
|---|---|:-:|
| V1.0 → V1.1 → **V1.2** 迭代 | 48 題（P0×10 + **P0-FW×7** + P1×12 + P2×11 + P3×8） | ✅ |
| Markdown V1.4；HTML／PDF 尚未同步 | [100_questions_for_quectel.md](../03_hardware/sch_analysis/100_questions_for_quectel.md) | 重產 HTML／PDF 後才能寄新版 |
| **尚未寄出** — 等 Stanley 動作 | | ⬜ |

---

## 🔧 Category 4 — Vendor Identity 修正

| 項目 | 狀態 |
|---|:-:|
| 全專案 `Fibocom` → `Freqchip (富芮坤)` 修正 | ✅ |
| 確認 FR3068E-C 來自 Freqchip（非 Fibocom LTE module vendor） | ✅ |
| 文件化在 100_questions V1.1 Revision History | ✅ |

---

## 💻 Category 5 — Firmware Toolchain 決策與驗證

| 項目 | 產出 | 狀態 |
|---|---|:-:|
| Toolchain ADR | [ADR 01 — GCC vs Keil vs IAR](../../fw/docs/99_decisions/01_toolchain_gcc.md) | ✅ |
| GCC 原生支援驗證（翻 SDK 源碼） | [Vendor README](../../fw/vendor/README.md) 詳列證據 | ✅ |
| **Phase 1a build validation** | 真正 build 跑過 | ✅ |
| Toolchain 裝好：ARM GCC 14.3.1 + CMake 4.4.3 + Ninja 1.13.2 | 在 Stanley Mac 上 | ✅ |
| PATH 設定 | `~/.zshrc` 已加 | ✅ |

---

## 🏗️ Category 6 — FW Project Skeleton

**[fw/](../../fw/) 底下 19+ 檔案**：

| 目錄 | 內容 | 狀態 |
|---|---|:-:|
| [fw/README.md](../../fw/README.md) | 專案說明 + Quickstart | ✅ |
| [fw/CMakeLists.txt](../../fw/CMakeLists.txt) | top-level build (SDK driver 骨架) | ✅ |
| [fw/cmake/arm-none-eabi.cmake](../../fw/cmake/arm-none-eabi.cmake) | GCC toolchain file | ✅ |
| [fw/.vscode/](../../fw/.vscode/) | settings/launch/tasks (Cortex-Debug + JLink/OpenOCD) | ✅ |
| [fw/.githooks/pre-commit](../../fw/.githooks/pre-commit) | 擋 main commit | ✅ |
| [fw/tools/](../../fw/tools/) | fetch_sdk.sh / branch_status.sh / release.sh / check_plan_vs_code.py | ✅ |
| [fw/src/main.c](../../fw/src/main.c) | placeholder（等 ET-100 application implementation） | ✅ |
| [fw/.gitignore](../../fw/.gitignore) | 排除 vendor/、build/、IDE 檔 | ✅ |
| [fw/comm.md](../../fw/comm.md) | 指回 `../comm.md`（Antigravity 新角色定義） | ✅ |
| [fw/docs/00_project/00_et100_firmware_platform_plan.md](../../fw/docs/00_project/00_et100_firmware_platform_plan.md) | 平台計畫 v0.1 | ✅ |
| [fw/IMPLEMENTATION_PLAN.md](../../fw/IMPLEMENTATION_PLAN.md) + [REVIEW_REPORT.md](../../fw/REVIEW_REPORT.md) | 跑過 2 輪 comm.md Step 1-9 流程 | ✅ |

---

## 📦 Category 7 — Freqchip SDK 落地

| 項目 | 狀態 |
|---|:-:|
| SDK 下載（`fr30xxc_sdk__202411`，165MB，3792 檔案） | ✅ 在 [fw/vendor/](../../fw/vendor/)（gitignored） |
| **8 組 active 本地 patch**（#1-#8；#7a／#7b 分開登錄，共 9 個 active entry） | #5 的錯誤 FR303x selector 已撤回為 vendor default FR509x；#8 為 EVB debug 停用 sleep。暫時診斷 Patch #9 已測試後撤回，不計入 active 數。完整限制見 PATCHES.md |
| Patch 政策 ADR | 已記錄於 [ADR 02 — SDK Local Patch Policy](../../fw/docs/99_decisions/02_sdk_local_patch_policy.md) |
| Patch 完整文件 | 已記錄於 [vendor/PATCHES.md](../../fw/vendor/PATCHES.md) |

---

## ⚙️ Category 8 — Build 產物（Reference BLE Peripheral Example）

**OTA 架構決策（文件，非 build）**：Stanley 已採 2 MB 單 App＋外部 NOR Candidate／Rollback／Sensor slot，不採晶片內 A/B。header 8 KB＋installer 120 KB、App 1792 KB、Settings 112 KB、內部 boot state 16 KB 為候選；內部 state 唯一決定安裝／trial／rollback，NOR journal 只管下載。Sensor slot 256 KB 但 package ≤225280 B（含 256 B header），RS232 更新 115200／8N1。詳見 [OTA 儲存規格](../../fw/docs/01_firmware/06_ota_storage_design.md)。[08 EP-C EVB 內建 Flash 實測報告](../../fw/docs/04_testing/08_evb_internal_flash_size.md)、[唯讀 probe](../../fw/tools/evb/flash_size_probe.py)／[sector 工具](../../fw/tools/evb/flash_sector_test.py)及[工具 README](../../fw/tools/evb/README.md)已依指示恢復，未執行或重跑。Q43-5 只確認 ROM／installer 邊界；NOR → 內部 copy 由自有 installer 實作，Q48-6／FlashEnv 遷移／VTOR／掉電恢復仍為 gate。本輪限 docs/tools，未改 SDK／source／linker／FAL，不 build／flash，板上仍 0.7.0-dev。

**Stanley App 確認**：0.7.0-dev／EUP_AIoT_HUB_FR 燒錄後回覆「yes, it works.」，記為 App 基本連線與 manager 路徑可用；沒有逐筆 log，不擴大為全部步驟／雙角色並行／長時間穩定性通過。詳見 App GATT 紀錄的驗收補充。

**v0.8.1 U11 NOR（EVB 實測，2026-10-22）**：G1 與 T2–T7 PASS。U11 = GD25LQ128（`C8 60 18`，16 MB），經 GPIO function 0xC 接精簡版 QSPI1（只用 STIG）；SFDP 的 8 dummy clock 為 `dummy_raw=1`。erase avg 4／32／64 KB = 13.1／58.1／99.7 ms；0x020000–0x41FFFF 三輪 SHA-256 吻合；MCU reset 中斷後可正常再擦寫；timeout 後 controller 可復原。速度為 STIG 8-byte PP（約 55 KB/s），不代表 OTA 效能。G2／T8（斷電）待量 C11／C30；4-byte 定址等 ET-100 實板。測完已恢復 0.7.0-dev。SDK patch 新增 0。參見 [09 NOR 報告](../../fw/docs/04_testing/09_evb_nor_bringup.md)。

**v0.8.2 download journal（2026-10-22）**：NOR journal（128 KB、完整 snapshot、commit marker、GC 先寫新再擦舊、session_id／resume takeover）完成。Host 斷電注入 42,388 次 PASS；EVB U11 2 MB 下載＋3 次 MCU reset 續傳 SHA-256 吻合；已恢復 0.7.0-dev。G2 取消、T8 實機斷電延到 ET-100。BLE 更新決定採 SMP＋MCUboot image 格式（06 §6.1）。下一步：v0.8.3 installer＋RS232 端到端 OTA 原型。

**v0.8.3 installer（2026-10-22）**：EVB 已是新布局——ROM → installer（0x08002000）→ App（0x08020200，0.8.4-dev）。實機證實 ROM 只驗 A 資訊段涵蓋的 installer、不驗 App；B 資訊段（全 00）全程未寫。Installer 從 NOR Candidate 安裝新版（erase／program／驗證各 reset 一次都能重做完成），拒絕壞 SHA、真實 nRF image 與 CRC=0 的 FR image。Build 自動產生與 NCS 同格式的 `dfu_application.zip`（EUPFWV1 CRC 已填）。**僅 SHA-256、不驗簽章，原型專用**；trial／confirm／rollback 在 v0.8.4。

**最新 App GATT build（0.7.0-dev）**：新增手機可連的 Gateway manager status／list／select／selected-data，名稱依 Stanley 選擇為 **EUP_AIoT_HUB_FR**，避免舊 App 將 FR 誤判為 AES sensor；不改 App／SDK。七組 host tests、ARM build 與 header／CRC／FlashEnv 通過。Burn 284,712 bytes，ELF text/data/bss=240,744/35,768/66,528 bytes，data+bss=102,296 bytes，取代下方歷史數據；runtime heap 餘量未量測。實機 App／sensor 並行仍待驗收，最新 flash 狀態見 [App GATT 紀錄](../../fw/docs/04_testing/07_evb_app_gatt_server.md)。SDK active patches 仍 8 組，新增 0。

**最新 D4 LED build**：0.6.0-dev 新增 PA7／D4 Gateway LED、PA6／D3 固定低電位熄滅，待機／資料／sensor GATT link 使用不同亮滅規則，不改 K1-K3。Burn 283,400 bytes，ELF text/data/bss=239,912/35,288/66,304 bytes，取代下方早期 build 的同名數字；SDK active patches 仍 8 組，新增 0。LED task 另需 1.5 KiB stack 與 TCB，runtime heap 餘量未量測。實體驗收及最新燒錄結果見 [LED 紀錄](../../fw/docs/04_testing/06_evb_gateway_led.md)。

**0.6.0-dev HTML 修正版**：STYPESWITCH 接受 `0;` 取消空欄位與 `1;<MAC>` 連線格式，回覆正規化；修正前的 HTML cancel 被 valid=0 拒絕。修正版 burn=283,120 bytes，ELF text/data/bss=239,632/35,288/66,304 bytes；數據取代下段首版 build 的對應數字，data+bss 不變。燒錄／測試最新狀態見 GATT 紀錄；未修改 SDK。

**最新 EVB 更新（0.6.0-dev）**：2026-10-21 已燒入 Gateway-style GATT client，UART3 仍 9600。支援 manual／auto、AES 認證、metadata／command／config、Type 8；host／build／program-readback PASS，真實 sensor 驗收 pending。Burn 283,056 bytes，ELF text/data/bss=239,568/35,288/66,304 bytes；data+bss 101,592 bytes 不等於 heap 可用量。新增 SDK patch 0，active patches 仍 8 組。詳見 [GATT 紀錄](../../fw/docs/04_testing/05_evb_gatt_client.md)。下段 0.5.0-dev 為已完成 Stanley 硬體驗收的歷史版本，不再是目前板上版本。

**前版 EVB 驗收紀錄**：2026-10-21 獨立 `fw/build_serial/` UART3 9600 target（`0.5.0-dev`），新增 GCID／SCID 保存、BLE scan、Type 1 GTIME/RSSI/XOR 回報；program/readback PASS，Stanley 實測 4 MAC／55 筆 Type 1、XOR 全通過，並確認 CID 切換與 CID／GTIME 手動 RST 後保存正常，見 [CID／scan 紀錄](../../fw/docs/04_testing/04_evb_cid_ble_scan.md)。前版 GTIME 驗證見 [GTIME 紀錄](../../fw/docs/04_testing/03_evb_gtime_settings.md)；GCHK 歷史見 [serial 紀錄](../../fw/docs/04_testing/02_evb_serial_gchk.md)。斷電／GC／長時間測試仍未驗收。此 target 無新增 SDK patch；下表仍保留原 reference BLE baseline metrics。

**最新結果：Phase 1b，2026-10-21**。GCC 14.3.1 build 的 reference BLE example 已透過板載 DAPLink + SDK `FR30xx.FLM` 寫入 FR3068EP-C DEV1.4。整份 `Project_burn.bin` 從 `0x08000000` programming，66 sectors，readback 逐 byte PASS；reset 後 app 啟動，nRF Connect 可連線並看到預期 GATT。已確認 post-process 必須使用 vendor default **FR509x header layout**；先前推論的 FR303x layout 會讓 options 欄位錯位並造成 boot/SWD 卡死。

| 檔案 | 大小 | 用途 | 狀態 |
|---|:-:|---|:-:|
| `Project.elf` | 1,339,200 bytes | GDB symbols | Phase 1b local build |
| `Project.bin` | 260,368 bytes | raw image | 已產出；未做 direct-flash boot test |
| `Project.hex` | 732,427 bytes | Intel HEX | 已產出；未作為本次 programming input |
| **`Project_burn.bin`** | **268,560 bytes** | FR509x layout header + CRC + payload | **EVB @ `0x08000000` programming／readback／boot PASS** |
| Flash 使用 | 25.0% | 254.266 KiB / 1016 KiB linker window，不是晶片 2 MB 總容量 | 已量測產物 |
| SRAM 使用 | **18.5%**（原 37.1%） | 97,232 bytes = 94.953 KiB / **512 KiB** linker window | 靜態配置不變；不是 runtime heap 餘量 |

Size：text **225,088**、data **35,272**、bss **61,976** bytes。Burn header word 7 = `0x51525251`、word 8 = `0x00000000`（TLV length）、word 9 = options、word 11 = `0x51525251`。MCU_DIV=2 的 96 MHz configuration 已完成開機與 BLE 基本連線，但實際 clock、功耗、長時間穩定性及 BLE／CAN／LTE 並行性能未驗證。Patch #8 讓 EVB development build 不進 sleep，故本次結果不代表產品低功耗。

---

## 🤝 Category 9 — comm.md Workflow 實戰驗證

- 第一次完整跑通 Claude Code（架構師）+ Codex（執行工程師）**雙 AI reviewer** 機制
- Codex 抓到 Claude Code 3 項 Major（Patch #4 不完整、scope creep、overclaim）
- 全部承認、全部修正、全部文件化
- comm.md 工作流程**實證可用**

---

## 🧠 Category 10 — 技術決策（ADR）

| ADR | 內容 | 狀態 |
|---|---|:-:|
| [ADR 01](../../fw/docs/99_decisions/01_toolchain_gcc.md) | Toolchain = ARM GCC + CMake + VS Code | ✅ Accepted + **Validated** |
| [ADR 02](../../fw/docs/99_decisions/02_sdk_local_patch_policy.md) | SDK local patch 政策（5 條規則） | ✅ Accepted |

---

## 📱 Category 11 — Reference BLE Build 的功能內容與資源分佈

**目前已 host build 並在 EVB 啟動的 example**：`ble_simple_periphreal`。Phase 1b 已驗證 boot、BLE 連線與基本 GATT discovery；UART command、BLE OTA、長時間運行與 ET-100 硬體仍未驗證。`fw/src/main.c` 仍為 placeholder。

### A. 開機序列（Boot Sequence）

| # | 步驟 | 細節 |
|:-:|---|---|
| 1 | Clock init | 初始化 clock／XIP；實際 core frequency 待核對 PLL／divider 並量測，不能把晶片最高 156 MHz 當成範例實際主頻 |
| 2 | GPIO init | PB4 / PB5 → UART3 pin mux |
| 3 | UART3 init | 921600 bps（高速 debug / AT console） |
| 4 | PMU + 校準 | cali_init, pmu_init |
| 5 | FreeRTOS 啟動 | kernel scheduler start |
| 6 | BLE Controller + Host 起 | libbtdm_host.a 展開 |
| 7 | BLE 開始廣播 | 裝置名稱 = `30xx_Ble_Periphreal` |
| 8 | AT command parser | 開始聽 UART3 |
| 9 | Monitor task | `vTaskDelay(2000000)`；目前 FreeRTOS tick 為 1000 Hz，名義延遲 2000 秒，不是 2 秒 |

### B. 連線後能做什麼（BLE Services）

| 項目 | UUID / Attribute | 內容 |
|---|---|---|
| 裝置名稱 | adv_data | `30xx_Ble_Periphreal`（nRF Connect 已實機連線） |
| GATT Service | **0xFFF0**（Simple Profile） | 2 個 characteristic |
| CHAR1 | **0xFFF3** | 20 bytes，可讀 / 可寫 / 可 notify |
| CHAR2 | 0xBA5C-FFF4-04A3-4071-A0B5-3585-3EB0-8307（128-bit） | 5 bytes，預設值 `11 22 33 44 55` |
| OTA Service | — | source 包含 BLE OTA service；完整更新／boot／rollback 待驗證 |

### C. UART3 @ 921600 AT Command Set

| Command | 功能 |
|---|---|
| `AT#AG` | print "hello world!" |
| `AT#AH20000000` | 讀 memory address |
| `AT#AI20000000 DEADBEEF` | 寫 memory address；僅為 parser 格式示例，不應隨意寫入記憶體 |
| `AT#AJ00` | 讀 OOL (on-chip debug) register |
| `AT#AK00 FF` | 寫 OOL register；地址／值需先確認 |
| `AT#AU20000000 10` | dump memory；count 以 hex 解析，`10` 為 16 words；實機未驗 |
| 更多 | BT vendor command、BLE scan、L2CAP、SMP 相關 |

以上依 `Src/app_at.c` parser：prefix 為 `AT#`，兩字 command 後立即接第一個參數，以 newline 結束。不是通用 `AT+` 語法；尚未完成 runtime 操作驗證。

### D. Flash 實測與模組清單

| 類別 | 大約大小 | 說明 |
|---|:-:|---|
| BLE/BT stack（binary） | 尚未按 map 歸屬統計 | `libbtdm_host.a` 可 link；archive 的 profile／object 清單不等於全部進入最終 ELF |
| FreeRTOS kernel | 尚未按 map 歸屬統計 | tasks／queues／timers、heap、CM33 port 等 |
| FlashDB／FAL；FatFS 編譯輸入 | 尚未按 map 歸屬統計 | 不代表 FatFS API 已保留於最終 ELF 或檔案系統已啟用 |
| Drivers／libc／app／startup／共用工具 | 尚未按 map 歸屬統計 | 舊 130／15／25 KB 等分類估算撤回，避免當成量測數據 |
| **Project.bin 實測** | **260,368 bytes = 254.266 KiB** | **25.0% / 1016 KiB linker window；名義差額不等於已規劃的 OTA／log 空間** |

### E. SRAM 97 KB 分配

| 類別 | 大小 | 說明 |
|---|:-:|---|
| GNU size 的 data | 35,272 bytes | 含 `.data`／`.dram_section` 等；不是純 app 變數 |
| GNU size 的 bss | 61,976 bytes | 包含靜態配置的 RTOS heap 等；不能只解讀為 runtime 已用量 |
| **靜態合計** | **97,232 bytes = 94.953 KiB** | **18.5% / 512 KiB linker SRAM window** |
| **名義差額** | **417.047 KiB** | 不是自動可 malloc 的容量；FreeRTOS heap 目前配置 50 KiB，且已含於上述配置，runtime 餘量需量測 |

**附加**：`.ram_code` **14.4 KB** 從 flash copy 到 **PRAM**（高速 RAM 0x1FFE0000）執行 — 含 low-power 進出、XIP flash 操作等對 timing 敏感的 code。

### F. 對 ET-100 專案的意義

| 項目 | 評估 |
|---|---|
| 架構複用程度 | 可作參考模板；未量測複用比例，撤回「95%」估計 |
| 直接留用 | FreeRTOS、BLE stack、OTA service、UART AT console — 全部 ET-100 需要 |
| 需替換 | app_ble.c 的 service UUID、app_at.c 的 AT command set |
| 需新增 | LTE / GNSS / NFC / CAN 周邊 driver |
| 資源規劃 | 依確認後的 memory map／boot 分區與外部 32 MB Flash 分別規劃；尚未證明 dual-bank OTA 或 30 天 log 已有足夠空間 |

---

## 📊 綜合進度儀表板

```text
HW Review          ████████████████████ 100% (Stage A + 6 關鍵頁 Stage B)
Vendor Comms       ███████████████░░░░░  75% (PDF 準備好，未寄)
Toolchain          ████████████████████ 100% (GCC 14.3.1 Mac 上 build pass)
FW Skeleton        ████████████████████ 100% (comm.md tooling 齊備)
SDK Integration    ████████████████████ 100% (165MB + 8 patch groups documented)
Build Validation   ████████████████████ 100% (Project_burn.bin EVB boot + BLE PASS)
Hardware (EVB)     ████████████████████ 100% (SWD/flash/boot/921600 UART/BLE GATT)
Hardware (EVT)     ░░░░░░░░░░░░░░░░░░░░   0% (SOW 前)
Driver Bring-up    ██░░░░░░░░░░░░░░░░░░░  10% (EVB debug/flash/BLE baseline ready)
App Logic          ░░░░░░░░░░░░░░░░░░░░   0% (等 EUP 協定)
```

---

## 🎯 當前阻擋項（三條線並行）

| 阻擋項 | 時間成本 | 依賴 |
|---|:-:|---|
| **重產 V1.4 HTML／PDF 後寄 Quectel** | 待排程 | Markdown 已同步；派生檔仍為舊版 |
| **EVB UART + SWD 並行** | 完成 | 四個 jumper 已接；921600 log + SWD reset 同時 PASS |
| **DEV1.4 K1-K3／D3-D4 mapping** | 待原理圖、continuity 或獲准設定 PMU input/pull 後重測 | 被動 PMU scan 僅得到 bit 0／2／3 候選且訊號不穩；Patch #9 已撤回 |
| **EUP 終端設備通訊協定考古** | 30 分鐘找人 + 半天整理 | Stanley（內部 PM / wiki） |

EVB 基本主幹已跑通；後續 GPIO/CAN/I2C bring-up 仍需各自的 `IMPLEMENTATION_PLAN.md` 與 Stanley 批准。

---

## 📁 連結快查

- 硬體 review 索引：[`docs/03_hardware/sch_analysis/00_索引.md`](../03_hardware/sch_analysis/00_索引.md)
- Quectel 問題清單 V1.4：[`100_questions_for_quectel.md`](../03_hardware/sch_analysis/100_questions_for_quectel.md)（HTML／PDF 尚未同步）
- FW 平台計畫：[`fw/docs/00_project/00_et100_firmware_platform_plan.md`](../../fw/docs/00_project/00_et100_firmware_platform_plan.md)
- FW README + Quickstart：[`fw/README.md`](../../fw/README.md)
- SDK Patches：[`fw/vendor/PATCHES.md`](../../fw/vendor/PATCHES.md)
- EVB Bring-up：[`FR3068EP-C Dev1.4/README.md`](../03_hardware/FR3068EP-C%20Dev1.4/README.md)
- ADR 清單：[`fw/docs/99_decisions/`](../../fw/docs/99_decisions/)
- Workspace workflow：[`comm.md`](../../comm.md)
