# Serum 2 popup stacking — verification

Serum 2's preset menu was invisible in native Wayland but worked under X11.
The baseline trace shows Wine creating and painting its standard Windows popup,
then placing its native subsurface immediately above the parent surface. The
GPU editor is another subsurface of that parent, owned by a child HWND. The
parent's `client_surface` field therefore does not identify it, and the menu
was placed below the opaque editor.

The fix removes explicit restacking from popup geometry updates. Wayland adds
new subsurfaces above their siblings; preserving that order keeps the menu and
new nested menus above the editor. Client geometry updates already place GPU
surfaces immediately above the parent, below existing popups. Hiding and
reshowing a popup recreates its subsurface. Positioning and parent commits are
unchanged. This is not a general implementation of all Win32 sibling z-order
changes on Wayland.

## Evidence

- The production-function protocol-order harness fails the child-GPU and
  multiple-GPU popup checks before the fix and passes all 12 checks afterward.
  It covers native client ordering, higher popups, direct owner clients,
  GDI-only owners, fractional positioning, configure consumption and absent
  owners. It runs with address/undefined-behavior sanitizers. Leak detection
  was disabled in the restricted test runner because ptrace prevents LSAN;
  the changed function performs no allocation.
- A real Win32 application creates an OpenGL editor in a child HWND, opens a
  root popup and nested submenu, and selects its test item. Both x64 and WoW64
  runs pass in an isolated KWin Wayland session. Protocol traces show the GPU
  child and both menus attached to the same native parent, without lowering
  the menus during geometry updates. This is protocol and selection evidence,
  not an automated compositor pixel assertion.
- Full incremental Wine compilation passes. All 11 existing pointer-refresh
  checks pass. Cumulative patch application, repeat application, staged-index
  preservation, conflict preflight, builder CLI and public naming checks pass.
- The final FL Studio/Serum test used hardware GPU access and the existing
  paired DXVK build. Root and nested menu creation are recorded in the trace.
  The user confirmed rendering and preset selection: “ok now it works”.

An earlier test launch ran in a sandbox without `/dev/dri`; DXVK failed to
initialize and Serum was white. That run was invalid for judging the patch.
The test prefix now has private copies of its Image-Line and Xfer documents,
so testing does not require writes to the user's regular Documents folder.

The correction is enabled by the default Audion Wine followup manifest. It
adds no scanning, allocation, timer, audio-thread or synchronization changes.
No PipeASIO/DXVK rebuild or configuration change is required specifically for
this fix; the standard Wine builder still installs the cumulative Wine patches
and PipeASIO.

## Reproduce the checks

```bash
python3 scripts/tests/wayland-popup-stacking.py /path/to/wine/dlls/winewayland.drv/wayland_surface.c
x86_64-w64-mingw32-gcc -Wall -Wextra -Werror scripts/tests/wayland-popup-gpu.c -o /tmp/wayland-popup-gpu.exe -lopengl32 -lgdi32 -luser32
```

Run the executable with the patched Wine in an isolated native Wayland prefix
with hardware GPU access. Repeat using the i686 compiler for WoW64. Use a
20-second timeout to bound a menu-loop failure. The executable creates its own
window and closes it after selecting the test menu item.
