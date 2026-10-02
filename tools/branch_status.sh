#!/bin/sh
#
# EUP AIoT firmware branch status helper.
#
# Default mode is a decision summary for the "git status" shortcut.
# Use --log [N] for a compact history graph.

usage() {
	echo "usage: sh tools/branch_status.sh [--log [N]]" >&2
	exit 2
}

cd "$(git rev-parse --show-toplevel)" || exit 1

case "${1-}" in
	"")
		mode=summary
		;;
	--log)
		mode=log
		count=${2:-10}
		case "$count" in
			*[!0-9]*|"") usage ;;
		esac
		[ "$#" -le 2 ] || usage
		;;
	-*)
		usage
		;;
	*)
		usage
		;;
esac

if [ "$mode" = log ]; then
	echo "HISTORY  (main, last $count)"
	echo
	git log main -n "$count" --graph --decorate=short --date=relative \
		--format='%h%x09%d%x09%s%x09%an%x09%ar' |
	awk -F '\t' '
		function trim(s) {
			gsub(/^[[:space:]]+|[[:space:]]+$/, "", s)
			return s
		}
		function refs(s, parts, n, i, item, out) {
			s = trim(s)
			if (s == "") {
				return ""
			}
			sub(/^\(/, "", s)
			sub(/\)$/, "", s)
			n = split(s, parts, /, /)
			out = ""
			for (i = 1; i <= n; i++) {
				item = parts[i]
				gsub(/tag: /, "tag ", item)
				if (out == "") {
					out = item
				} else {
					out = out " · " item
				}
			}
			return "(" out ")"
		}
		{
			if (NF < 5) {
				print "  " $0
				next
			}
			n = split($1, graph_parts, /[[:space:]]+/)
			sha = graph_parts[n]
			graph = $1
			sub("[[:space:]]*" sha "$", "", graph)
			graph = trim(graph)
			reftext = refs($2)
			if (reftext == "") {
				printf "  %-4s %-8s  %-44s  %-38s  %-12s %s\n", graph, sha, "", $3, $4, $5
			} else {
				printf "  %-4s %-8s  %-44s  %-38s  %-12s %s\n", graph, sha, reftext, $3, $4, $5
			}
		}
	'
	exit $?
fi

contains_line() {
	printf '%s\n' "$1" | grep -Fxq "$2"
}

print_two_columns() {
	items=$1
	set -- $items
	if [ "$#" -eq 0 ]; then
		echo "    none"
		return
	fi

	while [ "$#" -gt 0 ]; do
		left=$1
		shift
		if [ "$#" -gt 0 ]; then
			right=$1
			shift
			printf '    %-42s %s\n' "$left" "$right"
		else
			printf '    %s\n' "$left"
		fi
	done
}

print_recently_merged() {
	limit=3
	found=0
	tmp_file="${TMPDIR:-/tmp}/branch_status_recent_merges.$$"

	: > "$tmp_file"
	git log main --first-parent -n 30 --date=format:'%Y-%m-%d %H:%M' --format='%h	%ad	%s' |
	while IFS='	' read -r sha merge_time subject; do
		case "$subject" in
			merge:\ *\ into\ main)
				branch=${subject#merge: }
				branch=${branch% into main}
				if contains_line "$remote_heads" "$branch"; then
					state="✓ pushed branch"
				elif git show-ref --verify --quiet "refs/heads/$branch"; then
					state="! local only, safe prune"
				else
					state="branch ref gone"
				fi
				printf '    %-42s merge %-8s  %s  %s\n' "$branch" "$sha" "$merge_time" "$state" >> "$tmp_file"
				found=$((found + 1))
				[ "$found" -ge "$limit" ] && break
				;;
		esac
	done

	if [ -s "$tmp_file" ]; then
		cat "$tmp_file"
	else
		echo "    none"
	fi
	rm -f "$tmp_file"
}

remote_heads=$(git ls-remote --heads origin 2>/dev/null | sed 's|.*refs/heads/||')
remote_tags=$(git ls-remote --tags origin 2>/dev/null |
	sed 's|.*refs/tags/||; s|\^{}$||' |
	sort -u)

current_branch=$(git branch --show-current 2>/dev/null)
[ -n "$current_branch" ] || current_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)

remote_main_version() {
	ref=$1
	commit=$(git rev-parse --short "$ref" 2>/dev/null)
	version=$(git describe --tags "$ref" 2>/dev/null)
	[ -n "$version" ] || version=$commit
	printf '%s (%s)' "$version" "$commit"
}

append_remote_main_line() {
	remote=$1
	ref="refs/remotes/$remote/main"
	remote_ref="$remote/main"

	if git show-ref --verify --quiet "$ref"; then
		version=$(remote_main_version "$remote_ref")
		if [ "$remote" = "origin" ]; then
			set -- $(git rev-list --left-right --count origin/main...main 2>/dev/null)
			behind=${1:-0}
			ahead=${2:-0}
			if [ "$ahead" -eq 0 ] && [ "$behind" -eq 0 ]; then
				sync_note="✓ synced"
			else
				sync_note="! diverged"
			fi
			line=$(printf '  %-9s %-20s  %s   ↑%s ↓%s' \
				"$remote" "$version" "$sync_note" "$ahead" "$behind")
		else
			line=$(printf '  %-9s %s' "$remote" "$version")
		fi
	else
		[ "$remote" = "origin" ] || return
		line="  remote    origin/main          ! missing"
	fi

	remote_lines="${remote_lines}
$line"
}

remotes=$(git remote)
remote_lines=""
origin_seen=0
if contains_line "$remotes" "origin"; then
	origin_seen=1
	append_remote_main_line origin
fi

for remote in $remotes; do
	[ "$remote" = "origin" ] && continue
	append_remote_main_line "$remote"
done

if [ "$origin_seen" -eq 0 ]; then
	remote_lines="${remote_lines}
  remote    origin/main          ! missing"
fi

worktree_count=$(git status --porcelain | wc -l | tr -d ' ')
if [ "$worktree_count" -eq 0 ]; then
	worktree_note="clean"
else
	worktree_note="$worktree_count changes"
fi

local_tags=$(git tag --sort=version:refname)
tag_lines=""
local_only_tags=""
tags_on_origin=""

for tag in $local_tags; do
	tag_short=$(git rev-parse --short "$tag^{commit}" 2>/dev/null)
	[ -n "$tag_short" ] || tag_short=$(git rev-parse --short "$tag" 2>/dev/null)
	if contains_line "$remote_tags" "$tag"; then
		tag_state="✓ pushed"
		tags_on_origin="$tags_on_origin $tag"
	else
		tag_state="! local only"
		local_only_tags="$local_only_tags $tag"
	fi
	tag_lines="${tag_lines}
  tag       $tag  $tag_short      $tag_state"
done

merged_count=0
not_merged_lines=""
safe_local_only=""
at_risk_branches=""

for branch in $(git branch --format='%(refname:short)'); do
	[ "$branch" = "main" ] && continue

	if contains_line "$remote_heads" "$branch"; then
		push_state="✓ pushed"
	else
		push_state="! local only"
	fi

	if git merge-base --is-ancestor "$branch" main 2>/dev/null; then
		merged_count=$((merged_count + 1))
		if [ "$push_state" = "! local only" ] && [ "$branch" != "$current_branch" ]; then
			safe_local_only="$safe_local_only $branch"
		fi
	else
		sha=$(git rev-parse --short "$branch")
		ahead_main=$(git rev-list --count main.."$branch")
		not_merged_lines="${not_merged_lines}
  $branch  $sha  ↑$ahead_main  $push_state"
		if [ "$push_state" = "! local only" ]; then
			at_risk_branches="$at_risk_branches $branch"
		fi
	fi
done

echo "SYNC"
printf '  branch    %s\n' "$current_branch"
printf '%s\n' "$remote_lines" | sed '/^$/d'
printf '  worktree  %s\n' "$worktree_note"
if [ -n "$tag_lines" ]; then
	printf '%s\n' "$tag_lines" | sed '/^$/d'
else
	echo "  tag       none"
fi
echo

echo "BRANCHES"
if [ -n "$not_merged_lines" ]; then
	echo "  not merged into main:"
	printf '%s\n' "$not_merged_lines" | sed '/^$/d'
else
	echo "  not merged into main:   none"
fi
echo "  last merged into main:"
print_recently_merged
printf '  merged into main:       %s branches   (work all in main)\n' "$merged_count"
echo

echo "LOCAL ONLY  (label only on this laptop)"
echo "  merged — safe to prune:"
print_two_columns "$safe_local_only"
if [ -n "$local_only_tags" ]; then
	echo "  tags:"
	print_two_columns "$local_only_tags"
fi
if [ -n "$at_risk_branches" ]; then
	printf '  at risk (unmerged + unpushed):%s\n' "$at_risk_branches"
else
	echo "  at risk (unmerged + unpushed):   none"
fi
echo

echo "RISK"
would_lose=""
[ -n "$at_risk_branches" ] && would_lose="$would_lose branches:$at_risk_branches"
[ -n "$local_only_tags" ] && would_lose="$would_lose tags:$local_only_tags"
if [ -n "$would_lose" ]; then
	printf '  ! would be lost:%s\n' "$would_lose"
else
	if [ -n "$tags_on_origin" ]; then
		printf '  ✓ nothing at risk — main and tag%s are on origin\n' "$tags_on_origin"
	else
		echo "  ✓ nothing at risk — main is on origin"
	fi
fi
