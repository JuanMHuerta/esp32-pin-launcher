// SPDX-License-Identifier: GPL-3.0-only
import { translate } from "./i18n.js";

export class FlasherError extends Error {
    constructor(code) {
        super(translate("en", code));
        this.name = "FlasherError";
        this.code = code;
    }
}
