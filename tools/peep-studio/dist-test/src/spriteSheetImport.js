"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseSpriteSheetGrid = parseSpriteSheetGrid;
function parseSpriteSheetGrid(sourceWidth, sourceHeight, columnsText, rowsText) {
    const columns = Number(columnsText);
    const rows = Number(rowsText);
    const invalid = (error) => ({ frameWidth: 0, frameHeight: 0, error });
    if (!Number.isInteger(columns) || !Number.isInteger(rows) || columns < 1 || rows < 1) {
        return invalid("Columns and rows must be positive whole numbers.");
    }
    if (columns * rows > 256) {
        return invalid("A sprite asset can contain at most 256 frames.");
    }
    if (sourceWidth % columns !== 0 || sourceHeight % rows !== 0) {
        return invalid("Columns and rows must divide the PNG evenly.");
    }
    const frameWidth = sourceWidth / columns;
    const frameHeight = sourceHeight / rows;
    if (frameWidth > 168 || frameHeight > 144) {
        return invalid("Each sprite frame must fit inside 168x144.");
    }
    return { frameWidth, frameHeight };
}
