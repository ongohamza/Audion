#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
script="$project/scripts/build-wine-11.18-staging.sh"
[[ -f $script ]] || { echo 'FAIL: build script missing'; exit 1; }
bash -n "$script"
help=$(bash "$script" --help)
for text in '11.18' 'Staging' '/usr/bin' '1.7.0' '--prepare-only' '--build-only' '--register'; do
    [[ $help == *"$text"* ]] || { echo "FAIL: help lacks $text"; exit 1; }
done
set +e
output=$(bash "$script" --invalid-option 2>&1)
status=$?
set -e
[[ $status == 2 && $output == *'Unknown option'* ]] || {
    echo 'FAIL: unknown option must fail before touching files'; exit 1;
}
set +e
output=$(bash "$script" --register 2>&1)
status=$?
set -e
[[ $status == 2 && $output == *'requires an absolute prefix path'* ]] || {
    echo 'FAIL: missing registration path accepted'; exit 1;
}
set +e
output=$(bash "$script" --register relative/path 2>&1)
status=$?
set -e
[[ $status == 2 ]] || { echo 'FAIL: relative registration path accepted'; exit 1; }
echo 'PASS: script syntax, help, invalid options and registration path guards'
