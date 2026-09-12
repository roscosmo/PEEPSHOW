import { useEffect, useRef, useState } from "react";
import { EyeOff, RotateCcw, Trash2 } from "lucide-react";
import { FramePreviewCanvas } from "./FramebufferCanvas";
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

export function SceneObjectInspector({ scene, object, label, stateIds, ownership, frames, clips, busy, supports, onApply, assets = [] }: {
  scene: SceneDocument; object: SceneObject | undefined; label: string; stateIds: string[];
  ownership: PlacementOwnership["scenes"][string] | null;
  frames: CompiledAssetFrame[]; clips: AuthoredClip[]; busy: boolean;
  supports: (kind: string) => boolean;
  assets?: AssetRecord[];
  onApply: (commands: Command[]) => Promise<boolean>;
}) {
  if (!object) return <section className="inspector-section placement-inspector"><p className="muted">No object selected.</p></section>;
  const selectionKey = JSON.stringify([scene.scene_id, object.object_id, stateIds]);
  const animated = !!object.animation_ref;
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
  const frameLabel = (frame: CompiledAssetFrame) => {
    const asset = assets.find(item => item.asset_id === frame.asset_id);
    const index = asset?.frames.findIndex(item => item.frame_id === frame.frame_id) ?? -1;
    return `${asset?.display_name ?? asset?.text ?? "Sprite"} - Frame ${index >= 0 ? index + 1 : matchingFrames.indexOf(frame) + 1}`;
  };
  const clipLabel = (clip: AuthoredClip) => {
    const asset = assets.find(item => item.frames.some(frame => frame.frame_id === clip.frame_refs[0]));
    return `${asset?.display_name ?? asset?.text ?? "Animation"} - ${clip.frame_refs.length} frames (${matchingClips.indexOf(clip) + 1})`;
  };
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
      {(stateScope || animated) && <div className="scene-object-property">
        <details key={selectionKey} className="object-frame-picker">
          <summary>{stateScope ? "Change frame in this state" : "Change default frame"}</summary>
          <div className="object-frame-options" aria-label="Object frame">
            {matchingFrames.map(frame => <button key={frame.frame_id} type="button" data-frame-id={frame.frame_id}
              aria-label={frameLabel(frame)} title={frameLabel(frame)} aria-pressed={shared("visual_ref") === frame.frame_id}
              disabled={!editable} onClick={() => void set("visual_ref", frame.frame_id)}>
              <FramePreviewCanvas frame={frame} /><span>{frameLabel(frame)}</span>
            </button>)}
          </div>
        </details>{reset("visual_ref")}
        {stateScope && overrideStates("visual_ref").length > 0 && <small>{status("visual_ref")}</small>}
      </div>}
      {(animated || (!stateScope && matchingClips.length > 0)) && <>
      {(animated || matchingClips.length > 0) && <div className="scene-object-property"><label><span>Animation (all states)</span>
        <select aria-label="Object animation" value={object.animation_ref ?? ""}
          disabled={stateScope || busy || (!supports("object.bind_animation") && !supports("object.clear_animation"))}
          onChange={event => { void onApply([{ kind: event.target.value ? "object.bind_animation" : "object.clear_animation",
            scene_id: scene.scene_id, object_id: object.object_id, ...(event.target.value ? { animation_ref: event.target.value } : {}) }]); }}>
          <option value="" disabled={!supports("object.clear_animation")}>Static</option>
          {matchingClips.map(clip => <option key={clip.animation_id} value={clip.animation_id} disabled={!supports("object.bind_animation")}>{clipLabel(clip)}</option>)}
        </select></label></div>}
      </>}
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
