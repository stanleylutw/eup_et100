#!/usr/bin/env bash
# branch_status.sh — 「#git status」的 canonical output
#
# 用法：
#   sh tools/branch_status.sh            # summary mode（預設）
#   sh tools/branch_status.sh --log      # 最近 10 筆 main history graph
#   sh tools/branch_status.sh --log 5    # 指定筆數
#
# 輸出格式：SYNC / BRANCHES / LOCAL ONLY / RISK 四塊
# 詳見 ../comm.md §使用者說「git status」時的回覆格式

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

# ---------- --log 模式 ----------
if [[ "${1:-}" == "--log" ]]; then
    N="${2:-10}"
    git log --all --graph --oneline --decorate --date-order -n "${N}" \
        --pretty=format:"%h %d %s %an (%ar)"
    echo
    exit 0
fi

# ---------- summary mode ----------
CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo DETACHED)"
WORKTREE_STATUS="clean"
if [[ -n "$(git status --porcelain)" ]]; then
    WORKTREE_STATUS="dirty"
fi

echo "SYNC"
printf "  branch    %s\n" "${CURRENT_BRANCH}"

# 各 remote 的 main
for remote in $(git remote); do
    REMOTE_MAIN=$(git rev-parse --short "${remote}/main" 2>/dev/null || echo "?")
    REMOTE_TAG=$(git describe --tags --exact-match "${remote}/main" 2>/dev/null || echo "")
    printf "  %-8s  %s %s\n" "${remote}" "${REMOTE_TAG}" "(${REMOTE_MAIN})"
done

# origin ahead/behind
if git rev-parse origin/main >/dev/null 2>&1; then
    AHEAD=$(git rev-list --count origin/main..HEAD 2>/dev/null || echo 0)
    BEHIND=$(git rev-list --count HEAD..origin/main 2>/dev/null || echo 0)
    SYNC_NOTE="✓ synced"
    [[ "${AHEAD}" -gt 0 || "${BEHIND}" -gt 0 ]] && SYNC_NOTE="⚠ diverged"
    printf "  origin-sync  %s   ↑%s ↓%s\n" "${SYNC_NOTE}" "${AHEAD}" "${BEHIND}"
fi

printf "  worktree  %s\n" "${WORKTREE_STATUS}"

# 本地 tags
for tag in $(git tag -l 'v*' | sort -V); do
    TAG_SHA=$(git rev-parse --short "${tag}")
    PUSHED="! local only"
    if git ls-remote --tags origin "${tag}" 2>/dev/null | grep -q "${tag}"; then
        PUSHED="✓ pushed"
    fi
    printf "  tag       %-10s  %s  %s\n" "${tag}" "${TAG_SHA}" "${PUSHED}"
done

echo
echo "BRANCHES"

# 最近 merge 進 main 的分支
LAST_MERGED=$(git log main --merges --pretty=format:"%h %s" -n 1 2>/dev/null || echo "")
if [[ -n "${LAST_MERGED}" ]]; then
    MERGE_DATE=$(git log main --merges --pretty=format:"%ad" --date=format:"%Y-%m-%d %H:%M" -n 1)
    MERGE_SUBJECT=$(git log main --merges --pretty=format:"%s" -n 1 | sed 's/^merge: //' | awk '{print $1}')
    echo "  last merged into main:"
    printf "    %s  %s\n" "${MERGE_SUBJECT}" "${MERGE_DATE}"
fi

# 未 merge 進 main 的分支
echo "  not merged into main:"
UNMERGED=$(git branch --no-merged main 2>/dev/null | grep -v '^\*' | sed 's/^[[:space:]]*//' || true)
if [[ -z "${UNMERGED}" ]]; then
    echo "    (none)"
else
    while IFS= read -r b; do
        [[ -z "${b}" ]] && continue
        SHA=$(git rev-parse --short "${b}" 2>/dev/null || echo "?")
        PUSH_STATE="! local only"
        if git ls-remote --heads origin "${b}" 2>/dev/null | grep -q "${b}"; then
            PUSH_STATE="✓ pushed"
            AH=$(git rev-list --count "origin/${b}..${b}" 2>/dev/null || echo 0)
            [[ "${AH}" -gt 0 ]] && PUSH_STATE="↑${AH} needs push"
        fi
        printf "    %-40s  %s  %s\n" "${b}" "${SHA}" "${PUSH_STATE}"
    done <<< "${UNMERGED}"
fi

MERGED_COUNT=$(git branch --merged main 2>/dev/null | grep -v '^\*' | grep -v 'main$' | wc -l | tr -d ' ')
echo "  merged into main:       ${MERGED_COUNT} branches   (work all in main)"

echo
echo "RISK"
UNMERGED_UNPUSHED=""
while IFS= read -r b; do
    [[ -z "${b}" ]] && continue
    if ! git ls-remote --heads origin "${b}" 2>/dev/null | grep -q "${b}"; then
        UNMERGED_UNPUSHED+="${b} "
    fi
done <<< "${UNMERGED}"

if [[ -n "${UNMERGED_UNPUSHED}" ]]; then
    echo "  ! would be lost: branches: ${UNMERGED_UNPUSHED}"
else
    echo "  ✓ nothing at risk"
fi
