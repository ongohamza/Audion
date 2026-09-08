#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 [private-runner-prefix]" >&2
    exit 2
}

[[ $# -le 1 ]] || usage

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)

# shellcheck source=../manifests/wine-11.16.env
source "$project_dir/manifests/wine-11.16.env"

runner=${1:-"$HOME/.local/opt/wine-flstudio/current"}
wine_bin="$runner/bin/wine"
winepath_bin="$runner/bin/winepath"
prefix=${WINEPREFIX:-"$HOME/.wine"}
flengine=${FLENGINE_PATH:-"$prefix/drive_c/Program Files/Image-Line/FL Studio 2026/FLEngine_x64.dll"}
verify_tmp=$(mktemp -d "${TMPDIR:-/tmp}/wine-flstudio-verify.XXXXXXXX")
cleanup() {
    rm -rf -- "$verify_tmp"
}
trap cleanup EXIT
trap 'exit 130' HUP INT TERM
probe="$verify_tmp/verify_trust.exe"
tampered="$verify_tmp/FLEngine_x64.tampered.dll"

[[ -x "$wine_bin" && -x "$winepath_bin" ]] || {
    echo "error: private Wine runner not found under $runner" >&2
    exit 1
}
[[ -f "$flengine" ]] || {
    echo "error: FL Studio engine not found: $flengine" >&2
    exit 1
}
command -v x86_64-w64-mingw32-gcc >/dev/null || {
    echo "error: x86_64-w64-mingw32-gcc is required" >&2
    exit 1
}

actual_hash=$(sha256sum -- "$flengine" | awk '{print $1}')
[[ "$actual_hash" == "$FLENGINE_SHA256" ]] || {
    echo "error: refusing an unpinned FL Studio binary" >&2
    echo "       expected $FLENGINE_SHA256" >&2
    echo "       found    $actual_hash" >&2
    exit 1
}

x86_64-w64-mingw32-gcc -municode -O2 -Wall -Wextra \
    -o "$probe" "$project_dir/tools/verify_trust.c" -lwintrust

valid_windows_path=$(WINEPREFIX="$prefix" WINEDEBUG=-all "$winepath_bin" -w "$flengine")
echo "Verifying pinned original: $actual_hash"
if ! valid_output=$(WINEPREFIX="$prefix" WINEDEBUG=-all \
    "$wine_bin" "$probe" "$valid_windows_path" 2>&1); then
    echo "$valid_output" >&2
    echo "valid signature: FAIL" >&2
    exit 1
fi
echo "$valid_output"
valid_code=$(printf '%s\n' "$valid_output" | tr -d '\r' | tail -n 1)
[[ "$valid_code" == 00000000 ]] || {
    echo "error: trust probe returned unexpected success output: $valid_code" >&2
    exit 1
}
echo "valid signature: PASS"

cp -- "$flengine" "$tampered"
tamper_offset=1024
original_byte=$(od -An -tu1 -j "$tamper_offset" -N 1 -- "$tampered" | tr -d '[:space:]')
[[ -n "$original_byte" ]] || {
    echo "error: could not read tamper byte" >&2
    exit 1
}
replacement=$((original_byte ^ 1))
printf "\\$(printf '%03o' "$replacement")" | \
    dd of="$tampered" bs=1 seek="$tamper_offset" count=1 conv=notrunc status=none

tampered_windows_path=$(WINEPREFIX="$prefix" WINEDEBUG=-all "$winepath_bin" -w "$tampered")
set +e
tampered_output=$(WINEPREFIX="$prefix" WINEDEBUG=-all \
    "$wine_bin" "$probe" "$tampered_windows_path" 2>&1)
tampered_status=$?
set -e
echo "$tampered_output"
tampered_code=$(printf '%s\n' "$tampered_output" | tr -d '\r' | tail -n 1)
if [[ $tampered_status -ne 1 || "$tampered_code" != 80096010 ]]; then
    echo "error: expected TRUST_E_BAD_DIGEST (80096010), got status" \
        "$tampered_status and code $tampered_code" >&2
    exit 1
fi
echo "tampered signature: REJECTED"
