# 12 — v0.8.4 Trial／Confirm／Rollback

分支 `v0.8.4_trial_rollback`，base `5dd5d0e`。依施工單使用 hardware bring-up loop；不改 SDK、不擦寫 `0x08001000`，不做實體斷電、簽章或正式 OTA transport。原型仍只有 SHA-256，不能防偽造。

## M-WDT

**PASS，無須停下。** 使用 SDK PMU IWDT，2 秒測試門檻：清除持續 4 秒仍運行；停用後等 4 秒仍運行（CTRL 由 03→02）；重新啟用、不再清除，確實 reset 並回到舊 App。可啟動、可清除、可關閉。最初只在 TRIAL 跳轉前啟用；Stanley 後續核准修正 D3：**App always runs the watchdog; installer arms it before the jump**，TRIAL／CONFIRMED 均適用，約 60 秒。B-info hash 不變。

原始 log：`/tmp/et100_rb_wdt.log`；build：`/tmp/et100_rb_wdt_build.log`。沒有把 PMU reset reason 顯示加入 GCHK（選做項目）。

## 實作

- 共用 `fw/common/boot_state.c/.h`：兩個 8 KB bank，v2 record 100 B、CRC 在 offset 92、commit marker 在 96；magic `BST2`。保留 v1 60 B record 的唯讀匯入，INSTALLED=2 視為 CONFIRMED，下一次寫入才升級格式，不先擦掉舊有效 bank。
- 狀態值：NONE=0、PENDING=1、CONFIRMED=2、BACKUP_READY=3、TRIAL=4、ROLLBACK_REQUIRED=5。另存 confirmed 長度／hash、Candidate 長度／hash、last_update（OK=0／FAILED=1／ABORTED=2）。回滾成功仍保留失敗 Candidate 身分，但 confirmed hash 指向舊 App。
- PENDING 先驗 Candidate 與內建 confirmed App，再備份 NOR `0x220000–0x41FFFF`，讀回驗證才提交 BACKUP_READY；實際備份已符合 confirmed hash 時重用，不只比較 metadata 字串。備份錯誤中止更新、保留舊 App，標 ABORTED。
- BACKUP_READY 才能擦 App；完成後提交 TRIAL，再啟用 watchdog 跳 App。TRIAL 未確認就重開，先提交 ROLLBACK_REQUIRED，再驗 Rollback 並從頭搬回，成功後 CONFIRMED／FAILED。任何中斷都以內建 state 為唯一依據，不從 NOR journal 猜測安裝狀態。
- App 在既有 serial/main loop 每約 1 秒清除 watchdog，確認後仍持續；BLE ADV_START 成功、FlashEnv 初始化與讀寫探針通過、迴圈連續運行 30 秒才呼叫共用 confirm API。迴圈超過 2 秒未前進會重新計算健康窗口。FlashEnv 探針用獨立 key `0x00030004`，寫本次 trial sequence 並讀回，不改 GTIME／CID。
- Installer 每次跳 App 前啟用 watchdog；App 每次開機在 `pmu_init()` 返回後立即呼叫 `iwdt_init`／`iwdt_Enable`／`iwdt_Refresh`，不只依賴繼承設定。**剩餘盲窗**：SDK `pmu_init()` 內清掉 WDT clock，若在該處卡死不能保證 reset；返回後緊接重新 arm 的指令亦有極短交接時間。未更改 SDK，不宣稱此盲窗已消除。
- App confirm API 只接受 TRIAL 且 hash 等於自己完整 MCUboot image；錯誤 hash 不寫 NOR／內建 state。App 寫入 adapter 只允許 boot-state 區，不允許 App、Settings 或 B-info。
- 測試 build flag 預設皆關：NO_CONFIRM 仍清除 watchdog，但不確認；HANG 在 trial 約 5 秒後關 IRQ 卡死，停止清除。dev loader 的 reset mask bit 8 僅供備份途中 MCU reset 一次；清除 flag 先持久化，避免無限測試循環。

## Host／Build

Host sanitizer 矩陣與既有 UART／BLE 回歸 **PASS**：備份／TRIAL／confirm／rollback 各階段中斷、v1 匯入、備份失敗中止、已驗證備份重用、錯誤 hash confirm 拒絕，以及內建 copy 斷電矩陣。模型包含 8-byte NOR PP、torn program／erase 與 weak erased sector；不額外做晶片 retention 特性研究。

Logs：`/tmp/et100_rb_host.log`、`/tmp/et100_rb_copy_host.log`、`/tmp/et100_rb_serial_host.log`。Installer build `/tmp/et100_rb_installer_build.log`；App 四種 build 分別為 `/tmp/et100_rb_app_{A,B,no_confirm,hang}_build.log`。保留 SDK 既有 RWX LOAD warning，無新增 compiler warning。

## EVB M-RB

前置：新 installer 成功讀取 v0.8.3 INSTALLED seq 5，舊 App 正常啟動；再經 loader 安裝含 health/confirm 的 A baseline，自動確認 seq 9。見 `/tmp/et100_rb_installer_flash.log`、`/tmp/et100_rb_setup.log`。A=0.8.4-dev，B=0.8.5-dev **僅為區別測試映像，不代表 v0.8.5 功能或釋出**。

| 施工單情境 | 結果／log |
|---|---|
| 1 正常 A→B，確認後重開仍為 B | PASS：seq 13／CONFIRMED／OK；GCHK 0.8.5-dev，完整 readback hash 相同。`/tmp/et100_rb_normal.log` |
| 2 NO_CONFIRM 與 HANG 各一次，回滾至 A／FAILED | NO_CONFIRM PASS：確認仍為 TRIAL 後手動 reset，seq 23／CONFIRMED／FAILED，還原 A。HANG 首輪失敗，修正後自主回滾至修正版 A、seq 37／CONFIRMED／FAILED；UART runner 逾時，改以 SWD state／完整 hash 佐證，記錄限制見下節 |
| 3 備份中 MCU reset 一次，重開完成更新 | PASS：B→A，備份第一個 sector 完成後 MCU reset 一次，重做後 seq 18／CONFIRMED／OK。`/tmp/et100_rb_backup_reset.log` |

**M-RB 停下待 review**：正常更新、NO_CONFIRM、備份中 reset 沿用首輪 PASS；修正後 HANG 有自主回滾的 state／完整 hash 證據，但 UART runner 未通過，不能宣稱整套自動 runner 全 PASS。修正版 confirmed A 的正常開機與 120 秒 watchdog 檢查 PASS，詳見最後一節。

每個情境前後核對 B-info SHA-256 `ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7`，並以 SWD 讀回 App 完整 hash 與 state；不收 throughput 或細部 timing 統計。

## 重現

```sh
clang -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -Ifw/common -Ifw/installer -Ifw/include -Ifw/src \
  -Ifw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/include \
  fw/tests/test_trial_rollback.c fw/common/boot_state.c \
  fw/installer/{candidate,install,transaction}.c \
  fw/vendor/fr30xxc_sdk__202411/components/modules/mbedtls/library/{sha256,platform_util}.c \
  -o /tmp/et100_test_rb
/tmp/et100_test_rb
bash fw/tools/test_evb_serial_host.sh
```

## 首輪停止紀錄：App HANG 時 watchdog 未 reset

**以下為首次停止時的歷史紀錄，不是修正後板況。** 當時 M-RB 尚未通過，依 Stanley 指定的 watchdog 停止條件停止。M-WDT 的 installer 獨立測試可以清除與 reset，但保護交接到 App 後失效，不能以獨立測試 PASS 代替情境 2 驗收；後續 Stanley 接受根因並核准本輪修正與精簡重測。

- `/tmp/et100_rb_hang.log`：HANG build 進入 TRIAL、installer 記錄啟用約 60 秒 watchdog；100 秒等待期限內沒有出現 reset／回滾。
- `/tmp/et100_rb_hang_diag.log`：停止後只讀診斷。state seq **26／TRIAL**；PC `0x08026CE2`、PRIMASK=1、CFSR=0、HFSR=0，符合測試版關 IRQ 卡死，非 HardFault。
- PMU clock enable（0x04）=`0x05`，**WDT clock bit 1=0**；WDT control（0x10）=`0x13`，EN 仍為 1。SDK `driver_pmu.c` 的 `pmu_init()` 以 `ool_write(PMU_REG_CLK_EN, PMU_LP_TICK_CLK_EN_BIT)` 覆寫時脈，之後只 OR RTC clock；App 的 `evb_serial_main.c` 會呼叫該函式。這與讀回 0x05 一致，證明 App 初始化清掉了 installer 先開的 WDT clock。
- 建議下一輪在**專案 App** 的 PMU 初始化後恢復 watchdog clock／配置並驗證交接，不改 SDK；是否需要重設 reload 配置需修正時驗證，不能只憑程式碼宣稱已解決。
- 停止當時 EVB 仍運行 HANG／TRIAL B，SWD 可連，尚未手動 reset，也未宣稱 HANG 已回滾。confirmed hash 指向 A `748e53d2...679b7d`；Candidate 為 HANG B `755a6a60...ade198`。停止前唯讀 B-info SHA-256 仍與基準相同。

首輪停止時正常 build 為無測試 flag 的 A（280016 B，SHA-256 `748e53d2a58a6ce4eb763dcf0e272215712ede7e7a0f0c4b2a4e802da9679b7d`）。首輪測試檔 `/tmp/et100_rb_{A,B,B_no_confirm,B_hang}.bin`；首輪 installer raw 39232 B，burn SHA-256 `fe5772ccd090086f29d00c9d887aac8d905e6f77bb22615266570fd8869b0926`。修正後產物與結果另列下節。

Host／package／B-info tool guards／`check_plan_vs_code.py` PASS；本輪檔案 whitespace PASS。全 repo `git diff --check` 唯一問題仍是既有硬體 README 第 177 行 EOF blank，未修改該檔。施工期間出現的 06 文件與 `docs/02_protocol/` 變更非本輪操作，保留原樣。

未 commit／push。本輪依 Stanley 明確授權同步 06 §5 與施工單 D3；platform／status／REVIEW_REPORT 的 Step 7 同步留待 review 後。

## PMU clock 根因修正與精簡重測

Stanley 接受根因並重新授權 loop：只修專案 App 的 PMU 初始化交接，不改 SDK；installer 每次跳轉前 arm，App 每次開機重新 arm，確認後仍定期 refresh。本輪未重跑正常 A→B、NO_CONFIRM 或備份中 reset 情境。

1. 先 reset 原 HANG／TRIAL seq 26：installer 回滾至原 A，seq 28／CONFIRMED／FAILED；GCHK、完整 App readback 與 B-info 均通過。Log：`/tmp/et100_rb_wdt_fix_recover.log`。
2. 燒入修正版 installer，再透過既有 loader 部署修正版正常 A 作為健康 baseline，避免最後回滾至仍會關 WDT clock 的舊 A。A 自動確認為 seq 32／CONFIRMED／OK。Logs：`/tmp/et100_rb_wdt_fix_installer_flash.log`、`/tmp/et100_rb_wdt_fix_setup.log`。
3. 只重測 HANG：下載修正版 HANG B，loader 提交 PENDING seq 33 並自行 reset；之後**沒有主機 reset／燒錄**。SWD 最終讀回 seq 37／CONFIRMED／FAILED、Candidate hash 為本次 HANG B，confirmed hash 為修正版 A，完整 App readback MATCH。符合 PENDING→BACKUP_READY→TRIAL→ROLLBACK_REQUIRED→CONFIRMED 的五筆連續 state。HANG build 在 trial 主迴圈約 5 秒後關 IRQ 停止 refresh；自行重開回滾與這個測試設定一致。

**觀測限制，不能隱藏**：`/tmp/et100_rb_wdt_fix_hang.log` 的 UART runner 在 PENDING 後未收到 `INST COMPLETE`，180 秒逾時，**runner 本身 FAIL**，未取得 HANG 入口／watchdog reset／回滾過程的完整 UART 訊息。補驗 `/tmp/et100_rb_wdt_fix_install_diag.log` 讀到上述 state 與 B-info；該診斷後段誤嘗試在 running core 讀 PC，pyOCD 拒絕，沒有取得 PC。`/tmp/et100_rb_wdt_fix_hang_verify.log` 完整 hash MATCH，但當時 GCHK 亦逾時；不能把這支腳本整體標成 PASS。自主回滾的依據是沒有外部 reset 的 state 變化、Candidate／confirmed 身分與完整 readback，不宣稱已錄到 watchdog reset reason。

正常 build 輸出已恢復為不帶測試 flag 的修正版 A；版本 0.8.5-dev 僅供 HANG 測試區別。

| 產物 | 長度／SHA-256 |
|---|---|
| 修正版正常 A，0.8.4-dev | 280240 B；`3e340f6de14e6daacb621e5d90326b56af0dad9e9f3350a7184b3929ec1008d2` |
| 修正版 HANG B，0.8.5-dev | 280248 B；`7f9ec917b7ffe411f5618e54bc95e4683a179acdec3201186edff01fafd9359e` |
| 修正版 installer | raw 39256 B／burn 47448 B；burn SHA `b4342cfb56a7266331bcfbfef00d5fa9a82ccb5dcada59999c330e2a8fad58f6` |

修正版 build logs：`/tmp/et100_rb_wdt_fix_installer_build.log`、`/tmp/et100_rb_wdt_fix_hang_build.log`、`/tmp/et100_rb_wdt_fix_A_final_build.log`。Host sanitizer 矩陣重新編譯及執行 PASS（687 cases），UART／BLE 回歸 PASS：`/tmp/et100_rb_wdt_fix_matrix.log`、`/tmp/et100_rb_wdt_fix_host.log`。DFU package、B-info guard 與 `check_plan_vs_code.py` 亦 PASS。

### Confirmed A：一次正常開機與 120 秒檢查

回滾後只做一次正常 MCU reset，installer 記錄 `BOOT kind=2 last_update=1`、`BOOT WDT 60s enabled`，GCHK 回覆 0.8.4-dev。第一支診斷腳本 `/tmp/et100_rb_wdt_fix_confirmed.log` 讀到 `CLK_EN=0x07`、`WDT_CTRL=0x03`，但另加的 32-bit WDT LEN 讀回等於 1920000 斷言失敗（讀到 0）。沒有以該欄位讀回推論倒數器語義，也沒有因此 reset／重刷；保留原始診斷失敗記錄。

接續**同一次開機，不再 reset**，`/tmp/et100_rb_wdt_fix_confirmed_120s.log` PASS：

- 120 秒內五次 SWD 抽樣，WDT clock bit 1 與 enable bit 0 持續為 1（CLK_EN 07／CTRL 03）；CFSR／HFSR 均為 0。
- FreeRTOS uptime 抽樣持續增加（tick 80023→106174→132293→158400→184490），每次 GCHK 均回覆 0.8.4-dev，state 始終 seq 37／CONFIRMED／FAILED；未觀察到非預期 reset。SWD 抽樣短暫 halt 以避免與 App 的 FRSPIM 交易衝突，因此 tick 差不是頻率／效能統計。
- 最後完整 App readback SHA-256 等於修正版 A；B-info SHA-256 等於本報告基準。正常 build 輸出也與此 A 相同，沒有留下 HANG flag。

最終 EVB 為修正版正常 A（0.8.4-dev），已停止硬體操作，交 M-RB review；**UART 過程訊息缺口與 pmu_init 盲窗均仍保留為限制**。不改 SDK、不 commit／push。Scope 內 whitespace 與新檔案檢查通過；全 repo `git diff --check` 仍僅有非本輪硬體 README 的既有 EOF blank 警告，未修改該檔。
