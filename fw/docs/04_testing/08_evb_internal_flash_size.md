# DEV1.4 內建 Flash 容量實測

日期：2026-10-21；Branch：v0.8.0_ota_nor_spec；EVB：FR3068**EP**-C DEV1.4，板上韌體為 0.7.0-dev（App GATT manager）。

本輪恢復既有實測報告及其[唯讀 probe](../../tools/evb/flash_size_probe.py)／[sector 測試工具](../../tools/evb/flash_sector_test.py)，未重新執行任何硬體操作；以下量測為原紀錄，不代表本輪新增實測或產品 E-C 驗收。

## 授權與範圍

Stanley 要求確認 SDK linker 的 1016 KiB app window 是否就是晶片內建 Flash 全部容量。先做唯讀 probe；寫入測試經 Stanley 明確同意後執行。只動空白 sector 0x081F7000，不改 SDK、linker、FlashDB 位址或板上 app image。

## 方法

1. **Alias probe（唯讀）**：SWD attach，不 halt／reset。讀 0x08000000 起 6 個取樣點各 256 bytes（boot header、app vector、app code、0x08040000、FlashDB 0x080C8000、0x080F0000），再讀同 offset 加 1／2／4／8 MiB 的位址比對。若某 window 出現與 base 相同的非 0xFF 內容，即位址在該處 wrap。
2. **Sector 寫入測試**：pyOCD + SDK `FR30xx.FLM`，halt 後在 0x081F7000 以 page program 寫入 4 KiB 固定亂數 pattern，readback 比對；確認下半同 offset 0x080F7000 與 8 KiB boot header 未變；抹回 0xFF；清 DEMCR 後 SYSRESETREQ。

## 結果

| 位址範圍 | 讀到 | 判讀 |
|---|---|---|
| 0x08000000–0x080F0000 | header、app、FlashDB（`FDB0` magic @0x080C8000） | 現行 image |
| **0x08100000–0x081F0000** | **全部 0xFF，與 base 不同** | **獨立、未寫入的上半 1 MiB，不是 alias** |
| 0x08200000 起 | 與 0x08000000 逐點相同 | 2 MiB 處 wrap |
| 0x08400000、0x08800000 | 同上 | wrap |

| Sector 測試（0x081F7000） | 結果 |
|---|---|
| 測前 | 全 0xFF |
| Program 4 KiB | 0.83 s；readback SHA-256 一致 |
| 0x080F7000（下半同 offset） | 未變 |
| Boot header 8 KiB | 未變 |
| 0x083F7000（+2 MiB 鏡像） | 顯示同一 pattern，再次證明 2 MiB wrap |
| Erase-back | 0.34 s；全 0xFF |
| Reset 後 | PC=0x0800522A（app flash），RUNNING |

**結論：DEV1.4 EVB 的 FR3068EP-C 內建 Flash 為 2 MiB（0x08000000–0x081FFFFF），全區可讀；上半 1 MiB 已實測可 erase／program。** SDK linker `FLASH 1016K` 只是 example 設定，不是硬體上限。

## 限制

- 只測一個 sector，且是 debugger + FLM 寫入；app 在 XIP 執行中寫上半區（SDK flash driver）尚未測。FlashDB 已在下半區同樣方式運作，同一顆 Flash，風險低但未證明。
- 未讀 JEDEC ID：XIP 執行中操作 QSPI controller 有風險，本次不做。
- EVB 是 EP-C；ET-100 用 E-C，內建 Flash 顆粒可能不同（SDK 註解列 P25Q32、XM25LU32、GT25Q16A 等）。**ET-100 仍需 Q48 或實板重跑兩個工具確認。**
- 實測確認現行 FlashDB 在 0x080C8000、12 KiB（SDK `fal_cfg.h` 預設），位於 linker app window 內且未被保留；app 超過約 800 KiB 會覆蓋設定。見 [開發計畫 §5.1](../01_firmware/01_firmware_development_plan.md)。

## 重現

**安全提醒**：預設 0x081F7000 在新 NOR installer 候選布局中屬 Settings 區，不是永久 scratch。僅能在仍為舊布局、該 sector 經核准且確實空白時測試；新 map 落地後須另選獲准 scratch，不能沿用此預設。本輪未執行工具；寫入／erase／reset 均須另取 Stanley 授權，見[工具安全規則](../../tools/evb/README.md)。

```bash
python3 fw/tools/evb/flash_size_probe.py                       # 唯讀
python3 fw/tools/evb/flash_sector_test.py --address 0x081F7000 # 需 Stanley 同意
```

本次結果由 scratchpad 版本腳本取得；移入 `fw/tools/evb/` 後的 `flash_size_probe.py` 已在同一 EVB 重跑，結果相同。`flash_sector_test.py` 邏輯相同，僅重跑參數防呆（拒絕非上半區／未對齊位址），未再次寫入硬體。
