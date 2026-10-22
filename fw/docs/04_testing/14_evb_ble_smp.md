# 14 — EVB BLE SMP（BLE 更新入口）

日期：2026-10-22；分支 `v0.8.6_ble_smp`，base `62ec148`。
**實作：Claude Code（comm.md 備用規則）；手機操作：Stanley。未經獨立 review**，Codex 恢復後補 Step 5。測試依 Stanley 要求精簡。

## 結果摘要（M-BLE PASS）

| # | 驗收（施工單 §3） | 結果 |
|---|---|---|
| 1 | 手機 nRF Connect Device Manager 看得到 image | ✅ 連上 `EUP_AIoT_HUB_FR`，顯示 image 與 hash |
| 2 | 上傳 → Test → Reset → 安裝 → 自動確認 | ✅ 0.8.6 → **0.8.62**：App 自動完成上傳、Test、Resetting、Upload Complete；裝置 slot 0 = 0.8.62.0 confirmed，GCHK 回 `0.8.62-dev` |
| 3 | BLE 上傳中 UART 不能插入更新 | ✅ BLE 上傳期間 UART state read 正常；UART upload 回 **rc=10 EBUSY** |
| 4 | 原有功能、B 資訊段 | ✅ GCHK 正常；B-info SHA-256 `ad7facb2…92ca7` 不變 |
| 5 | 未定義 `ET100_SMP_BLE_OPEN` 的 build 沒有 SMP service | ✅ release build 的 `Project.bin` 不含 SMP service UUID；開發版含 |

手機端 hash（`0x9A95C9…EDC879`、`0x67E47A…700085`）與裝置回報的 slot hash 完全一致，確認 MCUboot image hash 與 Nordic App 相容。

速度：296 KB 上傳約 47–51 秒（5.77–6.27 kB/s，MTU 247）。

最終板況：**0.8.6-dev 已確認**（以 UART SMP 恢復），即本 branch 正常 build。

## 實機發現並修正

| 問題 | 原因 | 修正 |
|---|---|---|
| 第一次上傳完成後 App 顯示「Reset into Firmware Loader (Bootloader Mode) required」，沒有送 Test | nRF Connect Device Manager 以 OS group **bootloader info（command 8）** 決定流程；原本回 ENOTSUP | 回應 `{"bootloader":"MCUboot"}`，`query=mode` 回 `{"mode":3,"no-downgrade":false}`（swap with revert，對應備份＋試運行＋回滾）；host test 覆蓋 |

當次已上傳的 0.8.61 以 UART 下 test／reset 完成安裝並自動確認，證明 BLE 寫入的 image 可正常安裝；修正後第二次（0.8.62）由手機完整走完。

## Host 測試

| 測試 | 結果 |
|---|---|
| `fw/tests/test_smp.c`（`-DET100_SMP_BLE_HOST_TEST`，ASan／UBSan） | PASS：新增 BLE reassembly（244 B 分段、短 header、超出長度、2 秒逾時重來、超過 1024 B）、bootloader info、跨 transport EBUSY／30 秒後接手續傳並記錄 takeover |
| `fw/tests/test_smp_interop.py`、`fw/tools/test_evb_serial_host.sh` | PASS |
| ARM build | 開發版與 release（`ET100_SMP_BLE_FLAGS=`）皆 PASS |

## Codex review 後修正與複測（2026-10-22）

Codex 提出 4 項 P2（BLE 回應綁定連線世代、`set_baud()` 與 worker TX 互斥、notify 逾時後的遲到 completion、跨 transport 接手須重送 manifest），已全部修正，詳見 REVIEW_REPORT。

| 複測 | 結果 |
|---|---|
| 手機 nRF Connect Device Manager（0.8.6 → 0.8.63，Test and Confirm） | ✅ Test → Resetting → Upload Complete；slot 0 = 0.8.63.0 confirmed |
| Worker stack 高水位（`0xA5` 掃描，SWD 讀 `et100_ota_stack_free`） | ✅ 至少 1,916／4,096 bytes 未使用 |
| BLE drops／B-info | ✅ 0／不變 |

## 限制

- **安全**：BLE SMP 無認證、image 無簽章，只用於開發版；量產前須簽章（Q42）＋BLE 認證（06 §6.1）。
- Worker stack 已量測（見上）；FreeRTOS 剩餘 heap 仍未量測。
- BLE 更新期間僅暫停 Type-1 回報，未停止 sensor GATT client 的自動連線（EVB 測試時 auto list 為空）。
- Logs：`/tmp/et100_v086_*.log`；手機截圖由 Stanley 提供（IMG_5533–5535）。
