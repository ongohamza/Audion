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


## Final GPU and geometry candidate

The final native Wayland FL Studio test used the automatic Nexus GPU path, the
hidden-producer pacing fix, blocking vblank waits, and both geometry indexes.
Temporary D2D phase timing was removed before this test. The user reported
“fixed, looks good.” This is a live subjective result, not a measured end-to-end
input-latency number or a comparison across all plugins.

Measured causes and separate checks:

- Before the hidden-producer correction, 30 presentations with requested v-sync
  took 1,403 ms (ordinary chain) or 8,617 ms (waitable chain), versus 9/4 ms with
  immediate presentation. Afterward both hidden variants took approximately
  3 ms. A normal visible HWND still took 469 ms for 30 requested-vsync frames.
- CPU sampling identified 21% of sampled process CPU in DXVK's busy-wait loop.
  120 output vblank waits took 2,000 ms wall / 800 ms CPU before the blocking
  wait, versus 2,000 ms / 70 ms afterward. The user reported no subjective graph
  improvement from this change alone.
- Dense hollow polylines (ten 4,000-vertex paths) took 164 ms before the segment
  index and approximately 7 ms afterward. Filled versions took 195 ms after the
  segment index, then 50 ms with the fill-edge index. These are synthetic CPU
  geometry timings on this host, not plugin frame times.
- 206 deterministic path cases with lines, quadratic curves, multiple figures,
  winding/alternate fill, self-intersections, repeated points, multiway crossings,
  and nearly tangent curves at tiny/unit/large coordinate scales produced byte-identical
  tessellation output against the previous implementation.
- Clean final D2D/Dcomp binaries pass the 76 composition assertions and 22
  layer/sprite checks. No diagnostic D2D timing instrumentation is shipped.
- The automatic GPU policy test uses a JUCE window owned by a test PE named
  `Nexus.vst3`, with no `WINE_DCOMP_GPU` override. It completed 3,000 frames,
  360,000 geometry draws, 177 resizes and repeated content replacement. Tracing
  confirmed 205 target creations and 1,787 GPU presentations.
- Wine and DXVK patch-upgrade tests pass repeated application, existing-index
  preservation and conflict preflight. The reconstructed DXVK source matches
  the tested source exactly.
- Final allocation stress completed 3,000 frames with 5,431,872 concurrent
  allocations. The full-build rerun passes all 76 composition assertions,
  concurrent Unmap checks, and rejection of unprotected contexts.
- Generic premultiplied-alpha windows stayed on GDI even with the explicit GPU
  test override. The automatic Nexus policy trace contains no CPU readbacks.

The earlier cache-only CPU benchmark above does not describe this final stack.
The real Nexus test covers this user's AMD/Wayland setup; it does not establish
32-bit runtime behavior, all GPUs, complete DirectComposition conformance, or
realtime audio/xrun performance. The full Wine build completed successfully for both enabled architectures;
DXVK also built successfully. The independent legacy Wine 11.16 fixture remains unavailable in these
shallow source checkouts; the 11.18 follow-up stack is the tested target.
