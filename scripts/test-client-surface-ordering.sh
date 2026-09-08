#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 [wine-runner-prefix-or-binary]" >&2
    exit 2
}

[[ $# -le 1 ]] || usage

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)
runner=${1:-"$HOME/.local/opt/wine-flstudio/current"}
if [[ -x $runner/bin/wine ]]; then
    wine_bin=$runner/bin/wine
else
    wine_bin=$runner
fi
test_tmp=$(mktemp -d "${TMPDIR:-/tmp}/wine-client-surface-ordering.XXXXXXXX")

cleanup() {
    [[ $test_tmp == "${TMPDIR:-/tmp}"/wine-client-surface-ordering.* ]] || return
    rm -rf -- "$test_tmp"
}
trap cleanup EXIT

command -v x86_64-w64-mingw32-gcc >/dev/null || {
    echo "error: x86_64-w64-mingw32-gcc is required" >&2
    exit 1
}
[[ -x $wine_bin ]] || {
    echo "error: Wine runner not found: $wine_bin" >&2
    exit 1
}

x86_64-w64-mingw32-gcc -O2 -Wall -Wextra \
    -o "$test_tmp/client-surface-ordering.exe" \
    "$project_dir/tools/client_surface_ordering.c" \
    -lopengl32 -lgdi32 -luser32

set +e
DISPLAY=${DISPLAY:-:1} WINEPREFIX="$test_tmp/prefix" \
    WINEDEBUG=trace+win,trace+x11drv,trace+wgl \
    "$wine_bin" "$test_tmp/client-surface-ordering.exe" \
    >"$test_tmp/stdout.log" 2>"$test_tmp/wine.log"
status=$?
set -e
[[ $status -eq 0 ]] || {
    echo "error: ordering probe exited with status $status" >&2
    tail -80 "$test_tmp/wine.log" >&2
    exit 1
}

marker_line=$(rg -n -m 1 '^ORDERING_PARENT_FLUSH_DONE' "$test_tmp/wine.log" | cut -d: -f1 || true)
[[ -n $marker_line ]] || {
    echo "error: ordering probe completion marker is missing" >&2
    exit 1
}

trace_before_marker=$test_tmp/before-marker.log
head -n "$marker_line" "$test_tmp/wine.log" >"$trace_before_marker"
present_record=$(rg -n 'X11DRV_client_surface_present hwnd' "$trace_before_marker" | tail -1 || true)
flush_record=$(rg -n 'window_surface_flush[^ ]* Flushing hwnd' "$trace_before_marker" | tail -1 || true)
present_line=${present_record%%:*}
flush_line=${flush_record%%:*}

[[ -n $present_line && -n $flush_line ]] || {
    echo "error: required Wine presentation traces are missing" >&2
    exit 1
}
flush_hwnd=$(sed -E 's/.*Flushing hwnd (0x[0-9a-f]+).*/\1/' <<<"$flush_record")
present_toplevel=$(sed -E 's/.* to toplevel (0x[0-9a-f]+) .*/\1/' <<<"$present_record")
[[ $flush_hwnd == "$present_toplevel" ]] || {
    echo "FAIL: final flush and client presentation target different top-level windows" >&2
    echo "      flush hwnd:       $flush_hwnd" >&2
    echo "      present toplevel: $present_toplevel" >&2
    exit 1
}
if ((present_line <= flush_line)); then
    echo "FAIL: parent software surface was the final presentation" >&2
    echo "      last client present line: $present_line" >&2
    echo "      last parent flush line:   $flush_line" >&2
    exit 1
fi

echo "PASS: redirected child client surface follows the parent software flush"
