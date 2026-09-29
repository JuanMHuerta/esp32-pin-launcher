#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
port=${1:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_A0:85:E3:E7:A7:F0-if00}

if [[ ! -e "$port" ]]; then
    echo "Serial port not found: $port" >&2
    exit 1
fi

for app in conways-pin fluid-pin pet-pin render-pin; do
    idf.py -C "$root_dir/firmwares/$app" build
done
idf.py -C "$root_dir" build
idf.py -C "$root_dir" -p "$port" flash

esptool.py --chip esp32s3 -p "$port" -b 460800 write_flash \
    0xa0000  "$root_dir/firmwares/conways-pin/build/conways_pin.bin" \
    0x1a0000 "$root_dir/firmwares/fluid-pin/build/fluid_pin.bin" \
    0x2a0000 "$root_dir/firmwares/pet-pin/build/miso_pin.bin" \
    0x3a0000 "$root_dir/firmwares/render-pin/build/lumen_pin.bin"

echo "Flashed launcher plus Conway, Fluid, Miso, and Lumen. Reset the board to open the launcher."
