import type { InputAction, SceneDocument, StateRecord, StateRoute } from "./types";

export type GraphStateOutput = {
  id: string;
  routeId: string;
  label: string;
  guardCount: number;
  actionCount: number;
  targetState?: string;
  targetScene?: string;
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

export type GraphSceneNode = {
  id: string;
  label: string;
  isEntry: boolean;
  stateCount: number;
  routeCount: number;
  entryStateLabel: string;
  x: number;
  y: number;
};

export type GraphSceneEdge = {
  id: string;
  source: string;
  target: string;
  label: string;
  route: StateRoute;
};

export type SceneFlowGraphModel = {
  nodes: GraphSceneNode[];
  edges: GraphSceneEdge[];
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
      .filter((source) => stateIds.has(source) && (route.target_scene !== undefined || (route.target_state !== undefined && stateIds.has(route.target_state))))
      .forEach((source) => {
        const outputs = outputsByState.get(source) ?? [];
        outputs.push({
          id: `${route.route_id}:${source}`,
          routeId: route.route_id,
          label: inputLabel(inputActions, route.action_ref),
          guardCount: route.guards.length,
          actionCount: route.actions.length,
          targetState: route.target_state,
          targetScene: route.target_scene,
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

  const edges = routes.flatMap((route: StateRoute) => {
    const targetState = route.target_state;
    if (targetState === undefined || !stateIds.has(targetState)) {
      return [];
    }
    return route.from_states
      .filter((source) => stateIds.has(source))
      .map((source) => ({
        id: `${route.route_id}:${source}->${targetState}`,
        source,
        target: targetState,
        label: "",
        route,
        sourceHandle: `${route.route_id}:${source}`,
      }));
  });

  return { nodes, edges };
}

export function buildSceneFlowGraphModel(
  scenes: SceneDocument[],
  entrySceneId: string | null,
): SceneFlowGraphModel {
  const sceneIds = new Set(scenes.map((scene) => scene.scene_id));
  const edges = scenes.flatMap((scene) =>
    (scene.routes ?? [])
      .filter((route) => route.target_scene !== undefined && sceneIds.has(route.target_scene))
      .map((route) => ({
        id: `${scene.scene_id}:${route.route_id}->${route.target_scene}`,
        source: scene.scene_id,
        target: route.target_scene!,
        label: routeLabel(route, scene.input_actions ?? []),
        route,
      })),
  );

  const outgoingCounts = new Map<string, number>();
  edges.forEach((edge) => {
    outgoingCounts.set(edge.source, (outgoingCounts.get(edge.source) ?? 0) + 1);
  });

  const incomingTargets = new Set(edges.map((edge) => edge.target));
  const nodes = scenes.map((scene, index) => {
    const states = scene.states ?? [];
    const entryState = states.find((state) => state.state_id === scene.entry_state);
    const isEntry = scene.scene_id === entrySceneId;
    const column = isEntry || !incomingTargets.has(scene.scene_id) ? 0 : 1 + (index % 3);
    return {
      id: scene.scene_id,
      label: scene.display_name,
      isEntry,
      stateCount: states.length,
      routeCount: outgoingCounts.get(scene.scene_id) ?? 0,
      entryStateLabel: entryState?.display_name ?? scene.entry_state ?? "No start state",
      x: column * 300,
      y: index * 154,
    };
  });

  return { nodes, edges };
}
