# SD card file tool

[English](SD_CARD_FILE_TOOL.md) · [Español](SD_CARD_FILE_TOOL.es.md)

The launcher exposes its mounted microSD card over USB Serial/JTAG.
Use the Python client to inspect or copy files without removing the card.

## Scope and lifecycle

The file tool is implemented by the root launcher firmware and communicates
over USB Serial/JTAG. It is not available while one of the app
applications is running. Return to the launcher before using the host tool;
holding BOOT in a running app returns to the launcher.

The host utility is [`tools/sdcard.py`](../tools/sdcard.py). Opening the serial
port can reset the board, so allow it time to boot and perform its handshake.
If more than one ESP32 is connected, always pass `--port` explicitly.

## Firmware workflow

Before changing SD, display, or other hardware-facing code, read the
[board agent contract](AGENTS_WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91.md) and the
[validation ledger](WAVESHARE_ESP32S3_TOUCH_AMOLED_1_91_VALIDATION.md).

For a firmware change, build and flash the root launcher with the project’s
ESP-IDF environment, then use the tool to verify the card:

```sh
source "$IDF_PATH/export.sh"
idf.py -C . build
idf.py -C . -p /dev/ttyACM0 flash

python3 tools/sdcard.py --port /dev/ttyACM0 info
python3 tools/sdcard.py --port /dev/ttyACM0 list /
```

Flashing the launcher is sufficient for file-tool work when the partition layout
is unchanged. If image growth moves a boundary, use the root script to reinstall
the table and all images together.

## Host commands

The utility requires `pyserial`. It can usually find the board automatically
through `/dev/serial/by-id/` or `/dev/ttyACM*`; use `--port` to remove that
ambiguity. Use `--timeout` when testing a slow or busy setup.

```sh
python3 tools/sdcard.py info
python3 tools/sdcard.py list /
python3 tools/sdcard.py put local-file.bin /remote/file.bin
python3 tools/sdcard.py put local-directory /remote/directory
python3 tools/sdcard.py get /remote/file.bin local-file.bin
python3 tools/sdcard.py get /remote/directory local-directory
python3 tools/sdcard.py mkdir /remote/new/directory
python3 tools/sdcard.py rm /remote/file.bin
python3 tools/sdcard.py rm -r /remote/directory
```

`put` and `get` recurse for directories. `mkdir` creates missing parent
directories. `rm` removes a file or an empty directory; `rm -r` is required
for a non-empty directory. The command exits with an error rather than
silently choosing between multiple serial devices.

Remote paths are relative to the SD card root. `/` names the root. The host
rejects empty path components, `.` and `..`; the firmware also rejects
backslashes, colons, control characters, and paths that escape the card root.
The host normalizes backslashes to slashes, so use POSIX-style paths and do
not rely on that normalization. The root itself cannot be deleted. Long
UTF-8 FAT filenames are enabled in the project configuration.

Writes use a temporary `.mpfs.tmp` file and rename it after the transfer, so
an interrupted write does not replace the destination with a partial file.
The launcher never formats a card automatically.

## Board-specific constraints

This project targets the Waveshare ESP32-S3-Touch-AMOLED-1.91, SKU 28596.
The tested board reports ESP32-S3 silicon revision v0.2. The SD configuration
uses Waveshare's `VersionControl_V2` SDMMC 1-bit example:

| Signal | GPIO |
| --- | ---: |
| CLK | 9 |
| CMD | 42 |
| D0 | 8 |

This mapping coexists with the SH8601 display and avoids the display clock on
GPIO47. Do not apply it to another board revision or a different Waveshare
SKU without checking the board contract, ledger, and the vendor example for
that hardware. Silicon revision alone does not identify a PCB revision.

The mount path is `/sd`, and mounting uses `format_if_mount_failed=false`.
Changing the SD bus, pins, FATFS settings, or display relationship is a
hardware change: update the board documentation and add a reusable finding to
the device lessons when the work produces one.

## Ownership and protocol changes

Keep these responsibilities aligned:

- [`main/sdcard.c`](../main/sdcard.c) owns SD mounting, path validation, file
  operations, and the binary protocol implementation.
- [`main/sdcard.h`](../main/sdcard.h) exposes the launcher-facing SD API.
- [`main/main.c`](../main/main.c) owns USB Serial/JTAG setup, launcher command
  dispatch, and feeding framed file-tool input without breaking the existing
  `1`–`9` and `D` launcher commands.
- [`tools/sdcard.py`](../tools/sdcard.py) is the host client and must stay in
  sync with the firmware protocol.

The protocol uses a 16-byte little-endian header, `<4sBBBBII>`:

| Field | Meaning |
| --- | --- |
| magic | `MPFS` |
| version | `1` |
| type | request `0`, response `1`, data `2` |
| opcode | operation identifier |
| flags | `MORE=1`: data frames follow; `END=2`: transfer complete |
| status | response status code |
| payload length | bytes following the header |

Operation identifiers are `HELLO=1`, `STAT=2`, `LIST=3`, `READ=4`,
`WRITE_BEGIN=5`, `WRITE_DATA=6`, `WRITE_END=7`, `WRITE_ABORT=8`,
`MKDIR=9`, and `DELETE=10`. Status `0` means success; the remaining status
codes are not found, invalid request/path, I/O error, card not mounted, busy,
already exists, directory not empty, no space, and protocol error.

Paths use UTF-8, with at most 255 bytes per component and 507 bytes for the
normalized relative path. Empty components, `.`/`..`, control bytes and colons
are rejected.

The maximum framed payload is 4096 bytes. `READ` and `LIST` may return a
response followed by data frames and a final response; writes use
`WRITE_BEGIN`, repeated `WRITE_DATA`, and `WRITE_END`. A reconnecting `HELLO`
aborts any unfinished write. If the protocol changes, update the C
implementation, Python client, this document, and the tests together.

## Verification checklist

Use unique directories on the host and card so validation does not overwrite
application files. From the repository root:

```sh
work_dir=$(mktemp -d)
remote_dir="/repo-check-$(basename "$work_dir")"
trap 'python3 tools/sdcard.py --port "$PORT" rm -r "$remote_dir"; rm -rf -- "$work_dir"' EXIT
python3 tools/sdcard.py --port "$PORT" mkdir "$remote_dir"
python3 tools/sdcard.py --port "$PORT" put documentation "$remote_dir/documentation"
python3 tools/sdcard.py --port "$PORT" get "$remote_dir/documentation" "$work_dir/documentation"
diff -ru documentation "$work_dir/documentation"
```

Set `PORT` to the board's serial device before running this example. The exit
trap removes both temporary directories.

For a firmware or protocol change, also run `git diff --check`, run the host-client tests with `python3 -m unittest discover -s tools/tests`, and perform at least
one binary round trip using a checksum or `cmp`. Leave the SD root in the
same state in which the test found it.

## Troubleshooting

- `No unique ESP32 serial device found`: pass the exact `--port` and close
  other serial monitors that may be holding it.
- `HELLO` or command timeout: confirm the board is in the root launcher, let
  the reset finish, and retry. Use the launcher serial log to distinguish a
  boot problem from a protocol problem.
- `SD card is not mounted`: inspect the launcher log and the board contract;
  do not enable automatic formatting as a workaround.
- Path errors: use card-root-relative POSIX paths and remove `..`, backslashes,
  or unsupported control characters.
- A stale `.mpfs.tmp` file: it is an interrupted-transfer artifact. Inspect it
  before removing it; do not assume it is the intended destination.
