#!/usr/bin/env bash
# release.sh — 建立 release tag、build、產出 release 檔
#
# 用法：tools/release.sh vX.Y.Z
#
# 前置檢查（任一失敗即中止）：
#   1. 版本參數格式為 vMAJOR.MINOR.PATCH
#   2. 必須在 main，且不是 detached HEAD
#   3. working tree 乾淨
#   4. tag 不存在，或已存在且指向 HEAD；指向別處則中止
#   5. main 落後 origin/main 時中止；超前時警告但繼續
#   6. 環境 preflight：arm-none-eabi-gcc、cmake、ninja 任一不可用即中止
#
# TODO Phase 0：僅建立骨架；完整實作需等 EVT build 跑通後再補
#   - P1 OTA build / E9 OTA build 細節
#   - Changelog 自動 bump
#   - SHA256SUMS 產生
#   - rclone drive upload

set -euo pipefail

VERSION="${1:-}"

if [[ -z "${VERSION}" ]]; then
    echo "Usage: $0 vX.Y.Z"
    exit 1
fi

if [[ ! "${VERSION}" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "❌ 版本格式必須為 vMAJOR.MINOR.PATCH"
    exit 1
fi

echo "⚠️  release.sh is a placeholder (Phase 0)."
echo "   實際 release flow 要等韌體 bring-up 完成後實作。"
echo "   參考：comm.md §Release flow / §#release 指令"
exit 1
