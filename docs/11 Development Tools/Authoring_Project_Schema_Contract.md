# Authoring Project Schema Contract

This document defines the editable source-project format used by PeepShow game-authoring tools.

The authoring project is the GUI/tool-owned source of truth for content creation. It is not the firmware runtime format and is not the installable package format.

Related:

- [[Development_Tooling_Index]]
- [[Game_Authoring_API_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Package_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Package_Compatibility_Report_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]

---

## Purpose

The authoring project schema gives every future editor a stable target.

It allows PeepShow tools to:

- edit package metadata
- import and customize templates
- compose reusable Authoring Kits
- author prefabs
- author runtime units
- author hierarchical state machines
- author behavior graphs and behavior macros
- author transitions, guards, variables, and actions
- bind assets and content parameters
- define save/settings schemas
- select target profiles
- preserve editor layout and comments
- validate before package build
- feed preview/simulation tools
- compile deterministic package output

Tool UI choices such as React, Tauri, Electron, Python CLI, Rust helper, or Node process are implementation details around this schema.

---

## Layer Position

```text
visual editor / CLI tools
        |
authoring project schema
        |
validation and compiler pipeline
        |
compiled runtime logic tables and assets
        |
PeepPkg package blob
        |
Engine runtime hosts / digital twin / device
```

The editor owns source data.

The compiler owns package/runtime output.

The runtime executes validated compiled data.

---

## Non-Goals

The authoring project schema does not define:

- Platform hardware policy
- firmware runtime struct layout
- installed package storage layout
- raw package blob chunk encoding
- STM32, ThreadX, HAL, CubeMX, DMA, filesystem, or PMIC behavior
- Reference Game mechanics
- a mandatory GUI framework

Normal authoring source files may reference host source paths for editor/project convenience. Compiled package output must not contain host paths for runtime use.

---

## File Model

V1 should use a directory project model.

Conceptual layout:

```text
my_game.peepproj/
  project.json
  templates/
    virtual_pet.template.json
  kits/
    evolution.kit.json
    shop.kit.json
  prefabs/
    pet_actor.prefab.json
  graphs/
    pet_behavior.json
  macros/
    locked_gate_check.json
  scenes/
    map_scene.json
  assets/
    sprites/
    audio/
    maps/
  schemas/
    save.json
    content_parameters.json
  editor/
    layout.json
    notes.json
  validation/
    waivers.json
```

Rules:

- `project.json` is the root manifest.
- source files are editor/tool input only.
- generated files should live outside authored source or in an explicitly generated folder.
- editor layout and notes must be separable from package semantics.
- source paths are relative to the project root.
- package output must be reproducible from the project, selected target profile, tool versions, and build profile.

---

## Root Project Shape

Conceptual schema:

```text
peepshow_authoring_project:
  schema_id
  schema_version
  project_id
  project_name
  package_id
  package_version
  authoring_tool
  created_with
  last_saved_with
  selected_target_profile
  build_profiles[]
  templates[]
  authoring_kits[]
  prefabs[]
  behavior_macros[]
  runtime_units[]
  graphs[]
  scenes[]
  assets[]
  input_maps[]
  audio_profiles[]
  sensor_profiles[]
  communication_profiles[]
  time_power_profiles[]
  save_schema_ref
  content_parameter_schema_ref
  content_parameter_values_ref
  diagnostics_profile_ref
  validation_config
  editor_data_ref
```

Rules:

- `schema_id` and `schema_version` are mandatory.
- `project_id` is editor/tool identity and may differ from `package_id`.
- `package_id` is the install/runtime identity.
- schema versions must be recorded in package build outputs.
- a selected target profile may be provisional for authoring preview, but package export must use the profile rules defined by [[Target_Profile_Schema_Contract]].

---

## Stable IDs

Every authored object that can be referenced must have a stable ID.

Examples:

- runtime unit ID
- template ID
- Authoring Kit ID
- prefab ID
- behavior macro ID
- graph ID
- state ID
- transition ID
- action ID
- variable ID
- asset ID
- cue ID
- input action ID
- content parameter ID
- save field ID

Rules:

- display names may change without changing IDs.
- IDs must be deterministic and unique within their namespace.
- references must use IDs, not display names.
- deleted IDs must not be silently reused within the same project history where that would corrupt saves or references.
- generated runtime IDs must have a deterministic mapping from source IDs.

---

## Authoring Reuse Objects

Authoring projects may contain templates, Authoring Kits, prefabs, behavior graphs, and behavior macros.

These objects are editable source concepts. They are not installed as independent firmware objects and are not runtime class names.

Conceptual schema:

```text
template_ref:
  template_id
  display_name
  template_version
  source_ref
  imported_object_refs[]
  editable_slots[]
  required_capabilities[]
  optional_capabilities[]
  validation_rules[]

authoring_kit:
  kit_id
  display_name
  kit_version
  kit_type
  prefab_refs[]
  behavior_graph_refs[]
  behavior_macro_refs[]
  asset_refs[]
  content_parameter_refs[]
  save_schema_refs[]
  package_setting_refs[]
  required_capabilities[]
  optional_capabilities[]
  diagnostics_refs[]
  validation_rules[]
  generated_runtime_unit_refs[]

prefab:
  prefab_id
  display_name
  actor_or_entity_kind
  asset_refs[]
  local_parameters[]
  behavior_graph_refs[]
  behavior_macro_refs[]
  default_scene_bindings[]

behavior_macro:
  macro_id
  display_name
  input_slots[]
  output_slots[]
  graph_fragment_ref
  bounds
```

Rules:

- `Authoring Kit` is the canonical source-level name for reusable gameplay systems.
- do not use `module` as the general name for authoring reuse; `LP_MODULE` remains a runtime class token and hardware modules remain Platform/Hardware terminology.
- templates may import or instantiate Authoring Kits, prefabs, behavior graphs, behavior macros, assets, schemas, and content parameter defaults.
- Authoring Kits may generate one or more runtime units, but generated units must still declare a valid runtime class.
- prefabs may attach behavior graph references and local parameter defaults, but they must not bypass runtime-unit validation.
- behavior macros must compile into bounded graph/action data and must not introduce unbounded loops or hidden capability use.
- imported objects must preserve source/version metadata for validation, compatibility reports, and deterministic rebuilds.
- editable slots must be explicit so template customization changes package content without silently changing Platform policy.

Example:

```text
Virtual Pet Template
  editable slots:
    pet_sprites
    hunger_rates
    evolution_table
    care_triggers

  Authoring Kits:
    Pet Stats Kit
    Evolution Kit
    Feeding Kit
    Sleep/Idle Kit

  generated runtime units:
    ambient_pet: LP_GRAPH
    care_interaction: LP_MODULE
    play_microgame: RT_SCENE
```

---

## Runtime Units

Authoring projects declare one or more runtime units.

Conceptual schema:

```text
runtime_unit:
  unit_id
  display_name
  runtime_class          # LP_GRAPH, LP_MODULE, RT_SCENE
  entry_ref
  allowed_transitions[]
  required_capabilities[]
  optional_capabilities[]
  reactive_wait_default
  input_lock_policy_ref
  suspend_behavior
  resume_behavior
  fallback_unit
  budgets
```

Rules:

- every project must declare a default runtime unit.
- every runtime unit must declare exactly one runtime class.
- `SHELL` and `INSTALLER` are Platform-owned and are not authored package runtime units.
- transitions between runtime units must be declared.
- runtime unit push/pop behavior must fit the selected target profile limits.
- `RT_SCENE` units must declare frame budget, meaningful activity, suspend/resume behavior, bounded lock deferrals where used, and a reactive fallback route.
- every state/block that can settle must resolve a reactive wait contract.
- automatic input locking is optional package policy; enabled policies require an admitted route and bounded deferrals.

---

## Reactive Wait And Input Lock Policies

Reactive wait and input-lock records are authoring semantics, not hardware configuration.

Conceptual schema:

```text
reactive_wait_policy:
  policy_id
  settled_view_ref
  waiting_visual_ref
  reduced_waiting_visual_ref
  hold_fallback_allowed
  event_interests[]
  schedules[]
  gameplay_timeout_transitions[]
  wake_intents[]
  input_lock_context_ref

input_lock_policy:
  policy_id
  enabled
  meaningful_activity_sources[]
  lock_route                 # preserve_state, transition_to, exit_to_shell
  lock_target                # only for transition_to
  locked_waiting_visual_ref
  bounded_deferrals[]
```

Rules:

- a package may set `enabled = false`; no lock route is then executed.
- the authoring schema does not expose a lock-timeout field because the active target/system policy owns that timeout.
- an enabled policy must declare exactly one admitted lock route.
- Start is reserved for system unlock while locked; the authoring tool must not route that physical press to a package action.
- the package receives `DEVICE_LOCKED` and `DEVICE_UNLOCKED` lifecycle events according to [[Runtime_Host_Contract]].
- an `input_lock_context_ref` may select only entries declared by the referenced package policy; it cannot override timeout, route, or unlock semantics.
- every deferral must have a statically provable completion bound or timeout; unbounded deferral is a validation error.
- gameplay inactivity transitions remain ordinary schedules and do not mutate the system lock timer.
- neither record exposes STOP, LPBAM, DMA, SRAM4, wake pins, clocks, or display rows.

---

## Hierarchical State Machines

The editor may expose hierarchical state machines.

Example:

```text
Pet
  Awake
    Idle
    Eating
    Playing
  Sleeping
    LightSleep
    DeepSleep
```

Conceptual schema:

```text
hsm_graph:
  graph_id
  entry_state
  states[]
  transitions[]
  variables[]
  timers[]
  action_tables[]
  bounds

state:
  state_id
  parent_state_id
  display_name
  entry_actions[]
  exit_actions[]
  substates[]
```

Rules:

- hierarchical authoring is allowed only if compilation produces deterministic bounded runtime tables.
- every graph has one declared entry state.
- every state transition target must resolve.
- entry, exit, and transition actions must be bounded.
- transition selection order must be deterministic.
- parallel regions are allowed only if their scheduling, event ordering, and action cost are statically bounded.
- history states, deep history, or deferred events may exist only if the compiler can express them in bounded PeepOS runtime primitives.
- the editor may show hierarchy; the runtime package receives validated flattened or table-driven logic.

---

## Events, Guards, And Actions

Authoring source may define symbolic events, guards, and actions.

Allowed event sources are those defined by [[Runtime_Logic_State_API_Contract]]:

- lifecycle
- input action
- delayed timer
- local calendar schedule
- wake/resume reason
- render or animation completion
- audio timeline marker
- sensor event/snapshot
- communication session/message
- save/settings completion
- package diagnostic/fault route

Guard expressions may read:

- graph variables
- package settings
- save-backed values through schema
- event payload fields
- normalized sensor values
- PeepOS local calendar/logical time
- declared capability state
- deterministic random values where seed policy is declared

Actions must compile to symbolic Engine requests, such as:

- set variable
- transition state
- transition runtime unit
- request render/update
- request input focus
- request audio cue or BBB pattern
- request save/settings read or write
- schedule delayed/calendar event
- request sensor context
- send communication message
- emit package diagnostic marker

Rules:

- no authored event, guard, or action may reference hardware, RTOS, filesystem, raw memory, or Platform internals.
- every expression has bounded cost.
- every action list has bounded cost.
- no authored action may block, sleep, spin, retry forever, or wait for hardware completion.
- validation failures must report PeepOS authoring terms.

---

## Assets

Authoring projects may reference source assets.

Examples:

- PNG or Aseprite sprite sources
- five-color indexed tone5 art sources
- Tiled map sources
- WAV/audio sources
- RTTTL BBB melody sources
- font sources
- text/localization source tables

Conceptual schema:

```text
asset_ref:
  asset_id
  asset_type
  source_path
  source_format
  compiler_profile
  runtime_units[]
  required_capability_refs[]
  bounds
```

Rules:

- source asset paths are editor/compiler inputs only.
- runtime packages reference compiled asset IDs, not source paths.
- source assets must compile to bounded package asset formats.
- missing assets may be placeholders only in profiles that allow placeholders.
- shipping output must not depend on editor source files.
- asset compile settings must be deterministic and versioned.

---

## Content Parameters

Content parameters are package-authored values, not Platform knobs.

Authoring projects may define:

- schema
- default values
- authoring UI hints
- ranges
- enum labels
- validation rules
- preview overrides

Rules:

- content parameters may affect package behavior only.
- content parameters must not mutate Platform knobs, Platform settings, calibration, PMIC policy, power policy, storage policy, communication policy, or hardware policy.
- preview overrides must be recorded separately from source defaults where deterministic replay requires it.

---

## Save And Package Settings Schema

Authoring projects may define package save/settings schemas.

Rules:

- save fields require stable IDs, types, defaults, bounds, and migration policy.
- package settings are package-owned preferences only.
- save-backed runtime variables must map to the save/settings schema.
- schema changes require versioning.
- package output must include save schema summary and compatibility metadata.
- package-owned settings must not mutate Platform settings or Platform knobs.

---

## Editor-Only Data

The editor may store visual and workflow metadata.

Examples:

- node positions
- collapsed groups
- colors
- comments
- bookmarks
- panel layout
- editor selection state
- visual preview settings

Conceptual schema:

```text
editor_data:
  layout_version
  graph_layouts[]
  comments[]
  bookmarks[]
  local_ui_state
```

Rules:

- editor-only data must be clearly namespaced.
- editor-only data must not affect package runtime behavior.
- package builds must be deterministic when editor-only data changes.
- editor-only data must not be installed to the device except where a future debug artifact explicitly records it as tooling metadata.

---

## Validation Config And Waivers

Authoring projects may store validation preferences and runtime-safe waivers.

Conceptual schema:

```text
validation_config:
  build_profile
  selected_target_profile
  validation_ruleset_version
  allowed_placeholder_policy
  waivers[]

waiver:
  waiver_id
  validation_code
  affected_object_ref
  reason
  owner
  created_at
  expires_or_remove_when
  allowed_build_profiles[]
```

Rules:

- waivers must target stable validation codes.
- waivers may not suppress fatal errors.
- waivers may not suppress runtime safety, package integrity, required capability, power policy, deterministic build, or schema errors.
- waivers must appear in compatibility reports.
- preview/mock waivers are not hardware bring-up evidence.

---

## Validation Codes

Validation codes must be stable enough for CLI, GUI, compatibility reports, and future dashboards.

Initial code families:

| Family | Examples |
|---|---|
| `PROJECT_*` | `PROJECT_SCHEMA_UNSUPPORTED`, `PROJECT_ID_MISSING` |
| `RUNTIME_*` | `RUNTIME_ENTRY_MISSING`, `RUNTIME_UNIT_UNKNOWN`, `RUNTIME_CLASS_INVALID` |
| `TEMPLATE_*` | `TEMPLATE_SOURCE_MISSING`, `TEMPLATE_SLOT_UNRESOLVED`, `TEMPLATE_VERSION_UNSUPPORTED` |
| `KIT_*` | `KIT_REF_UNKNOWN`, `KIT_CAPABILITY_FALLBACK_MISSING`, `KIT_RUNTIME_OUTPUT_INVALID` |
| `PREFAB_*` | `PREFAB_ASSET_MISSING`, `PREFAB_BEHAVIOR_REF_UNKNOWN` |
| `GRAPH_*` | `GRAPH_ENTRY_MISSING`, `GRAPH_STATE_UNREACHABLE`, `GRAPH_TRANSITION_TARGET_UNKNOWN` |
| `MACRO_*` | `MACRO_SLOT_UNBOUND`, `MACRO_EXPANSION_UNBOUNDED` |
| `ACTION_*` | `ACTION_BUDGET_EXCEEDED`, `ACTION_UNBOUNDED_LOOP`, `ACTION_FORBIDDEN_PLATFORM_REF` |
| `GUARD_*` | `GUARD_TYPE_MISMATCH`, `GUARD_COST_EXCEEDED` |
| `ASSET_*` | `ASSET_MISSING`, `ASSET_TOO_LARGE`, `ASSET_FORMAT_UNSUPPORTED` |
| `SAVE_*` | `SAVE_SCHEMA_MISSING`, `SAVE_MIGRATION_INVALID` |
| `CAPABILITY_*` | `CAPABILITY_REQUIRED_BLOCKED`, `CAPABILITY_FALLBACK_MISSING` |
| `POWER_*` | `POWER_IDLE_ROUTE_MISSING`, `POWER_CADENCE_EXCEEDED`, `POWER_REALTIME_FALLBACK_MISSING` |
| `TARGET_*` | `TARGET_PROFILE_MISSING`, `TARGET_PROFILE_PENDING_FOR_SHIPPING` |
| `BUILD_*` | `BUILD_NONDETERMINISTIC_OUTPUT`, `BUILD_SCHEMA_VERSION_MISMATCH` |

Rules:

- codes are for tools and reports.
- normal GUI messages should translate codes into clear PeepOS authoring language.
- low-level forbidden-reference codes are internal verifier evidence for toolchain defects or corrupted artifacts, not normal game-author UX.

---

## Preview Runtime

The authoring preview runtime is not the HW6 digital twin.

It may use `HOST_AUTHORING_PREVIEW` and provisional profiles to make authoring productive before hardware validation is complete.

Allowed preview behavior:

- run HSM/state logic
- inject fake inputs
- advance fake time
- inject fake light, step, motion, or communication events
- show logical display preview
- show validation warnings
- simulate save data

Rules:

- preview output is not hardware bring-up evidence.
- preview mocks must be labeled.
- preview cannot grant shipping capabilities blocked by the selected target profile.
- `HOST_DIGITAL_TWIN_HW6` remains blocked until measured HW6 Platform behavior exists.

---

## Compiler Boundary

The compiler converts authoring source into package/runtime artifacts.

Expected outputs:

- normalized package manifest
- compiled runtime unit table
- compiled state/action/guard/variable tables
- compiled asset chunks
- save/settings schema metadata
- compatibility report
- `PeepPkg` package blob

Rules:

- compiled output must be deterministic for identical semantic inputs.
- editor-only data must not affect package output.
- source hierarchy may compile to flattened runtime tables.
- installable output must not be emitted if validation fails.
- firmware install validation still re-checks package integrity and compatibility.

---

## Toolchain Interface

Any GUI or CLI should call the toolchain through stable operations:

```text
project.load
project.save
project.import_template
project.import_kit
project.validate
project.preview
project.build_package
project.export_assets
project.generate_compatibility_report
project.clean_generated
```

Rules:

- operation results are schema-versioned.
- validation results use stable codes.
- package builds record selected target profile, tool versions, schema versions, and content hashes.
- the GUI must not bypass validation when exporting installable artifacts.
- the same validation/build path should be usable by CLI and GUI.

---

## Technology Guidance

The schema does not require a specific GUI framework.

Recommended direction for the visual editor:

- modern reactive UI
- typed source models
- graph editor with node/edge data model
- property inspector driven from schema metadata
- preview runtime isolated from package compiler

Python remains appropriate for:

- asset conversion
- validators
- package compilation
- orchestration
- command-line tooling

Python GUI frameworks should not be assumed as the primary editor direction.

---

## Validation Cases

1. project with valid metadata, one runtime unit, one entry graph, and complete reactive-wait contracts validates.
2. graph with missing entry state fails validation.
3. transition to unknown state fails validation.
4. transition to undeclared runtime unit fails validation.
5. editor-only node layout change does not change package output checksum.
6. source asset path appears in authoring data but not in compiled runtime package data.
7. placeholder sprite validates in authoring preview where allowed and fails shipping export where disallowed.
8. waiver for placeholder art appears in compatibility report.
9. waiver for unbounded action loop is rejected.
10. `RT_SCENE` without meaningful-activity rules and a reactive fallback fails validation.
11. package requiring a capability still pending on HW6 fails shipping export.
12. authoring preview mock sensor data does not count as hardware evidence.
13. generated package build includes schema versions, tool versions, target profile, and compatibility report.
14. package may disable automatic input locking.
15. enabled input lock without an admitted route fails validation.
16. unbounded input-lock deferral fails validation.
17. Start used to unlock is represented as a consumed system action plus symbolic lifecycle event, not a package action.
18. package-authored system lock timeout fails validation.

---

## Rule

The authoring project schema is editable source.

Compiled packages are deterministic runtime data.

Editor convenience must never become hidden firmware behavior.
