# 溝通規則

> 本檔自 `~/Firmware/EUP/eup_aiot/comm.md` 擷取三區塊：回覆規則、Work flow、git 規則。
> 原始權威來源以該檔與 `~/Firmware/EUP/eup_rules/` 為準。

## 目錄

   10  回覆規則
   25  輸出模板
   30  Claude Code / Codex 分工規則
   32    角色定義
   39    Claude Code 職責
   67    Codex 職責
   77    Work flow（Step 1~9）
  116    流程分支說明
  134    Release flow
  151    Step 2 — 確認規格 / Bug 確認（Claude Code 負責）
  180    Step 3 — Branch 建議規則
  234    Step 4 — Codex 標準指令格式
  250    Step 5 通過後的自動行為
  271    Step 7 — 同步更新永久文件（Claude Code 負責）
  362    Step 8 — 建議 Codex git commit（Claude Code 提供指令）
  370    Step 9 — 詢問並建議 merge to main（Claude Code 負責)
  467  git 規則
  469    使用者說「git status」時的回覆格式
  539    `#gitlog` 指令
  552    修改前置檢查（適用於所有修改，不限於標準流程）
  590    強制層：git hook
  613    Release tag 命名規則

## 回覆規則

> **權威來源**（workspace 共用；涵蓋本節與下方 §輸出模板）：
> [`~/Firmware/EUP/eup_rules/reply_format/reply_format_rule.md`](../eup_rules/reply_format/reply_format_rule.md)
>
> 本節保留原文作為本 repo 落地說明；**與權威來源不一致時以 eup_rules 為準**。
> 改動請先改 eup_rules，再同步回本節。

1. 第一步：提供使用者英文句子的修正。
2. 第二步：用繁體中文提供主要回覆。
3. 英文修正需短且自然。
4. 中文回覆需清楚，並以行動為主。
5. 技術請求需在相關處提供具體檔案路徑或設定。
6. 所有 Markdown 文件描述應使用中文，但特定技術詞可以保留英文。

## 輸出模板
English correction: `...`

中文主要回覆：...

## Claude Code / Codex 分工規則

### 角色定義

| 工具 | 角色 | 職責 |
|---|---|---|
| Claude Code | 架構師 / Reviewer / MD 維護者 | 讀 MD、分析 code、產生 plan、review diff、更新文件 |
| Codex | 執行工程師 | 依照 plan 修改 firmware code、修正 review issue |

### Claude Code 職責

1. 讀 MD、分析現有 code 結構。
2. 產生 `docs/IMPLEMENTATION_PLAN.md`，作為 Codex 的施工單。
3. 在 Step 7 更新 `00_eup_aiot_platform_development_plan.md`，包含 `Version:`、`Last updated` 與 `Revision History`。
   **不在 Step 2 更新** —— Step 7 寫的是已完成並 review 過的行為,不是打算做的事。
4. Review Codex diff，輸出 `docs/REVIEW_REPORT.md`，分類 Critical / Major / Minor issue。
5. **不直接修改任何會影響 firmware build 產出的檔案**，除非使用者明確指示。

   範圍不只 `src/`，而是所有進入 build 的檔案：

   ```text
   src/ include/          原始碼
   Kconfig                組態預設值（決定出廠行為）
   configs/*.conf         build 組態
   boards/                board 定義與 overlay
   CMakeLists.txt         build 腳本
   prj.conf               專案組態
   ```

   判準是**「改了之後燒出去的韌體行為會不會變」**，不是檔案放在哪個資料夾。
   例如 `Kconfig` 的 `M2_DEFAULT_MIXER_MODE` 只是一個數字，但它決定裝置
   出廠跑 realtime 還是 1S，續航差一倍。

   使用者明確指示時可以直接改，但**必須先說明這條依規則應走施工單**，
   取得同意後才動手，並在 `IMPLEMENTATION_PLAN.md` 補上決策紀錄。
   例外條款不等於可以不告知就使用。

### Codex 職責

1. **動手前先檢視修改內容**：確認 scope 表列的檔案、施工單引用的函式與常數
   確實存在且值正確。有疑問先提問，不要直接改。
2. 依照 `IMPLEMENTATION_PLAN.md` 修改 firmware code。
3. 只修改 plan 指定的檔案，不重構無關架構。
4. 不自行發明 payload byte 定義，一切以 MD spec 為準。
5. 修正 `REVIEW_REPORT.md` 中的 Critical / Major issue。
6. **不更新 plan MD**，除非極小 typo，且須在 diff 中說明。

### Work flow（Step 1~9）

> 使用者說「list work flow and release flow」時，**逐字輸出本節與下方
> Release flow 的兩張表格**，不要改寫、不要加減欄位。

| Step | 誰 | 做什麼 | 產出 |
|---|---|---|---|
| 1 | Claude | 討論需求 / Bug 討論分析 | 共識 |
| 2 | Claude | 確認規格 / Bug 確認 | 前提清單（選用：請 Codex 查證） |
| 3 | Claude | 寫施工單 | `IMPLEMENTATION_PLAN.md` |
| 4 | **Codex** | 檢視修改內容、開分支、改 code | 分支 + 程式碼 |
| 5 | Claude | Review，自己重跑 build | `REVIEW_REPORT.md` |
| 6 | **Codex** | 修 Critical / Major | 修正（有才跑） |
| 7 | Claude | 同步文件 | 文件與行為一致 |
| 8 | **Codex** | commit | 一筆含 code + 文件 |
| 9 | Claude | merge 建議 | merge 或暫緩 |

Step 9 merge 之後回到 Step 1。

有獨立小節的：Step 2、3、4、5、7、8、9。Step 1 為討論，無額外規定；
Step 6 的規則見「Step 5 通過後的自動行為」。

補充說明：

```text
Step 2  選用的查證：僅當前提「Claude Code 無法自己用指令確認」時才請 Codex
        —— 例如 Codex 環境才有的工具、實機行為、build 產物的實際內容。
        能自己 grep / build 確認的就自己確認，不佔用往返。
        小改動可跳過 Step 2，但施工單要標明該前提「未查證」。

Step 3  含 branch 建議，由 Codex 在 Step 4 前建立。

Step 4  使用標準指令格式，見下方「Step 4 — Codex 標準指令格式」。
        範圍或前提有疑問先提問，不要直接動手。

Step 7  plan MD 在此更新，不在 Step 2 —— 這時寫的是已完成並 review 過的
        行為，不是打算做的事。
```

### 流程分支說明

```text
有 Critical / Major issue：
  Step 4 → Step 5（有問題）
         → Step 6（Codex 修正）
         → Step 5（重新 review，通過後）
         → Step 7（Claude Code 更新文件）
         → Step 8（建議 Codex git commit）
         → Step 9（詢問並建議 merge to main）

無 Critical / Major issue：
  Step 4 → Step 5（全部通過）
         → Step 7（Claude Code 更新文件）
         → Step 8（建議 Codex git commit）
         → Step 9（詢問並建議 merge to main）
```

### Release flow

**Work flow 每個任務跑一次；Release flow 是累積若干次 merge 之後、決定出貨
時才跑。** 兩者頻率不同，因此不編成 Step 10、11 —— 那會暗示每個任務結束都要
發一版。

| 指令 | 做什麼 | 產出 |
|---|---|---|
| `#release vX.Y.Z` | 確保 tag、build P1 / E9、收檔 | `release/vX.Y.Z/` |
| `#push to google` | 上傳韌體與文件 | Drive + `RELEASES.txt` |

`#release` 先跑，`#push to google` 在其後。完整流程、前置條件與失敗回復見
「分支與 Release」一節。

Git commit 永遠在 Step 7 之後執行，確保 commit 同時包含 code 修改與文件更新。
**Step 7 的完整性搜尋沒跑過，就不要進 Step 8。**

### Step 2 — 確認規格 / Bug 確認（Claude Code 負責）

寫施工單之前，把它將依賴的事實列出來並確認：

```text
函式、常數、檔案路徑是否存在，目前的值是什麼
相關行為的現況（讀 code，不是憑印象）
Bug 的話：重現條件與根因，不是症狀
```

**能自己用指令確認的就自己確認。** `grep`、`git show`、build 一次，比問一輪快。

#### 選用：請 Codex 查證

僅在前提**無法由 Claude Code 自己用指令確認**時才請 Codex：

```text
Codex 環境才有的工具（例如 rclone、Google Drive）
實機行為、sniffer、PPK2 擷取
build 產物的實際內容
```

觸發條件刻意寫成「有沒有真的跑指令確認過」，而不是「我覺得有沒有把握」——
後者查不出來。

**小改動可跳過 Step 2，但施工單要標明該前提「未查證」**，讓 Codex 在 Step 4
知道要先確認再動手。

### Step 3 — Branch 建議規則

Claude Code 產生 `IMPLEMENTATION_PLAN.md` 時，**必須在文件開頭加入 `## Branch` section**，提供 Codex 在 Step 4 開始前建立並切換到正確 branch。

> 開分支的**時機**見下方「修改前置檢查」；本節只管**命名與版號**。

**Branch 命名格式：**
```
vMAJOR.MINOR.PATCH_short_topic
```

**版本號遞增規則（Claude Code 判斷）：**

判準是**牽動範圍**，不是「有沒有新東西」。移除功能與新增功能適用同一把尺。

| 遞增 | 判準 | 範例 |
|---|---|---|
| patch +1 | 只動單一模組，**不改變任何對外介面**（payload / GATT / NVS 語意 / 協定文件皆不變）。移除功能若符合此條件也算 patch | `v1.5.2_lis2dh_shock_lp_mode`<br>`v2.3.1_k1_ota_only` |
| minor +1 | 跨模組或改變架構層次，**或**改變 payload / GATT / NVS 的對外語意，需要更新協定文件 | `v1.5.0_power_optimisation`<br>`v2.3.0_single_role` |
| major +1 | 破壞相容，**App / Gateway 不同步改版就會壞** | `v2.0.0_new_role_arch` |

判定時的自我檢查：

```text
1. 這次改動要不要更新 02_protocol/ 底下的文件？
   要 -> 至少 minor
2. App / Gateway 不改會不會壞？
   會 -> major
3. 都不是 -> patch
```

**Branch section 範本（放在 IMPLEMENTATION_PLAN.md 最前面）：**

```markdown
    ## Branch

Before starting implementation, create and switch to the new branch:

git checkout -b v1.5.0_power_optimisation

Base branch: v1.4.1_mixer_code_chg
```

**規則：**
1. Branch topic 使用英文小寫與底線，簡短描述本次任務
2. Base branch 填寫目前所在的 working branch（不一定是 main）
3. Codex 在 Step 4 的第一步就執行 branch 建立，之後所有修改在新 branch 上進行

### Step 4 — Codex 標準指令格式

每次交給 Codex 實作時，使用以下標準格式：

```
Please read docs/IMPLEMENTATION_PLAN.md and implement it.
If you think something is not quite right or the scope is unclear,
please ask questions before modifying any files.
Do NOT modify files outside the scope listed in the plan.
```

**這個格式的作用：**
- Codex 有疑問時會先提問，不會直接亂改
- 明確限制修改範圍，避免 Codex 動到無關檔案
- 減少 Step 5 review 發現問題的機率

### Step 5 通過後的自動行為

當 Step 5 review 結果為「無 Critical / Major issue（全部 Pass）」時，Claude Code **必須自動進入 Step 7**，完成後再進入 Step 8，不需要使用者另外要求。

自動建議格式（Step 7 完成後）：
```
Step 7 完成。

**進入 Step 8：請將以下指令貼給 Codex 執行 git commit：**

git add <file1>
git add <file2>
...
git commit -m "<commit message>"

Do NOT merge to main.
After commit, run: git log --oneline -3
```

若有 Critical / Major issue，則進入 Step 6（Codex 修正），修正完成後再回到 Step 5 重新 review，通過後才依序執行 Step 7 → Step 8。

### Step 7 — 同步更新永久文件（Claude Code 負責）

**Step 7 必須在 Step 8 之前完成。** commit 要同時包含程式碼與文件，
因此開始 Step 8 之前，下方的完整性搜尋必須已經執行過。

#### 更新對象

| 文件 | 何時要更新 |
|---|---|
| `00_..._platform_development_plan.md` | 架構、行為、power strategy 有變動 |
| `02_protocol/` 底下的 spec | payload / OTA / UUID 的**語意或行為**有變動 |
| `04_testing/` 底下的報告 | **本次取得了新的量測數字** |
| `README.md`（根目錄與子專案） | 安裝、建置、環境設定有變動 |
| `comm.md` | 開發流程本身有變動 |
| `99_decisions/` | 有設計被否決，理由不會留在 commit 裡 |
| `IMPLEMENTATION_PLAN.md` / `REVIEW_REPORT.md` | 本次任務的施工單與 review |
| Changelog | 見下方 §Changelog 維護規則 |

#### 平台計畫更新強制檢查

**Step 7 完成前，Claude Code 必須在回覆裡顯式回答以下清單**（yes / no + 理由，
不能沉默判斷）：

- [ ] 本次是否新增或改變 Kconfig 選項／預設值？
- [ ] 本次是否改變 firmware 行為（LED / KEY / power strategy / NVS schema /
  BLE adv / gesture / timeout）？
- [ ] 本次是否新增或改變 payload / GATT / UUID / OTA 行為？

**任一 yes** → 必須更新 platform plan（加 Revision 條目 + 更新對應章節），
**否則 Step 7 不算完成**、不得進 Step 8。

**全 no** → 明確填寫理由（例：純 doc-only 引用、tool internal、workspace rule
更新），一併記錄在 Step 7 回覆內。

**判定需要更新但範圍太大**（例：一次多個 branch 累積的 backfill）→ 可標為
**doc-debt**、開獨立 backfill branch 處理；**但不能單純跳過**。跳過已知需更新
即違反 Step 7。

#### 完整性搜尋（**必跑，不可憑記憶判斷**）

在 repository 根目錄執行：

```bash
# 1. 這次改動的關鍵字，找出所有描述到它的文件
grep -rln "<關鍵字>" docs README.md comm.md

# 2. 若有檔案改名或升版，確認沒有殘留引用
grep -rn "<舊檔名>" .

# 3. 計畫與程式碼的一致性
python3 tools/check_plan_vs_code.py
```

第 1 項是關鍵。**受影響的文件要用搜尋找出來，不是用回想列出來。**

> **路徑要逐一寫出，不要放進變數。** zsh 不會對未加引號的變數做分詞，
> `grep -rn "$k" $DIRS` 會把整串當成單一路徑，找不到檔案卻**只回傳空結果、
> 不報錯**。
>
> 每次搜尋都應該預期至少一個已知的命中。**全空就先懷疑指令，不要當成結論。**

#### 敘述性內容必須重讀

`check_plan_vs_code.py` 比對的是數值。**一句話是否仍描述現行行為，腳本永遠
抓不到。**

因此：**凡是本次改動觸及的行為，描述該行為的段落要逐段讀過**，不能只靠
腳本綠燈就宣告 Step 7 完成。

#### 更新規則

```text
內容必須反映實際 firmware 行為，不可超前描述未實作的功能
量測數字一律寫進 04_testing/ 的報告，其他文件引用該報告，不各自保存副本
文件升版依「Plan Markdown 版本管理規則」；純 typo / 排版可不升版但須說明
Codex 不負責更新任何 MD
```

### Step 8 — 建議 Codex git commit（Claude Code 提供指令）

Claude Code 提供明確的 git 指令，包含：
- 要 `git add` 的檔案清單（明確列出，不使用 `git add .`）
- 完整 commit message（英文，說明這次做了什麼）
- 說明不 merge 到 main
- 要求 Codex 執行後顯示 `git log --oneline -3` 確認

### Step 9 — 詢問並建議 merge to main（Claude Code 負責）

Step 8 commit 確認後，Claude Code **必須**主動詢問使用者是否要 merge 到 main，並提供建議。

**Claude Code 判斷流程：**

```text
1. 列出 merge checklist，評估每項目的當前狀態（✅ / ⚠️ / ❌）
2. 根據 checklist 結果給出明確建議：
   - 全部 ✅  → 建議立即 merge
   - 有 ⚠️   → 建議視情況決定，說明風險
   - 有 ❌   → 建議暫緩，說明原因
3. 無論建議為何，最終決定權在使用者
```

**Merge Checklist（標準項目）：**

| 項目 | 說明 |
|---|---|
| Code review pass | 無 Critical / Major issue |
| Build 驗證 | P1 OTA build pass；merge 前補跑 E9 OTA build |
| 文件更新 | plan MD、protocol spec、IMPLEMENTATION_PLAN、REVIEW_REPORT 已更新 |
| Platform plan 已更新 | 依 §Step 7 §平台計畫更新強制檢查判定 —— 需更新則已加 Revision + 更新章節；不需要則明確理由；doc-debt 則已 tracked |
| 硬體測試 | 實機驗證核心功能通過 |
| Minor issue 處理 | 已修正或明確記錄延後原因 |
| NVS / 協定相容性 | NVS key 變動或 payload/GATT/RS232 格式變動不影響已部署裝置 |
| Changelog | 本 branch entry 已加入對應 Changelog |

**建議格式：**

```
    ## Step 9 — Merge 建議

    ### Checklist

| 項目 | 狀態 | 說明 |
|---|---|---|
| Code review pass | ✅ | 無 Critical / Major，C1/M1/M3 已修正 |
| Build 驗證 | ⚠️ | P1 pass；E9 merge 前需補跑 |
| 文件更新 | ✅ | plan v1.1、REVIEW_REPORT、IMPLEMENTATION_PLAN 已更新 |
| Platform plan 已更新 | ✅ | 加 Revision v3.3.NN + §8.6 章節更新 |
| 硬體測試 | ❌ | 尚未進行實機驗證 |
| Minor issue 處理 | ⚠️ | M2/M4 延後，已記錄於 REVIEW_REPORT |
| NVS / 協定相容性 | ✅ | 全新 branch，無已部署裝置 |

    ### 建議

<根據 checklist 結果，給出一句明確建議，例如：>

⚠️ 建議**暫緩 merge**，待硬體測試通過後再 merge。
原因：硬體測試尚未進行，核心行為未經實機驗證。

或：

✅ 建議**立即 merge**。所有 checklist 通過，風險可控。

---

如要 merge，請讓 Codex 執行：

git checkout main
git merge --no-ff v0.2.0_rs232_protocol_v1.3 -m "merge: v0.2.0_rs232_protocol_v1.3 into main"
git push origin main
```

**注意事項：**
- `--no-ff` 保留 branch 歷史，不可省略
- merge commit message 格式：`merge: <branch-name> into main`

#### 清空任務文件（Step 8 之後、merge 之前）

`docs/IMPLEMENTATION_PLAN.md` 與 `docs/REVIEW_REPORT.md` 是**每個任務的工作
文件**，每次覆寫。若讓 main 的 HEAD 一直扛著上一個任務的施工單，clone 的人
會以為有進行中或待執行的工作。

**時機很重要 —— 是 branch 上的最後一個 commit：**

```text
Step 8   commit 任務內容（plan / review 完整）      ← 內容進 git 歷史
         再一個 commit：兩份 reset 成佔位符          ← branch 最後一步
Step 9   merge --no-ff                             ← main 的 HEAD 是佔位符
```

**不可在 Step 8 之前清** —— 那樣完整內容從未進 commit，只剩 commit message，
日後找不回舊任務的施工單。**也不在 merge 之後清** —— 那樣 main 會先髒一個
commit，還要多一個 branch。

完整內容透過 `--no-ff` 進了 main 歷史，隨時取回：

```bash
git show <reset 前的 commit>:docs/IMPLEMENTATION_PLAN.md
```

**清空是零損失**（歷史完整），換來 main 永遠乾淨。這是常規，不是一次性。

## git 規則

### 使用者說「git status」時的回覆格式

> **權威來源**（workspace 共用）：
> [`~/Firmware/EUP/eup_rules/git_status/git_status_rule.md`](../eup_rules/git_status/git_status_rule.md)
>
> 本節保留原文作為本 repo 落地說明；**與權威來源不一致時以 eup_rules 為準**。
> 改動請先改 eup_rules，再同步回本節。

**不要貼 `git status` 的原始輸出。** 使用者要看的是 repo 層級的決策摘要：main 是否
同步、release tag 是否已在 origin、哪些 branch 還沒回到 main、哪些只存在本機。這些不是
`git status` 原始輸出回答得好的問題 —— 它只講當前分支的工作目錄。

執行（**預設 summary mode**）：

```bash
sh tools/branch_status.sh
```

**回覆時一律「原樣」貼工具的原始輸出，不要改寫、縮寫、摺疊或轉述成散文。**
canonical 格式就是 `branch_status.sh` 的輸出；任何 paraphrase 都會讓各 session
各自表述、標準形同虛設。需要補充解讀時，放在原始輸出「之後」，不要動輸出本身。

輸出為 `SYNC / BRANCHES / LOCAL ONLY / RISK` 四塊，格式範例：

```text
SYNC
  branch    v3.2.3_gitstatus_v2
  origin    v3.2.1 (fef3e1e)      ✓ synced   ↑0 ↓0
  company   v3.2.0 (b42a039)
  worktree  clean
  tag       v3.1.0  247efcd      ✓ pushed
  tag       v3.2.0  b42a039      ✓ pushed
  tag       v3.2.1  fef3e1e      ✓ pushed

BRANCHES
  last merged into main:
    v0.2.9_drop_doc_version_suffix  efc4784  2026-09-15 20:32
  not merged into main:
  v3.2.2_release_script_preflight  5298146  ↑3  ✓ pushed
  v3.2.3_gitstatus_v2              2e76315  ↑1  ! local only
  merged into main:       32 branches   (work all in main)

LOCAL ONLY  (label only on this laptop)
  merged — safe to prune:   none
  at risk (unmerged + unpushed): v3.2.3_gitstatus_v2

RISK
  ! would be lost: branches: v3.2.3_gitstatus_v2
```

| 區塊 / 欄位 | 意義 |
|---|---|
| `SYNC` | 目前 branch、**各 remote 的 `main` 版本（tag + commit）**、origin ahead/behind、worktree 是否乾淨、local tag 是否已在 origin。origin 那列帶 sync note，其他 remote（如 company）只顯示版本 |
| `↑N ↓M` | local main 相對 `origin/main` ahead / behind 數量 |
| `BRANCHES last merged into main` | 最近 merge 進 main 的分支，顯示 branch name、merge commit short SHA 與 date/time，讓 summary 直接看出最近進 main 的工作 |
| `BRANCHES not merged into main` | 尚未回到 main 的分支，這些才需要判斷是否 merge / push / 保留 |
| `BRANCHES merged into main` | 已合併分支只列 count，work 已在 main，逐條列只是雜訊 |
| `LOCAL ONLY merged — safe to prune` | 遠端沒有這條 branch label，但 commits 已在 main；安全的 stale label，可手動 prune |
| `LOCAL ONLY at risk (unmerged + unpushed)` | 遠端沒有這條 branch 且 work 尚未 merge 進 main；這才是「硬碟壞掉就沒了」的真正風險。**current branch 也會列在這** |
| `LOCAL ONLY tags` | 只存在本機的 tag；release tag 若未 push 也在這裡標出 |
| `RISK` | 一行結論：是否有 unmerged+unpushed branch 或 local-only tag |

重點：**local-only 不等於危險。** 已 merge 進 main 的 local-only branch 只是本機舊 label；
只有「未 merge 且未 push」的 branch，或未 push 的 release tag，才是真正會遺失的狀態。

若使用者要的是當前工作目錄的變更（哪些檔案被改了），那才用 `git status --short`，
兩者不同問題不要混用。

若使用者要看 commit graph，不要把 log 混進 summary；改用 `#gitlog`。

### `#gitlog` 指令

`#gitlog` 顯示 main 最近歷史 graph，對應：

```bash
sh tools/branch_status.sh --log        # 預設最近 10 筆
sh tools/branch_status.sh --log 5      # 指定筆數
```

只有 `--log` 這個輸出才含 TortoiseGit 風格 history graph（`*` `|` `\` `/` 連接線）、ref
badge、commit subject、author 與 relative date。一般 `git status` 回覆仍保持 summary
mode，不要把決策摘要變成 log viewer。

### 修改前置檢查（適用於所有修改，不限於標準流程）

**任何 tracked 檔案的第一次修改之前，必須先執行：**

```bash
git status --short --branch
```

依輸出判斷落在哪一種情況：

| HEAD 所在 | 動作 |
|---|---|
| `main` | **必須先建議開分支**，等使用者決定後才動檔案 |
| 已結案的分支 | **必須先建議開分支** |
| 本次任務自己的分支、工作進行中 | 直接改，不需再問 |

**分支結案的定義**（沒有定義的規則會被含糊掉）：

```text
分支結案 = REVIEW_REPORT 通過（無 Critical / Major）且 Step 8 commit 已完成
```

建議必須一次給齊，少一項就會多問一輪：

```text
分支名（依版號規則推導）、base、scope、以及為什麼這個改動不該留在 main
```

**決定權在使用者。** 使用者說「就在 main 上改」，陳述一次風險後照做，
不重複勸說。

### 強制層：git hook

comm.md 的規則是提醒，靠人記得；hook 是強制，記不記得都擋。

```bash
git config core.hooksPath .githooks
```

`.githooks/pre-commit` 在 HEAD 為 `main` 時拒絕 commit。它**放行 merge
commit**（`git merge --no-ff` 是 main 接收工作的正當管道）、放行 detached
HEAD（rebase / bisect），並保留逃生門：

```bash
ALLOW_COMMIT_ON_MAIN=1 git commit -m "..."
```

`core.hooksPath` 是本機設定、不隨 repo 散布（git 的安全設計），**新 clone
必須自己跑一次上面那行**。

誠實的限制：`--no-verify` 可跳過、部分 GUI client 不執行 hook。這是防健忘的
護欄，不是安全機制。**逃生門被用的頻率是一個訊號** —— 若經常需要用，代表
規則太嚴，或那次判斷值得寫下來。

### Release tag 命名規則

**格式 `vMAJOR.MINOR.PATCH`，與 branch 相同的三段式。**

```bash
git tag -a v3.0.0 -m "Release v3.0.0: <一句話說明這一版對外是什麼>"
```

一律使用 annotated tag（`-a`），訊息要寫得讓半年後的人看得懂這一版的相容性
邊界。lightweight tag 不留訊息，等於只是一個別名。

**tag 的遞增判準與 branch 相同，但衡量的對象不同 —— tag 看的是對外相容性。**

```text
branch   這次改動牽動了多少模組
tag      裝好舊版 App / Gateway 的人升級後會不會壞
```

因此**同一份改動的 branch 版號與 release tag 可以不同**。payload v1 就是這
個情況：實作當下沒有裝置在外，破壞相容的代價是零，branch 判 `v2.2.0`
（minor）；但 release tag 必須是 `v3.0.0`，因為任何 v2.7 的 parser 讀新封包
只會得到垃圾。

推送時明確指定這次 release tag：

```bash
git push origin main vX.Y.Z
```
