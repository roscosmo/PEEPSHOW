import {
  Background,
  Controls,
  MarkerType,
  MiniMap,
  ReactFlow,
  type Edge,
  type Node,
} from "@xyflow/react";
import { GitBranch, Hourglass, Layers3, Network, Route, Variable } from "lucide-react";
import { useEffect, useMemo, useState } from "react";
import { buildStateGraphModel } from "./stateGraph";
import type {
  InputAction,
  RenderModel,
  SceneDocument,
  StateAction,
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

function ActionList({ actions }: { actions: StateAction[] }) {
  if (actions.length === 0) {
    return <EmptyInspector>No actions.</EmptyInspector>;
  }
  return (
    <ol className="ordered-records">
      {actions.map((action, index) => (
        <li key={`${action.kind}-${index}`}>
          <code>{action.kind}</code>
          {action.variable_ref !== undefined && <> {action.variable_ref}</>}
          {action.operation !== undefined && <> {action.operation}</>}
          {action.value !== undefined && <> {action.value}</>}
        </li>
      ))}
    </ol>
  );
}

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
        position: { x: node.x, y: node.y },
        data: {
          label: (
            <div className={`state-node ${node.isEntry ? "entry" : ""}`}>
              <span>{node.isEntry ? "Entry" : "State"}</span>
              <strong>{node.label}</strong>
              <small>{node.id}</small>
            </div>
          ),
        },
        selected: selected.kind === "state" && selected.id === node.id,
        draggable: false,
        connectable: false,
      })),
    [graph.nodes, selected],
  );
  const edges: Edge[] = useMemo(
    () =>
      graph.edges.map((edge) => ({
        id: edge.id,
        source: edge.source,
        target: edge.target,
        label: edge.label,
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

export function SceneAuthoringInspector({
  scene,
  selection,
  onSelect,
  onRenameState,
  canEdit,
}: {
  scene: SceneDocument | null;
  selection: SceneSelection;
  onSelect: (selection: SceneSelection) => void;
  onRenameState: (sceneId: string, stateId: string, displayName: string) => Promise<void>;
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
      <section className="inspector-section">
        <h3><GitBranch size={14} aria-hidden="true" /> Scene graph</h3>
        {scene === null ? (
          <EmptyInspector>No normalized scene selected.</EmptyInspector>
        ) : (
          <InspectorList
            rows={[
              ["Scene", scene.scene_id],
              ["Type", scene.scene_type],
              ["Entry state", scene.entry_state],
              ["States", states.length],
              ["Routes", routes.length],
            ]}
          />
        )}
      </section>

      <section className="inspector-section">
        <h3><Variable size={14} aria-hidden="true" /> Variables</h3>
        {variables.length === 0 ? (
          <EmptyInspector>No authored variables.</EmptyInspector>
        ) : (
          <div className="record-list">
            {variables.map((variable: StateVariable) => (
              <button key={variable.variable_id} className="record-row" type="button">
                <strong>{variable.variable_id}</strong>
                <small>{variable.initial} in {variable.minimum}..{variable.maximum}</small>
              </button>
            ))}
          </div>
        )}
      </section>

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
      {route !== null && <RouteInspector route={route} inputActions={inputActions} />}
      {render !== null && <RenderInspector render={render} />}
      {waiting !== null && <WaitingInspector waiting={waiting} />}

      {selection.kind === "scene" && (
        <>
          <section className="inspector-section">
            <h3><Route size={14} aria-hidden="true" /> Routes</h3>
            {routes.length === 0 ? (
              <EmptyInspector>No routes.</EmptyInspector>
            ) : (
              <div className="record-list">
                {routes.map((item: StateRoute) => (
                  <button key={item.route_id} className="record-row" type="button" onClick={() => onSelect({ kind: "route", id: item.route_id })}>
                    <strong>{item.route_id}</strong>
                    <small>{item.from_states.join(", ")} {">"} {item.target_state}</small>
                  </button>
                ))}
              </div>
            )}
          </section>

          <section className="inspector-section">
            <h3><Layers3 size={14} aria-hidden="true" /> Render models</h3>
            {renderModels.length === 0 ? (
              <EmptyInspector>No render models.</EmptyInspector>
            ) : (
              <div className="record-list">
                {renderModels.map((item: RenderModel) => (
                  <button key={item.visual_id} className="record-row" type="button" onClick={() => onSelect({ kind: "render", id: item.visual_id })}>
                    <strong>{item.visual_id}</strong>
                    <small>{item.elements.length} elements</small>
                  </button>
                ))}
              </div>
            )}
          </section>

          <section className="inspector-section">
            <h3><Hourglass size={14} aria-hidden="true" /> Waiting visuals</h3>
            {waitingVisuals.length === 0 ? (
              <EmptyInspector>No waiting visuals.</EmptyInspector>
            ) : (
              <div className="record-list">
                {waitingVisuals.map((item: WaitingVisual) => (
                  <button key={item.waiting_visual_id} className="record-row" type="button" onClick={() => onSelect({ kind: "waiting", id: item.waiting_visual_id })}>
                    <strong>{item.waiting_visual_id}</strong>
                    <small>{item.combined_step_count} steps at {item.phase_quantum_ms} ms</small>
                  </button>
                ))}
              </div>
            )}
          </section>
        </>
      )}
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

  useEffect(() => {
    setDisplayName(state.display_name);
  }, [state.display_name, state.state_id]);

  const trimmed = displayName.trim();
  const renameDisabled = !canEdit || trimmed.length === 0 || trimmed === state.display_name;

  return (
    <section className="inspector-section selected-record">
      <h3>Selected state</h3>
      <InspectorList rows={[["ID", state.state_id], ["Name", state.display_name]]} />
      <form
        className="rename-form"
        onSubmit={(event) => {
          event.preventDefault();
          if (!renameDisabled) {
            void onRenameState(sceneId, state.state_id, trimmed);
          }
        }}
      >
        <label htmlFor={`state-name-${state.state_id}`}>Display name</label>
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
        Render model <strong>{render?.visual_id ?? state.render_model_ref}</strong>
      </button>
      <button className="link-row" type="button" onClick={() => onSelect({ kind: "waiting", id: state.waiting_visual_ref })}>
        Waiting visual <strong>{waiting?.waiting_visual_id ?? state.waiting_visual_ref}</strong>
      </button>
    </section>
  );
}

function RouteInspector({ route, inputActions }: { route: StateRoute; inputActions: InputAction[] }) {
  const input = inputActions.find((item) => item.action_id === route.action_ref);
  return (
    <section className="inspector-section selected-record">
      <h3>Selected route</h3>
      <InspectorList
        rows={[
          ["ID", route.route_id],
          ["Action", route.action_ref],
          ["Logical source", input?.logical_source],
          ["From", route.from_states.join(", ")],
          ["Target", route.target_state],
        ]}
      />
      <h4>Guards</h4>
      <GuardList guards={route.guards} />
      <h4>Ordered actions</h4>
      <ActionList actions={route.actions} />
    </section>
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
