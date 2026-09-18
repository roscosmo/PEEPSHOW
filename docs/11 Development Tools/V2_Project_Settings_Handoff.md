# API 48 Project Settings

Read using public `project.settings.get` with `project_revision`. The response
contains `project_revision` and `settings`. Settings also appear in normalized
`document.project.settings` when present.

Edit using `project.apply_commands` with the current revision:

```json
{
  "kind": "project.settings.set",
  "settings": {
    "sfx_import": {"normalization": "peak", "target_peak_dbfs": -6},
    "runtime_preferences": {"inactivity_timeout_ms": 30000}
  }
}
```

This is a whole-object replacement. Preserve other groups when changing one;
`settings: {}` clears all preferences. Both groups are optional, but their
fields are required when present. Unknown groups/fields are rejected.

- Normalization: `none` or `peak`. Target: integer -60..0 dBFS sample peak,
  not loudness normalization or device output volume. No implicit default.
- Inactivity preference: null for system selection or integer 1000..86400000
  milliseconds. Zero is not a disable command. Scene interaction policy remains
  separate and unchanged.
- Existing project revision, atomic batch, undo/redo and save behavior applies.
  A copied saved project retains settings; no new Save As operation is added.

## Important Capability Distinction

Gate from `service.hello.project_settings`:

- `persisted: true`, `undo_redo: true`, `runtime_encoded: false`.
- `sfx_import.status: persisted_only`, `import_applied: false`.
- `runtime_preferences.status: persisted_only`, `firmware_enforced: false`.

Studio may integrate preference persistence, but must not show these as applied
audio processing or active device policy. Do not introduce a Studio-side compiler
or normalization workaround. Existing WAVs, ADPCM, cue gain/priority, runtime
timeouts and exported eggs remain unchanged. Future enforcement needs a separate
capability increment. No firmware reflash is required.

Host tests cover V1/V2 byte-identical export, unchanged compiled audio, undo/redo,
save/reload, copied-project reopening, stale revisions and invalid data rejection.
