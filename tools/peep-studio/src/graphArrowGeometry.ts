import type { EditorNodePosition } from "./types";

type Point = EditorNodePosition;
export type ArrowSide = "top" | "right" | "bottom" | "left";
export type EntrySocket = { center: Point; width: number; height: number; rotation?: number };
export type IncomingEdge = { id: string; target: string; targetHandle?: string | null };

const angles: Record<ArrowSide, number> = { right: 0, bottom: Math.PI / 2, left: Math.PI, top: -Math.PI / 2 };

export function handleBoundary(
  origin: Point,
  handle: { x: number; y: number; width: number; height: number; position: ArrowSide },
): Point {
  return {
    x: origin.x + handle.x + (handle.position === "left" ? 0 : handle.position === "right" ? handle.width : handle.width / 2),
    y: origin.y + handle.y + (handle.position === "top" ? 0 : handle.position === "bottom" ? handle.height : handle.height / 2),
  };
}

export function incomingPeers(edges: IncomingEdge[], edge: IncomingEdge): string[] {
  return [...edges.filter((peer) => peer.id !== edge.id && peer.target === edge.target
    && peer.targetHandle === edge.targetHandle).map((peer) => peer.id), edge.id].sort();
}

// Intersect the ray with the actual rounded capsule, including rotated state corners.
export function socketBoundary(socket: EntrySocket, angle: number): Point {
  const localAngle = angle - (socket.rotation ?? 0);
  const dx = Math.cos(localAngle);
  const dy = Math.sin(localAngle);
  const radius = Math.min(socket.width, socket.height) / 2;
  const vertical = socket.height >= socket.width;
  const halfStem = Math.abs(socket.height - socket.width) / 2;
  const along = vertical ? Math.abs(dy) : Math.abs(dx);
  const across = vertical ? Math.abs(dx) : Math.abs(dy);
  const sideDistance = across > 0 ? radius / across : Infinity;
  const distance = sideDistance * along <= halfStem
    ? sideDistance
    : halfStem * along + Math.sqrt(Math.max(0, radius * radius - halfStem * halfStem * across * across));
  return { x: socket.center.x + Math.cos(angle) * distance, y: socket.center.y + Math.sin(angle) * distance };
}

function clipPolygon(points: Point[], normal: Point, limit: number): Point[] {
  const result: Point[] = [];
  points.forEach((end, index) => {
    const start = points[(index + points.length - 1) % points.length];
    const a = start.x * normal.x + start.y * normal.y - limit;
    const b = end.x * normal.x + end.y * normal.y - limit;
    if ((a <= 0) !== (b <= 0)) {
      const fraction = a / (a - b);
      result.push({ x: start.x + (end.x - start.x) * fraction, y: start.y + (end.y - start.y) * fraction });
    }
    if (b <= 0) result.push(end);
  });
  return result;
}

export function incomingArrow(socket: EntrySocket, side: ArrowSide, ids: string[], id: string, otherTips: Point[] = []) {
  const peers = ids.length > 0 ? ids : [id];
  const index = Math.max(0, peers.indexOf(id));
  const spread = (socket.rotation === undefined
    ? Math.min(70, 45 + peers.length * 5)
    : Math.min(40, 30 + peers.length * 5)) * Math.PI / 180;
  const tips = peers.map((_, slot) => {
    const angle = angles[side] + (peers.length === 1 ? 0 : (slot / (peers.length - 1) * 2 - 1) * spread);
    return { angle, point: socketBoundary(socket, angle) };
  });
  const { angle, point: tip } = tips[index];
  const allTips = [...tips, ...otherTips.map((point) => ({ point }))];
  const outward = { x: Math.cos(angle), y: Math.sin(angle) };
  // Crowding changes the fan, never the arrowhead dimensions.
  const halfWidth = 6;
  const base = { x: tip.x + outward.x * 10, y: tip.y + outward.y * 10 };
  const arrowPath = `M ${tip.x} ${tip.y} L ${base.x - outward.y * halfWidth} ${base.y + outward.x * halfWidth} L ${base.x + outward.y * halfWidth} ${base.y - outward.x * halfWidth} Z`;
  const routeTarget = peers.length === 1 ? tip : { x: tip.x + outward.x * 28, y: tip.y + outward.y * 28 };
  const leadPath = peers.length === 1 ? "" : ` L ${tip.x} ${tip.y}`;

  // Give each arrow a generous approach-side hit area, clipped at its neighbours'
  // bisectors. Overlapping HTML rectangles must not steal another arrow's click.
  const center = { x: tip.x + outward.x * 10, y: tip.y + outward.y * 10 };
  const box = { x: center.x - 18, y: center.y - 18, width: 36, height: 36 };
  let hitPolygon = [
    { x: box.x, y: box.y }, { x: box.x + 36, y: box.y },
    { x: box.x + 36, y: box.y + 36 }, { x: box.x, y: box.y + 36 },
  ];
  allTips.forEach(({ point }, slot) => {
    if (slot === index) return;
    const normal = { x: point.x - tip.x, y: point.y - tip.y };
    const limit = (point.x * point.x + point.y * point.y - tip.x * tip.x - tip.y * tip.y) / 2;
    hitPolygon = clipPolygon(hitPolygon, normal, limit - 0.2);
  });
  const hitStyle = {
    width: box.width,
    height: box.height,
    transform: `translate(${box.x}px, ${box.y}px)`,
    clipPath: `polygon(${hitPolygon.map((point) => `${(point.x - box.x) / 36 * 100}% ${(point.y - box.y) / 36 * 100}%`).join(",")})`,
  };
  return { tip, routeTarget, arrowPath, leadPath, hitStyle, hitPolygon };
}
