// SPDX-License-Identifier: GPL-3.0-only
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdtemp, readFile, writeFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import {
    makePlan,
    partitionTable,
    validateManifest,
    verifyDownload,
} from "../src/layout.js";

if (!process.env.IDF_PATH)
    throw new Error("Source ESP-IDF export.sh before checking a release.");
const manifest = validateManifest(
    JSON.parse(await readFile("public/manifest.json", "utf8")),
);
assert.equal(manifest.apps.length, 9, "The release must offer all nine apps.");
const source = await readFile(`public/${manifest.source.file}`);
assert.equal(source.length, manifest.source.size);
assert.equal(
    createHash("sha256").update(source).digest("hex"),
    manifest.source.sha256,
);
for (const image of [
    manifest.bootloader,
    manifest.launcher,
    ...manifest.apps,
]) {
    await verifyDownload(
        image,
        new Uint8Array(await readFile(`public/${image.file}`)),
    );
}
const work = await mkdtemp(join(tmpdir(), "multi-pin-release-"));
try {
    for (let mask = 1; mask < 512; mask++) {
        const ids = manifest.apps
            .filter((_, index) => mask & (1 << index))
            .map((app) => app.id);
        const plan = makePlan(manifest, ids);
        await writeFile(
            join(work, `${mask}.bin`),
            partitionTable(plan.entries),
        );
    }
    // Compare against ESP-IDF, independently of the browser serializer and its tests.
    const parser = join(process.env.IDF_PATH, "components", "partition_table");
    const result = spawnSync(
        "python",
        [
            "-c",
            `
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import gen_esp32part
for path in Path(sys.argv[2]).glob('*.bin'):
    data = path.read_bytes()
    table = gen_esp32part.PartitionTable.from_binary(data)
    table.verify()
    assert table.to_binary() == data, path.name
print('ESP-IDF parsed and reproduced all 511 release partition tables.')
`,
            parser,
            work,
        ],
        { encoding: "utf8" },
    );
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(result.stderr || result.stdout);
    process.stdout.write(result.stdout);
    console.log(
        "Every packaged image passed its size, SHA-256 and ESP32-S3 header checks.",
    );
} finally {
    await rm(work, { recursive: true, force: true });
}
