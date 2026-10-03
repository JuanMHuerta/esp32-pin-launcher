// SPDX-License-Identifier: GPL-3.0-only
import { FlasherError } from "./errors.js";
import { makePlan, partitionTable, verifyDownload, md5 } from "./layout.js";

export async function install({
    manifest,
    selectedIds,
    requestPort,
    createLoader,
    fetchFile,
    onStatus,
    onProgress,
    terminal,
}) {
    const plan = makePlan(manifest, selectedIds);
    // The picker must run directly in the click handler's user activation, before downloads.
    const port = await requestPort();
    let transport;
    try {
        onStatus("downloading");
        const payloads = await Promise.all(
            [manifest.bootloader, ...plan.images].map(async (image) => {
                const response = await fetchFile(image.file);
                if (!response.ok) throw new FlasherError("downloadFailed");
                const data = new Uint8Array(await response.arrayBuffer());
                await verifyDownload(image, data);
                return { data, address: image.address ?? 0 };
            }),
        );
        const fileArray = [
            payloads[0],
            { data: partitionTable(plan.entries), address: 0x8000 },
            { data: new Uint8Array(0x6000).fill(0xff), address: 0x9000 },
            { data: new Uint8Array(0x2000).fill(0xff), address: 0xf000 },
            ...payloads.slice(1),
        ];
        const connection = createLoader(port, terminal);
        transport = connection.transport;
        const loader = connection.loader;
        onStatus("connecting");
        await loader.main();
        if (loader.chip?.CHIP_NAME !== "ESP32-S3")
            throw new FlasherError("wrongChip");
        if ((await loader.detectFlashSize()) !== "16MB")
            throw new FlasherError("wrongFlash");
        onStatus("writing");
        // esptool-js reports compressed byte progress per file. Aggregate each fraction by image size.
        const sizes = fileArray.map((file) => file.data.length);
        const total = sizes.reduce((a, b) => a + b, 0);
        await loader.writeFlash({
            fileArray,
            flashSize: "keep",
            flashMode: "keep",
            flashFreq: "keep",
            eraseAll: false,
            compress: true,
            calculateMD5Hash: md5,
            reportProgress(index, written, fileTotal) {
                const previous = sizes
                    .slice(0, index)
                    .reduce((a, b) => a + b, 0);
                onProgress(
                    Math.min(
                        99,
                        Math.round(
                            ((previous + (sizes[index] * written) / fileTotal) /
                                total) *
                                100,
                        ),
                    ),
                );
            },
        });
        // Reset failure does not undo a verified installation (native USB may re-enumerate).
        let reset = true;
        try {
            await loader.after("hard_reset");
        } catch {
            reset = false;
        }
        onProgress(100);
        return { reset, count: plan.apps.length };
    } finally {
        if (transport) {
            try {
                await transport.disconnect();
            } catch {
                /* Device may already have re-enumerated. */
            }
        }
    }
}
