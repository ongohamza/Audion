# Nexus rendering — Wine patches

The combined Wine/DXVK candidate passed the Nexus FL Studio interaction test:
the user reported no flickering or crash after the context-state lock fix.
The Direct2D primitive patch also passes 22 pixel/error checks. See
`docs/NEXUS_RENDERING.md` for build instructions and the limits of validation.

`0001-d2d1-gpu-layers-and-sprites.patch` adds GPU-backed bitmap layers with
nested group opacity, geometry masks, brush snapshots, bounds and recovery,
plus sprite drawing with tint, transforms, and atlas clamping. It applies to
Wine 11.18 Staging after the original Audion patches. It is not a DXVK patch.

`0002-dcomp-completed-frame-snapshot.patch` consumes a stable completed-frame
snapshot supplied by the paired DXVK patches instead of the in-progress buffer.
Both components are required; apply DXVK patches to DXVK separately.

The combined Wine builder consumes `manifests/wine-11.18-followups.list`.
Use `--update-existing` for an existing prepared Audion 11.18 build; do not
manually reapply the original four-patch stack.

## Limits

- Per-primitive mask antialiasing inherits Wine's aliased geometry rendering.
- `INITIALIZE_FROM_BACKGROUND` returns `E_NOTIMPL`; it is not silently accepted.
- Full-target intermediate GPU allocations occur per layer. Broad performance,
  leak, and 32-bit runtime conformance are not established.
- These are Nexus-focused compatibility changes, not full Direct2D or
  DirectComposition conformance.

## Reproduce the primitive checks

From the Audion repository, compile the standalone probe:

```bash
x86_64-w64-mingw32-g++ -static -O2 tools/d2d-layer-probe.cpp -o /tmp/audion-d2d-layer-probe.exe -ld2d1 -ld3d11 -ldxguid
WINEDEBUG=-all DXVK_LOG_LEVEL=none /usr/local/bin/wine /tmp/audion-d2d-layer-probe.exe
```

Use the existing Wine build tree's `wine` executable instead of
`/usr/local/bin/wine` before installation. With installed unpatched Wine the
baseline failed 19 checks; with the candidate, 22/22 pass on both DXVK 3.1.1
and Wine's builtin D3D11. A second backend run uses
`WINEDLLOVERRIDES='d3d11,dxgi=b'` for that process only.

Hardware-accelerated drawing stays enabled. Wine's existing GDI-based
composition presentation still involves readback; this is not a zero-copy
GPU compositor. Application-level validation covered tabs, menus, resizing and
editor reopening in FL Studio; this is not a guarantee for other plugins.
