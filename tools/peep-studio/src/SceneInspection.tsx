import {
  Background,
  BaseEdge,
  Controls,
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
  useUpdateNodeInternals,
} from "@xyflow/react";
import { GitBranch, Hourglass, Layers3, Network, Plus, Route, Variable } from "lucide-react";
import { useEffect, useMemo, useRef, useState, type KeyboardEvent } from "react";
import { FramebufferCanvas } from "./FramebufferCanvas";
import {
  buildSceneFlowGraphModel,
  buildStateGraphModel,
  buildStateTransitionRoute,
  planStateTransitionRoutes,
  resolveStateEntryHandle,
  resolveStateExitSide,
  type GraphSceneNode,
  type GraphStateNode,
  type StateGraphEntrySide,
  type StateGraphExitSide,
  type StateTransitionLayout,
} from "./stateGraph";
import type {
  AudioCueRecord,
  Framebuffer,
  InputAction,
  ProjectEditorData,
  RenderModel,
  SceneDocument,
  StateGuard,
  StateRecord,
  StateRoute,
  StateVariable,
  WaitingVisual,
} from "./types";

export type SceneSelection =
  | { kind: "scene" }
  | { kind: "state"; id: string }
  | { kind: "route"; id: string }
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
const SCENE_EXIT_INPUTS = [
  "BUTTON_A",
  "BUTTON_START",
  "BUTTON_B",
  "BUTTON_L",
  "BUTTON_R",
  "JOY_LEFT",
  "JOY_RIGHT",
  "JOY_UP",
  "JOY_DOWN",
] as const;
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
  selectedRouteId: string | null;
  onSelectState: (stateId: string) => void;
  onSelectRoute: (routeId: string) => void;
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
};

const STATE_ENTRY_HANDLES = [
  { id: "entry-top-left", position: Position.Top },
  { id: "entry-top-mid-left", position: Position.Top },
  { id: "entry-top-center", position: Position.Top },
  { id: "entry-top-mid-right", position: Position.Top },
  { id: "entry-top-right", position: Position.Top },
  { id: "entry-bottom-left", position: Position.Bottom },
  { id: "entry-bottom-mid-left", position: Position.Bottom },
  { id: "entry-bottom-center", position: Position.Bottom },
  { id: "entry-bottom-mid-right", position: Position.Bottom },
  { id: "entry-bottom-right", position: Position.Bottom },
] as const;

function statePositionMap(graphNodes: GraphStateNode[], flowNodes: Node[]): Map<string, StateNodePosition> {
  const positions = new Map(graphNodes.map((node) => [node.id, { x: node.x, y: node.y }]));
  flowNodes.forEach((node) => {
    positions.set(node.id, { x: node.position.x, y: node.position.y });
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
      exitSide: transitionLayouts[output.id]?.sourceSide ?? resolveStateExitSide(
        sourcePosition,
        output.targetState === undefined ? undefined : positions.get(output.targetState),
      ),
    })),
  };
}

function StateCardNode({ data, selected }: NodeProps<Node<StateCardNodeData>>) {
  const { graphNode, selectedRouteId, onSelectRoute, onSelectState } = data;
  const updateNodeInternals = useUpdateNodeInternals();
  const localOutputs = graphNode.outputs.filter((output) => output.targetScene === undefined).length;
  const sceneOutputs = graphNode.outputs.length - localOutputs;
  const objectChangeLabel = `${graphNode.placementOverrideCount} object change${graphNode.placementOverrideCount === 1 ? "" : "s"}`;
  const outputSideKey = graphNode.outputs.map((output) => `${output.id}:${output.exitSide}`).join("|");
  useEffect(() => {
    updateNodeInternals(graphNode.id);
  }, [graphNode.id, outputSideKey, updateNodeInternals]);

  return (
    <button
      className={`state-card-node ${graphNode.isEntry ? "entry" : ""} ${selected ? "selected" : ""}`}
      type="button"
      onClick={() => onSelectState(graphNode.id)}
    >
      {STATE_ENTRY_HANDLES.map((handle) => (
        <Handle
          key={handle.id}
          id={handle.id}
          className={`state-entry-zone ${handle.id}`}
          type="target"
          position={handle.position}
        />
      ))}
      <div className="state-card-heading">
        <strong>{graphNode.label}</strong>
        <div className="state-badge-strip" aria-label="State badges">
          {graphNode.isEntry && <span className="state-card-badge">Start</span>}
          <span className="state-card-badge">{graphNode.outputs.length}</span>
        </div>
      </div>
      <div className="state-card-screen">
        <strong>{objectChangeLabel}</strong>
        <span>state variation</span>
      </div>
      <div className="state-card-counts" aria-label="Transition output summary">
        <span>{localOutputs} local</span>
        <span>{sceneOutputs} scene</span>
      </div>
      <div className="state-output-list">
        {graphNode.outputs.length === 0 ? (
          <span className="state-output-empty">No trigger outputs</span>
        ) : (
          graphNode.outputs.map((output) => (
            <span
              className={`state-output-row exit-${output.exitSide} ${selectedRouteId === output.routeId ? "selected" : ""}`}
              key={output.id}
              role="button"
              tabIndex={0}
              onClick={(event) => {
                event.stopPropagation();
                onSelectRoute(output.routeId);
              }}
              onKeyDown={(event) => {
                if (event.key === "Enter" || event.key === " ") {
                  event.preventDefault();
                  event.stopPropagation();
                  onSelectRoute(output.routeId);
                }
              }}
            >
              <span>{output.label}</span>
              <small>
                {output.targetScene === undefined ? `Go to ${output.targetStateLabel ?? output.targetState ?? "state"}` : `Open ${output.targetScene}`}
                {output.guardCount > 0 ? ` - ${output.guardCount} condition${output.guardCount === 1 ? "" : "s"}` : ""}
                {output.actionCount > 0 ? ` - ${output.actionCount} effect${output.actionCount === 1 ? "" : "s"}` : ""}
              </small>
              <Handle id={output.id} type="source" position={output.exitSide === "left" ? Position.Left : Position.Right} />
            </span>
          ))
        )}
      </div>
    </button>
  );
}

const STATE_NODE_TYPES = { stateCard: StateCardNode };

function edgeSourceSide(position: Position): StateGraphExitSide {
  return position === Position.Left ? "left" : "right";
}

function edgeTargetSide(position: Position): StateGraphEntrySide {
  return position === Position.Bottom ? "bottom" : "top";
}

function StateTransitionEdge({
  data,
  id,
  markerEnd,
  selected,
  sourcePosition,
  sourceX,
  sourceY,
  style,
  targetPosition,
  targetX,
  targetY,
}: EdgeProps) {
  const laneX = typeof data?.laneX === "number" ? data.laneX : undefined;
  const route = buildStateTransitionRoute({
    sourceX,
    sourceY,
    targetX,
    targetY,
    sourceSide: edgeSourceSide(sourcePosition),
    targetSide: edgeTargetSide(targetPosition),
    laneX,
  });
  const gradientId = `state-transition-gradient-${id.replace(/[^a-zA-Z0-9_-]/g, "_")}`;
  const edgeStyle = {
    ...style,
    stroke: `url(#${gradientId})`,
  };

  return (
    <>
      <defs>
        <linearGradient id={gradientId} gradientUnits="userSpaceOnUse" x1={sourceX} y1={sourceY} x2={targetX} y2={targetY}>
          <stop offset="0%" stopColor={selected ? "#7f8e87" : "#9da9a3"} />
          <stop offset="100%" stopColor={selected ? "#175f8a" : "#4f5f58"} />
        </linearGradient>
      </defs>
      <BaseEdge id={id} markerEnd={markerEnd} path={route.path} style={edgeStyle} />
    </>
  );
}

const STATE_EDGE_TYPES = { stateTransition: StateTransitionEdge };

type SceneCardNodeData = {
  graphNode: GraphSceneNode;
  thumbnail: Framebuffer | null;
  targetScenes: SceneDocument[];
  selectedRouteId: string | null;
  canEdit: boolean;
  onSelectScene: (sceneId: string) => void;
  onSelectSceneRoute: (sceneId: string, routeId: string) => void;
  onDeleteSceneExit: (sceneId: string, routeId: string) => void;
};

function SceneCardNode({ data, selected }: NodeProps<Node<SceneCardNodeData>>) {
  const {
    graphNode,
    targetScenes,
    canEdit,
    onSelectScene,
    onSelectSceneRoute,
    onDeleteSceneExit,
    selectedRouteId,
    thumbnail,
  } = data;
  const availableInputs = SCENE_EXIT_INPUTS.filter((source) => !graphNode.usedLogicalSources.includes(source));
  const canAddExit = canEdit && availableInputs.length > 0 && targetScenes.length > 0;

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
              className={`scene-exit-row ${selectedRouteId === exit.routeId ? "selected" : ""}`}
              key={exit.id}
              role="button"
              tabIndex={0}
              onClick={(event) => {
                event.stopPropagation();
                event.currentTarget.focus();
                onSelectSceneRoute(graphNode.id, exit.routeId);
              }}
              onKeyDown={(event) => {
                if (event.key === "Enter" || event.key === " ") {
                  event.preventDefault();
                  event.stopPropagation();
                  onSelectSceneRoute(graphNode.id, exit.routeId);
                  return;
                }
                if (canEdit && (event.key === "Delete" || event.key === "Backspace")) {
                  event.preventDefault();
                  event.stopPropagation();
                  onDeleteSceneExit(graphNode.id, exit.routeId);
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
          aria-label={canAddExit ? "Drag to create a new scene exit" : "No available triggers"}
          onClick={(event) => event.stopPropagation()}
        >
          <Plus size={14} aria-hidden="true" />
          <span>{availableInputs.length === 0 ? "All triggers used" : "New exit"}</span>
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
  canEdit,
}: {
  scene: SceneDocument | null;
  editor?: ProjectEditorData;
  layoutStatus: string;
  selected: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onMoveStateNode: (sceneId: string, stateId: string, x: number, y: number) => void;
  canEdit: boolean;
}) {
  const graph = useMemo(() => buildStateGraphModel(scene, editor), [editor, scene]);
  const flowRef = useRef<ReactFlowInstance<Node<StateCardNodeData>, Edge> | null>(null);
  const didInitialFit = useRef(false);
  const previousSceneId = useRef<string | null>(scene?.scene_id ?? null);
  const defaultPositionById = useMemo(() => statePositionMap(graph.nodes, []), [graph.nodes]);
  const baseNodes: Node<StateCardNodeData>[] = useMemo(
    () =>
      graph.nodes.map((node) => ({
        id: node.id,
        type: "stateCard",
        position: { x: node.x, y: node.y },
        data: {
          graphNode: routeStateNode(node, defaultPositionById, {}),
          selectedRouteId: selected.kind === "route" ? selected.id : null,
          onSelectState: (stateId: string) => onSelect({ kind: "state", id: stateId }),
          onSelectRoute: (routeId: string) => onSelect({ kind: "route", id: routeId }),
        },
        selected: selected.kind === "state" && selected.id === node.id,
        draggable: canEdit,
        connectable: false,
      })),
    [canEdit, defaultPositionById, graph.nodes, onSelect, selected],
  );
  const [nodes, setNodes] = useState<Node<StateCardNodeData>[]>(baseNodes);
  const graphNodeById = useMemo(() => new Map(graph.nodes.map((node) => [node.id, node])), [graph.nodes]);
  const positionById = useMemo(() => statePositionMap(graph.nodes, nodes), [graph.nodes, nodes]);
  const layoutNodes = useMemo(
    () => [...positionById.entries()].map(([id, position]) => ({ id, x: position.x, y: position.y })),
    [positionById],
  );
  const transitionLayouts = useMemo(
    () =>
      planStateTransitionRoutes(
        graph.edges.map((edge) => {
          const sourceNode = graphNodeById.get(edge.source);
          const sourceOutputIndex = sourceNode?.outputs.findIndex((output) => output.id === edge.sourceHandle) ?? 0;
          return {
            id: edge.sourceHandle,
            source: edge.source,
            target: edge.target,
            sourceOutputIndex: Math.max(0, sourceOutputIndex),
          };
        }),
        layoutNodes,
      ),
    [graph.edges, graphNodeById, layoutNodes],
  );
  const flowNodes: Node<StateCardNodeData>[] = useMemo(
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
            selectedRouteId: selected.kind === "route" ? selected.id : null,
            onSelectState: (stateId: string) => onSelect({ kind: "state", id: stateId }),
            onSelectRoute: (routeId: string) => onSelect({ kind: "route", id: routeId }),
          },
          selected: selected.kind === "state" && selected.id === node.id,
          draggable: canEdit,
          connectable: false,
        };
      }),
    [canEdit, graphNodeById, nodes, onSelect, positionById, selected, transitionLayouts],
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
    setNodes((current) => applyNodeChanges(changes, current) as Node<StateCardNodeData>[]);
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
    () =>
      graph.edges.map((edge) => {
        const transitionLayout = transitionLayouts[edge.sourceHandle];
        return {
          id: edge.id,
          source: edge.source,
          target: edge.target,
          sourceHandle: edge.sourceHandle,
          targetHandle: transitionLayout?.targetHandle ?? resolveStateEntryHandle(positionById.get(edge.source) ?? { x: 0, y: 0 }, positionById.get(edge.target)),
          markerEnd: { type: MarkerType.ArrowClosed },
          selected: selected.kind === "route" && selected.id === edge.route.route_id,
          className: selected.kind === "route" && selected.id === edge.route.route_id ? "state-transition-edge selected" : "state-transition-edge",
          data: { route_id: edge.route.route_id, laneX: transitionLayout?.laneX },
          label: edge.label,
          type: "stateTransition",
          style: { strokeWidth: selected.kind === "route" && selected.id === edge.route.route_id ? 3.4 : 1.8 },
        };
      }),
    [graph.edges, positionById, selected, transitionLayouts],
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
      nodesConnectable={false}
      elementsSelectable
      onNodesChange={onNodesChange}
      onNodeDragStop={(_, node) => onNodeDragStop(node)}
      onNodeClick={(_, node) => onSelect({ kind: "state", id: node.id })}
      onEdgeClick={(_, edge) => onSelect({ kind: "route", id: String(edge.data?.route_id ?? edge.id) })}
      onPaneClick={() => onSelect({ kind: "scene" })}
      proOptions={{ hideAttribution: true }}
    >
      <Background gap={18} size={1} />
      <GraphMiniMap
        nodes={graph.nodes}
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
  selectedRouteId,
  onSelectScene,
  onSelectSceneRoute,
  onAddSceneExit,
  onDeleteSceneExit,
  onMoveSceneNode,
  onConnectSceneExit,
  canEdit,
}: {
  scenes: SceneDocument[];
  entrySceneId: string | null;
  thumbnails: Record<string, Framebuffer>;
  editor?: ProjectEditorData;
  layoutStatus: string;
  selectedSceneId: string | null;
  selectedRouteId: string | null;
  onSelectScene: (sceneId: string) => void;
  onSelectSceneRoute: (sceneId: string, routeId: string) => void;
  onAddSceneExit: (sceneId: string, logicalSource: string, targetScene: string) => void;
  onDeleteSceneExit: (sceneId: string, routeId: string) => void;
  onMoveSceneNode: (sceneId: string, x: number, y: number) => void;
  onConnectSceneExit: (sceneId: string, routeId: string, targetScene: string) => void;
  canEdit: boolean;
}) {
  const graph = useMemo(() => buildSceneFlowGraphModel(scenes, entrySceneId, editor), [editor, entrySceneId, scenes]);
  const flowRef = useRef<ReactFlowInstance | null>(null);
  const didInitialFit = useRef(false);
  const [viewportText, setViewportText] = useState("viewport not ready");
  const [lastDragText, setLastDragText] = useState("No drag yet");
  const [lastConnectText, setLastConnectText] = useState("No connect yet");
  const [pendingNewExit, setPendingNewExit] = useState<{ sourceScene: string; targetScene: string } | null>(null);
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
          selectedRouteId,
          canEdit,
          onSelectScene,
          onSelectSceneRoute,
          onDeleteSceneExit,
        },
        selected: selectedSceneId === node.id,
        draggable: canEdit,
        connectable: false,
      })),
    [canEdit, graph.nodes, onDeleteSceneExit, onSelectScene, onSelectSceneRoute, scenes, selectedRouteId, selectedSceneId, thumbnails],
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
        sourceHandle: `${edge.source}:${edge.route.route_id}`,
        selected: selectedRouteId === edge.route.route_id,
        data: { route_id: edge.route.route_id, source_scene_id: edge.source },
        markerEnd: { type: MarkerType.ArrowClosed },
        style: { strokeWidth: selectedRouteId === edge.route.route_id ? 2.8 : 1.7 },
      })),
    [graph.edges, selectedRouteId],
  );
  const deleteSelectedSceneExit = () => {
    if (!canEdit || selectedRouteId === null || selectedSceneId === null) {
      return;
    }
    const selectedExit = graph.edges.find(
      (edge) => edge.source === selectedSceneId && edge.route.route_id === selectedRouteId,
    );
    if (selectedExit === undefined) {
      return;
    }
    onDeleteSceneExit(selectedExit.source, selectedExit.route.route_id);
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
    const routeId = sourceHandle.startsWith(`${sourceScene}:`)
      ? sourceHandle.slice(sourceScene.length + 1)
      : sourceHandle;
    if (routeId.length === 0 || sourceScene === targetScene) {
      setLastConnectText(`ignored ${sourceScene}:${routeId || "-"} -> ${targetScene}`);
      return;
    }
    if (routeId === NEW_SCENE_EXIT_HANDLE) {
      setPendingNewExit({ sourceScene, targetScene });
      setLastConnectText(`choose trigger for ${sourceScene} -> ${targetScene}`);
      return;
    }
    setLastConnectText(`${sourceScene}:${routeId} -> ${targetScene}`);
    onConnectSceneExit(sourceScene, routeId, targetScene);
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
  }, [nodes.length, edges.length, selectedSceneId, selectedRouteId]);
  const debugLines = [
    `nodes ${nodes.length} / graph ${graph.nodes.length}`,
    `edges ${edges.length} / graph ${graph.edges.length}`,
    `selected scene ${selectedSceneId ?? "-"}`,
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
  const pendingSource = pendingNewExit === null ? null : graph.nodes.find((node) => node.id === pendingNewExit.sourceScene) ?? null;
  const pendingTarget = pendingNewExit === null ? null : scenes.find((scene) => scene.scene_id === pendingNewExit.targetScene) ?? null;
  const pendingInputs =
    pendingSource === null
      ? []
      : SCENE_EXIT_INPUTS.filter((source) => !pendingSource.usedLogicalSources.includes(source));

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
        onSelectSceneRoute(String(edge.data?.source_scene_id ?? ""), String(edge.data?.route_id ?? edge.id));
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
      {pendingNewExit !== null && (
        <Panel position="top-right" className="scene-exit-picker">
          <div>
            <strong>Choose trigger</strong>
            <span>
              {pendingSource?.label ?? pendingNewExit.sourceScene} to {pendingTarget?.display_name ?? pendingNewExit.targetScene}
            </span>
          </div>
          <div className="scene-exit-picker-options">
            {pendingInputs.map((source) => (
              <button
                key={source}
                type="button"
                onClick={() => {
                  onAddSceneExit(pendingNewExit.sourceScene, source, pendingNewExit.targetScene);
                  setPendingNewExit(null);
                }}
              >
                {displayInputLabel(source, source)}
              </button>
            ))}
          </div>
          <button className="text-button" type="button" onClick={() => setPendingNewExit(null)}>
            Cancel
          </button>
        </Panel>
      )}
      <Controls showInteractive={false} />
    </ReactFlow>
  );
}

export function SceneAuthoringInspector({
  scene,
  scenes,
  selection,
  onSelect,
  onRenameState,
  onSetRouteTarget,
  onSetRouteSceneTarget,
  onSetRouteGuard,
  onSetRouteAction,
  onAddRouteAction,
  audioCues,
  canEdit,
}: {
  scene: SceneDocument | null;
  scenes: SceneDocument[];
  selection: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onRenameState: (sceneId: string, stateId: string, displayName: string) => Promise<void>;
  onSetRouteTarget: (sceneId: string, routeId: string, targetState: string) => Promise<void>;
  onSetRouteSceneTarget: (sceneId: string, routeId: string, targetScene: string) => Promise<void>;
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
          states={states}
          scenes={scenes}
          inputActions={inputActions}
          variables={variables}
          audioCues={audioCues}
          onSetRouteTarget={onSetRouteTarget}
          onSetRouteSceneTarget={onSetRouteSceneTarget}
          onSetRouteGuard={onSetRouteGuard}
          onSetRouteAction={onSetRouteAction}
          onAddRouteAction={onAddRouteAction}
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

function RouteInspector({
  sceneId,
  route,
  states,
  scenes,
  inputActions,
  variables,
  audioCues,
  onSetRouteTarget,
  onSetRouteSceneTarget,
  onSetRouteGuard,
  onSetRouteAction,
  onAddRouteAction,
  canEdit,
}: {
  sceneId: string;
  route: StateRoute;
  states: StateRecord[];
  scenes: SceneDocument[];
  inputActions: InputAction[];
  variables: StateVariable[];
  audioCues: AudioCueRecord[];
  onSetRouteTarget: (sceneId: string, routeId: string, targetState: string) => Promise<void>;
  onSetRouteSceneTarget: (sceneId: string, routeId: string, targetScene: string) => Promise<void>;
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
  canEdit: boolean;
}) {
  const input = inputActions.find((item) => item.action_id === route.action_ref);
  const fromLabels = route.from_states.map((stateId) => displayStateName(states, stateId)).join(", ");
  const exitsScene = route.target_scene !== undefined;
  const routeTarget = route.target_scene === undefined
    ? displayStateName(states, route.target_state ?? "")
    : scenes.find((scene) => scene.scene_id === route.target_scene)?.display_name ?? route.target_scene;
  const targetKind = route.target_scene === undefined ? "state" : "scene";
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
        <p className="plain-rule-note">This output leaves the current scene. Edit controls for scene exits belong in Scene flow.</p>
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
