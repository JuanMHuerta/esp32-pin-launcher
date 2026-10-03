# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "sdcard", Path(__file__).resolve().parents[1] / "sdcard.py"
)
sdcard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdcard)


def frame(kind, operation, payload=b"", flags=0, status=0):
    return sdcard.HEADER.pack(b"MPFS", 1, kind, operation, flags, status, len(payload)) + payload


class SerialStub:
    def __init__(self, incoming):
        self.incoming = bytearray(incoming)
        self.writes = bytearray()

    def read(self, count):
        # USB reads may split a frame anywhere, including its header.
        count = min(count, 3)
        result = bytes(self.incoming[:count])
        del self.incoming[:count]
        return result

    def write(self, data):
        self.writes.extend(data)
        return len(data)


class SDCardTests(unittest.TestCase):
    def device(self, data):
        return sdcard.Device(SerialStub(data), timeout=0.05)

    def test_path_validation_matches_firmware_limits(self):
        self.assertEqual(sdcard.normalize_remote("/assets/frame.bin"), "assets/frame.bin")
        self.assertEqual(sdcard.normalize_remote("/"), "")
        for value in [
            "../escape",
            "a/../b",
            "a//b",
            "//a",
            "a/",
            "a\0b",
            "a:b",
            "a\nb",
            "x" * 256,
            "é" * 128,
            "a" * 255 + "/" + "b" * 252,
        ]:
            with self.subTest(value=value), self.assertRaises(sdcard.DeviceError):
                sdcard.normalize_remote(value)
        self.assertEqual(len(sdcard.normalize_remote("a" * 255 + "/" + "b" * 251)), 507)

    def test_boot_logs_and_fragmented_frames(self):
        payload = bytes([1, 1, 0, 0]) + struct.pack("<Q", 12345)
        device = self.device(b"boot log\nMnot magic\n" + frame(1, sdcard.OP_HELLO, payload))
        self.assertEqual(device.hello(), (True, 12345))

    def test_device_errors_keep_their_status(self):
        for operation, action in [
            (sdcard.OP_LIST, lambda d: d.list("/")),
            (sdcard.OP_READ, lambda d: d.read_file("file", Path("unused"))),
        ]:
            with self.subTest(operation=operation), self.assertRaises(sdcard.DeviceError) as error:
                action(self.device(frame(1, operation, b"SD card is not mounted", status=4)))
            self.assertEqual(error.exception.status, 4)
            self.assertIn("not mounted", str(error.exception))

    def test_list_rejects_names_that_escape_local_destination(self):
        for name in [b"../escape", b"/absolute", b"a/b", b"a\\b", b"", b"\xff"]:
            entry = b"\0" + struct.pack("<QH", 1, len(name)) + name
            data = frame(1, sdcard.OP_LIST, flags=1) + frame(2, sdcard.OP_LIST, entry)
            with self.subTest(name=name), self.assertRaises(sdcard.DeviceError):
                self.device(data).list()

    def test_read_is_atomic_and_cleans_up_on_truncation(self):
        with tempfile.TemporaryDirectory(prefix="sd-client-test-") as directory:
            target = Path(directory) / "file.bin"
            target.write_bytes(b"old")
            header = frame(1, sdcard.OP_READ, struct.pack("<Q", 3), flags=1)
            tail = frame(1, sdcard.OP_READ, flags=2)
            with self.assertRaises(sdcard.DeviceError):
                self.device(header + frame(2, sdcard.OP_READ, b"ab") + tail).read_file(
                    "file", target
                )
            self.assertEqual(target.read_bytes(), b"old")
            self.assertEqual(list(Path(directory).iterdir()), [target])
            self.device(header + frame(2, sdcard.OP_READ, b"new") + tail).read_file("file", target)
            self.assertEqual(target.read_bytes(), b"new")

    def test_upload_rejects_wrong_acknowledgement_and_aborts(self):
        with tempfile.TemporaryDirectory(prefix="sd-client-test-") as directory:
            source = Path(directory) / "file.bin"
            source.write_bytes(b"abc")
            data = (
                frame(1, sdcard.OP_WRITE_BEGIN)
                + frame(1, sdcard.OP_WRITE_DATA, struct.pack("<Q", 1))
                + frame(1, sdcard.OP_WRITE_ABORT)
            )
            device = self.device(data)
            with self.assertRaisesRegex(sdcard.DeviceError, "byte count"):
                device.write_file(source, "file")
            self.assertTrue(device.serial.writes.endswith(frame(0, sdcard.OP_WRITE_ABORT)))

    def test_oversized_frame_is_rejected(self):
        header = sdcard.HEADER.pack(b"MPFS", 1, 1, 1, 0, 0, 4097)
        with self.assertRaisesRegex(sdcard.DeviceError, "oversized"):
            self.device(header).receive()


if __name__ == "__main__":
    unittest.main()
