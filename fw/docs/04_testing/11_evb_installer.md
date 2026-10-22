# 11 — v0.8.3 Installer／App relocation（M-E0／M-INST）

分支 `v0.8.3_installer`，base `a72a97e`。M-E0 已 review PASS；接續 M-INST 的 NOR 安裝／dev loader／boot state 與 §8 DFU 打包。trial／rollback 不在本輪範圍。**SHA-256 only、不驗簽章，不安全，原型專用**；MCUboot image 格式不代表已移植 MCUboot bootloader。

## 授權與修訂

依 [施工單](../../IMPLEMENTATION_PLAN.md) §0，Stanley 預先授權本 branch EVB 燒錄，但禁止對 B-info sector `0x08001000–0x08001FFF` erase／program。補充授權：`serial_gchk.c` 只改版本字串；建立 `fw/.venv`、加入頂層 ignore、官方 PyPI imgtool 精確 pin=2.4.0。

唯讀確認既有 B-info 全 00，不是 FF，原 0.7.0-dev 備份亦相同；SDK post_process 在該範圍 seek 產生檔案空洞。Stanley 修訂規則：全 00／全 FF 都視為沒有 ROM OTA；其他混合內容僅警告，不修改。映像若涵蓋 B-info，燒錄前要求 image bytes 與 board 完全相同，否則拒絕整個操作；相符也必須整個 sector 跳過。每次測試前後核對 hash。

既有 B-info SHA-256：`ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7`（4096 zero bytes）。沒有為了符合舊全 FF 規則而 erase。

## M-E0 實作（歷史紀錄）

- 專案 linker `ld/evb_app.ld` 將 App vectors 放在 0x08020200，FLASH 長度 0x1BFE00；PRAM／SRAM 沿用原 128 KB／512 KB。共用 sections 在 `installer/sections.ld`，保留 SDK startup copy symbols 與 ram_code_front，無 SDK 改動。
- stub 獨立 linker @0x08002000、120 KB 上限，無 BLE／FreeRTOS／NOR。ROM 的 FR509x code_length／CRC 只涵蓋 stub。stub 以 115200 UART 印 B-info hash、驗 App header／SHA TLV／vector，關 IRQ／SysTick、清 NVIC enable／pending，設 VTOR=0x08020200、MSP 後跳 Reset_Handler。
- App 沿用原 SDK startup，從新 flash LMA 複製 PRAM／data；FreeRTOS 首 task 會重新開 IRQ。jump 絕不從 App startup 返回 stub PRAM（App 會覆蓋同一 PRAM）。本輪未宣稱 sleep relocation，Patch #8 仍關 sleep。
- `include/fal_cfg.h` 排在 SDK includes 前，FlashEnv 改為 offset=0x1E0000、112 KB；不遷移舊資料。實際 fal_partition.o 已比對新 offset／size，不含舊 800 KB／12 KB table。
- 移除 vendor OTA service 註冊，mk 在 SDK prerequisites 展開前過濾 ota.o／service_ota.o；最終 map／nm 不含該服務或 OTA objects。未提供新版 BLE OTA。
- 官方 imgtool 2.4.0 在 `fw/.venv`，pin 見 `tools/requirements-imgtool.txt`。兩個 build scripts 都先檢查 venv 與 exact installed version，缺少時只顯示安裝指令並失敗，不自動安裝、不改系統 Python。
- App 使用官方 `imgtool sign --header-size 0x200 --pad-header --sha 256`，不帶 key，不 pad slot；產物 App_mcuboot.bin。build 移除舊 App 的 FR509x Project_burn.bin，避免誤燒；stub 仍使用既有 post_process，不修改它。
- `fr3068_flash.py` raw App 模式固定內建 Flash map base 0x08000000（不以 --base 改 memory map）；先生成全部安全 segments，再交 FlashLoader sector erase，從不排入 B-info。支援 --no-reset 先放 App、再放 stub；--reset-only 無 erase/program。raw 不是範圍保護 bypass。

## M-E0 Host／Build（歷史紀錄）

| 項目 | 結果 |
|---|---|
| MCUboot parser ASan／UBSan | PASS：官方 imgtool fixture、每個截斷長度、magic／header／size overflow／flags／TLV／hash 拒絕、重算 hash 後無效 MSP／Reset vector 拒絕 |
| B-info tool tests | PASS：全 00／FF／mixed 匹配才能拆段、mismatch 拒絕、partial overlap／only-B／overflow／空映像拒絕、sector rounding 邊界、FR509x CRC 檢查 |
| Serial／GATT host regression | 七組 PASS，只有 GCHK 版本 fixture 更新為 0.8.3-dev |
| App build | text=238824、data=35784、bss=66512 bytes；raw=274616、MCUboot image=275168 bytes ≤1835008-byte slot |
| Stub build | text=23048、data=4336、bss=4608 bytes；raw=27384 ≤122880、FR509x burn=35576 bytes |

App_mcuboot.bin SHA-256 `b5383bfbd2f42d6cf9155bed2c41bf597322b094d5ef3ddb00e6bad434726cce`；stub Project_burn.bin SHA-256 `d6538e2826f6d483fc26b80809d37c0b62188d22658df07c50794cdea0e841ab`。App map：g_pfnVectors=08020200、Reset_Handler=08020DA8、ram_code LMA=08056D60，PRAM VMA=1FFE0128–1FFE3AC8。

Logs：`/tmp/et100_e0_app_build.log`、`/tmp/et100_e0_stub_build.log`、`/tmp/et100_e0_serial_host.log`。保留 SDK 既有 RWX LOAD warning；新增 stub 的 misleading indentation／unused parameter warnings 已修正重建。

測試重現：

```sh
bash fw/tools/test_evb_serial_host.sh
PYTHONDONTWRITEBYTECODE=1 python3 fw/tests/test_flash_protection.py
cc -std=c11 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined \
  -Ifw/installer -Ifw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/include \
  fw/installer/mcuboot_image.c fw/tests/test_mcuboot_image.c \
  fw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/library/sha256.c \
  fw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/library/platform_util.c \
  -o /tmp/et100_test_mcuboot_image
/tmp/et100_test_mcuboot_image fw/build_serial/output/App_mcuboot.bin
```

## EVB M-E0

**M-E0 PASS；到此停止 review，不執行 M-INST。** 首次 UART 被 HTML／serial monitor 占用，腳本在開 serial 時即停止，未燒錄；Stanley 回覆 closed 後才繼續。最終手機掃描／連線／讀取 manager 資料，Stanley 回覆 **「yes, ok」**。此為使用者確認，不宣稱長時間雙角色／完整協定／sleep 驗證。

### 燒錄與啟動

1. raw App 275168 bytes 寫入 `0x08020000–0x080632DF`，68 sectors，完整 readback PASS；--no-reset 保持 halt。
2. stub burn 35576 bytes 分成 `0x08000000–0x08000FFF` 與 `0x08002000–0x08008AF7`，**中間 B-info 沒有任何 erase/program request**。整份 readback（含原地 gap）PASS。
3. reset 後 stub 驗證 App 並跳轉，UART 115200 捕捉下列 log；App UART 隨後切為 9600。再次 reset 也捕捉到相同驗證成功及跳轉。

```text
freqchipE0 STUB 0.8.3-dev UNSIGNED PROTOTYPE
BINFO kind=ZERO sha256=AD7FACB2586FC6E966C004D7D1D16B024F5805FF7CB47C7A85DABD8B48892CA7
E0 VERIFY rc=0 vector=0x08020200 payload=274616
E0 JUMP msp=0x20001020 reset=0x08020DA9 irq=masked
```

這是本版 ROM 接受「A header code_length／CRC 僅包含 stub、App 在其後獨立區域」的實機證據，不是所有 ROM revision／E-C 晶片保證。App 自己的 vectors／startup／PRAM copy 執行成功，stub 不修改 B-info。

### UART／Settings／FreeRTOS／PRAM

9600 bps 初次 GCHK 回覆 0.8.3-dev，FlashEnv ready=1；新區預設 GTIME=15、CID=0639，符合不遷移舊設定。之後設定 GTIME=17、CID=1234／0000／0000／0000，命令回成功；再次 MCU reset 後取得：

```text
AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.3-dev;0;E010
AT$PD=BLEGW$GGTIME;1;17;E011
AT$PD=BLEGW$GCID;1;1234;;;;E012
```

最初一次 SCID 只送單欄，依原協定被正常拒絕（rc validity=0）；改用正確四欄後成功，沒有改 GCHK／SCID parser。板上目前維持此測試設定，方便 review，並非基線 0.7.0-dev；本 plan 未要求 M-E0 後恢復舊版。

SWD 唯讀暫停檢查後恢復 CPU：VTOR=08020200、CFSR=0、HFSR=0；PC=1FFE3A68 位於 App ram_code，xSchedulerRunning=1、uxCurrentNumberOfTasks=8、xTickCount=67164。BFAR=10170004 但 BFARVALID 未設，不能當作 fault 證據；PRIMASK=1 是單點 halt 觀察，不推定常態關 IRQ，UART task／校準／BLE 與 FreeRTOS 實際仍運行。

| PRAM 區域 | VMA／新 LMA | bytes | 全區比對 |
|---|---|---:|---|
| ram_code_front | 1FFE0000／08056C38 | 296 | PASS |
| ram_code | 1FFE0128／08056D60 | 14752 | PASS |

新 Settings `0x081E0000` 起 112 KB 已有有效非 FF 內容；在實際 FAL table 確認新位址、UART set 成功及 reboot 重讀成功三項證據共同成立下，記為新區保存 PASS。沒有修改／遷移舊區，也未測 FlashDB GC／滿區／掉電。

### B-info 不變性與紀錄

App flash、stub flash、運行期間唯讀 SWD、再 reset 前後，都核對相同 `ad7facb2...92ca7` SHA-256 與 4096 zero bytes。沒有擦除、program 或修復該 sector；其非 FF 不視為故障。

完整 logs：`/tmp/et100_e0_app_flash.log`、`/tmp/et100_e0_stub_flash.log`、`/tmp/et100_e0_boot_uart.log`、`/tmp/et100_e0_uart_settings.log`、`/tmp/et100_e0_swd_verify.log`、`/tmp/et100_e0_reset.log`、`/tmp/et100_e0_reboot_uart.log`、`/tmp/et100_e0_uart_after_reset.log`。只有兩次 image flash 與後續一次 reset-only，無 rescue、無 U11 寫入、無 NOR 安裝、無其他內建 Flash sector 測試。

兩個 build scripts 均已使用持久 venv/version guard；官方 [MCUboot imgtool 格式與命令](https://github.com/mcu-tools/mcuboot/blob/main/docs/imgtool.md)及 [PyPI imgtool 2.4.0](https://pypi.org/project/imgtool/2.4.0/)為工具來源，parser 的實際 fixture 以本機 pinned 版本產物為準。

M-E0 當時停下 review；Stanley 確認 PASS 後才進行下列 M-INST。不更新硬體 README，不碰使用者既有修改，不 commit／push／merge。

## M-INST 設計與 Host 驗證

- 安裝前強制檢查唯一 EUPFWV1、schema 1、`EUP_ET100`／`ET100_EVB_` 前綴、MCUboot unsigned SHA-256 profile、vector 範圍與 boot state 的整份 package SHA-256。錯誤目標／hash 一律在擦 App 之前拒絕。
- 內建 boot state：`0x081FC000` 起兩個 8 KB bank，各一份 60-byte record；sequence、CRC、最後提交 marker。先擦另一 bank、寫入並讀回 body、提交並讀回 marker；舊 bank 留到下次更新。空白區代表 NONE；非空白且兩份都損毀時 fail closed，不猜測。
- Installer 只允許寫 App `0x08020000–0x081DFFFF` 或 boot state `0x081FC000–0x081FFFFF`；Settings、ROM header、B-info 都不在內建寫入 adapter 白名單內。
- MCU reset 後 PENDING 從頭重做；只擦本次 image 長度向上對齊的 App sectors，寫完讀回驗證才提交 INSTALLED。一般無 PENDING 開機不初始化 NOR。
- Dev-only loader：UART3 115200，開機 2 秒內 `ET100-LOAD\n`。20-byte header：`LDR1`、op u8、reserved u8=0、payload length u16、index u32、payload CRC32 u32、header CRC32 u32（前 16 bytes）；整數 little-endian。payload ≤4096 B，逐包 journal commit 後 ACK。BEGIN 48 B：session u64、length u32、SHA-256 32 B、測試 reset mask u32。CHUNK／FINISH／ABORT = op 2／3／4。這不是正式 RS232 OTA，沒有產品安全認證，不公開為量產介面。
- Loader 使用既有 session-aware journal；新 BEGIN 明確取消舊 dev session，FINISH 完成下載驗證與 Candidate 相容檢查後才提交 PENDING。錯誤映像可 ABORT 回舊 App；壞 framing／timeout fail closed。

### App 團隊：metadata CRC 相容契約

EUPFWV1 與 gateway struct 逐 byte 相同，共 **286 bytes**。`metadata_crc32_le` 在 offset **282**；IEEE CRC-32（Python `zlib.crc32`）涵蓋 block 的 **前 282 bytes**，little-endian 儲存。**FR3068 必須非 0 且正確；0 一律拒絕**，installer 與 host tests 使用同一規則。**「0 = 未提供」只適用 nRF 的 App-side 相容規則**，不適用 FR3068 installer。新 FR3068 打包工具回填 CRC；既有 nRF image 仍會因目標不符被拒絕。定義同步到 [06 §6.1](../01_firmware/06_ota_storage_design.md#61-ble-更新smpmcuboot-image-格式決策-2026-10-22)。

### Host 結果

`/tmp/et100_inst_host_o1.log`：原 M-INST ASan／UBSan、`-Wall -Wextra -Werror -O1`，**3,441 injections PASS**（1,147 個 erase/program 操作 × 操作前／中途／操作完成但回錯誤 3 模式）。每次清除模組 RAM 重新讀 boot state，再重做直到完成；保證 PENDING／INSTALLED、最終 image 一致，ROM／installer／B-info／Settings 保持原樣。原先 CRC=0 相容接受的規則有誤，本輪改為拒絕；嚴格規則重跑結果見文末。其餘測試包含正確非零／錯誤非零 CRC、project／hardware、package hash、MCUboot TLV hash、真實 nRF image 拒絕與 loader framing CRC／長度。

既有 UART／BLE host tests、MCUboot truncation／vector 測試、B-info flash-tool guards 均 PASS。`test_dfu_package.py` 唯讀呼叫現有 `eup_aiot/tools/eup_fw_info_scan.py`；獨立驗證 CRC，並與 gateway 現有 DFU zip 比較 manifest key 集合，PASS。不修改其他 repo。

重現新增矩陣：

```sh
clang -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -Ifw/installer -Ifw/include \
  -Ifw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/include \
  fw/tests/test_installer.c fw/installer/{candidate,boot_state,loader_frame,install}.c \
  fw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/library/{sha256,platform_util}.c \
  -o /tmp/et100_test_installer_o1
/tmp/et100_test_installer_o1 fw/build_serial/output/et100.signed.bin \
  ../eup_gateway/build_ms88sf2_app_ota/eup_gateway/zephyr/zephyr.signed.bin
python3 fw/tests/test_dfu_package.py
```

## M-INST EVB

**M-INST ＋ §8 驗收 PASS，到此停止 review。** 新版安裝、三次 reset、壞 SHA 與真實 nRF image 拒絕均已通過。UART／SWD 原始輸出存於 `/tmp/et100_inst_*.log`。

| 驗收 | 證據 |
|---|---|
| 新版下載／安裝 | `et100_inst_evb_install.log`：275456 B，journal VERIFIED → PENDING → INSTALLED；9600 GCHK 由 0.8.3-dev 變 0.8.4-dev |
| 三次 reset | erase 第一個 sector 後／program 第一頁後／讀回驗證途中各一次；seq 1→4 保持 PENDING，重做後 seq 5 INSTALLED |
| 壞 SHA 拒絕 | `et100_inst_evb_bad_hash.log`：改 MCUboot TLV hash 最後一 byte，但 journal manifest 使用壞檔案的正確整份 SHA，確認是 image verifier 拒絕（rc=-2），不是下載傳輸錯誤；ABORT 後原 App 0.8.4-dev 正常開機 |
| 真實 nRF 拒絕 | `et100_inst_evb_nrf_reject.log`：唯讀使用 gateway `build_ms88sf2_app_ota/eup_gateway/zephyr/zephyr.signed.bin`；target reject rc=-4；ABORT 後原 App 0.8.4-dev 正常開機 |
| 最終完整 readback | `et100_inst_final_readback.log`：內建 275456 B 與原 ET100 image 逐 byte 相同，整份 SHA 相同；boot state seq 5 kind=2、長度／hash／CRC／commit 正確；舊 bank seq 4 PENDING 保留 |
| B-info | 每次下載／reset／拒絕測試前後 SWD SHA-256 皆為 `ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7`；燒錄工具同樣跳過整個 sector |
| 無 fault | 最終 VTOR=0x08020200，CFSR=0、HFSR=0；沒有 HardFault／rescue |

EVB 保持本 branch installer ＋測試 App `0.8.4-dev`；未燒回 0.7.0，因本輪驗收目標是保留已搬家並安裝成功的 App。U11 最新 dev journal 為已下載但拒絕的 nRF image；**不會**觸發安裝，因內建 boot state 唯一權威仍為 seq 5 INSTALLED ET100 hash。正式下載開始前須明確 cancel／新 session，不把 NOR VERIFIED 當成安裝授權。

Build logs：`/tmp/et100_inst_build.log`、`/tmp/et100_inst_app_new_build.log`。Host logs：`/tmp/et100_inst_host_o1.log`、`/tmp/et100_inst_serial_host.log`、`/tmp/et100_inst_flash_guard.log`、`/tmp/et100_inst_package_test_new.log`。初版計時／交接診斷保留 `et100_inst_flash_boot.log`、`et100_inst_flash_boot_v2.log`；修正版 baseline 成功見 `et100_inst_flash_boot_v3.log`。

`check_plan_vs_code.py`：3 anchors PASS；本輪 scope 的 `git diff --check` 與新增檔 whitespace 檢查 PASS。全 repo `git diff --check` 唯一問題為使用者既有、未修改的硬體 README 第 177 行 EOF blank；沒有為了消除該警告而越界修改。SDK／其他 repo 未改，不 commit／push／merge。

### Bring-up 修正

1. pyOCD 斷線清掉 DEMCR.TRCENA，DWT 停止，初版 loader window 過久；唯讀連續取樣確認連線時 counter 才前進。修正：loader 計時入口維持 TRCENA／CYCCNTENA，另保留有限 loop budget。不改 SDK。
2. App 本身正常（VTOR `0x08020200`、8 tasks、tick 前進、CFSR/HFSR=0），但 UART divisor 留在 installer 的 115200；唯讀 GCHK 在 115200 回 0.8.3-dev，9600 收到的資料則全零。修正：跳轉前等 TX 完成，再用 SDK macro 重置 UART3，由 App 重新初始化。重測 9600 GCHK PASS。未遇 HardFault／rescue。

### 已完成安裝測試

`/tmp/et100_inst_evb_install.log`：先從 0.8.3-dev 經 dev loader 下載新版測試 image 275456 B。測試版僅以 build macro `ET100_VERSION_FLAGS='-DEUP_FIRMWARE_VERSION=\"0.8.4-dev\"'` 覆寫共用版本；不是 v0.8.4 功能或釋出，原始碼預設仍為 0.8.3-dev。

```text
STATE rc=0 kind=1 seq=1 len=275456 reset_mask=7
INST TEST_RESET phase=1 remaining=6 PENDING retained
STATE rc=0 kind=1 seq=2 len=275456 reset_mask=6
INST TEST_RESET phase=2 remaining=4 PENDING retained
STATE rc=0 kind=1 seq=3 len=275456 reset_mask=4
INST TEST_RESET phase=4 remaining=0 PENDING retained
STATE rc=0 kind=1 seq=4 len=275456 reset_mask=0
INST COMPLETE rc=0 kind=2
AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.4-dev;0;INST
```

Phase 1 = 已擦第一個 App sector、其餘尚未擦；Phase 2 = 已寫第一頁、其餘尚未寫；Phase 4 = 內建 image 讀回驗證途中（metadata scan 後的 hash 讀取）。每次 reset 前先持久提交清除對應測試 flag，避免無限測試循環；安裝狀態持續 PENDING。這是 MCU reset，不宣稱 erase/program 正在執行時的實體斷電耐受度。

### App 團隊交付包

測試版本 zip：`fw/build_serial/output/dfu_application.zip`，image：`fw/build_serial/output/et100.signed.bin`（另保留同內容 `App_mcuboot.bin`）。metadata offset 149808，CRC `0x745E889D`；image SHA-256 `7ccb32afab4ddf9e1cc2617a828033c5d4d3a2a97b93d36d36f88ba23faa41ce`。初次 M-INST Installer raw 37336 B／122880 B，burn image SHA-256 `b2895811f7778b9e2df8b8bd5ac1884cc648993d13b4949c70da6965d2487fec`；嚴格 CRC 修正版另見文末。

`manifest.json` 全文（時間欄位隨重建更新）：

```json
{
    "format-version": 1,
    "time": 1792636473,
    "files": [
        {
            "type": "application",
            "board": "dev14_fr3068ep_c",
            "soc": "fr3068e_c",
            "load_address": 134349312,
            "image_index": "0",
            "slot_index_primary": "1",
            "slot_index_secondary": "2",
            "version_MCUBOOT": "0.8.4+0",
            "size": 275456,
            "file": "et100.signed.bin",
            "modtime": 1792636473
        }
    ],
    "name": "et100"
}
```

### 限制

- `.signed.bin` 是 imgtool 慣用檔名，但本原型**沒有簽章**，只有 SHA-256 TLV；不能防止惡意偽造 metadata／image。不宣稱正式 BLE SMP 已實作或 App 實際 OTA 已通過。
- SDK QSPI0 Flash erase/write 沿用 RAM routine，WIP wait 尚無 timeout；目前硬體正常才通過，不宣稱內建 Flash 故障可自動恢復。
- 若第一次建立 boot state 就斷電而沒有任何可信 record，會 fail closed 停在 installer；本輪沒有設計救援傳輸或 trial／rollback。
- 保留讀取 M-E0 無 metadata 內建 App 的相容路徑（只驗 magic／SHA／vector）；**新 Candidate 一律要求 metadata**。
- 本輪不做 T8 實體切電，也不把 EVB U11 的結果當作 ET-100 U0103 驗證。

## M1 Review 修正：FR3068 CRC 不得為 0（2026-10-22）

**修正與驗證 PASS，停下等待 review。** 原版本把 nRF 的「0 = 未提供」App-side 例外錯套到 FR3068 installer；依 Stanley 本輪確認，現在 FR3068 Candidate CRC **必須非 0 且正確**，0 與 mismatch 都回 `IMAGE_EMETADATA (-5)`。未改其他 repo、SDK、App package 或 boot state／GC 設計。

- 修改：`installer/candidate.c` 拒絕 0；`tests/test_installer.c` 的 CRC=0 FR fixture 在重算 MCUboot TLV SHA 與整份 package SHA 後，仍必須得到 IMAGE_EMETADATA。06 §6.1 與本報告的 App 團隊契約同步修正，nRF 例外明確限定 App-side。
- Host：`/tmp/et100_inst_crc_strict_host.log`，ASan／UBSan 全矩陣 **3,441 injections PASS**；正確非零 CRC 接受、0／錯誤非零拒絕、真實 nRF target 拒絕均 PASS。
- Build：`/tmp/et100_inst_crc_strict_build.log`；installer raw **37336 B ≤122880 B**，burn image 45528 B，SHA-256 `cb00b1880425a62f03d47a70ec733656811b9e433c8eb75f4b256f77703f14f5`。燒錄工具跳過 B-info sector 並完整 readback。
- Fixture：`/tmp/et100_crc_zero.signed.bin`，275456 B；只清除 offset 149808+282 的 CRC 欄位，重算 MCUboot SHA-256 TLV。整份 SHA-256 `f7c586567f98bbb7858e24f61c50bcf5fb6f9f89e9994e9cbb3894a4663cf7cc`，loader manifest 使用此正確 hash。`/tmp/et100_inst_crc_zero_mcuboot.log` 證明基本 MCUboot 格式／SHA 驗證仍通過，不是以壞 TLV 測試 CRC。
- EVB：`/tmp/et100_inst_crc_zero_evb.log`，**只執行一次 CRC=0 下載案例**。FINISH `rc=-5`、ABORT 後原 App 於 9600 回覆 0.8.4-dev；前後 SWD 比對內建 App、16 KB boot state、4 KB B-info **逐 byte 相同**，CFSR=0、HFSR=0。boot state SHA-256 `20590a682319dbd98083757ce5bc49941286fe3b22f38769fcc22335257713e1`；B-info 維持原 `ad7facb2...92ca7`。

```text
ACK 3 0 rc=-5
FINISH rc=-5 expected=-5
ACK 4 0 rc=0 ABORT
LOADER EXIT rc=0
E0 VERIFY rc=0 vector=0x08020200 payload=274904
E0 JUMP msp=0x20001020 reset=0x08020DA9 irq=masked
AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.8.4-dev;0;INST
```

正常 `et100.signed.bin`／DFU zip 未重建或改寫；metadata CRC 仍為 **0x745E889D**，整份 SHA-256 仍為 `7ccb32af...faa41ce`，相容性／package tests 重跑 PASS（`/tmp/et100_inst_crc_package.log`）。EVB 保留嚴格 CRC installer 與原 App；U11 最新 dev Candidate 是已下載但拒絕的 CRC=0 fixture，沒有 PENDING 授權，不會自行安裝。

`check_plan_vs_code.py` 3 anchors PASS、B-info tool guards PASS、本輪 scope whitespace PASS；全 repo 仍只有既有硬體 README 第 177 行 EOF blank 警告，未修改該檔。未 commit／push／merge，沒有進入下一 milestone。
