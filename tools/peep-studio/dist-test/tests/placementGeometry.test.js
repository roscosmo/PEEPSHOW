"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const strict_1 = __importDefault(require("node:assert/strict"));
const placementGeometry_1 = require("../src/placementGeometry");
strict_1.default.deepEqual((0, placementGeometry_1.primitiveBoundsFromPoints)("outline_rect", { x: 10, y: 20 }, { x: 30, y: 50 }), { x: 10, y: 20, width: 21, height: 31 });
strict_1.default.deepEqual((0, placementGeometry_1.primitiveBoundsFromPoints)("filled_rect", { x: 30, y: 50 }, { x: 10, y: 20 }), { x: 10, y: 20, width: 21, height: 31 });
strict_1.default.deepEqual((0, placementGeometry_1.primitiveBoundsFromPoints)("circle", { x: 5, y: 5 }, { x: 10, y: 8 }), { x: 5, y: 5, width: 7, height: 7 });
strict_1.default.deepEqual((0, placementGeometry_1.primitiveBoundsFromPoints)("circle", { x: 10, y: 10 }, { x: 5, y: 8 }), { x: 5, y: 5, width: 7, height: 7 });
strict_1.default.deepEqual((0, placementGeometry_1.primitiveBoundsFromPoints)("ellipse", { x: 5, y: 5 }, { x: 10, y: 8 }), { x: 5, y: 5, width: 7, height: 5 });
strict_1.default.deepEqual((0, placementGeometry_1.primitiveBoundsFromPoints)("line", { x: 167, y: 143 }, { x: 167, y: 143 }), { x: 167, y: 143, width: 1, height: 1 });
strict_1.default.equal((0, placementGeometry_1.lineDirectionFromPoints)({ x: 10, y: 20 }, { x: 30, y: 50 }), "down_right");
strict_1.default.equal((0, placementGeometry_1.lineDirectionFromPoints)({ x: 30, y: 50 }, { x: 10, y: 20 }), "down_right");
strict_1.default.equal((0, placementGeometry_1.lineDirectionFromPoints)({ x: 10, y: 50 }, { x: 30, y: 20 }), "up_right");
strict_1.default.equal((0, placementGeometry_1.lineDirectionFromPoints)({ x: 30, y: 20 }, { x: 10, y: 50 }), "up_right");
strict_1.default.equal((0, placementGeometry_1.lineDirectionFromPoints)({ x: 10, y: 20 }, { x: 30, y: 20 }), "down_right");
strict_1.default.equal((0, placementGeometry_1.lineDirectionFromPoints)({ x: 10, y: 20 }, { x: 10, y: 50 }), "down_right");
