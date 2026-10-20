# modem_service 設計 — EG800Q-EU AT engine over UART

**Version**：v0.2 DRAFT
**日期**：2026-10-13
**作者**：Codex + Claude Code（共同設計與交叉 review）
**狀態**：Design proposal — 未實作、未實機驗證

> **交叉引用**：
> - 架構 owner：[`02_firmware_architecture_draft.md`](02_firmware_architecture_draft.md) §services → `modem_service`
> - 計畫位置：[`01_firmware_development_plan.md`](01_firmware_development_plan.md) §4.2 Communication Service
> - 執行里程碑：[`04_development_execution_draft.md`](04_development_execution_draft.md) M4 modem/positioning/storage slice
> - 硬體依據：[`sch_analysis/02_LTE_EG800Q-EU.md`](../../../docs/03_hardware/sch_analysis/02_LTE_EG800Q-EU.md)、[Q08 BJT→PMOS](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md)、[Q21 PWRKEY driver](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md)

---

## 0. 背景

EG800Q-EU **內建 TCP/IP stack**，FR3068 透過 UART 發送 AT command，就能完成註網、PDP 啟用、TCP 連線及資料收發。

Quectel 官方資料（AT command 細節以此為準）：
- [EG800Q-EU TCP/IP Application Note V1.0](https://quectel.com/content/uploads/2024/02/Quectel_EG800Q-EUEG915Q-NA_TCPIP_Application_Note_V1.0.pdf)
- [EG800Q-EU AT Commands Manual V1.0](https://quectel.com/content/uploads/2024/02/Quectel_EG800Q-EUEG915Q-NA_AT_Commands_Manual_V1.0.pdf)
- [EG800Q Series Hardware Design V1.1](https://quectel.com/content/uploads/2024/04/Quectel_EG800Q_Series_Hardware_Design_V1.1.pdf)
- [EG800Q Series product/document page](https://www.quectel.com/product/lte-eg800q-series/)（新版 AT、TCP/IP、MQTT、HTTP(S)、PSM/eDRX 文件索引）

---

## 1. 建議狀態機

```text
OFF
 -> POWERING
 -> AT_SYNC
 -> SIM_READY
 -> NETWORK_REGISTERED
 -> APN_CONFIGURED
 -> PDP_ACTIVE
 -> TCP_CONNECTING
 -> ONLINE
 -> RECONNECT_BACKOFF
```

**設計原則**：不要在單一函式中一次跑完整串流程。**每個狀態都要有 timeout、retry 與錯誤回復**。

---

## 2. 開機與基本初始化

### 2.1 硬體流程

1. LTE VBAT 穩定
2. 將 PWRKEY 拉低約 2 秒後釋放
3. 等 UART 可回應 `AT`

**ET100 既有疑點**：原理圖中 LTE 電源控制與 PWRKEY 驅動曾被列為疑點（[Q08 BJT→PMOS](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md)、[Q21 PWRKEY driver 架構](../../../docs/03_hardware/sch_analysis/100_questions_for_quectel.md)），**實板前要先確認這兩個問題**。

### 2.2 初始化命令

```text
AT
OK

ATE0
OK

AT+CMEE=2
OK

ATI
AT+CGMR
```

- `ATE0` 關閉 command echo，UART parser 會簡單很多
- `AT+CMEE=2` 讓錯誤回覆包含文字原因，debug 友善

---

## 3. 確認 SIM

```text
AT+CPIN?
+CPIN: READY

AT+QCCID
AT+CIMI
```

- `QCCID`：SIM ICCID，可用來判斷**是否換卡**（換卡觸發 APN 重選）
- `CIMI`：IMSI，前 5 或 6 位是 **MCC/MNC**，可用於 APN 選擇
- 如果 SIM 有 PIN，必須由**安全設定提供**，不應硬編碼在 firmware

---

## 4. 等候 LTE 註網

```text
AT+CEREG?
+CEREG: ...,1
```

或：

```text
+CEREG: ...,5
```

### 狀態意義

| CEREG state | 意義 |
|:-:|---|
| 1 | 已在**本地**網路註冊 |
| 5 | 已**漫遊**註冊 |
| 2 | 正在搜尋 |
| 3 | 被拒絕 |
| 0 / 4 | 尚未註冊或狀態不明 |

**可以用 `AT+CSQ` 協助診斷訊號，但不要只用 CSQ 決定是否已註網**。

---

## 5. Auto APN 設計

**EG800Q-EU 沒有一個能保證全球 SIM 都正確的「全自動 APN」命令**。建議 ET100 使用以下**優先順序**：

1. 後台或產品設定指定的 APN
2. 已成功使用過、且與目前 ICCID 綁定的 APN
3. 依 IMSI 的 MCC/MNC 查**內建 APN table**
4. 最後才嘗試空 APN
5. 全部失敗後**回報 provisioning error，不要無限重開機**

### 5.1 QICSGP 語法

一般無帳密 APN：

```text
AT+QICSGP=1,1,"internet","","",0
OK
```

參數：

```text
AT+QICSGP=<context>,<IP_type>,<APN>,<username>,<password>,<auth>
```

| 參數 | 值 | 意義 |
|---|:-:|---|
| context | 1 | PDP context ID |
| IP_type | 1 | IPv4 |
| IP_type | 2 | IPv6 |
| IP_type | 3 | IPv4v6 |
| auth | 0 | 無驗證 |
| auth | 1 | PAP |
| auth | 2 | CHAP |
| auth | 3 | PAP 或 CHAP |

### 5.2 空 APN fallback

可以測試：

```text
AT+QICSGP=1,1,"","","",0
```

但不能假設每家電信商、MVNO、漫遊卡或 private APN 都能工作。**成功 APN 應按 ICCID 儲存**，換 SIM 時重新選擇。

---

## 6. 啟用 PDP Context

```text
AT+QIACT=1
OK

AT+QIACT?
+QIACT: 1,1,1,"10.x.x.x"
```

**`QIACT` 最長可能需要約 150 秒**，所以不能用一般 2-5 秒 AT timeout。

---

## 7. TCP Client

### 7.1 建立連線

**建議 MCU 使用 buffer access mode，不建議先用 transparent mode**。Transparent mode 會讓控制與資料共用同一 UART data path，AT command、URC dispatch、escape sequence 與錯誤回復都較難可靠管理：

```text
AT+QIOPEN=1,0,"TCP","server.example.com",9000,0,0
OK
```

這個 `OK` **只代表命令被接受**。真正連線結果稍後由 URC 回報：

```text
+QIOPEN: 0,0
```

其中**第二個 `0` 才代表 TCP 連線成功**。

### 7.2 TCP 傳送

例如傳送 12 bytes：

```text
AT+QISEND=0,12
> hello world!
SEND OK
```

**必須在 `>` 出現後送出恰好指定長度的 binary data**。

**重要**：`SEND OK` **只代表 modem 接受或送出資料，不能證明伺服器應用程式已處理**。

ET100 protocol 仍應包含：

```text
message_type
sequence_number
payload_length
payload
CRC 或 authentication tag
```

並由 server 回傳 **application ACK**。

### 7.3 TCP 接收

收到資料時會出現：

```text
+QIURC: "recv",0
```

MCU 再讀：

```text
AT+QIRD=0,1024
+QIRD: 38
<38 bytes raw data>
OK
```

**TCP 是 byte stream，不保留封包邊界**。一次 `QIRD` 可能收到半個 packet，也可能一次收到數個 packet，因此 ET100 protocol 必須有 **length field**，parser 也要**支援 partial frame**。

---

## 8. 斷線與重連

### 8.1 URC 偵測

遠端關閉：

```text
+QIURC: "closed",0
```

PDP 被網路關閉：

```text
+QIURC: "pdpdeact",1
```

### 8.2 回復流程

```text
AT+QICLOSE=0
AT+QIDEACT=1
AT+QICSGP=...
AT+QIACT=1
AT+QIOPEN=...
```

### 8.3 退避策略

**不要每次 TCP 失敗就重開 modem**。建議退避：

```text
1 s -> 2 s -> 5 s -> 10 s -> 30 s -> 60 s -> 5 min
```

**只有以下情況才執行硬體重啟**：
- AT UART 完全沒有回應
- 長時間無法註網
- Modem 狀態無法恢復

---

## 9. 韌體架構

FR3068 端應**只有一個 `modem_service` task** 能直接操作 LTE UART：

```text
drv_lte_uart
    |
at_parser
    |
modem_service
    +-- SIM / registration state machine
    +-- APN manager
    +-- PDP manager
    +-- TCP socket manager
    +-- URC dispatcher
    +-- reconnect/backoff
    |
eup_transport
    |
EUP application protocol
```

**規則**：其他 task 必須透過 **queue** 向 `modem_service` 要求傳送，**不能同時直接向 UART 寫 AT command**，否則 command response、URC 與 TCP binary data 很容易互相打亂。

這條規則與 [`02_firmware_architecture_draft.md`](02_firmware_architecture_draft.md) §services 的 `modem_service` 「唯一 owner」描述一致。

---

## 10. 安全性

**普通 TCP 沒有加密**。開發初期可以先用 TCP 驗證連線，但正式產品必須使用 TLS socket（EG800Q-EU 支援 `AT+QSSLCFG` / `AT+QSSLOPEN`），並完成**裝置認證與伺服器憑證驗證**。Private APN / VPN 可作為額外網路隔離，但不能取代端到端加密與身份驗證。

---

## 11. 可靠性、流量控制與協定選擇

### 11.1 Modem UART watchdog

- Watchdog 必須是 **command-aware**，不能用單一固定 timeout。一般 `AT` 可等待 2-5 秒；`QIACT`、`QIOPEN` 等長命令使用原廠規定的專屬 timeout。
- 長命令仍在合法等待時間內時，不得由 health check 插入另一條 `AT`，也不得誤判 modem 無回應。
- 沒有長命令進行時，若一般 `AT` 連續 3 次無任何完整回應，依序執行：清理 RX/parser 狀態 → UART peripheral recovery → 再次同步 `AT` → 軟體關機/重新上電 → 最後才使用硬體 reset。
- APN 錯誤、註網失敗、PDP 失效、TCP timeout 與 server 無回應均屬 state-machine 錯誤，**不得直接觸發硬體 reset**。
- 所有 recovery 必須記錄原因、當前 state、最後命令、elapsed time 與 cumulative reset count，並限制單位時間內的 power cycle 次數。
- 本節與 §8.3 分工：退避處理網路/socket 的瞬時失效；watchdog 只處理 UART 或 modem 無回應。

### 11.2 Async URC parser 與 binary framing

- UART byte 順序本身不會被打亂；真正需要處理的是 **URC 可能出現在 command transaction 期間的完整行之間**。
- `+QIURC`、`+CEREG`、`+CPIN` 等可能是非同步事件；`+QIOPEN` 是 command 接受後延遲抵達的連線結果。不能一律當成目前 command 的同步 response。
- RX pipeline 建議固定為：`UART DMA/ISR ring buffer -> line/binary framer -> response matcher / URC dispatcher -> event queues`。
- AT line mode 以 CR/LF framing；收到 `QIRD` 的長度欄位後，parser 必須切入 **binary mode**，精確收滿指定 byte 數才回到 line mode。Payload 即使含有 `CR/LF`、`OK` 或 `+QIURC` 字樣，也不得被當成 AT 訊息。
- `modem_service` 維持單一 in-flight command，response matcher 只接受該 command 定義的 response；其餘完整 URC route 到獨立 event queue。
- RX ring buffer overflow、line 過長、binary length 超限與 response timeout 都要成為可觀測 error counter，不得靜默丟棄。

### 11.3 UART hardware flow control (RTS/CTS)

- EG800Q-EU **Main UART** 支援 RTS/CTS，115200 bps 為預設速率，最高支援 921600 bps。
- OTA download 經 LTE 轉 UART、log batch upload 等高吞吐情境應啟用 hardware flow control，並搭配 DMA/ISR ring buffer，避免 MCU 忙碌期間 RX overflow。
- **本 PCB 要確認 Sheet 11 level-shift 電路是否同時包含 MAIN_RTS/MAIN_CTS**；若只有 TX/RX，須新增兩條方向正確的 shift path，或接受較低吞吐與較高 overflow 風險。
- 原廠電晶體式 level shifter 不適合超過 460800 bps；採 921600 bps 前必須確認實際 level shifter、走線與 signal integrity。
- 只有在硬體接線確認後才能設定 `AT+IFC=2,2`。未接 RTS/CTS 卻啟用 flow control 可能造成 UART 停止傳輸。
- Bring-up 先以 115200 bps 驗證功能；OTA throughput test 優先測試 `460800 + RTS/CTS`，通過長時間壓力測試後才評估 921600 bps。

### 11.4 PSM (Power Save Mode) 設計

- PSM/eDRX 是 ET100 的選用省電機制，不是所有運行狀態都強制啟用。`AT+CPSMS=1,...` 的 T3324/T3412 是向 operator 提出的 request，實際核准值可能不同，必須讀回並記錄網路授予結果。
- PSM 期間 server 通常不能即時下行喚醒裝置，TCP/NAT mapping 也可能失效；喚醒後應重新確認 registration、PDP 與 socket，不假設原連線仍有效。
- 建議依車輛狀態切換：

| ET100 狀態 | Modem 策略 |
|---|---|
| 行駛 / 即時追蹤 | 保持 LTE 與 application session online |
| 短暫停車 | eDRX 或延長 heartbeat，依 server 下行延遲需求決定 |
| 長時間停車 | 關閉 application session 後進入 PSM |
| ACC / IMU / alarm event | MCU 喚醒 modem，重新確認 PDP/session 後上報 |

- eDRX 是延長 paging cycle、降低待機耗電的機制，**不是精準 RTC 排程器**。需要精準週期喚醒時，應由 MCU RTC/low-power timer 控制，並確認硬體喚醒路徑。
- PSM/eDRX 必須以實際 SIM、operator、漫遊狀態與 EG800Q firmware 版本做功耗、喚醒時間及 socket 存活測試後才能定版。

### 11.5 應用層 Keep-alive

**純 TCP 不會告訴你「網路通但 server 不理」**。建議 EUP 協定內建**應用層 keep-alive**：

- Ping 至少包含 `device_id`、`sequence`、`uptime`、`timestamp` 與 network state；pong 必須對應 sequence。
- Interval 不寫死：行駛追蹤可從 60-180 秒開始；停車在線可從 5-15 分鐘開始；深度省電模式應主動斷線，不維持 heartbeat。最終值由 server NAT timeout、即時性、流量費用與功耗測試決定。
- 連續遺失 2-3 次 pong 才進入 reconnect，避免單一 packet loss 造成重連風暴。
- TCP keepalive 可作為輔助，但不能取代 application ping/pong；TCP ACK 也不能證明 server application 已處理訊息。
- 本節與 §8 分工：退避處理已知連線錯誤；application heartbeat 偵測「socket 看似存在但 server application 不再回應」。

### 11.6 Quectel 高層 AT 的協定選擇

EG800Q-EU 提供 MQTT、HTTP(S)、FTP(S) 與 SSL/TLS AT command。採高層 AT 可以減少 MCU 端 protocol encoding，但**不能免除** PDP recovery、URC handling、timeout、reconnect/backoff、TLS 憑證與 session restore。

| 使用情境 | 建議介面 | MCU 仍須負責 |
|---|---|---|
| 即時 telemetry / command | MQTT over TLS：`AT+QMTCFG` / `AT+QMTOPEN` / `AT+QMTCONN` / `AT+QMTPUB` / `AT+QMTSUB` | PDP、MQTT URC、QoS result、subscription restore、backoff |
| OTA manifest / image download | HTTPS：`AT+QHTTPGET` / `AT+QHTTPREAD` 或 file API | Range/resume、hash/signature、storage、MCU image transfer |
| EUP 自訂雙向 binary protocol | TLS socket：`AT+QSSLCFG` / `AT+QSSLOPEN` | Framing、ACK、heartbeat、session recovery |
| 既有 FTP infrastructure | FTP(S) AT | Credential、resume、integrity verification |

- MQTT 的 QoS、retain 與 LWT 可交由 modem 協助處理，但 MCU 仍必須監控 disconnect URC 並在重連後恢復 subscription/session。
- HTTP(S) 適合 OTA request/response，不適合取代需要持續 server push 的雙向 telemetry channel。
- 高層 AT 會增加 Quectel vendor lock-in；上層必須透過 `eup_transport` abstraction 隔離 modem-specific command。
- 正式產品不得使用明文 MQTT/HTTP/TCP 傳輸憑證、定位或車況資料；必須啟用 TLS、驗證 server certificate，並定義 certificate rotation。
- 最終選擇取決於 EUP cloud 現有協定；不得在 backend protocol 尚未確認前以推估的開發百分比作決策依據。

---

## 12. 開發順序建議

1. **Phase 2b.6 LTE AT engine bring-up**（EVT 到之後）：
   - §2 開機 + §3 SIM 讀取 + §4 註網 + §11.1 command-aware timeout + §11.2 async parser
   - 可印 CSQ、CEREG、IMSI 與 recovery counters 到 debug UART
2. **Phase 3.4 EUP Protocol Stack 之前**：
   - §5 APN manager + §6 PDP + §7 TCP open → 可建立 raw TCP 連 echo server 驗證
3. **Phase 3.4 EUP Protocol Stack**：
   - 搭 §11.5 應用層 keep-alive + §7.3 partial frame parser
4. **Phase 5 整合測試之前**：
   - §10 TLS + §11.3 RTS/CTS flow control + §11.4 PSM/eDRX operating modes
5. **跨 Phase**：
   - §9 architecture 一開始就建對（single owner task + queue API）—— 不先建，後面重構會痛

---

## 13. Open Items

- [ ] 確認 Sheet 11 LTE UART shift 電路是否支援 RTS/CTS pin、方向及目標 baud rate；若無，決定改板或限制 throughput
- [ ] 等 EUP 協定考古結果 → 決定 §11.6 走 TCP / MQTT / HTTPS 哪條
- [ ] 取得並鎖定 EG800Q-EU firmware 版本，對照最新版 PSM/eDRX、MQTT、HTTP(S)、SSL 與 TCP/IP application notes
- [ ] 查 EG800Q-EU 的 PSM 支援細節（T3324/T3412 範圍、網路核准值、PSM 實測耗電、喚醒 latency 與 socket 存活）
- [ ] 查 QIACT 150s 的實際場景 → 典型多久？（決定 state machine timer 粒度）
- [ ] 定 APN 內建 table 範圍（提案書 §P04 列五國：TW/VN/MY/TH/ID → 需蒐集五國主要電信商 APN）
- [ ] 定義 watchdog recovery budget、reset telemetry 與 factory/field fault threshold
- [ ] 定義 EUP heartbeat interval、miss threshold、server NAT timeout 與各 operating mode 的流量/功耗預算
- [ ] 定義 OTA 的 resume、SHA-256、signature verification、rollback 與 modem-to-MCU image transfer 流程
