// SPDX-License-Identifier: GPL-3.0-only
import { test } from "node:test";
import assert from "node:assert/strict";
import { messages, appDescriptionsEs } from "../src/i18n.js";
import { fixture } from "./fixtures.mjs";

test("both languages cover every message, placeholder and catalog description", () => {
    assert.deepEqual(
        Object.keys(messages.es).sort(),
        Object.keys(messages.en).sort(),
    );
    for (const key of Object.keys(messages.en)) {
        const placeholders = (text) =>
            [...text.matchAll(/\{(\w+)\}/g)].map((match) => match[1]).sort();
        assert.deepEqual(
            placeholders(messages.es[key]),
            placeholders(messages.en[key]),
            key,
        );
        assert(messages.es[key].trim(), key);
    }
    const { manifest } = fixture();
    assert.deepEqual(
        Object.keys(appDescriptionsEs).sort(),
        manifest.apps.map((app) => app.id).sort(),
    );
});
