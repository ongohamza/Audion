# FL plugin pointer confinement

The user requested a permanent Audion fix, live validation, publication and
updated Wine/PipeASIO build commands. Existing isolated Audion and Wine worktrees
were reused.

1. Reproduce on native Wayland and trace clip rectangles. Completed: a desktop
   clip becomes a plugin-sized native constraint.
2. Preserve narrower and hidden-cursor constraints while releasing desktop-wide
   visible FL constraints. Completed, with a failing-then-passing backend test.
3. Validate related server coordinate behavior. Completed: distinguish explicit
   clipping from automatic full-desktop clipping in both clamp sites.
4. Build, exercise native 64-bit/WoW64 tests and obtain live plugin confirmation.
   Completed; see the pointer-confinement verification record.
5. Append the patch to the default manifest, verify upgrades, publish to Audion
   main, and provide the existing cumulative Wine/PipeASIO installer commands.

No audio processing, kernel tuning or synchronization backend change is in scope.
