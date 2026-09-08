#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <wine-source>" >&2
    echo "Set ALLOW_UNTESTED_WINE=1 to attempt a different Wine revision." >&2
    exit 2
}

[[ $# -eq 1 ]] || usage

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)
source_dir=$(realpath -- "$1")
patches=(
    "$project_dir/patches/flstudio-authattrs/0001-crypt32-tests-Test-signatures-with-unsorted-authenti.patch"
    "$project_dir/patches/flstudio-authattrs/0002-crypt32-Preserve-authenticated-attribute-order-when-.patch"
    "$project_dir/patches/opengl-flicker/0001-win32u-Re-present-offscreen-client-surfaces-after-wi.patch"
)

# shellcheck source=../manifests/wine-11.16.env
source "$project_dir/manifests/wine-11.16.env"

[[ -f "$source_dir/dlls/crypt32/msg.c" ]] || {
    echo "error: not a Wine source tree: $source_dir" >&2
    exit 1
}
git -C "$source_dir" rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
    echo "error: patch application requires a Git Wine checkout" >&2
    exit 1
}

git_dir=$(git -C "$source_dir" rev-parse --absolute-git-dir)
if [[ -e "$git_dir/rebase-apply" || -e "$git_dir/rebase-merge" ||
      -e "$git_dir/sequencer" ]] ||
   git -C "$source_dir" rev-parse --verify -q MERGE_HEAD >/dev/null ||
   git -C "$source_dir" rev-parse --verify -q CHERRY_PICK_HEAD >/dev/null ||
   git -C "$source_dir" rev-parse --verify -q REVERT_HEAD >/dev/null; then
    echo "error: another Git operation is already in progress" >&2
    exit 1
fi

if [[ -n $(git -C "$source_dir" status --porcelain) ]]; then
    echo "error: Wine checkout is not clean: $source_dir" >&2
    exit 1
fi

actual_commit=$(git -C "$source_dir" rev-parse HEAD)
if [[ "$actual_commit" != "$WINE_COMMIT" && ${ALLOW_UNTESTED_WINE:-0} != 1 ]]; then
    echo "error: expected Wine $WINE_VERSION commit $WINE_COMMIT" >&2
    echo "       found $actual_commit" >&2
    echo "       set ALLOW_UNTESTED_WINE=1 only after reviewing the patch context" >&2
    exit 1
fi

for patch_file in "${patches[@]}"; do
    [[ -f "$patch_file" ]] || {
        echo "error: required patch is missing: $patch_file" >&2
        exit 1
    }
done

echo "Applying ${#patches[@]} patch(es) to $source_dir"
abort_owned_am() {
    trap - HUP INT TERM
    echo "error: interrupted; aborting this script's partial git-am" >&2
    git -C "$source_dir" am --abort || true
    exit 130
}
trap abort_owned_am HUP INT TERM
if ! git -C "$source_dir" am --3way "${patches[@]}"; then
    echo "error: patch application failed; aborting the partial git-am" >&2
    git -C "$source_dir" am --abort || true
    trap - HUP INT TERM
    exit 1
fi
trap - HUP INT TERM

git -C "$source_dir" log --oneline -n "${#patches[@]}"
