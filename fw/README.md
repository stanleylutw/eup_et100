# ET-100 Firmware (Eupfin custom)

Firmware for **EUP ET-100** vehicle tracker — Eupfin 自行開發，與 Quectel ODM 平行。
Target MCU: **Freqchip (富芮坤) FR3068E-C**（Cortex-M33 最高 156 MHz + 32-bit RISC @48 MHz BT core，Bluetooth 5.3 BR/EDR/BLE，2 組 CAN FD，2 MB Flash、512 KB SRAM，QFN80 9×9 mm）。原廠 v0.4.9 p.7 訂購表的 AEC-Q100 欄為「否」，不能沿用系列 Grade 2 宣稱。

規格依據：[FR306x 開發參考](../docs/03_hardware/fr306x_reference/README.md)。最新 Phase 1a-ext SDK example linker 為 1016 KiB Flash、512 KiB SRAM、128 KiB PRAM，核心目標 96 MHz；見 [PATCHES.md](vendor/PATCHES.md)。這是 patched SDK 原生 Makefile 的 host build baseline，不代表 repo CMake 已等價驗證或晶片完整 memory map 已確認。Silicon revision、upper SRAM bank／retention、boot 與實機功能仍待確認。

## 專案狀態

- **階段**：Project kick-off（2026-10）；SOW 尚未簽訂；EVT 板尚未 tape-out
- **硬體**：Quectel 負責；SCH V1.1 已完成內部 review（見[../docs/03_hardware/sch_analysis/](../docs/03_hardware/sch_analysis/)）
- **韌體**：Eupfin 自寫（本專案），solo + Claude Code / Codex / Antigravity

## Repo 結構

```text
fw/
├── vendor/              # 第三方 SDK（gitignored；用 tools/fetch_sdk.sh 下載）
│   └── fr30xxc_sdk__202411/     # Freqchip SDK，含 driver / BLE stack / GCC Makefile 範例
├── src/                 # 應用層程式碼
├── include/             # 應用層 header
├── boards/              # 板子定義（EVB、EVT、DVT、PVT、MP）
├── configs/             # build 組態（正式 / debug / rtt_debug）
├── cmake/               # CMake toolchain file + helpers
├── docs/
│   ├── 00_project/      # 平台計畫、FW changelog
│   ├── 01_firmware/     # FW 設計決策、HAL 架構
│   ├── 02_protocol/     # EUP 終端設備通訊協定 spec
│   ├── 04_testing/      # 測試報告、功耗量測
│   └── 99_decisions/    # 設計否決紀錄
├── tools/               # branch_status.sh、release.sh、check_plan_vs_code.py、fetch_sdk.sh
├── .githooks/           # pre-commit（擋 commit on main）
├── .vscode/             # launch.json (Cortex-Debug)、tasks.json、settings.json
├── CMakeLists.txt       # top-level build
├── IMPLEMENTATION_PLAN.md       # 本次任務施工單（comm.md Step 3 產出）
├── REVIEW_REPORT.md             # 本次 review 結果（comm.md Step 5 產出）
├── comm.md              # Claude Code / Codex 分工規則（workspace 共用）
└── README.md            # 本檔
```

## Toolchain 定案（2026-10-02）

- **Compiler**：ARM GNU Toolchain（`arm-none-eabi-gcc` 13+）
- **Build system**：CMake + Ninja（也相容 SDK 原生 Makefile）
- **IDE**：VS Code + Cortex-Debug 擴充
- **Debug probe**：J-Link（commercial 版）或 DAPLink（省錢路線 + OpenOCD）
- **理由**：見 [docs/99_decisions/01_toolchain_gcc.md](docs/99_decisions/01_toolchain_gcc.md)

**不選 Keil MDK 的理由**：
- Keil GUI 無法與 Codex / Antigravity CLI 自動化整合
- 商用授權 $$ 不必要（SDK 原廠已支援 GCC）
- Comm.md workflow 的 `#b` `#f` 快捷指令全部要走 CLI

## Quickstart

```bash
# 1. 安裝 toolchain
brew install --cask gcc-arm-embedded      # ARM GCC
brew install cmake ninja                   # build
# J-Link 從 https://www.segger.com/downloads/jlink/ 下載安裝

# 2. 下載 Freqchip SDK（第一次 clone 後必跑）
./tools/fetch_sdk.sh

# 3. 設定 git hook（擋 commit on main）
git config core.hooksPath .githooks

# 4. 建置
mkdir build && cd build
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake ..
ninja
# 產出：build/app.elf、build/app.hex、build/app.bin
```

## 開發流程

遵循 [`comm.md`](comm.md)：
- Claude Code（架構師）寫 `IMPLEMENTATION_PLAN.md`、review diff、更新文件
- Codex（執行工程師）依 plan 改 code、修 Critical/Major issue、commit
- Antigravity（長任務自主執行）跑 bring-up trial-and-error（例如掃 I2C init 時序）
- 任何修改前先跑 `git status --short --branch`，不在 main 上改

## 相關文件

- 韌體架構草案：[02_firmware_architecture_draft.md](docs/01_firmware/02_firmware_architecture_draft.md)
- 全硬體覆蓋／驗證與文件衝突：[03_hardware_coverage_matrix.md](docs/01_firmware/03_hardware_coverage_matrix.md)
- 開發里程碑與下一個施工範圍：[04_development_execution_draft.md](docs/01_firmware/04_development_execution_draft.md)

- 硬體 review：[../docs/03_hardware/sch_analysis/00_索引.md](../docs/03_hardware/sch_analysis/00_索引.md)
- 寄給 Quectel 的技術問題清單：[../docs/03_hardware/sch_analysis/100_questions_for_quectel.pdf](../docs/03_hardware/sch_analysis/100_questions_for_quectel.pdf)
- 產品提案書 V1.8：[../docs/00_project/](../docs/00_project/)

## License / IP

- 本專案程式碼：Eupfin Technology Co., Ltd. All rights reserved
- Freqchip SDK：原廠授權，詳見 vendor/fr30xxc_sdk__202411/ 內的授權文件
- 第三方 open source（lwIP、FreeRTOS、mbedTLS、lvgl 等）：各自 license
