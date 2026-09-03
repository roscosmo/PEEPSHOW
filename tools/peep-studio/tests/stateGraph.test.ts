import assert from "node:assert/strict";
import {
  buildSceneFlowGraphModel,
  buildStateGraphModel,
  buildStateTransitionRoute,
  insertStateTransitionRouteSection,
  moveStateTransitionRouteSection,
  planStateTransitionRoutes,
  resolveStateEntryHandle,
  resolveStateExitSide,
  stateActionDescription,
  stateEntryPortPoint,
  stateGuardDescription,
  stateTransitionRouteSections,
} from "../src/stateGraph";
import type { SceneDocument } from "../src/types";

const scene: SceneDocument = {
  scene_id: "menu",
  display_name: "Menu",
  scene_type: "STATE_SCENE",
  entry_state: "idle",
  states: [
    {
      state_id: "idle",
      display_name: "Idle",
      render_model_ref: "idle_visual",
      waiting_visual_ref: "idle_wait",
    },
    {
      state_id: "armed",
      display_name: "Armed",
      render_model_ref: "armed_visual",
      waiting_visual_ref: "armed_wait",
    },
  ],
  input_actions: [
    {
      action_id: "select",
      logical_source: "BUTTON_A",
    },
  ],
  routes: [
    {
      route_id: "press_a",
      action_ref: "select",
      from_states: ["idle"],
      guards: [{ variable_ref: "coins", operator: "gt", value: 0 }],
      actions: [
        { kind: "set_variable", variable_ref: "coins", operation: "add", value: -1 },
        { kind: "request_render" },
      ],
      target_state: "armed",
    },
  ],
};

const graph = buildStateGraphModel(scene);
const savedGraph = buildStateGraphModel(scene, {
  state_graph: {
    scenes: {
      menu: {
        nodes: {
          armed: { x: -120, y: 220 },
        },
        routes: {
          press_a: {
            sources: {
              idle: {
                routing_version: 3,
                target_handle: "entry-bottom-right",
                target_side: "right",
                rails: [{ axis: "x", value: 180 }, { axis: "y", value: 90 }],
              },
            },
          },
        },
      },
    },
  },
});
const legacyRouteGraph = buildStateGraphModel(scene, {
  state_graph: {
    scenes: {
      menu: {
        nodes: {},
        routes: {
          press_a: {
            sources: {
              idle: {
                routing_version: 2,
                waypoints: [{ x: 900, y: 900 }, { x: 920, y: 940 }],
              },
            },
          },
        },
      },
    },
  },
});

assert.equal(graph.nodes.length, 2);
assert.equal(graph.endpoints.length, 1);
assert.equal(graph.endpoints[0]?.kind, "entry");
assert.equal(graph.entryEdge?.target, "idle");
assert.equal(graph.nodes[0]?.id, "idle");
assert.equal(graph.nodes[0]?.isEntry, true);
assert.equal(graph.nodes[0]?.outputs.length, 1);
assert.equal(graph.nodes[0]?.outputs[0]?.label, "Button A");
assert.equal(graph.nodes[0]?.outputs[0]?.guardCount, 1);
assert.equal(graph.nodes[0]?.outputs[0]?.actionCount, 1);
assert.equal(graph.nodes[0]?.outputs[0]?.effectLabels[0], "Coins -1");
assert.equal(graph.nodes[0]?.outputs[0]?.eventKind, "press");
assert.equal(graph.nodes[0]?.outputs[0]?.triggerKind, "physical");
assert.equal(graph.nodes[0]?.outputs[0]?.preferredExitSide, "right");
assert.equal(graph.nodes[0]?.outputs[0]?.exitRatio, 0.67);
assert.equal(graph.nodes[0]?.variableTouchCount, 1);
assert.equal(graph.nodes[1]?.isEntry, false);
assert.equal(graph.edges.length, 1);
assert.equal(graph.edges[0]?.source, "idle");
assert.equal(graph.edges[0]?.target, "armed");
assert.equal(graph.edges[0]?.label, "");
assert.equal(graph.edges[0]?.sourceHandle, "press_a:idle");
assert.equal(graph.edges[0]?.effectLabels[0], "Coins -1");
assert.equal(graph.edges[0]?.guards[0]?.variable_ref, "coins");
assert.equal(graph.edges[0]?.actions[0]?.kind, "set_variable");
assert.deepEqual(savedGraph.edges[0]?.rails, [{ axis: "x", value: 180 }, { axis: "y", value: 90 }]);
assert.equal(savedGraph.edges[0]?.targetHandle, "entry-bottom-right");
assert.equal(savedGraph.edges[0]?.targetSide, "right");
assert.deepEqual(legacyRouteGraph.edges[0]?.rails, []);
assert.equal(legacyRouteGraph.edges[0]?.targetHandle, undefined);
assert.equal(savedGraph.nodes.find((node) => node.id === "armed")?.x, -120);
assert.equal(savedGraph.nodes.find((node) => node.id === "armed")?.y, 220);

const lifecycleGraph = buildStateGraphModel({
  ...scene,
  input_actions: [
    ...scene.input_actions!,
    { action_id: "hold_b", logical_source: "BUTTON_B", event_kind: "hold" },
  ],
  routes: [
    ...scene.routes!,
    {
      route_id: "hold_b",
      action_ref: "hold_b",
      from_states: ["idle"],
      guards: [],
      actions: [],
      target_state: "armed",
    },
  ],
});
const holdOutput = lifecycleGraph.nodes[0]?.outputs.find((output) => output.routeId === "hold_b");
assert.equal(holdOutput?.label, "Button B held");
assert.equal(holdOutput?.eventKind, "hold");
assert.equal(holdOutput?.triggerKind, "physical");
assert.equal(lifecycleGraph.nodes[0]?.platformOutputCount, 0);

assert.equal(resolveStateExitSide({ x: 0, y: 0 }, { x: 340, y: 0 }), "right");
assert.equal(resolveStateExitSide({ x: 0, y: 0 }, { x: -120, y: 220 }), "left");
assert.equal(resolveStateEntryHandle({ x: 0, y: 0 }, { x: 340, y: 0 }), "entry-top-left");
assert.equal(resolveStateEntryHandle({ x: 0, y: 400 }, { x: 0, y: 0 }), "entry-bottom-right");
assert.deepEqual(stateEntryPortPoint({ x: 100, y: 200 }, "entry-top-left", "top"), { x: 108, y: 200 });
assert.deepEqual(stateEntryPortPoint({ x: 100, y: 200 }, "entry-top-left", "left"), { x: 100, y: 209 });

const verticalStackRoute = buildStateTransitionRoute({
  sourceX: 280,
  sourceY: 160,
  targetX: 140,
  targetY: 300,
  sourceSide: "right",
  targetSide: "top",
});
assert.equal(verticalStackRoute.points.some((point) => point.x > 340), true);
assert.equal(verticalStackRoute.points.at(-2)?.y, 252);
assert.equal(verticalStackRoute.path.startsWith("M 280 160"), true);

const verticalReturnRoute = buildStateTransitionRoute({
  sourceX: 280,
  sourceY: 460,
  targetX: 140,
  targetY: 210,
  sourceSide: "right",
  targetSide: "bottom",
});
assert.equal(verticalReturnRoute.points.some((point) => point.x > 340), true);
assert.equal(verticalReturnRoute.points.at(-2)?.y, 258);

const topTriggerRoute = buildStateTransitionRoute({
  sourceX: 66,
  sourceY: 0,
  targetX: 360,
  targetY: 300,
  sourceSide: "top",
  targetSide: "top",
});
assert.equal(topTriggerRoute.points[1]?.x, 66);
assert.equal((topTriggerRoute.points[1]?.y ?? 0) < 0, true);

const sideEntryRoute = buildStateTransitionRoute({
  sourceX: 40,
  sourceY: 100,
  targetX: 300,
  targetY: 200,
  sourceSide: "bottom",
  targetSide: "left",
});
assert.deepEqual(sideEntryRoute.points.at(-1), { x: 300, y: 200 });
assert.equal(sideEntryRoute.points.at(-2)?.y, 200);
assert.equal((sideEntryRoute.points.at(-2)?.x ?? 300) < 300, true);
assert.deepEqual(sideEntryRoute.rails, [{ axis: "y", value: 272 }, { axis: "x", value: 252 }]);
const movedSideEntryRails = moveStateTransitionRouteSection(
  sideEntryRoute.controlPoints,
  sideEntryRoute.controlPoints.length - 2,
  { x: 280, y: 140 },
  "bottom",
  "left",
);
assert.notEqual(movedSideEntryRails, null);
const movedSideEntryRoute = buildStateTransitionRoute({
  sourceX: 40,
  sourceY: 100,
  targetX: 300,
  targetY: 200,
  sourceSide: "bottom",
  targetSide: "left",
  rails: movedSideEntryRails ?? undefined,
});
assert.equal(movedSideEntryRoute.points.some((point) => point.y === 140), true);
assert.deepEqual(movedSideEntryRoute.points.at(-1), { x: 300, y: 200 });

const manualRoute = buildStateTransitionRoute({
  sourceX: 280,
  sourceY: 160,
  targetX: 140,
  targetY: 300,
  sourceSide: "right",
  targetSide: "top",
  rails: [{ axis: "x", value: 410 }, { axis: "y", value: 264 }],
});
assert.equal(manualRoute.points.some((point) => point.x === 410 && point.y === 160), true);
assert.equal(manualRoute.points.some((point) => point.x === 410 && point.y === 264), true);
assert.equal(manualRoute.path.startsWith("M 280 160"), true);
assert.deepEqual(verticalStackRoute.rails, [{ axis: "x", value: 352 }, { axis: "y", value: 252 }]);
assert.equal(manualRoute.controlPoints.length, 5);
const automaticSections = stateTransitionRouteSections(verticalStackRoute.controlPoints);
assert.equal(automaticSections.length, 4);
assert.deepEqual(automaticSections.map((section) => section.orientation), ["horizontal", "vertical", "horizontal", "vertical"]);
const movedSourceRails = moveStateTransitionRouteSection(
  verticalStackRoute.controlPoints,
  0,
  { x: 330, y: 110 },
  "right",
  "top",
);
assert.notEqual(movedSourceRails, null);
const movedSourceRoute = buildStateTransitionRoute({
  sourceX: 280,
  sourceY: 160,
  targetX: 140,
  targetY: 300,
  sourceSide: "right",
  targetSide: "top",
  rails: movedSourceRails ?? undefined,
});
assert.equal(movedSourceRoute.points.some((point) => point.y === 110), true);
assert.equal(
  movedSourceRoute.points.slice(1).every((point, index) => {
    const previous = movedSourceRoute.points[index];
    return previous.x === point.x || previous.y === point.y;
  }),
  true,
);
const movedTargetRails = moveStateTransitionRouteSection(
  verticalStackRoute.controlPoints,
  verticalStackRoute.controlPoints.length - 2,
  { x: 240, y: 330 },
  "right",
  "top",
);
assert.notEqual(movedTargetRails, null);
const movedTargetSectionRoute = buildStateTransitionRoute({
  sourceX: 280,
  sourceY: 160,
  targetX: 140,
  targetY: 300,
  sourceSide: "right",
  targetSide: "top",
  rails: movedTargetRails ?? undefined,
});
assert.equal(movedTargetSectionRoute.points.some((point) => point.x === 240), true);
assert.deepEqual(movedTargetSectionRoute.points.at(-1), { x: 140, y: 300 });
const movedMiddleRails = moveStateTransitionRouteSection(
  verticalStackRoute.controlPoints,
  1,
  { x: 460, y: 220 },
  "right",
  "top",
);
assert.deepEqual(movedMiddleRails, [{ axis: "x", value: 460 }, { axis: "y", value: 252 }]);
const insertedSection = insertStateTransitionRouteSection(
  verticalStackRoute.controlPoints,
  1,
  { x: 352, y: 212 },
  "top",
);
assert.notEqual(insertedSection, null);
const insertedRoute = buildStateTransitionRoute({
  sourceX: 280,
  sourceY: 160,
  targetX: 140,
  targetY: 300,
  sourceSide: "right",
  targetSide: "top",
  rails: insertedSection ?? undefined,
});
assert.equal(stateTransitionRouteSections(insertedRoute.controlPoints).length > automaticSections.length, true);
assert.equal(
  insertStateTransitionRouteSection(
    verticalStackRoute.controlPoints,
    1,
    { x: 352, y: 212 },
    "top",
    2,
  ),
  null,
);
const movedCardRoute = buildStateTransitionRoute({
  sourceX: 520,
  sourceY: 90,
  targetX: 180,
  targetY: 360,
  sourceSide: "right",
  targetSide: "top",
  rails: manualRoute.rails,
});
assert.equal((movedCardRoute.points[1]?.x ?? 0) > 520, true);
assert.equal(
  movedCardRoute.points.slice(1).every((point, index) => {
    const previous = movedCardRoute.points[index];
    return previous.x === point.x || previous.y === point.y;
  }),
  true,
);
const movedTargetRoute = buildStateTransitionRoute({
  sourceX: 280,
  sourceY: 160,
  targetX: 140,
  targetY: 100,
  sourceSide: "right",
  targetSide: "top",
  rails: manualRoute.rails,
});
assert.equal((movedTargetRoute.points.at(-2)?.y ?? 100) <= 76, true);
const selfCrossingRoute = buildStateTransitionRoute({
  sourceX: 0,
  sourceY: 0,
  targetX: 100,
  targetY: 100,
  sourceSide: "right",
  targetSide: "top",
  rails: [
    { axis: "x", value: 200 },
    { axis: "y", value: 200 },
    { axis: "x", value: -100 },
    { axis: "y", value: 0 },
  ],
});
assert.deepEqual(selfCrossingRoute.points, [{ x: 0, y: 0 }, { x: 100, y: 0 }, { x: 100, y: 100 }]);
assert.equal(stateGuardDescription(scene.routes![0]!.guards[0]!), "Coins is greater than 0");
assert.equal(stateActionDescription(scene.routes![0]!.actions[0]!), "Coins -1");
assert.equal(stateActionDescription(scene.routes![0]!.actions[1]!), null);

const lanePlan = planStateTransitionRoutes(
  [
    { id: "first", source: "top", target: "bottom", sourceOutputIndex: 0 },
    { id: "second", source: "top", target: "bottom", sourceOutputIndex: 1 },
  ],
  [
    { id: "top", x: 0, y: 0 },
    { id: "bottom", x: 0, y: 340 },
  ],
);
assert.notEqual(lanePlan.first?.laneX, lanePlan.second?.laneX);
assert.equal(lanePlan.first?.sourceSide, "right");
assert.equal(lanePlan.second?.targetHandle.startsWith("entry-top"), true);

const fixedTriggerPlan = planStateTransitionRoutes(
  [{ id: "shoulder", source: "top", target: "bottom", sourceOutputIndex: 0, sourceSide: "top", sourceRatio: 0.22 }],
  [
    { id: "top", x: 0, y: 0, platformOutputCount: 0 },
    { id: "bottom", x: 0, y: 420, platformOutputCount: 0 },
  ],
);
assert.equal(fixedTriggerPlan.shoulder?.sourceSide, "top");
assert.equal(fixedTriggerPlan.shoulder?.targetHandle.startsWith("entry-top"), true);

const pinnedEntryPlan = planStateTransitionRoutes(
  [{
    id: "pinned",
    source: "top",
    target: "bottom",
    sourceOutputIndex: 0,
    targetHandle: "entry-bottom-right",
    targetSide: "right",
  }],
  [
    { id: "top", x: 0, y: 0 },
    { id: "bottom", x: 0, y: 420 },
  ],
);
assert.equal(pinnedEntryPlan.pinned?.targetHandle, "entry-bottom-right");
assert.equal(pinnedEntryPlan.pinned?.targetSide, "right");

const invalidRouteGraph = buildStateGraphModel({
  ...scene,
  routes: [{ ...scene.routes![0]!, target_state: "missing" }],
});

assert.equal(invalidRouteGraph.edges.length, 0);

const menuScene: SceneDocument = {
  ...scene,
  scene_exits: [
    {
      scene_exit_id: "to_game",
      display_name: "Game",
      target_scene: "game",
    },
  ],
  routes: [
    {
      route_id: "menu_to_game",
      action_ref: "select",
      from_states: ["idle"],
      guards: [],
      actions: [],
      scene_exit_ref: "to_game",
      target_scene: "game",
    },
    {
      route_id: "menu_local",
      action_ref: "select",
      from_states: ["idle"],
      guards: [],
      actions: [],
      target_state: "armed",
    },
  ],
};

const sceneFlow = buildSceneFlowGraphModel([
  menuScene,
  {
    ...scene,
    scene_id: "game",
    display_name: "Game",
    routes: [],
  },
  {
    ...scene,
    scene_id: "summary",
    display_name: "Summary",
    routes: [
      {
        route_id: "summary_to_menu",
        action_ref: "select",
        from_states: ["idle"],
        guards: [],
        actions: [],
        target_scene: "menu",
      },
    ],
  },
], "menu");

const localWithSceneExit = buildStateGraphModel(menuScene);

assert.equal(localWithSceneExit.nodes.find((node) => node.id === "idle")?.outputs.length, 2);
assert.equal(localWithSceneExit.nodes.find((node) => node.id === "idle")?.outputs.some((output) => output.targetScene === "game"), true);
assert.equal(localWithSceneExit.endpoints.some((endpoint) => endpoint.sceneExitId === "to_game"), true);
assert.equal(localWithSceneExit.edges.length, 2);
assert.equal(localWithSceneExit.edges.some((edge) => edge.target === "scene-exit-to_game"), true);
assert.equal(sceneFlow.nodes.length, 3);
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.isEntry, true);
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits.length, 1);
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.label, "Game");
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.targetScene, "game");
assert.equal(sceneFlow.edges.length, 2);
assert.equal(sceneFlow.edges.some((edge) => edge.source === "menu" && edge.target === "game"), true);
assert.equal(sceneFlow.edges.some((edge) => edge.source === "summary" && edge.target === "menu"), true);
