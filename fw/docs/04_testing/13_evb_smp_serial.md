# 13 — EVB SMP over serial（RS232 入口）

日期：2026-10-22；分支 `v0.8.5_ota_manager_smp_serial`，base `182782f`。
**實作與測試：Claude Code（comm.md 備用規則，Codex 無 token）。未經獨立 review**，Codex 恢復後補 Step 5。測試依 Stanley 要求精簡。

## 結果摘要（M-SMP PASS）

| # | 驗收（施工單 §5） | 結果 |
|---|---|---|
| 1 | `smpclient` 經 UART3 讀 image state | ✅ slot 0 版本、MCUboot hash、active／confirmed 正確 |
| 2 | 上傳 → test → reset → installer 安裝 → 新 App 自動 confirm | ✅ A→B、B→A、A→B、B→A 共 4 次；後 2 次全程只用 SMP（含 SMP reset） |
| 3 | 上傳途中中斷 host → 續傳 | ✅ 9600 上傳到 30% 時強制結束 client；重跑從 `off=90112`（22 × 4 KB）續傳完成 |
| 4 | 上傳期間 GCHK 仍可回應、UART 不掉 byte | ✅ SMP 與 GCHK 交錯使用正常；每次安裝由 installer 驗整份 SHA-256 與 MCUboot TLV（任何掉 byte 都會被拒） |
| 5 | OTABAUD 115200；閒置自動回 9600 | ✅ 293 KB 約 53 秒（9600 約 8 分鐘）；閒置 35 秒後 9600 GCHK 正常 |
| 6 | B 資訊段不變 | ✅ 最後 SWD 讀回 SHA-256 `ad7facb2…92ca7`（與基準相同） |

最終板況：**0.8.5-dev（修正版 A）已確認**，是本 branch 的正常 build（`fw/build_serial/output`）。

## 實機發現並修正的問題

| 問題 | 原因 | 修正 |
|---|---|---|
| App 內含兩個 `EUPFWV1` magic，DFU 打包失敗 | installer 的 `candidate.c` 被連結進 App，其比對用字串常數也是 `"EUPFWV1\0"` | 改用拆開的常數（`'E'`＋`"UPFWV1"`），App 只剩真正的 metadata block。installer 若遇到也會拒絕這種 image，打包檢查擋在前面 |
| SMP reset 回 ENOTSUP | OS reset 的 command ID 是 **5**（Zephyr `OS_MGMT_ID_RESET`），原本寫成 1；host test 用了同一個錯誤 ID | 改成 5，host test 加入 ID 1 必須被拒的檢查 |
| slot 0 在 Candidate pending 時顯示未確認 | `confirmed` 只在 boot state 為 CONFIRMED 時為真 | 改為「非 TRIAL 即已確認」；host test 驗證 pending 時 slot 0 仍 confirmed |
| client 被強制結束後，下一次第一個請求被吃掉（2 秒逾時後恢復） | 裝置 decoder 停在半行，新封包的位元組被當成舊行內容 | 行中遇到 `0x06`／`0x04`（不可能出現在 base64）即丟棄舊行重新開始；host test 覆蓋 |

前 2 次安裝用的是修正前版本，SMP reset 不可用，因此當時以 SWD reset 代替；修正後的 2 次（A→B、B→A）完全以 SMP 完成。

## 速度

| Baud | 293 KB 上傳 | 說明 |
|---|---|---|
| 9600 | 約 8 分鐘（約 630 B/s） | base64 與 framing 約多 35% |
| 115200 | 約 53 秒 | 含 NOR 寫入（STIG 8-byte PP） |

另需安裝時間：installer 備份＋安裝約 1 分鐘內，新 App 自檢 30 秒後自動確認。

## Host 測試

| 測試 | 結果 |
|---|---|
| `fw/tests/test_smp.c`（ASan／UBSan） | PASS：CBOR 子集（含 indefinite map、截斷拒絕）；framing（8–1024 B、ASCII 交錯、CRC 錯誤、無 start 的續行、半行後重新開始）；SMP server（echo、reset=5、params、未知指令）；OTA manager（分段上傳、模擬重開續傳、offset 重新同步、test→PENDING、冪等、pending 時拒絕上傳、TRIAL confirm、永久安裝拒絕、壞 image 不列出且無法 test） |
| `fw/tests/test_smp_interop.py` | PASS：官方 `smp` 套件編碼 ↔ 韌體解碼／編碼 ↔ `smp` 解碼，11 種長度逐 byte 一致，行長 ≤ 127 |
| `fw/tools/test_evb_serial_host.sh` | PASS（新增 OTABAUD 案例） |
| installer 測試（含 `candidate.c` 修改） | PASS：metadata 規則與 3,663 次斷電注入 |
| ARM build | App 0.8.5-dev PASS（text 257 KB）；installer PASS（stub 39 KB） |

重現：

```bash
SDK=fw/vendor/fr30xxc_sdk__202411
cc -std=c11 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined \
  -Ifw/src -Ifw/tests -Ifw/common -Ifw/installer -Ifw/include -I$SDK/components/modules/mbedtls/include \
  fw/tests/test_smp.c fw/src/{cbor_min,smp_serial,smp_server,ota_manager,ota_journal}.c fw/tests/fake_nor.c \
  fw/common/boot_state.c fw/installer/{mcuboot_image,candidate}.c \
  $SDK/components/modules/mbedtls/library/{sha256,platform_util}.c -o /tmp/et100_test_smp
/tmp/et100_test_smp fw/build_serial/output/et100.signed.bin
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Ifw/src \
  fw/tests/smp_serial_echo.c fw/src/smp_serial.c -o /tmp/et100_smp_echo
fw/.venv/bin/python fw/tests/test_smp_interop.py /tmp/et100_smp_echo
```

## 與施工單的差異

- **OTA manager 沒有開獨立 task**：SMP 是一問一答，client 等回應才送下一包，所以在 serial task 內處理不會掉 byte；BLE（v0.8.6）的 callback 不能阻塞，屆時再改為 worker task。
- `smpmgr` 需要 Python ≥ 3.10，改用其底層的 `smpclient` 寫 `fw/tools/evb/smp_serial_ota.py`；`smpclient` 的 BLE 依賴（bleak／pyobjc）無法在本機編譯，以 `--no-deps` 安裝 serial 所需套件（`fw/tools/requirements-smp.txt`）。
- `installer/load_image.py`（v0.8.3 工具）仍期望安裝後為 `kind=2`，v0.8.4 起為 TRIAL（`kind=4`）；本次部署第一版 0.8.5 時腳本誤判失敗，實際安裝成功。該工具之後由 SMP 取代，未修改。

## Codex review 後修正（2026-10-22）

Codex 獨立 review 提出 3 項：半包後 GCHK 被吃掉、最大版本字串溢位、`off` 規格用詞錯誤，均已修正，詳見 REVIEW_REPORT。另「OTA manager 在 serial task 內執行時，NOR／驗證期間 GCHK 會延遲」屬實，已在 v0.8.6 改為 worker task。

## 限制

- 未驗證簽章（Q42），仍為原型。
- 未測 9600 下的 OTABAUD 失敗情境（例如 host 沒切 baud）；裝置 30 秒後自動回 9600。
- Logs：`/tmp/et100_v085_*.log`。
