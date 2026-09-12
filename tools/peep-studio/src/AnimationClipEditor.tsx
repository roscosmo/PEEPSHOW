import { useState } from "react";
import { ArrowDown, ArrowUp, Copy, Pencil, Plus, Trash2 } from "lucide-react";
import { FramePreviewCanvas } from "./FramebufferCanvas";
import type { AssetRecord, AuthoredClip, CompiledAssetFrame, SceneDocument } from "./types";

export function AnimationClipEditor({ clip, frames, assets, scenes, disabled, onApply, displayName, initiallyOpen = false, creating = false, loopPolicies, onCancel }: {
  clip: AuthoredClip; frames: CompiledAssetFrame[]; assets: AssetRecord[]; scenes: SceneDocument[];
  disabled: boolean; onApply: (commands: Record<string, unknown>[]) => Promise<boolean>;
  displayName?: string;
  initiallyOpen?: boolean; creating?: boolean; loopPolicies?: string[]; onCancel?: () => void;
}) {
  const [open, setOpen] = useState(initiallyOpen);
  const [loopPolicy, setLoopPolicy] = useState(clip.loop_policy);
  const initial = () => clip.frame_refs.map((frame, i) => ({ frame, duration: String(clip.frame_duration_ms[i]) }));
  const [steps, setSteps] = useState(initial);
  const byId = new Map(frames.map(frame => [frame.frame_id, frame]));
  const firstFrame = byId.get(steps[0]?.frame);
  const valid = steps.length > 0 && steps.length <= 256 && steps.every(step =>
    byId.has(step.frame) && step.duration.trim() && Number.isInteger(Number(step.duration))
    && Number(step.duration) >= 1 && Number(step.duration) <= 60000
    && byId.get(step.frame)?.width === firstFrame?.width && byId.get(step.frame)?.height === firstFrame?.height)
    && (!loopPolicies || loopPolicies.includes(loopPolicy));
  const changed = creating || loopPolicy !== clip.loop_policy || JSON.stringify(steps) !== JSON.stringify(initial());
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
    onClick={() => { setSteps(initial()); setOpen(true); }}><Pencil size={14} />Edit clip</button>;
  return <section className="clip-editor" aria-label="Animation clip editor" onKeyDown={event => {
      if (event.key === "Escape") { event.stopPropagation(); setOpen(false); onCancel?.(); }
  }}>
    <h4>{displayName ?? clip.animation_id}</h4>
    {!creating && <p className="muted">Changes apply everywhere this animation is used.</p>}
    <details><summary>Used by {users.length} scene object{users.length === 1 ? "" : "s"}</summary>
      <ul>{users.map((user, i) => <li key={i}>{user}</li>)}</ul>
    </details>
    <fieldset disabled={disabled}>
      {loopPolicies && <label>Playback<select aria-label="Animation playback" value={loopPolicy} onChange={event => setLoopPolicy(event.target.value)}>
        {loopPolicies.map(policy => <option key={policy} value={policy}>{policy === "loop" ? "Loop" : policy === "once" ? "Play once" : policy}</option>)}
      </select></label>}
      {steps.map((step, index) => <div className="clip-step" key={index}>
        <span className="clip-step-preview">{byId.get(step.frame) && <FramePreviewCanvas frame={byId.get(step.frame)!} />}</span>
        <label>Frame {index + 1}<select aria-label={`Clip frame ${index + 1}`} value={step.frame}
          onChange={event => { const frame = event.target.value; setSteps(current => current.map((item, i) => i === index ? { ...item, frame } : item)); }}>
          {!byId.has(step.frame) && <option value={step.frame}>{step.frame} (unavailable)</option>}
          {frames.map(frame => <option key={frame.frame_id} value={frame.frame_id}>{frameLabel(frame.frame_id)}</option>)}
        </select></label>
        <label>Duration (ms)<input type="number" min={1} max={60000} step={1} aria-label={`Clip duration ${index + 1}`}
          value={step.duration} onChange={event => { const duration = event.target.value; setSteps(current => current.map((item, i) => i === index ? { ...item, duration } : item)); }} /></label>
        <div className="clip-step-actions">
          <button type="button" className="icon-button" title={`Move step ${index + 1} up`} disabled={index === 0} onClick={() => move(index, -1)}><ArrowUp size={14} /></button>
          <button type="button" className="icon-button" title={`Move step ${index + 1} down`} disabled={index === steps.length - 1} onClick={() => move(index, 1)}><ArrowDown size={14} /></button>
          <button type="button" className="icon-button" title={`Duplicate step ${index + 1}`} disabled={steps.length >= 256}
            onClick={() => setSteps(current => [...current.slice(0, index + 1), { ...step }, ...current.slice(index + 1)])}><Copy size={14} /></button>
          <button type="button" className="icon-button" title={`Remove step ${index + 1}`} disabled={steps.length <= 1}
            onClick={() => setSteps(current => current.filter((_, i) => i !== index))}><Trash2 size={14} /></button>
        </div>
      </div>)}
      <button type="button" className="button secondary" disabled={steps.length >= 256 || !frames.length}
        onClick={() => setSteps(current => [...current, { frame: frames[0].frame_id, duration: "400" }])}><Plus size={14} />Add step</button>
      {!valid && <p role="status">Frames must be available and the same size, with whole durations from 1 to 60000 ms and supported playback.</p>}
      <div className="clip-editor-actions">
        <button className="button primary" type="button" disabled={!valid || !changed} onClick={async () => {
          if (await onApply([{ kind: "animation.upsert", animation: { ...clip,
            loop_policy: loopPolicy, frame_refs: steps.map(step => step.frame), frame_duration_ms: steps.map(step => Number(step.duration)) } }])) { if (!initiallyOpen) setOpen(false); }
        }}>{creating ? "Create animation" : "Apply clip"}</button>
        <button className="button secondary" type="button" onClick={() => { setOpen(false); onCancel?.(); }}>Cancel</button>
      </div>
    </fieldset>
  </section>;
}
