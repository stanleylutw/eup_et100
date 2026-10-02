# IMPLEMENTATION_PLAN — Phase 1a-ext: SRAM 512 KiB + Clock Guard + Rebuild

## Branch

Phase 1a-ext 繼續在 main（git repo 尚未 init；本任務執行時無 branch 概念）。
若之後決定 git init，整個 Phase 1a + 1a-ext 的改動一起進第一個 commit。

Base branch: main（git 尚未 init）

## Context

- Phase 1a 已完成 host-side build（patches #1-#5），產出 `Project_burn.bin`
- Codex 隨後審查發現 **GCC ldscript 的 SRAM 僅配 256 KiB，但 Keil scatter 同一顆晶片配 512 KiB**（FR3068E-C 實際有 512 KiB SRAM）
- 本次 Phase 1a-ext：**保守擴容 + 加 clock sanity check + 重 build 驗證**
- Stanley 2026-10-02 口頭授權（chat 內）

## Scope — 必做的事

### Step 1 — Patch #6：擴 SRAM 到 512 KiB

**檔案 A**：`fw/vendor/fr30xxc_sdk__202411/components/tools/gcc/ldscript.ld`

```ld
# 原（第 18 行）：
  SRAM (rw)     : ORIGIN = 0x20000000, LENGTH = 256K
# 改為：
  SRAM (rw)     : ORIGIN = 0x20000000, LENGTH = 512K      /* [eup-patch 2026-10-02] 256K→512K per datasheet p.7 + Keil .sct 一致 */
```

**檔案 B**：`fw/vendor/fr30xxc_sdk__202411/components/tools/gcc/ldscript_3068e.ld`

同樣修改（檔案第 18 行）。

**不改 `ldscript_3066d.ld`**（FR3066D-C 系列真的只有 256 KB SRAM，見 datasheet p.7）
**不改 `ldscript_3092e.ld`**（不同晶片，不確定 memory map）

### Step 2 — Patch #7：降頻到 96 MHz + 156 MHz Runtime Guard

**背景**（Codex 2026-10-02 發現並停下請示）：
- example `main.c:116` 設 PLL_N=8, SOC_DIV=1, MCU_DIV=1
- `fr30xx.h:249` 晶振 24 MHz
- → 24 × 8 ÷ 1 ÷ 1 = **192 MHz 核心時脈**
- **超過 datasheet p.7 列 FR3068E-C MC1(CM33) 最高 156 MHz 整整 36 MHz**

**Stanley 2026-10-02 擴大授權範圍**：允許修改以下兩個 .c，目的是把時脈降到 datasheet 內的保守值 + 加 runtime 上限 guard。

**檔案 A**：`fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/Src/main.c` 約 line 116

修改內容：把 `MCU_DIV = 1` 改為 `MCU_DIV = 2`
- 新時脈 = 24 × 8 ÷ 1 ÷ 2 = **96 MHz**（遠低於 156 MHz 上限，margin 60 MHz）
- 加 `[eup-patch 2026-10-02 patch #7a]` 註解 + 一行說明「datasheet p.7 limit 156 MHz」
- **96 MHz 選用理由**：功耗省一半（P∝f）、BLE 性能充足（nRF52 系列也常跑 64 MHz）、留餘裕給未來若 datasheet 更新

**檔案 B**：`fw/vendor/fr30xxc_sdk__202411/components/drivers/device/fr30xx/system_fr30xx.c`

找 clock init 函式入口（Codex 選最合適的，例如 `SystemCoreClockUpdate()` 或 `system_clock_set()` 等），加 runtime guard：

```c
/* [eup-patch 2026-10-02 patch #7b] FR3068E-C MC1 max 156 MHz per datasheet p.7 */
#define FR3068EC_MC1_MAX_HZ  156000000U

/* 在算出 target_core_clock_hz 之後、真正寫 PLL register 之前加 */
if (target_core_clock_hz > FR3068EC_MC1_MAX_HZ) {
    /* over-spec clock request; halt rather than silently over-clock */
    while (1) { __BKPT(0); }   /* or assert() if available */
}
```

**原則**：
- 只加 **一** 處最靠近「真正寫入 PLL」的點
- 若函式路徑複雜找不到唯一適合點 → 報告後 skipped（不要硬塞多處）
- 不改函式邏輯、不改回傳值、不加 printf

**標記**：`[eup-patch 2026-10-02 patch #7a + #7b]`

### Step 3 — 完整重 build

```bash
export PATH="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin:$PATH"
cd /Users/stanley/Firmware/EUP/eup_et100/fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC
rm -rf Objects output
make 2>&1 | tee /tmp/et100_build_v2.log
```

### Step 4 — 驗證

```bash
# 4a. 產物確認
ls -la output/
test -f output/Project_burn.bin && echo "✓ burn image produced"

# 4b. Size + section layout 看到 SRAM 可用區變成 512 KiB
arm-none-eabi-size Objects/Project.elf
arm-none-eabi-size -A Objects/Project.elf | head -25

# 4c. Linker map 檢查 SRAM 區域上限
grep -E "SRAM|0x2007|0x2008" output/Project.map | head

# 4d. burn.bin header CRC + magic 不變
python3 -c "
import struct
with open('output/Project_burn.bin','rb') as f:
    v = struct.unpack('I', f.read(4))[0]; print(f'version        : 0x{v:08x}')
    v = struct.unpack('I', f.read(4))[0]; print(f'store_offset   : 0x{v:08x}')
    v = struct.unpack('I', f.read(4))[0]; print(f'code_length    : {v}')
    v = struct.unpack('I', f.read(4))[0]; print(f'exec_offset    : 0x{v:08x}')
    v = struct.unpack('I', f.read(4))[0]; print(f'copy_unit      : {v}')
    v = struct.unpack('I', f.read(4))[0]; print(f'copy_flag_step : {v}')
    v = struct.unpack('I', f.read(4))[0]; print(f'image_crc      : 0x{v:08x}')
    v = struct.unpack('I', f.read(4))[0]; print(f'magic (QRRQ)   : 0x{v:08x}')
"
```

### Step 5 — 更新 PATCHES.md

Append Patch #6（SRAM）+ Patch #7（Clock guard）兩個 entry，格式仿照 #1-#5：
- 檔案路徑
- 症狀 / 原因
- 修改內容
- 影響評估
- 驗證方式

### Step 6 — 更新 PROJECT_STATUS 的 Category 7、8

- Category 7 新增 Patch #6 + #7（總共 7 處 patch）
- Category 8 更新 size：**SRAM 使用從 97/256=37.1% 改為 97/512=18.9%**

## Out of Scope — **禁止做的事**

- ❌ 不要碰 `ldscript_3066d.ld` / `ldscript_3092e.ld`（不同晶片）
- ❌ 不要改 CMakeLists.txt（本次只驗證 SDK 原生 Makefile）
- ⚠️ 原本禁改 `.c`；本次**例外授權改 2 檔**（example main.c clock divider + system_fr30xx.c runtime guard），其他 .c 仍禁改
- ❌ 不要碰應用層 BLE / AT / OTA 功能代碼
- ❌ 不要跑 `git init` 或任何 git 操作
- ❌ 不要重新下載 SDK
- ❌ 不要調整 toolchain（arm-none-eabi-gcc 14.3.1 繼續用）
- ❌ 不要修改 CHIP_TYPE 或其他 post_process.py 設定（Patch #5 已確認正確）
- ❌ 不要新增 `__attribute__((section("sram_extended")))` 類應用層代碼使用新 SRAM 區（這是 Phase 2 的事）
- ❌ **若 Patch #7 clock check 無合適插點，報告「skipped, no suitable injection point」而非硬塞**（不要為加 check 而破壞程式邏輯）

## Deliverable — Report Format

執行完請用以下格式回報：

```markdown
# Phase 1a-ext Report

## Changes applied

### Patch #6 — SRAM 512 KiB
- ldscript.ld line X: <old> → <new>
- ldscript_3068e.ld line X: <old> → <new>

### Patch #7 — Clock 156 MHz guard
- 插入位置: <file:line> OR "skipped: no suitable injection point"
- 原始碼: <paste the inserted lines>

## Build Result
- make exit code: <0 / nonzero>
- .elf produced: yes/no
- .burn.bin produced: yes/no

## Size Comparison
Before (Phase 1a):
   text     data     bss     dec     filename
 225072    35272   61976  322320    Project.elf
 SRAM use: 97 KB / 256 KB = 37.1%

After (Phase 1a-ext):
   text     data     bss     dec     filename
   XXXX     XXXX    XXXX    XXXX    Project.elf
 SRAM use: XX KB / 512 KB = XX.X%
 Flash use: XX KB / 1016 KB = XX.X%

## burn.bin Header Verification
<paste python decode output>
- magic (QRRQ): <0x51525251 expected>
- image_crc: <value>

## PATCHES.md updated: yes/no
## 11_project_bring_up_status.md updated: yes/no

## Issues / Warnings
<any new warnings; expected to be only the known RWX LOAD segment warning>

## Full build log location
/tmp/et100_build_v2.log
```

## Verification（Stanley 這邊怎麼驗）

Codex 回報後，Stanley 檢查：
1. Patch #6 Numbers correct（兩個 .ld 各改 1 行）
2. Patch #7 要嘛加成功要嘛明確 skipped（不含糊）
3. Size 應顯示 SRAM 512K pool 可用（而非 256K）；bss 不變或略變
4. burn.bin header magic 仍是 0x51525251，CRC 可能變（因 .data 位置可能微調）
5. `/tmp/et100_build_v2.log` 不含 fatal error

## Rollback

本任務全部可逆：
- 若 build 失敗 → `git diff` 看兩個 .ld + 一個 header 的改動，手動 revert
- 若 .elf 燒不進（EVB 到之後才驗，本任務不涉及）→ 退回 Patch #5 版本的 build

## 完成後下一步

- ✅ 若成功 → ADR 02 的 patches 擴到 7 處；Phase 1a-ext 關閉
- ⚠️ 若 Patch #7 clock check skipped → 記錄為 Phase 1b 的 TODO（等有 SDK 文件確認 clock init 流程再補）
- ⚠️ 若 build 失敗 → 分析錯誤，可能是我們以為 SRAM 可到 512K 但實際 BSP 內部有 hardcoded 256K 邊界

## 授權依據

Stanley 2026-10-02 chat 內口頭授權（兩次）：
1. 初次：「授權我讓 Codex 做『保守 rebuild』（升級 SRAM 到 512 KiB、加 156 MHz 檢查、重出 Project_burn.bin）」
2. 擴大：Codex 發現 example 預設 192 MHz 超 datasheet 156 MHz 上限後，Stanley 擴大授權允許修改 `main.c` + `system_fr30xx.c` 兩 .c 降頻到 96 MHz 並加 runtime guard

依 [ADR 02](docs/99_decisions/02_sdk_local_patch_policy.md) §4 Transparency 本次新增 SDK patches 需顯式授權，兩次條件皆滿足。

**Codex 2026-10-02 process excellence 加分**：發現 192 MHz over-spec 立即停下請示，而非悶頭跑完才報告。這是 ADR 02 §4 要的理想行為。

## 附帶動作 — 加 Q50 到 Quectel 問題清單

請把以下題目加進 [`100_questions_for_quectel.md`](../docs/03_hardware/sch_analysis/100_questions_for_quectel.md) 的 P0-FW 區：

### Q50-P0-FW — FR3068E-C 實際可用時脈上限 vs datasheet

**背景**：
- Datasheet p.7 列 FR3068E-C MC1(CM33) **最高 156 MHz**
- Freqchip SDK 的 `ble_simple_periphreal` example 預設 PLL_N=8 / MCU_DIV=1 → **實際跑 192 MHz**
- 超規 36 MHz（23%）
- Eupfin FW 已把 example clock divider 從 1 改到 2，降到 96 MHz 保守值

**問題 Q50-1**：192 MHz 配置是 SDK bug、是 silicon rev 差異、還是 datasheet marketing 降規？
**問題 Q50-2**：FR3068E-C 的實際 absolute max core clock（含 over-spec 可用但不保證範圍）？
**問題 Q50-3**：Quectel 的 FW 跑在多少 MHz？若非 datasheet 內的 ≤156 MHz，依據為何？
**問題 Q50-4**：96 MHz 是否足以支撐 BT 5.3 BR/EDR + BLE 多連線 + CAN FD + LTE AT 的 runtime load？

本題不列入 Phase 1a-ext scope，僅在本 plan 文件化。寫 Q50 由 Claude 另行處理。
