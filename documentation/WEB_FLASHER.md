# One-page USB web flasher

[English](WEB_FLASHER.md) · [Español](WEB_FLASHER.es.md)

[Open the USB web flasher](https://juanmhuerta.github.io/esp32-pin-launcher/).

The [web page](../web/index.html) installs the launcher and any selection of the
nine apps on the Waveshare ESP32-S3-Touch-AMOLED-1.91 (SKU 28596). Users need a
desktop browser with Web Serial (Chrome or Edge), a USB data cable, and the page
served over HTTPS. No server application, firmware compilation on the user's
machine, account, or ESP-IDF installation is required.

The page supports English and Spanish. It starts with the browser's language;
the language picker remembers a manual choice. Diagnostic output from esptool
retains its original language.

## Build and preview

Use the existing pinned ESP-IDF 5.5.1 environment and Node.js 22 or newer:

```sh
source /path/to/esp-idf/export.sh
./build-and-flash.sh --build-only
python3 tools/package_web_firmware.py
cd web
npm ci
npm test
npm run build
python3 -m http.server 8080 --directory dist
```

Open `http://localhost:8080`. Localhost is allowed by Web Serial; opening the
HTML file directly or serving over ordinary HTTP on another host is insufficient.
The generated `web/dist/` directory is the complete static release: a single
page, bundled JavaScript, stylesheet, previews, manifest, firmware, and licenses.
It also includes `source.tar.gz` with the matching project sources and
`SOURCE.txt` with dependency source locations and build instructions.
All asset paths are relative, including downloads, so GitHub project Pages
works under `/repository-name/`. Dependencies are bundled locally; the deployed
page does not depend on a JavaScript CDN.

The packager checks the current full build against both the CSV and compiled
partition table, checks ESP32-S3 image headers, copies the images with
content-derived filenames, and emits SHA-256 hashes in the manifest. Always
build all firmware first, especially after changes to the shared app switcher.
Generated payloads, previews and the site output are ignored by Git.
`FIRMWARE_LICENSES.txt` collects license and notice files from ESP-IDF and the
downloaded display/build components. `THIRD_PARTY_LICENSES.txt` contains the
web runtime dependency licenses.

## App selection, menu and Demo

Every install includes the factory launcher. At least one app must be selected.
The browser builds a partition table containing only the chosen apps, in catalog
order, with minimum 64 KiB allocations. Selected apps get contiguous OTA
subtypes starting at `ota_0`; labels preserve app identity. **Sparse OTA
subtypes cannot be used:** ESP-IDF's boot selection uses the installed OTA count.

The launcher discovers apps by partition label. BOOT navigation displays only
installed apps plus Demo, using one or two pages as necessary. USB shortcuts
retain their original identities (`1` Conway through `9` CRT); shortcuts for
omitted apps do nothing. Demo starts the first installed app and the shared
switcher rotates through the installed OTA partitions every five minutes,
wrapping to the first app, including when only one app is installed. Holding
BOOT for 1.5 seconds and releasing returns to the menu and stops Demo.

An install downloads and SHA-256 checks all required images before connecting
to the loader. It rejects chips other than ESP32-S3 and flash sizes other than
16 MB before writing. Espressif's [esptool-js](https://github.com/espressif/esptool-js)
performs compressed writes and compares each written image's MD5 with the
device. Bootloader flash parameters are retained from the IDF build.

Installing replaces the firmware collection and clears NVS and OTA selection
metadata. This resets settings and stale Demo state, and the next boot opens
the factory menu. It does not erase the entire flash chip. Old omitted image
bytes may remain outside the new partitions, but cannot appear in the menu or
Demo. Changing the selection always installs a complete matching set again;
this is not an incremental app installer. microSD is not accessed.

If auto-reset cannot complete after successful verification, the page asks the
user to press RESET. Connection and write failures release the serial port and
allow retry. For manual download mode: hold BOOT, press/release RESET, release
BOOT, then select the new USB Serial/JTAG port. Close other serial monitors.

## Publish on GitHub Pages

The manual [Publish web flasher workflow](../.github/workflows/web-flasher-pages.yml)
builds firmware with ESP-IDF 5.5.1, packages and bundles the site, uploads the
Pages artifact, and deploys it. To publish:

1. Push these files to GitHub.
2. In **Settings → Pages**, choose **GitHub Actions** as the source.
3. In **Actions → Publish web flasher**, run the workflow on the intended branch.
4. Open the URL reported by the deployment job.

This follows GitHub's [custom Pages workflow](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).
Publication is manual; regular pushes and pull requests build a downloadable
`web-flasher` artifact through the existing Checks workflow.

## Validation

```sh
cd web
npm test
npm run format:check
npx playwright install chromium
npm run test:browser
```

After building and packaging firmware, with ESP-IDF activated, also run
`npm run test:release` inside `web/`. It verifies the actual image hashes and
source archive, then checks all 511 generated partition tables against ESP-IDF's
parser and serializer. Both firmware CI and the publishing workflow run it.

Unit tests cover all 511 nonempty selections, table serialization and MD5,
overlap/capacity rejection, download integrity, chip/flash guards, write failure,
reset failure and serial-port cleanup. Browser checks cover selection, empty
selection, canceled connection, unsupported browsers, missing firmware,
repository subpaths and phone layout. Browser tests use synthetic firmware and
do not write to a device. Actual ROM download, on-device menu behavior and
five-minute rotation require a physical board check for firmware changes;
the repository validation record includes the completed board checks.

See the dated [repository validation record](REPOSITORY_VALIDATION.md) for
results and their scope. Local workflow checks do not establish a hosted
GitHub Actions run.
