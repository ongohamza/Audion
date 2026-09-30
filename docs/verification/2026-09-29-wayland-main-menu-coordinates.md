# FL main-menu coordinates — verification

## Reproduction and cause

The user reproduced missing dropdowns after moving/resizing FL Studio on native
Wayland. In the failing live trace, the main window's raw Windows rectangle was
(-1256,1064)-(32,2098), although the compositor still displayed it normally.
FL created dropdown windows inside Wine's desktop; their Wayland subsurface
positions relative to the main window included (931,-773)-(1324,-24). The
menus existed but were displaced from the visible menu bar.

The native geometry probe failed both the parent-coordinate and child-anchor
checks before the fix. With the correction it passes. A live candidate trace
records the main window corrected to (0,46)-(1288,1080) and repeated menu
subsurfaces at expected positions such as (144,31)-(299,443). The user tested
the candidate and confirmed: “looks like it worked.”

## Final checks

- Full incremental Wine build passes.
- Native geometry test: eight checks per run, 64-bit and WoW64, compatibility
  enabled and disabled, at 100% and actual 135% Wayland scaling. All pass.
  Checks cover both directions of off-desktop displacement, child menu anchors,
  valid positions, plugin positions, oversized windows, avoiding newly-triggered
  fullscreen and hidden windows. The compositor trace confirms scale=1.350000.
- Pointer-refresh harness compiles the real production functions with a stubbed
  host under ASAN/UBSAN. Eleven checks pass: fractional mapping, immediate
  application-thread delivery, event-thread correction after stale buffered
  motion, one-time consumption, focus/relative-mode guards, destroyed windows
  and surfaces, and edge rounding. Removing the immediate flush reproduces a
  failing check. This is not a measured concurrent stress test.
- Existing native coordinate/hit-test/confinement tests pass all 13 checks on
  each of 64-bit and WoW64. Existing move, clip, server and 12 restore-lifecycle
  checks pass. All four file-drop harnesses pass.
- Main-class versions of the native restore probe regain a usable normal
  window after normal, pre-maximized and WoW64 minimization. The artificial
  pre-maximized probe returns to 800x600, matching the preceding driver; this
  check verifies recovery from the iconic strip, not preservation of maximized
  state for every application.
- Cumulative patch application, repeat application, preservation of the user's
  staged index, conflict preflight, builder CLI and public naming checks pass.
- Independent review caught and resolved UI-thread motion buffering, stale
  event-thread frames and DPI-context issues. Final review found no blockers
  within the documented scope.

The interactive candidate preceded the final pointer-flush/ordering and monitor
coordinate guards, which were compiled and regression-tested afterward.

## Scope and reproduction

The default FL compatibility applies this only to fitting, visible, normal
TFruityLoopsMainForm toplevels. It preserves size and does not alter physical
compositor placement, audio threads, synchronization, timers or PipeASIO.
Hidden, iconic, maximized, oversized and plugin windows are excluded. A change
that would newly cover a monitor is rejected to avoid triggering fullscreen.
Raw/effective desktop-bound mismatches are excluded; arbitrary emulated display
modes, mixed-monitor configurations and audio latency were not runtime-tested.

Compile scripts/tests/wayland-main-geometry.c with either MinGW compiler. Run in
an isolated native Wayland prefix with WINE_WAYLAND_FLSTUDIO=1. Run it again with
WINE_WAYLAND_FLSTUDIO=0 and the argument `disabled`. A virtual desktop at least
1000x700 is required. The automated runs used isolated KWin sessions; a separate
session's output was set to scale 1.35 without changing the user's desktop.

```bash
python3 scripts/tests/wayland-pointer-refresh.py /path/to/wine/dlls/winewayland.drv/wayland_pointer.c
bash scripts/tests/followups-test.sh /path/to/wine
```
