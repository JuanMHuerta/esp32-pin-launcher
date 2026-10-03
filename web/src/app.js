// SPDX-License-Identifier: GPL-3.0-only
import { ESPLoader, Transport } from "esptool-js";
import { validateManifest, makePlan } from "./layout.js";
import { install } from "./install.js";
import { translate, appDescriptionsEs } from "./i18n.js";
import { FlasherError } from "./errors.js";

const $ = (id) => document.getElementById(id);
let manifest;
let busy = false;
const supported = window.isSecureContext && "serial" in navigator;
let language = navigator.language.startsWith("es") ? "es" : "en";
try {
    const saved = localStorage.getItem("language");
    if (saved === "en" || saved === "es") language = saved;
} catch {
    /* Browsing works when storage is disabled. */
}
const t = (key, values) => translate(language, key, values);
const selectedIds = () =>
    [...document.querySelectorAll(".app-card input:checked")].map(
        (input) => input.value,
    );
const status = (message, kind = "") => {
    $("status").textContent = message;
    $("status").dataset.kind = kind;
};

function applyLanguage() {
    document.documentElement.lang = language;
    document.title = t("title");
    $("language").value = language;
    for (const element of document.querySelectorAll("[data-i18n]")) {
        // These strings are local translations; release data is rendered with textContent.
        element.innerHTML = t(element.dataset.i18n);
    }
    $("progress").setAttribute("aria-label", t("progress"));
    $("log").setAttribute("aria-label", t("log"));
}

function showBrowserStatus() {
    if (!supported)
        status(t(window.isSecureContext ? "unsupported" : "insecure"), "error");
}

applyLanguage();
status(t("loading"));

$("language").addEventListener("change", () => {
    const selected = new Set(selectedIds());
    language = $("language").value;
    try {
        localStorage.setItem("language", language);
    } catch {
        /* Optional preference. */
    }
    applyLanguage();
    if (manifest) {
        renderApps(selected);
        showBrowserStatus();
    } else {
        status(t("unavailable"), "error");
    }
});

function updateSelection() {
    const ids = selectedIds();
    $("selection-count").textContent = t(
        ids.length === 1 ? "selectionOne" : "selectionMany",
        { count: ids.length },
    );
    $("select-all").textContent = t(
        ids.length === manifest.apps.length ? "clear" : "selectAll",
    );
    $("flash").disabled = busy || !supported || !ids.length;
    if (ids.length) {
        const plan = makePlan(manifest, ids);
        const size =
            manifest.bootloader.size +
            plan.images.reduce((sum, image) => sum + image.size, 0);
        $("download-size").textContent =
            `${(size / 1024 / 1024).toFixed(2)} MB`;
    } else {
        $("download-size").textContent = "—";
    }
    if (!busy && supported) status(t(ids.length ? "ready" : "empty"));
}

function renderApps(selected = new Set(manifest.apps.map((app) => app.id))) {
    $("apps").replaceChildren();
    for (const app of manifest.apps) {
        const label = document.createElement("label");
        label.className = "app-card";
        const input = document.createElement("input");
        input.type = "checkbox";
        input.value = app.id;
        input.checked = selected.has(app.id);
        input.setAttribute("aria-label", t("installApp", { name: app.name }));
        input.addEventListener("change", updateSelection);
        const img = document.createElement("img");
        img.src = app.preview;
        img.alt = "";
        img.loading = "lazy";
        img.width = 536;
        img.height = 240;
        const copy = document.createElement("div");
        copy.className = "card-copy";
        const heading = document.createElement("h3");
        heading.textContent = app.name;
        const description = document.createElement("p");
        description.textContent =
            language === "es"
                ? (appDescriptionsEs[app.id] ?? app.description)
                : app.description;
        copy.append(heading, description);
        label.append(img, input, copy);
        $("apps").append(label);
    }
    $("apps").setAttribute("aria-busy", "false");
    $("select-all").disabled = false;
    $("build").textContent = t("build", { build: manifest.build });
    updateSelection();
}

$("select-all").addEventListener("click", () => {
    const all = selectedIds().length !== manifest.apps.length;
    for (const input of document.querySelectorAll(".app-card input"))
        input.checked = all;
    updateSelection();
});

$("flash").addEventListener("click", async () => {
    if (busy) return;
    const ids = selectedIds();
    busy = true;
    $("flash").disabled = true;
    $("select-all").disabled = true;
    $("language").disabled = true;
    for (const input of document.querySelectorAll(".app-card input"))
        input.disabled = true;
    $("progress").hidden = false;
    $("progress").value = 0;
    $("log").textContent = "";
    const log = (text) => {
        $("log").textContent = ($("log").textContent + text).slice(-24000);
        $("log").scrollTop = $("log").scrollHeight;
    };
    try {
        status(t("picker"));
        const result = await install({
            manifest,
            selectedIds: ids,
            requestPort: () =>
                navigator.serial.requestPort({
                    filters: [{ usbVendorId: 0x303a, usbProductId: 0x1001 }],
                }),
            createLoader(port, terminal) {
                const transport = new Transport(port, false);
                return {
                    transport,
                    loader: new ESPLoader({
                        transport,
                        baudrate: 460800,
                        terminal,
                    }),
                };
            },
            fetchFile: (file) => fetch(file, { cache: "no-store" }),
            onStatus: (key) => status(t(key)),
            onProgress: (value) => {
                $("progress").value = value;
            },
            terminal: {
                clean() {
                    $("log").textContent = "";
                },
                write: log,
                writeLine: (text) => log(`${text}\n`),
            },
        });
        status(
            `${t(result.count === 1 ? "successOne" : "successMany", { count: result.count })} ${t(result.reset ? "resetAuto" : "resetManual")}`,
            "success",
        );
    } catch (error) {
        if (error.name === "NotFoundError") {
            status(t("canceled"));
            $("progress").hidden = true;
        } else {
            status(
                t("failed", {
                    error:
                        error instanceof FlasherError
                            ? t(error.code)
                            : error.message,
                }),
                "error",
            );
            log(`\n${error.stack || error.message}\n`);
            $("log-details").open = true;
        }
    } finally {
        busy = false;
        $("select-all").disabled = false;
        $("language").disabled = false;
        for (const input of document.querySelectorAll(".app-card input"))
            input.disabled = false;
        $("flash").disabled = !supported || !selectedIds().length;
    }
});

window.addEventListener("beforeunload", (event) => {
    if (busy) {
        event.preventDefault();
        event.returnValue = "";
    }
});

try {
    const response = await fetch("./manifest.json", { cache: "no-store" });
    if (!response.ok) throw new Error(t("unavailable"));
    manifest = validateManifest(await response.json());
    renderApps();
    showBrowserStatus();
} catch (error) {
    status(
        error instanceof FlasherError ? t(error.code) : error.message,
        "error",
    );
    $("apps").textContent = t("loadFailed");
    $("apps").setAttribute("aria-busy", "false");
}
