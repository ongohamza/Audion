# FL Studio native Wayland input

The Wine 11.18 build includes these patches by default:

1. `patches/wayland/0001-flstudio-wayland-input-and-move.patch`: preserve driver mouse coordinates outside the Windows virtual desktop, preserve application-requested cursor clips, and translate FL's custom plugin-wrapper moves into compositor moves.
2. `patches/wayland/0002-enable-flstudio-compatibility-by-default.patch`: enable compatibility automatically for `FL64.exe` and `FL.exe`. No launch environment variable is required. `WINE_WAYLAND_FLSTUDIO=0` is an optional diagnostic escape hatch.
3. `patches/wayland/0003-match-window-hit-tests-to-wayland-input.patch`: allow the standard window hit-test traversal beyond the desktop rectangle when the active driver input uses the compatibility mapping.

4. `patches/wayland/0004-release-desktop-pointer-confinement.patch`: release desktop-wide native pointer confinement for visible FL plugin cursors, while retaining hidden-cursor knob input and narrower clips. Automatic fullscreen clipping also preserves the driver's logical coordinates.

5. `patches/wayland/0005-restore-minimized-windows-and-import-file-drops.patch`: restore minimized windows when the compositor reactivates them, including FL's iconic window geometry, and import local file drops from Dolphin/desktop through Windows drag-and-drop handling.
6. `patches/wayland/0006-keep-main-window-menu-coordinates-on-desktop.patch`: keep the normal main window's logical origin inside the desktop so FL's dropdown placement remains consistent after moving/resizing.
7. `patches/wayland/0007-handle-main-window-custom-move-and-resize.patch`: translate FL main-window border resizing and custom movement into compositor gestures, preserving corner resizing and the menu-coordinate fix.
8. `patches/wayland/0008-preserve-popup-stacking-over-child-gpu-surfaces.patch`: preserve popup stacking above GPU-rendered child windows, fixing Serum 2 preset menus.

Use the [Wine + Audion + PipeASIO build instructions](BUILD_WINE_11.18.md). The manifest includes all five patches for both fresh and updated builds. Source patching and compilation alone do not update installed Wine. The installer refuses to replace a running Wine session.

## Why Nexus needed the third patch

In the observed Nexus session, the native Wayland plugin remained visible, but its Windows wrapper rectangle was `(1262,808)-(2529,1693)` on a `(0,0)-(1920,1080)` virtual desktop. A non-button driver-input probe submitted `(2429,1593)` and received the same cursor coordinates, proving that the first coordinate fix was active. However, `WindowFromPoint` at that point returned NULL.

Wine's server rejected the point against the desktop window before searching its children. JUCE's Windows `HWNDComponentPeer::contains()` uses `WindowFromPoint` to check which native window owns the point. That creates a second obstacle: a delivered mouse event can have valid local coordinates while the plugin's ownership check says that no window is there.

The third patch retains the normal child-window traversal for these otherwise-rejected points. It preserves child visibility, window regions, z-order, and the normal `WM_NCHITTEST` processing. Points outside every window still return NULL. Ordinary input without the Wayland compatibility flag keeps the original desktop boundary behavior. This is a shared window-query fix, not a Nexus DLL modification or per-plugin whitelist.

The framework explanation is based on [JUCE's Windows peer implementation](https://github.com/juce-framework/JUCE/blob/master/modules/juce_gui_basics/native/juce_Windowing_windows.cpp). Nexus's exact private JUCE build has not been source-audited. The live API mismatch and regression are directly observed; final interactive Nexus validation remains necessary after installing the updated server.

## Verification and limits

The coordinate regression reproduces the gap on the preceding patch set: pointer coordinates and child click delivery pass, but out-of-desktop `WindowFromPoint` returns NULL. The updated test also checks no-window results, cursor clip/reset/release behavior, and restoration of ordinary desktop-bound hit tests. See the [verification record](verification/2026-09-29-wayland-input.md).

The wrapper move translation applies to `TPluginForm` during a physical left-button gesture and sends one compositor move request per press. That first patch does not implement custom resizing; the seventh patch adds main-window resizing. Compositor-rejected moves, multiple monitors, fractional scaling, bridged plugins, keyboard text entry and every third-party plugin have not all been validated. This work addresses the identified shared failures; it is not a guarantee that every plugin UI is fully compatible with Wine Wayland.

No audio callback, PipeASIO code, background polling loop, scheduler policy, or kernel setting is changed. Configuration is read once at process initialization. Keep verbose tracing off for audio work. Realtime audio performance must be measured with the same project, device, sample rate and buffer size before/after. The development host used ntsync and a dynamically preemptible CachyOS BORE kernel; PREEMPT_RT performance is unmeasured.

## Source provenance

The original coordinate workaround adapts Alexandros Frantzis's draft [Wine MR7937](https://list.winehq.org/hyperkitty/list/wine-gitlab@list.winehq.org/thread/VSHC65GFGVYSB23BCIPE62NZESKEKYKT/), with the flag adjusted for Wine 11.18, default FL activation, explicit clipping and reapplication handling, and custom move integration. These are Audion compatibility patches, not upstream Wine acceptance claims.

## Pointer confinement fix

A native Wayland trace showed a full-desktop clip `(0,0)-(1920,1080)` being
applied to a smaller plugin surface. Wayland intersected the clip with that
surface, trapping the pointer inside the plugin. The fourth patch is automatic
under the existing FL compatibility detection; no new environment setting or
per-plugin name list is required.

The visible cursor can now leave desktop-wide native constraints. Hidden
cursor locking for relative knob gestures and narrower native clips remain.
On the server, only automatic clips covering the full virtual desktop are
bypassed for compatibility coordinates. Explicit Win32 clips and narrower
automatic monitor clips retain their coordinate limits. See the
[pointer verification record](verification/2026-09-29-pointer-confinement.md).

## Restoration and external file drops

The fifth patch restores FL Studio's normal window when clicking its taskbar
entry after minimization. It tracks compositor activation independently of
geometry acknowledgments, so delayed or coalesced configure events do not leave
FL as a tiny menu strip. No new custom edge-resize translation is introduced.
The live menu test worked during diagnosis; the demonstrated restoration bug
is repaired and the user confirmed the combined candidate test.

Dolphin and desktop drops now import local file paths through Wine's existing
OLE/WM_DROPFILES bridge. Incoming copies into the Sampler and Playlist use
that shared application path. File-transfer IO runs off the Wayland event and
application threads; transfers are limited to 1 MiB of URI metadata and three
seconds. This limit applies to the list of paths, not the audio file sizes.
The worker waits when idle and does not poll or change thread priorities.

This adds incoming local-file copies, not outgoing drags, remote downloads or
OLE hover previews. Existing clipboard handling is preserved. See the
[verification record](verification/2026-09-29-wayland-restore-file-drops.md).

## Main menus after resizing

The sixth patch addresses a separately reproduced dropdown-placement failure.
FL's custom movement can leave the main window's Windows coordinates outside
Wine's virtual desktop while the Wayland compositor still displays it. FL then
clamps its dropdown into the desktop and Wine places that popup far away from
its visible parent.

Under the existing automatic FL compatibility, the driver corrects the logical
origin of a normal visible `TFruityLoopsMainForm` after movement or resizing.
It preserves the size and lets Win32 update child coordinates. Native placement
remains controlled by the compositor; this does not recover global Wayland
window coordinates. Cached surface-local input is reprojected so a stationary
pointer remains accurate, including when an old motion frame is still queued.

Plugin, hidden, minimized, maximized and oversized windows keep their positions.
The correction also skips cases that would turn a normal window fullscreen or
where raw and emulated virtual desktop bounds differ. This is a focused repair
for fitting normal FL main windows, not general placement support for every
window/layout. See the [verification record](verification/2026-09-29-wayland-main-menu-coordinates.md).

## Main-window movement and resizing

FL's borderless main window resizes by changing its Windows rectangle directly.
Under native Wayland, changing that logical origin does not move the native
surface. Dragging the left/top edge can therefore feed incorrect coordinates
back into FL's next size calculation and make the window shrink or grow
incorrectly.

The seventh patch hands actual main-window border gestures to the compositor.
It remembers the physical press location, verifies the changed edge, and sends
one native resize request per press. Corners retain both axes even when the
first motion changes only one dimension. The normal borderless main window is
advertised as resizeable. Ordinary programmatic resizes and other window
classes do not use this handoff.

Main-window movement also uses the existing plugin-move handoff. Corrective
menu-position changes and active compositor resizes cannot start a main-window
move. FL's own drag is ended using the same synthetic release already used for
plugin movement. The user confirmed that main movement, resizing and menus
work together. See the [verification record](verification/2026-09-29-wayland-main-window-resize.md).

## Serum 2 preset menus

Popup geometry updates previously lowered Serum's menu beneath its GPU editor
when that editor belonged to a child HWND. The driver now preserves the native
stacking order established when the menu subsurface is created. Nested menus
remain above earlier menus, and the GPU clients retain their relative order.
This applies by default without a Serum-specific setting and adds no per-frame
work. The user confirmed rendering and preset selection. See the
[verification record](verification/2026-10-01-wayland-serum-popup-stacking.md).
