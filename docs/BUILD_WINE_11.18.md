# Build Wine 11.18 Staging + Audion + PipeASIO

This script is for x86_64 Linux, with Arch dependency checks. It installs Wine
at **/usr/bin/wine**, its SDK and libraries under **/usr**, and PipeASIO in
**/usr/lib/wine**. It does not create a separate test installation or remove
your existing /usr/local Wine.

## Run it

Save your work and close all Wine applications. Run as your normal user:

```bash
bash "$HOME/Desktop/Audion/scripts/build-wine-11.18-staging.sh" --register "$HOME/.wine"
```

`--register` targets an existing Wine prefix and changes its PipeASIO
registration. Omit it to build/install without touching a prefix. Running a
new Wine in an old prefix can update that prefix; back up important prefixes
first. The script never runs Wine as root. It requests sudo only for installing
into /usr.

If the install stage reports a running wineserver, save and close Wine apps.
For the existing /usr/local build, `/usr/local/bin/wineserver -k` stops all Wine
processes in the selected prefix (do not run it with unsaved work), then
`/usr/local/bin/wineserver -w` waits for them to exit. Rerun the script afterward.

Source and ordinary compilation files are kept in:

```text
~/.cache/audion-wine-11.18-staging/
```

The script reuses completed downloads and compilation. `build.log` records the
output. Interrupted patch application is not silently reset: if preparation
failed, inspect the log and use a new dedicated `AUDION_BUILD_DIR` to start
fresh. Changed patch contents or prepared tracked source also require a new
directory. There is no automatic recursive deletion.

## Dependencies

On Arch, missing basic tools can be installed with:

```bash
sudo pacman -S --needed base-devel git cmake python perl autoconf automake \
  flex bison pkgconf mingw-w64-gcc libpipewire freetype2 libx11 libxext \
  libxrender libxrandr libxi libxcursor libxfixes libglvnd alsa-lib libpulse gnutls
```

This does **not** install Arch Wine. Configure detects additional optional Wine
features from the libraries on your system. PipeASIO 1.7.0 requires PipeWire
development files >= 1.4.2 and a running PipeWire service for audio. The script
does not change your audio-service setup.

If an Arch package owns Wine/PipeASIO destination files, the script stops.
Resolve that package conflict deliberately; do not bypass the check by
overwriting package-owned files. This is a direct source installation, not a
pacman-managed package. Keep the source/build folder for future maintenance.

## Exact versions and build order

1. Wine 11.18: `7b3fff76fa5178f6ce0141b2c776afa2a822f101`.
2. Wine Staging v11.18: `627ccf4f350f41c6cea57fda05f52e57ba9fab1f`, all Staging patches.
3. Four Audion patches: two signature patches, the 11.18-context OpenGL patch,
   and the opt-in Effectrix deletion protection.
4. Regenerate configure and server protocol files; configure with
   `--prefix=/usr --libdir=/usr/lib --enable-archs=x86_64,i386`.
5. Build Wine using `make -j"$(nproc)"`, then install it.
6. Build PipeASIO v1.7.0 (`bb7911e0e7590a76c11b9b9843735fb116b9c1bf`), explicitly using
   `/usr/bin/winebuild`, `/usr/bin/winegcc`, `/usr/include/wine` and `/usr/lib/wine`.
   Run its non-integration tests, install, and optionally register.

Wine supports both 64-bit and new-WoW64 32-bit applications. This script builds
the **64-bit PipeASIO driver for FL64**; PipeASIO's experimental 32-bit frontend
and optional Qt settings panel are disabled.
Wine's large standalone regression-test executables are not built
(`--disable-tests`); this does not remove runtime Wine functionality.

The 11.18 OpenGL patch has the same logic as the 11.16 patch. Its first context
line was adapted to Wine's new designated structure initializer. Use this
build script for 11.18 Staging; `apply-patches.sh` remains pinned to 11.16.

## Launch the installed Wine explicitly

The old `/usr/local/bin/wine` may come first in PATH. Avoid that ambiguity:

```bash
/usr/bin/wine --version
WINE_GDI_STRICT_DELETEOBJECT=1 /usr/bin/wine \
  "$HOME/.wine/drive_c/Program Files/Image-Line/FL Studio 2026/FL64.exe"
```

Effectrix protection is off unless the variable is `1`. Do not export it
globally. The compatibility trade-off and regression results are documented in
[the Effectrix report](verification/2026-09-25-effectrix-crash.md).

If you omitted `--register`, close Wine apps and register later as your normal user:

```bash
WINEPREFIX="$HOME/.wine" WINE=/usr/bin/wine PIPEASIO_PREFIX=/usr \
  PIPEASIO_REGISTER_CANDIDATES=/usr/lib/wine /usr/bin/pipeasio-register
```

Then select PipeASIO in FL Studio's audio settings.

## Checks without system installation

```bash
bash scripts/tests/build-wine-11.18-test.sh
bash scripts/build-wine-11.18-staging.sh --prepare-only
bash scripts/build-wine-11.18-staging.sh --build-only
```

The first checks the command-line interface and shell syntax. The second
downloads the pinned revisions, applies Staging and all Audion patches, and
runs Wine configure. The third additionally compiles Wine in the same ordinary
build directory, but does not install anything. PipeASIO is compiled by the
default install run because it needs the newly installed Wine SDK. These checks
do not by themselves establish runtime stability of FL Studio on 11.18.

### Verification on 2026-09-26

The exact script successfully downloaded and checked the pinned sources,
applied all Staging patches and all four Audion patches, configured Wine, and
completed a full x86_64/i386 runtime build with `make -j"$(nproc)"`.
The built binary reports `wine-11.18-276-ge6206b6 (Staging)`.
Shell/argument tests and the existing 11.16 patch-application test also passed.
A resume check deliberately changed the generated Makefile's prefix to
`/usr/local`; rerunning `--prepare-only` restored `/usr` and `/usr/lib`.
No system installation or 11.18 FL Studio runtime test was performed.
PipeASIO's tool/header/library options were checked against its pinned CMake
sources and independently reviewed; its compilation, tests, installation and
registration are performed when you run the default installation mode, not
claimed as already verified here.

Upstream sources: [Wine mirror](https://github.com/wine-mirror/wine/tree/wine-11.18),
[Wine Staging](https://github.com/wine-staging/wine-staging/tree/v11.18),
[PipeASIO](https://github.com/M0n7y5/pipeasio/tree/v1.7.0).
