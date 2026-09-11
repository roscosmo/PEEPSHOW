import assert from "node:assert/strict";
import { parseSpriteSheetGrid } from "../src/spriteSheetImport.js";

assert.deepEqual(parseSpriteSheetGrid(64, 16, "4", "1"), { frameWidth: 16, frameHeight: 16 });
assert.deepEqual(parseSpriteSheetGrid(32, 32, "2", "2"), { frameWidth: 16, frameHeight: 16 });
assert.deepEqual(parseSpriteSheetGrid(16, 64, "1", "4"), { frameWidth: 16, frameHeight: 16 });
assert.deepEqual(parseSpriteSheetGrid(168, 144, "1", "1"), { frameWidth: 168, frameHeight: 144 });
assert.deepEqual(parseSpriteSheetGrid(256, 1, "256", "1"), { frameWidth: 1, frameHeight: 1 });
for (const invalid of ["", "0", "-1", "1.5", "NaN", "Infinity"]) {
  assert.match(parseSpriteSheetGrid(64, 16, invalid, "1").error!, /positive whole/);
  assert.match(parseSpriteSheetGrid(64, 16, "1", invalid).error!, /positive whole/);
}
assert.match(parseSpriteSheetGrid(64, 16, "3", "1").error!, /evenly/);
assert.match(parseSpriteSheetGrid(64, 16, "4", "3").error!, /evenly/);
assert.match(parseSpriteSheetGrid(16, 16, "32", "1").error!, /evenly/);
assert.match(parseSpriteSheetGrid(512, 16, "32", "16").error!, /256/);
assert.match(parseSpriteSheetGrid(169, 144, "1", "1").error!, /168x144/);
assert.match(parseSpriteSheetGrid(168, 145, "1", "1").error!, /168x144/);
console.log("Sprite sheet grid validation passed.");
