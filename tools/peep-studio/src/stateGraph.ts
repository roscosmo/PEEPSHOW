import type { SceneDocument, StateRecord, StateRoute } from "./types";

export type GraphStateNode = {
  id: string;
  label: string;
  isEntry: boolean;
  renderModelRef: string;
  waitingVisualRef: string;
  x: number;
  y: number;
};

export type GraphTransitionEdge = {
  id: string;
  source: string;
  target: string;
  label: string;
  route: StateRoute;
};

export type StateGraphModel = {
  nodes: GraphStateNode[];
  edges: GraphTransitionEdge[];
};

function routeLabel(route: StateRoute): string {
  const guards = route.guards.length > 0 ? ` +${route.guards.length} guard` : "";
  const actions = route.actions.length > 0 ? ` / ${route.actions.length} action` : "";
  return `${route.action_ref}${guards}${actions}`;
}

export function buildStateGraphModel(scene: SceneDocument | null): StateGraphModel {
  const states = scene?.states ?? [];
  const routes = scene?.routes ?? [];
  const entryState = scene?.entry_state ?? null;
  const columns = Math.max(1, Math.ceil(Math.sqrt(states.length)));

  const nodes = states.map((state: StateRecord, index: number) => ({
    id: state.state_id,
    label: state.display_name,
    isEntry: state.state_id === entryState,
    renderModelRef: state.render_model_ref,
    waitingVisualRef: state.waiting_visual_ref,
    x: (index % columns) * 220,
    y: Math.floor(index / columns) * 150,
  }));

  const stateIds = new Set(nodes.map((node) => node.id));
  const edges = routes.flatMap((route: StateRoute) =>
    route.from_states
      .filter((source) => stateIds.has(source) && stateIds.has(route.target_state))
      .map((source) => ({
        id: `${route.route_id}:${source}->${route.target_state}`,
        source,
        target: route.target_state,
        label: routeLabel(route),
        route,
      })),
  );

  return { nodes, edges };
}
