import {
  Background,
  BaseEdge,
  Controls,
  EdgeLabelRenderer,
  Handle,
  MarkerType,
  Panel,
  Position,
  ReactFlow,
  applyNodeChanges,
  getSmoothStepPath,
  type Connection,
  type Edge,
  type EdgeProps,
  type InternalNode,
  type Node,
  type NodeChange,
  type NodeProps,
  type ReactFlowInstance,
  useReactFlow,
  useStore,
  useUpdateNodeInternals,
} from "@xyflow/react";
import {
  Activity,
  ArrowDown,
  ArrowLeft,
  ArrowRight,
  ArrowUp,
  CalendarClock,
  Check,
  Clock,
  ExternalLink,
  Eye,
  Filter,
  Footprints,
  GitBranch,
  Hourglass,
  Image,
  Layers3,
  LogOut,
  Move,
  Network,
  Play,
  Plus,
  Route,
  RotateCcw,
  Triangle,
  Trash2,
  X,
  Volume2,
  Variable,
} from "lucide-react";
import { Fragment, useContext, useEffect, useMemo, useRef, useState, type KeyboardEvent, type PointerEvent as ReactPointerEvent } from "react";
import { ObjectActionContext, ObjectActionPosition } from "./ObjectActionContext";
import { FramebufferCanvas } from "./FramebufferCanvas";
import { ObjectMotionFields } from "./ObjectMotionFields";
import {
  buildSceneFlowGraphModel,
  buildStateGraphModel,
  buildStateTransitionRoute,
  insertStateTransitionRouteSection,
  moveStateTransitionRouteSection,
  nextStateGraphNodePosition,
  planStateTransitionRoutes,
  removeStateTransitionRouteSection,
  resolveStateExitSide,
  stateActionDescription,
  stateEntryPortId,
  stateEntryPortPoint,
  stateGuardDescription,
  stateTransitionRouteSections,
  STATE_GRAPH_ENTRY_HANDLES,
  STATE_GRAPH_ENTRY_PORTS,
  type GraphPackageEntryNode,
  type GraphSceneReferenceNode,
  type GraphSceneNode,
  type GraphSceneEndpointNode,
  type GraphStateNode,
  type StateGraphEntryHandle,
  type StateGraphEntrySide,
  type StateGraphExitSide,
  type StateTransitionLayout,
} from "./stateGraph";
import { planSceneFlowRoutes, type SceneFlowRouteRequest } from "./sceneFlowRouting";
import { handleBoundary, incomingArrow, incomingPeers, type EntrySocket } from "./graphArrowGeometry";
import type {
  AssetRecord,
  AudioCueRecord,
  EditorNodePosition,
  EditorRouteRail,
  EditorRouteTokenPositions,
  Framebuffer,
  InputAction,
  PeepOSTriggerCapability,
  PlacementOwnership,
  ProjectEditorData,
  RenderElement,
  RenderModel,
  SceneDocument,
  SceneExitRecord,
  StateAction,
  StateGuard,
  StateRecord,
  StateRoute,
  StateVariable,
  WaitingVisual,
} from "./types";

export type SceneSelection =
  | { kind: "project" }
  | { kind: "scene" }
  | { kind: "sceneExit"; id: string }
  | { kind: "packageEntry" }
  | { kind: "sceneReference"; id: string }
  | { kind: "systemExit" }
  | { kind: "state"; id: string }
  | { kind: "timer"; id: string }
  | { kind: "timerDraft"; eventType: string; stateId?: string }
  | { kind: "route"; id: string; sourceState?: string }
  | { kind: "render"; id: string }
  | { kind: "waiting"; id: string };

export type StateTriggerEventKind = "press" | "release" | "hold" | "repeat";

const PHYSICAL_TRIGGER_EVENT_KINDS: StateTriggerEventKind[] = ["press", "hold", "release", "repeat"];

export type NewStateTransitionTarget =
  | {
      kind: "state";
      stateId: string;
      targetHandle: StateGraphEntryHandle;
      targetSide: StateGraphEntrySide;
    }
  | { kind: "sceneExit"; sceneExitId: string }
  | { kind: "systemExit" };

type PendingPhysicalTriggerConnection = {
  sceneId: string;
  sourceState: string;
  logicalSource: string;
  target: NewStateTransitionTarget;
  eventKinds: StateTriggerEventKind[];
  eventKind: StateTriggerEventKind;
};

function fieldText(value: unknown): string {
  if (value === undefined || value === null) {
    return "-";
  }
  if (typeof value === "boolean") {
    return value ? "true" : "false";
  }
  return String(value);
}

function InspectorList({ rows }: { rows: Array<[string, unknown]> }) {
  return (
    <dl className="inspector-list">
      {rows.map(([label, value]) => (
        <div key={label}>
          <dt>{label}</dt>
          <dd>{fieldText(value)}</dd>
        </div>
      ))}
    </dl>
  );
}

function EmptyInspector({ children }: { children: string }) {
  return <p className="muted">{children}</p>;
}

function GuardList({ guards }: { guards: StateGuard[] }) {
  if (guards.length === 0) {
    return <EmptyInspector>No conditions.</EmptyInspector>;
  }
  return (
    <ol className="ordered-records">
      {guards.map((guard, index) => (
        <li key={`${guard.variable_ref}-${index}`}>
          <code>{guard.variable_ref}</code> {guard.operator} <strong>{guard.value}</strong>
        </li>
      ))}
    </ol>
  );
}

const GUARD_OPERATORS = ["eq", "ne", "lt", "le", "gt", "ge"] as const;
const ACTION_OPERATIONS = ["assign", "add"] as const;
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
const TRIGGER_EVENT_LABELS: Record<StateTriggerEventKind, string> = {
  press: "Press",
  release: "Release",
  hold: "Hold",
  repeat: "Repeat",
};
const NEW_SCENE_EXIT_HANDLE = "__new_scene_exit__";
const GUARD_OPERATOR_LABELS: Record<string, string> = {
  eq: "is",
  ne: "is not",
  lt: "is less than",
  le: "is at most",
  gt: "is greater than",
  ge: "is at least",
};

type MiniMapNode = {
  id: string;
  x: number;
  y: number;
};

type MiniMapEdge = {
  source: string;
  target: string;
};

function GraphMiniMap({
  nodes,
  edges,
  selectedId,
}: {
  nodes: MiniMapNode[];
  edges: MiniMapEdge[];
  selectedId: string | null;
}) {
  if (nodes.length === 0) {
    return null;
  }
  const nodeWidth = 220;
  const nodeHeight = 130;
  const padding = 80;
  const minX = Math.min(...nodes.map((node) => node.x));
  const minY = Math.min(...nodes.map((node) => node.y));
  const maxX = Math.max(...nodes.map((node) => node.x + nodeWidth));
  const maxY = Math.max(...nodes.map((node) => node.y + nodeHeight));
  const width = Math.max(1, maxX - minX + padding * 2);
  const height = Math.max(1, maxY - minY + padding * 2);
  const byId = new Map(nodes.map((node) => [node.id, node]));

  return (
    <div className="graph-mini-map" aria-hidden="true">
      <svg viewBox={`${minX - padding} ${minY - padding} ${width} ${height}`}>
        {edges.map((edge) => {
          const source = byId.get(edge.source);
          const target = byId.get(edge.target);
          if (source === undefined || target === undefined) {
            return null;
          }
          return (
            <line
              key={`${edge.source}->${edge.target}`}
              x1={source.x + nodeWidth}
              y1={source.y + nodeHeight / 2}
              x2={target.x}
              y2={target.y + nodeHeight / 2}
            />
          );
        })}
        {nodes.map((node) => (
          <rect
            key={node.id}
            className={selectedId === node.id ? "selected" : undefined}
            x={node.x}
            y={node.y}
            width={nodeWidth}
            height={nodeHeight}
            rx="12"
          />
        ))}
      </svg>
    </div>
  );
}

function displayInputLabel(source: string | undefined, fallback: string): string {
  if (source === undefined) {
    return fallback;
  }
  return INPUT_LABELS[source] ?? source;
}

function displayInputTrigger(input: InputAction | undefined, fallback: string): string {
  const label = displayInputLabel(input?.logical_source, fallback);
  const eventKind = input?.event_kind ?? "press";
  if (eventKind === "release") {
    return `${label} released`;
  }
  if (eventKind === "hold") {
    return `${label} held`;
  }
  if (eventKind === "repeat") {
    return `${label} repeat`;
  }
  return label;
}

function displayStateName(states: StateRecord[], stateId: string): string {
  return states.find((state) => state.state_id === stateId)?.display_name ?? stateId;
}

function displayVariableName(variableId: string): string {
  const friendlyNames: Record<string, string> = {
    selected_index: "Selected item",
  };
  if (friendlyNames[variableId] !== undefined) {
    return friendlyNames[variableId];
  }
  return variableId
    .split("_")
    .filter(Boolean)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

function visibleEffectCount(actions: Array<{ kind: string }>): number {
  return actions.filter((action) => action.kind !== "request_render" && action.kind !== "exit_to_shell").length;
}

type StateCardNodeData = {
  graphNode: RoutedGraphStateNode;
  activeEntryHandles: string[];
  runtimeActive: boolean;
  canEdit: boolean;
  selectedRouteId: string | null;
  joystickPolicy: "four_way" | "eight_way";
  physicalEventKinds: StateTriggerEventKind[];
  hasPeepOSTriggers: boolean;
  peepOSTriggerOpen: boolean;
  onSelectState: (stateId: string) => void;
  onSelectRoute: (routeId: string, sourceState: string) => void;
  onOpenPeepOSTriggers: (stateId: string) => void;
};

type StateTriggerStemLayout = {
  width: number;
  height: number;
  lines: Array<{ slot: string; x1: number; y1: number; x2: number; y2: number }>;
  sockets: Record<string, { x: number; y: number }>;
};

type RoutedStateOutput = GraphStateNode["outputs"][number] & {
  exitSide: StateGraphExitSide;
};

type RoutedGraphStateNode = Omit<GraphStateNode, "outputs"> & {
  outputs: RoutedStateOutput[];
};

type StateNodePosition = {
  x: number;
  y: number;
  platformOutputCount?: number;
};

const STATE_ENTRY_PORT_HANDLES = [
  { handle: "entry-top-left", side: "top", position: Position.Top, style: { left: 8, top: 0 } },
  { handle: "entry-top-left", side: "left", position: Position.Left, style: { left: 0, top: 9 } },
  { handle: "entry-top-right", side: "top", position: Position.Top, style: { left: "calc(100% - 8px)", top: 0 } },
  { handle: "entry-top-right", side: "right", position: Position.Right, style: { left: "100%", top: 9 } },
  { handle: "entry-bottom-left", side: "bottom", position: Position.Bottom, style: { left: 8, top: "100%" } },
  { handle: "entry-bottom-left", side: "left", position: Position.Left, style: { left: 0, top: "calc(100% - 9px)" } },
  { handle: "entry-bottom-right", side: "bottom", position: Position.Bottom, style: { left: "calc(100% - 8px)", top: "100%" } },
  { handle: "entry-bottom-right", side: "right", position: Position.Right, style: { left: "100%", top: "calc(100% - 9px)" } },
] as const;

const PHYSICAL_TRIGGER_CONTROLS = [
  { source: "BUTTON_L", label: "L", slot: "button-l", side: "top" },
  { source: "BUTTON_R", label: "R", slot: "button-r", side: "top" },
  { source: "JOY_UP", label: "up", slot: "joy-up", side: "left" },
  { source: "JOY_LEFT", label: "left", slot: "joy-left", side: "left" },
  { source: "JOY_RIGHT", label: "right", slot: "joy-right", side: "bottom" },
  { source: "JOY_DOWN", label: "down", slot: "joy-down", side: "bottom" },
  { source: "BUTTON_START", label: "Start", slot: "button-start", side: "bottom" },
  { source: "BUTTON_A", label: "A", slot: "button-a", side: "right" },
  { source: "BUTTON_B", label: "B", slot: "button-b", side: "bottom" },
] as const;

const OPTIONAL_DIAGONAL_CONTROLS = [
  { source: "JOY_UP_LEFT", label: "up left", slot: "joy-up-left", side: "left" },
  { source: "JOY_UP_RIGHT", label: "up right", slot: "joy-up-right", side: "right" },
  { source: "JOY_DOWN_LEFT", label: "down left", slot: "joy-down-left", side: "bottom" },
  { source: "JOY_DOWN_RIGHT", label: "down right", slot: "joy-down-right", side: "bottom" },
] as const;

function physicalEventKindsForSource(
  source: string,
  advertisedKinds: StateTriggerEventKind[],
): StateTriggerEventKind[] {
  if (source === "BUTTON_START") {
    return advertisedKinds.includes("press") ? ["press"] : [];
  }
  return PHYSICAL_TRIGGER_EVENT_KINDS.filter((eventKind) => advertisedKinds.includes(eventKind));
}

function availablePhysicalEventKinds(
  graphNode: GraphStateNode,
  source: string,
  advertisedKinds: StateTriggerEventKind[],
): StateTriggerEventKind[] {
  const usedKinds = new Set(
    graphNode.outputs
      .filter((output) => output.triggerKind === "physical" && output.logicalSource === source)
      .map((output) => output.eventKind),
  );
  return physicalEventKindsForSource(source, advertisedKinds).filter((eventKind) => !usedKinds.has(eventKind));
}

function physicalEventBadge(eventKind: StateTriggerEventKind): string | null {
  if (eventKind === "hold") {
    return "H";
  }
  if (eventKind === "release") {
    return "U";
  }
  if (eventKind === "repeat") {
    return "R";
  }
  return null;
}

function PeepOSTriggerGlyph({ kind }: { kind: string }) {
  if (kind === "step_count") {
    return <Footprints size={17} aria-hidden="true" />;
  }
  if (kind === "delay_elapsed") {
    return <Hourglass size={17} aria-hidden="true" />;
  }
  if (kind === "local_schedule") {
    return <CalendarClock size={17} aria-hidden="true" />;
  }
  if (kind === "device_active") {
    return <Play size={17} aria-hidden="true" />;
  }
  if (kind === "device_inactive") {
    return <LogOut size={17} aria-hidden="true" />;
  }
  if (kind === "wake_resume") {
    return <RotateCcw size={17} aria-hidden="true" />;
  }
  if (kind === "animation_complete") {
    return <Image size={17} aria-hidden="true" />;
  }
  if (kind === "audio_marker") {
    return <Volume2 size={17} aria-hidden="true" />;
  }
  return <Activity size={17} aria-hidden="true" />;
}

function triggerPosition(side: StateGraphExitSide): Position {
  if (side === "left") {
    return Position.Left;
  }
  if (side === "top") {
    return Position.Top;
  }
  if (side === "bottom") {
    return Position.Bottom;
  }
  return Position.Right;
}

function PhysicalTriggerGlyph({ label }: { label: string }) {
  if (label === "up") {
    return <ArrowUp size={16} aria-hidden="true" />;
  }
  if (label === "down") {
    return <ArrowDown size={16} aria-hidden="true" />;
  }
  if (label === "left") {
    return <ArrowLeft size={16} aria-hidden="true" />;
  }
  if (label === "right") {
    return <ArrowRight size={16} aria-hidden="true" />;
  }
  if (label === "up left") {
    return <span aria-hidden="true">&#8598;</span>;
  }
  if (label === "up right") {
    return <span aria-hidden="true">&#8599;</span>;
  }
  if (label === "down left") {
    return <span aria-hidden="true">&#8601;</span>;
  }
  if (label === "down right") {
    return <span aria-hidden="true">&#8600;</span>;
  }
  return <span>{label}</span>;
}

function statePositionMap(graphNodes: GraphStateNode[], flowNodes: Node[]): Map<string, StateNodePosition> {
  const positions = new Map<string, StateNodePosition>(
    graphNodes.map((node) => [node.id, {
      x: node.x,
      y: node.y,
      platformOutputCount: node.platformOutputCount,
    }]),
  );
  flowNodes.forEach((node) => {
    positions.set(node.id, {
      x: node.position.x,
      y: node.position.y,
      platformOutputCount: positions.get(node.id)?.platformOutputCount,
    });
  });
  return positions;
}

function routeStateNode(
  graphNode: GraphStateNode,
  positions: Map<string, StateNodePosition>,
  transitionLayouts: Record<string, StateTransitionLayout>,
): RoutedGraphStateNode {
  const sourcePosition = positions.get(graphNode.id) ?? { x: graphNode.x, y: graphNode.y };
  return {
    ...graphNode,
    outputs: graphNode.outputs.map((output) => ({
      ...output,
      exitSide: transitionLayouts[output.id]?.sourceSide ?? output.preferredExitSide ?? resolveStateExitSide(
        sourcePosition,
        output.targetState === undefined ? undefined : positions.get(output.targetState),
      ),
    })),
  };
}

function StateCardNode({ data, selected }: NodeProps<Node<StateCardNodeData>>) {
  const {
    activeEntryHandles,
    canEdit,
    graphNode,
    hasPeepOSTriggers,
    joystickPolicy,
    peepOSTriggerOpen,
    physicalEventKinds,
    runtimeActive,
    selectedRouteId,
    onOpenPeepOSTriggers,
    onSelectRoute,
    onSelectState,
  } = data;
  const updateNodeInternals = useUpdateNodeInternals();
  const cardRef = useRef<HTMLDivElement>(null);
  const [stemLayout, setStemLayout] = useState<StateTriggerStemLayout>({
    width: 0,
    height: 0,
    lines: [],
    sockets: {},
  });
  const physicalOutputsBySource = new Map<string, RoutedStateOutput[]>();
  graphNode.outputs
    .filter((output) => output.triggerKind === "physical")
    .forEach((output) => {
      const outputs = physicalOutputsBySource.get(output.logicalSource) ?? [];
      outputs.push(output);
      physicalOutputsBySource.set(output.logicalSource, outputs);
    });
  const physicalSources = new Set<string>([
    ...PHYSICAL_TRIGGER_CONTROLS.map((control) => control.source),
    ...OPTIONAL_DIAGONAL_CONTROLS.map((control) => control.source),
  ]);
  const dynamicOutputs = graphNode.outputs.filter(
    (output) => output.triggerKind === "platform" || !physicalSources.has(output.logicalSource),
  );
  const physicalControls = [
    ...PHYSICAL_TRIGGER_CONTROLS,
    ...OPTIONAL_DIAGONAL_CONTROLS.filter(
      (control) => joystickPolicy === "eight_way" || physicalOutputsBySource.has(control.source),
    ),
  ];
  const outputSideKey = graphNode.outputs
    .map((output) => `${output.id}:${output.logicalSource}:${output.exitSide}`)
    .join("|");
  const stemSocketKey = Object.entries(stemLayout.sockets)
    .map(([slot, point]) => `${slot}:${point.x}:${point.y}`)
    .join("|");
  useEffect(() => {
    updateNodeInternals(graphNode.id);
  }, [graphNode.id, outputSideKey, stemSocketKey, updateNodeInternals]);

  useEffect(() => {
    const card = cardRef.current;
    if (card === null) {
      return;
    }
    const measure = () => {
      const cardRect = card.getBoundingClientRect();
      const borderWidth = card.offsetWidth;
      const borderHeight = card.offsetHeight;
      const width = card.clientWidth;
      const height = card.clientHeight;
      if (borderWidth === 0 || borderHeight === 0 || width === 0 || height === 0 || cardRect.width === 0 || cardRect.height === 0) {
        return;
      }
      const scaleX = cardRect.width / borderWidth;
      const scaleY = cardRect.height / borderHeight;
      const originX = cardRect.left + card.clientLeft * scaleX;
      const originY = cardRect.top + card.clientTop * scaleY;
      const sockets: Record<string, { x: number; y: number }> = {};
      const lines = physicalControls.flatMap((control) => {
        const trigger = card.querySelector<HTMLElement>(`[data-trigger-slot="${control.slot}"]`);
        if (trigger === null) {
          return [];
        }
        const triggerRect = trigger.getBoundingClientRect();
        const x1 = (triggerRect.left - originX + triggerRect.width / 2) / scaleX;
        const y1 = (triggerRect.top - originY + triggerRect.height / 2) / scaleY;
        const outputs = physicalOutputsBySource.get(control.source) ?? [];
        const primaryOutput = outputs.find((output) => output.eventKind === "press") ?? outputs[0];
        const side = primaryOutput?.exitSide ?? control.side;
        const endpoint = {
          x: side === "left" ? 0 : side === "right" ? width : x1,
          y: side === "top" ? 0 : side === "bottom" ? height : y1,
        };
        sockets[control.slot] = endpoint;
        return [{
          slot: control.slot,
          x1,
          y1,
          x2: endpoint.x,
          y2: endpoint.y,
        }];
      });
      setStemLayout({ width, height, lines, sockets });
    };
    const frame = window.requestAnimationFrame(measure);
    const observer = new ResizeObserver(measure);
    observer.observe(card);
    return () => {
      window.cancelAnimationFrame(frame);
      observer.disconnect();
    };
  }, [graphNode.id, outputSideKey]);

  const renderPhysicalControl = (source: string, label: string, slot: string) => {
    const outputs = physicalOutputsBySource.get(source) ?? [];
    const primaryOutput = outputs.find((output) => output.eventKind === "press") ?? outputs[0];
    const isActive = outputs.length > 0;
    const isSelected = outputs.some((output) => selectedRouteId === output.routeId);
    const lifecycleBadges = outputs
      .map((output) => ({ eventKind: output.eventKind, label: physicalEventBadge(output.eventKind) }))
      .filter((badge): badge is { eventKind: StateTriggerEventKind; label: string } => badge.label !== null);
    return (
      <button
        className={`state-physical-trigger trigger-${slot} nodrag nopan ${isActive ? "active" : "inactive"} ${isSelected ? "selected" : ""}`}
        type="button"
        disabled={!isActive}
        aria-label={`${label} trigger${isActive ? ", configured" : ", unused"}`}
        title={`${label}${isActive ? " trigger" : " unused"}`}
        data-trigger-slot={slot}
        onClick={(event) => {
          event.stopPropagation();
          if (primaryOutput !== undefined) {
            onSelectRoute(primaryOutput.routeId, graphNode.id);
          }
        }}
      >
        <PhysicalTriggerGlyph label={label} />
        {lifecycleBadges.length > 0 && (
          <span className="state-physical-event-badges" aria-label={lifecycleBadges.map((badge) => TRIGGER_EVENT_LABELS[badge.eventKind]).join(", ")}>
            {lifecycleBadges.map((badge) => (
              <small key={badge.eventKind} title={TRIGGER_EVENT_LABELS[badge.eventKind]}>{badge.label}</small>
            ))}
          </span>
        )}
      </button>
    );
  };

  return (
    <div
      ref={cardRef}
      className={`state-card-node ${graphNode.isEntry ? "entry" : ""} ${runtimeActive ? "runtime-active" : ""} ${selected ? "selected" : ""}`}
      role="button"
      tabIndex={0}
      aria-current={runtimeActive ? "step" : undefined}
      onClick={() => onSelectState(graphNode.id)}
      onKeyDown={(event) => {
        if (event.target === event.currentTarget && (event.key === "Enter" || event.key === " ")) {
          event.preventDefault();
          onSelectState(graphNode.id);
        }
      }}
    >
      {STATE_GRAPH_ENTRY_HANDLES.map((handle) => (
        <span
          aria-hidden="true"
          className={`state-entry-zone ${handle} ${activeEntryHandles.includes(handle) ? "active" : ""}`}
          key={handle}
        />
      ))}
      {STATE_ENTRY_PORT_HANDLES.map((port) => (
        <Handle
          key={stateEntryPortId(port.handle, port.side)}
          id={stateEntryPortId(port.handle, port.side)}
          className="state-entry-port"
          style={port.style}
          type="target"
          position={port.position}
        />
      ))}
      {physicalControls.map((control) => {
        const outputs = physicalOutputsBySource.get(control.source) ?? [];
        const primaryOutput = outputs.find((output) => output.eventKind === "press") ?? outputs[0];
        const side = primaryOutput?.exitSide ?? control.side;
        const availableEventKinds = availablePhysicalEventKinds(graphNode, control.source, physicalEventKinds);
        const canCreate = canEdit && availableEventKinds.length > 0;
        const className = `state-physical-socket socket-${control.slot} ${outputs.length === 0 ? "inactive" : "active"} ${outputs.some((output) => selectedRouteId === output.routeId) ? "selected" : ""}`;
        const socket = stemLayout.sockets[control.slot];
        const socketStyle = socket === undefined ? undefined : {
          left: socket.x,
          top: socket.y,
          right: "auto",
          bottom: "auto",
          transform: "translate(-50%, -50%)",
        };
        return (
          <Fragment key={control.source}>
            {outputs.map((output) => (
              <Handle
                className="state-physical-route-anchor"
                id={output.id}
                isConnectable={false}
                key={output.id}
                position={triggerPosition(side)}
                style={socketStyle}
                type="source"
              />
            ))}
            {canCreate ? (
            <Handle
              className={className}
              data-socket-slot={control.slot}
              id={`new-physical-trigger:${control.source}`}
              isConnectable
              position={triggerPosition(side)}
              style={socketStyle}
              title={`Create ${INPUT_LABELS[control.source] ?? control.label} transition (${availableEventKinds.map((eventKind) => TRIGGER_EVENT_LABELS[eventKind]).join(", ")})`}
              type="source"
            />
            ) : (
              <span className={className} data-socket-slot={control.slot} style={socketStyle} aria-hidden="true" />
            )}
          </Fragment>
        );
      })}
      {stemLayout.width > 0 && stemLayout.height > 0 && (
        <svg
          aria-hidden="true"
          className="state-trigger-stem-layer"
          viewBox={`0 0 ${stemLayout.width} ${stemLayout.height}`}
          preserveAspectRatio="none"
        >
          {stemLayout.lines.map((line) => (
            <line key={line.slot} x1={line.x1} y1={line.y1} x2={line.x2} y2={line.y2} />
          ))}
        </svg>
      )}
      <div className="state-card-topline">
        {renderPhysicalControl("BUTTON_L", "L", "button-l")}
        <div className="state-badge-strip" aria-label="State summary">
          <span className="state-summary-badge" title="Variables used by this state">
            <span>Xy</span>
            <strong>{graphNode.variableTouchCount}</strong>
          </span>
          <span className="state-summary-badge" title="Objects changed by this state">
            <span>Obj</span>
            <strong>{graphNode.placementOverrideCount}</strong>
          </span>
        </div>
        {renderPhysicalControl("BUTTON_R", "R", "button-r")}
      </div>
      <strong className="state-card-name">{graphNode.label}</strong>
      {dynamicOutputs.length > 0 && (
        <div className="state-output-list">
          {dynamicOutputs.map((output) => (
            <span
              className={`state-output-row trigger-${output.triggerKind} exit-${output.exitSide} ${selectedRouteId === output.routeId ? "selected" : ""}`}
              key={output.id}
              role="button"
              tabIndex={0}
              onClick={(event) => {
                event.stopPropagation();
                onSelectRoute(output.routeId, graphNode.id);
              }}
              onKeyDown={(event) => {
                if (event.key === "Enter" || event.key === " ") {
                  event.preventDefault();
                  event.stopPropagation();
                  onSelectRoute(output.routeId, graphNode.id);
                }
              }}
            >
              <span className="state-trigger-label">{output.label}</span>
              {output.guardCount > 0 && (
                <small>{output.guardCount} condition{output.guardCount === 1 ? "" : "s"}</small>
              )}
              <Handle id={output.id} type="source" position={output.exitSide === "left" ? Position.Left : Position.Right} />
            </span>
          ))}
        </div>
      )}
      <div className="state-add-trigger nodrag nopan">
        <button
          aria-expanded={peepOSTriggerOpen}
          className="state-add-trigger-row"
          type="button"
          disabled={!hasPeepOSTriggers}
          title="Browse PeepOS triggers"
          onClick={(event) => {
            event.stopPropagation();
            onOpenPeepOSTriggers(graphNode.id);
          }}
        >
          <Plus size={13} aria-hidden="true" />
          <span>Add new trigger</span>
        </button>
        <span className="state-add-trigger-socket left" aria-hidden="true" />
        <span className="state-add-trigger-socket right" aria-hidden="true" />
      </div>
      <div className="state-controller-map" aria-label="Physical triggers">
        <div className="state-joystick-triggers">
          {OPTIONAL_DIAGONAL_CONTROLS
            .filter((control) => physicalOutputsBySource.has(control.source))
            .map((control) => renderPhysicalControl(control.source, control.label, control.slot))}
          {renderPhysicalControl("JOY_UP", "up", "joy-up")}
          {renderPhysicalControl("JOY_LEFT", "left", "joy-left")}
          {renderPhysicalControl("JOY_RIGHT", "right", "joy-right")}
          {renderPhysicalControl("JOY_DOWN", "down", "joy-down")}
        </div>
        {renderPhysicalControl("BUTTON_START", "Start", "button-start")}
        <div className="state-face-triggers">
          {renderPhysicalControl("BUTTON_A", "A", "button-a")}
          {renderPhysicalControl("BUTTON_B", "B", "button-b")}
        </div>
      </div>
    </div>
  );
}

type SceneEndpointNodeData = {
  endpoint: GraphSceneEndpointNode;
  canEdit: boolean;
  onSelect: (endpoint: GraphSceneEndpointNode) => void;
};

function SceneEndpointNode({ data, selected }: NodeProps<Node<SceneEndpointNodeData>>) {
  const { endpoint, canEdit, onSelect } = data;
  if (endpoint.kind === "system") {
    return (
      <div
        className={`state-scene-endpoint system ${selected ? "selected" : ""}`}
        role="button"
        tabIndex={0}
        onClick={() => onSelect(endpoint)}
        onKeyDown={(event) => {
          if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            onSelect(endpoint);
          }
        }}
      >
        <Handle id="system-exit-in" type="target" position={Position.Left} isConnectable={canEdit} />
        <span>System action</span>
        <strong>{endpoint.label}</strong>
        <small>{endpoint.detail}</small>
      </div>
    );
  }
  const isEntry = endpoint.kind === "entry";
  return (
    <div
      className={`state-scene-endpoint boundary ${endpoint.kind} ${selected ? "selected" : ""}`}
      role="button"
      tabIndex={0}
      aria-label={`${isEntry ? "Scene entry" : "Scene exit"}: ${endpoint.label}. ${endpoint.detail}`}
      title={`${isEntry ? "Scene entry" : "Scene exit"}: ${endpoint.detail}`}
      onClick={() => onSelect(endpoint)}
      onKeyDown={(event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          onSelect(endpoint);
        }
      }}
    >
      <span className="scene-boundary-direction" aria-hidden="true">
        <Triangle size={36} strokeWidth={1.5} fill="currentColor" />
      </span>
      {isEntry ? (
        <Handle
          className="scene-boundary-local-handle"
          id="scene-entry-out"
          type="source"
          position={Position.Right}
          isConnectable={canEdit}
        />
      ) : (
        <Handle
          className="scene-boundary-local-handle"
          id="scene-exit-in"
          type="target"
          position={Position.Left}
          isConnectable={canEdit}
        />
      )}
      <strong>{endpoint.label}</strong>
    </div>
  );
}

const STATE_NODE_TYPES = { stateCard: StateCardNode, sceneEndpoint: SceneEndpointNode };

type StateTransitionEdgeData = {
  tone?: "blue" | "green";
  route_id?: string;
  source_state?: string;
  laneX?: number;
  guards?: StateGuard[];
  actions?: StateAction[];
  rails?: EditorRouteRail[];
  tokenPositions?: EditorRouteTokenPositions;
  targetHandle?: StateGraphEntryHandle;
  targetSide?: StateGraphEntrySide;
  targetEntryPorts?: Array<{
    stateId?: string;
    handle: StateGraphEntryHandle;
    side: StateGraphEntrySide;
    point: EditorNodePosition;
  }>;
  canEdit?: boolean;
  showSectionHandles?: boolean;
  onSetEntryTarget?: (stateId: string, handle: StateGraphEntryHandle, side: StateGraphEntrySide) => void;
  onSelectRoute?: (routeId: string, sourceState: string) => void;
  onSetRouteLayout?: (
    routeId: string,
    sourceState: string,
    rails: EditorRouteRail[],
    targetHandle: StateGraphEntryHandle | null,
    targetSide: StateGraphEntrySide | null,
    tokenPositions: EditorRouteTokenPositions,
  ) => void;
};

function edgeSourceSide(position: Position): StateGraphExitSide {
  if (position === Position.Left) {
    return "left";
  }
  if (position === Position.Top) {
    return "top";
  }
  if (position === Position.Bottom) {
    return "bottom";
  }
  return "right";
}

function edgeTargetSide(position: Position): StateGraphEntrySide {
  if (position === Position.Left) {
    return "left";
  }
  if (position === Position.Right) {
    return "right";
  }
  return position === Position.Bottom ? "bottom" : "top";
}

function routeLabelPoint(points: Array<{ x: number; y: number }>, fraction = 0.5): { x: number; y: number } {
  if (points.length === 0) {
    return { x: 0, y: 0 };
  }
  if (points.length === 1) {
    return points[0];
  }
  const lengths: number[] = [];
  let totalLength = 0;
  for (let index = 1; index < points.length; index += 1) {
    const previous = points[index - 1];
    const current = points[index];
    const length = Math.hypot(current.x - previous.x, current.y - previous.y);
    lengths.push(length);
    totalLength += length;
  }
  let remaining = totalLength * fraction;
  for (let index = 1; index < points.length; index += 1) {
    const length = lengths[index - 1] ?? 0;
    const previous = points[index - 1];
    const current = points[index];
    if (remaining <= length || index === points.length - 1) {
      const ratio = length === 0 ? 0 : remaining / length;
      return {
        x: previous.x + (current.x - previous.x) * ratio,
        y: previous.y + (current.y - previous.y) * ratio,
      };
    }
    remaining -= length;
  }
  return points[points.length - 1];
}

function closestRouteFraction(
  points: EditorNodePosition[],
  point: EditorNodePosition,
): number {
  const lengths: number[] = [];
  let totalLength = 0;
  for (let index = 1; index < points.length; index += 1) {
    const previous = points[index - 1];
    const current = points[index];
    const length = Math.hypot(current.x - previous.x, current.y - previous.y);
    lengths.push(length);
    totalLength += length;
  }
  if (totalLength === 0) {
    return 0.5;
  }

  let closestDistance = Number.POSITIVE_INFINITY;
  let closestLength = totalLength * 0.5;
  let traversed = 0;
  for (let index = 1; index < points.length; index += 1) {
    const start = points[index - 1];
    const end = points[index];
    const dx = end.x - start.x;
    const dy = end.y - start.y;
    const length = lengths[index - 1] ?? 0;
    const lengthSquared = dx * dx + dy * dy;
    const segmentFraction = lengthSquared === 0
      ? 0
      : Math.max(0, Math.min(1, ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared));
    const projectedX = start.x + dx * segmentFraction;
    const projectedY = start.y + dy * segmentFraction;
    const distance = Math.hypot(point.x - projectedX, point.y - projectedY);
    if (distance < closestDistance) {
      closestDistance = distance;
      closestLength = traversed + length * segmentFraction;
    }
    traversed += length;
  }
  return closestLength / totalLength;
}

function defaultRouteTokenFractions(count: number): number[] {
  if (count <= 0) {
    return [];
  }
  if (count === 1) {
    return [0.5];
  }
  const spacing = Math.min(0.12, 0.72 / (count - 1));
  const start = 0.5 - spacing * (count - 1) / 2;
  return Array.from({ length: count }, (_, index) => start + index * spacing);
}

function resolvedRouteTokenFractions(
  hasCondition: boolean,
  actionCount: number,
  positions: EditorRouteTokenPositions | undefined,
): number[] {
  const count = actionCount + (hasCondition ? 1 : 0);
  const defaults = defaultRouteTokenFractions(count);
  const actions = positions?.actions ?? [];
  if (
    actions.length !== actionCount ||
    (hasCondition && typeof positions?.condition !== "number")
  ) {
    return defaults;
  }
  const fractions = [
    ...(hasCondition ? [positions?.condition ?? 0.5] : []),
    ...actions,
  ];
  const valid = fractions.every((fraction, index) => (
    Number.isFinite(fraction) &&
    fraction >= 0.02 &&
    fraction <= 0.98 &&
    (index === 0 || fraction > fractions[index - 1])
  ));
  return valid ? fractions : defaults;
}

function clampRouteTokenFraction(fractions: number[], index: number, fraction: number): number {
  const gap = Math.min(0.08, 0.72 / Math.max(1, fractions.length - 1));
  const minimum = index === 0 ? 0.02 : fractions[index - 1] + gap;
  const maximum = index === fractions.length - 1 ? 0.98 : fractions[index + 1] - gap;
  return Math.max(minimum, Math.min(maximum, fraction));
}

function routeTokenPositions(
  hasCondition: boolean,
  fractions: number[],
): EditorRouteTokenPositions {
  const actionOffset = hasCondition ? 1 : 0;
  return {
    ...(hasCondition ? { condition: Math.round((fractions[0] ?? 0.5) * 10000) / 10000 } : {}),
    actions: fractions.slice(actionOffset).map((fraction) => Math.round(fraction * 10000) / 10000),
  };
}

function closestRouteSegment(points: EditorNodePosition[], point: EditorNodePosition): number {
  let closestIndex = 0;
  let closestDistance = Number.POSITIVE_INFINITY;
  for (let index = 0; index < points.length - 1; index += 1) {
    const start = points[index];
    const end = points[index + 1];
    const dx = end.x - start.x;
    const dy = end.y - start.y;
    const lengthSquared = dx * dx + dy * dy;
    const fraction = lengthSquared === 0
      ? 0
      : Math.max(0, Math.min(1, ((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared));
    const projectedX = start.x + dx * fraction;
    const projectedY = start.y + dy * fraction;
    const distance = Math.hypot(point.x - projectedX, point.y - projectedY);
    if (distance < closestDistance) {
      closestDistance = distance;
      closestIndex = index;
    }
  }
  return closestIndex;
}

function TransitionActionIcon({ action }: { action: StateAction }) {
  if (action.kind === "set_variable") {
    return <span className="state-transition-xy-icon">Xy</span>;
  }
  if (action.kind === "play_sfx") {
    return <Volume2 size={15} aria-hidden="true" />;
  }
  if (action.kind === "set_element_visibility" || action.kind === "object.set_visibility") {
    return <Eye size={15} aria-hidden="true" />;
  }
  if (action.kind === "set_element_position" || action.kind === "object.set_position" || action.kind === "object.move_by") {
    return <Move size={15} aria-hidden="true" />;
  }
  if (action.kind === "set_element_frame" || action.kind === "object.set_frame" || action.kind === "object.clear_frame") {
    return <Image size={15} aria-hidden="true" />;
  }
  if (action.kind === "set_element_waiting_animation") {
    return <Play size={15} aria-hidden="true" />;
  }
  if (action.kind === "transition_scene") {
    return <ExternalLink size={15} aria-hidden="true" />;
  }
  if (action.kind === "exit_to_shell") {
    return <LogOut size={15} aria-hidden="true" />;
  }
  return <Route size={15} aria-hidden="true" />;
}

function graphEntryArrow(
  edges: Edge[],
  node: InternalNode | undefined,
  edge: Pick<Edge, "id" | "target" | "targetHandle">,
  fallback: EditorNodePosition,
  side: StateGraphEntrySide,
) {
  const handle = node?.internals.handleBounds?.target?.find((candidate) => candidate.id === edge.targetHandle);
  let socket: EntrySocket = { center: fallback, width: 0, height: 0 };
  if (node !== undefined && handle !== undefined) {
    const center = {
      x: node.internals.positionAbsolute.x + handle.x + handle.width / 2,
      y: node.internals.positionAbsolute.y + handle.y + handle.height / 2,
    };
    socket = { center, width: handle.width, height: handle.height };
    if (node.type === "stateCard") {
      const top = edge.targetHandle?.startsWith("entry-top") === true;
      const left = edge.targetHandle?.includes("-left:") === true;
      socket = {
        center: {
          x: center.x + (side === "left" || side === "right" ? left ? 8 : -8 : 0),
          y: center.y + (side === "top" || side === "bottom" ? top ? 9 : -9 : 0),
        },
        width: 20,
        height: 29,
        rotation: (top === left ? 30 : -30) * Math.PI / 180,
      };
    }
  }
  const otherTips = node?.type !== "stateCard" || handle === undefined ? [] : edges
    .filter((peer) => peer.id !== edge.id && peer.target === edge.target
      && peer.targetHandle !== edge.targetHandle
      && peer.targetHandle?.split(":")[0] === edge.targetHandle?.split(":")[0])
    .flatMap((peer) => {
      const port = STATE_GRAPH_ENTRY_PORTS.find((candidate) => stateEntryPortId(candidate.handle, candidate.side) === peer.targetHandle);
      return port === undefined ? [] : [incomingArrow(socket, port.side, incomingPeers(edges, peer), peer.id).tip];
    });
  return incomingArrow(socket, side, handle === undefined ? [edge.id] : incomingPeers(edges, edge), edge.id, otherTips);
}

type StateTransitionVisualSegment = {
  routeId: string;
  orientation: "horizontal" | "vertical";
  fixed: number;
  min: number;
  max: number;
};

type StateTransitionVisualCrossing = EditorNodePosition & {
  orientation: "horizontal" | "vertical";
};

function visualSegmentsForTransition(
  routeId: string,
  points: EditorNodePosition[],
): StateTransitionVisualSegment[] {
  const segments: StateTransitionVisualSegment[] = [];
  for (let index = 1; index < points.length; index += 1) {
    const start = points[index - 1];
    const end = points[index];
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

function visualTransitionSegmentsCross(
  left: StateTransitionVisualSegment,
  right: StateTransitionVisualSegment,
  margin = 0,
): EditorNodePosition | null {
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

function StateTransitionEdge({
  data,
  id,
  selected,
  source,
  sourcePosition,
  sourceX,
  sourceY,
  style,
  target,
  targetHandleId,
  targetPosition,
  targetX,
  targetY,
}: EdgeProps) {
  const edgeData = data as StateTransitionEdgeData | undefined;
  const { screenToFlowPosition } = useReactFlow();
  const flowEdges = useStore((state) => state.edges);
  const nodeLookup = useStore((state) => state.nodeLookup);
  const [draftTargetState, setDraftTargetState] = useState(target);
  const persistedRails = Array.isArray(edgeData?.rails) ? edgeData.rails : [];
  const persistedRailKey = persistedRails.map((rail) => `${rail.axis}:${rail.value}`).join("|");
  const persistedTargetHandle = edgeData?.targetHandle;
  const persistedTargetSide = edgeData?.targetSide;
  const [draftRails, setDraftRails] = useState<EditorRouteRail[]>(persistedRails);
  const draftRailsRef = useRef<EditorRouteRail[]>(persistedRails);
  const [draftTargetHandle, setDraftTargetHandle] = useState<StateGraphEntryHandle | undefined>(persistedTargetHandle);
  const draftTargetHandleRef = useRef<StateGraphEntryHandle | undefined>(persistedTargetHandle);
  const [draftTargetSide, setDraftTargetSide] = useState<StateGraphEntrySide | undefined>(persistedTargetSide);
  const draftTargetSideRef = useRef<StateGraphEntrySide | undefined>(persistedTargetSide);
  const [selectedSection, setSelectedSection] = useState<number | null>(null);
  const [arrowHovered, setArrowHovered] = useState(false);
  const dragCleanupRef = useRef<(() => void) | null>(null);
  useEffect(() => {
    const next = persistedRails.map((rail) => ({ ...rail }));
    draftRailsRef.current = next;
    setDraftRails(next);
    draftTargetHandleRef.current = persistedTargetHandle;
    setDraftTargetHandle(persistedTargetHandle);
    draftTargetSideRef.current = persistedTargetSide;
    setDraftTargetSide(persistedTargetSide);
    setSelectedSection(null);
    setDraftTargetState(target);
  }, [persistedRailKey, persistedTargetHandle, persistedTargetSide, target]);
  useEffect(() => () => dragCleanupRef.current?.(), []);

  const routeId = typeof edgeData?.route_id === "string" ? edgeData.route_id : id;
  const sourceState = typeof edgeData?.source_state === "string" ? edgeData.source_state : "";
  const canEdit = edgeData?.canEdit === true;
  const showSectionHandles = edgeData?.showSectionHandles === true;
  const effectiveTargetHandle = draftTargetHandle ?? persistedTargetHandle;
  const effectiveTargetSide = draftTargetSide ?? persistedTargetSide ?? edgeTargetSide(targetPosition);
  const targetPort = edgeData?.targetEntryPorts?.find((port) => (
    port.handle === effectiveTargetHandle && port.side === effectiveTargetSide
    && (port.stateId === undefined || port.stateId === draftTargetState)
  ));
  const usesReactFlowTarget = effectiveTargetHandle === persistedTargetHandle
    && effectiveTargetSide === persistedTargetSide && draftTargetState === target;
  const targetPoint = usesReactFlowTarget
    ? { x: targetX, y: targetY }
    : targetPort?.point ?? { x: targetX, y: targetY };
  const arrow = graphEntryArrow(flowEdges, nodeLookup.get(draftTargetState), {
    id, target: draftTargetState,
    targetHandle: effectiveTargetHandle === undefined ? targetHandleId : stateEntryPortId(effectiveTargetHandle, effectiveTargetSide),
  }, targetPoint, effectiveTargetSide);
  const arrowTip = arrow.tip;
  const green = edgeData?.tone === "green";
  const laneX = usesReactFlowTarget && typeof edgeData?.laneX === "number"
    ? edgeData.laneX
    : undefined;
  const automaticRoute = buildStateTransitionRoute({
    sourceX,
    sourceY,
    targetX: arrow.routeTarget.x,
    targetY: arrow.routeTarget.y,
    sourceSide: edgeSourceSide(sourcePosition),
    targetSide: effectiveTargetSide,
    laneX,
  });
  const route = draftRails.length === 0
    ? automaticRoute
    : buildStateTransitionRoute({
        sourceX,
        sourceY,
        targetX: arrow.routeTarget.x,
        targetY: arrow.routeTarget.y,
        sourceSide: edgeSourceSide(sourcePosition),
        targetSide: effectiveTargetSide,
        rails: draftRails,
      });
  const routeSections = stateTransitionRouteSections(route.controlPoints);
  const gradientId = `state-transition-gradient-${id.replace(/[^a-zA-Z0-9_-]/g, "_")}`;
  const edgeStyle = {
    ...style,
    stroke: `url(#${gradientId})`,
    strokeDasharray: "10 8",
    strokeLinecap: "round" as const,
    strokeLinejoin: "round" as const,
  };
  const visualCrossings = useMemo(() => {
    const edgeOrder = new Map(flowEdges.map((edge, index) => [edge.id, index]));
    const currentOrder = edgeOrder.get(id) ?? 0;
    const currentSegments = visualSegmentsForTransition(id, route.points);
    const crossings: StateTransitionVisualCrossing[] = [];
    const seen = new Set<string>();

    flowEdges.forEach((flowEdge) => {
      if (flowEdge.id === id || flowEdge.type !== "stateTransition") {
        return;
      }
      const peerOrder = edgeOrder.get(flowEdge.id) ?? 0;
      if (peerOrder >= currentOrder) {
        return;
      }
      if (flowEdge.source === source || flowEdge.target === target) {
        return;
      }

      const sourceNode = nodeLookup.get(flowEdge.source);
      const targetNode = nodeLookup.get(flowEdge.target);
      const sourceHandle = sourceNode?.internals.handleBounds?.source?.find(
        (handle) => handle.id === flowEdge.sourceHandle,
      );
      const targetHandle = targetNode?.internals.handleBounds?.target?.find(
        (handle) => handle.id === flowEdge.targetHandle,
      );
      if (sourceNode === undefined || targetNode === undefined || sourceHandle === undefined || targetHandle === undefined) {
        return;
      }

      const candidateData = flowEdge.data as StateTransitionEdgeData | undefined;
      const peerSource = handleBoundary(sourceNode.internals.positionAbsolute, sourceHandle);
      const peerTargetSide = candidateData?.targetSide ?? edgeTargetSide(targetHandle.position);
      const peerTarget = graphEntryArrow(
        flowEdges,
        targetNode,
        flowEdge,
        handleBoundary(targetNode.internals.positionAbsolute, targetHandle),
        peerTargetSide,
      ).routeTarget;
      const peerRoute = buildStateTransitionRoute({
        sourceX: peerSource.x,
        sourceY: peerSource.y,
        targetX: peerTarget.x,
        targetY: peerTarget.y,
        sourceSide: edgeSourceSide(sourceHandle.position),
        targetSide: peerTargetSide,
        laneX: typeof candidateData?.laneX === "number" ? candidateData.laneX : undefined,
        rails: Array.isArray(candidateData?.rails) ? candidateData.rails : undefined,
      });
      const peerSegments = visualSegmentsForTransition(flowEdge.id, peerRoute.points);
      currentSegments.forEach((segment) => {
        peerSegments.forEach((peerSegment) => {
          const crossing = visualTransitionSegmentsCross(segment, peerSegment, 14);
          if (crossing === null) {
            return;
          }
          const key = `${Math.round(crossing.x)}:${Math.round(crossing.y)}`;
          if (seen.has(key)) {
            return;
          }
          seen.add(key);
          crossings.push({ ...crossing, orientation: segment.orientation });
        });
      });
    });

    return crossings;
  }, [flowEdges, id, nodeLookup, route.points, source, target]);
  const guards = Array.isArray(edgeData?.guards) ? edgeData.guards : [];
  const actions = Array.isArray(edgeData?.actions) ? edgeData.actions : [];
  const tokens = [
    ...(guards.length === 0 ? [] : [{
      key: "condition",
      kind: "condition" as const,
      description: `Only if ${guards.map(stateGuardDescription).join(" and ")}`,
      count: guards.length,
    }]),
    ...actions.map((action, index) => ({
      key: `action-${index}`,
      kind: "action" as const,
      description: stateActionDescription(action) ?? "Background screen update",
      action,
    })),
  ];
  const hasConditionToken = guards.length > 0;
  const persistedTokenPositions = edgeData?.tokenPositions;
  const persistedTokenKey = JSON.stringify(persistedTokenPositions ?? {});
  const tokenKey = tokens.map((token) => token.key).join("|");
  const initialTokenFractions = resolvedRouteTokenFractions(
    hasConditionToken,
    actions.length,
    persistedTokenPositions,
  );
  const [draftTokenFractions, setDraftTokenFractions] = useState(initialTokenFractions);
  const draftTokenFractionsRef = useRef(initialTokenFractions);
  useEffect(() => {
    const next = resolvedRouteTokenFractions(
      hasConditionToken,
      actions.length,
      persistedTokenPositions,
    );
    draftTokenFractionsRef.current = next;
    setDraftTokenFractions(next);
  }, [actions.length, hasConditionToken, persistedTokenKey, tokenKey]);

  const commitLayout = (
    rails: EditorRouteRail[],
    targetHandle: StateGraphEntryHandle | null,
    targetSide: StateGraphEntrySide | null,
    tokenPositions = routeTokenPositions(hasConditionToken, draftTokenFractionsRef.current),
  ) => {
    const rounded = rails.map((rail) => ({ axis: rail.axis, value: Math.round(rail.value) }));
    draftRailsRef.current = rounded;
    setDraftRails(rounded);
    draftTargetHandleRef.current = targetHandle ?? undefined;
    setDraftTargetHandle(targetHandle ?? undefined);
    draftTargetSideRef.current = targetSide ?? undefined;
    setDraftTargetSide(targetSide ?? undefined);
    edgeData?.onSetRouteLayout?.(routeId, sourceState, rounded, targetHandle, targetSide, tokenPositions);
  };

  useEffect(() => {
    if (!showSectionHandles || selectedSection === null || !canEdit) {
      return;
    }
    const onKeyDown = (event: globalThis.KeyboardEvent) => {
      if (event.key !== "Delete" && event.key !== "Backspace") {
        return;
      }
      const target = event.target as HTMLElement | null;
      if (target !== null && target.closest("input, select, textarea, [contenteditable='true']") !== null) {
        return;
      }
      event.preventDefault();
      event.stopPropagation();
      const next = removeStateTransitionRouteSection(route.controlPoints, selectedSection, effectiveTargetSide);
      if (next === null) {
        return;
      }
      setSelectedSection(null);
      commitLayout(next, effectiveTargetHandle ?? null, effectiveTargetSide);
    };
    window.addEventListener("keydown", onKeyDown, true);
    return () => window.removeEventListener("keydown", onKeyDown, true);
  }, [canEdit, effectiveTargetHandle, effectiveTargetSide, route, routeId, selectedSection, showSectionHandles, sourceState]);

  const addRouteSection = (clientX: number, clientY: number) => {
    if (!canEdit || edgeData?.onSetRouteLayout === undefined) {
      return;
    }
    const point = screenToFlowPosition({ x: clientX, y: clientY });
    const segmentIndex = closestRouteSegment(route.controlPoints, point);
    const inserted = insertStateTransitionRouteSection(route.controlPoints, segmentIndex, point, effectiveTargetSide);
    if (inserted === null) {
      return;
    }
    setSelectedSection(null);
    commitLayout(inserted, effectiveTargetHandle ?? null, effectiveTargetSide);
  };

  const closestEntryPort = (point: EditorNodePosition): {
    stateId?: string;
    handle: StateGraphEntryHandle;
    side: StateGraphEntrySide;
    distance: number;
  } | null => {
    let closest: { stateId?: string; handle: StateGraphEntryHandle; side: StateGraphEntrySide; distance: number } | null = null;
    for (const port of edgeData?.targetEntryPorts ?? []) {
      const distance = Math.hypot(point.x - port.point.x, point.y - port.point.y);
      if (closest === null || distance < closest.distance) {
        closest = { stateId: port.stateId, handle: port.handle, side: port.side, distance };
      }
    }
    return closest;
  };
  const arrowCanMove = canEdit && (edgeData?.targetEntryPorts?.length ?? 0) > 0;
  const beginArrowDrag = (event: ReactPointerEvent<HTMLButtonElement>) => {
    event.stopPropagation();
    edgeData?.onSelectRoute?.(routeId, sourceState);
    if (!arrowCanMove) {
      return;
    }
    event.preventDefault();
    dragCleanupRef.current?.();
    const pointerId = event.pointerId;
    const originalHandle = effectiveTargetHandle;
    const originalSide = effectiveTargetSide;
    const originalState = draftTargetState;
    const onPointerMove = (pointerEvent: globalThis.PointerEvent) => {
      if (pointerEvent.pointerId !== pointerId) {
        return;
      }
      pointerEvent.preventDefault();
      const point = screenToFlowPosition({ x: pointerEvent.clientX, y: pointerEvent.clientY });
      const closest = closestEntryPort(point);
      if (closest !== null && closest.distance <= 72) {
        draftTargetHandleRef.current = closest.handle;
        setDraftTargetHandle(closest.handle);
        draftTargetSideRef.current = closest.side;
        setDraftTargetSide(closest.side);
        setDraftTargetState(closest.stateId ?? target);
      }
    };
    const finishDrag = (pointerEvent: globalThis.PointerEvent) => {
      if (pointerEvent.pointerId !== pointerId) {
        return;
      }
      const point = screenToFlowPosition({ x: pointerEvent.clientX, y: pointerEvent.clientY });
      const closest = closestEntryPort(point);
      dragCleanupRef.current?.();
      dragCleanupRef.current = null;
      if (closest !== null && closest.distance <= 72) {
        if (edgeData?.onSetEntryTarget !== undefined) {
          edgeData.onSetEntryTarget(closest.stateId ?? target, closest.handle, closest.side);
        } else {
          commitLayout(draftRailsRef.current, closest.handle, closest.side);
        }
      } else {
        setDraftTargetState(originalState);
        draftTargetHandleRef.current = originalHandle;
        setDraftTargetHandle(originalHandle);
        draftTargetSideRef.current = originalSide;
        setDraftTargetSide(originalSide);
      }
    };
    const cancelDrag = (pointerEvent: globalThis.PointerEvent) => {
      if (pointerEvent.pointerId !== pointerId) {
        return;
      }
      dragCleanupRef.current?.();
      dragCleanupRef.current = null;
      setDraftTargetState(originalState);
      draftTargetHandleRef.current = originalHandle;
      setDraftTargetHandle(originalHandle);
      draftTargetSideRef.current = originalSide;
      setDraftTargetSide(originalSide);
    };
    window.addEventListener("pointermove", onPointerMove, true);
    window.addEventListener("pointerup", finishDrag, true);
    window.addEventListener("pointercancel", cancelDrag, true);
    dragCleanupRef.current = () => {
      window.removeEventListener("pointermove", onPointerMove, true);
      window.removeEventListener("pointerup", finishDrag, true);
      window.removeEventListener("pointercancel", cancelDrag, true);
    };
  };

  return (
    <>
      <defs>
        <linearGradient id={gradientId} gradientUnits="userSpaceOnUse" x1={sourceX} y1={sourceY} x2={arrowTip.x} y2={arrowTip.y}>
          <stop offset="0%" stopColor={green ? "#8ce99a" : selected ? "#4dabf7" : "#74c0fc"} />
          <stop offset="100%" stopColor={green ? "#2f9e44" : selected ? "#1864ab" : "#1971c2"} />
        </linearGradient>
      </defs>
      <BaseEdge id={id} path={route.path + arrow.leadPath} style={edgeStyle} />
      <path
        className="state-transition-hit-path"
        d={route.path + arrow.leadPath}
        onDoubleClick={(event) => {
          event.stopPropagation();
          edgeData?.onSelectRoute?.(routeId, sourceState);
          addRouteSection(event.clientX, event.clientY);
        }}
      />
      {visualCrossings.map((crossing, index) => {
        const horizontal = crossing.orientation === "horizontal";
        const maskPath = horizontal
          ? `M ${crossing.x - 10} ${crossing.y} L ${crossing.x + 10} ${crossing.y}`
          : `M ${crossing.x} ${crossing.y - 10} L ${crossing.x} ${crossing.y + 10}`;
        const bridgePath = horizontal
          ? `M ${crossing.x - 10} ${crossing.y} Q ${crossing.x} ${crossing.y - 11} ${crossing.x + 10} ${crossing.y}`
          : `M ${crossing.x} ${crossing.y - 10} Q ${crossing.x + 11} ${crossing.y} ${crossing.x} ${crossing.y + 10}`;
        return (
          <g key={`${id}:local-crossing-${index}`} className="state-transition-bridge" pointerEvents="none">
            <path d={maskPath} stroke="#eef2f0" strokeWidth={10} fill="none" />
            <path d={bridgePath} stroke={`url(#${gradientId})`} strokeWidth={selected ? 4.8 : 4} strokeLinecap="round" fill="none" />
          </g>
        );
      })}
      <path
        className={`state-transition-arrow ${selected ? "selected" : ""} ${arrowHovered ? "hovered" : ""}`}
        d={arrow.arrowPath}
        style={green ? { fill: "#2f9e44" } : undefined}
      />
      <EdgeLabelRenderer>
        <button
          className={`state-transition-arrow-hit nodrag nopan ${arrowCanMove ? "editable" : ""}`}
          data-edge-id={id}
          type="button"
          aria-label={arrowCanMove ? "Move transition destination" : "Select transition"}
          style={arrow.hitStyle}
          onClick={(event) => {
            event.stopPropagation();
            edgeData?.onSelectRoute?.(routeId, sourceState);
          }}
          onPointerEnter={() => setArrowHovered(true)}
          onPointerLeave={() => setArrowHovered(false)}
          onPointerDown={beginArrowDrag}
        />
      </EdgeLabelRenderer>
      {tokens.map((token, index) => {
        const fraction = draftTokenFractions[index] ?? initialTokenFractions[index] ?? 0.5;
        const labelPoint = routeLabelPoint(route.points, fraction);
        return (
          <EdgeLabelRenderer key={`${routeId}:${sourceState}:${token.key}`}>
            <div
              className="state-transition-token-anchor"
              style={{
                transform: `translate(-50%, -50%) translate(${labelPoint.x}px, ${labelPoint.y}px)`,
              }}
            >
              <button
                className={`state-transition-token state-transition-${token.kind} nodrag nopan ${selected ? "selected" : ""} ${canEdit ? "editable" : ""}`}
                type="button"
                aria-label={token.description}
                onClick={(event) => {
                  event.stopPropagation();
                  edgeData?.onSelectRoute?.(routeId, sourceState);
                }}
                onPointerDown={(event) => {
                  event.stopPropagation();
                  edgeData?.onSelectRoute?.(routeId, sourceState);
                  if (!canEdit) {
                    return;
                  }
                  event.preventDefault();
                  dragCleanupRef.current?.();
                  setSelectedSection(null);
                  const pointerId = event.pointerId;
                  const startX = event.clientX;
                  const startY = event.clientY;
                  const original = [...draftTokenFractionsRef.current];
                  let moved = false;
                  const fractionAtPointer = (pointerEvent: globalThis.PointerEvent) => {
                    const point = screenToFlowPosition({ x: pointerEvent.clientX, y: pointerEvent.clientY });
                    return clampRouteTokenFraction(
                      original,
                      index,
                      closestRouteFraction(route.points, point),
                    );
                  };
                  const onPointerMove = (pointerEvent: globalThis.PointerEvent) => {
                    if (pointerEvent.pointerId !== pointerId) {
                      return;
                    }
                    pointerEvent.preventDefault();
                    if (!moved && Math.hypot(pointerEvent.clientX - startX, pointerEvent.clientY - startY) < 3) {
                      return;
                    }
                    moved = true;
                    const next = [...original];
                    next[index] = fractionAtPointer(pointerEvent);
                    draftTokenFractionsRef.current = next;
                    setDraftTokenFractions(next);
                  };
                  const finishDrag = (pointerEvent: globalThis.PointerEvent) => {
                    if (pointerEvent.pointerId !== pointerId) {
                      return;
                    }
                    dragCleanupRef.current?.();
                    dragCleanupRef.current = null;
                    if (!moved) {
                      return;
                    }
                    const next = [...original];
                    next[index] = fractionAtPointer(pointerEvent);
                    draftTokenFractionsRef.current = next;
                    setDraftTokenFractions(next);
                    commitLayout(
                      draftRailsRef.current,
                      effectiveTargetHandle ?? null,
                      effectiveTargetSide,
                      routeTokenPositions(hasConditionToken, next),
                    );
                  };
                  const cancelDrag = (pointerEvent: globalThis.PointerEvent) => {
                    if (pointerEvent.pointerId !== pointerId) {
                      return;
                    }
                    dragCleanupRef.current?.();
                    dragCleanupRef.current = null;
                    draftTokenFractionsRef.current = original;
                    setDraftTokenFractions(original);
                  };
                  window.addEventListener("pointermove", onPointerMove, true);
                  window.addEventListener("pointerup", finishDrag, true);
                  window.addEventListener("pointercancel", cancelDrag, true);
                  dragCleanupRef.current = () => {
                    window.removeEventListener("pointermove", onPointerMove, true);
                    window.removeEventListener("pointerup", finishDrag, true);
                    window.removeEventListener("pointercancel", cancelDrag, true);
                  };
                }}
              >
                <span className="state-transition-token-symbol">
                  {token.kind === "condition" ? <Filter size={15} aria-hidden="true" /> : <TransitionActionIcon action={token.action} />}
                </span>
                {token.kind === "condition" && (
                  <span className="state-transition-token-count">{token.count}</span>
                )}
                <span className="state-transition-token-tooltip" role="tooltip">{token.description}</span>
              </button>
            </div>
          </EdgeLabelRenderer>
        );
      })}
      {showSectionHandles && routeSections.map((section) => {
        const sectionIsSelected = selectedSection === section.controlSegmentIndex;
        return (
        <EdgeLabelRenderer key={`${routeId}:${sourceState}:section-${section.controlSegmentIndex}`}>
          <button
            className={`state-route-section state-route-section-${section.orientation} nodrag nopan ${sectionIsSelected ? "selected" : ""}`}
            type="button"
            aria-label={`Move ${section.orientation} transition section`}
            title={section.orientation === "horizontal" ? "Drag up or down" : "Drag left or right"}
            style={{
              width: section.orientation === "horizontal" ? `${Math.max(22, section.length)}px` : "16px",
              height: section.orientation === "vertical" ? `${Math.max(22, section.length)}px` : "16px",
              transform: `translate(-50%, -50%) translate(${section.center.x}px, ${section.center.y}px)`,
            }}
            disabled={!canEdit}
            onClick={(event) => {
              event.stopPropagation();
            }}
            onPointerDown={(event) => {
              if (!canEdit) {
                return;
              }
              event.preventDefault();
              event.stopPropagation();
              dragCleanupRef.current?.();
              const basePoints = route.controlPoints.map((point) => ({ ...point }));
              setSelectedSection(section.controlSegmentIndex);
              const pointerId = event.pointerId;
              const onPointerMove = (pointerEvent: globalThis.PointerEvent) => {
                if (pointerEvent.pointerId !== pointerId) {
                  return;
                }
                pointerEvent.preventDefault();
                const point = screenToFlowPosition({ x: pointerEvent.clientX, y: pointerEvent.clientY });
                const next = moveStateTransitionRouteSection(
                  basePoints,
                  section.controlSegmentIndex,
                  point,
                  edgeSourceSide(sourcePosition),
                  effectiveTargetSide,
                );
                if (next !== null) {
                  draftRailsRef.current = next;
                  setDraftRails(next);
                }
              };
              const finishDrag = (pointerEvent: globalThis.PointerEvent) => {
                if (pointerEvent.pointerId !== pointerId) {
                  return;
                }
                dragCleanupRef.current?.();
                dragCleanupRef.current = null;
                commitLayout(draftRailsRef.current, effectiveTargetHandle ?? null, effectiveTargetSide);
              };
              const cancelDrag = (pointerEvent: globalThis.PointerEvent) => {
                if (pointerEvent.pointerId !== pointerId) {
                  return;
                }
                dragCleanupRef.current?.();
                dragCleanupRef.current = null;
                const restored = persistedRails.map((rail) => ({ ...rail }));
                draftRailsRef.current = restored;
                setDraftRails(restored);
              };
              window.addEventListener("pointermove", onPointerMove, true);
              window.addEventListener("pointerup", finishDrag, true);
              window.addEventListener("pointercancel", cancelDrag, true);
              dragCleanupRef.current = () => {
                window.removeEventListener("pointermove", onPointerMove, true);
                window.removeEventListener("pointerup", finishDrag, true);
                window.removeEventListener("pointercancel", cancelDrag, true);
              };
            }}
          />
        </EdgeLabelRenderer>
        );
      })}
    </>
  );
}

type SceneFlowTransitionEdgeData = {
  tone?: "blue" | "green";
  sourceSceneId: string;
  endpointKind: "scene_exit" | "route";
  endpointId: string;
  rails?: EditorRouteRail[];
  canEdit?: boolean;
  onSelect: () => void;
  onSetRouteLayout: (rails: EditorRouteRail[]) => void;
};

function SceneFlowTransitionEdge({
  data,
  id,
  selected,
  sourcePosition,
  sourceX,
  sourceY,
  style,
  target,
  targetHandleId,
  targetPosition,
  targetX,
  targetY,
}: EdgeProps) {
  const edgeData = data as SceneFlowTransitionEdgeData | undefined;
  const { screenToFlowPosition } = useReactFlow();
  const flowEdges = useStore((state) => state.edges);
  const flowNodes = useStore((state) => state.nodes);
  const nodeLookup = useStore((state) => state.nodeLookup);
  const persistedRails = Array.isArray(edgeData?.rails) ? edgeData.rails : [];
  const persistedRailKey = persistedRails.map((rail) => `${rail.axis}:${rail.value}`).join("|");
  const [draftRails, setDraftRails] = useState<EditorRouteRail[]>(persistedRails);
  const draftRailsRef = useRef<EditorRouteRail[]>(persistedRails);
  const [selectedSection, setSelectedSection] = useState<number | null>(null);
  const [arrowHovered, setArrowHovered] = useState(false);
  const dragCleanupRef = useRef<(() => void) | null>(null);
  useEffect(() => {
    const next = persistedRails.map((rail) => ({ ...rail }));
    draftRailsRef.current = next;
    setDraftRails(next);
    setSelectedSection(null);
  }, [persistedRailKey]);
  useEffect(() => () => dragCleanupRef.current?.(), []);

  const targetSide = edgeTargetSide(targetPosition);
  const arrow = graphEntryArrow(flowEdges, nodeLookup.get(target), {
    id, target, targetHandle: targetHandleId,
  }, { x: targetX, y: targetY }, targetSide);
  const plans = useMemo(() => {
    const requests: SceneFlowRouteRequest[] = flowEdges.flatMap((flowEdge) => {
      if (flowEdge.type !== "sceneTransition") {
        return [];
      }
      const sourceNode = nodeLookup.get(flowEdge.source);
      const targetNode = nodeLookup.get(flowEdge.target);
      const sourceHandle = sourceNode?.internals.handleBounds?.source?.find(
        (handle) => handle.id === flowEdge.sourceHandle,
      );
      const targetHandle = targetNode?.internals.handleBounds?.target?.find(
        (handle) => handle.id === flowEdge.targetHandle,
      );
      if (sourceNode === undefined || targetNode === undefined || sourceHandle === undefined || targetHandle === undefined) {
        if (flowEdge.id !== id) {
          return [];
        }
        return [{
          id,
          sourceNode: flowEdge.source,
          targetNode: flowEdge.target,
          source: { x: sourceX, y: sourceY },
          target: arrow.routeTarget,
          sourceSide: edgeSourceSide(sourcePosition),
          targetSide: edgeTargetSide(targetPosition),
          rails: draftRails,
        }];
      }
      const candidateData = flowEdge.data as SceneFlowTransitionEdgeData | undefined;
      const source = handleBoundary(sourceNode.internals.positionAbsolute, sourceHandle);
      const target = graphEntryArrow(
        flowEdges, targetNode, flowEdge,
        handleBoundary(targetNode.internals.positionAbsolute, targetHandle),
        edgeTargetSide(targetHandle.position),
      ).routeTarget;
      return [{
        id: flowEdge.id,
        sourceNode: flowEdge.source,
        targetNode: flowEdge.target,
        source,
        target,
        sourceSide: edgeSourceSide(sourceHandle.position),
        targetSide: edgeTargetSide(targetHandle.position),
        rails: flowEdge.id === id
          ? draftRails
          : Array.isArray(candidateData?.rails) ? candidateData.rails : [],
      }];
    });
    const obstacles = flowNodes.flatMap((flowNode) => {
      const internal = nodeLookup.get(flowNode.id);
      if (internal === undefined) {
        return [];
      }
      const width = internal.measured.width ?? flowNode.measured?.width;
      const height = internal.measured.height ?? flowNode.measured?.height;
      if (width === undefined || height === undefined) {
        return [];
      }
      return [{
        id: flowNode.id,
        x: internal.internals.positionAbsolute.x,
        y: internal.internals.positionAbsolute.y,
        width,
        height,
      }];
    });
    return planSceneFlowRoutes(requests, obstacles);
  }, [draftRails, flowEdges, flowNodes, id, nodeLookup, sourcePosition, sourceX, sourceY, targetPosition, targetX, targetY, arrow.routeTarget.x, arrow.routeTarget.y]);

  const fallbackTarget = arrow.routeTarget;
  const route = plans[id]?.route ?? buildStateTransitionRoute({
    sourceX,
    sourceY,
    targetX: fallbackTarget.x,
    targetY: fallbackTarget.y,
    sourceSide: edgeSourceSide(sourcePosition),
    targetSide,
    rails: draftRails,
  });
  const arrowTip = arrow.tip;
  const routeSections = stateTransitionRouteSections(route.controlPoints);
  const gradientId = `scene-transition-gradient-${id.replace(/[^a-zA-Z0-9_-]/g, "_")}`;
  const canEdit = edgeData?.canEdit === true;
  const commitLayout = (rails: EditorRouteRail[]) => {
    const rounded = rails.map((rail) => ({ axis: rail.axis, value: Math.round(rail.value) }));
    draftRailsRef.current = rounded;
    setDraftRails(rounded);
    edgeData?.onSetRouteLayout(rounded);
  };

  useEffect(() => {
    if (!selected || selectedSection === null || !canEdit) {
      return;
    }
    const onKeyDown = (event: globalThis.KeyboardEvent) => {
      if (event.key !== "Delete" && event.key !== "Backspace") {
        return;
      }
      const target = event.target as HTMLElement | null;
      if (target !== null && target.closest("input, select, textarea, [contenteditable='true']") !== null) {
        return;
      }
      const next = removeStateTransitionRouteSection(route.controlPoints, selectedSection, targetSide);
      if (next === null) {
        return;
      }
      event.preventDefault();
      event.stopPropagation();
      setSelectedSection(null);
      commitLayout(next);
    };
    window.addEventListener("keydown", onKeyDown, true);
    return () => window.removeEventListener("keydown", onKeyDown, true);
  }, [canEdit, route.controlPoints, selected, selectedSection, targetSide]);

  const edgeStyle = {
    ...style,
    stroke: `url(#${gradientId})`,
    strokeDasharray: "10 8",
    strokeLinecap: "round" as const,
    strokeLinejoin: "round" as const,
  };

  return (
    <>
      <defs>
        <linearGradient id={gradientId} gradientUnits="userSpaceOnUse" x1={sourceX} y1={sourceY} x2={arrowTip.x} y2={arrowTip.y}>
          <stop offset="0%" stopColor={selected ? "#4dabf7" : "#74c0fc"} />
          <stop offset="100%" stopColor={selected ? "#1864ab" : "#1971c2"} />
        </linearGradient>
      </defs>
      <BaseEdge id={id} path={route.path + arrow.leadPath} style={edgeStyle} />
      <path
        className="state-transition-hit-path"
        d={route.path + arrow.leadPath}
        onClick={(event) => {
          event.stopPropagation();
          edgeData?.onSelect();
        }}
        onDoubleClick={(event) => {
          event.stopPropagation();
          edgeData?.onSelect();
          if (!canEdit) {
            return;
          }
          const point = screenToFlowPosition({ x: event.clientX, y: event.clientY });
          const segmentIndex = closestRouteSegment(route.controlPoints, point);
          const next = insertStateTransitionRouteSection(route.controlPoints, segmentIndex, point, targetSide);
          if (next !== null) {
            setSelectedSection(null);
            commitLayout(next);
          }
        }}
      />
      {(plans[id]?.crossings ?? []).map((crossing, index) => {
        const horizontal = crossing.orientation === "horizontal";
        const maskPath = horizontal
          ? `M ${crossing.x - 10} ${crossing.y} L ${crossing.x + 10} ${crossing.y}`
          : `M ${crossing.x} ${crossing.y - 10} L ${crossing.x} ${crossing.y + 10}`;
        const bridgePath = horizontal
          ? `M ${crossing.x - 10} ${crossing.y} Q ${crossing.x} ${crossing.y - 11} ${crossing.x + 10} ${crossing.y}`
          : `M ${crossing.x} ${crossing.y - 10} Q ${crossing.x + 11} ${crossing.y} ${crossing.x} ${crossing.y + 10}`;
        return (
          <g key={`${id}:crossing-${index}`} className="scene-transition-bridge" pointerEvents="none">
            <path d={maskPath} stroke="#eef2f0" strokeWidth={10} fill="none" />
            <path d={bridgePath} stroke={`url(#${gradientId})`} strokeWidth={selected ? 4.8 : 4} strokeLinecap="round" fill="none" />
          </g>
        );
      })}
      <path className={`state-transition-arrow ${selected ? "selected" : ""} ${arrowHovered ? "hovered" : ""}`} d={arrow.arrowPath} />
      <EdgeLabelRenderer>
        <button
          className="state-transition-arrow-hit nodrag nopan"
          type="button"
          aria-label="Select scene transition"
          data-edge-id={id}
          style={arrow.hitStyle}
          onClick={(event) => {
            event.stopPropagation();
            edgeData?.onSelect();
          }}
          onPointerEnter={() => setArrowHovered(true)}
          onPointerLeave={() => setArrowHovered(false)}
        />
      </EdgeLabelRenderer>
      {selected && draftRails.length > 0 && (
        <EdgeLabelRenderer>
          <button
            className="scene-route-reset nodrag nopan"
            type="button"
            title="Return scene transition to automatic routing"
            aria-label="Return scene transition to automatic routing"
            style={{
              transform: `translate(-50%, -50%) translate(${routeLabelPoint(route.points).x}px, ${routeLabelPoint(route.points).y}px)`,
            }}
            disabled={!canEdit}
            onClick={(event) => {
              event.stopPropagation();
              commitLayout([]);
            }}
          >
            <RotateCcw size={13} aria-hidden="true" />
          </button>
        </EdgeLabelRenderer>
      )}
      {selected && routeSections.map((section) => (
        <EdgeLabelRenderer key={`${id}:section-${section.controlSegmentIndex}`}>
          <button
            className={`state-route-section scene-route-section state-route-section-${section.orientation} nodrag nopan ${selectedSection === section.controlSegmentIndex ? "selected" : ""}`}
            type="button"
            aria-label={`Move ${section.orientation} scene transition section`}
            title={section.orientation === "horizontal" ? "Drag up or down" : "Drag left or right"}
            style={{
              width: section.orientation === "horizontal" ? `${Math.max(22, section.length)}px` : "16px",
              height: section.orientation === "vertical" ? `${Math.max(22, section.length)}px` : "16px",
              transform: `translate(-50%, -50%) translate(${section.center.x}px, ${section.center.y}px)`,
            }}
            disabled={!canEdit}
            onClick={(event) => event.stopPropagation()}
            onPointerDown={(event) => {
              if (!canEdit) {
                return;
              }
              event.preventDefault();
              event.stopPropagation();
              edgeData?.onSelect();
              dragCleanupRef.current?.();
              const basePoints = route.controlPoints.map((point) => ({ ...point }));
              setSelectedSection(section.controlSegmentIndex);
              const pointerId = event.pointerId;
              const onPointerMove = (pointerEvent: globalThis.PointerEvent) => {
                if (pointerEvent.pointerId !== pointerId) {
                  return;
                }
                pointerEvent.preventDefault();
                const point = screenToFlowPosition({ x: pointerEvent.clientX, y: pointerEvent.clientY });
                const next = moveStateTransitionRouteSection(
                  basePoints,
                  section.controlSegmentIndex,
                  point,
                  edgeSourceSide(sourcePosition),
                  targetSide,
                );
                if (next !== null) {
                  draftRailsRef.current = next;
                  setDraftRails(next);
                }
              };
              const finishDrag = (pointerEvent: globalThis.PointerEvent) => {
                if (pointerEvent.pointerId !== pointerId) {
                  return;
                }
                dragCleanupRef.current?.();
                dragCleanupRef.current = null;
                commitLayout(draftRailsRef.current);
              };
              const cancelDrag = (pointerEvent: globalThis.PointerEvent) => {
                if (pointerEvent.pointerId !== pointerId) {
                  return;
                }
                dragCleanupRef.current?.();
                dragCleanupRef.current = null;
                const restored = persistedRails.map((rail) => ({ ...rail }));
                draftRailsRef.current = restored;
                setDraftRails(restored);
              };
              window.addEventListener("pointermove", onPointerMove, true);
              window.addEventListener("pointerup", finishDrag, true);
              window.addEventListener("pointercancel", cancelDrag, true);
              dragCleanupRef.current = () => {
                window.removeEventListener("pointermove", onPointerMove, true);
                window.removeEventListener("pointerup", finishDrag, true);
                window.removeEventListener("pointercancel", cancelDrag, true);
              };
            }}
          />
        </EdgeLabelRenderer>
      ))}
    </>
  );
}

type GradientTransitionEdgeData = {
  tone?: "blue" | "green";
  onSelect?: () => void;
};

function GradientTransitionEdge({
  data,
  id,
  selected,
  sourcePosition,
  sourceX,
  sourceY,
  style,
  target,
  targetHandleId,
  targetPosition,
  targetX,
  targetY,
}: EdgeProps) {
  const flowEdges = useStore((state) => state.edges);
  const targetNode = useStore((state) => state.nodeLookup.get(target));
  const arrow = graphEntryArrow(flowEdges, targetNode, {
    id, target, targetHandle: targetHandleId,
  }, { x: targetX, y: targetY }, edgeTargetSide(targetPosition));
  const [path] = getSmoothStepPath({
    sourceX,
    sourceY,
    sourcePosition,
    targetX: arrow.routeTarget.x,
    targetY: arrow.routeTarget.y,
    targetPosition,
    borderRadius: 10,
  });
  const tone = (data as GradientTransitionEdgeData | undefined)?.tone ?? "blue";
  const gradientId = `gradient-transition-${id.replace(/[^a-zA-Z0-9_-]/g, "_")}`;
  const startColor = tone === "green"
    ? selected ? "#69db7c" : "#8ce99a"
    : selected ? "#4dabf7" : "#74c0fc";
  const endColor = tone === "green"
    ? selected ? "#2b8a3e" : "#2f9e44"
    : selected ? "#1864ab" : "#1971c2";
  return (
    <>
      <defs>
        <linearGradient id={gradientId} gradientUnits="userSpaceOnUse" x1={sourceX} y1={sourceY} x2={arrow.tip.x} y2={arrow.tip.y}>
          <stop offset="0%" stopColor={startColor} />
          <stop offset="100%" stopColor={endColor} />
        </linearGradient>
      </defs>
      <BaseEdge
        id={id}
        path={path + arrow.leadPath}
        style={{
          ...style,
          stroke: `url(#${gradientId})`,
          strokeDasharray: "10 8",
          strokeLinecap: "round",
          strokeLinejoin: "round",
        }}
      />
      <path className="state-transition-arrow" d={arrow.arrowPath} style={{ fill: endColor }} />
      <EdgeLabelRenderer>
        <button
          className="state-transition-arrow-hit nodrag nopan"
          type="button"
          aria-label="Select package entry transition"
          data-edge-id={id}
          style={arrow.hitStyle}
          onClick={(event) => {
            event.stopPropagation();
            (data as GradientTransitionEdgeData | undefined)?.onSelect?.();
          }}
        />
      </EdgeLabelRenderer>
    </>
  );
}

const STATE_EDGE_TYPES = {
  gradientTransition: GradientTransitionEdge,
  stateTransition: StateTransitionEdge,
};

type SceneCardNodeData = {
  graphNode: GraphSceneNode;
  entryActive: boolean;
  thumbnail: Framebuffer | null;
  targetScenes: SceneDocument[];
  selectedSceneExitId: string | null;
  selectedRouteId: string | null;
  canEdit: boolean;
  onSelectScene: (sceneId: string) => void;
  onSelectSceneRoute: (sceneId: string, routeId: string) => void;
  onSelectSceneExit: (sceneId: string, sceneExitId: string) => void;
  onDeleteSceneExit: (sceneId: string, sceneExitId: string) => void;
  onDeleteLegacyRoute: (sceneId: string, routeId: string) => void;
};

function SceneCardNode({ data, selected }: NodeProps<Node<SceneCardNodeData>>) {
  const {
    graphNode,
    entryActive,
    targetScenes,
    canEdit,
    onSelectScene,
    onSelectSceneRoute,
    onSelectSceneExit,
    onDeleteSceneExit,
    onDeleteLegacyRoute,
    selectedSceneExitId,
    selectedRouteId,
    thumbnail,
  } = data;
  const canAddExit = canEdit && targetScenes.length > 0;

  const handleSelectKey = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      onSelectScene(graphNode.id);
    }
  };

  return (
    <div
      className={`scene-card-node ${entryActive ? "entry-active" : ""} ${selected ? "selected" : ""}`}
      role="button"
      tabIndex={0}
      onClick={() => onSelectScene(graphNode.id)}
      onKeyDown={handleSelectKey}
    >
      <div className="scene-card-heading">
        <strong>{graphNode.label}</strong>
      </div>
      <div className="scene-card-preview-row">
        <span className="scene-entry-stem" aria-hidden="true" />
        <Handle
          className="scene-entry-handle"
          id="entry"
          type="target"
          position={Position.Left}
          isConnectable={canEdit}
        />
        <div className="scene-card-preview" aria-hidden="true">
          <FramebufferCanvas framebuffer={thumbnail} />
        </div>
      </div>
      <div className="scene-exit-list">
        {graphNode.exits.map((exit) => (
          <span
            className={`scene-exit-row ${
              (exit.sceneExitId !== undefined
                ? selectedSceneExitId === exit.sceneExitId
                : selectedRouteId === exit.routeId)
                ? "selected" : ""}`}
            key={exit.id}
            role="button"
            tabIndex={0}
            onClick={(event) => {
              event.stopPropagation();
              event.currentTarget.focus();
              if (exit.sceneExitId !== undefined) {
                onSelectSceneExit(graphNode.id, exit.sceneExitId);
              } else if (exit.routeId !== undefined) {
                onSelectSceneRoute(graphNode.id, exit.routeId);
              }
            }}
            onKeyDown={(event) => {
              if (event.key === "Enter" || event.key === " ") {
                event.preventDefault();
                event.stopPropagation();
                if (exit.sceneExitId !== undefined) {
                  onSelectSceneExit(graphNode.id, exit.sceneExitId);
                } else if (exit.routeId !== undefined) {
                  onSelectSceneRoute(graphNode.id, exit.routeId);
                }
                return;
              }
              if (canEdit && (event.key === "Delete" || event.key === "Backspace")) {
                event.preventDefault();
                event.stopPropagation();
                if (exit.sceneExitId !== undefined) {
                  onDeleteSceneExit(graphNode.id, exit.sceneExitId);
                } else if (exit.routeId !== undefined) {
                  onDeleteLegacyRoute(graphNode.id, exit.routeId);
                }
              }
            }}
          >
            <span>{exit.label}</span>
            <span className="scene-exit-stem" aria-hidden="true" />
            <Handle
              className="scene-exit-handle"
              id={exit.id}
              type="source"
              position={Position.Right}
              isConnectable={canEdit}
            />
          </span>
        ))}
      </div>
      <div
        className={`scene-new-exit-slot ${canAddExit ? "" : "disabled"}`}
        aria-label={canAddExit ? "Drag to create a new scene exit" : "No destination scenes available"}
        onClick={(event) => event.stopPropagation()}
      >
        <Plus size={14} aria-hidden="true" />
        <span>Add new exit</span>
        <span className="scene-new-exit-stem" aria-hidden="true" />
        <Handle
          className="scene-new-exit-handle"
          id={`${graphNode.id}:${NEW_SCENE_EXIT_HANDLE}`}
          type="source"
          position={Position.Right}
          isConnectable={canAddExit}
        />
      </div>
    </div>
  );
}

type PackageEntryNodeData = {
  graphNode: GraphPackageEntryNode;
  canEdit: boolean;
  onSelect: () => void;
};

const SCENE_FLOW_HANDLE_POSITION: Record<StateGraphExitSide, Position> = {
  top: Position.Top,
  right: Position.Right,
  bottom: Position.Bottom,
  left: Position.Left,
};

function PackageEntryNode({ data, selected }: NodeProps<Node<PackageEntryNodeData>>) {
  const outputSide = data.graphNode.outputSide;
  const updateNodeInternals = useUpdateNodeInternals();
  useEffect(() => {
    const animationFrame = window.requestAnimationFrame(() => {
      updateNodeInternals(data.graphNode.id);
    });
    return () => window.cancelAnimationFrame(animationFrame);
  }, [data.graphNode.id, outputSide, updateNodeInternals]);
  return (
    <div
      className={`package-entry-node ${selected ? "selected" : ""}`}
      role="button"
      tabIndex={0}
      onClick={(event) => {
        event.stopPropagation();
        data.onSelect();
      }}
      onKeyDown={(event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          data.onSelect();
        }
      }}
    >
      <strong>Start</strong>
      <Handle
        className={`package-entry-handle ${outputSide}`}
        id="package-entry-out"
        type="source"
        position={SCENE_FLOW_HANDLE_POSITION[outputSide]}
        isConnectable={data.canEdit}
      />
    </div>
  );
}

type SceneReferenceNodeData = {
  graphNode: GraphSceneReferenceNode;
  canEdit: boolean;
  onSelect: (referenceId: string, targetScene: string) => void;
};

function SceneReferenceNode({ data, selected }: NodeProps<Node<SceneReferenceNodeData>>) {
  const { graphNode } = data;
  return (
    <div
      className={`state-scene-endpoint boundary exit scene-reference-node ${selected ? "selected" : ""}`}
      role="button"
      tabIndex={0}
      aria-label={`Go to ${graphNode.label.replace(/^Go to /, "")}`}
      onClick={(event) => {
        event.stopPropagation();
        data.onSelect(graphNode.id, graphNode.targetScene);
      }}
      onKeyDown={(event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          data.onSelect(graphNode.id, graphNode.targetScene);
        }
      }}
    >
      <span className="scene-boundary-direction" aria-hidden="true">
        <Triangle size={36} strokeWidth={1.5} fill="currentColor" />
      </span>
      <strong>{graphNode.label.replace(/^Go to /, "")}</strong>
      <Handle
        className="scene-boundary-local-handle scene-reference-handle"
        id="reference-entry"
        type="target"
        position={Position.Left}
        isConnectable={data.canEdit}
      />
    </div>
  );
}

const SCENE_NODE_TYPES = {
  packageEntry: PackageEntryNode,
  sceneCard: SceneCardNode,
  sceneReference: SceneReferenceNode,
};
const SCENE_EDGE_TYPES = {
  gradientTransition: GradientTransitionEdge,
  sceneTransition: SceneFlowTransitionEdge,
};

function sameNodeSet(left: Node[], right: Node[]) {
  if (left.length !== right.length) {
    return false;
  }
  const rightIds = new Set(right.map((node) => node.id));
  return left.every((node) => rightIds.has(node.id));
}

export function StateGraphView({
  timerTypes = [],
  onRequestTimer,
  scene,
  activeStateId,
  editor,
  layoutStatus,
  selected,
  physicalEventKinds,
  peepOSTriggers,
  onSelect,
  onSelectBackground,
  onCreateState,
  onDeleteState,
  onMoveStateNode,
  onSetEntryConnection,
  onSetRouteLayout,
  onCreateTriggerRoute,
  onRebindTriggerRoute,
  onConnectRouteToSceneExit,
  onDeleteSystemExit,
  canCreateState,
  canEdit,
  canMoveStates = canEdit,
  canDeleteStates = canEdit,
  canEditEntry = canEdit,
  canConnectScenes = canEdit,
}: {
  scene: SceneDocument | null;
  activeStateId: string | null;
  editor?: ProjectEditorData;
  layoutStatus: string;
  selected: SceneSelection;
  physicalEventKinds: StateTriggerEventKind[];
  peepOSTriggers: PeepOSTriggerCapability[];
  timerTypes?: string[];
  onRequestTimer?: (stateId: string, eventType: string) => void;
  onSelect: (selection: SceneSelection) => void;
  onSelectBackground?: () => void;
  onCreateState: (sceneId: string, x: number, y: number) => void;
  onDeleteState: (sceneId: string, stateId: string) => void;
  onMoveStateNode: (sceneId: string, stateId: string, x: number, y: number) => void;
  onSetEntryConnection: (
    sceneId: string,
    stateId: string,
    targetHandle: StateGraphEntryHandle,
    targetSide: StateGraphEntrySide,
  ) => void;
  onSetRouteLayout: (
    sceneId: string,
    routeId: string,
    sourceState: string,
    rails: EditorRouteRail[],
    targetHandle: StateGraphEntryHandle | null,
    targetSide: StateGraphEntrySide | null,
    tokenPositions: EditorRouteTokenPositions,
  ) => void;
  onCreateTriggerRoute: (
    sceneId: string,
    sourceState: string,
    logicalSource: string,
    eventKind: StateTriggerEventKind,
    target: NewStateTransitionTarget,
  ) => void;
  onRebindTriggerRoute: (sceneId: string, routeId: string, logicalSource: string) => void;
  onConnectRouteToSceneExit: (
    sceneId: string,
    routeId: string,
    sceneExitId: string,
    targetScene: string,
  ) => void;
  onDeleteSystemExit: (sceneId: string) => void;
  canCreateState: boolean;
  canEdit: boolean;
  canMoveStates?: boolean;
  canDeleteStates?: boolean;
  canEditEntry?: boolean;
  canConnectScenes?: boolean;
}) {
  const graph = useMemo(() => buildStateGraphModel(scene, editor), [editor, scene]);
  const flowRef = useRef<ReactFlowInstance | null>(null);
  const didInitialFit = useRef(false);
  const previousSceneId = useRef<string | null>(scene?.scene_id ?? null);
  const [pendingPhysicalConnection, setPendingPhysicalConnection] = useState<PendingPhysicalTriggerConnection | null>(null);
  const [peepOSTriggerStateId, setPeepOSTriggerStateId] = useState<string | null>(null);
  const defaultPositionById = useMemo(() => statePositionMap(graph.nodes, []), [graph.nodes]);
  const baseNodes: Node[] = useMemo(
    () => [
      ...graph.nodes.map((node) => ({
        id: node.id,
        type: "stateCard",
        position: { x: node.x, y: node.y },
        data: {
          graphNode: routeStateNode(node, defaultPositionById, {}),
          activeEntryHandles: node.isEntry ? [graph.entryEdge?.targetHandle ?? "entry-top-left"] : [],
          runtimeActive: activeStateId === node.id,
          canEdit,
          joystickPolicy: scene?.joystick_policy ?? "four_way",
          physicalEventKinds,
          hasPeepOSTriggers: peepOSTriggers.length > 0,
          peepOSTriggerOpen: peepOSTriggerStateId === node.id,
          selectedRouteId: selected.kind === "route" && (selected.sourceState === undefined || selected.sourceState === node.id)
            ? selected.id
            : null,
          onOpenPeepOSTriggers: (stateId: string) => {
            setPendingPhysicalConnection(null);
            setPeepOSTriggerStateId((current) => current === stateId ? null : stateId);
          },
          onSelectState: (stateId: string) => onSelect({ kind: "state", id: stateId }),
          onSelectRoute: (routeId: string, sourceState: string) => onSelect({ kind: "route", id: routeId, sourceState }),
        },
        selected: selected.kind === "state" && selected.id === node.id,
        draggable: canMoveStates,
        connectable: canEdit,
      })),
      ...graph.endpoints.map((endpoint) => ({
        id: endpoint.id,
        type: "sceneEndpoint",
        position: { x: endpoint.x, y: endpoint.y },
        data: {
          endpoint,
          canEdit: endpoint.kind === "exit" ? canConnectScenes : canEdit,
          onSelect: (selectedEndpoint: GraphSceneEndpointNode) => {
            onSelect(selectedEndpoint.kind === "exit" && selectedEndpoint.sceneExitId !== undefined
              ? { kind: "sceneExit", id: selectedEndpoint.sceneExitId }
              : selectedEndpoint.kind === "system"
                ? { kind: "systemExit" }
                : { kind: "scene" });
          },
        },
        selected: endpoint.kind === "exit"
          ? endpoint.sceneExitId !== undefined && selected.kind === "sceneExit" && selected.id === endpoint.sceneExitId
          : endpoint.kind === "system" && selected.kind === "systemExit",
        draggable: endpoint.kind === "entry" ? canMoveStates : endpoint.kind === "exit" ? canConnectScenes && endpoint.declared : canEdit,
        connectable: endpoint.kind === "exit" ? canConnectScenes : canEdit,
      })),
    ],
    [activeStateId, canEdit, canMoveStates, canConnectScenes, defaultPositionById, graph.endpoints, graph.entryEdge?.targetHandle, graph.nodes, onSelect, peepOSTriggerStateId, peepOSTriggers.length, physicalEventKinds, scene?.joystick_policy, selected],
  );
  const [nodes, setNodes] = useState<Node[]>(baseNodes);
  const graphNodeById = useMemo(() => new Map(graph.nodes.map((node) => [node.id, node])), [graph.nodes]);
  const positionById = useMemo(() => statePositionMap(graph.nodes, nodes), [graph.nodes, nodes]);
  const layoutNodes = useMemo(
    () => [...positionById.entries()].map(([id, position]) => ({
      id,
      x: position.x,
      y: position.y,
      platformOutputCount: graphNodeById.get(id)?.platformOutputCount ?? 0,
    })),
    [graphNodeById, positionById],
  );
  const transitionLayouts = useMemo(
    () =>
      planStateTransitionRoutes(
        graph.edges.filter((edge) => edge.targetKind === "state").map((edge) => {
          const sourceNode = graphNodeById.get(edge.source);
          const sourceOutput = sourceNode?.outputs.find((output) => output.id === edge.sourceHandle);
          const sourceOutputIndex = sourceNode?.outputs
            .filter((output) => output.triggerKind === "platform")
            .findIndex((output) => output.id === edge.sourceHandle) ?? 0;
          return {
            id: edge.sourceHandle,
            source: edge.source,
            target: edge.target,
            sourceOutputIndex: Math.max(0, sourceOutputIndex),
            sourceSide: sourceOutput?.preferredExitSide,
            sourceRatio: sourceOutput?.exitRatio,
            targetHandle: edge.targetHandle,
            targetSide: edge.targetSide,
            rails: edge.rails,
          };
        }),
        layoutNodes.filter((node) => graphNodeById.has(node.id)),
      ),
    [graph.edges, graphNodeById, layoutNodes],
  );
  const flowNodes: Node[] = useMemo(
    () =>
      nodes.map((node) => {
        const graphNode = graphNodeById.get(node.id);
        if (graphNode === undefined) {
          return node;
        }
        return {
          ...node,
          data: {
            graphNode: routeStateNode(graphNode, positionById, transitionLayouts),
            activeEntryHandles: [
              ...(graphNode.isEntry ? [graph.entryEdge?.targetHandle ?? "entry-top-left"] : []),
              ...graph.edges
                .filter((edge) => edge.target === graphNode.id)
                .map((edge) => transitionLayouts[edge.sourceHandle]?.targetHandle)
                .filter((handle): handle is StateGraphEntryHandle => handle !== undefined),
            ],
            runtimeActive: activeStateId === graphNode.id,
            canEdit,
            joystickPolicy: scene?.joystick_policy ?? "four_way",
            physicalEventKinds,
            hasPeepOSTriggers: peepOSTriggers.length > 0,
            peepOSTriggerOpen: peepOSTriggerStateId === graphNode.id,
            selectedRouteId: selected.kind === "route" && (selected.sourceState === undefined || selected.sourceState === graphNode.id)
              ? selected.id
              : null,
            onOpenPeepOSTriggers: (stateId: string) => {
              setPendingPhysicalConnection(null);
              setPeepOSTriggerStateId((current) => current === stateId ? null : stateId);
            },
            onSelectState: (stateId: string) => onSelect({ kind: "state", id: stateId }),
            onSelectRoute: (routeId: string, sourceState: string) => onSelect({ kind: "route", id: routeId, sourceState }),
          },
          selected: selected.kind === "state" && selected.id === node.id,
          draggable: canMoveStates,
          connectable: canEdit,
        };
      }),
    [activeStateId, canEdit, canMoveStates, graph.edges, graph.entryEdge?.targetHandle, graphNodeById, nodes, onSelect, peepOSTriggerStateId, peepOSTriggers.length, physicalEventKinds, positionById, scene?.joystick_policy, selected, transitionLayouts],
  );
  useEffect(() => {
    setPendingPhysicalConnection(null);
    setPeepOSTriggerStateId(null);
  }, [scene?.scene_id]);
  useEffect(() => {
    if (scene === null || (selected.kind !== "state" && selected.kind !== "systemExit")
      || (selected.kind === "state" ? !canDeleteStates : !canEdit)) {
      return;
    }
    const onKeyDown = (event: globalThis.KeyboardEvent) => {
      if (event.key !== "Delete" && event.key !== "Backspace") {
        return;
      }
      const target = event.target instanceof Element ? event.target : null;
      if (target !== null && target.closest("input, select, textarea, button, [contenteditable='true']") !== null) {
        return;
      }
      event.preventDefault();
      event.stopPropagation();
      if (selected.kind === "state") {
        onDeleteState(scene.scene_id, selected.id);
      } else {
        onDeleteSystemExit(scene.scene_id);
      }
    };
    window.addEventListener("keydown", onKeyDown, true);
    return () => window.removeEventListener("keydown", onKeyDown, true);
  }, [canEdit, canDeleteStates, onDeleteState, onDeleteSystemExit, scene, selected]);
  useEffect(() => {
    const sceneId = scene?.scene_id ?? null;
    setNodes((current) => {
      if (previousSceneId.current !== sceneId) {
        previousSceneId.current = sceneId;
        return baseNodes;
      }
      if (sameNodeSet(current, baseNodes)) {
        const currentById = new Map(current.map((node) => [node.id, node]));
        return baseNodes.map((node) => {
          const currentNode = currentById.get(node.id);
          if (currentNode === undefined) {
            return node;
          }
          return {
            ...currentNode,
            type: node.type,
            data: node.data,
            selected: node.selected,
            draggable: node.draggable,
            connectable: node.connectable,
            position: currentNode.position,
          };
        });
      }
      return baseNodes;
    });
  }, [baseNodes, scene?.scene_id]);
  const onNodesChange = (changes: NodeChange[]) => {
    setNodes((current) => applyNodeChanges(changes, current));
  };
  const onNodeDragStop = (node: Node) => {
    if (scene === null) {
      return;
    }
    const x = Math.round(node.position.x);
    const y = Math.round(node.position.y);
    setNodes((current) =>
      current.map((item) => (item.id === node.id ? { ...item, position: { x, y } } : item)),
    );
    onMoveStateNode(scene.scene_id, node.id, x, y);
  };
  const onConnect = (connection: Connection) => {
    if (scene === null || connection.source === null || connection.target === null || connection.sourceHandle === null) {
      return;
    }
    const sourceEndpoint = graph.endpoints.find((item) => item.id === connection.source);
    if (sourceEndpoint?.kind === "entry" && connection.sourceHandle === "scene-entry-out") {
      const targetState = graph.nodes.find((item) => item.id === connection.target);
      const [targetHandleValue, targetSideValue] = (connection.targetHandle ?? "").split(":");
      const targetPort = STATE_GRAPH_ENTRY_PORTS.find(
        (port) => port.handle === targetHandleValue && port.side === targetSideValue,
      );
      if (targetState !== undefined && targetPort !== undefined) {
        onSetEntryConnection(
          scene.scene_id,
          targetState.id,
          targetPort.handle,
          targetPort.side,
        );
      }
      return;
    }
    const sourceNode = graph.nodes.find((item) => item.id === connection.source);
    if (sourceNode === undefined) {
      return;
    }
    if (connection.sourceHandle.startsWith("new-physical-trigger:")) {
      if (!canEdit) return;
      const logicalSource = connection.sourceHandle.slice("new-physical-trigger:".length);
      let target: NewStateTransitionTarget | null = null;
      const targetState = graph.nodes.find((item) => item.id === connection.target);
      if (targetState !== undefined) {
        const [targetHandleValue, targetSideValue] = (connection.targetHandle ?? "").split(":");
        const targetPort = STATE_GRAPH_ENTRY_PORTS.find(
          (port) => port.handle === targetHandleValue && port.side === targetSideValue,
        );
        if (targetPort === undefined) {
          return;
        }
        target = {
          kind: "state",
          stateId: targetState.id,
          targetHandle: targetPort.handle,
          targetSide: targetPort.side,
        };
      }
      if (target === null) {
        const newRouteEndpoint = graph.endpoints.find((item) => item.id === connection.target);
        if (canConnectScenes && newRouteEndpoint?.kind === "exit" && newRouteEndpoint.sceneExitId !== undefined) {
          target = {
            kind: "sceneExit",
            sceneExitId: newRouteEndpoint.sceneExitId,
          };
        } else if (newRouteEndpoint?.kind === "system") {
          target = { kind: "systemExit" };
        }
      }
      const eventKinds = availablePhysicalEventKinds(sourceNode, logicalSource, physicalEventKinds);
      if (target !== null && eventKinds.length > 0) {
        setPeepOSTriggerStateId(null);
        setPendingPhysicalConnection({
          sceneId: scene.scene_id,
          sourceState: sourceNode.id,
          logicalSource,
          target,
          eventKinds,
          eventKind: eventKinds[0],
        });
      }
      return;
    }
    const endpoint = graph.endpoints.find((item) => item.id === connection.target);
    if (!canConnectScenes || endpoint?.kind !== "exit" || endpoint.sceneExitId === undefined || endpoint.targetScene === undefined) {
      return;
    }
    const output = sourceNode.outputs.find((item) => item.id === connection.sourceHandle);
    if (output === undefined) {
      return;
    }
    onConnectRouteToSceneExit(
      scene.scene_id,
      output.routeId,
      endpoint.sceneExitId,
      endpoint.targetScene,
    );
  };
  const onReconnect = (edge: Edge, connection: Connection) => {
    if (scene !== null && graph.entryEdge !== undefined && edge.id === `${graph.entryEdge.source}->${graph.entryEdge.target}`) {
      const targetState = graph.nodes.find((node) => node.id === connection.target);
      const [targetHandleValue, targetSideValue] = (connection.targetHandle ?? "").split(":");
      const targetPort = STATE_GRAPH_ENTRY_PORTS.find(
        (port) => port.handle === targetHandleValue && port.side === targetSideValue,
      );
      if (targetState !== undefined && targetPort !== undefined) {
        onSetEntryConnection(scene.scene_id, targetState.id, targetPort.handle, targetPort.side);
      }
      return;
    }
    if (
      scene === null
      || connection.source !== edge.source
      || connection.sourceHandle === null
      || !connection.sourceHandle.startsWith("new-physical-trigger:")
    ) {
      return;
    }
    const routeId = typeof edge.data?.route_id === "string" ? edge.data.route_id : null;
    const sourceNode = graph.nodes.find((node) => node.id === edge.source);
    const sourceOutput = sourceNode?.outputs.find((output) => output.routeId === routeId);
    if (routeId === null || sourceOutput?.triggerKind !== "physical") {
      return;
    }
    const logicalSource = connection.sourceHandle.slice("new-physical-trigger:".length);
    onRebindTriggerRoute(scene.scene_id, routeId, logicalSource);
  };
  useEffect(() => {
    if (flowRef.current === null) {
      return;
    }
    window.requestAnimationFrame(() => {
      flowRef.current?.fitView({ padding: 0.22, maxZoom: 1 });
    });
  }, [scene?.scene_id]);
  useEffect(() => {
    if (activeStateId === null) {
      return;
    }
    const animationFrame = window.requestAnimationFrame(() => {
      const instance = flowRef.current;
      const activeNode = instance?.getNode(activeStateId);
      if (instance === null || instance === undefined || activeNode === undefined) {
        return;
      }
      const width = activeNode.measured?.width ?? activeNode.width ?? 300;
      const height = activeNode.measured?.height ?? activeNode.height ?? 268;
      const viewport = instance.getViewport();
      const flowElement = document.querySelector<HTMLElement>(".state-graph-flow");
      if (flowElement === null) {
        return;
      }
      const padding = 72;
      const nodeLeft = activeNode.position.x * viewport.zoom + viewport.x;
      const nodeTop = activeNode.position.y * viewport.zoom + viewport.y;
      const nodeRight = nodeLeft + width * viewport.zoom;
      const nodeBottom = nodeTop + height * viewport.zoom;
      let shiftX = 0;
      let shiftY = 0;
      if (nodeLeft < padding) {
        shiftX = padding - nodeLeft;
      } else if (nodeRight > flowElement.clientWidth - padding) {
        shiftX = flowElement.clientWidth - padding - nodeRight;
      }
      if (nodeTop < padding) {
        shiftY = padding - nodeTop;
      } else if (nodeBottom > flowElement.clientHeight - padding) {
        shiftY = flowElement.clientHeight - padding - nodeBottom;
      }
      if (shiftX !== 0 || shiftY !== 0) {
        void instance.setViewport(
          { x: viewport.x + shiftX, y: viewport.y + shiftY, zoom: viewport.zoom },
          { duration: 220 },
        );
      }
    });
    return () => window.cancelAnimationFrame(animationFrame);
  }, [activeStateId, scene?.scene_id]);
  const selectedTransition = useMemo(() => {
    if (selected.kind !== "route") {
      return null;
    }
    for (const node of graph.nodes) {
      if (selected.sourceState !== undefined && selected.sourceState !== node.id) {
        continue;
      }
      const output = node.outputs.find((item) => item.routeId === selected.id);
      if (output !== undefined) {
        const selectedEdge = graph.edges.find((edge) => edge.route.route_id === selected.id && edge.source === node.id);
        const targetsSystem = selectedEdge?.targetKind === "system_exit";
        const targetLabel = targetsSystem
          ? "Exit to PeepOS"
          : output.targetScene === undefined
          ? output.targetStateLabel ?? output.targetState ?? "state"
          : output.targetScene;
        return {
          from: node.label,
          trigger: output.label,
          target: targetLabel,
          targetKind: targetsSystem ? "system action" : output.targetScene === undefined ? "state" : "scene",
          conditions: output.guardCount,
          effects: output.actionCount,
        };
      }
    }
    return null;
  }, [graph.edges, graph.nodes, selected]);
  const pendingPhysicalSourceLabel = pendingPhysicalConnection === null
    ? ""
    : INPUT_LABELS[pendingPhysicalConnection.logicalSource] ?? pendingPhysicalConnection.logicalSource;
  const pendingPhysicalTargetLabel = (() => {
    const target = pendingPhysicalConnection?.target;
    if (target === undefined) {
      return "";
    }
    if (target.kind === "state") {
      const stateId = target.stateId;
      return graph.nodes.find((node) => node.id === stateId)?.label ?? stateId;
    }
    if (target.kind === "systemExit") {
      return "Exit to PeepOS";
    }
    const sceneExitId = target.sceneExitId;
    return graph.endpoints.find((endpoint) => endpoint.sceneExitId === sceneExitId)?.label ?? sceneExitId;
  })();
  const peepOSTriggerStateLabel = graph.nodes.find((node) => node.id === peepOSTriggerStateId)?.label ?? "State";
  const edges: Edge[] = useMemo(
    () => {
      const transitionEdges = graph.edges.map((edge) => {
        const transitionLayout = transitionLayouts[edge.sourceHandle];
        const sourceNode = graphNodeById.get(edge.source);
        const sourceOutput = sourceNode?.outputs.find((output) => output.id === edge.sourceHandle);
        const targetNode = graphNodeById.get(edge.target);
        const targetNodePosition = positionById.get(edge.target);
        const targetEntryPorts = edge.targetKind !== "state" || targetNode === undefined || targetNodePosition === undefined
          ? undefined
          : STATE_GRAPH_ENTRY_PORTS.map((port) => ({
              handle: port.handle,
              side: port.side,
              point: stateEntryPortPoint(
                  { ...targetNodePosition, platformOutputCount: targetNode.platformOutputCount },
                  port.handle,
                  port.side,
                ),
            }));
        const isSelected = selected.kind === "route"
          && selected.id === edge.route.route_id
          && (selected.sourceState === undefined || selected.sourceState === edge.source);
        return {
          id: edge.id,
          source: edge.source,
          target: edge.target,
          sourceHandle: edge.sourceHandle,
          targetHandle: transitionLayout === undefined
            ? edge.targetKind === "scene_exit"
              ? "scene-exit-in"
              : edge.targetKind === "system_exit" ? "system-exit-in" : undefined
            : stateEntryPortId(transitionLayout.targetHandle, transitionLayout.targetSide),
          reconnectable: sourceOutput?.triggerKind === "physical" ? "source" as const : false,
          selected: isSelected,
          className: isSelected ? "state-transition-edge selected" : "state-transition-edge",
          data: {
            route_id: edge.route.route_id,
            source_state: edge.source,
            laneX: transitionLayout?.laneX,
            guards: edge.guards,
            actions: edge.actions,
            rails: edge.rails,
            tokenPositions: edge.tokenPositions,
            targetHandle: edge.targetKind === "state" ? transitionLayout?.targetHandle : undefined,
            targetSide: edge.targetKind === "state" ? transitionLayout?.targetSide : "left",
            targetEntryPorts,
            canEdit,
            showSectionHandles: isSelected && selected.kind === "route" && selected.sourceState !== undefined,
            onSelectRoute: (routeId: string, sourceState: string) => onSelect({ kind: "route", id: routeId, sourceState }),
            onSetRouteLayout: (
              routeId: string,
              sourceState: string,
              rails: EditorRouteRail[],
              targetHandle: StateGraphEntryHandle | null,
              targetSide: StateGraphEntrySide | null,
              tokenPositions: EditorRouteTokenPositions,
            ) => {
              if (scene !== null) {
                onSetRouteLayout(scene.scene_id, routeId, sourceState, rails, targetHandle, targetSide, tokenPositions);
              }
            },
          },
          label: edge.label,
          type: "stateTransition",
          style: { strokeWidth: isSelected ? 4.8 : 4 },
        };
      });
      const entryEdge = graph.entryEdge === undefined ? [] : [{
        id: `${graph.entryEdge.source}->${graph.entryEdge.target}`,
        source: graph.entryEdge.source,
        target: graph.entryEdge.target,
        sourceHandle: "scene-entry-out",
        targetHandle: stateEntryPortId(graph.entryEdge.targetHandle, graph.entryEdge.targetSide),
        type: "stateTransition",
        reconnectable: "target" as const,
        selectable: canEditEntry,
        focusable: canEditEntry,
        data: {
          tone: "green",
          targetHandle: graph.entryEdge.targetHandle,
          targetSide: graph.entryEdge.targetSide,
          targetEntryPorts: graph.nodes.flatMap((node) => STATE_GRAPH_ENTRY_PORTS.map((port) => ({
            ...port, stateId: node.id,
            point: stateEntryPortPoint({ ...(positionById.get(node.id) ?? node), platformOutputCount: node.platformOutputCount }, port.handle, port.side),
          }))),
          canEdit: canEditEntry,
          onSelectRoute: () => onSelect({ kind: "scene" }),
          onSetEntryTarget: (stateId: string, handle: StateGraphEntryHandle, side: StateGraphEntrySide) => {
            if (scene !== null) {
              onSetEntryConnection(scene.scene_id, stateId, handle, side);
            }
          },
        },
        markerEnd: { type: MarkerType.ArrowClosed, color: "#2f9e44" },
        style: { strokeWidth: 4 },
      }];
      return [...transitionEdges, ...entryEdge];
    },
    [canEdit, canEditEntry, graph.edges, graph.entryEdge, graph.nodes, graphNodeById, onSelect, onSetEntryConnection, onSetRouteLayout, positionById, scene, selected, transitionLayouts],
  );

  if (scene === null) {
    return (
      <div className="graph-empty">
        <Network size={22} aria-hidden="true" />
        <span>Open a STATE scene to inspect its graph.</span>
      </div>
    );
  }

  if (scene.scene_type !== "STATE_SCENE") {
    return (
      <div className="graph-empty">
        <Network size={22} aria-hidden="true" />
        <span>{scene.scene_type} graph inspection is not exposed in Stage 1.</span>
      </div>
    );
  }

  return (
    <ReactFlow
      className="state-graph-flow"
      nodes={flowNodes}
      edges={edges}
      edgeTypes={STATE_EDGE_TYPES}
      nodeTypes={STATE_NODE_TYPES}
      fitViewOptions={{ padding: 0.22, maxZoom: 1 }}
      onInit={(instance) => {
        flowRef.current = instance;
        if (!didInitialFit.current) {
          didInitialFit.current = true;
          window.requestAnimationFrame(() => {
            instance.fitView({ padding: 0.22, maxZoom: 1 });
          });
        }
      }}
      nodesDraggable={canMoveStates}
      nodesConnectable={canEdit}
      edgesReconnectable={canEdit}
      deleteKeyCode={null}
      elementsSelectable
      onNodesChange={onNodesChange}
      onConnect={onConnect}
      onReconnect={onReconnect}
      onNodeDragStop={(_, node) => onNodeDragStop(node)}
      onNodeClick={(_, node) => {
        if (node.type === "stateCard") {
          onSelect({ kind: "state", id: node.id });
          return;
        }
        const endpoint = (node.data as SceneEndpointNodeData | undefined)?.endpoint;
        onSelect(endpoint?.kind === "exit" && endpoint.sceneExitId !== undefined
          ? { kind: "sceneExit", id: endpoint.sceneExitId }
          : endpoint?.kind === "system"
            ? { kind: "systemExit" }
            : { kind: "scene" });
      }}
      onEdgeClick={(_, edge) => {
        if (edge.data?.route_id === undefined) {
          onSelect({ kind: "scene" });
          return;
        }
        onSelect({
          kind: "route",
          id: String(edge.data.route_id),
          sourceState: String(edge.data?.source_state ?? edge.source),
        });
      }}
      onPaneClick={() => {
        onSelect({ kind: "scene" });
        onSelectBackground?.();
      }}
      proOptions={{ hideAttribution: true }}
    >
      <Background gap={18} size={1} />
      <GraphMiniMap
        nodes={[...graph.nodes, ...graph.endpoints]}
        edges={graph.edges}
        selectedId={selected.kind === "state" ? selected.id : null}
      />
      {pendingPhysicalConnection !== null && (
        <Panel
          position="top-center"
          className="state-graph-trigger-panel"
          onClick={(event) => event.stopPropagation()}
        >
          <div className="state-graph-trigger-heading">
            <div>
              <span>Physical trigger</span>
              <strong>{pendingPhysicalSourceLabel} to {pendingPhysicalTargetLabel}</strong>
            </div>
            <button
              aria-label="Cancel transition"
              className="icon-button"
              title="Cancel"
              type="button"
              onClick={() => setPendingPhysicalConnection(null)}
            >
              <X size={15} aria-hidden="true" />
            </button>
          </div>
          <div className="state-trigger-event-picker" role="group" aria-label="Input event">
            {pendingPhysicalConnection.eventKinds.map((eventKind) => (
              <button
                className={pendingPhysicalConnection.eventKind === eventKind ? "active" : ""}
                key={eventKind}
                type="button"
                onClick={() => setPendingPhysicalConnection((current) => current === null ? null : { ...current, eventKind })}
              >
                {TRIGGER_EVENT_LABELS[eventKind]}
              </button>
            ))}
          </div>
          <div className="state-graph-trigger-actions">
            <button className="button secondary" type="button" onClick={() => setPendingPhysicalConnection(null)}>
              Cancel
            </button>
            <button
              className="button primary"
              type="button"
              onClick={() => {
                const pending = pendingPhysicalConnection;
                setPendingPhysicalConnection(null);
                onCreateTriggerRoute(
                  pending.sceneId,
                  pending.sourceState,
                  pending.logicalSource,
                  pending.eventKind,
                  pending.target,
                );
              }}
            >
              Create transition
            </button>
          </div>
        </Panel>
      )}
      {peepOSTriggerStateId !== null && pendingPhysicalConnection === null && (
        <Panel
          position="top-center"
          className="state-graph-trigger-panel peepos-trigger-panel"
          onClick={(event) => event.stopPropagation()}
        >
          <div className="state-graph-trigger-heading">
            <div>
              <span>PeepOS triggers</span>
              <strong>{peepOSTriggerStateLabel}</strong>
            </div>
            <button
              aria-label="Close PeepOS triggers"
              className="icon-button"
              title="Close"
              type="button"
              onClick={() => setPeepOSTriggerStateId(null)}
            >
              <X size={15} aria-hidden="true" />
            </button>
          </div>
          <div className="peepos-trigger-list">
            {timerTypes.map(eventType => <button key={eventType} type="button" onClick={() => {
              onRequestTimer?.(peepOSTriggerStateId, eventType); setPeepOSTriggerStateId(null);
            }}><Clock size={16} /><span><strong>{eventType === "time.scene_elapsed" ? "Scene timer" : "State-entry timer"}</strong></span></button>)}
            {peepOSTriggers.filter(trigger => trigger.kind !== "delay_elapsed" || timerTypes.length === 0).map((trigger) => (
              <button
                disabled
                key={trigger.kind}
                title={`${trigger.detail}. Requires ${trigger.requires.join(", ")}.`}
                type="button"
              >
                <PeepOSTriggerGlyph kind={trigger.kind} />
                <span>
                  <strong>{trigger.label}</strong>
                  <small>{trigger.detail}</small>
                </span>
                <em>Not exposed</em>
              </button>
            ))}
          </div>
        </Panel>
      )}
      <Panel position="top-left" className="state-graph-toolbar">
        <button
          className="button secondary"
          disabled={!canCreateState}
          title="Add state"
          type="button"
          onClick={() => {
            const stateNodes = nodes.flatMap((node) => graphNodeById.has(node.id)
              ? [{ id: node.id, x: node.position.x, y: node.position.y }]
              : []);
            const position = nextStateGraphNodePosition(
              stateNodes,
              selected.kind === "state" ? selected.id : undefined,
            );
            setPendingPhysicalConnection(null);
            setPeepOSTriggerStateId(null);
            onCreateState(scene.scene_id, position.x, position.y);
          }}
        >
          <Plus size={14} aria-hidden="true" />
          Add state
        </button>
        {!graph.endpoints.some((endpoint) => endpoint.kind === "system") && (
          <button
            className="button secondary"
            disabled={!canEdit}
            title="Place Exit to PeepOS"
            type="button"
            onClick={() => {
              const stateNodes = nodes.flatMap((node) => graphNodeById.has(node.id)
                ? [{ id: node.id, x: node.position.x, y: node.position.y }]
                : []);
              const position = nextStateGraphNodePosition(
                stateNodes,
                selected.kind === "state" ? selected.id : undefined,
              );
              setPendingPhysicalConnection(null);
              setPeepOSTriggerStateId(null);
              onSelect({ kind: "systemExit" });
              onMoveStateNode(scene.scene_id, "system-exit", position.x, position.y);
            }}
          >
            <LogOut size={14} aria-hidden="true" />
            Exit to PeepOS
          </button>
        )}
      </Panel>
      {selectedTransition !== null && (
        <Panel position="top-right" className="graph-selection-summary">
          <span>Selected transition</span>
          <strong>{selectedTransition.trigger}</strong>
          <small>
            {selectedTransition.from} to {selectedTransition.target} {selectedTransition.targetKind}
          </small>
          <div>
            <span>{selectedTransition.conditions} condition{selectedTransition.conditions === 1 ? "" : "s"}</span>
            <span>{selectedTransition.effects} effect{selectedTransition.effects === 1 ? "" : "s"}</span>
          </div>
        </Panel>
      )}
      <Panel position="top-left" className="scene-flow-debug state-graph-status">
        <details>
          <summary>
            <span>Logic Layout</span>
          </summary>
          <pre>{layoutStatus}</pre>
        </details>
      </Panel>
      <Controls showInteractive={false} />
    </ReactFlow>
  );
}

// Keep the optional default stable: it participates in node/edge memoization.
const NO_READ_ONLY_SCENES: string[] = [];

export function SceneFlowView({
  scenes,
  entrySceneId,
  thumbnails,
  editor,
  layoutStatus,
  selectedSceneId,
  selectedSceneExitId,
  selectedRouteId,
  selectedReferenceId,
  packageEntrySelected,
  onAddScene,
  onSelectScene,
  onOpenScene,
  onSelectBackground,
  onSelectSceneExit,
  onSelectSceneRoute,
  onSelectPackageEntry,
  onSelectSceneReference,
  onAddSceneExit,
  onAddSceneReference,
  onDeleteSceneExit,
  onDeleteLegacyRoute,
  onDeleteSceneReference,
  onMoveSceneNode,
  onMovePackageEntry,
  onMoveSceneReference,
  onSetRouteLayout,
  onSetEntryScene,
  onSetSceneExitTarget,
  onConnectSceneExit,
  canEdit,
  canAddScene,
  readOnlySceneIds = NO_READ_ONLY_SCENES,
}: {
  scenes: SceneDocument[];
  entrySceneId: string | null;
  thumbnails: Record<string, Framebuffer>;
  editor?: ProjectEditorData;
  layoutStatus: string;
  selectedSceneId: string | null;
  selectedSceneExitId: string | null;
  selectedRouteId: string | null;
  selectedReferenceId: string | null;
  packageEntrySelected: boolean;
  onAddScene: (displayName: string) => void;
  onSelectScene: (sceneId: string) => void;
  onOpenScene?: (sceneId: string) => void;
  onSelectBackground?: () => void;
  onSelectSceneExit: (sceneId: string, sceneExitId: string) => void;
  onSelectSceneRoute: (sceneId: string, routeId: string) => void;
  onSelectPackageEntry: () => void;
  onSelectSceneReference: (referenceId: string, targetScene: string) => void;
  onAddSceneExit: (sceneId: string, targetScene: string, referenceId?: string) => void;
  onAddSceneReference: (targetScene: string, x: number, y: number) => void;
  onDeleteSceneExit: (sceneId: string, sceneExitId: string) => void;
  onDeleteLegacyRoute: (sceneId: string, routeId: string) => void;
  onDeleteSceneReference: (referenceId: string) => void;
  onMoveSceneNode: (sceneId: string, x: number, y: number) => void;
  onMovePackageEntry: (x: number, y: number) => void;
  onMoveSceneReference: (referenceId: string, x: number, y: number) => void;
  onSetRouteLayout: (
    sceneId: string,
    endpointKind: "scene_exit" | "route",
    endpointId: string,
    rails: EditorRouteRail[],
  ) => void;
  onSetEntryScene: (sceneId: string) => void;
  onSetSceneExitTarget: (sceneId: string, sceneExitId: string, targetScene: string, referenceId?: string) => void;
  onConnectSceneExit: (sceneId: string, routeId: string, targetScene: string, referenceId?: string) => void;
  canEdit: boolean;
  canAddScene: boolean;
  readOnlySceneIds?: string[];
}) {
  const graph = useMemo(() => buildSceneFlowGraphModel(scenes, entrySceneId, editor), [editor, entrySceneId, scenes]);
  const flowRef = useRef<ReactFlowInstance | null>(null);
  const didInitialFit = useRef(false);
  const [viewportText, setViewportText] = useState("viewport not ready");
  const [lastDragText, setLastDragText] = useState("No drag yet");
  const [lastConnectText, setLastConnectText] = useState("No connect yet");
  const [paletteTool, setPaletteTool] = useState<"scene" | "reference" | null>(null);
  const [newSceneName, setNewSceneName] = useState("");
  const baseNodes: Node[] = useMemo(
    () => {
      const activeEntrySceneIds = new Set(graph.edges.map((edge) => edge.targetScene));
      if (graph.packageEntry !== undefined) {
        activeEntrySceneIds.add(graph.packageEntry.targetScene);
      }
      const sceneNodes = graph.nodes.map((node) => ({
        id: node.id,
        type: "sceneCard",
        position: { x: node.x, y: node.y },
        data: {
          graphNode: node,
          entryActive: activeEntrySceneIds.has(node.id),
          thumbnail: thumbnails[node.id] ?? null,
          targetScenes: scenes.filter((scene) => scene.scene_type === "STATE_SCENE" && scene.scene_id !== node.id),
          selectedSceneExitId,
          selectedRouteId,
          canEdit: canEdit && !readOnlySceneIds.includes(node.id),
          onSelectScene,
          onSelectSceneExit,
          onSelectSceneRoute,
          onDeleteSceneExit,
          onDeleteLegacyRoute,
        },
        selected: selectedSceneId === node.id,
        draggable: canEdit && !readOnlySceneIds.includes(node.id),
        connectable: canEdit && !readOnlySceneIds.includes(node.id),
      }));
      const referenceNodes = graph.references.map((reference) => ({
        id: reference.id,
        type: "sceneReference",
        position: { x: reference.x, y: reference.y },
        data: {
          graphNode: reference,
          canEdit,
          onSelect: onSelectSceneReference,
        },
        selected: selectedReferenceId === reference.id,
        draggable: canEdit,
        connectable: canEdit,
      }));
      const packageNodes = graph.packageEntry === undefined ? [] : [{
        id: graph.packageEntry.id,
        type: "packageEntry",
        position: { x: graph.packageEntry.x, y: graph.packageEntry.y },
        data: {
          graphNode: graph.packageEntry,
          canEdit,
          onSelect: onSelectPackageEntry,
        },
        selected: packageEntrySelected,
        draggable: canEdit,
        connectable: canEdit,
      }];
      return [...packageNodes, ...sceneNodes, ...referenceNodes];
    },
    [canEdit, readOnlySceneIds, graph.nodes, graph.packageEntry, graph.references, onDeleteLegacyRoute, onDeleteSceneExit, onSelectPackageEntry, onSelectScene, onSelectSceneExit, onSelectSceneReference, onSelectSceneRoute, packageEntrySelected, scenes, selectedReferenceId, selectedRouteId, selectedSceneExitId, selectedSceneId, thumbnails],
  );
  const [nodes, setNodes] = useState<Node[]>(baseNodes);
  useEffect(() => {
    setNodes((current) => {
      if (sameNodeSet(current, baseNodes)) {
        const currentById = new Map(current.map((node) => [node.id, node]));
        return baseNodes.map((node) => {
          const currentNode = currentById.get(node.id);
          if (currentNode === undefined) {
            return node;
          }
          return {
            ...currentNode,
            type: node.type,
            data: node.data,
            selected: node.selected,
            draggable: node.draggable,
            connectable: node.connectable,
            position: currentNode.position,
          };
        });
      }
      return baseNodes;
    });
  }, [baseNodes]);
  const onNodesChange = (changes: NodeChange[]) => {
    const layoutChanges = changes.filter((change) => change.type !== "select");
    if (layoutChanges.length > 0) {
      setNodes((current) => applyNodeChanges(layoutChanges, current));
    }
  };
  const onNodeDragStop = (node: Node) => {
    const x = Math.round(node.position.x);
    const y = Math.round(node.position.y);
    setLastDragText(`${node.id} dropped @ ${x}, ${y}`);
    setNodes((current) =>
      current.map((item) => (item.id === node.id ? { ...item, position: { x, y } } : item)),
    );
    if (node.type === "packageEntry") {
      onMovePackageEntry(x, y);
    } else if (node.type === "sceneReference") {
      onMoveSceneReference(node.id, x, y);
    } else {
      onMoveSceneNode(node.id, x, y);
    }
  };
  const edges: Edge[] = useMemo(
    () => {
      const sceneEdges: Edge[] = graph.edges.map((edge) => ({
        id: edge.id,
        source: edge.source,
        target: edge.target,
        sourceHandle: edge.sceneExit.id,
        targetHandle: edge.referenceId === undefined ? "entry" : "reference-entry",
        type: "sceneTransition",
        selected: edge.sceneExit.sceneExitId !== undefined
          ? selectedSceneExitId === edge.sceneExit.sceneExitId
          : selectedRouteId === edge.sceneExit.routeId,
        data: {
          tone: "blue",
          sourceSceneId: edge.source,
          endpointKind: edge.sceneExit.endpointKind,
          endpointId: edge.sceneExit.endpointId,
          rails: editor?.scene_flow?.routes?.[edge.source]?.[
            `${edge.sceneExit.endpointKind}:${edge.sceneExit.endpointId}`
          ]?.rails ?? [],
          canEdit: canEdit && !readOnlySceneIds.includes(edge.source),
          onSelect: () => {
            if (edge.sceneExit.sceneExitId !== undefined) {
              onSelectSceneExit(edge.source, edge.sceneExit.sceneExitId);
            } else if (edge.sceneExit.routeId !== undefined) {
              onSelectSceneRoute(edge.source, edge.sceneExit.routeId);
            }
          },
          onSetRouteLayout: (rails: EditorRouteRail[]) => {
            onSetRouteLayout(
              edge.source,
              edge.sceneExit.endpointKind,
              edge.sceneExit.endpointId,
              rails,
            );
          },
          route_id: edge.sceneExit.routeId,
          scene_exit_id: edge.sceneExit.sceneExitId,
          source_scene_id: edge.source,
          reference_id: edge.referenceId,
        },
        style: {
          strokeWidth: (edge.sceneExit.sceneExitId !== undefined
            ? selectedSceneExitId === edge.sceneExit.sceneExitId
            : selectedRouteId === edge.sceneExit.routeId) ? 4.8 : 4,
        },
      }));
      const packageEdge: Edge[] = graph.packageEntry === undefined ? [] : [{
        id: `package-entry->${graph.packageEntry.targetScene}`,
        source: graph.packageEntry.id,
        target: graph.packageEntry.targetScene,
        sourceHandle: "package-entry-out",
        targetHandle: "entry",
        type: "gradientTransition",
        selectable: true,
        selected: packageEntrySelected,
        data: {
          package_entry: true,
          tone: "green",
          onSelect: onSelectPackageEntry,
        },
        markerEnd: { type: MarkerType.ArrowClosed, color: "#2f9e44" },
        style: {
          strokeWidth: packageEntrySelected ? 4.8 : 4,
        },
      }];
      return [...packageEdge, ...sceneEdges];
    },
    [canEdit, readOnlySceneIds, editor, graph.edges, graph.packageEntry, onSelectPackageEntry, onSelectSceneExit, onSelectSceneRoute, onSetRouteLayout, packageEntrySelected, selectedRouteId, selectedSceneExitId],
  );
  const deleteSelectedSceneExit = () => {
    if (canEdit && selectedReferenceId !== null) {
      onDeleteSceneReference(selectedReferenceId);
      return;
    }
    if (!canEdit || selectedSceneId === null || readOnlySceneIds.includes(selectedSceneId)) {
      return;
    }
    const selectedExit = graph.nodes.find((node) => node.id === selectedSceneId)?.exits.find(
      (exit) => exit.sceneExitId !== undefined
        ? exit.sceneExitId === selectedSceneExitId
        : exit.routeId === selectedRouteId,
    );
    if (selectedExit === undefined) {
      return;
    }
    if (selectedExit.sceneExitId !== undefined) {
      onDeleteSceneExit(selectedSceneId, selectedExit.sceneExitId);
    } else if (selectedExit.routeId !== undefined) {
      onDeleteLegacyRoute(selectedSceneId, selectedExit.routeId);
    }
  };
  const handleSceneFlowKeyDown = (event: KeyboardEvent<Element>) => {
    if (event.key !== "Delete" && event.key !== "Backspace") {
      return;
    }
    const target = event.target as HTMLElement | null;
    if (target !== null && target.closest("input, select, textarea, button, [contenteditable='true']") !== null) {
      return;
    }
    event.preventDefault();
    deleteSelectedSceneExit();
  };
  const onConnect = (connection: Connection) => {
    const sourceScene = connection.source;
    if (!canEdit || (sourceScene !== null && readOnlySceneIds.includes(sourceScene))) return;
    const targetNode = connection.target;
    const sourceHandle = connection.sourceHandle;
    if (sourceScene === null || targetNode === null || sourceHandle === null) {
      setLastConnectText("ignored incomplete connection");
      return;
    }
    if (sourceScene === "package-entry") {
      if (!graph.nodes.some((node) => node.id === targetNode)) {
        setLastConnectText("package entry must connect to a scene");
        return;
      }
      setLastConnectText(`package entry -> ${targetNode}`);
      onSetEntryScene(targetNode);
      return;
    }
    const targetReference = graph.references.find((reference) => reference.id === targetNode);
    const targetScene = targetReference?.targetScene ?? targetNode;
    if (sourceScene === targetScene) {
      setLastConnectText(`ignored ${sourceScene} -> ${targetScene}`);
      return;
    }
    if (sourceHandle === `${sourceScene}:${NEW_SCENE_EXIT_HANDLE}`) {
      setLastConnectText(`created exit ${sourceScene} -> ${targetScene}`);
      onAddSceneExit(sourceScene, targetScene, targetReference?.id);
      return;
    }
    const sourceExit = graph.nodes.find((node) => node.id === sourceScene)?.exits.find((exit) => exit.id === sourceHandle);
    if (sourceExit?.sceneExitId !== undefined) {
      setLastConnectText(`${sourceScene}:${sourceExit.sceneExitId} -> ${targetScene}`);
      onSetSceneExitTarget(sourceScene, sourceExit.sceneExitId, targetScene, targetReference?.id);
    } else if (sourceExit?.routeId !== undefined) {
      setLastConnectText(`${sourceScene}:${sourceExit.routeId} -> ${targetScene}`);
      onConnectSceneExit(sourceScene, sourceExit.routeId, targetScene, targetReference?.id);
    }
  };
  const updateViewportText = () => {
    const viewport = flowRef.current?.getViewport();
    if (viewport === undefined) {
      setViewportText("viewport unavailable");
      return;
    }
    setViewportText(`x ${viewport.x.toFixed(1)}, y ${viewport.y.toFixed(1)}, zoom ${viewport.zoom.toFixed(3)}`);
  };
  useEffect(() => {
    updateViewportText();
  }, [nodes.length, edges.length, packageEntrySelected, selectedReferenceId, selectedSceneId, selectedRouteId, selectedSceneExitId]);
  const debugLines = [
    `nodes ${nodes.length} / graph ${graph.nodes.length + graph.references.length + (graph.packageEntry === undefined ? 0 : 1)}`,
    `edges ${edges.length} / graph ${graph.edges.length + (graph.packageEntry === undefined ? 0 : 1)}`,
    `selected scene ${selectedSceneId ?? "-"}`,
    `selected scene exit ${selectedSceneExitId ?? "-"}`,
    `selected route ${selectedRouteId ?? "-"}`,
    `selected reference ${selectedReferenceId ?? "-"}`,
    `package entry selected ${packageEntrySelected ? "yes" : "no"}`,
    `viewport ${viewportText}`,
    `last drag ${lastDragText}`,
    `last connect ${lastConnectText}`,
    `layout save ${layoutStatus}`,
    ...nodes.map((node) => {
      const measured = "measured" in node && node.measured !== undefined
        ? `${Math.round(node.measured.width ?? 0)}x${Math.round(node.measured.height ?? 0)}`
        : "unmeasured";
      return `node ${node.id} @ ${Math.round(node.position.x)}, ${Math.round(node.position.y)} ${measured}`;
    }),
    ...edges.map((edge) => `edge ${edge.id}: ${edge.source}:${edge.sourceHandle ?? "-"} -> ${edge.target}:${edge.targetHandle ?? "-"}`),
  ];
  const debugText = debugLines.join("\n");

  if (scenes.length === 0) {
    return (
      <div className="graph-empty">
        <Network size={22} aria-hidden="true" />
        <span>Open a project to inspect its scene flow.</span>
      </div>
    );
  }

  return (
    <ReactFlow
      nodes={nodes}
      edges={edges}
      edgeTypes={SCENE_EDGE_TYPES}
      nodeTypes={SCENE_NODE_TYPES}
      fitViewOptions={{ padding: 0.28, maxZoom: 1 }}
      onInit={(instance) => {
        flowRef.current = instance;
        if (!didInitialFit.current) {
          didInitialFit.current = true;
          window.requestAnimationFrame(() => {
            instance.fitView({ padding: 0.28, maxZoom: 1 });
            updateViewportText();
          });
        }
      }}
      nodesDraggable={canEdit}
      nodesConnectable={canEdit}
      edgesReconnectable={false}
      elementsSelectable
      onNodesChange={onNodesChange}
      onConnect={onConnect}
      onNodeDragStop={(_, node) => onNodeDragStop(node)}
      onMoveEnd={updateViewportText}
      onPaneClick={() => {
        setPaletteTool(null);
        onSelectBackground?.();
      }}
      onKeyDown={handleSceneFlowKeyDown}
      zoomOnDoubleClick={false}
      onNodeDoubleClick={(_, node) => {
        if (scenes.some((scene) => scene.scene_id === node.id)) onOpenScene?.(node.id);
      }}
      tabIndex={0}
      onNodeClick={(_, node) => {
        if (node.type === "packageEntry") {
          onSelectPackageEntry();
        } else if (node.type === "sceneReference") {
          const reference = graph.references.find((item) => item.id === node.id);
          if (reference !== undefined) {
            onSelectSceneReference(reference.id, reference.targetScene);
          }
        } else {
          onSelectScene(node.id);
        }
      }}
      onEdgeClick={(event, edge) => {
        (event.currentTarget as HTMLElement).focus();
        if (edge.data?.package_entry === true) {
          onSelectPackageEntry();
          return;
        }
        const sourceScene = String(edge.data?.source_scene_id ?? "");
        if (typeof edge.data?.scene_exit_id === "string") {
          onSelectSceneExit(sourceScene, edge.data.scene_exit_id);
        } else if (typeof edge.data?.route_id === "string") {
          onSelectSceneRoute(sourceScene, edge.data.route_id);
        }
      }}
      proOptions={{ hideAttribution: true }}
    >
      <Background gap={18} size={1} />
      <GraphMiniMap
        nodes={[
          ...(graph.packageEntry === undefined ? [] : [graph.packageEntry]),
          ...graph.nodes,
          ...graph.references,
        ]}
        edges={[
          ...(graph.packageEntry === undefined ? [] : [{ source: graph.packageEntry.id, target: graph.packageEntry.targetScene }]),
          ...graph.edges,
        ]}
        selectedId={packageEntrySelected ? "package-entry" : selectedReferenceId ?? selectedSceneId}
      />
      <Panel position="top-left" className="scene-flow-palette-panel">
        <div className="scene-flow-tool-palette" role="toolbar" aria-label="Scene Flow tools">
          <button
            className={paletteTool === "scene" ? "active" : ""}
            type="button"
            disabled={!canAddScene}
            title="New scene"
            aria-label="New scene"
            onClick={() => setPaletteTool((current) => current === "scene" ? null : "scene")}
          >
            <Plus size={18} aria-hidden="true" />
          </button>
          <button
            className={paletteTool === "reference" ? "active" : ""}
            type="button"
            disabled={!canEdit}
            title="Add Go To reference"
            aria-label="Add Go To reference"
            onClick={() => setPaletteTool((current) => current === "reference" ? null : "reference")}
          >
            <ExternalLink size={17} aria-hidden="true" />
          </button>
        </div>
        {paletteTool === "scene" && (
          <form
            className="scene-flow-tool-popover scene-flow-new-scene-form"
            onSubmit={(event) => {
              event.preventDefault();
              const displayName = newSceneName.trim();
              if (displayName.length === 0) {
                return;
              }
              onAddScene(displayName);
              setNewSceneName("");
              setPaletteTool(null);
            }}
          >
            <input
              autoFocus
              aria-label="New scene name"
              value={newSceneName}
              maxLength={96}
              placeholder="Scene name"
              onChange={(event) => setNewSceneName(event.target.value)}
              onKeyDown={(event) => {
                if (event.key === "Escape") {
                  event.stopPropagation();
                  setNewSceneName("");
                  setPaletteTool(null);
                }
              }}
            />
            <button type="submit" className="icon-button" disabled={newSceneName.trim().length === 0} title="Create scene">
              <Check size={16} aria-hidden="true" />
            </button>
            <button
              type="button"
              className="icon-button"
              title="Cancel"
              onClick={() => {
                setNewSceneName("");
                setPaletteTool(null);
              }}
            >
              <X size={16} aria-hidden="true" />
            </button>
          </form>
        )}
        {paletteTool === "reference" && (
          <div className="scene-flow-tool-popover scene-flow-reference-picker">
            {scenes.filter((scene) => scene.scene_type === "STATE_SCENE").map((scene) => (
              <button
                type="button"
                key={scene.scene_id}
                onClick={(event) => {
                  const bounds = event.currentTarget.closest(".react-flow")?.getBoundingClientRect();
                  const instance = flowRef.current;
                  if (bounds === undefined || instance === null) {
                    return;
                  }
                  const point = instance.screenToFlowPosition({
                    x: bounds.left + bounds.width / 2,
                    y: bounds.top + bounds.height / 2,
                  });
                  onAddSceneReference(scene.scene_id, Math.round(point.x - 90), Math.round(point.y - 35));
                  setPaletteTool(null);
                }}
              >
                {scene.display_name}
              </button>
            ))}
          </div>
        )}
      </Panel>
      <Panel position="bottom-right" className="scene-flow-debug">
        <details>
          <summary>
            <span>Scene Flow Debug</span>
            <button
              type="button"
              onClick={(event) => {
                event.preventDefault();
                event.stopPropagation();
                void navigator.clipboard.writeText(debugText);
              }}
            >
              Copy
            </button>
          </summary>
          <pre>{debugText}</pre>
        </details>
      </Panel>
      <Controls showInteractive={false} />
    </ReactFlow>
  );
}

export function SceneFlowInspector({
  canRenameScene,
  scene,
  scenes,
  editor,
  entrySceneId,
  selection,
  onSelect,
  onRenameScene,
  onSetSceneExitTarget,
  onSetLegacyRouteTarget,
  onSetReferenceTarget,
  onDeleteReference,
  canEdit,
}: {
  scene: SceneDocument | null;
  scenes: SceneDocument[];
  editor?: ProjectEditorData;
  entrySceneId: string | null;
  selection: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onRenameScene: (sceneId: string, displayName: string) => Promise<void>;
  onSetSceneExitTarget: (sceneId: string, sceneExitId: string, targetScene: string) => Promise<void>;
  onSetLegacyRouteTarget: (sceneId: string, routeId: string, targetScene: string) => Promise<void>;
  onSetReferenceTarget: (referenceId: string, targetScene: string) => Promise<void>;
  onDeleteReference: (referenceId: string) => Promise<void>;
  canEdit: boolean;
  canRenameScene?: boolean;
}) {
  if (selection.kind === "packageEntry") {
    const entryScene = scenes.find((item) => item.scene_id === entrySceneId);
    return (
      <section className="inspector-section selected-record">
        <h3><Play size={14} aria-hidden="true" /> Package entry</h3>
        <div className="compact-grid">
          <div>
            <span>Connected scene</span>
            <strong>{entryScene?.display_name ?? "Not connected"}</strong>
          </div>
          <div>
            <span>Output</span>
            <strong>1 scene link</strong>
          </div>
        </div>
      </section>
    );
  }

  if (selection.kind === "sceneReference") {
    const reference = editor?.scene_flow?.references?.[selection.id];
    if (reference === undefined) {
      return <EmptyInspector>Select a Go To reference to inspect it.</EmptyInspector>;
    }
    const attachedCount = Object.values(editor?.scene_flow?.exit_references ?? {})
      .reduce((total, group) => total + Object.values(group).filter((value) => value === selection.id).length, 0);
    const attachedSources = new Set(
      Object.entries(editor?.scene_flow?.exit_references ?? {})
        .filter(([, group]) => Object.values(group).some((value) => value === selection.id))
        .map(([sceneId]) => sceneId),
    );
    return (
      <section className="inspector-section selected-record">
        <h3><ExternalLink size={14} aria-hidden="true" /> Go to</h3>
        <label className="select-field" htmlFor={`scene-reference-target-${selection.id}`}>
          Destination scene
          <select
            id={`scene-reference-target-${selection.id}`}
            value={reference.target_scene}
            disabled={!canEdit}
            onChange={(event) => void onSetReferenceTarget(selection.id, event.target.value)}
          >
            {scenes
              .filter((item) => item.scene_type === "STATE_SCENE" && !attachedSources.has(item.scene_id))
              .map((item) => (
              <option key={item.scene_id} value={item.scene_id}>{item.display_name}</option>
              ))}
          </select>
        </label>
        <div className="compact-grid">
          <div>
            <span>Connected exits</span>
            <strong>{attachedCount}</strong>
          </div>
          <div>
            <span>Reference ID</span>
            <strong>{selection.id}</strong>
          </div>
        </div>
        <button
          className="button state-delete-button"
          type="button"
          disabled={!canEdit}
          onClick={() => void onDeleteReference(selection.id)}
        >
          <Trash2 size={14} aria-hidden="true" />
          Delete reference
        </button>
      </section>
    );
  }

  if (selection.kind === "sceneExit" && scene !== null) {
    const sceneExit = (scene.scene_exits ?? []).find((item) => item.scene_exit_id === selection.id);
    if (sceneExit !== undefined) {
      return (
        <SceneExitInspector
          scene={scene}
          scenes={scenes}
          sceneExit={sceneExit}
          onSetSceneExitTarget={onSetSceneExitTarget}
          canEdit={canEdit}
        />
      );
    }
  }

  if (selection.kind === "route" && scene !== null) {
    const route = (scene.routes ?? []).find((item) => item.route_id === selection.id);
    if (route?.target_scene !== undefined) {
      return (
        <section className="inspector-section selected-record">
          <h3><ExternalLink size={14} aria-hidden="true" /> Scene transition</h3>
          <label className="select-field" htmlFor={`legacy-scene-target-${route.route_id}`}>
            Destination scene
            <select
              id={`legacy-scene-target-${route.route_id}`}
              value={route.target_scene}
              disabled={!canEdit}
              onChange={(event) => void onSetLegacyRouteTarget(scene.scene_id, route.route_id, event.target.value)}
            >
              {scenes.filter((item) => item.scene_id !== scene.scene_id).map((item) => (
                <option key={item.scene_id} value={item.scene_id}>{item.display_name}</option>
              ))}
            </select>
          </label>
          <div className="internal-ref-note">Legacy route ID: <code>{route.route_id}</code></div>
        </section>
      );
    }
  }

  return (
    <SceneNodeInspector
      scene={scene}
      scenes={scenes}
      entrySceneId={entrySceneId}
      onSelect={onSelect}
      onRenameScene={onRenameScene}
      canEdit={canRenameScene ?? canEdit}
    />
  );
}

function SceneNodeInspector({
  scene,
  scenes,
  entrySceneId,
  onSelect,
  onRenameScene,
  canEdit,
}: {
  scene: SceneDocument | null;
  scenes: SceneDocument[];
  entrySceneId: string | null;
  onSelect: (selection: SceneSelection) => void;
  onRenameScene: (sceneId: string, displayName: string) => Promise<void>;
  canEdit: boolean;
}) {
  const [displayName, setDisplayName] = useState(scene?.display_name ?? "");
  useEffect(() => {
    setDisplayName(scene?.display_name ?? "");
  }, [scene?.display_name, scene?.scene_id]);

  if (scene === null) {
    return (
      <section className="inspector-section">
        <h3><Network size={14} aria-hidden="true" /> Scene</h3>
        <EmptyInspector>Select a scene node to inspect it.</EmptyInspector>
      </section>
    );
  }

  const trimmed = displayName.trim();
  const renameDisabled = !canEdit || trimmed.length === 0 || trimmed === scene.display_name;
  const entryState = (scene.states ?? []).find((state) => state.state_id === scene.entry_state);
  return (
    <section className="inspector-section selected-record">
      <h3><Network size={14} aria-hidden="true" /> Scene</h3>
      <form
        className="rename-form"
        onSubmit={(event) => {
          event.preventDefault();
          if (!renameDisabled) {
            void onRenameScene(scene.scene_id, trimmed);
          }
        }}
      >
        <label htmlFor={`scene-name-${scene.scene_id}`}>Scene name</label>
        <div>
          <input
            id={`scene-name-${scene.scene_id}`}
            value={displayName}
            disabled={!canEdit}
            onChange={(event) => setDisplayName(event.target.value)}
          />
          <button type="submit" disabled={renameDisabled}>Rename</button>
        </div>
      </form>
      <div className="compact-grid">
        <div>
          <span>Package entry</span>
          <strong>{scene.scene_id === entrySceneId ? "Yes" : "No"}</strong>
        </div>
        <div>
          <span>Starts at</span>
          <strong>{entryState?.display_name ?? scene.entry_state}</strong>
        </div>
        <div>
          <span>Scene type</span>
          <strong>{scene.scene_type.replace(/_/g, " ")}</strong>
        </div>
        <div>
          <span>Scene exits</span>
          <strong>{scene.scene_exits?.length ?? 0}</strong>
        </div>
      </div>
      {(scene.scene_exits ?? []).length > 0 && (
        <div className="record-list scene-inspector-exits">
          {(scene.scene_exits ?? []).map((sceneExit) => (
            <button
              key={sceneExit.scene_exit_id}
              className="record-row"
              type="button"
              onClick={() => onSelect({ kind: "sceneExit", id: sceneExit.scene_exit_id })}
            >
              <strong>{sceneExit.display_name}</strong>
              <small>Go to {scenes.find((item) => item.scene_id === sceneExit.target_scene)?.display_name ?? sceneExit.target_scene}</small>
            </button>
          ))}
        </div>
      )}
      <div className="internal-ref-note">Internal scene ID: <code>{scene.scene_id}</code></div>
    </section>
  );
}

export function SceneAuthoringInspector({
  canConnectScenes = false,
  sceneExitActionKinds = ["play_sfx"],
  timerActionKinds = [],
  onDeleteRoute,
  localCommandAllowed,
  stateCommandAllowed,
  objectActionsEditable = false,
  scene,
  scenes,
  editor,
  selection,
  onSelect,
  onPreviewState,
  onRenameState,
  onSetEntryState,
  onDeleteState,
  onSetRouteTarget,
  onSetRouteSceneTarget,
  onSetSceneExitTarget,
  onSetRouteGuard,
  onAddRouteGuard,
  onDeleteRouteGuard,
  onMoveRouteGuard,
  onSetRouteAction,
  onAddRouteAction,
  onDeleteRouteAction,
  onMoveRouteAction,
  onAddVariable,
  onUpdateVariable,
  onDeleteVariable,
  onResetRouteLayout,
  placementOwnership,
  assets,
  audioCues,
  variableLimit,
  guardLimit,
  actionLimit,
  canEdit,
  canPreview,
}: {
  scene: SceneDocument | null;
  scenes: SceneDocument[];
  editor?: ProjectEditorData;
  selection: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onPreviewState: (sceneId: string, stateId: string) => Promise<boolean>;
  onRenameState: (sceneId: string, stateId: string, displayName: string) => Promise<void>;
  onSetEntryState: (sceneId: string, stateId: string) => Promise<void>;
  onDeleteState: (sceneId: string, stateId: string) => Promise<void>;
  onSetRouteTarget: (sceneId: string, routeId: string, targetState: string) => Promise<void>;
  onSetRouteSceneTarget: (sceneId: string, routeId: string, targetScene: string, sceneExitRef?: string) => Promise<void>;
  onSetSceneExitTarget: (sceneId: string, sceneExitId: string, targetScene: string) => Promise<void>;
  onSetRouteGuard: (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    variableRef: string,
    operator: string,
    value: number,
  ) => Promise<void>;
  onAddRouteGuard: (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    guard: Record<string, unknown>,
  ) => Promise<void>;
  onDeleteRouteGuard: (sceneId: string, routeId: string, guardIndex: number) => Promise<void>;
  onMoveRouteGuard: (sceneId: string, routeId: string, guardIndex: number, targetIndex: number) => Promise<void>;
  onSetRouteAction: (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => Promise<void>;
  onAddRouteAction: (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => Promise<void>;
  onDeleteRouteAction: (sceneId: string, routeId: string, actionIndex: number) => Promise<void>;
  onMoveRouteAction: (sceneId: string, routeId: string, actionIndex: number, targetIndex: number) => Promise<void>;
  onAddVariable: (sceneId: string, variable: StateVariable) => Promise<void>;
  onUpdateVariable: (sceneId: string, variable: StateVariable) => Promise<void>;
  onDeleteVariable: (sceneId: string, variableId: string) => Promise<void>;
  onResetRouteLayout: (sceneId: string, routeId: string, sourceState: string) => Promise<void>;
  placementOwnership: PlacementOwnership | null;
  assets: AssetRecord[];
  audioCues: AudioCueRecord[];
  variableLimit: number;
  guardLimit: number;
  actionLimit: number;
  canEdit: boolean;
  canPreview: boolean;
  objectActionsEditable?: boolean;
  stateCommandAllowed?: (command: string) => boolean;
  localCommandAllowed?: (command: string) => boolean;
  onDeleteRoute?: (sceneId: string, routeId: string) => Promise<void>;
  timerActionKinds?: string[];
  canConnectScenes?: boolean;
  sceneExitActionKinds?: string[];
}) {
  const variables = scene?.variables ?? [];
  const inputActions = scene?.input_actions ?? [];
  const states = scene?.states ?? [];
  const routes = scene?.routes ?? [];
  const sceneObjects = scene?.schema_version === 2;
  const renderModels = sceneObjects ? [] : scene?.render_models ?? [];
  const waitingVisuals = sceneObjects ? [] : scene?.waiting_visuals ?? [];
  const state = selection.kind === "state" ? states.find((item) => item.state_id === selection.id) ?? null : null;
  const route = selection.kind === "route" ? routes.find((item) => item.route_id === selection.id) ?? null : null;
  const sceneExit = selection.kind === "sceneExit"
    ? (scene?.scene_exits ?? []).find((item) => item.scene_exit_id === selection.id) ?? null
    : null;
  const render = selection.kind === "render" ? renderModels.find((item) => item.visual_id === selection.id) ?? null : null;
  const waiting = selection.kind === "waiting" ? waitingVisuals.find((item) => item.waiting_visual_id === selection.id) ?? null : null;
  const systemExitRouteCount = routes.filter((item) =>
    item.actions.some((action) => action.kind === "exit_to_shell"),
  ).length;

  return (
    <>
      {state !== null && scene !== null && (
        <StateInspector
          sceneObjectCount={sceneObjects ? scene.objects?.length ?? 0 : undefined}
          canRename={stateCommandAllowed?.("state.rename")}
          canSetEntry={stateCommandAllowed?.("state.set_entry")}
          canDelete={stateCommandAllowed?.("state.delete")}
          sceneId={scene.scene_id}
          state={state}
          isEntry={scene.entry_state === state.state_id}
          renderModels={renderModels}
          waitingVisuals={waitingVisuals}
          onSelect={onSelect}
          onPreviewState={onPreviewState}
          onRenameState={onRenameState}
          onSetEntryState={onSetEntryState}
          onDeleteState={onDeleteState}
          canEdit={canEdit}
          canPreview={canPreview}
        />
      )}
      {route !== null && scene !== null && (
        <RouteInspector
          canConnectScenes={canConnectScenes}
          sceneExitActionKinds={sceneExitActionKinds}
          timerActionKinds={timerActionKinds}
          onDeleteRoute={onDeleteRoute}
          localCommandAllowed={localCommandAllowed}
          objectActionsEditable={objectActionsEditable}
          sceneId={scene.scene_id}
          route={route}
          sourceState={selection.kind === "route" ? selection.sourceState : undefined}
          hasManualRoute={selection.kind === "route" && selection.sourceState !== undefined
            && ((editor?.state_graph?.scenes?.[scene.scene_id]?.routes?.[route.route_id]?.sources?.[selection.sourceState]?.rails?.length ?? 0) > 0
              || editor?.state_graph?.scenes?.[scene.scene_id]?.routes?.[route.route_id]?.sources?.[selection.sourceState]?.target_handle !== undefined)}
          states={states}
          scenes={scenes}
          sceneExits={scene.scene_exits ?? []}
          inputActions={inputActions}
          variables={variables}
          renderModels={renderModels}
          waitingVisuals={waitingVisuals}
          placementOwnership={placementOwnership}
          assets={assets}
          audioCues={audioCues}
          guardLimit={guardLimit}
          actionLimit={actionLimit}
          onSetRouteTarget={onSetRouteTarget}
          onSetRouteSceneTarget={onSetRouteSceneTarget}
          onSetRouteGuard={onSetRouteGuard}
          onAddRouteGuard={onAddRouteGuard}
          onDeleteRouteGuard={onDeleteRouteGuard}
          onMoveRouteGuard={onMoveRouteGuard}
          onSetRouteAction={onSetRouteAction}
          onAddRouteAction={onAddRouteAction}
          onDeleteRouteAction={onDeleteRouteAction}
          onMoveRouteAction={onMoveRouteAction}
          onResetRouteLayout={onResetRouteLayout}
          canEdit={canEdit}
        />
      )}
      {sceneExit !== null && scene !== null && (
        <SceneExitInspector
          scene={scene}
          scenes={scenes}
          sceneExit={sceneExit}
          onSetSceneExitTarget={onSetSceneExitTarget}
          canEdit={canConnectScenes || canEdit}
        />
      )}
      {render !== null && <RenderInspector render={render} />}
      {waiting !== null && <WaitingInspector waiting={waiting} />}
      {selection.kind === "systemExit" && (
        <section className="inspector-section selected-record">
          <h3><LogOut size={14} aria-hidden="true" /> Exit to PeepOS</h3>
          <div className="compact-grid">
            <div>
              <span>Destination</span>
              <strong>System shell</strong>
            </div>
            <div>
              <span>Transitions</span>
              <strong>{systemExitRouteCount}</strong>
            </div>
          </div>
        </section>
      )}

      {selection.kind === "scene" && (
        <SceneOverview
          scene={scene}
          states={states}
          routes={routes}
          variables={variables}
          renderModels={renderModels}
          waitingVisuals={waitingVisuals}
          onSelect={onSelect}
          variableLimit={variableLimit}
          canEdit={localCommandAllowed ? ["variable.add", "variable.update", "variable.delete"].every(localCommandAllowed) : canEdit}
          onAddVariable={onAddVariable}
          onUpdateVariable={onUpdateVariable}
          onDeleteVariable={onDeleteVariable}
        />
      )}
    </>
  );
}

function SceneOverview({
  scene,
  states,
  routes,
  variables,
  renderModels,
  waitingVisuals,
  onSelect,
  variableLimit,
  canEdit,
  onAddVariable,
  onUpdateVariable,
  onDeleteVariable,
}: {
  scene: SceneDocument | null;
  states: StateRecord[];
  routes: StateRoute[];
  variables: StateVariable[];
  renderModels: RenderModel[];
  waitingVisuals: WaitingVisual[];
  onSelect: (selection: SceneSelection) => void;
  variableLimit: number;
  canEdit: boolean;
  onAddVariable: (sceneId: string, variable: StateVariable) => Promise<void>;
  onUpdateVariable: (sceneId: string, variable: StateVariable) => Promise<void>;
  onDeleteVariable: (sceneId: string, variableId: string) => Promise<void>;
}) {
  if (scene === null) {
    return (
      <section className="inspector-section">
        <h3><GitBranch size={14} aria-hidden="true" /> Scene overview</h3>
        <EmptyInspector>Select a scene to inspect its states and transitions.</EmptyInspector>
      </section>
    );
  }
  const entryState = states.find((state) => state.state_id === scene.entry_state);
  const sceneObjects = scene.schema_version === 2;
  const elementCount = sceneObjects ? scene.objects?.length ?? 0
    : renderModels.reduce((total, item) => total + item.elements.length, 0);
  return (
    <>
      <section className="inspector-section">
        <h3><GitBranch size={14} aria-hidden="true" /> Scene overview</h3>
        <div className="scene-overview-card">
          <div>
            <span>Scene</span>
            <strong>{scene.display_name}</strong>
          </div>
          <div>
            <span>Starts at</span>
            <strong>{entryState?.display_name ?? scene.entry_state}</strong>
          </div>
          <div>
            <span>States</span>
            <strong>{states.length}</strong>
          </div>
          <div>
            <span>Transitions</span>
            <strong>{routes.length}</strong>
          </div>
          <div>
            <span>Variables</span>
            <strong>{variables.length}</strong>
          </div>
          <div>
            <span>{sceneObjects ? "Scene objects" : "Screen items"}</span>
            <strong>{elementCount}</strong>
          </div>
        </div>
        {!sceneObjects && <p className="scene-overview-prompt">Select a state or transition in the graph to edit it.</p>}
      </section>

      <section className="inspector-section">
        <h3><Variable size={14} aria-hidden="true" /> Variables</h3>
        <VariableEditorList
          sceneId={scene.scene_id}
          variables={variables}
          variableLimit={variableLimit}
          canEdit={canEdit}
          onAddVariable={onAddVariable}
          onUpdateVariable={onUpdateVariable}
          onDeleteVariable={onDeleteVariable}
        />
      </section>

      <section className="inspector-section">
        <h3><Route size={14} aria-hidden="true" /> Transitions</h3>
        {routes.length === 0 ? (
          <EmptyInspector>No transitions in this scene.</EmptyInspector>
        ) : (
          <div className="record-list">
            {routes.map((item: StateRoute) => (
              <button key={item.route_id} className="record-row" type="button" onClick={() => onSelect({ kind: "route", id: item.route_id })}>
                <strong>
                  {item.from_states.join(", ")} {"->"} {item.actions.some((action) => action.kind === "exit_to_shell")
                    ? "PeepOS"
                    : item.target_scene === undefined ? item.target_state : `scene:${item.target_scene}`}
                </strong>
                <small>{item.guards.length === 0 ? "always allowed" : `${item.guards.length} condition${item.guards.length === 1 ? "" : "s"}`}; {visibleEffectCount(item.actions)} effect{visibleEffectCount(item.actions) === 1 ? "" : "s"}</small>
              </button>
            ))}
          </div>
        )}
      </section>

      {!sceneObjects && <section className="inspector-section">
        <h3><Layers3 size={14} aria-hidden="true" /> Screen layouts</h3>
        {renderModels.length === 0 ? (
          <EmptyInspector>No screen layouts in this scene.</EmptyInspector>
        ) : (
          <div className="record-list">
            {renderModels.map((item: RenderModel) => (
              <button key={item.visual_id} className="record-row" type="button" onClick={() => onSelect({ kind: "render", id: item.visual_id })}>
                <strong>{item.elements.length} element{item.elements.length === 1 ? "" : "s"}</strong>
                <small>{item.visual_id}</small>
              </button>
            ))}
          </div>
        )}
      </section>}

      {!sceneObjects && <section className="inspector-section">
        <h3><Hourglass size={14} aria-hidden="true" /> Waiting animations</h3>
        {waitingVisuals.length === 0 ? (
          <EmptyInspector>No waiting animations in this scene.</EmptyInspector>
        ) : (
          <div className="record-list">
            {waitingVisuals.map((item: WaitingVisual) => (
              <button key={item.waiting_visual_id} className="record-row" type="button" onClick={() => onSelect({ kind: "waiting", id: item.waiting_visual_id })}>
                <strong>{item.combined_step_count} step{item.combined_step_count === 1 ? "" : "s"}</strong>
                <small>{item.phase_quantum_ms} ms per step; {item.waiting_visual_id}</small>
              </button>
            ))}
          </div>
        )}
      </section>}
    </>
  );
}

const VARIABLE_ID_PATTERN = /^[a-z][a-z0-9_.-]{0,63}$/;
const INT32_MINIMUM = -2147483648;
const INT32_MAXIMUM = 2147483647;

function VariableEditorList({
  sceneId,
  variables,
  variableLimit,
  canEdit,
  onAddVariable,
  onUpdateVariable,
  onDeleteVariable,
}: {
  sceneId: string;
  variables: StateVariable[];
  variableLimit: number;
  canEdit: boolean;
  onAddVariable: (sceneId: string, variable: StateVariable) => Promise<void>;
  onUpdateVariable: (sceneId: string, variable: StateVariable) => Promise<void>;
  onDeleteVariable: (sceneId: string, variableId: string) => Promise<void>;
}) {
  const [adding, setAdding] = useState(false);
  const [variableId, setVariableId] = useState("counter");
  const [minimum, setMinimum] = useState(0);
  const [initial, setInitial] = useState(0);
  const [maximum, setMaximum] = useState(1);
  const atLimit = variables.length >= variableLimit;
  const duplicateId = variables.some((variable) => variable.variable_id === variableId);
  const validRange = minimum <= initial && initial <= maximum;
  const canAdd = canEdit
    && !atLimit
    && VARIABLE_ID_PATTERN.test(variableId)
    && !duplicateId
    && validRange;

  useEffect(() => {
    setAdding(false);
    setVariableId("counter");
    setMinimum(0);
    setInitial(0);
    setMaximum(1);
  }, [sceneId]);

  return (
    <div className="variable-editor-list">
      {variables.length === 0 && !adding && <EmptyInspector>No variables in this scene.</EmptyInspector>}
      {variables.map((variable) => (
        <VariableEditorRow
          key={variable.variable_id}
          sceneId={sceneId}
          variable={variable}
          canEdit={canEdit}
          onUpdateVariable={onUpdateVariable}
          onDeleteVariable={onDeleteVariable}
        />
      ))}
      {adding ? (
        <form
          className="variable-editor-row variable-add-form"
          onSubmit={(event) => {
            event.preventDefault();
            if (!canAdd) {
              return;
            }
            void onAddVariable(sceneId, {
              variable_id: variableId,
              value_type: "int32",
              minimum,
              initial,
              maximum,
            }).then(() => {
              setAdding(false);
              setVariableId("counter");
              setMinimum(0);
              setInitial(0);
              setMaximum(1);
            });
          }}
        >
          <label className="variable-id-field">
            <span>Internal name</span>
            <input
              value={variableId}
              maxLength={64}
              pattern="[a-z][a-z0-9_.-]{0,63}"
              title="Use a lowercase ID beginning with a letter"
              disabled={!canEdit}
              onChange={(event) => setVariableId(event.target.value)}
            />
          </label>
          <VariableRangeFields
            minimum={minimum}
            initial={initial}
            maximum={maximum}
            disabled={!canEdit}
            onMinimum={setMinimum}
            onInitial={setInitial}
            onMaximum={setMaximum}
          />
          {!validRange && <div className="field-error">Minimum must be at or below the start value, and maximum at or above it.</div>}
          {duplicateId && <div className="field-error">That variable already exists.</div>}
          <div className="variable-form-actions">
            <button className="button secondary" type="button" onClick={() => setAdding(false)}>Cancel</button>
            <button className="button primary" type="submit" disabled={!canAdd}>Add variable</button>
          </div>
        </form>
      ) : (
        <button
          className="button secondary logic-add-button"
          type="button"
          disabled={!canEdit || atLimit}
          title={atLimit ? `This scene supports at most ${variableLimit} variables` : undefined}
          onClick={() => setAdding(true)}
        >
          <Plus size={13} aria-hidden="true" />
          Add variable
        </button>
      )}
    </div>
  );
}

function VariableEditorRow({
  sceneId,
  variable,
  canEdit,
  onUpdateVariable,
  onDeleteVariable,
}: {
  sceneId: string;
  variable: StateVariable;
  canEdit: boolean;
  onUpdateVariable: (sceneId: string, variable: StateVariable) => Promise<void>;
  onDeleteVariable: (sceneId: string, variableId: string) => Promise<void>;
}) {
  const [minimum, setMinimum] = useState(variable.minimum);
  const [initial, setInitial] = useState(variable.initial);
  const [maximum, setMaximum] = useState(variable.maximum);
  const validRange = minimum <= initial && initial <= maximum;
  const changed = minimum !== variable.minimum || initial !== variable.initial || maximum !== variable.maximum;

  useEffect(() => {
    setMinimum(variable.minimum);
    setInitial(variable.initial);
    setMaximum(variable.maximum);
  }, [variable.initial, variable.maximum, variable.minimum]);

  return (
    <form
      className="variable-editor-row"
      onSubmit={(event) => {
        event.preventDefault();
        if (canEdit && validRange && changed) {
          void onUpdateVariable(sceneId, { ...variable, minimum, initial, maximum });
        }
      }}
    >
      <div className="variable-row-heading">
        <strong>{displayVariableName(variable.variable_id)}</strong>
        <code>{variable.variable_id}</code>
        <button
          className="icon-button danger"
          type="button"
          disabled={!canEdit}
          title="Delete variable"
          aria-label={`Delete ${displayVariableName(variable.variable_id)}`}
          onClick={() => void onDeleteVariable(sceneId, variable.variable_id)}
        >
          <Trash2 size={13} aria-hidden="true" />
        </button>
      </div>
      <VariableRangeFields
        minimum={minimum}
        initial={initial}
        maximum={maximum}
        disabled={!canEdit}
        onMinimum={setMinimum}
        onInitial={setInitial}
        onMaximum={setMaximum}
      />
      {!validRange && <div className="field-error">Minimum must be at or below the start value, and maximum at or above it.</div>}
      <button className="button secondary variable-save-button" type="submit" disabled={!canEdit || !validRange || !changed}>
        Save range
      </button>
    </form>
  );
}

function VariableRangeFields({
  minimum,
  initial,
  maximum,
  disabled,
  onMinimum,
  onInitial,
  onMaximum,
}: {
  minimum: number;
  initial: number;
  maximum: number;
  disabled: boolean;
  onMinimum: (value: number) => void;
  onInitial: (value: number) => void;
  onMaximum: (value: number) => void;
}) {
  const numberField = (label: string, value: number, onChange: (value: number) => void) => (
    <label>
      <span>{label}</span>
      <input
        type="number"
        min={INT32_MINIMUM}
        max={INT32_MAXIMUM}
        step={1}
        value={value}
        disabled={disabled}
        onChange={(event) => {
          const nextValue = Number.parseInt(event.target.value, 10);
          if (Number.isFinite(nextValue)) {
            onChange(nextValue);
          }
        }}
      />
    </label>
  );
  return (
    <div className="variable-range-fields">
      {numberField("Minimum", minimum, onMinimum)}
      {numberField("Starts at", initial, onInitial)}
      {numberField("Maximum", maximum, onMaximum)}
    </div>
  );
}

function StateInspector({
  sceneObjectCount,
  sceneId,
  state,
  isEntry,
  renderModels,
  waitingVisuals,
  onSelect,
  onPreviewState,
  onRenameState,
  onSetEntryState,
  onDeleteState,
  canEdit,
  canPreview,
  canRename = canEdit,
  canSetEntry = canEdit,
  canDelete = canEdit,
}: {
  sceneId: string;
  state: StateRecord;
  sceneObjectCount?: number;
  isEntry: boolean;
  renderModels: RenderModel[];
  waitingVisuals: WaitingVisual[];
  onSelect: (selection: SceneSelection) => void;
  onPreviewState: (sceneId: string, stateId: string) => Promise<boolean>;
  onRenameState: (sceneId: string, stateId: string, displayName: string) => Promise<void>;
  onSetEntryState: (sceneId: string, stateId: string) => Promise<void>;
  onDeleteState: (sceneId: string, stateId: string) => Promise<void>;
  canEdit: boolean;
  canPreview: boolean;
  canRename?: boolean;
  canSetEntry?: boolean;
  canDelete?: boolean;
}) {
  const render = renderModels[0];
  const waiting = waitingVisuals.find((item) => item.waiting_visual_id === state.waiting_visual_ref);
  const [displayName, setDisplayName] = useState(state.display_name);
  const screenElementCount = render?.elements.length ?? 0;
  const placementOverrideCount = state.object_overrides?.length ?? state.placement_overrides?.length ?? 0;
  const waitingStepCount = waiting?.combined_step_count ?? 0;

  useEffect(() => {
    setDisplayName(state.display_name);
  }, [state.display_name, state.state_id]);

  const commitDisplayName = (value: string) => {
    const trimmed = value.trim();
    if (!canRename || trimmed.length === 0 || trimmed === state.display_name) {
      setDisplayName(state.display_name);
      return;
    }
    setDisplayName(trimmed);
    void onRenameState(sceneId, state.state_id, trimmed);
  };

  return (
    <section className="inspector-section selected-record">
      <h3>Selected state</h3>
      <div className="state-summary-card">
        <div>
          <label htmlFor={`state-name-${state.state_id}`}>Name</label>
          <input
            id={`state-name-${state.state_id}`}
            className="state-name-input"
            value={displayName}
            maxLength={64}
            disabled={!canRename}
            aria-label="State name"
            onChange={(event) => setDisplayName(event.target.value)}
            onBlur={(event) => commitDisplayName(event.currentTarget.value)}
            onKeyDown={(event) => {
              if (event.key === "Enter") {
                event.preventDefault();
                event.currentTarget.blur();
              } else if (event.key === "Escape") {
                event.preventDefault();
                event.stopPropagation();
                event.currentTarget.value = state.display_name;
                setDisplayName(state.display_name);
                event.currentTarget.blur();
              }
            }}
          />
        </div>
        <div>
          <span>Scene start</span>
          <strong>{isEntry ? "Yes" : "No"}</strong>
        </div>
        <div>
          <span>{sceneObjectCount === undefined ? "Object changes" : "Objects overridden"}</span>
          <strong>{placementOverrideCount}</strong>
        </div>
        {sceneObjectCount !== undefined && <div><span>Scene objects</span><strong>{sceneObjectCount}</strong></div>}
        {sceneObjectCount === undefined && state.waiting_visual_ref !== undefined && <div>
          <span>Waiting</span>
          <strong>{waiting === undefined ? "Not linked" : `${waitingStepCount} step${waitingStepCount === 1 ? "" : "s"}`}</strong>
        </div>}
      </div>
      {render !== undefined && (
        <button className="link-row" type="button" onClick={() => onSelect({ kind: "render", id: render.visual_id })}>
          Scene placement <strong>{screenElementCount} object{screenElementCount === 1 ? "" : "s"}</strong>
        </button>
      )}
      {sceneObjectCount === undefined && state.waiting_visual_ref !== undefined && <button className="link-row" type="button" onClick={() => {
        if (state.waiting_visual_ref !== undefined) onSelect({ kind: "waiting", id: state.waiting_visual_ref });
      }}>
        Waiting animation <strong>{waiting === undefined ? "Missing" : `${waiting.combined_step_count} step${waiting.combined_step_count === 1 ? "" : "s"}`}</strong>
      </button>}
      <div className="state-lifecycle-actions">
        <button
          className="button secondary"
          disabled={!canPreview}
          type="button"
          onClick={() => void onPreviewState(sceneId, state.state_id)}
        >
          <Eye size={14} aria-hidden="true" />
          Load in emulator
        </button>
        <button
          className={`button secondary ${isEntry ? "active" : ""}`}
          disabled={!canSetEntry || isEntry}
          type="button"
          onClick={() => void onSetEntryState(sceneId, state.state_id)}
        >
          <Play size={14} aria-hidden="true" />
          {isEntry ? "Starts scene" : "Set as start"}
        </button>
        <button
          className="button state-delete-button"
          disabled={!canDelete || isEntry}
          title="Delete selected state"
          type="button"
          onClick={() => void onDeleteState(sceneId, state.state_id)}
        >
          <Trash2 size={14} aria-hidden="true" />
          Delete state
        </button>
      </div>
      <div className="internal-ref-note">
        Internal state ID: <code>{state.state_id}</code>
      </div>
    </section>
  );
}

function SceneExitInspector({
  scene,
  scenes,
  sceneExit,
  onSetSceneExitTarget,
  canEdit,
}: {
  scene: SceneDocument;
  scenes: SceneDocument[];
  sceneExit: SceneExitRecord;
  onSetSceneExitTarget: (sceneId: string, sceneExitId: string, targetScene: string) => Promise<void>;
  canEdit: boolean;
}) {
  return (
    <section className="inspector-section selected-record">
      <h3>Scene exit</h3>
      <label className="field-block">
        <span>Destination</span>
        <select
          value={sceneExit.target_scene}
          disabled={!canEdit}
          onChange={(event) => {
            void onSetSceneExitTarget(scene.scene_id, sceneExit.scene_exit_id, event.target.value);
          }}
        >
          {scenes
            .filter((candidate) => candidate.scene_type === "STATE_SCENE" && candidate.scene_id !== scene.scene_id)
            .map((candidate) => (
              <option key={candidate.scene_id} value={candidate.scene_id}>{candidate.display_name}</option>
            ))}
        </select>
      </label>
      <div className="internal-ref-note">
        Internal scene exit ID: <code>{sceneExit.scene_exit_id}</code>
      </div>
    </section>
  );
}

function RouteInspector({
  canConnectScenes = false,
  sceneExitActionKinds = ["play_sfx"],
  timerActionKinds = [],
  onDeleteRoute,
  localCommandAllowed,
  objectActionsEditable = false,
  sceneId,
  route,
  sourceState,
  hasManualRoute,
  states,
  scenes,
  sceneExits,
  inputActions,
  variables,
  renderModels,
  waitingVisuals,
  placementOwnership,
  assets,
  audioCues,
  guardLimit,
  actionLimit,
  onSetRouteTarget,
  onSetRouteSceneTarget,
  onSetRouteGuard,
  onAddRouteGuard,
  onDeleteRouteGuard,
  onMoveRouteGuard,
  onSetRouteAction,
  onAddRouteAction,
  onDeleteRouteAction,
  onMoveRouteAction,
  onResetRouteLayout,
  canEdit,
}: {
  sceneId: string;
  route: StateRoute;
  sourceState?: string;
  canConnectScenes?: boolean;
  sceneExitActionKinds?: string[];
  hasManualRoute: boolean;
  objectActionsEditable?: boolean;
  localCommandAllowed?: (command: string) => boolean;
  onDeleteRoute?: (sceneId: string, routeId: string) => Promise<void>;
  timerActionKinds?: string[];
  states: StateRecord[];
  scenes: SceneDocument[];
  sceneExits: SceneExitRecord[];
  inputActions: InputAction[];
  variables: StateVariable[];
  renderModels: RenderModel[];
  waitingVisuals: WaitingVisual[];
  placementOwnership: PlacementOwnership | null;
  assets: AssetRecord[];
  audioCues: AudioCueRecord[];
  guardLimit: number;
  actionLimit: number;
  onSetRouteTarget: (sceneId: string, routeId: string, targetState: string) => Promise<void>;
  onSetRouteSceneTarget: (sceneId: string, routeId: string, targetScene: string, sceneExitRef?: string) => Promise<void>;
  onSetRouteGuard: (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    variableRef: string,
    operator: string,
    value: number,
  ) => Promise<void>;
  onAddRouteGuard: (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    guard: Record<string, unknown>,
  ) => Promise<void>;
  onDeleteRouteGuard: (sceneId: string, routeId: string, guardIndex: number) => Promise<void>;
  onMoveRouteGuard: (sceneId: string, routeId: string, guardIndex: number, targetIndex: number) => Promise<void>;
  onSetRouteAction: (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => Promise<void>;
  onAddRouteAction: (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => Promise<void>;
  onDeleteRouteAction: (sceneId: string, routeId: string, actionIndex: number) => Promise<void>;
  onMoveRouteAction: (sceneId: string, routeId: string, actionIndex: number, targetIndex: number) => Promise<void>;
  onResetRouteLayout: (sceneId: string, routeId: string, sourceState: string) => Promise<void>;
  canEdit: boolean;
}) {
  const input = inputActions.find((item) => item.action_id === route.action_ref);
  const fromLabels = route.from_states.map((stateId) => displayStateName(states, stateId)).join(", ");
  const exitsToPeepOS = route.actions.some((action) => action.kind === "exit_to_shell");
  const exitsScene = !exitsToPeepOS && route.target_scene !== undefined;
  const routeSceneExit = sceneExits.find((sceneExit) => sceneExit.scene_exit_id === route.scene_exit_ref)
    ?? sceneExits.find((sceneExit) => sceneExit.target_scene === route.target_scene);
  const routeTarget = exitsToPeepOS
    ? "PeepOS"
    : route.target_scene === undefined
    ? displayStateName(states, route.target_state ?? "")
    : routeSceneExit?.display_name ?? scenes.find((scene) => scene.scene_id === route.target_scene)?.display_name ?? route.target_scene;
  const targetKind = exitsToPeepOS ? "system" : route.target_scene === undefined ? "state" : "scene exit";
  const targetState = states.find((state) => state.state_id === route.target_state);
  const targetRenderModel = renderModels.find((renderModel) => renderModel.visual_id === targetState?.render_model_ref);
  const sceneObjects = scenes.find(scene => scene.scene_id === sceneId)?.schema_version === 2;
  const targetElements = targetState === undefined
    ? targetRenderModel?.elements ?? []
    : placementOwnership?.scenes[sceneId]?.states[targetState.state_id]?.resolved_elements
      ?? targetRenderModel?.elements
      ?? [];
  return (
    <section className="inspector-section selected-record">
      <h3>Selected transition</h3>
      <div className="transition-summary">
        <div>
          <span>When</span>
          <strong>{displayInputTrigger(input, route.action_ref ?? route.event_ref ?? "")}</strong>
        </div>
        <div>
          <span>From</span>
          <strong>{fromLabels}</strong>
        </div>
        <div>
          <span>Go to {targetKind}</span>
          <strong>{routeTarget}</strong>
        </div>
      </div>
      {exitsToPeepOS ? null : exitsScene ? (
        <label className="select-field" htmlFor={`route-scene-exit-${route.route_id}`}>
          Scene exit
          <select
            id={`route-scene-exit-${route.route_id}`}
            value={routeSceneExit?.scene_exit_id ?? ""}
            disabled={!(canConnectScenes || canEdit) || sceneExits.length === 0}
            onChange={(event) => {
              const sceneExit = sceneExits.find((item) => item.scene_exit_id === event.target.value);
              if (sceneExit !== undefined) {
                void onSetRouteSceneTarget(
                  sceneId,
                  route.route_id,
                  sceneExit.target_scene,
                  sceneExit.scene_exit_id,
                );
              }
            }}
          >
            {routeSceneExit === undefined && <option value="">Legacy direct scene target</option>}
            {sceneExits.map((sceneExit) => (
              <option key={sceneExit.scene_exit_id} value={sceneExit.scene_exit_id}>
                {sceneExit.display_name}
              </option>
            ))}
          </select>
        </label>
      ) : (
        <label className="select-field" htmlFor={`route-target-${route.route_id}`}>
          Go to state
          <select
            id={`route-target-${route.route_id}`}
            value={route.target_state}
            disabled={!(localCommandAllowed?.("route.set_target") ?? canEdit)}
            onChange={(event) => {
              void onSetRouteTarget(sceneId, route.route_id, event.target.value);
            }}
          >
            {states.map((state) => (
              <option key={state.state_id} value={state.state_id}>
                {state.display_name}
              </option>
            ))}
          </select>
        </label>
      )}
      {sourceState !== undefined && (
        <div className="route-layout-actions">
          <button
            className="button secondary"
            type="button"
            disabled={!(localCommandAllowed?.("editor.state_graph.set_route_layout") ?? canEdit) || !hasManualRoute}
            title="Return this transition line to automatic routing"
            onClick={() => {
              void onResetRouteLayout(sceneId, route.route_id, sourceState);
            }}
          >
            <RotateCcw size={14} aria-hidden="true" />
            Reset route
          </button>
        </div>
      )}
      <h4>Only if</h4>
      <EditableGuardList
        sceneId={sceneId}
        route={route}
        variables={variables}
        guardLimit={guardLimit}
        canEdit={localCommandAllowed ? ["route.set_guard", "route.guard.add", "route.guard.delete", "route.guard.move"].every(localCommandAllowed) : canEdit}
        onSetRouteGuard={onSetRouteGuard}
        onAddRouteGuard={onAddRouteGuard}
        onDeleteRouteGuard={onDeleteRouteGuard}
        onMoveRouteGuard={onMoveRouteGuard}
      />
      <h4>Then</h4>
      <EditableActionList
        allowedActionKinds={exitsScene ? sceneExitActionKinds : undefined}
        timers={(scenes.find(scene => scene.scene_id === sceneId)?.event_bindings ?? [])
          .filter(binding => binding.event_type === "time.scene_elapsed").map(binding => binding.binding_id)}
        timerActionKinds={timerActionKinds}
        sceneObjects={sceneObjects}
        sceneId={sceneId}
        route={route}
        variables={variables}
        targetState={targetState}
        targetElements={targetElements}
        waitingVisuals={waitingVisuals}
        assets={assets}
        audioCues={audioCues}
        localActionsAllowed={!exitsScene}
        canAddActions={route.actions.length < actionLimit}
        canEdit={sceneObjects ? objectActionsEditable : canEdit}
        onSetRouteAction={onSetRouteAction}
        onAddRouteAction={onAddRouteAction}
        onDeleteRouteAction={onDeleteRouteAction}
        onMoveRouteAction={onMoveRouteAction}
      />
      <div className="internal-ref-note">
        Internal transition ID: <code>{route.route_id}</code>
      </div>
      {onDeleteRoute && <button className="button secondary" type="button" title="Delete transition"
        disabled={!(localCommandAllowed?.("route.delete") ?? canEdit)}
        onClick={() => void onDeleteRoute(sceneId, route.route_id)}><Trash2 size={14} />Delete transition</button>}
    </section>
  );
}

export function EditableGuardList({
  sceneId,
  route,
  variables,
  guardLimit,
  canEdit,
  onSetRouteGuard,
  onAddRouteGuard,
  onDeleteRouteGuard,
  onMoveRouteGuard,
}: {
  sceneId: string;
  route: StateRoute;
  variables: StateVariable[];
  guardLimit: number;
  canEdit: boolean;
  onSetRouteGuard: (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    variableRef: string,
    operator: string,
    value: number,
  ) => Promise<void>;
  onAddRouteGuard: (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    guard: Record<string, unknown>,
  ) => Promise<void>;
  onDeleteRouteGuard: (sceneId: string, routeId: string, guardIndex: number) => Promise<void>;
  onMoveRouteGuard: (sceneId: string, routeId: string, guardIndex: number, targetIndex: number) => Promise<void>;
}) {
  const defaultVariable = variables[0];
  const canAdd = canEdit && defaultVariable !== undefined && route.guards.length < guardLimit;
  return (
    <div className="guard-editor-list">
      {route.guards.length === 0 && <div className="plain-rule-note">Always allowed.</div>}
      {route.guards.map((guard, index) => {
        const variable = variables.find((item) => item.variable_id === guard.variable_ref);
        const commit = (variableRef: string, operator: string, value: number) => {
          void onSetRouteGuard(sceneId, route.route_id, index, variableRef, operator, value);
        };
        return (
          <div className="logic-sentence-row" key={`${route.route_id}-guard-${index}`}>
            <div className="logic-row-heading">
              <span className="logic-row-index">Condition {index + 1}</span>
              <div className="logic-row-actions">
                <button
                  className="icon-button"
                  type="button"
                  disabled={!canEdit || index === 0}
                  title="Move condition earlier"
                  aria-label={`Move condition ${index + 1} earlier`}
                  onClick={() => void onMoveRouteGuard(sceneId, route.route_id, index, index - 1)}
                >
                  <ArrowUp size={13} aria-hidden="true" />
                </button>
                <button
                  className="icon-button"
                  type="button"
                  disabled={!canEdit || index === route.guards.length - 1}
                  title="Move condition later"
                  aria-label={`Move condition ${index + 1} later`}
                  onClick={() => void onMoveRouteGuard(sceneId, route.route_id, index, index + 1)}
                >
                  <ArrowDown size={13} aria-hidden="true" />
                </button>
                <button
                  className="icon-button danger"
                  type="button"
                  disabled={!canEdit}
                  title="Delete condition"
                  aria-label={`Delete condition ${index + 1}`}
                  onClick={() => void onDeleteRouteGuard(sceneId, route.route_id, index)}
                >
                  <Trash2 size={13} aria-hidden="true" />
                </button>
              </div>
            </div>
            <div className="logic-sentence">
              <span>Only if</span>
              <select
                aria-label={`Condition ${index + 1} variable`}
                value={guard.variable_ref}
                disabled={!canEdit}
                onChange={(event) => commit(event.target.value, guard.operator, guard.value)}
              >
                {variables.map((variable) => (
                  <option key={variable.variable_id} value={variable.variable_id}>
                    {displayVariableName(variable.variable_id)}
                  </option>
                ))}
              </select>
              <select
                aria-label={`Condition ${index + 1} operator`}
                value={guard.operator}
                disabled={!canEdit}
                onChange={(event) => commit(guard.variable_ref, event.target.value, guard.value)}
              >
                {GUARD_OPERATORS.map((operator) => (
                  <option key={operator} value={operator}>
                    {GUARD_OPERATOR_LABELS[operator]}
                  </option>
                ))}
              </select>
              <input
                aria-label={`Condition ${index + 1} value`}
                type="number"
                step={1}
                min={variable?.minimum}
                max={variable?.maximum}
                value={guard.value}
                disabled={!canEdit}
                onChange={(event) => {
                  const parsed = Number.parseInt(event.target.value, 10);
                  if (Number.isFinite(parsed)) {
                    commit(guard.variable_ref, guard.operator, parsed);
                  }
                }}
              />
            </div>
          </div>
        );
      })}
      <button
        className="button secondary logic-add-button"
        type="button"
        disabled={!canAdd}
        title={defaultVariable === undefined ? "Create a variable before adding a condition" : undefined}
        onClick={() => {
          if (defaultVariable !== undefined) {
            void onAddRouteGuard(sceneId, route.route_id, route.guards.length, {
              variable_ref: defaultVariable.variable_id,
              operator: "eq",
              value: defaultVariable.initial,
            });
          }
        }}
      >
        <Plus size={13} aria-hidden="true" />
        Add condition
      </button>
    </div>
  );
}

type WaitingAnimationChoice = {
  key: string;
  label: string;
  waitingVisualRef: string;
  waitingElementRef: string;
};

const EFFECT_KIND_LABELS: Record<string, string> = {
  start_timer: "Start timer",
  restart_timer: "Restart timer",
  cancel_timer: "Cancel timer",
  "object.move_by": "Move by (relative)",
  "object.set_position": "Set position (absolute)",
  "object.set_visibility": "Show or hide object",
  "object.set_frame": "Set frame override",
  "object.clear_frame": "Clear frame override",
  set_variable: "Change variable",
  set_element_visibility: "Show or hide object",
  set_element_position: "Set position",
  set_element_frame: "Change sprite frame",
  set_element_waiting_animation: "Change animation",
  play_sfx: "Play SFX",
};

function spriteFrameChoices(element: RenderElement | undefined, assets: AssetRecord[], frameRef?: string) {
  if (element?.kind !== "sprite") {
    return [];
  }
  const asset = assets.find((candidate) => candidate.frames.some((frame) => frame.frame_id === element.visual_ref))
    ?? assets.find((candidate) => candidate.frames.some((frame) => frame.frame_id === frameRef));
  return (asset?.frames ?? []).map((frame, index) => ({
    frameId: frame.frame_id,
    label: `${asset?.display_name ?? asset?.text ?? "Sprite"} - ${frame.display_name ?? `Frame ${index + 1}`}`,
  }));
}

function waitingAnimationChoices(
  elementRef: string,
  targetState: StateRecord | undefined,
  waitingVisuals: WaitingVisual[],
): WaitingAnimationChoice[] {
  const targetTimeline = waitingVisuals.find((waiting) => waiting.waiting_visual_id === targetState?.waiting_visual_ref);
  if (targetTimeline === undefined) {
    return [];
  }
  return waitingVisuals.flatMap((waiting) => {
    if (
      waiting.phase_quantum_ms !== targetTimeline.phase_quantum_ms
      || waiting.combined_step_count !== targetTimeline.combined_step_count
    ) {
      return [];
    }
    return waiting.elements
      .filter((element) => element.source_element_ref === elementRef)
      .map((element) => ({
        key: `${waiting.waiting_visual_id}:${element.element_id}`,
        label: `${waiting.waiting_visual_id} / ${element.element_id}`,
        waitingVisualRef: waiting.waiting_visual_id,
        waitingElementRef: element.element_id,
      }));
  });
}

export function EditableActionList({
  allowedActionKinds,
  timers = [],
  timerActionKinds = [],
  sceneObjects = false,
  sceneId,
  route,
  variables,
  targetState,
  targetElements,
  waitingVisuals,
  assets,
  audioCues,
  localActionsAllowed,
  canAddActions,
  canEdit,
  onSetRouteAction,
  onAddRouteAction,
  onDeleteRouteAction,
  onMoveRouteAction,
}: {
  sceneId: string;
  route: StateRoute;
  variables: StateVariable[];
  sceneObjects?: boolean;
  timers?: string[];
  timerActionKinds?: string[];
  targetState?: StateRecord;
  targetElements: RenderElement[];
  waitingVisuals: WaitingVisual[];
  assets: AssetRecord[];
  audioCues: AudioCueRecord[];
  localActionsAllowed: boolean;
  allowedActionKinds?: string[];
  canAddActions: boolean;
  canEdit: boolean;
  onSetRouteAction: (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => Promise<void>;
  onAddRouteAction: (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => Promise<void>;
  onDeleteRouteAction: (sceneId: string, routeId: string, actionIndex: number) => Promise<void>;
  onMoveRouteAction: (sceneId: string, routeId: string, actionIndex: number, targetIndex: number) => Promise<void>;
}) {
  const visibleActions = route.actions
    .map((action, actionIndex) => ({ action, actionIndex }))
    .filter(({ action }) => action.kind !== "request_render" && action.kind !== "exit_to_shell");
  const systemExitIndex = route.actions.findIndex((action) => action.kind === "exit_to_shell");
  const addActionIndex = systemExitIndex >= 0 ? systemExitIndex : route.actions.length;
  const targetElementById = new Map(targetElements.map((element) => [element.element_id, element]));
  const { label: objectLabel, scene: actionScene } = useContext(ObjectActionContext);
  const labels = targetElements.map(objectLabel);
  const objectOptionLabel = (element: RenderElement) => {
    const label = objectLabel(element);
    return labels.filter(item => item === label).length > 1 ? `${label} (${element.element_id})` : label;
  };

  const defaultActionForKind = (kind: string, preferredElementRef?: string): Record<string, unknown> | null => {
    if (timerActionKinds.includes(kind)) return timers.length ? { kind, timer_ref: timers[0] } : null;
    if (kind === "set_variable") {
      const variable = variables[0];
      return variable === undefined ? null : {
        kind,
        variable_ref: variable.variable_id,
        operation: "assign",
        value: variable.initial,
      };
    }
    if (kind === "play_sfx") {
      const cue = audioCues[0];
      return cue === undefined ? null : { kind, cue_ref: cue.cue_id };
    }
    const preferredElement = preferredElementRef === undefined ? undefined : targetElementById.get(preferredElementRef);
    if (kind.startsWith("object.")) {
      const element = preferredElement ?? targetElements.find(item =>
        !["object.set_frame", "object.clear_frame"].includes(kind) || item.kind === "sprite");
      if (element === undefined) return null;
      const common = { kind, object_ref: element.element_id };
      if (kind === "object.move_by") return { ...common, dx: 1 };
      if (kind === "object.set_position") return { ...common, x: element.x };
      if (kind === "object.set_visibility") return { ...common, visible: true };
      if (kind === "object.clear_frame") return element.kind === "sprite" ? common : null;
      const frame = spriteFrameChoices(element, assets)[0];
      return frame === undefined ? null : { ...common, frame_ref: frame.frameId };
    }
    if (kind === "set_element_visibility" || kind === "set_element_position") {
      const element = preferredElement ?? targetElements[0];
      if (element === undefined) {
        return null;
      }
      return kind === "set_element_visibility"
        ? { kind, element_ref: element.element_id, visible: true }
        : { kind, element_ref: element.element_id, x: element.x, y: element.y };
    }
    if (kind === "set_element_frame") {
      const candidates = preferredElement === undefined
        ? targetElements.filter((element) => element.kind === "sprite")
        : [preferredElement];
      for (const element of candidates) {
        const frames = spriteFrameChoices(element, assets);
        const frameRef = frames.find((frame) => frame.frameId === element.visual_ref)?.frameId ?? frames[0]?.frameId;
        if (frameRef !== undefined) {
          return { kind, element_ref: element.element_id, frame_ref: frameRef };
        }
      }
      return null;
    }
    if (kind === "set_element_waiting_animation") {
      const candidates = preferredElement === undefined ? targetElements : [preferredElement];
      for (const element of candidates) {
        const choice = waitingAnimationChoices(element.element_id, targetState, waitingVisuals)[0];
        if (choice !== undefined) {
          return {
            kind,
            element_ref: element.element_id,
            waiting_visual_ref: choice.waitingVisualRef,
            waiting_element_ref: choice.waitingElementRef,
            timeline_policy: "preserve",
          };
        }
      }
    }
    return null;
  };

  const localEffectKinds = sceneObjects ? ["set_variable", "object.move_by", "object.set_position", "object.set_visibility", "object.set_frame", "object.clear_frame"] : [
    "set_variable",
    "set_element_visibility",
    "set_element_position",
    "set_element_frame",
    "set_element_waiting_animation",
  ];
  const availableEffectKinds = [
    ...(localActionsAllowed ? [...localEffectKinds, ...timerActionKinds] : []),
    "play_sfx",
  ].filter((kind) => defaultActionForKind(kind) !== null && (allowedActionKinds === undefined || allowedActionKinds.includes(kind)));

  return (
    <div className="action-editor-list">
      {visibleActions.length === 0 && <div className="plain-rule-note">No visible effects.</div>}
      {visibleActions.map(({ action, actionIndex }, visibleIndex) => {
        const variableRef = action.variable_ref ?? variables[0]?.variable_id ?? "";
        const variable = variables.find((item) => item.variable_id === variableRef);
        const operation = action.operation === "add" ? "add" : "assign";
        const value = typeof action.value === "number" ? action.value : variable?.initial ?? 0;
        const isAdd = operation === "add";
        const cueRef = action.cue_ref ?? audioCues[0]?.cue_id ?? "";
        const isElementAction = action.kind.startsWith("set_element_") || action.kind.startsWith("object.");
        const elementRef = action.object_ref ?? action.element_ref ?? targetElements[0]?.element_id ?? "";
        const element = targetElementById.get(elementRef);
        const frames = spriteFrameChoices(element, assets, action.frame_ref);
        const animationChoices = waitingAnimationChoices(elementRef, targetState, waitingVisuals);
        const animationKey = `${action.waiting_visual_ref ?? ""}:${action.waiting_element_ref ?? ""}`;
        const kindOptions = availableEffectKinds.includes(action.kind)
          ? availableEffectKinds
          : [action.kind, ...availableEffectKinds];
        const commit = (nextAction: Record<string, unknown>) => {
          void onSetRouteAction(sceneId, route.route_id, actionIndex, nextAction);
        };
        return (
          <div className="logic-sentence-row" key={`${route.route_id}-action-${actionIndex}`}>
            <div className="logic-row-heading">
              <span className="logic-row-index">Effect {visibleIndex + 1}</span>
              <div className="logic-row-actions">
                <button
                  className="icon-button"
                  type="button"
                  disabled={!canEdit || visibleIndex === 0}
                  title="Move effect earlier"
                  aria-label={`Move effect ${visibleIndex + 1} earlier`}
                  onClick={() => void onMoveRouteAction(
                    sceneId,
                    route.route_id,
                    actionIndex,
                    visibleActions[visibleIndex - 1]!.actionIndex,
                  )}
                >
                  <ArrowUp size={13} aria-hidden="true" />
                </button>
                <button
                  className="icon-button"
                  type="button"
                  disabled={!canEdit || visibleIndex === visibleActions.length - 1}
                  title="Move effect later"
                  aria-label={`Move effect ${visibleIndex + 1} later`}
                  onClick={() => void onMoveRouteAction(
                    sceneId,
                    route.route_id,
                    actionIndex,
                    visibleActions[visibleIndex + 1]!.actionIndex,
                  )}
                >
                  <ArrowDown size={13} aria-hidden="true" />
                </button>
                <button
                  className="icon-button danger"
                  type="button"
                  disabled={!canEdit}
                  title="Delete effect"
                  aria-label={`Delete effect ${visibleIndex + 1}`}
                  onClick={() => void onDeleteRouteAction(sceneId, route.route_id, actionIndex)}
                >
                  <Trash2 size={13} aria-hidden="true" />
                </button>
              </div>
            </div>
            <div className="effect-fields">
              <div className="effect-field-group effect-field-group-main">
              <label className="effect-field">
              <span>Action</span>
              <select
                aria-label={`Effect ${visibleIndex + 1} kind`}
                value={action.kind}
                disabled={!canEdit || (!localActionsAllowed && action.kind !== "play_sfx")}
                onChange={(event) => {
                  const nextAction = defaultActionForKind(event.target.value);
                  if (nextAction !== null) {
                    commit(nextAction);
                  }
                }}
              >
                {kindOptions.map((kind) => (
                  <option key={kind} value={kind}>{EFFECT_KIND_LABELS[kind] ?? "Advanced effect"}</option>
                ))}
              </select>
              </label>
              {isElementAction && (
                <label className="effect-field">
                <span>Object</span>
                <select
                  aria-label={`Effect ${visibleIndex + 1} object`}
                  value={elementRef}
                  disabled={!canEdit || targetElements.length === 0}
                  onChange={(event) => {
                    const nextAction = defaultActionForKind(action.kind, event.target.value);
                    if (nextAction !== null) {
                      if (action.kind.startsWith("object.") && action.kind !== "object.set_frame") {
                        commit({ ...action, object_ref: event.target.value });
                        return;
                      }
                      commit(action.kind === "set_element_visibility"
                        ? { ...nextAction, visible: action.visible !== false }
                        : action.kind === "set_element_waiting_animation"
                          ? { ...nextAction, timeline_policy: action.timeline_policy === "rebase" ? "rebase" : "preserve" }
                          : nextAction);
                    }
                  }}
                >
                  {targetElements.filter(candidate => !["object.set_frame", "object.clear_frame"].includes(action.kind) || candidate.kind === "sprite").map((candidate) => (
                    <option key={candidate.element_id} value={candidate.element_id}>{objectOptionLabel(candidate)}</option>
                  ))}
                </select>
                </label>
              )}
              </div>
              {action.kind === "set_variable" && (
                <div className="effect-field-group effect-field-group-values">
                  <label className="effect-field">
                  <span>Operation</span>
                  <select
                    aria-label={`Effect ${visibleIndex + 1} operation`}
                    value={operation}
                    disabled={!canEdit}
                    onChange={(event) => commit({
                      kind: "set_variable",
                      variable_ref: variableRef,
                      operation: event.target.value,
                      value,
                    })}
                  >
                    {ACTION_OPERATIONS.map((item) => (
                      <option key={item} value={item}>{item === "assign" ? "Set value" : "Add amount"}</option>
                    ))}
                  </select>
                  </label>
                  <label className="effect-field">
                  <span>Variable</span>
                  <select
                    aria-label={`Effect ${visibleIndex + 1} variable`}
                    value={variableRef}
                    disabled={!canEdit || variables.length === 0}
                    onChange={(event) => commit({
                      kind: "set_variable",
                      variable_ref: event.target.value,
                      operation,
                      value,
                    })}
                  >
                    {variables.map((candidate) => (
                      <option key={candidate.variable_id} value={candidate.variable_id}>
                        {displayVariableName(candidate.variable_id)}
                      </option>
                    ))}
                  </select>
                  </label>
                  <label className="effect-field">
                  <span>{isAdd ? "Amount" : "Value"}</span>
                  <input
                    aria-label={isAdd ? `Effect ${visibleIndex + 1} change amount` : `Effect ${visibleIndex + 1} target value`}
                    type="number"
                    step={1}
                    min={isAdd ? undefined : variable?.minimum}
                    max={isAdd ? undefined : variable?.maximum}
                    value={value}
                    disabled={!canEdit}
                    onChange={(event) => {
                      const parsed = Number.parseInt(event.target.value, 10);
                      if (Number.isFinite(parsed)) {
                        commit({ kind: "set_variable", variable_ref: variableRef, operation, value: parsed });
                      }
                    }}
                  />
                  </label>
                </div>
              )}
              {(action.kind === "object.move_by" || action.kind === "object.set_position") && (
                <div className="effect-field-group effect-field-group-motion">
                  <ObjectMotionFields action={action} index={visibleIndex + 1} disabled={!canEdit} onCommit={commit} />
                  <ObjectActionPosition objectId={elementRef} action={action}
                    targetState={targetState ?? actionScene?.states?.find(state => state.state_id === route.target_state)} />
                </div>
              )}
              {(action.kind === "set_element_visibility" || action.kind === "object.set_visibility") && (
                <div className="effect-field-group effect-field-group-values">
                <label className="logic-toggle">
                  <input
                    type="checkbox"
                    checked={action.visible !== false}
                    disabled={!canEdit}
                    onChange={(event) => commit({
                      kind: action.kind,
                      ...(sceneObjects ? { object_ref: elementRef } : { element_ref: elementRef }),
                      visible: event.target.checked,
                    })}
                  />
                  Visible
                </label>
                </div>
              )}
              {action.kind === "set_element_position" && (
                <div className="effect-field-group effect-field-group-values effect-coordinate-fields">
                  <label className="effect-field">
                  <span>X (px from left)</span>
                  <input
                    aria-label={`Effect ${visibleIndex + 1} X position`}
                    type="number"
                    step={1}
                    min={0}
                    max={Math.max(0, 168 - (element?.width ?? 0))}
                    value={action.x ?? element?.x ?? 0}
                    disabled={!canEdit}
                    onChange={(event) => {
                      const x = Number.parseInt(event.target.value, 10);
                      if (Number.isFinite(x)) {
                        commit({ kind: action.kind, element_ref: elementRef, x, y: action.y ?? element?.y ?? 0 });
                      }
                    }}
                  />
                  </label>
                  <label className="effect-field">
                  <span>Y (px from top)</span>
                  <input
                    aria-label={`Effect ${visibleIndex + 1} Y position`}
                    type="number"
                    step={1}
                    min={0}
                    max={Math.max(0, 144 - (element?.height ?? 0))}
                    value={action.y ?? element?.y ?? 0}
                    disabled={!canEdit}
                    onChange={(event) => {
                      const y = Number.parseInt(event.target.value, 10);
                      if (Number.isFinite(y)) {
                        commit({ kind: action.kind, element_ref: elementRef, x: action.x ?? element?.x ?? 0, y });
                      }
                    }}
                  />
                  </label>
                </div>
              )}
              {(action.kind === "set_element_frame" || action.kind === "object.set_frame") && (
                <div className="effect-field-group effect-field-group-values">
                <label className="effect-field">
                <span>Frame</span>
                <select
                  aria-label={`Effect ${visibleIndex + 1} sprite frame`}
                  value={action.frame_ref ?? frames[0]?.frameId ?? ""}
                  disabled={!canEdit || frames.length === 0}
                  onChange={(event) => commit({
                    kind: action.kind,
                    ...(sceneObjects ? { object_ref: elementRef } : { element_ref: elementRef }),
                    frame_ref: event.target.value,
                  })}
                >
                  {frames.map((frame) => (
                    <option key={frame.frameId} value={frame.frameId}>{frame.label}</option>
                  ))}
                </select>
                </label>
                </div>
              )}
              {action.kind === "set_element_waiting_animation" && (
                <div className="effect-field-group effect-field-group-values">
                  <label className="effect-field">
                  <span>Animation</span>
                  <select
                    aria-label={`Effect ${visibleIndex + 1} animation`}
                    value={animationKey}
                    disabled={!canEdit || animationChoices.length === 0}
                    onChange={(event) => {
                      const choice = animationChoices.find((candidate) => candidate.key === event.target.value);
                      if (choice !== undefined) {
                        commit({
                          kind: action.kind,
                          element_ref: elementRef,
                          waiting_visual_ref: choice.waitingVisualRef,
                          waiting_element_ref: choice.waitingElementRef,
                          timeline_policy: action.timeline_policy === "rebase" ? "rebase" : "preserve",
                        });
                      }
                    }}
                  >
                    {animationChoices.map((choice) => (
                      <option key={choice.key} value={choice.key}>{choice.label}</option>
                    ))}
                  </select>
                  </label>
                  <label className="effect-field">
                  <span>Timing</span>
                  <select
                    aria-label={`Effect ${visibleIndex + 1} timeline behavior`}
                    value={action.timeline_policy === "rebase" ? "rebase" : "preserve"}
                    disabled={!canEdit}
                    onChange={(event) => commit({
                      kind: action.kind,
                      element_ref: elementRef,
                      waiting_visual_ref: action.waiting_visual_ref,
                      waiting_element_ref: action.waiting_element_ref,
                      timeline_policy: event.target.value,
                    })}
                  >
                    <option value="preserve">Keep timing</option>
                    <option value="rebase">Restart timing</option>
                  </select>
                  </label>
                </div>
              )}
            {timerActionKinds.includes(action.kind) && <div className="effect-field-group effect-field-group-values"><label className="effect-field"><span>Scene timer</span>
              <select aria-label={`Effect ${visibleIndex + 1} timer`} value={action.timer_ref ?? ""} disabled={!canEdit}
                onChange={event => commit({ kind: action.kind, timer_ref: event.target.value })}>
                {timers.map(id => <option key={id} value={id}>{id}</option>)}
              </select></label></div>}
            {action.kind === "play_sfx" && (
                <div className="effect-field-group effect-field-group-values">
                <label className="effect-field">
                <span>SFX cue</span>
                <select
                  aria-label={`Effect ${visibleIndex + 1} SFX cue`}
                  value={cueRef}
                  disabled={!canEdit || audioCues.length === 0}
                  onChange={(event) => commit({ kind: "play_sfx", cue_ref: event.target.value })}
                >
                  {audioCues.map((cue) => (
                    <option key={cue.cue_id} value={cue.cue_id}>{cue.display_name?.trim() || cue.cue_id}</option>
                  ))}
                </select>
                </label>
                </div>
              )}
            </div>
            {action.kind === "play_sfx" && audioCues.length === 0 && (
              <div className="plain-rule-note">Import a WAV SFX before assigning this effect.</div>
            )}
          </div>
        );
      })}
      {canAddActions && availableEffectKinds.length > 0 && (
        <label className="effect-add-control">
          <Plus size={13} aria-hidden="true" />
          <select
            aria-label="Add effect"
            value=""
            disabled={!canEdit}
            onChange={(event) => {
              const action = defaultActionForKind(event.target.value);
              if (action !== null) {
                void onAddRouteAction(sceneId, route.route_id, addActionIndex, action);
              }
            }}
          >
            <option value="">Add effect...</option>
            {availableEffectKinds.map((kind) => (
              <option key={kind} value={kind}>{EFFECT_KIND_LABELS[kind]}</option>
            ))}
          </select>
        </label>
      )}
    </div>
  );
}

function RenderInspector({ render }: { render: RenderModel }) {
  return (
    <section className="inspector-section selected-record">
      <h3>Screen layout</h3>
      <InspectorList rows={[["Internal ID", render.visual_id], ["Focus order", render.focus_index], ["Elements", render.elements.length]]} />
      <div className="element-table">
        {render.elements.map((element) => (
          <div key={element.element_id}>
            <strong>{element.element_id}</strong>
            <span>{element.kind}</span>
            <small>{element.visual_ref}</small>
            <small>{element.x},{element.y} {element.width}x{element.height} z{element.z_order}</small>
          </div>
        ))}
      </div>
    </section>
  );
}

function WaitingInspector({ waiting }: { waiting: WaitingVisual }) {
  return (
    <section className="inspector-section selected-record">
      <h3>Waiting animation</h3>
      <InspectorList
        rows={[
          ["Internal ID", waiting.waiting_visual_id],
          ["Presentation", waiting.presentation_id],
          ["Quantum", `${waiting.phase_quantum_ms} ms`],
          ["Steps", waiting.combined_step_count],
          ["Settled step", waiting.settled_step + 1],
          ["Cycle", waiting.cycle_policy],
        ]}
      />
      <div className="element-table">
        {waiting.elements.map((element) => (
          <div key={element.element_id}>
            <strong>{element.element_id}</strong>
            <span>from {element.source_element_ref}</span>
            <small>{element.phase_visual_refs.join(", ")}</small>
            <small>steps {element.step_phase_indices.join(", ")}</small>
          </div>
        ))}
      </div>
    </section>
  );
}
