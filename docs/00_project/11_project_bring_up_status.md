# ET-100 Project Bring-up — 已完成項目總覽

**起算日期**：2026-10-01
**最後更新**：2026-10-02
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
| Markdown V1.3；HTML／PDF 尚未同步 | [100_questions_for_quectel.md](../03_hardware/sch_analysis/100_questions_for_quectel.md) | 重產 HTML／PDF 後才能寄新版 |
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
| [fw/src/main.c](../../fw/src/main.c) | placeholder（等 EVB） | ✅ |
| [fw/.gitignore](../../fw/.gitignore) | 排除 vendor/、build/、IDE 檔 | ✅ |
| [fw/comm.md](../../fw/comm.md) | 指回 `../comm.md`（Antigravity 新角色定義） | ✅ |
| [fw/docs/00_project/00_et100_firmware_platform_plan.md](../../fw/docs/00_project/00_et100_firmware_platform_plan.md) | 平台計畫 v0.1 | ✅ |
| [fw/IMPLEMENTATION_PLAN.md](../../fw/IMPLEMENTATION_PLAN.md) + [REVIEW_REPORT.md](../../fw/REVIEW_REPORT.md) | 跑過 2 輪 comm.md Step 1-9 流程 | ✅ |

---

## 📦 Category 7 — Freqchip SDK 落地

| 項目 | 狀態 |
|---|:-:|
| SDK 下載（`fr30xxc_sdk__202411`，165MB，3792 檔案） | ✅ 在 [fw/vendor/](../../fw/vendor/)（gitignored） |
| **7 組本地 patch 完成**（#1-#6 + #7；#7a／#7b 分開登錄，共 8 個 entry） | Phase 1a-ext clean build PASS；#6 SRAM 512 KiB、#7a 96 MHz 目標、#7b runtime 156 MHz guard；實機待驗 |
| Patch 政策 ADR | 已記錄於 [ADR 02 — SDK Local Patch Policy](../../fw/docs/99_decisions/02_sdk_local_patch_policy.md) |
| Patch 完整文件 | 已記錄於 [vendor/PATCHES.md](../../fw/vendor/PATCHES.md) |

---

## ⚙️ Category 8 — Build 產物（Reference BLE Peripheral Example）

**最新結果：Phase 1a-ext，2026-10-02**。GCC 14.3.1／SDK 原生 Makefile 完整重編譯 exit 0；完整 log：`/tmp/et100_build_v2.log`。SPLL 保留 192 MHz，MCU_DIV=2 的核心目標為 96 MHz，未實機量測。SRAM linker 改為 512 KiB，PRAM／Flash window 不變；沒有啟用額外 heap 或驗證 upper-bank。下方 Category 11 保留 Phase 1a 分析，舊容量／產物數字不是本次最新結果。

| 檔案 | 大小 | 用途 | 狀態 |
|---|:-:|---|:-:|
| `Project.elf` | 1.3 MB | GDB symbols | ✅ |
| `Project.bin` | 260,376 bytes | raw image；燒錄／boot 流程待原廠確認 | 本次已產出，實機未驗 |
| `Project.hex` | 732,469 bytes | Intel HEX | 本次已產出 |
| **`Project_burn.bin`** | **268,568 bytes** | post-process 封裝候選映像（header + CRC），bootloader 相容性待驗 | payload／CRC host 驗證通過，實機未驗 |
| Flash 使用 | 25.0% | 254.273 KiB / 1016 KiB linker window，不是晶片 2 MB 總容量 | 已量測產物 |
| SRAM 使用 | **18.5%**（原 37.1%） | 97,232 bytes = 94.953 KiB / **512 KiB** linker window | 靜態配置不變；不是 runtime heap 餘量 |

Size：text **225,096**、data **35,272**、bss **61,976** bytes；map SRAM `0x20000000`／length `0x80000`，上限 `0x20080000`（exclusive）。Guard 9 組 host 路徑測試與 ARM disassembly 檢查通過；只有既有 RWX warning。Burn header magic `0x51525251`、CRC32 `0x5b1a2ae6`、code_length `260376`、exec_offset `0x2000` 通過 host 一致性檢查。實際功耗下降比例及 BLE／CAN／LTE 並行性能未驗證。

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

**目前已 host build 的 example**：`ble_simple_periphreal`（SDK 範例，Patch 後 Mac GCC build pass）；尚未在 EVB 或 ET-100 上運行。以下功能為 source 描述，不是實機驗收結果；`fw/src/main.c` 仍為 placeholder。

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
| 裝置名稱 | adv_data | `30xx_Ble_Periphreal`（source 配置；手機掃描待實機驗證） |
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
| **Project.bin 實測** | **260,352 bytes = 254.25 KiB** | **25.0% / 1016 KiB linker window；名義差額 761.75 KiB，不等於已規劃的 OTA／log 空間** |

### E. SRAM 97 KB 分配

| 類別 | 大小 | 說明 |
|---|:-:|---|
| GNU size 的 data | 35,272 bytes | 含 `.data`／`.dram_section` 等；不是純 app 變數 |
| GNU size 的 bss | 61,976 bytes | 包含靜態配置的 RTOS heap 等；不能只解讀為 runtime 已用量 |
| **靜態合計** | **97,232 bytes = 94.953 KiB** | **37.1% / 256 KiB linker SRAM window** |
| **名義差額** | **161.047 KiB** | 不是自動可 malloc 的容量；FreeRTOS heap 目前配置 50 KiB，且已含於上述配置，runtime 餘量需量測 |

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
SDK Integration    ████████████████████ 100% (165MB + 5 patches)
Build Validation   ████████████████████ 100% (Project_burn.bin 產出)
Hardware (EVB)     ░░░░░░░░░░░░░░░░░░░░   0% (未購)
Hardware (EVT)     ░░░░░░░░░░░░░░░░░░░░   0% (SOW 前)
Driver Bring-up    ░░░░░░░░░░░░░░░░░░░░   0% (等 EVB)
App Logic          ░░░░░░░░░░░░░░░░░░░░   0% (等 EUP 協定)
```

---

## 🎯 當前阻擋項（三條線並行）

| 阻擋項 | 時間成本 | 依賴 |
|---|:-:|---|
| **重產 V1.3 HTML／PDF 後寄 Quectel** | 待排程 | Markdown 已同步；派生檔仍為舊版 |
| **買 Freqchip FR3068E-C EVB** | 10 分鐘下單 + 3-7 天到貨 | Stanley（email Freqchip sales 或淘寶/Quectel 要） |
| **EUP 終端設備通訊協定考古** | 30 分鐘找人 + 半天整理 | Stanley（內部 PM / wiki） |

**三件事全部不依賴我，但 FW 專案任何進一步工作都需要上面至少 1-2 件完成**。

---

## 📁 連結快查

- 硬體 review 索引：[`docs/03_hardware/sch_analysis/00_索引.md`](../03_hardware/sch_analysis/00_索引.md)
- Quectel 問題清單 V1.3：[`100_questions_for_quectel.md`](../03_hardware/sch_analysis/100_questions_for_quectel.md)（HTML／PDF 尚未同步）
- FW 平台計畫：[`fw/docs/00_project/00_et100_firmware_platform_plan.md`](../../fw/docs/00_project/00_et100_firmware_platform_plan.md)
- FW README + Quickstart：[`fw/README.md`](../../fw/README.md)
- SDK Patches：[`fw/vendor/PATCHES.md`](../../fw/vendor/PATCHES.md)
- ADR 清單：[`fw/docs/99_decisions/`](../../fw/docs/99_decisions/)
- Workspace workflow：[`comm.md`](../../comm.md)
