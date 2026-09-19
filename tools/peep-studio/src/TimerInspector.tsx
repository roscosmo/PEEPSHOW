import { useEffect, useRef, useState } from "react";
import { CalendarClock, Clock, Trash2 } from "lucide-react";
import { EditableActionList, EditableGuardList, type SceneSelection } from "./SceneInspection";
import { baseObjectRows } from "./sceneCapabilities";
import { CALENDAR_SCHEDULE, calendarScheduleCapability, createCalendarScheduleCommands, createTimerCommands, deleteTimerCommands, SCENE_TIMER, STATE_TIMER, timerBounds } from "./timerAuthoring";
import type { TimerCommand } from "./timerAuthoring";
import type { AssetRecord, AudioCueRecord, PlacementOwnership, SceneDocument, ServiceHello, StateAction, StateGuard, StateRoute } from "./types";

function timerKindLabel(eventType: string | null | undefined): string {
  return eventType === CALENDAR_SCHEDULE ? "Calendar schedule" : eventType === SCENE_TIMER ? "Scene timer" : "State-entry timer";
}

function TimerKindIcon({ eventType, className = "" }: { eventType: string | null | undefined; className?: string }) {
  if (eventType === CALENDAR_SCHEDULE) return <CalendarClock className={`timer-kind-icon ${className}`.trim()} aria-hidden="true" />;
  const src = eventType === SCENE_TIMER ? "/ui-icons/scene_timer.png" : "/ui-icons/state_timer.png";
  return <img className={`timer-kind-icon ${className}`.trim()} src={src} alt="" aria-hidden="true" />;
}

function timerShortDetail(eventType: string | null | undefined): string {
  return eventType === CALENDAR_SCHEDULE ? "Local calendar time" : eventType === SCENE_TIMER ? "Survives state changes" : "Restarts on state entry";
}

function timeInputValue(seconds: number): string {
  const bounded = Math.max(0, Math.min(86399, seconds));
  return `${String(Math.floor(bounded / 3600)).padStart(2, "0")}:${String(Math.floor((bounded % 3600) / 60)).padStart(2, "0")}:${String(bounded % 60).padStart(2, "0")}`;
}

function timeInputSeconds(value: string): number | null {
  const match = /^(\d{2}):(\d{2})(?::(\d{2}))?$/.exec(value);
  if (!match) return null;
  const hours = Number(match[1]), minutes = Number(match[2]), seconds = Number(match[3] ?? 0);
  return hours <= 23 && minutes <= 59 && seconds <= 59 ? hours * 3600 + minutes * 60 + seconds : null;
}

function timerStartLabel(policy: string | null | undefined): string {
  return policy === "action" ? "Started by action" : "When scene opens";
}

function stateName(scene: SceneDocument, stateId: string | undefined): string {
  return scene.states?.find(state => state.state_id === stateId)?.display_name ?? stateId ?? "None";
}

export function TimerInspector({ scene, scenes = [], service, profileId, selection, onSelect, supports, onApply, ownership, assets, audioCues, canConnectScenes = false, sceneExitActionKinds = [], routeActionKinds = [], previewClockAvailable = false, onSetPreviewLocalTime }: {
  scene: SceneDocument; service: ServiceHello | null; profileId: string;
  selection: SceneSelection; onSelect: (selection: SceneSelection) => void; supports: (kind: string) => boolean;
  onApply: (commands: TimerCommand[]) => Promise<boolean>;
  ownership: PlacementOwnership["scenes"][string] | null; assets: AssetRecord[]; audioCues: AudioCueRecord[];
  canConnectScenes?: boolean; sceneExitActionKinds?: string[]; routeActionKinds?: string[];
  previewClockAvailable?: boolean; onSetPreviewLocalTime?: (localTime: string) => Promise<boolean>;
  scenes?: SceneDocument[];
}) {
  const section = useRef<HTMLElement>(null);
  const stateId = selection.kind === "state" ? selection.id : selection.kind === "timerDraft" ? selection.stateId : undefined;
  const route = selection.kind === "route" ? scene.routes?.find(item => item.route_id === selection.id) : undefined;
  const selected = selection.kind === "timer" ? selection.id : route?.event_ref ?? "";
  const adding = selection.kind === "timerDraft" ? selection.eventType : null;
  const [delay, setDelay] = useState("5000");
  const [start, setStart] = useState("scene_entry");
  const [destination, setDestination] = useState("");
  const [source, setSource] = useState(stateId ?? scene.entry_state ?? "");
  const [calendarMode, setCalendarMode] = useState<"daily" | "today_offset" | "next_occurrence">("next_occurrence");
  const [calendarTime, setCalendarTime] = useState("12:30:00");
  const [dayOffset, setDayOffset] = useState("0");
  const [previewLocalTime, setPreviewLocalTimeValue] = useState(() => {
    const now = new Date();
    const local = new Date(now.getTime() - now.getTimezoneOffset() * 60000);
    return local.toISOString().slice(0, 19);
  });
  const bindings = (scene.event_bindings ?? []).filter(binding => [SCENE_TIMER, STATE_TIMER, CALENDAR_SCHEDULE].includes(binding.event_type));
  const binding = bindings.find(item => item.binding_id === selected);
  const handler = scene.event_handlers?.find(item => item.event_ref === selected);
  const timerRoutes = (scene.routes ?? []).filter(item => item.event_ref === selected);
  const record = handler ?? route;
  const ownerId = handler?.handler_id ?? route?.route_id ?? "";
  const ownerKind = handler ? "handler" : "route";
  const isScene = binding?.event_type === SCENE_TIMER;
  const isCalendar = binding?.event_type === CALENDAR_SCHEDULE;
  const exits = scene.scene_exits ?? [];
  const exitFields = (value: string) => {
    const exit = value.startsWith('exit:') ? exits.find(item => item.scene_exit_id === value.slice(5)) : undefined;
    return exit ? { target_scene: exit.target_scene, scene_exit_ref: exit.scene_exit_id } : value ? { target_state: value } : {};
  };
  const bounds = timerBounds(service, profileId, adding ?? binding?.event_type ?? SCENE_TIMER);
  const calendarCapability = calendarScheduleCapability(service);
  const timerActions = service?.state_scene_graph.scene_timers?.actions ?? [];
  const sceneTimers = bindings.filter(item => item.event_type === SCENE_TIMER).map(item => item.binding_id);
  const viewRecord: StateRoute = { route_id: ownerId, from_states: route?.from_states ?? [],
    guards: record?.guards ?? [], actions: record?.actions ?? [], target_state: record?.target_state };
  const all = (...kinds: string[]) => kinds.every(supports);
  const canCreate = (type: string) => type === CALENDAR_SCHEDULE
    ? !!calendarCapability && !bindings.some(item => item.event_type === CALENDAR_SCHEDULE)
      && all("event_binding.add", "event_handler.add", "scene.set_reactive_wait_default")
    : !!timerBounds(service, profileId, type)
      && all("event_binding.add", "scene.set_reactive_wait_default", type === SCENE_TIMER ? "event_handler.add" : "route.add");
  useEffect(() => { if (adding) {
    setDelay("5000"); setStart("scene_entry"); setSource(stateId ?? scene.entry_state ?? "");
    setCalendarMode("next_occurrence"); setCalendarTime("12:30:00"); setDayOffset("0");
    setDestination(adding === STATE_TIMER ? stateId ?? scene.entry_state ?? "" : "");
    section.current?.scrollIntoView({ block: "nearest" });
  } }, [adding, stateId, scene.scene_id]);
  useEffect(() => { if (binding) { setDelay(String(binding.configuration.delay_ms)); setStart(binding.configuration.start_policy ?? "scene_entry"); } },
    [binding?.binding_id, binding?.configuration.delay_ms, binding?.configuration.start_policy]);
  useEffect(() => { if (binding?.event_type === CALENDAR_SCHEDULE) {
    setCalendarMode(binding.configuration.mode as typeof calendarMode);
    setCalendarTime(timeInputValue(Number(binding.configuration.time_of_day_seconds)));
    setDayOffset(String(binding.configuration.day_offset ?? 0));
  } }, [binding?.binding_id, binding?.configuration.mode, binding?.configuration.time_of_day_seconds, binding?.configuration.day_offset]);

  const setActions = async (actions: StateAction[]) => {
    await onApply([{ kind: "object_actions.set", scene_id: scene.scene_id, owner_kind: ownerKind, owner_id: ownerId, actions }]);
  };
  const setGuards = async (guards: StateGuard[]) => {
    if (handler) await onApply([{ kind: "event_handler.update", scene_id: scene.scene_id, event_handler: { ...handler, guards } }]);
  };
  const externalReferences = [...(scene.routes ?? []), ...(scene.event_handlers ?? [])].filter(item => item.event_ref !== selected)
    .some(item => item.actions.some(action => action.timer_ref === selected));
  const deletion = binding ? deleteTimerCommands(scene, selected) : [];
  const delayNumber = Number(delay);
  const validDelay = bounds !== undefined && Number.isInteger(delayNumber) && delayNumber >= bounds.minimum && delayNumber <= bounds.maximum;
  const timerType = adding ?? binding?.event_type ?? null;
  const timerIsScene = timerType === SCENE_TIMER;
  const timerIsCalendar = timerType === CALENDAR_SCHEDULE;
  const calendarSeconds = timeInputSeconds(calendarTime);
  const dayOffsetNumber = Number(dayOffset);
  const validCalendar = calendarCapability !== undefined && calendarSeconds !== null
    && calendarSeconds >= calendarCapability.time_of_day_seconds.minimum
    && calendarSeconds <= calendarCapability.time_of_day_seconds.maximum
    && Number.isInteger(dayOffsetNumber) && dayOffsetNumber >= calendarCapability.day_offset.minimum
    && dayOffsetNumber <= calendarCapability.day_offset.maximum;
  const startPolicyLabel = timerStartLabel(start);
  const expiryValue = adding ? destination : record?.target_scene
    ? handler?.scene_exit_ref ? `exit:${handler.scene_exit_ref}` : `scene:${record.target_scene}`
    : record?.target_state ?? "";
  const expiryActionsOnly = expiryValue === "";
  const selectedStateName = stateName(scene, source);
  const updateBinding = async (nextDelay: number, startPolicy: string) => {
    if (binding) await onApply([{ kind: "event_binding.update", scene_id: scene.scene_id, event_binding: {
      ...binding, configuration: { ...binding.configuration, delay_ms: nextDelay,
        ...(isScene ? { start_policy: startPolicy } : {}) },
    } }]);
  };
  const updateCalendarBinding = async () => {
    if (binding && calendarSeconds !== null && validCalendar) await onApply([{ kind: "event_binding.update", scene_id: scene.scene_id,
      event_binding: { ...binding, configuration: { mode: calendarMode, time_of_day_seconds: calendarSeconds,
        ...(calendarMode === "today_offset" ? { day_offset: dayOffsetNumber } : {}) } } }]);
  };
  if (!adding && !binding) return null;

  return <section ref={section} className="inspector-section timer-inspector">
    <h3><Clock size={14} /> Timers</h3>
    {(adding || binding) && <>
      <div className={`timer-mode-card ${timerIsScene ? "scene" : "state"}`}>
        <span><TimerKindIcon eventType={timerType} /></span>
        <div>
          <strong>{timerKindLabel(timerType)}</strong>
          <small>{timerShortDetail(timerType)}{timerIsScene ? ` / ${startPolicyLabel}` : adding === STATE_TIMER ? ` / from ${selectedStateName}` : ""}</small>
        </div>
      </div>
      {!timerIsCalendar && <label className="select-field">Delay
        <div className="timer-delay-field">
          <input aria-label="Timer delay" type="number" step={1} min={bounds?.minimum} max={bounds?.maximum} value={delay}
            disabled={!adding && !supports("event_binding.update")} onChange={event => setDelay(event.target.value)}
            onBlur={() => { if (!adding && binding) {
              if (validDelay && delayNumber !== binding.configuration.delay_ms) void updateBinding(delayNumber, start);
              else setDelay(String(binding.configuration.delay_ms));
            } }} />
          <span>ms</span>
        </div>
      </label>}
      {!timerIsCalendar && bounds !== undefined && !validDelay && <p className="muted timer-field-note">Allowed range: {bounds.minimum}-{bounds.maximum} ms.</p>}
      {timerIsCalendar && <>
        <label className="select-field">Schedule
          <select value={calendarMode} disabled={!adding && !supports("event_binding.update")}
            onChange={event => setCalendarMode(event.target.value as typeof calendarMode)}>
            {(calendarCapability?.modes ?? []).map(mode => <option key={mode} value={mode}>{mode === "daily" ? "Daily" : mode === "today_offset" ? "Today plus days" : "Next occurrence"}</option>)}
          </select>
        </label>
        <label className="select-field">Local time
          <input type="time" step={1} value={calendarTime} disabled={!adding && !supports("event_binding.update")}
            onChange={event => setCalendarTime(event.target.value)} />
        </label>
        {calendarMode === "today_offset" && <label className="select-field">Day offset
          <input type="number" step={1} min={calendarCapability?.day_offset.minimum} max={calendarCapability?.day_offset.maximum}
            value={dayOffset} disabled={!adding && !supports("event_binding.update")} onChange={event => setDayOffset(event.target.value)} />
        </label>}
        {!adding && <button className="button secondary" type="button" disabled={!validCalendar || !supports("event_binding.update")}
          onClick={() => void updateCalendarBinding()}>Apply schedule</button>}
        <div className="calendar-preview-clock">
          <label className="select-field">Preview local time
            <input type="datetime-local" step={1} value={previewLocalTime}
              disabled={!previewClockAvailable || onSetPreviewLocalTime === undefined}
              onChange={event => setPreviewLocalTimeValue(event.target.value)} />
          </label>
          <button className="button secondary" type="button"
            disabled={!previewClockAvailable || onSetPreviewLocalTime === undefined
              || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}(?::\d{2})?$/.test(previewLocalTime)}
            onClick={() => void onSetPreviewLocalTime?.(previewLocalTime)}>Set preview clock</button>
        </div>
      </>}
      {timerIsScene && <label className="select-field">Starts
        <select aria-label="Timer start policy" value={start} disabled={!adding && !supports("event_binding.update")}
          onChange={event => { setStart(event.target.value); if (!adding) void updateBinding(Number(binding?.configuration.delay_ms), event.target.value); }}>
          {(service?.state_scene_graph.scene_timers?.start_policies ?? []).map(policy => <option key={policy} value={policy}>
            {timerStartLabel(policy)}
          </option>)}
        </select></label>}
      {adding === STATE_TIMER && <label className="select-field">Source state
        <select aria-label="Timer source state" value={source} onChange={event => setSource(event.target.value)}>
          {(scene.states ?? []).map(state => <option key={state.state_id} value={state.state_id}>{state.display_name}</option>)}
        </select></label>}
      {(adding || handler) && <label className="select-field">When timer expires
        <select aria-label="Timer destination" value={expiryValue}
          disabled={!adding && (!supports(isScene ? "event_handler.update" : "route.set_target") || (!!record?.target_scene && !canConnectScenes))}
          onChange={event => {
            if (adding) setDestination(event.target.value);
            else if (handler) { const { target_state: _target, target_scene: _scene, scene_exit_ref: _exit, ...rest } = handler;
              void onApply([{ kind: "event_handler.update", scene_id: scene.scene_id, event_handler: {
                ...rest, ...exitFields(event.target.value),
              } }]);
            } else if (route) void onApply([{ kind: "route.set_target", scene_id: scene.scene_id, route_id: route.route_id, target_state: event.target.value }]);
          }}>
          {(timerIsScene || timerIsCalendar) && <option value="">Run effects only</option>}
          {!adding && record?.target_scene && !handler?.scene_exit_ref && <option value={`scene:${record.target_scene}`} disabled>
            Go to scene: {scenes.find(item => item.scene_id === record.target_scene)?.display_name ?? record.target_scene}
          </option>}
          {(scene.states ?? []).map(state => <option key={state.state_id} value={state.state_id}>Go to state: {state.display_name}</option>)}
          {(timerIsScene || timerIsCalendar) && canConnectScenes && exits.map(exit => <option key={exit.scene_exit_id}
            value={`exit:${exit.scene_exit_id}`} disabled={(record?.actions.length ?? 0) > 0}>Go to scene: {exit.display_name}</option>)}
        </select></label>}
      {(timerIsScene || timerIsCalendar) && canConnectScenes && (record?.actions.length ?? 0) > 0
        && <p className="muted timer-field-note">Remove effects before choosing a scene exit.</p>}
      {(timerIsScene || timerIsCalendar) && expiryActionsOnly && <p className="muted timer-field-note">This event will run its effects without re-entering a state.</p>}
      {adding && <div className="timer-toolbar">
        <button className="button secondary" type="button" onClick={() => onSelect(stateId ? { kind: "state", id: stateId } : { kind: "scene" })}>Cancel</button>
        <button className="button primary" type="button" disabled={!(timerIsCalendar ? validCalendar : validDelay) || !canCreate(adding) || (adding === STATE_TIMER && (!source || !destination))}
          onClick={async () => { const commands = timerIsCalendar
            ? createCalendarScheduleCommands(scene, calendarMode, calendarSeconds ?? 0, dayOffsetNumber, destination.startsWith('exit:') ? undefined : destination)
            : createTimerCommands(scene, adding, Number(delay), source, destination.startsWith('exit:') ? undefined : destination, start);
            if ((adding === SCENE_TIMER || adding === CALENDAR_SCHEDULE) && destination.startsWith('exit:')) {
              const command = commands.find(item => item.kind === 'event_handler.add');
              if (command) command.event_handler = { ...(command.event_handler as Record<string, unknown>), ...exitFields(destination) };
            }
            if (await onApply(commands)) {
              const routeCommand = commands.find(command => command.kind === "route.add");
              onSelect(routeCommand ? { kind: "route", id: (routeCommand.route as StateRoute).route_id, sourceState: source }
                : { kind: "timer", id: (commands[0].event_binding as { binding_id: string }).binding_id });
            }
          }}>Create {timerKindLabel(adding).toLowerCase()}</button>
      </div>}
    </>}
    {!adding && binding && handler && record && <>
      {handler && <><h4>Conditions</h4><EditableGuardList sceneId={scene.scene_id} route={viewRecord} variables={scene.variables ?? []}
        guardLimit={service?.state_scene_graph.limits.guards_per_route ?? 0} canEdit={supports("event_handler.update")}
        onSetRouteGuard={async (_s, _r, index, variable_ref, operator, value) => {
          const guards = [...record.guards]; guards[index] = { variable_ref, operator, value }; await setGuards(guards);
        }} onAddRouteGuard={async (_s, _r, index, guard) => { const guards = [...record.guards]; guards.splice(index, 0, guard as StateGuard); await setGuards(guards); }}
        onDeleteRouteGuard={async (_s, _r, index) => { await setGuards(record.guards.filter((_g, i) => i !== index)); }}
        onMoveRouteGuard={async (_s, _r, index, target) => { const guards = [...record.guards]; guards.splice(target, 0, ...guards.splice(index, 1)); await setGuards(guards); }} /></>}
      <h4>Effects</h4>
      <EditableActionList sceneObjects sceneId={scene.scene_id} route={viewRecord} variables={scene.variables ?? []}
        allowedActionKinds={record.target_scene ? sceneExitActionKinds : undefined}
        routeActionKinds={routeActionKinds}
        targetElements={baseObjectRows(scene, ownership)} waitingVisuals={[]} assets={assets} audioCues={audioCues}
        localActionsAllowed canAddActions={record.actions.length < (service?.state_scene_graph.limits.actions_per_route ?? 0)}
        canEdit={supports("object_actions.set")} timers={sceneTimers} timerActionKinds={timerActions}
        onSetRouteAction={async (_s, _r, index, action) => { const actions = [...record.actions]; actions[index] = action as StateAction; await setActions(actions); }}
        onAddRouteAction={async (_s, _r, index, action) => { const actions = [...record.actions]; actions.splice(index, 0, action as StateAction); await setActions(actions); }}
        onDeleteRouteAction={async (_s, _r, index) => { await setActions(record.actions.filter((_a, i) => i !== index)); }}
        onMoveRouteAction={async (_s, _r, index, target) => { const actions = [...record.actions]; actions.splice(target, 0, ...actions.splice(index, 1)); await setActions(actions); }} />
    </>}
    {!adding && binding && selection.kind === "timer" && <>
      {!handler && timerRoutes.length > 0 && <div className="record-list" aria-label="Timer transitions">
        {timerRoutes.map(item => <button className="record-row" key={item.route_id} type="button"
          onClick={() => onSelect({ kind: "route", id: item.route_id, sourceState: item.from_states[0] })}>
          <strong>{item.from_states.map(id => stateName(scene, id)).join(", ")}</strong>
          <small>expires to {stateName(scene, item.target_state) ?? "timer actions"}</small>
        </button>)}
      </div>}
      <button className="button secondary" type="button" disabled={externalReferences || !deletion.every(item => supports(String(item.kind)))}
        title={externalReferences ? "Remove actions referencing this timer before deleting it" : "Delete timer and expiry branch"}
        onClick={async () => { if (await onApply(deletion)) onSelect({ kind: "scene" }); }}><Trash2 size={14} />Delete timer</button>
    </>}
  </section>;
}
