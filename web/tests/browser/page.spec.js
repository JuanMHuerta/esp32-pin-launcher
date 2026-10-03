// SPDX-License-Identifier: GPL-3.0-only
import { test, expect } from "@playwright/test";

test("selection, menu/demo explanation, empty selection, canceled picker and repository subpath", async ({
    page,
}) => {
    await page.addInitScript(() =>
        Object.defineProperty(navigator, "serial", {
            value: {
                requestPort: async () => {
                    throw new DOMException("No port selected", "NotFoundError");
                },
            },
        }),
    );
    await page.goto("./");
    await expect(page.getByRole("checkbox")).toHaveCount(9);
    await expect(page.locator("#selection-count")).toHaveText("9 apps");
    await expect(
        page.getByText("Launcher menu + Demo are always included"),
    ).toBeVisible();
    await page.getByRole("button", { name: "Clear selection" }).click();
    await expect(page.locator("#flash")).toBeDisabled();
    await page.getByRole("checkbox", { name: "Install Miso" }).check();
    await expect(page.locator("#selection-count")).toHaveText("1 app");
    await expect(page.locator("#flash")).toBeEnabled();
    await page.locator("#flash").click();
    await expect(page.locator("#status")).toContainText("No device selected");
    await expect(page.locator("#flash")).toBeEnabled();
});

test("unsupported browser can browse but cannot flash", async ({ page }) => {
    await page.addInitScript(() => {
        delete Navigator.prototype.serial;
    });
    await page.goto("./");
    await expect(page.getByRole("checkbox")).toHaveCount(9);
    await expect(page.locator("#status")).toContainText(
        "Use desktop Chrome or Edge",
    );
    await expect(page.locator("#flash")).toBeDisabled();
});

test("a browser permission error displays its reason and allows retry", async ({
    page,
}) => {
    await page.addInitScript(() =>
        Object.defineProperty(navigator, "serial", {
            value: {
                requestPort: async () => {
                    throw new DOMException(
                        "Serial access blocked",
                        "SecurityError",
                    );
                },
            },
        }),
    );
    await page.goto("./");
    await expect(page.locator("#flash")).toBeEnabled();
    await page.locator("#flash").click();
    await expect(page.locator("#status")).toContainText(
        "Serial access blocked",
    );
    await expect(page.locator("#flash")).toBeEnabled();
});

test("missing firmware fails visibly", async ({ page }) => {
    await page.route("**/manifest.json", (route) =>
        route.fulfill({ status: 404 }),
    );
    await page.goto("./");
    await expect(page.locator("#status")).toContainText(
        "Firmware release is unavailable",
    );
    await expect(page.locator("#flash")).toBeDisabled();
});

test("phone layout has no horizontal overflow", async ({ page }) => {
    await page.setViewportSize({ width: 390, height: 844 });
    await page.goto("./");
    await expect(page.getByRole("checkbox")).toHaveCount(9);
    expect(
        await page.evaluate(() => document.documentElement.scrollWidth),
    ).toBeLessThanOrEqual(390);
});

test("Spanish selection survives language changes and reload, including picker cancellation", async ({
    page,
}) => {
    await page.addInitScript(() =>
        Object.defineProperty(navigator, "serial", {
            value: {
                requestPort: async () => {
                    throw new DOMException("No port selected", "NotFoundError");
                },
            },
        }),
    );
    await page.goto("./");
    await page.locator("#language").selectOption("es");
    await expect(page.locator("html")).toHaveAttribute("lang", "es");
    await page.getByRole("button", { name: "Quitar selección" }).click();
    await expect(page.locator("#status")).toHaveText(
        "Elegí al menos una app para instalar.",
    );
    await page.getByRole("checkbox", { name: "Instalar Miso" }).check();
    await page.locator("#language").selectOption("en");
    await expect(
        page.getByRole("checkbox", { name: "Install Miso" }),
    ).toBeChecked();
    await expect(page.locator("#selection-count")).toHaveText("1 app");
    await page.locator("#language").selectOption("es");
    await page.locator("#flash").click();
    await expect(page.locator("#status")).toContainText(
        "No elegiste un dispositivo",
    );
    await page.reload();
    await expect(page.locator("html")).toHaveAttribute("lang", "es");
    await expect(
        page.getByRole("checkbox", { name: "Instalar Miso" }),
    ).toBeVisible();
});

test("Spanish narrow layout and release errors remain readable", async ({
    page,
}) => {
    await page.setViewportSize({ width: 320, height: 700 });
    await page.goto("./");
    await page.locator("#language").selectOption("es");
    await expect(page.getByRole("checkbox")).toHaveCount(9);
    expect(
        await page.evaluate(() => document.documentElement.scrollWidth),
    ).toBeLessThanOrEqual(320);
    await page.route("**/manifest.json", (route) =>
        route.fulfill({ status: 404 }),
    );
    await page.reload();
    await expect(page.locator("#status")).toContainText(
        "El firmware no está disponible",
    );
    await expect(page.locator("#flash")).toBeDisabled();
});
