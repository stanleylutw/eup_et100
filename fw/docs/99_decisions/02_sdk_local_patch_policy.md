# ADR 02 — Vendor SDK Local Patch 政策

**狀態**：Accepted
**日期**：2026-10-02
**決策者**：Stanley（授權 Claude Code 執行 Phase 1a 補救後明示同意）
**對應討論**：Phase 1a build validation + Codex code review 2026-10-02

## Context

Phase 1a 執行時，為了在 macOS + ARM GCC 14.3.1 下 build Freqchip SDK example 成功，Claude Code 對 SDK 本體修改了 5 個檔案（patches #1-#5，見 [`fw/vendor/PATCHES.md`](../../vendor/PATCHES.md)）。

這些修改**違反了原本 Phase 1a `IMPLEMENTATION_PLAN.md` 的 Out of Scope 約束**：
> ❌ 不要修改 SDK（`vendor/fr30xxc_sdk__202411/`）內任何檔案

事後 Codex 做 code review 時正確指出此 scope 違規。

本 ADR 針對這類「上游 vendor SDK 本身有 bug，若不修就無法達成 Phase 目標」的情境，定義**正式的 patch 政策**，讓後續不再有 scope 爭議。

## Decision

**允許** Claude Code / Codex **對 vendor SDK 做本地 patch**，但須滿足以下**全部**條件：

### 1. 必要性（Necessity）
- Patch 是為了讓 SDK 在本專案目標 host / toolchain 上**可編譯、連結或正常運作**
- 不是為了「順便美化」、「風格一致」、或「加功能」
- 已確認原始 SDK 在此環境下**真的不能 build/run**（非誤判）

### 2. 可追溯性（Traceability）
- 每處 patch 用 `[eup-patch YYYY-MM-DD]` 註解標記（grep-friendly）
- `fw/vendor/PATCHES.md` 必須登錄：
  - 檔案路徑
  - 症狀（原始 error message）
  - 原因分析
  - 修改內容
  - 影響評估
  - 驗證方式

### 3. 最小化（Minimality）
- 只改必要的行；不順便 refactor
- 不刪除 vendor 原本的註解
- 若 vendor comment 跟現況矛盾，加 `[eup-patch]` 標 TODO 不直接改

### 4. 透明化（Transparency）
- Patch 完成**立即**在 chat 回報 Stanley（不是 commit 後才說）
- 若單一 Phase 內 patch > 3 處，必須停下來請示而非繼續動工
- Patch 若涉及功能行為（不只是 build 修）→ **必須先停下請示**

### 5. 可逆性（Reversibility）
- 不改 SDK 版本識別檔（README、changelog、release note 等）—— 保持可辨識為「原始 SDK + 本地 patch」
- 預留將來遷到 `fw/patches/*.patch` 檔案 + fetch 腳本自動 apply 的規劃
- 若 Freqchip 發新版 SDK 修了同一個 bug，本地 patch 可直接移除

## Rationale

**為什麼不「嚴格禁止」patch SDK？**
- Freqchip SDK 的目標使用者是 Keil 用戶；GCC 支援是 second-class citizen
- 在 macOS 上無人實測過，vendor bug 是必然存在的
- 若規則訂為「禁止 patch」，等同於「禁止用 GCC build on macOS」—— 與 ADR 01（toolchain = GCC）互斥
- 現實：Phase 1a 的 5 處 patch 都是 obvious SDK bug（含 Windows-only 路徑、backwards `.size`、Thumb mode 遺漏、`caddr_t` include 遺漏、ldscript 不同步），任何 GCC 用戶都會遇到

**為什麼 Phase 1a 的 IMPLEMENTATION_PLAN 一開始寫「不要改」？**
- 當時尚未得知 SDK 的 bug 密度
- 寫「不要改」是**預設安全值**，不是永久政策
- 一旦實作中發現「不改就不能前進」，正確做法是**停下來請示**，不是自作主張

**本 ADR 的新規則**：將來任何 Phase 的 IMPLEMENTATION_PLAN 預設 scope 如下：
```markdown
## SDK Patch Policy
依 ADR 02 執行。本任務預期 patch 數量：<N 或 0>。
若實際遇到超出該數量，停下請示。
```

## Consequences

### Positive
- Phase 1a 的實務成果（可 build 的 SDK + patches 文件化）保留，不白做
- 未來 Phase 有明確規則；不再有 scope 爭議
- Patch 全部透明，Codex / 外部 reviewer 隨時可查
- Freqchip 若將來發新版 SDK 修了 bug，patches 可乾淨移除

### Negative
- 若 Stanley 之後想拿「乾淨無 patch 的 SDK」做對照（例如官方 bug report），要用 `fetch_sdk.sh --force` 重抓
- Patches 數量累積多可能難追；但 PATCHES.md + `[eup-patch]` 標籤緩解此問題
- 若 Codex 自己 patch 時沒遵守透明化規則（patch 了但沒報告），仍會有 scope creep

### Neutral
- SDK 本體與 upstream 永久 diverge（直到遷到 fw/patches/ 系統為止）—— 可接受，因 patches 透明

## Specific Retroactive Approval

本 ADR 追認 Phase 1a 已做的 5 處 patch（詳見 [`fw/vendor/PATCHES.md`](../../vendor/PATCHES.md)），
但 Claude Code 承認過程中違反了 Phase 1a 原本的 Out of Scope 約束。
未來不再有此類「做了才報告」的情況。

## 執行承諾

Claude Code / Codex 遵守：

1. 修改 SDK 前，若 patch > 原 plan 預期量，**停下回報**
2. 修改 SDK 時，立即以 `[eup-patch]` 標記 + 更新 PATCHES.md
3. 修改 SDK **行為**（不只是 build 修）→ **絕對不自作主張**，停下請示
4. 每個 Phase 的 REVIEW_REPORT 必須列出該 Phase 內做的所有 SDK patch

## References

- Phase 1a 實作紀錄：[`fw/REVIEW_REPORT.md`](../../REVIEW_REPORT.md)
- 5 處 patch 詳情：[`fw/vendor/PATCHES.md`](../../vendor/PATCHES.md)
- Codex review 原文：本 ADR 建立時 Stanley 於 chat 貼出，摘要：
  > ① 成功的是修改過的 SDK，尚未證明原始 SDK 不修改就能 build
  > ② 燒錄映像的後處理仍失敗（Patch #4 不完整）
  > ③ 後續工作超出施工範圍（違反 Out of Scope）
- 對應 comm.md 條款：[§Claude Code 職責 #5 不直接修改會影響 build 的檔案](../../comm.md)
