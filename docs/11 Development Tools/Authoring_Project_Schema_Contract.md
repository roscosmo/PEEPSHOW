# Authoring Project Schema Contract

This document defines the editable source-project format used by PeepShow game-authoring tools.

The authoring project is the GUI/tool-owned source of truth for content creation. It is not the firmware runtime format and is not the installable package format.

Related:

- [[Development_Tooling_Index]]
- [[Authoring_Tool_Architecture]]
- [[Game_Authoring_API_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Scene_Runtime_and_Interaction_Model]]
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
- author package scenes and transitions
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

The editable project directory keeps the `.peepproj` suffix. The compiler
emits the installable PeepPkg blob with the `.egg` suffix. Source and compiled
artifacts use different suffixes deliberately: tools edit `.peepproj`, while
PeepOS installs and loads `.egg`.

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

The first executable authoring subset is defined by:

- `schemas/authoring/peepshow-project-v1.schema.json`
- `schemas/authoring/state-scene-v1.schema.json`

This subset covers package identity, target selection, STATE scene source
references, bounded variables, symbolic input actions, guarded routes,
retained render elements, reactive wait policy, and waiting visuals. It is a
strict subset of this contract and does not redefine the future complete
schema.

The current executable STATE input-source set is `BUTTON_A`, `BUTTON_B`,
`BUTTON_L`, `BUTTON_R`, `BUTTON_START`, `JOY_LEFT`, `JOY_RIGHT`, `JOY_UP`,
`JOY_DOWN`, `JOY_UP_LEFT`, `JOY_UP_RIGHT`, `JOY_DOWN_LEFT`, and
`JOY_DOWN_RIGHT`. Each input action may bind one `event_kind`: `press`,
`release`, `hold`, or `repeat`; omitted `event_kind` remains backward-compatible
`press`. START permits package `press` only because long START gestures are
system-owned for manual `INACTIVE` and shipping-entry behavior. Joystick
sources are normalized logical activations; authored content does not receive
raw TMAG values, calibration records, wake pins, or hardware thresholds.
Diagonal joystick bindings require `joystick_policy: "eight_way"`. Normalized
vector bindings are not part of this subset yet.

In the executable STATE subset, every route declares exactly one destination:

- `target_state` names a state in the route's source scene.
- `target_scene` names another STATE scene in the same project/package.

An authoring scene may also declare bounded semantic scene exits:

```text
scene_exits[]:
  scene_exit_id
  display_name
  target_scene
```

A scene exit declares a destination endpoint; it is not an input binding or an
executable transition by itself. Local STATE routes may identify the endpoint
with `scene_exit_ref` while retaining the corresponding `target_scene` used by
the executable subset. The service owns keeping those two fields consistent.
Creating an exit must not create an input action, choose a trigger, add a route,
or attach the exit to every state. The destination scene's declared
`entry_state` is the corresponding scene-entry endpoint.

When `target_scene` is used, `actions` may be empty or contain only
package-global `play_sfx` actions. Guards still evaluate in the source scene.
The cue request is committed only after the destination loads successfully and
may finish while that destination is active. A successful replacement enters
the destination entry state, restores its local variables to authored initial
values, and begins a new presentation epoch. Unknown scene targets, both
target fields, neither target field, or scene-local variable, render, element,
or system actions on a scene-target route fail validation.

The executable STATE sampled-SFX source subset uses optional records in an
asset catalog:

```text
audio_assets[]:
  asset_id
  asset_type = sampled_sfx
  source_path
  source_format = wav

audio_cues[]:
  cue_id
  asset_ref
  priority       # 0..255
  volume         # 0..255
```

A local STATE route may include
`{"kind":"play_sfx","cue_ref":"stable.cue.id"}` in its ordered action list.
The cue must exist. Direct `target_scene` routes remain actionless. WAV paths
are source-only; packages contain compiled audio IDs, metadata, and ADPCM bytes,
never host paths. The current subset is one-shot only and rejects looping,
music, streaming, and procedural audio.

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
  entry_scene
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

- scene ID
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
- generated scene/runtime IDs must have a deterministic mapping from source IDs.

---

## Authoring Reuse Objects

Authoring projects may contain templates, Authoring Kits, prefabs, behavior graphs, and behavior macros.

These objects are editable source concepts. They are not installed as independent firmware objects and are not scene type names.

Conceptual schema:

```text
template_ref:
  template_id
  display_name
  template_version
  source_ref
  imported_object_refs[]
  editable_slots[]
  required_service_refs[]
  optional_service_fallbacks[]
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
  required_service_refs[]
  optional_service_fallbacks[]
  diagnostics_refs[]
  validation_rules[]
  generated_scene_refs[]

prefab:
  prefab_id
  display_name
  actor_or_entity_kind
  editor_node_kind
  asset_refs[]
  local_parameters[]
  editable_slots[]
  declared_outputs[]
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
- do not use `module` as the general name for authoring reuse; hardware and firmware modules remain Platform terminology, not package scene types.
- templates may import or instantiate Authoring Kits, prefabs, behavior graphs, behavior macros, assets, schemas, and content parameter defaults.
- Authoring Kits may generate one or more scenes, but generated scenes must still declare a valid scene type.
- prefabs may attach behavior graph references and local parameter defaults, but they must not bypass scene validation.
- prefabs may appear as one editor node while compiling into multiple bounded
  STATE records, variables, routes, guards, actions, render elements, and
  waiting visuals.
- prefab nodes must expose declared editable slots rather than arbitrary
  internal mutation. Generated internals may be displayed read-only for
  inspection/debugging.
- prefab nodes may expose declared outputs. Each local STATE output connects to
  at most one destination state or prefab output target.
- behavior macros must compile into bounded graph/action data and must not introduce unbounded loops or hidden capability use.
- imported objects must preserve source/version metadata for validation, compatibility reports, and deterministic rebuilds.
- editable slots must be explicit so template customization changes package content without silently changing Platform policy.

Menu prefab target:

- a menu prefab is one self-contained prefab node, not a pile of mandatory
  user-authored graph nodes.
- it has a default navigation input template.
- it owns the selection variable and normal up/down navigation logic by default.
- it exposes editable slots for menu type, item labels, selected-item marker,
  visuals, and confirm/cancel behavior.
- it exposes one routable output per item choice.
- advanced authors may copy or convert a prefab into custom editable source
  records when they need behavior outside the prefab slots.

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

  generated scenes:
    ambient_pet: STATE_SCENE
    care_interaction: STATE_SCENE
    intro_animation: SEQUENCE_SCENE
    play_microgame: PROGRAM_SCENE
```

---

## Package Scenes

Authoring projects declare one or more package scenes and one entry scene.

Conceptual schema:

```text
scene:
  scene_id
  display_name
  scene_type             # STATE_SCENE, SEQUENCE_SCENE, PROGRAM_SCENE
  entry_ref
  object_refs[]
  world_ref              # optional; STATE_SCENE only
  presentation_ref
  allowed_transitions[]
  derived_capability_preview
  reactive_wait_default
  interaction_policy_ref
  suspend_behavior
  resume_behavior
  inactive_route
  failure_route
  budgets
```

Rules:

- every project must declare an `entry_scene`.
- every scene must declare exactly one scene type.
- `SHELL` and `INSTALLER` are Platform-owned hosts and are not authored package scenes.
- transitions between scenes must be declared and bounded.
- scene push/pop behavior must fit the selected target profile limits.
- every `STATE_SCENE` state/block that can settle must resolve a reactive wait contract.
- every `SEQUENCE_SCENE` must declare bounded tracks, target FPS, scene-end route, and suspend/resume behavior; a `TIMEOUT` package also requires an inactive route to a `STATE_SCENE` or shell.
- every `PROGRAM_SCENE` must declare instruction/memory/frame budgets, suspend/resume behavior, and failure routes; a `TIMEOUT` package also requires an inactive route.
- world-enabled `STATE_SCENE` records must satisfy [[State_Scene_World_Entity_and_Turn_Contract]].
- `REALTIME_SCENE` is obsolete terminology and is rejected as an unknown scene type.
- every package selects `CONTINUOUS` or `TIMEOUT`; only `TIMEOUT` requires an admitted automatic-timeout route and bounded deferrals, while both modes admit system-owned manual inactivity.
- capability declarations shown by tools are compiler-derived from scene content, service use, and fallback structure.

### World And Entity Authoring Records

Conceptual source schema:

```text
world:
  world_id
  map_source_ref
  collision_layer_ref
  camera_policy_ref
  entity_definition_refs[]
  entity_instances[]
  collection_defs[]
  turn_controller_ref
  property_schema_ref
  budgets

entity_definition:
  definition_id
  visual_ref
  property_schema_ref
  default_property_values[]
  tags[]
  collision_ref
  behavior_ref
  inventory_schema_ref

entity_instance:
  instance_id
  definition_ref
  world_x
  world_y
  property_overrides[]

turn_controller:
  controller_id
  phases[]
  per_phase_candidate_max
  per_phase_operation_max
  failure_route
```

Source-level names, prefab inheritance, Tiled object records, and tag strings are
resolved to stable IDs and bounded tables during compilation. Runtime packages
do not retain authoring graphs, JSON objects, dynamic dictionaries, or arbitrary
collections. Tools must show target-profile entity, operation, render, journal,
and result-payload budgets before export.

---

## Reactive Wait And Interaction Policies

Reactive wait and interaction-policy records are authoring semantics, not hardware configuration.

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
  interaction_context_ref

waiting_visual:
  waiting_visual_id
  presentation_id
  phase_quantum_multiple
  combined_step_count
  settled_step
  cycle_policy
  elements[]

waiting_visual_element:
  element_id
  visual_ref
  logical_bounds
  phase_visual_refs[]
  step_phase_indices[]

interaction_policy:
  policy_id
  mode                       # continuous, timeout
  meaningful_activity_sources[]
  inactive_route             # preserve_scene, transition_to_scene, exit_to_shell
  inactive_target_scene      # only for transition_to_scene
  inactive_waiting_visual_ref
  bounded_deferrals[]
```

`event_interests[]` may be empty while a STATE scene is being authored. An
empty list compiles as zero event-interest records and must not force tools to
invent an input binding or transition.

Rules:

- every package declares exactly one interaction mode: `continuous` or `timeout`.
- `continuous` disables automatic inactivity timeout and must not declare inactive-route or inactivity-deferral fields; system-owned manual inactivity preserves the current scene.
- `timeout` enables the system `ACTIVE`/`INACTIVE` lifecycle and must declare exactly one admitted inactive route.
- the authoring schema does not expose a numeric inactivity-timeout field because the active target/system policy owns that timeout.
- the system activation gesture is target-owned; HW6 initially uses Start, while future target profiles may admit another button or a chord such as `L+R`.
- the authoring tool must not route the physical activation gesture to a package action for the same event.
- while active, the authoring tool may bind short `START`; firmware publishes it only on release before the target-owned manual-INACTIVE threshold, while a hold reaching that threshold is consumed and never reaches package logic.
- the package receives `DEVICE_INACTIVE` and `DEVICE_ACTIVE` lifecycle events according to [[Runtime_Host_Contract]].
- in the initial HW6 inactive policy, A/B/L/R are consumed by PeepOS to show a bounded `PRESS START` cue, while `START` activates and joystick movement wake is disarmed; packages cannot restyle or route those physical cue inputs.
- an `interaction_context_ref` may select only entries declared by the referenced package policy; it cannot override timeout, route, or activation semantics.
- every deferral must have a statically provable completion bound or timeout; unbounded deferral is a validation error.
- gameplay inactivity transitions remain ordinary schedules and do not mutate the system interaction-state timer.
- every waiting-visual element is bounded by the selected target profile's phase and combined-step limits.
- all elements in one waiting visual provide a phase index for every combined step; elements do not run independent autonomous clocks.
- same-presentation updates preserve the combined step and deadline, while a different presentation identity starts at its declared settled step.
- tools must preview the preferred result, deterministic target reduction, and held-frame fallback, and must show a target-derived visual budget without exposing transport implementation details.
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
- transition scene
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
  scenes[]
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

### Retained STATE Presentation Boundary

Authoring elements are retained package records, not calls into the firmware
renderer. Peep Studio edits records through the Python service; the compiler
resolves source assets and geometry; host preview and firmware consume the same
compiled meaning.

The initial package-authorable layers are `BACKGROUND`, `SCENE`, and `UI`.
`OVERLAY` is system-owned and must be rejected as a package element layer.

Conceptual retained element classes:

```text
state_render_element:
  element_id
  element_type              # sprite, line, outline_rect, filled_rect, circle, ellipse
  layer                     # background, scene, ui
  visible
  order
  bounds
  asset_ref                 # sprite only
  frame_ref                 # sprite only
  primitive_geometry        # primitive only; bounded integer coordinates
  primitive_ink             # fixed black in the initial executable subset
```

Rules:

- coordinates are panel-native integers; the initial subset rejects elements
  outside the panel instead of relying on runtime clipping.
- primitive rasterization is deterministic and identical in exact preview and
  firmware.
- package elements cannot reference framebuffer addresses, dirty rows, display
  transfers, DMA, or target-specific driver functions.
- initial authored text is rasterized by the compiler into masked 1bpp sprite
  assets through the frozen `peepshow.system.8x8.basic.v1` font; it is not a
  runtime font or mutable-string facility.
- STATE sprite records select compiled frame IDs only. They do not bind general
  frame animations; bounded repeating STATE motion is declared by the waiting
  presentation below.
- STATE actions may later target stable element IDs for bounded `show`, `hide`,
  `move`, `set_frame`, and `set_animation` operations.
- `RND2` is the initial executable retained-presentation record. It carries
  explicit package layer, visibility, z-order, bounds, and one of `sprite`,
  `line`, `outline_rect`, `filled_rect`, `circle`, or `ellipse`.
- `RND1` remains accepted by the package parser and HW6 loader for backward
  compatibility; new builds emit `RND2`.
- initial primitives use fixed black ink. White/clear ink is not exposed yet.
- authored text records compile to ordinary masked 1bpp sprite frames; runtime
  font records are not part of this subset.

The `RND2` checkpoint is hardware-validated. Schema validation, deterministic
compilation, package parsing, exact host preview, HW6 loading, retained
composition, scene replacement, and STOP2 presentation were proven together on
2026-08-27. Static primitives remained composed while both package sprite
animations continued in STOP2.

### Initial Masked-1bpp STATE Subset

The first executable Peep Studio subset accepts PNG sources that compile to
`masked_1bpp` frames. Peep Studio is a provisional working name.

Conceptual source records:

```text
masked_1bpp_asset:
  asset_id
  display_name                 # optional author-facing label, not packaged
  source_path
  source_format: png
  alpha_policy: binary_nonzero_opaque
  frames[]:
    frame_id
    display_name               # optional author-facing label, not packaged
    source_rect
    pivot_x
    pivot_y

system_font_text_asset:
  asset_id
  display_name                 # optional author-facing label, not packaged
  asset_type: masked_1bpp
  source_format: system_font_text
  font_id: peepshow.system.8x8.basic.v1
  text                         # printable ASCII plus newline
  scale                        # integer 1..8
  frames[1]:
    frame_id
    display_name               # optional author-facing label, not packaged
    pivot_x
    pivot_y

frame_animation:
  animation_id
  frame_refs[]
  frame_duration_ms[]
  loop_policy

state_scene_placement:
  render_models[1]            # one scene-owned placed-object surface
    visual_id
    focus_index
    elements[]                # scene-owned placed objects

state:
  state_id
  display_name
  waiting_visual_ref
  placement_overrides[]       # optional per-state changes to scene objects

state_placement_override:
  element_ref                 # must name a scene-owned placed object
  x                           # optional state-specific position
  y                           # optional state-specific position
  visible                     # optional state-specific visibility
  visual_ref                  # optional state-specific sprite frame
```

`frame_animation` is the general asset-timeline record intended for future
SEQUENCE authoring. It is not the STOP2-capable STATE animation contract.

A `STATE_SCENE` has one spatial placement surface. Logic states do not own
independent render models. When a state needs the cursor, marker, or another
object to appear differently, it declares a bounded placement override against
the scene-owned object. Package compilation may flatten those overrides into
target-specific retained records, but authored source must keep the scene object
identity stable.

The initial system-font contract is fixed-cell 8x8, black ink on a transparent
background, printable ASCII `0x20..0x7e`, newline line breaks, and integer
nearest-neighbor scaling. Rasterized output must fit `168x144` and emits exactly
one `masked_1bpp` frame. The frozen glyph table is derived from
`firmware/peepshow_hw4_fw1/Core/Src/font8x8_basic.c`; the authoring backend must
not read that legacy firmware source at build time. A future 16x16 font requires
a new stable `font_id` and explicit provenance rather than changing this ID.

A STATE animated element is authored through the scene `waiting_visual`:

```text
state_waiting_visual:
  waiting_visual_id
  presentation_id
  phase_quantum_ms          # 1..60000
  combined_step_count       # 1..12
  settled_step
  cycle_policy: loop
  elements[]:               # at most 32 declared sprite elements
    element_id
    source_element_ref
    phase_visual_refs[]     # 1..4 compiled frame IDs
    step_phase_indices[]    # one phase index per combined step
```

The waiting timeline is shared by awake waiting presentation and admitted
STOP2 playback. A static sprite may use one phase; static primitives remain in
the retained model but do not receive phase animation in this version.

Rules:

- a source without alpha is fully opaque.
- alpha zero is transparent and any nonzero alpha is opaque.
- source pixels resolve deterministically to logical white or black; no tone
  reduction or dither profile is implied by this subset.
- frame IDs and animation IDs are stable authoring IDs and compile to bounded
  package indexes.
- every frame declares dimensions, source bounds, and a pivot.
- animation durations are positive integer milliseconds and must fit the
  selected target profile.
- opaque frames may compile without a stored mask when the package record marks
  the entire frame owned.
- package output contains no PNG decoder dependency or source path.
- pixel-model and compiler-profile fields remain versioned enums so later fixed
  tone/dither import profiles can be added without changing masked-1bpp meaning.

### Initial Sampled STATE Audio Subset

This subset is the next STATE authoring milestone and is not executable in the
current HW6 package format yet.

Conceptual source records:

```text
sampled_sfx_asset:
  asset_id
  source_path
  source_format: wav
  compiler_profile: hw6_mono_16k_ima_adpcm
  priority
  volume

state_sfx_action:
  operation: play_sfx
  asset_ref
  priority
  volume
```

Rules:

- source WAV files are editor/compiler inputs only.
- compilation deterministically converts accepted input to mono 16 kHz 4-bit
  IMA ADPCM with bounded decoded size and duration metadata.
- STATE permits bounded SFX bursts only; music, loops without a proven bound,
  and sustained-audio grants are not part of this subset.
- the complete SFX payload is admitted and resident before playback; runtime
  playback never reads FileX/FAT or host paths.
- package logic requests a symbolic cue. `thAudio` alone owns decode, PCM
  buffers, SAI, DMA, amplifier state, clock intent, and error recovery.
- Peep Studio may audition the source or compiled result, but audition is not
  HW6 timing, power, or electrical evidence.

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
  scene_flow:
    nodes:
      scene_id:
        x
        y
  state_graph:
    scenes:
      scene_id:
        nodes:
          state_id:
            x
            y
        routes:
          route_id:
            sources:
              state_id:
                routing_version
                target_handle
                target_side
                rails[]:
                  axis
                  value
                token_positions:
                  condition
                  actions[]
  comments[]
  bookmarks[]
  local_ui_state
```

The first executable Peep Studio subset stores scene-flow positions in
`project.editor.scene_flow.nodes[scene_id] = { x, y }` and per-scene STATE graph
positions in
`project.editor.state_graph.scenes[scene_id].nodes[node_id] = { x, y }`. State
IDs are used for state cards; `scene-entry` and
`scene-exit-<scene_exit_id>` identify the semantic endpoint nodes. These
coordinates are for author comprehension only; they do not change scene order,
entry behavior, routes, preview behavior, or compiled package bytes.

Manual STATE-transition routing stores zero to eight alternating horizontal or
vertical rail coordinates per visible route branch in
`project.editor.state_graph.scenes[scene_id].routes[route_id].sources[state_id].rails`.
Each rail has `axis: x|y` and a numeric `value`. Version 3 layouts may also save
one `target_handle` chosen from the four corner entry zones plus one
`target_side`. Each corner exposes two valid directional ports: top and left,
top and right, bottom and left, or bottom and right. The port choice is
presentation-only and does not change the route's semantic target state.

Version 3 route layouts may also store `token_positions`. `condition` is the
optional aggregated guard-chip position, and `actions` contains visible action-
chip positions in semantic execution order. Each value is a normalized path
fraction from `0.02` to `0.98`; present values must be strictly increasing.
Peep Studio prevents adjacent tokens from crossing, so moving a chip never
reorders guards or actions.

Peep Studio derives right-angle intersections and endpoint joins from the saved
rails. Generated corners, hover controls, card-relative endpoints, and arrow
geometry must never be persisted. Moving a card updates only those derived
joins while preserving the author's middle rails. Every straight section,
including the outgoing and incoming sections, is draggable; Peep Studio derives
the required right-angle endpoint joins. Dragging the arrow selects a
destination corner and approach side, and double-clicking a route adds a movable
jog. Clearing the rails, target handle, and target side restores fully automatic
routing. Version 2 waypoint layouts and older
unversioned bring-up layouts remain readable project data but Peep Studio must
ignore them rather than carrying obsolete helper geometry into the rail router.
Route layouts are editor-only and must not alter action order, transition
behavior, preview behavior, or compiled package bytes.

Rules:

- editor-only data must be clearly namespaced.
- editor-only data must not affect package runtime behavior.
- package builds must be deterministic when editor-only data changes.
- editor-only data must not be installed to the device except where a future debug artifact explicitly records it as tooling metadata.
- stale node and route layout records must be removed when their semantic records are deleted.

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
| `SCENE_*` | `SCENE_ENTRY_MISSING`, `SCENE_ID_UNKNOWN`, `SCENE_TYPE_INVALID` |
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

For the initial STATE subset, preview launch accepts a selected `scene_id` plus
an explicit initial state or the scene's declared entry state. This is an
editor-only launch fixture. Preview then consumes compiled package scene,
sprite, animation, and waiting-visual records; it must not draw from source PNG
files or use React-only animation rules. Input and time advance only through
explicit preview operations, and every returned frame is an exact `168 x 144`
1bpp logical framebuffer plus bounded trace data.

---

## Compiler Boundary

The compiler converts authoring source into package/runtime artifacts.

Expected outputs:

- normalized package manifest
- compiled scene table
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
project.create
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
- new projects are created from ordinary editable schema records, not copied
  examples or UI-protected templates.
- new project source references are relative to the `.peepproj` root, and
  creation must reject an existing destination rather than overwrite it.
- validation results use stable codes.
- package builds record selected target profile, tool versions, schema versions, and content hashes.
- the GUI must not bypass validation when exporting installable artifacts.
- the same validation/build path should be usable by CLI and GUI.

---

## Technology Guidance

The schema remains independent of the GUI framework. The accepted V1 process,
technology, preview, and editor boundaries are defined by
[[Authoring_Tool_Architecture]].

The visual editor uses a modern reactive UI, typed source models, a graph view
over node/edge semantic records, and schema-driven property inspectors. The UI
is a client of the headless toolchain; it is not a second compiler.

The authoring preview is isolated from compiler implementation state, but it
must consume normalized or temporarily compiled semantics produced through the
same authoritative Python toolchain used for `.egg` export.

Python remains appropriate for:

- asset conversion
- validators
- package compilation
- orchestration
- command-line tooling

Python GUI frameworks should not be assumed as the primary editor direction.

---

## Validation Cases

1. project with valid metadata, one state scene, one entry graph, and complete reactive-wait contracts validates.
2. graph with missing entry state fails validation.
3. transition to unknown state fails validation.
4. transition to undeclared scene fails validation.
5. editor-only node layout change does not change package output checksum.
6. source asset path appears in authoring data but not in compiled runtime package data.
7. placeholder sprite validates in authoring preview where allowed and fails shipping export where disallowed.
8. waiver for placeholder art appears in compatibility report.
9. waiver for unbounded action loop is rejected.
10. sequence or program scene in a `timeout` package without meaningful-activity rules and an inactive route fails validation.
11. package requiring a capability still pending on HW6 fails shipping export.
12. authoring preview mock sensor data does not count as hardware evidence.
13. generated package build includes schema versions, tool versions, target profile, and compatibility report.
14. package interaction mode other than `continuous` or `timeout` fails validation.
15. `timeout` interaction policy without an admitted inactive route fails validation.
16. unbounded inactivity deferral fails validation.
17. the target-owned activation gesture is represented as a consumed system action plus symbolic lifecycle event, not a package action.
18. package-authored system lock timeout fails validation.
19. `continuous` interaction policy with inactive-route or inactivity-deferral fields fails validation.

---

## Rule

The authoring project schema is editable source.

Compiled packages are deterministic runtime data.

Editor convenience must never become hidden firmware behavior.
