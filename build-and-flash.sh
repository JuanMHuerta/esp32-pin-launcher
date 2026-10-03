#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

usage() {
    echo "Usage: $0 --build-only | SERIAL_PORT"
    echo "Build the launcher and all apps. With a serial port, flash and verify all images."
}

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 2
fi
case "$1" in
    --help|-h) usage; exit 0 ;;
    --build-only) build_only=true ;;
    -*) usage >&2; exit 2 ;;
    *) build_only=false; port=$1 ;;
esac

if ! command -v idf.py >/dev/null; then
    echo "Source your ESP-IDF 5.5.x export.sh before running this script." >&2
    exit 1
fi
if [[ "$build_only" == false && ! -e "$port" ]]; then
    echo "Serial port not found: $port" >&2
    exit 1
fi

project_list=$(python3 "$root_dir/tools/app_layout.py" projects)
mapfile -t projects <<< "$project_list"
for project in "${projects[@]}"; do
    idf.py -C "$project" build
done

# Size the launcher before checking it against the regenerated partition table.
idf.py -C "$root_dir" reconfigure
cmake --build "$root_dir/build" --target gen_project_binary
python3 "$root_dir/tools/app_layout.py" generate
idf.py -C "$root_dir" build
python3 "$root_dir/tools/app_layout.py" check --compiled

if [[ "$build_only" == true ]]; then
    echo "Built launcher and ${#projects[@]} apps."
    exit 0
fi

# One argument per line preserves image paths containing spaces.
app_args=$(python3 "$root_dir/tools/app_layout.py" flash-args)
mapfile -t app_flash_args <<< "$app_args"
idf.py -C "$root_dir" -p "$port" flash
esptool.py --chip esp32s3 -p "$port" -b 460800 write_flash "${app_flash_args[@]}"
esptool.py --chip esp32s3 -p "$port" -b 460800 verify_flash "${app_flash_args[@]}"

# Clearing OTA selection makes the next reset boot the factory launcher.
esptool.py --chip esp32s3 -p "$port" -b 460800 erase_region 0xf000 0x2000
echo "Flashed and verified launcher and ${#projects[@]} apps. Reset the board to open the launcher."
