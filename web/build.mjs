// SPDX-License-Identifier: GPL-3.0-only
import { build } from "esbuild";
import { cp, mkdir, rm, access, readFile, writeFile } from "node:fs/promises";
import { validateManifest } from "./src/layout.js";

await access(new URL("./public/manifest.json", import.meta.url));
await rm("dist", { recursive: true, force: true });
await mkdir("dist");
const manifest = validateManifest(
    JSON.parse(await readFile("public/manifest.json", "utf8")),
);
await mkdir("dist/firmware");
await mkdir("dist/previews");
for (const file of [
    "manifest.json",
    "source.tar.gz",
    "SOURCE.txt",
    "FIRMWARE_LICENSES.txt",
    manifest.bootloader.file,
    manifest.launcher.file,
    ...manifest.apps.flatMap((app) => [app.file, app.preview]),
]) {
    await cp(`public/${file}`, `dist/${file}`);
}
await cp("index.html", "dist/index.html");
await cp("style.css", "dist/style.css");
await cp("../LICENSE", "dist/LICENSE.txt");
await cp("../NOTICE.md", "dist/NOTICE.txt");
const licenses = [];
for (const [name, filename] of [
    ["esptool-js", "LICENSE"],
    ["pako", "LICENSE"],
    ["atob-lite", "LICENSE.md"],
    ["spark-md5", "LICENSE"],
]) {
    licenses.push(
        `${name}\n${await readFile(`node_modules/${name}/${filename}`, "utf8")}`,
    );
}
await writeFile("dist/THIRD_PARTY_LICENSES.txt", licenses.join("\n\n"));
await build({
    entryPoints: ["src/app.js"],
    bundle: true,
    format: "esm",
    target: "es2022",
    outfile: "dist/app.js",
    minify: true,
    legalComments: "eof",
});
