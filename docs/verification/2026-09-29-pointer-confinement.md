# FL Studio Wayland pointer confinement verification

## Diagnosis and scope

The baseline trace showed Wine passing a full-desktop rectangle
`(0,0)-(1920,1080)` to a plugin at `(710,614)-(1115,1026)`. The Wayland
backend confined the cursor to that plugin's `405x412` surface. The user
reproduced the pointer trap in the isolated FL Studio test.

Patch `wayland/0004` releases desktop-covering native constraints for visible
FL-compatible cursors. It retains hidden-cursor locks, pending-warp handling,
and narrower clips. The screen comparison uses raw desktop coordinates,
outside the pointer lock. Existing automatic FL detection enables the fix.

The server also distinguishes automatic full-desktop clipping from explicit
application clipping. Only input with the existing Wayland compatibility flag
can bypass the former. Narrower fullscreen/monitor clips still apply.
No audio callback, PipeASIO, scheduling, ntsync, timer or buffer setting changes.

## Evidence

- Before the driver change, the recording-backend regression failed five cases.
  Afterward, all 11 cases pass, including visible/hidden cursors, the cursor-shape
  protocol, negative desktop origins, narrower clips, non-FL mode and warp cleanup.
- A native Wayland regression reproduced the server gap: under automatic
  fullscreen clipping, `(2000,1140)` became `(1919,1079)`. The patch preserves
  `(2000,1140)`. Trace flags `0x28` confirm automatic fullscreen clipping was active.
- All 13 coordinate/hit-test/clip checks pass for both 64-bit and WoW64 programs
  on an isolated virtual KWin Wayland compositor, at 1920x1080. This avoids real
  desktop pointer events racing the synthetic assertions. Live-desktop test
  attempts had such interference and are not the final automated results.
- The ordinary-hardware check deliberately uses a different point after the
  Wayland check: Wine drops unchanged motion before reaching cursor clipping.
- All 12 server policy combinations pass: ordinary/Wayland input, no/explicit/
  automatic clip, full-desktop/narrower region. This is a mocked policy check;
  a physical multi-monitor setup has not been validated.
- The user confirmed pointer escape through plugin edges and retained controls
  in response to the Nexus/second-plugin, knob/menu/window-movement test.
  This live confirmation preceded the additional server coordinate fix.
- Full incremental Wine build succeeds. Existing move-gating, builder CLI,
  public naming, follow-up application, idempotence, staged-index preservation,
  and conflict-preflight checks pass.
- Independent review found no blocking issue with the driver lock order,
  visibility logic or the shared server predicate.

## Reproduce policy checks

```bash
python3 scripts/tests/wayland-clip-gating.py /path/to/wine/dlls/winewayland.drv/wayland_pointer.c
python3 scripts/tests/wayland-server-clip.py /path/to/wine/server/queue.c
python3 scripts/tests/wayland-move-gating.py /path/to/wine/dlls/winewayland.drv/window.c
bash scripts/tests/followups-test.sh /path/to/wine
```

Compile `scripts/tests/wayland-input.c` with both MinGW compilers and run with
native Wayland (`DISPLAY` unset), using a dedicated prefix. Prefer an isolated
virtual compositor for synthetic input; actual desktop input can overwrite the
asserted positions. Confirm automatic fullscreen trace flags in both runs.

The fix addresses the shared confinement failure, not a guarantee of every
plugin or compositor. Fractional scaling, physical multi-monitor behavior and
PREEMPT_RT audio performance remain unmeasured.
