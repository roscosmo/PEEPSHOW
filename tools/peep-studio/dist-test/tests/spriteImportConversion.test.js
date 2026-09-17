"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const strict_1 = __importDefault(require("node:assert/strict"));
const spriteImportConversion_js_1 = require("../src/spriteImportConversion.js");
const base = {
    mode: "threshold_1bpp",
    threshold: 128,
    ditherStrength: 100,
    alphaCutoff: 1,
    alphaMode: "respect_alpha",
    invert: false,
};
const rgba = (...pixels) => new Uint8ClampedArray(pixels.flat());
const threshold = (0, spriteImportConversion_js_1.convertSpritePixels)(rgba([0, 0, 0, 255], [255, 255, 255, 255], [255, 0, 0, 255], [0, 255, 0, 0]), 4, 1, base);
strict_1.default.deepEqual([threshold.blackPixels, threshold.whitePixels, threshold.transparentPixels], [2, 1, 1]);
strict_1.default.equal(threshold.sourceColors, 3);
strict_1.default.deepEqual([...threshold.data.slice(12, 16)], [255, 255, 255, 0]);
const orderedSource = new Uint8ClampedArray(4 * 4 * 4);
for (let index = 0; index < orderedSource.length; index += 4) {
    orderedSource.set([128, 128, 128, 255], index);
}
const ordered = (0, spriteImportConversion_js_1.convertSpritePixels)(orderedSource, 4, 4, { ...base, mode: "ordered_4x4" });
strict_1.default.deepEqual([ordered.width, ordered.height, ordered.scale], [16, 16, 4]);
strict_1.default.equal(ordered.blackPixels, 128);
strict_1.default.equal(ordered.whitePixels, 128);
const indexed = (0, spriteImportConversion_js_1.convertSpritePixels)(rgba([0, 0, 0, 255], [85, 85, 85, 255], [170, 170, 170, 255], [255, 255, 255, 255]), 4, 1, { ...base, mode: "ordered_2x2" });
strict_1.default.deepEqual([indexed.width, indexed.height, indexed.scale], [8, 2, 2]);
const cellBlackCounts = Array.from({ length: 4 }, (_, sourceX) => {
    let count = 0;
    for (let y = 0; y < 2; y += 1) {
        for (let x = sourceX * 2; x < sourceX * 2 + 2; x += 1) {
            if (indexed.data[(y * indexed.width + x) * 4] === 0)
                count += 1;
        }
    }
    return count;
});
strict_1.default.deepEqual(cellBlackCounts, [4, 3, 1, 0]);
const expandedTransparency = (0, spriteImportConversion_js_1.convertSpritePixels)(rgba([0, 0, 0, 0]), 1, 1, { ...base, mode: "ordered_2x2" });
strict_1.default.deepEqual([expandedTransparency.transparentPixels, expandedTransparency.visiblePixels], [4, 0]);
const diffusionSource = new Uint8ClampedArray(8 * 4);
for (let index = 0; index < diffusionSource.length; index += 4) {
    const tone = Math.round((index / 4) * (255 / 7));
    diffusionSource.set([tone, tone, tone, 255], index);
}
const diffusion = (0, spriteImportConversion_js_1.convertSpritePixels)(diffusionSource, 8, 1, { ...base, mode: "floyd_steinberg" });
(0, strict_1.default)(diffusion.blackPixels > 0 && diffusion.whitePixels > 0);
strict_1.default.deepEqual((0, spriteImportConversion_js_1.convertSpritePixels)(diffusionSource, 8, 1, { ...base, mode: "floyd_steinberg", invert: true }).blackPixels, diffusion.whitePixels);
const ignoredAlpha = (0, spriteImportConversion_js_1.convertSpritePixels)(rgba([0, 0, 0, 0]), 1, 1, { ...base, alphaMode: "ignore_alpha" });
strict_1.default.deepEqual([ignoredAlpha.blackPixels, ignoredAlpha.transparentPixels], [1, 0]);
console.log("Sprite import conversion checks passed");
