# DEV1.4 UART3 GCHK 實測紀錄

日期：2026-10-21
分支：`v0.3.0_evb_serial_gchk`

## Host 與 build

- GCC build PASS；完整 log `/tmp/et100_serial_build.log`。唯一 warning 為既有 RWX LOAD segment。
- AddressSanitizer／UndefinedBehaviorSanitizer host tests PASS：拆包、黏包、token、非法輸入、超長行恢復及 100 次 query。
- text 225,288、data 35,272、bss 62,624 bytes；靜態 SRAM 合計 97,896 bytes，占 linker 512 KiB 的 18.7%，不代表 runtime heap 餘量。
- raw image 260,568 bytes；burn image 268,760 bytes。FR509x header、code length、exec offset、payload 與獨立 CRC32 全部 PASS；CRC32 `0x1C9E6ED5`。
- burn SHA-256：`b8b12155c7977fc29205d7a248e7bf752a3ff2dbfbc56c03a147c3002f380907`。
- 舊 SDK baseline 產物 SHA-256 保持 `0710a7e79de7ac2dd6c1f930119f1b82c4400a64a1d1ea76086da9e08be76fc2`，SDK 無新增 patch。

## 燒錄

Stanley 明確授權後，透過 DAPLink 將 `fw/build_serial/output/Project_burn.bin` 寫到 `0x08000000`，66 sectors，35.7 秒。readback 逐 byte PASS；SYSRESETREQ 後 PC `0x08006112`、SP `0x20013CC0`，已 resume。完整 log `/tmp/et100_serial_flash.log`。

## Stanley HTML 測試

2026-10-21 16:29，Stanley 提供原始收發紀錄：

```text
16:29:26.327 TX  BLEGW$GCHK;ZEM0
16:29:26.429 RSP AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.3.0-dev;0;ZEM0
16:29:27.257 TX  BLEGW$GTYPESWITCH;SDDV
16:29:27.328 RSP AT$PD=BLEGW$GTYPESWITCH;0;SDDV
16:29:28.803 TX  BLEGW$GCID;6569
16:29:28.861 RSP AT$PD=BLEGW$GCID;0;6569
16:29:30.048 TX  BLEGW$GGTIME;N2MF
16:29:30.107 RSP AT$PD=BLEGW$GGTIME;0;N2MF
16:29:31.247 TX  BLEGW$GMAXRPT;XU2U
16:29:31.303 RSP AT$PD=BLEGW$GMAXRPT;0;XU2U
16:29:32.236 TX  BLEGW$GLIST;GJ7K
16:29:32.297 RSP AT$PD=BLEGW$GLIST;0;GJ7K
```

結果：GCHK PASS；5 個未支援命令均回覆 `valid=0`，6 筆 token 全部吻合。約 56-102 ms 為 HTML 顯示的 TX 到 RSP 時間，包含傳輸與 host 時間，並非 MCU 純處理延遲。此紀錄未證明 factory unique ID 的唯一性或完整 Gateway 相容性。

## 待驗證

依 Stanley 指示，Codex 負責修改、build、flash，UART／HTML 操作由 Stanley 執行。自動 UART script 未執行；硬體上的連續 100 次、拆包／黏包、overflow recovery、reset reconnect、BLE regression 與長時間測試尚未完成。
