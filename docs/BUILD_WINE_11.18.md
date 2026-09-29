# Build Wine 11.18 Staging + Audion + PipeASIO

This script is for x86_64 Linux, with Arch dependency checks. It installs Wine
at **/usr/local/bin/wine**, its SDK and libraries under **/usr/local**, and PipeASIO in
**/usr/local/lib/wine**. It updates that installation, without creating a separate
test installation, deleting a Wine prefix, or removing files under /usr.

## Run it

Save your work and close all Wine applications. Run as your normal user:

```bash
bash "$HOME/Desktop/Audion/scripts/build-wine-11.18-staging.sh" --register "$HOME/.wine"
```

If this build folder already contains the earlier Audion 11.18 build, update
it in place (reuse downloads and compiled objects):

```bash
bash "$HOME/Desktop/Audion/scripts/build-wine-11.18-staging.sh" --update-existing --register "$HOME/.wine"
```

This checks the original four patches and all follow-ups in a private Git
index before applying missing follow-ups. Conflicts stop without resetting
your source. Existing unrelated source changes are preserved, so review them
before building. Do not edit the source or run another build concurrently.

`--register` targets an existing Wine prefix and changes its PipeASIO
registration. Omit it to build/install without touching a prefix. Running a
new Wine in an old prefix can update that prefix; back up important prefixes
first. The script never runs Wine as root. It requests sudo only for installing
into /usr/local.

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
fresh. For the already prepared Audion 11.18 stack, use `--update-existing`
to validate and apply additions without making another build directory.
There is no automatic recursive deletion.

## Dependencies

On Arch, missing basic tools can be installed with:

```bash
sudo pacman -S --needed base-devel git cmake python perl autoconf automake \
  flex bison pkgconf mingw-w64-gcc libpipewire freetype2 libx11 libxext \
  libxrender libxrandr libxi libxcursor libxfixes libglvnd alsa-lib libpulse gnutls \
  wayland wayland-protocols libxkbcommon
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
3. Audion patches: two signature patches, the 11.18-context OpenGL patch,
   Effectrix deletion protection, its always-on follow-up, and the Nexus
   Direct2D/completed-frame and FL Studio Wayland input follow-ups listed in the manifest.
4. Regenerate configure and server protocol files; configure with
   `--prefix=/usr/local --libdir=/usr/local/lib --enable-archs=x86_64,i386`.
5. Build Wine using `make -j"$AUDION_JOBS"` (default 8), then install it.
6. Build PipeASIO v1.7.0 (`bb7911e0e7590a76c11b9b9843735fb116b9c1bf`), explicitly using
   `/usr/local/bin/winebuild`, `/usr/local/bin/winegcc`, `/usr/local/include/wine` and `/usr/local/lib/wine`.
   Run its non-integration tests, install, and optionally register.

Wine supports both 64-bit and new-WoW64 32-bit applications. This script builds
the **64-bit PipeASIO driver for FL64**; PipeASIO's experimental 32-bit frontend
and optional Qt settings panel are disabled.
Wine's large standalone regression-test executables are not built
(`--disable-tests`); this does not remove runtime Wine functionality.

The 11.18 OpenGL patch has the same logic as the 11.16 patch. Its first context
line was adapted to Wine's new designated structure initializer. Use this
build script for 11.18 Staging; `apply-patches.sh` remains pinned to 11.16.

## Native Wayland and synchronization

FL Studio's Wayland fixes are enabled automatically for `FL64.exe` and `FL.exe`.
They apply to the host window/input path, not a plugin-name whitelist. The Nexus
follow-up repairs `WindowFromPoint` beyond the Windows desktop rectangle; see
[scope and verification](FLSTUDIO_WAYLAND.md). An optional
`WINE_WAYLAND_FLSTUDIO=0` override disables compatibility for diagnosis.

To select native Wayland for an existing FL prefix, with Wine applications closed:

```bash
WINEPREFIX="$HOME/.wine" /usr/local/bin/wine reg add 'HKCU\Software\Wine\Drivers' \
  /v Graphics /t REG_SZ /d wayland,x11 /f
```

Launch without forcing a synchronization backend:

```bash
env -u DISPLAY -u WINEFSYNC -u WINEESYNC WINEPREFIX="$HOME/.wine" WINEDEBUG=-all \
  /usr/local/bin/wine 'C:\Program Files\Image-Line\FL Studio 2026\FL64.exe'
```

The graphics choice persists in the prefix, and FL compatibility needs no launch
variable. Ntsync uses Wine's normal selection when kernel support is available.
The build does not change PipeWire buffer sizes, realtime priorities, or kernel
settings. Set `AUDION_JOBS=8` (or fewer) to limit build load.

## Launch the installed Wine explicitly

Use the explicitly installed binary, irrespective of other Wine packages in PATH:

```bash
/usr/local/bin/wine --version
/usr/local/bin/wine \
  "$HOME/.wine/drive_c/Program Files/Image-Line/FL Studio 2026/FL64.exe"
```

Effectrix protection is always active in this build, including if the old
environment variable is set to `0`. The original opt-in implementation's
compatibility trade-off and regression results are documented in
[the Effectrix report](verification/2026-09-25-effectrix-crash.md).

If you omitted `--register`, close Wine apps and register later as your normal user:

```bash
WINEPREFIX="$HOME/.wine" WINE=/usr/local/bin/wine PIPEASIO_PREFIX=/usr/local \
  PIPEASIO_REGISTER_CANDIDATES=/usr/local/lib/wine /usr/local/bin/pipeasio-register
```

Then select PipeASIO in FL Studio's audio settings.

For accelerated Nexus rendering, also build/install the separate DXVK patch
set and enable its required options: [Nexus instructions](NEXUS_RENDERING.md).
Wine's builder does not install DXVK into your prefix.

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

### Historical verification on 2026-09-26 (superseded /usr destination)

The following records the earlier script, not verification of the current
/usr/local and always-on changes. Current shell/argument tests pass; a fresh
end-to-end run of the updated combined script is not yet claimed.

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
