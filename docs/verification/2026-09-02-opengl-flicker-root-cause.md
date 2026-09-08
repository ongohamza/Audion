# Hosted OpenGL/VST Flicker Root-Cause Record

## Scope

This record compares a working standalone OpenGL plug-in renderer with the same
VST3 editor flickering inside FL Studio 26.1.5.5618 under the pinned private
Wine-Staging 11.16 runner. The evidence gate passed: compositor frames, Wine
debug events, and decoded X11 protocol traffic identify the same client-surface
presentation-order failure.

The application names are not relevant to the implementation. The responsible
Wine invariant applies to any overlapping offscreen client surface composited
into a shared top-level drawable.

## Reproduction protocol

Every run used the same Wine prefix, display, GPU, screen geometry, plug-in
state, and interaction sequence. The sequence contained five note triggers,
five control movements, and ten seconds idle. The hosted case contained exactly
one open OpenGL VST3 editor.

## Experiments

| Run | Capture mode | Complete | Visible outcome | Trace outcome | Interpretation |
|---|---|---|---|---|---|
| s01 | Standalone baseline | Not run | Not recorded | Not recorded | Working control |
| f01h | FL Studio baseline | Yes | Five wrapper title drags alternate between complete editor frames and blank FL wrapper content | Baseline frame recorder only | Deterministic hosted failure |
| s02 | Standalone Wine log | Not run | Not recorded | Not recorded | Wine-log control |
| w02 | FL Studio Wine log | Yes | Five wrapper title drags reproduce the blanking | `NtUserSetWindowPos` moves wrapper `0x1800d8`; client `0x5038c` keeps swapping while its surface geometry follows the move | Wine-log failure |
| s03 | Standalone X11 trace | Not run | Not recorded | Not recorded | X11 control |
| x03 | FL Studio X11 trace | Yes | Frames 456–485 alternate between complete and blank editor content during five title drags | Redirected client `0x01802fd2` is copied into top-level `0x01a0000d`, then the parent surface overwrites the same rectangle | X11 presentation failure |
| s04 | Standalone OpenGL trace | Not run | Not recorded | Not recorded | OpenGL control |
| f04 | FL Studio OpenGL trace | Not run | Not recorded | Not recorded | OpenGL failure |
| fix03 | Patched FL Studio baseline | Yes, 406 frames | All 40 inspected frames 208–247 remain rendered during ten back-and-forth title drags | The generic re-presentation helper runs after overlapping parent software flushes | Patched behavior passes the hosted stress case |

Raw traces and recordings were local-only beneath `artifacts/opengl-flicker` and
are deliberately excluded from Git.

## First divergent event

The first title-drag marker in `x03` is monotonic `10301848277964`, UTC
`2026-09-02T11:31:44.822192+00:00`. During the first visible alternation:

1. X11 presents the plug-in pixmap and Wine copies redirected client window
   `0x01802fd2` into FL top-level `0x01a0000d` at monotonic
   `10301905155759`, UTC `11:31:44.879067`, destination `(351,330)`, size
   `1280x688` (`x11.raw.tsv:365450`).
2. At monotonic `10301906042473`, UTC `11:31:44.879954`, just 887 microseconds
   later, Wine sends a 1296x714 `MIT-SHM PutImage` from FL's software window
   surface to the same top-level drawable at `(344,304)`
   (`x11.raw.tsv:365455`). Its rectangle completely contains the OpenGL client
   destination.
3. The next client copy restores the plug-in editor, and the parent surface
   overwrites it again. For example, the next pair is a client `CopyArea` at
   `11:31:44.898576` (`x11.raw.tsv:365590`) followed by a containing parent
   `PutImage` at `11:31:44.902806` (`x11.raw.tsv:365664`).

There is no preceding X11 unmap, destroy, or reparent of the OpenGL client.

## Independent evidence

Three independent observations agree:

- The compositor frame recorder captures the visible alternation during the
  exact title-drag sequence in baseline `f01h` and traced runs `w02` and `x03`.
- Wine debug output shows wrapper `TPluginForm` `0x1800d8` moving, followed by
  `client_surface_update_geometry` for OpenGL child `0x5038c`; the GL thread
  continues `win32u_wglSwapBuffers` and `x11drv_egl_surface_swap` with the same
  HWND and context throughout the failure.
- Decoded X11 protocol shows the correct redirected GL surface remaining mapped
  and continually presented, while large software-surface `PutImage` requests
  overwrite its pixels in FL's top-level drawable.

Static Win32/X11 window inspection also confirms that the GL window is a nested
child composited into FL's one native top-level window. Heavy OpenGL capture is
unnecessary because the protocol trace directly observes both the correct
client presentation and the later overwrite at their final drawable.

## Rejected hypotheses

| Hypothesis | Evidence | Status |
|---|---|---|
| Detaching the editor prevents the flicker | The flicker reproduced while detached | Rejected as workaround |
| FL Studio bridged mode prevents the flicker | The flicker reproduced while bridged; one run also moved the editor to a screen corner | Rejected as workaround; coordinate symptom retained |
| The plug-in cannot render under Wine | The standalone renderer works | Rejected |
| The plug-in stops accepting input while blank | Controls and notes remain usable | Rejected |
| The plug-in stops swapping while blank | Wine traces record uninterrupted swaps for the same HWND and context | Rejected |
| The client window is hidden, destroyed, or reparented during movement | No matching X11 lifecycle event precedes the blank frames | Rejected |
| Detached or bridged mode is required to reproduce the bug | The native hosted wrapper reproduces deterministically; both modes were already observed to remain affected | Rejected as causal requirement |

## Causal chain

`title-move-1 marker (11:31:44.822192) -> FL repeatedly calls SetWindowPos for
its internal TPluginForm -> Wine updates the nested client-surface geometry and
continues presenting the OpenGL editor -> X11 CopyArea places the redirected GL
client in FL's top-level drawable (11:31:44.879067) -> Wine's software parent
surface flush sends a containing MIT-SHM PutImage 887 microseconds later
(11:31:44.879954) -> the next captured frame can contain FL's blank wrapper
pixels instead of the OpenGL editor.`

The responsible invariant is generic: after an offscreen child client surface
has been composited into a shared top-level drawable, a later overlapping flush
of that top-level's software window surface must not leave the child covered.

## Patch validation

The implementation is Wine commit
`3693d0baf73042cf0417ae1856ffcc26332630f0` (`win32u: Re-present offscreen
client surfaces after window flushes`). It adds no application, executable,
vendor, class-name, filename, or path checks.

- `scripts/test-client-surface-ordering.sh` fails against the previous private
  runner because the final traced event is the parent software flush, and
  passes against the patched build because a redirected client presentation
  follows that flush.
- Wine's focused 64-bit `opengl32` and `win32u` test modules pass with the
  implementation applied. Expected platform-dependent skips remain skips.
- A clean 32/64-bit build installed as
  `~/.local/opt/wine-flstudio/11.16-authattrs-opengl-ordering`, reporting
  `wine-11.16-283-g3693d0b (Staging)`. The generic ordering regression passed
  against the installed files. The local FL verifier accepted the pinned
  genuine binary with `00000000` and rejected the tampered copy with
  `80096010`.
- The portable mail patch passes `git apply --check` against both the clean
  authentication-patched Wine 11.16 tree and pristine Wine commit
  `8da89f8493b21ebfbe344a54dbef0cde23c7ea59`.
- The complete `fix03` FL Studio/OpenGL capture contains 406 compositor frames.
  The exact movement that flickered before was repeated ten times; visual
  inspection of frames 208–247 found the hosted editor rendered in every frame.

An in-tree screen-pixel assertion was not used as the regression oracle:
Wine's rootless X11 model means a Win32 screen DC does not reliably expose the
final XWayland-composited pixels. The repository therefore uses a generic,
non-product-specific Win32 reproducer plus Wine trace ordering as its automated
regression. The real compositor-frame capture is the end-to-end visual gate.

## Patch decision

| Evidence | Responsible boundary | Next test location |
|---|---|---|
| Coincident map/unmap/reparent/configure | Wine window/parent state | `dlls/user32/tests/win.c` or `dlls/win32u/tests/win.c` |
| Stable window, swaps stop | Hosted event/render scheduling | Targeted GDB trace before any Wine behavior change |
| Swaps continue, drawable changes | Wine GL drawable lifecycle | `dlls/opengl32/tests/opengl.c` |
| Captured GL frames correct, live output blank | Wine/XWayland presentation | `dlls/opengl32/tests/opengl.c` plus winex11 trace |
| Captured frames blank | Context/framebuffer state | Targeted OpenGL state dump before any presentation change |

Selected classification: **captured client frames are presented correctly, but
live output is overwritten in Wine/X11 presentation**. This is supported by
screen frames, Wine debug events, and decoded X11 requests. The implementation
uses a failing generic nested-child OpenGL regression and does not special-case
FL Studio, any plug-in, any vendor, filenames, paths, or hashes.
