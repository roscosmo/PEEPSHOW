import type { StateAction } from "./types";

export function ObjectMotionFields({ action, index, disabled, onCommit }: {
  action: StateAction; index: number; disabled: boolean; onCommit: (action: Record<string, unknown>) => void;
}) {
  const relative = action.kind === "object.move_by";
  const axes = relative ? ["dx", "dy"] as const : ["x", "y"] as const;
  return <div className={`effect-motion-card ${relative ? "relative" : "absolute"}`}>
    <div className="effect-motion-heading">
      <strong>{relative ? "Relative movement" : "Stored position"}</strong>
      <span>{relative ? "Adds to the object's stored position" : "Sets the object's stored position"}</span>
    </div>
    <div className="effect-coordinate-fields">
      {axes.map(axis => {
        const value = action[axis];
        const enabled = value !== undefined;
        const otherEnabled = axes.some(other => other !== axis && action[other] !== undefined);
        const label = axis === "dx" ? "X (+ right)" : axis === "dy" ? "Y (+ up)"
          : axis === "x" ? "X (from left)" : "Y (from top)";
        return <label className="effect-field" key={axis}>
          <span className="object-action-axis"><input type="checkbox" aria-label={`Effect ${index} use ${axis}`} checked={enabled}
            disabled={disabled || (enabled && !otherEnabled)} onChange={event => {
              const next = { ...action };
              if (event.target.checked) next[axis] = 0;
              else delete next[axis];
              onCommit(next);
            }} /> {label}</span>
          <input key={`${action.object_ref}:${value}`} aria-label={`Effect ${index} ${axis}`} type="number" step={1}
            min={-2147483648} max={2147483647} defaultValue={value ?? ""} disabled={disabled || !enabled}
            placeholder="Unchanged" onBlur={event => {
              const raw = event.currentTarget.value;
              const parsed = Number(raw);
              if (!raw.trim() || !Number.isInteger(parsed) || parsed < -2147483648 || parsed > 2147483647) {
                event.currentTarget.value = String(value ?? "");
              } else if (parsed !== value) onCommit({ ...action, [axis]: parsed });
            }} onKeyDown={event => {
              if (event.key === "Escape") {
                event.stopPropagation(); event.currentTarget.value = String(value ?? ""); event.currentTarget.blur();
              } else if (event.key === "Enter") event.currentTarget.blur();
            }} />
        </label>;
      })}
    </div>
  </div>;
}
