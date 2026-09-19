"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.CALENDAR_SCHEDULE = exports.STATE_TIMER = exports.SCENE_TIMER = void 0;
exports.calendarScheduleModeLabel = calendarScheduleModeLabel;
exports.calendarScheduleSummary = calendarScheduleSummary;
exports.timerBounds = timerBounds;
exports.calendarScheduleCapability = calendarScheduleCapability;
exports.nextTimerId = nextTimerId;
exports.createTimerCommands = createTimerCommands;
exports.createCalendarScheduleCommands = createCalendarScheduleCommands;
exports.deleteTimerCommands = deleteTimerCommands;
exports.SCENE_TIMER = "time.scene_elapsed";
exports.STATE_TIMER = "time.state_entry_elapsed";
exports.CALENDAR_SCHEDULE = "time.local_schedule";
function calendarScheduleModeLabel(mode, dayOffset = 0) {
    if (mode === "daily")
        return "Daily";
    if (mode === "next_occurrence")
        return "Next occurrence";
    if (mode === "today_offset") {
        const days = Number(dayOffset);
        if (days === 0)
            return "Today";
        if (days === 1)
            return "Tomorrow";
        return `Today + ${days} days`;
    }
    return "Calendar schedule";
}
function calendarScheduleSummary(configuration, includeSeconds = false) {
    const seconds = Math.max(0, Math.min(86399, Number(configuration.time_of_day_seconds ?? 0)));
    const time = [Math.floor(seconds / 3600), Math.floor((seconds % 3600) / 60), seconds % 60]
        .map(value => String(value).padStart(2, "0"));
    if (!includeSeconds)
        time.pop();
    return `${calendarScheduleModeLabel(configuration.mode, configuration.day_offset)} at ${time.join(":")}`;
}
function timerBounds(service, profileId, eventType) {
    const source = service?.target_profiles?.available.find(profile => profile.profile_id === profileId)
        ?.state_scene_events?.sources.find(item => item.event_type === eventType);
    return source && ["available", "available_pending_validation"].includes(source.status)
        ? source.configuration_schema.delay_ms : undefined;
}
function calendarScheduleCapability(service) {
    const capability = service?.state_scene_graph.calendar_schedules;
    return capability && ["available", "available_pending_validation"].includes(capability.status)
        ? capability : undefined;
}
function nextTimerId(scene, base) {
    const ids = new Set([...(scene.input_actions ?? []).map(item => item.action_id),
        ...(scene.event_bindings ?? []).map(item => item.binding_id), ...(scene.routes ?? []).map(item => item.route_id),
        ...(scene.event_handlers ?? []).map(item => item.handler_id)]);
    let id = base, suffix = 2;
    while (ids.has(id) || ids.has(`${id}_expired`))
        id = `${base}_${suffix++}`;
    return id;
}
function createTimerCommands(scene, eventType, delay, stateId, targetState, startPolicy = "scene_entry") {
    const id = nextTimerId(scene, eventType === exports.SCENE_TIMER ? "scene_timer" : "state_timer");
    const common = { scene_id: scene.scene_id };
    const commands = [{ ...common, kind: "event_binding.add", event_binding: {
                binding_id: id, event_type: eventType, configuration: { delay_ms: delay,
                    ...(eventType === exports.SCENE_TIMER ? { start_policy: startPolicy } : {}) },
            } }];
    if (eventType === exports.SCENE_TIMER)
        commands.push({ ...common, kind: "event_handler.add", event_handler: {
                handler_id: `${id}_expired`, event_ref: id, guards: [], actions: [],
                ...(targetState ? { target_state: targetState } : {}),
            } });
    else {
        if (!stateId || !targetState)
            throw new Error("State timers require source and destination states");
        commands.push({ ...common, kind: "route.add", route: { route_id: `${id}_expired`, event_ref: id,
                from_states: [stateId], target_state: targetState, guards: [], actions: [] } });
    }
    if (scene.reactive_wait_default)
        commands.push({ ...common, kind: "scene.set_reactive_wait_default",
            reactive_wait_default: { ...scene.reactive_wait_default,
                event_interests: [...new Set([...(scene.reactive_wait_default.event_interests ?? []), id])] } });
    return commands;
}
function createCalendarScheduleCommands(scene, mode, timeOfDaySeconds, dayOffset, targetState) {
    const id = nextTimerId(scene, "calendar_schedule");
    const common = { scene_id: scene.scene_id };
    const commands = [{ ...common, kind: "event_binding.add", event_binding: {
                binding_id: id,
                event_type: exports.CALENDAR_SCHEDULE,
                configuration: { mode, time_of_day_seconds: timeOfDaySeconds, ...(mode === "today_offset" ? { day_offset: dayOffset } : {}) },
            } }, { ...common, kind: "event_handler.add", event_handler: {
                handler_id: `${id}_matched`, event_ref: id, guards: [], actions: [],
                ...(targetState ? { target_state: targetState } : {}),
            } }];
    if (scene.reactive_wait_default)
        commands.push({ ...common, kind: "scene.set_reactive_wait_default",
            reactive_wait_default: { ...scene.reactive_wait_default,
                event_interests: [...new Set([...(scene.reactive_wait_default.event_interests ?? []), id])] } });
    return commands;
}
function deleteTimerCommands(scene, bindingId) {
    const common = { scene_id: scene.scene_id };
    const commands = [];
    // Reference-protected actions must be removed explicitly by the author, never silently stripped.
    if (scene.reactive_wait_default)
        commands.push({ ...common, kind: "scene.set_reactive_wait_default",
            reactive_wait_default: { ...scene.reactive_wait_default,
                event_interests: (scene.reactive_wait_default.event_interests ?? []).filter(id => id !== bindingId) } });
    if (scene.interaction_policy?.meaningful_activity_actions.includes(bindingId))
        commands.push({ ...common,
            kind: "scene.set_interaction_policy", interaction_policy: { ...scene.interaction_policy,
                meaningful_activity_actions: scene.interaction_policy.meaningful_activity_actions.filter(id => id !== bindingId) } });
    for (const handler of scene.event_handlers ?? [])
        if (handler.event_ref === bindingId)
            commands.push({ ...common, kind: "event_handler.delete", handler_id: handler.handler_id });
    for (const route of scene.routes ?? [])
        if (route.event_ref === bindingId)
            commands.push({ ...common, kind: "route.delete", route_id: route.route_id });
    commands.push({ ...common, kind: "event_binding.delete", binding_id: bindingId });
    return commands;
}
