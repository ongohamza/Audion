# FL custom main-window resizing

The recording and live native Wayland trace show border resizing changing the
main window's Win32 origin through SetWindowPos, without SC_SIZE or an
xdg_toplevel.resize request. The physical Wayland origin stays put. Surface
coordinates become negative during the left-edge drag, Wine clamps them to the
moving logical edge, and the application repeatedly shrinks/grows by an
incorrect amount. This is distinct from missing menu placement.

Translate a visible normal TFruityLoopsMainForm edge gesture into one native
compositor resize per physical left press. Require the initiating press near
the changed edge and the opposite edge anchored; exclude programmatic, hidden,
iconic/maximized and compositor-configure resizes. End FL's own drag through
the existing synthetic release pattern used for plugin movement. Advertise
resizability for the borderless main window so fixed native size hints do not
block the request. Preserve earlier menu, pointer, plugin, restoration and drop
fixes. No audio, ntsync, scheduler, kernel, buffer or timer changes.

Verify helper gating for all edges/corners and fractional scaling; replay the
real FL failure and test menus/restoration; rerun native geometry/input and
existing driver regressions. Review, export as next default Audion patch,
verify cumulative application and publish to Audion main after verification.

The first live candidate fixes resizing, but the user cannot move the main
window. Its trace shows FL repeatedly moving outside the desktop while the
menu fix corrects it back, without an xdg_toplevel.move request. Extend the
existing plugin move handoff to the main class, excluding rebase operations,
active compositor configurations and gestures already handed off as resizing.
Preserve both corner axes even when the first size delta changes only one.
