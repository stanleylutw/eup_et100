# REVIEW_REPORT — Phase 1a: Validate GCC Build Chain

## 2026-10-22 Codex Step 5 獨立 review — v0.8.5 M-SMP（RS232 入口）

**結論：PASS，無 Critical／Major issue。**

- 依 comm.md 備用規則，Codex 補行 Step 5 獨立代碼審查與 host 測試驗證（commit `62ec148`）。
- **四項重點審查結果**：
  1. `ota_manager.c`：
     - `same_manifest()` 續傳判定要求 length、sha256、target 完全吻合；不同 image 自動以 `ota_journal_cancel()` 清理舊 session 後重啟，邏輯正確。
     - `upload()` line 141 嚴格檢查 `state.kind == BOOT_CONFIRMED`，未確認的 trial boot 或 pending 期間一律拒絕（回 `SMP_EBADSTATE`），防止映像損壞。
  2. `smp_serial.c`：
     - 長度與溢位防護完整：`s->line` 滿 128 bytes 即設 `line_bad` 標記丟棄；`b64_decode()` 以剩餘緩衝區空間為界；`raw[0..1]` 長度界限嚴格限制在 `[10, SMP_SERIAL_MAX_PACKET + 2]`；CRC 驗證失敗安全丟棄。
     - 遇到 `0x06`/`0x04` 封包標頭時能立即拋棄前次中斷之殘留半行，實機中斷復原測試通過。
  3. `serial_gchk.c` `set_baud()`：
     - 切換波特率前加入 `vTaskDelay(5ms)` 確保 ACK 完整移出 FIFO；以 `taskENTER_CRITICAL()` 保護重置流程與 re-arm `uart_receive_IT`；30 秒閒置無 SMP 流量自動安全 fallback 回 9600 bps。
  4. Boot state 寫入與並行保護：
     - `ota_manager.c` 的 `image_test()` / `image_confirm()` 均透過 `lock()`/`unlock()` 調用 `taskENTER_CRITICAL()` 寫入 boot state，與 v0.8.4 作法一致。
     - `serial_task` 於 SMP 傳輸期間暫停 Type-1 BLE sensor 自主上報，避免 UART3 封包競爭。
- **測試驗證**：
  - ASan/UBSan host 測試（CBOR 子集、framing、SMP server、OTA manager 斷點續傳／狀態機）全部 PASS。
  - `test_smp_interop.py` 與官方 `smp` 套件 11 種封包長度編解碼互通測試 PASS（0 CRC error, 0 overflow）。
  - EVB 實機驗收報告（[13 報告](docs/04_testing/13_evb_smp_serial.md)）結果確實吻合設計需求。
- **架構觀察（供 v0.8.6 參考）**：
  - v0.8.5 將 OTA manager 置於 serial task 內處理對於嚴格一問一答的 SMP serial 是合理的取捨；在 v0.8.6 引入 BLE SMP 時，因 GATT callback 不可阻塞，已正確規劃並實作獨立 `ota_worker` task。

## 2026-10-22 Codex Step 5 review — v0.8.7（Step 6 由 Claude 修正）

Codex 審查 `8d94fd8`，提出 2 項 P2（並附 host 重現程式），全部確認並修正：

| # | 問題 | 修正 |
|---|---|---|
| 1 | 最後一包 completion 後 `pending` 要等下一輪 poll 才清除，期間正常的接續查詢被丟棄 | `eup_meta_service.c` 改為 2 格 queue（同 gateway）；最後一個 part 交給 `gatt_notification` 後立即釋放該格 |
| 2 | 世代檢查、編碼、取 link、送出、推進 part 非原子，斷線／重連可使舊回應送到新 peer | 整個 poll（檢查→編碼→notify→推進）在 `taskENTER_CRITICAL` 內；RX callback 與 gap handler 也進 critical；connect／disconnect 清空 queue 並 `generation++`；每格記錄提交時的世代，不符即丟棄。identity MAC 改在 `et100_meta_add_service` 預先算好（critical 內不呼叫 SDK）。`smp_ble_send()` 同樣改成檢查＋copy＋notify＋`outstanding++` 一次完成 |
| 註 | 施工單寫「不拆 TLV」，實作以 byte 切分 | 施工單改為「以 byte 切分（可能拆開 TLV，與 eup_gateway 實作相同）」 |

驗證：Codex 重現程式改寫成新結構後（原 injection 點在 encode 內，現已在 critical section 中、目標上不可能被搶佔，改在 callback→poll 之間及 notify→completion 之間注入斷線／重連）：接續 PING `dropped=0`、兩筆連續請求都保留；舊 seq 不會送到新 link，新請求從 part 0 開始完整送出（MTU 23，8 parts）。Host 測試全部 PASS，ARM build 無 warning。

**Codex 複審：PASS**，未發現新的阻擋問題。另驗證 notify 失敗重試、MTU 23 分包、既有 serial host 回歸，ASan／UBSan 無錯誤。修正版已部署 EVB 並自動 confirm（見 15 報告）。

## 2026-10-22 v0.8.7 App 實測修正：Confirm only 模式（Claude 實作＋自審，Stanley 指示不送 Codex）

- 問題：自家 iOS App（nRF 函式庫預設「Confirm only」）上傳後送 confirm(新 image hash)，韌體回 ENOTSUP。
- 修正：`image_confirm()` 對 candidate hash 改走與 test 相同的 `mark_pending()`，回報 permanent；不做永久安裝，備份／TRIAL／自檢確認／rollback 不變；非 CONFIRMED 狀態回 EBADSTATE。另依 MCUmgr 規格實作 upload `upgrade` 旗標（只接受較新版本，否則 EBADSTATE）。
- 驗證：`test_smp` ALL PASS；App 實機 0.8.7 → 0.8.71 成功並自動確認（15 報告）。
- **自審（`cbeab33`）結論：PASS，1 項 P3 已修。**
  - 不會永久安裝：confirm(candidate) 與 test 共用 `mark_pending()`，只寫 PENDING；之後的 installer 備份、TRIAL、自檢確認、rollback 路徑完全未改。TRIAL 中新 image 即 running，hash 相同走原本的 confirm 分支；非 CONFIRMED 一律 EBADSTATE。
  - `pending_confirm`：`ota_manager_init` memset 清零；只在 PENDING 時影響回報；PENDING 期間 upload 被拒、無取消指令，只能經重開機離開，RAM 清除與狀態一致。語意差異（回報 permanent 但仍可 rollback）較 MCUboot 更保守，已記於 04。
  - `upgrade`：在 `begin()` 前檢查，被拒時不動既有 session；續傳重送 off 0 會重查但結果相同。被拒仍會 `claim()` 30 s owner，與其他拒絕路徑一致，可接受。
  - P3（已修）：版本比較原含 build number，與註解宣稱的 Zephyr 預設（`VERSION_CMP_USE_BUILD_NUMBER=n`，只比 major.minor.revision）不符；改為三欄，`test_smp` 新增「只差 build number 不算較新」案例。我們的 image build number 恆為 0，實機行為不變。

## 2026-10-22 v0.8.7 自我檢查（Claude 實作，待 Codex Step 5）

- 範圍：`eup_meta_protocol`（packet／TLV／分 part，host 可測）、`eup_meta_service`（GATT，callback 內建好回應、serial loop 逐 part 送出，沿用 v0.8.6 的 completion 計數與連線世代）、`BLEGW$GFWINFO`、`evb_serial_ble.c` 一律註冊。SDK 未修改。
- 驗收 M-META PASS（[15 報告](docs/04_testing/15_evb_device_metadata.md)）。
- **請 Codex review 時特別看**：與 `eup_gateway/src/service/eup_ota_metadata_service.c` 的逐項行為一致性（錯誤 status、空 payload、分 part 規則、capabilities 值）；`server_callback` 在 `pending` 時直接丟棄新請求（gateway 用 queue 長度 2）是否可接受；identity MAC 推導是否與實際廣播位址一致（目前與 GCHK 同源，但未以手機掃描比對）。

## 2026-10-22 Codex Step 5 review — v0.8.6（Step 6 由 Claude 修正）

Codex 審查 `521b933＋cf3daeb`，提出 4 項 P2，全部確認並修正：

| # | 問題 | 修正 |
|---|---|---|
| 1 | BLE 請求未綁定連線世代，斷線後新 peer 可能收到舊回應 | `ota_worker_submit()` 帶 tag（BLE 為提交時的 `generation`），`smp_ble_send()` 送出前核對，不符即丟棄 |
| 2 | `set_baud()` 未與 worker TX 共用 mutex | `set_baud()` 先取 `tx_lock`，等 `LSR.TEMT`（FIFO＋shift register 清空）再重設 UART，完成後釋放 |
| 3 | Notify 逾時後，晚到的 completion 可能誤完成下一筆 | `in_flight` 改為 `outstanding` 計數：送出前先等所有先前 notify（含逾時者）完成；completion 只在 `outstanding>0` 時遞減；斷線歸零 |
| 4 | Owner 逾時後，另一 transport 只帶 `off／data` 就能續接，略過 manifest 且不記 takeover | 只有建立 session 的 transport 可送續傳包；其他 transport 回 `EBADSTATE`，必須重送第一包（`len`／`sha`），經 `begin()` 核對 manifest 並寫入 journal takeover。Host test 覆蓋 |

**Stack 實測**（回應「無法宣稱餘量足夠」）：SDK `configCHECK_FOR_STACK_OVERFLOW=2` 使 FreeRTOS 以 `0xA5` 預填 stack；worker 每次處理後掃描未被觸及的區段，結果存於 `et100_ota_stack_free`（SWD 讀取）。EVB 115200 完整上傳＋裝置端驗證（最深路徑）後：**至少 1,916／4,096 bytes 未使用**（保守下限，實際峰值約 2.2 KB）。另將 `smp_serial_encode` 的 1 KB 暫存改為 static（唯一呼叫者為 worker）。

**驗證**：host（含新增 fix 4 案例）、interop、serial 回歸 PASS；EVB：修正版經 UART 部署並完成一次 115200 上傳；**手機 nRF Connect Device Manager 再測 PASS**（0.8.6 → 0.8.63，Candidate 已存在時 App 立即顯示上傳完成，接著 Test → Resetting → Upload Complete，裝置 slot 0 = 0.8.63.0 confirmed）；BLE drops 0；B-info 不變。測後以 UART 恢復 0.8.6-dev。

觀察（非韌體問題）：一次 `smp_serial_ota.py state` 收到 `SMPBadSequence`，原因是前一個 client 逾時離開後，裝置的遲到回應留在序列埠緩衝；工具端之後可在連線時先清空輸入緩衝。

## 2026-10-22 Codex Step 5 review — v0.8.5（Step 6 由 Claude 修正）

Codex 獨立 review 提出 3 項 P2，全部確認並修正（在 `v0.8.6_ble_smp` 上，因 v0.8.6 已包含 v0.8.5）：

| # | 問題 | 修正 |
|---|---|---|
| 1 | 半包 SMP 後，第一筆 GCHK 被 decoder 吃掉 | `smp_serial_expire()`：行內閒置 200 ms 回 IDLE、未完成的多行封包 2 s 丟棄；serial task 每個 byte 前呼叫。Host test：半行＋靜默後 GCHK 17 bytes 全部回到 GCHK parser |
| 2 | `version[24]` 放不下最大版本 `255.255.65535.4294967295`（24 字元＋NUL），結尾 NUL 蓋到 hash | 改為 32 bytes；host test 以最大版本 image 驗證 state 回應的版本字串與 hash 完整 |
| 3 | 規格把 `off` 寫成「已 commit」，實際是本次接收位置 | 程式行為不變（與 Zephyr 相同，改成只回 committed 會讓 client 無法前進）；04 協定、06 §6.1、施工單改為區分「本次接收位置」與「可恢復位置」 |

關於「沒有獨立 task」：Codex 指出「SMP 一問一答」不足以證明 NOR／驗證阻塞期間 GCHK 仍能及時處理。**v0.8.6 已將 SMP 處理移到 OTA worker task**，serial task 不再被阻塞；v0.8.5 單獨的版本確實有此限制，記錄於此。

## 2026-10-22 v0.8.6 自我檢查（Claude 實作，**未經獨立 review**，待 Codex 補 Step 5）

- 範圍：`smp_ble`（GATT service、reassembly、分段 notify）、`ota_worker`（單一 owner task、兩個靜態請求槽，不用 heap queue）、`ota_manager` 跨 transport owner／EBUSY／takeover、OS bootloader info、UART TX mutex；`ET100_SMP_BLE_OPEN` build flag（release build 經檢查不含 SMP UUID）。SDK 未修改。
- 驗收 M-BLE PASS（[14 報告](docs/04_testing/14_evb_ble_smp.md)）：手機完整 Test and Confirm、BLE 上傳中 UART 回 EBUSY、B-info 不變。
- 實機修正：nRF Connect Device Manager 需要 OS bootloader info，否則停在「Reset into Firmware Loader」。
- **請 Codex review 時特別看**：`smp_ble.c` callback（BLE task）與 worker 之間的共享狀態（`in_flight`、`generation`、reassembly 緩衝）；`ota_worker_submit` 的槽位 full 旗標與 `__DMB`；`serial_send` mutex 與 `set_baud()` 在 worker 傳送時的互動；worker 4 KB stack 是否足夠（未量測 high-water）。
- 已知限制：未量測 FreeRTOS 剩餘 heap 與 stack 水位；BLE 更新期間未停止 sensor GATT client 自動連線（僅暫停 Type-1 回報）。

## 2026-10-22 v0.8.5 自我檢查（Claude 實作，Codex 補審通過，已併入上節）


依 comm.md 備用規則，本 branch 由 Claude 實作與測試，以下為自我檢查，不等同獨立 review。

- 範圍：`cbor_min`、`smp_serial`、`smp_server`、`ota_manager`（新）；`serial_gchk.c` 分流＋`OTABAUD`；`nor_qspi1` weak yield；`boot_health` 匯出 boot port；`installer/candidate.c` magic 常數拆開；工具 `smp_serial_ota.py`；SDK 未修改。
- 驗收 M-SMP PASS（見 [13 報告](docs/04_testing/13_evb_smp_serial.md)）：state、上傳、9600 中斷續傳、test／reset／安裝／自動確認、115200、閒置回 9600、B-info 不變。
- 實機修正 4 項：App 內重複 `EUPFWV1` 字串（打包工具擋下）、OS reset ID 應為 5、pending 時 slot 0 應維持 confirmed、client 中斷後的半行。
- **請 Codex review 時特別看**：`ota_manager.c` 的續傳／取消舊 session 邏輯與 boot state 檢查；`smp_serial.c` 的長度／溢位邊界；serial task 內 `set_baud()` 重新初始化 UART 的時序；`taskENTER_CRITICAL` 內寫 boot state（沿用 v0.8.4 作法）。
- 已知差異：OTA manager 未開獨立 task（SMP 一問一答）；`load_image.py` 仍期望 v0.8.3 結果（未修改）。

## 2026-10-22 Claude Code Step 5 review — v0.8.4 M-WDT／M-RB

**結論：PASS（含已記錄的限制）。**

- 首輪 HANG 未 reset，依停止條件停下；SWD 唯讀診斷定位根因：SDK `pmu_init()` 以 `ool_write(PMU_REG_CLK_EN, …)` 覆寫時脈，WDT clock bit 被清掉（CLK_EN=0x05）。修正：App 在 `pmu_init()` 後立即 `iwdt_init／Enable／Refresh`（`evb_serial_main.c:276-280`），不改 SDK。
- 修正後 HANG：loader 提交 PENDING 後**無主機 reset／燒錄**，最終 boot state seq 37／CONFIRMED／FAILED、confirmed hash＝修正版 A、App readback 吻合 → 由 watchdog 觸發的自主回滾成立。UART runner 逾時未取得過程訊息，屬觀測缺口，依「測試不做太細」接受，不重跑。
- 正常 A：120 秒內 WDT clock／enable 持續為 1，無異常 reset；B-info 全程不變。
- 限制：`pmu_init()` 內部盲窗（記於 06 §5）；簽章未做。
- 提醒：repo 根目錄新增的 `docs/02_protocol/`（TMT 周邊資料格式報告）非本 branch 產出，commit 時不納入，待 Stanley 決定。

## 2026-10-22 Claude Code Step 5 review — v0.8.3 M-E0／M-INST／§8

**結論：PASS。** 一個 Major 已在 Step 6 修正。

- M-E0：ROM 接受 A 資訊段 code_length／CRC 只涵蓋 stub，App 在 0x08020200 獨立開機；`jump.c` 清 IRQ／SysTick／NVIC（依 ICTR）／PendSV、設 VTOR／MSP，naked 跳轉；vendor OTA 在 link（mk filter）與註冊兩層移除；燒錄工具整段跳過 B 資訊段並比對 hash。Stanley 手機確認 BLE。
- M-INST：host 3,441 次注入 PASS；EVB 新版安裝、三階段 reset 重做、壞 SHA／nRF image／CRC=0 拒絕，App／boot state／B-info 逐 byte 不變。Installer 寫入白名單僅 App slot 與 boot state。
- §8：EUPFWV1 與 gateway struct 逐 byte 相同（286 B）；`dfu_application.zip` manifest schema 與 NCS 相同，`load_address`=0x08020200，產出 image CRC 0x745E889D 正確。
- Major（已修）：installer 原本對 FR3068 接受 CRC=0，且 06 §6.1 FR3068 列被改回寬鬆規則；已改為 0 一律拒絕並重測。
- Minor：`evb_serial_ble.c:7` 殘留 `#include "service_ota.h"`（未使用）。
- 仍未做：簽章（Q42）、trial／confirm／rollback（v0.8.4）、正式 RS232（v0.8.5）、BLE SMP（v0.8.6）、E-C 晶片上的 ROM 行為確認。

## 2026-10-22 Claude Code Step 5 review — v0.8.2 M-HOST／M-EVB

**結論：PASS，無 Critical／Major。** M1（session 身分）已修：`write_chunk／finish／cancel` 帶 session_id，不符回 `OTA_ESESSION` 且不存取 NOR；`resume(manifest)` 需 session_id／length／sha256／target 完全一致，換 source 寫 takeover snapshot。

- M-HOST：42,388 次斷電注入 PASS（逐操作 3 模式、GC 矩陣、連續 3 次、2 MB 隨機 10,000 次），並斷言重傳 ≤1 chunk。fake NOR 以 `used[]`／weak assert 強制「不重寫半寫位置」「不在 weak sector program」。
- M-EVB（Stanley 縮減範圍）：同一 session 2 MB，在 chunk 128／256／384 MCU reset，三次以不同 source takeover 續傳，整區 SHA-256 與 host 一致；已恢復 0.7.0-dev。
- 首次 reset 的 UART collector 因非 ASCII byte 中止，已誠實記錄並修 runner（reset 路徑 raw 保存）；不影響結果判定。
- 殘餘風險（07 已記錄）：已完整寫入並驗證的 cell 自行失效屬媒體故障，不在保證範圍。

## 2026-10-22 Claude Code Step 5 review — v0.8.1 M-G1／M-T7

**結論：PASS，無 Critical／Major。** 依 comm.md「Hardware bring-up loop」在 milestone review。

- M-G1：UART log 確認 `G1 init_rc=0`、16 MB、erase 20／52／D8、SR 00／00；dummy=11 timeout（rc=-3）後再次 init 成功，前後 SFDP／geometry 一致。
- M-T7：T2–T7 PASS；T3 四點 UART 與 SWD DAC（0x28000000＋offset）一致；T6 三輪 SHA-256 與 host 獨立期望值相同；已恢復 0.7.0-dev 並以 GCHK 確認。
- 根因鏈（G1 五輪）：QSPI1 indirect 暫存器不存在（HardFault @0x50290060）→ 誤用 dedicated OSPI pad 設定 → U11 實際接 GPIO function 0xC（由 SchDoc net label 確認，原 md 接線表為轉錄錯誤）→ SFDP dummy 編碼（raw=1 = 8 clocks）。
- Minor（不擋 commit）：09 報告保留多段歷史快照，已有「以本文最上方為準」聲明；之後可整理成精簡版。DWT 在 debugger disconnect 後停止，已記為 journal 規則 3。
- 仍未驗證：T8／G2 斷電、4-byte 定址、XM25QH256D／SPIMX2_1、page-PP 效能。

## 2026-10-22 NOR bring-up loop 執行補充（非獨立 review）

Stanley 已 review M-G1 PASS，並授權 U11 scratch T2–T7、M-T7 後恢復 0.7.0-dev；不含 non-volatile status／OTP 變更或 T8／power-cut。沿用 43,928-byte SFDP dummy 修正版，未修改 SDK、linker、FAL 或產品 source。詳細原始證據與限制見 [09 NOR bring-up](docs/04_testing/09_evb_nor_bringup.md)。

T2–T5 已完成：00／00 status、unarmed gate、四個指定邊界的 UART／host pattern／SWD DAC 一致、三種 erase 各 20 次及未對齊拒絕、整頁／跨頁／部分頁／bench 逐 byte 與 SHA-256 驗證。首輪 SWD disconnect 後零計時無效，已重新 init 並重測；另修正暫存 runner 的既有 arm 狀態前提，不改 firmware。有效數據與失敗原始紀錄均保留於 09 所列 logs。

T6 PASS：0x020000–0x41FFFF 三輪共 12 MB，每輪 4 MB SHA-256 皆等於 host 獨立期望值 `54EAE10E589E0EDE8456DF201377D80E30D8A802C3E534753BA20148CA69F617`。T7 PASS：PP／erase 後立即 MCU reset，兩次 G1／SR／write gate／全 sector SHA／再次擦寫讀回通過，未切 NOR 電源；未直接量測 reset 瞬間 WIP，不擴大宣稱。

T5／T6 throughput 明確標記 **STIG 8-byte PP, not OTA-representative**，不得推論為優化後 OTA 性能、4-byte 定址、電源故障耐受或 ET-100 32 MB NOR 驗收。M-T7 已達，依授權恢復 284,712-byte 0.7.0-dev 備份，完整 readback PASS，9600 bps GCHK 回覆版本 0.7.0-dev；目前板上已非測試韌體。結果以 09 最新節為準；G2／T8 pending C11／C30。無 commit／push／merge，等待 milestone review。

**Date**：2026-10-02（初版）、2026-10-02（Codex review 修訂）
**Reviewer**：Claude Code（Step 5）
**Cross-review**：Codex（在初版發送後審查，發現 3 項 Major → 已修正）
**Executor**：Codex（toolchain install）+ Claude Code（SDK copy + patches + build）
**Scope**：macOS 上實證 ADR 01（toolchain = ARM GCC）

> **2026-10-02 文件同步（Codex）**：依 FR306x v0.4.9 校正容量來源、使用率與未驗證宣稱；未重新 build、未修改 SDK，以下產物數據保留既有結果。晶片總容量為 2 MB Flash／512 KB SRAM，但 linker 可用區與總容量的對應仍待確認，見 [開發對照](../docs/03_hardware/fr306x_reference/05_et100_development_reference.md)。

> **2026-10-21 Phase 1b 更新**：本檔前半保留 Phase 1a／1a-ext 當時的 review 紀錄；文中 `FR303x` selector 與「EVB 尚未驗證」已被文末 Phase 1b addendum 取代。實機證據確認正確 burn header 為 vendor default **FR509x layout**，見 [`vendor/PATCHES.md`](vendor/PATCHES.md) Patch #5。

## Summary（修訂）

✅ **ADR 01 經實證可行**，條件如下：
- Build 成功的是 **patched 版本的 SDK**（5 處本地 patch，見 [`vendor/PATCHES.md`](vendor/PATCHES.md)）
- 原始 SDK 直接 build **會失敗**（已實測）
- 產出 `output/Project_burn.bin` 為 post-process 封裝候選映像（header／CRC／padding），是否符合板上 bootloader 仍未驗證

❗ **不可過度推論**：
- 「可燒錄 + 可啟動 + 可廣播 BLE」**尚未實機驗證**，EVB 到之前無法確認
- Patch #5 的 header 格式推自 `post_process.py` 的 struct 定義；bootloader 真的接受此格式仍要 EVB 證實
- Flash 燒錄 address（推測 0x08002000）需等 Quectel 或 Freqchip 燒錄工具文件確認

## Build Result

```
Flash raw image: 260,352 bytes = 254.25 KiB / 1016 KiB = 25.0%
SRAM static allocation: 97,232 bytes = 94.953 KiB / 256 KiB = 37.1%
上述分母為目前 linker window，不是 datasheet 晶片總容量；不代表 runtime heap 餘量。

text:  225,072 bytes  (GNU size 分類，含需載入 RAM 的 code)
data:   35,272 bytes  (GNU size 分類，含 .dram_section)
bss:    61,976 bytes  (GNU size 分類，含靜態 heap 配置)
```

## Products — 用途與限制

| 檔案 | 大小 | 用途 | 可否燒錄 |
|---|:-:|---|:-:|
| `Objects/Project.elf` | 1.3 MB | GDB / Cortex-Debug 讀 symbol | 是否可供 programming 取決於 loader；未驗證 |
| `output/Project.hex` | 732 KB | Intel HEX（JLink 多半可直接讀）| ⚠ 內含 raw code，無 bootloader header，實測後定 |
| `output/Project.bin` | 260,352 bytes | raw image，不含 post-process header | 未驗證；不能保證直接啟動，也不能斷言所有流程都不會 boot |
| **`output/Project_burn.bin`** | **268,544 bytes** | **封裝候選映像（header + CRC + padding）** | 未驗證；需確認 board-matched boot 格式與燒錄流程 |
| `output/Project.map` | 1.1 MB | Linker map | — |
| `output/Project.lst` | 4.3 MB | Disassembly | — |

**本報告不是已驗證的燒錄指引**。候選映像 payload 從 offset 0x2000 開始；linker app address 是 0x08002000。整份封裝是否應寫入 0x08000000、是否需其他 ROM metadata／分區，須依 board-matched 原廠文件與 EVB 測試確認，不直接以工具 default 作保證。

## Findings

### Critical
_None._

### Major
- **MJ1（Codex review 揭露，已修）**：Patch #4 一開始只改 `python.exe → python3` + 加 `-` 讓失敗不致命，實際上 **post_process.py 本身因 Windows backslash 失敗，`Project_burn.bin` 從未產生**；initial REVIEW_REPORT 誤稱「可直接燒錄」是 overclaim。Patch #5 補上跨平台路徑；當時另改 CHIP_TYPE 為 FR303x 的決定已於 2026-10-21 證明錯誤並撤回。
- **MJ2（Codex review 揭露，已處理）**：Phase 1a 原 `IMPLEMENTATION_PLAN.md` Out of Scope 明示「不要修改 SDK」，但實作過程改了 5 個 SDK 檔案。此為 **scope creep**。已補寫 [`ADR 02 — SDK Local Patch Policy`](docs/99_decisions/02_sdk_local_patch_policy.md) 追認並立下未來規則：將來若 patch > plan 預期量，Claude Code 必須停下請示。
- **MJ3（Codex review 揭露）**：原 REVIEW_REPORT 的 **「完整成功、可直接燒錄」為 overclaim**。精確的宣稱應為「patched SDK 的 host-side build pass；啟動 + BLE 廣播 + 硬體除錯仍待 EVB 驗證」。本修訂版已改。

### Minor（原列，不變）
- **M1**：Linker 警告 `LOAD segment with RWX permissions`。原因：`.ram_code*` section 同時 exec + write + load，符合「code copied to RAM then executed」用途。可忽略。
- **M2**：Toolchain PATH 需手動加入 `~/.zshrc`（Stanley 已完成）。
- **M3**：Codex 從 gitee 下載 SDK 失敗（curl 18/33/56），改由 Claude Code 從 scratchpad cache 複製；長期需找更穩的 SDK 分發管道。
- **M4**：`.ram_code_front` section patch 用 full-section 而非 empty symbol 作法，實測 Section 有 296 bytes 內容 → full-section 是對的。

### Informational
- **I1**：ARM GNU Toolchain 14.3.rel1 / CMake 4.4.3 / Ninja 1.13.2 — 現役版本
- **I2**：Build 時間單 thread ~45 秒；可 `make -j8` 加速
- **I3**：BLE binary library (`libbtdm_host.a`) 與 ARM GCC 14 link 無 ABI 衝突
- **I4**：Codex 參與了 install toolchain 與 review 兩個角色，分工照 comm.md Step 1-5 跑了一輪：Claude Code 寫 plan → Codex 試行 → Claude Code 補救 build → Codex 做 cross-review → Claude Code 修正 → Codex 可再 review

## 真正證明了什麼 / 沒證明什麼

### ✅ 已證明
- macOS 26.3.1 arm64 + ARM GCC 14.3.1 已以 SDK 原生 Makefile 完成範例 build；CMake／Ninja 已安裝，但本次未證明 CMake 整合流程
- Freqchip 公開 SDK + 5 處本地 patch 後，GCC build 流程完整走通到 `Project_burn.bin`
- 產物與靜態配置落在本次 linker window 內；完整晶片 memory map、runtime heap／stack 餘量仍待確認
- `libbtdm_host.a` (BLE binary) 可與 ARM GCC 14 link 無錯誤
- `post_process.py` 的 header 填入邏輯對 `Project.bin` 執行成功、產出 `Project_burn.bin`

### ❌ 尚未證明
- `Project_burn.bin` 燒進真實 FR3068E-C 後**能啟動**
- 啟動後**能跑到 main()**
- 跑起來後**能廣播 BLE packet**（example app 的功能）
- SWD / JTAG debug 流程可用
- Flash 燒錄 address 0x08000000 推測正確
- Freqchip bootloader 真的接受此 header 格式
- Patches 對產品行為無副作用（特別是 `.ram_code_front` 的 patch）

**上述所有 ❌ 項目都需要 EVB 到才能驗證**。

## Verification Evidence

```bash
# 執行紀錄：
$ cd fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/GCC
$ rm -rf Objects output
$ make 2>&1 | tail -5
python3 ../../../../components/tools/post_process.py Project output
program target with file output/Project_burn.bin
Build: Project make done.

$ ls -la output/
Project.bin        260352 bytes
Project.hex        732395 bytes
Project.lst        4344035 bytes
Project.map        1179136 bytes
Project_burn.bin   268544 bytes   ← 封裝候選映像，boot 尚未驗證

$ python3 -c "import struct; f=open('output/Project_burn.bin','rb'); \
    print(hex(struct.unpack('I',f.read(4))[0]))"
0xffffffff    # version header (expected)

$ arm-none-eabi-size Objects/Project.elf
   text    data     bss     dec     hex filename
 225072   35272   61976  322320   4eb10 Objects/Project.elf

$ file Objects/Project.elf
Objects/Project.elf: ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV)
```

Full build log: `/tmp/et100_build.log`

## 已記錄的本地 patches（全在 SDK 本體內；commit 狀態未查證）

依 [ADR 02](docs/99_decisions/02_sdk_local_patch_policy.md) 政策保留，詳見 [`vendor/PATCHES.md`](vendor/PATCHES.md)：

1. `sysmem.c` + `#include <sys/types.h>`
2. `ldscript.ld` + `.ram_code_front` section
3. `cpu_context_gcc.S` + `.thumb_func` / `.type` / 搬 `.size`
4. `Makefile` python.exe → python3
5. `post_process.py` 跨平台路徑；當時誤改 CHIP_TYPE FR303x，已於 2026-10-21 撤回為 FR509x layout

## Overall（修訂後）

**Pass with Caveats** — Phase 1a 的 host-side 目標達成；但 initial REVIEW_REPORT 的「可直接燒錄」宣稱是 overclaim，已修正。將 Pass 狀態從「完整成功」降為「host build 成功 + 燒錄行為待 EVB 驗證」。

## Next Steps

### 立即（Stanley）
1. ✅ PATH 已加 `~/.zshrc`
2. ⬜ 重產 100_questions V1.3 HTML／PDF 後寄 Quectel（目前僅 Markdown 同步）
3. ✅ FR3068EP-C DEV1.4 EVB 已到貨

### EVB 到貨後的 Phase 1b
4. ✅ 透過板載 DAPLink + SDK FLM 燒 `Project_burn.bin` 到 EVB
5. ✅ 補 jumper 後，以 SWD software reset 與 921600 UART capture 並行確認 application log
6. ✅ 用 nRF Connect 連線並確認 GATT
7. ✅ 驗證 DAPLink SWD 可連線
8. ✅ 證實 FR509x layout 的 `_burn.bin` 可啟動；錯誤 FR303x selector 已撤回

### 並行
9. ⬜ EUP 終端設備通訊協定考古（不需 EVB）

## 感謝 Codex Review

Codex 在初版 REVIEW_REPORT 發送後做 cross-review，正確抓出 3 項 Major issue（Patch #4 不完整、scope creep、overclaim）。本修訂版針對該 review 全盤接受並修正。
這是 comm.md 的 Claude Code / Codex 分工機制在單人 + AI toolchain 環境下實際跑的第一輪，證明雙 AI reviewer 架構有效。

---

# Addendum — Phase 1a-ext（2026-10-02 下午）

## Context

Codex Review 完 Phase 1a 後再做 datasheet-驅動的全文件同步，發現：
- **SRAM 真實容量 512 KiB**（Keil .sct 證實；GCC .ld 原僅配 256 KiB）
- 同時發現 example `main.c` clock init 把核心跑到 **192 MHz**（datasheet 上限 156 MHz）
- Codex 在修改前停下請示 → Stanley 兩階段擴大授權 → 轉成 Phase 1a-ext

## Scope + Deliverable

- Patch #6：ldscript.ld + ldscript_3068e.ld SRAM 256K → 512K
- Patch #7a：example main.c MCU_DIV 1 → 2（192 MHz → **96 MHz**）
- Patch #7b：system_fr30xx.c `System_MCU_clock_Config()` 入口加 156 MHz runtime guard
- 全部重 build + 驗證

## Build Result

| 指標 | Phase 1a | Phase 1a-ext | 差 |
|---|---:|---:|---|
| text | 225,072 | **225,096** | +24 B（Patch #7b guard 幾條指令） |
| data | 35,272 | 35,272 | 0 |
| bss | 61,976 | 61,976 | 0 |
| `.ram_code` | 14,728 | 14,752 | +24 B（guard 進了 ram_code 區） |
| Project.bin | 260,352 | 260,376 | +24 B |
| Project_burn.bin | 268,544 | 268,568 | +24 B |
| image_crc | 0xe87afbd9 | 0x5b1a2ae6 | 變（因 code 變） |
| magic (QRRQ) | 0x51525251 | 0x51525251 | 不變 ✓ |
| SRAM 可用容量 | 256 KiB | **512 KiB** | +256 KiB |
| SRAM 實際佔用 | 97 KB → 37.1% | 97 KB → **18.9%** | 相同佔用，使用率對半 |

## 驗證重點

- **Section 位址確認**：bss / data / dram_section / _user_heap 全部落在 0x20000000 - 0x2001E008 之間，距 512K 上限（0x20080000）仍有 >400 KB 餘裕。
- **Patch #7b guard 位置設計優秀**：Codex 插入 `System_MCU_clock_Config()` 函式入口，計算 target_core_clock_hz 後、進 PLL register 前，比 plan 要求的「最靠近寫入 PLL 的點」做得還到位。而且智能處理了兩種 clock source（CORE_HSCLK vs SPLLCLK）—— plan 沒要求但 Codex 自己做了，加分。
- **burn.bin header 結構不變**：exec_offset 0x2000、magic 0x51525251，只有 image_crc 隨 code 變化而重算。

## Findings

### Critical / Major
_None._（Codex 自我約束 + plan 寫得明確，一次 pass）

### Minor
- **m1**：Phase 1a-ext build 的實際 SRAM 使用量不變（97 KB），因 example app 本來就不會吃到 256 KB → 512 KB 擴容對這個 example build 的統計意義是「未來 ET-100 app 可用空間 +256 KB」而非「現在省 256 KB」
- **m2**：Patch #7b 的 guard 進了 `.ram_code` section（因為 `__RAM_CODE` 屬性），開機時 copy 到 PRAM 執行。這代表 guard 速度最快但吃一點 PRAM。影響可忽略。

### Informational
- **I1**：Patch #7a 的降頻結果 96 MHz 是 datasheet 156 MHz 上限的 61.5%，margin 充足；功耗相對 192 MHz 省 50%
- **I2**：Codex 完全沒越界（SDK 以外、CMakeLists、本專案 src 都沒碰）
- **I3**：Build 一次 pass，無新 warning（除了 Phase 1a 已知的 RWX LOAD segment）

## Overall

**Pass**（無 Caveats）—— Phase 1a-ext 的全部 scope 達成。host build 成功、Project_burn.bin 產出、SRAM 擴容到 datasheet 一致、clock 降到 datasheet 內保守值 + runtime guard。

## 感謝 Codex (round 2)

Phase 1a-ext 展示了 Codex 兩個進化：
1. **主動停下請示的習慣鞏固**：發現 192 MHz over-spec 時立即停、提兩個選項、解釋 trade-off，而非悶頭跑完
2. **執行時的工程判斷力加分**：Patch #7b 的 guard 插入位置選得比 plan 要求更精準；處理雙 clock source 的 formula 更周到

這證明 ADR 02 + comm.md 分工在**第二輪實測**仍穩定運作。

## Phase 1a-ext 的 Next Steps（接續 Phase 1a 的清單）

- ⬜ 把 Q49（AEC-Q100 否）+ Q50（SDK 192 MHz vs datasheet 156 MHz）加進 100_questions_for_quectel V1.4
- ⬜ 重 render HTML + PDF V1.4
- ⬜ Stanley 寄 Quectel PDF（V1.4，取代 V1.3 send）
- ✅ 更新 [`docs/00_project/11_project_bring_up_status.md`](../docs/00_project/11_project_bring_up_status.md) Category 7／8；Phase 1b 再同步實機結論
- EVB 已到貨；EUP 協定考古仍待處理

---

# Addendum — Phase 1b EVB Bring-up（2026-10-21）

## Scope

- FR3068EP-C DEV1.4 EVB 基本辨識與 jumper mapping
- 板載 DAPLink SWD attach、internal flash programming 與 readback
- `Project_burn.bin` boot header layout 實機判定
- Reference BLE peripheral 開機、廣播、連線與 GATT discovery
- 96 MHz development configuration 基本啟動驗證

本階段未驗證 ET-100 PCB、量產燒錄、OTA update/rollback、外部 NOR、低功耗、BLE 長時間穩定性或 512 KiB SRAM upper bank。

## Result

| 項目 | Phase 1b 結果 |
|---|---|
| EVB | FR3068EP-C DEV1.4；EP-C 含 2 MB PSRAM，產品 E-C firmware 不得依賴 |
| SWD | DP IDCODE `0x0BE12477`、AHB5-AP、CPUID `0x410FD214` = Cortex-M33 r0p4 |
| Flash loader | SDK `components/tools/keil/FR30xx.FLM` |
| Programming | `Project_burn.bin` 寫入 `0x08000000`；66 sectors；readback byte-for-byte PASS |
| Boot | reset 後 PC 曾觀察於 PRAM `0x1FFE3A90` 的 `__RAM_CODE` idle 路徑；app 正常執行 |
| UART + SWD | 同一 DAPLink USB 上，SWD `SYSRESETREQ` 與 921600 bps UART capture 並行 PASS；收到 `FR5090: ble_simple_peripheral.` |
| BLE | nRF Connect 可連上 `30xx_Ble_Periphreal` |
| GATT | 可看到 FFF0、FFF3（Read/Write/Notify）與預期 128-bit characteristic |
| 96 MHz | Patch #7a configuration 可開機與建立 BLE 連線；未量測實際 clock、功耗或長期穩定性 |

## Critical Finding — Burn Header Selector

Phase 1a Patch #5 曾在沒有原廠證據下把 `post_process.py` 的 selector 從 vendor default `CHIP_TYPE_FR509x` 改成 `CHIP_TYPE_FR303x`。這是錯誤修改：

- FR303x layout 少了 `tlv_length` word，導致 options 欄位錯位。
- 錯誤 image 讓 ROM flash access 卡死，表現為 BLE 不啟動、SWD AP WAIT ACK。
- SDK 內原廠 Keil burn image 使用 FR509x layout。
- 恢復 FR509x 後，header word 7 = `0x51525251`、word 8 = `0`、word 11 = `0x51525251`，實機 programming、boot 與 BLE PASS。

**結論**：Patch #5 只保留跨平台 `os.path.join()`、非零 error exit 與診斷訊息；CHIP_TYPE 變更已撤回。`FR509x` 在此是 header layout selector，不是 MCU 型號判定。

## SDK Patch #8

BLE example 的 `main.c` 在 EVB development build 中保留 `SYSTEM_PREVENT_SLEEP_TYPE_DISABLE`，不再清除該 sleep prevention bit。原因是板載 DAPLink 沒有 nRESET 線，sleep/壞 image 狀態下無法使用 under-reset attach。

此 patch 改變 runtime power behavior，只能用於 EVB debug；ET-100 production firmware 不得直接採用。Patch 已以 `[eup-patch 2026-10-21 patch #8]` 標記並登錄於 [`vendor/PATCHES.md`](vendor/PATCHES.md)。

## Tooling

新增 [`tools/evb/`](tools/evb/)：

- `uart_listen.py`：限時 UART capture
- `swd_probe.py` / `swd_retry.py`：唯讀 attach 診斷
- `fr3068_flash.py`：正常 program + verify + reset
- `rescue_and_flash.py`：DPv2 dormant wake + reset-release catch + 同 session 救援燒錄

燒錄工具會拒絕非 FR509x layout image，並以 repo-relative defaults 尋找 SDK、FLM 與 image。Phase 1b 板卡與操作細節見 [`../docs/03_hardware/FR3068EP-C Dev1.4/README.md`](../docs/03_hardware/FR3068EP-C%20Dev1.4/README.md)。

## Findings

### Critical / Major

_None remaining after restoring the FR509x header selector._

### Minor / Remaining Risk

- DAPLink 沒有 nRESET；壞 image 仍可能需要手動 RST timing rescue。
- Patch #8 停用 sleep，因此本次不能代表產品待機功耗。
- BLE 只完成基本連線/GATT discovery，尚未做 soak、throughput、disconnect/reconnect 或 OTA。
- EVB 外部 Flash 與 ET-100 型號、容量、電壓和 addressing 不同。

## Overall

**PASS for Phase 1b basic EVB bring-up**。macOS + GCC patched SDK 的 burn image 已完成真實硬體 programming、boot 與 BLE GATT 驗證；Phase 1a 的「候選映像、未驗證」狀態可針對此 EVB 組合解除。產品板、量產流程與 OTA 仍是獨立驗收項目。

---

# Addendum — Phase 1b.1 DEV1.4 GPIO/Key Passive Diagnosis（2026-10-21）

## Scope

- 暫時 Patch #9：每 20 ms 唯讀 `PMU_REG_PIN_DATA`，僅在值改變時輸出 UART log。
- 不設定 PMU direction、pull、mux、wakeup 或 interrupt，不驅動 D3/D4。
- clean build、FR509x image 檢查、EVB program/readback、K1-K3 逐鍵長按 capture；若無法辨識便停止並 rollback。

## Result

| 項目 | 結果 |
|---|---|
| Diagnostic build | PASS；text 225,304、data 35,272、bss 61,976 bytes |
| Diagnostic image | `Project_burn.bin` 268,776 bytes；header、CRC 與 payload consistency PASS |
| EVB program/readback | PASS |
| UART evidence | 受控測試 497 行；PMU bit 0、2、3 曾變化，但沒有按住期間的穩定 level |
| Key mapping | **INCONCLUSIVE**；`0x0008`、`0x0001`、`0x0004` 只能列為候選 mask，不能對應到 K1／K2／K3 |
| Rollback | PASS；Patch #9 已從 SDK source 移除，baseline clean rebuild 並重新燒錄／verify |
| Current image | baseline `Project_burn.bin` 268,560 bytes；SHA-256 `0710a7e79de7ac2dd6c1f930119f1b82c4400a64a1d1ea76086da9e08be76fc2` |

## Findings

- 被動 PMU read 不足以識別按鍵。input/pull 未設定造成浮動是合理假說，但目前沒有 register readback、原理圖或 continuity 證據可以確認。
- D3/D4 在 reset 後熄滅，約 3 秒後與 application banner 大致同時亮起；仍不能據此斷言 LED pin 或 active polarity。
- 依 plan 與 ADR 02，沒有擴大成 PMU configuration patch。若要繼續，需 Stanley 另行授權，並把修改限制在候選 pins 的 input/pull 設定。

## Overall

**PASS for bounded diagnosis and rollback; key mapping not established.** 臨時診斷、紀錄與 rollback 均完成，active SDK patch 數仍為 8 組。完整 log：`/tmp/et100_gpio_diag_build.log`、`/tmp/et100_gpio_diag_uart.log`、`/tmp/et100_gpio_diag_rollback_build.log`。

---

# Addendum — Phase 1b.2 Serial GCHK（2026-10-21）

專案自有 source 與 Makefile overlay 提供 UART3 9600/8N1、CRLF bounded parser、GCHK 及未支援命令失敗回覆。SDK 新增 patch：0；active SDK patches 仍為 8 組。原 SDK target 與 baseline image 保留。

**自我檢查結果**：host sanitizer tests、ARM build、FR509x header／獨立 CRC／payload、EVB program/readback 通過。Stanley HTML log 證明 GCHK 成功、5 個未支援命令回覆失敗，token 均正確。未發現阻擋此最小範圍的問題；尚未進行獨立 reviewer 審查。

**限制**：GCHK state 0 僅是 compatibility idle，不代表完整 Gateway state machine。RTOS serial task 另占 runtime heap stack；大量命令可能溢出 bounded ring，需 host 逐筆等待回覆。依 Stanley 指示未執行 Codex 自動 UART 測試，硬體 stress、reset reconnect、BLE regression 仍待驗證。詳細數值、hash 與原始 log 見 [測試紀錄](docs/04_testing/02_evb_serial_gchk.md)。

**Overall**：基本 GCHK UART/HTML 路徑 PASS；完整 Gateway 功能及產品 RS232 電路不在本次驗收範圍。

---

# Addendum — Phase 1b.3 GTIME Persistence（2026-10-21）

新增 GGTIME／SGTIME，採 Gateway frame 與 token；預設 15，接受十進位整數 1-1800。沿用既有 FlashEnv，使用獨立 key `0x00030002`、schema 1。FlashDB 初始化狀態會傳至 serial task；set 成功且讀回吻合後才更新 RAM／回覆成功，相同 record 不重寫。serial task stack 增至 1024 words。

**自我檢查**：host sanitizer tests、ARM build、header／CRC／payload、FlashEnv 邊界檢查 PASS。SDK 新增 patch 0。未執行獨立 reviewer 審查；燒錄結果與硬體驗收狀態以 [GTIME 測試紀錄](docs/04_testing/03_evb_gtime_settings.md) 為準。

**限制**：MAXRPT 尚未實作，暫無 Gateway 的 GTIME ≥ MAXRPT 交叉約束；GTIME 不啟動 BLE reporter。Flash driver/FAL 為原 SDK 路徑；write 後讀回降低假成功風險，不能代替斷電恢復與實際持久性驗收。保留最後確認 RAM 值不等於 Flash 失敗後必定保留舊 durable record。Stanley 負責 HTML／reset 測試，Codex 不操作 UART tester。

**燒錄結果**：0.4.0-dev program／逐 byte readback／reset resume PASS。HTML 設定及 reset 後保存仍待 Stanley 測試，尚不宣稱實體 FlashDB 持久性驗收通過。沒有 commit／merge／push。

**後續 Stanley 驗收**：Stanley 回覆「OK. it works.」並要求繼續 CID／BLE scan；因此 GTIME 本輪手動設定／reset 保存記為通過，未取得逐筆 log，詳見 [GTIME 紀錄](docs/04_testing/03_evb_gtime_settings.md)。

---

# Addendum — Phase 1b.4 CID／BLE Scan（2026-10-21）

新增四欄 GCID／SCID 與 FlashDB 保存、general discovery scan、0xFF／0x16 payload 過濾、32-entry cache、GTIME snapshot、RSSI 排序、Type 1 XOR、一秒節流及 EUP v1/v2 1500 ms burst 去重。採專案自有 BLE profile 副本與 build overlay，SDK patch 新增 0。

**自我檢查**：host sanitizer tests、5,000 AD fuzz、Gateway checksum vector、ARM build、FR509x header／CRC／payload／FlashEnv 範圍檢查 PASS。尚未經獨立 reviewer 審查；flash 與 radio 驗收詳見 [CID／scan 測試紀錄](docs/04_testing/04_evb_cid_ble_scan.md)。

**實作選擇**：SDK 註解表示 observer mode 可能在 host 過濾 general-discoverable 封包，因此改用 SDK central example 同款 general discovery active scan。保留原 peripheral profile，scan 由 ADV_START 成功事件啟動，並行能力仍須 EVB 證實。

**限制**：沒有 MAC whitelist／MAXRPT setting／realtime XYZ batch，採 min(GTIME,15) 周期上限。SCID 不合法參數回覆空欄，不等同 Gateway 所有 error-echo 細節。SDK scan API 失敗／SCAN_END 不自動無限重試，以 SWD status/counters 診斷。Radio 真實收包、CID 保存、filter、回報時序、FlashDB GC／斷電與 stack 餘量均由 Stanley 後續測試。

**燒錄結果**：0.5.0-dev program／逐 byte readback／reset resume PASS；未執行 Codex UART／HTML 測試，無真實 radio 驗收宣稱。沒有 commit／merge／push。

**Stanley 實測更新**：`rs232_log_20261021_165642.txt` 證明 4 個 MAC、55 筆真實 Type 1，55/55 XOR 正確；23 筆命令 response/token 全部吻合。基本 scan→Type 1→HTML 路徑 PASS。SAUTOGATT 等未支援功能 valid=0 為預期；不同 CID 的負向／清空對照與 CID reset 保存仍待驗證。完整統計及推論限制見 [實測紀錄](docs/04_testing/04_evb_cid_ble_scan.md)。

**驗收補充**：後續 17:00 export 顯示切至 CID 1234 後停止 Type 1、讀回 CID=1234／GTIME=3，再恢復 0639 成功。Stanley 明確確認查詢前有按 RST 且整體正常；因此 CID／GTIME 手動 reset 保存 PASS。突然斷電、GC 與長時間測試仍未驗收，詳見同份測試紀錄。

---

# Addendum — v0.8.1 U11 NOR 實作自查（2026-10-21，待獨立 review／硬體核准）

依 v0.8.1 施工單與 Stanley 補充授權，在 `v0.8.1_nor_driver_bringup` 新增專案內 nor_port／QSPI1 bounded STIG、獨立 115200 bare-metal console／build overlay、host fake tests 與 runner。不改 SDK／linker／FAL／FlashEnv／產品 evb_serial；不修改硬體 README／PCB／施工單，不 commit／push。SDK patch 新增 0。

ASan／UBSan 覆蓋真實 SDK register layout、ID／BFPT geometry、overflow／alignment、page 切割、保護／write gate、controller idle／STIG／WIP timeout、IRQ-disabled／DWT-stalled／wrap、abort 後 next init；既有七組 host tests PASS。ARM build／FR509x header／CRC／payload／800 KB guard PASS。Python runner AST／help 與 fake serial G1-only／ID mismatch／write gate 測試 PASS；沒有開實體 UART／SWD。機械 plan check exit 0 但無 CHECK anchors，屬 no-op。全工作區 diff check 仍有原先硬體 README EOF 空行警告，不越權修正。

重要界線：實際 JEDEC／SFDP、OSPI mux／PSRAM coexistence、DWT／controller reset 復原、C19 真正切電、T1–T8 與 DAC coherence **尚未實測**。readback 採 STIG 03 fallback；單筆 PP 最多 8 bytes，不能推論整頁性能或命中 program 中途切電。T8 off-state 的軟體 ENOTREADY 不是硬體 timeout 證據；實際斷電規則尚屬保守設計前提。每次 flash 包含恢復基線都需另取 Stanley 授權；下一步只申請測試韌體 flash＋G1 唯讀，不先擦寫／切電。

完整來源、指令、產物／logs、待量測表與 v0.8.2 journal 前提見 [09 U11 NOR bring-up](docs/04_testing/09_evb_nor_bringup.md)。本節是執行者自查，不代替 Claude Step 5 的獨立 review，也不宣稱 Phase 已完整驗收。

# Addendum — v0.8.0 NOR OTA 規格審查（2026-10-21）

**範圍**：依 Stanley 授權做 docs/tools 限定修訂，採完整 2 MB 單 App＋NOR staging／rollback／sensor slot，不採晶片內 A/B。恢復指定[08 實測報告](docs/04_testing/08_evb_internal_flash_size.md)、[唯讀 probe](tools/evb/flash_size_probe.py)、[sector 工具](tools/evb/flash_sector_test.py)與[工具 README](tools/evb/README.md)，未執行工具。板上仍 0.7.0-dev，不改 SDK／source／linker／FAL、不 rebuild／flash、不 commit。使用者硬體 README／PCB 保留。

**已修正**：前 128 KB 拆為 FR509x header 8 KB＋installer 120 KB，App @0x08020000／1792 KB，Settings 112 KB、內部 boot state 16 KB；內部 state 唯一決定安裝／trial／confirm／rollback，NOR journal 只管下載；Sensor slot 雖預留 256 KB，package 上限依 Gateway header 核對為 225280 B（含 256 B header、image ≤225024 B）。先前 262144 B package 上限不相容，已明確撤回。

**新增契約**：confirmed App 的 Rollback 必須在 erase 前建立並驗 hash；同 hash 備份只有實際驗證後才跳過寫入。Installer 完全自帶必要 NOR／Flash／RAM-code／hash／驗簽／watchdog，不依賴安裝中可能不存在的 App。一般 confirmed boot 不初始化 NOR；App relocation／VTOR 與睡眠還原列 gate。EVB 16 MB NOR 不能驗產品高位地址；需求來源記錄 Stanley 的 RS232 入口與取消 Factory recovery 指示。

**查證與未證明事項**：本機 Gateway sensor_ota_package.h 常數、SDK SystemInit 未設 VTOR、freertos_sleep.c 保存／還原 VTOR、flash_write／flash_erase 的 __RAM_CODE 均已只讀核對。這不證明完整 RAM call graph 或 ROM CRC 範圍；後者明確待 Q43-5，exact E-C 2 MB／runtime write 待 Q48-6。16 KB journal 仍須 record／wear／GC proof；現行 FlashEnv C8000／12 KB 移轉為實作前置。[08 報告](docs/04_testing/08_evb_internal_flash_size.md)及其工具已恢復，量測屬既有 EP-C EVB 證據，本輪未執行硬體測試。NOR → 內部 copy 由自有 installer 實作；Q43-5 只確認 ROM／installer 邊界，不依賴原廠 ROM 提供 NOR install。

**驗證類型**：文件地址／總和／alignment、package 邊界、相對連結、語意一致性及變更範圍檢查；沒有韌體 runtime／ROM／NOR 測試。完整候選與保留清單見 [OTA 儲存規格](docs/01_firmware/06_ota_storage_design.md)。Overall：**規格修正供 Claude Code 複審；不得當 boot／安全更新實作完成**。HTML／PDF 技術問題清單尚未重產。

**實際文件檢查結果**：內部 2097152 B／外部 33554432 B、十個分區的區間長度／連續／不重疊／4 KB alignment PASS；Gateway 常數與 Sensor image 上限／slot 餘量算術 PASS；新增／修改行的 Markdown 相對檔案 links PASS；git diff --check（排除使用者既有硬體 README）PASS。source／tests／targets／SDK 未變；本輪工具變更限恢復[唯讀 probe](tools/evb/flash_size_probe.py)／[sector 工具](tools/evb/flash_sector_test.py)與 README，原保存記錄未刪除或整批套用。這些是文件與 scope 檢查，不是 erase geometry／ROM／硬體驗收。

**M1／M2／m1／m2 複查**：四個指定檔案已選擇性恢復；08 分支改為 v0.8.0_ota_nor_spec、無晶片內 A/B 引用。兩支 Python 工具 byte 比對一致、僅 AST syntax 解析，未執行。06／01 §5／本報告／status 均以實體相對 links 引用證據／工具；V1.4 row／Q48-6 使用中性 EVB 量測句並詢問 E-C，01 的問題清單引用皆改 V1.4，NOR copy 明確由自有 installer 實作、Q43-5 僅確認 ROM／installer 邊界。分區算術／連續／alignment／package 上限／新增相對 links／範圍檢查均 PASS，log `/tmp/et100_ota_nor_doc_checks.log`。git diff --cached --check 及本輪 working-tree／combined 範圍檢查 PASS；全工作區 git diff --check／git diff HEAD --check 僅有原已存在的硬體 README EOF 空行警告（本輪不修改該檔）。08／工具 README 另明記 0x081F7000 未來屬 Settings，不能永久當 scratch。沒有 build／flash／commit，HEAD 未變。

---

# Addendum — Phase 1b.6 App GATT Server（2026-10-21）

**Stanley 實測更新**：燒入 EUP_AIoT_HUB_FR／0.7.0-dev 後，Stanley 回覆「yes, it works.」，App 基本連線與 manager 路徑記為使用者確認可用；沒有逐筆 log，不將雙角色並行／長時間／所有驗收步驟一併列為 PASS。詳細證據界線見 [測試紀錄](docs/04_testing/07_evb_app_gatt_server.md)。下方「待驗收」描述保留為當時狀態，以本補充為準。

在已推送 checkpoint `db4f2f7` 之後，依授權分支新增 Gateway manager 四 characteristic、八筆 RAM table、status/list v5/select/selected-data 格式。Scanner 與 UART 共用同一 AD/filter；FFF0 demo service 被取代，vendor OTA 保留但未驗證。UART 仍 9600、版本 0.7.0-dev；原 clock／linker／FlashDB／按鍵／D4/D3 邏輯保留，SDK patch 新增 0。

**相容性阻擋與決策**：先前只依 service UUID 推論 App 不需修改不完整；實讀 App code 發現 EUP_AIoT_FR 會觸發 sensor AES 分類。已停在燒錄前請示；Stanley 明確選擇 EUP_AIoT_HUB_FR，不改 App。ADV 與 GAP name 已一致更新，byte 長度由 host tests 核對。只證明 code-level compatibility，沒有 iOS 實測通過宣稱。

**自我檢查**：七組 ASan／UBSan tests、ARM build、獨立 FR509x header／CRC／payload／FlashEnv／名稱版本比對 PASS。READ_REQ buffer capacity／offset、WRITE callback 無 ATT error 回傳能力，以及通知 payload copy 依 SDK binary 只讀查證；未為繞過限制修改 SDK。App／sensor link 分開，second peer rejection 不重啟既有 App 廣播，sensor disconnect 不清 App CCCD。尚未獨立 reviewer 審查。

**限制**：單 App link，table 五秒 TTL／八筆、僅辨識 EUP v1/v2；selected notify latest-only、MTU 不足時 read-blob、不發截斷資料。無強制加密／App AES／OTA／sensor-control relay，非法 write 僅忽略，SDK 可能仍回 ATT success。真正 App／sensor 雙角色、read-blob／MTU、CCCD／advertising recovery、heap／stack／長時間壓力尚待 Stanley 驗收。產物與燒錄狀態見 [App GATT 紀錄](docs/04_testing/07_evb_app_gatt_server.md)。本新功能尚未 commit／push。

---

# Addendum — Phase 1b.5 Gateway-style GATT（2026-10-21）

**D4 LED 增補**：依 Stanley 指示，專案 source 以 PA7 驅動 D4 Gateway 狀態、PA6 固定低電位關閉 D3。採 Gateway 的待機／資料／GATT 100／200／3000 ms 規則，移植到 sensor central link；不是認證成功指示。獨立 task 避免 UART blocking 影響更新；LED 模型 ASan／UBSan、四組 regression、ARM build 與 header／CRC／FlashEnv 檢查 PASS。SDK 新 patch 0，未改按鍵。實體亮滅、heap／stack 與時序由 Stanley 驗收，詳見 [LED 紀錄](docs/04_testing/06_evb_gateway_led.md)；本小改尚未獨立 reviewer 審查。

**HTML 相容性修正**：Stanley 的 17:46 log 揭露 `STYPESWITCH;0;;token` 被首版 parser 拒絕，未實際執行斷線。修正接受 HTML 的空 MAC 欄位與手動連線分號格式，保留簡寫並正規化 response。補 unit regression 與 mock session 經 HTML cancel handler 的清理／scan restore；不改 SDK、HTML 或連線生命週期。修正版數據與驗收狀態以 [GATT 紀錄](docs/04_testing/05_evb_gatt_client.md) 最新補充為準。

0.6.0-dev 新增手動／自動單一 central session、MTU／service／characteristic／CCCD discovery、Gateway-compatible AES challenge、metadata／raw command／config、Type 8 與八筆 auto list 保存。專案 overlay 引入 Gateway portable helpers 與 TinyCrypt AES subset；認證材料不輸出，SDK patch 新增 0。原 GTIME／CID keys、clock／linker／post-process 不變。

**Review**：獨立 reviewer 多輪檢查並修正取消／晚到 MASTER、零 status CONN_END、owner 釋放、DC 順序、舊 AP/MR、掃描啟動／恢復與 backpressure 問題。最後無確認的 Critical／Major production findings。另補成功即時 metadata 讀取的 cache 更新、失敗 command MAC/CMD echo。四組 ASan／UBSan host tests、ARM build 與獨立 header／CRC／payload／FlashEnv 邊界檢查 PASS。

**燒錄**：283,056-byte burn image，70 sectors，37.7 秒完成；readback 逐 byte PASS，reset 後 resumed。此結果不等於 UART app／GATT 成功。完整 metrics／hash／logs 與 Stanley 測試步驟見 [GATT 紀錄](docs/04_testing/05_evb_gatt_client.md)。Codex 未操作 UART／HTML，不做 sensor 認證、讀取或設定通過的宣稱。

**限制**：僅一個 metadata cache、一次 connect／auth；三秒認證期限包含 discovery。模糊零 status 結束事件無後續確認可保留 DRAIN 需 RST；持續 queue 過載可丟 ACK 並清理。Mock 不能證明真實 SDK callback 排程、radio 並行、heap／stack high-water 或持久化故障恢復。無 Sensor OTA、完整 Gateway retry policy／MAXRPT／whitelist。Overall：**host／build／flash PASS，GATT 硬體驗收 pending**。未 commit／merge／push。
