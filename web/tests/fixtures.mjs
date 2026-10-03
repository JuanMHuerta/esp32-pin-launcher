// SPDX-License-Identifier: GPL-3.0-only
import { createHash } from "node:crypto";

export function fixture() {
    const bytes = new Uint8Array(32);
    bytes[0] = 0xe9;
    bytes[12] = 9;
    const image = (name) => ({
        file: `firmware/${name}.bin`,
        size: bytes.length,
        sha256: createHash("sha256").update(bytes).digest("hex"),
        capacity: 65536,
    });
    const ids = [
        "conways",
        "fluid",
        "miso",
        "lumen",
        "dungeon",
        "maze",
        "wayfarer",
        "threebody",
        "crt",
    ];
    const names = [
        "Conway",
        "Fluid",
        "Miso",
        "Lumen",
        "Dungeon",
        "3D Maze",
        "Wayfarer",
        "Three Body",
        "CRT",
    ];
    return {
        bytes,
        manifest: {
            schema: 1,
            chip: "ESP32-S3",
            flashSize: 16 * 1024 * 1024,
            build: "test-fixture",
            bootloader: image("bootloader"),
            launcher: image("launcher"),
            apps: ids.map((id, i) => ({
                ...image(id),
                id,
                name: names[i],
                description: "A scene for your pin.",
                preview: `previews/${id}.gif`,
            })),
        },
    };
}
