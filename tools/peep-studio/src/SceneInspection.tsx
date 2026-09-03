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
  type Connection,
  type Edge,
  type EdgeProps,
  type Node,
  type NodeChange,
  type NodeProps,
  type ReactFlowInstance,
  useReactFlow,
  useUpdateNodeInternals,
} from "@xyflow/react";
import {
  ArrowDown,
  ArrowLeft,
  ArrowRight,
  ArrowUp,
  ExternalLink,
  Eye,
  Filter,
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
  Volume2,
  Variable,
} from "lucide-react";
import { useEffect, useMemo, useRef, useState, type KeyboardEvent } from "react";
import { FramebufferCanvas } from "./FramebufferCanvas";
import {
  buildSceneFlowGraphModel,
  buildStateGraphModel,
  buildStateTransitionRoute,
  insertStateTransitionRouteSection,
  moveStateTransitionRouteSection,
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
  type GraphSceneNode,
  type GraphSceneEndpointNode,
  type GraphStateNode,
  type StateGraphEntryHandle,
  type StateGraphEntrySide,
  type StateGraphExitSide,
  type StateTransitionLayout,
} from "./stateGraph";
import type {
  AudioCueRecord,
  EditorNodePosition,
  EditorRouteRail,
  Framebuffer,
  InputAction,
  ProjectEditorData,
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
  | { kind: "scene" }
  | { kind: "sceneExit"; id: string }
  | { kind: "state"; id: string }
  | { kind: "route"; id: string; sourceState?: string }
  | { kind: "render"; id: string }
  | { kind: "waiting"; id: string };

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
  return actions.filter((action) => action.kind !== "request_render").length;
}

type StateCardNodeData = {
  graphNode: RoutedGraphStateNode;
  activeEntryHandles: string[];
  canEdit: boolean;
  selectedRouteId: string | null;
  onSelectState: (stateId: string) => void;
  onSelectRoute: (routeId: string, sourceState: string) => void;
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
  const { activeEntryHandles, canEdit, graphNode, selectedRouteId, onSelectRoute, onSelectState } = data;
  const updateNodeInternals = useUpdateNodeInternals();
  const cardRef = useRef<HTMLDivElement>(null);
  const [stemLayout, setStemLayout] = useState<StateTriggerStemLayout>({
    width: 0,
    height: 0,
    lines: [],
    sockets: {},
  });
  const physicalOutputBySource = new Map(
    graphNode.outputs
      .filter((output) => output.triggerKind === "physical")
      .map((output) => [output.logicalSource, output]),
  );
  const physicalSources = new Set<string>([
    ...PHYSICAL_TRIGGER_CONTROLS.map((control) => control.source),
    ...OPTIONAL_DIAGONAL_CONTROLS.map((control) => control.source),
  ]);
  const dynamicOutputs = graphNode.outputs.filter(
    (output) => output.triggerKind === "platform" || !physicalSources.has(output.logicalSource),
  );
  const physicalControls = [
    ...PHYSICAL_TRIGGER_CONTROLS,
    ...OPTIONAL_DIAGONAL_CONTROLS.filter((control) => physicalOutputBySource.has(control.source)),
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
        const output = physicalOutputBySource.get(control.source);
        const side = output?.exitSide ?? control.side;
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
    const output = physicalOutputBySource.get(source);
    const isActive = output !== undefined;
    const isSelected = output !== undefined && selectedRouteId === output.routeId;
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
          if (output !== undefined) {
            onSelectRoute(output.routeId, graphNode.id);
          }
        }}
      >
        <PhysicalTriggerGlyph label={label} />
      </button>
    );
  };

  return (
    <div
      ref={cardRef}
      className={`state-card-node ${graphNode.isEntry ? "entry" : ""} ${selected ? "selected" : ""}`}
      role="button"
      tabIndex={0}
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
        const output = physicalOutputBySource.get(control.source);
        const side = output?.exitSide ?? control.side;
        const className = `state-physical-socket socket-${control.slot} ${output === undefined ? "inactive" : "active"} ${selectedRouteId === output?.routeId ? "selected" : ""}`;
        const socket = stemLayout.sockets[control.slot];
        const socketStyle = socket === undefined ? undefined : {
          left: socket.x,
          top: socket.y,
          right: "auto",
          bottom: "auto",
          transform: "translate(-50%, -50%)",
        };
        if (output === undefined) {
          return <span aria-hidden="true" className={className} data-socket-slot={control.slot} key={control.source} style={socketStyle} />;
        }
        return (
          <Handle
            className={className}
            data-socket-slot={control.slot}
            id={output.id}
            key={control.source}
            style={socketStyle}
            type="source"
            position={triggerPosition(side)}
          />
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
      <button
        className="state-add-trigger-row nodrag nopan"
        type="button"
        disabled
        title={canEdit ? "PeepOS trigger creation is not exposed yet" : "Project is read-only"}
      >
        <span className="state-add-trigger-socket left" aria-hidden="true" />
        <Plus size={13} aria-hidden="true" />
        <span>Add new trigger</span>
        <span className="state-add-trigger-socket right" aria-hidden="true" />
      </button>
      <div className="state-controller-map" aria-label="Physical triggers">
        <div className="state-joystick-triggers">
          {OPTIONAL_DIAGONAL_CONTROLS
            .filter((control) => physicalOutputBySource.has(control.source))
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
  return (
    <div
      className={`state-scene-endpoint ${endpoint.kind} ${selected ? "selected" : ""}`}
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
      {endpoint.kind === "entry" ? (
        <Handle id="scene-entry-out" type="source" position={Position.Right} isConnectable={false} />
      ) : (
        <Handle id="scene-exit-in" type="target" position={Position.Left} isConnectable={canEdit} />
      )}
      <span>{endpoint.kind === "entry" ? "Scene entry" : "Scene exit"}</span>
      <strong>{endpoint.label}</strong>
      <small>{endpoint.detail}</small>
    </div>
  );
}

const STATE_NODE_TYPES = { stateCard: StateCardNode, sceneEndpoint: SceneEndpointNode };

type StateTransitionEdgeData = {
  route_id?: string;
  source_state?: string;
  laneX?: number;
  guards?: StateGuard[];
  actions?: StateAction[];
  rails?: EditorRouteRail[];
  targetHandle?: StateGraphEntryHandle;
  targetSide?: StateGraphEntrySide;
  targetEntryPorts?: Array<{
    handle: StateGraphEntryHandle;
    side: StateGraphEntrySide;
    point: EditorNodePosition;
  }>;
  canEdit?: boolean;
  showSectionHandles?: boolean;
  onSelectRoute?: (routeId: string, sourceState: string) => void;
  onSetRouteLayout?: (
    routeId: string,
    sourceState: string,
    rails: EditorRouteRail[],
    targetHandle: StateGraphEntryHandle | null,
    targetSide: StateGraphEntrySide | null,
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
  if (action.kind === "set_element_visibility") {
    return <Eye size={15} aria-hidden="true" />;
  }
  if (action.kind === "set_element_position") {
    return <Move size={15} aria-hidden="true" />;
  }
  if (action.kind === "set_element_frame") {
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

function StateTransitionEdge({
  data,
  id,
  selected,
  sourcePosition,
  sourceX,
  sourceY,
  style,
  targetPosition,
  targetX,
  targetY,
}: EdgeProps) {
  const edgeData = data as StateTransitionEdgeData | undefined;
  const { screenToFlowPosition } = useReactFlow();
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
  }, [persistedRailKey, persistedTargetHandle, persistedTargetSide]);
  useEffect(() => () => dragCleanupRef.current?.(), []);

  const routeId = typeof edgeData?.route_id === "string" ? edgeData.route_id : id;
  const sourceState = typeof edgeData?.source_state === "string" ? edgeData.source_state : "";
  const canEdit = edgeData?.canEdit === true;
  const showSectionHandles = edgeData?.showSectionHandles === true;
  const effectiveTargetHandle = draftTargetHandle ?? persistedTargetHandle;
  const effectiveTargetSide = draftTargetSide ?? persistedTargetSide ?? edgeTargetSide(targetPosition);
  const targetPort = edgeData?.targetEntryPorts?.find((port) => (
    port.handle === effectiveTargetHandle && port.side === effectiveTargetSide
  ));
  const usesReactFlowTarget = effectiveTargetHandle === persistedTargetHandle
    && effectiveTargetSide === persistedTargetSide;
  const arrowTip = usesReactFlowTarget
    ? { x: targetX, y: targetY }
    : targetPort?.point ?? { x: targetX, y: targetY };
  const laneX = usesReactFlowTarget && typeof edgeData?.laneX === "number"
    ? edgeData.laneX
    : undefined;
  const automaticRoute = buildStateTransitionRoute({
    sourceX,
    sourceY,
    targetX: arrowTip.x,
    targetY: arrowTip.y,
    sourceSide: edgeSourceSide(sourcePosition),
    targetSide: effectiveTargetSide,
    laneX,
  });
  const route = draftRails.length === 0
    ? automaticRoute
    : buildStateTransitionRoute({
        sourceX,
        sourceY,
        targetX: arrowTip.x,
        targetY: arrowTip.y,
        sourceSide: edgeSourceSide(sourcePosition),
        targetSide: effectiveTargetSide,
        rails: draftRails,
      });
  const routeSections = stateTransitionRouteSections(route.controlPoints);
  const gradientId = `state-transition-gradient-${id.replace(/[^a-zA-Z0-9_-]/g, "_")}`;
  const edgeStyle = {
    ...style,
    stroke: `url(#${gradientId})`,
  };
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

  const commitLayout = (
    rails: EditorRouteRail[],
    targetHandle: StateGraphEntryHandle | null,
    targetSide: StateGraphEntrySide | null,
  ) => {
    const rounded = rails.map((rail) => ({ axis: rail.axis, value: Math.round(rail.value) }));
    draftRailsRef.current = rounded;
    setDraftRails(rounded);
    draftTargetHandleRef.current = targetHandle ?? undefined;
    setDraftTargetHandle(targetHandle ?? undefined);
    draftTargetSideRef.current = targetSide ?? undefined;
    setDraftTargetSide(targetSide ?? undefined);
    edgeData?.onSetRouteLayout?.(routeId, sourceState, rounded, targetHandle, targetSide);
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
    if (!canEdit) {
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

  const arrowPath = effectiveTargetSide === "top"
    ? `M ${arrowTip.x} ${arrowTip.y} L ${arrowTip.x - 6} ${arrowTip.y - 9} L ${arrowTip.x + 6} ${arrowTip.y - 9} Z`
    : effectiveTargetSide === "bottom"
      ? `M ${arrowTip.x} ${arrowTip.y} L ${arrowTip.x - 6} ${arrowTip.y + 9} L ${arrowTip.x + 6} ${arrowTip.y + 9} Z`
      : effectiveTargetSide === "left"
        ? `M ${arrowTip.x} ${arrowTip.y} L ${arrowTip.x - 9} ${arrowTip.y - 6} L ${arrowTip.x - 9} ${arrowTip.y + 6} Z`
        : `M ${arrowTip.x} ${arrowTip.y} L ${arrowTip.x + 9} ${arrowTip.y - 6} L ${arrowTip.x + 9} ${arrowTip.y + 6} Z`;

  const closestEntryPort = (point: EditorNodePosition): {
    handle: StateGraphEntryHandle;
    side: StateGraphEntrySide;
    distance: number;
  } | null => {
    let closest: { handle: StateGraphEntryHandle; side: StateGraphEntrySide; distance: number } | null = null;
    for (const port of edgeData?.targetEntryPorts ?? []) {
      const distance = Math.hypot(point.x - port.point.x, point.y - port.point.y);
      if (closest === null || distance < closest.distance) {
        closest = { handle: port.handle, side: port.side, distance };
      }
    }
    return closest;
  };

  return (
    <>
      <defs>
        <linearGradient id={gradientId} gradientUnits="userSpaceOnUse" x1={sourceX} y1={sourceY} x2={arrowTip.x} y2={arrowTip.y}>
          <stop offset="0%" stopColor={selected ? "#7f8e87" : "#9da9a3"} />
          <stop offset="100%" stopColor={selected ? "#175f8a" : "#4f5f58"} />
        </linearGradient>
      </defs>
      <BaseEdge id={id} path={route.path} style={edgeStyle} />
      <path
        className="state-transition-hit-path"
        d={route.path}
        onDoubleClick={(event) => {
          event.stopPropagation();
          edgeData?.onSelectRoute?.(routeId, sourceState);
          addRouteSection(event.clientX, event.clientY);
        }}
      />
      <path
        className={`state-transition-arrow ${selected ? "selected" : ""} ${canEdit ? "editable" : ""}`}
        d={arrowPath}
        onClick={(event) => {
          event.stopPropagation();
          edgeData?.onSelectRoute?.(routeId, sourceState);
        }}
        onPointerDown={(event) => {
          if (!canEdit) {
            return;
          }
          event.preventDefault();
          event.stopPropagation();
          edgeData?.onSelectRoute?.(routeId, sourceState);
          dragCleanupRef.current?.();
          const pointerId = event.pointerId;
          const originalHandle = effectiveTargetHandle;
          const originalSide = effectiveTargetSide;
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
              commitLayout(draftRailsRef.current, closest.handle, closest.side);
            } else {
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
        }}
      />
      {tokens.map((token, index) => {
        const spacing = Math.min(0.12, 0.7 / Math.max(1, tokens.length - 1));
        const fraction = 0.5 + (index - (tokens.length - 1) / 2) * spacing;
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
                className={`state-transition-token state-transition-${token.kind} nodrag nopan ${selected ? "selected" : ""}`}
                type="button"
                aria-label={token.description}
                onClick={(event) => {
                  event.stopPropagation();
                  edgeData?.onSelectRoute?.(routeId, sourceState);
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

const STATE_EDGE_TYPES = { stateTransition: StateTransitionEdge };

type SceneCardNodeData = {
  graphNode: GraphSceneNode;
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
      className={`scene-card-node ${graphNode.isEntry ? "entry" : ""} ${selected ? "selected" : ""}`}
      role="button"
      tabIndex={0}
      onClick={() => onSelectScene(graphNode.id)}
      onKeyDown={handleSelectKey}
    >
      <div className="scene-card-heading">
        <strong>{graphNode.label}</strong>
        <div className="scene-badge-strip" aria-label="Scene badges">
          <span className="scene-card-badge" title="Internal states">
            <strong>{graphNode.stateCount}</strong>
            <small>states</small>
          </span>
        </div>
      </div>
      <div className="scene-card-preview" aria-hidden="true">
        <FramebufferCanvas framebuffer={thumbnail} />
      </div>
      <div className="scene-entry-row">
        <Handle id="entry" type="target" position={Position.Left} isConnectable={canEdit} />
        <span>{graphNode.isEntry ? "Start scene" : "Scene entry"}</span>
        <small>{graphNode.entryStateLabel}</small>
      </div>
      <div className="scene-exit-list">
        {graphNode.exits.length === 0 ? (
          <span className="scene-exit-empty">No scene exits</span>
        ) : (
          graphNode.exits.map((exit) => (
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
              <small>Go to {exit.targetScene}</small>
              <Handle id={exit.id} type="source" position={Position.Right} isConnectable={canEdit} />
            </span>
          ))
        )}
      </div>
      {targetScenes.length > 0 && (
        <div
          className={`scene-new-exit-slot ${canAddExit ? "" : "disabled"}`}
          aria-label={canAddExit ? "Drag to create a new scene exit" : "No destination scenes available"}
          onClick={(event) => event.stopPropagation()}
        >
          <Plus size={14} aria-hidden="true" />
          <span>New exit</span>
          {canAddExit && (
            <Handle
              id={`${graphNode.id}:${NEW_SCENE_EXIT_HANDLE}`}
              type="source"
              position={Position.Right}
              isConnectable={canEdit}
            />
          )}
        </div>
      )}
    </div>
  );
}

const SCENE_NODE_TYPES = { sceneCard: SceneCardNode };

function sameNodeSet(left: Node[], right: Node[]) {
  if (left.length !== right.length) {
    return false;
  }
  const rightIds = new Set(right.map((node) => node.id));
  return left.every((node) => rightIds.has(node.id));
}

export function StateGraphView({
  scene,
  editor,
  layoutStatus,
  selected,
  onSelect,
  onMoveStateNode,
  onSetRouteLayout,
  onConnectRouteToSceneExit,
  canEdit,
}: {
  scene: SceneDocument | null;
  editor?: ProjectEditorData;
  layoutStatus: string;
  selected: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onMoveStateNode: (sceneId: string, stateId: string, x: number, y: number) => void;
  onSetRouteLayout: (
    sceneId: string,
    routeId: string,
    sourceState: string,
    rails: EditorRouteRail[],
    targetHandle: StateGraphEntryHandle | null,
    targetSide: StateGraphEntrySide | null,
  ) => void;
  onConnectRouteToSceneExit: (
    sceneId: string,
    routeId: string,
    sceneExitId: string,
    targetScene: string,
  ) => void;
  canEdit: boolean;
}) {
  const graph = useMemo(() => buildStateGraphModel(scene, editor), [editor, scene]);
  const flowRef = useRef<ReactFlowInstance | null>(null);
  const didInitialFit = useRef(false);
  const previousSceneId = useRef<string | null>(scene?.scene_id ?? null);
  const defaultPositionById = useMemo(() => statePositionMap(graph.nodes, []), [graph.nodes]);
  const baseNodes: Node[] = useMemo(
    () => [
      ...graph.nodes.map((node) => ({
        id: node.id,
        type: "stateCard",
        position: { x: node.x, y: node.y },
        data: {
          graphNode: routeStateNode(node, defaultPositionById, {}),
          activeEntryHandles: node.isEntry ? ["entry-top-left"] : [],
          canEdit,
          selectedRouteId: selected.kind === "route" && (selected.sourceState === undefined || selected.sourceState === node.id)
            ? selected.id
            : null,
          onSelectState: (stateId: string) => onSelect({ kind: "state", id: stateId }),
          onSelectRoute: (routeId: string, sourceState: string) => onSelect({ kind: "route", id: routeId, sourceState }),
        },
        selected: selected.kind === "state" && selected.id === node.id,
        draggable: canEdit,
        connectable: canEdit,
      })),
      ...graph.endpoints.map((endpoint) => ({
        id: endpoint.id,
        type: "sceneEndpoint",
        position: { x: endpoint.x, y: endpoint.y },
        data: {
          endpoint,
          canEdit,
          onSelect: (selectedEndpoint: GraphSceneEndpointNode) => {
            onSelect(selectedEndpoint.kind === "exit" && selectedEndpoint.sceneExitId !== undefined
              ? { kind: "sceneExit", id: selectedEndpoint.sceneExitId }
              : { kind: "scene" });
          },
        },
        selected: endpoint.kind === "exit"
          && endpoint.sceneExitId !== undefined
          && selected.kind === "sceneExit"
          && selected.id === endpoint.sceneExitId,
        draggable: canEdit && endpoint.declared,
        connectable: canEdit,
      })),
    ],
    [canEdit, defaultPositionById, graph.endpoints, graph.nodes, onSelect, selected],
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
        graph.edges.filter((edge) => edge.targetKind !== "scene_exit").map((edge) => {
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
              ...(graphNode.isEntry ? ["entry-top-left"] : []),
              ...graph.edges
                .filter((edge) => edge.target === graphNode.id)
                .map((edge) => transitionLayouts[edge.sourceHandle]?.targetHandle)
                .filter((handle): handle is StateGraphEntryHandle => handle !== undefined),
            ],
            canEdit,
            selectedRouteId: selected.kind === "route" && (selected.sourceState === undefined || selected.sourceState === graphNode.id)
              ? selected.id
              : null,
            onSelectState: (stateId: string) => onSelect({ kind: "state", id: stateId }),
            onSelectRoute: (routeId: string, sourceState: string) => onSelect({ kind: "route", id: routeId, sourceState }),
          },
          selected: selected.kind === "state" && selected.id === node.id,
          draggable: canEdit,
          connectable: canEdit,
        };
      }),
    [canEdit, graph.edges, graphNodeById, nodes, onSelect, positionById, selected, transitionLayouts],
  );
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
    const endpoint = graph.endpoints.find((item) => item.id === connection.target);
    if (endpoint?.kind !== "exit" || endpoint.sceneExitId === undefined || endpoint.targetScene === undefined) {
      return;
    }
    const sourceNode = graph.nodes.find((item) => item.id === connection.source);
    const output = sourceNode?.outputs.find((item) => item.id === connection.sourceHandle);
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
  useEffect(() => {
    if (flowRef.current === null) {
      return;
    }
    window.requestAnimationFrame(() => {
      flowRef.current?.fitView({ padding: 0.22, maxZoom: 1 });
    });
  }, [scene?.scene_id]);
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
        const targetLabel = output.targetScene === undefined
          ? output.targetStateLabel ?? output.targetState ?? "state"
          : output.targetScene;
        return {
          from: node.label,
          trigger: output.label,
          target: targetLabel,
          targetKind: output.targetScene === undefined ? "state" : "scene",
          conditions: output.guardCount,
          effects: output.actionCount,
        };
      }
    }
    return null;
  }, [graph.nodes, selected]);
  const edges: Edge[] = useMemo(
    () => {
      const transitionEdges = graph.edges.map((edge) => {
        const transitionLayout = transitionLayouts[edge.sourceHandle];
        const targetNode = graphNodeById.get(edge.target);
        const targetNodePosition = positionById.get(edge.target);
        const targetEntryPorts = edge.targetKind === "scene_exit" || targetNode === undefined || targetNodePosition === undefined
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
            ? edge.targetKind === "scene_exit" ? "scene-exit-in" : undefined
            : stateEntryPortId(transitionLayout.targetHandle, transitionLayout.targetSide),
          selected: isSelected,
          className: isSelected ? "state-transition-edge selected" : "state-transition-edge",
          data: {
            route_id: edge.route.route_id,
            source_state: edge.source,
            laneX: transitionLayout?.laneX,
            guards: edge.guards,
            actions: edge.actions,
            rails: edge.rails,
            targetHandle: edge.targetKind === "scene_exit" ? undefined : transitionLayout?.targetHandle,
            targetSide: edge.targetKind === "scene_exit" ? "left" : transitionLayout?.targetSide,
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
            ) => {
              if (scene !== null) {
                onSetRouteLayout(scene.scene_id, routeId, sourceState, rails, targetHandle, targetSide);
              }
            },
          },
          label: edge.label,
          type: "stateTransition",
          style: { strokeWidth: isSelected ? 3.4 : 1.8 },
        };
      });
      const entryEdge = graph.entryEdge === undefined ? [] : [{
        id: `${graph.entryEdge.source}->${graph.entryEdge.target}`,
        source: graph.entryEdge.source,
        target: graph.entryEdge.target,
        sourceHandle: "scene-entry-out",
        targetHandle: stateEntryPortId("entry-top-left", "left"),
        type: "smoothstep",
        selectable: false,
        focusable: false,
        markerEnd: { type: MarkerType.ArrowClosed, color: "#2f9e44" },
        style: { stroke: "#2f9e44", strokeWidth: 2 },
      }];
      return [...transitionEdges, ...entryEdge];
    },
    [canEdit, graph.edges, graphNodeById, onSelect, onSetRouteLayout, positionById, scene, selected, transitionLayouts],
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
      nodesDraggable={canEdit}
      nodesConnectable={canEdit}
      elementsSelectable
      onNodesChange={onNodesChange}
      onConnect={onConnect}
      onNodeDragStop={(_, node) => onNodeDragStop(node)}
      onNodeClick={(_, node) => {
        if (node.type === "stateCard") {
          onSelect({ kind: "state", id: node.id });
          return;
        }
        const endpoint = (node.data as SceneEndpointNodeData | undefined)?.endpoint;
        onSelect(endpoint?.kind === "exit" && endpoint.sceneExitId !== undefined
          ? { kind: "sceneExit", id: endpoint.sceneExitId }
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
      onPaneClick={() => onSelect({ kind: "scene" })}
      proOptions={{ hideAttribution: true }}
    >
      <Background gap={18} size={1} />
      <GraphMiniMap
        nodes={[...graph.nodes, ...graph.endpoints]}
        edges={graph.edges}
        selectedId={selected.kind === "state" ? selected.id : null}
      />
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

export function SceneFlowView({
  scenes,
  entrySceneId,
  thumbnails,
  editor,
  layoutStatus,
  selectedSceneId,
  selectedSceneExitId,
  selectedRouteId,
  onSelectScene,
  onSelectSceneExit,
  onSelectSceneRoute,
  onAddSceneExit,
  onDeleteSceneExit,
  onDeleteLegacyRoute,
  onMoveSceneNode,
  onSetSceneExitTarget,
  onConnectSceneExit,
  canEdit,
}: {
  scenes: SceneDocument[];
  entrySceneId: string | null;
  thumbnails: Record<string, Framebuffer>;
  editor?: ProjectEditorData;
  layoutStatus: string;
  selectedSceneId: string | null;
  selectedSceneExitId: string | null;
  selectedRouteId: string | null;
  onSelectScene: (sceneId: string) => void;
  onSelectSceneExit: (sceneId: string, sceneExitId: string) => void;
  onSelectSceneRoute: (sceneId: string, routeId: string) => void;
  onAddSceneExit: (sceneId: string, targetScene: string) => void;
  onDeleteSceneExit: (sceneId: string, sceneExitId: string) => void;
  onDeleteLegacyRoute: (sceneId: string, routeId: string) => void;
  onMoveSceneNode: (sceneId: string, x: number, y: number) => void;
  onSetSceneExitTarget: (sceneId: string, sceneExitId: string, targetScene: string) => void;
  onConnectSceneExit: (sceneId: string, routeId: string, targetScene: string) => void;
  canEdit: boolean;
}) {
  const graph = useMemo(() => buildSceneFlowGraphModel(scenes, entrySceneId, editor), [editor, entrySceneId, scenes]);
  const flowRef = useRef<ReactFlowInstance | null>(null);
  const didInitialFit = useRef(false);
  const [viewportText, setViewportText] = useState("viewport not ready");
  const [lastDragText, setLastDragText] = useState("No drag yet");
  const [lastConnectText, setLastConnectText] = useState("No connect yet");
  const baseNodes: Node[] = useMemo(
    () =>
      graph.nodes.map((node) => ({
        id: node.id,
        type: "sceneCard",
        position: { x: node.x, y: node.y },
        data: {
          graphNode: node,
          thumbnail: thumbnails[node.id] ?? null,
          targetScenes: scenes.filter((scene) => scene.scene_type === "STATE_SCENE" && scene.scene_id !== node.id),
          selectedSceneExitId,
          selectedRouteId,
          canEdit,
          onSelectScene,
          onSelectSceneExit,
          onSelectSceneRoute,
          onDeleteSceneExit,
          onDeleteLegacyRoute,
        },
        selected: selectedSceneId === node.id,
        draggable: canEdit,
        connectable: false,
      })),
    [canEdit, graph.nodes, onDeleteLegacyRoute, onDeleteSceneExit, onSelectScene, onSelectSceneExit, onSelectSceneRoute, scenes, selectedRouteId, selectedSceneExitId, selectedSceneId, thumbnails],
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
    setNodes((current) => applyNodeChanges(changes, current));
  };
  const onNodeDragStop = (node: Node) => {
    const x = Math.round(node.position.x);
    const y = Math.round(node.position.y);
    setLastDragText(`${node.id} dropped @ ${x}, ${y}`);
    setNodes((current) =>
      current.map((item) => (item.id === node.id ? { ...item, position: { x, y } } : item)),
    );
    onMoveSceneNode(node.id, x, y);
  };
  const edges: Edge[] = useMemo(
    () =>
      graph.edges.map((edge) => ({
        id: edge.id,
        source: edge.source,
        target: edge.target,
        sourceHandle: edge.sceneExit.id,
        selected: edge.sceneExit.sceneExitId !== undefined
          ? selectedSceneExitId === edge.sceneExit.sceneExitId
          : selectedRouteId === edge.sceneExit.routeId,
        data: {
          route_id: edge.sceneExit.routeId,
          scene_exit_id: edge.sceneExit.sceneExitId,
          source_scene_id: edge.source,
        },
        markerEnd: { type: MarkerType.ArrowClosed },
        style: {
          strokeWidth: (edge.sceneExit.sceneExitId !== undefined
            ? selectedSceneExitId === edge.sceneExit.sceneExitId
            : selectedRouteId === edge.sceneExit.routeId) ? 2.8 : 1.7,
        },
      })),
    [graph.edges, selectedRouteId, selectedSceneExitId],
  );
  const deleteSelectedSceneExit = () => {
    if (!canEdit || selectedSceneId === null) {
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
    const targetScene = connection.target;
    const sourceHandle = connection.sourceHandle;
    if (sourceScene === null || targetScene === null || sourceHandle === null) {
      setLastConnectText("ignored incomplete connection");
      return;
    }
    if (sourceScene === targetScene) {
      setLastConnectText(`ignored ${sourceScene} -> ${targetScene}`);
      return;
    }
    if (sourceHandle === `${sourceScene}:${NEW_SCENE_EXIT_HANDLE}`) {
      setLastConnectText(`created exit ${sourceScene} -> ${targetScene}`);
      onAddSceneExit(sourceScene, targetScene);
      return;
    }
    const sourceExit = graph.nodes.find((node) => node.id === sourceScene)?.exits.find((exit) => exit.id === sourceHandle);
    if (sourceExit?.sceneExitId !== undefined) {
      setLastConnectText(`${sourceScene}:${sourceExit.sceneExitId} -> ${targetScene}`);
      onSetSceneExitTarget(sourceScene, sourceExit.sceneExitId, targetScene);
    } else if (sourceExit?.routeId !== undefined) {
      setLastConnectText(`${sourceScene}:${sourceExit.routeId} -> ${targetScene}`);
      onConnectSceneExit(sourceScene, sourceExit.routeId, targetScene);
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
  }, [nodes.length, edges.length, selectedSceneId, selectedRouteId, selectedSceneExitId]);
  const debugLines = [
    `nodes ${nodes.length} / graph ${graph.nodes.length}`,
    `edges ${edges.length} / graph ${graph.edges.length}`,
    `selected scene ${selectedSceneId ?? "-"}`,
    `selected scene exit ${selectedSceneExitId ?? "-"}`,
    `selected route ${selectedRouteId ?? "-"}`,
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
      onKeyDown={handleSceneFlowKeyDown}
      tabIndex={0}
      onNodeClick={(_, node) => onSelectScene(node.id)}
      onEdgeClick={(event, edge) => {
        (event.currentTarget as HTMLElement).focus();
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
      <GraphMiniMap nodes={graph.nodes} edges={graph.edges} selectedId={selectedSceneId} />
      <Panel position="top-left" className="scene-flow-debug">
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

export function SceneAuthoringInspector({
  scene,
  scenes,
  editor,
  selection,
  onSelect,
  onRenameState,
  onSetRouteTarget,
  onSetRouteSceneTarget,
  onSetSceneExitTarget,
  onSetRouteGuard,
  onSetRouteAction,
  onAddRouteAction,
  onResetRouteLayout,
  audioCues,
  canEdit,
}: {
  scene: SceneDocument | null;
  scenes: SceneDocument[];
  editor?: ProjectEditorData;
  selection: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onRenameState: (sceneId: string, stateId: string, displayName: string) => Promise<void>;
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
  onResetRouteLayout: (sceneId: string, routeId: string, sourceState: string) => Promise<void>;
  audioCues: AudioCueRecord[];
  canEdit: boolean;
}) {
  const variables = scene?.variables ?? [];
  const inputActions = scene?.input_actions ?? [];
  const states = scene?.states ?? [];
  const routes = scene?.routes ?? [];
  const renderModels = scene?.render_models ?? [];
  const waitingVisuals = scene?.waiting_visuals ?? [];
  const state = selection.kind === "state" ? states.find((item) => item.state_id === selection.id) ?? null : null;
  const route = selection.kind === "route" ? routes.find((item) => item.route_id === selection.id) ?? null : null;
  const sceneExit = selection.kind === "sceneExit"
    ? (scene?.scene_exits ?? []).find((item) => item.scene_exit_id === selection.id) ?? null
    : null;
  const render = selection.kind === "render" ? renderModels.find((item) => item.visual_id === selection.id) ?? null : null;
  const waiting = selection.kind === "waiting" ? waitingVisuals.find((item) => item.waiting_visual_id === selection.id) ?? null : null;

  return (
    <>
      {state !== null && scene !== null && (
        <StateInspector
          sceneId={scene.scene_id}
          state={state}
          renderModels={renderModels}
          waitingVisuals={waitingVisuals}
          onSelect={onSelect}
          onRenameState={onRenameState}
          canEdit={canEdit}
        />
      )}
      {route !== null && scene !== null && (
        <RouteInspector
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
          audioCues={audioCues}
          onSetRouteTarget={onSetRouteTarget}
          onSetRouteSceneTarget={onSetRouteSceneTarget}
          onSetRouteGuard={onSetRouteGuard}
          onSetRouteAction={onSetRouteAction}
          onAddRouteAction={onAddRouteAction}
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
          canEdit={canEdit}
        />
      )}
      {render !== null && <RenderInspector render={render} />}
      {waiting !== null && <WaitingInspector waiting={waiting} />}

      {selection.kind === "scene" && (
        <SceneOverview
          scene={scene}
          states={states}
          routes={routes}
          variables={variables}
          renderModels={renderModels}
          waitingVisuals={waitingVisuals}
          onSelect={onSelect}
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
}: {
  scene: SceneDocument | null;
  states: StateRecord[];
  routes: StateRoute[];
  variables: StateVariable[];
  renderModels: RenderModel[];
  waitingVisuals: WaitingVisual[];
  onSelect: (selection: SceneSelection) => void;
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
  const elementCount = renderModels.reduce((total, item) => total + item.elements.length, 0);
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
            <span>Screen items</span>
            <strong>{elementCount}</strong>
          </div>
        </div>
        <p className="scene-overview-prompt">Select a state or transition in the graph to edit it.</p>
      </section>

      <section className="inspector-section">
        <h3><Variable size={14} aria-hidden="true" /> Variables</h3>
        {variables.length === 0 ? (
          <EmptyInspector>No variables in this scene.</EmptyInspector>
        ) : (
          <div className="record-list">
            {variables.map((variable: StateVariable) => (
              <button key={variable.variable_id} className="record-row" type="button">
                <strong>{variable.variable_id}</strong>
                <small>starts at {variable.initial}; allowed {variable.minimum} to {variable.maximum}</small>
              </button>
            ))}
          </div>
        )}
      </section>

      <section className="inspector-section">
        <h3><Route size={14} aria-hidden="true" /> Transitions</h3>
        {routes.length === 0 ? (
          <EmptyInspector>No transitions in this scene.</EmptyInspector>
        ) : (
          <div className="record-list">
            {routes.map((item: StateRoute) => (
              <button key={item.route_id} className="record-row" type="button" onClick={() => onSelect({ kind: "route", id: item.route_id })}>
                <strong>{item.from_states.join(", ")} {"->"} {item.target_scene === undefined ? item.target_state : `scene:${item.target_scene}`}</strong>
                <small>{item.guards.length === 0 ? "always allowed" : `${item.guards.length} condition${item.guards.length === 1 ? "" : "s"}`}; {visibleEffectCount(item.actions)} effect{visibleEffectCount(item.actions) === 1 ? "" : "s"}</small>
              </button>
            ))}
          </div>
        )}
      </section>

      <section className="inspector-section">
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
      </section>

      <section className="inspector-section">
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
      </section>
    </>
  );
}

function StateInspector({
  sceneId,
  state,
  renderModels,
  waitingVisuals,
  onSelect,
  onRenameState,
  canEdit,
}: {
  sceneId: string;
  state: StateRecord;
  renderModels: RenderModel[];
  waitingVisuals: WaitingVisual[];
  onSelect: (selection: SceneSelection) => void;
  onRenameState: (sceneId: string, stateId: string, displayName: string) => Promise<void>;
  canEdit: boolean;
}) {
  const render = renderModels[0];
  const waiting = waitingVisuals.find((item) => item.waiting_visual_id === state.waiting_visual_ref);
  const [displayName, setDisplayName] = useState(state.display_name);
  const screenElementCount = render?.elements.length ?? 0;
  const placementOverrideCount = state.placement_overrides?.length ?? 0;
  const waitingStepCount = waiting?.combined_step_count ?? 0;

  useEffect(() => {
    setDisplayName(state.display_name);
  }, [state.display_name, state.state_id]);

  const trimmed = displayName.trim();
  const renameDisabled = !canEdit || trimmed.length === 0 || trimmed === state.display_name;

  return (
    <section className="inspector-section selected-record">
      <h3>Selected state</h3>
      <div className="state-summary-card">
        <div>
          <span>State</span>
          <strong>{state.display_name}</strong>
        </div>
        <div>
          <span>Object changes</span>
          <strong>{placementOverrideCount}</strong>
        </div>
        <div>
          <span>Waiting</span>
          <strong>{waiting === undefined ? "Not linked" : `${waitingStepCount} step${waitingStepCount === 1 ? "" : "s"}`}</strong>
        </div>
      </div>
      <form
        className="rename-form"
        onSubmit={(event) => {
          event.preventDefault();
          if (!renameDisabled) {
            void onRenameState(sceneId, state.state_id, trimmed);
          }
        }}
      >
        <label htmlFor={`state-name-${state.state_id}`}>State name</label>
        <div>
          <input
            id={`state-name-${state.state_id}`}
            value={displayName}
            onChange={(event) => setDisplayName(event.target.value)}
            disabled={!canEdit}
          />
          <button type="submit" disabled={renameDisabled}>Rename</button>
        </div>
      </form>
      {render !== undefined && (
        <button className="link-row" type="button" onClick={() => onSelect({ kind: "render", id: render.visual_id })}>
          Scene placement <strong>{screenElementCount} object{screenElementCount === 1 ? "" : "s"}</strong>
        </button>
      )}
      <button className="link-row" type="button" onClick={() => onSelect({ kind: "waiting", id: state.waiting_visual_ref })}>
        Waiting animation <strong>{waiting === undefined ? "Missing" : `${waiting.combined_step_count} step${waiting.combined_step_count === 1 ? "" : "s"}`}</strong>
      </button>
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
  sceneId,
  route,
  sourceState,
  hasManualRoute,
  states,
  scenes,
  sceneExits,
  inputActions,
  variables,
  audioCues,
  onSetRouteTarget,
  onSetRouteSceneTarget,
  onSetRouteGuard,
  onSetRouteAction,
  onAddRouteAction,
  onResetRouteLayout,
  canEdit,
}: {
  sceneId: string;
  route: StateRoute;
  sourceState?: string;
  hasManualRoute: boolean;
  states: StateRecord[];
  scenes: SceneDocument[];
  sceneExits: SceneExitRecord[];
  inputActions: InputAction[];
  variables: StateVariable[];
  audioCues: AudioCueRecord[];
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
  onResetRouteLayout: (sceneId: string, routeId: string, sourceState: string) => Promise<void>;
  canEdit: boolean;
}) {
  const input = inputActions.find((item) => item.action_id === route.action_ref);
  const fromLabels = route.from_states.map((stateId) => displayStateName(states, stateId)).join(", ");
  const exitsScene = route.target_scene !== undefined;
  const routeSceneExit = sceneExits.find((sceneExit) => sceneExit.scene_exit_id === route.scene_exit_ref)
    ?? sceneExits.find((sceneExit) => sceneExit.target_scene === route.target_scene);
  const routeTarget = route.target_scene === undefined
    ? displayStateName(states, route.target_state ?? "")
    : routeSceneExit?.display_name ?? scenes.find((scene) => scene.scene_id === route.target_scene)?.display_name ?? route.target_scene;
  const targetKind = route.target_scene === undefined ? "state" : "scene exit";
  return (
    <section className="inspector-section selected-record">
      <h3>Selected transition</h3>
      <div className="transition-summary">
        <div>
          <span>When</span>
          <strong>{displayInputLabel(input?.logical_source, route.action_ref)}</strong>
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
      {exitsScene ? (
        <label className="select-field" htmlFor={`route-scene-exit-${route.route_id}`}>
          Scene exit
          <select
            id={`route-scene-exit-${route.route_id}`}
            value={routeSceneExit?.scene_exit_id ?? ""}
            disabled={!canEdit || sceneExits.length === 0}
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
            disabled={!canEdit}
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
            disabled={!canEdit || !hasManualRoute}
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
        canEdit={canEdit}
        onSetRouteGuard={onSetRouteGuard}
      />
      <h4>Then</h4>
      <EditableActionList
        sceneId={sceneId}
        route={route}
        variables={variables}
        audioCues={audioCues}
        canAddActions={!exitsScene}
        canEdit={canEdit}
        onSetRouteAction={onSetRouteAction}
        onAddRouteAction={onAddRouteAction}
      />
      <div className="internal-ref-note">
        Internal transition ID: <code>{route.route_id}</code>
      </div>
    </section>
  );
}

function EditableGuardList({
  sceneId,
  route,
  variables,
  canEdit,
  onSetRouteGuard,
}: {
  sceneId: string;
  route: StateRoute;
  variables: StateVariable[];
  canEdit: boolean;
  onSetRouteGuard: (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    variableRef: string,
    operator: string,
    value: number,
  ) => Promise<void>;
}) {
  if (route.guards.length === 0) {
    return <div className="plain-rule-note">Always allowed.</div>;
  }
  return (
    <div className="guard-editor-list">
      {route.guards.map((guard, index) => {
        const commit = (variableRef: string, operator: string, value: number) => {
          void onSetRouteGuard(sceneId, route.route_id, index, variableRef, operator, value);
        };
        return (
          <div className="logic-sentence-row" key={`${route.route_id}-guard-${index}`}>
            {route.guards.length > 1 && <span className="logic-row-index">Condition {index + 1}</span>}
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
    </div>
  );
}

function EditableActionList({
  sceneId,
  route,
  variables,
  audioCues,
  canAddActions,
  canEdit,
  onSetRouteAction,
  onAddRouteAction,
}: {
  sceneId: string;
  route: StateRoute;
  variables: StateVariable[];
  audioCues: AudioCueRecord[];
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
}) {
  const visibleActions = route.actions
    .map((action, actionIndex) => ({ action, actionIndex }))
    .filter(({ action }) => action.kind !== "request_render");
  const addActionIndex = route.actions.length;
  const addVariableAction = variables[0] === undefined ? null : {
    kind: "set_variable",
    variable_ref: variables[0].variable_id,
    operation: "assign",
    value: 0,
  };
  const addSfxAction = audioCues[0] === undefined ? null : {
    kind: "play_sfx",
    cue_ref: audioCues[0].cue_id,
  };
  const addEffectButtons = canAddActions && (addVariableAction !== null || addSfxAction !== null) ? (
    <div className="action-add-row">
      {addVariableAction !== null && (
        <button
          className="button secondary"
          type="button"
          disabled={!canEdit}
          onClick={() => {
            void onAddRouteAction(sceneId, route.route_id, addActionIndex, addVariableAction);
          }}
        >
          Add variable
        </button>
      )}
      {addSfxAction !== null && (
        <button
          className="button secondary"
          type="button"
          disabled={!canEdit}
          onClick={() => {
            void onAddRouteAction(sceneId, route.route_id, addActionIndex, addSfxAction);
          }}
        >
          Add SFX
        </button>
      )}
    </div>
  ) : null;
  if (visibleActions.length === 0) {
    return (
      <div className="action-editor-list">
        <div className="plain-rule-note">No visible effects.</div>
        {addEffectButtons}
      </div>
    );
  }
  return (
    <div className="action-editor-list">
      {visibleActions.map(({ action, actionIndex }, visibleIndex) => {
        const variableRef = action.variable_ref ?? variables[0]?.variable_id ?? "";
        const operation = action.operation === "add" ? "add" : "assign";
        const value = typeof action.value === "number" ? action.value : 0;
        const isAdd = operation === "add";
        const cueRef = action.cue_ref ?? audioCues[0]?.cue_id ?? "";
        const commit = (nextAction: Record<string, unknown>) => {
          void onSetRouteAction(sceneId, route.route_id, actionIndex, nextAction);
        };
        return (
          <div className="logic-sentence-row" key={`${route.route_id}-action-${actionIndex}`}>
            {visibleActions.length > 1 && <span className="logic-row-index">Effect {visibleIndex + 1}</span>}
            <div className="logic-sentence">
              <span>Then</span>
              <select
                aria-label={`Effect ${visibleIndex + 1} kind`}
                value={action.kind}
                disabled={!canEdit}
                onChange={(event) => {
                  if (event.target.value === "play_sfx") {
                    if (cueRef !== "") {
                      commit({ kind: "play_sfx", cue_ref: cueRef });
                    }
                    return;
                  }
                  commit({ kind: "set_variable", variable_ref: variableRef, operation, value });
                }}
              >
                <option value="set_variable">Change variable</option>
                <option value="play_sfx" disabled={audioCues.length === 0}>Play SFX</option>
              </select>
              {action.kind === "set_variable" && (
                <>
                  <select
                    aria-label={`Effect ${visibleIndex + 1} operation`}
                    value={operation}
                    disabled={!canEdit}
                    onChange={(event) => {
                      commit({ kind: "set_variable", variable_ref: variableRef, operation: event.target.value, value });
                    }}
                  >
                    {ACTION_OPERATIONS.map((item) => (
                      <option key={item} value={item}>
                        {item === "assign" ? "set" : "change"}
                      </option>
                    ))}
                  </select>
                  <select
                    aria-label={`Effect ${visibleIndex + 1} variable`}
                    value={variableRef}
                    disabled={!canEdit || variables.length === 0}
                    onChange={(event) => {
                      commit({ kind: "set_variable", variable_ref: event.target.value, operation, value });
                    }}
                  >
                    {variables.map((variable) => (
                      <option key={variable.variable_id} value={variable.variable_id}>
                        {displayVariableName(variable.variable_id)}
                      </option>
                    ))}
                  </select>
                  <span>{isAdd ? "by" : "to"}</span>
                  <input
                    aria-label={isAdd ? `Effect ${visibleIndex + 1} change amount` : `Effect ${visibleIndex + 1} target value`}
                    type="number"
                    step={1}
                    value={value}
                    disabled={!canEdit}
                    onChange={(event) => {
                      const parsed = Number.parseInt(event.target.value, 10);
                      if (Number.isFinite(parsed)) {
                        commit({ kind: "set_variable", variable_ref: variableRef, operation, value: parsed });
                      }
                    }}
                  />
                </>
              )}
              {action.kind === "play_sfx" && (
                <select
                  aria-label={`Effect ${visibleIndex + 1} SFX cue`}
                  value={cueRef}
                  disabled={!canEdit || audioCues.length === 0}
                  onChange={(event) => {
                    commit({ kind: "play_sfx", cue_ref: event.target.value });
                  }}
                >
                  {audioCues.map((cue) => (
                    <option key={cue.cue_id} value={cue.cue_id}>
                      {cue.cue_id}
                    </option>
                  ))}
                </select>
              )}
            </div>
            {action.kind === "play_sfx" && audioCues.length === 0 && (
              <div className="plain-rule-note">Import a WAV SFX before assigning this effect.</div>
            )}
          </div>
        );
      })}
      {addEffectButtons}
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
