import { useState } from "react";
import { ArrowDown, ArrowUp, Copy, Pencil, Trash2 } from "lucide-react";
import { FramePreviewCanvas } from "./FramebufferCanvas";
import type { AssetRecord, AuthoredClip, CompiledAssetFrame, SceneDocument } from "./types";

export function AnimationClipEditor({ clip, frames, assets, scenes, disabled, onApply, displayName, initiallyOpen = false,
  creating = false, loopPolicies, onCancel, onDuplicate, onDelete }: {
  clip: AuthoredClip; frames: CompiledAssetFrame[]; assets: AssetRecord[]; scenes: SceneDocument[];
  disabled: boolean; onApply: (commands: Record<string, unknown>[]) => Promise<boolean>;
  displayName?: string;
  initiallyOpen?: boolean; creating?: boolean; loopPolicies?: string[]; onCancel?: () => void;
  onDuplicate?: () => Promise<void>;
  onDelete?: () => Promise<void>;
}) {
  const [open, setOpen] = useState(initiallyOpen);
  const initialName = () => clip.display_name ?? displayName ?? clip.animation_id;
  const [name, setName] = useState(initialName);
  const [loopPolicy, setLoopPolicy] = useState(clip.loop_policy);
  const initial = () => clip.frame_refs.map((frame) => ({ frame }));
  const initialCadence = () => String(clip.frame_duration_ms[0] ?? 400);
  const clipHasMixedCadence = clip.frame_duration_ms.some((duration) => duration !== clip.frame_duration_ms[0]);
  const [cadence, setCadence] = useState(initialCadence);
  const [steps, setSteps] = useState(initial);
  const byId = new Map(frames.map(frame => [frame.frame_id, frame]));
  const firstFrame = byId.get(steps[0]?.frame);
  const cadenceNumber = Number(cadence);
  const frameRefs = steps.map(step => step.frame);
  const trimmedName = name.trim();
  const valid = steps.length > 0 && steps.length <= 256 && steps.every(step =>
    byId.has(step.frame) && byId.get(step.frame)?.width === firstFrame?.width && byId.get(step.frame)?.height === firstFrame?.height)
    && cadence.trim() !== "" && Number.isInteger(cadenceNumber) && cadenceNumber >= 1 && cadenceNumber <= 60000
    && trimmedName.length > 0 && trimmedName.length <= 64
    && (!loopPolicies || loopPolicies.includes(loopPolicy));
  const changed = creating || clipHasMixedCadence || loopPolicy !== clip.loop_policy
    || trimmedName !== initialName()
    || cadenceNumber !== (clip.frame_duration_ms[0] ?? 400)
    || JSON.stringify(frameRefs) !== JSON.stringify(clip.frame_refs);
  const frameLabel = (id: string) => {
    const asset = assets.find(item => item.frames.some(frame => frame.frame_id === id));
    const frame = asset?.frames.find(item => item.frame_id === id);
    return asset ? `${asset.display_name ?? asset.text ?? "Sprite"} / ${frame?.display_name ?? `Frame ${asset.frames.findIndex(item => item.frame_id === id) + 1}`}` : "Unavailable frame";
  };
  const users = scenes.flatMap(scene => (scene.objects ?? []).filter(object => object.animation_ref === clip.animation_id)
    .map(object => `${scene.display_name} / ${object.object_id}`));
  const move = (index: number, delta: number) => setSteps(current => {
    const next = [...current];
    [next[index], next[index + delta]] = [next[index + delta], next[index]];
    return next;
  });
  if (!open) return <button className="button secondary" type="button" disabled={disabled}
    onClick={() => { setName(initialName()); setSteps(initial()); setCadence(initialCadence()); setOpen(true); }}><Pencil size={14} />Edit clip</button>;
  const deleteDisabled = disabled || creating || onDelete === undefined || users.length > 0;
  return <section className="clip-editor" aria-label="Animation clip editor" onKeyDown={event => {
      if (event.key === "Escape") { event.stopPropagation(); setOpen(false); onCancel?.(); }
  }}>
    <div className="clip-editor-heading">
      <div>
        <h4>{displayName ?? clip.animation_id}</h4>
        {!creating && <p className="muted">Changes apply everywhere this animation is used.</p>}
      </div>
      {!creating && <div className="clip-editor-record-actions">
        <button className="button secondary" type="button" disabled={disabled || onDuplicate === undefined}
          onClick={() => { void onDuplicate?.(); }}><Copy size={14} />Duplicate</button>
        <button className="button danger" type="button" disabled={deleteDisabled}
          title={users.length > 0 ? "Clear this animation from scene objects before deleting it." : "Delete this animation"}
          onClick={() => { void onDelete?.(); }}><Trash2 size={14} />Delete animation</button>
      </div>}
    </div>
    <dl className="clip-editor-summary">
      <div><dt>Frames</dt><dd>{steps.length}</dd></div>
      <div><dt>Cadence</dt><dd>{cadence} ms</dd></div>
      <div><dt>Playback</dt><dd>{loopPolicy === "loop" ? "Loop" : loopPolicy === "once" ? "Play once" : loopPolicy}</dd></div>
      <div><dt>ID</dt><dd>{clip.animation_id}</dd></div>
    </dl>
    <details><summary>Used by {users.length} scene object{users.length === 1 ? "" : "s"}</summary>
      <ul>{users.map((user, i) => <li key={i}>{user}</li>)}</ul>
    </details>
    <fieldset disabled={disabled}>
      <section className="clip-editor-panel">
        <h5>Details</h5>
        <label className="clip-editor-name">Name<input type="text" maxLength={64} aria-label="Animation name"
          value={name} onChange={event => setName(event.target.value)} /></label>
      </section>
      <section className="clip-editor-panel">
        <h5>Playback</h5>
        <div className="clip-editor-settings">
          {loopPolicies && <label>Mode<select aria-label="Animation playback" value={loopPolicy} onChange={event => setLoopPolicy(event.target.value)}>
            {loopPolicies.map(policy => <option key={policy} value={policy}>{policy === "loop" ? "Loop" : policy === "once" ? "Play once" : policy}</option>)}
          </select></label>}
          <label>Cadence (ms)<input type="number" min={1} max={60000} step={1} aria-label="Animation cadence"
            value={cadence} onChange={event => setCadence(event.target.value)} /></label>
        </div>
      </section>
      {clipHasMixedCadence && <p className="muted">This clip has older mixed frame timings. Applying it will use one cadence for every frame.</p>}
      <section className="clip-editor-panel">
        <div className="clip-editor-panel-heading">
          <h5>Frame order</h5>
          <span>{steps.length} frame{steps.length === 1 ? "" : "s"}</span>
        </div>
        {steps.map((step, index) => <div className="clip-step" key={index}>
          <span className="clip-step-preview">{byId.get(step.frame) && <FramePreviewCanvas frame={byId.get(step.frame)!} />}</span>
          <div className="clip-step-frame">
            <span>Frame {index + 1}</span>
            <strong>{frameLabel(step.frame)}</strong>
            <small>{byId.has(step.frame) ? `${byId.get(step.frame)!.width}x${byId.get(step.frame)!.height}` : "Unavailable"}</small>
          </div>
          <div className="clip-step-actions">
            <button type="button" className="icon-button" title={`Move step ${index + 1} up`} disabled={index === 0} onClick={() => move(index, -1)}><ArrowUp size={14} /></button>
            <button type="button" className="icon-button" title={`Move step ${index + 1} down`} disabled={index === steps.length - 1} onClick={() => move(index, 1)}><ArrowDown size={14} /></button>
            <button type="button" className="icon-button" title={`Duplicate step ${index + 1}`} disabled={steps.length >= 256}
              onClick={() => setSteps(current => [...current.slice(0, index + 1), { ...step }, ...current.slice(index + 1)])}><Copy size={14} /></button>
            <button type="button" className="icon-button" title={`Remove step ${index + 1}`} disabled={steps.length <= 1}
              onClick={() => setSteps(current => current.filter((_, i) => i !== index))}><Trash2 size={14} /></button>
          </div>
        </div>)}
      </section>
      {!valid && <p role="status">Use a name from 1 to 64 characters. Frames must be available and the same size, with one whole cadence from 1 to 60000 ms and supported playback.</p>}
      <div className="clip-editor-actions">
        <button className="button primary" type="button" disabled={!valid || !changed} onClick={async () => {
          if (await onApply([{ kind: "animation.upsert", animation: { ...clip,
            display_name: trimmedName, loop_policy: loopPolicy, frame_refs: frameRefs, frame_duration_ms: frameRefs.map(() => cadenceNumber) } }])) { if (!initiallyOpen) setOpen(false); }
        }}>{creating ? "Create animation" : "Apply clip"}</button>
        <button className="button secondary" type="button" onClick={() => { setOpen(false); onCancel?.(); }}>Cancel</button>
      </div>
    </fieldset>
  </section>;
}
