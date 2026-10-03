#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
work_dir=$(mktemp -d -t new-apps-test-XXXXXX)
trap 'rm -rf "$work_dir"' EXIT
sanitizer_flags=()
if [[ -n "${SANITIZERS:-}" ]]; then
    sanitizer_flags=("-fsanitize=$SANITIZERS" -fno-omit-frame-pointer)
fi
for app in three-body-pin crt-pin; do
    "${CC:-cc}" -std=c11 -O2 -g -Wall -Wextra -Werror \
        "${sanitizer_flags[@]}" \
        -I"$root_dir/firmwares/$app/main" -I"$root_dir/common" \
        "$root_dir/firmwares/$app/tests/test_scene.c" \
        "$root_dir/firmwares/$app/main/scene.c" "$root_dir/common/pin_gfx.c" \
        -lm -o "$work_dir/$app-test"
    "$work_dir/$app-test"
done
