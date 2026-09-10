import assert from "node:assert/strict";
import { createTimerCommands, deleteTimerCommands, SCENE_TIMER, STATE_TIMER, timerBounds } from "../src/timerAuthoring.js";
import type { SceneDocument, ServiceHello } from "../src/types.js";

const scene = { scene_id: "main", display_name: "Main", scene_type: "STATE_SCENE", schema_version: 2,
  input_actions: [{action_id:"scene_timer",logical_source:"BUTTON_A"}],
  reactive_wait_default: {policy_id:"wait",hold_fallback_allowed:true,event_interests:["a"]},
} as SceneDocument;
const created = createTimerCommands(scene,SCENE_TIMER,1000);
assert.deepEqual(created.map(c => c.kind),["event_binding.add","event_handler.add","scene.set_reactive_wait_default"]);
assert.equal((created[0].event_binding as {binding_id:string}).binding_id,"scene_timer_2");
assert(!("target_state" in (created[1].event_handler as object)));
assert.deepEqual(scene.reactive_wait_default?.event_interests,["a"]);
const state = createTimerCommands(scene,STATE_TIMER,500,"one","two");
assert.equal(state[1].kind,"route.add");
assert.deepEqual((state[1].route as {from_states:string[]}).from_states,["one"]);
const deleted = deleteTimerCommands({...scene,event_handlers:[{handler_id:"expiry",event_ref:"tick",guards:[],actions:[]}],
  reactive_wait_default:{...scene.reactive_wait_default!,event_interests:["a","tick"]}},"tick");
assert.deepEqual(deleted.map(c => c.kind),["scene.set_reactive_wait_default","event_handler.delete","event_binding.delete"]);
const policyDelete = deleteTimerCommands({...scene,interaction_policy:{policy_id:"interaction",mode:"continuous",meaningful_activity_actions:["a","tick"]}},"tick");
assert.deepEqual(policyDelete.find(c => c.kind === "scene.set_interaction_policy")?.interaction_policy,
  {policy_id:"interaction",mode:"continuous",meaningful_activity_actions:["a"]});
const host = { target_profiles:{available:[{profile_id:"hw6",state_scene_events:{sources:[{
  event_type:SCENE_TIMER,status:"available_pending_validation",configuration_schema:{delay_ms:{minimum:10,maximum:5000}},
}]}}]} } as ServiceHello;
assert.deepEqual(timerBounds(host,"hw6",SCENE_TIMER),{minimum:10,maximum:5000});
assert.equal(timerBounds(host,"unknown",SCENE_TIMER),undefined);
assert.equal(timerBounds(host,"hw6",STATE_TIMER),undefined);
host.target_profiles!.available[0].state_scene_events!.sources[0].status="contracted_not_exposed";
assert.equal(timerBounds(host,"hw6",SCENE_TIMER),undefined);
console.log("Timer command batches and capability bounds passed");
