import assert from "node:assert/strict";
import { buildSceneFlowGraphModel, buildStateGraphModel } from "../src/stateGraph";
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
      actions: [{ kind: "set_variable", variable_ref: "coins", operation: "add", value: -1 }],
      target_state: "armed",
    },
  ],
};

const graph = buildStateGraphModel(scene);

assert.equal(graph.nodes.length, 2);
assert.equal(graph.nodes[0]?.id, "idle");
assert.equal(graph.nodes[0]?.isEntry, true);
assert.equal(graph.nodes[0]?.outputs.length, 1);
assert.equal(graph.nodes[0]?.outputs[0]?.label, "Button A");
assert.equal(graph.nodes[0]?.outputs[0]?.guardCount, 1);
assert.equal(graph.nodes[0]?.outputs[0]?.actionCount, 1);
assert.equal(graph.nodes[1]?.isEntry, false);
assert.equal(graph.edges.length, 1);
assert.equal(graph.edges[0]?.source, "idle");
assert.equal(graph.edges[0]?.target, "armed");
assert.equal(graph.edges[0]?.label, "");
assert.equal(graph.edges[0]?.sourceHandle, "press_a:idle");

const invalidRouteGraph = buildStateGraphModel({
  ...scene,
  routes: [{ ...scene.routes![0]!, target_state: "missing" }],
});

assert.equal(invalidRouteGraph.edges.length, 0);

const menuScene: SceneDocument = {
  ...scene,
  routes: [
    {
      route_id: "menu_to_game",
      action_ref: "select",
      from_states: ["idle"],
      guards: [],
      actions: [],
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
assert.equal(localWithSceneExit.edges.length, 1);
assert.equal(sceneFlow.nodes.length, 3);
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.isEntry, true);
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits.length, 1);
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.label, "Button A");
assert.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.targetScene, "game");
assert.equal(sceneFlow.edges.length, 2);
assert.equal(sceneFlow.edges.some((edge) => edge.source === "menu" && edge.target === "game"), true);
assert.equal(sceneFlow.edges.some((edge) => edge.source === "summary" && edge.target === "menu"), true);
