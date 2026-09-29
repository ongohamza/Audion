# FL Studio native Wayland input

The Wine 11.18 build includes these patches by default:

1. `patches/wayland/0001-flstudio-wayland-input-and-move.patch`: preserve driver mouse coordinates outside the Windows virtual desktop, preserve application-requested cursor clips, and translate FL's custom plugin-wrapper moves into compositor moves.
2. `patches/wayland/0002-enable-flstudio-compatibility-by-default.patch`: enable compatibility automatically for `FL64.exe` and `FL.exe`. No launch environment variable is required. `WINE_WAYLAND_FLSTUDIO=0` is an optional diagnostic escape hatch.
3. `patches/wayland/0003-match-window-hit-tests-to-wayland-input.patch`: allow the standard window hit-test traversal beyond the desktop rectangle when the active driver input uses the compatibility mapping.

Use the [Wine + Audion + PipeASIO build instructions](BUILD_WINE_11.18.md). The manifest includes all three patches for both fresh and updated builds. Source patching and compilation alone do not update installed Wine. The installer refuses to replace a running Wine session.

## Why Nexus needed the third patch

In the observed Nexus session, the native Wayland plugin remained visible, but its Windows wrapper rectangle was `(1262,808)-(2529,1693)` on a `(0,0)-(1920,1080)` virtual desktop. A non-button driver-input probe submitted `(2429,1593)` and received the same cursor coordinates, proving that the first coordinate fix was active. However, `WindowFromPoint` at that point returned NULL.

Wine's server rejected the point against the desktop window before searching its children. JUCE's Windows `HWNDComponentPeer::contains()` uses `WindowFromPoint` to check which native window owns the point. That creates a second obstacle: a delivered mouse event can have valid local coordinates while the plugin's ownership check says that no window is there.

The third patch retains the normal child-window traversal for these otherwise-rejected points. It preserves child visibility, window regions, z-order, and the normal `WM_NCHITTEST` processing. Points outside every window still return NULL. Ordinary input without the Wayland compatibility flag keeps the original desktop boundary behavior. This is a shared window-query fix, not a Nexus DLL modification or per-plugin whitelist.

The framework explanation is based on [JUCE's Windows peer implementation](https://github.com/juce-framework/JUCE/blob/master/modules/juce_gui_basics/native/juce_Windowing_windows.cpp). Nexus's exact private JUCE build has not been source-audited. The live API mismatch and regression are directly observed; final interactive Nexus validation remains necessary after installing the updated server.

## Verification and limits

The coordinate regression reproduces the gap on the preceding patch set: pointer coordinates and child click delivery pass, but out-of-desktop `WindowFromPoint` returns NULL. The updated test also checks no-window results, cursor clip/reset/release behavior, and restoration of ordinary desktop-bound hit tests. See the [verification record](verification/2026-09-29-wayland-input.md).

The wrapper move translation applies to `TPluginForm` during a physical left-button gesture and sends one compositor move request per press. Custom resizing is not implemented by this patch. Compositor-rejected moves, multiple monitors, fractional scaling, bridged plugins, keyboard text entry and every third-party plugin have not all been validated. This work addresses the identified shared failures; it is not a guarantee that every plugin UI is fully compatible with Wine Wayland.

No audio callback, PipeASIO code, background polling loop, scheduler policy, or kernel setting is changed. Configuration is read once at process initialization. Keep verbose tracing off for audio work. Realtime audio performance must be measured with the same project, device, sample rate and buffer size before/after. The development host used ntsync and a dynamically preemptible CachyOS BORE kernel; PREEMPT_RT performance is unmeasured.

## Source provenance

The original coordinate workaround adapts Alexandros Frantzis's draft [Wine MR7937](https://list.winehq.org/hyperkitty/list/wine-gitlab@list.winehq.org/thread/VSHC65GFGVYSB23BCIPE62NZESKEKYKT/), with the flag adjusted for Wine 11.18, default FL activation, explicit clipping and reapplication handling, and custom move integration. These are Audion compatibility patches, not upstream Wine acceptance claims.
