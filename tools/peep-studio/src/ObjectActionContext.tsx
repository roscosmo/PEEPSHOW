import { createContext, useContext } from "react";
import type { PreviewSnapshot, RenderElement, SceneDocument, StateAction, StateRecord } from "./types";

export const ObjectActionContext = createContext<{
  scene: SceneDocument | null;
  preview: PreviewSnapshot | null;
  label: (element: RenderElement) => string;
}>({ scene: null, preview: null, label: element => element.element_id });

export function ObjectActionPosition({ objectId, action, targetState }: {
  objectId: string;
  action: StateAction;
  targetState?: StateRecord;
}) {
  const { scene, preview } = useContext(ObjectActionContext);
  const live = preview?.scene.scene_id === scene?.scene_id ? preview : null;
  const object = live?.objects?.find(item => item.object_id === objectId);
  const activeState = scene?.states?.find(state => state.state_id === live?.scene.state_id);
  const axes = (state?: StateRecord) => {
    const override = state?.object_overrides?.find(item => item.object_ref === objectId);
    return (["x", "y"] as const).filter(axis => override?.[axis] !== undefined &&
      (action.kind === "object.move_by" ? (action[axis === "x" ? "dx" : "dy"] ?? 0) !== 0 : action[axis] !== undefined))
      .map(axis => axis.toUpperCase()).join(" / ");
  };
  const masked = axes(activeState);
  const destinationMasked = axes(targetState);
  return <div className="object-action-position">
    {object ? <>
      <dl className="inspector-list" title="Screen coordinates in pixels: X from left, Y from top">
        <div><dt>Emulator state</dt><dd>{activeState?.display_name ?? live?.scene.state_id}</dd></div>
        <div><dt>Underlying position</dt><dd>X {object.underlying.x}, Y {object.underlying.y}</dd></div>
        <div><dt>Effective position</dt><dd>X {object.effective.x}, Y {object.effective.y}</dd></div>
      </dl>
      {masked && <p className="plain-rule-note">{masked} movement masked by the active state's override.</p>}
    </> : <p className="plain-rule-note">No current emulator position for this object.</p>}
    {destinationMasked && targetState?.state_id !== activeState?.state_id &&
      <p className="plain-rule-note">Destination {targetState?.display_name ?? targetState?.state_id}: {destinationMasked} movement masked by an override.</p>}
  </div>;
}
