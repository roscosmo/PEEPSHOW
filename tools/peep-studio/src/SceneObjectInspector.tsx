import { useEffect, useRef, useState } from "react";
import { EyeOff, Plus, RotateCcw, Trash2 } from "lucide-react";
import { AnimationClipEditor } from "./AnimationClipEditor";
import type { AssetRecord, AuthoredClip, CompiledAssetFrame, PlacementOwnership, RenderElement, SceneDocument, SceneObject } from "./types";

type Property = "x" | "y" | "visible" | "visual_ref";
type Command = Record<string, unknown>;

function Coordinate({ axis, value, maximum, disabled, onCommit }: {
  axis: "x" | "y"; value: number | undefined; maximum: number; disabled: boolean;
  onCommit: (value: number) => Promise<boolean>;
}) {
  const [draft, setDraft] = useState(value === undefined ? "" : String(value));
  useEffect(() => setDraft(value === undefined ? "" : String(value)), [value]);
  const restore = () => setDraft(value === undefined ? "" : String(value));
  return <input aria-label={`Object ${axis.toUpperCase()}`} type="number" min={0} max={maximum} step={1}
    disabled={disabled} value={draft} placeholder="Mixed" onChange={event => setDraft(event.target.value)}
    onBlur={async event => {
      const raw = event.currentTarget.value;
      const next = Number(raw);
      if (!raw.trim() || !Number.isInteger(next) || next < 0 || next > maximum || next === value) { restore(); return; }
      if (!await onCommit(next)) restore();
    }} onKeyDown={event => {
      if (event.key === "Escape") {
        event.stopPropagation();
        event.currentTarget.value = value === undefined ? "" : String(value);
        restore(); event.currentTarget.blur();
      } else if (event.key === "Enter") event.currentTarget.blur();
    }} />;
}

function Visibility({ value, disabled, onChange }: { value: boolean | undefined; disabled: boolean; onChange: (value: boolean) => void }) {
  const ref = useRef<HTMLInputElement>(null);
  useEffect(() => { if (ref.current) ref.current.indeterminate = value === undefined; }, [value]);
  return <input ref={ref} aria-label="Object visible" type="checkbox" disabled={disabled}
    checked={value === true} onChange={event => onChange(event.target.checked)} />;
}

function SpriteLoopCreator({ sceneId, object, frames, assets, clips, disabled, onApply }: {
  sceneId: string; object: SceneObject; frames: CompiledAssetFrame[]; clips: AuthoredClip[];
  assets: AssetRecord[];
  disabled: boolean; onApply: (commands: Command[]) => Promise<boolean>;
}) {
  const [open, setOpen] = useState(false);
  const [duration, setDuration] = useState("400");
  const assetId = frames.find(frame => frame.frame_id === object.defaults.visual_ref)?.asset_id;
  const sourceFrames = assets.find(asset => asset.asset_id === assetId)?.frames ?? [];
  const byId = new Map(frames.map(frame => [frame.frame_id, frame]));
  const sequence = sourceFrames.flatMap(frame => {
    const compiled = byId.get(frame.frame_id);
    return compiled ? [compiled] : [];
  });
  const compatible = sequence.length === sourceFrames.length && sequence.length >= 2 && sequence.length <= 256 &&
    sequence.every(frame => frame.width === object.width && frame.height === object.height);
  const ms = Number(duration);
  const valid = duration.trim() !== "" && Number.isInteger(ms) && ms >= 1 && ms <= 60000;
  if (!compatible) return null;
  return <div className="scene-object-property">
    {!open ? <button className="button secondary" type="button" disabled={disabled} onClick={() => setOpen(true)}>
      <Plus size={14} />New loop
    </button> : <>
      <label><span>Frame duration (ms)</span><input aria-label="New loop frame duration" type="number"
        min={1} max={60000} step={1} value={duration} disabled={disabled}
        onChange={event => setDuration(event.target.value)} /></label>
      <small>{sequence.length} frames, imported order</small>
      <button className="button primary" type="button" disabled={disabled || !valid} onClick={async () => {
        let index = 1;
        while (clips.some(clip => clip.animation_id === `${assetId}_loop_${index}`)) index++;
        const animation_id = `${assetId}_loop_${index}`;
        if (await onApply([
          { kind: "animation.upsert", animation: { animation_id, frame_refs: sequence.map(frame => frame.frame_id),
            frame_duration_ms: sequence.map(() => ms), loop_policy: "loop" } },
          { kind: "object.bind_animation", scene_id: sceneId, object_id: object.object_id, animation_ref: animation_id },
        ])) setOpen(false);
      }}>Create loop</button>
      <button className="button secondary" type="button" disabled={disabled} onClick={() => setOpen(false)}>Cancel</button>
    </>}
  </div>;
}

export function SceneObjectInspector({ scene, object, label, stateIds, ownership, frames, clips, busy, supports, onApply, canCreateAnimation = false, assets = [], scenes = [scene] }: {
  scene: SceneDocument; object: SceneObject | undefined; label: string; stateIds: string[];
  ownership: PlacementOwnership["scenes"][string] | null;
  frames: CompiledAssetFrame[]; clips: AuthoredClip[]; busy: boolean;
  supports: (kind: string) => boolean;
  canCreateAnimation?: boolean;
  assets?: AssetRecord[];
  scenes?: SceneDocument[];
  onApply: (commands: Command[]) => Promise<boolean>;
}) {
  if (!object) return <section className="inspector-section placement-inspector"><p className="muted">No object selected.</p></section>;
  const stateScope = stateIds.length > 0;
  const targets = stateIds.map(id => ownership?.states[id]?.resolved_elements.find(item => item.element_id === object.object_id));
  const shared = <K extends Property,>(property: K): RenderElement[K] | undefined => {
    const defaults: Pick<RenderElement, Property> = object.defaults;
    if (!stateScope) return defaults[property];
    const values = targets.map(item => item?.[property]);
    return values.every(value => value === values[0]) ? values[0] : undefined;
  };
  const setKind = stateScope ? "object_override.set" : "object.set_defaults";
  const editable = supports(setKind) && !busy;
  const set = (property: Property, value: number | boolean | string) => onApply(stateScope
    ? stateIds.map(state_id => ({ kind: setKind, scene_id: scene.scene_id, object_id: object.object_id, state_id, properties: { [property]: value } }))
    : [{ kind: setKind, scene_id: scene.scene_id, object_id: object.object_id, properties: { [property]: value } }]);
  const overrideStates = (property: Property) => stateIds.filter(id =>
    ownership?.states[id]?.changes[object.object_id]?.local_properties.includes(property));
  const reset = (property: Property) => stateScope && <button className="icon-button" type="button"
    title="Use scene default"
    aria-label={`Use scene default for ${property === "visual_ref" ? "frame" : property === "visible" ? "visibility" : property.toUpperCase()}`} disabled={busy || !supports("object_override.clear") || !overrideStates(property).length}
    onClick={() => void onApply(overrideStates(property).map(state_id => ({ kind: "object_override.clear",
      scene_id: scene.scene_id, object_id: object.object_id, state_id, properties: [property] })))}><RotateCcw size={14} /></button>;
  const status = (property: Property) => !stateScope ? "Scene default"
    : overrideStates(property).length === 0 ? "Using scene default"
      : overrideStates(property).length === stateIds.length ? "Changed in this state" : "Changed in some states";
  const matchingFrames = frames.filter(frame => frame.width === object.width && frame.height === object.height);
  const matchingIds = new Set(matchingFrames.map(frame => frame.frame_id));
  const matchingClips = clips.filter(clip => clip.frame_refs.length > 0 && clip.frame_refs.every(id => matchingIds.has(id)));
  return <section className="inspector-section placement-inspector scene-object-inspector">
    <h3>Object</h3>
    <dl className="inspector-list">
      <div><dt>Name</dt><dd>{label}</dd></div>
      <div><dt>Scene</dt><dd>{scene.display_name}</dd></div>
      <div><dt>Size</dt><dd>{object.width} x {object.height}</dd></div>
      <div><dt>Internal ID</dt><dd>{object.object_id}</dd></div>
    </dl>
    {(["x", "y"] as const).map(axis => <div className="scene-object-property" key={axis}>
      <label><span>{axis === "x" ? "X (from left)" : "Y (from top)"}</span>
        <Coordinate axis={axis} value={shared(axis)} maximum={(axis === "x" ? 168 : 144) - (axis === "x" ? object.width : object.height)}
          disabled={!editable} onCommit={value => set(axis, value)} /></label>
      {reset(axis)}<small>{status(axis)}</small>
    </div>)}
    <div className="scene-object-property"><label className="scene-object-visibility"><span>Visible</span>
      <Visibility value={shared("visible")} disabled={!editable} onChange={value => void set("visible", value)} /></label>
      {reset("visible")}<small>{status("visible")}</small></div>
    {object.kind === "sprite" && <>
      <div className="scene-object-property"><label><span>{stateScope ? "Frame in this state" : "Default frame"}</span>
        <select aria-label="Object frame" value={shared("visual_ref") ?? ""} disabled={!editable}
          onChange={event => void set("visual_ref", event.target.value)}>
          <option value="" disabled>Mixed</option>
          {matchingFrames.map(frame => <option key={frame.frame_id} value={frame.frame_id}>{frame.frame_id}</option>)}
        </select></label>{reset("visual_ref")}<small>{status("visual_ref")}</small></div>
      <div className="scene-object-property"><label><span>Animation (all states)</span>
        <select aria-label="Object animation" value={object.animation_ref ?? ""}
          disabled={stateScope || busy || (!supports("object.bind_animation") && !supports("object.clear_animation"))}
          onChange={event => void onApply([{ kind: event.target.value ? "object.bind_animation" : "object.clear_animation",
            scene_id: scene.scene_id, object_id: object.object_id, ...(event.target.value ? { animation_ref: event.target.value } : {}) }])}>
          <option value="" disabled={!supports("object.clear_animation")}>Static</option>
          {matchingClips.map(clip => <option key={clip.animation_id} value={clip.animation_id} disabled={!supports("object.bind_animation")}>{clip.animation_id}</option>)}
        </select></label></div>
      {canCreateAnimation && <SpriteLoopCreator key={object.object_id} sceneId={scene.scene_id} object={object}
        frames={frames} assets={assets} clips={clips} disabled={stateScope || busy || !supports("object.bind_animation")}
        onApply={onApply} />}
      {canCreateAnimation && clips.filter(clip => clip.animation_id === object.animation_ref).map(clip =>
        <AnimationClipEditor key={JSON.stringify(clip)} clip={clip} frames={matchingFrames} assets={assets} scenes={scenes}
          disabled={stateScope || busy} onApply={onApply} />)}
    </>}
    <button className="button secondary" type="button"
      disabled={busy || !supports(stateScope ? "object_override.set" : "object.delete")}
      onClick={() => void (stateScope ? set("visible", false) : onApply([
        { kind: "object.delete", scene_id: scene.scene_id, object_id: object.object_id },
      ]))}>
      {stateScope ? <EyeOff size={14} /> : <Trash2 size={14} />}
      {stateScope ? stateIds.length === 1 ? "Hide in this state" : "Hide in selected states" : "Delete object"}
    </button>
  </section>;
}
