#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
src=${1:?Pass the pinned DXVK source directory}
base=b1a1c99ab52b687cf950d62c88bc2fa316b41663
git -C "$src" cat-file -e "$base^{commit}"
scratch=$(mktemp -d /tmp/audion-dxvk-patches.XXXXXX)
trap '[[ $scratch == /tmp/audion-dxvk-patches.* ]] && rm -rf -- "$scratch"' EXIT
patches=(
    "$project/patches/dxvk/0001-nexus-composition-snapshot.patch"
    "$project/patches/dxvk/0002-incremental-scroll.patch"
    "$project/patches/dxvk/0003-lock-context-state-swap.patch"
    "$project/patches/dxvk/0004-lock-unmap-entry.patch"
    "$project/patches/dxvk/0005-lock-import-allocation-pool.patch"
)
mapfile -t paths < <(git apply --numstat "${patches[@]}" | cut -f3 | sort -u)
git -C "$src" archive "$base" "${paths[@]}" | tar -x -C "$scratch"
git -C "$scratch" init -q
git -C "$scratch" add .
for patch in "${patches[@]}"; do
    git -C "$scratch" apply --check "$patch"
    git -C "$scratch" apply "$patch"
done
for patch in "${patches[@]}"; do
    git -C "$scratch" apply --reverse --check "$patch"
done
for path in "${paths[@]}"; do
    cmp "$scratch/$path" "$src/$path"
done
echo 'PASS: five DXVK patches apply to pinned base and match tested source'
