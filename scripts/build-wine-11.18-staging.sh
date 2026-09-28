#!/usr/bin/env bash
# Build as your normal user. Only the two installation steps use sudo.
set -Eeuo pipefail

usage() {
    cat <<'EOF'
Build Wine 11.18 Staging + Audion patches + PipeASIO 1.7.0.
Install prefix: /usr/local (Wine: /usr/local/bin/wine; libraries: /usr/local/lib/wine).
Usage: bash build-wine-11.18-staging.sh [--prepare-only | --build-only] [--update-existing] [--register /absolute/wine-prefix]
  default         Build Wine, install it, then build and install 64-bit PipeASIO.
  --prepare-only  Download, apply patches and configure; no compilation/install.
  --build-only    Also compile Wine; no install or PipeASIO build (needs installed SDK).
  --register DIR  After installation, register PipeASIO in this existing Wine prefix.
  --update-existing  Validate the original Audion stack, add follow-ups, reuse this build tree.
  --help          Show this help.
AUDION_BUILD_DIR overrides ~/.cache/audion-wine-11.18-staging.
Builds use make -j"$(nproc)". No test installation is created.
Close Wine applications before installation. Do not run this script with sudo.
This updates /usr/local Wine. It does not delete prefixes or remove /usr Wine.
EOF
}

mode=install
register_prefix=
update_existing=0
while (($#)); do
    case $1 in
        --help|-h) usage; exit 0 ;;
        --update-existing) update_existing=1; shift ;;
        --prepare-only|--build-only)
            [[ $mode == install ]] || { echo 'Choose only one build mode.' >&2; exit 2; }
            mode=${1#--}; shift ;;
        --register)
            [[ $# -ge 2 && $2 == /* ]] || {
                echo '--register requires an absolute prefix path' >&2; exit 2;
            }
            register_prefix=$2; shift 2 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

die() { echo "ERROR: $*" >&2; exit 1; }
[[ $EUID -ne 0 ]] || die 'Run as your normal user, not sudo/root.'
[[ $(uname -m) == x86_64 ]] || die 'This script targets x86_64 Linux.'
if [[ -n $register_prefix ]]; then
    [[ $mode == install ]] || die '--register requires the default install mode.'
    [[ -f $register_prefix/system.reg && -d $register_prefix/drive_c ]] ||
        die "Not an existing Wine prefix: $register_prefix"
fi

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work_dir=${AUDION_BUILD_DIR:-"$HOME/.cache/audion-wine-11.18-staging"}
[[ $work_dir == /* && $work_dir != *[[:space:]]* ]] || die 'Build directory must be absolute, without spaces.'
[[ $work_dir != / && $work_dir != "$HOME" && $work_dir != /usr* ]] || die 'Choose a dedicated user build directory.'
for command in git python3 perl autoreconf make gcc g++ flex bison pkg-config cmake \
               x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc sha256sum flock tee realpath; do
    command -v "$command" >/dev/null || die "Missing $command. On Arch, install build dependencies (see docs/BUILD_WINE_11.18.md)."
done
pkg-config --atleast-version=1.4.2 libpipewire-0.3 || die 'PipeASIO needs libpipewire >= 1.4.2 (Arch: libpipewire).'
pkg-config --exists freetype2 x11 xext xrender xrandr xi xcursor xfixes gl alsa libpulse gnutls ||
    die 'Missing Wine graphics/audio development libraries; see docs/BUILD_WINE_11.18.md.'

check_package_conflicts() {
    local file owner
    command -v pacman >/dev/null || return 0
    for file in /usr/local/bin/wine /usr/local/bin/wineserver /usr/local/bin/winegcc /usr/local/bin/winebuild \
                /usr/local/include/wine/windows/windows.h /usr/local/lib/wine/x86_64-unix/ntdll.so \
                /usr/local/lib/wine/x86_64-windows/ntdll.dll /usr/local/bin/pipeasio-register \
                /usr/local/lib/wine/x86_64-windows/pipeasio64.dll; do
        if owner=$(pacman -Qoq "$file" 2>/dev/null); then
            die "$file is owned by Arch package $owner. Resolve the package conflict before installing custom Wine."
        fi
    done
}
check_package_conflicts
mkdir -p "$work_dir"
work_dir=$(realpath "$work_dir")
exec 9>"$work_dir/build.lock"
flock -n 9 || die "Another build is using $work_dir."
exec > >(tee -a "$work_dir/build.log") 2>&1
trap 'result=$?; echo "Stopped with status $result at line $LINENO. Log: $work_dir/build.log" >&2; exit "$result"' ERR
printf '\nStarting Wine 11.18 Staging build (%s).\n' "$mode"

wine_commit=7b3fff76fa5178f6ce0141b2c776afa2a822f101
staging_commit=627ccf4f350f41c6cea57fda05f52e57ba9fab1f
pipeasio_commit=bb7911e0e7590a76c11b9b9843735fb116b9c1bf
wine_src="$work_dir/wine-src"
wine_build="$work_dir/wine-build"
staging_src="$work_dir/staging"
pipeasio_src="$work_dir/pipeasio"
patches=(
    "$project_dir/patches/flstudio-authattrs/0001-crypt32-tests-Test-signatures-with-unsorted-authenti.patch"
    "$project_dir/patches/flstudio-authattrs/0002-crypt32-Preserve-authenticated-attribute-order-when-.patch"
    "$project_dir/patches/wine-11.18/0001-win32u-Re-present-offscreen-client-surfaces-after-wi.patch"
    "$project_dir/patches/effectrix-crash/0001-gdi32-Add-opt-in-protection-against-malformed-Delete.patch"
)
followup_patches=()
while IFS= read -r entry || [[ -n $entry ]]; do
    [[ -z $entry || $entry == \#* ]] && continue
    [[ $entry == patches/* && $entry != *..* ]] || die "Invalid follow-up patch path: $entry"
    followup_patches+=("$project_dir/$entry")
done < "$project_dir/manifests/wine-11.18-followups.list"
for patch_file in "${patches[@]}" "${followup_patches[@]}"; do [[ -f $patch_file ]] || die "Missing patch: $patch_file"; done
patch_digest=$(sha256sum "${patches[@]}" "${followup_patches[@]}" | sha256sum | cut -d' ' -f1)

clone_pinned() {
    local url=$1 tag=$2 commit=$3 directory=$4
    if [[ ! -e $directory ]]; then
        git clone --depth 1 --branch "$tag" "$url" "$directory"
    fi
    [[ $(git -C "$directory" rev-parse HEAD) == "$commit" ]] || die "Unexpected revision in $directory; use a new AUDION_BUILD_DIR."
    [[ -z $(git -C "$directory" status --porcelain) ]] || die "Uncommitted changes in $directory; preserving them and stopping."
}
clone_pinned https://github.com/wine-staging/wine-staging.git v11.18 "$staging_commit" "$staging_src"
clone_pinned https://github.com/M0n7y5/pipeasio.git v1.7.0 "$pipeasio_commit" "$pipeasio_src"
[[ $(python3 "$staging_src/staging/patchinstall.py" --upstream-commit) == "$wine_commit" ]] ||
    die 'Staging does not match the pinned Wine source.'

# The resume marker records both the exact patch set and generated source changes.
# Never reset a checkout automatically after an interrupted/failed patch operation.
source_signature() {
    { git -C "$wine_src" rev-parse HEAD; git -C "$wine_src" diff --binary HEAD; } | sha256sum | cut -d' ' -f1
}
if ((update_existing)); then
    [[ -f $work_dir/prepared && -f $wine_build/Makefile ]] || die '--update-existing needs an existing prepared Audion build.'
    git -C "$wine_src" merge-base --is-ancestor "$wine_commit" HEAD || die 'Existing source is not based on pinned Wine 11.18.'
    bash "$project_dir/scripts/apply-wine-11.18-followups.sh" "$wine_src"
    (cd "$wine_src" && autoreconf -f && ./tools/make_requests)
    printf '%s %s\n' "$patch_digest" "$(source_signature)" > "$work_dir/prepared"
elif [[ -f $work_dir/prepared ]]; then
    read -r saved_patches saved_source < "$work_dir/prepared"
    [[ $saved_patches == "$patch_digest" && $saved_source == "$(source_signature)" ]] ||
        die 'Prepared source/patches changed. For an existing Audion 11.18 build, review changes and use --update-existing; files were preserved.'
else
    clone_pinned https://github.com/wine-mirror/wine.git wine-11.18 "$wine_commit" "$wine_src"
    export GIT_COMMITTER_NAME="${GIT_COMMITTER_NAME:-Audion builder}"
    export GIT_COMMITTER_EMAIL="${GIT_COMMITTER_EMAIL:-audion-builder@example.invalid}"
    python3 "$staging_src/staging/patchinstall.py" --all --backend=git-am --no-autoconf --destdir="$wine_src"
    git -C "$wine_src" am "${patches[@]}"
    for patch_file in "${followup_patches[@]}"; do
        git -C "$wine_src" apply --check "$patch_file"
        git -C "$wine_src" apply --index "$patch_file"
        git -C "$wine_src" -c user.name='Audion builder' -c user.email='audion-builder@example.invalid' \
            commit -m "Audion: apply $(basename "$patch_file")"
    done
    (cd "$wine_src" && autoreconf -f && ./tools/make_requests)
    printf '%s %s\n' "$patch_digest" "$(source_signature)" > "$work_dir/prepared"
fi

mkdir -p "$wine_build"
# Reassert the destination even on resumed builds. An old Makefile configured
# for /usr must never redirect this script's privileged installation.
(cd "$wine_build" && "$wine_src/configure" --prefix=/usr/local --libdir=/usr/local/lib \
    --enable-archs=x86_64,i386 --enable-build-id --disable-tests --with-x --with-opengl \
    --with-freetype --with-alsa --with-pulse --with-gnutls \
    CC=gcc CXX=g++ CFLAGS='-O2 -g' CXXFLAGS='-O2 -g')
if [[ $mode == prepare-only ]]; then
    echo 'PASS: Wine 11.18 Staging + Audion patches prepared and configured. Nothing installed.'
    exit 0
fi
make -C "$wine_build" -j"$(nproc)"
if [[ $mode == build-only ]]; then
    echo 'PASS: Wine built. Nothing installed; PipeASIO will build against the new SDK during the install run.'
    exit 0
fi

check_package_conflicts
command -v sudo >/dev/null || die 'sudo is required for /usr/local installation.'
if pgrep -u "$UID" -x wineserver >/dev/null; then
    die 'A wineserver is running. Save and close Wine applications, stop the old wineserver, then rerun this script (compiled files are reused).'
fi
sudo -v
sudo make -C "$wine_build" install
[[ $(/usr/local/bin/wine --version) == wine-11.18*Staging* ]] || die 'Installed Wine is not 11.18 Staging.'

# All SDK paths and the driver destination belong to this exact /usr/local build.
export PATH="/usr/local/bin:/usr/bin:/bin:$PATH"
unset WINEDLLPATH WINELOADER WINESERVER WINEARCH
pipeasio_build="$work_dir/pipeasio-build"
cmake -S "$pipeasio_src" -B "$pipeasio_build" -G 'Unix Makefiles' \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_INSTALL_LIBDIR=lib \
    -DWINEBUILD=/usr/local/bin/winebuild -DWINEGCC=/usr/local/bin/winegcc \
    '-DWINE_INCLUDE_DIRS=/usr/local/include;/usr/local/include/wine;/usr/local/include/wine/windows' \
    -DWINE_LIB_ROOT=/usr/local/lib/wine -DPIPEASIO_WINE_INSTALL_ROOT=/usr/local/lib/wine \
    -DPIPEASIO_PE_COMPILER=gcc -DBUILD_ARM64=OFF -DBUILD_WOW64_32=OFF \
    -DBUILD_SETTINGS_PANEL=OFF -DBUILD_TESTS=ON
make -C "$pipeasio_build" -j"$(nproc)"
ctest --test-dir "$pipeasio_build" --output-on-failure -LE 'integration|wine|pipewire' -j"$(nproc)"
sudo cmake --install "$pipeasio_build"
test -f /usr/local/lib/wine/x86_64-windows/pipeasio64.dll
test -f /usr/local/lib/wine/x86_64-unix/pipeasio64.so
if [[ -n $register_prefix ]]; then
    WINEPREFIX="$register_prefix" WINE=/usr/local/bin/wine PIPEASIO_PREFIX=/usr/local \
        PIPEASIO_REGISTER_CANDIDATES=/usr/local/lib/wine /usr/local/bin/pipeasio-register
fi
echo 'Installed: /usr/local/bin/wine (11.18 Staging), Audion patches, and 64-bit PipeASIO 1.7.0.'
echo 'Effectrix protection is always active. Use /usr/local/bin/wine explicitly.'
if [[ -z $register_prefix ]]; then
    echo 'To register PipeASIO in your existing FL prefix (after closing Wine apps):'
    echo 'WINEPREFIX="$HOME/.wine" WINE=/usr/local/bin/wine PIPEASIO_PREFIX=/usr/local PIPEASIO_REGISTER_CANDIDATES=/usr/local/lib/wine /usr/local/bin/pipeasio-register'
fi
