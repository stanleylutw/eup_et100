# 17 — Sensor OTA relay 接手驗證

日期：2026-10-23；分支 `v0.8.9_sensor_ota_relay`，base `cc78df0`。實作 Claude Code；Codex 接手 review、修正、build／打包及文件。Stanley 自行進行硬體驗收。

## 結論

**Host／ARM／DFU PASS，M-R1／M-R2 未執行。** 本輪沒有 UART、SWD、燒錄、NOR 寫入或 Sensor 操作，不標示實機 milestone PASS，也沒有 commit／push。

| 項目 | 結果 |
|---|---|
| 完整 serial host suite（ASan／UBSan，含既有 GATT／boot health／SMP） | PASS |
| Relay fake Sensor | 完整更新、MTU 23 分段、相同映像跳過更新、斷線續傳、取消、auth／target／SMP／version failure、package 變更、指令格式 PASS |
| Review 修正 | 格式錯誤的 SMP image state 原本和 active hash 不同共用 EBADMSG，可能繼續 erase；現在格式錯誤轉為 protocol error。新增 fake malformed-state case，驗證 erase／upload／reset 次數均為 0，PASS |
| Review Minor 修正 | GSENSORINFO 在 relay session 內及兩個 session 之間回 BUSY；內部 metadata 仍能讀取，reservation 釋放後快取查詢恢復。Host 三情境覆蓋 |
| NOR owner | manager host 測試確認 relay 期間 image 0／1 及 delete 回 BUSY；讀取邊界 PASS。RTOS 排程、SDK TX backpressure 的實機行為仍待驗證 |
| ARM GCC | PASS；僅既有 LOAD segment RWX warning |
| DFU | 0.8.9-dev，MCUboot 0.8.9.0；SHA-256-only 開發原型，未驗簽章 |
| M-R1／M-R2 | 待 Stanley；MTU／吞吐／真實續傳、取消、B-info、auto 清單、scan 恢復均未量測 |

## 產物與重建

本 worktree 的 `fw/.venv` 是主工作區 venv 的 symlink，原打包腳本以 resolved prefix 檢查路徑。使用主工作區的既有腳本處理 **本 worktree** 的 Project.bin，避免修改共享 venv、SDK 或放寬檢查：

```sh
# 在本 worktree 執行；非零參數讓 wrapper 不自動進行其本地 prefix 檢查失敗的打包步驟。
bash fw/tools/build_evb_serial.sh ET100_TEST_FLAGS=-fstack-usage
/Users/stanley/Firmware/EUP/eup_et100/fw/.venv/bin/python \
  /Users/stanley/Firmware/EUP/eup_et100/fw/tools/make_dfu_package.py \
  fw/build_serial/output/Project.bin
bash fw/tools/test_evb_serial_host.sh
```

- `fw/build_serial/output/et100.signed.bin`：313024 B，SHA-256 `2cf4da49cba3f5eb9be439472976a73d5c6f591f3570a417ba74eb70df74cde4`。
- `fw/build_serial/output/dfu_application.zip`：SHA-256 `aab7b8f261f7e0dccfbf1e79c704b40ab06b0c4a0c0b57b10968b14ea2ac54f8`；主工作區交付副本為 `fw/build_serial/output/et100_0.8.9-dev_dfu_application.zip`。這是含 GSENSORINFO BUSY 修正的新 build，取代先前 fa7b2388 開頭的 ZIP，版本字串不變。
- ELF：text 276656 B、data 35808 B、bss 84960 B；不可把 section 加總直接解讀成完整 runtime SRAM 餘量。
- GATT task stack 配置 1536 words（6144 B）；GCC `.su`：session 120 B、upload_run 272 B、verify_target_firmware 384 B、SMP state_request 184 B、upload 112 B、send_request 56 B。這些是個別 frame，不是完整 call-chain 高水位；SDK／libc、IRQ 及真機峰值仍待測。
- 完整 GATT setup／AES 要求 MTU ≥35（請求 247）；MTU 23 的 fake-Sensor 測試只涵蓋 SMP framing，不涵蓋真實 GATT setup／認證。
- 日誌：`/tmp/et100_v089_build.log`、`/tmp/et100_v089_package.log`、`/tmp/et100_v089_host.log`，/tmp 非永久證據儲存區。

## 限制與接續

### HTML D9（Codex，Stanley 已授權）

- 主工作區的既有未提交 UI 修改保留；完成後僅同步 `tools/rs232_tester.html` 與 `tools/tests/test_et100_ui.cjs` 到本 worktree，沒有切換主工作區 branch 或移動其硬體檔案。
- GCHK 0.8.9 以上啟用 ET-100 Device／GATT OTA；重新讀 VALID package，再偵測 GSENSOROTA。沿用 Gateway relay job UI、五個指令、15 步狀態、開始／取消。來源圖改為 EVB NOR 16 MB／Sensor slot 256 KB，不偽裝成 nRF Gateway Flash。
- `node tools/tests/test_et100_smp.cjs`、`test_et100_sensor.cjs`、`test_et100_ui.cjs` PASS。瀏覽器只用 mock Web Serial，未存取 USB。
- UI 覆蓋：舊版停用、新版啟用、VALID／EMPTY、exact MAC 開始、取消、SUCCESS、metadata wire format、query／flow、RS232/GATT 檔案保留、NOR→Sensor 圖及 mobile 無水平溢出。禁止走未實作 probe／ZVERO；套件儲存不會自動啟動 relay。
- UI 測試不是實機 M-R1／M-R2；請依 [06 協定的 HTML 操作步驟](../02_protocol/06_sensor_ota_relay.md)驗收。

- 未增加 SDK patch；`fw/.venv`、`fw/vendor/fr30xxc_sdk__202411` symlink 不可提交。
- v0.8.8 獨立 review 的既有 gate 仍未被本次 relay 審查取代。
- metadata／package／MCUboot hash 比對不等於驗證數位簽章，量產 gate 不變。
- 使用 [06 relay 協定](../02_protocol/06_sensor_ota_relay.md)的手動步驟。`GSENSOROTAMETA` 只是快照查詢；版本一樣但 hash 不同的 `SENSOROTA` 仍會寫入 Sensor，M-R1 必須事先確定相同映像。
- 完成 M-R1 後再決定 M-R2；收到 Stanley 實機紀錄後更新本報告，不預填 PASS。
