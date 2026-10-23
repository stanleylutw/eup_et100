# 08 — ET-100 系統狀態協定（SYS）

| 項目 | 定義 |
|---|---|
| 狀態 | **v0.9.0-dev 第一階段已實作，待獨立 review／Stanley 實機驗收；未凍結為量產介面** |
| 日期 | 2026-10-23 |
| 英文名稱 | ET-100 System Status Protocol |
| 用途 | 開機原因通知、完整周邊盤點、周邊狀態變更 |
| 適用 | App 韌體、RS232 debug、HTML Dash |
| 本次範圍 | App SYS＋HTML Dash；不修改 Installer／boot record／SDK |

本文依 Stanley 確認的方向：**Type + Index,Status**。不傳英文名稱、Boot ID、Revision、Uptime、Age、Flags、Phase 或 Reason；由工具查表解碼。本文取代先前 DRAFT 的多欄位提案，尚未成為已發行介面。實作範圍以 §12 為準，registry 完整不表示所有 provider 已完成。
相關：[硬體覆蓋矩陣](../01_firmware/03_hardware_coverage_matrix.md)、[裝置資訊](05_device_metadata.md)、[Installer 協定](07_installer_status.md)。

## 1. 主動訊息格式

```text
ET100$SYS;<Type>[;<Index>,<Status>...]*<CRC>\r\n
```

| 欄位 | 定義 |
|---|---|
| Type | 十進位訊息類型，見 §2 |
| Index | 固定兩位十進位，01–33，見 §4 |
| Status | 十進位 uint8；先查共用碼，再查該 Index 的狀態表 |
| CRC | 四位大寫 hex，CRC-16/XMODEM，不含 0x |
| 行尾 | CRLF |

- ASCII、區分大小寫，不含空白或自由文字。Type／Status 不補零；Index 固定兩位。
- Type=0–7 是啟動通知，**不附周邊 pair**。隨後另送 Type=10，將啟動原因與周邊盤點分開。
- Type=10 必須包含全部 33 個 pair，按 Index 升冪，每項一次；尚未檢查也要回 UNKNOWN，不得省略。
- Type=11 包含 1–33 個有變化的 pair，按 Index 升冪；未列出的項目維持原狀，不能推定為正常。
- 一行最大 **320 bytes，含 CRC 與 CRLF**。全部 33 項皆使用三位 Status 時仍須容納；不拆成多行，不使用 END／ITEM 字樣。
- 不認得 Type／Index／Status 不得解讀為正常，不得觸發硬體操作。

## 2. Type 定義

| Type | English Name | 定義 |
|---:|---|---|
| 0 | BOOT_UNKNOWN | App 已啟動，但無可靠原因證據 |
| 1 | POWER_ON | 已確認為上電啟動 |
| 2 | OTA_TRIAL | 新版安裝後，本次 App 為 TRIAL |
| 3 | OTA_ROLLBACK | 本次確實由 Installer 還原舊版後啟動 |
| 4 | SOFTWARE_RESET | 已確認為軟體重啟 |
| 5 | WATCHDOG_RESET | 已確認為看門狗重啟 |
| 6 | EXTERNAL_RESET | 已確認為外部 reset |
| 7 | BROWNOUT_RESET | 已確認為欠壓重啟 |
| 10 | PERIPHERAL_STATUS | 全部周邊的完整 RAM 狀態快照 |
| 11 | PERIPHERAL_CHANGED | 只回報狀態改變的周邊 |

每次 App 開機只送一種啟動 Type。可靠的本次 OTA_TRIAL／OTA_ROLLBACK 證據優先於 MCU reset cause，其餘使用可靠 MCU／PMU 原因；無法判定送 0。不能把歷史 last_update=FAILED 當成每次都在 rollback，不能以回到 9600、HTML 斷線或猜測判定重啟原因。本次 rollback 證據與 reset 原因取得方式須在施工單確認；無證據就不送對應 Type。

Type=10 不表示整機 PASS；Type=2／3 也不能取代既有 SMP hash／version／confirmed 判定。

## 3. 所有 Index 共用的 Status

下列碼在每個 Index 意義相同，優先於各周邊表。未列出的 7–239 等值不能任意使用；合法值為本節共用碼及 §4 對應列明列的碼。

| Status | English Name | 定義 |
|---:|---|---|
| 0 | UNKNOWN | 尚未檢查、證據不足，或無法可靠分類 |
| 240 | DISABLED | 已配置，但依合法電源／功能政策停用 |
| 241 | NOT_FITTED | 核准板型明確未裝配／未引出 |
| 242 | UNSUPPORTED | 硬體可能／預期存在，但韌體尚無狀態提供者 |
| 243 | STALE | 已有觀測超過有效期限，不能沿用之前正常狀態 |
| 244 | ERROR | 已確認錯誤，但該 Index 無適用的特定錯誤碼 |

只有核准板型配置能回 NOT_FITTED。無回應不等於沒裝。UNKNOWN／UNSUPPORTED／NOT_FITTED／DISABLED 為不同狀態，不得合併顯示。

## 4. 全部周邊 Index 與個別 Status 表

共 **33 個監測項目**，包含硬體與服務，不是 33 顆晶片。Index／英文名稱固定，EVB 與產品板不能重新編號；名稱只在工具顯示，不在線上傳送。

每列列出該項全部專用碼，並額外支援 §3 共用碼。專用碼可重複數字但不同意義；例如 03,2 是 NOR READY，08,2 是 GNSS ACQUIRING。

| Index | English Name | 監測對象 | 專用 Status 定義 |
|---|---|---|---|
| 01 | SYSTEM | 主系統服務，不是整機健康總分 | 1 STARTING；2 RUNNING；3 SHUTTING_DOWN |
| 02 | SETTINGS | FlashDB／設定儲存 | 1 INITIALIZING；2 READY；3 READ_ERROR；4 INTEGRITY_ERROR |
| 03 | NOR | 外部 NOR | 1 INITIALIZING；2 READY；3 ID_MISMATCH；4 TIMEOUT；5 IO_ERROR |
| 04 | BLE | Stack 與角色 | 1 INITIALIZING；2 IDLE；3 SCANNING；4 ADVERTISING；5 CONNECTED；6 MULTI_ROLE |
| 05 | MODEM | EG800 | 1 POWERING；2 WAIT_AT；3 AT_READY；4 INITIALIZING；5 ONLINE；6 RECOVERING；7 TIMEOUT |
| 06 | SIM | Modem 提供的 SIM 狀態 | 1 CHECKING；2 ABSENT；3 READY；4 PIN_REQUIRED；5 SIM_ERROR |
| 07 | NETWORK | 網路註冊／資料連線 | 1 SEARCHING；2 REGISTERING；3 REGISTERED；4 DATA_CONNECTING；5 DATA_CONNECTED；6 DENIED；7 NO_SERVICE |
| 08 | GNSS | GNSS 資料／定位 | 1 WAIT_DATA；2 ACQUIRING；3 FIX_VALID；4 FIX_LOST；5 DATA_TIMEOUT；6 INVALID_DATA |
| 09 | IMU | SC7U22 識別／樣本 | 1 INITIALIZING；2 READY；3 SAMPLING；4 ID_MISMATCH；5 TIMEOUT；6 DATA_ERROR |
| 10 | NFC | PN7160 控制器／卡片活動 | 1 INITIALIZING；2 WAIT_CARD；3 CARD_PRESENT；4 TIMEOUT；5 TRANSACTING；6 IO_ERROR |
| 11 | IOEXP | AW9523 識別／設定 | 1 INITIALIZING；2 READY；3 ID_MISMATCH；4 TIMEOUT；5 CONFIG_ERROR |
| 12 | CAN | 控制器／匯流排 | 1 INITIALIZING；2 LISTENING；3 RECEIVING；4 BUS_OFF；5 RECOVERING；6 INIT_ERROR |
| 13 | RS232_0 | 第一組產品串口 | 1 INITIALIZING；2 OPEN；3 RECEIVING；4 TRANSMITTING；5 DUPLEX；6 IO_ERROR |
| 14 | RS232_1 | 第二組產品串口 | 1 INITIALIZING；2 OPEN；3 RECEIVING；4 TRANSMITTING；5 DUPLEX；6 IO_ERROR |
| 15 | RS232_2 | 第三組產品串口 | 1 INITIALIZING；2 OPEN；3 RECEIVING；4 TRANSMITTING；5 DUPLEX；6 IO_ERROR |
| 16 | ACC | 點火輸入 | 1 INITIALIZING；2 INACTIVE；3 ACTIVE；4 READ_ERROR |
| 17 | DIGITAL_IN | DI 群組 | 1 INITIALIZING；2 READY；3 READ_ERROR |
| 18 | DIGITAL_OUT | 外部 DO 命令／driver | 1 INITIALIZING；2 COMMANDED_OFF；3 COMMANDED_ON；4 DRIVER_ERROR |
| 19 | ACC_OUT | 共用 ACC_OUT 邏輯輸出 | 1 INITIALIZING；2 COMMANDED_OFF；3 COMMANDED_ON；4 DRIVER_ERROR |
| 20 | ANALOG_IN | 已支援 ADC 通道 | 1 INITIALIZING；2 VALID；3 OUT_OF_RANGE；4 READ_ERROR |
| 21 | BATTERY | 備援電池量測／狀態 | 1 CHECKING；2 NORMAL；3 LOW；4 ABSENT；5 READ_ERROR |
| 22 | BATTERY_TEMP | 選配 NTC | 1 CHECKING；2 NORMAL；3 TOO_COLD；4 TOO_HOT；5 SENSOR_ERROR |
| 23 | ONEWIRE | 外部探頭 ROM／CRC／取樣 | 1 DISCOVERING；2 NO_PROBE；3 READY；4 SAMPLING；5 CRC_ERROR；6 TIMEOUT |
| 24 | LED | LED driver 命令 | 1 INITIALIZING；2 COMMANDED_OFF；3 COMMANDED_ON；4 PATTERN_ACTIVE；5 DRIVER_ERROR |
| 25 | BUZZER | 蜂鳴器 driver 命令 | 1 INITIALIZING；2 COMMANDED_OFF；3 COMMANDED_ON；4 PATTERN_ACTIVE；5 DRIVER_ERROR |
| 26 | CHARGER | 可判讀的充電器狀態 | 1 CHECKING；2 IDLE；3 CHARGING；4 FULL；5 FAULT |
| 27 | POWER | 車電／備援供電來源 | 1 CHECKING；2 EXTERNAL；3 BACKUP；4 TRANSITION；5 LOW_VOLTAGE |
| 28 | TIME | RTC／UTC | 1 UNSYNCED；2 SYNCING；3 VALID；4 CLOCK_ERROR |
| 29 | WATCHDOG | 硬體 Watchdog | 1 ARMING；2 ARMED；3 REFRESHING；4 CONFIG_ERROR |
| 30 | RESET | 本次 reset 原因，與啟動通知不同 | 1 POWER_ON；2 SOFTWARE；3 WATCHDOG；4 EXTERNAL；5 BROWNOUT；6 OTHER |
| 31 | LTE_RF | LTE RF／天線診斷 | 1 CHECKING；2 DIAG_PASSED；3 DIAG_FAILED |
| 32 | BLE_RF | BLE RF switch／天線診斷 | 1 CHECKING；2 DIAG_PASSED；3 DIAG_FAILED |
| 33 | NFC_RF | NFC 天線診斷 | 1 CHECKING；2 DIAG_PASSED；3 DIAG_FAILED |

### 4.1 判讀限制與優先順序

- 周邊存在的證據與功能就緒不同：MODEM AT_READY 不表示 SIM READY／NETWORK DATA_CONNECTED；GNSS ACQUIRING 不等於硬體故障。
- NFC WAIT_CARD、CAN LISTENING、RS232 OPEN 都是合法狀態；不能因沒有卡／frame／RX 判定故障或沒接設備。
- 通道群組只報整組檢查結果，不報 DI bitmap、探頭數、電壓、容量。另訂數值介面前不把這些資料塞進 Status。
- DIGITAL_OUT／ACC_OUT／LED／BUZZER 的 COMMANDED 狀態只表示命令與 driver，不代表物理負載真的動作；自檢不得切換輸出。
- 群組有任何已確認錯誤優先回錯誤碼；未初始化優先 INITIALIZING。正常時 BLE 多角色回 MULTI_ROLE，SERIAL 雙向活動回 DUPLEX。活動期間的時間窗由 provider 登記，避免每個 byte 都產生狀態事件。
- 電池 ABSENT、LOW、NTC 閾值、ADC OUT_OF_RANGE 等必須有核准量測／電路證據；不能猜測。沒有可靠區分方法回 UNKNOWN。
- RF DIAG_PASSED 僅表示核准的內建診斷通過，不等於 RF／天線性能驗收。沒有診斷能力回 UNSUPPORTED，不因晶片回應就判定正常。
- 外部 NOR 16 MB EVB／32 MB 產品配置不同；READY 只表示本板初始化通過，不代表全容量、內容、壽命均經掃描。

## 5. 開機與狀態更新流程

1. 依板型建立完整 33 項。所有尚無證據的項目為 UNKNOWN；確定未裝配為 NOT_FITTED，無 provider 為 UNSUPPORTED。
2. UART 可用後送一次啟動 Type=0–7。SYS 只由 App 發送，不要求修改 Installer。
3. owner 的既有初始化／背景監測更新 RAM。基本收集期為狀態服務啟動後 5 秒；不等待 LTE 註冊或 GNSS 定位才回報。
4. 取得一致的全表 RAM 快照，單行發送 Type=10。仍在初始化、等待、異常的項目保留真實狀態，不為了完成盤點改成 READY。
5. 後續 Status 有變化送 Type=11，合併變動；沒有變化不持續洗 log。Host 亦可查完整 Type=10。

單一 Status 無法同時表達所有錯誤細節；provider 必須有固定優先順序，不在此介面加入自由文字。每個 provider 的施工單需登記：owner、板型適用性、證據來源、操作 timeout、更新週期、有效期限與錯誤優先順序。缺登記者不能回報虛構正常值。

不傳時間或開機識別，因此工具無法由漏接訊息判斷重啟或精確資料年齡。韌體過期改為 STALE；Host 斷線清除舊快照，重連重新查詢。未收到啟動通知不代表沒有重啟。

## 6. 唯讀完整查詢

v0.9.0-dev 新增既有命令家族的查詢指令；沿用目前 serial parser，含回覆最大長度與 token 檢查。

```text
Host → BLEGW$GSYSSTAT;<token>\r\n
成功 → AT$PD=BLEGW$GSYSSTAT;1;<token>;10;<Index>,<Status>;...*<CRC>\r\n
失敗 → AT$PD=BLEGW$GSYSSTAT;0;<token>;<error>*<CRC>\r\n
```

- token 固定四個 [A-Z0-9]，Host 精確配對；失敗 error：1 BAD_REQUEST、2 NOT_READY、3 BUSY、4 INTERNAL_ERROR。
- 成功旗標後的 token 是查詢關聯，不是新增周邊欄位；Type=10 與主動回報共用完全相同的 33 項 payload。
- 查詢單次複製 RAM 全表，不發 AT、不讀 NOR、不掃 I2C。UNKNOWN／UNSUPPORTED／NOT_FITTED 是成功查到的結果，不是指令失敗。
- 查詢回覆與主動行使用不同前綴，避免把未經 token 配對的主動回報當成查詢答案。缺合法 token 的壞 request 不回覆。
- 連線先註冊接收，再查完整狀態。收到啟動 Type 時清除舊快照／未完成請求。查詢期間收到 Type=11，於目前請求結束後重新查詢，避免快照與事件先後順序不明。
- 舊韌體仍按原 unknown-command 回覆。Host 將其視為不支援 SYS，不解析為新格式。
- request 含 CRLF 最長 32 bytes；所有成功／失敗回覆含 CRLF 最長 320 bytes。

## 7. CRC／接收規則

CRC-16/XMODEM：poly=0x1021、init=0、非反射、xorout=0；標準 check value「123456789」=31C3。計算範圍為行首第一字元至星號前，不含星號／CRC／CRLF；CRC 不是認證。

接收端先檢查長度／CRC，再驗 Type、欄位、Index 唯一性及 Status。Type=10 缺項／重複／順序錯誤整行拒絕，不能把缺項當正常。Type=11 重複 Index 亦拒絕。未知 Status 可保留 raw code 並顯示 UNKNOWN_CODE，不能當 OK。

超長行丟棄到下一個換行才恢復；不得截斷後解析。CRC 錯或格式錯保留上一份資料但標示接收異常，並重新查詢；無法完成查詢時不能持續顯示新鮮正常狀態。

## 8. 完整封包範例

以下為**合成測試資料，不是 EVB／產品板實測結果**。全部 33 項為了展示不同狀態而指定；不能拿此範例當板型配置。每行最後另加 CRLF，CRC 已計算。

### 8.1 確認上電啟動

```text
ET100$SYS;1*3094
```

### 8.2 全部周邊狀態（Type=10）

```text
ET100$SYS;10;01,2;02,2;03,2;04,3;05,3;06,3;07,2;08,2;09,242;10,242;11,242;12,242;13,2;14,241;15,241;16,242;17,242;18,242;19,242;20,242;21,242;22,241;23,242;24,2;25,241;26,242;27,242;28,1;29,2;30,1;31,242;32,242;33,242*D562
```

部分解碼：03,2=NOR READY；04,3=BLE SCANNING；05,3=MODEM AT_READY；07,2=NETWORK REGISTERING；08,2=GNSS ACQUIRING；10,242=NFC UNSUPPORTED；22,241=BATTERY_TEMP NOT_FITTED。

### 8.3 網路資料連線建立（Type=11）

```text
ET100$SYS;11;07,5*439C
```

只更新 NETWORK 為 DATA_CONNECTED，其他項目不變。不代表雲端應用已登入。

### 8.4 查詢成功／失敗

```text
AT$PD=BLEGW$GSYSSTAT;1;A001;10;01,2;02,2;03,2;04,3;05,3;06,3;07,2;08,2;09,242;10,242;11,242;12,242;13,2;14,241;15,241;16,242;17,242;18,242;19,242;20,242;21,242;22,241;23,242;24,2;25,241;26,242;27,242;28,1;29,2;30,1;31,242;32,242;33,242*EC46
AT$PD=BLEGW$GSYSSTAT;0;A002;2*6161
```

A001 為完整快照；A002 的 error=2 表示狀態服務尚未就緒。

## 9. 安全與資源限制

- 檢查依核准板型與 driver，不盲掃所有介面、不重複初始化；無限等待不可接受。
- 自檢不擦寫 Flash／NOR、不切 DO／ACC_OUT、不發 CAN 測試 frame、不重置 modem、不啟動充電測試、不故意 reset Watchdog。
- 不改 TRIAL 自我確認條件、120 秒期限、boot record 或 INST v1。SYS 不作 OTA 成功判定，也不增加新的開機 gate。
- 所有輸出由既有 UART TX owner 序列化，完整單行不可交錯；不為 SYS 切換 baud。
- 主動回報最多每秒兩行，合併後每項只保留最新待報狀態；queue 有界。錯過啟動通知可由完整查詢恢復周邊狀態，但不能恢復已漏接的開機原因。
- SMP／relay bulk 期間暫停主動 SYS，不回放舊 queue；結束後送最新 Type=10，再繼續 Type=11。查詢回覆優先，忙碌時可回 BUSY。
- 不含 PIN、金鑰、APN 密碼、IMSI 或完整 ICCID。量產通知開關／權限須另外審核。

## 10. HTML、BLE 與版本相容性

- HTML Dash 依 Index 顯示固定「中文名稱 英文／型號」，例如「行動通訊 EG800Q-EU」；型號是板型設計標示，不是偵測結果。協定 English Name／Index 不變，狀態仍依 Status 表解碼，不用自由文字解析或平台猜測。
- 顯示正常／等待／故障／未裝配／未支援等不同樣式，不能把非零 Status 全當正常或全當錯誤。每個已知 Status 的 UI 類別須在實作前登記。
- 不改共用 Log 高度或 Flash 圖。BLE cmd ID／capability bit 尚未分配，需先與 gateway／App 協調。
- 線上沒有版本欄位。此格式一旦凍結，既有 Type／Index／Status 不得重用或改義；破壞性修改須使用新前綴／新查詢指令，不能悄悄改 SYS。
- 此版 Type=10 固定 33 項；擴充 Index 數量或欄位須新介面，避免沒有 count／version 時舊工具無法區分完整快照。可於既有 Index 的保留碼新增 Status，舊工具顯示 UNKNOWN_CODE。
- SYS 與永久 Installer INST v1 獨立，不修改 INST 格式。

## 11. 驗收與實作前 gate

1. 驗證全部 33 項、所有專用／共用碼、未裝配／未支援／無回應的不同意義。
2. 驗證 Type=10 完整性、Type=11 部分更新、token、CRC、最大行長、分段輸入、超長、未知碼及錯誤恢復。
3. 驗證查詢／事件並行、啟動清除舊資料、斷線重連、SMP 暫停恢復、節流及有界 queue。
4. 實機驗無卡、無網、未定位、CAN 無資料、I2C timeout、停用；不把等待當硬體故障，不讓自檢阻塞開機或 Watchdog。
5. 每個 provider 需完成證據／期限／優先順序登記與板型配置審核；輸出無回授、RF 無診斷時不能宣稱物理功能正常。

Stanley 已授權 Codex 代寫施工單與實作第一階段。產品 provider、量產通知權限及可靠 reset／rollback 原因來源仍為後續 gate；本文件不是硬體全面驗收聲明。

## 12. v0.9.0-dev 實作對照

| Index | 第一階段來源／實際回報 | 有效範圍 |
|---|---|---|
| 01 SYSTEM | serial task 開始執行 → RUNNING | 只證明本 task 已執行，不是所有 task 自檢 |
| 02 SETTINGS | FlashDB init 成功 → READY，失敗 → 共用 ERROR | 本次初始化證據，不執行額外測試寫入 |
| 03 NOR | OTA worker 原有 prepare 成功 → READY，失敗 → IO_ERROR | 本次 prepare；不宣稱全容量掃描或偵測所有後續 I/O 故障 |
| 04 BLE | GAP scan／adv start/end、master/slave connect/disconnect | 依已收到事件維護角色；多角色為 MULTI_ROLE，未細分手機與 Sensor |
| 13 RS232_0 | EVB UART3 開始接收 → OPEN | 邏輯 debug transport，不能當產品 RS232 收發器／線束測試 |
| 29 WATCHDOG | serial task 讀 PMU WDT enable bit → ARMED 或共用 ERROR | 不故意觸發 reset，也不宣稱已驗證所有 WDT clock 路徑 |
| 30 RESET | UNKNOWN | 目前無經核准 reset cause binding |
| 其他 | UNSUPPORTED | 未替產品板推測 NOT_FITTED，也未新增周邊驅動 |

- boot Type：首輪 boot RAM 快照確定為 TRIAL 才送 2，否則送 0。其他 Type 是已保留的協定定義，第一階段不猜測、不以歷史 last_update 觸發 3。
- 無新 task、無動態事件 queue；固定目前值／已報值各 33 bytes。serial task 排程主動行，每 500 ms 至多一行；五秒收集後全表。RX 半行／待處理、非 9600、SMP 30 秒活躍窗口、OTA worker 活躍或 Sensor relay 時暫停，恢復補全表。
- 目前 provider 是本次初始化或 callback 事件證據，在本次開機有效；第一階段沒有替未實作產品 provider 建立定期量測／STALE timer。未來有時效資料時必須由對應 owner 主動設 STALE。
- 查詢回覆僅讀 RAM，不重新探測。HTML 在 Dash 提供重新讀取，僅 FR3068 且版本 >=0.9.0（或已收到合法 SYS）才可查；查詢三秒逾時、CRC 異常或離線時不維持正常綠色。
- host／C→JS byte 格式、mock browser 與 ARM App build 已驗證，詳見 [20 報告](../04_testing/20_system_status.md)。實機、stack high-water 及 UART 並行壓力由 Stanley 驗收，不能以模擬結果代替。
