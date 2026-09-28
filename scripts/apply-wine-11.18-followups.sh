#!/usr/bin/env bash
# Upgrade an existing four-patch Audion 11.18 source tree without resetting it.
set -euo pipefail
die() { echo "ERROR: $*" >&2; exit 1; }
[[ $# == 1 ]] || die 'Usage: apply-wine-11.18-followups.sh /absolute/wine-source'
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src=$(realpath -- "$1")
[[ -f $src/dlls/d2d1/device.c ]] || die 'Not a Wine source tree.'
[[ $(git -C "$src" describe --tags --match 'wine-[0-9]*') == wine-11.18* ]] || die 'Expected Wine 11.18 source.'
git_dir=$(git -C "$src" rev-parse --absolute-git-dir)
[[ ! -e $git_dir/rebase-apply && ! -e $git_dir/rebase-merge && ! -e $git_dir/MERGE_HEAD && ! -e $git_dir/sequencer ]] ||
    die 'Another Git operation is in progress.'
patches=()
while IFS= read -r entry || [[ -n $entry ]]; do
    [[ -z $entry || $entry == \#* ]] && continue
    [[ $entry == patches/* && $entry != *..* && -f $project/$entry ]] || die "Invalid/missing patch: $entry"
    patches+=("$project/$entry")
done < "$project/manifests/wine-11.18-followups.list"
((${#patches[@]})) || die 'Empty patch manifest.'

# Simulate the whole operation in a private index, including existing working
# changes. The user's index, commits and files remain untouched during preflight.
scratch=$(mktemp -d)
cleanup() { rm -f -- "$scratch/index" "$scratch/index.lock"; rmdir -- "$scratch"; }
trap cleanup EXIT
index_git() { GIT_INDEX_FILE="$scratch/index" git -C "$src" "$@"; }
mapfile -t touched < <(git apply --numstat "${patches[@]}" | cut -f3 | sort -u)
signature() {
    { git -C "$src" rev-parse HEAD; git -C "$src" diff --binary HEAD;
      for path in "${touched[@]}"; do [[ ! -f $src/$path ]] || sha256sum "$src/$path"; done;
    } | sha256sum
}
before=$(signature)
index_git read-tree HEAD
if ! git -C "$src" diff --quiet HEAD; then
    git -C "$src" diff --binary HEAD | index_git apply --cached
fi
# Previously applied patches can introduce new, not-yet-committed files.
for path in "${touched[@]}"; do
    if [[ -f $src/$path ]] && ! git -C "$src" ls-files --error-unmatch -- "$path" >/dev/null 2>&1; then
        index_git add -- "$path"
    fi
done
pending=()
for patch in "${patches[@]}"; do
    if index_git apply --cached --reverse --check "$patch" 2>/dev/null; then
        echo "Already applied: $(basename "$patch")"
    else
        index_git apply --cached --check "$patch" || die "Patch conflicts: $patch; no source files changed."
        index_git apply --cached "$patch"
        pending+=("$patch")
    fi
done

# Verify the original four Audion patches are still present under the additions.
for ((i=${#patches[@]}-1; i>=0; --i)); do index_git apply --cached --reverse "${patches[i]}"; done
core=(
    patches/effectrix-crash/0001-gdi32-Add-opt-in-protection-against-malformed-Delete.patch
    patches/wine-11.18/0001-win32u-Re-present-offscreen-client-surfaces-after-wi.patch
    patches/flstudio-authattrs/0002-crypt32-Preserve-authenticated-attribute-order-when-.patch
    patches/flstudio-authattrs/0001-crypt32-tests-Test-signatures-with-unsorted-authenti.patch
)
for entry in "${core[@]}"; do
    index_git apply --cached --reverse "$project/$entry" || die "Original Audion stack missing or modified: $entry"
done
[[ $(signature) == "$before" ]] || die 'Source changed during preflight; retry when no editor/build is modifying it.'
if ((${#pending[@]})); then git -C "$src" apply "${pending[@]}"; fi
echo 'PASS: all listed follow-up patches present; original Audion patches verified. No compilation or installation performed.'
