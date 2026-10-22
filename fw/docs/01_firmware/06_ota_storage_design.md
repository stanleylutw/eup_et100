# ET100 NOR Staging 與三入口更新規格

Version: v0.2 REVIEW DRAFT（2026-10-22：BLE 更新採 SMP＋MCUboot image 格式，見 §6.1）
日期：2026-10-21；分支：v0.8.0_ota_nor_spec；基底：v0.7.0_evb_app_gatt_server（e5274b2）。

**狀態**：Stanley 已同意產品方向與本次文件修訂。以下為候選地址及行為契約，不是 SDK／linker／FlashDB 修改、燒錄或量產授權。EVB 仍執行 0.7.0-dev，現有 FlashEnv／linker 不變。Q43-5／Q48-6、installer 可獨立開機與斷電復原為實作 gate。

## 1. 決策與來源

| 項目 | Stanley 在本對話的明確決定 |
|---|---|
| 三入口 | LTE／FTP、手機 BLE、RS232 共用更新管理器；RS232 是有線更新，不是無線 OTA |
| RS232 baud | 正常協定 9600，更新模式 115200／8N1；不採 921600 |
| 內建容量 | 按完整 2 MB 規劃，不把 SDK 1016 KB app window 誤當硬體上限 |
| Factory recovery | 使用者明確要求「no need, we need sensor ota slot」；不保留工廠映像 slot，但必須保留可獨立運作的 installer 與已確認 App rollback |
| Sensor slot | 使用者將預留容量定為 256 KB；不是允許任意 256 KB package |
| 更新方向 | 採單一內部 App＋外部 NOR candidate／rollback，不採晶片內 App A/B |
| BLE 更新協定與 image 格式（2026-10-22） | **BLE 採 Nordic SMP（MCUmgr）＋MCUboot image 格式**，沿用 nRF52840 現行 App 流程；App 端可依需要修改以支援 FR3068。見 §6.1 |
| 本次施工 | Stanley 明確同意文件／工具限定修訂，只恢復指定[08 報告](../04_testing/08_evb_internal_flash_size.md)及其工具，不恢復其他分區修改、不改 SDK、不執行工具／燒錄 |

本文件以 byte／hex 定義邊界；文中 KB／MB 採二進位容量，1 KB=1024 B、1 MB=1048576 B（等同 KiB／MiB）。所有 end 地址均含該 byte。

## 2. 內建 Flash 候選布局：2 MB

| 區域 | CPU 起始地址 | 結束地址 | Bytes | 容量 |
|---|---|---|---:|---:|
| ROM A 資訊段（header） | 0x08000000 | 0x08000FFF | 4096 | 4 KB |
| ROM B 資訊段（**不得寫入**，目前全 00） | 0x08001000 | 0x08001FFF | 4096 | 4 KB |
| 自有 installer | 0x08002000 | 0x0801FFFF | 122880 | 120 KB |
| ET100 App slot | 0x08020000 | 0x081DFFFF | 1835008 | 1792 KB |
| Settings／KV | 0x081E0000 | 0x081FBFFF | 114688 | 112 KB |
| Boot／Update state | 0x081FC000 | 0x081FFFFF | 16384 | 16 KB |
| **合計** | 0x08000000 | 0x081FFFFF | **2097152** | **2048 KB** |

前 128 KB 是 **8 KB header＋120 KB installer**，不是額外再加 8 KB。先前把 0x08000000..0x08002000 寫成「原廠 bootloader」及重複扣 header 的草案有誤；mask ROM 不在這段 user Flash，這裡是 image header 保留區。

現有成功燒錄格式使用 FR509x header 與 exec_offset=0x2000；未因此證實 ROM 完整校驗策略。候選設計讓 header 的 code_length／CRC **只涵蓋 installer 載入映像**（含它所需 RAM-code／data load image），不涵蓋後方 App。一般更新只寫 App slot，永不改 ROM header／installer。**前提：ROM 不驗 header 指定範圍外內容，且允許第二階段 installer 自行跳 App；須由 Q43-5 或獲准實測確認。** 不能直接把現有整個 App 的 Project_burn.bin 當 installer factory image。

App slot 1792 KB 是包含所需 App image header／驗證結構與程式資料的總上限，不等於可用 C code bytes。實際 executable budget 要扣封裝／alignment，不能用 2 MB Candidate 容量放寬 App 上限。標準 OTA 不更新 installer／header／Settings；installer 自更新另立安全需求與授權。

### ROM A／B 資訊段（2026-10-22，依原廠 `examples/common/ota/FR30xx OTA机制.pdf`＋`ota.h`）

前 8 KB 不是單一 header，而是 ROM 的兩個 4 KB 資訊段：A（0x08000000，目前 image 資訊）與 B（0x08001000，ROM OTA 用）。開機時 ROM 檢查 B 資訊段的 `image_valid`／`info_crc`／`ota_magic`，有效才把**內建 Flash** 的 B 代碼段以 `copy_unit` 搬到 A 代碼段（有 copy flag 可斷電續搬），否則直接跳 0x08002000。流程圖中正常開機不驗 A 代碼段 CRC（v0.8.3 M-E0 實測確認）。

- **ROM OTA 不能從 NOR 安裝**，且原廠 A／B 需在內建 Flash 放兩份 App（上限約 960 KB），故維持自有 installer＋NOR staging。
- **硬規則：0x08001000 sector 永遠不擦、不寫，維持原內容。** 目前內容是全 0x00（`post_process.py` 以 `seek(0x2000)` 留下檔案空洞；原廠 baseline 與所有 burn image 皆如此，EVB 一直以此正常開機）。全 00 與全 FF 對 ROM 都是「無 OTA」，但不主動改成 FF。Installer、App、工具都不得寫入；燒錄工具須跳過此 sector；SDK vendor OTA service（`examples/common/ota`）會寫 B 資訊段，產品 App 不得啟用。

### 現行 FlashEnv 風險與遷移

目前 SDK fal_cfg.h 的 FlashEnv 在 **0x080C8000、12 KB**，位於目前 linker window 中。build guard 將 burn image 限於 Flash offset 800 KB 前，尚未撞設定；raw App 起點是 0x08002000，並非目前可安全使用整個 1016 KB window。將來新 App slot 包含舊 FlashEnv 地址，**未完成遷移不得安裝新 App**。

遷移施工單必須處理舊 schema／BT bonding／GTIME／CID／auto-GATT keys、版本與 CRC、可重入 copy／readback，以及中斷／降版政策。App rollback 不代表 Settings schema 自動 rollback；未確認新 App 前不得做破壞舊版可讀性的設定遷移。112 KB 是新 Settings 容量預留，不是現在 FAL 已配置的大小。

## 3. 外部 NOR 候選布局：32 MB

以下是 NOR-relative offsets，不能與上表 CPU 地址混用。

| 分區 | 起始 offset | 結束 offset | Bytes | 容量 |
|---|---|---|---:|---:|
| Download journal／metadata | 0x000000 | 0x01FFFF | 131072 | 128 KB |
| ET100 Candidate | 0x020000 | 0x21FFFF | 2097152 | 2 MB |
| ET100 Rollback | 0x220000 | 0x41FFFF | 2097152 | 2 MB |
| Sensor OTA | 0x420000 | 0x45FFFF | 262144 | 256 KB |
| Logs／其他資料 | 0x460000 | 0x1FFFFFF | 28966912 | 27.625 MB |
| **合計** | 0x000000 | 0x1FFFFFF | **33554432** | **32 MB** |

Logs／其他資料仍須在內部切 telemetry ring／diagnostics／wear reserve，不把全部 27.625 MB 宣稱可裝 30 天 raw CAN。原 24 MB telemetry 目標可保留，剩餘 3.625 MB 的用途待 record budget 審查。本規格取代舊 §5.3a 4 MB staging／1 MB config／3 MB reserve 的容量投影；主要 Settings 改放內建 Flash，必要擴充資料由 storage owner 另分區。

### Sensor package 相容上限

依本機 Gateway `include/sensor_ota_package.h` 已核對：

| 參數 | Bytes |
|---|---:|
| 實體 slot 預留 | 262144 |
| **允許完整 package 上限**（SENSOR_OTA_PACKAGE_MAX_SIZE） | **225280** |
| Gateway package header（SENSOR_OTA_HEADER_SIZE） | 256 |
| header 後 image 最大值 | 225024 |

所有入口在 begin／接收／finish 均驗證 package 上限；225281..262144 B 也必須拒絕，不以「slot 放得下」為由接受。完整 package 包含 256 B header；其 image 內簽章／trailer 開銷仍算在 225024 B，不能再額外加上。ET100 package 與 sensor package 使用不同格式／target 規則，不改 Gateway／sensor 契約；若未來擴大，須另行跨 repo 授權。Sensor slot 餘量 36864 B 暫保留，不能自動拿來延伸 package。

## 4. 唯一權威與狀態分工

**內建 Boot／Update state 是安裝／boot 決策的唯一依據。** 記錄 schema、遞增序號、target／App hash／長度／版本、rollback hash／長度／版本、安裝／恢復進度、trial 次數／確認、CRC 與 commit marker。需先確保舊有效 record 存在，再提交新 record；不能先擦掉唯一可用狀態。

16 KB 暫按四個 4 KB erase sectors 預算，分成兩個 8 KB journal banks（雙份、各兩 sectors）。實作前須核對真實 erase geometry、record 尺寸、每 block checkpoint 策略與磨耗；不能只因「16 KB 看起來夠」就忽略最大 record 數／GC 中斷測試。不得對每個 UART／BLE packet 擦一個 state sector。

**NOR journal 只記下載狀態**：session／source owner、target、manifest／hash／長度、持久化 offset／chunk bitmap、RECEIVING／RECEIVED／download validation 結果與 CRC。它不決定 INSTALLING、TRIAL、CONFIRMED 或 rollback；NOR manifest／檔案存在／download complete 都不能自行觸發內部安裝。

App 完整驗證 Candidate 後，才向內建 state 原子提交 INSTALL_PENDING。重開機即使 NOR journal 過期或與 state 不一致，也只依內部 state 的預期 hash／長度挑映像並重新驗證；不猜哪邊「比較新」。兩份內部 state 都損壞時進可診斷 recovery/fault，不能信任 NOR journal 將未驗證 App 標為 confirmed；實際救援入口另需安全設計。

## 5. Installer、rollback 與正常開機

### 安裝順序

1. Installer 讀內部 state，看到 INSTALL_PENDING 才初始化 NOR，驗 Candidate 型號／長度／固定 App 目的地址／hash／簽章及版本政策；不讓 package 任意指定目的地址。
2. 確認目前內部 App 是「已確認版本」，驗其與 state 綁定的 hash。將這個 App 備份至 NOR Rollback，讀回驗證 hash／完整性後才持久化 BACKUP_READY。若 Rollback **實際內容驗證**已與該 confirmed hash 相同可跳過重寫；只比 journal 的 hash 字串不足以跳過。
3. 原子記錄 INSTALLING，保留 rollback hash 與進度，**此後才允許擦除內部 App**。這時可能完全沒有可執行 App；不可依賴 App 的任何 callback／driver／RTOS。
4. Installer 自帶 NOR bus／供電／必要 board init、內部 Flash driver、RAM-code／load sections、hash／簽章 verifier、key material／公鑰存取、watchdog 與持久 checkpoint。SDK flash_write／flash_erase 已標 __RAM_CODE，但還須證明整個 call graph、literal／ISR／cache／XIP 操作安全，不是加 attribute 就等於驗證完成。
5. 分塊 erase／program／readback；在「erase 後、write 後、checkpoint 前後」任一點斷電，重啟 installer 依內部 state 重作／續作，不跳到半寫 App。重作必須冪等，NOR Candidate／Rollback 不因下載新工作被覆寫。
6. 完成後重新驗證 App，內部 state 設 TRIAL，記錄啟動嘗試並跳轉。App 自檢通過才提交 CONFIRMED；超次／失敗設 ROLLBACK_REQUIRED，再由 installer 用已驗證的 Rollback 恢復並讀回驗證。**不可拿未確認的新 App 覆寫舊 confirmed Rollback。** Trial 確認期限／次數與狀態 record 格式仍須後續施工單定義。

在 INSTALL_PENDING／備份尚未完成且舊 App 仍有效時，NOR 故障可中止更新、保留舊 confirmed App；一旦 INSTALLING 已擦除 App，NOR 故障可能使裝置停留 recovery。取消 Factory recovery 不會使這個硬體依賴消失，必須回報為維修／救援故障，不宣稱任意 NOR 故障都能復原。

### 一般開機不碰 NOR

CONFIRMED 狀態：讀內建 state、驗內部 App 後直接啟動，不初始化／讀取 NOR 來決定版本。NOR 故障不能阻擋一份有效 confirmed App 的正常開機；App 啟動後仍可依 logging 功能需求初始化 NOR，這與 boot 決策分開。

只有安裝相關狀態（INSTALL_PENDING／BACKUP_READY／INSTALLING）或恢復相關狀態（ROLLBACK_REQUIRED／ROLLING_BACK）才由 installer 初始化 NOR。TRIAL 正常再試／CONFIRMED 都不需 NOR；TRIAL 超次時先持久化 rollback 決策再進 NOR 恢復路徑。Bootloader 不需要 LTE、BLE 或完整產品 App 才能執行上述工作。

### App relocation 與 VTOR

ROM 先啟 installer @0x08002000，App 改為 **0x08020000**。SDK SystemInit 目前只做 FPU／clock／misc，未設 VTOR；不能期待它替新位置修好 vectors。跳轉契約必須確保 App vector 已驗證、初始 MSP／Reset_Handler 有效、待處理中斷／SysTick／NVIC 已清理，並將 **SCB->VTOR 設為 0x08020000**、配合 barrier／MSP 後跳轉；App startup 也可明確重設 VTOR，責任不可兩邊都缺失。

SDK freertos_sleep.c 目前會保存／還原 SCB->VTOR，可沿用但必須驗證它保存的是 App 的 VTOR，而不是 installer 的。Linker ORIGIN、ram_code load address、ISR／fault vectors 與低功耗恢復都須納入 relocation 測試；本輪不修改任何 startup／SDK／linker。

## 6. 三個 transport、兩種 target

| 入口 | 已確認需求／尚待定義 |
|---|---|
| LTE／FTP | EG800Q-EU 分塊下載交 MCU；FTP 是已批准需求，不改寫成「僅 HTTPS」。可另建議 FTPS／HTTPS，但來源安全仍以簽章為必要條件；modem 斷點行為待 AT 文件／實測 |
| BLE／App | **SMP（MCUmgr）over BLE**，見 §6.1；MTU-aware、ACK／重連續傳由 SMP image upload 的 `off` 機制提供 |
| RS232 | 9600 正常協定協商 → 115200／8N1 binary 傳輸；切速 ACK 順序、逾時與雙方恢復規則必須定義，不假定 RTS/CTS 已接線 |

API 候選 begin(manifest,source)／write(session,offset,data)／query_progress／finish／cancel。第一階段全系統僅一個 update owner，兩個 target 也互斥：ET100 寫 Candidate，sensor 寫 Sensor slot。切換 transport 須先釋放／驗證映像 ID／明確接管，不能交錯下載兩個檔案。分塊 ACK 只確認已持久化範圍，重傳須可冪等處理；NOR 已寫過的不同內容不能不 erase 就覆寫。

ET100 走 installer；Sensor 走現有相容 package＋BLE DFU relay，不走 MCU App 安裝器。Sensor 更新失敗不應修改 ET100 的 confirmed App／boot state，但 sensor 自身的 boot／rollback 保證需依其 DFU 契約確認。接收／續傳／授權都要套用各 target 的大小／型號／簽章規則。

### 6.1 BLE 更新：SMP＋MCUboot image 格式（決策 2026-10-22）

**決策**：ET100 的 BLE 更新沿用 eup_gateway／nRF52840 現行流程（`eup_gateway/prj.conf` 的 MCUmgr＋MCUboot；App 流程見 `eup_gateway/docs/protocol_cache/03_eup_aiot_ota_app_validation.md`），讓同一個 App 能更新 FR3068。App 端可配合修改（Stanley 確認）。

```text
dfu_application.zip
  ├─ manifest.json        board / soc / version（FR3068 新增對應值）
  └─ *.signed.bin         imgtool sign 產生的 MCUboot image，內含 EUPFWV1 metadata
App：讀 EUPFWV1 → 連線 → GET_FW_INFO / GET_DEVICE_ID → 相容性比對 → SMP image upload
FR3068：SMP server → OTA manager → download journal → NOR Candidate → installer
```

| 項目 | FR3068 端要求 |
|---|---|
| SMP transport | SMP GATT service／characteristic 與 Nordic 定義相同（service `8D53DC1D-1DB7-4CD3-868B-8A527460AA84`）；CBOR 編碼；只實作需要的子集 |
| SMP 指令子集 | img group：upload、state read／test／confirm；os group：reset。其他指令回 not supported |
| Upload 對應 journal | SMP 分包大小不固定，RAM 湊滿 4 KB 才 `write_chunk`；回給 App 的 `off` = **連續已 commit 的 byte 數**，App 重連後從該處續傳；第一包的 `len`／`sha` 對應 journal manifest（session 身分見 v0.8.2 M1） |
| 狀態對應 | upload 完成＝journal VERIFIED；`test`＝內建 state INSTALL_PENDING；`reset`＝進 installer；新版開機自檢通過後＝CONFIRMED（`confirm` 由 App 或韌體自身發出，規則於 installer 施工單定義） |
| Image 格式 | FR3068 App binary 以 `imgtool sign` 包成 MCUboot image（header＋TLV）；installer 驗 header magic、image hash TLV 與簽章 TLV，再安裝到 App slot；EUPFWV1 metadata 放在 image 內供 App 掃描 |
| 裝置資訊 | FR3068 以「被更新裝置」身分回應 `GET_FW_INFO`／`GET_DEVICE_ID`（目前 FR3068 只有讀取 sensor 的 client 端，`src/eup_aiot_metadata.c`） |
| 相容性 | `manifest.json` 的 board／soc 與 EUPFWV1 `hardware_version` 要能讓 App 擋下「nRF 韌體刷進 FR3068」及反向錯誤 |

**同時適用 RS232／LTE**：三個入口共用同一個 MCUboot image 格式與 installer 驗證，transport 只負責把位元組送進 journal；RS232／LTE 不必使用 SMP。

**開發驗證**：App 修改完成前，先用 Nordic 官方 **nRF Connect Device Manager**（通用 SMP client）驗證 FR3068 SMP server。

**EUPFWV1 `metadata_crc32_le` 定義（2026-10-22）**：IEEE CRC-32（同 Python `zlib.crc32`），涵蓋 `struct eup_fw_info` 前 282 bytes（CRC 欄位之前全部），little-endian 儲存；寫法沿用 `eup_gateway/tools/sensor_ota_package.py:146`。既有 nRF 韌體此欄位皆為 0（`eup_gateway_fw_info.c:24`、`eup_aiot/src/main.c:94`）。

**平台規則（裝置端與 App 端共用原則）**：

| 平台 | `metadata_crc32_le` | 簽章 |
|---|---|---|
| nRF（既有，App-side） | 0 = 未提供，不驗；非 0 則必須正確 | 依現行 MCUboot 設定 |
| FR3068 | **必須非 0 且正確；0 一律拒絕**；新打包工具會回填 CRC | Q42 定案後必驗 |

- **FR3068 installer**：Candidate 的 EUPFWV1 必須是 FR3068 project／hardware，且 CRC 必須非 0 並符合前 282 bytes 的 IEEE CRC-32；0 或不符一律拒絕。Host tests 使用相同嚴格規則（Stanley 2026-10-22 review 再確認）。「0 = 未提供」只適用 nRF 的 App-side 相容規則，不適用 FR3068 installer。
- **App 選規則的依據**：以**連上的裝置**（`GET_FW_INFO`／`GET_DEVICE_ID` 回報的 SoC／hardware_version）決定平台，再要求 `manifest.json`（soc／board）與 image 內 EUPFWV1（project／hardware_version）**三者屬於同一平台**，不一致直接擋下；不可只依 package 自己的 metadata 決定用寬鬆或嚴格規則。

**App 端待辦**（App 團隊，**排在 FR3068 韌體端完成之後**，Stanley 2026-10-22）：允許清單加入 FR3068 的 board／soc／hardware_version；依上表實作平台規則與三方一致檢查；確認 SMP MTU／reassembly 設定可用於 FR3068 的 BLE stack。在那之前以 nRF Connect Device Manager 驗證 FR3068 端。

**仍待定**：簽章演算法與金鑰管理（Q42）、MCUboot header size／對齊與 App slot 0x08020000 的配合、`confirm` 由誰發出。

## 7. EVB 與產品差異

| 項目 | EVB FR3068EP-C DEV1.4 | ET100 FR3068E-C |
|---|---|---|
| 外部 NOR | GD25LQ128，16 MB、1.8 V | XM25QH256D，32 MB、3.3 V |
| 本草案 OTA 區 | 最高資料 end 0x45FFFF，全部低於 16 MB，可測 staging／rollback／sensor slot | 同 offset；先核對正式 routing／電源 |
| 高地址驗證 | 不能驗產品 16 MB 邊界以上／4-byte addressing | 必測 0x01000000 以上與最高 sector 無 alias |

Logs 從 0x460000 起，EVB 只能使用其 16 MB 實體範圍內的部分；不能照 ET100 32 MB end 擦寫。[08 內建 Flash 實測](../04_testing/08_evb_internal_flash_size.md)只屬 EP-C／debugger FLM 證據，不證明 E-C 的 XIP runtime erase 安全。

**v0.8.1 EVB U11 實測（2026-10-22，G1 與 T2–T7 PASS，T8 未做）**，詳見 [09 U11 NOR 報告](../04_testing/09_evb_nor_bringup.md)：

| 項目 | 結果 |
|---|---|
| 晶片／介面 | GD25LQ128（JEDEC `C8 60 18`，16 MB）；GPIO PB12–PB15／PC0／PC1 function 0xC → 精簡版 QSPI1（無 indirect／DMA 暫存器，只用 STIG） |
| Geometry | SFDP BFPT v1.6：4／32／64 KB erase（20／52／D8），page 256 B；SFDP 的 8 dummy clock 在此 controller 為 `dummy_raw=1` |
| Erase 實測 avg | 4 KB 13.1 ms、32 KB 58.1 ms、64 KB 99.7 ms（各 20 次） |
| 完整性 | 06 的 Candidate＋Rollback 範圍 0x020000–0x41FFFF 寫入 3 輪，SHA-256 全部吻合 |
| MCU reset 中斷 | PP／erase 後立即 reset：NOR 有電時操作會完成，reboot 後 init 正常、可再擦寫 |
| Timeout | 實際觸發 STIG timeout 後 controller 可復原、再次 init 成功 |

給 v0.8.2 journal 的規則（目前已知）：

1. MCU reset 不代表 NOR 操作被中斷；reboot 後一律先等 WIP 清除，再以 journal commit marker 判斷，不以 sector 內容判斷完成與否。
2. 斷電中斷（T8）尚未量測，**一律假設被中斷的 sector 不可信，必須重新 erase**。G2（韌體關 LDO）已取消（無法量測 C11／C30）；實機斷電驗證延到 ET-100 實板（Stanley 2026-10-22）。
3. Debugger disconnect 會清掉 DEMCR.TRCENA 讓 DWT 停止；timeout 不可只依賴 DWT（現行實作已有迴圈上限 fallback）。

**v0.8.2 download journal（2026-10-22）**：格式與狀態機見 [07 設計](07_download_journal_design.md)。Host 斷電注入 42,388 次 PASS（含 session takeover、≤1 chunk 重傳斷言）；EVB U11 上 2 MB 下載＋3 次 MCU reset 續傳，整區 SHA-256 吻合（[10 報告](../04_testing/10_download_journal.md)）。API 已帶 session_id，`resume(manifest)` 為明確接管。

限制：T5／T6 速度為 STIG 8-byte PP（約 55 KB/s），**不代表 OTA 效能**；4-byte 定址、XM25QH256D、SPIMX2_1 後端要等 ET-100 實板；download journal／installer／OTA manager 尚未實作。

## 8. 實測證據／工具與實作 gate

以下指定證據與工具已恢復至本分支；報告標記本分支，量測事實維持原紀錄。本輪不執行工具、不重新量測，不帶回任何晶片內 A/B 分區表或其他計畫修改。

| 已納入證據／工具 | 本輪處理 |
|---|---|
| [08 內建 Flash 實測](../04_testing/08_evb_internal_flash_size.md) | 恢復既有 EP-C EVB 2 MB alias probe 與 0x081F7000 sector test 證據；未重跑，不當本輪量測、不改寫成 E-C 保證 |
| [flash_size_probe.py](../../tools/evb/flash_size_probe.py)／[flash_sector_test.py](../../tools/evb/flash_sector_test.py) | 僅還原工具，未執行／修改 Python 內容；sector 寫入仍須獨立授權，安全條件見 [工具 README](../../tools/evb/README.md) |
| Q43-5／Q48-6 | 本輪採用問題意圖並改寫成 NOR installer 候選架構，補至正式 Markdown 清單；不拷貝 A/B 假設 |
| FlashDB 位址風險 | 本文及 §5.1 明記現行 C8000／12 KB 與遷移 gate |
| 晶片內 A/B：868 KB＋868 KB 分區 | **不採用**；不套用其 §5.1，也不將 sensor slot 放回內部 Flash |

實作前必須通過：Q43-5 ROM header／CRC 範圍與自有 installer，Q48-6 exact E-C 2 MB map／保留區／runtime writes；installer 120 KB 大小可行性；NOR board driver；FlashEnv 無損遷移與 schema rollback；VTOR relocation；16 KB journal 容量／磨耗模型；完整斷電故障矩陣。

建議順序：host model／record 與 bounds → NOR driver → installer／斷電復原 → RS232 → BLE → LTE → sensor relay。此順序不代表 transport 可以繞過 boot gate 提前宣稱安全更新。簽章／公鑰／降版政策、驗證錯誤與不可用時的安全輸出須列正式產品 gate；本輪不宣稱 secure boot root-of-trust 已成立。
