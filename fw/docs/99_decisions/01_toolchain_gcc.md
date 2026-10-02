# ADR 01 — Toolchain 選 GCC（而非 Keil MDK / IAR）

**狀態**：Accepted
**日期**：2026-10-02
**決策者**：Stanley
**對應討論**：Claude Code chat（階段 B SDK 調查後）

## Context

Freqchip FR3068E-C 的 SDK（`fr30xxc_sdk__202411`）原廠同時支援三套 toolchain：
- Keil MDK（ARMCC）
- IAR Workbench
- **ARM GNU GCC**（`arm-none-eabi-gcc`）

SDK 公開提供 GCC 專屬目錄：
- `components/tools/gcc/ldscript_3068e.ld`（178 行 GNU ld 標準）
- `components/drivers/device/fr30xx/gcc/{startup_fr30xx.S, syscalls.c, sysmem.c}`
- `examples/application/{btdm,ble_simple_periphreal}/GCC/Makefile`
- `components/btdm/libbtdm_host.a`（GNU ar 標準 archive，317 個 .o）

## Decision

採用 **ARM GNU Toolchain (arm-none-eabi-gcc 13+) + CMake + Ninja + VS Code**。

## Rationale

| 評估維度 | GCC | Keil MDK | IAR |
|---|:-:|:-:|:-:|
| 原廠 SDK 支援 | ✅ first-class | ✅ first-class | ✅ |
| License 成本 | **$0** | $$ 永久 ~$5k 或 ~$1500/yr | $$ 永久 ~$3k |
| CLI 自動化（Codex / Antigravity） | ✅ 完全 CLI | ❌ GUI-locked | ⚠️ CLI 需買額外 license |
| CI/CD（GitHub Actions） | ✅ apt install 即可 | ❌ 無 Linux 版 | ⚠️ 繁 |
| Cross-platform（macOS / Linux） | ✅ | ❌ Windows only | ⚠️ Linux only partial |
| Debugger（Cortex-Debug + J-Link / OpenOCD）| ✅ | ✅ µVision debugger | ✅ 內建 |
| 社群支援 | ✅ 大 | ✅ 大但 Keil-specific | ⚠️ 小 |
| MicroPython / Rust 等語言 port | ✅ | ❌ | ⚠️ |

**本專案 solo + AI toolchain 的加權**：
- CLI 自動化 = 必要（Codex 跑 build loop 不能靠 GUI）
- License 成本 = 必要（solo 開發者省錢路線）
- Cross-platform = 必要（Stanley 用 macOS，CI 可能是 Linux）

→ **GCC 是唯一同時滿足三項的選擇**。

## Consequences

### Positive
- 零 license fee；可在 macOS、Linux、Windows 開發
- Codex / Antigravity 可直接 `cmake --build build` + 讀 compiler error
- CI/CD 簡單（GitHub Actions runner 內建 apt）
- 第三方工具友善：OpenOCD、PyOCD、Zephyr Project 等皆可配合

### Negative
- SDK 內大多數 example 預設 Keil 專案（.uvprojx）；需看 GCC 版 Makefile 做範例
- 無原廠技術支援專線（Freqchip 支援可能主推 Keil）
- Keil 的 GUI debug 工具（Event Recorder、CMSIS-RTOS Viewer）不可用

### Neutral
- GCC vs Keil 的 .elf 效能差異通常 <5%，對 Cortex-M33 應用層不敏感
- 未來若要 port 到 Keil 只需加 Keil 專屬 scatter file + 改 startup

## Open Items

- [ ] 待首次 build 成功（Phase 1，等 EVB 到）驗證此決策
- [ ] 評估是否要加 Zephyr RTOS（本地 driver 可能需 port）
- [ ] 若 BLE 認證（BQB）要以 Eupfin 名義做，需確認 Freqchip binary stack（libbtdm_host.a）的 BQB 歸屬條款

## References

- SDK 內部 GCC 證據：[`../../vendor/README.md`](../../vendor/README.md)
- Chat 討論：Stage B「VS Code is possible to develop FR3068」及 SDK 翻查
