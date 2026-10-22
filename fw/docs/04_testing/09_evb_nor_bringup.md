# 09 — DEV1.4 U11 外部 NOR Bring-up

日期：2026-10-21；分支：`v0.8.1_nor_driver_bringup`；基底：`v0.8.0_ota_nor_spec`（`9b1fdec`）。

## 2026-10-22 T2–T7 授權執行紀錄

Stanley 已 review M-G1 PASS，授權 U11 scratch T2–T7 bring-up loop 與達 M-T7 後恢復 0.7.0-dev。允許 arm／erase／program／MCU reset 中斷測試，不允許改 non-volatile status／OTP，也不允許 T8／切電；G2 等 C11／C30 實測。沿用 43,928-byte 映像，未修改 SDK 或測試韌體；僅以暫存 host runner 收集 UART／SWD 與獨立計算 pattern／SHA-256。

### T2–T5

| 測試 | 結果 |
|---|---|
| T2 | PASS：SR1／SR2=00／00，未 arm 時 erase 回 -9；arm 後才執行 scratch 寫入；無 status／OTP write |
| T3 | PASS：0x000000、0x7FF000、0x800000、0xFFF000 四點不同 signature，寫完後逐點重讀並與 host 獨立 pattern 比對；SWD DAC 0x28000000+offset 的 32 bytes 四點皆一致，無 alias 證據 |
| T4 | PASS：4／32／64 KB 各 20 次，driver 每次抹後全範圍驗證 FF；未對齊 0x10001／4096 回 -1 |
| T5 | PASS：整頁 256 bytes、跨頁 offset=0x100FD／513 bytes、部分頁 offset=0x10025／19 bytes，以及 4096-byte bench；逐 byte UART 讀回與 host pattern 相符，SHA-256 亦相符 |

T4 在 0x10000 測試，每次先 PP 8-byte zero，再 erase；因此是同一區域重複測試，不是全晶片／溫度／壽命認證。WIP 時間包含 WREN／指令／poll overhead，不含抹後全區讀回，非純晶片忙碌時間。

| Erase size／opcode | 樣本 | min／avg／max（µs） | 現有 datasheet 最大值對照 |
|---|---:|---|---|
| 4096／20 | 20 | 10898／13145／13315 | 最大樣本 13.315 ms < 500 ms |
| 32768／52 | 20 | 57951／58094／58433 | 最大樣本 58.433 ms < 1.5 s |
| 65536／D8 | 20 | 99249／99687／100151 | 最大樣本 100.151 ms < 3 s |

**T5／T6 throughput：STIG 8-byte PP, not OTA-representative。** SPI 設定 6 MHz；本測試包含 PP 前檢查、WIP polling 與讀回驗證，不能當作 page-PP／DMA／實際 LTE／BLE／RS232 OTA 效能。

| T5 內容 | bytes | program µs | decimal KB/s |
|---|---:|---:|---:|
| 整頁 | 256 | 4626 | 55.339 |
| 跨頁 | 513 | 9528 | 53.841 |
| 部分頁 | 19 | 406 | 46.798 |
| bench | 4096 | 74043 | 55.319 |

首輪 SWD DAC 比對後的韌體時間欄位全零，功能驗證仍通過，但該輪時間全數作廢。唯讀 SWD 檢查顯示連線期間 DWT 可運作；後續在 SWD disconnect 後重新 init（重新啟用韌體 trace counter）並 arm，重測後時間非零。尚未獨立擷取 disconnect 瞬間的 DEMCR，故不斷言是哪個 cleanup 寫入造成。另一次 host 重跑未先清除既有 arm，預期 -9 的 erase 實際回 0；此為已授權 scratch 範圍內操作與 runner 狀態前提錯誤，已改成每輪先 init 關閉 gate，再重新驗證 -9；未改韌體 gate 邏輯。

完整逐命令／SWD／原始時間 log：`/tmp/et100_nor_t2_t7_20261022.log`；有效 T2–T5 重跑：`/tmp/et100_nor_t2_t5_rerun2_20261022.log`。結果摘要：`/tmp/et100_nor_t2_t7_results_20261022.json`。

### T6 — 三輪 4 MB 完整性

**PASS**：固定 `0x020000–0x41FFFF`（4,194,304 bytes）、seed `0x83E19A47`，每輪先 64 KB erase、再 deterministic pattern program、全區讀回 SHA-256。三輪 MCU SHA-256 均與 host 獨立生成 pattern／計算的 SHA-256 一致：

```text
54EAE10E589E0EDE8456DF201377D80E30D8A802C3E534753BA20148CA69F617
```

| Run（從 0 起算） | program 秒 | total 秒 | decimal KB/s（program） |
|---|---:|---:|---:|
| 0 | 75.797084 | 102.619797 | 55.336 |
| 1 | 75.797564 | 107.881203 | 55.336 |
| 2 | 75.797976 | 107.958802 | 55.335 |

total 包含 erase、program、SHA 讀回與區塊進度 UART；整個 integrity 3 的 host wall time 323.086 s。SR1／SR2 結束仍為 00／00。以相同比例粗估 1792×1024-byte App 的 NOR program 部分約 33.16 s，含本測試 erase／驗證的線性估算約 44.90–47.23 s；不是實測 App 搬移，不含內建 Flash 讀寫、傳輸、簽章或安裝。**STIG 8-byte PP, not OTA-representative**，不據此定義 OTA SLA。Log：`/tmp/et100_nor_t6_20261022.log`。

### T7 — MCU reset，NOR 保持供電

**PASS**：在 `0x10000` 分別發出 8-byte zero PP／4 KB erase 後立即 MCU SYSRESETREQ；兩次 boot 都 `G1 init_rc=0 writes=locked`、SR=00／00。未送 NOR 66／99 reset opcode、未切電。

| 操作 | reset 後內容／整個 4 KB sector hash | 後續復原 |
|---|---|---|
| PP 8 bytes | 首 8 bytes=00、其餘 FF；SHA-256 `D76C97438CD27CDFEA8D4E7E581D8626F1CE7CD181AA9CBFB4252D70BBB83325` | 未 arm erase 被拒 -9；重新 arm、erase、256-byte program、host pattern 逐 byte 讀回 PASS |
| Erase 4 KB | 整區 FF；SHA-256 `F47A8EC3E9AFF2318D896942282AD4FE37D6391C82914F54A5DA8A37DE1300C6` | 同上，PASS |

兩次原操作在重新開機檢查時均已完成；init 會等 WIP 清除，但未直接量測 reset 瞬間的 WIP／實體時序，不宣稱每個 reset 都精準落於 NOR busy window，也不代表 power-cut 安全。Log：`/tmp/et100_nor_t7_20261022.log`。

**M-T7 已達成**，依本次授權立即恢復已核對的 0.7.0-dev 備份。無 SDK／linker／FAL／產品 source 變更，無 commit／push；G2／T8 仍 pending C11／C30 量測。

### M-T7 後恢復基線

**PASS，板上目前為 0.7.0-dev，不是 NOR 測試韌體。** 已將 `/tmp/et100_0.7.0-dev_before_nor_20261022.bin`（284,712 bytes，SHA-256 `effbd9aad13fb8c134ee9f39f475413e8287f78a7bab4e4f2e3007ef526921ec`）寫回 `0x08000000–0x08045827`，70 sectors，program 37.9 s，完整逐 byte readback PASS；SYSRESETREQ 後 PC=0800612A、位於 app flash。

9600 bps 8N1 唯讀送出 `BLEGW$GCHK;CHK7`，收到下列有效回覆（原始捕捉在此 frame 前另有一個非 ASCII byte，未抹除原始 log）：

```text
AT$PD=BLEGW$GCHK;1;C03232325903;Freqchip;DEV1.4_fr3068ep-c;0.7.0-dev;0;CHK7
```

此證明基線啟動與 UART command task 回應，不新增宣稱 App／BLE radio／sensor 重新驗收。未操作其他產品命令；外部 NOR 留下核准的 scratch 測試資料，未為清理而額外擦除或切電。Logs：`/tmp/et100_nor_restore_070_flash_20261022.log`、`/tmp/et100_nor_restore_070_uart_20261022.log`。M-T7 到此停止，等待 Stanley／Claude Code review。

---

## 歷史紀錄 — G1 定位與修正

以下「最新」等字眼均為各次 G1 當時快照；目前狀態以本文最上方 M-T7／恢復基線節為準。

### 2026-10-22 SFDP dummy 編碼修正（以下較早狀態保留為歷史）

43,920-byte 版已燒錄且逐 byte readback PASS。`diag-addr` 實測 `90 @0=C817`、`90 @1=17C8`，cmd_address 與請求一致；這確認 REMS 位址相位，並非 PP／erase 已實際驗證。SFDP @0、8 bytes 在 dummy_raw=1 回 `53464450060101FF`。本輪依 Stanley 授權，正常 SFDP 的 8-clock dummy 改成 raw=1；READ 03 保持 raw=0。這是本板 reduced QSPI1 的實測屬性，不推廣到其他 dummy 數、控制器或 opcode；現無其他 8-clock dummy 讀取路徑。

| dummy_raw | 實測 byte skew k |
|---|---|
| 0 | -1 |
| 1 | 0 |
| 2 | 1 |
| 3 | 2 |
| 4 | 6 |
| 5 | 10 |
| 6 | 14 |
| 7 | 18 |
| 8 | 22（0x16） |
| 9、10 | 捕捉資料全 FF，未據此推定 skew |
| 11 | timeout，rc=-3；其後收到 READY |
| 12–16 | 因 11 timeout 提前停止，未實測 |

原始 UART：`/tmp/et100_nor_diag_addr_raw_20261022.log`。此表只列觀察值，不以公式外推，也不進行 offset 補償。

修正版 host ASan／UBSan 通過，包含 SFDP raw=1、READ 03 raw=0、dummy=11 真正進入 bounded STIG timeout、IRQ 已關閉且 DWT 停止的 controller 復原，以及後續 init 成功／write gate 維持關閉。ARM build 通過：text=31,400、data=4,336、bss=8,808 bytes；raw=35,736、burn=43,928 bytes，header／CRC／payload guard PASS。Logs：`/tmp/et100_nor_dummy_fix_host_20261022.log`、`/tmp/et100_nor_dummy_fix_build_20261022.log`。

Stanley 本輪核准重建後燒錄、boot G1、id／sfdp／status，再以既有 diag-addr 觸發 dummy=11 timeout 並執行 init 驗證復原。禁止 arm／erase／program／power／cut／NOR reset-op；G2／T8 仍延後。

**最新結果：43,928-byte 修正版已燒錄，逐 byte readback PASS；boot G1 PASS，實機 timeout 後 init 復原 PASS。** SHA-256 `ee37082c5eb5fe7b7f4a3880e2440177fa8af37b457b84cba6205b8cc119875b`。燒錄範圍 `0x08000000–0x0800AB97`，11 sectors、program 6.3 s。

| 項目 | 本輪實機結果 |
|---|---|
| Boot | `G1 init_rc=0 writes=locked`，mux function C、三次 RDID 一致 |
| GPIO／供電間接證據 | PortB_H_FuncMux=CCCC9900、PortC_L_FuncMux=000000CC、OOL_FC=01；未量實際電壓 |
| JEDEC | C8 60 18 |
| SFDP header | 53 46 44 50 06 01 01 FF（1.6，兩個 parameter headers） |
| BFPT header | 00 06 01 10 30 00 00 FF（1.6、16 DWORDs、位址 0x30） |
| Density DWORD2 | 07FFFFFF → 16,777,216 bytes（16 MB） |
| Erase types（只讀解析，未執行） | 4096／20、32768／52、65536／D8，第四組未使用 |
| Page／SPI | 256 bytes（限定型號 datasheet 設定）、6 MHz 設定值，非示波器量測 |
| SR1／SR2 | timeout 前後均 00／00 |
| Deliberate timeout | `DIAG op=5A addr=000000 dummy=11 len=8 data= rc=-3`，隨後 RESULT rc=-3／READY |
| Recovery | 不重啟 MCU、不切 NOR 電源，直接 init → RESULT rc=0；id／sfdp／status 均 rc=0 |
| 一致性 | 復原前後完整 256-byte SFDP dump、geometry、erase-type 輸出完全一致 |

UART TX 順序僅 `id → sfdp → status → diag-addr → init → id → sfdp → status`。diag-addr 保留原始 dummy sweep，不自動補償 offset；遇 11 timeout 即停止，不執行 12–16。init 只重新初始化 controller／GPIO、維持供電開啟並唯讀辨識，沒有 NOR reset、WREN 或 status write。外部 NOR 未擦寫、未切電；READ 03 的資料正確性、PP／erase 實際操作仍待後續明確授權。

完整 boot／DIAG／兩次 SFDP dump／命令回覆：`/tmp/et100_nor_dummy_fix_uart_20261022.log`；燒錄與 readback：`/tmp/et100_nor_dummy_fix_flash_20261022.log`。完成後已停止，未 commit／push。

**最新狀態：43,080-byte GPIO muxscan 版已核准燒錄並逐 byte 讀回 PASS。實測 function `0xC` 三次 RDID 均 `C8 60 18`，UART id 再確認一致；SR1／SR2 均 `00`。G1 仍未通過：SFDP 位址 0 header 異常，boot 回 `NOR_ESFDP (-5)`。** 完整 boot 與原始 SFDP dump 證據見 §1。外部 NOR 未擦寫、未切電，G2／T8 延後。EVB 現為 43,080-byte 版，UART 僅送 id／sfdp／status，已停止等待審查。

最新待燒錄候選：加入 `diag-addr`，burn image **43,920 bytes**，host ASan／UBSan／ARM rebuild 通過。本輪尚未操作 EVB；診斷只用 RDID／90／5A，不會補償位址或自動選定 dummy。READ／PP／ERASE 的 addressed path 均未被證明，所有外部 NOR 寫入持續禁止。

2026-10-22 修正版：Stanley 提供 QSPI1 實測 reduced register map，確認 `0x60` indirect register 缺席。已移除 indirect cancel 存取，加入編譯期 MMIO allowlist 與 host 缺席位址 abort 測試；ASan／UBSan 與 ARM rebuild 通過。Stanley 隨後核准燒錄與唯讀 G1，結果見下方最新紀錄；原始 HardFault 與最新 ID 不符兩次失敗分別保留。

OSPI input 修正：已依 SDK 接通 input，加入三個 pad 暫存器的開機 log；host tests／ARM build 通過，burn image **42,560 bytes** 已燒錄。Stanley 表示目前無法量 C19，明確授權不等待量測而重跑唯讀 G1。Input 設定生效仍不能取得有效 ID，因此此修正不足以解決全零 ID。

**GPIO muxscan，43,080 bytes，已燒錄。** 原始 SchDoc／PcbDoc 確認 U11 經 22R 電阻接正常 GPIO 的 QSPI1 functions；先前接線表與 dedicated-OSPI 假設錯誤，已於 §2 更正。本版移除所有 OspiPadConfig 存取，實際掃描在 C 停止，未再嘗試 D–F。Host tests／ARM build 通過；硬體 GPIO／JEDEC 已成功，SFDP 路徑仍待修正，不能宣告整個 G1 PASS。

2026-10-22 執行紀錄：確認唯一 DAPLink probe `2024-2425-6285` 與 VCOM `/dev/cu.usbmodem2024_2425_62851` 存在；測試 burn image 42,480 bytes，header／CRC／800 KiB guard 通過，SHA-256 與本報告一致。原 0.7.0-dev 映像 SHA-256 `effbd9aad13fb8c134ee9f39f475413e8287f78a7bab4e4f2e3007ef526921ec`，另備份至 `/tmp/et100_0.7.0-dev_before_nor_20261022.bin`。首次 USB HID open 失敗與自動審查的執行順序阻擋，在 Stanley 明確補充「先燒入，再由新韌體執行 G1」並允許 USB 存取後解除。

### 2026-10-22 實際 G1 結果

- `fr3068_flash.py` 將 42,480 bytes 寫入 `0x08000000–0x0800A5EF`（11 sectors），program 5.8 s，逐 byte readback PASS。燒錄工具完成 MCU SYSRESETREQ，後續 PC 位於 app flash。完整 log：`/tmp/et100_nor_flash_20261022.log`。
- VCOM 115200 8N1 只送出一次 `id`，10 s 內沒有回覆，因此停止，未送 `sfdp`、`status` 或 `init`。沒有送 `arm`、erase、program、power、reset-op、cut。log：`/tmp/et100_nor_g1_20261022.log`。
- SWD attach 後暫停讀取 fault 暫存器，再恢復原 RUNNING 狀態；沒有額外 MCU reset。`PC=0x08002B3C`（Default/HardFault handler），`xPSR=0x69000003`，`CFSR=0x00008200`（PRECISERR、BFARVALID），`HFSR=0x40000000`（FORCED），`BFAR=0x50290060`。Stacked PC `0x08003EAE` 對應 `nor_qspi1.c` 的 `controller_abort()`，首次存取 `q->ind_read_ctrl.cancel`；當時尚未執行後面的 QSPI1 reset macro。log：`/tmp/et100_nor_g1_debug_20261022.log`。
- 首次 fault 只證明 controller register 存取失敗；其後 Stanley 的唯讀 SWD scan 確認 QSPI1 沒有 `0x60` indirect register，因而定位根因。不得據此宣稱 NOR 型號不合或接線錯誤。JEDEC ID、SFDP、SR1／SR2、PSRAM pads 共存均未取得實機結果，G1 不通過。
- 已依實測 map 修復並重建；下一步取得修正版燒錄授權，再重跑開機 G1，UART 僅送 `id`、`sfdp`、`status`。Stanley 審查 G1 報告前不得開啟外部 NOR 寫入或切電測試。

## 1. 授權與範圍

### 2026-10-22 GPIO Muxscan G1（最新）

Stanley 核准 43,080-byte image（SHA-256 `932501192943ddd8173e2ff1293007983aea9e0531f4a12b64e71646d9c2d2ad`）boot muxscan＋唯讀 G1，UART 只允許 id／sfdp／status；不允許 arm／erase／program／power／reset-op／cut。寫入 `0x08000000–0x0800A847`、11 sectors、program 6.0 s，逐 byte readback PASS。先開 VCOM 擷取 boot，再執行燒錄工具。

完整可觀察 boot log：

```text
freqchipET100 NOR 0.8.1-dev UART=115200 BLE=off PSRAM=off
MUXSCAN func=0x8 read=1 id=000000 rc=0
MUXSCAN func=0x8 read=2 id=000000 rc=0
MUXSCAN func=0x8 read=3 id=000000 rc=0
MUXSCAN func=0x9 read=1 id=000000 rc=0
MUXSCAN func=0x9 read=2 id=000000 rc=0
MUXSCAN func=0x9 read=3 id=000000 rc=0
MUXSCAN func=0xA read=1 id=000000 rc=0
MUXSCAN func=0xA read=2 id=000000 rc=0
MUXSCAN func=0xA read=3 id=000000 rc=0
MUXSCAN func=0xB read=1 id=000000 rc=0
MUXSCAN func=0xB read=2 id=000000 rc=0
MUXSCAN func=0xB read=3 id=000000 rc=0
MUXSCAN func=0xC read=1 id=C86018 rc=0
MUXSCAN func=0xC read=2 id=C86018 rc=0
MUXSCAN func=0xC read=3 id=C86018 rc=0
MUXSCAN selected=0xC stable=3
GPIO PortB_H_FuncMux=0xCCCC9900 PortC_L_FuncMux=0x000000CC OOL_FC=0x01
G1 init_rc=-5 writes=locked
READY
```

自動收集器原先要求 init_rc=0，收到 -5 時先結束；核對此為選中穩定預期 ID 後的 SFDP 驗證失敗，非無穩定 ID 或 controller fault，故依既有授權僅補送 id／sfdp／status，未送 init 或重掃。UART 三個命令均 RESULT rc=0；SFDP 的命令 rc=0 只代表 STIG 交易完成，不表示 SFDP 內容有效。

| 項目 | 實際值 |
|---|---|
| selected function | `0xC`，本板 PB12–PB15／PC0–PC1 的實測結果 |
| PortB_H_FuncMux／PortC_L_FuncMux | `0xCCCC9900`／`0x000000CC` |
| ool_read(0xfc) | boot `0x01` |
| UART JEDEC ID | `C8 60 18`，與 boot 三次一致 |
| SR1／SR2 | `00`／`00` |
| boot G1 | `NOR_ESFDP (-5)`，writes=locked，未通過 |
| requested SFDP offset 0 header | `00 FF FF FF FF FF FF FF`，不是有效 SFDP header |
| Firmware geometry | capacity=0、page=0、spi_hz=0、四組 erase_type 全零；這是 init 失敗的空欄，不是晶片實際幾何或 SPI clock |

完整 256-byte raw dump 保留在 UART log；`53 46 44 50` 簽名出現在 requested offset `0xEA`。**僅作離線診斷假設**：將捕捉到的 256 bytes 以 0xEA 為起點循環重排，可見以下候選結構，未以此繞過 firmware 驗證／啟用寫入，也沒有重讀修正後位址。

| 候選（傳輸／位址未驗證） | 候選值 |
|---|---|
| Header | `53 46 44 50 06 01 01 FF`，revision 1.6，兩個 parameter headers |
| BFPT header | `00 06 01 10 30 00 00 FF`，ID FF00、revision 1.6、16 DWORDs、pointer 0x30 |
| BFPT density DWORD2 | `0x07FFFFFF`，候選 16,777,216 bytes（16 MiB） |
| BFPT erase types | `0C 20`＝4 KiB／20；`0F 52`＝32 KiB／52；`10 D8`＝64 KiB／D8；第四組未使用 |
| Page size | source 的 256 bytes 是限定 GD25LQ128E datasheet 設定；本次未以有效位址 BFPT 驗證 |
| Vendor header | `C8 00 01 03 90 00 00 FF`，ID FFC8、revision 1.0、3 DWORDs、pointer 0x90 |

上述一致性提示 SFDP 位址／dummy／controller 交易仍有問題，根因未定位；候選 geometry 不可視為 G1 PASS。此次沒有改 source、重建或再次 reset。

依 Stanley 接受的證據規則，OOL_FC=01 加上穩定有效 JEDEC ID 已構成 bit0 開啟 U11 供電的**間接證據**；不是已量得 1.8 V，也未證明 power-off。C11／C30 未量測，G2／T8 延後。所有結果取得後停止，沒有送外部 NOR 修改命令。

Logs：`/tmp/et100_nor_muxscan_flash_20261022.log`、`/tmp/et100_nor_muxscan_g1_20261022.log`（完整 boot／唯一三筆 UART TX／原始 SFDP dump）、`/tmp/et100_nor_muxscan_sfdp_decode_20261022.log`（僅離線候選解碼）。

### 2026-10-22 Reduced-QSPI1 修正版 G1

授權：燒入 42,464-byte 映像，boot 自動 G1；UART 僅允許 `id`／`sfdp`／`status`，若 ID 不符或 fault 立即停止並擷取 CPU 狀態。映像 SHA-256 `b8fd5704f1a05c1b765bdf4544815aa5277aaffa994c64102e442f75459d5722`。

燒錄 11 sectors，範圍 `0x08000000–0x0800A5DF`，program 5.8 s，逐 byte readback PASS。燒錄前先開啟 VCOM 115200 8N1，因此收到完整可觀察開機輸出（`freqchip` 與 app banner 沒有分行）：

```text
freqchipET100 NOR 0.8.1-dev UART=115200 BLE=off PSRAM=off
G1 init_rc=-4 writes=locked
READY
```

| 項目 | 結果 |
|---|---|
| JEDEC ID | `00 00 00`；從 boot 已讀取並保存在 `port.info.id` 的 RAM 讀回，非重送 RDID |
| 期望 ID | `C8 60 18`；不符，G1 FAIL |
| SFDP header | 未讀取：`discover()` 在 ID 比對失敗時先返回 |
| BFPT capacity／page／erase geometry | 未取得，不能把 host fixture 的 16 MB／256 B／erase types 當成實機結果 |
| SR1／SR2 | 未取得精確值；boot 曾經過 RDSR 的 ready 檢查，但沒有保存該 byte，未送 UART `status` |
| CPU | RUNNING；`PC=0x08003086`（`uart_receive`），`xPSR=0x49000000`，沒有停在 exception handler |
| CFSR／HFSR | `0x00000000`／`0x00000000` |
| BFAR | `0x50118014`；CFSR.BFARVALID=0，因此不是有效 fault address，不應據此判定 UART fault |
| 軟體 gate | `writes_allowed=0`、`identified=0`、`powered=1` |

Boot 回 `NOR_EID` 時即停止，**沒有送出任何 UART command**，包括原本允許的 id／sfdp／status；也沒有發 arm／erase／program／power／reset-op／cut。SWD 僅暫停後讀取 CFSR／HFSR／BFAR／PC 與保存的 ID／gate，隨即恢復原 RUNNING 狀態，未額外 reset。讀取的 RAM symbol address 已用本次 ELF `nm` 核對：port `0x200021B0`、gate `0x200021AC–AE`。

Logs：`/tmp/et100_nor_reduced_flash_20261022.log`、`/tmp/et100_nor_reduced_g1_20261022.log`（boot 與零 TX 紀錄）、`/tmp/et100_nor_reduced_g1_debug_20261022.log`。目前只證明移除缺席 register 後不再出現先前的 bus fault；`00 00 00` 的 routing／供電／clock／STIG 交易原因尚未定位，依指示等待審查，不繼續操作。

### 2026-10-22 Input-fix G1（最新實測）

Stanley 核准不等待 C19 量測，燒入 42,560 bytes（SHA-256 `cb1ca2026fc27c2488d0e7366684a51c46cb1c5fb9e43083743d96618ad9f6e7`），只允許 boot G1 與 UART id／sfdp／status；全零／全 FF ID 時停止並擷取指定暫存器。燒錄範圍 `0x08000000–0x0800A63F`，11 sectors、program 5.9 s、逐 byte readback PASS。

開機 log（VCOM 在燒錄前開啟，全程擷取）：

```text
freqchipET100 NOR 0.8.1-dev UART=115200 BLE=off PSRAM=off
OSPI InputOpenCircuit=0xFFFFFFFF FuncMux=0x17FFF577 PullEN=0x00001006
G1 init_rc=-4 writes=locked
READY
```

開機已回 NOR_EID，故沒有再送任何 UART 命令。SWD 只暫停讀取下表，再恢復 RUNNING，沒有額外 reset、NOR command 或切電。

| 讀取項目 | 實際值 |
|---|---|
| boot 保存的 JEDEC ID，RAM `0x200021B0` | `00 00 00`（與本次 ELF symbol 核對） |
| OSPI_InputOpenCircuit `0xE00501A4` | `0xFFFFFFFF` |
| OSPI_FuncMux `0xE00501AC` | `0x17FFF577` |
| OSPI_PullEN `0xE005019C` | `0x00001006` |
| QSPI1 config `0x50290000` | `0x80F90087` |
| QSPI1 cmd_ctrl `0x50290090` | `0x9FA20000`（RDID） |
| QSPI1 cmd_address `0x50290094` | `0x00000000` |
| QSPI1 read_data_L／H `0x502900A0／A4` | `0x00000000`／`0x00000000` |
| CPU PC／xPSR | `0x08003086`（uart_receive）／`0x49000000` |
| CFSR／HFSR | `0x00000000`／`0x00000000` |
| BFAR | `0x50118014`，BFARVALID=0，不是有效 fault address |
| writes_allowed／identified／powered | `0`／`0`／`1`（powered 只是軟體狀態） |

SFDP header、BFPT geometry、可靠的 SR1／SR2 數值均未取得：boot 在 ID 比對失敗處返回，沒有繼續 SFDP，也未送 status。ID 仍為全零的原因未定位，不繼續試改設定或送命令。

供電證據規則（Stanley 本次明示）：有效 JEDEC ID 可算作 `ool_write(0xfc)` bit0 開啟 U11 供電的**間接證據**，不能寫成電錶已量到 1.8 V。本次 ID 無效，所以此間接證據尚未成立；C19 仍未量測，G2／T8 延後。下一次需要 build 時再加 `ool_read(0xfc)` boot log，本次不改 source 或額外重建。

Logs：`/tmp/et100_nor_inputs_flash_20261022.log`、`/tmp/et100_nor_inputs_g1_20261022.log`、`/tmp/et100_nor_inputs_g1_debug_20261022.log`。已依指示停止並等待審查。

Stanley 本輪同意：專案內 `nor_qspi1.c` 以 SDK 暫存器結構／macro 實作有界 STIG、controller init／abort，不使用 SDK 的無限等待版本；IRQ disabled 也必須有效，host tests 覆蓋 timeout。衍生函式註明 SDK 來源。不修改 SDK、linker、FAL、FlashEnv、產品 evb_serial target、硬體 README 或 PCB 資料，不 commit／push。

Stanley 明確禁止修改既有硬體 README；U11 wiring／power／DAC 資訊只收於本報告。`IMPLEMENTATION_PLAN.md` 與硬體 README 的原有未提交修改保留。文件只記錄本次實作／驗證界線；Step 5 的獨立 review 與 Step 7 的最終量測同步仍待執行。

## 2. U11 接線／供電／DAC

以下已依 Stanley 的原始 SchDoc 連線確認，以及 Codex 獨立解析原始 Altium OLE 資料交叉更正：SchDoc FileHeader 的 U1/U11 pin／net label／wire／resistor records，以及 PcbDoc Components6／Nets6／Pads6 streams。PCB 共 190 components、181 nets、704 pad records，pad 數與 Pads6/Header 一致；元件 source unique IDs 與 SchDoc 對應。這是設計檔證據，非實板電氣 continuity／電壓量測。

| U11 訊號／Pin | series resistor（原理圖 22R） | GPIO／U1 Pin |
|---|---|---|
| CLK／6 | R36 | PB12／23 |
| CSN／1 | R64 | PC1／26 |
| IO0／5 | R60 | PB13／24 |
| IO1／2 | R63 | PC0／27 |
| IO2／3 | R62 | PB15／28 |
| IO3／7 | R61 | PB14／25 |

先前「CS→PB13、IO0→PC0、IO1→PC1、IO2→PB14、IO3→PB15」是 Markdown／施工單抄寫錯誤，不能再作 pin assignment 依據。實際使用 PortB_H_FuncMux／PortC_L_FuncMux（每 pin 四 bits），不使用 SYSTEM->OspiPadConfig；lvgl_demo dedicated-OSPI mux 與 FR30xx_ext.FLM 的另一組 SPIMX8_0 接法均不適用此板。

| 項目 | 原始檔證據／目前契約 |
|---|---|
| VCC | U11.8、U1.57、C11.2、C30.2 均為 PCB net 174 `IO1V8_LDO`，名義 1.8 V |
| U11 供電量測點 | C11（225／2.2 uF）或 C30（105／1 uF）電源端對 GND；均尚未量測 |
| C19 更正 | C19.2 接 net 171 `MCU_VCC`（U1.65），C19.1 接 GND；**C19 不是 U11 供電量測點** |
| 電源控制 | `ool_write(0xfc)` bit0，RMW 保留其他 bits；最新 OOL_FC=01、JEDEC C86018 穩定，已符合 Stanley 接受的供電間接證據；未量電壓／未驗證切電 |
| controller | QSPI1；獨立 reset，不 reset OSPI／QSPI0 |
| DAC | `DSP_FLASH_DAC_BASE = 0x28000000`，coherence／SWD readback 待驗證 |

原始 binary 解析證據（含 SHA-256、SchDoc record index、PCB pad record byte offset）：`/tmp/et100_nor_raw_net_audit_20261022.log`。例如 R64.1 是 net43 PSRAM-CS，R64.2 與 U1.26 是 net89 PC1；R60.1 是 net42 PSRAM-IO0，R60.2 與 U1.24 是 net93 PB13。六條訊號均已如此逐端核對。

原理圖的 `XT25Q128` 標註有誤：實測 JEDEC `C8 60 18` 已確認 GigaDevice GD25LQ128 系列，照片既有紀錄為 `GD25LQ128EWIG`；symbol 的「PSRAM」描述也不能作 RAM 證據。此 target 不啟動 PSRAM／BLE／RTOS／FlashDB；GPIO function C 的 JEDEC 讀取成功，但 addressed path／SFDP 仍未驗證。

GPIO 的 QSPI1 function number 已由本板 boot scan 實測為 `0xC`；原廠正式表格尚未提供。Host fake 的 A 只是合成案例，實機結果以 §1 log 為準。

DAC cache／prefetch invalidation 未在 SDK 找到可確認的 QSPI1 流程；因此 read／program verify／erase verify 全用 **STIG `0x03`**，不把 DAC 當作已驗證的 coherence 路徑。T3 仍需獨立 SWD 讀 DAC 與 console 比對；本輪未執行。

### 2.1 QSPI1 Reduced Register Map（2026-10-22）

來源：Stanley 在本對話提供的唯讀 SWD scan，base `0x50290000`；掃描後板子維持 RUNNING。修正依此 map 進行，沒有重新掃描全部 offsets。SDK 的 `qspi_regs_t` 是完整 controller layout，不能據此認定 QSPI1 所有欄位都有實體暫存器。

| 類別 | 32-bit aligned offset（範圍每次 +4） |
|---|---|
| 實測存在 | `00–14`、`24`、`28`、`30–44`、`50–58`、`90`、`94`、`A0–AC`、`B0`、`B4`、`FC` |
| 實測 bus fault | `18`、`1C`、`20`、`2C`、`48`、`4C`、`5C`、`60–8C`、`98`、`9C`、`B8–F8` |
| Module ID | offset `FC`，值 `0x00010400`（Stanley 提供） |

本後端實際使用的 subset：

| SDK 欄位 | offset |
|---|---|
| config／read_conf／write_conf／delay | `00`／`04`／`08`／`0C` |
| size_cfg／remap_address／poll_cfg | `14`／`24`／`38` |
| cmd_ctrl／cmd_address | `90`／`94` |
| read_data_L／read_data_H／write_data_L／write_data_H | `A0`／`A4`／`A8`／`AC` |

每次後端 MMIO 都經 `QSPI1_REG(member)`，以 `offsetof(qspi_regs_t, member)` 做 `_Static_assert` allowlist 檢查。Host 另有獨立的完整實測存在表：讀寫任何缺席 word 均 `abort()`；子程序逐一測試所有缺席 aligned offsets 的讀／寫，含原故障點 `0x60` 與 indirect-write `0x70`。所有正常操作及 timeout recovery 也走相同 host MMIO 檢查。

`controller_abort()` 現在僅 disable QSPI1 → QSPI1 reset → controller_configure，完全移除 indirect read/write cancel。這項修正的 host 證據不代表新的 EVB G1 已通過。

### 2.2 歷史 OSPI Input 修正（已撤除；已燒錄版本 ID 全零）

Stanley 提供的唯讀 SWD 記錄：QSPI1 `cmd_ctrl=0x9FA20000`（RDID）、`read_data_L=0`，`OSPI_InputOpenCircuit @0xE00501A4=0`。這是全零 ID 的根因候選；尚未完成改設定後的實機對照，不能先斷言已解決。

SDK `driver_psram.c:314` 的 `psram_init()` 在使用 dedicated pads 前設 `OSPI_InputOpenCircuit=0xFFFFFFFF`；`__SYSTEM_OSPI_INPUT_OPEN_CIRCUIT_DISABLE(mask)` 也以 OR 設位元，確認 **bit=1 表示 input 接通**。原參考 `hw_dsp_xip_flash_init()` 只設 mux，沒有做這一步。

歷史 42,560-byte 版的 `configure_pads()` 在 NOR power-on、RDSR／RDID 之前設 OSPI input 與 dedicated mux。此路徑已確認不適用本板，**目前 source 已完全移除 configure_pads 與所有 OspiPadConfig 存取**，改用 §2.3 的 GPIO 設定。下列 OSPI boot log 只屬舊版歷史，不代表新版本仍會寫這些暫存器。

`nor_test_main.c` 在 boot init 返回後、G1 結果行之前，讀回並輸出三個實際暫存器，成功或錯誤返回都會輸出：

```text
OSPI InputOpenCircuit=0x........ FuncMux=0x........ PullEN=0x........
G1 init_rc=... writes=locked
```

上述為 log 格式，最新實際數值見 §1 的 input-fix G1 紀錄。Host fake 使用相同 pad 設定程式；input 全斷時回 RDID `00 00 00`，再次 init 後恢復 fixture ID，並檢查 input 在 power-on 前接通、mux mask 正確、PullEN 保持原值。這只能驗證軟體設定；實機 InputOpenCircuit 已為 FFFFFFFF，ID 仍全零，C19 尚未量測。依 G1 失敗停止規則等待審查。

### 2.3 GPIO RDID-only Boot Muxscan（已燒錄，選中 C）

Stanley 授權改用 `gpio_init`，在 PB12–PB15、PC0–PC1 設 `GPIO_MODE_AF_PP`＋`GPIO_PULLUP`，依序試 function `0x8` 到 `0xF`；不寫 dedicated OSPI pad、不啟用 PSRAM。初始先將六 pin 設為 GPIO input＋pull-up，再開啟 NOR 供電。

每個候選設定 GPIO mux 後重設／初始化 reduced QSPI1，只送 `RDID 0x9F` 三次，逐次輸出 func／sample／ID／rc。只有三次完全一致且不是 `000000`／`FFFFFF` 才停止掃描並保留該 function；未知但穩定的 ID 也先停止掃描，再由正常 G1 ID 白名單拒絕，不能繼續尋找另一個「符合預期」的結果。若全部無匹配，或 controller／STIG timeout，六 pin 恢復 GPIO input＋pull-up 並返回錯誤；不做無限重試。

掃描階段不讀 status／SFDP，不送 WREN／erase／program／status write；選中穩定 ID 後才進入正常 G1 的 RDSR／RDID／SFDP。最終 UART 允許項仍只有 id／sfdp／status，不新增 muxscan console 命令。

Boot init 返回後印出真實 `PortB_H_FuncMux`／`PortC_L_FuncMux` 及 `ool_read(0xfc)`，格式：

```text
MUXSCAN func=0x8 read=1 id=...... rc=...
...
MUXSCAN selected=0x... stable=3
GPIO PortB_H_FuncMux=0x........ PortC_L_FuncMux=0x........ OOL_FC=0x..
G1 init_rc=... writes=locked
```

這是格式範例，實測完整 log 與 selected C 見 §1。Host fake 以 SDK GPIO 型別驗證六 pin 的 mask／AF_PP／pull-up、PB8–PB11（含 SWD）與 PC2–PC7 mux 未被改動；獨立測試全零／全 FF、不穩定、首個穩定未知 ID、最後 function F、無匹配 input restore、controller timeout 後 init、scan 前綴只有 RDID。Fake 的 function A 是合成測試資料，不是原廠 function number。

### 2.4 diag-addr 唯讀位址診斷（待燒錄）

Stanley 指出原 dump 是乾淨的 byte rotation：true SFDP[0] 在 requested EA，true BFPT[30] 在 requested 1A，density[34] 在 requested 1E，均符合 requested address +16h。這比 capture timing 更像固定位址偏移；RDID 沒有 address phase，不能據此保證 READ／PP／ERASE 定址正確。此處只記錄待診斷現象，不將離線重排變成讀取修正。

新增 console `diag-addr`（無參數），可以在 boot NOR_ESFDP 之後執行，不需 init 或 arm。入口撤銷 write gate，再讀 RDID，僅 C86018 才繼續；任一 transport error／timeout 即回錯誤，不繼續。固定序列共 24 筆 NOR 指令：

| 順序 | 指令／位址／長度／dummy | 輸出目的 |
|---|---|---|
| 1 | 9F、3 bytes、無 address／dummy | 確認目前還是 C86018 |
| 2–3 | 90，000000／000001，各 2 bytes，dummy=0 | 原廠 MID／DID，期待 C817／17C8，實際 raw 不符也照印後繼續其他 probes |
| 4–7 | 5A，000000／000001／000016／000030，各 1 byte，dummy=8 | 對照單 byte 位址偏移 |
| 8–24 | 5A，000000、8 bytes，dummy 0–16 逐一 | 觀察哪個 dummy 把 53464450 放在 byte 0；不自動套用 |

每筆印 `DIAG op/addr/dummy/len/data/rc`。每筆 SFDP 完成後，在下一筆 NOR 指令前立即讀取 QSPI1 `cmd_ctrl@90`、`cmd_address@94`、`size_cfg@14`，以 snapshot 印 raw words，並印 addr_en、addr_bytes_raw／decoded、dummy_raw、read_bytes_raw／decoded、size_addr_raw／decoded。這三個 offsets 皆在 reduced-QSPI1 allowlist。

SDK `driver_qspi.h` 編碼核對：

| 欄位 | Header 定義與目前送出方式 |
|---|---|
| STIG addr_bytes | bits 17:16，enum QSPI_STIG_ADDR_BYTES_3=2；SET(bytes) 也填 bytes-1，本版 3 bytes 填 2 |
| STIG dummy_cycles | bits 11:7，SET(cycles) 直接填值，不減一；診斷逐一送 0–16 |
| STIG read_bytes | bits 22:20，length-1；1 byte 填 0、8 bytes 填 7 |
| size_cfg.addr_bytes | 低四 bits，DEVICE_ADDR_BYTES_3=2；目前填 2 |
| addr_en 巨集矛盾 | ADDRESS_ENABLE 巨集清 0、DISABLE 巨集設 1，與名稱相反；本地 command 直接用 enable_cmd_addr=1，不呼叫這兩個巨集。本輪不改此行為，由實機 raw／解碼進一步查證 |

Host golden-word 核對（忽略 execute/progress bits）：SFDP 1 byte、dummy8 是 `0x5A8A0400`；8 bytes 是 `0x5AFA0000 | (dummy << 7)`。這驗證 C struct/macro 的軟體編碼，不能代替 controller 硬體語意驗證。

診斷結束回 rc=0 只代表固定 probes 都完成，並印 `DIAG COMPLETE transport_only=1 addressing_proven=0 writes=locked`；不代表位址已證明，不設定 geometry、不改 read_sfdp 的 dummy=8、不做 address -16h workaround、不發 WREN／erase／program／status write／NOR reset／切電。正常 nor_test_host.py 仍有 G1 必須成功的前置檢查，本輪將以核准後的直接 console 命令擷取，不以該 runner 重送 init。

Host 測試用正常位址、固定 +16h 位址偏移、有效 dummy 改為 12 三種模型核對所有 24 筆 op/address/length/dummy/data；確認保護 SR1/SR2 不變、無寫入、write gate 撤銷、未驗證 geometry 不被提升、錯誤 ID／STIG timeout／未供電即停止。這些模型不是對實機根因的預判。

## 3. 實作與安全契約

- [nor_port.h](../../src/nor_port.h)：offset-relative、錯誤碼、geometry 與後端函式介面。呼叫者負責單一 owner 序列化；本版不宣稱 RTOS thread-safe。
- [nor_qspi1.c](../../src/nor_qspi1.c)：只有 QSPI1 的 STIG／controller register 操作，不呼叫 SDK `flash_init`／erase／write／protect。保留 SDK startup 原有內建 Flash preinit／trim，不把「後端不碰 QSPI0」誤寫成整個 ROM／SDK 啟動完全不存取內建 Flash。
- 首次 boot／`init`：GPIO input、供電、GPIO muxscan（controller 設定＋RDID-only），選中穩定 ID 後才做 RDSR、RDID、RDSFDP；不送 NOR WREN／reset／status write／erase／program。未知 ID、SFDP signature／版本／density／erase opcode 不合即停止，不作 fallback 猜料號。
- BFPT density 必須與 JEDEC capacity byte 一致；解析 BFPT 四種 erase types，確認 4K `20`、32K `52`、64K `D8`；3-byte offset、overflow、對齊與 256-byte page boundary 檢查均在寫入前完成。page size 256 取自限定 GD25LQ128E datasheet。
- 只有 `arm U11-SCRATCH` 才打開寫入 gate。SR1 BP／SRP0、SR2 CMP／SRP1／SUS1／SUS2 會阻擋；不清除保護，不修改 QE、non-volatile status、OTP，不提供 chip erase。每次實際寫入前再檢查狀態／WEL。
- `program` 預先讀完整個 request，拒絕任何 0→1；每筆寫後逐 byte 驗證。STIG 硬體資料暫存器最多 **8 bytes**，故一頁拆成最多 32 筆 PP，每筆都有 WREN／WIP wait；跨頁不 wrap。這是保守 bring-up 路徑，不是 DMA／整頁單筆性能。
- `erase` 只接受 SFDP 確認的單一 erase unit，完成後全範圍讀回 `FF`。console 沒有隱藏整顆清空行為；T3、T6、T8 的固定範圍／seed 在程式與下表明列。
- IRQ 關閉只在單次 STIG register 操作／短等待／必要 controller recovery，保存並還原原 PRIMASK；WIP 等待期間 IRQ 恢復。RTOS 版未接入，後續應在 WIP 輪詢間加入 yield。
- controller idle／STIG wait 使用 DWT CYCCNT 的 1 ms deadline，加 100000 次迴圈上限；不依賴 tick／`system_delay_us`。WIP 同樣有 DWT 與有限輪詢 fallback，DWT 停止也不會無限等待。
- controller timeout：disable QSPI1、QSPI1-only reset、重新配置 single-SPI controller；只存取 §2.1 的 allowlist，不碰缺席的 indirect registers。撤銷 identified／write gate、回 `NOR_ETIMEOUT`。WIP timeout 也撤銷授權；全 `FF` status 視為無回應而失敗。再次 init 重新識別，不自動重試破壞性指令。
- Host fake 已證明軟體 timeout／重入路徑；**實體 controller reset 是否恢復、DWT 是否運作及 next init 成功仍待 EVB，不作已實證保證**。controller 硬體持續失效時 next init 有界失敗，不偽造成功。

### Timeout 來源

已讀 [GigaDevice 官方 GD25LQ128E Rev1.3 datasheet](https://download.gigadevice.com/Datasheet/DS-00533-GD25LQ128E-Rev1.3.pdf)，§7 指令／ID、§8 AC characteristics 各溫度等級及 reset recovery。採各等級最大值 ×1.5：

| 操作 | 最大值 | 本版 timeout／delay |
|---|---:|---:|
| Page Program | 4 ms | 6 ms |
| 4 KB erase | 500 ms | 750 ms |
| 32 KB erase | 1.5 s | 2.25 s |
| 64 KB erase | 3 s | 4.5 s |
| Reset recovery | 12 ms | 18 ms |

Power-on wait 10 ms，高於表列 tVSL minimum 2.5 ms；power-off 加 10 ms 停留，但是否真的低於 VPWD 0.5 V、至少維持 300 us，必須量 U11 的 IO1V8_LDO（C11／C30），不能由軟體 delay 證明。原先寫 C19 有誤，見 §2 更正。G2／T8 延後。原廠文件下載／文字暫存：`/tmp/et100_gd25lq128e.pdf`、`/tmp/et100_gd25lq128e.txt`，未加入 repo。

QSPI1 source SPLL 192 MHz、divider 32，名義 **6 MHz**；MCU 為 96 MHz，未啟 AUPLL。這是設定推算，不是示波器量測。

## 4. Build／Host 驗證

```sh
bash fw/tools/build_evb_nor.sh --host-test
bash fw/tools/build_evb_nor.sh
bash fw/tools/test_evb_serial_host.sh
python3 fw/tools/evb/nor_test_host.py --help
python3 fw/tools/check_plan_vs_code.py
```

| 項目 | 結果與證據 |
|---|---|
| NOR host ASan／UBSan | PASS：diag-addr 固定 24 reads、正常／+16h／dummy12 模型、golden words、無 mutation／不提升 geometry／錯誤停止；既有 GPIO scan、缺席 MMIO abort、G1 無寫入、SFDP／range／page／erase／保護、IRQ-mask、timeout、DWT 停止／wrap、再次 init、power gate 回歸。最新 log：`/tmp/et100_nor_diag_addr_host_tests_20261022.log` |
| 既有 serial／scan／GATT／LED／App host regression | PASS；未改原 target／測試。`/tmp/et100_nor_serial_regression.log` |
| ARM GCC 14.3.1 build | PASS，獨立 `build_nor`；SDK 原有 RWX segment linker warning 保留。最新 log：`/tmp/et100_nor_diag_addr_build_20261022.log` |
| FR509x header／CRC／payload／800 KB guard | PASS，build script 獨立解析並比對；沒有更改 post_process.py／header selector |
| Host runner | Python AST／`--help` PASS；**未開實體串口，也未做 fake serial 測試**。`/tmp/et100_nor_runner_help.log` |
| check_plan_vs_code.py | exit 0，但沒有 CHECK anchors，僅 no-op，不是 NOR 規格全面比對 |
| diff whitespace | 本輪檔案檢查通過；全工作區仍有既有硬體 README 第 177 行 EOF 空行警告，依授權不修改 |

最新 diag-addr 產物：`fw/build_nor/output/Project.bin`（35,728 bytes；SHA-256 `dfe6976dd37536f0ed9c82e8f21a4c2271922ea4a20cd00e7d0b2c5298228fbe`）、`Project.hex`、`Project_burn.bin`（43,920 bytes；SHA-256 `cdc67c9f86edbbaab8a5ae20a6ee92ca57feb38c753b575421dabde0c48f6c8e`）；ELF／map／lst 保留於獨立 build 目錄。Header／CRC／payload／800 KiB guard PASS，**尚未燒錄**。板上仍是 §1 的 43,080-byte muxscan 版；既有 GPIO C／JEDEC／SR 成功、SFDP 異常，不因 host PASS 而提升為完整 G1 PASS。

## 5. 實機 Gate／測試步驟（G1 FAIL，其餘待執行）

每次 flash（包括恢復 0.7.0-dev）前都需 Stanley 明確同意。已保留並核對原 0.7.0-dev 映像，未覆蓋 `build_serial`。2026-10-22 已核准燒入並執行 G1；G1 失敗後停止，其餘命令不執行。

| Gate／測試 | 操作與證據要求 | 現況 |
|---|---|---|
| G1／T1 | 43,080-byte boot scan 選 C，三次 C86018；僅補送 id／sfdp／status，取得原始 SFDP 與 SR | 未通過：JEDEC 成功，SFDP header 位移異常，boot NOR_ESFDP |
| T2 | `status` 記錄 SR1=00／SR2=00；沒有改保護位元或開寫入 gate | 唯讀狀態已記錄；擦寫／保護行為未測 |
| T3 | 明確核准擦寫後 `--allow-write --command 'boundary'`；改寫四個 4K sectors：000000、7FF000、800000、FFF000，各不同 32-byte signature；第二輪逐點重讀，SWD DAC 另比對 | 未執行 |
| T4 | `erase-bench OFFSET SIZE 20`，SIZE 分別 4096／32768／65536；輸出 WREN／指令／WIP overhead-inclusive min/avg/max，另列全區驗證耗時。對照 datasheet 時說明量測 overhead；未對齊應失敗 | 未執行 |
| T5 | 先 erase 範圍，再 `program OFFSET LENGTH SEED`／`bench`：整頁 256、跨頁例 offset=253 length=513、部分 page。每 byte readback；log bytes／us／decimal KBps×1000／SPI clock | 未執行 |
| T6 | `--timeout 14400 --allow-write --command 'integrity 3'`；固定 020000–41FFFF 共 4 MB，64K erase、deterministic xorshift pattern seed 83E19A47、三輪 SHA-256 與獨立期望 digest 比對；輸出 program／total us。之後才能推估 1792 KB App 備份時間 | 未執行 |
| T7 | 先準備範圍；`reset-op OFFSET 0/1` 送 8-byte PP／4K erase 後立即 SYSRESETREQ；重啟 init 等 WIP（不先 soft reset NOR）、收 G1 與區域內容，再 arm、erase＋program 證明可用 | 未執行 |
| G2 | 延後；未來另取得切電核准，量真正 U11 電源 C11／C30，驗證 off 接近 0 V、on 約 1.8 V，配合 RDID 消失／恢復；不是量 C19 | 延後，未執行 |
| T8 | G2 實測完成後才帶 `--allow-power --g2-measured --allow-write`；`cut OFFSET KIND PERCENT 20`，KIND=1 sector erase／0 PP，PERCENT=10/50/90，各至少 20 次。先 erase，erase case 另填滿 4K zeros；切電後分別記 FF／zero／other byte counts，重新 erase＋program 驗證 | 未執行 |
| 恢復基線 | 再請 Stanley 核准，用既有 fr3068_flash.py 寫回核對過的 0.7.0-dev，readback，Stanley 確認 App 正常 | 未執行 |

數字參數為 decimal 或 `0x` hex，不使用 `4k` 字串。Host runner `--help` 列所有指令及 gate。init／power／reset／timeout 都會撤銷 write gate；若同次 runner 先 init 再要寫，必須另起一次明確 allow-write 流程。

G2 工具／console 原有的 `C19-MEASURED` 字串是舊量測點假設，不能作 U11 電源量測完成的依據；G2／T8 持續禁止執行，未來啟用前須修正該確認字串／help／量測步驟，本次 source 變更不擴及其流程。

T8 的 PP 每次是 8 bytes；目前 cut 延遲以 datasheet full-page typ 500 us 與 sector typ 70 ms ×10/50/90% 計算，**不保證切在 8-byte PP 進行中**，必須照實記完成／部分／未開始結果。若沒有命中，不能宣稱 page program power-cut coverage；需以實測 timing 決定後續測試調整。off 時 `wait_ready` 立即回 ENOTREADY 是 software power gate，不是實體 WIP timeout 證據；真正卡住的 timeout 目前只有 host fault injection 證據。

T6 pattern／digest 在 MCU 內用 SDK 原有 mbedTLS SHA-256；未加入新第三方套件或自寫 hash。讀回與寫入驗證走 STIG；需要另記無 DAC／DMA 優化的性能限制。

## 6. 斷電處理規則（v0.8.2 的保守設計前提，非本輪量測結論）

1. 任意 reset／斷電／timeout 後，受影響 program／erase unit 的完成狀態未知；即使全 `FF` 也不可信，不以內容判定 commit。
2. 未提交區重新 erase、讀驗後才重寫；write error 不自動重送，避免上層誤以為未修改。
3. 後續 download journal 必須以有效 commit marker、record CRC／sequence 判斷持久化進度；NOR journal 仍只管下載，不取代內建 boot state 權威。
4. 初始化先確認供電／ID／SFDP／WIP／保護；逾時撤銷 gate 並回報，不猜料號、不解除 non-volatile protection。
5. T8 尚未執行，不能聲稱已驗證鄰 sector 不受影響、retention、endurance 或整頁 atomicity。

EVB 只有 16 MB／3-byte；ET-100 的 XM25QH256D 32 MB、SPIMX2_1 single-SPI、3.3 V、4-byte addressing／timing／protection 必須另開後端並重跑同套測試，不能由本報告推論通過。
