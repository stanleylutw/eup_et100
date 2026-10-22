# Handoff to Codex — 2026-10-21（Phase 1b EVB bring-up）

> **交接完成註記（Codex，2026-10-21）**：§5 TODO 1-6 已在 `v0.2.1_evb_bringup` 處理；本檔保留為當時交接快照，完成後的權威狀態以 `vendor/PATCHES.md`、`REVIEW_REPORT.md`、EVB README 與 project bring-up status 為準。

**From**：Claude Code（token 即將用完）
**To**：Codex
**Branch**：`v0.2.0_firmware_dev_plan`（已 push 到 origin，但本次 EVB 工作**尚未 commit**）
**規則**：照 [`comm.md`](../comm.md) + [ADR 02](docs/99_decisions/02_sdk_local_patch_policy.md) 工作；改 SDK 前先問 Stanley。回覆格式：英文修正 + 繁中主回覆。

---

## 1. 一句話現況

Freqchip **FR3068EP-C DEV1.4 EVB** 已到貨；macOS + ARM GCC 14.3.1 build 的 `ble_simple_periphreal` 已**透過板載 DAPLink + pyOCD 燒入、開機成功**；手機 nRF Connect **已連上 `30xx_Ble_Periphreal`，看得到 FFF0 / FFF3（Read/Write/Notify）/ 128-bit char**。Phase 1b 主幹跑通。

---

## 2. 本次完成（已實機驗證）

| 項目 | 結果 |
|---|---|
| EVB 型號 | FR3068**EP**-C（= E-C + 2 MB PSRAM；ET-100 用 E-C，韌體不可依賴 PSRAM）|
| DAPLink VCOM | `/dev/cu.usbmodem2024_2425_62851`；ROM UART **115200 8N1** |
| ROM 行為 | 每次 reset 從 UART 送出 `freqchip`（mask ROM，字串不在 SDK 內）；flash 空時停在 ROM 等 UART 下載 |
| SWD | DP IDCODE `0x0BE12477`（DPv2）、AHB5-AP、CPUID `0x410FD214` = **Cortex-M33 r0p4** |
| 燒錄 | pyOCD + SDK `components/tools/keil/FR30xx.FLM`（start 0x08000000、sector 4 KiB、page 256）；燒 **`Project_burn.bin` @ 0x08000000**；66 sectors，~35 s；readback 逐 byte 一致 |
| 開機 | reset 後 PC 在 PRAM `0x1FFE3A90`（`__RAM_CODE` idle 路徑）→ app 正常 |
| BLE | 已實測連線、GATT 正確 |
| 96 MHz（Patch #7a）| 實機開機正常（但未驗證 BLE 長時間穩定性）|

### EVB 硬體重點（請寫進 EVB README，見 §5 TODO-1）
- DEBUG 跳線區：**PB5→DAPLink RX、PB4←DAPLink TX**（UART，MCU PB5=TX、PB4=RX）；**PB10=SWCLK、PB11=SWDIO**
- 出廠只插 UART 兩個 jumper；**目前兩個 jumper 已移到 PB10/PB11（SWD）→ UART log 看不到**。要同時看 log + SWD 需再 2 個 jumper cap 或杜邦線
- DAPLink **沒有 nRESET 線** → pyOCD `under-reset` 不可用
- 外部 flash：**GigaDevice GD25LQ128EWIG（16 MB、1.8 V）** ≠ ET-100 的 XM25QH256D（32 MB、3.3 V、需 4-byte addressing）
- 板上有 CAN + RS485 transceiver 與端子 → CAN J1939 可在 EVB 上 bring-up
- ET-100 的 SWD 也在 **PB10/PB11**（Sheet 1 原圖 pin 21/22），**我 Stage B 的 `01_MCU_FR3068E-C.md` 寫成 PC1/PC2 是錯的**，請修正
- ET-100 的 PB4/PB5 是 **RS232_2**，不是 debug console → 移到 ET-100 時 debug UART 要換腳

---

## 3. 本次事故與根因（重要，避免再犯）

1. **Phase 1a Patch #5 改錯 `CHIP_TYPE`**：Claude 把 `post_process.py` 的 `CHIP_TYPE` 從原廠 `FR509x` 改成 `FR303x`（無證據）。產生的 header 少一個 `tlv_length` 欄位 → ROM 用錯誤 options 存取 flash → **匯流排卡死（AP WAIT ACK）、BLE 不起來、SWD 連不上**。
   - 證據：SDK 附的原廠 Keil 產物 `examples/evb_demo/lvgl_demo/MDK-ARM/output/Project_burn.bin` 是 **FR509x 格式**（word8 = `0x00000000` tlv_length，word9 = options `0x00D02E83`）
   - **已還原為 FR509x**（保留 `os.path.join` 跨平台修正），header 已與原廠格式一致
2. **救援方法**（已寫成腳本）：DPv2 在晶片 reset 後進入 **Dormant**，必須送 dormant→SWD 喚醒序列（pyOCD `SWJSequenceSender(p, True)`）；reset 後 debug 電源需 ~2 ms power-up ACK；放開 RST 後約 4 ms 內寫 DHCSR halt + DEMCR.VC_CORERESET 即可在 ROM 卡住前抓住 CPU；**抓到後不可 disconnect**，同一 process 交給 pyOCD Session 燒錄。
3. 教訓：沒有原廠證據不要改 header / boot 相關設定；`read_ap()` 已處理 posted read，不要再讀 RDBUFF（會得到 0）。

---

## 4. 未 commit 的變更（請 review 後 commit，依 comm.md 走 branch）

| 檔案 | 狀態 | 說明 |
|---|:-:|---|
| `fw/vendor/fr30xxc_sdk__202411/.../tools/post_process.py` | gitignored（SDK）| **CHIP_TYPE 還原 FR509x**（2026-10-21）|
| `fw/vendor/.../ble_simple_periphreal/Src/main.c` | gitignored（SDK）| **Patch #8**：註解掉 `system_prevent_sleep_clear(SYSTEM_PREVENT_SLEEP_TYPE_DISABLE)`，開發版不睡眠，保住 SWD |
| `fw/tools/evb/*.py` | untracked | 燒錄/救援/UART 腳本（見 §6）|
| `docs/03_hardware/FR3068EP-C Dev1.4/` | untracked | EVB 照片 IMG_5516~5519 |
| 本檔 | untracked | 交接文件 |

**SDK 本體被 .gitignore**，所以 Patch #5 修正與 Patch #8 **只存在本機**，必須寫進 [`fw/vendor/PATCHES.md`](vendor/PATCHES.md) 才不會遺失。

---

## 5. TODO（依優先序）

1. **更新 `fw/vendor/PATCHES.md`**：
   - Patch #5：標註「CHIP_TYPE 部分已撤回（2026-10-21），FR303x 錯誤導致開機卡死；正確為 FR509x」，保留 os.path.join 部分
   - 新增 Patch #8（sleep disable for EVB dev build，含原因：DAPLink 無 nRESET）
2. **寫 `docs/03_hardware/FR3068EP-C Dev1.4/README.md`**：§2 的 EVB 硬體重點 + 照片引用 + 燒錄/救援步驟
3. **修正 `docs/03_hardware/sch_analysis/01_MCU_FR3068E-C.md`**：SWCLK/SWDIO = PB10/PB11（不是 PC1/PC2）
4. **更新 `fw/REVIEW_REPORT.md`**（Phase 1b addendum）與 `docs/00_project/11_project_bring_up_status.md`（Hardware(EVB) 進度、Category 8 burn header 結論改為 FR509x）
5. **整理 `fw/tools/evb/` 腳本**：路徑目前寫死 `/Users/stanley/...`，改成相對 repo 根目錄；加 README / `--help`；`fr3068_flash.py` 已含「燒前清 DEMCR」邏輯
6. Commit（建議 branch `v0.2.1_evb_bringup`），push
7. **之後的 bring-up**（每項先寫 IMPLEMENTATION_PLAN 給 Stanley 批准）：
   - 補 jumper 讓 UART + SWD 同時可用，確認 921600 app log（`FR5090: ble_simple_peripheral.`）
   - GPIO/LED（K1-K3、D1/D3）→ CAN（EVB 有 transceiver）→ I²C 外接模組
   - 驗證 Patch #7a（96 MHz）下 BLE 長時間穩定、Patch #7b guard 不誤觸發
8. 仍待 Quectel：`100_questions` V1.3 **尚未寄出**；Q50（SDK 192 vs datasheet 156 MHz）尚未加入

---

## 6. 腳本用法（`fw/tools/evb/`，需 `pip3 install --user pyserial pyocd`）

| 腳本 | 用途 |
|---|---|
| `uart_listen.py <port> <baud> <sec>` | 被動聽 UART（例：`/dev/cu.usbmodem2024_2425_62851 115200 20`）|
| `swd_probe.py` | 唯讀 attach：DP/AP ID、CPUID、peek flash/SRAM |
| `swd_retry.py` | 唯讀 attach 重試 5 次（判斷是否 WAIT ACK 卡死）|
| `fr3068_flash.py` | 正常燒錄：`Project_burn.bin` → 0x08000000，verify，清 DEMCR，reset，回報 PC |
| `rescue_and_flash.py` | **救援**：等使用者按放 RST 一次 → 4 ms 內 halt → 不斷線燒錄 → verify → reset |

pyOCD 執行檔在 `~/Library/Python/3.9/bin/pyocd`。Toolchain PATH 已寫入 `~/.zshrc`。

Build：
```bash
cd fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC
make            # 產出 output/Project_burn.bin（FR509x header：word7=0x51525251, word8=0, word11=0x51525251）
```
**燒錄前務必確認 header word8 = 0x00000000**（FR509x 格式），否則會再次卡死。
