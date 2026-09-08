# Audion

Audion is a small set of patches for building Wine for music software.

It fixes two problems:

- FL Studio says **“The validity of the program could not be verified.”**
- Hosted OpenGL/VST plug-in windows flicker or turn blank inside FL Studio.

These are general Wine compatibility fixes. Audion does **not** crack FL
Studio, unlock trial features, or skip signature checking. A damaged signature
is still rejected.

## Apply the patches — the very easy version

Think of this as putting three Band-Aids on Wine's source code.

You need two folders:

```text
Audion/        ← the Band-Aids
wine-audion/   ← Wine's source code
```

The Wine folder means the folder containing files such as `configure` and
`dlls/`. It does **not** mean `~/.wine`, `/usr/bin/wine`, or an installed Wine
prefix.

### 1. Download Audion

```bash
mkdir -p "$HOME/src"
git clone https://github.com/ongohamza/Audion.git "$HOME/src/Audion"
```

### 2. Download the tested Wine source

```bash
git clone https://gitlab.winehq.org/wine/wine.git "$HOME/src/wine-audion"
git -C "$HOME/src/wine-audion" checkout 8da89f8493b21ebfbe344a54dbef0cde23c7ea59
```

That commit is Wine 11.16, the version used for this patch set.

### 3. Apply all three patches

```bash
"$HOME/src/Audion/scripts/apply-patches.sh" "$HOME/src/wine-audion"
```

Success looks like this:

```text
Applying 3 patch(es)
crypt32/tests: Test signatures with unsorted authenticated attributes
crypt32: Preserve authenticated attribute order when verifying
win32u: Re-present offscreen client surfaces after window flushes
```

That is all “apply the patches” means. Your Wine source now contains the fixes.

### 4. Rebuild Wine

Build the patched source with your normal Wine build process. Here is a simple
private-install example:

```bash
mkdir -p "$HOME/build/wine-audion"
cd "$HOME/build/wine-audion"
"$HOME/src/wine-audion/configure" \
  --enable-archs=i386,x86_64 \
  --prefix="$HOME/.local/opt/wine-audion"
make -j"$(nproc)"
make install
```

Your patched executable will be:

```text
$HOME/.local/opt/wine-audion/bin/wine
```

This does not replace `/usr/bin/wine`. Wine needs its normal build dependencies,
including the tools required for both selected architectures. If `configure`
reports a missing dependency, install that dependency and run it again.

## What the script protects you from

`scripts/apply-patches.sh` checks everything before it starts:

- the path must be a Wine Git source checkout;
- the checkout must have no uncommitted changes;
- the checkout must be the tested Wine 11.16 commit; and
- every required patch file must exist.

If `git am` fails, the script aborts the partial operation instead of leaving a
half-patched tree.

Do not use `ALLOW_UNTESTED_WINE=1` just to silence a version error. That option
is for developers who reviewed the patch against another Wine revision and are
prepared to run the tests again.

## What each patch does

| Patch | Plain-English job |
|---|---|
| `crypt32` regression | Proves Wine accepts the valid unusual signature and still rejects a changed signature. |
| `crypt32` implementation | Hashes authenticated attributes in the original encoded order during verification. |
| `win32u` implementation | Re-presents an overlapping offscreen plug-in surface after the parent window repaints. |

There are no checks for application or plug-in vendors, filenames, product
versions, publishers, or installation paths in the implementation patches.

## Verify the patch application

The newest three Wine commits should be the Audion patches:

```bash
git -C "$HOME/src/wine-audion" log -3 --oneline
```

Developers with a local Wine Git repository can run the clean-application test:

```bash
./scripts/tests/apply-patches-test.sh /path/to/local/wine-git-repository
```

After building Wine, the generic graphics ordering test is:

```bash
./scripts/test-client-surface-ordering.sh \
  "$HOME/.local/opt/wine-audion"
```

It requires `x86_64-w64-mingw32-gcc`, a working display, and the patched Wine
runner.

## Detailed explanation

- [Technical reference (PDF)](docs/WINE_DAW_PATCHES.pdf)
- [Technical reference (editable Markdown)](docs/WINE_DAW_PATCHES.md)
- [FL Studio signature verification record](docs/verification/2026-08-31-wine-11.16-fl-studio.md)
- [OpenGL/VST flicker root-cause record](docs/verification/2026-09-02-opengl-flicker-root-cause.md)

The full report explains the exact CMS byte-order mismatch, why the signature
fix is not a bypass, the X11 `CopyArea`/`MIT-SHM PutImage` overwrite, Wine lock
ordering, regression tests, and end-to-end results.

## Patch files

```text
patches/flstudio-authattrs/
  0001-crypt32-tests-Test-signatures-with-unsorted-authenti.patch
  0002-crypt32-Preserve-authenticated-attribute-order-when-.patch

patches/opengl-flicker/
  0001-win32u-Re-present-offscreen-client-surfaces-after-wi.patch
```

The repository contains no FL Studio or third-party plug-in binaries.
