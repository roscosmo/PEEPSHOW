import { useEffect, useRef, useState } from "react";
import { AlignCenter, AlignLeft, AlignRight, EyeOff, Maximize2, RotateCcw, Trash2 } from "lucide-react";
import { FramePreviewCanvas } from "./FramebufferCanvas";
import { isFillableShapeKind, shapeKindWithFill } from "./placementGeometry";
import type { AssetRecord, AuthoredClip, CompiledAssetFrame, PlacementOwnership, RenderElement, RuntimeTextProfile, SceneDocument, SceneObject } from "./types";

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

export function SceneObjectInspector({ scene, object, label, stateIds, ownership, frames, clips, busy, supports, onApply, assets = [], runtimeTextProfile }: {
  scene: SceneDocument; object: SceneObject | undefined; label: string; stateIds: string[];
  ownership: PlacementOwnership["scenes"][string] | null;
  frames: CompiledAssetFrame[]; clips: AuthoredClip[]; busy: boolean;
  supports: (kind: string) => boolean;
  assets?: AssetRecord[];
  runtimeTextProfile?: RuntimeTextProfile | null;
  onApply: (commands: Command[]) => Promise<boolean>;
}) {
  if (!object) return <section className="inspector-section placement-inspector"><p className="muted">No object selected.</p></section>;
  const selectionKey = JSON.stringify([scene.scene_id, object.object_id, stateIds]);
  const animated = !!object.animation_ref;
  const stateScope = stateIds.length > 0;
  const fillableShape = isFillableShapeKind(object.kind);
  const filledShape = object.kind === "filled_rect" || object.kind === "filled_circle" || object.kind === "filled_ellipse";
  const fillEditable = !busy && supports("object.set_kind");
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
    <div className="asset-name-editor">
      <label>
        Object name
        <input
          key={`${object.object_id}-${object.display_name ?? ""}`}
          type="text"
          maxLength={96}
          defaultValue={object.display_name ?? label}
          disabled={busy || !supports("object.rename")}
          onBlur={event => {
            const value = event.currentTarget.value.trim();
            const current = object.display_name ?? label;
            if (value.length === 0) {
              event.currentTarget.value = current;
            } else if (value !== current) {
              void onApply([{ kind: "object.rename", scene_id: scene.scene_id,
                object_id: object.object_id, display_name: value }]);
            }
          }}
          onKeyDown={event => {
            if (event.key === "Enter") event.currentTarget.blur();
            if (event.key === "Escape") {
              event.preventDefault();
              event.currentTarget.value = object.display_name ?? label;
              event.currentTarget.blur();
            }
          }}
        />
      </label>
    </div>
    <dl className="inspector-list">
      <div><dt>Scene</dt><dd>{scene.display_name}</dd></div>
      <div><dt>Size</dt><dd>{object.width} x {object.height}</dd></div>
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
    {fillableShape && <div className="scene-object-property"><label className="scene-object-visibility"><span>Filled</span>
      <input
        aria-label="Object filled"
        type="checkbox"
        checked={filledShape}
        disabled={!fillEditable}
        title={fillEditable ? "Fill this shape in every state" : "Fill editing is not available in this project version"}
        onChange={event => {
          const objectKind = shapeKindWithFill(object.kind, event.target.checked);
          if (objectKind !== null) {
            void onApply([{ kind: "object.set_kind", scene_id: scene.scene_id, object_id: object.object_id, object_kind: objectKind }]);
          }
        }}
      />
    </label><small>All states</small></div>}
    {object.kind === "text" && runtimeTextProfile && <RuntimeTextEditor scene={scene} object={object}
      profile={runtimeTextProfile} busy={busy || stateScope || !supports("object.set_text")} onApply={onApply} />}
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

function RuntimeTextEditor({ scene, object, profile, busy, onApply }: {
  scene: SceneDocument;
  object: SceneObject;
  profile: RuntimeTextProfile;
  busy: boolean;
  onApply: (commands: Command[]) => Promise<boolean>;
}) {
  const [text, setText] = useState(object.text ?? "");
  const [scale, setScale] = useState(object.scale ?? profile.scale.minimum);
  const [alignment, setAlignment] = useState<"left" | "center" | "right">(object.alignment ?? "left");
  const [width, setWidth] = useState(object.width);
  const [height, setHeight] = useState(object.height);
  useEffect(() => {
    setText(object.text ?? "");
    setScale(object.scale ?? profile.scale.minimum);
    setAlignment(object.alignment ?? "left");
    setWidth(object.width);
    setHeight(object.height);
  }, [object.object_id, object.text, object.scale, object.alignment, object.width, object.height, profile.scale.minimum]);
  const lines = text.split("\n");
  const requiredWidth = Math.max(1, ...lines.map(line => line.length)) * profile.glyph_cell.width * scale;
  const requiredHeight = lines.length * profile.glyph_cell.height * scale;
  const columns = Math.floor(width / (profile.glyph_cell.width * scale));
  const rows = Math.floor(height / (profile.glyph_cell.height * scale));
  const fittedWidth = Math.min(profile.bounds.width.maximum, Math.max(profile.bounds.width.minimum, requiredWidth));
  const fittedHeight = Math.min(profile.bounds.height.maximum, Math.max(profile.bounds.height.minimum, requiredHeight));
  const invalidCharacters = !/^[\x20-\x7e\n]+$/.test(text);
  const overflow = requiredWidth > width || requiredHeight > height;
  const scaleValid = Number.isInteger(scale) && scale >= profile.scale.minimum && scale <= profile.scale.maximum;
  const boundsValid = Number.isInteger(width) && width >= profile.bounds.width.minimum && width <= profile.bounds.width.maximum
    && Number.isInteger(height) && height >= profile.bounds.height.minimum && height <= profile.bounds.height.maximum;
  const changed = text !== object.text || scale !== object.scale || alignment !== object.alignment
    || width !== object.width || height !== object.height;
  const valid = text.length >= 1 && text.length <= profile.maximum_length && !invalidCharacters && !overflow
    && scaleValid && boundsValid;
  const fitBounds = () => {
    setWidth(fittedWidth);
    setHeight(fittedHeight);
  };
  return <div className="runtime-text-editor">
    <label>Text
      <textarea rows={5} value={text} maxLength={profile.maximum_length} disabled={busy}
        onChange={event => setText(event.target.value)} />
      <span className="runtime-text-count">{text.length} / {profile.maximum_length} characters; {lines.length} {lines.length === 1 ? "line" : "lines"}</span>
    </label>
    <div className="runtime-text-fields">
      <label>Scale
        <input type="number" min={profile.scale.minimum} max={profile.scale.maximum} step={1}
          value={scale} disabled={busy} onChange={event => setScale(Number(event.target.value))} />
      </label>
      <fieldset className="runtime-text-alignment">
        <legend>Alignment</legend>
        <div className="segmented-control">
          {profile.alignment.map(value => <button key={value} type="button" disabled={busy}
            className={alignment === value ? "active" : ""} aria-label={`${value} align`} title={`${value[0].toUpperCase() + value.slice(1)} align`}
            onClick={() => setAlignment(value)}>{value === "left" ? <AlignLeft size={15} /> : value === "center" ? <AlignCenter size={15} /> : <AlignRight size={15} />}</button>)}
        </div>
      </fieldset>
      <label>Width
        <input type="number" min={profile.bounds.width.minimum} max={profile.bounds.width.maximum} step={1}
          value={width} disabled={busy} onChange={event => setWidth(Number(event.target.value))} />
      </label>
      <label>Height
        <input type="number" min={profile.bounds.height.minimum} max={profile.bounds.height.maximum} step={1}
          value={height} disabled={busy} onChange={event => setHeight(Number(event.target.value))} />
      </label>
    </div>
    <div className={`runtime-text-fit ${overflow || !scaleValid || !boundsValid ? "invalid" : ""}`}>
      <span>{!scaleValid ? `Scale must be ${profile.scale.minimum}-${profile.scale.maximum}`
        : !boundsValid ? "Bounds must be whole pixels within the supported range"
        : overflow ? `Needs ${requiredWidth} x ${requiredHeight} px` : `Fits ${columns} columns x ${rows} rows`}</span>
      <button className="button secondary" type="button" disabled={busy || !scaleValid || (width === fittedWidth && height === fittedHeight)}
        onClick={fitBounds}><Maximize2 size={14} />Fit bounds</button>
    </div>
    {invalidCharacters && <small className="error-text">Use printable ASCII characters and explicit line breaks only.</small>}
    {overflow && <small className="error-text">Increase the bounds, reduce the scale, or add explicit line breaks.</small>}
    <button className="button primary" type="button" disabled={busy || !changed || !valid}
      onClick={() => void onApply([{ kind: "object.set_text", scene_id: scene.scene_id, object_id: object.object_id,
        text, font_id: object.font_id ?? profile.font_ids[0], scale, alignment, width, height }])}>Apply text</button>
  </div>;
}
