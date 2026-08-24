import assert from "node:assert/strict";
import { buildStateGraphModel } from "../src/stateGraph";
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
