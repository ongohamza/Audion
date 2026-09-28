# DXVK composition candidate (development)

Base: DXVK **v3.1.1**, commit `b1a1c99ab52b687cf950d62c88bc2fa316b41663`.
These are DXVK patches, not Wine patches. Apply in filename order:

1. `0001-nexus-composition-snapshot.patch`: exposes an independently owned
   snapshot of the last completed frame through a private interface shared
   with Audion's Wine dcomp patch. Requires startup context protection.
2. `0002-incremental-scroll.patch`: copies the complete saved previous-frame
   scroll rectangle once, then applies the dirty rectangles.
3. `0003-lock-context-state-swap.patch`: protects SwapDeviceContextState with
   the immediate-context lock, preventing Direct2D state changes from racing
   composition commands. The previous candidate crashed the geometry stress
   test after its first frame; this change passes 3,000 frames and 177 resizes.
4. `0004-lock-unmap-entry.patch`: locks before reading shared mapped-image
   state in Unmap. A lock-contract regression fails before this change and
   passes afterward; it is a separate review finding, not the proven crash cause.
5. `0005-lock-import-allocation-pool.patch`: protects resource-import allocation
   against normal allocation/free on other threads. The concurrent-allocation
   test crashes after frame 0 without it and passes 3,000 frames with it.

Build from the Audion repository with:

```bash
bash scripts/build-dxvk-nexus.sh
```

The script pins source/submodules, applies all five patches, and uses
`ninja -j"$(nproc)"`. It builds 64-bit DLLs without installing them.
Wine still uses `make -j"$(nproc)"`; its installation prefix is `/usr/local`.
See `docs/NEXUS_RENDERING.md` for required settings and current limitations.

The context-state lock fixed the reproduced stress/resize crashes, and the
user confirmed no flickering or crash in FL Studio. See the verification
scope and limitations in the Nexus instructions; this is not universal
DirectComposition support.
