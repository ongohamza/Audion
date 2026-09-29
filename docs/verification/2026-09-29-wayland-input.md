# Wayland input verification — 2026-09-29

## Live Nexus diagnosis

- FL64.exe loaded `/usr/local/lib/wine/x86_64-unix/winewayland.so`; the installed driver contained the automatic FL activation strings. No compatibility environment variable was set.
- Nexus ran in the FL process, inside `TPluginForm` → `TVST3Panel` → `JUCE_...`, with the JUCE editor focused and no mouse capture.
- Wrapper rectangle: `(1262,808)-(2529,1693)`; virtual desktop/clip rectangle: `(0,0)-(1920,1080)`.
- A mouse-move-only private driver input at `(2429,1593)` was preserved by `GetCursorPos`. At the identical point, `WindowFromPoint` returned NULL.
- The probe restored the previous pointer coordinates and did not click controls.
- A bounded, 45-second message observer was removed normally. No new user clicks were captured during its window, so it supplies no additional Nexus event-delivery evidence.

## Regression results

The test is `scripts/tests/wayland-input.c`. It calls Wine 11.18's private driver-input ABI, not Windows SendInput's normalized coordinate API. Its 32-bit branch supplies the native 64-bit INPUT layout expected by Wine's driver call thunk.

| Assertion | Preceding Wayland patches | With hit-test patch |
|---|---|---|
| Ordinary hardware is desktop-clipped | PASS | PASS (64-bit + WoW64) |
| Wayland mapped position stays outside desktop | PASS | PASS (64-bit + WoW64) |
| Child receives click at local `(160,140)` | PASS | PASS (64-bit + WoW64) |
| WindowFromPoint identifies that child | **FAIL: NULL** | PASS (64-bit + WoW64) |
| Point outside all windows returns NULL | PASS | PASS (64-bit + WoW64) |
| Default clip reset preserves mapped position | PASS | PASS (64-bit + WoW64) |
| Explicit full-desktop clip is respected | PASS | PASS (64-bit + WoW64) |
| Releasing explicit full-desktop clip restores mapping | PASS | PASS (64-bit + WoW64) |
| Smaller explicit clip is respected | PASS | PASS (64-bit + WoW64) |
| Ordinary hardware restores clipping | PASS | PASS (64-bit + WoW64) |
| Ordinary desktop hit test stays bounded | PASS | PASS (64-bit + WoW64) |

The helper-level move test, `scripts/tests/wayland-move-gating.py`, passes enabled/disabled mode, target class, same-size movement, visibility, real left-button state, and one request per press. It mocks compositor calls and does not establish compositor behavior.

## Build and patch integration

- Wine server and Wayland driver targets compile with the hit-test patch. The existing Audion tree also completed a full incremental build with all follow-ups; the final built runtime passed the 64-bit regression without compatibility/fsync/esync environment settings.
- Builder CLI/syntax and registration-path guards pass.
- Follow-up application is checked from the original Audion follow-ups, from the preceding Wayland stack, and on a repeated fully patched invocation. Overlapping follow-ups are peeled in reverse order in a private Git index before validation.
- The installed system runtime is not changed during these checks.

## Reproduce

Use a dedicated test prefix and the newly built Wine, with ordinary Wine applications closed if sharing any prefix. The following example uses a separate prefix below the current directory:

```bash
wine_build="$HOME/.cache/audion-wine-11.18-staging/wine-build"
mkdir -p .wayland-tests
x86_64-w64-mingw32-gcc -O2 -Wall scripts/tests/wayland-input.c \
  -o .wayland-tests/input64.exe -luser32
i686-w64-mingw32-gcc -O2 -Wall scripts/tests/wayland-input.c \
  -o .wayland-tests/input32.exe -luser32
for exe in input64.exe input32.exe; do
  env -u DISPLAY -u WINEFSYNC -u WINEESYNC WINEDEBUG=-all \
    WINEPREFIX="$PWD/.wayland-tests/prefix" \
    "$wine_build/wine" "$PWD/.wayland-tests/$exe"
done
python3 scripts/tests/wayland-move-gating.py \
  "$HOME/.cache/audion-wine-11.18-staging/wine-src/dlls/winewayland.drv/window.c"
```

These tests deliberately move the Wine cursor and restore it. Do not use them during audio performance measurements.

## Remaining validation

After installing and starting a fresh Wine server: open Nexus, test menus/presets/knobs and keyboard text entry, move its wrapper repeatedly, and repeat with previously working plugins. Actual Nexus UI confirmation, multi-monitor/fractional-scale behavior, and sustained PipeASIO xrun comparisons remain pending. The tested shared bug is repaired; compatibility with every plugin is not established.
