# Wayland restoration and incoming file drops — verification

## Observed failures

The installed runtime was verified to include the preceding pointer-confinement
patch by matching driver hashes. The user reproduced failed Dolphin-to-Sampler
drops. The Wayland enter, motion, leave and drop callbacks were empty.

The user also reported poor interaction after moving/resizing and supplied a
screenshot of only FL's menu strip at the desktop's top-left. Tracing showed
WS_MINIMIZE with a 160x31 rectangle at (-128,-16). Wine's old restoration code
required coordinates at or below (-32000,-32000), missing this iconic geometry.
During a separate menu input trace, clicks reached TNewMenu and the user said
menus worked. We did not infer or add an unrelated custom-resize workaround.

## Regression evidence

- The original restoration predicate failed iconic and positive-coordinate
  minimized cases, and wrongly accepted an inactive sentinel configuration.
- A first activation-based implementation exposed coalescing/acknowledgment
  races in review. The final code tracks received activation and latches restore
  independently of geometry acknowledgments. Its lifecycle regression failed
  6/12 cases before this correction and passes 12/12 afterward.
- A real borderless Win32 integration probe used WS_MINIMIZE and FL's iconic
  geometry. On isolated virtual KWin, compositor restoration left the old driver
  iconic with zero SC_RESTORE calls (FAIL). The candidate delivered SC_RESTORE,
  cleared WS_MINIMIZE and restored 800x600 (PASS). Normal and pre-maximized 64-bit
  cases and the 32-bit WoW64 case pass.
- Four ASAN/UBSAN-backed file-drop harnesses pass: URI decoding; complete UTF16
  multi-file DROPFILES layout; bounded pipe transfer; GUI delivery contract.
  Cases include spaces/Unicode, CRLF/LF/comments, malformed and remote URIs,
  unmapped paths, terminal doubleNUL, oversized/stalled transfers, stale HWNDs
  and generations, duplicate delivery, rejected targets, failed bridge calls,
  and protocol timeout while the application is still handling a drop.
- A failing duplicate-message test caught completion being overwritten after a
  successful drop; the final claimed guard fixes it.
- Live native Wayland trace records FL's OLE target accepting and completing an
  external drop. Asked to test minimize/taskbar restore, moving/resizing, menus,
  Dolphin-to-Sampler and Playlist drops, the user replied: “it works!”
- That live candidate preceded small robustness refinements for duplicate
  delivery, seat replacement and older core-protocol versions. Those changes
  were reviewed, tested and rebuilt afterward; physical seat hotplug was not
  exercised.

## Build and compatibility checks

Full incremental Wine build passes, including Unix and PE driver components.
Both 64-bit and WoW64 native Wayland coordinate/hit-test/clip suites pass all 13
checks after the final build. Existing pointer-confinement policy, move gesture
and server clipping checks pass. Follow-up application, repeat application,
staged-index preservation, conflict preflight, builder CLI and naming checks
pass. Independent review found no remaining blocker in the final changes.

The core drag device coexists with wlr clipboard selection. Version-aware
cleanup and seat generations are reviewed; a physical compositor/clipboard
matrix and seat hotplug are not claimed as runtime-tested. Clipboard behavior
is retained by construction; the change does not implement outgoing drags.

## Reproduce

```bash
python3 scripts/tests/wayland-restore.py /path/to/wine/dlls/winewayland.drv/window.c
for part in uri files transfer dispatch; do
    python3 "scripts/tests/wayland-dnd-$part.py" /path/to/wine
done
```

Compile `scripts/tests/wayland-restore-integration.c` with the appropriate MinGW
compiler and run with native Wayland in a dedicated prefix. After it prints
MINIMIZED, restore the named window through the compositor/taskbar. Passing an
argument maximizes before minimizing. Our automated run used a separate virtual
KWin session and its scripting API to unminimize and activate only that test
window. No user desktop configuration was modified.

The file-list transfer limit is 1 MiB / 3 seconds; this is metadata, not audio-file
content. App delivery has a bounded protocol wait, while a separate busy flag
prevents overlap if app callbacks run longer. One idle transfer thread per Wine
GUI process waits on a condition variable. No polling loop, real-time priority,
ntsync, audio callback, PipeASIO buffer or kernel setting is changed. Audio
latency and PREEMPT_RT performance were not measured in this UI task.
