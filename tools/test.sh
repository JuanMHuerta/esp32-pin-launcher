#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
selected=all
soak=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --app) selected=${2:?--app requires a project name}; shift 2 ;;
        --soak) soak=true; shift ;;
        --help|-h) echo "Usage: $0 [--app PROJECT] [--soak]"; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done
case "$selected" in
    all|conways-pin|fluid-pin|pet-pin|render-pin|dungeon-pin|maze-pin|wayfarer-pin|three-body-pin|crt-pin) ;;
    *) echo "Unknown app: $selected" >&2; exit 2 ;;
esac

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/multi-pin-tests.XXXXXXXX")
trap 'rm -rf -- "$work_dir"' EXIT
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror)
if [[ -n "${SANITIZERS:-}" ]]; then
    flags+=("-fsanitize=$SANITIZERS" -fno-omit-frame-pointer)
    if [[ "${SANITIZER_TRAP:-0}" == 1 ]]; then
        flags+=(-fsanitize-undefined-trap-on-error)
    fi
fi

run_test() {
    local app=$1 test=$2
    shift 2
    local app_dir="$root_dir/firmwares/$app"
    local sources=()
    for source in "$@"; do sources+=("$app_dir/$source"); done
    "${CC:-cc}" "${flags[@]}" -I"$app_dir/main" -I"$root_dir/common" \
        "${sources[@]}" -lm -o "$work_dir/$test"
    "$work_dir/$test"
}

cd "$work_dir"
python3 -m unittest discover -s "$root_dir/tools/tests" -v
"${CC:-cc}" "${flags[@]}" -I"$root_dir/common" \
    "$root_dir/tools/tests/test_gfx.c" "$root_dir/common/pin_gfx.c" -o "$work_dir/test-gfx"
"$work_dir/test-gfx"

for app in conways-pin fluid-pin pet-pin render-pin dungeon-pin maze-pin wayfarer-pin three-body-pin crt-pin; do
    if [[ "$selected" != all && "$selected" != "$app" ]]; then continue; fi
    echo "Testing $app"
    case "$app" in
        conways-pin)
            run_test "$app" life tests/life_test.c main/life.c
            run_test "$app" palette tests/palette_test.c main/palette.c
            run_test "$app" touch-map tests/touch_map_test.c main/touch_map.c ;;
        fluid-pin)
            make --no-print-directory -C "$root_dir/firmwares/$app/tools" \
                BIN="$work_dir/fluid" CC="${CC:-cc}" CFLAGS="${flags[*]}" all
            for test in "$work_dir"/fluid/test-* "$work_dir/fluid/solver-validation"; do "$test"; done
            if [[ "$soak" == true ]]; then
                "$work_dir/fluid/test-axis-translation" --soak
                "$work_dir/fluid/solver-validation" --soak
            fi ;;
        pet-pin)
            run_test "$app" pet tests/test_pet.c main/pet.c main/paint.c main/input.c ;;
        render-pin)
            run_test "$app" renderer tests/renderer_test.c main/renderer.c
            run_test "$app" motion tests/motion_test.c main/motion.c ;;
        dungeon-pin)
            run_test "$app" dungeon tests/test_dungeon.c main/dungeon.c main/paint.c main/assets_generated.c
            python3 "$root_dir/firmwares/$app/tests/test_assets.py" ;;
        maze-pin)
            run_test "$app" maze tests/test_maze.c main/maze.c main/paint.c ;;
        wayfarer-pin)
            app_dir="$root_dir/firmwares/$app"
            "${CC:-cc}" "${flags[@]}" -I"$app_dir/main" "$app_dir/tests/test_scene.c" \
                "$app_dir/main/scene.c" "$app_dir/main/assets_generated.c" \
                "$app_dir/main/exterior_generated.c" -o "$work_dir/wayfarer"
            "$work_dir/wayfarer" "$app_dir/main/assets/cockpit.rgb565" ;;
        three-body-pin|crt-pin)
            app_dir="$root_dir/firmwares/$app"
            "${CC:-cc}" "${flags[@]}" -I"$app_dir/main" -I"$root_dir/common" \
                "$app_dir/tests/test_scene.c" "$app_dir/main/scene.c" "$root_dir/common/pin_gfx.c" \
                -lm -o "$work_dir/$app"
            "$work_dir/$app" ;;
    esac
done
echo "Host tests passed."
