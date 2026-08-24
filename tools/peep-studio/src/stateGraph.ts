import type { InputAction, SceneDocument, StateRecord, StateRoute } from "./types";

export type GraphStateOutput = {
  id: string;
  routeId: string;
  label: string;
  guardCount: number;
  actionCount: number;
  targetState: string;
};

export type GraphStateNode = {
  id: string;
  label: string;
  isEntry: boolean;
  renderModelRef: string;
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
};

export type StateGraphModel = {
  nodes: GraphStateNode[];
  edges: GraphTransitionEdge[];
};

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

function inputLabel(inputActions: InputAction[], actionRef: string): string {
  const input = inputActions.find((item) => item.action_id === actionRef);
  return INPUT_LABELS[input?.logical_source ?? ""] ?? input?.logical_source ?? actionRef;
}

function countLabel(count: number, singular: string): string {
  if (count === 0) {
    return "";
  }
  return `${count} ${singular}${count === 1 ? "" : "s"}`;
}

function routeLabel(route: StateRoute, inputActions: InputAction[]): string {
  const badges = [countLabel(route.guards.length, "rule"), countLabel(route.actions.length, "effect")].filter(Boolean);
  return [inputLabel(inputActions, route.action_ref), ...badges].join(" - ");
}

export function buildStateGraphModel(scene: SceneDocument | null): StateGraphModel {
  const states = scene?.states ?? [];
  const routes = scene?.routes ?? [];
  const inputActions = scene?.input_actions ?? [];
  const entryState = scene?.entry_state ?? null;
  const columns = Math.max(1, Math.ceil(Math.sqrt(states.length)));
  const stateIds = new Set(states.map((state) => state.state_id));
  const outputsByState = new Map<string, GraphStateOutput[]>();

  routes.forEach((route: StateRoute) => {
    route.from_states
      .filter((source) => stateIds.has(source) && stateIds.has(route.target_state))
      .forEach((source) => {
        const outputs = outputsByState.get(source) ?? [];
        outputs.push({
          id: `${route.route_id}:${source}`,
          routeId: route.route_id,
          label: inputLabel(inputActions, route.action_ref),
          guardCount: route.guards.length,
          actionCount: route.actions.length,
          targetState: route.target_state,
        });
        outputsByState.set(source, outputs);
      });
  });

  const nodes = states.map((state: StateRecord, index: number) => ({
    id: state.state_id,
    label: state.display_name,
    isEntry: state.state_id === entryState,
    renderModelRef: state.render_model_ref,
    waitingVisualRef: state.waiting_visual_ref,
    outputs: outputsByState.get(state.state_id) ?? [],
    x: (index % columns) * 300,
    y: Math.floor(index / columns) * 190,
  }));

  const edges = routes.flatMap((route: StateRoute) =>
    route.from_states
      .filter((source) => stateIds.has(source) && stateIds.has(route.target_state))
      .map((source) => ({
        id: `${route.route_id}:${source}->${route.target_state}`,
        source,
        target: route.target_state,
        label: "",
        route,
        sourceHandle: `${route.route_id}:${source}`,
      })),
  );

  return { nodes, edges };
}
