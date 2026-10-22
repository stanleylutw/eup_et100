# 07 — Download journal 設計（v0.8.2）

## 範圍與依賴

單一下載 session，僅使用 `nor_port.h` 的 read／program／erase／wait_ready；不初始化硬體、不開啟 write gate、不存取 boot state／Rollback。SHA-256 provider 由 caller 注入，host 與 EVB adapter 均使用既有 mbedTLS；模組不依賴 SDK，也不自行實作 SHA。caller 必須序列化 API，後端必須有獨立於 tick／DWT 的 timeout 上限。恢復先 wait_ready，再掃描。

<!-- CHECK: OTA_JOURNAL_CHUNK_SIZE == 4096 -->
<!-- CHECK: OTA_JOURNAL_ET100_MAX == 2097152 -->
<!-- CHECK: OTA_JOURNAL_SENSOR_MAX == 225280 -->

Journal：0x000000–0x01FFFF，32 個 4096-byte sectors。ET100 Candidate：0x020000–0x21FFFF，最多 512 chunks；sensor：0x420000–0x45FFFF，slot 有 64 sectors，但 package 上限 225280 bytes，最多 55 chunks。source LTE／BLE／RS232 僅記錄，無 transport 實作。

## 格式

所有欄位明確 little-endian，不將 C struct 直接寫入。sector slot 0 是 header；slot 1–15 各是一份 **完整 256-byte snapshot**，無 delta chain。每份包含 session metadata 與 512-bit bitmap，故 GC 不需拼接舊 entries。32-byte hash 識別 package，caller 提供非零 uint64 session ID。

| Offset／bytes | 意義 |
|---|---|
| 0／4 | magic=0x314C4E4A（little-endian `JNL1`） |
| 4／4 | sector header=0x00010001、snapshot=0x00010002（版本 1） |
| 8／8 | 單調遞增 sequence；header 表示本 sector 最早 snapshot sequence |
| 16／8 | session ID |
| 24／4 | NONE／DOWNLOADING／VERIFIED／FAILED／CANCELLED |
| 28／4 | ET100／SENSOR |
| 32／4 | LTE／BLE／RS232 |
| 36／4 | package length |
| 40／32 | package SHA-256 |
| 72／64 | bitmap，小端 byte bit index，超出 chunk count 的 bits 必須 0 |
| 136–243 | 保留 FF，納入 CRC |
| 244／4 | IEEE CRC-32，範圍 bytes 0–243 |
| 248／8 | ASCII `COMMIT1!`，獨立最後一次 PP |

寫入分成最多 8-byte PP。先寫 bytes 0–247、讀回比對，再寫 marker，最後全筆讀回驗證；marker 不完全吻合、CRC／metadata／bitmap 不合法者忽略。sequence 不 wrap；耗盡回錯誤。相同最大 sequence 若內容衝突，fail closed，不猜測。

CRC reflected polynomial=0xEDB88320、initial=0xFFFFFFFF、final xor=0xFFFFFFFF。sector header 的 bytes 16–243 為 FF，snapshot 才使用 metadata 欄位。state 數值 NONE=0、DOWNLOADING=1、VERIFIED=2、FAILED=3、CANCELLED=4；target ET100=1／SENSOR=2；source LTE=1／BLE=2／RS232=3。Bitmap bit n 對應 chunk n；最後 chunk 的精確長度是 length−4096×index，不把 slot padding 算入 SHA。

## 狀態與 API

M1：`write_chunk(j, session_id, index, data, length)`／`finish(j, session_id)`／`cancel(j, session_id)` 必須傳入非零且符合目前 session 的 ID，不符回 `OTA_ESESSION`，在任何 NOR read／write／erase／hash 前拒絕。`resume(j, manifest)` 要求 session_id／length／sha256／target 完全相同；source 可不同，只有 source 改變時提交一筆明確 takeover snapshot，不清 bitmap、不另開 session。同 source resume 為無 NOR 存取的冪等成功；只允許 DOWNLOADING／VERIFIED。

`recover` 重建 context；`begin` 從 NONE 或終態建立新 session，DOWNLOADING／VERIFIED 未 cancel 前禁止另一個 begin。`write_chunk` 在 DOWNLOADING 可寫入，VERIFIED 只接受內容相同的唯讀 duplicate；index 對應完整 4096 bytes，最後 chunk 可較短，sector 未用尾端保持 FF。先準備 journal 空間，再 erase data sector、PP、全 4096 bytes 讀回，最後 commit 新 snapshot，才回成功。全 FF chunk 也必須走此流程，不用內容判斷完成。

重複的已完成 chunk：讀回實際內容，完全相同回成功且不寫 NOR，不同回錯誤，不覆寫。`query` 只複製已提交狀態；API I/O 錯誤後 context 鎖定，必須 recover，不能以 RAM 猜進度。未完成 chunk 一律 erase 後重傳；本版故障模型內，最多重傳一個 4096-byte chunk（GC 不消耗已完成 bitmap），由 host oracle 明確 assert。

`finish` 要求全部 bits 完成，對精確 package length 做 SHA-256，比對後 commit VERIFIED 或 FAILED；VERIFIED 不代表可安裝／可信簽章。`cancel` commit CANCELLED 並清 bitmap，之後拒絕 write／finish；允許新 begin。cancel 不擦掉 slot 資料，權威為新終態 snapshot。

VERIFIED 持續占有 session，阻擋新的 begin，直到明確 cancel 或未來 installer 消費／釋放；本版沒有 installer release API，因此目前需 cancel 才能 begin。

## 恢復與 GC

恢復掃描所有有效 header 及全部 snapshot slots，取最大有效 sequence；全部無效即 NONE。僅掃描有 CRC／marker 的 metadata，不用全 FF 推論某位置「從未被 program」。**每次 recover 後首次 mutation 必須換 sector**，即使舊 tail 讀起來全 FF：它可能是完全未改變可見 bit 的 torn PP；這避免再次 program 同一個未知位置。已知完整新 erase 的 sector 才可順序 append。

換 sector 選 active+1（循環），先 erase＋全 sector 驗證 FF、提交 header，再提交當前完整 snapshot；只有新 snapshot commit 並讀回後，才 erase 舊 sector。新 begin 可直接把新 session 作為新 sector 的第一份 snapshot。一般 GC 先複製舊狀態再繼續 mutation。任何失敗都要求重新 recover；不在失敗位置重試 PP。舊 sector erase 失敗時，新 snapshot 仍是權威。

| 中斷點 | 恢復／後果 |
|---|---|
| 新 sector erase，含讀成 FF 的 weak sector | 舊 snapshot 保留；無有效 header 的新 sector 下次使用前重新 erase |
| header body／marker | 無有效 snapshot，不取代舊狀態；不重用其 tail |
| 新 snapshot body／marker | 沒有有效 commit 就使用舊 snapshot；有效完整 commit 可恢復，即使 ACK 遺失 |
| GC 新 snapshot commit 後、舊 erase 前／期間 | 新 sequence 更大，保留所有已 commit bits；舊 erase 不影響新 sector |
| data erase／PP／verify | chunk bit 未 commit，不完成；下次重新 erase，不能信任全 FF |
| chunk snapshot commit | 完整有效者才進 bitmap；傳輸 ACK 遺失可冪等重送 |
| finish／cancel snapshot | 舊狀態或完整新狀態，沒有半個終態 |

CRC 不提供抗惡意修改／簽章保證；不宣稱能抵抗任意永久性媒體損壞或 CRC 碰撞。host weak 模型只針對被中斷 erase 的 sector，完整成功的重新 erase 才清除 weak；故不會在未重新 erase 的 weak sector 新增 header／chunk commit。

殘餘風險：本輪未測 cut-interrupted program 的 weak marker 延遲衰退；≤1 chunk 上限僅限現有故障模型，完整成功 program 並驗證之後的 retention／媒體故障亦不在涵蓋範圍（依 Stanley 縮減範圍，不擴充模型、不改 GC）。

## 驗證 Gate

M-HOST：ASan／UBSan 功能、逐操作斷電、連續 3 次、固定 seed 2 MB／10000 次注入、GC 逐操作矩陣，包含 partial byte／partial bit 與 weak erase。計數與限制寫入 [10 測試報告](../04_testing/10_download_journal.md)。M-HOST 停下 review；本輪不燒錄、不寫 U11。實機 reset、傳輸效能、實際斷電與 ET100 高地址不由 host PASS 推論。
