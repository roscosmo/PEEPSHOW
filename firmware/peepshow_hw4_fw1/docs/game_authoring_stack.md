# Game Authoring Stack (Tiled + Project IDE)

This document defines the practical split between map authoring and gameplay authoring.

If this document conflicts with `docs/authority.md`, `docs/authority.md` wins.

---

## Ownership Split

### Tiled owns (spatial layer)

- Map geometry and tile layers
- Object placement (instances only)
- Collision/object markers
- Trigger regions
- Spawn markers
- Exit/door geometry

Tiled objects should carry lightweight references:

- `entity_id`
- `dialogue_id`
- `script_id`
- `item_id`
- `target_map`
- `target_spawn`

### Project IDE owns (gameplay layer)

- Package/runtime routing authoring (mode keys, pet menu slot actions, page targets)
- Entity definitions
- Runtime tuning profiles (controller/camera/input) referenced by game modes
- Dialogue data
- Script/action data
- Item database
- Cross-reference validation

The IDE is the source of truth for reusable gameplay definitions.

---

## Current v1 Files

Project data root:

- `Assets/game_project/project.json`
- `Assets/game_project/pages.json`
- `Assets/game_project/pet_menu_slots.json`
- `Assets/game_project/maps.json`
- `Assets/game_project/controller_profiles.json`
- `Assets/game_project/camera_profiles.json`
- `Assets/game_project/input_profiles.json`
- `Assets/game_project/entities.json`
- `Assets/game_project/dialogues.json`
- `Assets/game_project/scripts.json`
- `Assets/game_project/items.json`

Package routing root:

- `Assets/game_package/manifest.example.json`
  - `modes[]`
  - `pet_routes[]`
  - `pet_menu_items[]` (compiled from `pet_menu_slots.json`)

### Authoring Contract (Manifest + Project Split)

- `manifest.example.json` owns runtime package routing and mode descriptors.
- `game_project/*.json` owns reusable content domains and IDE-authored references.
- `pages.json` + `pet_menu_slots.json` compile into runtime pet menu routing payload.
- Controller/camera/input tuning values live in profile domains, not duplicated in mode records.

### IDE Tree Contract

- `Game Package`
  - `Game Modes`
  - `Pet Menu Slots`
- `Pages`
- `Maps`
- `Controller Profiles`
- `Camera Profiles`
- `Input Profiles`
- `Entities`
- `Dialogues`
- `Scripts`
- `Items`

### Field Wiring Matrix (v1)

| IDE field | JSON key | Runtime/tool consumer |
|---|---|---|
| Game Mode: `Map` | `manifest.modes[].scene_map_id` + `scene_tileset_id` | `tools/gen_game_package_manifest.py` -> mode blob fields |
| Game Mode: `Controller Profile` | `manifest.modes[].controller_profile_key` | generator resolves `controller_profiles.json` |
| Game Mode: `Camera Profile` | `manifest.modes[].camera_profile_key` | generator resolves `camera_profiles.json` |
| Game Mode: `Input Profile` | `manifest.modes[].input_profile_key` | generator resolves `input_profiles.json` |
| Game Mode: `Runtime Kind` | `manifest.modes[].runtime_kind` | `game_package_manifest` runtime config |
| Game Mode: `Resume Behavior` | `manifest.modes[].scene_lifecycle` | REALTIME retained resume policy |
| Pet Menu Slot: `Select Behavior` | `pet_menu_slots[].select_kind` | generator compiles to `pet_menu_items[].select_kind` |
| Pet Menu Slot: `Target Mode` | `pet_menu_slots[].mode_key` | generator resolves to `pet_menu_items[].arg0 = mode_id` |
| Pet Menu Slot: `Target Page` | `pet_menu_slots[].page_key` | generator resolves to `pet_menu_items[].arg0 = page_id` |
| Page: `Route Kind` + target | `pages[].route_kind` + `native_*_key` | generator emits `ui_page_registry_autogen.h` |
| Profile fields (controller/camera/input) | `*_profiles[].*` | generator materializes V2/V4/V5 runtime tuning fields |

### Runtime Wiring Status (Current)

- `script_id` / `dialogue_id` on Tiled `interact` objects are packed into map object args
  by `tools/gen_tiled_map_blob.py` (hash-based).
- Topdown runtime now consumes those object args on player interact input and records
  last-seen script/dialogue hashes in mode state.
- Full script execution + dialogue UI progression remains a separate bounded runtime step.

---

## v1 Tools

### Validator

- `tools/validate_game_project.py`

Checks:

- unique IDs in each domain
- cross-domain references (entity->dialogue, script actions, hooks)
- Tiled object reference integrity (`entity_id/dialogue_id/script_id/item_id`)
- map exit references (`target_map/target_spawn`)

Run:

```bash
python tools/validate_game_project.py
```

### Map Build + Runtime Registry

- `tools/build_game_maps.py`

Builds:

- `Assets/maps/build/<map_id>.tmap.bin`
- `Assets/maps/build/<map_id>.tset.bin`
- `Assets/game_project/build/map_registry.json`
- `Core/Inc/game_map_registry_autogen.h` (firmware runtime map lookup table)
- `Assets/game_project/build/map_debug_helpers_autogen.gdb` (debug install helpers)

Runtime cross-map transitions resolve `target_map` hashes through this generated
registry, so map count scales with `maps.json` entries rather than fixed
hardcoded map IDs in firmware.

Run:

```bash
python tools/build_game_maps.py
```

Debug flow:

- `source debug.gdb`
- `ps_topdown_mx_prepare` (reloads generated map install helpers and installs all maps from `maps.json`)
- `ps_topdown_mx_verify`

### IDE Scaffold

- `tools/game_ide.py`

Provides:

- left project tree rooted at `Game Package` with domains:
  `Game Modes`, `Maps`, `Controller Profiles`, `Camera Profiles`, `Input Profiles`,
  plus entities/dialogues/scripts/items
- entity form editor (v1 fields) with load/apply flow
- game mode form editor (`manifest.example.json` -> `modes[]`) with
  creator-facing mode identity (`id`, `display_name`) and profile references
- game mode map/profile selection is name-based in IDE; internal IDs stay in JSON/runtime data
- game mode form defaults to a basic authoring view; advanced/internal tuning is behind
  "Show Advanced Fields"
- game mode editing includes lifecycle controls
  `scene_lifecycle` and `resume_domain_id`
- JSON editor for selected record
- add/delete record
- save all domain files
- run validator from UI

Run:

```bash
python tools/game_ide.py
```

---

## Direction Rules

- Keep IDs stable and explicit across all domains.
- Keep Tiled object logic lightweight and reference-driven.
- Keep gameplay behavior in project-domain data, not map-local ad-hoc properties.
- Keep runtime consumption deterministic and bounded.
- Do not move hardware/RTOS ownership into game authoring tools.

---

## Next Recommended Expansion

After v1 scaffold is stable:

1. Expand form editing beyond entities (dialogues/scripts/items/maps).
2. Add script action templates and argument validation.
3. Add dialogue node editor with basic branching UI.
4. Add export stage that merges:
   - Tiled maps
   - project domains
   - asset IDs
   into runtime-friendly package tables.
