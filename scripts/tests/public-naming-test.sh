#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/../.." && pwd)
forbidden='pig''ments'
failed=0

cd -- "$project_dir"

path_matches=$(find . \
    -path ./.git -prune -o \
    -path ./.worktrees -prune -o \
    -iname "*$forbidden*" -print)
if [[ -n $path_matches ]]; then
    echo "FAIL: forbidden product name appears in a path:" >&2
    printf '%s\n' "$path_matches" >&2
    failed=1
fi

text_matches=$(git grep -I -in "$forbidden" -- . || true)
if [[ -n $text_matches ]]; then
    echo "FAIL: forbidden product name appears in tracked text:" >&2
    printf '%s\n' "$text_matches" >&2
    failed=1
fi

pdf_matches=$(pdftotext docs/WINE_DAW_PATCHES.pdf - | rg -in "$forbidden" || true)
if [[ -n $pdf_matches ]]; then
    echo "FAIL: forbidden product name appears in PDF text:" >&2
    printf '%s\n' "$pdf_matches" >&2
    failed=1
fi

mapfile -t commits < <(git rev-list --all --reflog | sort -u)
if ((${#commits[@]})); then
    message_matches=$(git show -s --format='%H %s%n%b' "${commits[@]}" |
        rg -in "$forbidden" || true)
    if [[ -n $message_matches ]]; then
        echo "FAIL: forbidden product name appears in reachable commit messages:" >&2
        printf '%s\n' "$message_matches" >&2
        failed=1
    fi

    history_text_matches=$(git grep -I -in "$forbidden" "${commits[@]}" -- . || true)
    if [[ -n $history_text_matches ]]; then
        echo "FAIL: forbidden product name appears in reachable commit content:" >&2
        printf '%s\n' "$history_text_matches" | sed -n '1,40p' >&2
        failed=1
    fi

    history_path_matches=$(
        for commit in "${commits[@]}"; do
            git ls-tree -r --name-only "$commit" | rg -i "$forbidden" || true
        done | sort -u
    )
    if [[ -n $history_path_matches ]]; then
        echo "FAIL: forbidden product name appears in reachable historical paths:" >&2
        printf '%s\n' "$history_path_matches" >&2
        failed=1
    fi
fi

((failed == 0)) || exit 1
echo "PASS: public naming is product-neutral in paths, text, PDF, and reachable history"
