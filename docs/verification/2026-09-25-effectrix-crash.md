# Effectrix editor crash: cause, protection and tests

## Finding

Effectrix's GUI constructor passes uninitialized brush/pen fields to
`gdi32!DeleteObject`. Wine's legacy short-handle expansion can interpret those
values as another component's live GDI object. In the captured failure it
deleted FL Studio's memory DC, then its DIB bitmap. FL subsequently wrote to
the freed bitmap pixels and crashed. This is separate from the OpenGL flicker.

## Causal evidence (local debugger and allocation traces)

- Effectrix module base: `0x6ffff33c0000`; constructor: module + `0x8b5d0`.
  Fields at object + `0x80` and + `0x88` were read without initialization;
  DeleteObject return addresses were module + `0x8b7af` and + `0x8bb33`.
- One raw argument was `0x0000018c00000190`, matching packed stack rectangle
  coordinates, not a complete brush/pen handle. Wine normalized its short
  index to FL's live memory DC `0x0e410190`.
- That DC selected bitmap `0x07090191`: 1064 x 23 pixels, 4256-byte stride,
  pixel storage `0x05c50000`. A subsequent bad cleanup deleted this bitmap.
- Trace timestamps: creation `27103.297`; DC deletion `27111.945`;
  bitmap deletion and `NtFreeVirtualMemory(..., MEM_RELEASE)` `27112.021`.
  Within approximately 4 ms FL's pixel-copy routine wrote to that freed address.
- Captured fault: `dsp_ippv2_x64.dll + 0x189b9ba`, `vmovdqu ymm0,(rdx)`.
  FL's renderer object still contained the same bitmap pointer, dimensions and
  stride. Other captures failed in pixel-fill/store paths, not always at this
  exact instruction.
- A debugger intervention that rejected only malformed deletion calls allowed
  the run to continue; 5,839 such calls were rejected. This, together with the
  allocation/free trace and isolated regression, identifies the causal chain.

Private cores, raw logs and the project are not published: they can contain
personal data. Local evidence lives in WineDAWPatches/diagnostics/effectrix-2026-09-24,
especially capture.vOiKT4. A historical mid-instruction debugger breakpoint
caused an artificial fault; that event is explicitly excluded from this evidence.

## Patch behavior and limitations

`WINE_GDI_STRICT_DELETEOBJECT=1` enables a process-local guard, read once when
gdi32 loads. Before `get_full_gdi_handle`, `DeleteObject` rejects non-null
arguments whose lower 32 bits have no type/generation word (`!HIWORD(obj)`).
It returns FALSE rather than allowing a legacy short alias to destroy a live
object. Normal full handles, null handling and stock-object behavior remain.
Junk in the upper 32 bits of an otherwise complete handle is still ignored,
as required by existing Wine tests.

This is an **opt-in defensive workaround**, not a repair to the plugin binary
or a claim about native Windows conformance. Default Wine behavior is unchanged.
Applications intentionally deleting objects through old short handles may be
incompatible with this mode, so do not enable it globally. It does not prevent
every possible invalid-handle bug (for example, a random value matching a full
valid handle). No claim is made that every crash in this plugin has this cause.

## Verification

- Full Wine build succeeded with `make -j"$(nproc)"`, x86_64 and i386, on the
  existing Wine 11.16 Staging tree with both prior Audion fixes.
- Isolated malformed-handle regression fails on installed pre-patch Wine;
  strict mode on the new build passes, and explicit legacy mode passes its
  opposite expectations. It covers bitmap/DC/brush survival, full-handle
  cleanup, high-bit aliases, null and stock objects.
- Existing Wine gdiobj tests: 170 tests, 4 todo, **0 failures** in strict mode.
- OpenGL client-surface ordering regression: **PASS**.
- FL signature check: original engine `00000000`; tampered engine `80096010`
  (bad digest), so validation remains enforced.
- Live compiled-Wine test: Effectrix editor playback, dragging, close/reopen
  for 2 minutes 8 seconds, no access violation, normal process exit.
  PipeASIO was mapped from the existing /usr/local installation. An xrun was
  logged; these checks do not certify dropout-free audio or long-term stability.
- A second compiled-Wine run lasted 2 minutes 22 seconds, including playback,
  editor dragging and reopening, with no access violation and normal exit (0).
  GetGuiResources returned unavailable zero counts; leak freedom is not verified.
- A later unprotected GUI run did not crash during its short observation;
  the GUI trigger is intermittent. The isolated deletion regression is deterministic.
- All four published patches apply cleanly in order to pinned Wine 11.16.

## Run the regression yourself

```bash
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra tools/strict-deleteobject-test.c \
  -o /tmp/audion-deleteobject-test.exe -lgdi32
WINE_GDI_STRICT_DELETEOBJECT=1 /path/to/patched/wine /tmp/audion-deleteobject-test.exe
WINE_GDI_STRICT_DELETEOBJECT=0 /path/to/patched/wine /tmp/audion-deleteobject-test.exe --legacy
```

Both commands should report zero failures. Running the first command with
pre-patch Wine must fail: the malformed arguments delete the probe's own objects.

To enable protection for FL, prepend `WINE_GDI_STRICT_DELETEOBJECT=1` to its
Wine launch command. Merely applying/building/installing the patch does not
enable it. PipeASIO is a separate driver and is not included in this patch set.
