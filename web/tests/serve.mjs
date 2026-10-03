// SPDX-License-Identifier: GPL-3.0-only
import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { build } from "esbuild";
import { fixture } from "./fixtures.mjs";
const result = await build({
    entryPoints: ["src/app.js"],
    bundle: true,
    format: "esm",
    write: false,
});
const { manifest, bytes } = fixture();
const files = {
    "index.html": ["text/html", await readFile("index.html")],
    "style.css": ["text/css", await readFile("style.css")],
    "app.js": ["application/javascript", result.outputFiles[0].contents],
    "manifest.json": ["application/json", JSON.stringify(manifest)],
};
createServer((req, res) => {
    const path = req.url.replace(/^\/repo\//, "") || "index.html";
    let file = files[path];
    if (path.startsWith("firmware/"))
        file = ["application/octet-stream", bytes];
    if (path.startsWith("previews/"))
        file = [
            "image/gif",
            Buffer.from(
                "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7",
                "base64",
            ),
        ];
    if (!file) {
        res.writeHead(404).end();
        return;
    }
    res.writeHead(200, { "Content-Type": file[0] }).end(file[1]);
}).listen(4173, "127.0.0.1");
