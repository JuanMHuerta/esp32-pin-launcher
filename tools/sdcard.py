#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Copy and manage files on the launcher's mounted microSD card.

The launcher speaks a small binary protocol over ESP32-S3 USB Serial/JTAG.
Paths are always relative to the SD card root; ``/`` names that root.
"""

from __future__ import annotations

import argparse
import glob
import os
from pathlib import Path
import struct
import sys
import tempfile
import time
from typing import Iterable

MAGIC = b"MPFS"
VERSION = 1
FRAME_REQUEST = 0
FRAME_RESPONSE = 1
FRAME_DATA = 2
FRAME_FLAG_MORE = 1 << 0
FRAME_FLAG_END = 1 << 1
HEADER = struct.Struct("<4sBBBBII")
MAX_FRAME_PAYLOAD = 4096
CHUNK = 4096
MAX_REMOTE_PATH_BYTES = 507  # Firmware's 512-byte VFS path includes '/sd/' and NUL.
MAX_ENTRY_NAME_BYTES = 255

OP_HELLO = 1
OP_STAT = 2
OP_LIST = 3
OP_READ = 4
OP_WRITE_BEGIN = 5
OP_WRITE_DATA = 6
OP_WRITE_END = 7
OP_WRITE_ABORT = 8
OP_MKDIR = 9
OP_DELETE = 10

STATUS_NAMES = {
    1: "not found",
    2: "invalid path or request",
    3: "I/O error",
    4: "SD card is not mounted",
    5: "another operation is active",
    6: "already exists",
    7: "directory is not empty",
    8: "not enough space",
    9: "protocol error",
}


class DeviceError(RuntimeError):
    def __init__(self, message: str, *, status: int | None = None) -> None:
        super().__init__(message)
        self.status = status


def normalize_remote(value: str) -> str:
    """Return a safe, slash-separated path relative to the SD root."""

    value = value.replace("\\", "/")
    if value in ("", "/"):
        return ""
    if value.startswith("/"):
        value = value[1:]
    parts = value.split("/")
    if not parts or any(part in ("", ".", "..") for part in parts):
        raise DeviceError(f"invalid SD path: {value!r}")
    if any(ord(character) < 32 or character == ":" for character in value):
        raise DeviceError(f"invalid SD path: {value!r}")
    if any(len(part.encode("utf-8")) > MAX_ENTRY_NAME_BYTES for part in parts):
        raise DeviceError("SD filename is too long")
    result = "/".join(parts)
    encoded = result.encode("utf-8")
    if len(encoded) > MAX_REMOTE_PATH_BYTES:
        raise DeviceError("SD path is too long")
    return result


def path_payload(value: str) -> bytes:
    encoded = normalize_remote(value).encode("utf-8")
    return struct.pack("<H", len(encoded)) + encoded


def join_remote(directory: str, name: str) -> str:
    return normalize_remote(f"{directory}/{name}" if directory else name)


def response_error(status: int, payload: bytes) -> DeviceError:
    detail = payload.decode("utf-8", errors="replace") if payload else ""
    return DeviceError(
        f"device error {status} ({STATUS_NAMES.get(status, detail or 'unknown')})", status=status
    )


class Device:
    def __init__(self, serial_port, timeout: float) -> None:
        self.serial = serial_port
        self.timeout = timeout

    def send(self, frame_type: int, operation: int, payload: bytes = b"", flags: int = 0) -> None:
        if len(payload) > MAX_FRAME_PAYLOAD:
            raise DeviceError(f"frame payload exceeds {MAX_FRAME_PAYLOAD} bytes")
        header = HEADER.pack(MAGIC, VERSION, frame_type, operation, flags, 0, len(payload))
        try:
            written = self.serial.write(header + payload)
        except OSError as exc:
            raise DeviceError(f"USB write failed: {exc}") from exc
        if written != len(header) + len(payload):
            raise DeviceError("USB write ended before the complete frame was sent")

    def _read_exact(self, count: int) -> bytes:
        result = bytearray()
        deadline = time.monotonic() + self.timeout
        while len(result) < count:
            if time.monotonic() >= deadline:
                raise DeviceError("timed out waiting for device")
            chunk = self.serial.read(count - len(result))
            if chunk:
                result.extend(chunk)
        return bytes(result)

    def receive(self) -> tuple[int, int, int, int, bytes]:
        """Receive one frame, skipping boot logs before the binary magic."""

        magic_window = bytearray()
        deadline = time.monotonic() + self.timeout
        while True:
            if time.monotonic() >= deadline:
                raise DeviceError("timed out waiting for device response")
            byte = self.serial.read(1)
            if not byte:
                continue
            magic_window += byte
            if len(magic_window) > len(MAGIC):
                del magic_window[0]
            if bytes(magic_window) == MAGIC:
                break
        rest = self._read_exact(HEADER.size - len(MAGIC))
        magic, version, frame_type, operation, flags, status, length = HEADER.unpack(MAGIC + rest)
        if magic != MAGIC or version != VERSION:
            raise DeviceError("invalid device protocol header")
        if length > MAX_FRAME_PAYLOAD:
            raise DeviceError(f"device sent an oversized frame ({length} bytes)")
        return frame_type, operation, flags, status, self._read_exact(length)

    def response(self, operation: int, *, flags: int = 0) -> bytes:
        frame_type, received_operation, received_flags, status, payload = self.receive()
        if frame_type != FRAME_RESPONSE or received_operation != operation:
            raise DeviceError("unexpected device response")
        if status:
            raise response_error(status, payload)
        if flags and not received_flags & flags:
            raise DeviceError("device response ended unexpectedly")
        return payload

    def request(self, operation: int, payload: bytes = b"") -> bytes:
        self.send(FRAME_REQUEST, operation, payload)
        return self.response(operation)

    def hello(self) -> tuple[bool, int]:
        payload = self.request(OP_HELLO)
        if len(payload) != 12:
            raise DeviceError("malformed hello response")
        version, mounted = payload[0], payload[1]
        if version != VERSION:
            raise DeviceError(f"unsupported device protocol version {version}")
        sectors = struct.unpack_from("<Q", payload, 4)[0]
        return bool(mounted), sectors

    def stat(self, remote: str) -> tuple[bool, int]:
        payload = self.request(OP_STAT, path_payload(remote))
        if len(payload) != 12:
            raise DeviceError("malformed stat response")
        is_directory = bool(payload[0])
        size = struct.unpack_from("<Q", payload, 4)[0]
        return is_directory, size

    def list(self, remote: str = "") -> list[tuple[str, bool, int]]:
        self.send(FRAME_REQUEST, OP_LIST, path_payload(remote))
        frame_type, operation, flags, status, payload = self.receive()
        if frame_type != FRAME_RESPONSE or operation != OP_LIST:
            raise DeviceError("malformed list response")
        if status:
            raise response_error(status, payload)
        if not flags & FRAME_FLAG_MORE:
            raise DeviceError("malformed list response")
        entries: list[tuple[str, bool, int]] = []
        while True:
            frame_type, operation, flags, status, payload = self.receive()
            if operation != OP_LIST:
                raise DeviceError("unexpected list frame")
            if frame_type == FRAME_DATA:
                if len(payload) < 11:
                    raise DeviceError("malformed directory entry")
                is_directory = bool(payload[0])
                size = struct.unpack_from("<Q", payload, 1)[0]
                name_length = struct.unpack_from("<H", payload, 9)[0]
                if len(payload) != 11 + name_length:
                    raise DeviceError("malformed directory entry name")
                try:
                    name = payload[11:].decode("utf-8")
                except UnicodeDecodeError as exc:
                    raise DeviceError("invalid directory entry encoding") from exc
                if not name or "/" in name or "\\" in name or normalize_remote(name) != name:
                    raise DeviceError("invalid directory entry name")
                entries.append((name, is_directory, size))
            elif frame_type == FRAME_RESPONSE and flags & FRAME_FLAG_END:
                if status:
                    raise response_error(status, payload)
                return entries
            else:
                raise DeviceError("malformed list terminator")

    def read_file(self, remote: str, local: Path) -> None:
        self.send(FRAME_REQUEST, OP_READ, path_payload(remote))
        frame_type, operation, flags, status, payload = self.receive()
        if frame_type != FRAME_RESPONSE or operation != OP_READ:
            raise DeviceError("malformed read response")
        if status:
            raise response_error(status, payload)
        if not flags & FRAME_FLAG_MORE:
            raise DeviceError("malformed read response")
        if len(payload) != 8:
            raise DeviceError("malformed read size")
        remaining = struct.unpack("<Q", payload)[0]
        local.parent.mkdir(parents=True, exist_ok=True)
        output = tempfile.NamedTemporaryFile(
            prefix=local.name + ".", suffix=".mpfs.tmp", dir=local.parent, delete=False
        )
        temporary = Path(output.name)
        try:
            with output:
                while True:
                    frame_type, operation, flags, status, payload = self.receive()
                    if operation != OP_READ:
                        raise DeviceError("unexpected read frame")
                    if frame_type == FRAME_DATA:
                        if len(payload) > remaining:
                            raise DeviceError("device sent more data than declared")
                        output.write(payload)
                        remaining -= len(payload)
                    elif frame_type == FRAME_RESPONSE and flags & FRAME_FLAG_END:
                        if status:
                            raise response_error(status, payload)
                        if remaining:
                            raise DeviceError("device ended read before the declared size")
                        break
                    else:
                        raise DeviceError("malformed read terminator")
            os.replace(temporary, local)
        except Exception:
            temporary.unlink(missing_ok=True)
            raise

    def write_file(self, local: Path, remote: str) -> None:
        size = local.stat().st_size
        begin = path_payload(remote) + struct.pack("<QB", size, 0)
        self.send(FRAME_REQUEST, OP_WRITE_BEGIN, begin)
        self.response(OP_WRITE_BEGIN)
        sent = 0
        try:
            with local.open("rb") as source:
                while True:
                    chunk = source.read(CHUNK)
                    if not chunk:
                        break
                    self.send(FRAME_DATA, OP_WRITE_DATA, chunk)
                    acknowledgement = self.response(OP_WRITE_DATA)
                    if len(acknowledgement) != 8:
                        raise DeviceError("malformed write acknowledgement")
                    sent += len(chunk)
                    if struct.unpack("<Q", acknowledgement)[0] != sent:
                        raise DeviceError("device acknowledged an unexpected byte count")
            if sent != size:
                raise DeviceError(f"local file changed while reading ({sent}/{size} bytes)")
            self.send(FRAME_REQUEST, OP_WRITE_END)
            self.response(OP_WRITE_END)
        except Exception:
            try:
                self.send(FRAME_REQUEST, OP_WRITE_ABORT)
                self.response(OP_WRITE_ABORT)
            except DeviceError:
                pass
            raise

    def mkdir(self, remote: str, parents: bool = True) -> None:
        self.request(OP_MKDIR, path_payload(remote) + bytes([parents]))

    def delete(self, remote: str, recursive: bool = False) -> None:
        self.request(OP_DELETE, path_payload(remote) + bytes([recursive]))


def open_device(port: str | None, timeout: float) -> tuple[Device, object]:
    try:
        import serial
    except ImportError as exc:
        raise DeviceError("pyserial is required; use the ESP-IDF Python environment") from exc

    if port is None:
        candidates = sorted(
            glob.glob("/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00")
        )
        if not candidates:
            candidates = sorted(glob.glob("/dev/ttyACM*"))
        if len(candidates) != 1:
            raise DeviceError("pass --port; could not choose a unique ESP32 USB serial device")
        port = candidates[0]
    try:
        serial_port = serial.Serial(port, 115200, timeout=0.1, write_timeout=timeout)
    except Exception as exc:
        raise DeviceError(f"could not open {port}: {exc}") from exc
    device = Device(serial_port, timeout)
    # Opening USB Serial/JTAG commonly resets the board.  Let the launcher
    # mount the card and finish its display bring-up before handshaking.
    time.sleep(1.0)
    serial_port.reset_input_buffer()
    last_error: Exception | None = None
    for _ in range(5):
        try:
            device.hello()
            return device, serial_port
        except DeviceError as exc:
            last_error = exc
            time.sleep(0.3)
    serial_port.close()
    raise DeviceError(f"device did not answer the SD protocol: {last_error}")


def remote_exists_as_directory(device: Device, remote: str) -> bool:
    try:
        return device.stat(remote)[0]
    except DeviceError as exc:
        if exc.status == 1:
            return False
        raise


def push(device: Device, local: Path, remote: str) -> None:
    remote = normalize_remote(remote)
    if local.is_dir():
        device.mkdir(remote, parents=True)
        for child in sorted(local.iterdir()):
            push(device, child, join_remote(remote, child.name))
    elif local.is_file():
        if remote_exists_as_directory(device, remote):
            remote = join_remote(remote, local.name)
        device.write_file(local, remote)
    else:
        raise DeviceError(f"unsupported local path: {local}")


def pull(device: Device, remote: str, local: Path) -> None:
    remote = normalize_remote(remote)
    is_directory, _ = device.stat(remote)
    if is_directory:
        local.mkdir(parents=True, exist_ok=True)
        for name, entry_is_directory, _ in device.list(remote):
            child_remote = join_remote(remote, name)
            child_local = local / name
            if entry_is_directory:
                pull(device, child_remote, child_local)
            else:
                device.read_file(child_remote, child_local)
    else:
        if local.is_dir():
            local = local / Path(remote).name
        device.read_file(remote, local)


def print_listing(entries: Iterable[tuple[str, bool, int]]) -> None:
    for name, is_directory, size in sorted(
        entries, key=lambda entry: (not entry[1], entry[0].lower())
    ):
        print(f"{'d' if is_directory else 'f'} {size:>10} {name}{'/' if is_directory else ''}")


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="ESP32 USB Serial/JTAG device (auto-detected when unique)")
    parser.add_argument("--timeout", type=float, default=5.0, help="per-frame timeout in seconds")
    commands = parser.add_subparsers(dest="command", required=True)

    info = commands.add_parser("info", help="show SD mount status")
    info.set_defaults(action="info")

    listing = commands.add_parser("list", aliases=["ls"], help="list a file or folder")
    listing.add_argument("remote", nargs="?", default="/")
    listing.set_defaults(action="list")

    get = commands.add_parser("get", help="copy a file or folder from the SD card")
    get.add_argument("remote")
    get.add_argument("local", type=Path)
    get.set_defaults(action="get")

    put = commands.add_parser("put", help="copy a file or folder to the SD card")
    put.add_argument("local", type=Path)
    put.add_argument("remote")
    put.set_defaults(action="put")

    mkdir = commands.add_parser("mkdir", help="create a folder")
    mkdir.add_argument("remote")
    mkdir.add_argument("-p", "--parents", action="store_true", default=True)
    mkdir.set_defaults(action="mkdir")

    remove = commands.add_parser("rm", help="delete a file or folder")
    remove.add_argument("remote")
    remove.add_argument("-r", "--recursive", action="store_true")
    remove.set_defaults(action="rm")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = make_parser().parse_args(argv)
    try:
        device, serial_port = open_device(args.port, args.timeout)
        try:
            if args.action == "info":
                mounted, sectors = device.hello()
                print(f"mounted={'yes' if mounted else 'no'} sectors={sectors}")
            elif args.action == "list":
                print_listing(device.list(args.remote))
            elif args.action == "get":
                pull(device, args.remote, args.local)
            elif args.action == "put":
                push(device, args.local, args.remote)
            elif args.action == "mkdir":
                device.mkdir(args.remote, args.parents)
            elif args.action == "rm":
                device.delete(args.remote, args.recursive)
        finally:
            serial_port.close()
        return 0
    except (DeviceError, OSError) as exc:
        print(f"sdcard: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
