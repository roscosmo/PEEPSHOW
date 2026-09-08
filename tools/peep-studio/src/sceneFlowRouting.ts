import {
  buildOrthogonalTransitionRoute,
  buildStateTransitionRoute,
  type StateGraphEntrySide,
  type StateGraphExitSide,
  type StateTransitionRoute,
} from "./stateGraph";
import type { EditorNodePosition, EditorRouteRail } from "./types";

export type SceneFlowRouteRequest = {
  id: string;
  sourceNode: string;
  targetNode: string;
  source: EditorNodePosition;
  target: EditorNodePosition;
  sourceSide: StateGraphExitSide;
  targetSide: StateGraphEntrySide;
  rails?: EditorRouteRail[];
};

export type SceneFlowObstacle = EditorNodePosition & {
  id: string;
  width: number;
  height: number;
};

export type SceneFlowCrossing = EditorNodePosition & {
  orientation: "horizontal" | "vertical";
};

export type PlannedSceneFlowRoute = {
  route: StateTransitionRoute;
  crossings: SceneFlowCrossing[];
};

type Segment = {
  routeId: string;
  orientation: "horizontal" | "vertical";
  fixed: number;
  min: number;
  max: number;
};

function segmentsForRoute(routeId: string, route: StateTransitionRoute): Segment[] {
  const segments: Segment[] = [];
  for (let index = 1; index < route.points.length; index += 1) {
    const start = route.points[index - 1];
    const end = route.points[index];
    if (start.y === end.y && start.x !== end.x) {
      segments.push({
        routeId,
        orientation: "horizontal",
        fixed: start.y,
        min: Math.min(start.x, end.x),
        max: Math.max(start.x, end.x),
      });
    } else if (start.x === end.x && start.y !== end.y) {
      segments.push({
        routeId,
        orientation: "vertical",
        fixed: start.x,
        min: Math.min(start.y, end.y),
        max: Math.max(start.y, end.y),
      });
    }
  }
  return segments;
}

function intervalOverlap(leftMin: number, leftMax: number, rightMin: number, rightMax: number): number {
  return Math.max(0, Math.min(leftMax, rightMax) - Math.max(leftMin, rightMin));
}

function segmentsCross(left: Segment, right: Segment, margin = 0): EditorNodePosition | null {
  if (left.orientation === right.orientation) {
    return null;
  }
  const horizontal = left.orientation === "horizontal" ? left : right;
  const vertical = left.orientation === "vertical" ? left : right;
  if (
    vertical.fixed <= horizontal.min + margin
    || vertical.fixed >= horizontal.max - margin
    || horizontal.fixed <= vertical.min + margin
    || horizontal.fixed >= vertical.max - margin
  ) {
    return null;
  }
  return { x: vertical.fixed, y: horizontal.fixed };
}

function routeShapeScore(route: StateTransitionRoute): number {
  const length = route.points.slice(1).reduce((total, point, index) => {
    const previous = route.points[index];
    return total + Math.abs(point.x - previous.x) + Math.abs(point.y - previous.y);
  }, 0);
  const direct = Math.abs(route.points.at(-1)!.x - route.points[0].x)
    + Math.abs(route.points.at(-1)!.y - route.points[0].y);
  return Math.max(0, route.points.length - 2) * 12 + length * 0.015 + (length - direct) * 0.18;
}

function obstacleScore(
  segments: Segment[],
  obstacles: SceneFlowObstacle[],
  sourceNode: string,
  targetNode: string,
): number {
  let score = 0;
  obstacles.forEach((obstacle) => {
    if (obstacle.id === sourceNode || obstacle.id === targetNode) {
      return;
    }
    const left = obstacle.x - 18;
    const right = obstacle.x + obstacle.width + 18;
    const top = obstacle.y - 18;
    const bottom = obstacle.y + obstacle.height + 18;
    segments.forEach((segment) => {
      const overlap = segment.orientation === "horizontal"
        ? intervalOverlap(segment.min, segment.max, left, right)
        : intervalOverlap(segment.min, segment.max, top, bottom);
      const crosses = segment.orientation === "horizontal"
        ? segment.fixed >= top && segment.fixed <= bottom && overlap > 0
        : segment.fixed >= left && segment.fixed <= right && overlap > 0;
      if (crosses) {
        score += 25000 + overlap * 30;
      }
    });
  });
  return score;
}

function placedRouteScore(candidate: Segment[], placed: Segment[]): number {
  let score = 0;
  candidate.forEach((segment) => {
    placed.forEach((other) => {
      if (segment.orientation === other.orientation) {
        const overlap = intervalOverlap(segment.min, segment.max, other.min, other.max);
        const separation = Math.abs(segment.fixed - other.fixed);
        if (overlap > 0 && separation < 20) {
          score += overlap * (20 - separation) * 1.5;
        }
      } else if (segmentsCross(segment, other, 10) !== null) {
        score += 90;
      }
    });
  });
  return score;
}

function automaticCandidates(
  request: SceneFlowRouteRequest,
  obstacles: SceneFlowObstacle[],
): StateTransitionRoute[] {
  const { source, target } = request;
  if (request.sourceSide !== "right" || request.targetSide !== "left") {
    return [buildStateTransitionRoute({
      sourceX: source.x,
      sourceY: source.y,
      targetX: target.x,
      targetY: target.y,
      sourceSide: request.sourceSide,
      targetSide: request.targetSide,
    })];
  }

  const candidates: StateTransitionRoute[] = [];
  const forwardGap = target.x - source.x;
  if (forwardGap >= 64) {
    const minimumLane = source.x + 24;
    const maximumLane = target.x - 24;
    const laneValues = [
      source.x + forwardGap * 0.25,
      source.x + forwardGap * 0.5,
      source.x + forwardGap * 0.75,
      source.x + 52,
      target.x - 52,
    ];
    [...new Set(laneValues.map((value) => Math.round(Math.max(minimumLane, Math.min(maximumLane, value)))))]
      .forEach((laneX) => {
        candidates.push(buildOrthogonalTransitionRoute([
          source,
          { x: laneX, y: source.y },
          { x: laneX, y: target.y },
          target,
        ], request.targetSide));
      });
  }

  const allTop = Math.min(source.y, target.y, ...obstacles.map((obstacle) => obstacle.y));
  const allBottom = Math.max(source.y, target.y, ...obstacles.map((obstacle) => obstacle.y + obstacle.height));
  const bridgeValues = [
    allTop - 54,
    allBottom + 54,
    Math.min(source.y, target.y) - 54,
    Math.max(source.y, target.y) + 54,
    source.y + (target.y - source.y) / 2,
  ];
  [...new Set(bridgeValues.map(Math.round))].forEach((bridgeY) => {
    candidates.push(buildOrthogonalTransitionRoute([
      source,
      { x: source.x + 48, y: source.y },
      { x: source.x + 48, y: bridgeY },
      { x: target.x - 48, y: bridgeY },
      { x: target.x - 48, y: target.y },
      target,
    ], request.targetSide));
  });

  return candidates;
}

export function planSceneFlowRoutes(
  requests: SceneFlowRouteRequest[],
  obstacles: SceneFlowObstacle[],
): Record<string, PlannedSceneFlowRoute> {
  const plans: Record<string, PlannedSceneFlowRoute> = {};
  const renderOrder = new Map(requests.map((request, index) => [request.id, index]));
  const placedSegments: Segment[] = [];
  const sorted = [...requests].sort((left, right) => {
    const manualDifference = Number((right.rails?.length ?? 0) > 0) - Number((left.rails?.length ?? 0) > 0);
    if (manualDifference !== 0) {
      return manualDifference;
    }
    const leftDistance = Math.abs(left.target.x - left.source.x) + Math.abs(left.target.y - left.source.y);
    const rightDistance = Math.abs(right.target.x - right.source.x) + Math.abs(right.target.y - right.source.y);
    return rightDistance - leftDistance || left.id.localeCompare(right.id);
  });

  sorted.forEach((request) => {
    const candidates = (request.rails?.length ?? 0) > 0
      ? [buildStateTransitionRoute({
          sourceX: request.source.x,
          sourceY: request.source.y,
          targetX: request.target.x,
          targetY: request.target.y,
          sourceSide: request.sourceSide,
          targetSide: request.targetSide,
          rails: request.rails,
        })]
      : automaticCandidates(request, obstacles);
    let best = candidates[0];
    let bestScore = Number.POSITIVE_INFINITY;
    candidates.forEach((candidate, index) => {
      const segments = segmentsForRoute(request.id, candidate);
      const score = obstacleScore(segments, obstacles, request.sourceNode, request.targetNode)
        + placedRouteScore(segments, placedSegments)
        + routeShapeScore(candidate)
        + index * 0.001;
      if (score < bestScore) {
        best = candidate;
        bestScore = score;
      }
    });
    if (best === undefined) {
      return;
    }
    plans[request.id] = { route: best, crossings: [] };
    placedSegments.push(...segmentsForRoute(request.id, best));
  });

  const plannedRequests = sorted.filter((request) => plans[request.id] !== undefined);
  for (let leftIndex = 0; leftIndex < plannedRequests.length; leftIndex += 1) {
    const leftRequest = plannedRequests[leftIndex];
    const leftSegments = segmentsForRoute(leftRequest.id, plans[leftRequest.id].route);
    for (let rightIndex = leftIndex + 1; rightIndex < plannedRequests.length; rightIndex += 1) {
      const rightRequest = plannedRequests[rightIndex];
      if (
        leftRequest.sourceNode === rightRequest.sourceNode
        || leftRequest.targetNode === rightRequest.targetNode
      ) {
        continue;
      }
      const rightSegments = segmentsForRoute(rightRequest.id, plans[rightRequest.id].route);
      leftSegments.forEach((leftSegment) => {
        rightSegments.forEach((rightSegment) => {
          const crossing = segmentsCross(leftSegment, rightSegment, 14);
          if (crossing === null) {
            return;
          }
          const bridgedSegment = (renderOrder.get(leftSegment.routeId) ?? 0) > (renderOrder.get(rightSegment.routeId) ?? 0)
            ? leftSegment
            : rightSegment;
          const targetPlan = plans[bridgedSegment.routeId];
          const key = `${Math.round(crossing.x)}:${Math.round(crossing.y)}`;
          if (!targetPlan.crossings.some((item) => `${Math.round(item.x)}:${Math.round(item.y)}` === key)) {
            targetPlan.crossings.push({ ...crossing, orientation: bridgedSegment.orientation });
          }
        });
      });
    }
  }
  return plans;
}
