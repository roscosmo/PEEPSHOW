"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.PLACEMENT_HEIGHT = exports.PLACEMENT_WIDTH = void 0;
exports.normalizePrimitiveBounds = normalizePrimitiveBounds;
exports.primitiveBoundsFromPoints = primitiveBoundsFromPoints;
exports.lineDirectionFromPoints = lineDirectionFromPoints;
exports.PLACEMENT_WIDTH = 168;
exports.PLACEMENT_HEIGHT = 144;
function oddDimension(value, maximum) {
    const bounded = Math.min(maximum, Math.max(3, Math.round(value)));
    if (bounded % 2 === 1) {
        return bounded;
    }
    return bounded < maximum ? bounded + 1 : bounded - 1;
}
function normalizePrimitiveBounds(kind, bounds) {
    const x = Math.min(exports.PLACEMENT_WIDTH - 1, Math.max(0, Math.round(bounds.x)));
    const y = Math.min(exports.PLACEMENT_HEIGHT - 1, Math.max(0, Math.round(bounds.y)));
    if (kind === "circle") {
        const maximum = Math.min(exports.PLACEMENT_WIDTH - x, exports.PLACEMENT_HEIGHT - y);
        const size = oddDimension(Math.max(bounds.width, bounds.height), maximum);
        return { x, y, width: size, height: size };
    }
    const width = kind === "ellipse"
        ? oddDimension(bounds.width, exports.PLACEMENT_WIDTH - x)
        : Math.min(exports.PLACEMENT_WIDTH - x, Math.max(1, Math.round(bounds.width)));
    const height = kind === "ellipse"
        ? oddDimension(bounds.height, exports.PLACEMENT_HEIGHT - y)
        : Math.min(exports.PLACEMENT_HEIGHT - y, Math.max(1, Math.round(bounds.height)));
    return { x, y, width, height };
}
function primitiveBoundsFromPoints(kind, start, end) {
    const constrainedEnd = kind === "circle"
        ? (() => {
            const span = Math.max(Math.abs(end.x - start.x), Math.abs(end.y - start.y));
            return {
                x: start.x + (end.x < start.x ? -span : span),
                y: start.y + (end.y < start.y ? -span : span),
            };
        })()
        : end;
    return normalizePrimitiveBounds(kind, {
        x: Math.min(start.x, constrainedEnd.x),
        y: Math.min(start.y, constrainedEnd.y),
        width: Math.abs(constrainedEnd.x - start.x) + 1,
        height: Math.abs(constrainedEnd.y - start.y) + 1,
    });
}
function lineDirectionFromPoints(start, end) {
    return (end.x - start.x) * (end.y - start.y) < 0 ? "up_right" : "down_right";
}
