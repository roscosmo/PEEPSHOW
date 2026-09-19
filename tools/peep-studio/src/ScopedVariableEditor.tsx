import { useEffect, useState } from "react";
import { Plus, Trash2 } from "lucide-react";
import type { StateVariable } from "./types";

const ID_PATTERN = /^[a-z][a-z0-9_.-]{0,63}$/;
const INT32_MIN = -2147483648;
const INT32_MAX = 2147483647;

type Scope = "scene" | "package";

function numeric(value: number | boolean | undefined, fallback: number): number {
  return typeof value === "number" ? value : fallback;
}

export function ScopedVariableEditor({ scope, sceneId, variables, combinedCount, limit, disabled, onApply }: {
  scope: Scope;
  sceneId?: string;
  variables: StateVariable[];
  combinedCount: number;
  limit: number;
  disabled: boolean;
  onApply: (commands: Array<Record<string, unknown>>) => Promise<boolean>;
}) {
  const [adding, setAdding] = useState(false);
  const [id, setId] = useState("value");
  const [valueType, setValueType] = useState<"int32" | "bool">("int32");
  const [initial, setInitial] = useState<number | boolean>(0);
  const [minimum, setMinimum] = useState(0);
  const [maximum, setMaximum] = useState(1);
  const duplicate = variables.some(variable => variable.variable_id === id);
  const validRange = valueType === "bool" || (typeof initial === "number" && minimum >= INT32_MIN
    && maximum <= INT32_MAX && minimum <= initial && initial <= maximum);
  const canAdd = !disabled && combinedCount < limit && ID_PATTERN.test(id) && !duplicate && validRange;
  const command = (operation: "add" | "update" | "delete", variable?: StateVariable, variableId?: string) => ({
    kind: `${scope === "package" ? "package_variable" : "variable"}.${operation}`,
    ...(scope === "scene" ? { scene_id: sceneId } : {}),
    ...(variable ? { variable } : {}),
    ...(variableId ? { variable_id: variableId } : {}),
  });
  const declaration = (): StateVariable => valueType === "bool"
    ? { variable_id: id, value_type: "bool", initial: initial === true }
    : { variable_id: id, value_type: "int32", initial: numeric(initial, 0), minimum, maximum };

  useEffect(() => { setAdding(false); }, [scope, sceneId]);

  return <div className="variable-editor-list">
    {variables.length === 0 && !adding && <p className="muted">No {scope} variables.</p>}
    {variables.map(variable => <ScopedVariableRow key={variable.variable_id} variable={variable} disabled={disabled}
      onSave={next => onApply([command("update", next)])}
      onDelete={() => onApply([command("delete", undefined, variable.variable_id)])} />)}
    {adding ? <form className="variable-editor-row variable-add-form" onSubmit={event => {
      event.preventDefault();
      if (canAdd) void onApply([command("add", declaration())]).then(ok => { if (ok) setAdding(false); });
    }}>
      <label className="variable-id-field"><span>Name</span><input value={id} maxLength={64}
        pattern="[a-z][a-z0-9_.-]{0,63}" disabled={disabled} onChange={event => setId(event.target.value)} /></label>
      <label className="select-field">Type<select value={valueType} disabled={disabled} onChange={event => {
        const next = event.target.value as "int32" | "bool"; setValueType(next); setInitial(next === "bool" ? false : 0);
      }}><option value="int32">Number</option><option value="bool">True / false</option></select></label>
      {valueType === "bool" ? <label className="toggle-field"><input type="checkbox" checked={initial === true}
        disabled={disabled} onChange={event => setInitial(event.target.checked)} /><span>Starts true</span></label>
        : <div className="variable-range-fields">
          <label>Minimum<input type="number" value={minimum} onChange={event => setMinimum(Number(event.target.value))} /></label>
          <label>Start<input type="number" value={numeric(initial, 0)} onChange={event => setInitial(Number(event.target.value))} /></label>
          <label>Maximum<input type="number" value={maximum} onChange={event => setMaximum(Number(event.target.value))} /></label>
        </div>}
      {!validRange && <div className="field-error">Minimum, start and maximum must be ordered signed 32-bit integers.</div>}
      {duplicate && <div className="field-error">That name already exists in this scope.</div>}
      <div className="variable-form-actions"><button className="button secondary" type="button" onClick={() => setAdding(false)}>Cancel</button>
        <button className="button primary" type="submit" disabled={!canAdd}>Add variable</button></div>
    </form> : <button className="button secondary logic-add-button" type="button" disabled={disabled || combinedCount >= limit}
      onClick={() => setAdding(true)}><Plus size={13} /> Add variable</button>}
  </div>;
}

function ScopedVariableRow({ variable, disabled, onSave, onDelete }: {
  variable: StateVariable; disabled: boolean;
  onSave: (variable: StateVariable) => Promise<boolean>; onDelete: () => Promise<boolean>;
}) {
  const [initial, setInitial] = useState(variable.initial);
  const [minimum, setMinimum] = useState(numeric(variable.minimum, 0));
  const [maximum, setMaximum] = useState(numeric(variable.maximum, 1));
  useEffect(() => { setInitial(variable.initial); setMinimum(numeric(variable.minimum, 0)); setMaximum(numeric(variable.maximum, 1)); }, [variable]);
  const valid = variable.value_type === "bool" || (typeof initial === "number" && minimum <= initial && initial <= maximum);
  const next = variable.value_type === "bool" ? { ...variable, initial: initial === true }
    : { ...variable, initial: numeric(initial, 0), minimum, maximum };
  return <form className="variable-editor-row" onSubmit={event => { event.preventDefault(); if (valid) void onSave(next); }}>
    <div className="variable-row-heading"><strong>{variable.variable_id}</strong><span>{variable.value_type === "bool" ? "True / false" : "Number"}</span>
      <button className="icon-button danger" type="button" disabled={disabled} title="Delete variable" onClick={() => void onDelete()}><Trash2 size={13} /></button></div>
    {variable.value_type === "bool" ? <label className="toggle-field"><input type="checkbox" checked={initial === true}
      disabled={disabled} onChange={event => setInitial(event.target.checked)} /><span>Starts true</span></label>
      : <div className="variable-range-fields">
        <label>Minimum<input type="number" value={minimum} disabled={disabled} onChange={event => setMinimum(Number(event.target.value))} /></label>
        <label>Start<input type="number" value={numeric(initial, 0)} disabled={disabled} onChange={event => setInitial(Number(event.target.value))} /></label>
        <label>Maximum<input type="number" value={maximum} disabled={disabled} onChange={event => setMaximum(Number(event.target.value))} /></label>
      </div>}
    {!valid && <div className="field-error">Minimum, start and maximum must be ordered.</div>}
    <button className="button secondary variable-save-button" type="submit" disabled={disabled || !valid}>Save</button>
  </form>;
}
