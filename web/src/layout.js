// SPDX-License-Identifier: GPL-3.0-only
import { FlasherError } from "./errors.js";
import SparkMD5 from "spark-md5";

export const FLASH_SIZE = 16 * 1024 * 1024;
export const APP_START = 0x20000;
export const BLOCK = 0x10000;

export function md5(data) {
    return SparkMD5.ArrayBuffer.hash(
        data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength),
    );
}

function validateImage(image, partition = false) {
    if (
        !image ||
        !/^firmware\/[a-z0-9-]+\.bin$/.test(image.file) ||
        !Number.isSafeInteger(image.size) ||
        image.size < 24 ||
        !/^[a-f0-9]{64}$/.test(image.sha256)
    ) {
        throw new FlasherError("invalidManifest");
    }
    const capacity = image.capacity;
    if (
        partition &&
        (!Number.isSafeInteger(capacity) ||
            capacity <= 0 ||
            capacity % BLOCK !== 0 ||
            capacity !== Math.ceil(image.size / BLOCK) * BLOCK)
    ) {
        throw new FlasherError("invalidCapacity");
    }
}

export function validateManifest(manifest) {
    if (
        !manifest ||
        manifest.schema !== 1 ||
        manifest.chip !== "ESP32-S3" ||
        manifest.flashSize !== FLASH_SIZE ||
        !Array.isArray(manifest.apps) ||
        manifest.apps.length < 1 ||
        manifest.apps.length > 9
    ) {
        throw new FlasherError("incompatible");
    }
    validateImage(manifest.bootloader);
    if (manifest.bootloader.size > 0x8000)
        throw new FlasherError("bootloaderOverlap");
    validateImage(manifest.launcher, true);
    const labels = new Set();
    for (const app of manifest.apps) {
        validateImage(app, true);
        if (
            !/^[a-z][a-z0-9]{0,14}$/.test(app.id) ||
            labels.has(app.id) ||
            ["factory", "nvs", "otadata"].includes(app.id) ||
            typeof app.name !== "string" ||
            typeof app.description !== "string" ||
            !/^previews\/[a-z0-9]+\.gif$/.test(app.preview)
        )
            throw new FlasherError("invalidCatalog");
        labels.add(app.id);
    }
    return manifest;
}

export function makePlan(manifest, selectedIds) {
    validateManifest(manifest);
    const selected = new Set(selectedIds);
    const apps = manifest.apps.filter((app) => selected.has(app.id));
    if (apps.length !== selected.size)
        throw new FlasherError("unknownSelection");
    if (!apps.length) throw new FlasherError("empty");
    let offset = APP_START;
    const entries = [
        { label: "nvs", type: 1, subtype: 2, offset: 0x9000, capacity: 0x6000 },
        {
            label: "otadata",
            type: 1,
            subtype: 0,
            offset: 0xf000,
            capacity: 0x2000,
        },
    ];
    const images = [];
    for (const [index, image] of [manifest.launcher, ...apps].entries()) {
        if (offset + image.capacity > FLASH_SIZE)
            throw new FlasherError("overflow");
        entries.push({
            label: index === 0 ? "factory" : image.id,
            type: 0,
            subtype: index === 0 ? 0 : 0x0f + index,
            offset,
            capacity: image.capacity,
        });
        images.push({ ...image, address: offset });
        offset += image.capacity;
    }
    return { entries, images, apps, used: offset };
}

export function partitionTable(entries) {
    const table = new Uint8Array(0xc00).fill(0xff);
    const view = new DataView(table.buffer);
    entries.forEach((entry, index) => {
        const at = index * 32;
        view.setUint16(at, 0x50aa, true);
        view.setUint8(at + 2, entry.type);
        view.setUint8(at + 3, entry.subtype);
        view.setUint32(at + 4, entry.offset, true);
        view.setUint32(at + 8, entry.capacity, true);
        table.fill(0, at + 12, at + 32);
        table.set(new TextEncoder().encode(entry.label), at + 12);
    });
    const at = entries.length * 32;
    view.setUint16(at, 0xebeb, true);
    const digest = md5(table.subarray(0, at));
    table.set(
        Uint8Array.from(digest.match(/../g), (hex) => parseInt(hex, 16)),
        at + 16,
    );
    return table;
}

export async function verifyDownload(image, data) {
    const digest = await crypto.subtle.digest("SHA-256", data);
    const hex = Array.from(new Uint8Array(digest), (byte) =>
        byte.toString(16).padStart(2, "0"),
    ).join("");
    if (
        data.length !== image.size ||
        hex !== image.sha256 ||
        data[0] !== 0xe9 ||
        new DataView(data.buffer, data.byteOffset, data.byteLength).getUint16(
            12,
            true,
        ) !== 9
    ) {
        throw new FlasherError("integrity");
    }
}
