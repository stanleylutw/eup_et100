#!/usr/bin/env python3
"""
check_plan_vs_code.py — 比對平台計畫與程式碼的機械性一致性

用途（comm.md §Build 驗證規則 要求）：
    檢查 docs/00_project/*_plan.md 中以 HTML 註解錨點標註的常數值，
    是否與 src/ include/ 底下的 #define / 常數定義一致。

錨點格式（在 MD 內）：
    <!-- CHECK: EUP_PAYLOAD_LEN == 128 -->

本工具會：
    1. glob docs/**/*.md 找所有 CHECK 錨點
    2. grep src/ include/ 找對應 #define 或 const
    3. 不一致則回報並 exit 1

TODO Phase 0：placeholder；完整實作需等 docs/00_project/ 有實際內容
"""

import glob
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DOCS_DIR = PROJECT_ROOT / "docs"
SRC_DIRS = [PROJECT_ROOT / "src", PROJECT_ROOT / "include"]

CHECK_PATTERN = re.compile(r"<!--\s*CHECK:\s*(\w+)\s*==\s*(\S+?)\s*-->")


def find_checks():
    checks = []
    for md in glob.glob(str(DOCS_DIR / "**" / "*.md"), recursive=True):
        with open(md, "r", encoding="utf-8") as f:
            for lineno, line in enumerate(f, 1):
                for match in CHECK_PATTERN.finditer(line):
                    name, expected = match.group(1), match.group(2)
                    checks.append({
                        "md": md,
                        "line": lineno,
                        "name": name,
                        "expected": expected,
                    })
    return checks


def find_define(name):
    """Grep #define <name> <value> in src/include/."""
    define_re = re.compile(rf"#define\s+{re.escape(name)}\s+(\S+)")
    for src_dir in SRC_DIRS:
        if not src_dir.exists():
            continue
        for src in src_dir.rglob("*.[ch]"):
            with open(src, "r", encoding="utf-8", errors="ignore") as f:
                for lineno, line in enumerate(f, 1):
                    m = define_re.search(line)
                    if m:
                        return {"file": str(src), "line": lineno, "value": m.group(1)}
    return None


def main():
    checks = find_checks()
    if not checks:
        print("✓ No CHECK anchors found (nothing to verify yet).")
        return 0

    failures = 0
    for c in checks:
        found = find_define(c["name"])
        if found is None:
            print(f"❌ {c['md']}:{c['line']}  {c['name']} == {c['expected']} (not defined in code)")
            failures += 1
            continue
        if found["value"] != c["expected"]:
            print(f"❌ {c['md']}:{c['line']}  {c['name']} expected {c['expected']}, "
                  f"code has {found['value']} ({found['file']}:{found['line']})")
            failures += 1
        else:
            print(f"✓ {c['name']} == {c['expected']}  ({found['file']}:{found['line']})")

    if failures:
        print(f"\n{failures} check(s) failed")
        return 1
    print(f"\n{len(checks)} check(s) passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
