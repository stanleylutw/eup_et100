# 15 — EVB 裝置資訊查詢（GET_FW_INFO／GET_DEVICE_ID）

日期：2026-10-22；分支 `v0.8.7_device_metadata`，base `6817f6c`。實作：Claude Code；待 Codex review。測試依 Stanley 要求精簡。

## 結果摘要（M-META PASS）

| # | 驗收（施工單 §3） | 結果 |
|---|---|---|
| 1 | Host：回應能被 FR3068 既有 client parser 解析 | ✅ request 由 `eup_aiot_uuid_client` 產生，回應的每個 part 經 `eup_uuid_parse_response` 檢查、串回後由 `eup_aiot_metadata_parse` 解出 project／hardware／firmware／soc／schema／protocol、device_id `C03232325903`、capabilities `0x23`。GET_FW_INFO 171 B：MTU 247 為 1 part、MTU 23 為 18 parts |
| 1b | 錯誤處理（照 gateway） | ✅ 壞 checksum／短封包／錯 magic → `0x02`；錯版本 → `0x05`；未知指令 → `0x01`；EUPFWV1 欄位填滿無結尾時不越界 |
| 2 | EVB：`GFWINFO` | ✅ `AT$PD=BLEGW$GFWINFO;1;EUP_ET100;ET100_EVB_DEV1.4;0.8.7-dev;fr3068e_c;M087` |
| 3 | EVB：BLE service 已註冊 | ✅ SWD 讀三個 service id：App manager 80、metadata 82、SMP 83（皆非 0xff）；`et100_meta_dropped`=0。手機 PING 為選做，未執行 |
| 4 | SMP 更新流程、B 資訊段 | ✅ 0.8.7 以 SMP serial 部署並自動確認；B-info `ad7facb2…` 不變 |

## Host 測試

`fw/tests/test_eup_meta.c`（已加入 `fw/tools/test_evb_serial_host.sh`）、`test_serial_gchk.c` 新增 GFWINFO 案例、`test_app_gatt_server.c` 加上 metadata service hook stub；全部 PASS（ASan／UBSan）。ARM build 無 warning（SDK 既有 RWX 除外）。

## Codex Step 5 review 修正（2 項 P2）

- 2 格 queue 取代 `pending`，最後一個 part 送出即釋放；poll／callback／gap handler 全程 critical，斷線重連清空 queue；`smp_ble_send()` 同步改為原子送出。詳見 `fw/REVIEW_REPORT.md`。
- Codex 重現程式改寫後兩項皆通過；host 測試 PASS；ARM build 無 warning。
- EVB 重新部署 ✅：板上原為 Codex review 留下的測試 image `0.8.63`（停在 115200、無 GFWINFO），直接以 115200 SMP 上傳修正版（298184 B，53.6 s）→ test → reset → 自檢 30 s 後自動 confirm；slot 0 `0.8.7.0` hash `7f1cb09c…` active confirmed。`GFWINFO` 9600 回 `…;0.8.7-dev;fr3068e_c;M087`；CFSR 0、`et100_meta_dropped` 0；B-info `ad7facb2…` 不變。
- Codex 複審 PASS。手機 BLE 實機複測（連續 PING、斷線重連後查詢、BLE SMP 更新）依 Stanley 決定未做；修正以 host 重現程式驗證。

## HTML 工具驗收（`tools/rs232_tester.html` ET-100 SMP，Codex 實作）

Stanley 以 Chrome 在 EVB 實測（log `rs232_log_20261022_152526.txt`），測試 zip 由 Claude 準備：

| # | 檔案 | 結果 |
|---|---|---|
| 1 | `0.8.72-dev`，EUPFWV1 hardware 改 `ET100_EVB_DEV9.9` | ✅ GFWINFO 比對擋下：「裝置／檔案硬體不符」，未切 baud、未上傳 |
| 2 | `0.8.71-dev` 正常 | ✅ OTABAUD 115200 → 上傳 53 s → 驗證 → Test／重啟 → 自動確認；GCHK／GFWINFO 回 `0.8.71-dev` |
| 3 | `0.8.7-dev`（還原） | ✅ 同流程，回 `0.8.7-dev` |

B-info `ad7facb2…` 不變。`tools/tests/test_et100_smp.cjs`、`test_et100_ui.cjs` PASS（含 180 s 確認逾時、ASCII 逾時提示、pre-0.8.7 警告）。`tools/vendor/README.md` 已附 fflate MIT 授權說明。GLIST／GMAXRPT／GSOTAPKG 回 0 為 HTML 既有 gateway 查詢，ET-100 未實作，不影響更新。

## 自家 iOS App 更新驗收（2026-10-22）

第一次以 App 更新（0.8.7 → 0.8.71）：metadata 檢查全過，上傳完成後在最後一步失敗 `Not supported (RC: 8)`。App log（ios_app_log 27）顯示 App 用 nRF 函式庫預設的 **「Confirm only」** 模式，上傳後送 `state write {hash: 新 image, confirm: true}`；韌體原本「不做永久安裝」一律回 ENOTSUP。

修正（`ota_manager.c`）：confirm 新 image 時與 test 相同排入 PENDING，回報 `pending／permanent`；重開機後照常備份、TRIAL、自檢 30 s 自動確認、失敗 rollback，只在 CONFIRMED 狀態接受。同時把 upload 的 `upgrade` 旗標改依 MCUmgr 規格實作（只接受較新版本；原誤解為永久安裝，與此次失敗無關）。`test_smp` 新增兩者案例，ALL PASS。

複測（ios_app_log 28）✅：Confirm only 上傳 56 s → confirm 回 OK →「Upgrade complete」→ reset → App 重連讀到 `fw=0.8.71-dev`；SMP 確認 slot 0 `0.8.71.0` active confirmed。之後以 SMP serial 還原 0.8.7-dev（中途 Chrome HTML 分頁自動搶 port 造成一次逾時，關分頁後續傳完成），slot 0 `5e001460…` confirmed，CFSR 0，B-info `ad7facb2…` 不變。App 的「版本無法比較」警告為 App 端無法比較 `-dev` 版本字串，留待 App 修改。

## 限制

- 未以手機實際送 PING／GET_FW_INFO；格式相容性以 FR3068 既有、已與真實 nRF sensor 互通的 client parser 驗證。
- 與 gateway 相同，回應分 part 時以 byte 切分（不保證不拆 TLV）；App 需串回全部 part 再解析。
