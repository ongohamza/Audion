# Nexus rendering verification — 2026-09-27

Environment: Wine 11.18 Staging with earlier Audion patches; DXVK v3.1.1;
Nexus 5.5.12; FL Studio 26.1.5; AMD RX 5700-series GPU on Arch Linux.
Hardware acceleration remained enabled. This is scoped compatibility work.

## Evidence progression

- Original DXVK rejects CreateSwapChainForComposition; dummy composition
  initialization alone leaves controls missing.
- Unpatched Direct2D fails 19 primitive checks. Wine layer/sprite patch passes
  22 checks on both DXVK and Wine's builtin D3D11.
- Original composition path samples an unpresented buffer. The paired immutable
  snapshot path passes 61 pixel/lifetime/resize/concurrency assertions. A separate
  scroll correction resolves a failing incremental-frame test.
- First live candidate renders but flickers/crashes. DXVK SwapDeviceContextState
  bypasses its context lock. Geometry stress crashes after frame 0; entry locking
  passes 3,000 frames, 360,000 draws and 177 resizes. A real Nexus probe improves
  from a crash after five forced resizes to 162 resizes and normal exit.
- User confirms the three-patch DXVK candidate (`bca2e81e`) stable in FL Studio
  across tabs, menus, resizing and reopening, without flickering/crashes.
- Review identifies Unmap's pre-lock counter read. A lock-contract test fails
  before entry locking and passes afterward, including 1,000 concurrent map cycles.
- Extended real Nexus testing of the four-patch candidate (`0b91a345`) crashes
  in allocation-pool handling. Imports bypass the mutex protecting the shared
  allocation free list. Concurrent allocation/presentation stress crashes after
  frame 0, then passes 3,000 frames with 4,714,176 concurrent allocations after
  import-path locking. This is not a claim to fix every sparse allocation path.
- Final five-patch candidate passes the 61 pixel checks, Unmap contract tests,
  and three real Nexus stress runs: 162 + 163 + 161 = 486 forced resizes, all
  normal exits. Automated resizes target the plugin child window; they are not
  a substitute for the separately recorded FL Studio interaction test.

## Final binary identities

```
d3d11.dll 9f4da2ca9fcd6eed71470138a424b95198164886c19305947242fea9ad301063
dxgi.dll  43eb4cf34db9c11a740786c733db9778b434b29f3e2c099220a147e74a812d40
```

The patch-application test reconstructs the pinned DXVK base and verifies that
all five patches produce the tested source files. Wine follow-up tests cover
application, repeat application, preservation of a staged index and rejection
of damaged original patches. Both build scripts use all available processors.

## Limitations

No universal Direct2D/DirectComposition conformance claim. Mask edges retain
Wine's aliased rendering; background-initialized layers are explicitly unsupported.
Snapshot allocations/readback retain the existing Wine presentation approach;
this is not a native zero-copy DirectComposition implementation. Wine's HWND
alpha-accumulation behavior is unchanged. 32-bit runtime behavior, all hardware,
long-duration performance and every plugin are not established. Audio routing,
presets and licensing were not part of this rendering acceptance test.

Regression sources are in `tools/`. Private logs, screenshots and proprietary
Nexus files are intentionally not included in the repository.
