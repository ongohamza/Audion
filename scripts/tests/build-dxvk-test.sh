#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
script="$project/scripts/build-dxvk-nexus.sh"
bash -n "$script"
help=$(bash "$script" --help)
for text in '3.1.1' '64-bit' 'nproc' 'No installation'; do
    [[ $help == *"$text"* ]] || { echo "FAIL: missing $text"; exit 1; }
done
if bash "$script" --invalid >/dev/null 2>&1; then echo 'FAIL: invalid option accepted'; exit 1; fi
echo 'PASS: DXVK builder help, syntax, and invalid-option guard'
