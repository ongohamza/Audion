# Audion

Audion is a small set of patches for building Wine for music software.

**Wine 11.18 Staging + PipeASIO:** use the
[build-and-install script instructions](docs/BUILD_WINE_11.18.md). That script
installs to `/usr/local/bin` and uses the 11.18-adapted OpenGL patch plus
always-on Effectrix protection, Nexus rendering follow-ups, and automatic
FL Studio native Wayland input fixes. The manual
instructions below remain for Wine 11.16.

**Nexus hardware-accelerated GUI:** the paired Wine and DXVK patches address
missing controls, in-progress-frame flicker and a context-state race.
Use the [combined Nexus build instructions](docs/NEXUS_RENDERING.md).

**FL Studio native Wayland input:** compatibility activates automatically for
`FL64.exe` and `FL.exe`. The patches preserve plugin coordinates, hand custom
wrapper dragging to the compositor, and repair the out-of-desktop window lookup
used by JUCE plugins such as Nexus. See [Wayland details and testing](docs/FLSTUDIO_WAYLAND.md).

It addresses these problems:

- Native Wayland plugin controls lose mouse targeting, including JUCE window lookup.
- FL Studio says **“The validity of the program could not be verified.”**
- Hosted OpenGL/VST plug-in windows flicker or turn blank inside FL Studio.
- Effectrix can delete FL Studio's graphics objects and crash its renderer
  (always-on protection in the 11.18 build; legacy 11.16 instructions below are opt-in).

The input, authentication and presentation changes address Wine compatibility;
Effectrix protection is a defensive workaround for invalid plugin handles. Audion does **not** crack FL
Studio, unlock trial features, or skip signature checking. A damaged signature
is still rejected.

## Apply the patches — the very easy version

Think of this as putting four Band-Aids on Wine's source code.

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

### 3. Apply all four patches

```bash
"$HOME/src/Audion/scripts/apply-patches.sh" "$HOME/src/wine-audion"
```

Success looks like this:

```text
Applying 4 patch(es)
crypt32/tests: Test signatures with unsorted authenticated attributes
crypt32: Preserve authenticated attribute order when verifying
win32u: Re-present offscreen client surfaces after window flushes
gdi32: Add opt-in protection against malformed DeleteObject aliases
```

That is all “apply the patches” means. Your Wine source now contains the fixes.

If you already have Audion on your Desktop, use that local copy instead:

```bash
bash "$HOME/Desktop/Audion/scripts/apply-patches.sh" /path/to/clean/wine-source
```

Use a clean checkout of the pinned Wine commit above. Do not apply the full
stack again to a Wine tree that already contains Audion patches. Applying
patches changes source code; rebuilding and installing are still required.

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

### 5. Enable the Effectrix protection for FL Studio

For the Effectrix protection, start FL Studio with this environment variable:

```bash
WINE_GDI_STRICT_DELETEOBJECT=1 "$HOME/.local/opt/wine-audion/bin/wine" /path/to/FL64.exe
```

It is off by default. Enable it only for affected applications: it disables
legacy short-handle deletion compatibility in that process. See the
[Effectrix evidence and test record](docs/verification/2026-09-25-effectrix-crash.md).

No patch can promise zero regressions. With this protection enabled, an older
application or another plugin in the same FL process that intentionally uses
short deletion handles could fail to release graphics objects. Enable the
variable only on the FL launch command, not globally in your shell settings.
To disable it, close FL and relaunch with `WINE_GDI_STRICT_DELETEOBJECT=0`.
No rebuild is needed to switch it on or off. The previous signature and OpenGL
fixes remain active either way.

PipeASIO is separate from these four Wine patches. The private-install example
above does not install or migrate an existing PipeASIO driver.

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
| `gdi32` opt-in protection | Rejects malformed short deletion handles before they can resolve to another live graphics object. |

There are no checks for application or plug-in vendors, filenames, product
versions, publishers, or installation paths in the implementation patches.

## Verify the patch application

The newest four Wine commits should be the Audion patches:

```bash
git -C "$HOME/src/wine-audion" log -4 --oneline
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

patches/effectrix-crash/
  0001-gdi32-Add-opt-in-protection-against-malformed-Delete.patch
```

The repository contains no FL Studio or third-party plug-in binaries.
