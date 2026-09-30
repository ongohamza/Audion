# FL main-window movement and resizing — verification

## Observed failure

The user's recording shows the main window becoming narrow while an edge is
being dragged. The native Wayland trace confirms FL repeatedly changes its
Win32 rectangle through SetWindowPos, with no SC_SIZE or native resize request.
For a left-edge drag the logical left edge progresses from 656 to 1223 while
the right edge stays at 1793. The compositor's surface origin does not follow
those position changes; later pointer events have negative surface-local X and
are clamped to the moving logical edge, feeding back into FL's size calculation.

The first candidate translates border drags into compositor resize requests.
The user confirmed unrestricted resizing, but reported that main-window
movement did not work. Its trace shows position-only requests with flags 0815
and repeated menu-coordinate corrections, without a native move request.
The second candidate extends the existing plugin-move handoff to FL's normal
main window, excluding corrective rebases and native resize/configure changes.

## Validation

- The resize detector initially fails its positive handoff checks on the
  preceding code. The final extracted-production-code harness passes 39 checks
  under ASAN/UBSAN: all edges/corners, repeat suppression, main-class/style
  restrictions, physical press/focus guards, programmatic/configure exclusions,
  absent seat, fractional scale, opposite-edge presses, border tolerance,
  unanchored changes and corners whose first motion changes only one axis.
- Two corner-axis checks fail before the corner correction and pass afterward.
- The extended main/plugin movement harness fails the main-window handoff before
  implementation and passes afterward, including rebase/native-resize exclusion
  and a new physical press after resizing.
- Final live trace records native move, resize-left, resize-top-left, another
  move, and resize-bottom-left requests. Asked to move, resize, move again,
  shrink to the minimum and open File/View, the user replied “works”. This
  validates that combined path; no universal compositor/application claim is made.
- Full Wine build passes. Native main/child geometry checks pass for x64/WoW64,
  compatibility enabled/disabled, at 100% and actual 135% Wayland scale.
- Existing native input/hit-test/confinement checks pass 13 cases per bitness.
  Main-class restore probes recover usable normal windows in normal,
  pre-maximized and WoW64 cases, matching prior behavior. The artificial
  pre-maximized case restores 800x600; preservation of maximized state is not
  asserted by that probe.
- Eleven pointer-refresh checks, existing move/clip/server policy checks,
 12 restoration lifecycle cases and four file-drop harnesses pass.
- Independent review found no blockers. Gesture lifetime and menu-correction
  exclusions use the existing data→pointer→seat lock order; synthetic release
  is sent after releasing locks. The move-then-resize-on-the-same-press sequence
  is not claimed as a tested application behavior.
- Cumulative patch application/reapplication, staged-index preservation,
  conflict preflight, builder CLI and public naming checks pass.

## Scope

This is automatic under the existing FL compatibility. Main-window handoff is
restricted to normal visible TFruityLoopsMainForm windows; existing plugin
movement is retained. Border eligibility uses a 12-pixel tolerance. Normal
Windows system-command sizing continues through its existing path. The patch
does not implement a general custom-resize detector for every Windows program.

No audio callbacks, ntsync, PipeASIO buffers, scheduling, kernel or timer
settings change. There is no new polling thread. Audio latency was not measured.

```bash
python3 scripts/tests/wayland-resize-gating.py /path/to/wine/dlls/winewayland.drv/window.c
python3 scripts/tests/wayland-move-gating.py /path/to/wine/dlls/winewayland.drv/window.c
```
