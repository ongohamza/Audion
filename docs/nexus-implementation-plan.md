# Nexus accelerated rendering implementation plan

Approved scope: retain DXVK and hardware acceleration; repair the rendering support required by Nexus, not claim full Windows DirectComposition conformance.

## Constraints

- Preserve existing OpenGL, FL signature, and always-on Effectrix patches.
- Wine 11.18 Staging; installation prefix /usr/local; compile with make -j"$(nproc)".
- Reuse the existing source/build tree; no extra test Wine installation.
- No killing or modifying the user's live FL session, no public upload of proprietary plugin files.
- Export independently applicable Wine patches into patches/nexus-rendering. Add patches/dxvk only if DXVK source changes are actually needed.

## Tasks and acceptance gates

1. Implement Direct2D layer/sprite functionality against failing GPU pixel tests, including layer geometry masks, nesting, opacity, and source/destination transform preservation. Inspect the existing Wine rendering pipeline before selecting implementation. Compile d2d1 only for initial testing. Never disguise unsupported behavior with unconditional success.
2. Test DLLs through a process-scoped override without replacing the system Wine while it is running. Compare Nexus before/after on the same accelerated rendering path. The dummy DXVK swapchain remains configuration, not a source patch.
3. Review implementation and limitations, then export patches and tests into Audion. Do not label Nexus fixed without visual confirmation in FL Studio and interactive reopen/menu checks.
4. Integrate the entire patch stack into version-pinned build/apply scripts. Test patch application on an unmodified baseline, test idempotence and failure paths, and correct all install instructions to /usr/local. Preserve user prefixes.

## Progress

- Baseline: GPU pixel test shows PushLayer clipping fails while axis-aligned clipping passes with both DXVK and Wine builtin D3D11. Nexus logs also exercise unimplemented bitmap-target sprite drawing.
- Direct2D candidate: 22 GPU checks pass; patch and probe exported. Geometry-mask antialiasing and background-initialized layers remain limited.
- Composition candidate: paired Wine/DXVK completed-frame snapshot support plus incremental-scroll correction exported. 61 GPU assertions and two unprotected-device rejection checks pass. Startup context locking is mandatory.
- Integration: Wine manifest includes all follow-ups; existing-tree updater preserves the index and rejects damaged core patches. DXVK builder applies both separately packaged patches against pinned v3.1.1. Script tests and DXVK build pass.
- Runtime: missing SwapDeviceContextState locking reproduced a graphics-command race. Entry locking passes the formerly crashing geometry stress and real Nexus resize probe. User confirmed stable FL Studio tabs, menus, resizing and reopening without flickering/crash. A separate Unmap lock-contract failure is also corrected and covered by a regression.
- No global Wine installation performed by this task; the existing build compiles successfully. Prefix DXVK candidate DLLs were replaced with originals retained in the private diagnostic backup. Install Wine using the documented /usr/local builder.
