#!/usr/bin/env bash
set -euo pipefail
[[ $# == 1 ]] || { echo 'Usage: followups-test.sh /local/wine-repository' >&2; exit 2; }
repo=$(realpath "$1")
base_ref=$(git -C "$repo" log -1 --format=%H --fixed-strings \
    --grep='gdi32: Add opt-in protection against malformed DeleteObject aliases')
[[ -n $base_ref && $(git -C "$repo" describe --tags --match 'wine-[0-9]*' "$base_ref") == wine-11.18* ]] || {
    echo 'Test input must contain the original Audion 11.18 four-patch commit.' >&2; exit 1;
}
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
scratch=$(mktemp -d /tmp/audion-followups-test.XXXXXXXX)
cleanup() { [[ $scratch == /tmp/audion-followups-test.* ]] && rm -rf -- "$scratch"; }
trap cleanup EXIT
fixture="$scratch/wine"
mkdir "$fixture"
git -C "$fixture" init -q
git -C "$fixture" config user.name 'Audion test'
git -C "$fixture" config user.email 'test@example.invalid'
patches=(
    "$project/patches/flstudio-authattrs/0001-crypt32-tests-Test-signatures-with-unsorted-authenti.patch"
    "$project/patches/flstudio-authattrs/0002-crypt32-Preserve-authenticated-attribute-order-when-.patch"
    "$project/patches/wine-11.18/0001-win32u-Re-present-offscreen-client-surfaces-after-wi.patch"
    "$project/patches/effectrix-crash/0001-gdi32-Add-opt-in-protection-against-malformed-Delete.patch"
)
while IFS= read -r entry; do
    [[ -z $entry || $entry == \#* ]] || patches+=("$project/$entry")
done < "$project/manifests/wine-11.18-followups.list"
# Generated fixture: only tracked Wine files touched by the tested patch stack.
mapfile -t paths < <({ git apply --numstat "${patches[@]}" | cut -f3; echo dlls/d2d1/device.c; } | sort -u)
for path in "${paths[@]}"; do
    mkdir -p "$fixture/$(dirname "$path")"
    if git -C "$repo" cat-file -e "$base_ref:$path" 2>/dev/null; then
        git -C "$repo" show "$base_ref:$path" > "$fixture/$path"
    fi
done
git -C "$fixture" add .
git -C "$fixture" commit -qm 'original Staging and Audion stack fixture'
git -C "$fixture" tag wine-11.18
printf 'unrelated staged work\n' > "$fixture/user-note.txt"
git -C "$fixture" add user-note.txt
index_before=$(git -C "$fixture" diff --cached --binary | sha256sum)
bash "$project/scripts/apply-wine-11.18-followups.sh" "$fixture"
first=$(git -C "$fixture" diff --binary | sha256sum)
bash "$project/scripts/apply-wine-11.18-followups.sh" "$fixture"
[[ $(git -C "$fixture" diff --binary | sha256sum) == "$first" ]]
[[ $(git -C "$fixture" diff --cached --binary | sha256sum) == "$index_before" ]] || { echo 'FAIL: user index changed'; exit 1; }
# A source change that removes an original patched file must fail preflight.
mv "$fixture/dlls/crypt32/msg.c" "$scratch/saved-msg.c"
before=$(git -C "$fixture" diff --binary | sha256sum)
if bash "$project/scripts/apply-wine-11.18-followups.sh" "$fixture" > "$scratch/conflict.log" 2>&1; then
    echo 'FAIL: damaged original stack accepted'; exit 1
fi
[[ $(git -C "$fixture" diff --binary | sha256sum) == "$before" ]]
echo 'PASS: follow-up application, idempotence, index preservation, and conflict preflight'
