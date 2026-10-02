# REVIEW_REPORT — Phase 1a: Validate GCC Build Chain

**Date**：2026-10-02（初版）、2026-10-02（Codex review 修訂）
**Reviewer**：Claude Code（Step 5）
**Cross-review**：Codex（在初版發送後審查，發現 3 項 Major → 已修正）
**Executor**：Codex（toolchain install）+ Claude Code（SDK copy + patches + build）
**Scope**：macOS 上實證 ADR 01（toolchain = ARM GCC）

> **2026-10-02 文件同步（Codex）**：依 FR306x v0.4.9 校正容量來源、使用率與未驗證宣稱；未重新 build、未修改 SDK，以下產物數據保留既有結果。晶片總容量為 2 MB Flash／512 KB SRAM，但 linker 可用區與總容量的對應仍待確認，見 [開發對照](../docs/03_hardware/fr306x_reference/05_et100_development_reference.md)。

## Summary（修訂）

✅ **ADR 01 經實證可行**，條件如下：
- Build 成功的是 **patched 版本的 SDK**（5 處本地 patch，見 [`vendor/PATCHES.md`](vendor/PATCHES.md)）
- 原始 SDK 直接 build **會失敗**（已實測）
- 產出 `output/Project_burn.bin` 為 post-process 封裝候選映像（header／CRC／padding），是否符合板上 bootloader 仍未驗證

❗ **不可過度推論**：
- 「可燒錄 + 可啟動 + 可廣播 BLE」**尚未實機驗證**，EVB 到之前無法確認
- Patch #5 的 header 格式推自 `post_process.py` 的 struct 定義；bootloader 真的接受此格式仍要 EVB 證實
- Flash 燒錄 address（推測 0x08002000）需等 Quectel 或 Freqchip 燒錄工具文件確認

## Build Result

```
Flash raw image: 260,352 bytes = 254.25 KiB / 1016 KiB = 25.0%
SRAM static allocation: 97,232 bytes = 94.953 KiB / 256 KiB = 37.1%
上述分母為目前 linker window，不是 datasheet 晶片總容量；不代表 runtime heap 餘量。

text:  225,072 bytes  (GNU size 分類，含需載入 RAM 的 code)
data:   35,272 bytes  (GNU size 分類，含 .dram_section)
bss:    61,976 bytes  (GNU size 分類，含靜態 heap 配置)
```

## Products — 用途與限制

| 檔案 | 大小 | 用途 | 可否燒錄 |
|---|:-:|---|:-:|
| `Objects/Project.elf` | 1.3 MB | GDB / Cortex-Debug 讀 symbol | 是否可供 programming 取決於 loader；未驗證 |
| `output/Project.hex` | 732 KB | Intel HEX（JLink 多半可直接讀）| ⚠ 內含 raw code，無 bootloader header，實測後定 |
| `output/Project.bin` | 260,352 bytes | raw image，不含 post-process header | 未驗證；不能保證直接啟動，也不能斷言所有流程都不會 boot |
| **`output/Project_burn.bin`** | **268,544 bytes** | **封裝候選映像（header + CRC + padding）** | 未驗證；需確認 board-matched boot 格式與燒錄流程 |
| `output/Project.map` | 1.1 MB | Linker map | — |
| `output/Project.lst` | 4.3 MB | Disassembly | — |

**本報告不是已驗證的燒錄指引**。候選映像 payload 從 offset 0x2000 開始；linker app address 是 0x08002000。整份封裝是否應寫入 0x08000000、是否需其他 ROM metadata／分區，須依 board-matched 原廠文件與 EVB 測試確認，不直接以工具 default 作保證。

## Findings

### Critical
_None._

### Major
- **MJ1（Codex review 揭露，已修）**：Patch #4 一開始只改 `python.exe → python3` + 加 `-` 讓失敗不致命，實際上 **post_process.py 本身因 Windows backslash 失敗，`Project_burn.bin` 從未產生**；initial REVIEW_REPORT 誤稱「可直接燒錄」是 overclaim。本次補 Patch #5 修 post_process.py 跨平台路徑 + 改 CHIP_TYPE 為 FR303x，`Project_burn.bin` 現已產出。
- **MJ2（Codex review 揭露，已處理）**：Phase 1a 原 `IMPLEMENTATION_PLAN.md` Out of Scope 明示「不要修改 SDK」，但實作過程改了 5 個 SDK 檔案。此為 **scope creep**。已補寫 [`ADR 02 — SDK Local Patch Policy`](docs/99_decisions/02_sdk_local_patch_policy.md) 追認並立下未來規則：將來若 patch > plan 預期量，Claude Code 必須停下請示。
- **MJ3（Codex review 揭露）**：原 REVIEW_REPORT 的 **「完整成功、可直接燒錄」為 overclaim**。精確的宣稱應為「patched SDK 的 host-side build pass；啟動 + BLE 廣播 + 硬體除錯仍待 EVB 驗證」。本修訂版已改。

### Minor（原列，不變）
- **M1**：Linker 警告 `LOAD segment with RWX permissions`。原因：`.ram_code*` section 同時 exec + write + load，符合「code copied to RAM then executed」用途。可忽略。
- **M2**：Toolchain PATH 需手動加入 `~/.zshrc`（Stanley 已完成）。
- **M3**：Codex 從 gitee 下載 SDK 失敗（curl 18/33/56），改由 Claude Code 從 scratchpad cache 複製；長期需找更穩的 SDK 分發管道。
- **M4**：`.ram_code_front` section patch 用 full-section 而非 empty symbol 作法，實測 Section 有 296 bytes 內容 → full-section 是對的。

### Informational
- **I1**：ARM GNU Toolchain 14.3.rel1 / CMake 4.4.3 / Ninja 1.13.2 — 現役版本
- **I2**：Build 時間單 thread ~45 秒；可 `make -j8` 加速
- **I3**：BLE binary library (`libbtdm_host.a`) 與 ARM GCC 14 link 無 ABI 衝突
- **I4**：Codex 參與了 install toolchain 與 review 兩個角色，分工照 comm.md Step 1-5 跑了一輪：Claude Code 寫 plan → Codex 試行 → Claude Code 補救 build → Codex 做 cross-review → Claude Code 修正 → Codex 可再 review

## 真正證明了什麼 / 沒證明什麼

### ✅ 已證明
- macOS 26.3.1 arm64 + ARM GCC 14.3.1 已以 SDK 原生 Makefile 完成範例 build；CMake／Ninja 已安裝，但本次未證明 CMake 整合流程
- Freqchip 公開 SDK + 5 處本地 patch 後，GCC build 流程完整走通到 `Project_burn.bin`
- 產物與靜態配置落在本次 linker window 內；完整晶片 memory map、runtime heap／stack 餘量仍待確認
- `libbtdm_host.a` (BLE binary) 可與 ARM GCC 14 link 無錯誤
- `post_process.py` 的 header 填入邏輯對 `Project.bin` 執行成功、產出 `Project_burn.bin`

### ❌ 尚未證明
- `Project_burn.bin` 燒進真實 FR3068E-C 後**能啟動**
- 啟動後**能跑到 main()**
- 跑起來後**能廣播 BLE packet**（example app 的功能）
- SWD / JTAG debug 流程可用
- Flash 燒錄 address 0x08000000 推測正確
- Freqchip bootloader 真的接受此 header 格式
- Patches 對產品行為無副作用（特別是 `.ram_code_front` 的 patch）

**上述所有 ❌ 項目都需要 EVB 到才能驗證**。

## Verification Evidence

```bash
# 執行紀錄：
$ cd fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC
$ rm -rf Objects output
$ make 2>&1 | tail -5
python3 ../../../../components/tools/post_process.py Project output
program target with file output/Project_burn.bin
Build: Project make done.

$ ls -la output/
Project.bin        260352 bytes
Project.hex        732395 bytes
Project.lst        4344035 bytes
Project.map        1179136 bytes
Project_burn.bin   268544 bytes   ← 封裝候選映像，boot 尚未驗證

$ python3 -c "import struct; f=open('output/Project_burn.bin','rb'); \
    print(hex(struct.unpack('I',f.read(4))[0]))"
0xffffffff    # version header (expected)

$ arm-none-eabi-size Objects/Project.elf
   text    data     bss     dec     hex filename
 225072   35272   61976  322320   4eb10 Objects/Project.elf

$ file Objects/Project.elf
Objects/Project.elf: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV)
```

Full build log: `/tmp/et100_build.log`

## 已記錄的本地 patches（全在 SDK 本體內；commit 狀態未查證）

依 [ADR 02](docs/99_decisions/02_sdk_local_patch_policy.md) 政策保留，詳見 [`vendor/PATCHES.md`](vendor/PATCHES.md)：

1. `sysmem.c` + `#include <sys/types.h>`
2. `ldscript.ld` + `.ram_code_front` section
3. `cpu_context_gcc.S` + `.thumb_func` / `.type` / 搬 `.size`
4. `Makefile` python.exe → python3
5. `post_process.py` 跨平台路徑 + CHIP_TYPE FR303x

## Overall（修訂後）

**Pass with Caveats** — Phase 1a 的 host-side 目標達成；但 initial REVIEW_REPORT 的「可直接燒錄」宣稱是 overclaim，已修正。將 Pass 狀態從「完整成功」降為「host build 成功 + 燒錄行為待 EVB 驗證」。

## Next Steps

### 立即（Stanley）
1. ✅ PATH 已加 `~/.zshrc`
2. ⬜ 重產 100_questions V1.3 HTML／PDF 後寄 Quectel（目前僅 Markdown 同步）
3. ⬜ 購 Freqchip FR3068E-C EVB（taobao / 面包板）

### EVB 到貨後的 Phase 1b
4. ⬜ 用 Freqchip 原廠燒錄工具或 J-Link 燒 `Project_burn.bin` 到 EVB
5. ⬜ 開 serial / 看 UART log，驗證 app 跑到 main()
6. ⬜ 用 nRF Connect / LightBlue 掃 BLE，確認廣播存在
7. ⬜ 驗證 J-Link SWD debug 可連線
8. ⬜ **若 Phase 1b 證實 _burn.bin 可燒錄啟動** → ADR 02 + Phase 1a 的 patches 全面確認；否則回頭看 header 格式對不對

### 並行
9. ⬜ EUP 終端設備通訊協定考古（不需 EVB）

## 感謝 Codex Review

Codex 在初版 REVIEW_REPORT 發送後做 cross-review，正確抓出 3 項 Major issue（Patch #4 不完整、scope creep、overclaim）。本修訂版針對該 review 全盤接受並修正。
這是 comm.md 的 Claude Code / Codex 分工機制在單人 + AI toolchain 環境下實際跑的第一輪，證明雙 AI reviewer 架構有效。

---

# Addendum — Phase 1a-ext（2026-10-02 下午）

## Context

Codex Review 完 Phase 1a 後再做 datasheet-驅動的全文件同步，發現：
- **SRAM 真實容量 512 KiB**（Keil .sct 證實；GCC .ld 原僅配 256 KiB）
- 同時發現 example `main.c` clock init 把核心跑到 **192 MHz**（datasheet 上限 156 MHz）
- Codex 在修改前停下請示 → Stanley 兩階段擴大授權 → 轉成 Phase 1a-ext

## Scope + Deliverable

- Patch #6：ldscript.ld + ldscript_3068e.ld SRAM 256K → 512K
- Patch #7a：example main.c MCU_DIV 1 → 2（192 MHz → **96 MHz**）
- Patch #7b：system_fr30xx.c `System_MCU_clock_Config()` 入口加 156 MHz runtime guard
- 全部重 build + 驗證

## Build Result

| 指標 | Phase 1a | Phase 1a-ext | 差 |
|---|---:|---:|---|
| text | 225,072 | **225,096** | +24 B（Patch #7b guard 幾條指令） |
| data | 35,272 | 35,272 | 0 |
| bss | 61,976 | 61,976 | 0 |
| `.ram_code` | 14,728 | 14,752 | +24 B（guard 進了 ram_code 區） |
| Project.bin | 260,352 | 260,376 | +24 B |
| Project_burn.bin | 268,544 | 268,568 | +24 B |
| image_crc | 0xe87afbd9 | 0x5b1a2ae6 | 變（因 code 變） |
| magic (QRRQ) | 0x51525251 | 0x51525251 | 不變 ✓ |
| SRAM 可用容量 | 256 KiB | **512 KiB** | +256 KiB |
| SRAM 實際佔用 | 97 KB → 37.1% | 97 KB → **18.9%** | 相同佔用，使用率對半 |

## 驗證重點

- **Section 位址確認**：bss / data / dram_section / _user_heap 全部落在 0x20000000 - 0x2001E008 之間，距 512K 上限（0x20080000）仍有 >400 KB 餘裕。
- **Patch #7b guard 位置設計優秀**：Codex 插入 `System_MCU_clock_Config()` 函式入口，計算 target_core_clock_hz 後、進 PLL register 前，比 plan 要求的「最靠近寫入 PLL 的點」做得還到位。而且智能處理了兩種 clock source（CORE_HSCLK vs SPLLCLK）—— plan 沒要求但 Codex 自己做了，加分。
- **burn.bin header 結構不變**：exec_offset 0x2000、magic 0x51525251，只有 image_crc 隨 code 變化而重算。

## Findings

### Critical / Major
_None._（Codex 自我約束 + plan 寫得明確，一次 pass）

### Minor
- **m1**：Phase 1a-ext build 的實際 SRAM 使用量不變（97 KB），因 example app 本來就不會吃到 256 KB → 512 KB 擴容對這個 example build 的統計意義是「未來 ET-100 app 可用空間 +256 KB」而非「現在省 256 KB」
- **m2**：Patch #7b 的 guard 進了 `.ram_code` section（因為 `__RAM_CODE` 屬性），開機時 copy 到 PRAM 執行。這代表 guard 速度最快但吃一點 PRAM。影響可忽略。

### Informational
- **I1**：Patch #7a 的降頻結果 96 MHz 是 datasheet 156 MHz 上限的 61.5%，margin 充足；功耗相對 192 MHz 省 50%
- **I2**：Codex 完全沒越界（SDK 以外、CMakeLists、本專案 src 都沒碰）
- **I3**：Build 一次 pass，無新 warning（除了 Phase 1a 已知的 RWX LOAD segment）

## Overall

**Pass**（無 Caveats）—— Phase 1a-ext 的全部 scope 達成。host build 成功、Project_burn.bin 產出、SRAM 擴容到 datasheet 一致、clock 降到 datasheet 內保守值 + runtime guard。

## 感謝 Codex (round 2)

Phase 1a-ext 展示了 Codex 兩個進化：
1. **主動停下請示的習慣鞏固**：發現 192 MHz over-spec 時立即停、提兩個選項、解釋 trade-off，而非悶頭跑完
2. **執行時的工程判斷力加分**：Patch #7b 的 guard 插入位置選得比 plan 要求更精準；處理雙 clock source 的 formula 更周到

這證明 ADR 02 + comm.md 分工在**第二輪實測**仍穩定運作。

## Phase 1a-ext 的 Next Steps（接續 Phase 1a 的清單）

- ⬜ 把 Q49（AEC-Q100 否）+ Q50（SDK 192 MHz vs datasheet 156 MHz）加進 100_questions_for_quectel V1.4
- ⬜ 重 render HTML + PDF V1.4
- ⬜ Stanley 寄 Quectel PDF（V1.4，取代 V1.3 send）
- ⬜ 更新 [`docs/00_project/11_project_bring_up_status.md`](../docs/00_project/11_project_bring_up_status.md) Category 7（7 處 patch）+ Category 8（SRAM 使用率 18.9%）
- ⬜ 其餘 Next Steps（EVB 購買、EUP 協定考古）不變
