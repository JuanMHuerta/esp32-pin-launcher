# Multi Pin Launcher

This project keeps four separately bootable firmware images for the Waveshare
ESP32-S3-Touch-AMOLED-1.91 and provides a factory launcher to select one.

The copied source projects live in `firmwares/`; the originals beside this
directory are not used or changed. The launcher occupies the factory partition;
Conway, Fluid, Miso, and Lumen occupy `ota_0` through `ota_3` respectively.

At the launcher, tap BOOT briefly to select the next entry and hold it for
0.7 seconds to start the highlighted image. USB Serial/JTAG commands `1`–`4`
start the corresponding image. While a copied app is running, holding BOOT for
1.5 seconds returns to this launcher.

Run `./build-and-flash.sh` after sourcing ESP-IDF to build every copied image,
flash the factory launcher and four app partitions, and verify the flash writes.
