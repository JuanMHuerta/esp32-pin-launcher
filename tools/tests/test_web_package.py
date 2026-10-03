# SPDX-License-Identifier: GPL-3.0-only
import hashlib
import json
import os
from pathlib import Path
import struct
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import app_layout
import package_web_firmware


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="web-package-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "project"
        self.root.mkdir()
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)
        (self.root / ".gitignore").write_text("**/build/\n**/managed_components/\nweb/public/\n")
        (self.root / "README.md").write_text("Fixture source\n")
        script = self.root / "build-and-flash.sh"
        script.write_text("#!/bin/sh\nexit 0\n")
        script.chmod(0o755)
        self.image = bytes([0xE9]) + bytes(11) + bytes([9]) + bytes(19)
        for (_, _, relative), (_, _, preview) in zip(
            app_layout.IMAGES[1:], package_web_firmware.APP_DETAILS, strict=True
        ):
            path = self.root / relative
            path.parent.mkdir(parents=True)
            path.write_bytes(self.image)
            (path.parent.parent / preview).write_bytes(b"GIF89a")
        launcher = self.root / app_layout.IMAGES[0][2]
        launcher.parent.mkdir(parents=True)
        launcher.write_bytes(self.image)
        bootloader = self.root / "build/bootloader/bootloader.bin"
        bootloader.parent.mkdir()
        bootloader.write_bytes(self.image)
        self.settings = {
            "extra_esptool_args": {"chip": "esp32s3"},
            "flash_settings": {"flash_size": "16MB"},
            "bootloader": {"offset": "0x0", "file": "bootloader/bootloader.bin"},
        }
        self.settings_path = self.root / "build/flasher_args.json"
        self.settings_path.write_text(json.dumps(self.settings))
        entries = app_layout.allocate(self.root)
        (self.root / "partitions.csv").write_text(app_layout.render_csv(entries))
        records = [("nvs", 1, 2, 0x9000, 0x6000), ("otadata", 1, 0, 0xF000, 0x2000)]
        records += [(e[0], 0, 0 if i == 0 else 0x0F + i, e[3], e[4]) for i, e in enumerate(entries)]
        table = (
            b"".join(
                struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size, label.encode(), 0)
                for label, kind, subtype, offset, size in records
            )
            + b"\xff" * 32
        )
        self.table_path = self.root / "build/partition_table/partition-table.bin"
        self.table_path.parent.mkdir()
        self.table_path.write_bytes(table)
        idf = Path(self.temp.name) / "idf"
        (idf / "components/example").mkdir(parents=True)
        (idf / "LICENSE").write_text("IDF license text\n")
        (idf / "components/example/COPYING").write_text("Component notice\n")
        component = self.root / "managed_components/espressif__example"
        component.mkdir(parents=True)
        (component / "license.txt").write_text("Managed component license\n")
        (self.root / "build/project_description.json").write_text(
            json.dumps({"idf_path": str(idf), "build_component_paths": [str(component)]})
        )
        self.output = self.root / "web/public"

    def test_packaged_images_notices_and_matching_source(self):
        manifest = package_web_firmware.package(self.root, self.output)
        self.assertEqual(len(manifest["apps"]), 9)
        for image in [manifest["bootloader"], manifest["launcher"], *manifest["apps"]]:
            data = (self.output / image["file"]).read_bytes()
            self.assertEqual(data, self.image)
            self.assertEqual(image["sha256"], hashlib.sha256(data).hexdigest())
        source = manifest["source"]
        self.assertEqual(
            source["sha256"],
            hashlib.sha256((self.output / source["file"]).read_bytes()).hexdigest(),
        )
        with tarfile.open(self.output / source["file"]) as archive:
            names = archive.getnames()
            self.assertIn("multi-pin-launcher/README.md", names)
            self.assertFalse(any("/build/" in name or "/web/public/" in name for name in names))
            self.assertEqual(archive.getmember("multi-pin-launcher/build-and-flash.sh").mode, 0o755)
        notices = (self.output / "FIRMWARE_LICENSES.txt").read_text()
        for expected in ["IDF license text", "Component notice", "Managed component license"]:
            self.assertIn(expected, notices)
        again = package_web_firmware.package(self.root, self.output)
        self.assertEqual(manifest, again)
        (self.root / "README.md").write_text("Changed fixture source\n")
        changed = package_web_firmware.package(self.root, self.output)
        self.assertNotEqual(source["sha256"], changed["source"]["sha256"])

    def test_container_checkout_with_different_owner_can_be_packaged(self):
        environment = {
            "GIT_TEST_ASSUME_DIFFERENT_OWNER": "1",
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_CONFIG_NOSYSTEM": "1",
        }
        with patch.dict(os.environ, environment):
            untrusted = subprocess.run(
                ["git", "ls-files"], cwd=self.root, capture_output=True, text=True
            )
            self.assertNotEqual(untrusted.returncode, 0)
            self.assertIn("dubious ownership", untrusted.stderr)
            manifest = package_web_firmware.package(self.root, self.output)
        with tarfile.open(self.output / manifest["source"]["file"]) as archive:
            self.assertIn("multi-pin-launcher/README.md", archive.getnames())

    def test_wrong_chip_and_stale_layout_do_not_publish_a_manifest(self):
        path = self.root / app_layout.IMAGES[2][2]
        broken = bytearray(self.image)
        broken[12] = 0
        path.write_bytes(broken)
        with self.assertRaisesRegex(ValueError, "Not an ESP32-S3"):
            package_web_firmware.package(self.root, self.output)

        self.assertFalse((self.output / "manifest.json").exists())
        path.write_bytes(self.image)
        broken = bytearray(self.table_path.read_bytes())
        struct.pack_into("<I", broken, 3 * 32 + 4, 0x40000)
        self.table_path.write_bytes(broken)
        with self.assertRaisesRegex(ValueError, "Compiled partition table is stale"):
            package_web_firmware.package(self.root, self.output)

    def test_source_archive_can_be_repackaged_without_git(self):
        manifest = package_web_firmware.package(self.root, self.output)
        shutil.rmtree(self.root / ".git")
        repackaged = package_web_firmware.package(self.root, self.output)
        self.assertEqual(manifest, repackaged)

    def test_wrong_flash_and_bootloader_location_are_rejected(self):
        for section, field, value, message in [
            ("flash_settings", "flash_size", "8MB", "16 MB"),
            ("bootloader", "offset", "0x1000", "bootloader offset"),
        ]:
            settings = json.loads(json.dumps(self.settings))
            settings[section][field] = value
            self.settings_path.write_text(json.dumps(settings))
            with self.assertRaisesRegex(ValueError, message):
                package_web_firmware.package(self.root, self.output)
            self.assertFalse((self.output / "manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
