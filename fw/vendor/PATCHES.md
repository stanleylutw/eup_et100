# Freqchip SDK Local Patches

本檔記錄為了讓 **ARM GCC 14.3.1 on macOS** 可以 build Freqchip FR3068E-C SDK，對 SDK source 做的本地修改。

> **2026-10-02 文件同步**：依 v0.4.9 校正 memory 容量來源與 build 使用率，撤回尚未實機驗證的 boot 保證；此次只修改本 Markdown，沒有修改 SDK 或重跑 build。規格與配置對照見 [FR306x 開發參考](../../docs/03_hardware/fr306x_reference/05_et100_development_reference.md)。

**原則**：
- 所有 patch 處用 `[eup-patch YYYY-MM-DD]` 註解標記，方便 grep
- 每個 patch 說明原因、影響範圍
- 將來 Freqchip 發新版 SDK，用 `tools/fetch_sdk.sh --force` 覆蓋後，重新套用這些 patch

**TODO**：之後把 patch 搬到 `fw/patches/*.patch` + fetch 腳本自動 apply，SDK 本體保持原始狀態。

---

## Patch #1 — sysmem.c：補 `#include <sys/types.h>`

**檔案**：`fr30xxc_sdk__202411/components/drivers/device/fr30xx/gcc/sysmem.c`

**症狀**：
```
error: unknown type name 'caddr_t'
```

**原因**：新版 newlib（GCC 14.3 內附）不再自動 include `<sys/types.h>`，`caddr_t` 需明示 include。

**修改**：在 `#include <stdio.h>` 後加一行 `#include <sys/types.h>`

**影響**：零 side effect，只是補 header。

---

## Patch #2 — ldscript.ld：新增 `.ram_code_front` section

**檔案**：`fr30xxc_sdk__202411/components/tools/gcc/ldscript.ld`

**症狀**：
```
undefined reference to `_siram_code_front'
undefined reference to `_sram_code_front'
undefined reference to `_eram_code_front'
```

**原因**：`startup_fr30xx.S` 的 Reset_Handler 有一段「copy .ram_code_front from flash to IRAM」的 loop，需要 `_siram_code_front / _sram_code_front / _eram_code_front` 這組 symbol。但原始 ldscript.ld 沒定義（只定義 `_siram_code / _sram_code / _eram_code`）。這是 SDK vendor bug — startup 與 ldscript 版本不同步。

**修改**：在 `.ram_code` 定義前新增一個完整的 `.ram_code_front` section：

```ld
_siram_code_front = LOADADDR(.ram_code_front);
.ram_code_front :
{
    . = ALIGN(8);
    _sram_code_front = .;
    *(.ram_code_front.*)
    *(ram_code_front*)
    . = ALIGN(8);
    _eram_code_front = .;
} >PRAM  AT>FLASH
```

**影響**：若無任何 `.c` 用 `__RAM_CODE_FRONT` macro（SDK 定義在 `system_fr30xx.h` 但目前無 .c 使用），section 空，copy loop 零次執行；若有 .c 用該 macro（例如 lvgl 的 `LV_ATTRIBUTE_FAST_MEM` 其實指向 `ram_code`，不是 `ram_code_front`），code 會正確放到 PRAM。

**建議長期**：Freqchip 的新版 SDK 若已修此 bug，本 patch 可移除。

---

## Patch #3 — cpu_context_gcc.S：加 `.thumb_func` + `.type` 聲明

**檔案**：`fr30xxc_sdk__202411/components/modules/FreeRTOS/cpu_context_gcc.S`

**症狀**：
```
Unknown destination type (ARM/Thumb) in Objects/freertos_sleep.o
dangerous relocation: unsupported relocation
```

**原因**：`low_power_save_cpu` 與 `low_power_restore_cpu` 兩個 label 前沒標 `.thumb_func`，連結器 (ld) 不確定目標是 ARM 還是 Thumb mode，無法產生正確的 BL 指令 relocation。

另外，原始的 `.size low_power_save_cpu, .-low_power_save_cpu` 放在 label **之前**（倒反），尺寸計算錯誤。

**修改**：
1. 每個 label 前加 `.thumb_func` + `.type <name>, %function`
2. 把 `.size` 搬到 function body 之後（正確計算 .-label 距離）

**影響**：純修 metadata，程式邏輯不變。Branch instruction 現在能正確解析 Thumb target。

---

## Patch #4 — ble_simple_periphreal/GCC/Makefile：python3

**檔案**：`fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC/Makefile`

**症狀**：
```
make: python.exe: No such file or directory
```

**原因**：Makefile 最後一行 call `python.exe` 跑 post-process 腳本。macOS/Linux 沒 `python.exe`。

**修改**：`python.exe` → `python3`

**影響**：Makefile call 得到 interpreter；但 post_process.py 本身仍有 Windows backslash 問題，見 Patch #5。

---

## Patch #5 — post_process.py：跨平台路徑與錯誤傳遞

**檔案**：`fr30xxc_sdk__202411/components/tools/post_process.py`

**症狀**：
Makefile 跑 `python3 post_process.py Project output` 後印：
```
INVALID INPUT PARAMTER for python script
```
**實際後果**：`output/Project_burn.bin` 從未產生；第一代 patch (#4) 當時把錯誤當成非必要步驟，這個判斷錯誤。`Project.bin` 是 raw image，沒有 post-process 加入的 header／CRC。

**原因**：
1. 腳本第 89 行 `"%s\\%s.bin" % (sys.argv[2], sys.argv[1])` 用 Windows 反斜線組成路徑；macOS 下 `output\Project.bin` 內含字面反斜線，`os.path.exists()` 回 False。
2. 2026-10-02 的原 patch 在沒有 ROM header 規格或原廠產物證據下，另把 vendor default `CHIP_TYPE_FR509x` 改為 `CHIP_TYPE_FR303x`。這項推論後來由 EVB 實測證明錯誤，詳見下方撤回紀錄。

**修改**：
1. `"%s\\%s.bin"` → `os.path.join(sys.argv[2], "%s.bin" % sys.argv[1])`（跨平台）
2. Error path 加上 `sys.exit(1)` 讓 Makefile 看到真實失敗（原 patch #4 的 `-` 已拿掉）
3. Error message 印出 looked-for path，便於 debug

### 2026-10-21 撤回錯誤的 CHIP_TYPE 修改

`CHIP_TYPE_FR509x → CHIP_TYPE_FR303x` **已撤回**，目前恢復 vendor default：

```python
CHIP_TYPE = CHIP_TYPE_FR509x
```

撤回原因與證據：

1. FR303x layout 少了 `tlv_length` word，後續 `options` 欄位向前錯位。把這種 image 燒到 FR3068EP-C EVB 後，ROM 以錯誤 options 存取 flash，造成 AP WAIT ACK、BLE 不啟動且一般 SWD attach 失敗。
2. SDK 內原廠 Keil 產物 `examples/evb_demo/lvgl_demo/MDK-ARM/output/Project_burn.bin` 使用 FR509x layout：word 8 是 `tlv_length = 0`，word 9 才是 options。
3. 恢復 FR509x layout 後，本次映像 word 7 = `0x51525251`、word 8 = `0x00000000`、word 11 = `0x51525251`；經 SDK `FR30xx.FLM` 燒到 `0x08000000` 後可啟動並完成 BLE 實機連線。

這裡的 `FR509x` 是 post-process 腳本的 **header layout selector**，不能依名稱直接推論 MCU 是 FR509x 系列。boot/header 類設定必須以原廠產物、ROM 規格或實機結果為依據。

**Host 驗證（2026-10-02）**：rebuild 後 `output/Project_burn.bin` 存在，header／payload／CRC 自洽。Phase 1a 當時的解析紀錄：
```
magic (0x51525251 QRRQ): 0x51525251 ✓
image_crc (CRC32):       0xe87afbd9
exec_offset:             0x00002000 (符合 flash 0x08002000 + 8KB bootloader)
store_offset:            0x00000000 (first slot)
code_length:             260352 (= Project.bin size)
```

**EVB 驗證（2026-10-21）**：FR509x layout 的 `Project_burn.bin` 透過板載 DAPLink + SDK `FR30xx.FLM` 寫入 `0x08000000`，readback 逐 byte 相同；reset 後 app 啟動，手機可連上 `30xx_Ble_Periphreal` 並看到預期 GATT service/characteristics。

**影響**：跨平台 post-process 可可靠產生含 header／CRC 的 `Project_burn.bin`，失敗時 build 會收到非零狀態。FR509x layout 已在 FR3068EP-C DEV1.4 EVB 驗證可啟動；`Project.bin` raw image 仍未做直接燒錄測試，不能推論其 boot 行為。

**限制**：已驗證的是目前 EVB、目前 SDK 產物與目前 FLM 的組合，不等於取得完整 ROM header 規格，也不自動證明 ET-100 量產燒錄、OTA 或 rollback 流程。

---

## Patch #6 — FR3068E-C GCC linker：SRAM 512 KiB

**檔案**：
- `fr30xxc_sdk__202411/components/tools/gcc/ldscript.ld`
- `fr30xxc_sdk__202411/components/tools/gcc/ldscript_3068e.ld`

**症狀／原因**：GCC 的 SRAM window 只配 256 KiB；v0.4.9 p.7 列 FR3068E-C SRAM 512 KB，同 SDK 的 `components/tools/keil/xip_flash_fr3068e.sct` 為 `RW_IRAM1 0x20000000 0x00080000`。依 Phase 1a-ext 施工單與 Stanley 2026-10-02 授權同步 GCC 配置。

**修改**：兩檔各一行，`SRAM ORIGIN = 0x20000000, LENGTH = 256K` → `512K`，保留 origin，標記 `[eup-patch 2026-10-02]`。Flash 1016 KiB／PRAM 128 KiB 不變；不修改其他晶片的 linker。

**影響**：允許 linker 配置到 0x20080000（exclusive）。不擴大既有 FreeRTOS heap，不新增 upper-bank 使用程式；完整 bank／retention 及實機可用性仍待確認。

**驗證**：clean build PASS；map 的 SRAM 為 `0x20000000 0x00080000 rw`，上限 `0x20080000`（exclusive）。ELF 靜態 SRAM 配置 97,232 bytes，section 仍落在原先低位址區；沒有實機 upper-bank 存取測試。完整結果見下方 Phase 1a-ext 驗證。

---

## Patch #7a — BLE 範例 main.c：核心時脈降為 96 MHz

**檔案**：`fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/Src/main.c`，`hw_clock_init()`。

**症狀／原因**：HSE 24 MHz、PLL_N 8、SOC_DIV 1、MCU_DIV 1，依 SDK 計算核心目標為 192 MHz，超過 FR3068E-C p.7 的 156 MHz。

**修改**：`sys_clk_cfg.MCU_DIV = 1` → `2`，加 `[eup-patch 2026-10-02 patch #7a]`，保留其他 PLL／bus／XIP 設定。

**影響**：MCU 目標 96 MHz，SPLL 仍 192 MHz；cold init 與 wake 共用此函式。降低核心頻率不保證整機功耗下降 50%，BLE／CAN 等並行性能須實測。

**驗證**：clean build PASS；靜態確認 24 MHz × 8 ÷ 1 ÷ 2 = 96 MHz。實際 clock／功耗／runtime 性能待 EVB。

---

## Patch #7b — system_fr30xx.c：156 MHz runtime guard

**檔案**：`fr30xxc_sdk__202411/components/drivers/device/fr30xx/system_fr30xx.c`，`System_MCU_clock_Config()` 入口。

**症狀／原因**：原函式依 caller 設定直接寫核心 divider／source，沒有 FR3068E-C 核心頻率上限檢查。PLL 設定函式尚不知道最終 MCU divider，不能直接限制 PLL 為 156 MHz。

**修改**：標記 `[eup-patch 2026-10-02 patch #7b]`；沿用兩條既有計算路徑，先算 `target_core_clock_hz`，大於 `156000000U` 時執行 `while (1) { __BKPT(0); }`，位於核心 clock register 寫入之前。

**影響**：不變更原 clock 寫入邏輯、回傳值或 printf；trap 不受 `NDEBUG` 影響。此 FR3068E-C 專案本地限制會作用於共用函式，不宣稱適用其他料號，也不提供 PLL readback 或除數有效性驗證。事先切換後再重設 PLL 的其他流程不在此 guard 的保證範圍。

**驗證**：clean build PASS；`/tmp/et100_clock_guard_test.py` 擷取實際函式，以 register stub 測試 CORE_HSCLK／SPLL、不同 divider、96 MHz、156 MHz 邊界及 >156 MHz，共 9 組 PASS。超規時零 register writes；ARM disassembly 的 compare／branch 位於 write 前，trap 為 BKPT + loop，不受 `NDEBUG` 影響。Host stub 不是硬體驗證；硬體行為待 EVB。

---

## Patch #8 — BLE 範例 main.c：EVB 開發版停用 sleep

**檔案**：`fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/Src/main.c`，scheduler 啟動前的 sleep enable 路徑。

**症狀／原因**：FR3068EP-C DEV1.4 板載 DAPLink 沒有連接 MCU nRESET；原範例清除 `SYSTEM_PREVENT_SLEEP_TYPE_DISABLE` 後可進入低功耗，SWD 可能無法穩定 attach，而 pyOCD `under-reset` 也無法使用。

**修改**：以 `[eup-patch 2026-10-21 patch #8]` 標記，註解掉：

```c
system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_DISABLE);
```

其餘 clock、BLE、FreeRTOS 與 application 邏輯不變。

**影響**：僅供 EVB bring-up 的 development build 使用。系統維持 sleep prevention，方便 SWD attach，但待機功耗與低功耗行為不具產品代表性；ET-100 production firmware 不得直接沿用此設定。

**驗證**：patched example build 後，以 FR509x header 的 `Project_burn.bin` 寫入 EVB；reset 後 app 正常啟動，SWD 可再次 attach，手機可連線 BLE 並看到 FFF0／FFF3 與 128-bit characteristic。尚未驗證長時間 BLE 穩定性或低功耗流程。

---

## Patch #9（暫時診斷，已撤回）— BLE 範例 main.c：DEV1.4 PMU 按鍵 mapping

**檔案**：`fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/Src/main.c`

**症狀／原因**：FR3068EP-C DEV1.4 EVB 的 K1-K3 不會觸發現有 BLE example 行為，repo 也沒有此板 revision 的原理圖。唯讀 SWD 輪詢 GPIOA-E input registers 時沒有觀察到按鍵變化；SDK 其他 board profile 暗示按鍵可能位於 PMU `PPx` domain，但不能直接當成 DEV1.4 mapping。

**暫時修改**：依 Stanley 2026-10-21 明確授權，曾新增低優先序 `key_scan_task`：每 20 ms 被動讀取 `ool_read16(PMU_REG_PIN_DATA)`，啟動時印一次 baseline，之後僅在值改變時輸出 tick、old/new value 與 changed mask。程式以 `[eup-patch 2026-10-21 patch #9]` 標記。

**診斷結果**：diagnostic image clean build、FR509x header/payload 檢查、program 與 readback 均 PASS。K1、K2、K3 依序長按的 497 行 UART capture 中，PMU bit 0、2、3 都曾改變，但 bit 0／3 在按住期間仍以約 20-40 ms 反覆跳動，bit 2 也沒有形成可重複的穩定電位。因此只能保留 `0x0008`、`0x0001`、`0x0004` 為三顆按鍵的候選 changed mask，**不能確認 K1-K3 對應關係、active level 或 debounce 行為**。被動讀取時沒有設定 PMU direction／pull／mux；輸入浮動或 pin ownership 衝突是可能原因，但尚未證實。

**影響**：診斷期間增加一個 128-word FreeRTOS task stack、週期性 PMU read 與狀態改變時的 UART output；沒有設定 PMU direction、pull、mux、XOR、wakeup 或 interrupt registers，也沒有驅動 D3/D4。依 plan 在結果無法定論後停止，未自行增加 PMU configuration patch。

**Rollback（已完成）**：已移除 `key_scan_task` 與 `main()` 中對應的 `xTaskCreate`，clean rebuild 並燒回 Phase 1b baseline。baseline `Project_burn.bin` 為 268,560 bytes，SHA-256 `0710a7e79de7ac2dd6c1f930119f1b82c4400a64a1d1ea76086da9e08be76fc2`；program/readback PASS。Patch #9 不在目前 SDK source，也不計入 active patch 數。

**Logs**：diagnostic build `/tmp/et100_gpio_diag_build.log`；受控按鍵 capture `/tmp/et100_gpio_diag_uart.log`；rollback build `/tmp/et100_gpio_diag_rollback_build.log`。

### Phase 1a-ext 驗證（2026-10-02）

此階段新增 #6、#7a、#7b；當時合計 7 組 patch（#7 有 a／b 兩個子項，登錄共 8 個 entry）。SDK 手動修改限兩個 GCC linker 與兩個 .c；Makefile、post_process.py／CHIP_TYPE、其他晶片 linker 均維持 Phase 1a 當時狀態。CHIP_TYPE 的錯誤於 2026-10-21 撤回，見 Patch #5。

```text
make exit code: 0
   text    data     bss     dec filename
 225096   35272   61976  322344 Project.elf
SRAM: 97232 bytes = 94.953125 KiB / 512 KiB = 18.5%
Flash raw image: 260376 bytes = 254.2734375 KiB / 1016 KiB = 25.0%
Project_burn.bin: 268568 bytes
magic: 0x51525251
image_crc: 0x5b1a2ae6
```

與 Phase 1a 相比 text／raw image 增加 24 bytes，data／bss 不變，`.ram_code` 為 14,752 bytes。Header code_length 與 raw BIN 相同，offset 0x2000 後 payload 逐 byte 相同，CRC32 以 zlib 獨立重算一致，兩處 magic 皆通過。這些只證明 host build／封裝一致性，非實機 boot／BLE／OTA 驗收。

**Warning**：僅既有 `LOAD segment with RWX permissions`；無新增 compiler／linker warning。施工單的 18.9% 為估算；依精確 byte／KiB 重算為 18.5%，不改寫原施工單。

**完整 build log**：`/tmp/et100_build_v2.log`。Phase 1a 歷史 metrics／產物表保留於下方，並非最新 build。

---

## 感謝 Codex Review（2026-10-02）

Patch #5 由 Codex 的 code review 發現：
> post_process.py 仍使用 Windows 的反斜線組合路徑，單改 python.exe 並未完整解決。
> 現有 BIN 是原始映像，尚不能保證可直接燒錄後啟動。

本 patch 根據此 review 補正。更詳細 scope 政策見 [`fw/docs/99_decisions/02_sdk_local_patch_policy.md`](../docs/99_decisions/02_sdk_local_patch_policy.md)。

---

## 驗證指令

```bash
export PATH="$HOME/.local/share/arm-gnu-toolchains/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin:$PATH"
cd fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC
rm -rf Objects output
make
# 期望：Build: Project make done. + Objects/Project.elf 存在
```

## Build Metrics（歷史：all-5-patches build，2026-10-02）

> 本節保留 Phase 1a 當時數據與判斷；`Project_burn.bin` 的「候選／待 EVB」結論已由上方 Patch #5 的 2026-10-21 FR509x layout 實機驗證取代。

```
   text    data     bss     dec     hex filename
 225072   35272   61976  322320   4eb10 Objects/Project.elf
```

- Flash raw image: 260,352 bytes = 254.25 KiB / 1016 KiB = **25.0%**（linker window）
- SRAM 靜態配置: 97,232 bytes = 94.953 KiB / 256 KiB = **37.1%**（linker window；不是 runtime heap 餘量）
- `.ram_code_front`: 296 bytes
- `.ram_code`: 14,728 bytes
- `.dram_section`: 30,784 bytes

Section mapping 與本次 SDK linker 配置一致（Flash 0x08002000、PRAM 0x1FFE0000、SRAM 0x20000000）。v0.4.9 p.7 的晶片總容量為 2 MB Flash／512 KB SRAM，但未提供 bank 位址及保留區；不能因此宣稱完整 silicon memory map 已驗證或直接擴大 linker。

## Products（歷史：after Patch #5，2026-10-02）

| File | Size | Purpose |
|---|:-:|---|
| `Objects/Project.elf` | 1.3 MB | Debug symbols for GDB / Cortex-Debug |
| `output/Project.hex` | 732 KB | Intel HEX（JLink flashloader 可讀） |
| `output/Project.bin` | 260,352 bytes | raw image；programming／boot 行為待確認 |
| `output/Project_burn.bin` | **268,544 bytes** | 封裝候選映像（含 header + CRC），bootloader 相容性待驗證 |
| `output/Project.map` | 1.1 MB | Linker map |
| `output/Project.lst` | 4.3 MB | Disassembly |

候選映像的 payload offset 為 0x2000，本次 linker app address 為 0x08002000；這不等於已確認 bootloader 大小或整份映像燒錄地址。使用何種產物、地址與 loader，須先確認原廠 programming guide，再於 EVB 驗證。
