import { useEffect, useRef, useState } from "react";
import { Clock, Plus, Trash2 } from "lucide-react";
import { EditableActionList, EditableGuardList } from "./SceneInspection";
import { baseObjectRows } from "./sceneCapabilities";
import { createTimerCommands, deleteTimerCommands, SCENE_TIMER, STATE_TIMER, timerBounds } from "./timerAuthoring";
import type { TimerCommand } from "./timerAuthoring";
import type { AssetRecord, AudioCueRecord, PlacementOwnership, SceneDocument, ServiceHello, StateAction, StateGuard, StateRoute } from "./types";

export type TimerRequest = { serial: number; sceneId: string; eventType: string; stateId?: string };

export function TimerInspector({ scene, service, profileId, stateId, routeId, request, supports, onApply, ownership, assets, audioCues }: {
  scene: SceneDocument; service: ServiceHello | null; profileId: string; stateId?: string; routeId?: string;
  request: TimerRequest | null; supports: (kind: string) => boolean;
  onApply: (commands: TimerCommand[]) => Promise<boolean>;
  ownership: PlacementOwnership["scenes"][string] | null; assets: AssetRecord[]; audioCues: AudioCueRecord[];
}) {
  const section = useRef<HTMLElement>(null);
  const [selected, setSelected] = useState("");
  const [adding, setAdding] = useState<string | null>(null);
  const [delay, setDelay] = useState("5000");
  const [start, setStart] = useState("scene_entry");
  const [destination, setDestination] = useState("");
  const [source, setSource] = useState(stateId ?? scene.entry_state ?? "");
  const bindings = (scene.event_bindings ?? []).filter(binding => [SCENE_TIMER, STATE_TIMER].includes(binding.event_type));
  const binding = bindings.find(item => item.binding_id === selected);
  const handler = scene.event_handlers?.find(item => item.event_ref === selected);
  const route = scene.routes?.find(item => item.event_ref === selected);
  const record = handler ?? route;
  const ownerId = handler?.handler_id ?? route?.route_id ?? "";
  const ownerKind = handler ? "handler" : "route";
  const isScene = binding?.event_type === SCENE_TIMER;
  const bounds = timerBounds(service, profileId, adding ?? binding?.event_type ?? SCENE_TIMER);
  const timerActions = service?.state_scene_graph.scene_timers?.actions ?? [];
  const sceneTimers = bindings.filter(item => item.event_type === SCENE_TIMER).map(item => item.binding_id);
  const viewRecord: StateRoute = { route_id: ownerId, from_states: route?.from_states ?? [],
    guards: record?.guards ?? [], actions: record?.actions ?? [], target_state: record?.target_state };
  const all = (...kinds: string[]) => kinds.every(supports);
  const canCreate = (type: string) => !!timerBounds(service, profileId, type)
    && all("event_binding.add", "scene.set_reactive_wait_default", type === SCENE_TIMER ? "event_handler.add" : "route.add");
  const begin = (type: string, from = stateId) => {
    setAdding(type); setSelected(""); setDelay("5000"); setStart("scene_entry");
    setSource(from ?? scene.entry_state ?? ""); setDestination(type === STATE_TIMER ? from ?? scene.entry_state ?? "" : "");
  };
  useEffect(() => { setSelected(""); setAdding(null); }, [scene.scene_id]);
  useEffect(() => { if (request?.sceneId === scene.scene_id) {
    begin(request.eventType, request.stateId);
    section.current?.scrollIntoView({ block: "nearest" });
  } }, [request]);
  useEffect(() => {
    const eventRef = scene.routes?.find(item => item.route_id === routeId)?.event_ref;
    if (eventRef) { setSelected(eventRef); setAdding(null); }
    else if (routeId) { setSelected(""); setAdding(null); }
  }, [routeId, scene.scene_id]);
  useEffect(() => { if (binding) { setDelay(String(binding.configuration.delay_ms)); setStart(binding.configuration.start_policy ?? "scene_entry"); } },
    [binding?.binding_id, binding?.configuration.delay_ms, binding?.configuration.start_policy]);

  const setActions = async (actions: StateAction[]) => {
    await onApply([{ kind: "object_actions.set", scene_id: scene.scene_id, owner_kind: ownerKind, owner_id: ownerId, actions }]);
  };
  const setGuards = async (guards: StateGuard[]) => {
    if (handler) await onApply([{ kind: "event_handler.update", scene_id: scene.scene_id, event_handler: { ...handler, guards } }]);
  };
  const externalReferences = [...(scene.routes ?? []), ...(scene.event_handlers ?? [])].filter(item => item.event_ref !== selected)
    .some(item => item.actions.some(action => action.timer_ref === selected));
  const deletion = binding ? deleteTimerCommands(scene, selected) : [];
  const validDelay = bounds && Number.isInteger(Number(delay)) && Number(delay) >= bounds.minimum && Number(delay) <= bounds.maximum;
  const updateBinding = async (nextDelay: number, startPolicy: string) => {
    if (binding) await onApply([{ kind: "event_binding.update", scene_id: scene.scene_id, event_binding: {
      ...binding, configuration: { ...binding.configuration, delay_ms: nextDelay,
        ...(isScene ? { start_policy: startPolicy } : {}) },
    } }]);
  };
  return <section ref={section} className="inspector-section timer-inspector">
    <h3><Clock size={14} /> Timers</h3>
    <div className="timer-toolbar">
      <button className="button secondary" type="button" disabled={!canCreate(SCENE_TIMER)} onClick={() => begin(SCENE_TIMER)}><Plus size={14} />Scene timer</button>
      <button className="button secondary" type="button" disabled={!canCreate(STATE_TIMER) || !stateId} onClick={() => begin(STATE_TIMER)}><Plus size={14} />State timer</button>
    </div>
    {bindings.length > 0 && <label className="select-field">Timer
      <select aria-label="Selected timer" value={selected} onChange={event => { setSelected(event.target.value); setAdding(null); }}>
        <option value="">Select timer</option>
        {bindings.map(item => <option key={item.binding_id} value={item.binding_id}>
          {item.binding_id} ({item.event_type === SCENE_TIMER ? "Scene" : "State entry"})
        </option>)}
      </select></label>}
    {(adding || binding) && <>
      <h4>{(adding ?? binding?.event_type) === SCENE_TIMER ? "Scene timer" : "State-entry timer"}</h4>
      <label className="select-field">Delay (ms)
        <input aria-label="Timer delay" type="number" step={1} min={bounds?.minimum} max={bounds?.maximum} value={delay}
          disabled={!adding && !supports("event_binding.update")} onChange={event => setDelay(event.target.value)}
          onBlur={() => { if (!adding && binding) {
            if (validDelay && Number(delay) !== binding.configuration.delay_ms) void updateBinding(Number(delay), start);
            else setDelay(String(binding.configuration.delay_ms));
          } }} /></label>
      {(adding ?? binding?.event_type) === SCENE_TIMER && <label className="select-field">Start
        <select aria-label="Timer start policy" value={start} disabled={!adding && !supports("event_binding.update")}
          onChange={event => { setStart(event.target.value); if (!adding) void updateBinding(Number(binding?.configuration.delay_ms), event.target.value); }}>
          {(service?.state_scene_graph.scene_timers?.start_policies ?? []).map(policy => <option key={policy} value={policy}>
            {policy === "scene_entry" ? "On scene entry" : "By action"}
          </option>)}
        </select></label>}
      {adding === STATE_TIMER && <label className="select-field">State
        <select aria-label="Timer source state" value={source} onChange={event => setSource(event.target.value)}>
          {(scene.states ?? []).map(state => <option key={state.state_id} value={state.state_id}>{state.display_name}</option>)}
        </select></label>}
      <label className="select-field">On expiry
        <select aria-label="Timer destination" value={adding ? destination : record?.target_state ?? ""}
          disabled={!adding && (!!record?.target_scene || !supports(isScene ? "event_handler.update" : "route.set_target"))}
          onChange={event => {
            if (adding) setDestination(event.target.value);
            else if (handler) { const { target_state: _target, ...rest } = handler;
              void onApply([{ kind: "event_handler.update", scene_id: scene.scene_id, event_handler: {
                ...rest, ...(event.target.value ? { target_state: event.target.value } : {}),
              } }]);
            } else if (route) void onApply([{ kind: "route.set_target", scene_id: scene.scene_id, route_id: route.route_id, target_state: event.target.value }]);
          }}>
          {(adding === SCENE_TIMER || isScene) && <option value="">Actions only</option>}
          {(scene.states ?? []).map(state => <option key={state.state_id} value={state.state_id}>{state.display_name}</option>)}
        </select></label>
      {adding && <div className="timer-toolbar">
        <button className="button secondary" type="button" onClick={() => setAdding(null)}>Cancel</button>
        <button className="button primary" type="button" disabled={!validDelay || !canCreate(adding) || (adding === STATE_TIMER && (!source || !destination))}
          onClick={async () => { const commands = createTimerCommands(scene, adding, Number(delay), source, destination, start);
            if (await onApply(commands)) { setSelected((commands[0].event_binding as { binding_id: string }).binding_id); setAdding(null); }
          }}>Create timer</button>
      </div>}
    </>}
    {!adding && binding && record && (handler || routeId !== route?.route_id) && <>
      {handler && <><h4>Only if</h4><EditableGuardList sceneId={scene.scene_id} route={viewRecord} variables={scene.variables ?? []}
        guardLimit={service?.state_scene_graph.limits.guards_per_route ?? 0} canEdit={supports("event_handler.update")}
        onSetRouteGuard={async (_s, _r, index, variable_ref, operator, value) => {
          const guards = [...record.guards]; guards[index] = { variable_ref, operator, value }; await setGuards(guards);
        }} onAddRouteGuard={async (_s, _r, index, guard) => { const guards = [...record.guards]; guards.splice(index, 0, guard as StateGuard); await setGuards(guards); }}
        onDeleteRouteGuard={async (_s, _r, index) => { await setGuards(record.guards.filter((_g, i) => i !== index)); }}
        onMoveRouteGuard={async (_s, _r, index, target) => { const guards = [...record.guards]; guards.splice(target, 0, ...guards.splice(index, 1)); await setGuards(guards); }} /></>}
      <h4>Then</h4>
      <EditableActionList sceneObjects sceneId={scene.scene_id} route={viewRecord} variables={scene.variables ?? []}
        targetElements={baseObjectRows(scene, ownership)} waitingVisuals={[]} assets={assets} audioCues={audioCues}
        localActionsAllowed canAddActions={record.actions.length < (service?.state_scene_graph.limits.actions_per_route ?? 0)}
        canEdit={supports("object_actions.set")} timers={sceneTimers} timerActionKinds={timerActions}
        onSetRouteAction={async (_s, _r, index, action) => { const actions = [...record.actions]; actions[index] = action as StateAction; await setActions(actions); }}
        onAddRouteAction={async (_s, _r, index, action) => { const actions = [...record.actions]; actions.splice(index, 0, action as StateAction); await setActions(actions); }}
        onDeleteRouteAction={async (_s, _r, index) => { await setActions(record.actions.filter((_a, i) => i !== index)); }}
        onMoveRouteAction={async (_s, _r, index, target) => { const actions = [...record.actions]; actions.splice(target, 0, ...actions.splice(index, 1)); await setActions(actions); }} />
      <button className="button secondary" type="button" disabled={externalReferences || !deletion.every(item => supports(String(item.kind)))}
        title={externalReferences ? "Remove actions referencing this timer before deleting it" : "Delete timer and expiry branch"}
        onClick={async () => { if (await onApply(deletion)) setSelected(""); }}><Trash2 size={14} />Delete timer</button>
    </>}
  </section>;
}
