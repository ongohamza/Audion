# Nested client-surface ordering patch

This patch fixes a Wine X11 compositing failure exposed by a hosted OpenGL/VST
editor inside FL Studio. It is deliberately generic: there are no application,
vendor, executable, class-name, or plug-in checks.

## What it changes

Nested OpenGL and Vulkan windows may use an offscreen `client_surface` which
Wine copies into the application's shared top-level drawable. A later software
`window_surface` flush can cover those pixels, particularly when a parent
without `WS_CLIPCHILDREN` is moved or repainted. The patch re-presents only
offscreen client surfaces which belong to the flushed top-level and intersect
the flushed rectangle.

The re-presentation happens after the window-surface mutex is released.
Recursive scaled-surface flushes use an internal no-presentation path so that
DPI scaling cannot re-enter the outer surface lock. The cost is one additional
client-surface blit per overlapping software flush; non-overlapping, attached,
and unrelated client surfaces are unchanged.

## Apply

The patch was generated from Wine-Staging 11.16 plus the repository's existing
authenticated-attribute patches at source commit
`fd3bd73ee8623f226eb0bc3ba4f62a8ebf8c717e`. Its context also applies cleanly
to pristine Wine 11.16 commit
`8da89f8493b21ebfbe344a54dbef0cde23c7ea59`.

```sh
git apply /path/to/patches/opengl-flicker/0001-win32u-Re-present-offscreen-client-surfaces-after-wi.patch
```

Apply it after Wine-Staging and any unrelated local patches, then configure and
build Wine normally.

## Verify

From this repository, run the generic trace-order regression against a Wine
build tree or installed prefix:

```sh
scripts/test-client-surface-ordering.sh /path/to/wine-build/wine
scripts/test-client-surface-ordering.sh /path/to/installed-wine-prefix
```

The unpatched Wine 11.16 runner fails with `parent software surface was the
final presentation`. The patched runner must report:

```text
PASS: redirected child client surface follows the parent software flush
```

The focused Wine checks used for this patch are:

```sh
make -j1 dlls/opengl32/tests/x86_64-windows/opengl.ok
make -j1 dlls/win32u/tests/x86_64-windows/win32u.ok
```

A clean serial 32/64-bit build was also installed and verified as
`wine-11.16-283-g3693d0b (Staging)`. The installed runner passed the generic
ordering regression and the repository's original/tampered FL signature gate.

The FL Studio integration gate is ten alternating title-bar drags with a hosted
OpenGL editor open in the native wrapper. Every compositor-captured frame must
retain the plug-in image.

## Roll back

For an uncommitted source tree, run `git apply -R` with the same patch. For an
installed private runner, point the user's runner symlink back to the previous
versioned installation; do not delete or overwrite the old runner or Wine
prefix.
