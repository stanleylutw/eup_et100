#!/bin/sh
#
# EUP AIoT firmware branch status helper.
#
# Default mode is a short decision summary for the "git status" shortcut.
# Use --full for the complete SYNC / BRANCHES / LOCAL ONLY / RISK report.
# Use --log [N] for a compact history graph.

usage() {
	echo "usage: sh tools/branch_status.sh [--full | --log [N]]" >&2
	exit 2
}

cd "$(git rev-parse --show-toplevel)" || exit 1

case "${1-}" in
	"")
		[ "$#" -eq 0 ] || usage
		mode=short
		;;
	--full)
		[ "$#" -eq 1 ] || usage
		mode=full
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
	date_format='%Y-%m-%d %H:%M'
	if [ "$mode" = short ]; then
		limit=1
		date_format='%m-%d %H:%M'
	fi
	found=0
	tmp_file="$cache_dir/recent_merges"

	: > "$tmp_file"
	git log main --first-parent -n 30 --date="format:$date_format" --format='%h	%ad	%s' |
	while IFS='	' read -r sha merge_time subject; do
		case "$subject" in
			merge:\ *\ into\ main)
				branch=${subject#merge: }
				branch=${branch% into main}
				if [ "$origin_ok" -eq 0 ]; then
					state="? unknown"
				elif contains_line "$remote_heads" "$branch"; then
					state="✓ pushed branch"
				elif git show-ref --verify --quiet "refs/heads/$branch"; then
					state="! local only, safe prune"
				else
					state="branch ref gone"
				fi
				if [ "$mode" = short ]; then
					printf '  last merge  %s  %s  %s\n' "$branch" "$sha" "$merge_time" >> "$tmp_file"
				else
					printf '    %-42s merge %-8s  %s  %s\n' "$branch" "$sha" "$merge_time" "$state" >> "$tmp_file"
				fi
				found=$((found + 1))
				[ "$found" -ge "$limit" ] && break
				;;
		esac
	done

	if [ -s "$tmp_file" ]; then
		cat "$tmp_file"
	else
		if [ "$mode" = short ]; then
			echo "  last merge  none"
		else
			echo "    none"
		fi
	fi
	rm -f "$tmp_file"
}

cache_dir=$(mktemp -d "${TMPDIR:-/tmp}/branch_status.XXXXXXXX") || exit 1
trap 'rm -rf "$cache_dir"' 0
trap 'exit 1' HUP INT TERM

current_branch=$(git branch --show-current 2>/dev/null)
[ -n "$current_branch" ] || current_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
main_sha=$(git rev-parse main)

remote_main_version() {
	ref=$1
	commit=$(printf '%.7s' "$ref")
	if git cat-file -e "$ref^{commit}" 2>/dev/null; then
		commit=$(git rev-parse --short "$ref")
	fi
	version=$(git describe --tags "$ref" 2>/dev/null)
	[ -n "$version" ] || version=$commit
	printf '%s (%s)' "$version" "$commit"
}

append_remote_main_line() {
	remote=$1
	ref="refs/remotes/$remote/main"
	remote_file="$cache_dir/remote_$remote_index"
	remote_index=$((remote_index + 1))
	remote_ok=0
	# Parse only successful snapshots: failed queries are not empty remotes.
	if GIT_TERMINAL_PROMPT=0 git ls-remote "$remote" > "$remote_file" 2>/dev/null; then
		remote_ok=1
	else
		: > "$remote_file"
	fi
	live_main=$(awk '$2 == "refs/heads/main" {print $1; exit}' "$remote_file")
	remote_tag_names=$(awk '$2 ~ /^refs\/tags\// {sub(/^refs\/tags\//, "", $2); sub(/\^\{\}$/, "", $2); print $2}' "$remote_file" | sort -u)

	if [ "$remote" = origin ]; then
		origin_ok=$remote_ok
		origin_main=$live_main
		remote_heads=$(awk '$2 ~ /^refs\/heads\// {sub(/^refs\/heads\//, "", $2); print $2}' "$remote_file")
		remote_tags=$remote_tag_names
	elif [ "$remote" = company ]; then
		company_seen=1
		company_ok=$remote_ok
		company_tags=$remote_tag_names
	fi

	if [ "$remote_ok" -eq 0 ]; then
		stale_main=$(git rev-parse --verify "$ref" 2>/dev/null)
		stale_version=unreachable
		if [ -n "$stale_main" ]; then
			version=$(remote_main_version "$stale_main")
			stale_version=$(git describe --tags "$stale_main" 2>/dev/null)
			[ -n "$stale_version" ] || stale_version=$(printf '%.7s' "$stale_main")
			line=$(printf '  %-9s ? %s  (stale: remote unreachable)' "$remote" "$version")
		else
			line=$(printf '  %-9s ? unreachable' "$remote")
		fi
		[ "$remote" != company ] || company_note="? $stale_version  (stale: remote unreachable)"
	elif [ -z "$live_main" ]; then
		line=$(printf '  %-9s ? main missing' "$remote")
		if [ "$remote" = origin ]; then
			origin_note="⚠ main missing"
			line="  remote    origin/main          ! missing"
		elif [ "$remote" = company ]; then
			company_note="? main missing"
		fi
	else
		version=$(remote_main_version "$live_main")
		if [ "$remote" = origin ]; then
			if git cat-file -e "$live_main^{commit}" 2>/dev/null; then
				set -- $(git rev-list --left-right --count "$live_main...main")
				behind=$1
				ahead=$2
				sync_note="! diverged"
				if [ "$ahead" -eq 0 ] && [ "$behind" -eq 0 ]; then
					sync_note="✓ synced"
					origin_note="✓ synced  ↑0 ↓0"
				elif [ "$behind" -eq 0 ]; then
					origin_note="⚠ ahead  ↑$ahead ↓0"
				elif [ "$ahead" -eq 0 ]; then
					origin_note="⚠ behind  ↑0 ↓$behind"
				else
					origin_note="⚠ diverged  ↑$ahead ↓$behind"
				fi
				line=$(printf '  %-9s %-20s  %s   ↑%s ↓%s' "$remote" "$version" "$sync_note" "$ahead" "$behind")
			else
				origin_unfetched=1
				origin_note="⚠ behind  (run git fetch)"
				line=$(printf '  %-9s %-20s  %s' "$remote" "$version" "$origin_note")
			fi
		else
			line=$(printf '  %-9s %s' "$remote" "$version")
			if [ "$remote" = company ]; then
				company_version=$(git describe --tags --match 'v[0-9]*' "$live_main" 2>/dev/null)
				if [ -z "$company_version" ]; then
					company_version=$(awk -v sha="$live_main" '$1 == sha && $2 ~ /^refs\/tags\/v[0-9]/ {sub(/^refs\/tags\//, "", $2); sub(/\^\{\}$/, "", $2); print $2; exit}' "$remote_file")
				fi
				[ -n "$company_version" ] || company_version=$(printf '%.7s' "$live_main")
				if [ "$live_main" = "$main_sha" ]; then
					company_note="✓ $company_version  main 已領先 0 個 release"
				elif git cat-file -e "$live_main^{commit}" 2>/dev/null &&
					git merge-base --is-ancestor "$live_main" main; then
					releases=0
					for release_tag in $(git tag --merged main --no-merged "$live_main" --list 'v[0-9]*'); do
						releases=$((releases + 1))
					done
					company_note="⚠ $company_version  main 已領先 $releases 個 release"
				else
					company_note="⚠ $company_version  (main difference unknown)"
				fi
			fi
		fi
	fi
	remote_lines="$remote_lines
$line"
}

remotes=$(git remote)
remote_lines=""
origin_seen=0
origin_ok=0
company_seen=0
company_ok=0
remote_index=0
remote_heads=""
remote_tags=""
company_tags=""
origin_main=""
origin_note="? unreachable"
origin_unfetched=0
behind=0
ahead=0
if contains_line "$remotes" origin; then
	origin_seen=1
	append_remote_main_line origin
fi
for remote in $remotes; do
	[ "$remote" = origin ] && continue
	append_remote_main_line "$remote"
done
if [ "$origin_seen" -eq 0 ]; then
	remote_lines="$remote_lines
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
	if [ "$origin_ok" -eq 0 ]; then
		tag_state="? unknown"
	elif contains_line "$remote_tags" "$tag"; then
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
short_branch_risks=""

for branch in $(git branch --format='%(refname:short)'); do
	[ "$branch" = "main" ] && continue

	if [ "$origin_ok" -eq 0 ]; then
		push_state="? unknown"
	elif contains_line "$remote_heads" "$branch"; then
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
			short_branch_risks="$short_branch_risks
  ⚠ unpushed      $branch  ↑$ahead_main  local only"
		fi
	fi
done
if [ "$mode" = short ]; then
	echo "SYNC"
	printf '  branch      %s\n' "$current_branch"
	printf '  origin      %s\n' "$origin_note"
	if [ "$company_seen" -eq 1 ]; then
		printf '  company     %s\n' "$company_note"
	fi
	print_recently_merged
	last_tag=$(git describe --tags --abbrev=0 --match 'v[0-9]*' main 2>/dev/null)
	if [ -n "$last_tag" ]; then
		origin_tag_state="?"
		if [ "$origin_ok" -eq 1 ]; then
			origin_tag_state="✗"
			contains_line "$remote_tags" "$last_tag" && origin_tag_state="✓"
		fi
		printf '  last tag    %s  %s origin' "$last_tag" "$origin_tag_state"
		if [ "$company_seen" -eq 1 ]; then
			company_tag_state="?"
			if [ "$company_ok" -eq 1 ]; then
				company_tag_state="✗"
				contains_line "$company_tags" "$last_tag" && company_tag_state="✓"
			fi
			printf '  %s company' "$company_tag_state"
		fi
		echo
	else
		echo "  last tag    none"
	fi
	echo
	echo "RISK"
	risk_found=0
	if [ "$worktree_count" -gt 0 ]; then
		printf '  ⚠ worktree      %s uncommitted files\n' "$worktree_count"
		risk_found=1
	fi
	if [ "$origin_ok" -eq 0 ]; then
		echo "  ? origin unreachable — push state unknown"
		risk_found=1
	else
		if [ "$origin_unfetched" -eq 0 ] && [ -n "$origin_main" ] && [ "$ahead" -gt 0 ]; then
			printf '  ⚠ unpushed main  ↑%s  commits not on origin\n' "$ahead"
			risk_found=1
		fi
		if [ -n "$short_branch_risks" ]; then
			printf '%s\n' "$short_branch_risks" | sed '/^$/d'
			risk_found=1
		fi
		for tag in $local_only_tags; do
			printf '  ⚠ unpushed tag  %s\n' "$tag"
			risk_found=1
		done
		if [ "$origin_unfetched" -eq 1 ]; then
			echo "  ⚠ main behind origin  (run git fetch)"
			risk_found=1
		elif [ "$behind" -gt 0 ]; then
			printf '  ⚠ main behind origin  ↓%s\n' "$behind"
			risk_found=1
		elif [ -z "$origin_main" ]; then
			echo "  ? origin/main missing — sync state unknown"
			risk_found=1
		fi
	fi
	[ "$risk_found" -ne 0 ] || echo "  ✓ nothing at risk"
	exit 0
fi

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
if [ "$origin_ok" -eq 0 ]; then
	echo "    ? unknown (origin unreachable)"
else
	print_two_columns "$safe_local_only"
fi
if [ -n "$local_only_tags" ]; then
	echo "  tags:"
	print_two_columns "$local_only_tags"
fi
if [ "$origin_ok" -eq 0 ]; then
	echo "  at risk (unmerged + unpushed):   ? unknown (origin unreachable)"
elif [ -n "$at_risk_branches" ]; then
	printf '  at risk (unmerged + unpushed):%s\n' "$at_risk_branches"
else
	echo "  at risk (unmerged + unpushed):   none"
fi
echo

echo "RISK"
would_lose=""
[ "$worktree_count" -gt 0 ] && would_lose="$would_lose worktree:$worktree_count"
if [ "$origin_ok" -eq 0 ]; then
	[ -z "$would_lose" ] || printf '  ! would be lost:%s\n' "$would_lose"
	echo "  ? origin unreachable — push state unknown"
	exit 0
fi
if [ "$origin_unfetched" -eq 0 ] && [ -n "$origin_main" ] && [ "$ahead" -gt 0 ]; then
	would_lose="$would_lose main:↑$ahead"
fi
[ -n "$at_risk_branches" ] && would_lose="$would_lose branches:$at_risk_branches"
[ -n "$local_only_tags" ] && would_lose="$would_lose tags:$local_only_tags"
if [ -n "$would_lose" ]; then
	printf '  ! would be lost:%s\n' "$would_lose"
fi
if [ "$origin_unfetched" -eq 1 ]; then
	echo "  ⚠ main behind origin  (run git fetch)"
elif [ "$behind" -gt 0 ]; then
	printf '  ⚠ main behind origin  ↓%s\n' "$behind"
elif [ -z "$origin_main" ]; then
	echo "  ? origin/main missing — sync state unknown"
elif [ -z "$would_lose" ] && [ "$ahead" -eq 0 ]; then
	if [ -n "$tags_on_origin" ]; then
		printf '  ✓ nothing at risk — main and tag%s are on origin\n' "$tags_on_origin"
	else
		echo "  ✓ nothing at risk — main is on origin"
	fi
fi
