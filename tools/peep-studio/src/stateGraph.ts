import type { EditorNodePosition, EditorRouteRail, EditorRouteTokenPositions, InputAction, ProjectEditorData, SceneDocument, SceneExitRecord, StateAction, StateGuard, StateRecord, StateRoute } from "./types";

export type StateGraphExitSide = "left" | "right" | "top" | "bottom";
export type StateGraphEntrySide = "left" | "right" | "top" | "bottom";
export type StateGraphEntryHandle =
  | "entry-top-left"
  | "entry-top-right"
  | "entry-bottom-left"
  | "entry-bottom-right";

export type StateGraphEntryPort = {
  handle: StateGraphEntryHandle;
  side: StateGraphEntrySide;
};

export type StateTransitionRoutePoint = {
  x: number;
  y: number;
};

export type StateTransitionRoute = {
  path: string;
  points: StateTransitionRoutePoint[];
  controlPoints: StateTransitionRoutePoint[];
  rails: EditorRouteRail[];
};

export type StateTransitionRouteSection = {
  controlSegmentIndex: number;
  orientation: "horizontal" | "vertical";
  start: StateTransitionRoutePoint;
  end: StateTransitionRoutePoint;
  center: StateTransitionRoutePoint;
  length: number;
};

export type StateGraphLayoutNode = {
  id: string;
  x: number;
  y: number;
  platformOutputCount?: number;
};

export function nextStateGraphNodePosition(
  nodes: StateGraphLayoutNode[],
  selectedStateId?: string,
): EditorNodePosition {
  if (nodes.length === 0) {
    return { x: 0, y: 0 };
  }
  const anchor = nodes.find((node) => node.id === selectedStateId)
    ?? nodes.reduce((rightmost, node) => node.x > rightmost.x ? node : rightmost);
  const gapX = 360;
  const gapY = 420;
  const isOpen = (x: number, y: number) => !nodes.some(
    (node) => Math.abs(node.x - x) < 330 && Math.abs(node.y - y) < 360,
  );
  for (let radius = 1; radius <= 8; radius += 1) {
    const offsets = [
      [radius, 0],
      [0, radius],
      [-radius, 0],
      [0, -radius],
      [radius, radius],
      [-radius, radius],
      [radius, -radius],
      [-radius, -radius],
    ];
    for (const [column, row] of offsets) {
      const x = anchor.x + column * gapX;
      const y = anchor.y + row * gapY;
      if (isOpen(x, y)) {
        return { x, y };
      }
    }
  }
  return { x: anchor.x + gapX * 9, y: anchor.y };
}

export type StateTransitionLayoutRequest = {
  id: string;
  source: string;
  target: string;
  sourceOutputIndex: number;
  sourceSide?: StateGraphExitSide;
  sourceRatio?: number;
  targetHandle?: StateGraphEntryHandle;
  targetSide?: StateGraphEntrySide;
  rails?: EditorRouteRail[];
};

export type StateTransitionLayout = {
  sourceSide: StateGraphExitSide;
  targetHandle: StateGraphEntryHandle;
  targetSide: StateGraphEntrySide;
  laneX: number;
};

export type GraphStateOutput = {
  id: string;
  routeId: string;
  label: string;
  guardCount: number;
  actionCount: number;
  effectLabels: string[];
  logicalSource: string;
  eventKind: "press" | "release" | "hold" | "repeat";
  triggerKind: "physical" | "platform";
  preferredExitSide?: StateGraphExitSide;
  exitRatio?: number;
  targetState?: string;
  targetStateLabel?: string;
  targetScene?: string;
};

export type GraphStateNode = {
  id: string;
  label: string;
  isEntry: boolean;
  variableTouchCount: number;
  placementOverrideCount: number;
  platformOutputCount: number;
  waitingVisualRef: string;
  outputs: GraphStateOutput[];
  x: number;
  y: number;
};

export type GraphTransitionEdge = {
  id: string;
  source: string;
  target: string;
  label: string;
  route: StateRoute;
  sourceHandle: string;
  guards: StateGuard[];
  actions: StateAction[];
  effectLabels: string[];
  rails: EditorRouteRail[];
  tokenPositions?: EditorRouteTokenPositions;
  targetHandle?: StateGraphEntryHandle;
  targetSide?: StateGraphEntrySide;
  targetKind?: "state" | "scene_exit" | "system_exit";
};

export type GraphSceneEndpointNode = {
  id: string;
  kind: "entry" | "exit" | "system";
  label: string;
  detail: string;
  sceneExitId?: string;
  targetState?: string;
  targetScene?: string;
  declared: boolean;
  x: number;
  y: number;
};

export type StateGraphModel = {
  nodes: GraphStateNode[];
  endpoints: GraphSceneEndpointNode[];
  edges: GraphTransitionEdge[];
  entryEdge?: {
    source: string;
    target: string;
    targetHandle: StateGraphEntryHandle;
    targetSide: StateGraphEntrySide;
  };
};

export type GraphSceneNode = {
  id: string;
  label: string;
  isEntry: boolean;
  stateCount: number;
  routeCount: number;
  entryStateLabel: string;
  exits: GraphSceneExit[];
  x: number;
  y: number;
};

export type GraphSceneExit = {
  id: string;
  endpointKind: "scene_exit" | "route";
  endpointId: string;
  sceneExitId?: string;
  routeId?: string;
  label: string;
  targetScene: string;
  guardCount: number;
  declared: boolean;
};

export type GraphSceneEdge = {
  id: string;
  source: string;
  target: string;
  targetScene: string;
  referenceId?: string;
  label: string;
  sceneExit: GraphSceneExit;
  route?: StateRoute;
};

export type GraphSceneReferenceNode = {
  id: string;
  label: string;
  targetScene: string;
  x: number;
  y: number;
};

export type GraphPackageEntryNode = {
  id: "package-entry";
  targetScene: string;
  x: number;
  y: number;
};

export type SceneFlowGraphModel = {
  nodes: GraphSceneNode[];
  references: GraphSceneReferenceNode[];
  packageEntry?: GraphPackageEntryNode;
  edges: GraphSceneEdge[];
};

const INPUT_LABELS: Record<string, string> = {
  BUTTON_A: "Button A",
  BUTTON_START: "Start",
  BUTTON_B: "Button B",
  BUTTON_L: "Left button",
  BUTTON_R: "Right button",
  JOY_LEFT: "Joystick left",
  JOY_RIGHT: "Joystick right",
  JOY_UP: "Joystick up",
  JOY_DOWN: "Joystick down",
  JOY_UP_LEFT: "Joystick up left",
  JOY_UP_RIGHT: "Joystick up right",
  JOY_DOWN_LEFT: "Joystick down left",
  JOY_DOWN_RIGHT: "Joystick down right",
};

function inputLabel(inputActions: InputAction[], actionRef: string): string {
  const input = inputActions.find((item) => item.action_id === actionRef);
  const sourceLabel = INPUT_LABELS[input?.logical_source ?? ""] ?? input?.logical_source ?? actionRef;
  const eventKind = input?.event_kind ?? "press";
  if (eventKind === "release") {
    return `${sourceLabel} released`;
  }
  if (eventKind === "hold") {
    return `${sourceLabel} held`;
  }
  if (eventKind === "repeat") {
    return `${sourceLabel} repeat`;
  }
  return sourceLabel;
}

function inputKind(inputActions: InputAction[], actionRef: string): "physical" | "platform" {
  const source = inputActions.find((item) => item.action_id === actionRef)?.logical_source ?? "";
  return source.startsWith("BUTTON_") || source.startsWith("JOY_") ? "physical" : "platform";
}

function inputSource(inputActions: InputAction[], actionRef: string): string {
  return inputActions.find((item) => item.action_id === actionRef)?.logical_source ?? actionRef;
}

const PHYSICAL_TRIGGER_EXITS: Record<string, { side: StateGraphExitSide; ratio: number }> = {
  BUTTON_L: { side: "top", ratio: 0.183 },
  BUTTON_R: { side: "top", ratio: 0.817 },
  JOY_UP: { side: "left", ratio: 0.63 },
  JOY_UP_LEFT: { side: "left", ratio: 0.69 },
  JOY_UP_RIGHT: { side: "right", ratio: 0.72 },
  JOY_LEFT: { side: "left", ratio: 0.75 },
  JOY_RIGHT: { side: "bottom", ratio: 0.363 },
  JOY_DOWN: { side: "bottom", ratio: 0.24 },
  JOY_DOWN_LEFT: { side: "bottom", ratio: 0.16 },
  JOY_DOWN_RIGHT: { side: "bottom", ratio: 0.4 },
  BUTTON_START: { side: "bottom", ratio: 0.533 },
  BUTTON_A: { side: "right", ratio: 0.67 },
  BUTTON_B: { side: "bottom", ratio: 0.713 },
};

function countLabel(count: number, singular: string): string {
  if (count === 0) {
    return "";
  }
  return `${count} ${singular}${count === 1 ? "" : "s"}`;
}

function displayRefName(ref: string | undefined, fallback: string): string {
  if (ref === undefined || ref.length === 0) {
    return fallback;
  }
  const label = ref.replaceAll("_", " ");
  return label.charAt(0).toUpperCase() + label.slice(1);
}

function signedValue(value: number | undefined): string {
  if (value === undefined) {
    return "";
  }
  return value > 0 ? `+${value}` : String(value);
}

export function stateActionDescription(action: StateAction): string | null {
  if (action.kind === "request_render") {
    return null;
  }
  if (action.kind === "set_variable") {
    const variableName = displayRefName(action.variable_ref, "variable");
    if (action.operation === "add") {
      return `${variableName}${action.value === undefined ? "" : ` ${signedValue(action.value)}`}`;
    }
    if (action.operation === "assign" || action.operation === "set") {
      return `${variableName}${action.value === undefined ? "" : ` = ${action.value}`}`;
    }
    return variableName;
  }
  if (action.kind === "play_sfx") {
    return `Play sound${action.cue_ref === undefined ? "" : `: ${displayRefName(action.cue_ref, "sound")}`}`;
  }
  if (action.kind === "exit_to_shell") {
    return "Exit to shell";
  }
  if (action.kind === "transition_scene") {
    return "Open scene";
  }
  if (action.kind === "set_element_visibility") {
    return `${action.visible === false ? "Hide" : "Show"} ${displayRefName(action.element_ref, "object")}`;
  }
  if (action.kind === "set_element_position") {
    const target = displayRefName(action.element_ref, "object");
    return action.x === undefined || action.y === undefined ? `Move ${target}` : `Move ${target} to ${action.x}, ${action.y}`;
  }
  if (action.kind === "set_element_frame") {
    return `Change ${displayRefName(action.element_ref, "object")} frame${action.frame_ref === undefined ? "" : ` to ${displayRefName(action.frame_ref, "frame")}`}`;
  }
  if (action.kind === "set_element_waiting_animation") {
    return `Animate ${displayRefName(action.element_ref, "object")}`;
  }
  return "Advanced effect";
}

export function visibleStateActions(route: StateRoute): StateAction[] {
  return route.actions.filter((action) => action.kind !== "request_render" && action.kind !== "exit_to_shell");
}

const GUARD_DESCRIPTION_OPERATORS: Record<string, string> = {
  eq: "is",
  ne: "is not",
  lt: "is less than",
  le: "is at most",
  gt: "is greater than",
  ge: "is at least",
};

export function stateGuardDescription(guard: StateGuard): string {
  const operator = GUARD_DESCRIPTION_OPERATORS[guard.operator] ?? guard.operator;
  return `${displayRefName(guard.variable_ref, "variable")} ${operator} ${guard.value}`;
}

function routeEffectLabels(route: StateRoute): string[] {
  return visibleStateActions(route)
    .map(stateActionDescription)
    .filter((label): label is string => label !== null);
}

function visibleActionCount(route: StateRoute): number {
  return routeEffectLabels(route).length;
}

function routeLabel(route: StateRoute, inputActions: InputAction[]): string {
  const badges = [countLabel(route.guards.length, "rule"), countLabel(visibleActionCount(route), "effect")].filter(Boolean);
  return [inputLabel(inputActions, route.action_ref), ...badges].join(" - ");
}

function statePosition(
  state: StateRecord,
  index: number,
  columns: number,
  savedPositions: Record<string, EditorNodePosition> | undefined,
) {
  return {
    x: savedPositions?.[state.state_id]?.x ?? (index % columns) * 340,
    y: savedPositions?.[state.state_id]?.y ?? Math.floor(index / columns) * 270,
  };
}

const STATE_CARD_ROUTING_WIDTH = 300;
const STATE_CARD_BASE_HEIGHT = 268;
const STATE_CARD_ROUTE_CLEARANCE = 72;
const STATE_CARD_ROUTE_LANE_STEP = 36;
const STATE_PLATFORM_OUTPUT_FIRST_Y = 104;
const STATE_OUTPUT_STEP_Y = 52;
const STATE_ENTRY_HANDLE_INSET = 8;
const STATE_ENTRY_HANDLE_EDGE_INSET = 9;

export const STATE_GRAPH_ENTRY_HANDLES: StateGraphEntryHandle[] = [
  "entry-top-left",
  "entry-top-right",
  "entry-bottom-left",
  "entry-bottom-right",
];

export const STATE_GRAPH_ENTRY_PORTS: StateGraphEntryPort[] = [
  { handle: "entry-top-left", side: "top" },
  { handle: "entry-top-left", side: "left" },
  { handle: "entry-top-right", side: "top" },
  { handle: "entry-top-right", side: "right" },
  { handle: "entry-bottom-left", side: "bottom" },
  { handle: "entry-bottom-left", side: "left" },
  { handle: "entry-bottom-right", side: "bottom" },
  { handle: "entry-bottom-right", side: "right" },
];

function stateCardHeight(platformOutputCount?: number): number {
  return STATE_CARD_BASE_HEIGHT + (platformOutputCount ?? 0) * STATE_OUTPUT_STEP_Y;
}

function centerX(position: EditorNodePosition): number {
  return position.x + STATE_CARD_ROUTING_WIDTH / 2;
}

function centerY(position: EditorNodePosition & { platformOutputCount?: number }): number {
  return position.y + stateCardHeight(position.platformOutputCount) / 2;
}

export function resolveStateExitSide(
  sourcePosition: EditorNodePosition,
  targetPosition: EditorNodePosition | undefined,
): StateGraphExitSide {
  if (targetPosition === undefined) {
    return "right";
  }
  return centerX(targetPosition) < centerX(sourcePosition) ? "left" : "right";
}

export function resolveStateEntryHandle(
  sourcePosition: EditorNodePosition,
  targetPosition: EditorNodePosition | undefined,
): StateGraphEntryHandle {
  if (targetPosition === undefined) {
    return "entry-top-left";
  }
  return resolveStateEntryHandleFromPoint(
    { x: centerX(sourcePosition), y: centerY(sourcePosition) },
    targetPosition,
  );
}

function resolveStateEntryHandleFromPoint(
  sourcePoint: StateTransitionRoutePoint,
  targetPosition: EditorNodePosition,
): StateGraphEntryHandle {
  const verticalSide = sourcePoint.y > centerY(targetPosition) ? "bottom" : "top";
  const horizontalSlot = sourcePoint.x < centerX(targetPosition) ? "left" : "right";
  return `entry-${verticalSide}-${horizontalSlot}` as StateGraphEntryHandle;
}

export function stateEntryHandleSide(handle: StateGraphEntryHandle): StateGraphEntrySide {
  return handle.startsWith("entry-bottom") ? "bottom" : "top";
}

export function stateEntryHandleSides(handle: StateGraphEntryHandle): StateGraphEntrySide[] {
  return [
    handle.startsWith("entry-bottom") ? "bottom" : "top",
    handle.endsWith("-left") ? "left" : "right",
  ];
}

export function stateEntryPortId(handle: StateGraphEntryHandle, side: StateGraphEntrySide): string {
  return `${handle}:${side}`;
}

export function stateEntryPortPoint(
  position: EditorNodePosition & { platformOutputCount?: number },
  handle: StateGraphEntryHandle,
  side: StateGraphEntrySide,
): StateTransitionRoutePoint {
  const height = stateCardHeight(position.platformOutputCount);
  const left = handle.endsWith("-left");
  const top = handle.startsWith("entry-top");
  if (side === "top" || side === "bottom") {
    return {
      x: position.x + (left ? STATE_ENTRY_HANDLE_INSET : STATE_CARD_ROUTING_WIDTH - STATE_ENTRY_HANDLE_INSET),
      y: side === "top" ? position.y : position.y + height,
    };
  }
  return {
    x: side === "left" ? position.x : position.x + STATE_CARD_ROUTING_WIDTH,
    y: position.y + (top ? STATE_ENTRY_HANDLE_EDGE_INSET : height - STATE_ENTRY_HANDLE_EDGE_INSET),
  };
}

export function stateEntryHandlePoint(
  position: EditorNodePosition & { platformOutputCount?: number },
  handle: StateGraphEntryHandle,
): StateTransitionRoutePoint {
  return stateEntryPortPoint(position, handle, stateEntryHandleSide(handle));
}

function resolveStateEntryPortFromPoint(
  sourcePoint: StateTransitionRoutePoint,
  targetPosition: EditorNodePosition & { platformOutputCount?: number },
  handle?: StateGraphEntryHandle,
): StateGraphEntryPort {
  const candidates = handle === undefined
    ? STATE_GRAPH_ENTRY_PORTS
    : STATE_GRAPH_ENTRY_PORTS.filter((port) => port.handle === handle);
  return candidates.reduce((closest, candidate) => {
    const closestPoint = stateEntryPortPoint(targetPosition, closest.handle, closest.side);
    const candidatePoint = stateEntryPortPoint(targetPosition, candidate.handle, candidate.side);
    const closestDistance = Math.hypot(sourcePoint.x - closestPoint.x, sourcePoint.y - closestPoint.y);
    const candidateDistance = Math.hypot(sourcePoint.x - candidatePoint.x, sourcePoint.y - candidatePoint.y);
    return candidateDistance < closestDistance ? candidate : closest;
  });
}

function stateOutputPoint(
  position: EditorNodePosition & { platformOutputCount?: number },
  sourceSide: StateGraphExitSide,
  outputIndex: number,
  sourceRatio?: number,
): StateTransitionRoutePoint {
  const height = stateCardHeight(position.platformOutputCount);
  if (sourceRatio !== undefined) {
    if (sourceSide === "top" || sourceSide === "bottom") {
      return {
        x: position.x + STATE_CARD_ROUTING_WIDTH * sourceRatio,
        y: sourceSide === "top" ? position.y : position.y + height,
      };
    }
    return {
      x: sourceSide === "left" ? position.x : position.x + STATE_CARD_ROUTING_WIDTH,
      y: position.y + height * sourceRatio,
    };
  }
  return {
    x: sourceSide === "left" ? position.x : position.x + STATE_CARD_ROUTING_WIDTH,
    y: position.y + STATE_PLATFORM_OUTPUT_FIRST_Y + outputIndex * STATE_OUTPUT_STEP_Y,
  };
}

function defaultStateLane(
  sourceX: number,
  sourceY: number,
  targetX: number,
  targetY: number,
  sourceSide: StateGraphExitSide,
): number {
  if (sourceSide === "top" || sourceSide === "bottom") {
    const naturallySeparated = sourceSide === "top"
      ? sourceY > targetY
      : sourceY < targetY;
    if (naturallySeparated) {
      return sourceY + (targetY - sourceY) / 2;
    }
    return sourceSide === "top"
      ? Math.min(sourceY, targetY) - STATE_CARD_ROUTE_CLEARANCE
      : Math.max(sourceY, targetY) + STATE_CARD_ROUTE_CLEARANCE;
  }
  const naturallySeparated = sourceSide === "right"
    ? sourceX < targetX
    : sourceX > targetX;
  if (naturallySeparated) {
    return sourceX + (targetX - sourceX) / 2;
  }
  return sourceSide === "left"
    ? Math.min(sourceX, targetX) - STATE_CARD_ROUTE_CLEARANCE
    : Math.max(sourceX, targetX) + STATE_CARD_ROUTE_CLEARANCE;
}

function candidateLaneXs(
  sourceX: number,
  sourceY: number,
  targetX: number,
  targetY: number,
  sourceSide: StateGraphExitSide,
): number[] {
  const verticalExit = sourceSide === "top" || sourceSide === "bottom";
  const outsideLane = verticalExit
    ? sourceSide === "top"
      ? Math.min(sourceY, targetY) - STATE_CARD_ROUTE_CLEARANCE
      : Math.max(sourceY, targetY) + STATE_CARD_ROUTE_CLEARANCE
    : sourceSide === "left"
      ? Math.min(sourceX, targetX) - STATE_CARD_ROUTE_CLEARANCE
      : Math.max(sourceX, targetX) + STATE_CARD_ROUTE_CLEARANCE;
  const lanes = new Set<number>([defaultStateLane(sourceX, sourceY, targetX, targetY, sourceSide)]);
  for (let index = 0; index < 6; index += 1) {
    const negativeDirection = sourceSide === "left" || sourceSide === "top";
    lanes.add(outsideLane + (negativeDirection ? -1 : 1) * index * STATE_CARD_ROUTE_LANE_STEP);
  }
  return [...lanes];
}

function roundedPolylinePath(points: StateTransitionRoutePoint[], radius: number): string {
  if (points.length === 0) {
    return "";
  }
  if (points.length === 1) {
    return `M ${points[0].x} ${points[0].y}`;
  }
  const commands = [`M ${points[0].x} ${points[0].y}`];
  for (let index = 1; index < points.length - 1; index += 1) {
    const previous = points[index - 1];
    const current = points[index];
    const next = points[index + 1];
    const previousDistance = Math.hypot(current.x - previous.x, current.y - previous.y);
    const nextDistance = Math.hypot(next.x - current.x, next.y - current.y);
    const cornerRadius = Math.min(radius, previousDistance / 2, nextDistance / 2);

    if (cornerRadius <= 0) {
      commands.push(`L ${current.x} ${current.y}`);
      continue;
    }

    const before = {
      x: current.x - ((current.x - previous.x) / previousDistance) * cornerRadius,
      y: current.y - ((current.y - previous.y) / previousDistance) * cornerRadius,
    };
    const after = {
      x: current.x + ((next.x - current.x) / nextDistance) * cornerRadius,
      y: current.y + ((next.y - current.y) / nextDistance) * cornerRadius,
    };
    commands.push(`L ${before.x} ${before.y}`);
    commands.push(`Q ${current.x} ${current.y} ${after.x} ${after.y}`);
  }
  const last = points[points.length - 1];
  commands.push(`L ${last.x} ${last.y}`);
  return commands.join(" ");
}

function compactRoutePoints(points: StateTransitionRoutePoint[]): StateTransitionRoutePoint[] {
  const compacted: StateTransitionRoutePoint[] = [];
  points.forEach((point) => {
    const previous = compacted[compacted.length - 1];
    if (previous?.x === point.x && previous.y === point.y) {
      return;
    }
    compacted.push({ ...point });
    while (compacted.length >= 3) {
      const start = compacted[compacted.length - 3];
      const middle = compacted[compacted.length - 2];
      const end = compacted[compacted.length - 1];
      if (!((start.x === middle.x && middle.x === end.x) || (start.y === middle.y && middle.y === end.y))) {
        break;
      }
      compacted.splice(compacted.length - 2, 1);
    }
  });
  return compacted;
}

function routeIntersection(
  firstStart: StateTransitionRoutePoint,
  firstEnd: StateTransitionRoutePoint,
  secondStart: StateTransitionRoutePoint,
  secondEnd: StateTransitionRoutePoint,
): StateTransitionRoutePoint | null {
  const firstHorizontal = firstStart.y === firstEnd.y;
  const secondHorizontal = secondStart.y === secondEnd.y;
  if (firstHorizontal === secondHorizontal) {
    return null;
  }
  const horizontalStart = firstHorizontal ? firstStart : secondStart;
  const horizontalEnd = firstHorizontal ? firstEnd : secondEnd;
  const verticalStart = firstHorizontal ? secondStart : firstStart;
  const verticalEnd = firstHorizontal ? secondEnd : firstEnd;
  const x = verticalStart.x;
  const y = horizontalStart.y;
  const onHorizontal = x >= Math.min(horizontalStart.x, horizontalEnd.x)
    && x <= Math.max(horizontalStart.x, horizontalEnd.x);
  const onVertical = y >= Math.min(verticalStart.y, verticalEnd.y)
    && y <= Math.max(verticalStart.y, verticalEnd.y);
  return onHorizontal && onVertical ? { x, y } : null;
}

function simplifyRoutePoints(points: StateTransitionRoutePoint[]): StateTransitionRoutePoint[] {
  let simplified = compactRoutePoints(points);
  let changed = true;
  while (changed) {
    changed = false;
    for (let first = 0; first < simplified.length - 1 && !changed; first += 1) {
      for (let second = first + 2; second < simplified.length - 1; second += 1) {
        const intersection = routeIntersection(
          simplified[first],
          simplified[first + 1],
          simplified[second],
          simplified[second + 1],
        );
        if (intersection === null) {
          continue;
        }
        simplified = compactRoutePoints([
          ...simplified.slice(0, first + 1),
          intersection,
          ...simplified.slice(second + 1),
        ]);
        changed = true;
        break;
      }
    }
  }
  return simplified;
}

function canonicalRails(rails: EditorRouteRail[]): EditorRouteRail[] {
  const result: EditorRouteRail[] = [];
  rails.forEach((rail) => {
    const rounded: EditorRouteRail = { axis: rail.axis, value: Math.round(rail.value) };
    if (result[result.length - 1]?.axis === rounded.axis) {
      result[result.length - 1] = rounded;
    } else {
      result.push(rounded);
    }
  });
  return result;
}

export function routeRailsFromPoints(
  points: StateTransitionRoutePoint[],
  targetSide: StateGraphEntrySide,
): EditorRouteRail[] {
  const simplified = simplifyRoutePoints(points);
  const target = simplified[simplified.length - 1];
  const targetAxis = targetSide === "left" || targetSide === "right" ? "x" : "y";
  const rails: EditorRouteRail[] = [];
  for (let index = 1; index < simplified.length - 1; index += 1) {
    const previous = simplified[index - 1];
    const point = simplified[index];
    const next = simplified[index + 1];
    const finalSegmentUsesTargetAxis = targetAxis === "x"
      ? point.y === target.y && next.y === target.y
      : point.x === target.x && next.x === target.x;
    if (index === simplified.length - 2 && finalSegmentUsesTargetAxis) {
      continue;
    }
    if (point.x !== previous.x) {
      rails.push({ axis: "x", value: point.x });
    } else if (point.y !== previous.y) {
      rails.push({ axis: "y", value: point.y });
    }
  }
  return canonicalRails(rails);
}

function manualRoutePoints(
  source: StateTransitionRoutePoint,
  target: StateTransitionRoutePoint,
  sourceSide: StateGraphExitSide,
  targetSide: StateGraphEntrySide,
  rails: EditorRouteRail[],
): StateTransitionRoutePoint[] {
  const resolvedRails = canonicalRails(rails);
  const sourceAxis = sourceSide === "left" || sourceSide === "right" ? "x" : "y";
  const sourceValue = sourceAxis === "x" ? source.x : source.y;
  const sourceDirection = sourceSide === "left" || sourceSide === "top" ? -1 : 1;
  const firstRail = resolvedRails[0];
  if (firstRail?.axis !== sourceAxis) {
    resolvedRails.unshift({ axis: sourceAxis, value: sourceValue + sourceDirection * 24 });
  } else if ((firstRail.value - sourceValue) * sourceDirection <= 0) {
    firstRail.value = sourceValue + sourceDirection * 24;
  }

  const targetAxis = targetSide === "left" || targetSide === "right" ? "x" : "y";
  const targetValue = targetAxis === "x" ? target.x : target.y;
  const targetDirection = targetSide === "left" || targetSide === "top" ? -1 : 1;
  const requiredApproachValue = targetValue + targetDirection * 48;
  if (sourceAxis === targetAxis && resolvedRails.length === 1) {
    const sharedRail = resolvedRails[0];
    const sourceIsClear = (sharedRail.value - sourceValue) * sourceDirection > 0;
    const targetIsClear = (sharedRail.value - targetValue) * targetDirection > 0;
    if (!sourceIsClear || !targetIsClear) {
      const bridgeAxis = targetAxis === "x" ? "y" : "x";
      const sourceBridgeValue = bridgeAxis === "x" ? source.x : source.y;
      const targetBridgeValue = bridgeAxis === "x" ? target.x : target.y;
      const bridgeValue = sourceBridgeValue === targetBridgeValue
        ? sourceBridgeValue - STATE_CARD_ROUTE_CLEARANCE
        : sourceBridgeValue + (targetBridgeValue - sourceBridgeValue) / 2;
      resolvedRails.push({ axis: bridgeAxis, value: bridgeValue });
      resolvedRails.push({ axis: targetAxis, value: requiredApproachValue });
    }
  }
  let finalTargetRailIndex = -1;
  for (let index = resolvedRails.length - 1; index >= 0; index -= 1) {
    if (resolvedRails[index].axis === targetAxis) {
      finalTargetRailIndex = index;
      break;
    }
  }
  if (finalTargetRailIndex < 0) {
    resolvedRails.push({ axis: targetAxis, value: requiredApproachValue });
  } else {
    const finalTargetRail = resolvedRails[finalTargetRailIndex];
    const approachIsClear = targetDirection < 0
      ? finalTargetRail.value < targetValue
      : finalTargetRail.value > targetValue;
    if (!approachIsClear) {
      finalTargetRail.value = requiredApproachValue;
    }
  }

  const points: StateTransitionRoutePoint[] = [{ ...source }];
  canonicalRails(resolvedRails).forEach((rail) => {
    const current = points[points.length - 1];
    points.push(rail.axis === "x"
      ? { x: rail.value, y: current.y }
      : { x: current.x, y: rail.value });
  });
  const current = points[points.length - 1];
  if (targetAxis === "y" && current.x !== target.x) {
    points.push({ x: target.x, y: current.y });
  } else if (targetAxis === "x" && current.y !== target.y) {
    points.push({ x: current.x, y: target.y });
  }
  points.push({ ...target });
  return simplifyRoutePoints(points);
}

export function buildStateTransitionRoute({
  sourceX,
  sourceY,
  targetX,
  targetY,
  sourceSide,
  targetSide,
  laneX,
  rails,
}: {
  sourceX: number;
  sourceY: number;
  targetX: number;
  targetY: number;
  sourceSide: StateGraphExitSide;
  targetSide: StateGraphEntrySide;
  laneX?: number;
  rails?: EditorRouteRail[];
}): StateTransitionRoute {
  const horizontalExit = sourceSide === "left" || sourceSide === "right";
  const resolvedLane = laneX ?? defaultStateLane(sourceX, sourceY, targetX, targetY, sourceSide);
  if (rails !== undefined && rails.length > 0) {
    const controlPoints = manualRoutePoints(
      { x: sourceX, y: sourceY },
      { x: targetX, y: targetY },
      sourceSide,
      targetSide,
      rails,
    );
    return {
      path: roundedPolylinePath(controlPoints, 14),
      points: controlPoints,
      controlPoints,
      rails: routeRailsFromPoints(controlPoints, targetSide),
    };
  }
  const targetHorizontal = targetSide === "left" || targetSide === "right";
  const targetDirection = targetSide === "left" || targetSide === "top" ? -1 : 1;
  const approach = targetHorizontal
    ? { x: targetX + targetDirection * 48, y: targetY }
    : { x: targetX, y: targetY + targetDirection * 48 };
  const sourceDirection = sourceSide === "left" || sourceSide === "top" ? -1 : 1;
  const sourceAxisValue = horizontalExit ? sourceX : sourceY;
  const targetAxisValue = targetHorizontal ? targetX : targetY;
  const sourcePoint = { x: sourceX, y: sourceY };
  const targetPoint = { x: targetX, y: targetY };
  let rawPoints: StateTransitionRoutePoint[];
  if (horizontalExit && !targetHorizontal) {
    const corner = { x: targetX, y: sourceY };
    const leavesSourceCleanly = (corner.x - sourceX) * sourceDirection >= 0;
    const reachesTargetCleanly = (corner.y - targetY) * targetDirection >= 0;
    rawPoints = leavesSourceCleanly && reachesTargetCleanly
      ? [sourcePoint, corner, targetPoint]
      : [
          sourcePoint,
          { x: resolvedLane, y: sourceY },
          { x: resolvedLane, y: approach.y },
          approach,
          targetPoint,
        ];
  } else if (!horizontalExit && targetHorizontal) {
    const corner = { x: sourceX, y: targetY };
    const leavesSourceCleanly = (corner.y - sourceY) * sourceDirection >= 0;
    const reachesTargetCleanly = (corner.x - targetX) * targetDirection >= 0;
    rawPoints = leavesSourceCleanly && reachesTargetCleanly
      ? [sourcePoint, corner, targetPoint]
      : [
          sourcePoint,
          { x: sourceX, y: resolvedLane },
          { x: approach.x, y: resolvedLane },
          approach,
          targetPoint,
        ];
  } else if (horizontalExit) {
    const sameDirectionLane = sourceDirection === targetDirection
      ? sourceDirection < 0
        ? Math.min(sourceX, targetX) - STATE_CARD_ROUTE_CLEARANCE
        : Math.max(sourceX, targetX) + STATE_CARD_ROUTE_CLEARANCE
      : resolvedLane;
    const sharedLaneIsClear = (sameDirectionLane - sourceAxisValue) * sourceDirection >= 0
      && (sameDirectionLane - targetAxisValue) * targetDirection >= 0;
    if (sourceY === targetY
      && (targetX - sourceX) * sourceDirection >= 0
      && (sourceX - targetX) * targetDirection >= 0) {
      rawPoints = [sourcePoint, targetPoint];
    } else if (sharedLaneIsClear) {
      rawPoints = [
        sourcePoint,
        { x: sameDirectionLane, y: sourceY },
        { x: sameDirectionLane, y: targetY },
        targetPoint,
      ];
    } else {
      const sourceLane = (resolvedLane - sourceX) * sourceDirection >= 0
        ? resolvedLane
        : sourceX + sourceDirection * 48;
      const bridgeY = Math.abs(sourceY - targetY) >= 48
        ? sourceY + (targetY - sourceY) / 2
        : Math.min(sourceY, targetY) - STATE_CARD_ROUTE_CLEARANCE;
      rawPoints = [
        sourcePoint,
        { x: sourceLane, y: sourceY },
        { x: sourceLane, y: bridgeY },
        { x: approach.x, y: bridgeY },
        approach,
        targetPoint,
      ];
    }
  } else {
    const sameDirectionLane = sourceDirection === targetDirection
      ? sourceDirection < 0
        ? Math.min(sourceY, targetY) - STATE_CARD_ROUTE_CLEARANCE
        : Math.max(sourceY, targetY) + STATE_CARD_ROUTE_CLEARANCE
      : resolvedLane;
    const sharedLaneIsClear = (sameDirectionLane - sourceAxisValue) * sourceDirection >= 0
      && (sameDirectionLane - targetAxisValue) * targetDirection >= 0;
    if (sourceX === targetX
      && (targetY - sourceY) * sourceDirection >= 0
      && (sourceY - targetY) * targetDirection >= 0) {
      rawPoints = [sourcePoint, targetPoint];
    } else if (sharedLaneIsClear) {
      rawPoints = [
        sourcePoint,
        { x: sourceX, y: sameDirectionLane },
        { x: targetX, y: sameDirectionLane },
        targetPoint,
      ];
    } else {
      const sourceLane = (resolvedLane - sourceY) * sourceDirection >= 0
        ? resolvedLane
        : sourceY + sourceDirection * 48;
      const bridgeX = Math.abs(sourceX - targetX) >= 48
        ? sourceX + (targetX - sourceX) / 2
        : Math.min(sourceX, targetX) - STATE_CARD_ROUTE_CLEARANCE;
      rawPoints = [
        sourcePoint,
        { x: sourceX, y: sourceLane },
        { x: bridgeX, y: sourceLane },
        { x: bridgeX, y: approach.y },
        approach,
        targetPoint,
      ];
    }
  }
  const controlPoints = simplifyRoutePoints(rawPoints);
  return {
    path: roundedPolylinePath(controlPoints, 14),
    points: controlPoints,
    controlPoints,
    rails: routeRailsFromPoints(controlPoints, targetSide),
  };
}

export function stateTransitionRouteSections(
  controlPoints: StateTransitionRoutePoint[],
): StateTransitionRouteSection[] {
  const sections: StateTransitionRouteSection[] = [];
  for (let index = 0; index < controlPoints.length - 1; index += 1) {
    const start = controlPoints[index];
    const end = controlPoints[index + 1];
    const horizontal = start.y === end.y && start.x !== end.x;
    const vertical = start.x === end.x && start.y !== end.y;
    if (!horizontal && !vertical) {
      continue;
    }
    sections.push({
      controlSegmentIndex: index,
      orientation: horizontal ? "horizontal" : "vertical",
      start,
      end,
      center: {
        x: start.x + (end.x - start.x) / 2,
        y: start.y + (end.y - start.y) / 2,
      },
      length: Math.abs(horizontal ? end.x - start.x : end.y - start.y),
    });
  }
  return sections;
}

export function moveStateTransitionRouteSection(
  controlPoints: StateTransitionRoutePoint[],
  controlSegmentIndex: number,
  position: StateTransitionRoutePoint,
  sourceSide: StateGraphExitSide,
  targetSide: StateGraphEntrySide,
  maximumRails = 8,
): EditorRouteRail[] | null {
  if (controlSegmentIndex < 0 || controlSegmentIndex >= controlPoints.length - 1) {
    return null;
  }
  const next = controlPoints.map((point) => ({ ...point }));
  const start = next[controlSegmentIndex];
  const end = next[controlSegmentIndex + 1];
  const lastSegmentIndex = controlPoints.length - 2;
  if (controlSegmentIndex === 0) {
    const horizontal = start.y === end.y;
    const distance = Math.abs(horizontal ? end.x - start.x : end.y - start.y);
    const departure = Math.min(24, Math.max(10, distance / 3));
    const direction = horizontal
      ? sourceSide === "left" ? -1 : 1
      : sourceSide === "top" ? -1 : 1;
    const firstCorner = horizontal
      ? { x: start.x + direction * departure, y: start.y }
      : { x: start.x, y: start.y + direction * departure };
    const secondCorner = horizontal
      ? { x: firstCorner.x, y: position.y }
      : { x: position.x, y: firstCorner.y };
    const movedEnd = horizontal ? { x: end.x, y: position.y } : { x: position.x, y: end.y };
    next.splice(1, 1, firstCorner, secondCorner, movedEnd);
  } else if (controlSegmentIndex === lastSegmentIndex) {
    const horizontal = start.y === end.y;
    const distance = Math.abs(horizontal ? end.x - start.x : end.y - start.y);
    const arrival = Math.min(40, Math.max(28, distance / 3));
    const direction = targetSide === "left" || targetSide === "top" ? -1 : 1;
    const movedStart = horizontal ? { x: start.x, y: position.y } : { x: position.x, y: start.y };
    const firstCorner = horizontal
      ? { x: end.x + direction * arrival, y: position.y }
      : { x: position.x, y: end.y + direction * arrival };
    const secondCorner = horizontal
      ? { x: firstCorner.x, y: end.y }
      : { x: end.x, y: firstCorner.y };
    next.splice(controlSegmentIndex, 1, movedStart, firstCorner, secondCorner);
  } else if (start.y === end.y && start.x !== end.x) {
    next[controlSegmentIndex].y = position.y;
    next[controlSegmentIndex + 1].y = position.y;
  } else if (start.x === end.x && start.y !== end.y) {
    next[controlSegmentIndex].x = position.x;
    next[controlSegmentIndex + 1].x = position.x;
  } else {
    return null;
  }
  const rails = routeRailsFromPoints(next, targetSide);
  return rails.length <= maximumRails ? rails : null;
}

export function insertStateTransitionRouteSection(
  controlPoints: StateTransitionRoutePoint[],
  segmentIndex: number,
  position: StateTransitionRoutePoint,
  targetSide: StateGraphEntrySide,
  maximumRails = 8,
): EditorRouteRail[] | null {
  if (segmentIndex < 0 || segmentIndex >= controlPoints.length - 1) {
    return null;
  }
  const start = controlPoints[segmentIndex];
  const end = controlPoints[segmentIndex + 1];
  if (start === undefined || end === undefined) {
    return null;
  }
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const length = Math.hypot(dx, dy);
  if (length < 18 || (dx !== 0 && dy !== 0)) {
    return null;
  }
  const unitX = dx / length;
  const unitY = dy / length;
  const halfSpan = Math.min(24, Math.max(6, length / 4));
  const projected = (position.x - start.x) * unitX + (position.y - start.y) * unitY;
  const center = Math.max(halfSpan + 2, Math.min(length - halfSpan - 2, projected));
  const first = {
    x: Math.round(start.x + unitX * (center - halfSpan)),
    y: Math.round(start.y + unitY * (center - halfSpan)),
  };
  const second = {
    x: Math.round(start.x + unitX * (center + halfSpan)),
    y: Math.round(start.y + unitY * (center + halfSpan)),
  };
  const offset = 32;
  const offsetFirst = dx === 0
    ? { x: first.x + offset, y: first.y }
    : { x: first.x, y: first.y + offset };
  const offsetSecond = dx === 0
    ? { x: second.x + offset, y: second.y }
    : { x: second.x, y: second.y + offset };
  const next = controlPoints.map((point) => ({ ...point }));
  next.splice(segmentIndex + 1, 0, first, offsetFirst, offsetSecond, second);
  const rails = routeRailsFromPoints(next, targetSide);
  return rails.length <= maximumRails ? rails : null;
}

export function removeStateTransitionRouteSection(
  controlPoints: StateTransitionRoutePoint[],
  segmentIndex: number,
  targetSide: StateGraphEntrySide,
  maximumRails = 8,
): EditorRouteRail[] | null {
  if (segmentIndex <= 0 || segmentIndex >= controlPoints.length - 2) {
    return null;
  }
  const before = controlPoints[segmentIndex - 1];
  const after = controlPoints[segmentIndex + 2];
  if (before === undefined || after === undefined) {
    return null;
  }
  const bridge = before.x === after.x || before.y === after.y
    ? []
    : [{ x: after.x, y: before.y }];
  const next = [
    ...controlPoints.slice(0, segmentIndex),
    ...bridge,
    ...controlPoints.slice(segmentIndex + 2),
  ];
  const rails = routeRailsFromPoints(next, targetSide);
  return rails.length <= maximumRails ? rails : null;
}

type StateTransitionSegment = {
  orientation: "horizontal" | "vertical";
  min: number;
  max: number;
  fixed: number;
};

type StateTransitionCandidate = {
  layout: StateTransitionLayout;
  segments: StateTransitionSegment[];
  score: number;
};

function routeSegments(points: StateTransitionRoutePoint[]): StateTransitionSegment[] {
  const segments: StateTransitionSegment[] = [];
  for (let index = 1; index < points.length; index += 1) {
    const previous = points[index - 1];
    const current = points[index];
    if (previous.x === current.x) {
      segments.push({
        orientation: "vertical",
        min: Math.min(previous.y, current.y),
        max: Math.max(previous.y, current.y),
        fixed: previous.x,
      });
    } else if (previous.y === current.y) {
      segments.push({
        orientation: "horizontal",
        min: Math.min(previous.x, current.x),
        max: Math.max(previous.x, current.x),
        fixed: previous.y,
      });
    }
  }
  return segments;
}

function intervalOverlap(leftMin: number, leftMax: number, rightMin: number, rightMax: number): number {
  return Math.max(0, Math.min(leftMax, rightMax) - Math.max(leftMin, rightMin));
}

function overlappingSegmentScore(candidate: StateTransitionSegment[], placed: StateTransitionSegment[]): number {
  let score = 0;
  candidate.forEach((candidateSegment) => {
    placed.forEach((placedSegment) => {
      if (candidateSegment.orientation === placedSegment.orientation) {
        const fixedDistance = Math.abs(candidateSegment.fixed - placedSegment.fixed);
        if (fixedDistance <= 18) {
          const overlap = intervalOverlap(candidateSegment.min, candidateSegment.max, placedSegment.min, placedSegment.max);
          score += overlap * (22 - fixedDistance);
        }
        return;
      }

      const horizontal = candidateSegment.orientation === "horizontal" ? candidateSegment : placedSegment;
      const vertical = candidateSegment.orientation === "vertical" ? candidateSegment : placedSegment;
      const crosses = vertical.fixed >= horizontal.min
        && vertical.fixed <= horizontal.max
        && horizontal.fixed >= vertical.min
        && horizontal.fixed <= vertical.max;
      if (crosses) {
        score += 18;
      }
    });
  });
  return score;
}

function nodeCrossingScore(
  candidate: StateTransitionSegment[],
  nodes: StateGraphLayoutNode[],
  source: string,
  target: string,
): number {
  let score = 0;
  nodes
    .filter((node) => node.id !== source && node.id !== target)
    .forEach((node) => {
      const left = node.x - 10;
      const right = node.x + STATE_CARD_ROUTING_WIDTH + 10;
      const top = node.y - 10;
      const bottom = node.y + stateCardHeight(node.platformOutputCount) + 10;
      candidate.forEach((segment) => {
        if (segment.orientation === "horizontal") {
          const crosses = segment.fixed >= top
            && segment.fixed <= bottom
            && intervalOverlap(segment.min, segment.max, left, right) > 0;
          if (crosses) {
            score += 5000;
          }
          return;
        }
        const crosses = segment.fixed >= left
          && segment.fixed <= right
          && intervalOverlap(segment.min, segment.max, top, bottom) > 0;
        if (crosses) {
          score += 5000;
        }
      });
    });
  return score;
}

function routeShapeScore(points: StateTransitionRoutePoint[]): number {
  const length = points.slice(1).reduce((total, point, index) => {
    const previous = points[index];
    return total + Math.abs(point.x - previous.x) + Math.abs(point.y - previous.y);
  }, 0);
  const directDistance = Math.abs(points.at(-1)!.x - points[0].x)
    + Math.abs(points.at(-1)!.y - points[0].y);
  const bends = Math.max(0, points.length - 2);
  return bends * 8 + length * 0.02 + Math.max(0, length - directDistance) * 0.25;
}

export function planStateTransitionRoutes(
  requests: StateTransitionLayoutRequest[],
  nodes: StateGraphLayoutNode[],
): Record<string, StateTransitionLayout> {
  const nodeById = new Map(nodes.map((node) => [node.id, node]));
  const placedSegments: StateTransitionSegment[] = [];
  const planned: Record<string, StateTransitionLayout> = {};
  const sortedRequests = [...requests].sort((left, right) => {
    const manualDifference = Number((right.rails?.length ?? 0) > 0) - Number((left.rails?.length ?? 0) > 0);
    if (manualDifference !== 0) {
      return manualDifference;
    }
    const leftSource = nodeById.get(left.source);
    const leftTarget = nodeById.get(left.target);
    const rightSource = nodeById.get(right.source);
    const rightTarget = nodeById.get(right.target);
    const leftDistance = leftSource === undefined || leftTarget === undefined
      ? 0
      : Math.abs(centerX(leftSource) - centerX(leftTarget)) + Math.abs(centerY(leftSource) - centerY(leftTarget));
    const rightDistance = rightSource === undefined || rightTarget === undefined
      ? 0
      : Math.abs(centerX(rightSource) - centerX(rightTarget)) + Math.abs(centerY(rightSource) - centerY(rightTarget));
    return rightDistance - leftDistance || left.id.localeCompare(right.id);
  });

  sortedRequests.forEach((request) => {
    const sourcePosition = nodeById.get(request.source);
    const targetPosition = nodeById.get(request.target);
    if (sourcePosition === undefined || targetPosition === undefined) {
      return;
    }
    const preferredSide = request.sourceSide ?? resolveStateExitSide(sourcePosition, targetPosition);
    if ((request.rails?.length ?? 0) > 0) {
      const sourcePoint = stateOutputPoint(
        sourcePosition,
        preferredSide,
        request.sourceOutputIndex,
        request.sourceRatio,
      );
      const targetHandle = request.targetHandle ?? resolveStateEntryHandleFromPoint(sourcePoint, targetPosition);
      const targetPort = request.targetSide === undefined
        ? resolveStateEntryPortFromPoint(sourcePoint, targetPosition, targetHandle)
        : { handle: targetHandle, side: request.targetSide };
      const targetPoint = stateEntryPortPoint(targetPosition, targetPort.handle, targetPort.side);
      const route = buildStateTransitionRoute({
        sourceX: sourcePoint.x,
        sourceY: sourcePoint.y,
        targetX: targetPoint.x,
        targetY: targetPoint.y,
        sourceSide: preferredSide,
        targetSide: targetPort.side,
        rails: request.rails,
      });
      planned[request.id] = {
        sourceSide: preferredSide,
        targetHandle: targetPort.handle,
        targetSide: targetPort.side,
        laneX: defaultStateLane(sourcePoint.x, sourcePoint.y, targetPoint.x, targetPoint.y, preferredSide),
      };
      placedSegments.push(...routeSegments(route.points));
      return;
    }
    const candidateSides: StateGraphExitSide[] = request.sourceSide !== undefined
      ? [request.sourceSide]
      : preferredSide === "right" ? ["right", "left"] : ["left", "right"];
    let best: StateTransitionCandidate | undefined;

    candidateSides.forEach((sourceSide) => {
      const sourcePoint = stateOutputPoint(
        sourcePosition,
        sourceSide,
        request.sourceOutputIndex,
        request.sourceRatio,
      );
      const preferredPort = resolveStateEntryPortFromPoint(sourcePoint, targetPosition);
      const candidatePorts = STATE_GRAPH_ENTRY_PORTS.filter((port) => (
        (request.targetHandle === undefined || request.targetHandle === port.handle)
        && (request.targetSide === undefined || request.targetSide === port.side)
      )).sort((left, right) => {
        const leftPreferred = left.handle === preferredPort.handle && left.side === preferredPort.side;
        const rightPreferred = right.handle === preferredPort.handle && right.side === preferredPort.side;
        return Number(rightPreferred) - Number(leftPreferred);
      });
      candidatePorts.forEach((targetPort) => {
        const targetPoint = stateEntryPortPoint(targetPosition, targetPort.handle, targetPort.side);
        const baseLaneX = defaultStateLane(
          sourcePoint.x,
          sourcePoint.y,
          targetPoint.x,
          targetPoint.y,
          sourceSide,
        );
        candidateLaneXs(
          sourcePoint.x,
          sourcePoint.y,
          targetPoint.x,
          targetPoint.y,
          sourceSide,
        ).forEach((laneX) => {
          const route = buildStateTransitionRoute({
            sourceX: sourcePoint.x,
            sourceY: sourcePoint.y,
            targetX: targetPoint.x,
            targetY: targetPoint.y,
            sourceSide,
            targetSide: targetPort.side,
            laneX,
          });
          const segments = routeSegments(route.points);
          const sidePenalty = sourceSide === preferredSide ? 0 : 24;
          const handlePenalty = targetPort.handle === preferredPort.handle && targetPort.side === preferredPort.side ? 0 : 16;
          const distancePenalty = Math.abs(laneX - baseLaneX) * 0.08;
          const score = sidePenalty
            + handlePenalty
            + distancePenalty
            + routeShapeScore(route.points)
            + overlappingSegmentScore(segments, placedSegments)
            + nodeCrossingScore(segments, nodes, request.source, request.target);
          if (best === undefined || score < best.score) {
            best = {
              layout: {
                sourceSide,
                targetHandle: targetPort.handle,
                targetSide: targetPort.side,
                laneX,
              },
              segments,
              score,
            };
          }
        });
      });
    });

    if (best !== undefined) {
      planned[request.id] = best.layout;
      placedSegments.push(...best.segments);
    }
  });

  return planned;
}

export function buildStateGraphModel(scene: SceneDocument | null, editor?: ProjectEditorData): StateGraphModel {
  const states = scene?.states ?? [];
  const routes = scene?.routes ?? [];
  const declaredSceneExits = scene?.scene_exits ?? [];
  const inputActions = scene?.input_actions ?? [];
  const entryState = scene?.entry_state ?? null;
  const columns = Math.max(1, Math.ceil(Math.sqrt(states.length)));
  const stateIds = new Set(states.map((state) => state.state_id));
  const stateLabels = new Map(states.map((state) => [state.state_id, state.display_name]));
  const outputsByState = new Map<string, GraphStateOutput[]>();
  const variableRefsByState = new Map<string, Set<string>>();
  const savedPositions =
    scene === null ? undefined : editor?.state_graph?.scenes?.[scene.scene_id]?.nodes;
  const savedRouteLayouts =
    scene === null ? undefined : editor?.state_graph?.scenes?.[scene.scene_id]?.routes;
  const savedEntryLayout =
    scene === null ? undefined : editor?.state_graph?.scenes?.[scene.scene_id]?.entry;
  const exitForRoute = (route: StateRoute): SceneExitRecord | undefined => {
    if (route.scene_exit_ref !== undefined) {
      return declaredSceneExits.find((sceneExit) => sceneExit.scene_exit_id === route.scene_exit_ref);
    }
    return declaredSceneExits.find((sceneExit) => sceneExit.target_scene === route.target_scene);
  };
  const legacySceneExits: GraphSceneEndpointNode[] = routes
    .filter((route) => route.target_scene !== undefined && exitForRoute(route) === undefined)
    .map((route, index) => {
      const id = `scene-exit-legacy-${route.route_id}`;
      const savedPosition = savedPositions?.[id];
      return {
        id,
        kind: "exit" as const,
        label: route.target_scene ?? "Scene exit",
        detail: "Legacy scene transition",
        targetScene: route.target_scene,
        declared: false,
        x: savedPosition?.x ?? columns * 340 + 180,
        y: savedPosition?.y ?? index * 104,
      };
    });

  routes.forEach((route: StateRoute) => {
    const effectLabels = routeEffectLabels(route);
    route.from_states
      .filter((source) => stateIds.has(source) && (route.target_scene !== undefined || (route.target_state !== undefined && stateIds.has(route.target_state))))
      .forEach((source) => {
        const outputs = outputsByState.get(source) ?? [];
        const variableRefs = variableRefsByState.get(source) ?? new Set<string>();
        const logicalSource = inputSource(inputActions, route.action_ref);
        const eventKind = inputActions.find((item) => item.action_id === route.action_ref)?.event_kind ?? "press";
        const physicalExit = PHYSICAL_TRIGGER_EXITS[logicalSource];
        route.guards.forEach((guard) => variableRefs.add(guard.variable_ref));
        route.actions.forEach((action) => {
          if (action.kind === "set_variable" && action.variable_ref !== undefined) {
            variableRefs.add(action.variable_ref);
          }
        });
        outputs.push({
          id: `${route.route_id}:${source}`,
          routeId: route.route_id,
          label: inputLabel(inputActions, route.action_ref),
          guardCount: route.guards.length,
          actionCount: effectLabels.length,
          effectLabels,
          logicalSource,
          eventKind,
          triggerKind: inputKind(inputActions, route.action_ref),
          preferredExitSide: physicalExit?.side,
          exitRatio: physicalExit?.ratio,
          targetState: route.target_state,
          targetStateLabel: route.target_state === undefined ? undefined : stateLabels.get(route.target_state),
          targetScene: route.target_scene,
        });
        outputsByState.set(source, outputs);
        variableRefsByState.set(source, variableRefs);
      });
  });

  const nodes = states.map((state: StateRecord, index: number) => {
    const position = statePosition(state, index, columns, savedPositions);
    const outputs = outputsByState.get(state.state_id) ?? [];
    return {
      id: state.state_id,
      label: state.display_name,
      isEntry: state.state_id === entryState,
      variableTouchCount: variableRefsByState.get(state.state_id)?.size ?? 0,
      placementOverrideCount: state.placement_overrides?.length ?? 0,
      platformOutputCount: outputs.filter((output) => output.triggerKind === "platform").length,
      waitingVisualRef: state.waiting_visual_ref,
      outputs,
      x: position.x,
      y: position.y,
    };
  });

  const statePositions = new Map(nodes.map((node) => [node.id, { x: node.x, y: node.y }]));
  const leftmostX = nodes.length === 0 ? 0 : Math.min(...nodes.map((node) => node.x));
  const topmostY = nodes.length === 0 ? 0 : Math.min(...nodes.map((node) => node.y));
  const rightmostX = nodes.length === 0 ? 0 : Math.max(...nodes.map((node) => node.x));
  const entryNodeId = "scene-entry";
  const systemExitNodeId = "system-exit";
  const entryPosition = savedPositions?.[entryNodeId] ?? { x: leftmostX - 220, y: topmostY + 96 };
  const systemExitPosition = savedPositions?.[systemExitNodeId] ?? { x: rightmostX + 440, y: topmostY + 180 };
  const hasSystemExitRoute = routes.some((route) =>
    route.actions.some((action) => action.kind === "exit_to_shell"),
  );
  const declaredEndpoints: GraphSceneEndpointNode[] = declaredSceneExits.map((sceneExit, index) => {
    const id = `scene-exit-${sceneExit.scene_exit_id}`;
    const savedPosition = savedPositions?.[id];
    return {
      id,
      kind: "exit",
      label: sceneExit.display_name,
      detail: `Go to ${sceneExit.target_scene}`,
      sceneExitId: sceneExit.scene_exit_id,
      targetScene: sceneExit.target_scene,
      declared: true,
      x: savedPosition?.x ?? rightmostX + 440,
      y: savedPosition?.y ?? topmostY + index * 104,
    };
  });
  const entryEndpoint: GraphSceneEndpointNode = {
    id: entryNodeId,
    kind: "entry",
    label: "Scene entry",
    detail: stateLabels.get(entryState ?? "") ?? entryState ?? "No entry state",
    targetState: entryState ?? undefined,
    declared: true,
    x: entryPosition.x,
    y: entryPosition.y,
  };
  const systemExitEndpoint: GraphSceneEndpointNode | undefined =
    savedPositions?.[systemExitNodeId] !== undefined || hasSystemExitRoute
      ? {
          id: systemExitNodeId,
          kind: "system",
          label: "Exit to PeepOS",
          detail: "Return control to the system shell",
          declared: savedPositions?.[systemExitNodeId] !== undefined,
          x: systemExitPosition.x,
          y: systemExitPosition.y,
        }
      : undefined;
  const endpoints = [
    entryEndpoint,
    ...declaredEndpoints,
    ...legacySceneExits,
    ...(systemExitEndpoint === undefined ? [] : [systemExitEndpoint]),
  ];
  const endpointByExitId = new Map(
    declaredEndpoints
      .filter((endpoint) => endpoint.sceneExitId !== undefined)
      .map((endpoint) => [endpoint.sceneExitId!, endpoint]),
  );
  const legacyEndpointByRoute = new Map(
    legacySceneExits.map((endpoint) => [endpoint.id.replace("scene-exit-legacy-", ""), endpoint]),
  );

  const edges = routes.flatMap((route: StateRoute) => {
    const targetState = route.target_state;
    const targetsSystemExit = route.actions.some((action) => action.kind === "exit_to_shell");
    const declaredExit = exitForRoute(route);
    const targetEndpoint = declaredExit === undefined
      ? legacyEndpointByRoute.get(route.route_id)
      : endpointByExitId.get(declaredExit.scene_exit_id);
    const target = targetsSystemExit && systemExitEndpoint !== undefined
      ? systemExitEndpoint.id
      : targetState !== undefined && stateIds.has(targetState)
      ? targetState
      : targetEndpoint?.id;
    if (target === undefined) {
      return [];
    }
    return route.from_states
      .filter((source) => stateIds.has(source))
      .map((source) => {
        const savedLayout = savedRouteLayouts?.[route.route_id]?.sources?.[source];
        const currentLayout = savedLayout?.routing_version === 3 ? savedLayout : undefined;
        return {
          id: `${route.route_id}:${source}->${target}`,
          source,
          target,
          label: "",
          route,
          sourceHandle: `${route.route_id}:${source}`,
          guards: route.guards,
          actions: visibleStateActions(route),
          effectLabels: routeEffectLabels(route),
          rails: currentLayout?.rails ?? [],
          tokenPositions: currentLayout?.token_positions,
          targetHandle: currentLayout?.target_handle,
          targetSide: currentLayout?.target_side,
          targetKind: targetsSystemExit
            ? "system_exit" as const
            : targetState === undefined ? "scene_exit" as const : "state" as const,
        };
      });
  });

  return {
    nodes,
    endpoints,
    edges,
    entryEdge: entryState !== null && statePositions.has(entryState)
      ? {
          source: entryNodeId,
          target: entryState,
          targetHandle: savedEntryLayout?.target_handle ?? "entry-top-left",
          targetSide: savedEntryLayout?.target_side ?? "left",
        }
      : undefined,
  };
}

export function buildSceneFlowGraphModel(
  scenes: SceneDocument[],
  entrySceneId: string | null,
  editor?: ProjectEditorData,
): SceneFlowGraphModel {
  const sceneIds = new Set(scenes.map((scene) => scene.scene_id));
  const sceneById = new Map(scenes.map((scene) => [scene.scene_id, scene]));
  const exitsByScene = new Map<string, GraphSceneExit[]>();
  scenes.forEach((scene) => {
    const declared = (scene.scene_exits ?? [])
      .filter((sceneExit) => sceneIds.has(sceneExit.target_scene))
      .map((sceneExit) => ({
        id: `${scene.scene_id}:scene-exit-${sceneExit.scene_exit_id}`,
        endpointKind: "scene_exit" as const,
        endpointId: sceneExit.scene_exit_id,
        sceneExitId: sceneExit.scene_exit_id,
        label: sceneExit.display_name,
        targetScene: sceneExit.target_scene,
        guardCount: 0,
        declared: true,
      }));
    const coveredTargets = new Set(declared.map((sceneExit) => sceneExit.targetScene));
    const legacy = (scene.routes ?? [])
      .filter((route) => route.target_scene !== undefined
        && sceneIds.has(route.target_scene)
        && route.scene_exit_ref === undefined
        && !coveredTargets.has(route.target_scene))
      .map((route) => ({
        id: `${scene.scene_id}:legacy-${route.route_id}`,
        endpointKind: "route" as const,
        endpointId: route.route_id,
        routeId: route.route_id,
        label: routeLabel(route, scene.input_actions ?? []),
        targetScene: route.target_scene!,
        guardCount: route.guards.length,
        declared: false,
      }));
    exitsByScene.set(scene.scene_id, [...declared, ...legacy]);
  });
  const referenceRecords = editor?.scene_flow?.references ?? {};
  const references = Object.entries(referenceRecords)
    .filter(([, reference]) => sceneIds.has(reference.target_scene))
    .map(([referenceId, reference]) => ({
      id: referenceId,
      label: `Go to ${sceneById.get(reference.target_scene)?.display_name ?? reference.target_scene}`,
      targetScene: reference.target_scene,
      x: reference.x,
      y: reference.y,
    }));
  const referenceById = new Map(references.map((reference) => [reference.id, reference]));
  const edges = scenes.flatMap((scene) =>
    (exitsByScene.get(scene.scene_id) ?? []).map((sceneExit) => {
      const route = sceneExit.routeId === undefined
        ? undefined
        : (scene.routes ?? []).find((item) => item.route_id === sceneExit.routeId);
      const mappedReferenceId = editor?.scene_flow?.exit_references?.[scene.scene_id]?.[
        `${sceneExit.endpointKind}:${sceneExit.endpointId}`
      ];
      const mappedReference = mappedReferenceId === undefined
        ? undefined
        : referenceById.get(mappedReferenceId);
      const referenceId = mappedReference?.targetScene === sceneExit.targetScene
        ? mappedReference.id
        : undefined;
      return {
        id: `${sceneExit.id}->${sceneExit.targetScene}`,
        source: scene.scene_id,
        target: referenceId ?? sceneExit.targetScene,
        targetScene: sceneExit.targetScene,
        referenceId,
        label: sceneExit.label,
        sceneExit,
        route,
      };
    }),
  );
  const outgoingCounts = new Map<string, number>();
  edges.forEach((edge) => {
    outgoingCounts.set(edge.source, (outgoingCounts.get(edge.source) ?? 0) + 1);
  });
  const depthByScene = new Map<string, number>();
  if (entrySceneId !== null && sceneIds.has(entrySceneId)) {
    depthByScene.set(entrySceneId, 0);
    const queue = [entrySceneId];
    while (queue.length > 0) {
      const source = queue.shift()!;
      const depth = depthByScene.get(source) ?? 0;
      edges
        .filter((edge) => edge.source === source)
        .forEach((edge) => {
          if (!depthByScene.has(edge.targetScene)) {
            depthByScene.set(edge.targetScene, depth + 1);
            queue.push(edge.targetScene);
          }
        });
    }
  }
  const columnY = new Map<number, number>();
  const nodes = scenes.map((scene) => {
    const states = scene.states ?? [];
    const entryState = states.find((state) => state.state_id === scene.entry_state);
    const isEntry = scene.scene_id === entrySceneId;
    const column = depthByScene.get(scene.scene_id) ?? 0;
    const exits = exitsByScene.get(scene.scene_id) ?? [];
    const y = columnY.get(column) ?? 0;
    columnY.set(column, y + 360 + exits.length * 58);
    const savedPosition = editor?.scene_flow?.nodes?.[scene.scene_id];
    return {
      id: scene.scene_id,
      label: scene.display_name,
      isEntry,
      stateCount: states.length,
      routeCount: outgoingCounts.get(scene.scene_id) ?? 0,
      entryStateLabel: entryState?.display_name ?? scene.entry_state ?? "No start state",
      exits,
      x: savedPosition?.x ?? column * 430,
      y: savedPosition?.y ?? y,
    };
  });
  const entryNode = nodes.find((node) => node.id === entrySceneId);
  const savedPackageEntry = editor?.scene_flow?.package_entry;
  const packageEntry = entryNode === undefined
    ? undefined
    : {
        id: "package-entry" as const,
        targetScene: entryNode.id,
        x: savedPackageEntry?.x ?? entryNode.x - 190,
        y: savedPackageEntry?.y ?? entryNode.y + 96,
      };

  return { nodes, references, packageEntry, edges };
}
