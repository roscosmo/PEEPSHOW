import {
  Background,
  Controls,
  Handle,
  MarkerType,
  MiniMap,
  Position,
  ReactFlow,
  type Edge,
  type Node,
  type NodeProps,
} from "@xyflow/react";
import { GitBranch, Hourglass, Layers3, Network, Route, Variable } from "lucide-react";
import { useEffect, useMemo, useState } from "react";
import { buildSceneFlowGraphModel, buildStateGraphModel, type GraphSceneNode, type GraphStateNode } from "./stateGraph";
import type {
  InputAction,
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
    return <EmptyInspector>No guards.</EmptyInspector>;
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
  BUTTON_B: "Button B",
  BUTTON_L: "Left button",
  BUTTON_R: "Right button",
  JOY_LEFT: "Joystick left",
  JOY_RIGHT: "Joystick right",
  JOY_UP: "Joystick up",
  JOY_DOWN: "Joystick down",
};
const GUARD_OPERATOR_LABELS: Record<string, string> = {
  eq: "is",
  ne: "is not",
  lt: "is less than",
  le: "is at most",
  gt: "is greater than",
  ge: "is at least",
};

function displayInputLabel(source: string | undefined, fallback: string): string {
  if (source === undefined) {
    return fallback;
  }
  return INPUT_LABELS[source] ?? source;
}

function displayStateName(states: StateRecord[], stateId: string): string {
  return states.find((state) => state.state_id === stateId)?.display_name ?? stateId;
}

function routeTargetLabel(route: StateRoute, states: StateRecord[]): string {
  if (route.target_scene !== undefined) {
    return `Scene exit: ${route.target_scene}`;
  }
  return displayStateName(states, route.target_state ?? "");
}

type StateCardNodeData = {
  graphNode: GraphStateNode;
  selectedRouteId: string | null;
  onSelectState: (stateId: string) => void;
  onSelectRoute: (routeId: string) => void;
};

function StateCardNode({ data, selected }: NodeProps<Node<StateCardNodeData>>) {
  const { graphNode, selectedRouteId, onSelectRoute, onSelectState } = data;
  return (
    <button
      className={`state-card-node ${graphNode.isEntry ? "entry" : ""} ${selected ? "selected" : ""}`}
      type="button"
      onClick={() => onSelectState(graphNode.id)}
    >
      <Handle type="target" position={Position.Left} />
      <div className="state-card-heading">
        <span>{graphNode.isEntry ? "Start" : "State"}</span>
        <strong>{graphNode.label}</strong>
      </div>
      <div className="state-output-list">
        {graphNode.outputs.length === 0 ? (
          <span className="state-output-empty">No outputs</span>
        ) : (
          graphNode.outputs.map((output) => (
            <span
              className={`state-output-row ${selectedRouteId === output.routeId ? "selected" : ""}`}
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
                {output.targetScene === undefined ? "Local" : `Scene exit: ${output.targetScene}`} - {output.guardCount} rule{output.guardCount === 1 ? "" : "s"} - {output.actionCount} effect{output.actionCount === 1 ? "" : "s"}
              </small>
              <Handle id={output.id} type="source" position={Position.Right} />
            </span>
          ))
        )}
      </div>
    </button>
  );
}

const STATE_NODE_TYPES = { stateCard: StateCardNode };

type SceneCardNodeData = {
  graphNode: GraphSceneNode;
  onSelectScene: (sceneId: string) => void;
};

function SceneCardNode({ data, selected }: NodeProps<Node<SceneCardNodeData>>) {
  const { graphNode, onSelectScene } = data;
  return (
    <button
      className={`scene-card-node ${graphNode.isEntry ? "entry" : ""} ${selected ? "selected" : ""}`}
      type="button"
      onClick={() => onSelectScene(graphNode.id)}
    >
      <Handle type="target" position={Position.Left} />
      <div className="scene-card-preview" aria-hidden="true">
        <span>{graphNode.label.slice(0, 2).toUpperCase()}</span>
      </div>
      <div className="scene-card-heading">
        <span>{graphNode.isEntry ? "Starts here" : "Scene"}</span>
        <strong>{graphNode.label}</strong>
      </div>
      <div className="scene-card-facts">
        <span>{graphNode.stateCount} state{graphNode.stateCount === 1 ? "" : "s"}</span>
        <span>{graphNode.exitCount} exit{graphNode.exitCount === 1 ? "" : "s"}</span>
      </div>
      <small>First screen: {graphNode.entryStateLabel}</small>
      <Handle type="source" position={Position.Right} />
    </button>
  );
}

const SCENE_NODE_TYPES = { sceneCard: SceneCardNode };

export function StateGraphView({
  scene,
  selected,
  onSelect,
}: {
  scene: SceneDocument | null;
  selected: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
}) {
  const graph = useMemo(() => buildStateGraphModel(scene), [scene]);
  const nodes: Node[] = useMemo(
    () =>
      graph.nodes.map((node) => ({
        id: node.id,
        type: "stateCard",
        position: { x: node.x, y: node.y },
        data: {
          graphNode: node,
          selectedRouteId: selected.kind === "route" ? selected.id : null,
          onSelectState: (stateId: string) => onSelect({ kind: "state", id: stateId }),
          onSelectRoute: (routeId: string) => onSelect({ kind: "route", id: routeId }),
        },
        selected: selected.kind === "state" && selected.id === node.id,
        draggable: false,
        connectable: false,
      })),
    [graph.nodes, onSelect, selected],
  );
  const edges: Edge[] = useMemo(
    () =>
      graph.edges.map((edge) => ({
        id: edge.id,
        source: edge.source,
        target: edge.target,
        sourceHandle: edge.sourceHandle,
        markerEnd: { type: MarkerType.ArrowClosed },
        selected: selected.kind === "route" && selected.id === edge.route.route_id,
        data: { route_id: edge.route.route_id },
        style: { strokeWidth: selected.kind === "route" && selected.id === edge.route.route_id ? 2.5 : 1.6 },
      })),
    [graph.edges, selected],
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
      nodes={nodes}
      edges={edges}
      nodeTypes={STATE_NODE_TYPES}
      fitView
      nodesDraggable={false}
      nodesConnectable={false}
      elementsSelectable
      onNodeClick={(_, node) => onSelect({ kind: "state", id: node.id })}
      onEdgeClick={(_, edge) => onSelect({ kind: "route", id: String(edge.data?.route_id ?? edge.id) })}
      onPaneClick={() => onSelect({ kind: "scene" })}
      proOptions={{ hideAttribution: true }}
    >
      <Background gap={18} size={1} />
      <MiniMap pannable={false} zoomable={false} />
      <Controls showInteractive={false} />
    </ReactFlow>
  );
}

export function SceneFlowView({
  scenes,
  entrySceneId,
  selectedSceneId,
  onSelectScene,
}: {
  scenes: SceneDocument[];
  entrySceneId: string | null;
  selectedSceneId: string | null;
  onSelectScene: (sceneId: string) => void;
}) {
  const graph = useMemo(() => buildSceneFlowGraphModel(scenes, entrySceneId), [entrySceneId, scenes]);
  const nodes: Node[] = useMemo(
    () =>
      graph.nodes.map((node) => ({
        id: node.id,
        type: "sceneCard",
        position: { x: node.x, y: node.y },
        data: { graphNode: node, onSelectScene },
        selected: selectedSceneId === node.id,
        draggable: false,
        connectable: false,
      })),
    [graph.nodes, onSelectScene, selectedSceneId],
  );
  const edges: Edge[] = useMemo(
    () =>
      graph.edges.map((edge) => ({
        id: edge.id,
        source: edge.source,
        target: edge.target,
        label: edge.label,
        markerEnd: { type: MarkerType.ArrowClosed },
        style: { strokeWidth: 1.7 },
      })),
    [graph.edges],
  );

  if (scenes.length === 0) {
    return (
      <div className="graph-empty">
        <Network size={22} aria-hidden="true" />
        <span>Open a project to inspect scene flow.</span>
      </div>
    );
  }

  return (
    <ReactFlow
      nodes={nodes}
      edges={edges}
      nodeTypes={SCENE_NODE_TYPES}
      fitView
      nodesDraggable={false}
      nodesConnectable={false}
      elementsSelectable
      onNodeClick={(_, node) => onSelectScene(node.id)}
      proOptions={{ hideAttribution: true }}
    >
      <Background gap={18} size={1} />
      <MiniMap pannable={false} zoomable={false} />
      <Controls showInteractive={false} />
    </ReactFlow>
  );
}

export function SceneAuthoringInspector({
  scene,
  selection,
  onSelect,
  onRenameState,
  onSetRouteTarget,
  onSetRouteGuard,
  onSetRouteAction,
  canEdit,
}: {
  scene: SceneDocument | null;
  selection: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onRenameState: (sceneId: string, stateId: string, displayName: string) => Promise<void>;
  onSetRouteTarget: (sceneId: string, routeId: string, targetState: string) => Promise<void>;
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
          inputActions={inputActions}
          variables={variables}
          onSetRouteTarget={onSetRouteTarget}
          onSetRouteGuard={onSetRouteGuard}
          onSetRouteAction={onSetRouteAction}
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
                <strong>{item.from_states.join(", ")} {"->"} {routeTargetLabel(item, states)}</strong>
                <small>{item.guards.length === 0 ? "always allowed" : `${item.guards.length} condition${item.guards.length === 1 ? "" : "s"}`}; {item.actions.length} effect{item.actions.length === 1 ? "" : "s"}</small>
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
  const render = renderModels.find((item) => item.visual_id === state.render_model_ref);
  const waiting = waitingVisuals.find((item) => item.waiting_visual_id === state.waiting_visual_ref);
  const [displayName, setDisplayName] = useState(state.display_name);
  const screenElementCount = render?.elements.length ?? 0;
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
          <span>Screen</span>
          <strong>{state.display_name}</strong>
        </div>
        <div>
          <span>Draws</span>
          <strong>{screenElementCount} element{screenElementCount === 1 ? "" : "s"}</strong>
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
      <button className="link-row" type="button" onClick={() => onSelect({ kind: "render", id: state.render_model_ref })}>
        Screen layout <strong>{render === undefined ? "Missing" : `${render.elements.length} element${render.elements.length === 1 ? "" : "s"}`}</strong>
      </button>
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
  inputActions,
  variables,
  onSetRouteTarget,
  onSetRouteGuard,
  onSetRouteAction,
  canEdit,
}: {
  sceneId: string;
  route: StateRoute;
  states: StateRecord[];
  inputActions: InputAction[];
  variables: StateVariable[];
  onSetRouteTarget: (sceneId: string, routeId: string, targetState: string) => Promise<void>;
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
  canEdit: boolean;
}) {
  const input = inputActions.find((item) => item.action_id === route.action_ref);
  const exitsScene = route.target_scene !== undefined;
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
          <strong>{route.from_states.map((stateId) => displayStateName(states, stateId)).join(", ")}</strong>
        </div>
        <div>
          <span>Go to</span>
          <strong>{routeTargetLabel(route, states)}</strong>
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
                {state.display_name} ({state.state_id})
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
        canEdit={canEdit}
        onSetRouteAction={onSetRouteAction}
      />
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
          <div className="guard-editor-row" key={`${route.route_id}-guard-${index}`}>
            <div className="rule-row-heading">Rule {index + 1}</div>
            <div className="rule-field-grid">
              <label>
                Variable
                <select
                  aria-label={`Guard ${index + 1} variable`}
                  value={guard.variable_ref}
                  disabled={!canEdit}
                  onChange={(event) => commit(event.target.value, guard.operator, guard.value)}
                >
                  {variables.map((variable) => (
                    <option key={variable.variable_id} value={variable.variable_id}>
                      {variable.variable_id}
                    </option>
                  ))}
                </select>
              </label>
              <label>
                Check
                <select
                  aria-label={`Guard ${index + 1} operator`}
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
              </label>
              <label>
                Number
                <input
                  aria-label={`Guard ${index + 1} value`}
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
              </label>
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
  canEdit,
  onSetRouteAction,
}: {
  sceneId: string;
  route: StateRoute;
  variables: StateVariable[];
  canEdit: boolean;
  onSetRouteAction: (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => Promise<void>;
}) {
  if (route.actions.length === 0) {
    return <EmptyInspector>No actions.</EmptyInspector>;
  }
  return (
    <div className="action-editor-list">
      {route.actions.map((action, index) => {
        const variableRef = action.variable_ref ?? variables[0]?.variable_id ?? "";
        const operation = action.operation === "add" ? "add" : "assign";
        const value = typeof action.value === "number" ? action.value : 0;
        const isAdd = operation === "add";
        const commit = (nextAction: Record<string, unknown>) => {
          void onSetRouteAction(sceneId, route.route_id, index, nextAction);
        };
        return (
          <div className="action-editor-row" key={`${route.route_id}-action-${index}`}>
            <div className="rule-row-heading">Effect {index + 1}</div>
            <label className="effect-kind-field">
              What happens
              <select
                aria-label={`Action ${index + 1} kind`}
                value={action.kind}
                disabled={!canEdit}
                onChange={(event) => {
                  if (event.target.value === "request_render") {
                    commit({ kind: "request_render" });
                  } else {
                    commit({ kind: "set_variable", variable_ref: variableRef, operation, value });
                  }
                }}
              >
                <option value="set_variable">Change variable</option>
                <option value="request_render">Refresh screen</option>
              </select>
            </label>
            {action.kind === "set_variable" ? (
              <div className="rule-field-grid">
                <label>
                  Variable
                  <select
                    aria-label={`Action ${index + 1} variable`}
                    value={variableRef}
                    disabled={!canEdit || variables.length === 0}
                    onChange={(event) => {
                      commit({ kind: "set_variable", variable_ref: event.target.value, operation, value });
                    }}
                  >
                    {variables.map((variable) => (
                      <option key={variable.variable_id} value={variable.variable_id}>
                        {variable.variable_id}
                      </option>
                    ))}
                  </select>
                </label>
                <label>
                  How
                  <select
                    aria-label={`Action ${index + 1} operation`}
                    value={operation}
                    disabled={!canEdit}
                    onChange={(event) => {
                      commit({ kind: "set_variable", variable_ref: variableRef, operation: event.target.value, value });
                    }}
                  >
                    {ACTION_OPERATIONS.map((item) => (
                      <option key={item} value={item}>
                        {item === "assign" ? "Set to" : "Change by"}
                      </option>
                    ))}
                  </select>
                </label>
                <label>
                  {isAdd ? "Amount" : "Value"}
                  <input
                    aria-label={isAdd ? `Effect ${index + 1} change amount` : `Effect ${index + 1} target value`}
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
                </label>
              </div>
            ) : (
              <span className="action-static-note">Refresh the screen.</span>
            )}
          </div>
        );
      })}
    </div>
  );
}

function RenderInspector({ render }: { render: RenderModel }) {
  return (
    <section className="inspector-section selected-record">
      <h3>Selected render model</h3>
      <InspectorList rows={[["ID", render.visual_id], ["Focus index", render.focus_index], ["Elements", render.elements.length]]} />
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
      <h3>Selected waiting visual</h3>
      <InspectorList
        rows={[
          ["ID", waiting.waiting_visual_id],
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
