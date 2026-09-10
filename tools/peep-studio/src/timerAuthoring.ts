import type { SceneDocument, ServiceHello } from "./types";

export const SCENE_TIMER = "time.scene_elapsed";
export const STATE_TIMER = "time.state_entry_elapsed";
export type TimerCommand = Record<string, unknown>;

export function timerBounds(service: ServiceHello | null, profileId: string, eventType: string) {
  const source = service?.target_profiles?.available.find(profile => profile.profile_id === profileId)
    ?.state_scene_events?.sources.find(item => item.event_type === eventType);
  return source && ["available", "available_pending_validation"].includes(source.status)
    ? source.configuration_schema.delay_ms : undefined;
}

export function nextTimerId(scene: SceneDocument, base: string): string {
  const ids = new Set([...(scene.input_actions ?? []).map(item => item.action_id),
    ...(scene.event_bindings ?? []).map(item => item.binding_id), ...(scene.routes ?? []).map(item => item.route_id),
    ...(scene.event_handlers ?? []).map(item => item.handler_id)]);
  let id = base, suffix = 2;
  while (ids.has(id) || ids.has(`${id}_expired`)) id = `${base}_${suffix++}`;
  return id;
}

export function createTimerCommands(scene: SceneDocument, eventType: string, delay: number, stateId?: string,
  targetState?: string, startPolicy = "scene_entry"): TimerCommand[] {
  const id = nextTimerId(scene, eventType === SCENE_TIMER ? "scene_timer" : "state_timer");
  const common = { scene_id: scene.scene_id };
  const commands: TimerCommand[] = [{ ...common, kind: "event_binding.add", event_binding: {
    binding_id: id, event_type: eventType, configuration: { delay_ms: delay,
      ...(eventType === SCENE_TIMER ? { start_policy: startPolicy } : {}) },
  } }];
  if (eventType === SCENE_TIMER) commands.push({ ...common, kind: "event_handler.add", event_handler: {
    handler_id: `${id}_expired`, event_ref: id, guards: [], actions: [],
    ...(targetState ? { target_state: targetState } : {}),
  } });
  else {
    if (!stateId || !targetState) throw new Error("State timers require source and destination states");
    commands.push({ ...common, kind: "route.add", route: { route_id: `${id}_expired`, event_ref: id,
      from_states: [stateId], target_state: targetState, guards: [], actions: [] } });
  }
  if (scene.reactive_wait_default) commands.push({ ...common, kind: "scene.set_reactive_wait_default",
    reactive_wait_default: { ...scene.reactive_wait_default,
      event_interests: [...new Set([...(scene.reactive_wait_default.event_interests ?? []), id])] } });
  return commands;
}

export function deleteTimerCommands(scene: SceneDocument, bindingId: string): TimerCommand[] {
  const common = { scene_id: scene.scene_id };
  const commands: TimerCommand[] = [];
  // Reference-protected actions must be removed explicitly by the author, never silently stripped.
  if (scene.reactive_wait_default) commands.push({ ...common, kind: "scene.set_reactive_wait_default",
    reactive_wait_default: { ...scene.reactive_wait_default,
      event_interests: (scene.reactive_wait_default.event_interests ?? []).filter(id => id !== bindingId) } });
  if (scene.interaction_policy?.meaningful_activity_actions.includes(bindingId)) commands.push({ ...common,
    kind: "scene.set_interaction_policy", interaction_policy: { ...scene.interaction_policy,
      meaningful_activity_actions: scene.interaction_policy.meaningful_activity_actions.filter(id => id !== bindingId) } });
  for (const handler of scene.event_handlers ?? []) if (handler.event_ref === bindingId)
    commands.push({ ...common, kind: "event_handler.delete", handler_id: handler.handler_id });
  for (const route of scene.routes ?? []) if (route.event_ref === bindingId)
    commands.push({ ...common, kind: "route.delete", route_id: route.route_id });
  commands.push({ ...common, kind: "event_binding.delete", binding_id: bindingId });
  return commands;
}
