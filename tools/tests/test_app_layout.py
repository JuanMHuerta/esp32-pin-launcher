# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "app_layout", Path(__file__).resolve().parents[1] / "app_layout.py"
)
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


class LayoutTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="app-layout-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for _, _, relative in layout.IMAGES:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"\xe9")

    def resize(self, index, size):
        with (self.root / layout.IMAGES[index][2]).open("r+b") as image:
            image.truncate(size)

    def test_minimum_rounding_and_contiguous_offsets(self):
        sizes = [1, 65535, 65536, 65537, 269424, 341728, 743648, 654464]
        expected_blocks = [1, 1, 1, 2, 5, 6, 12, 10]
        for index, size in enumerate(sizes):
            self.resize(index, size)
        entries = layout.allocate(self.root)
        offset = layout.APP_START
        for entry in entries:
            self.assertEqual(entry[3], offset)
            self.assertEqual(entry[3] % 65536, 0)
            self.assertGreaterEqual(entry[4], entry[5])
            self.assertLess(entry[4] - entry[5], 65536)
            offset += entry[4]
        self.assertEqual([e[4] // 65536 for e in entries[: len(sizes)]], expected_blocks)

    def test_missing_and_empty_images_fail(self):
        self.resize(1, 0)
        with self.assertRaisesRegex(ValueError, "Empty firmware"):
            layout.allocate(self.root)
        (self.root / layout.IMAGES[1][2]).unlink()
        with self.assertRaises(FileNotFoundError):
            layout.allocate(self.root)

    def test_board_capacity_boundary(self):
        # Other images each need one block; the last address can equal 16 MiB.
        size = layout.FLASH_SIZE - layout.APP_START - (len(layout.IMAGES) - 1) * layout.BLOCK_SIZE
        self.resize(0, size)
        entries = layout.allocate(self.root)
        self.assertEqual(entries[-1][3] + entries[-1][4], layout.FLASH_SIZE)
        self.resize(0, size + 1)
        with self.assertRaisesRegex(ValueError, "16 MiB"):
            layout.allocate(self.root)

    def test_changed_image_requires_regeneration(self):
        entries = layout.allocate(self.root)
        (self.root / "partitions.csv").write_text(layout.render_csv(entries))
        layout.check_layout(self.root, entries)
        self.resize(2, 65537)
        with self.assertRaisesRegex(ValueError, "partitions.csv is stale"):
            layout.check_layout(self.root, layout.allocate(self.root))

    def test_compiled_table_must_match_before_flashing(self):
        entries = layout.allocate(self.root)
        (self.root / "partitions.csv").write_text(layout.render_csv(entries))
        records = [
            ("nvs", 1, 2, 0x9000, 0x6000),
            ("otadata", 1, 0, 0xF000, 0x2000),
        ]
        records += [(e[0], 0, 0 if i == 0 else 0x0F + i, e[3], e[4]) for i, e in enumerate(entries)]
        data = bytearray()
        for label, kind, subtype, offset, size in records:
            data += struct.pack(
                "<HBBII16sI", 0x50AA, kind, subtype, offset, size, label.encode(), 0
            )
        data += b"\xff" * 32
        path = self.root / "build/partition_table/partition-table.bin"
        path.parent.mkdir(parents=True)
        path.write_bytes(data)
        layout.check_layout(self.root, entries, compiled=True)
        # Corrupt a app address while leaving the CSV correct.
        struct.pack_into("<I", data, 3 * 32 + 4, 0xA0000)
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, "Compiled partition table is stale"):
            layout.check_layout(self.root, entries, compiled=True)


if __name__ == "__main__":
    unittest.main()
