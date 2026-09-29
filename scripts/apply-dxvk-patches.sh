#!/usr/bin/env bash
# Preflight overlapping DXVK upgrades without resetting local work.
set -euo pipefail
die() { echo "ERROR: $*" >&2; exit 1; }
[[ $# == 1 ]] || die 'Usage: apply-dxvk-patches.sh /absolute/dxvk-source'
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src=$(realpath -- "$1")
patches=()
while IFS= read -r entry || [[ -n $entry ]]; do
    [[ -z $entry || $entry == \#* ]] && continue
    [[ $entry == patches/dxvk/* && $entry != *..* && -s $project/$entry ]] || die "Invalid patch: $entry"
    patches+=("$project/$entry")
done < "$project/manifests/dxvk-3.1.1.list"
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
# Peel applied follow-ups in reverse order so an overlapping later patch does
# not prevent recognizing its prerequisite on a resumed build.
pending=()
declare -A present=()
for ((i=${#patches[@]}-1; i>=0; --i)); do
    patch=${patches[i]}
    if index_git apply --cached --reverse --check "$patch" 2>/dev/null; then
        index_git apply --cached --reverse "$patch"
        present["$patch"]=1
    fi
done
for patch in "${patches[@]}"; do
    index_git apply --cached --check "$patch" || die "Patch conflicts: $patch; no source files changed."
    index_git apply --cached "$patch"
    if [[ ${present["$patch"]:-0} == 1 ]]; then
        echo "Already applied: $(basename "$patch")"
    else
        pending+=("$patch")
    fi
done

[[ $(signature) == "$before" ]] || die 'Source changed during preflight; retry when it is idle.'
if ((${#pending[@]})); then git -C "$src" apply "${pending[@]}"; fi
echo 'PASS: DXVK patch stack applied; existing index preserved.'
