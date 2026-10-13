# modem_service 設計 — EG800Q-EU AT engine over UART

**Version**：v0.1 DRAFT
**日期**：2026-10-13
**作者**：Codex（主體內容）+ Claude Code（§8 補充 + 格式化）
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

**建議 MCU 使用 buffer access mode，不建議先用 transparent mode**（transparent mode 無法與 URC 共存）：

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

**普通 TCP 沒有加密**。開發初期可以先用 TCP 驗證連線，但**正式產品應改成以下其中之一**：

- TLS socket（EG800Q-EU 支援 `AT+QSSLCFG` / `AT+QSSLOPEN`）
- Private APN / VPN
- 加上**裝置認證與伺服器憑證驗證**

---

## 11. Supplementary Considerations（Claude Code 補）

### 11.1 Modem UART watchdog

- 設 1 條獨立 watchdog：若 `AT` 連續 **N 次（例 5 次 × 30s）完全無回應**才拉硬體 reset
- 內部軟體 state machine 錯誤**不重啟 modem**
- 這條與 §8.3 退避分離 — 退避處理「網路/socket 類瞬時失效」，watchdog 處理「modem 無反應類硬性失效」

### 11.2 URC parser 必須 async + 混序

- URC 可能插在 command response 中間
- Parser 要能辨別 `+QIURC`、`+QIOPEN`、`+CEREG`、`+CPIN` 等**與正在跑的 command 無關**的訊號
- 不要被 UART 位元組順序打亂
- 實作建議：**雙 queue**（response queue + URC event queue），parser 讀每行後 route 到對應 queue

### 11.3 UART hardware flow control (RTS/CTS)

- EG800Q-EU 支援 RTS/CTS
- 高吞吐情境（OTA download 經 LTE 轉 UART / log batch upload）必須啟用，避免 UART overflow
- **本 PCB 要確認 Sheet 11 BJT level shift 電路是否也走 RTS/CTS** — 不然這條走不通；若 Sheet 11 只有 TX/RX，須設計 2 條新 shift path
- 開發期 debug UART (921600 bps) 可不啟用；production 建議啟用

### 11.4 PSM (Power Save Mode) 設計

- ET100 要省電必用：`AT+CPSMS=1,...` 設 T3324/T3412
- 喚醒時機與 TCP keep-alive 要協調 — PSM 下 socket 可能被 operator 中斷
- 典型 ET100 策略：
  - 車熄火 → MCU 要求 modem PSM
  - 車發動 or event trigger → MCU 喚醒 modem → PSM 退出需秒級
  - 若需要**精確 wake schedule**，用 eDRX

### 11.5 應用層 Keep-alive

**純 TCP 不會告訴你「網路通但 server 不理」**。建議 EUP 協定內建**應用層 keep-alive**：

- 例：30-60s 一次 ping/pong
- Server 無回才 reconnect
- 這條與 §8 重連分離 — 退避處理「連線錯誤」，keep-alive 處理「連線在但 app 掛」

### 11.6 考慮用 Quectel 更高層 AT（strategic choice）

若 EUP 協定走以下任一，可**跳過 TCP 層自己管**：

| EUP cloud 若用 | Quectel 內建 AT | 省掉的工作 |
|---|---|---|
| MQTT | `AT+QMTCFG` / `AT+QMTOPEN` / `AT+QMTPUB` / `AT+QMTSUB` | QoS / retain / LWT / reconnect state |
| HTTPS | `AT+QHTTPGET` / `AT+QHTTPPOST` | HTTP 封包構建 / TLS handshake |
| MQTT-SN / CoAP | 不支援 | — |

- MCU 省 20-30% 開發時間 + Flash 占用
- 代價：綁死 Quectel modem 特定 AT 命令（換 modem 要重寫 driver）
- **決策 depend on EUP 協定考古結果**：若 EUP cloud 本來就是 HTTP/MQTT，用高層 AT；若 EUP 是自訂 binary protocol，必須走 TCP

---

## 12. 開發順序建議

1. **Phase 2b.6 LTE AT engine bring-up**（EVT 到之後）：
   - §2 開機 + §3 SIM 讀取 + §4 註網 → 可印 CSQ、CEREG、IMSI 到 debug UART
2. **Phase 3.4 EUP Protocol Stack 之前**：
   - §5 APN manager + §6 PDP + §7 TCP open → 可建立 raw TCP 連 echo server 驗證
3. **Phase 3.4 EUP Protocol Stack**：
   - 搭 §11.5 應用層 keep-alive + §7.3 partial frame parser
4. **Phase 5 整合測試之前**：
   - §10 TLS + §11.4 PSM + §11.3 RTS/CTS flow control
5. **跨 Phase**：
   - §9 architecture 一開始就建對（single owner task + queue API）—— 不先建，後面重構會痛

---

## 13. Open Items

- [ ] 確認 Sheet 11 LTE UART shift 電路是否支援 RTS/CTS pin（若無，省電 vs flow control 要取捨）
- [ ] 等 EUP 協定考古結果 → 決定 §11.6 走 TCP / MQTT / HTTPS 哪條
- [ ] 查 EG800Q-EU 的 PSM 支援細節（T3324/T3412 可調範圍、PSM 下 modem 真正省多少電）
- [ ] 查 QIACT 150s 的實際場景 → 典型多久？（決定 state machine timer 粒度）
- [ ] 定 APN 內建 table 範圍（提案書 §P04 列五國：TW/VN/MY/TH/ID → 需蒐集五國主要電信商 APN）
