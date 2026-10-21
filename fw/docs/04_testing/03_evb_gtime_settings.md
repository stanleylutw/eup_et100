# DEV1.4 GTIME 設定與保存驗證

日期：2026-10-21
分支：`v0.4.0_evb_gtime_settings`
版本：`0.4.0-dev`

**Stanley 驗收更新**：2026-10-21 Stanley 回覆「OK. it works.」並指示進行 CID／BLE scan；據此記錄本輪手動 GTIME 設定與 reset 保存驗收通過。未提供逐筆 UART log，不把此回覆擴大為斷電／GC／所有邊界情境驗收。

## 已完成 host 驗證

AddressSanitizer／UndefinedBehaviorSanitizer 測試 PASS：既有 parser regression、預設值 15、GGTIME／SGTIME、合法邊界 1／1800、非法參數及 overflow、未初始化 storage 拒絕操作、mock save failure 保留 RAM 舊值、mock durable value 重新載入。這是 host 模擬測試，不代表實體 FlashDB 或 reset 保存已通過。

完整 log：`/tmp/et100_gtime_host_tests.log`。

## Build 與 image

- ARM GCC build PASS；僅原有 RWX LOAD segment warning。完整 log：`/tmp/et100_gtime_build.log`。
- text 225,832、data 35,272、bss 62,640 bytes；靜態 SRAM 97,912 bytes，為 512 KiB linker window 的 18.7%。serial task 另由 RTOS heap 配置 1024-word stack，未量測 high-water。
- `Project.bin` 261,112 bytes；`Project_burn.bin` 269,304 bytes。
- FR509x header、code length、exec offset `0x2000`、payload 與獨立 CRC32 PASS；CRC32 `0x7F326ABA`。
- burn SHA-256：`a7e664ecd173cebf405dbbd16bdc6c46f69585eef6d31b4a72860f624e53a5b3`。
- burn image 末端低於 SDK FlashEnv offset `0xC8000`；未修改 linker、分區、SDK 或 Gateway repo。

## Flash 與硬體驗收

依 Stanley 授權，已透過 DAPLink program 到 `0x08000000`，66 sectors、35.7 秒，逐 byte readback PASS。SYSRESETREQ 後 PC `0x1FFE23DE`、SP `0x20013488`，已 resume；PC 位於 PRAM ram_code。此結果不代替 UART／持久性驗收。完整 log `/tmp/et100_gtime_flash.log`。依 Stanley 指示，Codex 未執行自動 UART／HTML 操作。

Stanley 手動測試清單（每筆包含 CRLF；HTML 會自動產生 token）：

| 步驟 | 請求 | 預期 |
|---|---|---|
| 1 | `BLEGW$GCHK;V001` | 版本 `0.4.0-dev` |
| 2 | `BLEGW$GGTIME;Q001` | 首次無 GTIME record 時 `...;1;15;Q001` |
| 3 | `BLEGW$SGTIME;30;S001` | `AT$PD=BLEGW$SGTIME;1;30;S001` |
| 4 | `BLEGW$GGTIME;Q002` | `AT$PD=BLEGW$GGTIME;1;30;Q002` |
| 5 | 按 RST，等待約 3 秒，再送 `BLEGW$GGTIME;Q003` | 仍為 `...;1;30;Q003` |
| 6 | `BLEGW$SGTIME;0;E001`／`BLEGW$SGTIME;1801;E002` | valid=0；再查 GTIME 仍為 30 |

合法邊界、同值設定、拔插 USB 後保存、斷電／GC 恢復、BLE regression、serial task high-water 尚待硬體驗證。0.4.0-dev 的 GTIME 只保存設定；後續 0.5.0-dev 開始控制 sensor snapshot 周期，MAXRPT setting 尚未實作。後續版本繼續接受目前 schema 1 的 record。
