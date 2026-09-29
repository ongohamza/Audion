# Composition performance verification — 2026-09-29

Environment: Wine 11.18 Staging/Audion, DXVK 3.1.1, AMD RX 5700-series,
KWin native Wayland. Candidate runs used an isolated prefix and `DISPLAY` unset.
Wine input fixes, PipeASIO configuration and kernel scheduling were unchanged.

## Regression evidence

- New repeated-acquisition test failed in all four BGRA/RGBA and two/three-buffer
  combinations on the previous DXVK stack. Candidate shares one immutable
  snapshot allocation until a new presentation or resize.
- Expanded composition suite: 76 assertions pass. Covers retained snapshots,
  no unpublished frames, resize, scroll/dirty rectangles, concurrent snapshots,
  swapchain destruction, BGRA/RGBA channels, window repaint after GDI erasure,
  content replacement, premultiplied/translucent and fully transparent pixels,
  and preservation of the application's immediate-context blend state.
- Direct2D layer/sprite regression checks pass (22 checks).
- Geometry stress: 3,000 frames, 360,000 draws and 177 resizes, successful exit.
- Allocation stress: 3,000 frames with 5,166,976 concurrent allocations,
  successful exit. Unmap locking/concurrent Map/Unmap checks pass.
- Native Wayland trace shows one readback for an unchanged frame per content
  lifetime, rather than every compositor tick; clearing and restoring content
  correctly allocates a new target and reads back again.
- Wine follow-up stack and DXVK five-to-six-patch upgrade tests cover repeat
  application, preserving the user's index and preflight rejection of conflicts.
  Builder argument checks and Wayland move-gating checks pass.
- Both x86_64 and i386 Wine dcomp binaries compile. Runtime GPU tests use x86_64.
- Legacy Wine 11.16 patch-application test could not run against the local 11.18
  shallow source: its pinned 11.16 commit is absent. The current 11.18 complete
  follow-up/core-preservation fixture passed. Public naming check passed.
- Read-only code review found no blocking lifetime, synchronization or state
  issues. Its requested translucent-pixel and context-state tests were added.

## Matched measurements

`tools/composition-d2d-stress.cpp 600 8` draws 120 moving geometries each frame,
resizes every 17 frames and requests an 8 ms pause between presentations.
Adding `idle` renders one frame and waits for the remaining iterations.
GetProcessTimes measures summed process user+kernel CPU time; QPC measures wall
 time, excluding initial device creation. Runs use baseline/candidate/candidate/
baseline order, with the matching Wine and DXVK binaries for each version.

| Native Wayland workload | Baseline CPU ms | Candidate CPU ms | Baseline wall ms | Candidate wall ms |
|---|---:|---:|---:|---:|
| Animated, run 1 | 1990 | 1870 | 5919.903 | 5937.145 |
| Animated, run 2 | 2090 | 1960 | 5990.895 | 5950.009 |
| Idle, run 1 | 520 | 320 | 4840.177 | 4841.275 |
| Idle, run 2 | 520 | 330 | 4839.300 | 4841.031 |

Mean process CPU reduction: 6.1% animated, 37.5% idle. Animated wall-time difference
is under 1%, too small to claim an improvement from this sample. This small local
benchmark is not a statistical hardware survey, Nexus input-to-display latency
measurement, displayed-frame count or proof of an audio latency improvement.

The user tested the caching candidate in Nexus and reported routing-graph dragging
unchanged. Its benchmark improvement must not be described as fixing that symptom.

## Remaining work

Each new frame still incurs GPU-to-CPU readback while holding the existing forced
immediate-context lock. Unchanged pixels still undergo GDI source-over blending
for host repaint compatibility. Existing repeated-alpha accumulation semantics
are preserved. Snapshot textures are immutable, so retained readers remain safe;
there is still one allocation per acquired new presentation. No claim of a native
GPU-only DirectComposition backend or full DirectComposition feature support.
