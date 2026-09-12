"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const strict_1 = __importDefault(require("node:assert/strict"));
const timerAuthoring_js_1 = require("../src/timerAuthoring.js");
const scene = { scene_id: "main", display_name: "Main", scene_type: "STATE_SCENE", schema_version: 2,
    input_actions: [{ action_id: "scene_timer", logical_source: "BUTTON_A" }],
    reactive_wait_default: { policy_id: "wait", hold_fallback_allowed: true, event_interests: ["a"] },
};
const created = (0, timerAuthoring_js_1.createTimerCommands)(scene, timerAuthoring_js_1.SCENE_TIMER, 1000);
strict_1.default.deepEqual(created.map(c => c.kind), ["event_binding.add", "event_handler.add", "scene.set_reactive_wait_default"]);
strict_1.default.equal(created[0].event_binding.binding_id, "scene_timer_2");
(0, strict_1.default)(!("target_state" in created[1].event_handler));
strict_1.default.deepEqual(scene.reactive_wait_default?.event_interests, ["a"]);
const state = (0, timerAuthoring_js_1.createTimerCommands)(scene, timerAuthoring_js_1.STATE_TIMER, 500, "one", "two");
strict_1.default.equal(state[1].kind, "route.add");
strict_1.default.deepEqual(state[1].route.from_states, ["one"]);
const deleted = (0, timerAuthoring_js_1.deleteTimerCommands)({ ...scene, event_handlers: [{ handler_id: "expiry", event_ref: "tick", guards: [], actions: [] }],
    reactive_wait_default: { ...scene.reactive_wait_default, event_interests: ["a", "tick"] } }, "tick");
strict_1.default.deepEqual(deleted.map(c => c.kind), ["scene.set_reactive_wait_default", "event_handler.delete", "event_binding.delete"]);
const policyDelete = (0, timerAuthoring_js_1.deleteTimerCommands)({ ...scene, interaction_policy: { policy_id: "interaction", mode: "continuous", meaningful_activity_actions: ["a", "tick"] } }, "tick");
strict_1.default.deepEqual(policyDelete.find(c => c.kind === "scene.set_interaction_policy")?.interaction_policy, { policy_id: "interaction", mode: "continuous", meaningful_activity_actions: ["a"] });
const host = { target_profiles: { available: [{ profile_id: "hw6", state_scene_events: { sources: [{
                            event_type: timerAuthoring_js_1.SCENE_TIMER, status: "available_pending_validation", configuration_schema: { delay_ms: { minimum: 10, maximum: 5000 } },
                        }] } }] } };
strict_1.default.deepEqual((0, timerAuthoring_js_1.timerBounds)(host, "hw6", timerAuthoring_js_1.SCENE_TIMER), { minimum: 10, maximum: 5000 });
strict_1.default.equal((0, timerAuthoring_js_1.timerBounds)(host, "unknown", timerAuthoring_js_1.SCENE_TIMER), undefined);
strict_1.default.equal((0, timerAuthoring_js_1.timerBounds)(host, "hw6", timerAuthoring_js_1.STATE_TIMER), undefined);
host.target_profiles.available[0].state_scene_events.sources[0].status = "contracted_not_exposed";
strict_1.default.equal((0, timerAuthoring_js_1.timerBounds)(host, "hw6", timerAuthoring_js_1.SCENE_TIMER), undefined);
console.log("Timer command batches and capability bounds passed");
