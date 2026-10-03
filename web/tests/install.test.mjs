// SPDX-License-Identifier: GPL-3.0-only
import { test } from "node:test";
import assert from "node:assert/strict";
import { install } from "../src/install.js";
import { fixture } from "./fixtures.mjs";

function harness() {
    const { manifest, bytes } = fixture();
    const events = [];
    let written;
    const loader = {
        chip: { CHIP_NAME: "ESP32-S3" },
        async main() {
            events.push("connect");
        },
        async detectFlashSize() {
            return "16MB";
        },
        async writeFlash(options) {
            written = options;
            events.push("write");
            options.reportProgress(0, 16, 32);
        },
        async after() {
            events.push("reset");
        },
    };
    return {
        loader,
        events,
        get written() {
            return written;
        },
        options: {
            manifest,
            selectedIds: ["miso", "crt"],
            requestPort: async () => {
                events.push("picker");
                return {};
            },
            createLoader: () => ({
                loader,
                transport: {
                    async disconnect() {
                        events.push("disconnect");
                    },
                },
            }),
            fetchFile: async () => {
                events.push("download");
                return { ok: true, arrayBuffer: async () => bytes.buffer };
            },
            onStatus() {},
            onProgress(value) {
                assert(value >= 0 && value <= 100);
            },
            terminal: {},
        },
    };
}

test("picker precedes downloads; install always includes menu, cleared state and selected apps only", async () => {
    const h = harness();
    const result = await install(h.options);
    assert.deepEqual(result, { reset: true, count: 2 });
    assert.equal(h.events[0], "picker");
    assert.deepEqual(h.events.slice(-3), ["write", "reset", "disconnect"]);
    assert.deepEqual(
        h.written.fileArray.map((f) => f.address),
        [0, 0x8000, 0x9000, 0xf000, 0x20000, 0x30000, 0x40000],
    );
    assert.equal(h.written.eraseAll, false);
    assert.equal(
        h.written.calculateMD5Hash(new Uint8Array([1, 2, 3])),
        "5289df737df57326fcdd22597afb1fac",
    );
    assert(h.written.fileArray[2].data.every((b) => b === 255));
    assert(h.written.fileArray[3].data.every((b) => b === 255));
});

test("wrong chip and capacity never write and always disconnect", async () => {
    for (const wrong of ["chip", "size"]) {
        const h = harness();
        if (wrong === "chip") h.loader.chip.CHIP_NAME = "ESP32";
        else h.loader.detectFlashSize = async () => "8MB";
        await assert.rejects(install(h.options), /Wrong/);
        assert(!h.events.includes("write"));
        assert.equal(h.events.at(-1), "disconnect");
    }
});

test("write failures release the port, and reset failures preserve verified success", async () => {
    const fail = harness();
    fail.loader.writeFlash = async () => {
        throw new Error("write failed");
    };
    await assert.rejects(install(fail.options), /write failed/);
    assert.equal(fail.events.at(-1), "disconnect");
    assert(!fail.events.includes("reset"));
    const reset = harness();
    reset.loader.after = async () => {
        throw new Error("port disappeared");
    };
    assert.deepEqual(await install(reset.options), { reset: false, count: 2 });
});

test("download failures and canceled pickers never connect or write", async () => {
    const download = harness();
    download.options.fetchFile = async () => ({ ok: false });
    await assert.rejects(install(download.options), /download failed/);
    assert(!download.events.includes("connect"));
    const cancel = harness();
    cancel.options.requestPort = async () => {
        throw new Error("canceled");
    };
    await assert.rejects(install(cancel.options), /canceled/);
    assert(!cancel.events.includes("download"));
});
