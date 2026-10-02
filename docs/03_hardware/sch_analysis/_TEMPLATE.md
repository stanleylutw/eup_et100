# Sheet N — 功能塊名稱

**PDF Page**：N / 15
**Ref Designator Prefix**：NNxx（例：05xx = Sheet 5）
**圖面標題（SCH 綠色框文字）**：中文原文
**對應提案書章節**：連結到 `../../00_project/0X_...md` 的對應段落（如適用）

---

## 1. 功能定位

這塊在系統中做什麼、上游是誰、下游是誰。1-3 句話。

## 2. 關鍵元件

| Ref | 型號 | 封裝 | 功能 | DNP? | 備註 |
|---|---|---|---|:-:|---|
| U0XX1 | ... | ... | ... |  | datasheet 連結或頁次 |
| ... | | | | | |

**被動元件要點**：只列有設計意義的（Rref、FB divider、filter LC、decoupling 以外的關鍵電容），不逐一列 bypass cap。

## 3. 電源輸入

| Rail | 來源 Sheet | 電流上限（圖註） | 保護 | 消費 IC |
|---|:-:|:-:|---|---|
| ... | Sheet N | ... mA | TVS/fuse/none | ... |

對照 [`90_power_tree.md`](90_power_tree.md)。

## 4. 介面（I/O Nets）

跨 sheet net 以 `[→ Sheet N]` / `[← Sheet N]` 標示方向。

| Net | 方向 | 源 / 目的 | 用途 |
|---|:-:|---|---|
| MCU_RS232_TXD1 | in（本 sheet 的角度） | ← Sheet 1 (U0101) | ... |
| RS232_TXD1 | out | → Sheet 9 (J0901 pin 11) | 對外 RS232 TX |
| ... | | | |

## 5. 設計細節

- **保護**：TVS 型號、clamp 電壓、ESD 等級
- **走線要求**：diff pair、50Ω trace、closed-to-ANT 等圖面標註
- **特殊 layout 註記**：圖面上的中文手寫說明（例：「Pin76 與 Pin2 之間走線需盡量遠離晶振」）
- **時脈**：crystal 型號、負載電容、頻率精度

## 6. 疑點 / Review Notes

- [ ] **DNP (NM) 元件**：列出，推測原因（EMC tuning 預留？替代料？去掉的功能？）
- [ ] **未接腳 / RESERVED**：IC 哪些 pin 懸空、哪些 pull-up/pull-down
- [ ] **datasheet 對照**（階段 B）：recommended value vs. 實際放的值
- [ ] **與提案書 V1.8 的差異**：是否有新增、刪除、替換
- [ ] **ERC 類**（階段 C）：open input、conflicting drive、power sequencing

## 7. 參考

- SCH PDF：Sheet N
- 相關 sheet：Sheet A（原因）、Sheet B（原因）
- 提案書章節：如適用
- Datasheet 連結：如階段 B 已找到
