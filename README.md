# Multi Pin Launcher

This project keeps seven separately bootable firmware images for the Waveshare
ESP32-S3-Touch-AMOLED-1.91 and provides a factory launcher to select one.

The cartridge sources live in `firmwares/`; the original projects beside this
directory are not used or changed. The launcher occupies the factory partition;
Conway, Fluid, Miso, Lumen, MECH//BAY-07, and DUNGEON//SEED occupy `ota_0`
through `ota_5` respectively. 3D MAZE occupies `ota_6` at `0x6a0000`.

At the launcher, tap BOOT briefly to select the next entry and hold it for
0.7 seconds to start the highlighted image. USB Serial/JTAG commands `1`–`7`
start the corresponding image. While a copied app is running, holding BOOT for
1.5 seconds returns to this launcher.

Run `./build-and-flash.sh` after sourcing ESP-IDF to build every copied image,
flash the factory launcher and seven app partitions, and verify the flash writes.
