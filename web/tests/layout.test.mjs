// SPDX-License-Identifier: GPL-3.0-only
import { test } from "node:test";
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
    makePlan,
    partitionTable,
    validateManifest,
    verifyDownload,
} from "../src/layout.js";
import { fixture } from "./fixtures.mjs";

test("all 511 app combinations have contiguous OTA subtypes, aligned non-overlapping images and valid MD5", () => {
    const { manifest } = fixture();
    for (let mask = 1; mask < 512; mask++) {
        const ids = manifest.apps
            .filter((_, i) => mask & (1 << i))
            .map((app) => app.id);
        const plan = makePlan(manifest, ids.reverse());
        const entries = plan.entries;
        assert.equal(entries[2].label, "factory");
        assert.equal(entries[2].offset, 0x20000);
        assert.deepEqual(
            entries.slice(3).map((e) => e.subtype),
            ids.map((_, i) => 0x10 + i),
        );
        assert.deepEqual(
            plan.apps.map((a) => a.id),
            manifest.apps.filter((a) => ids.includes(a.id)).map((a) => a.id),
        );
        for (let i = 2; i < entries.length; i++) {
            assert.equal(entries[i].offset % 65536, 0);
            assert.equal(
                entries[i].offset,
                i === 2
                    ? 0x20000
                    : entries[i - 1].offset + entries[i - 1].capacity,
            );
        }
        const table = partitionTable(entries);
        const view = new DataView(table.buffer);
        for (let i = 0; i < entries.length; i++) {
            assert.equal(view.getUint16(i * 32, true), 0x50aa);
            assert.equal(view.getUint8(i * 32 + 3), entries[i].subtype);
            assert.equal(view.getUint32(i * 32 + 4, true), entries[i].offset);
            assert.equal(view.getUint32(i * 32 + 8, true), entries[i].capacity);
        }
        const md5at = entries.length * 32;
        assert.equal(view.getUint16(md5at, true), 0xebeb);
        assert.equal(
            Buffer.from(table.subarray(md5at + 16, md5at + 32)).toString("hex"),
            createHash("md5").update(table.subarray(0, md5at)).digest("hex"),
        );
        assert.equal(table.length, 0xc00);
        assert(table.subarray(md5at + 32).every((byte) => byte === 255));
    }
});

test("rejects empty, unknown, overflowing and invalid release selections", () => {
    const { manifest } = fixture();
    assert.throws(() => makePlan(manifest, []), /Choose/);
    assert.throws(() => makePlan(manifest, ["unknown"]), /Unknown/);
    const overflow = structuredClone(manifest);
    overflow.launcher.size = 16 * 1024 * 1024;
    overflow.launcher.capacity = overflow.launcher.size;
    assert.throws(() => makePlan(overflow, ["miso"]), /exceeds/);
    const duplicate = structuredClone(manifest);
    duplicate.apps[1].id = duplicate.apps[0].id;
    assert.throws(() => validateManifest(duplicate), /catalog/);
    const path = structuredClone(manifest);
    path.bootloader.file = "https://other.example/fw.bin";
    assert.throws(() => validateManifest(path), /manifest/);
    const capacity = structuredClone(manifest);
    capacity.apps[0].capacity = 100;
    assert.throws(() => validateManifest(capacity), /capacity/);
});

test("rejects tampered or wrong-chip downloads before connecting", async () => {
    const { manifest, bytes } = fixture();
    await verifyDownload(manifest.launcher, bytes);
    const tampered = bytes.slice();
    tampered[20] ^= 1;
    await assert.rejects(
        verifyDownload(manifest.launcher, tampered),
        /integrity/,
    );
    const wrongChip = bytes.slice();
    wrongChip[12] = 0;
    const image = {
        ...manifest.launcher,
        sha256: createHash("sha256").update(wrongChip).digest("hex"),
    };
    await assert.rejects(verifyDownload(image, wrongChip), /integrity/);
});

test("missing capacities and reserved partition labels cannot reach the installer", () => {
    const { manifest } = fixture();
    for (const target of ["launcher", "app"]) {
        for (const capacity of [null, undefined, -65536, 0, 1.5]) {
            const broken = structuredClone(manifest);
            const image =
                target === "launcher" ? broken.launcher : broken.apps[0];
            image.capacity = capacity;
            assert.throws(() => makePlan(broken, ["miso"]), /capacity/);
        }
    }
    for (const id of ["factory", "nvs", "otadata"]) {
        const broken = structuredClone(manifest);
        broken.apps[0].id = id;
        assert.throws(() => validateManifest(broken), /catalog/);
    }
    assert.throws(() => validateManifest(null), /incompatible/);
});
