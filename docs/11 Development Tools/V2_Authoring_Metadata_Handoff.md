# API 47: Object Names and Asset Tags

Studio can integrate these through public `project.apply_commands`; ordinary
project revision, undo/redo and save/reload handling applies.

## Object Names

```json
{"kind":"object.rename","scene_id":"garden","object_id":"selection_marker","display_name":"Selection marker"}
```

- Optional `display_name` lives on the native V2 object definition. `object.add`
  accepts it too. Read it from normalized scene objects or placement ownership.
- Use `object_id` as the display fallback when absent. Keep IDs as selector
  values and references; names need not be unique.
- Nonblank, 1..96 characters. No state override or runtime text behavior.
- Gate from `service.hello.scene_object_authoring.display_names` and per-scene
  `object_display_names` / `supported_commands`.

## Asset Tags

```json
{"kind":"asset.set_tags","asset_id":"selection_sheet","tags":["UI","Selection"]}
```

```json
{"kind":"audio_asset.set_tags","asset_id":"selection_audio","tags":["UI","Short cue"]}
```

- Whole-list replacement; `[]` clears. Unknown assets are rejected.
- Up to 16 distinct case-sensitive tags, 1..32 characters each, no surrounding
  whitespace. Ordering is preserved. No hierarchical grouping semantics.
- Stored on sprite/audio source catalog records and exposed on normalized
  `assets` / `audio_assets`. Missing sprite tags mean `[]`; normalized audio
  assets expose `[]` when absent. Cues, frames and animations are not tagged.
- Existing asset upsert commands accept tags but replace the entire record:
  preserve tags when sending a replacement, or prefer the dedicated commands.
- Gate from `service.hello.asset_metadata.tags`; commands are also advertised
  in the existing visual/audio asset command lists.

## Persistence and Runtime

Metadata survives save/reload and copying the saved project directory to a new
location. The backend has no separate Save As operation; Studio should preserve
the saved source files in its existing Save As flow.

No IDs, references, firmware behavior or runtime package fields change.
Focused host tests verify byte-identical exports, unchanged object preview,
undo/redo, invalid-input atomicity, save/reload and copied-project reopening.
No hardware test or firmware reflash is required for this increment.

This does not advertise project settings, system-font text, scene-exit actions,
expanded timer execution, memory or parallel regions.
