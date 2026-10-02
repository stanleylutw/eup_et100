# FR306x RF 完整參數

來源：[PDF](../QT0013201514_FR306x技术规格书_v0.4.9.pdf) p.19-25，表 3-1／3-2。返回 [索引](README.md)。跨頁合併儲存格已展開；原圖可在 [逐頁附錄](07_full_text_by_page.md) 核對。

## 1. 表格解讀規則

- Min／Typ／Max 是原表欄位；`—` 是未列。大多數 RF 結果只列 Typ，不應升級為全溫保証。
- 「最大輸出功率設定」是量測項目名稱；數字放在 Typ 欄，不等於有 Max 欄保證。
- `+1/-1` 等順序依來源，前者為正頻率 offset、後者為負 offset，不排序或取平均。
- C/I 的單位是 dB，輸出功率／接收靈敏度通常為 dBm；p.23 的 S2 靈敏度原表單位寫 dB，明列為來源疑點。
- 表沒有完整指定供電、溫度、封裝、晶片 revision、RF matching 與量測配置；不能把 p.5 功耗條件自動套到所有 RF 表格。

## 2. RF 範圍（表 3-1，p.19）

| 項目 | Min | Typ | Max | 單位／條件 |
|---|---|---|---|---|
| 工作頻率 | 2400 | — | 2483.5 | MHz；步進 1 MHz |
| 資料符號速率 | — | — | — | 列舉 BR 1 Mbps、EDR 2 Mbps、EDR 3 Mbps、LE 1 Mbps、LE 2 Mbps、LE 125 kbps、LE 500 kbps |

頻段範圍不等於實際 channel center 清單；來源未列 regulatory band edge 條件。

## 3. BR 1 Mbps 接收（p.19）

| 描述 | 參數 | Min | Typ | Max | 單位 | 條件 |
|---|---|---|---|---|---|---|
| 靈敏度 | BR 1 Mbps | — | -93 | — | dBm | 原表條件 `-` |
| Maximum received signal | 0.1% BER | — | 0 | — | dBm | 0.1% BER |
| Co-channel C/I | 同信道 | — | 8 | — | dB | 未另列 |
| Adjacent C/I | +1/-1 MHz | — | -5/-5 | — | dB | wanted signal -60 dBm |
| Adjacent C/I | +2 MHz | — | -38 | — | dB | wanted signal -60 dBm |
| Adjacent C/I | +3/-3 MHz | — | -41/-40 | — | dB | wanted signal -67 dBm |
| Image rejection | -2 MHz | — | -34 | — | dB | wanted signal -67 dBm；Image=-2 MHz |

## 4. BR 發射（p.19-20）

| 描述 | 參數 | Min | Typ | Max | 單位 | 條件 |
|---|---|---|---|---|---|---|
| Output power, maximum setting | 原表名稱 | -20 | 0 | 10 | dBm | 未另列 |
| Transmitter power control step | — | 2 | — | 8 | dB | 跨頁續列 |
| Output power control range | — | — | 30 | — | dB | 未另列 |
| Initial carrier frequency offset | — | — | ±5 | — | kHz | 未另列 |
| Frequency drift | — | — | ±10 | — | kHz | 未另列 |
| Frequency deviation | Δf1avg | — | 160 | — | kHz | 來源條件 140-175 kHz |
| Frequency deviation | Δf2max | 115 | — | — | kHz | ≥115 kHz |
| Frequency deviation ratio | Δf2avg/Δf1avg | 0.8 | — | — | 無因次 | ≥0.8 |
| ACP | IM-NI=2 | — | -50 | — | dBm | 保留來源 channel index 寫法 |
| ACP | IM-NI=3 | — | -54 | — | dBm | 保留來源 channel index 寫法 |

`IM-NI` 的符號定義未在本文展開；不自行轉換成特定 offset 與 receiver 的 adjacent C/I 混用。

## 5. EDR2／EDR3 接收（p.20-21）

| 模式 | 描述 | 參數 | Min | Typ | Max | 單位 | 條件 |
|---|---|---|---|---|---|---|---|
| EDR2 | 靈敏度 | 2 Mbps | — | -94 | — | dBm | 未另列 |
| EDR3 | 靈敏度 | 3 Mbps | — | -85 | — | dBm | 未另列 |
| EDR 共用列 | Maximum received signal | 0.1% BER | — | 0 | — | dBm | 0.1% BER |
| EDR2 | Co-channel C/I | — | — | 15 | — | dB | 未另列 |
| EDR2 | Adjacent C/I | +1/-1 MHz | — | -7/-8 | — | dB | wanted -60 dBm |
| EDR2 | Adjacent C/I | +2 MHz | — | -30 | — | dB | wanted -60 dBm |
| EDR2 | Adjacent C/I | +3/-3 MHz | — | -40/-40 | — | dB | wanted -67 dBm |
| EDR2 | Image rejection | -2 MHz | — | -22 | — | dB | wanted -67 dBm；Image=-2 MHz |
| EDR3 | Co-channel C/I | — | — | 18 | — | dB | 未另列 |
| EDR3 | Adjacent C/I | +1/-1 MHz | — | 0/-1 | — | dB | wanted -60 dBm |
| EDR3 | Adjacent C/I | +2 MHz | — | -25 | — | dB | wanted -60 dBm |
| EDR3 | Adjacent C/I | +3/-3 MHz | — | -33/-33 | — | dB | wanted -67 dBm |
| EDR3 | Image rejection | -2 MHz | — | -17 | — | dB | wanted -67 dBm；Image=-2 MHz |

## 6. EDR 發射（p.21-22）

| 描述 | 模式／參數 | Min | Typ | Max | 單位 | 原表條件 |
|---|---|---|---|---|---|---|
| Output power, maximum setting | EDR 共用列 | — | 7 | — | dBm | 未另列 |
| Transmitter power control step | EDR 共用列 | 2 | — | 8 | dB | 未另列 |
| Output power control range | EDR 共用列 | — | 27 | — | dB | 未另列 |
| Initial carrier frequency offset | EDR 共用列 | — | ±5 | — | kHz | 未另列 |
| Frequency drift | EDR2，2DH5 | — | ±10 | — | kHz | 2DH5 |
| Frequency drift | EDR3，3DH5 | — | ±10 | — | kHz | 3DH5 |
| EDR2 ACP | IM-NI=2 | — | -28 | — | dBm | 未另列 |
| EDR2 ACP | IM-NI=3 | — | -40 | — | dBm | 未另列 |
| EDR3 ACP | IM-NI=2 | — | -28 | — | dBm | 未另列 |
| EDR3 ACP | IM-NI=3 | — | -40 | — | dBm | 未另列 |
| EDR2 modulation accuracy | RMS DEVM | — | 6 | — | % | ≤20% |
| EDR2 modulation accuracy | PEAK DEVM | — | 13 | — | % | ≤30% |
| EDR2 modulation accuracy | 99% DEVM | — | 10 | — | % | ≤35% |
| EDR3 modulation accuracy | RMS DEVM | — | 6 | — | % | ≤13% |
| EDR3 modulation accuracy | PEAK DEVM | — | 14 | — | % | ≤25% |
| EDR3 modulation accuracy | 99% DEVM | — | 10 | — | % | ≤20% |

表內 ≤20% 等位於「條件」欄，不把它們搬進 Max 欄。

## 7. BLE 1 Mbps 接收（p.22）

來源群組標題包含 `±250 kHz 偏離`；條件欄 wanted signal 為 -67 dBm，適用 C/I／image rows。

| 描述 | 參數 | Min | Typ | Max | 單位 |
|---|---|---|---|---|---|
| 靈敏度 | 1 Mbps | — | -97 | — | dBm |
| Co-channel C/I | — | — | 6 | — | dB |
| Adjacent C/I | +1/-1 MHz | — | -6/-5 | — | dB |
| Adjacent C/I | +2 MHz | — | -42 | — | dB |
| Adjacent C/I | ≥3 MHz | — | -46 | — | dB |
| Image rejection | -2 MHz | — | -28 | — | dB |

Image=-2 MHz。原表 `≥3 MHz` 未明寫 `±`，本表不補負方向數字。

## 8. BLE 2 Mbps 接收（p.22-23）

來源群組標題包含 `±500 kHz 偏離`；wanted signal -67 dBm，Image=-4 MHz。

| 描述 | 參數 | Min | Typ | Max | 單位 |
|---|---|---|---|---|---|
| 靈敏度 | 2 Mbps | — | -93 | — | dBm |
| Co-channel C/I | — | — | 7 | — | dB |
| Adjacent C/I | +2/-2 MHz | — | -6/-5 | — | dB |
| Adjacent C/I | +4 MHz | — | -30 | — | dB |
| Adjacent C/I | ≥6 MHz | — | -33 | — | dB |
| Image rejection | -4 MHz | — | -21 | — | dB |

## 9. BLE 500 kbps／S2 接收（p.23）

来源群組標題包含 `±250 kHz 偏離`；wanted signal -72 dBm，Image=-2 MHz。

| 描述 | 參數 | Min | Typ | Max | 原表單位 |
|---|---|---|---|---|---|
| 靈敏度 | 500 kbps | — | -100.5 | — | **dB** |
| Co-channel C/I | — | — | 7 | — | dB |
| Adjacent C/I | +1/-1 MHz | — | -8/-8 | — | dB |
| Adjacent C/I | +2 MHz | — | -43 | — | dB |
| Adjacent C/I | ≥3 MHz | — | -46 | — | dB |
| Image rejection | -2 MHz | — | -40 | — | dB |

靈敏度單位疑似筆誤：p.5／9 同一 LE S2 數值為 -100.5 dBm。開發比較可標為「摘要 -100.5 dBm，p.23 原表 dB」，不要偷偷修改來源表。

## 10. BLE 125 kbps／S8 接收（p.24）

來源群組標題包含 `±250 kHz 偏離`；wanted signal -79 dBm，Image=-2 MHz。

| 描述 | 參數 | Min | Typ | Max | 單位 |
|---|---|---|---|---|---|
| 靈敏度 | 125 kbps | — | -102.5 | — | dBm |
| Co-channel C/I | — | — | 1 | — | dB |
| Adjacent C/I | +1/-1 MHz | — | -9/-10 | — | dB |
| Adjacent C/I | +2 MHz | — | -48 | — | dB |
| Adjacent C/I | ≥3 MHz | — | -55 | — | dB |
| Image rejection | -2 MHz | — | -40 | — | dB |

## 11. BLE 所有速率的發射參數

來源：1 Mbps p.22；2 Mbps p.23；500 kbps p.24；125 kbps p.24-25。以下各數值均在原表 Typ 欄，Min／Max 欄未列數值。

| PHY | Output power maximum setting，Typ | Output power minimum setting，Typ | Programmable range，Typ | Modulation 20 dB bandwidth，Typ |
|---|---|---|---|---|
| BLE 1 Mbps | +10 dBm | -20 dBm | 30 dB | 1.2 MHz |
| BLE 2 Mbps | +10 dBm | -20 dBm | 30 dB | 2.3 MHz |
| BLE 500 kbps／S2 | +10 dBm | -20 dBm | 30 dB | 1.2 MHz |
| BLE 125 kbps／S8 | +10 dBm | -20 dBm | 30 dB | 1.2 MHz |

## 12. 開發與 RF 驗證使用方式

以上是 IC RF 性能，不是 ET-100 板級天線／外殼／線纜的實测。Bring-up 應確認 PHY、實際 TX setting、天線 matching、晶體精度、UART／CAN／LTE 的干擾、RF supply ripple 及 sleep／wake 行為。

BLE coded PHY 是否可用需要 SDK API 與對端共同支援；接收靈敏度不是距離保證。BR／EDR profile 是否進最終 ELF，也要以 linker map 和 runtime configuration 判斷，不能由本 datasheet 的 dual-mode 宣稱直接推出。
