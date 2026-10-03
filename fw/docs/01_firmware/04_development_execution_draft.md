# ET-100 分階段開發執行草案

日期：2026-10-02。版本：v0.1 DRAFT。對應 [架構](02_firmware_architecture_draft.md)、[硬體矩陣](03_hardware_coverage_matrix.md)。目的：把「全部硬體功能」轉成可批准的小施工單；本次不建 driver／改 SDK／重編譯。

## 1. Markdown 審查結論

盤點 repo 專案 Markdown，對 firmware 相關需求、SCH 分析、net/pin inventory、datasheet 開發參考、現有 plan/review/ADR/patch 紀錄做交叉搜尋與重點閱讀。第三方 SDK 的泛用 library README 不作 ET-100 board 規格；原廠 PDF 逐頁文字摘錄是先前整理的來源，不在本次重新解析 PDF。這不是全部既有文件已修正的宣告。

| 文件群 | 作為什麼依據 | 本次發現與設計處理 |
|---|---|---|
| [客戶需求](../../../docs/00_project/02_客戶需求.md)、[系統設計](../../../docs/00_project/03_技術方案_系統設計.md)、[線束與軟體](../../../docs/00_project/06_技術方案_線束與軟體.md) | Product functions／logical interfaces | 三 RS232、ACC+3 DI、DO、四探頭、logging；舊設備數值與 ET-100 requirement 分開 |
| [關鍵零件](../../../docs/00_project/04_技術方案_關鍵零件.md)、[天線與外觀](../../../docs/00_project/05_技術方案_天線與外觀.md) | BOM 候選與 RF/容量目標 | Exact part、variant/firmware 和最終 BOM 尚需確認 |
| [時程](../../../docs/00_project/07_專案時程.md)、[交付責任](../../../docs/00_project/09_補充資料.md)、[風險](../../../docs/00_project/10_風險與談判清單.md) | Dependency／驗收責任 | SOW/T0、樣品、source/boot access、protocol/keys；工期不等於合約保證 |
| [SCH sheets/index](../../../docs/03_hardware/sch_analysis/00_索引.md)、[net inventory](../../../docs/03_hardware/sch_analysis/91_net_inventory.md)、[connectors](../../../docs/03_hardware/sch_analysis/92_connector_pinmap.md) | 接線候選與全硬體 coverage | UART/I2C labels、AW CAN_STB、DO polarity、connector/rail conflicts 列入 B01-B10 |
| [Power tree](../../../docs/03_hardware/sch_analysis/90_power_tree.md)、[SCH risks](../../../docs/03_hardware/sch_analysis/99_risks_vs_proposal_v1.8.md) | Power sequencing／fault tests | Shared rails、battery dropout、未供電IO、motion wake、RF 不交由 driver 猜測 |
| [FR306x reference](../../../docs/03_hardware/fr306x_reference/README.md) | Exact-part capabilities／缺失規格 | 156 MHz、512 KB、pin-change；不等同 memory/register/boot reference manual |
| [Bring-up](../../../docs/00_project/11_project_bring_up_status.md)、[PATCHES](../../vendor/PATCHES.md)、[Review](../../REVIEW_REPORT.md) | Host build evidence | 最新採 Phase 1a-ext；historical metrics／「無 caveats」不免除 EVB/boot/memory hardware gate |
| [ADR 01](../99_decisions/01_toolchain_gcc.md)、[ADR 02](../99_decisions/02_sdk_local_patch_policy.md)、[comm](../../../comm.md) | Toolchain／授權／工作流程 | Scope、branch、patch traceability；設計文件不能取代 SDK 行為修改授權 |
| [平台計畫](../00_project/00_et100_firmware_platform_plan.md)、[原草案](01_firmware_development_plan.md) | 已有概念與待決策 | 保存原文，補充可執行版本；修正不合法 priorities、resource double counting、未定 OTA partitions |

文件仍有歷史 SRAM 256 KiB、CMake/RTT/boot 成功或功耗隨頻率精確減半的陳述。新設計採最新 host baseline，不宣稱已量到整機功耗、已 boot 或已具備 rollback。本次只同步草案入口與補充設計；歷史文件的全域數值同步另列文件維護工作。新增文件的相對連結需經本機存在性檢查。

## 2. 里程碑與 Gate

下列 M0-M6 是本草案的工作編號，不改寫既有 Phase 1a/1b 的歷史成果。先 dependency，再安排日期；原草案約 100 工作天不是已批准交期，也不是本次核實的估算。

| Gate | 產出與範圍 | 可無板先做 | 必須通過才能下一步 |
|---|---|---|---|
| M0：Build integration／contracts | 凍結 SDK/patch/build baseline；repo build 與 SDK example parity；logical board manifest schema；host tests入口 | Source/config/linker 對照、test harness 設計 | Verified image／map／metadata；沒有擅自換 boot format／新增 partition |
| M1：EVB foundation | Boot/debug、SDK RTOS/BLE、clock、memory、basic IO／health | Parser vectors、mock state machines | EVB boot/restore procedure；clock、heap/stack、BLE、RAM bank evidence |
| M2：ET-100 safe BSP | Revision profile、AW9523、rail sequencing、NOR、LED/buzzer、DI、safe DO | Profiles/schema 和故障模型 | Board pin/domain/rail safety approved；all storage regions 可讀寫且不 alias；DO 矛盾解除 |
| M3：Device bring-up | LTE/SIM、GNSS、NFC、IMU、三 RS232、CAN、ADC、1-Wire、battery | AT/NMEA/NCI transport mocks、CAN vectors、timeouts | 每個適用 Hxx 的 EVT evidence；不省略缺件/blocked功能 |
| M4：Tracking vertical slice | 固定版記錄、config、EUP cloud ACK、offline/replay、driver sessions、alerts、BLE commands | Schema/ACK/retry/command authorization tests | Sensor→record→persist→LTE→ACK→reboot recover 完整路徑 |
| M5：Dual OTA | Common manager、LTE/BLE transport、驗簽、staging、OEM install adapter、boot recovery | Chunk/resume/journal tests、corrupt image rejection | Approved boot/map/keys；兩路完整更新、掉電/失敗/recovery 證據 |
| M6：Integration／release | Sleep/wake、battery mode、stress、fault injection、security、RF/temperature/ATE evidence | Test procedure／release manifest schema | Product acceptance matrix、resource/power/error budgets、未解 P0=0 |

EVB 不含所有 ET-100 周邊；外接 module 的測試只是該 module setup 的證據。M3 可在 interface breakout bench 平行推進，不保證所有外設都能直接接 EVB。

M4 可早做最小 GNSS/LTE/storage slice；不需等所有 driver 完成。M5 transport／host models 可與 M3-M4 平行，但沒有 boot gate 不能交付「field-safe OTA」。Low-power state design 從 M2 開始，實測驗收在 M6，不拖到最後才查 shared rails。

## 3. 第一個建議施工單：M0

此處只提出工作，尚未執行。下一次批准前先形成獨立 `IMPLEMENTATION_PLAN.md`，列出確切檔案、branch、不可改項目及 deliverable。

1. 對照 vendor Makefile 與 repo CMake 的 sources、defines、includes、ABI、archive order、startup、syscalls/heap、RAM copy 與 packaging。
2. 特別查 repo CMake 指向 `ldscript_3068e.ld`，而成功 example 用 `ldscript.ld`；前者目前沒有同樣的 `.ram_code_front` section/symbol 配對，不能默認等價。
3. 確認 current `fw/src/main.c` placeholder 與完整 RTOS/BLE bootstrap 的移植路線；先 baseline parity，後功能裁剪。
4. 留存工具／SDK來源與 patch manifest、完整 log、ELF/map/raw/burn hashes、byte size；新的 target 名稱與 build flags 必須真的存在才寫 quickstart。
5. 建立 native host tests 的最小入口，只測 parser/state machine/contracts；不在同一施工單順便寫全部 device drivers。
6. 定義 board-profile 未確認字段與 capability flags，unknown binding 在 build/init fail，不落到 guessed GPIO。

M0 完成後再批准首個 hardware plan；若出現 SDK 行為 patch／超出指定檔案，按 ADR 02 停下請示。不修改 post_process/chip selector 來「順便優化」。

## 4. 測試與驗收策略

| 等級 | 必測內容 | 證據 |
|---|---|---|
| Host unit | NMEA checksum/fragmentation/stale fix；AT/URC/prompt/binary framing；CAN decode；config version/bounds | Deterministic test vectors、expected outputs |
| Host fault models | Queue full、timeouts、duplicate chunks/ACK、offset overflow、metadata torn write、record reboot recovery | Fault point coverage、seed/replay log |
| EVB HIL | Startup、clock、RAM bank、BLE、IRQ/task priorities、watchdog、debug recovery | Board revision、waveforms、heap/stack/latency data |
| EVT per-device | 全 H01-H30 適用功能；connector continuity、voltages、bus faults、battery/DO safety | Approved fixture、measured expected/actual、PASS/FAIL/BLOCKED |
| Integration | LTE download + BLE connection + GNSS + CAN + record writes；SIM remove；bus fault；storage full | Drops/corruption/latency、recovery time、resource high water |
| OTA destructive | 掉電於 erase/write/metadata/install/first boot；invalid signature/product/version；transport switch/reconnect | Old/confirmed image可恢復；故障不得 boot 未驗證 image |
| Product acceptance | Logging interval/30days、power modes、temperature/RF、permissions、production flash/recovery | Approved product limits；非任意 default thresholds |

OTA interruption 與 NOR erase 等 destructive tests 先用測試板／approved scratch region，不能用唯一量產裝置或未備份原廠 boot region。車用供電、負載與電池測試由合適 fixture/硬體人員確認；不以 MCU GPIO 直接接車上高壓。

## 5. 需要 Stanley／供應商確認

| 決策 | 建議起點 | 阻擋什麼 |
|---|---|---|
| Logging contract | 評估 15秒／128 bytes fully serialized + bounded event quota，不是固定承諾 | 30days 與 NOR partition驗收 |
| Cloud/EUP contract | 指定版本、test server、ACK/dedup、authentication、config command set | M4 interoperability |
| BLE App contract | 兩路 OTA 都支援；configuration/OTA權限、UUID／chunk／ACK 一起 freeze | M4/M5 App integration |
| OTA release policy | Server image signing、key owner/provisioning、boot recovery明確 | M5 production safety |
| Hardware baseline | Exact silicon/BOM/SCH/netlist/pinmux、DO/CAN_STB、battery pinout | M1-M3相應硬體操作 |
| Parking/battery behavior | 回報週期、哪些 wake source、IMU供電、low voltage cutoff、續航測量目標 | M6 power/retention acceptance |
| Vehicle data scope | Receive-first、PGN/SPN白名單、負載/tx permission、RS232設備协议 | M3/M4車輛整合 |

已有供應商題號可引用 Q03/Q07/Q08/Q28/Q33-Q36/Q40/Q42-Q45/Q48；細節以 [問題 Markdown](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md) 為準，不把未收到的回答當已解除 blocker。

## 6. 本次交付與下一步

交付：architecture、hardware coverage/conflict register、milestone gates 與 M0 建議範圍。僅 Markdown；沒有 install、code、SDK、build config 或 firmware artifact 變更，沒有 build／flash／實板 PASS。

建議先批准 M0 的 build-integration 施工單與 logging/EUP/App contract，再安排 EVB/EVT bring-up。共用 architecture 在本分支審查後才成為正式 baseline；後續變更按 comm.md 和 ADR 02 留下 scope、tests 與 review evidence。
