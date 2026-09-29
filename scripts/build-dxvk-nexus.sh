#!/usr/bin/env bash
set -euo pipefail
usage() {
    cat <<'EOF'
Build Audion's 64-bit DXVK 3.1.1 composition support for FL64/Nexus.
Usage: bash build-dxvk-nexus.sh [--prepare-only | --help]
AUDION_JOBS controls build jobs (default 8; DXVK uses Meson/Ninja).
AUDION_DXVK_DIR overrides ~/.cache/nexus-composition-dxvk-3.1.1.
No installation is performed. Prefix DLL replacement is a separate step.
EOF
}
jobs=${AUDION_JOBS:-8}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo "Invalid AUDION_JOBS" >&2; exit 2; }
mode=build
case ${1:-} in
    --help|-h) usage; exit 0 ;;
    --prepare-only) mode=prepare; shift ;;
    '') ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
esac
[[ $# == 0 ]] || { usage >&2; exit 2; }
die() { echo "ERROR: $*" >&2; exit 1; }
[[ $EUID != 0 ]] || die 'Build as your normal user.'
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src=${AUDION_DXVK_DIR:-"$HOME/.cache/nexus-composition-dxvk-3.1.1"}
[[ $src == /* && $src != / && $src != "$HOME" ]] || die 'Choose a dedicated absolute source directory.'
for tool in git meson ninja glslangValidator x86_64-w64-mingw32-g++ flock; do
    command -v "$tool" >/dev/null || die "Missing dependency: $tool"
done
base=b1a1c99ab52b687cf950d62c88bc2fa316b41663
if [[ ! -e $src ]]; then
    git clone --recursive --depth 1 --shallow-submodules --branch v3.1.1 https://github.com/doitsujin/dxvk.git "$src"
fi
exec 9>"$src/audion-build.lock"
flock -n 9 || die 'Another DXVK build is running.'
[[ $(git -C "$src" rev-parse HEAD) == "$base" ]] || die 'Unexpected DXVK revision; preserving source and stopping.'
bash "$project/scripts/apply-dxvk-patches.sh" "$src"
if git -C "$src" submodule status --recursive | grep -Eq '^[-+U]'; then
    die 'Submodule revisions differ or are missing; restore the pinned submodules before building.'
fi
if [[ -f $src/build/meson-private/coredata.dat ]]; then
    meson setup --reconfigure "$src/build" "$src" --cross-file "$src/build-win64.txt" \
        --buildtype release -Denable_d3d8=false -Denable_d3d9=false
else
    meson setup "$src/build" "$src" --cross-file "$src/build-win64.txt" \
        --buildtype release -Denable_d3d8=false -Denable_d3d9=false
fi
[[ $mode == build ]] || exit 0
ninja -C "$src/build" -j"$jobs"
test -s "$src/build/src/dxgi/dxgi.dll"
test -s "$src/build/src/d3d11/d3d11.dll"
printf 'Built (not installed):\n%s\n%s\n' "$src/build/src/dxgi/dxgi.dll" "$src/build/src/d3d11/d3d11.dll"
