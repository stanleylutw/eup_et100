# 16 — EVB Sensor package 存到 NOR（SMP image 1）

日期：2026-10-23；分支 `v0.8.8_sensor_package_nor`，base `9af7210`。實作：Claude Code；待 Codex review。測試依 Stanley 要求精簡。

## 結果摘要（M-PKG PASS）

第一輪 EVB 測試用的是兩項修正之前的 build；最新修正版的複測結果見下方「複測（最新修正版）」。

測試 package：Stanley 提供的 `eup_aiot/release/v3.6.14/p1_dfu_v3.6.14.zip`，以 `eup_gateway/tools/sensor_ota_package.py build` 打包（167312 B＝header 256＋image 167056；upload SHA-256 `DA19F5AC…5063`）。

| # | 驗收（施工單 §4） | 結果 |
|---|---|---|
| 1 | Host 測試 | ✅ 見下 |
| 2 | 上傳 → `GSOTAPKG` VALID | ✅ `smp_serial_ota.py --baud 115200 upload --image 1` 30.8 s（最後一包含驗證約 4 s）；`GSOTAPKG;1;VALID;1;167312;167056;EUP_AIoT_P1;p1_nrf52832;nrf52832;v3.6.14;DA19F5AC…5063;20261017223216`，與檔案一致。上傳前為 `EMPTY;0` |
| 3 | 重開機後仍 VALID；刪除後 EMPTY | ✅ SMP reset 後同上 VALID；`DSOTAPKG` 1.8 s 後 `1;OK`，期間 `GSOTAPKG;0;BUSY`，之後 `EMPTY;0` |
| 4 | ET-100 自身更新不受影響、B 資訊段 | ✅ package VALID 時以 image 0 更新 0.8.8 → 0.8.81（test → reset → 自動確認），之後 package 仍 VALID；最後還原 0.8.8-dev 並重新上傳 package（VALID）。B-info `ad7facb2…` 不變，CFSR 0，OTA worker stack 最低剩 1916 B（4 KB） |

第一輪結束時板況：0.8.8-dev（`471f5314…`）已確認，屬修正前版本。

## 複測（最新修正版，2026-10-23 00:20–00:35，Codex 執行）

部署 build：`fw/build_serial/output/` 10/23 00:10，含續傳修正與 FlashDB key 修正；`dfu_application.zip` SHA-256 `d05af517…`，`et100.signed.bin` `7c69980c…`，MCUboot hash `d85fae0e…`。工具：`smp_serial_ota.py` 加測試腳本，未使用 HTML。

| # | 項目 | 結果 |
|---|---|---|
| 部署 | image 0：0.8.8（`471f5314…`）→ 最新 0.8.8（`d85fae0e…`） | ✅ 上傳 54.5 s，TRIAL 開機後自動確認 |
| 2 | 上傳 P1 package → VALID | ✅ 167312 B，31.3 s；`GSOTAPKG` VALID，長度／product／board／soc／firmware／SHA `DA19F5AC…5063`／build id 與檔案一致（generation 2：板上原本已有 generation 1 的 package） |
| 3 | 重開機仍 VALID；刪除後 EMPTY | ✅ SMP reset 後 VALID；`DSOTAPKG` 約 2 s 回 `1;OK`，之後 `EMPTY;0` |
| 4 | 自身更新不影響 package | ✅ 重新上傳 package（VALID，generation 1）後，image 0 更新到 0.8.81（`754eaed5…`），自動確認後 package 欄位與更新前完全一致；最後還原為最新 0.8.8-dev（`d85fae0e…`）並自動確認 |
| auto 清單 | FlashDB key 修正的實機驗證 | ✅ 部署前 `GAUTOLIST` 為空（可能已被舊 bug 清掉，也可能原本就沒設定），測試前以 `SAUTOGATT` 設定 `C2DEC0DE0002;C2DEC0DE0001`。之後三次 TRIAL 更新（部署、0.8.81、還原）與 reset 後均保留 |
| B-info | SWD 唯讀讀回 | ✅ 7 次均為 `ad7facb2…92ca7`；CFSR／HFSR 均為 0 |

最終板況：最新 0.8.8-dev（`d85fae0e…`）已確認，UART 9600；NOR sensor 區有 P1 v3.6.14 package（VALID，generation 1）；auto 清單兩筆。

通訊異常：以下三次均出現在測試程式端，重新以單一程序查詢都通過，原因未確定。

- `SMPBadSequence('Bad sequence 21, expected 0')`：發生在第 3 項 SMP reset 之後約 15 s。推測是 reset 前殘留的 SMP 回應被新連線讀到。之後 `GCHK` 正常，裝置維持確認狀態。
- `GSOTAPKG` 逾時：指令在 00:33:26 送出，00:33:30 同時進行 SWD 讀回。推測兩者重疊，SWD 存取影響了 UART 回應，尚未證實。
- 第二個程序在 00:33:33 開同一個 port，被 UART 鎖拒絕。這是測試腳本兩個程序重疊，不是韌體問題。

原始紀錄：`/tmp/et100_v088_recheck/hardware.log`、`host.log`（/tmp 不保證保留）。

## Host 測試

| 測試 | 內容 |
|---|---|
| `fw/tests/test_smp.c`（`-DET100_SMP_BLE_HOST_TEST`，ASan／UBSan） | 新增 `test_sensor`：VALID 與欄位、不列為 slot 1、重開機持久、ET-100 自身更新後仍 VALID 且 NOR bytes 不變、PENDING 時拒收、header CRC 錯／image 與 header SHA 不符 → ECORRUPT＋INVALID（generation 遞增）、upload SHA 與 CRC 正確但 MCUboot SHA-256 TLV 與 header 不符／TLV info 損毀 → ECORRUPT（以移除 TLV 比對的 mutation 確認會失敗）、> 225280 與 image 2 → EINVAL、剛好 225280 B、上傳中斷＋重開機從 4 KB 位置續傳、他方 transport 持有時刪除 EBUSY、刪除後 EMPTY／gen 0／256 KB 全 FF／journal session 取消、無 KVDB → ENOTSUP、記錄損毀 → INVALID |
| `fw/tests/test_serial_gchk.c` | `GSOTAPKG` EMPTY／INVALID／VALID（含無 build id 回 `-`、欄位填滿不越界）／BUSY／READ_FAILED／UNSUPPORTED；`DSOTAPKG` 延後回覆 OK／BUSY／DELETE_FAILED |
| 實際 package | 移植的 `sensor_pkg_validate` 對 gateway 工具產生的 p1 v3.6.14 package 回 0；改 1 byte 回 EBADMSG |
| `fw/tests/test_boot_health.c` | TRIAL 開機的 FlashEnv 探針只寫 `0x00030006`，100 B auto 清單與 GTIME 不變；所有專案 key 互不相同；探針通過後仍需 ADV＋30 s 才 confirm |
| `fw/tools/test_evb_serial_host.sh` | PASS；已納入 `test_smp` 與 `test_boot_health`（`test_smp` 需要先 build 出 `et100.signed.bin`，找不到時直接失敗） |
| ARM build | 無 warning（SDK 既有 RWX 除外） |

## 自查修正（EVB 測試後）

- 重開機若剛好發生在最後一個 4 KB chunk 已寫入 NOR、journal 尚未 finish 時，重送第一包會回 `off=len` 但記錄停在 INVALID。已改為：續傳時若資料已完整（VERIFIED 或已 commit 全長），先 finish＋驗證＋commit 再回覆；`test_smp` 新增此案例。修正版已在 EVB 複測第 2–4 項 PASS（見上），但「最後一個 chunk 寫完、尚未 finish 時重開機」這個時機仍只有 host 測試覆蓋。

## 備註／限制

- `GSOTAPKG` 只讀內建 Settings 記錄（VALID 時保存驗證過的 header），不像 gateway 每次再讀 flash header 比對；NOR 由 OTA worker 獨占，避免跨 task 存取。
- 與施工單 D5 的差異：沿用 journal 後，重開機後也能續傳（比承諾的多）。
- FlashDB key 衝突已修正：`boot_health.c` 的 TRIAL 探針原本和 `gatt_client.c` 的 auto 清單共用 `0x00030004`，每次 OTA 後的 TRIAL 開機都會清掉 auto 清單（v0.8.4 起的既有問題）。現在探針改用 `0x00030006`，所有專案 key 集中在 `fw/src/et100_fdb_keys.h`（GTIME `…02`、CID `…03`、auto 清單 `…04`、sensor package `…05`、boot health `…06`）。已經過 TRIAL 開機的舊裝置，auto 清單需要重新設定一次。
- 驗證範圍與 gateway 相同（D3）：檢查 header CRC、upload SHA-256，以及 MCUboot SHA-256 TLV 與 header 一致；**不驗 MCUboot 簽章**，也不重新計算 image 的 MCUboot hash。不可稱為「已驗證簽章」。
- Relay（推給 sensor）未做。HTML（D8）已由 Codex 完成，離線測試 `test_et100_sensor／smp／ui.cjs` PASS；瀏覽器打包結果與 gateway Python 工具逐 byte 相同（167312 B，`f0dbaaf6…`）。本輪 EVB 複測未使用 HTML。
