# 本專案 comm.md

本專案的 Claude Code / Codex / Antigravity 分工規則、git / release 流程、文件規則，
**複用上層專案已完成的 comm.md**：

👉 [`../comm.md`](../comm.md)

**原因**：
- ET-100 的硬體 review（`../docs/03_hardware/`）與韌體（`./`）共用同一套工作流程
- 單一 source of truth，避免兩份 comm.md 漂移
- 日常使用時，若快捷指令或規則參照「本專案」行為，一律回指 `../comm.md`

## 本專案特化說明

在 `../comm.md` 的框架下，本專案的具體對應：

| comm.md 中的用語 | 本專案的具體值 |
|---|---|
| `src/ include/` | `fw/src/` `fw/include/`（本資料夾）|
| `tools/` | `fw/tools/`（本資料夾）|
| `docs/` | `fw/docs/`（本資料夾）|
| `configs/` | `fw/configs/`（本資料夾） |
| `boards/` | `fw/boards/` — EVB / EVT / DVT / PVT / MP |
| 平台 plan MD | `fw/docs/00_project/00_et100_firmware_platform_plan.md`（待建） |
| Changelog | `fw/docs/00_project/ET100_FIRMWARE_CHANGELOG.md`（待建） |
| 預設 build 指令 `#b` | `cmake --build build`（P1 EVT target） |
| 燒錄 `#f` | `JLinkExe -CommanderScript tools/flash.jlink`（待建） |
| RTT debug `#rttb` | `cmake --build build_rtt_debug`（待建） |
| Branch status `git status` | `sh fw/tools/branch_status.sh` |

## Antigravity 新增角色

`../comm.md` 寫 Claude Code（架構師）+ Codex（執行工程師）兩角，本專案額外加入
**Antigravity**（長時間自主執行任務）：

| 工具 | 角色 | 典型工作 |
|---|---|---|
| Claude Code | 架構師 / Reviewer / MD 維護 | 讀 datasheet、寫 plan、review diff、更新文件 |
| Codex | 執行工程師 | 依 plan 改 code、修 review issue、commit |
| **Antigravity** | **自主實驗執行者** | bring-up 時 trial-and-error 類工作：例如「掃 I2C init 100 種時序組合找出可用的」、「跑一整晚 fuzz test」 |

Antigravity 的任務必須在 `IMPLEMENTATION_PLAN.md` 的 Scope 內明確列出，不可自主擴展。
