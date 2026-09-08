#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <local-wine-git-repository>" >&2
    exit 2
}

[[ $# -eq 1 ]] || usage

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/../.." && pwd)
wine_repo=$(realpath -- "$1")

# shellcheck source=../../manifests/wine-11.16.env
source "$project_dir/manifests/wine-11.16.env"

git -C "$wine_repo" rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
    echo "error: not a Wine Git repository: $wine_repo" >&2
    exit 1
}
git -C "$wine_repo" cat-file -e "$WINE_COMMIT^{commit}" 2>/dev/null || {
    echo "error: Wine repository does not contain pinned commit $WINE_COMMIT" >&2
    exit 1
}

test_root=$(mktemp -d "${TMPDIR:-/tmp}/audion-apply-test.XXXXXXXX")
test_tree="$test_root/wine"
worktree_added=0

cleanup() {
    if ((worktree_added)); then
        git -C "$wine_repo" worktree remove --force "$test_tree" >/dev/null 2>&1 || true
        git -C "$wine_repo" worktree prune >/dev/null 2>&1 || true
    fi
    rmdir -- "$test_root" >/dev/null 2>&1 || true
}
trap cleanup EXIT

git -C "$wine_repo" worktree add --detach "$test_tree" "$WINE_COMMIT" >/dev/null
worktree_added=1

GIT_COMMITTER_NAME="Audion integration test" \
GIT_COMMITTER_EMAIL="audion-test@example.invalid" \
    "$project_dir/scripts/apply-patches.sh" "$test_tree"

mapfile -t subjects < <(git -C "$test_tree" log -3 --format=%s)
expected=(
    "win32u: Re-present offscreen client surfaces after window flushes"
    "crypt32: Preserve authenticated attribute order when verifying"
    "crypt32/tests: Test signatures with unsorted authenticated attributes"
)

for index in "${!expected[@]}"; do
    if [[ ${subjects[$index]:-missing} != "${expected[$index]}" ]]; then
        echo "FAIL: patch subject $((index + 1)) is wrong" >&2
        echo "      expected: ${expected[$index]}" >&2
        echo "      actual:   ${subjects[$index]:-missing}" >&2
        exit 1
    fi
done

[[ -z $(git -C "$test_tree" status --porcelain) ]] || {
    echo "FAIL: patched Wine test tree is not clean" >&2
    git -C "$test_tree" status --short >&2
    exit 1
}

printf 'Applied patch subjects (newest first):\n'
printf '  %s\n' "${subjects[@]}"
echo "PASS: Audion applies the complete three-patch stack in order"
