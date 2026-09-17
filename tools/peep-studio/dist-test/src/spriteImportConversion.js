"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.spriteImportOutputScale = void 0;
exports.convertSpritePixels = convertSpritePixels;
const BAYER_2X2 = [0, 2, 3, 1];
const BAYER_4X4 = [
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5,
];
const SOURCE_COLOR_LIMIT = 4096;
const spriteImportOutputScale = (mode) => (mode === "ordered_2x2" ? 2 : mode === "ordered_4x4" ? 4 : 1);
exports.spriteImportOutputScale = spriteImportOutputScale;
const luminance = (red, green, blue) => ((54 * red + 183 * green + 19 * blue) / 256);
function convertSpritePixels(source, width, height, conversion) {
    if (source.length !== width * height * 4)
        throw new Error("Sprite pixel buffer dimensions do not match.");
    const scale = (0, exports.spriteImportOutputScale)(conversion.mode);
    const outputWidth = width * scale;
    const outputHeight = height * scale;
    const data = new Uint8ClampedArray(outputWidth * outputHeight * 4);
    const tones = new Float64Array(width * height);
    const transparent = new Uint8Array(width * height);
    const sourceColorKeys = new Set();
    let sourceColorsLimited = false;
    for (let pixel = 0; pixel < width * height; pixel += 1) {
        const index = pixel * 4;
        const red = source[index] ?? 0;
        const green = source[index + 1] ?? 0;
        const blue = source[index + 2] ?? 0;
        const alpha = source[index + 3] ?? 0;
        const sourceTransparent = conversion.alphaMode !== "ignore_alpha" && alpha < conversion.alphaCutoff;
        transparent[pixel] = sourceTransparent && conversion.alphaMode === "respect_alpha" ? 1 : 0;
        tones[pixel] = sourceTransparent ? 255 : luminance(red, green, blue);
        if (!sourceTransparent) {
            const colorKey = (red << 16) | (green << 8) | blue;
            if (!sourceColorKeys.has(colorKey)) {
                if (sourceColorKeys.size < SOURCE_COLOR_LIMIT)
                    sourceColorKeys.add(colorKey);
                else
                    sourceColorsLimited = true;
            }
        }
    }
    let visiblePixels = 0;
    let blackPixels = 0;
    let whitePixels = 0;
    let transparentPixels = 0;
    const writeOutputPixel = (x, y, black, isTransparent) => {
        const index = (y * outputWidth + x) * 4;
        const value = black ? 0 : 255;
        data[index] = value;
        data[index + 1] = value;
        data[index + 2] = value;
        data[index + 3] = isTransparent ? 0 : 255;
        if (isTransparent)
            transparentPixels += 1;
        else {
            visiblePixels += 1;
            if (black)
                blackPixels += 1;
            else
                whitePixels += 1;
        }
    };
    if (scale > 1) {
        const matrix = conversion.mode === "ordered_2x2" ? BAYER_2X2 : BAYER_4X4;
        const cells = scale * scale;
        const strength = conversion.ditherStrength / 100;
        for (let sourceY = 0; sourceY < height; sourceY += 1) {
            for (let sourceX = 0; sourceX < width; sourceX += 1) {
                const sourcePixel = sourceY * width + sourceX;
                const isTransparent = transparent[sourcePixel] === 1;
                const tone = tones[sourcePixel] ?? 255;
                const adjustedTone = Math.max(0, Math.min(255, tone + 128 - conversion.threshold));
                const continuousCoverage = ((255 - adjustedTone) / 255) * cells;
                const binaryCoverage = tone < conversion.threshold ? cells : 0;
                let blackCells = Math.round(binaryCoverage * (1 - strength) + continuousCoverage * strength);
                blackCells = Math.max(0, Math.min(cells, blackCells));
                if (conversion.invert)
                    blackCells = cells - blackCells;
                for (let cellY = 0; cellY < scale; cellY += 1) {
                    for (let cellX = 0; cellX < scale; cellX += 1) {
                        const rank = matrix[cellY * scale + cellX] ?? 0;
                        writeOutputPixel(sourceX * scale + cellX, sourceY * scale + cellY, !isTransparent && rank < blackCells, isTransparent);
                    }
                }
            }
        }
        return {
            data,
            width: outputWidth,
            height: outputHeight,
            scale,
            visiblePixels,
            blackPixels,
            whitePixels,
            transparentPixels,
            sourceColors: sourceColorKeys.size,
            sourceColorsLimited,
        };
    }
    const ditheredTones = new Float64Array(tones);
    for (let y = 0; y < height; y += 1) {
        for (let x = 0; x < width; x += 1) {
            const pixel = y * width + x;
            if (transparent[pixel] === 1) {
                writeOutputPixel(x, y, false, true);
                continue;
            }
            const tone = ditheredTones[pixel] ?? 255;
            const sourceBlack = tone < conversion.threshold;
            if (conversion.mode === "floyd_steinberg") {
                const quantized = sourceBlack ? 0 : 255;
                const error = (tone - quantized) * (conversion.ditherStrength / 100);
                const diffuse = (targetX, targetY, weight) => {
                    if (targetX < 0 || targetX >= width || targetY < 0 || targetY >= height)
                        return;
                    const target = targetY * width + targetX;
                    if (transparent[target] === 0)
                        ditheredTones[target] += error * weight;
                };
                diffuse(x + 1, y, 7 / 16);
                diffuse(x - 1, y + 1, 3 / 16);
                diffuse(x, y + 1, 5 / 16);
                diffuse(x + 1, y + 1, 1 / 16);
            }
            const black = sourceBlack !== conversion.invert;
            writeOutputPixel(x, y, black, false);
        }
    }
    return {
        data,
        width: outputWidth,
        height: outputHeight,
        scale,
        visiblePixels,
        blackPixels,
        whitePixels,
        transparentPixels,
        sourceColors: sourceColorKeys.size,
        sourceColorsLimited,
    };
}
