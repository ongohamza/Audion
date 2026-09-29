#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
src=${1:?Pass the pinned DXVK source directory}
base=b1a1c99ab52b687cf950d62c88bc2fa316b41663
git -C "$src" cat-file -e "$base^{commit}"
scratch=$(mktemp -d /tmp/audion-dxvk-patches.XXXXXX)
trap '[[ $scratch == /tmp/audion-dxvk-patches.* ]] && rm -rf -- "$scratch"' EXIT
mapfile -t entries < "$project/manifests/dxvk-3.1.1.list"
patches=()
for entry in "${entries[@]}"; do patches+=("$project/$entry"); done
mapfile -t paths < <(git apply --numstat "${patches[@]}" | cut -f3 | sort -u)
git -C "$src" archive "$base" "${paths[@]}" | tar -x -C "$scratch"
git -C "$scratch" init -q
git -C "$scratch" add .
for patch in "${patches[@]:0:5}"; do
    git -C "$scratch" apply --check "$patch"
    git -C "$scratch" apply "$patch"
done
git -C "$scratch" -c user.name=Test -c user.email=test@example.invalid commit -qm baseline
printf 'unrelated staged work\n' > "$scratch/user-note.txt"
git -C "$scratch" add user-note.txt
index_before=$(git -C "$scratch" diff --cached --binary | sha256sum)
bash "$project/scripts/apply-dxvk-patches.sh" "$scratch"
bash "$project/scripts/apply-dxvk-patches.sh" "$scratch"
[[ $(git -C "$scratch" diff --cached --binary | sha256sum) == "$index_before" ]]
for path in "${paths[@]}"; do
    cmp "$scratch/$path" "$src/$path"
done
rm "$scratch/src/d3d11/d3d11_swapchain.cpp"
before=$(git -C "$scratch" diff --binary | sha256sum)
if bash "$project/scripts/apply-dxvk-patches.sh" "$scratch" > "$scratch/conflict.log" 2>&1; then
    echo 'FAIL: damaged DXVK stack accepted'; exit 1
fi
[[ $(git -C "$scratch" diff --binary | sha256sum) == "$before" ]]
echo 'PASS: DXVK upgrade, repeat application, index preservation, tested source match and conflict preflight'
