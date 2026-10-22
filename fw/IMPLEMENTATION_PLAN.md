# IMPLEMENTATION_PLAN — v0.8.7 裝置端 GET_FW_INFO／GET_DEVICE_ID（App 相容性查詢）

> **Claude 實作**（Stanley 2026-10-22 指定）。Step 5 review 由 Codex 做。

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.8.7_device_metadata

Base branch: v0.8.6_ble_smp（6817f6c）

## 0. 工作模式與決策

採用 comm.md「Hardware bring-up loop」，**測試不做太細**。Stanley 2026-10-22 決定：

| 決策 | 內容 |
|---|---|
| D1 | Claude 實作，Codex review |
| D2 | **照 eup_gateway**：EUP AIoT Communication Service 只做 4 個唯讀指令、**不需驗證**、**一律註冊**（不受 `ET100_SMP_BLE_OPEN` 控制）；寫入（SMP）的安全 gate 不變（06 §6.1） |
| D3 | RS232 新增 `BLEGW$GFWINFO;<token>`，讓 HTML 在 9600 下比對裝置與檔案的硬體 |

| Milestone | 內容 | 之後 |
|---|---|---|
| M-META | Host：新回應經 FR3068 既有 client parser（`src/eup_aiot_metadata.c`）正確解析；EVB：`GFWINFO` 正確、BLE service 存在且 PING 有回應 | 停下，等 Codex review |

授權：燒錄本 branch App（以 SMP serial 部署）；0x08001000 sector 規則不變。

## 1. 參考來源（照抄行為，不自創）

- 規格：`eup_gateway/docs/protocol_cache/02_eup_aiot_uuid_communication_protocol.md` §3（UUID）、§5（packet）、§6–§7（command／status）、§8–§11（TLV）
- 參考實作：`eup_gateway/src/service/eup_ota_metadata_service.c`（被更新端，4 個指令，無驗證）

| 項目 | 內容 |
|---|---|
| Service | `E9E00000-4F4C-4F47-4943-455550000001`；RX `E9E00001`（Write／Write Without Response）；TX `E9E00002`（Notify） |
| Request | `E9 55 01 cmd seq len payload checksum`（checksum＝前面所有 byte 加總 & 0xFF） |
| Response | `E9 55 01 cmd\|0x80 seq status part total len payload checksum`；超過一包時分 part，**以 byte 切分（可能拆開 TLV，與 eup_gateway 實作相同）**，App 串回全部 part 再解析（2026-10-22 依 Codex review 統一；原寫「不拆 TLV」與實作不符） |
| 指令 | `0x01 GET_FW_INFO`、`0x05 GET_DEVICE_ID`、`0x07 GET_CAPABILITIES`、`0x7F PING`；其他回 `0x01 UNKNOWN_COMMAND`；格式錯回 `0x02 INVALID_PACKET` |
| GET_FW_INFO TLV | 0x01 project、0x02 hardware、0x03 firmware、0x04 build_time、0x05 build_id、0x06 git_commit、0x07 git_branch、0x08 ble_name_normal、0x09 ble_name_ota、0x0A release_note、0x0B metadata_schema、**0x0C soc=`fr3068e_c`**、0x0D protocol_version；字串取自 App 內的 EUPFWV1 block |
| GET_DEVICE_ID | 0x01 `mac_string`＝BLE identity address（與 GCHK identity 同源，例如 `C0:32:32:32:59:03`） |
| GET_CAPABILITIES | 依 gateway：`FW_INFO`、`OTA_READY`、`DEVICE_ID` 等 bit，以及 max payload／preferred MTU（數值照 FR3068 實際） |

## 2. 檔案範圍

| 檔案 | 內容 |
|---|---|
| `fw/src/eup_meta_protocol.c／.h`（新） | 與 transport 無關的 packet 解析、checksum、TLV 組裝、分 part（host 可測） |
| `fw/src/eup_meta_service.c／.h`（新） | BLE GATT service（RX write → 處理 → TX notify），沿用 v0.8.6 的 notify 完成計數作法 |
| `fw/src/evb_serial_ble.c` | 一律註冊 service；連線事件轉給 service |
| `fw/src/serial_gchk.c` | `BLEGW$GFWINFO;<tok>` → `AT$PD=BLEGW$GFWINFO;1;<project>;<hardware>;<firmware>;<soc>;<tok>` |
| `fw/targets/evb_serial.mk` | 新檔案；版本 0.8.7-dev |
| `fw/tests/` | 協定 host test：回應交給 `src/eup_aiot_metadata.c` 解析；checksum／錯誤 packet／未知指令／分 part；GFWINFO |
| `fw/docs/02_protocol/05_device_metadata.md`（新） | 給 App 團隊與 HTML 的對照（BLE 與 RS232 兩種查法） |
| `fw/docs/04_testing/15_evb_device_metadata.md`（新） | 測試報告（精簡） |

**不做**：GATT window auth（0x10／0x11）、CONFIG_SET、GET_BLE_MODE 等其他指令、App 修改、SDK 修改。

## 3. 驗收（精簡）

1. Host：四個指令的回應都能被 FR3068 既有 client parser 解出正確欄位；壞 checksum／未知指令回對應 status。
2. EVB：UART `GFWINFO` 回 `EUP_ET100;ET100_EVB_DEV1.4;0.8.7-dev;fr3068e_c`。
3. EVB：BLE 上看得到 EUP AIoT Communication Service（例如手機 nRF Connect 掃描服務清單，或 SWD 確認註冊成功）；PING 有回應（選做，需手機時請 Stanley 協助）。
4. SMP 更新流程（v0.8.6）不受影響；B 資訊段不變。

## 4. 交付

> **實作結果（2026-10-22）**：M-META PASS，見 [15 報告](docs/04_testing/15_evb_device_metadata.md)。

M-META 停下，請 Codex review。Step 7 同步 06 §6.1（App 端待辦更新為「韌體端已提供」）、platform plan、status MD、REVIEW_REPORT；commit 等 Stanley 說「git commit」。

---

# 歷史施工單 — v0.8.6 BLE SMP（BLE 更新入口）

> **備用規則：Claude 實作**（comm.md「備用規則」）。Step 5 review 由 Codex 恢復後補做（連同 v0.8.5）。

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.8.6_ble_smp

Base branch: v0.8.5_ota_manager_smp_serial（62ec148）

## 0. 工作模式與決策

採用 comm.md「Hardware bring-up loop」，**測試不做太細**。Stanley 2026-10-22 決定：

| 決策 | 內容 |
|---|---|
| D1 | Claude 實作（備用規則） |
| D2 安全 | **A：EVB 先開放**，以 build flag `ET100_SMP_BLE_OPEN` 控制，只有開發版打開；未定義時 BLE SMP service 不註冊。06 寫明「**量產前一定要有簽章（Q42）與 BLE 認證**」 |
| D3 | `GET_FW_INFO`／`GET_DEVICE_ID`（被更新端）移到 v0.8.7 |
| D4 測試 | BLE 實機測試由 Stanley 用手機 **nRF Connect Device Manager** 操作；Claude 同時以 UART／SWD 確認裝置狀態 |

| Milestone | 內容 | 之後 |
|---|---|---|
| M-BLE | 手機以 nRF Connect Device Manager 完成「上傳 → Test → Reset → 安裝 → 自動確認」 | 停下，等 Codex review |

授權：燒錄本 branch 的 App、寫入 U11 scratch；0x08001000 sector 規則不變。

## 1. 架構

```text
BLE write（SMP characteristic）─┐                         ┌─ notify（BLE，依 MTU 分段）
                                ├─→ queue → OTA worker task → smp_server → ota_manager
UART3 SMP frame ────────────────┘   （單一 owner）          └─ UART3 framing
```

- **OTA worker task**（新）：唯一呼叫 `smp_server_process`／`ota_manager` 的 task。serial 與 BLE 都把完整 SMP 封包放進 queue，回應依來源 transport 送回。同時只能有一個更新 session；另一個 transport 的 upload／test 回 `EBUSY`（state read 仍可回應）。
- **SMP GATT service**：service `8D53DC1D-1DB7-4CD3-868B-8A527460AA84`，characteristic `DA2E7828-FBCE-4E01-AE9E-261174997C48`（write without response＋notify），與 App manager service 並存於同一條連線。
- **Reassembly**：依 SMP header 的長度把多次 write 拼成完整封包（上限 1024 B，超過或逾時 2 秒丟棄）；回應超過 MTU−3 時分多個 notify 送出，等每個 notify 完成再送下一個。
- **與既有功能共存**：BLE SMP 進行中（最後一個封包 30 秒內）不主動連 sensor、不送 Type-1 回報；App manager service 照常。
- `boot_health` 的自檢、watchdog 清除不受 worker 阻塞影響（worker 優先序不高於 serial task；長時間 hash 期間仍讓出 CPU）。

## 2. 檔案範圍

| 檔案 | 內容 |
|---|---|
| `fw/src/smp_ble.c／.h`（新） | GATT service、reassembly、分段 notify |
| `fw/src/ota_worker.c／.h`（新） | queue、單一 owner、transport 回呼 |
| `fw/src/serial_gchk.c` | SMP 封包改丟給 worker（GCHK／OTABAUD 不變） |
| `fw/src/evb_serial_ble.c` | 在 `ET100_SMP_BLE_OPEN` 下註冊 SMP service；連線／MTU 事件轉給 `smp_ble` |
| `fw/src/ota_manager.c` | 記錄 session 的 transport（`source`），跨 transport 回 `EBUSY` |
| `fw/targets/evb_serial.mk` | 新檔案；開發版預設定義 `ET100_SMP_BLE_OPEN` |
| `fw/tests/` | reassembly／分段、跨 transport busy 的 host tests |
| `fw/docs/02_protocol/04_smp_serial_ota.md` | 增加 BLE transport 一節（UUID、MTU、reassembly） |
| `fw/docs/04_testing/14_evb_ble_smp.md`（新） | 測試報告（精簡） |
| `fw/docs/01_firmware/06_ota_storage_design.md` | §6.1 安全聲明、roadmap |

**不做**：`GET_FW_INFO`／`GET_DEVICE_ID`（v0.8.7）、BLE 認證與簽章（量產 gate）、自家 App 修改、SDK 修改。

## 3. 驗收（精簡）

1. 手機 nRF Connect Device Manager 連上 EVB，看得到 image 清單（slot 0 版本、hash）。
2. 上傳新版 `dfu_application.zip` → Test → Reset → installer 安裝 → 新 App 自動確認；GCHK 版本變更。
3. BLE 上傳中，UART 的 SMP upload 回 `EBUSY`（或反之），state read 正常。
4. 原有 App manager service 與 GCHK 照常；B 資訊段不變。
5. 不定義 `ET100_SMP_BLE_OPEN` 的 build 看不到 SMP service（host 或 build 檢查即可）。

## 4. 交付

> **實作結果（2026-10-22）**：M-BLE PASS，見 [14 報告](docs/04_testing/14_evb_ble_smp.md)。新增 OS bootloader info（實機發現 nRF Connect Device Manager 需要）；worker 改用兩個靜態請求槽（避免占用 50 KB heap 的 queue）。

M-BLE 停下；Codex 恢復後 review v0.8.5＋v0.8.6。Step 7 同步 06、platform plan、status MD、REVIEW_REPORT（標明未經獨立 review）；commit 等 Stanley 說「git commit」。

---

# 歷史施工單 — v0.8.5 OTA manager 進 App＋SMP serial（RS232 入口）

> **備用規則：Claude 實作**（comm.md「備用規則」，Codex 無 token）。Step 5 review 由 Codex 恢復後補做。

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.8.5_ota_manager_smp_serial

Base branch: v0.8.4_trial_rollback（182782f）

## 0. 工作模式與決策

採用 comm.md「Hardware bring-up loop」，**測試不做太細**。Stanley 2026-10-22 決定：

| 決策 | 內容 |
|---|---|
| D1 協定 | RS232 入口採 **SMP over serial**（MCUmgr UART transport），不用 ZVERO；與 v0.8.6 BLE 共用同一個 SMP server |
| D2 Port | 開發先用 EVB **UART3**（DAPLink VCOM，9600，與 GCHK 共用）；ET-100 的更新 port 待實板決定，程式中做成可設定 |
| D3 HTML | 韌體完成後，`tools/rs232_tester.html` 再依本協定新增功能（不在本 branch） |

| Milestone | 內容 | 之後 |
|---|---|---|
| M-SMP | 用現成 SMP client 經 UART3 完成「upload → test → reset → installer 安裝 → 新 App 自動 confirm」 | 停下，等 Codex review |

授權：燒錄本 branch 的 App／installer、寫入 U11 scratch；0x08001000 sector 規則不變。

## 1. 架構

```text
UART3 ISR（1 byte IT，既有）→ ring buffer → serial task 分流
   ├─ ASCII 行（BLEGW$…）→ 既有 GCHK parser（不變）
   └─ SMP frame（開頭 0x06 0x09 或續段 0x04 0x14）→ SMP serial decoder（base64＋CRC16＋長度）
                                                     → SMP server（transport 無關）
                                                        → OTA manager（單一 session）→ ota_journal → nor_qspi1（U11）
                                                        → boot_state（test → PENDING）
```

- **OTA manager**（新 task）：持有 journal／NOR，唯一寫入者；SMP 請求以 queue 交給它，回應再送回 transport。
- **NOR 在 App 內**：重用 `nor_qspi1.c`；等待 WIP 期間改用 `vTaskDelay(1)` 讓出 CPU（保留既有 timeout／非 DWT 上限）。STIG 關中斷區段很短，但需確認 UART3 9600／115200 不掉 byte（ring buffer 足夠大，ISR 只搬資料）。
- **4 KB 組包**：SMP upload 每包大小不固定；RAM 湊滿 4 KB 才 `write_chunk`。回給 client 的 `off` = 本次接收位置；重新連線時回可恢復位置（連續已 commit 的 byte 數），從那裡續傳。（2026-10-22 依 Codex review 更正：原寫「off = 已 commit」不正確）
- **第一包**：`len`＋`sha`（整份 image SHA-256）→ journal `begin`／`resume`（session_id 由 sha 前 8 bytes 導出）。
- 上傳完成 → journal `finish` → VERIFIED。`image state` 的 `test`（帶 hash）→ boot state PENDING。`os reset` → 重開 → installer 安裝（v0.8.3／v0.8.4 流程）。

## 2. SMP 子集

| Group／ID | 指令 | 實作 |
|---|---|---|
| OS (0) / 1 | reset | 回應後 reset |
| OS (0) / 0 | echo | 選做（方便連線測試） |
| IMG (1) / 0 | state read | 回報 slot 0（目前 App：version、hash、confirmed）與 slot 1（Candidate：hash、pending） |
| IMG (1) / 0 | state write（test／confirm） | test → PENDING；confirm 只接受目前 App 自己的 hash（v0.8.4 規則） |
| IMG (1) / 1 | upload | off／data／len／sha，續傳 |
| 其他 | — | 回 `rc = MGMT_ERR_ENOTSUP` |

CBOR：只做需要的子集（map／uint／bytes／text／bool），自寫或用小型函式庫（例如 zcbor 的子集）；**不修改 SDK**。

## 3. Baud 切換（06：平常 9600、更新 115200）

```text
Host  → BLEGW$OTABAUD;115200;<token>
ET100 → AT$PD=BLEGW$OTABAUD;1;115200;<token>    （送完才切換）
雙方切 115200 → SMP → 完成／cancel／30 秒無 SMP frame → 自動回 9600
```

只接受 9600／115200。開發初期可先在 9600 打通 SMP，再加 OTABAUD。

## 4. 檔案範圍

| 檔案 | 內容 |
|---|---|
| `fw/src/ota_manager.c／.h`（新） | OTA task、session、4 KB 組包、journal／boot state 介接 |
| `fw/src/smp_server.c／.h`（新） | transport 無關的 SMP 處理（v0.8.6 BLE 共用） |
| `fw/src/smp_serial.c／.h`（新） | UART framing（base64、CRC16、分段） |
| `fw/src/cbor_min.c／.h`（新） | CBOR 子集 |
| `fw/src/serial_gchk.c` | ring buffer 分流、`OTABAUD` 指令 |
| `fw/src/nor_qspi1.c` | App 內等待改為讓出 CPU（介面不變） |
| `fw/src/evb_serial_main.c`、`fw/targets/evb_serial.mk` | 建立 task、加入新檔案 |
| `fw/tests/` | CBOR、SMP serial framing、upload 續傳／4 KB 組包的 host tests |
| `fw/tools/requirements-smp.txt`（新） | 現成 SMP client（`smpmgr` 或等效）安裝到 `fw/.venv`，版本鎖定 |
| `fw/docs/02_protocol/04_smp_serial_ota.md`（新） | 給之後 HTML 工具用的協定說明（framing、指令、OTABAUD、流程範例） |
| `fw/docs/04_testing/13_evb_smp_serial.md`（新） | 測試報告（精簡） |
| `comm.md` | 備用規則（已加入） |

**不做**：BLE transport（v0.8.6）、HTML 工具、簽章、LTE、sensor target、SDK 修改。

## 5. 驗收（精簡）

1. `smpmgr`（或等效）經 UART3：image state read 正確顯示目前 App。
2. 上傳 `et100.signed.bin`（新版本號）→ test → reset → installer 安裝 → 新 App 自動 confirm；GCHK 版本變更。
3. 上傳途中拔掉／中斷 host 連線一次 → 重連後從 `off` 續傳完成。
4. 上傳期間 GCHK 指令仍可回應（分流正常），UART 無掉 byte（以 SHA-256 一致為準）。
5. OTABAUD：115200 完成一次上傳；逾時自動回 9600。
6. B-info SHA-256 不變。

## 6. 交付

> **實作結果（2026-10-22）**：M-SMP PASS，見 [13 報告](docs/04_testing/13_evb_smp_serial.md)。與本施工單的差異：OTA manager 在 serial task 內執行（未開獨立 task，SMP 一問一答不會掉 byte；BLE 時改 worker task）；SMP client 用 `smpclient`（`smpmgr` 需 Python ≥ 3.10）。

M-SMP 停下；Codex 恢復後做 Step 5 review。Step 7 同步 06 §6／§8.1、platform plan、status MD、REVIEW_REPORT（標明實作者為 Claude）；不 commit／push（等 Stanley 說「git commit」）。

---

# 歷史施工單 — v0.8.4 Trial／Confirm／Rollback

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.8.4_trial_rollback

Base branch: v0.8.3_installer（5dd5d0e）

## 0. 工作模式、決策與授權

**採用 [comm.md](../comm.md) 的「Hardware bring-up loop」**。Stanley 2026-10-22 核准 D1–D4，並要求**測試不要做太細**。

| 決策 | 內容 |
|---|---|
| D1 confirm | **App 自己確認**：BLE 開始廣播、FlashEnv 讀寫正常、main loop 持續 30 秒 → 寫入 CONFIRMED。v0.8.6 的 SMP `confirm` 之後也接受，但非必要 |
| D2 機會次數 | **1 次**：TRIAL 狀態下重開（未確認）即回滾，同 MCUboot 預設 |
| D3 卡死 | **App always runs the watchdog; installer arms it before the jump**。Installer 每次跳 App 前啟動 PMU watchdog（約 60 秒）；App 每次開機在 pmu_init() 後立即重新 arm，TRIAL／CONFIRMED 均定期清除，確認後不中止。未確認 App 卡死 → reset → 回滾 |
| D4 測試 | EVB 只跑 3 個情境（§5）；host 沿用既有斷電矩陣擴充新狀態 |

| Milestone | 內容 | 之後 |
|---|---|---|
| M-WDT | 確認 PMU watchdog 可啟動、可清除、逾時會 reset；**是否可關閉**（關不掉也可以，但要記錄） | 只在**做不到 reset 或無法清除**時停下回報；否則直接往下 |
| M-RB | §5 三個情境 PASS | 停下 review → Step 7／8 |

**本施工單核准即授權**：在 EVB 燒錄本 branch 的 installer／App／測試版 App、寫入 U11 scratch、啟用 PMU watchdog。0x08001000 sector 規則不變（永不擦寫）。

## 1. 流程（06 §5）

```text
安裝前  boot state = PENDING
        → 備份目前已確認的 App 到 NOR Rollback（0x220000–0x41FFFF）→ 讀回驗 hash
          （Rollback 已是同一份且實際讀回驗證通過 → 跳過）→ BACKUP_READY
        → 安裝 Candidate（v0.8.3 流程）→ TRIAL → 啟動 watchdog → 跳新 App
新 App  自檢通過（D1）→ 寫 CONFIRMED（新 hash）
重開機  installer 看到 TRIAL（未確認就重開了）→ ROLLBACK_REQUIRED
        → 從 Rollback 搬回舊版（同安裝流程，reset 就重做）→ CONFIRMED（舊 hash），記錄 last_update=FAILED
```

- 備份失敗（NOR 錯誤）→ 放棄這次安裝、清除 PENDING、舊 App 照常開機，記錄 last_update=ABORTED。
- 回滾時 Rollback 驗證失敗 → 停在 installer、UART 印錯誤（06 §5：沒有 factory image 可退，誠實回報）。
- **相容 v0.8.3**：EVB 上現有的 `INSTALLED` record 視為 `CONFIRMED`。

## 2. Boot state 擴充

沿用 v0.8.3 的雙 record 格式（seq＋CRC＋commit marker）。狀態：`CONFIRMED／PENDING／BACKUP_READY／TRIAL／ROLLBACK_REQUIRED`，另存 `confirmed_hash`、`candidate_hash`、`last_update`（OK／FAILED／ABORTED）。

**App 也要寫 boot state**（只寫 CONFIRMED）：把 boot state 模組移到 installer 與 App 共用的位置（例如 `fw/common/`），App 只呼叫 `boot_state_confirm(hash)`；只有在目前是 TRIAL 且 hash 等於自己的 image hash 時才寫入。App 寫入時用 SDK `flash_write`（QSPI0，短暫關中斷，一筆 record）。

## 3. Watchdog（D3）

- 先做 M-WDT 小實驗（只用 installer／測試韌體，不必 BLE）。
- Installer：每次跳 App 前均啟動，包含 TRIAL／CONFIRMED；installer 自身安裝／回滾期間的處理維持既有流程。
- App：**每次開機**在 SDK pmu_init() 返回後立即重新配置並啟動約 60 秒 watchdog，不能只依賴繼承 installer 設定；定期清除，**確認後也持續清除**。
- 已知盲窗：SDK pmu_init() 內會關閉 WDT clock；若該函式在關閉後卡死，不能保證 watchdog reset。返回後緊接重新 arm，不把盲窗延到其他 App 初始化步驟；本輪不改 SDK。
- Watchdog 造成的 reset 要能從 reset reason 分辨，記錄在 UART／GCHK 可查的地方（選做，不擋 milestone）。

## 4. 檔案範圍（Codex）

| 檔案 | 內容 |
|---|---|
| `fw/common/boot_state.c／.h`（從 installer 移出） | 狀態擴充、App 用的 confirm API |
| `fw/installer/` | 備份、TRIAL／回滾流程、watchdog 啟動 |
| `fw/src/`（App） | 自檢＋confirm、watchdog 清除；測試用 build flag：`ET100_TEST_NO_CONFIRM`、`ET100_TEST_HANG`（**只用於測試 build，預設關閉**） |
| `fw/targets/*.mk`、`fw/tools/build_*.sh`、`make_dfu_package.py` | 共用模組、測試版 App 的 build／打包 |
| `fw/tests/` | 擴充 boot state 與安裝／回滾斷電矩陣 |
| `fw/docs/04_testing/12_evb_trial_rollback.md` | 測試報告（精簡） |

**不做**：簽章、RS232／BLE 傳輸、連續失敗次數限制、installer 自我更新、SDK 修改。

## 5. 驗收（M-RB，精簡）

1. **正常更新**：A→B，B 自檢通過 → CONFIRMED；再重開仍是 B。
2. **壞的 App**：B 用 `ET100_TEST_NO_CONFIRM` 或 `ET100_TEST_HANG` → reset 或 watchdog → 回滾到 A，GCHK 版本變回 A，last_update=FAILED。兩種 flag 各跑一次。
3. **備份中 reset**：備份途中 MCU reset 一次 → 重開後接著完成更新。
4. 全程 B 資訊段 SHA-256 不變。

Host：斷電注入涵蓋新狀態（備份、TRIAL、回滾每一步），全部 PASS 即可，不必另列細部統計。

## 6. 交付

M-RB 停下 review；Step 7 同步 06 §5（實作狀態）、platform plan、status MD、REVIEW_REPORT；`git diff --check`、`check_plan_vs_code.py` PASS；不 commit／push。

---

# 歷史施工單 — v0.8.3 Installer 與 App 搬家（從 NOR 安裝到內建 Flash）

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.8.3_installer

Base branch: v0.8.2_download_journal（a72a97e）

## 0. 工作模式、決策與授權

**採用 [comm.md](../comm.md) 的「Hardware bring-up loop」**。Stanley 2026-10-22 核准 D1–D4：

| 決策 | 內容 |
|---|---|
| D1 範圍 | v0.8.3 = ① E0 App 搬家實驗 ② installer 從 NOR 安裝。trial／confirm／rollback 在 v0.8.4，RS232 在 v0.8.5，BLE SMP 在 v0.8.6 |
| D2 FlashEnv | 直接搬到 0x081E0000，不做資料遷移（尚無出貨裝置）；EVB 的 GTIME／CID／auto-GATT 恢復預設值 |
| D3 驗證 | 原型只驗 MCUboot TLV 的 SHA-256，**不驗簽章**；報告標明「不安全，原型專用」 |
| D4 格式 | App slot 0x08020000 開頭放 0x200 MCUboot header，vector table 在 **0x08020200** |

| Milestone | 內容 | 之後 |
|---|---|---|
| M-E0 | Boot stub @0x08002000 ＋ 搬到 0x08020200 的 App：BLE／UART／FreeRTOS／PRAM ram_code 正常 | 停下 review |
| M-INST | Installer 從 NOR 安裝新版 App 並開機；安裝中 MCU reset 後能重做到完成 | 停下 review → Step 7／8 |

**本施工單核准即授權**：在 EVB 上燒錄本 branch 的 installer／stub／搬家後 App（內建 Flash 0x08000000–0x081FFFFF，**0x08001000 sector 除外**）、寫入 U11 scratch。其餘依 comm.md 停止條件。

## 1. 背景：ROM 的 A／B 資訊段（原廠 `examples/common/ota/FR30xx OTA机制.pdf`＋`ota.h`）

```text
0x08000000  A 資訊段 4 KB   目前 image 資訊（post_process.py 的 header）
0x08001000  B 資訊段 4 KB   ROM OTA 用；image_valid＋info_crc＋ota_magic 有效時，ROM 會把內建 B 代碼段搬到 A
0x08002000  A 代碼段         ROM 跳到這裡（exec_offset 0x2000）
```

**硬規則（2026-10-22 修訂）**：0x08001000 sector **永遠不擦、不寫，維持原內容**（目前全 00，來自 post_process.py 的檔案空洞）。Installer 開機時若既非全 00 也非全 FF，只記錄警告、不動它；燒錄工具跳過整個 sector，且 image 該段須與板上內容相同才燒；驗收以測試前後該 sector SHA-256 不變為準。

## 2. 目標布局（06 §2）

| 區域 | 位址 | 內容 |
|---|---|---|
| A 資訊段 | 0x08000000 | post_process.py header，`code_length`／CRC **只涵蓋 installer** |
| B 資訊段 | 0x08001000 | 保持 0xFF |
| Installer | 0x08002000–0x0801FFFF（120 KB） | 本 branch 新增 |
| App slot | 0x08020000–0x081DFFFF（1792 KB） | MCUboot header 0x200 ＋ App（vectors @0x08020200） |
| Settings | 0x081E0000–0x081FBFFF（112 KB） | FlashEnv 新位址 |
| Boot state | 0x081FC000–0x081FFFFF（16 KB） | 本 branch 最小版（§4.3） |

## 3. 第一步：M-E0 App 搬家實驗（不碰 NOR）

1. **Boot stub**（之後長成 installer）：獨立 target，連結在 0x08002000，不含 BLE／FreeRTOS。檢查 0x08020000 的 MCUboot magic → 驗 SHA-256 → 關 IRQ／SysTick、清 NVIC pending → `SCB->VTOR = 0x08020200` → 設 MSP → 跳 Reset_Handler。驗證失敗就停在 UART 印錯誤（不跳）。
2. **App 搬家**（evb_serial target，版本改 0.8.3-dev）：
   - 專案自己的 linker script（在 mk 覆寫 `LD`，**不改 SDK 的 ldscript**），FLASH ORIGIN 0x08020200、LENGTH 1792 KB − 0x200
   - FlashEnv 改到 0x081E0000：專案自己的 `fal_cfg.h` 覆蓋 SDK example 的版本（include 順序），不改 SDK
   - **移除 vendor OTA service**（`evb_serial_ble.c:86` 的 `ota_gatt_add_service()`），因為它會寫 B 資訊段觸發 ROM OTA，會蓋掉 installer
   - Build 後用 `imgtool sign --header-size 0x200 --pad-header`（不帶 key，只產生 SHA-256 TLV）產生 App image；800 KB guard 改成「≤ App slot 大小」
3. **燒錄工具**：`fr3068_flash.py` 增加 raw 模式（燒 App image 到 0x08020000，不檢查 FR509x header），並**拒絕任何會寫到 0x08001000 sector 的請求**。
4. **驗收 M-E0**：ROM → stub → App 開機；UART 9600 GCHK 回 0.8.3-dev；BLE 廣播與手機連線正常（Stanley 驗）；FlashEnv 在新位址可寫入、重開機後保留；確認 ROM 沒有因為 App 不在 header 範圍內而拒絕開機。

## 4. 第二步：M-INST Installer

### 4.1 流程

```text
開機 → 檢查 B 資訊段 → 讀 boot state
  無 pending：驗 App（magic＋SHA-256）→ 跳 App；App 無效 → 停在 installer、UART 印錯誤
  有 pending：init NOR（v0.8.1 nor_qspi1）→ 驗 Candidate（magic、長度 ≤ slot、SHA-256 TLV、與 state 記錄的 hash 一致）
             → erase App slot（只到 image 長度對齊的 sectors）→ program → 全部讀回驗 SHA-256
             → boot state 寫 INSTALLED → 跳新 App
```

- 安裝途中 MCU reset：state 仍是 pending，Candidate 沒動 → **從頭重做**即可（冪等），不需要 copy flag。
- Installer 自帶：nor_qspi1、SDK `flash_erase／flash_write`（QSPI0，`__RAM_CODE`；installer 沒有 RTOS，關中斷可接受）、mbedTLS SHA-256、MCUboot header／TLV parser。
- Installer 大小 ≤ 120 KB，build 時檢查。

### 4.2 讓 Candidate 進 NOR（開發用，非正式 RS232 協定）

Installer 的 **dev-only loader**：開機 2 秒內從 UART3（115200）收到指定字串才進入。簡單 framing（chunk index＋4 KB＋CRC32，逐包 ACK），經 v0.8.2 journal 寫進 Candidate，`finish` VERIFIED 後寫 boot state pending 並重開。正式 RS232 協定在 v0.8.5 另外設計，本 loader 屆時移除或改為正式版。

### 4.3 最小 Boot state（v0.8.4 會擴充 trial／rollback）

16 KB 區域，兩份 record 輪流寫（seq＋CRC＋commit marker，規則比照 journal）：`NONE／PENDING(hash,len)／INSTALLED(hash)`。只有 installer 和 loader 會寫。

### 4.4 驗收 M-INST

1. 用 loader 把「版本號不同的 App image」送進 NOR → 重開 → installer 安裝 → 新 App 開機，GCHK 版本變更。
2. 安裝中觸發 MCU reset **3 次**（erase 中、program 中、驗證中各一次）→ 每次都能重做到完成。
3. 故意送 hash 錯誤的 image → installer 拒絕安裝，舊 App 照常開機。
4. 全程 0x08001000 sector 的 SHA-256 不變（每次測試前後 SWD 讀回確認）。

## 5. 檔案範圍（Codex）

| 檔案 | 內容 |
|---|---|
| `fw/installer/`（新） | installer main、boot_state、mcuboot_image parser、jump、dev loader |
| `fw/targets/evb_installer.mk`、`fw/tools/build_evb_installer.sh` | Installer build（post_process.py header、120 KB guard） |
| `fw/ld/evb_app.ld`（新） | App 專案 linker script |
| `fw/include/fal_cfg.h`（新）或等效覆蓋 | FlashEnv @0x081E0000 |
| `fw/targets/evb_serial.mk`、`fw/tools/build_evb_serial.sh` | 用新 linker、imgtool sign、新 guard、版本 0.8.3-dev |
| `fw/src/evb_serial_ble.c` | 移除 vendor OTA service |
| `fw/tools/evb/fr3068_flash.py` | raw App 模式、B 資訊段保護 |
| `fw/tests/` | MCUboot parser、boot state（含斷電注入）、loader framing 的 host tests |
| `fw/docs/04_testing/11_evb_installer.md` | 測試報告 |

**不做**：簽章驗證、trial／confirm／rollback、Rollback 區備份、正式 RS232／BLE／LTE、SDK 修改、FlashEnv 資料遷移、installer 自我更新。

## 6. 風險與停止點

- **開不了機**：ROM 仍會跳 0x08002000，`rescue_and_flash.py` 可救；第 2 次需要 rescue 即停（comm.md）。
- **App 依賴 0x08002000**：若搬家後 BLE／sleep／PRAM 異常，停在 M-E0 回報，不往下做 installer。
- `freertos_sleep.c` 會保存／還原 VTOR；EVB 目前關閉 sleep（Patch #8），M-E0 只記錄，不宣稱 sleep 下 relocation 已驗證。

## 7. 交付

M-E0、M-INST 各停一次 review；Step 7 同步 06（B 資訊段規則、布局已實作的部分）、status MD、EVB README（新燒錄方式，需先問 Stanley 其未 commit 修改）、REVIEW_REPORT；`git diff --check`、`check_plan_vs_code.py` PASS；不 commit／push。

## 8. DFU 打包（2026-10-22 M-E0 review 後加入，與 M-INST 一起做）

目標：產生與 NCS 相同格式的 `dfu_application.zip`，讓 App 團隊可以開始解析／相容性檢查（06 §6.1）。

1. **EUPFWV1 metadata**：新增 `fw/include/eup_fw_info.h`，`struct eup_fw_info` 欄位、順序、大小與 `eup_gateway/include/eup_fw_info.h` **逐 byte 相同**（以 GCC `__attribute__((packed))` 取代 Zephyr `__packed`，並用 `_Static_assert` 鎖定總大小與各 offset）。App 內放一個 `const` block（`__attribute__((used))`）：project_name `EUP_ET100`、hardware_version `ET100_EVB_DEV1.4`（EVB build）、firmware_version 與 GCHK 同一來源（順便把 `serial_gchk.c` 的版本字串改成引用同一個定義，避免兩處不一致）。`metadata_crc32_le` 的計算方式照 gateway 的 `tools/sensor_ota_package.py`／`eup_aiot/tools/eup_fw_info_scan.py`，在 build 後由腳本回填。
2. **`fw/tools/make_dfu_package.py`**：Project.bin → 回填 EUPFWV1 CRC → `imgtool sign --header-size 0x200 --pad-header --slot-size 0x1C0000 --version <ver>`（不帶 key，原型）→ `et100.signed.bin` → `manifest.json`（欄位與 NCS 相同：`format-version`、`time`、`files[]`：`type=application`、`board=dev14_fr3068ep_c`、`soc=fr3068e_c`、`load_address=0x08020200`（十進位）、`image_index="0"`、`slot_index_primary="1"`、`slot_index_secondary="2"`、`version_MCUBOOT`、`size`、`file`、`modtime`；`name`）→ `dfu_application.zip`。`build_evb_serial.sh` 結尾自動呼叫。
3. **Host test**：用 `eup_aiot/tools/eup_fw_info_scan.py`（唯讀呼叫，不修改該 repo）掃描產出的 `.signed.bin`，必須找到 EUPFWV1 且 CRC 正確；zip 結構與 manifest key 集合與 `eup_gateway/build_ms88sf2_app_ota/dfu_application.zip` 相同。
4. **Installer**：安裝前在 Candidate 內找 EUPFWV1，`project_name`／`hardware_version` 前綴不符就拒絕（防止 nRF 韌體寫入 FR3068）。M-INST 驗收新增：送 eup_gateway 的 nRF `.signed.bin` → 必須被拒絕、舊 App 照常開機。
5. 報告附上產出的 `manifest.json` 全文與 zip 路徑，交給 App 團隊。

---

# 歷史施工單 — v0.8.2 Download journal（NOR 下載進度與斷電恢復）

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.8.2_download_journal

Base branch: v0.8.1_nor_driver_bringup（a425358）

## 0. 工作模式與決策

**採用 [comm.md](../comm.md) 的「Hardware bring-up loop」**。Stanley 2026-10-22 決定：

- **斷電驗證只用 host 模擬（方案 A）**；實機斷電延到 ET-100 實板。v0.8.1 的 G2（韌體關 LDO）**取消**，原因是無法量測 C11／C30。
- Chunk 大小 **4 KB**（= 最小 erase 單位）。

| Milestone | 內容 | 之後 |
|---|---|---|
| M-HOST | Host 斷電注入矩陣全部 PASS（§5） | 停下 review；EVB 上寫 U11 需 Stanley 核准 |
| M-EVB | U11 上完成「下載 → MCU reset 中斷 → 續傳 → 完成」並寫入報告 | 停下 review → Step 7／8 |

## 1. 目標

依 [06 OTA 儲存規格](docs/01_firmware/06_ota_storage_design.md) §4，實作 NOR 上的 **download journal**：記錄一次下載的進度，讓下載在 MCU reset 或斷電後可以續傳，且**絕不把沒寫完或寫壞的 chunk 當成已完成**。

Journal **只管下載**，不決定安裝、trial、confirm、rollback；那些屬於內建 boot state（之後的 installer branch）。

## 2. 範圍

| NOR 區域（06 §3） | 用途 |
|---|---|
| 0x000000–0x01FFFF（128 KB，32 sectors） | Journal |
| 0x020000–0x21FFFF（2 MB） | ET100 Candidate 資料，512 chunks |
| 0x420000–0x45FFFF（256 KB） | Sensor slot 資料，64 chunks；package 上限 225,280 B |

**不做**：RS232／BLE／LTE 傳輸、簽章驗證（只比 SHA-256）、installer／內建 Flash、Rollback 區、FreeRTOS 產品 app 整合、4-byte 定址、SDK／linker／FlashEnv 修改。

## 3. 設計不變式（Codex 依此設計格式，寫成 07 文件後實作）

1. **單一 session**：同時只有一個下載；target（ET100／sensor）與 source（LTE／BLE／RS232，此版只記錄）寫在 session 開頭。
2. **長度上限**：ET100 package ≤ 2,097,152 B，sensor package ≤ 225,280 B；`begin` 時就拒絕，不靠 slot 大小判斷。
3. **Chunk 完成的定義**：該 4 KB 資料 sector「erase → program → 全區讀回比對」成功，**且**對應的 journal entry 已 commit。兩者缺一，就視為未完成。
4. **ACK 順序**：只有在第 3 點成立後才回報 chunk 完成；重複寫同一個 chunk 要冪等（內容相同直接回成功，內容不同回錯誤）。
5. **Commit 判斷**：每筆 journal record／entry 有 CRC 與最後寫入的 commit marker；marker 不完全吻合就視為無效（torn write）。**不以 sector 內容（例如全 0xFF）判斷是否完成。**
6. **不重寫半寫過的位置**：被中斷的 record 位置直接跳過，下一筆寫到下一個空位；不對同一位置做第二次 program。
7. **Sector 使用前必須可信**：journal sector 在 erase＋全區驗證後寫入 sector header；沒有有效 header 的 sector 一律重新 erase 才能用。資料 chunk 被中斷時，該 4 KB sector 也一律重新 erase（06 §7 規則 2）。
8. **開機恢復**：先等 WIP 清除（規則 1）→ 掃描 journal → 取最新有效的 session 狀態 → 回報可續傳的 bitmap。Journal 全部無效時視為沒有 session，不猜測。
9. **GC**：sector 用滿時，先在新 sector 寫入完整 snapshot（session＋整份 bitmap）並 commit，之後才能 erase 舊 sector；GC 任何一步斷電都不能遺失已 commit 的狀態。
10. **Finish**：所有 chunk 完成後，對整個 package 計算 SHA-256，與 `begin` 時的值比對；相同才進 VERIFIED，否則 FAILED。VERIFIED 只代表「下載完整」，不觸發安裝。
11. **Timeout**：不只依賴 DWT（06 §7 規則 3）。

## 4. 檔案範圍（Codex）

| 檔案 | 內容 |
|---|---|
| `fw/docs/01_firmware/07_download_journal_design.md` | **先寫**：record／entry／sector header 格式、bitmap、狀態機、GC 流程、每一步的斷電後果表。寫完即可開始實作，不必停下等 review |
| `fw/src/ota_journal.c`／`.h` | 只依賴 `nor_port.h` 的可攜模組；API：`begin／write_chunk／query／finish／cancel／recover` |
| `fw/tests/fake_nor.c`／`.h` | Host NOR 模型＋斷電注入（§5） |
| `fw/tests/test_ota_journal.c` | 功能測試＋斷電矩陣 |
| `fw/src/nor_test_main.c`、`fw/targets/evb_nor.mk` | 新增 journal console 指令（用 pattern 模擬 package），供 M-EVB 使用 |
| `fw/tools/evb/nor_test_host.py` | 驅動 M-EVB 流程與 log |
| `fw/docs/04_testing/10_download_journal.md` | 測試報告 |

## 5. Host 斷電模擬（M-HOST，主要證據）

Fake NOR 要模擬 NOR 真實限制：program 只能把 1 變 0、erase 以 sector 為單位、單次 PP 最多 8 B（與 v0.8.1 STIG 一致）。

**斷電模型**（在第 i 個 NOR 操作中途注入）：

| 被中斷的操作 | 模擬結果 |
|---|---|
| Erase | 該 sector 變成「舊內容與 0xFF 隨機混合」；另一種情況是全 0xFF 但標記為 weak，之後讀取可能隨機翻 bit |
| Program | 只有部分 byte 被寫入，或某些 bit 只被清除了一部分 |
| 重開機 | RAM 狀態全部清空，只留 fake NOR 的內容 |

**測試矩陣**：

1. **窮舉**：小型 package（16 chunks），從第 1 個到最後一個 NOR 操作，每個點都注入一次斷電 → recover → 續傳到完成。全部都要滿足：最終 SHA-256 正確、沒有 chunk 被錯判為完成、已 commit 的狀態沒有遺失、需要重傳的量有上限。
2. **連續斷電**：在 recover 和 GC 的過程中再次斷電（最多連續 3 次）。
3. **隨機**：2 MB package，以固定 seed 隨機注入斷電 ≥ 10,000 次。
4. **GC 邊界**：讓 journal 用滿、反覆觸發 GC，並在 GC 的每一步注入斷電。
5. **規則類**：長度超過上限被拒絕、重複寫同一 chunk（相同與不同內容）、cancel 後無法續傳、第二個 `begin` 被拒絕。

全部以 ASan／UBSan 執行；列出每一類的注入次數與 PASS 數。

## 6. EVB 驗證（M-EVB，需 Stanley 核准寫入 U11）

用 v0.8.1 的獨立測試 target：以 pattern 模擬 2 MB ET100 package 和 225,280 B sensor package，透過 journal 寫入 U11。

- 在隨機 chunk（含 GC 期間）觸發 MCU reset，**≥ 20 次** → reboot → recover → 續傳 → finish；SHA-256 必須與 host 期望值相同。
- 記錄每次 reset 後的重傳量與 recover 時間。
- 測完燒回 0.7.0-dev。

**明確不宣稱**：實機斷電耐受度（延到 ET-100）、實際傳輸效能（STIG 8-byte PP）。

## 7. 驗收與交付

1. M-HOST：§5 全部 PASS → 停下給 Claude review。
2. Stanley 核准後執行 M-EVB → 停下 review。
3. Step 7：06 §7 規則 2 註明「T8 延到 ET-100，G2 取消」；09 報告同步 G2 取消；07 設計文件定稿；status MD、REVIEW_REPORT。
4. `git diff --check`、`check_plan_vs_code.py` PASS；不 commit／push。

---

# 歷史施工單 — v0.8.1 外部 NOR bring-up（EVB 板上 U11）

## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v0.8.1_nor_driver_bringup

Base branch: v0.8.0_ota_nor_spec（9b1fdec）

## 0. 工作模式

**採用 [comm.md](../comm.md) 的「Hardware bring-up loop」**（2026-10-22 起）。Milestone：

| Milestone | 內容 | 之後 |
|---|---|---|
| M-G1 | 開機 G1 PASS：JEDEC＋SFDP／BFPT geometry＋SR1／SR2，並證明 timeout 後可再次 init | 停下 review；T2 起含寫入，需 Stanley 核准 |
| M-T7 | T2–T7 完成並寫入 09 報告 | 停下 review → Step 7／8 |

T8／G2 仍延後（需量 C11／C30）。

## 1. 目標與定位

用 DEV1.4 EVB 板上 U11 開發 **NOR 上層共用部分**：`nor_port` 介面、測試方法／工具、erase／program 特性與**斷電後處理規則**，供 v0.8.2 download journal 與 [06 OTA 儲存規格](docs/01_firmware/06_ota_storage_design.md) 使用。

| 項目 | EVB U11（本次） | ET-100 U0103（之後） |
|---|---|---|
| 晶片 | 照片 GD25LQ128EWIG、原理圖 XT25Q128 → **以 JEDEC ID 為準** | XM25QH256D |
| 介面 | QSPI1（OSPI pads：PB12 CLK、PB13 CS、PC0 IO0、PC1 IO1、PB14 IO2、PB15 IO3） | SPIMX2_1 single-SPI（PD8–PD11） |
| 電壓／容量 | 1.8 V（IO1V8_LDO）／16 MB，3-byte 位址 | 3.3 V／32 MB，4-byte |
| Memory-mapped 讀 | DSP_FLASH_DAC_BASE **0x28000000**（SWD 可直接讀） | 無 |

**可帶到 ET-100**：`nor_port` 介面、上層流程、測試工具、斷電規則。**帶不過去**：底層後端、4-byte 定址、XM25QH256D 的 timing／保護位元 → ET-100 板到後另開 branch 補 SPIMX2_1 後端，用同一套測試重跑。

## 2. 前置 Gate

| Gate | 內容 | 負責 |
|---|---|---|
| G1 JEDEC 唯讀 | 測試韌體第一個指令只做 power-on（`ool_write(0xfc)` bit0，同 lvgl_demo `hw_dsp_xip_flash_init`）＋OSPI pad mux＋讀 JEDEC ID／SFDP，**不寫不擦**。預期 GigaDevice `C8 60 18`；讀不到或不符 → 停止回報。另確認 EP-C 內建 PSRAM 沒有占用這組 OSPI pads（讀 ID 正常且 PSRAM 未啟用即視為通過，並記錄） | Codex 實作，Stanley 授權燒錄 |
| G2 LDO 切電可行性 | 確認 `ool_write(0xfc)` bit0 是否真的切斷 U11 VCC（IO1V8_LDO）：關掉後 ID 讀不到、再開後恢復；**Stanley 用電錶量 C19 兩端電壓**確認真的掉到 ~0 V。通過才用韌體做 T8，否則 T8 延後 | Codex＋Stanley |
| G3 燒錄 | 測試韌體會取代板上 0.7.0-dev；每次燒錄前取得 Stanley 同意，測完用 `fr3068_flash.py` 燒回 0.7.0-dev | Stanley |

## 3. 已查到的 SDK 問題（不可直接沿用）

`components/drivers/peripheral/Src/driver_flash.c` 支援 QSPI1（`DSP_QSPI_BASE`），page 切割正確，但：

1. `flash_erase`／`flash_write` 在**整個操作期間 `GLOBAL_INT_DISABLE`**；多 sector erase 會關中斷數秒 → 產品中會拖垮 FreeRTOS tick／BLE。
2. `flash_wait_wip_clear` 是**無 timeout 的無限迴圈**；NOR 掉電時 status 可能一直讀到 busy → 系統卡死（正是 T8 會遇到的情況）。
3. 對 QSPI1 操作時仍呼叫 `flash_protect_bit_set(QSPI0, …)`，**順便解除／恢復內建 Flash 的保護位元**；操作外部 NOR 不該碰內建 Flash 的 status register。
4. `flash_init` 遇到不在白名單的 ID 會 `while(1)`；`flash_erase` 只有 4 KB sector erase。
5. `components/drivers/bsp/spi_flash/` 的 `ext_flash.c`／`IC_W25Qxx` 是另一種 EVB 接法（SPIMX8_1／PD0–PD5），且 page program 不切 page、`WaitBusy` 無 timeout、`BLOCK_ERASE_64K` 錯定成 0x52 → 不使用。

**做法**：專案內寫 `nor_qspi1.c`，只用 SDK 的低階 QSPI 原語（`qspi_stig_cmd`、`flash_init_controller`／`flash_set_baudrate`、DAC 讀）自組指令；不 patch SDK（ADR 02）。

## 4. 施工範圍（Codex）

| 檔案 | 內容 |
|---|---|
| `fw/src/nor_port.h` | 後端介面：`init／read_id／read_sfdp／read／program／erase(4K/32K/64K)／wait_ready(timeout)／soft_reset／power(on/off)`，全部回錯誤碼 |
| `fw/src/nor_qspi1.c`／`.h` | U11 後端，要求見 §5 |
| `fw/src/nor_test_main.c` | 獨立測試韌體：UART console（沿用 evb_serial 的 DAPLink VCOM 腳位，115200 8N1），執行 §6 指令並逐行輸出；不啟 BLE |
| `fw/targets/evb_nor.mk`、`fw/tools/build_evb_nor.sh` | 獨立 build target，產物與 evb_serial 分開；沿用 FR509x header 與 800 KB build guard |
| `fw/tests/test_nor_port.c` | Host 測試（fake 後端）：位址編碼、範圍／對齊拒絕、page 切割、timeout 路徑 |
| `fw/tools/evb/nor_test_host.py` | 送 console 指令、收結果、存 log，`--help` 完整 |
| `fw/docs/04_testing/09_evb_nor_bringup.md` | 測試報告：JEDEC ID、量測數據、PASS／FAIL、斷電規則 |
| `docs/03_hardware/FR3068EP-C Dev1.4/README.md` | 只**新增**「U11 外部 NOR」一節；Stanley 未 commit 的既有修改不得覆蓋，動之前先問 |

## 5. 後端要求（nor_qspi1.c）

1. 指令全部經 `qspi_stig_cmd` 自組：WREN 0x06、RDSR 0x05／0x35、SE 0x20、BE32 0x52、BE64 0xD8、PP 0x02、RDID 0x9F、RDSFDP 0x5A、soft reset 0x66＋0x99；opcode 以 JEDEC／SFDP 結果與 datasheet 核對後才啟用。
2. **Busy 等待有 timeout**（datasheet max × 1.5），逾時回錯誤，不 hang。
3. **關中斷只包住送指令那一小段**，等待 WIP 期間不關中斷、可讓出 CPU（測試韌體用 polling＋短 delay 即可，但結構要能直接搬進 RTOS）。
4. **不碰 QSPI0／內建 Flash 的任何暫存器或保護位元**。
5. Program 在 256 B page 邊界切割；erase 必須對齊；offset＋len 做 overflow／容量檢查（容量取 JEDEC／SFDP，不寫死）。
6. 寫入後的讀回驗證走 DAC（0x28000000）；讀之前確保 controller 不在 STIG 操作中。若 DAC 有 cache／prefetch，讀回前要讓舊內容失效（查 SDK／datasheet，查不到就改用 STIG read 0x03 驗證並記錄）。
7. 不提供 chip erase；不改 non-volatile status／OTP 位元。

## 6. 測試項目（console 指令）

U11 整顆視為 scratch，寫入只在指令指定的範圍。

| # | 項目 | 方法 | PASS 條件 |
|---|---|---|---|
| T1 | ID／SFDP（=G1） | `id`、`sfdp` | 實際料號、容量、erase types 確認並記錄 |
| T2 | 狀態／保護 | `status` | 記錄 SR1／SR2 預設值；若保護位元會擋寫入，回報並停止，不自動改 non-volatile 位元 |
| T3 | 位址邊界（16 MB） | 在 0x000000、0x7FF000、0x800000、0xFFF000 各寫不同 signature | 四點讀回正確、互不 alias；SWD 讀 0x28000000＋offset 與 console 結果一致。**4-byte 定址不在本次範圍**，報告註明待 ET-100 |
| T4 | Erase 幾何／時間 | `erase 4k／32k／64k`，各 ≥ 20 次 | 抹後全 0xFF；記錄 min／avg／max，對照 datasheet；未對齊被拒絕 |
| T5 | Program | 整 page、跨 page、部分 page、`bench` | 讀回逐 byte 一致；記錄 KB/s 與 SPI clock |
| T6 | 大範圍完整性 | 0x020000–0x41FFFF（06 的 Candidate＋Rollback）寫 pseudo-random → SHA-256，重複 3 次 | 3 次一致；記錄總耗時，推估 1792 KB App 備份／下載寫入時間 |
| T7 | MCU reset 中斷 | 下 erase／program 後立即 SYSRESETREQ（NOR 仍有電） | reset 後 init 成功、WIP 最終清除；記錄操作是否完成；之後 erase＋program 正常 |
| T8 | 斷電中斷（需 G2） | 在 sector erase 與 page program 的 typ 時間 10％／50％／90％ 用 LDO 切電，各點 ≥ 20 次 | 復電後 init 成功、無 hang（驗證 timeout 路徑）；記錄被中斷區域的狀態（全 0xFF／混合／部分寫入）；再 erase＋program 成功 |

**T8 結論要寫成 v0.8.2 journal 的設計規則**，至少回答：中斷後讀到全 0xFF 是否可信？（預期：不可信，必須重新 erase；完成與否以 journal commit marker 判斷，不以內容判斷。）

## 7. 不做事項

不做 OTA manager、download journal、installer、RS232／BLE／LTE 傳輸；不碰內建 Flash 分區；不改 SDK、linker、FAL、FlashEnv；不改產品 evb_serial target；不做 4-byte 定址與 SPIMX2_1 後端；不 commit／push（等 Step 5 review 與 Stanley 指示）。

## 8. 驗收與交付

1. Host test PASS（ASan／UBSan）、`git diff --check`、`check_plan_vs_code.py` PASS。
2. Stanley 授權燒錄後先跑 G1；通過才跑 T2–T7；G2 通過才跑 T8。
3. `09_evb_nor_bringup.md` 填完整數據；FAIL 照實記錄，不改測試期望配合結果。
4. 測完燒回 0.7.0-dev，確認 App 正常。
5. Step 7 同步：06 §7（EVB 用 U11、結果與限制）、`11_project_bring_up_status.md`、`REVIEW_REPORT.md` addendum。

---

# 歷史施工單 — v0.8.0 NOR OTA 規格修訂（文件限定）

## Branch／授權

Base v0.7.0_evb_app_gatt_server／e5274b2。Stanley 已明確同意開 v0.8.0_ota_nor_spec 並更新文件，採其 review 的 header／state authority／sensor package 相容修正；RS232 與取消 Factory recovery 的需求也已在原對話明確確認。本輪不 commit/push，除非另有指示。

## 檔案範圍

本輪 M1／M2／m1／m2 補充授權：只恢復指定 08 報告、flash_size_probe.py／flash_sector_test.py（原 untracked evidence）與工具 README；不恢復其他計畫／A/B 分區。修正 08 分支標記、以相對 links 取代未還原說法、對外 V1.4／Q48-6 中性措辭、01 的 V1.4 引用及自有 installer／ROM 邊界；重跑文件與 diff 檢查，不 build／flash／commit。

- 新增 `fw/docs/01_firmware/06_ota_storage_design.md` 為完整 OTA／記憶體候選契約。
- 同步 `01_firmware_development_plan.md` §2.0a／§5.1／§5.3a、`02_firmware_architecture_draft.md` §7／§8、`03_hardware_coverage_matrix.md` OTA gate、`04_development_execution_draft.md` M5。
- 同步 `fw/docs/00_project/00_et100_firmware_platform_plan.md` 文件版 v0.8 與 revision、`fw/REVIEW_REPORT.md`、`docs/00_project/11_project_bring_up_status.md` 的設計／實測邊界。
- 更新 `docs/03_hardware/sch_analysis/100_questions_for_quectel.md` Markdown V1.4，補 NOR installer 意圖的 Q43-5／Q48-6，明記 HTML／PDF 尚未重產。
- 本檔只追加本輪授權／scope，保留下方歷史施工單。

## 不做事項與驗收

不改 source／SDK／linker／FAL／FlashDB；工具變更限恢復[唯讀 probe](tools/evb/flash_size_probe.py)／[sector 工具](tools/evb/flash_sector_test.py)及[README](tools/evb/README.md)，Python 內容不改、不執行。恢復[08 報告](docs/04_testing/08_evb_internal_flash_size.md)並修正本分支標記；不安裝、不 build、不燒錄、不操作 UART／App。不整批套用／刪除保存記錄，不恢復其他計畫檔或內部 A/B 分區。不動使用者硬體 README／PCB 資料。

驗證為候選地址／byte sum／erase-alignment、225280／256／225024 package arithmetic、Markdown 相對 links、變更檔案 scope／舊說法一致性與 git diff --check。內部 state 16 KB 可行性、ROM header 只涵蓋 installer、Q48 E-C mapping、runtime Flash／VTOR／掉電恢復不因文件通過而視為已驗證。

---

# 歷史施工單 — Phase 1b.6 App GATT Server

## Branch／授權

Stanley 2026-10-21 要求 Gateway 同款 App 連線功能、名稱 `EUP_AIoT_FR`，並先 commit/push。Checkpoint `db4f2f7` 已推 origin/v0.6.0_evb_gatt_client，SHA 核對一致；Stanley 明確同意新分支 `v0.7.0_evb_app_gatt_server` 後才實作。沿用修改／build／flash、Stanley App／HTML 驗收分工，不替使用者操作 App，不改 SDK／sibling repos／PCB 資料，不自動 commit/push 新功能。

實作中查到現有 App 在 discovery 前依名稱分類：以 EUP_AIoT 開頭、不含 HUB 者要求 sensor AES。Stanley 明確選擇改名 **EUP_AIoT_HUB_FR**，不修改 App；以下 EUP_AIoT_FR 名稱規劃均由此決策取代。名稱同時套用 ADV 與 GAP name。

## 本次範圍

- 新增 `src/app_manager.c`／`.h`（portable 八筆 RAM table、Gateway status/list/selected/select binary codec）、`src/app_gatt_server.c`／`.h`（SDK service、單 App link、CCCD／MTU／通知／GAP 生命週期）。
- 修改 `ble_reporter.c`／`.h`：沿用同一 AD parser/filter 輸出所選 manufacturer/service metadata，僅 RTOS adapter 將已接受 raw ADV 送入 manager；不改 Type 1 格式／節流／GTIME／CID 保存。
- 修改專案 `evb_serial_ble.c`：以 App manager 四 characteristic 取代 FFF0 demo service，保留 vendor OTA service（未驗證），改 GAP name／ADV／scan response 為 EUP_AIoT_FR，廣播包含 manager 128-bit UUID，App 斷線後重新廣播，不因 sensor 斷線重複啟動。
- 修改 `serial_gchk.c`、`targets/evb_serial.mk` 與 host tests／optional UART script：0.7.0-dev、poll 通知、加入新 objects／mock adapter tests。更新 protocol／test／platform／review／bring-up 文件；保留既有硬體 README 編輯。
- 新增 `tools/test_evb_serial_host.sh`，統一編譯並執行七組 native sanitizer tests，僅使用 /tmp 測試產物，不安裝工具。
- 不含 App 操作 sensor GATT／OTA relay、新認證協定、SDK patch、配對安全性產品化、按鍵／clock／linker 變動。App manager 的選取僅為觀察，不會對 sensor 建立連線。

## 驗證與限制

Host ASan／UBSan：codec／ADR 邊界、timeout／selection／capacity、CCCD／MTU／read offset／bounded write、notify backpressure 與雙角色隔離；既有五組回歸、ARM build、header／CRC／FlashEnv 後燒錄／讀回。Logs `/tmp/et100_app_gatt_{build,host_tests,flash}.log`。App／scan／sensor GATT 並行、真正 read-blob／MTU／通知、heap／stack 皆待 Stanley 實測。

SDK binary callback 已只讀反組譯確認：READ_REQ 的 msg_len 是容量、offset 是起點，return 為實際 copy 長度；WRITE callback return 被 SDK 忽略，無應用層 ATT error setter。不合法寫入只保證不改狀態，不能宣稱有 Gateway 等價 ATT error；不為此修改 SDK。

---

# 歷史施工單 — Phase 1b.5 Gateway-style GATT Client

## Branch／授權

Branch `v0.6.0_evb_gatt_client`，base `v0.5.0_evb_cid_ble_scan`。Stanley 要求加入 Gateway 同款 GATT；沿用 Codex 修改／build／flash、Stanley HTML／sensor 測試的分工。不 commit／push，不碰 PCB 原始資料與 SDK。

## 範圍

- 新增 `fw/src/gatt_client.c`／`.h`：單一 RAM session owner、manual／auto、SDK discovery／CCCD／MTU、AES challenge、metadata／command／config、timeout／cancel／confirmed disconnect／scan resume、UART 輸出 queue。
- 從 Gateway 複製 portable UUID codec、metadata parser、auth helper（六個 `fw/src/eup_aiot_*` 檔案）；認證材料不輸出至 log／chat。
- 複製本機 NCS 的 TinyCrypt AES encrypt／utils 與必要 headers 至 `fw/third_party/tinycrypt/`，保留授權，不安裝套件、不修改 SDK。
- 修改 `serial_gchk.c`／`.h`、`ble_reporter.c`、`evb_serial_ble.c`、`targets/evb_serial.mk`：UART 命令分派、狀態、GAP callback、scan resume 與新 objects。
- 命令：STYPESWITCH／GTYPESWITCH、SAUTOGATT／GAUTOLIST、GSENSORINFO／GSENSORCMD、SSENSORCFG；Type 8 狀態／MR。GTIME/CID keys 保留，auto list 用新 user key；session 不寫 FlashDB。
- 新增 GATT tests、更新 serial regression 版本；optional UART script 同步版本。更新 serial protocol、platform plan、REVIEW_REPORT、bring-up status、EVB README；新增 GATT 測試紀錄與第三方來源文件。
- 不包含 Sensor OTA／package staging／MCUmgr、MAC whitelist 或 MAXRPT。

## 驗證

Host codec／TLV／AES 公開測試向量、session／callback／timeout 的可模擬檢查；ARM GCC build 及 header／CRC／payload／FlashEnv 範圍驗證後 flash/readback。Build log `/tmp/et100_gatt_build.log`，flash log `/tmp/et100_gatt_flash.log`。只有實際收到 Sensor auth status 00 才送 AP；沒有硬體驗收前不宣稱 GATT／認證／設定通過。失敗原因與未驗證 SDK callback 行為需明記。

---

# 歷史施工單 — Phase 1b.4 CID 與 BLE Type 1 回報

## 本次分支與授權

- Branch：`v0.5.0_evb_cid_ble_scan`；base：`v0.4.0_evb_gtime_settings`。
- Stanley 回報 GTIME 實機可用後，同意繼續 GCID／SCID、BLE scan 與 Type 1 回報。
- 沿用分工：Codex 修改／build／flash，Stanley HTML 與真實 sensor 驗收；不 commit/merge/push。
- SDK patch 預期 0；不改 sibling Gateway、clock、linker、FlashEnv 分區與 boot header。

## 本次範圍

- `fw/src/serial_gchk.c`／`.h`：四個 CID 欄位、預設 0639、FlashDB key 0x00030003，schema 1；版本 0.5.0-dev。
- 新增 `fw/src/ble_reporter.c`／`.h`：AD 邊界解析、CID/UUID filter、32-entry cache、RSSI 排序、最多 min(GTIME,15) 筆、一秒節流、Type 1 XOR、EUP v1/v2 1500 ms 去重。
- 新增 `fw/src/evb_serial_ble.c`：從 patched SDK peripheral app_ble.c 複製，廣播成功後啟動 SDK central 範例同款 general discovery active scan，scan event 轉交 cache。SDK 註解表示 observer mode 可能在 host 過濾 general-discoverable 封包，故採參考範例模式；SCAN_RSP 仍丟棄。保留原 peripheral profile，並行能力待實機驗證。
- `fw/targets/evb_serial.mk`：替換 app_ble object，加入 reporter object。
- 更新 `fw/tests/test_serial_gchk.c`、新增 `fw/tests/test_ble_reporter.c`；更新 optional UART tester 的版本與已支援命令，Codex 不執行該工具。
- 更新 serial protocol、platform plan、REVIEW_REPORT、bring-up status、EVB README；新增 `fw/docs/04_testing/04_evb_cid_ble_scan.md`，補記前版 GTIME 的 Stanley 回覆。

## 驗收與限制

- 沿用 Gateway ASCII/CRLF/token、四個 CID 欄位及 Type 1 format；CID 比對 little-endian ID，支援 0xFF／0x16，符合者優先 MFG。
- 只接收 legacy LE 1M advertising，忽略 scan response／extended／periodic reports；不依賴新 GATT central 連線。
- CID 空欄／0000 為 unused，全部空欄代表不過濾，但仍只回報 MFG／Service Data。
- 尚無 MAC whitelist／MAXRPT setting／realtime XYZ batch；依 min(GTIME,15) 決定每周期上限，不宣稱完整 Gateway 等價。
- CID 變更成功後清除舊 cache/queue；GTIME 變更重設周期，保留 pending cache。
- host sanitizer tests、AD malformed/fuzz、XOR vector、排程／去重／wraparound；build log `/tmp/et100_cid_scan_build.log`。
- header／CRC／FlashEnv 範圍驗證後 flash，log `/tmp/et100_cid_scan_flash.log`；BLE scan／過濾／保存／真實 payload 由 Stanley 驗證。

---

# 歷史施工單 — Phase 1b.3 GTIME 查詢、設定與保存

## 本次分支與授權

- Branch：`v0.4.0_evb_gtime_settings`；base：`v0.3.0_evb_serial_gchk`。
- Stanley 同意 GTIME 查詢、設定、FlashDB 保存；依前次已確認分工，Codex 修改／build／flash，Stanley 操作 UART／HTML 及 reset 測試。
- 保留前次未提交變更，不 commit/merge/push。SDK patch 數量 0。

## 本次範圍

- 修改 `fw/src/serial_gchk.c`／`.h`：GGTIME、SGTIME、FlashDB user key `0x00030002`、版本 `0.4.0-dev`；預設 15、合法 1-1800 秒。
- 修改 `fw/src/evb_serial_main.c`：保留原 FlashDB 初始化流程並傳遞成功狀態，避免失敗時假裝設定成功。
- 修改 `fw/tests/test_serial_gchk.c`：值範圍、儲存失敗保留舊值、mock reload、parser regression。
- 修改 `fw/tools/build_evb_serial.sh`：映像必須位於既有 FlashEnv offset 800 KiB 之前；不擴大 linker 或更改分區。
- 修改未執行的 optional `fw/tools/evb/test_serial_gchk.py`，同步 GCHK 開發版識別。
- 更新 `fw/docs/02_protocol/01_evb_serial_gchk.md`、新增 `fw/docs/04_testing/03_evb_gtime_settings.md`；同步 platform plan、REVIEW_REPORT、EVB README、bring-up status。

## 行為與驗收

- 沿用 SDK FlashEnv（internal flash offset 800 KiB、12 KiB），不清空 DB、不改 BT keys。
- 初始化失敗：GGTIME／SGTIME valid=0；缺失／不合法 record 回到 15 秒，可由 SGTIME 重寫。
- SGTIME 僅接受十進位數字，成功保存並讀回比對後才更新 RAM 與回覆 1；同值避免重寫。
- GTIME 目前只保存設定，不啟動 BLE scan 或計時回報。MAXRPT 未實作，所以未提供 Gateway 的 MAXRPT cross-validation；需於後續 reporter/settings 階段一起加入。
- host sanitizer tests、ARM build、header／CRC／payload 檢查後，依授權 flash/readback/reset。
- build log `/tmp/et100_gtime_build.log`；flash log `/tmp/et100_gtime_flash.log`。HTML 設定／reset 保存測試由 Stanley 操作。

---

# 歷史施工單 — Phase 1b.2 UART3 GCHK 相容子集

## 本次分支與授權

- Branch：`v0.3.0_evb_serial_gchk`；base：`v0.2.2_evb_gpio_diag`。
- Stanley 在本次對話同意執行 UART3 9600 + Gateway GCHK 相容子集，供既有 HTML tester 使用。
- 前次 GPIO 診斷的未提交文件保留；下方保留其歷史施工單。
- 本次 SDK patch 預期數量：0。使用專案 source 與 build overlay；ADR 02 仍適用。

## 本次範圍

- 新增 `fw/src/evb_serial_main.c`：從現有 patched BLE example main 複製，UART3 改成 9600，GCC `_write` 丟棄 debug log。
- 新增 `fw/src/serial_gchk.c`／`.h`：bounded CRLF parser、四字元 token、GCHK、未支援命令失敗回覆、ISR ring buffer 與 task。
- 新增 `fw/targets/evb_serial.mk`、`fw/tools/build_evb_serial.sh`：透過原 SDK Makefile 的 explicit object rules 替換 main/app_at，輸出獨立 `fw/build_serial/`。
- 新增 `fw/tests/test_serial_gchk.c`；新增 `fw/docs/02_protocol/01_evb_serial_gchk.md`。
- 新增 `fw/tools/evb/test_serial_gchk.py`：9600 UART 自動驗收；Stanley 已在本次對話明確同意燒錄並測試。
- 新增 `fw/docs/04_testing/02_evb_serial_gchk.md`：本次 host／build／實機證據。
- 更新 firmware platform plan、REVIEW_REPORT、EVB README、bring-up status，說明候選產物與未驗證項。
- 保留 baseline SDK、921600 build、clock、sleep、linker、post-process 及既有 GPIO 診斷狀態。

## 驗收與限制

- Host parser tests 覆蓋拆包、黏包、token、非法輸入、超長行恢復與連續 100 次 query。
- ARM GCC build；完整 log `/tmp/et100_serial_build.log`；獨立驗證 FR509x header、CRC、payload。
- 協定規格以新文件為準；GCHK state 0 是 compatibility idle，沒有 Gateway central/GATT session 語意。
- 本輪先交付可 review 的 build。EVB 燒錄需依既有明確授權規則執行；未實測不得宣稱 UART／BLE regression PASS。
- 不修改 sibling Gateway repo，不實作 OTA、Gateway scanning/filter/settings，不 commit/merge/push。

---

# 歷史施工單 — Phase 1b.1 GPIO diagnostic: PMU key mapping

## Branch

Implementation branch: `v0.2.2_evb_gpio_diag`

Base branch: `v0.2.1_evb_bringup`

## Context

- FR3068EP-C DEV1.4 EVB has LEDs D3/D4 and buttons K1-K3.
- D3/D4 turn off on reset and turn on near the UART line `FR5090: ble_simple_peripheral.`.
- K1-K3 do not affect the current BLE example.
- Read-only SWD polling of GPIOA-E input registers showed no transitions while K1-K3 were pressed.
- SDK board code for other revisions suggests buttons may use the PMU `PPx` domain, but that mapping is not accepted as DEV1.4 evidence.
- Stanley authorized this diagnostic SDK behavior change and EVB flashing in chat on 2026-10-21.

## Goal

Identify the raw `PMU_REG_PIN_DATA` bits corresponding to K1, K2, and K3 while preserving the current BLE, UART, clock, boot-header, and debug behavior.

## Scope

### Firmware behavior

Modify only:

`fw/vendor/fr30xxc_sdk__202411/examples/application/ble_simple_periphreal/Src/main.c`

Add one low-priority FreeRTOS diagnostic task that:

1. Reads `ool_read16(PMU_REG_PIN_DATA)` every 20 ms.
2. Prints one baseline line after startup.
3. Prints only when the raw value changes, including tick, old value, new value, and changed mask.
4. Does not configure PMU direction, pull, mux, XOR, wakeup, or interrupt registers.
5. Does not drive D3/D4 or any unknown pin.

The code must carry marker:

```text
[eup-patch 2026-10-21 patch #9]
```

### Documentation

Update:

- `fw/vendor/PATCHES.md` with Patch #9.
- `fw/REVIEW_REPORT.md` with build/flash/runtime evidence and limitations.
- `docs/03_hardware/FR3068EP-C Dev1.4/README.md` with confirmed K1-K3 mapping only if runtime evidence identifies it.
- `docs/00_project/11_project_bring_up_status.md` with the diagnostic result.

### Build and hardware test

1. Clean build the existing GCC example.
2. Save the full build log to `/tmp/et100_gpio_diag_build.log`.
3. Verify the FR509x burn-header layout before flashing.
4. Flash `Project_burn.bin` with the existing `fw/tools/evb/fr3068_flash.py`.
5. Capture UART3 at 921600 bps to `/tmp/et100_gpio_diag_uart.log`.
6. Ask Stanley to press K1, K2, and K3 separately and correlate each key with a changed PMU bit.
7. Confirm BLE startup still reaches `adv start :0x00`.

## Out of Scope

- No modification outside the files listed above.
- No changes to linker scripts, Makefile, `post_process.py`, CHIP_TYPE, clock configuration, sleep Patch #8, or EVB tools.
- No PMU pin configuration changes in this first diagnostic image.
- No D3/D4 output control.
- No ET-100 production BSP or application implementation.
- No merge or push without a separate instruction.

## SDK Patch Policy

ADR 02 applies. Expected SDK patch count for this task: one behavior patch in `main.c`, Patch #9. Stanley explicitly approved it before modification. If passive PMU reads do not expose K1-K3, stop and report; do not add PMU configuration or another SDK change without renewed approval.

## Acceptance Criteria

- Build exits 0 and produces `Project.elf` and `Project_burn.bin`.
- Burn header word 7 and word 11 are `0x51525251`; word 8 is `0x00000000`.
- Flash programming and byte-for-byte verify pass.
- UART contains the normal BLE startup log and one `[KEY-SCAN] baseline` line.
- Pressing K1-K3 either yields distinct repeatable changed masks or is explicitly reported as inconclusive.
- No unexpected reset, assertion, BLE startup regression, or continuous UART flooding.

## Rollback

Remove the Patch #9 diagnostic task and task creation from `main.c`, rebuild the previously validated Phase 1b image, and flash it with the existing tool.

## Deliverable

Report:

- Changed files and exact diagnostic behavior.
- Build, header, flash, UART, BLE, and K1-K3 mapping results.
- Build and UART log paths.
- Any remaining uncertainty about D3/D4 or PMU pin configuration.
