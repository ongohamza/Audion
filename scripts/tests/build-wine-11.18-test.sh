#!/usr/bin/env bash
set -euo pipefail
project=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
script="$project/scripts/build-wine-11.18-staging.sh"
[[ -f $script ]] || { echo 'FAIL: build script missing'; exit 1; }
bash -n "$script"
help=$(bash "$script" --help)
for text in '11.18' 'Staging' '/usr/local/bin' '1.7.0' '--prepare-only' '--build-only' '--register' '--update-existing'; do
    [[ $help == *"$text"* ]] || { echo "FAIL: help lacks $text"; exit 1; }
done
for text in '--prefix=/usr/local' '-DCMAKE_INSTALL_PREFIX=/usr/local' \
            '-DWINEBUILD=/usr/local/bin/winebuild' 'wine-11.18-followups.list'; do
    grep -Fq -- "$text" "$script" || { echo "FAIL: build script lacks $text"; exit 1; }
done
grep -Fq '0002-enable-protection-unconditionally.patch' "$project/manifests/wine-11.18-followups.list"
if grep -Eq -- '--prefix=/usr([[:space:]]|$)|-DCMAKE_INSTALL_PREFIX=/usr([[:space:]]|$)' "$script"; then
    echo 'FAIL: build script still installs to /usr'; exit 1
fi
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
