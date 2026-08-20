# Package Contract

This document defines the package-facing contract independent of hardware implementation details.

Related:

- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Target_Profile_Schema_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[Package_Save_Settings_API_Contract]]
- [[Runtime_Host_Contract]]
- [[Runtime_Logic_State_API_Contract]]
- [[Scene_Runtime_and_Interaction_Model]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Audio_API_Contract]]
- [[Communication_API_Contract]]
- [[Sensor_API_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Diagnostics_API_Contract]]

---

## Package Model

Packages provide:
- metadata
- assets
- package scenes and declared transitions
- state graphs/tables
- variables and configuration
- optional host-allowed scripted logic

Packages do not provide:
- direct peripheral control
- thread creation
- power mode transitions

---

## Authoring Source Versus Package Output

Templates, Authoring Kits, prefabs, behavior graphs, and behavior macros are authoring-source concepts defined by [[Game_Authoring_API_Contract]] and [[Authoring_Project_Schema_Contract]].

Compiled packages do not install those source objects as independent firmware components.

The package compiler lowers authoring reuse into:

- manifest data
- package scenes
- runtime logic references
- state/action/guard tables
- scene data
- asset tables
- content parameter schemas and resolved values
- save/settings schemas
- diagnostics metadata
- compatibility report metadata

Rules:

- `Authoring Kit` is the canonical name for reusable gameplay systems.
- do not use `module` as the general name for gameplay authoring reuse.
- `module` is reserved for actual hardware or firmware modules and is not a package scene type.
- an Authoring Kit may generate or reference one or more scenes, but each generated scene declares exactly one canonical scene type.

---

## Manifest Requirements

Every package must declare:
- `package_id`
- `name`
- `version`
- `build_profile`
- `target_profile`
- `entry_scene`
- `scenes`
- `required_capabilities`
- `optional_capabilities`
- `wake_intents`
- `cadence_hints`
- `latency_tolerance`
- `asset_table`
- `audio_profile`
- `sensor_profile`
- `communication_profile`
- `time_power_profile`
- `diagnostics_profile`
- `save_schema_version`
- `storage_write_budget`
- `compatibility_constraints`

Capability names are defined in [[PeepOS_Capability_Registry]].

---

## Manifest Schema Outline

Final serialized schemas live in the package tooling schema files. This outline defines the required conceptual fields.

The installable package container is defined by [[Package_Blob_Format_Contract]].

```text
package_manifest:
  package_id
  name
  publisher_id
  package_version
  package_format_version
  build_profile
  target_profile
  entry_scene
  scenes[]
  required_capabilities[]
  optional_capabilities[]
  wake_intents[]
  cadence_hints
  latency_tolerance
  power_policy
  asset_table_ref
  audio_profile_ref
  sensor_profile_ref
  communication_profile_ref
  time_power_profile_ref
  diagnostics_profile_ref
  save_schema_ref
  message_schema_ref
  package_blob_ref
  compatibility_constraints
  package_checksum
```

Rules:

- `build_profile` must be one of the profiles defined in [[Game_Authoring_API_Contract]].
- `target_profile` must name a profile from [[PeepOS_Capability_Registry]].
- `entry_scene` must resolve to one entry in `scenes`.
- each scene declares one `scene_type`: `STATE_SCENE`, `SEQUENCE_SCENE`, or `PROGRAM_SCENE`.
- required and optional capability declarations are compiler-derived from scene content, service use, and fallback structure; the resolved declarations are serialized in the manifest.
- required capabilities must be granted by the target profile.
- optional capabilities must include fallback behavior.
- `package_blob_ref` must resolve to the installable `PeepPkg` container metadata.

---

## Scene Schema Outline

A package may contain multiple scenes.

The package is the installable artifact. A scene is a bounded authored experience inside that package. Scene transitions do not remount the package.

Examples:

```text
ambient_pet      STATE_SCENE
dialogue_flow    STATE_SCENE
intro_sequence   SEQUENCE_SCENE
microgame        PROGRAM_SCENE
```

Conceptual schema:

```text
scene:
  scene_id
  scene_type
  entry_ref
  logic_refs[]
  object_refs[]
  presentation_ref
  interaction_policy_ref
  required_capabilities[]
  optional_capabilities[]
  asset_refs[]
  audio_context_refs[]
  sensor_context_refs[]
  communication_context_refs[]
  save_refs[]
  cadence_hints
  wake_intents[]
  latency_tolerance
  reactive_wait_policy_ref       # STATE_SCENE only
  realtime_policy_ref            # SEQUENCE_SCENE or PROGRAM_SCENE
  allowed_transitions[]
  scene_stack_policy
  suspend_resume_policy
  failure_fallback
```

Rules:

- `scene_id` must be unique within the package.
- `scene_type` must be one of `STATE_SCENE`, `SEQUENCE_SCENE`, or `PROGRAM_SCENE`.
- `entry_ref` and `logic_refs` must resolve to data accepted for that scene type by [[Runtime_Logic_State_API_Contract]].
- `allowed_transitions` must target valid scenes in the same package or an approved system route such as shell return.
- scene transitions must use declared transition edges.
- arbitrary jumps to undeclared scene IDs are invalid.
- compiler-derived scene capabilities must be granted by the selected target profile; optional capabilities must include fallback behavior.
- assets, audio contexts, sensor contexts, communication contexts, and save records used by the scene must be declared.
- lifecycle handlers and scene transition actions must be bounded.

Scene type requirements:

| Scene Type | Execution | Required Scene Fields |
|---|---|---|
| `STATE_SCENE` | `REACTIVE` | state/behavior entry, bounded action tables, settled presentation, reactive wait contracts, schedule/wake intent |
| `SEQUENCE_SCENE` | `REALTIME` | bounded timeline/tracks, target FPS, duration or end marker, meaningful-activity rules, suspend/resume behavior, inactive and scene-end routes |
| `PROGRAM_SCENE` | `REALTIME` | sandbox program entry, instruction/memory/frame budgets, meaningful-activity rules, suspend/resume behavior, inactive and failure routes |

Every `SEQUENCE_SCENE` and `PROGRAM_SCENE` must declare an inactivity route to a `STATE_SCENE` or shell. A sequence also declares a scene-end route. A realtime scene may remain active while meaningful user activity or Platform-approved bounded work continues, but it does not select a CPU frequency or clock source. Platform chooses the lowest validated operating point that meets its admitted deadlines.

An interactive communication context may request only the bounded peer-wait grace defined by [[Communication_API_Contract]] and the selected target profile. That policy is bounded where admitted; it is not a package-owned inactivity override, unbounded deferral, or stay-awake grant.

---

## Scene Transition Model

Scene transitions are declared, bounded, and Engine-managed. A transition is a graph/action construct, not a scene.

Allowed transition forms:

```text
transition_to(scene_id)
push_scene(scene_id)
pop_scene()
exit_to_shell(reason)
```

Rules:

- `transition_to` replaces the current scene with a declared target.
- `push_scene` enters a declared target while preserving a bounded return path.
- `pop_scene` returns to the previous scene if the stack is non-empty.
- stack depth is bounded by target profile and package validation.
- recursive push loops are invalid unless statically bounded and approved by validation.
- transition guards and actions must be bounded.
- transition targets must be declared in `allowed_transitions`.
- `exit_to_shell` is an approved system route, not a package-defined shell implementation.

Typical use:

```text
map -> push dialogue -> pop map
ambient state -> play sequence -> return to state
ambient state -> run program scene -> return to state
```

The package remains mounted across these transitions.

---

## Content Parameters, Package Settings, And Capability Contexts

Package settings and Platform settings are separate.

Detailed package save/settings API rules are defined in [[Package_Save_Settings_API_Contract]].

Detailed content parameter schema rules are defined in [[Content_Parameter_Schema_Contract]].

Content parameters:

- are package-authored balancing or behavior values
- are declared by package schemas or package source data
- may be edited by normal package-authoring tools
- compile into package data or package-owned settings
- are not Platform knobs and do not live in `config/knobs.json`

Package settings:

- are declared by package schema
- have defaults
- may be rendered or edited through PeepOS UI
- are stored through package save/settings APIs
- may include package preferences such as difficulty, text speed, package-local sound preference, or package-local input preferences

Platform settings:

- are owned by PeepOS
- are not mutated directly by packages
- include hardware, sleep, storage, power, sensor, communication, and system policy

Packages may declare temporary capability contexts.

Examples:

```text
sensor_context.motion_stream_20hz_for_microgame
sensor_context.step_session_for_walk_goal
sensor_context.light_band_for_scene_logic
communication_context.multiplayer_session
audio_context.active_music_or_sfx
runtime_context.realtime_activity_for_minigame
```

Rules:

- contexts are declarations, not settings writes.
- tool-side validation must prove each context is valid for the selected target profile and scene.
- Platform may internally clamp, coalesce, substitute, or degrade hardware behavior while preserving the package-facing contract.
- package gameplay code does not handle hardware-level grant/reject/revoke paths for primitives required from the selected target profile.
- if a required context cannot be maintained at runtime, Platform/Engine handles fault logging and lifecycle policy.
- contexts must have bounded duration, declared scene scope, or explicit release behavior.
- packages must not directly write Platform knobs, Platform settings, hardware registers, or storage policy.

---

## Reactive, Presentation, And Interaction Policy Schema

Packages express execution and gameplay intent only. Detailed behavior is defined in [[Time_And_Power_Intent_API_Contract]].

```text
time_power_profile:
  calendar_requirements
  schedule_table_ref
  lifecycle_policy
  wake_intents[]
  catch_up_policy

reactive_policy:
  state_wait_table_ref
  reactive_input_latency_class
  reactive_schedule_cadence
  waiting_visual_table_ref
  waiting_visual_fallback_table_ref
  wake_intents[]

interaction_policy:
  meaningful_activity_sources[]
  inactive_route             # preserve_scene, transition_to_scene, exit_to_shell
  inactive_target_scene
  inactive_waiting_visual_ref
  bounded_deferral_table_ref

realtime_policy:
  target_fps
  frame_budget
  meaningful_activity_sources[]
  suspend_behavior
  resume_behavior
  inactive_route
  bounded_inactivity_deferral_table_ref
```

A state wait entry resolves:

```text
reactive_wait:
  state_id
  waiting_visual_ref
  reduced_waiting_visual_ref
  hold_fallback_allowed
  event_interests[]
  schedule_refs[]
  gameplay_timeout_transitions[]
  wake_intents[]
  interaction_context_ref
```

Rules:

- a reactive runtime yields after each bounded event transaction settles
- every state that can wait must resolve a reactive wait entry
- waiting visual assets describe intended cosmetic presentation; they do not select LPBAM, DMA, STOP, or periodic CPU wake behavior
- the Platform may compile the preferred waiting visual autonomously, use a reduced visual, hold the settled frame, or use another target-profile backend
- autonomous visual playback cannot mutate package state
- package gameplay inactivity is represented by a normal schedule and transition
- PeepOS always owns inactivity detection; packages cannot disable it or author its timeout
- a reactive wait may select an `interaction_context_ref` only from the package policy's declared meaningful-activity and bounded-deferral entries; it cannot override timeout, route, or activation semantics
- inactive routes are limited to preserving the current scene, transitioning to a declared scene, or exiting to shell
- the target-owned activation gesture is consumed by PeepOS and is not delivered as a package action
- activation does not select another package route: the scene established by the inactive route remains authoritative
- bounded inactivity deferral must name a statically bounded completion/timeout; unbounded deferral is invalid
- every `SEQUENCE_SCENE` and `PROGRAM_SCENE` requires an inactivity route to a `STATE_SCENE` or shell
- packages must not implement polling loops to approximate reactive cadence or display animation
- missed scheduled events use a bounded catch-up policy

Profile behavior:

| Target Profile | Package Implication |
|---|---|
| `HW6_VALIDATED_BASELINE` | reactive waits may hold or use admitted wake/update/return behavior |
| `HW6_VALIDATED_LPBAM` | eligible waiting visuals may compile to autonomous playback within measured budgets |

The package artifact remains portable. It must not contain hardware row addresses, SPI payloads, SRAM4 addresses, DMA descriptors, LPBAM nodes, or wake-pin configuration.

---

## Asset Table Schema Outline

Assets are referenced by ID, not filesystem path.

Asset records resolve to package chunks in [[Package_Blob_Format_Contract]].

```text
asset_table:
  assets[]:
    asset_id
    asset_type
    format_version
    byte_size
    bounds
    chunk_id
    checksum
    scene_type_limits
    required_capability
```

Allowed asset classes include:

- masked 1bpp sprites/images
- tone5 masked sprites/images
- tilesets
- tilemaps
- animation tables
- fonts
- audio/music/SFX
- BBB patterns
- text/localization tables
- state graph tables
- data tables
- waiting-visual sequences

Rules:

- asset IDs must be stable within the package.
- all asset references must resolve at validation time.
- external editor files are import sources only and are not runtime assets unless compiled into PeepOS package formats.
- package runtime must not use arbitrary host or FAT paths.
- tone5 assets are semantic coverage assets and must not be described as a color-depth format.
- waiting-visual sequences are portable package assets; continued motion while reactive logic is yielded requires the target profile to grant `display.waiting_visual_animation`.
- runtime assets are loaded by package/asset APIs from installed raw package storage or bounded caches, not by filesystem path.
- package chunks must not contain SRAM4 addresses, SPI payloads, DMA descriptors, LPBAM descriptors, or hardware row formats.

---

## State Graph Schema Outline

State graphs are bounded runtime logic data.

Detailed runtime logic behavior is defined in [[Runtime_Logic_State_API_Contract]].

```text
state_graph:
  graph_id
  entry_node
  nodes[]
  transitions[]
  event_bindings[]
  timers[]
  local_variables[]
  action_tables[]
  persistence_policy
  bounds
```

Rules:

- `entry_node` is required.
- every transition target must exist.
- action table length is bounded.
- expression/instruction cost is bounded.
- timers declare timebase and maximum duration.
- graph-local variables declare type, size, reset behavior, and persistence behavior.
- event bindings must use package-visible Engine event classes.
- action tables must use symbolic Engine requests and must be non-blocking.
- graph validation failures block package compilation/export.

---

## Input Map Schema Outline

Input maps bind logical input to package actions.

Detailed input/focus API behavior is defined in [[Input_Focus_API_Contract]].

```text
input_map:
  focus_scopes[]
  actions[]
  bindings[]
  repeat_policy
  chord_policy
  joystick_policy
  encoder_policy
  wake_intents[]
  fallback_bindings[]
```

Rules:

- bindings use logical PeepOS input concepts only.
- Platform-reserved inputs may be rejected or overridden by shell/system policy.
- `BTN_BOOT` and Start shipping intent are not package inputs.
- bindings must target package-local actions.
- focus scope stack depth must be bounded.
- optional input capabilities require fallback bindings or fallback behavior.
- scene transitions must release or transfer input focus explicitly.

---

## Audio Profile Schema Outline

Audio profiles declare symbolic package audio behavior.

Detailed audio API behavior is defined in [[Audio_API_Contract]].

```text
audio_profile:
  cues[]:
    cue_id
    cue_type
    asset_ref
    bus
    group
    priority
    default_volume
    loop_policy
    fade_policy
    ducking_policy
    max_duration_ms
    preload_policy
  bbb_patterns[]:
    pattern_id
    steps[]
    priority
    max_duration_ms
  audio_contexts[]:
    context_id
    scene_refs[]
    active_cue_refs[]
    bbb_pattern_refs[]
    volume_defaults
    preload_refs[]
    power_behavior_hint
    diagnostic_label
```

Rules:

- audio cue IDs are symbolic package-local IDs.
- audio profiles must not name SAI, DMA, LPTIM, GPIO, `SD_MODE`, amplifier state, mixer buffers, decoder internals, or filesystem paths.
- sampled audio assets and BBB patterns must resolve to package assets.
- BBB pattern frequency, duration, step count, repeat count, curve, and envelope must be bounded.
- scene audio contexts must be valid for the selected scene type and target profile.
- PeepOS does not require packages to remain semantically complete when muted.
- audio-centric package behavior is valid when it remains within bounded package/runtime rules.

---

## Sensor Profile Schema Outline

Sensor profiles declare package use of PeepOS sensor primitives.

Detailed sensor API behavior is defined in [[Sensor_API_Contract]].

```text
sensor_profile:
  contexts[]:
    context_id
    scene_refs[]
    required_capabilities[]
    optional_capabilities[]
    mode
    cadence_hint
    max_duration_ms
    event_interests[]
    wake_intents[]
    fallback_policy
    diagnostic_label
```

Rules:

- sensor contexts use PeepOS capability names only.
- sensor contexts must not name hardware parts, pins, ADC channels, I2C addresses, EXTI lines, registers, or HAL handles.
- each sensor context must be referenced by at least one scene.
- high-rate motion or light streaming must be bounded and valid for the scene type.
- step sessions use package baselines and must not reset the hardware step counter.
- optional sensor features require declared content fallback behavior.
- required sensor primitive failure at runtime is handled by Platform/Engine lifecycle and diagnostics, not normal gameplay logic.

---

## Save Schema Outline

Save writes require a schema.

Detailed save/settings API behavior is defined in [[Package_Save_Settings_API_Contract]].

```text
save_schema:
  save_schema_id
  save_schema_version
  records[]:
    record_id
    record_type
    fields[]
    max_size_bytes
    default_value
    migration_policy
    reset_policy
    write_policy
    durability_class
  package_settings[]:
    setting_id
    value_type
    default_value
    allowed_values
    ui_metadata
    storage_record_ref
    migration_policy
    reset_policy
  write_budget
  reset_policy
```

Rules:

- all save writes must target a declared record.
- schema changes require versioning.
- migrations must be explicit.
- write frequency assumptions must be declared.
- failed writes should preserve the previous valid record where possible.
- package settings are package-owned schema records, not Platform settings.
- package writes use Engine save/settings APIs only.

---

## Message Schema Outline

Communication messages are bounded and versioned.

Detailed communication API behavior is defined in [[Communication_API_Contract]].

```text
communication_profile:
  contexts[]:
    context_id
    scene_refs[]
    mode
    role_intent
    session_type
    max_peers
    message_schema_ref
    rate_limits
    timeout_policy
    interactive_wait_policy
    ordering_policy
    session_end_route
    fallback_route
    diagnostic_label
```

```text
message_schema:
  schema_id
  schema_version
  max_message_bytes
  message_types[]
  rate_limits
  session_behavior
```

Rules:

- communication profiles must not name BLE, NINA, UART, GAP, GATT, module commands, pins, or bonding storage.
- messages must fit communication capability limits.
- message schemas must be versioned and bounded.
- each communication context must declare `none`, `optional`, or `session_required` behavior.
- optional communication contexts require fallback/route behavior.
- session-required scenes require admission/session routes when no session exists.
- HW6 profiles must reject communication wake behavior until measured evidence and the capability registry grant it.
- peer disconnects and session timeouts are package-visible events.
- hardware/module faults are Platform/Engine diagnostics, not normal gameplay branches.

---

## Diagnostics Profile Schema Outline

Diagnostics profiles declare bounded package observability.

Detailed diagnostics API behavior is defined in [[Diagnostics_API_Contract]].

```text
diagnostics_profile:
  marker_table[]
  counter_table[]
  timing_scope_table[]
  trace_value_table[]
  warning_code_table[]
  package_fault_code_table[]
  profile_gates
  rate_limits
  export_policy
```

Rules:

- package diagnostics explain package behavior, not Platform hardware behavior.
- diagnostic IDs must be package-local and stable.
- diagnostic payload types must be declared and bounded.
- shipping diagnostics must be explicitly marked as shipping-allowed.
- package fault codes must map to Engine lifecycle policy.
- diagnostics profiles must not name SWD, SWO, UART, USB, BLE, protected storage, hardware registers, RTOS objects, or filesystem paths.
- Platform owns debug transport, persistent fault logs, and diagnostic export.

---

## Intent-Driven Policy

Packages may declare intent such as:
- "wake on button and step"
- "no periodic tick needed"
- "update every 1000 ms"
- "requires short audio cues only"

Platform decides exact hardware behavior.

---

## Storage and Save Rules

- Package saves and package-owned settings go through [[Package_Save_Settings_API_Contract]] only.
- Save schema version must support migration handlers.
- Package writes must be bounded and power-safe.
- Package data cannot bypass installer validation path.

---

## Versioning and Compatibility

Use semantic versioning for package-facing schemas and package container compatibility:
- `pkg_format_major`
- `pkg_format_minor`

Rules:
- Major mismatch: reject install.
- Minor mismatch: allow if backward-compatible.
- Validation output must include exact rejection reason.
- The `PeepPkg` container format and individual chunk schemas carry explicit versions.

---

## Validation Checklist

Before package compilation/export, tooling must validate:

1. manifest schema
2. scene type compatibility
3. required and optional capability declarations
4. reactive wait contracts and realtime fallback declarations
5. interaction policy, inactive route, meaningful activity, and bounded deferrals
6. waiting-visual fallback and target compiler admission
7. asset table bounds
8. save schema declaration
9. authoring validation in PeepOS terms
10. internal forbidden hardware, RTOS, filesystem, and Platform-internal API access

At install time, firmware must validate:

1. validate `PeepPkg` header, chunk table, ranges, and integrity metadata
2. validate manifest schema and signatures/checksums
3. validate scene type compatibility
4. validate required capability compatibility against the active target profile
5. validate reactive-wait and interaction-policy records are structurally valid and bounded
6. validate asset table bounds
7. validate save schema declaration
8. stage package before commit

---

## Minimum Package API Surface

Expose package-safe APIs only:
- metadata query
- asset metadata query by ID
- asset open/close by ID
- typed asset views/handles through [[Package_Asset_Loading_API_Contract]]
- bounded asset read windows for approved large assets
- save/settings read/write by declared schema through [[Package_Save_Settings_API_Contract]]
- capability query
- host event submission

No HAL or RTOS internals are exposed to packages.

Packages must not receive raw chunk offsets, storage addresses, filesystem paths, SRAM4 addresses, DMA descriptors, LPBAM descriptors, or display payload pointers.
