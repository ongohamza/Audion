# FL main-window menu coordinates

Native Wayland trace reproduces a visible normal main window with raw Windows
rect (-1256,1064)-(32,2098). FL clamps popup coordinates into Wine's virtual
desktop and Wine places those popups hundreds of pixels away from the parent.
The native regression fails parent and child-anchor checks before the change.

Keep the logical origin of fitting, normal, visible TFruityLoopsMainForm windows
inside the raw virtual desktop under the existing default FL compatibility.
Defer position-only normalization to the application thread, coalesce messages,
and suppress recursive normalization during the correction. Re-check live state.
Preserve hidden/iconic/maximized/plugin/oversized windows; reject changes that
would newly cover a monitor and trigger Wine's fullscreen heuristic. Monitor
queries run in a temporary per-monitor-aware context to match raw coordinates.
Refresh the focused pointer from cached native surface coordinates, immediately
flushing on the application thread; do not warp the native cursor.

Verify native geometry/child anchors, negative non-FL/plugin/hidden/oversized
cases, fullscreen avoidance, minimize/restore, stationary clicks, both bitnesses,
fractional scale, live FL menus and existing driver regressions. Review the final
change, export patch, verify cumulative application and publish after live test.
No audio path, timer, scheduling, PipeASIO or synchronization setting changes.
