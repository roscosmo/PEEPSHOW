import assert from "node:assert/strict";
import { isFillableShapeKind, lineDirectionFromPoints, primitiveBoundsFromPoints, shapeKindWithFill } from "../src/placementGeometry";

assert.deepEqual(
  primitiveBoundsFromPoints("outline_rect", { x: 10, y: 20 }, { x: 30, y: 50 }),
  { x: 10, y: 20, width: 21, height: 31 },
);
assert.deepEqual(
  primitiveBoundsFromPoints("filled_rect", { x: 30, y: 50 }, { x: 10, y: 20 }),
  { x: 10, y: 20, width: 21, height: 31 },
);
assert.deepEqual(
  primitiveBoundsFromPoints("circle", { x: 5, y: 5 }, { x: 10, y: 8 }),
  { x: 5, y: 5, width: 7, height: 7 },
);
assert.deepEqual(
  primitiveBoundsFromPoints("circle", { x: 10, y: 10 }, { x: 5, y: 8 }),
  { x: 5, y: 5, width: 7, height: 7 },
);
assert.deepEqual(
  primitiveBoundsFromPoints("filled_circle", { x: 5, y: 5 }, { x: 10, y: 8 }),
  { x: 5, y: 5, width: 7, height: 7 },
);
assert.deepEqual(
  primitiveBoundsFromPoints("ellipse", { x: 5, y: 5 }, { x: 10, y: 8 }),
  { x: 5, y: 5, width: 7, height: 5 },
);
assert.deepEqual(
  primitiveBoundsFromPoints("line", { x: 167, y: 143 }, { x: 167, y: 143 }),
  { x: 167, y: 143, width: 1, height: 1 },
);
assert.equal(lineDirectionFromPoints({ x: 10, y: 20 }, { x: 30, y: 50 }), "down_right");
assert.equal(lineDirectionFromPoints({ x: 30, y: 50 }, { x: 10, y: 20 }), "down_right");
assert.equal(lineDirectionFromPoints({ x: 10, y: 50 }, { x: 30, y: 20 }), "up_right");
assert.equal(lineDirectionFromPoints({ x: 30, y: 20 }, { x: 10, y: 50 }), "up_right");
assert.equal(lineDirectionFromPoints({ x: 10, y: 20 }, { x: 30, y: 20 }), "down_right");
assert.equal(lineDirectionFromPoints({ x: 10, y: 20 }, { x: 10, y: 50 }), "down_right");
assert.equal(isFillableShapeKind("ellipse"), true);
assert.equal(isFillableShapeKind("line"), false);
assert.equal(shapeKindWithFill("ellipse", true), "filled_ellipse");
assert.equal(shapeKindWithFill("filled_rect", false), "outline_rect");
