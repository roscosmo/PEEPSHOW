"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const strict_1 = __importDefault(require("node:assert/strict"));
const spriteSheetImport_js_1 = require("../src/spriteSheetImport.js");
strict_1.default.deepEqual((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(64, 16, "4", "1"), { frameWidth: 16, frameHeight: 16 });
strict_1.default.deepEqual((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(32, 32, "2", "2"), { frameWidth: 16, frameHeight: 16 });
strict_1.default.deepEqual((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(16, 64, "1", "4"), { frameWidth: 16, frameHeight: 16 });
strict_1.default.deepEqual((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(168, 144, "1", "1"), { frameWidth: 168, frameHeight: 144 });
strict_1.default.deepEqual((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(256, 1, "256", "1"), { frameWidth: 1, frameHeight: 1 });
for (const invalid of ["", "0", "-1", "1.5", "NaN", "Infinity"]) {
    strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(64, 16, invalid, "1").error, /positive whole/);
    strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(64, 16, "1", invalid).error, /positive whole/);
}
strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(64, 16, "3", "1").error, /evenly/);
strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(64, 16, "4", "3").error, /evenly/);
strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(16, 16, "32", "1").error, /evenly/);
strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(512, 16, "32", "16").error, /256/);
strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(169, 144, "1", "1").error, /168x144/);
strict_1.default.match((0, spriteSheetImport_js_1.parseSpriteSheetGrid)(168, 145, "1", "1").error, /168x144/);
console.log("Sprite sheet grid validation passed.");
