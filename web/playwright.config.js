// SPDX-License-Identifier: GPL-3.0-only
import { defineConfig } from "@playwright/test";
export default defineConfig({
    testDir: "./tests/browser",
    use: { baseURL: "http://localhost:4173/repo/" },
    webServer: {
        command: "node tests/serve.mjs",
        url: "http://localhost:4173/repo/",
        reuseExistingServer: false,
    },
});
