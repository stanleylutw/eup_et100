# 10 — Download journal M-HOST 測試報告

## 2026-10-22 M1 與縮減 M-EVB（最新）

Stanley review 後要求 session identity 與顯式 takeover，後續縮減範圍為：不做 weak-program marker 衰退擴充、不改 GC；host 全矩陣通過後，M-EVB 僅一份 2 MB package、三次 MCU reset 中斷／resume、最終 SHA 一致，之後恢復 0.7.0-dev。不做 timing statistics、sensor package 實機測試或額外 U11 characterization。以下初版 M-HOST／20-reset 等內容保留為歷史，不是本輪實機授權。

### M1 與 Host 重跑

- `write_chunk／finish／cancel` 改為必填 uint64 session_id；不符回 OTA_ESESSION（-26），沒有 NOR 存取，包含 VERIFIED 的冪等路徑也先檢查。
- `resume(manifest)` 核對 session_id／length／sha256／target；source 變更必須提交完整 takeover snapshot，bitmap 不變。同 source 為不寫 NOR 的冪等成功，CANCELLED／FAILED 不接受 resume。
- 規則測試以 NOR operation counter 驗證三種錯誤 session API、四種 manifest mismatch 均零存取；source takeover 經 recover 後仍存在。VERIFIED 阻擋 begin 直到 cancel，本版尚無 installer-release API。
- Oracle 明確 assert：相較「已 commit／已恢復的 bits ＋當次嘗試 chunk」，恢復後缺少的 chunk 數 ≤1；未來從未嘗試的 chunks 不計。仍另外要求所有已 commit／已恢復 bits 不遺失、每個完成 bit 的資料正確。
- 16-chunk 完整流程在第 8 chunk 後加入 source takeover；GC matrix 從滿 sector checkpoint 執行 takeover，再繼續 chunk。三種既有斷電模型不變；weak-program 衰退擴充已移除，限制見 07 的殘餘風險一行。

| 矩陣 | 注入／PASS |
|---|---:|
| 小 package 每操作窮舉（含 takeover） | 30,192／30,192（10,064 operations ×3 modes） |
| GC＋takeover 每操作 | 2,187／2,187（729×3） |
| 連續三次中斷 | 9／9 |
| 2 MB 固定 seed 隨機 | 10,000／10,000 |
| 合計 | **42,388／42,388** |

全組 ASan／UBSan PASS。Log `/tmp/et100_journal_m1_host_20261022.log`；使用下方重現指令同一組 sources／flags，執行檔 `/tmp/et100_test_ota_journal_m1`。已核對 fake_nor 沒有殘留 weak-program pending／reboot 擴充。

ARM rebuild PASS，header／CRC／payload／800 KB guard PASS；text=35,688、data=4,336、bss=13,144 bytes，raw=40,024、burn=48,216 bytes。SHA-256 `77b681aadc7bc3da3814723bd99563676f39989a388ffdb0876a84308980908b`。Log `/tmp/et100_journal_m1_build_20261022.log`。

### 縮減 M-EVB

**PASS（縮減版）**。本輪依 Stanley 授權將上述 48,216-byte image 燒入 EVB，逐 byte readback PASS，boot `G1 init_rc=0 writes=locked`、function C、JEDEC C86018。Logs：`/tmp/et100_journal_mevb_flash_20261022.log`、`/tmp/et100_journal_mevb_boot_20261022.log`。

只建立一次 session `83E19A4700200000`，ET100 Candidate `0x020000–0x21FFFF`，長度 2,097,152 bytes，seed `0x83E19A47`。在 chunk 128／256／384 的 jwrite 過程，分別由 port-call 邊界 200／400／528 觸發 MCU reset（NOR 保持供電）；恢復後既有 128／256／384 個完成 chunks 不遺失，各重寫當次未 commit 的 chunk，再繼續同一 session。沒有 cancel／重開 package，也沒有 sensor 實機下載。

| MCU reset | 下載中位置（0 起算） | 恢復／resume |
|---|---:|---|
| 1 | chunk 128 | UART collector 在 reset 雜訊停止；唯讀確認 ID／SR 正常、jquery=-20（RAM journal 未恢復），jrecover 成功找回原 session 與前 128 bits；顯式 source 3→1 takeover，重寫 chunk 128 後續傳 |
| 2 | chunk 256 | 重開機 G1=0、恢復 bitmap；source 1→2 takeover，續傳 |
| 3 | chunk 384 | 重開機 G1=0、恢復 bitmap；source 2→3 takeover，續傳 |

最終 `jfinish SESSION_ID` 成功，jquery=VERIFIED（state=2）、512 bits 全 1；再以獨立 `sha 0x20000 2097152` 直接讀回整份 Candidate，與 host hashlib 相同：

```text
222C666A1C1C081B7BB4903B9643238CC401003AC7C7DF70796DB63A63A9137E
```

第一次 collector 以 strict ASCII 解碼，reset 時非 ASCII byte 0xF5（位置 2）使其結束，沒有完整捕捉該次 boot。此為 host 收集失敗，不隱藏為完整 boot log。之後只唯讀確認原 session，修 runner 為 reset 路徑保存 RX_RAW／backslashreplace，仍以完整 G1=0／READY 判定新的 reset 成功；一般 command 回覆保持 strict decode。第二、第三次均捕捉到 raw prefix `4A4FF5...` 後接完整 boot/G1。以 `--journal-resets-done 1` 連回同一 manifest，未重新計數第一次、未多做第四次 reset。Runner 現在也拒絕自動 cancel 不同／已完成的 session，僅對完全吻合的 DOWNLOADING manifest 顯式 resume。

原始完整證據：第一次下載 `/tmp/et100_journal_mevb_20261022.log`（collector failure 保留於 `/tmp/et100_journal_mevb_runner_20261022.log`）；第一次 reset 後唯讀確認 `/tmp/et100_journal_mevb_reset1_capture_20261022.log`；同 session 後續下載／兩次 reset／finish／整區 SHA `/tmp/et100_journal_mevb_resume_20261022.log`。沒有做 timing statistics；log 時戳僅供事件順序，**STIG 8-byte PP, not OTA-representative**。

新 console：`jwrite SESSION_ID INDEX SEED`、`jfinish SESSION_ID`、`jcancel SESSION_ID`，ID 支援完整 64-bit 十六進位；`jresume TARGET LENGTH SEED SOURCE` 重建 pattern manifest 後呼叫核心 resume（測試 session_id 由 seed／length 組成）。`jrecover` 僅記 rc、不輸出 timing statistics。Runner 的縮減 scenario 預設且限定三次 MCU reset；最終另外 `sha` 直接讀回整份 Candidate，以 host hashlib 期望值比對。

**基線恢復 PASS，硬體操作已停止。** M-EVB 後將 284,712-byte 備份（SHA-256 `effbd9aad13fb8c134ee9f39f475413e8287f78a7bab4e4f2e3007ef526921ec`）燒回，完整逐 byte readback PASS，9600 bps 唯讀 `BLEGW$GCHK;J082` 回覆 `AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.7.0-dev;0;J082`。原始 UART frame 前有一個非 ASCII byte，保留於 log；版本及命令 task 回應正常，不擴大宣稱 App／BLE 重新驗收。Logs：`/tmp/et100_journal_mevb_restore_flash_20261022.log`、`/tmp/et100_journal_mevb_restore_uart_20261022.log`。

本輪未改 GC、SDK、linker、FAL 或產品 source；07 記錄 weak-program 延遲衰退未測風險，未保留擴充測試。未做切電、OTP／non-volatile status write、sensor 實機下載或額外性能／晶片鑑定。Candidate／journal 保留此次驗證資料，沒有為清理而額外擦除。M1＋縮減 M-EVB 到此停止，等待 review；不 commit／push／merge，Step 7 其他文件同步留待 review。

---

## 初版 M-HOST 歷史

日期：2026-10-22；分支 `v0.8.2_download_journal`，base `a425358`。執行者 Codex；**M-HOST PASS，停止等待獨立 review；M-EVB 未執行**。本輪無 UART／SWD／燒錄／U11 寫入，板上維持前輪已恢復的 0.7.0-dev，沒有再次查詢硬體。

## 實作與邊界

依 [v0.8.2 plan](../../IMPLEMENTATION_PLAN.md) 與 [07 設計](../01_firmware/07_download_journal_design.md)，新增可攜 `ota_journal`、NOR 斷電模型與測試。sector header／snapshot 都有 CRC＋最後獨立 PP 的 commit marker；每筆完整 bitmap snapshot，GC 先 commit replacement 才 erase 舊 authority。recover 後不重用未知 append tail。4 KB chunk 成功完成 erase／program／全 sector 比對及 snapshot commit 後才 ACK。第二個 live begin、不同內容 duplicate、cancel 後 write 都被拒絕。

只涉及 journal、Candidate、sensor slot；Rollback／boot state 不觸碰。SHA provider 使用既有 mbedTLS；journal 核心只依賴 nor_port 與 callback。CRC 為完整性檢查，不是簽章／抗惡意機制。state VERIFIED 只表示 package SHA 符合 begin 值，不代表可安裝。

## Host 結果（ASan／UBSan）

| 類別 | 注入／PASS 次數 | 驗證內容 |
|---|---:|---|
| 16 chunks 逐操作窮舉 | 30,090／30,090 | 正常流程 10,030 個 NOR 操作，每點各用三種 tear mode；含 begin、資料、metadata、GC、finish/hash 及恢復後續傳 |
| GC 每步窮舉 | 2,187／2,187 | 14 chunks 已 commit 的滿 sector checkpoint，729 個操作×3 modes；涵蓋新 header／snapshot 與舊 erase |
| 連續中斷 | 9／9 | 三種 mode，各連續三次中斷，含 recover 及 GC header 工作；之後清空 journal RAM 再恢復完成 |
| 2 MB 固定 seed 隨機 | 10,000／10,000 | seed=0x20261022，隨機 chunk index、操作點與 tear mode，期間可跨多個 session；最後完成完整 2 MB 並驗證 SHA |
| 規則測試 | PASS，非斷電計數 | ET100／sensor 上限與 55-chunk sensor package、短尾頁、全 FF chunk、重複相同／不同內容、第二個 begin、cancel、finish 不完整、hash mismatch 後 FAILED 恢復 |

合計 **42,286 次斷電注入**。三種 mode：partial-byte PP＋混合 erase、partial-bit PP＋全 FF weak erase、未改變可見 bit 的 PP＋全 FF weak erase。read／wait 操作被切斷只造成重啟；program／erase 則持久化部分結果。weak sector 後續讀取可翻 bit，只有完整成功的新 erase 才清除 weak。

Fake backend 強制：PP ≤8 bytes、不可跨 page／8-byte block、1→0、sector erase 對齊；任何已嘗試 PP 的位置（即使仍全 FF）在下次成功 erase 前不可再 program。模組 RAM 在 recover 前清空。Oracle 在每次恢復檢查所有已 ACK bits 不遺失，所有恢復出的完成 bit 都指向正確、非 weak 的 package 資料；正常 ACK 同樣逐 chunk 更新。recover 找回的已 commit／遺失 ACK bits 也立即納入後續不得遺失的 oracle，強化後整組矩陣已重跑通過。中斷最多使當次未 ACK 的一個 4096-byte chunk 需要重傳，已 ACK chunks 不重傳；未來尚未下載的 chunks 不計入重傳量。

這是明確故障模型下的證據，不代表任意 bit corruption／CRC 碰撞／永久壞 sector 或真實掉電時序的完整證明。cancel 的功能恢復已測，未另建立 cancel 每個 PP 點的專屬斷電窮舉；begin／write／finish／GC 為本輪主要斷電路徑。

完整 log：`/tmp/et100_journal_host_20261022.log`。首輪規則測試曾讓「刻意破壞 package 以驗證 FAILED」通過只接受正確 chunk 的正常 oracle，導致測試 assertion；已將此負向案例改為直接驗證持久化 FAILED，未放寬一般恢復 oracle、未修改 crash matrix 期望。最終整組重跑 PASS。

重現指令（SDK 僅讀取既有 SHA 實作，不修改、不安裝）：

```sh
cc -std=c11 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined \
  -Ifw/src -Ifw/tests \
  -Ifw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/include \
  fw/src/ota_journal.c fw/tests/fake_nor.c fw/tests/test_ota_journal.c \
  fw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/library/sha256.c \
  fw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/library/platform_util.c \
  -o /tmp/et100_test_ota_journal
/tmp/et100_test_ota_journal
```

## Build 與工具檢查

ARM `bash fw/tools/build_evb_nor.sh` PASS，FR509x header／CRC／payload／800 KB guard PASS。text=35,008、data=4,336、bss=13,144 bytes；raw=39,344、burn=47,536 bytes。SHA-256 `50dbc4a8864093ef375d89ae06731419899a31823f95b6151c257191c2ed40a1`，測試 banner=0.8.2-dev。**未燒錄**。保留既有 RWX segment linker warning，未改 linker／SDK。Build log：`/tmp/et100_journal_build_20261022.log`。

既有 NOR backend sanitizer 回歸另跑，log `/tmp/et100_journal_nor_regression_20261022.log`。新增 07 的三個 CHECK anchors，`check_plan_vs_code.py` 通過 3／3，不再是沒有 anchors 的 no-op。Runner syntax 與 `--help` 僅 host 檢查，不開啟 UART；Python cache 指定 `/tmp`（初次預設 cache 路徑受 sandbox 拒絕，已改到暫存路徑）。

本輪 scoped `git diff --check`、新增檔案 whitespace 與文件相對連結檢查 PASS。全 repo `git diff --check` 仍回報使用者既存硬體 README 第 177 行 EOF 空白行；該檔及 PCB 資料未觸碰，沒有為通過檢查而清理範圍外檔案。

## M-EVB 候選（未執行）

Console：`jrecover`、`jquery`、`jbegin TARGET LENGTH SEED SOURCE`、`jwrite INDEX SEED`、`jfinish`、`jcancel`。`jcut N` 在後續第 N 次成功的 journal port read／program／erase／wait 返回後做 MCU reset，**不是 NOR power-cut，也不保證 reset 位於 WIP 中間**。不自動 arm；jrecover 先 wait_ready，回報 us。此 wrapper 的 SHA callback 不計入 jcut counter；GC／資料／record 操作有計數。重新啟動後需明確 arm 才能續寫。

Runner 新增 `--journal-target et100|sensor` 與 `--journal-resets`（至少 20），需 `--allow-write`，拒絕混合 power／任意 command 選項。預設 package 2 MB／225280 B、pattern seed=0x83E19A47；host 獨立算 SHA，MCU begin 值必須一致。每次 jcut 前 recover 退役 tail，確保涵蓋 GC；重啟後檢查原 bitmap 不丟失，記錄 missing chunk 的重傳 bytes 與 JRECOVER us，再續傳到 finish。每個 target 各執行一次，最後依 plan 恢復 0.7.0-dev。

僅在 Stanley 核准 M-EVB 後使用：

```sh
python3 fw/tools/evb/nor_test_host.py PORT --allow-write --timeout 120 \
  --journal-target et100 --log /tmp/et100_journal_evb_et100.log
python3 fw/tools/evb/nor_test_host.py PORT --allow-write --timeout 120 \
  --journal-target sensor --log /tmp/et100_journal_evb_sensor.log
```

上述程式已編譯／語法檢查，尚未實機驗證，不能列為 M-EVB PASS。**STIG 8-byte PP, not OTA-representative**。G2 取消／T8 延到 ET100 是新 plan 決策；06／09／status／REVIEW_REPORT 的 Step 7 同步留待 M-EVB review 後，依本輪 scope 不預先修改這些檔案。無 commit／push／merge。
