export const PLACEMENT_WIDTH = 168;
export const PLACEMENT_HEIGHT = 144;

export type PlacementPrimitiveKind = "line" | "outline_rect" | "filled_rect" | "circle" | "ellipse";
export type PlacementLineDirection = "down_right" | "up_right";

export type PlacementPoint = {
  x: number;
  y: number;
};

export type PlacementBounds = PlacementPoint & {
  width: number;
  height: number;
};

function oddDimension(value: number, maximum: number): number {
  const bounded = Math.min(maximum, Math.max(3, Math.round(value)));
  if (bounded % 2 === 1) {
    return bounded;
  }
  return bounded < maximum ? bounded + 1 : bounded - 1;
}

export function normalizePrimitiveBounds(
  kind: PlacementPrimitiveKind,
  bounds: PlacementBounds,
): PlacementBounds {
  const x = Math.min(PLACEMENT_WIDTH - 1, Math.max(0, Math.round(bounds.x)));
  const y = Math.min(PLACEMENT_HEIGHT - 1, Math.max(0, Math.round(bounds.y)));
  if (kind === "circle") {
    const maximum = Math.min(PLACEMENT_WIDTH - x, PLACEMENT_HEIGHT - y);
    const size = oddDimension(Math.max(bounds.width, bounds.height), maximum);
    return { x, y, width: size, height: size };
  }
  const width = kind === "ellipse"
    ? oddDimension(bounds.width, PLACEMENT_WIDTH - x)
    : Math.min(PLACEMENT_WIDTH - x, Math.max(1, Math.round(bounds.width)));
  const height = kind === "ellipse"
    ? oddDimension(bounds.height, PLACEMENT_HEIGHT - y)
    : Math.min(PLACEMENT_HEIGHT - y, Math.max(1, Math.round(bounds.height)));
  return { x, y, width, height };
}

export function primitiveBoundsFromPoints(
  kind: PlacementPrimitiveKind,
  start: PlacementPoint,
  end: PlacementPoint,
): PlacementBounds {
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

export function lineDirectionFromPoints(
  start: PlacementPoint,
  end: PlacementPoint,
): PlacementLineDirection {
  return (end.x - start.x) * (end.y - start.y) < 0 ? "up_right" : "down_right";
}
