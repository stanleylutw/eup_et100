# IMPLEMENTATION_PLAN — Phase 1b.4 CID 與 BLE Type 1 回報

## 本次分支與授權

- Branch：`v0.5.0_evb_cid_ble_scan`；base：`v0.4.0_evb_gtime_settings`。
- Stanley 回報 GTIME 實機可用後，同意繼續 GCID／SCID、BLE scan 與 Type 1 回報。
- 沿用分工：Codex 修改／build／flash，Stanley HTML 與真實 sensor 驗收；不 commit/merge/push。
- SDK patch 預期 0；不改 sibling Gateway、clock、linker、FlashEnv 分區與 boot header。

## 本次範圍

- `fw/src/serial_gchk.c`／`.h`：四個 CID 欄位、預設 0639、FlashDB key 0x00030003，schema 1；版本 0.5.0-dev。
- 新增 `fw/src/ble_reporter.c`／`.h`：AD 邊界解析、CID/UUID filter、32-entry cache、RSSI 排序、最多 min(GTIME,15) 筆、一秒節流、Type 1 XOR、EUP v1/v2 1500 ms 去重。
- 新增 `fw/src/evb_serial_ble.c`：從 patched SDK peripheral app_ble.c 複製，廣播成功後啟動 SDK central 範例同款 general discovery active scan，scan event 轉交 cache。SDK 註解表示 observer mode 可能在 host 過濾 general-discoverable 封包，故採參考範例模式；SCAN_RSP 仍丟棄。保留原 peripheral profile，並行能力待實機驗證。
- `fw/targets/evb_serial.mk`：替換 app_ble object，加入 reporter object。
- 更新 `fw/tests/test_serial_gchk.c`、新增 `fw/tests/test_ble_reporter.c`；更新 optional UART tester 的版本與已支援命令，Codex 不執行該工具。
- 更新 serial protocol、platform plan、REVIEW_REPORT、bring-up status、EVB README；新增 `fw/docs/04_testing/04_evb_cid_ble_scan.md`，補記前版 GTIME 的 Stanley 回覆。

## 驗收與限制

- 沿用 Gateway ASCII/CRLF/token、四個 CID 欄位及 Type 1 format；CID 比對 little-endian ID，支援 0xFF／0x16，符合者優先 MFG。
- 只接收 legacy LE 1M advertising，忽略 scan response／extended／periodic reports；不依賴新 GATT central 連線。
- CID 空欄／0000 為 unused，全部空欄代表不過濾，但仍只回報 MFG／Service Data。
- 尚無 MAC whitelist／MAXRPT setting／realtime XYZ batch；依 min(GTIME,15) 決定每周期上限，不宣稱完整 Gateway 等價。
- CID 變更成功後清除舊 cache/queue；GTIME 變更重設周期，保留 pending cache。
- host sanitizer tests、AD malformed/fuzz、XOR vector、排程／去重／wraparound；build log `/tmp/et100_cid_scan_build.log`。
- header／CRC／FlashEnv 範圍驗證後 flash，log `/tmp/et100_cid_scan_flash.log`；BLE scan／過濾／保存／真實 payload 由 Stanley 驗證。

---

# 歷史施工單 — Phase 1b.3 GTIME 查詢、設定與保存

## 本次分支與授權

- Branch：`v0.4.0_evb_gtime_settings`；base：`v0.3.0_evb_serial_gchk`。
- Stanley 同意 GTIME 查詢、設定、FlashDB 保存；依前次已確認分工，Codex 修改／build／flash，Stanley 操作 UART／HTML 及 reset 測試。
- 保留前次未提交變更，不 commit/merge/push。SDK patch 數量 0。

## 本次範圍

- 修改 `fw/src/serial_gchk.c`／`.h`：GGTIME、SGTIME、FlashDB user key `0x00030002`、版本 `0.4.0-dev`；預設 15、合法 1-1800 秒。
- 修改 `fw/src/evb_serial_main.c`：保留原 FlashDB 初始化流程並傳遞成功狀態，避免失敗時假裝設定成功。
- 修改 `fw/tests/test_serial_gchk.c`：值範圍、儲存失敗保留舊值、mock reload、parser regression。
- 修改 `fw/tools/build_evb_serial.sh`：映像必須位於既有 FlashEnv offset 800 KiB 之前；不擴大 linker 或更改分區。
- 修改未執行的 optional `fw/tools/evb/test_serial_gchk.py`，同步 GCHK 開發版識別。
- 更新 `fw/docs/02_protocol/01_evb_serial_gchk.md`、新增 `fw/docs/04_testing/03_evb_gtime_settings.md`；同步 platform plan、REVIEW_REPORT、EVB README、bring-up status。

## 行為與驗收

- 沿用 SDK FlashEnv（internal flash offset 800 KiB、12 KiB），不清空 DB、不改 BT keys。
- 初始化失敗：GGTIME／SGTIME valid=0；缺失／不合法 record 回到 15 秒，可由 SGTIME 重寫。
- SGTIME 僅接受十進位數字，成功保存並讀回比對後才更新 RAM 與回覆 1；同值避免重寫。
- GTIME 目前只保存設定，不啟動 BLE scan 或計時回報。MAXRPT 未實作，所以未提供 Gateway 的 MAXRPT cross-validation；需於後續 reporter/settings 階段一起加入。
- host sanitizer tests、ARM build、header／CRC／payload 檢查後，依授權 flash/readback/reset。
- build log `/tmp/et100_gtime_build.log`；flash log `/tmp/et100_gtime_flash.log`。HTML 設定／reset 保存測試由 Stanley 操作。

---

# 歷史施工單 — Phase 1b.2 UART3 GCHK 相容子集

## 本次分支與授權

- Branch：`v0.3.0_evb_serial_gchk`；base：`v0.2.2_evb_gpio_diag`。
- Stanley 在本次對話同意執行 UART3 9600 + Gateway GCHK 相容子集，供既有 HTML tester 使用。
- 前次 GPIO 診斷的未提交文件保留；下方保留其歷史施工單。
- 本次 SDK patch 預期數量：0。使用專案 source 與 build overlay；ADR 02 仍適用。

## 本次範圍

- 新增 `fw/src/evb_serial_main.c`：從現有 patched BLE example main 複製，UART3 改成 9600，GCC `_write` 丟棄 debug log。
- 新增 `fw/src/serial_gchk.c`／`.h`：bounded CRLF parser、四字元 token、GCHK、未支援命令失敗回覆、ISR ring buffer 與 task。
- 新增 `fw/targets/evb_serial.mk`、`fw/tools/build_evb_serial.sh`：透過原 SDK Makefile 的 explicit object rules 替換 main/app_at，輸出獨立 `fw/build_serial/`。
- 新增 `fw/tests/test_serial_gchk.c`；新增 `fw/docs/02_protocol/01_evb_serial_gchk.md`。
- 新增 `fw/tools/evb/test_serial_gchk.py`：9600 UART 自動驗收；Stanley 已在本次對話明確同意燒錄並測試。
- 新增 `fw/docs/04_testing/02_evb_serial_gchk.md`：本次 host／build／實機證據。
- 更新 firmware platform plan、REVIEW_REPORT、EVB README、bring-up status，說明候選產物與未驗證項。
- 保留 baseline SDK、921600 build、clock、sleep、linker、post-process 及既有 GPIO 診斷狀態。

## 驗收與限制

- Host parser tests 覆蓋拆包、黏包、token、非法輸入、超長行恢復與連續 100 次 query。
- ARM GCC build；完整 log `/tmp/et100_serial_build.log`；獨立驗證 FR509x header、CRC、payload。
- 協定規格以新文件為準；GCHK state 0 是 compatibility idle，沒有 Gateway central/GATT session 語意。
- 本輪先交付可 review 的 build。EVB 燒錄需依既有明確授權規則執行；未實測不得宣稱 UART／BLE regression PASS。
- 不修改 sibling Gateway repo，不實作 OTA、Gateway scanning/filter/settings，不 commit/merge/push。

---

# 歷史施工單 — Phase 1b.1 GPIO diagnostic: PMU key mapping

## Branch

Implementation branch: `v0.2.2_evb_gpio_diag`

Base branch: `v0.2.1_evb_bringup`

## Context

- FR3068EP-C DEV1.4 EVB has LEDs D3/D4 and buttons K1-K3.
- D3/D4 turn off on reset and turn on near the UART line `FR5090: ble_simple_peripheral.`.
- K1-K3 do not affect the current BLE example.
- Read-only SWD polling of GPIOA-E input registers showed no transitions while K1-K3 were pressed.
- SDK board code for other revisions suggests buttons may use the PMU `PPx` domain, but that mapping is not accepted as DEV1.4 evidence.
- Stanley authorized this diagnostic SDK behavior change and EVB flashing in chat on 2026-10-21.

## Goal

Identify the raw `PMU_REG_PIN_DATA` bits corresponding to K1, K2, and K3 while preserving the current BLE, UART, clock, boot-header, and debug behavior.

## Scope

### Firmware behavior

Modify only:

`fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/Src/main.c`

Add one low-priority FreeRTOS diagnostic task that:

1. Reads `ool_read16(PMU_REG_PIN_DATA)` every 20 ms.
2. Prints one baseline line after startup.
3. Prints only when the raw value changes, including tick, old value, new value, and changed mask.
4. Does not configure PMU direction, pull, mux, XOR, wakeup, or interrupt registers.
5. Does not drive D3/D4 or any unknown pin.

The code must carry marker:

```text
[eup-patch 2026-10-21 patch #9]
```

### Documentation

Update:

- `fw/vendor/PATCHES.md` with Patch #9.
- `fw/REVIEW_REPORT.md` with build/flash/runtime evidence and limitations.
- `docs/03_hardware/FR3068EP-C Dev1.4/README.md` with confirmed K1-K3 mapping only if runtime evidence identifies it.
- `docs/00_project/11_project_bring_up_status.md` with the diagnostic result.

### Build and hardware test

1. Clean build the existing GCC example.
2. Save the full build log to `/tmp/et100_gpio_diag_build.log`.
3. Verify the FR509x burn-header layout before flashing.
4. Flash `Project_burn.bin` with the existing `fw/tools/evb/fr3068_flash.py`.
5. Capture UART3 at 921600 bps to `/tmp/et100_gpio_diag_uart.log`.
6. Ask Stanley to press K1, K2, and K3 separately and correlate each key with a changed PMU bit.
7. Confirm BLE startup still reaches `adv start :0x00`.

## Out of Scope

- No modification outside the files listed above.
- No changes to linker scripts, Makefile, `post_process.py`, CHIP_TYPE, clock configuration, sleep Patch #8, or EVB tools.
- No PMU pin configuration changes in this first diagnostic image.
- No D3/D4 output control.
- No ET-100 production BSP or application implementation.
- No merge or push without a separate instruction.

## SDK Patch Policy

ADR 02 applies. Expected SDK patch count for this task: one behavior patch in `main.c`, Patch #9. Stanley explicitly approved it before modification. If passive PMU reads do not expose K1-K3, stop and report; do not add PMU configuration or another SDK change without renewed approval.

## Acceptance Criteria

- Build exits 0 and produces `Project.elf` and `Project_burn.bin`.
- Burn header word 7 and word 11 are `0x51525251`; word 8 is `0x00000000`.
- Flash programming and byte-for-byte verify pass.
- UART contains the normal BLE startup log and one `[KEY-SCAN] baseline` line.
- Pressing K1-K3 either yields distinct repeatable changed masks or is explicitly reported as inconclusive.
- No unexpected reset, assertion, BLE startup regression, or continuous UART flooding.

## Rollback

Remove the Patch #9 diagnostic task and task creation from `main.c`, rebuild the previously validated Phase 1b image, and flash it with the existing tool.

## Deliverable

Report:

- Changed files and exact diagnostic behavior.
- Build, header, flash, UART, BLE, and K1-K3 mapping results.
- Build and UART log paths.
- Any remaining uncertainty about D3/D4 or PMU pin configuration.
