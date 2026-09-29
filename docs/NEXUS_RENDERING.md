# Nexus accelerated rendering

The combined candidate was tested in FL Studio 26.1.5 with Nexus 5.5.12,
hardware acceleration enabled, and DXVK 3.1.1 on an AMD RX 5700-series GPU.
After the context-state lock fix, the user confirmed stable tabs, menus,
resizing and editor reopening with no flickering or crash. This is scoped
compatibility support, not a full DirectComposition implementation.

The user-confirmed interaction build was `bca2e81e`. Extended testing of
`0b91a345` exposed an additional allocation-pool race. The final five-patch
build (`9f4da2ca` D3D11 / `43eb4cf3` DXGI) corrects it and passed three real
Nexus resize runs (486 resizes total). See the detailed evidence below;
different candidates' results are not interchangeable.

## Components

Nexus uses Direct2D for drawing. Wine supplies Direct2D; DXVK supplies D3D11 and
DXGI underneath it. The original white window occurred when Nexus requested a
composition swapchain and DXVK rejected it. DXVK 3.1.1's existing dummy
composition option allows initialization to proceed, but does not implement
general-purpose Windows DirectComposition.

The subsequent missing controls exercise Wine's unimplemented Direct2D layer
and bitmap sprite operations. Those need separate Wine changes; rebuilding
DXVK alone cannot supply Wine's Direct2D functionality.

## DXVK configuration

The installed DXVK 3.1.1 accepts this setting in `dxvk.conf` in FL Studio's
working directory:

```ini
[FL64.exe]
dxgi.enableDummyCompositionSwapchain = True
d3d11.enableContextLock = True
```

An alternative for one launch is:

```bash
DXVK_CONFIG='dxgi.enableDummyCompositionSwapchain = True; d3d11.enableContextLock = True' /usr/local/bin/wine /absolute/path/to/FL64.exe
```

This option is process-wide, so other plugins in that FL Studio process also
see it. Keep Nexus's **Use hardware acceleration** enabled for accelerated
rendering tests. Do not edit Nexus settings while another instance is running:
it may write its old settings back when closing.

The paired DXVK patches under `patches/dxvk/` add an independently owned copy
of the last completed frame and correct incremental scrolling. Wine's dcomp
patch consumes that copy instead of reading the buffer Nexus is still drawing.
Context protection must be enabled before device creation; the snapshot API
rejects an unprotected device. Restart FL Studio after changing the setting.
Do not apply Wine patches to DXVK source.

## What caused the defects

- Missing Direct2D layer/sprite support prevented controls from drawing.
- Wine's polling compositor read DXVK's next drawing buffer, not an immutable
  completed frame. The paired snapshot interface fixes that ownership issue.
- DXVK incremental scrolling applied the displacement twice; its separate
  correction preserves previous-frame content before dirty rectangles.
- DXVK's `SwapDeviceContextState` changed rendering state/commands without
  acquiring its context lock. Concurrent Direct2D and composition use could
  corrupt queued graphics commands. Entry locking fixes the reproduced race.
- Review found another pre-lock shared-state access in `Unmap`. Its separate
  entry-lock fix passes a previously failing lock-contract test and 1,000
  concurrent image/buffer map cycles. It is not attributed as the crash cause.
- Resource imports allocated from DXVK's shared object pool without its mutex,
  racing normal allocation/free during presentation and concurrent drawing.
  The fifth patch locks the image/buffer import paths. Sparse allocation paths
  are outside this scoped fix.

## Verification

Direct2D's 22 GPU checks pass. The composition suite passes 61 assertions,
including completed-frame selection, immutable snapshots, dirty rectangles,
scrolling, concurrent presentation/resizing, and a Wine HWND pixel check.
Two additional checks verify rejection without startup context protection.
These tests do not establish that the real plugin is stable.

A realistic geometry/composition stress test crashed after frame 0 before
the context-state lock fix; afterward it passed 3,000 frames, 360,000 geometry
draws and 177 resizes. The real Nexus diagnostic host previously crashed after
five automatic resizes; the lock-fixed run completed 162 and exited normally.
The subsequent FL Studio interactive test was confirmed stable by the user.
Extended allocation stress then reproduced a second race: it crashed after
frame 0 before patch 5 and passed 3,000 frames with 4,714,176 concurrent texture
allocations afterward. All 61 pixel checks and the Unmap lock-contract tests
remain green. Three final real Nexus stress runs completed 162, 163 and 161
resizes respectively, each with a normal exit.

Not established: every GPU, every plugin, 32-bit runtime behavior, full
DirectComposition conformance, or perfect antialiasing. Wine's existing alpha
accumulation behavior is not changed by the snapshot patch. Audio routing,
content availability and licensing are separate from this rendering test.

## Build all components

From the Audion repository, on this existing prepared Audion 11.18 build:

```bash
bash scripts/build-dxvk-nexus.sh
bash scripts/build-wine-11.18-staging.sh --update-existing --register "$HOME/.wine"
```

For a fresh Wine build, omit `--update-existing`. Close Wine applications
before installation. Wine installs to `/usr/local` and compiles using
`make -j8`; DXVK uses `ninja -j8` (override both with `AUDION_JOBS`). Existing downloads/build
objects are reused. PipeASIO and all Wine follow-ups are included by the Wine
script; DXVK DLLs must be installed into the prefix separately as below.
Dependencies for DXVK on Arch include `meson`, `ninja`, `glslang`, and
`mingw-w64-gcc`, in addition to Git.

After saving and closing all Wine applications, back up and install the two
64-bit DXVK DLLs (no sudo):

```bash
export WINEPREFIX="$HOME/.wine"
(
set -e
/usr/local/bin/wineserver -w
dxvk_build="$HOME/.cache/nexus-composition-dxvk-3.1.1/build"
dxvk_backup=$(mktemp -d "$WINEPREFIX/audion-dxvk-backup.XXXXXX")
cp -p "$WINEPREFIX/drive_c/windows/system32/d3d11.dll" "$dxvk_backup/"
cp -p "$WINEPREFIX/drive_c/windows/system32/dxgi.dll" "$dxvk_backup/"
install -m 644 "$dxvk_build/src/d3d11/d3d11.dll" "$WINEPREFIX/drive_c/windows/system32/d3d11.dll"
install -m 644 "$dxvk_build/src/dxgi/dxgi.dll" "$WINEPREFIX/drive_c/windows/system32/dxgi.dll"
printf 'Original DLL backup: %s\n' "$dxvk_backup"
)
```

This sequence assumes an existing 64-bit DXVK installation, as used for the
reported tests. If a backup command fails, stop before installing anything.
`wineserver -w` waits; it does not terminate or save applications. If you set
`AUDION_DXVK_DIR` for the build, adjust `dxvk_build` to match it.

Launch with explicit settings so a different working directory cannot silently
disable the required options:

```bash
DXVK_CONFIG='dxgi.enableDummyCompositionSwapchain = True; d3d11.enableContextLock = True' \
WINEDLLOVERRIDES='d3d11,dxgi=n;d2d1,dcomp=b' \
/usr/local/bin/wine "$WINEPREFIX/drive_c/Program Files/Image-Line/FL Studio 2026/FL64.exe"
```

Keep Nexus hardware acceleration on. To roll back DXVK, close Wine apps and
restore both DLLs from the printed backup directory. Do not mix snapshot and
non-snapshot components and assume the same tested rendering path.

## Verification gates

- GPU readback tests for bounds, nesting, opacity and geometry masks.
- Sprite drawing and transformed/tinted sprite coverage.
- Same Nexus DLL and saved settings, before/after, DXVK unchanged.
- FL Studio editor open, menus, resize/repaint and reopen, with acceleration on.
- All previous Audion patches preserved, including always-on Effectrix protection.

These checks establish the tested configuration, not universal compatibility.


## Presentation caching (2026-09-29)

The sixth DXVK patch caches an immutable completed-frame snapshot until the
next successful presentation or resize. The third Wine Nexus-rendering patch
reuses its per-visual bitmap, graphics state and read-only GDI DC. Unchanged
frames can repaint from that DC without another GPU readback. New RGBA frames
use Direct2D's cached conversion pipeline instead of compiling shaders and
creating conversion resources every tick. Read-only DC release uses an empty
dirty rectangle, avoiding an unnecessary upload back to the GPU. Frame pacing
subtracts composition time from the refresh budget using the performance counter.

These optimizations are active automatically with the existing paired snapshot
configuration above. No new opt-in setting, fsync requirement, realtime priority,
timer-resolution change or audio-thread modification is introduced. Normal build
and install commands above include these patches. Existing five-patch DXVK sources
upgrade in place; repeated builds preserve the patch stack and user index.

A matched native Wayland synthetic test measured approximately 6% less process CPU
time during animation and 38% less while idle. Animation wall time was essentially
unchanged. These are whole-test-process CPU measurements, not measured Nexus input
latency, compositor frame-rate measurements, or audio/xrun results. See
[verification](verification/2026-09-29-composition-performance.md).

One synchronous GPU readback remains for each newly composed frame, as does the
GDI blend into the HWND. This is not zero-copy presentation and does not establish
that Nexus routing-graph dragging now matches its software renderer. A GPU-only
compositor needs a separate design for alpha blending, child windows and fallback.
