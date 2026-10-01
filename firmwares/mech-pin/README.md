# MECH//BAY-07

The first game-cartridge firmware: an original mech hangar attract loop for the
Waveshare ESP32-S3 Touch AMOLED 1.91. It boots into a brief cartridge card,
then cycles through standby, diagnostic scan, reactor charge, launch, and
return. The silhouette and cyan/amber/red palette are designed to read at
backpack distance on the AMOLED's black background.

A short BOOT press arms an immediate launch. Hold BOOT for 1.5 seconds to
return to the factory cartridge library. It needs no network, account, touch
controller, or sensor to run.

## Build and test

```bash
. /home/juan/.local/share/esp-idf/export.sh
gcc -std=c11 -Wall -Wextra -Werror tests/test_mech.c main/mech.c main/paint.c -o /tmp/mech-test
/tmp/mech-test
idf.py build
idf.py -p /dev/ttyACM0 flash
```

The collection-level `./build-and-flash.sh` also updates the launcher partition
table and flashes MECH//BAY-07 into `ota_4` at `0x4a0000`.
