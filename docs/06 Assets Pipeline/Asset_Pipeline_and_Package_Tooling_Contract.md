# Asset Pipeline and Package Tooling Contract

This document defines how external tools produce package content for runtime hosts.

Related:

- [[Game_Authoring_API_Contract]]
- [[Authoring_Tool_Architecture]]
- [[Authoring_Project_Schema_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[Package_Contract]]
- [[Package_Blob_Format_Contract]]
- [[Package_Compatibility_Report_Contract]]
- [[Package_Save_Settings_API_Contract]]
- [[Dev_Orchestration_CLI_Contract]]

---

## Goal

Keep tooling output stable and host-oriented so tools never depend on RTOS or hardware internals.

Tool-side validation is a required pre-compilation gate. Install-time firmware validation remains mandatory, but it is not a substitute for validating content before package compilation or export.

Normal game-authoring validation must use PeepOS concepts. Low-level forbidden-token checks are internal verifier guardrails for toolchain defects, corrupted artifacts, malicious packages, or future advanced tooling.

Project-level command orchestration is defined in [[Dev_Orchestration_CLI_Contract]]. CLI package commands must call this pipeline and must not bypass validation.

Editable source projects are defined in [[Authoring_Project_Schema_Contract]]. This pipeline consumes those authoring sources and emits deterministic package/runtime artifacts.

---

## Package Build Inputs

Tooling inputs may include:
- package manifest JSON
- template metadata and imported template fragments
- Authoring Kit metadata and kit instances
- prefab definitions
- behavior graphs and behavior macros
- state graph JSON/GraphML
- runtime logic graph/action/scene definitions
- time/power profile and schedule definitions
- input map and focus scope definitions
- audio profile/cue/context definitions
- sensor profile/context definitions
- communication profile/message schema definitions
- diagnostics profile definitions
- content parameter schemas and values
- asset metadata tables
- image/audio source assets
- Aseprite/PNG sprite and tile sources
- Tiled map sources
- optional script/config data allowed by host contract

---

## Package Build Outputs

Tooling outputs must include:
- normalized manifest
- packaged asset blobs/chunks
- `PeepPkg` package blob following [[Package_Blob_Format_Contract]]
- integrity metadata
- version identifiers

Output format must be deterministic from identical inputs.

The package blob is the installable artifact. Editor-native source files are never runtime assets.

### Runtime Package Source Boundary

Firmware package decoding accepts an immutable `(blob, size, source,
generation)` view. The decoder must remain independent of filesystem paths,
installed-flash offsets, and the embedded development symbol.

The current HW6 vertical slice provides:

- `EMBEDDED`, backed by the generated development `.egg` artifact.
- `STAGED_RAM`, backed by one complete bounded `.egg` copied from the reclaimed
  FileX staging volume after every storage handle is closed.
- `INSTALLED_RAM`, backed by the selected persistent A/B package generation
  copied from raw package storage by `thStorage` into the current bounded
  runtime cache.
- `NONE`, which cleanly returns to the shell and displays `EGGLESS`.

Installed-package publication uses the same immutable view only after
`thStorage` has selected a committed index generation, copied the package, and
parked storage. Active STATE, SEQUENCE, and PROGRAM execution must never open
or stream from the FAT staging filesystem. Full pre-commit semantic validation
and bounded installed-asset reads beyond the whole-package cache remain
production work.

Templates, Authoring Kits, prefabs, behavior graphs, and behavior macros are source-level authoring objects. Tooling must lower them into the package output forms defined by this contract. They must not appear as independent firmware components, direct Platform hooks, or new scene types.

---

## Authoring Reuse Compilation

Authoring reuse objects compile as follows:

| Source Object | Compiled Output |
|---|---|
| `Template` | package/project structure, imported kits/prefabs/graphs/assets/schemas, content parameter defaults, validation metadata |
| `Authoring Kit` | scene declarations where needed, graph/action/guard tables, presentation data, asset refs, content parameter schema entries, save/settings schema entries, compiler-derived capability metadata, diagnostics metadata |
| `Prefab` | actor/entity data, asset refs, local parameter defaults, behavior graph refs, scene bindings |
| `Behavior Graph` | bounded runtime logic tables accepted by [[Runtime_Logic_State_API_Contract]] |
| `Behavior Macro` | expanded or referenced bounded graph/action fragments with resolved slots and static bounds |

Rules:

- `Authoring Kit` is the canonical name for reusable gameplay systems.
- do not use `module` as the general name for gameplay authoring reuse.
- an Authoring Kit may emit or reference one or more package scenes.
- generated scenes must declare `STATE_SCENE`, `SEQUENCE_SCENE`, or `PROGRAM_SCENE` and pass normal scene-type validation.
- generated capability declarations must be visible in the compatibility report.
- template/kit/prefab source IDs and versions should be retained as non-authoritative provenance metadata for diagnostics, compatibility reports, and deterministic rebuilds.
- editable template slots must resolve before dev or shipping package export unless the selected preview profile explicitly allows placeholders.
- behavior macro expansion must not hide unbounded loops, service use absent from capability derivation, undeclared save fields, or scene transitions.

---

## Schema Governance

- all package schemas are versioned
- the package container format is versioned separately from chunk schemas
- breaking schema changes require major version increment
- tooling must validate schema before package compilation or export
- firmware must reject incompatible schema versions cleanly

---

## Scene Compatibility

Packages must declare one or more scenes and one entry scene.

Each scene must declare one scene type.

Tooling must validate declared scenes against available host capabilities:
- `STATE_SCENE`
- `SEQUENCE_SCENE`
- `PROGRAM_SCENE`

Capability names and target profiles are defined in [[PeepOS_Capability_Registry]].

Tooling must validate package output against the selected target profile.

---

## Runtime Logic Pipeline

Runtime logic output must target [[Runtime_Logic_State_API_Contract]], not RTOS, Platform, or hardware internals.

Required package-facing runtime logic artifacts:

| Artifact | Purpose |
|---|---|
| `scene_table` | scenes, entries, declared transitions, budgets, and lifecycle policy |
| `state_graph_table` | state/substate nodes, edges, timers, and event bindings |
| `logic_action_table` | bounded symbolic Engine requests |
| `logic_guard_expression_table` | bounded transition/action guard expressions |
| `logic_variable_table` | `system.*`, `game.*`, `scene.*`, and `entity.*` typed variable/property records and stable IDs |
| `world_table` | bounded world descriptors, map/collision references, camera policy, turn controllers, and budgets |
| `entity_definition_table` | reusable immutable entity defaults, visual/property schemas, tags, collision, inventory, and behavior references |
| `entity_instance_table` | stable initial instances, world positions, and bounded property overrides |
| `world_behavior_table` | bounded reusable entity behaviors and deterministic collection/turn iteration records |
| `scene_result_schema_table` | fixed-schema bounded results returned from pushed sequence/program scenes |
| `sequence_scene_table` | bounded data-driven tracks, FPS, markers, end route, inactive route, and suspend/resume policy |
| `program_scene_table` | sandbox program reference, instruction/memory/frame budgets, inactive/failure routes, and suspend/resume policy |

Rules:

- package authors express events, states, guards, actions, variables, and frame budgets.
- authored hierarchy, visual scripting, dialogue trees, or scene timelines must compile to bounded runtime logic tables.
- sequence/program output must declare frame budget, input focus, asset preparation, meaningful-activity rules, suspend/resume behavior, and inactive routing to a state scene or shell.
- reactive graph logic must not use polling loops to approximate realtime behavior.
- action tables must use symbolic Engine APIs and must be non-blocking.
- event queues, timers, variable storage, action cost, expression cost, and transition stack depth must be bounded.
- world maps, entities, behaviors, turn phases, collection queries, pathfinding,
  mutation journals, projection output, and scene-result payloads must fit the
  selected target profile before export.

Tooling must reject runtime logic that references RTOS objects, threads, interrupts, hardware timers, HAL/LL APIs, Platform internals, filesystem paths, raw pointers, function pointers, dynamic code loading, or unbounded loops.

---

## Input Map And Focus Pipeline

Input map output must target [[Input_Focus_API_Contract]], not Platform button, encoder, joystick, EXTI, or GPIO internals.

Required package-facing input artifacts:

| Artifact | Purpose |
|---|---|
| `action_table` | symbolic package action identifiers |
| `focus_scope_table` | bounded focus scopes, modal behavior, fallbacks, and allowed actions |
| `binding_table` | logical source-to-action bindings |
| `repeat_hold_policy` | bounded hold/repeat behavior where allowed |
| `joystick_policy` | normalized vector/direction use, deadzones, scaling, and fallback behavior |
| `encoder_policy` | logical delta mapping and acceleration/fallback behavior |
| `wake_intent_table` | package requests for approved low-power wake input classes |

Rules:

- package authors bind logical sources to package actions, not hardware pins.
- `BTN_BOOT` is never a valid package input.
- Start shipping and power-intent events are not package actions.
- every focus scope must have bounded stack behavior and a declared fallback or close path.
- optional inputs must have runtime-safe fallback behavior when the selected target profile does not provide them.
- wake input intent must be declared as capability intent; Platform decides whether and how the source is armed.
- host keyboard, mouse, and gamepad bindings in the digital twin are adapters into this contract, not runtime package sources.

Tooling must reject input maps that reference GPIO, EXTI, timer counters, I2C/register values, raw joystick magnetic readings, debounce internals, wake-pin configuration, or Platform maintenance actions.

### Volatile Device Load

The HW6 development bridge accepts the same complete `.egg` bytes produced by
the authoritative compiler. It is a transport convenience, not a second
package format or a persistent installer.

- the generated `.egg` must fit the fixed `65536`-byte staged-RAM capacity;
- `thStorage` copies it from FileX and closes storage before publication;
- runtime validates and consumes the immutable RAM bytes through the normal
  package-source interface;
- tools must not depend on FAT paths, staging filenames, RAM addresses, or the
  presence of this development bridge in shipping package semantics;
- exceeding the bridge capacity is a deterministic load error, not a request
  for streaming or dynamic allocation.

### Persistent Device Install

HW6 also accepts that same deterministic `.egg` through the MSC staging path
and atomically replaces the inactive installed-package slot and index record.
The index commit marker is programmed last, the previous generation remains
available until the new record is valid, and the selected generation can be
loaded and launched through `INSTALLED_RAM` without runtime FileX access.

This does not remove the current `65536`-byte runtime-cache ceiling. The raw
slots are `5 MiB` each; larger production packages require the bounded asset
handle and storage-owner read path defined by [[Package_Asset_Loading_API_Contract]].
Production install must also move complete SHA-256/container/chunk/scene
validation ahead of the commit marker.

---

## Sensor Profile Pipeline

Sensor profile output must target [[Sensor_API_Contract]], not Platform sensor drivers.

Required package-facing sensor artifacts:

| Artifact | Purpose |
|---|---|
| `sensor_context_table` | scene sensor contexts and bounds |
| `sensor_capability_refs` | PeepOS sensor capabilities used by each context |
| `event_interest_table` | motion, tap, shake, tilt, orientation, light-band, or step events |
| `step_session_table` | package step baselines and counters |
| `sensor_wake_intents` | declared low-power sensor wake intent where supported |
| `sensor_diagnostic_labels` | package-facing labels for developer traces |

Rules:

- package authors use PeepOS sensor primitives, not hardware sensors.
- target-profile validation must reject invalid mode/cadence combinations before export.
- high-rate sensor contexts must be bounded and tied to declared scenes.
- step sessions must use package baselines and must not reset the hardware step counter.
- optional sensor features must declare content fallback behavior.
- required sensor primitive failure at runtime is handled by Platform/Engine lifecycle and diagnostics, not game logic.

Tooling must reject sensor profiles that reference ADC, GPIO, EXTI, I2C addresses, hardware part numbers, registers, HAL handles, calibration storage, sensor power state, or wake-pin configuration.

---

## Audio Profile Pipeline

Audio profile output must target [[Audio_API_Contract]], not Platform audio drivers.

Current HW6 package status: audio records are contractual but not executable.
The firmware currently has a generated diagnostic tone only. Peep Studio must
not report sampled package audio as supported until the audio chunks, loader,
STATE action routing, `thAudio` decode/playback path, and HW6 proof all exist.

### Initial STATE SFX Asset Slice

Before SEQUENCE or PROGRAM audio, the toolchain must implement one bounded HW6
STATE SFX path:

1. import WAV as source-only authoring data;
2. deterministically convert to mono 16 kHz 4-bit IMA ADPCM;
3. emit stable symbolic asset/cue IDs, compressed bytes, sample count, duration,
   decoded-size bound, and integrity metadata;
4. reject unsupported formats, excessive duration or memory, looping, music,
   streaming, and `audio.bbb` for HW6;
5. let STATE actions request the symbolic SFX without exposing hardware;
6. support host audition and compatibility reporting while clearly separating
   them from HW6 power and timing evidence.

Required package-facing audio artifacts:

| Artifact | Purpose |
|---|---|
| `audio_cue_table` | symbolic music/SFX/BBB cue IDs, groups, priorities, loops, fades, and defaults |
| `audio_context_table` | scene audio contexts and preload requirements |
| `audio_asset_table` | validated sampled audio references and decode budgets |
| `bbb_pattern_table` | bounded BBB tone/gap/sweep/repeat steps |
| `bbb_melody_sources` | RTTTL/Nokia-style melody authoring sources compiled into BBB patterns |
| `audio_timeline_table` | optional symbolic markers for replay, diagnostics, or package logic |

Rules:

- package authors use symbolic audio cues, not hardware output paths.
- PeepOS does not require packages to remain semantically complete when muted.
- audio-centric packages are valid when their assets, contexts, and runtime behavior are bounded.
- sampled audio assets must match the accepted target profile format.
- RTTTL is the v1 BBB melody authoring format and must be compiled by tooling before package export.
- runtime packages must not require firmware to parse RTTTL or any other melody source text.
- BBB patterns must validate duration, frequency, envelope, curve, step count, and repeat bounds.
- compiled BBB melodies must validate the same frequency, duration, envelope, curve, step count, repeat, priority, and total-duration bounds as hand-authored BBB patterns.
- cue priorities, groups, loop policy, fade policy, and ducking policy must be deterministic.
- active runtime audio must not require FAT, host paths, or editor source files.

Tooling must reject audio profiles that reference SAI, DMA, LPTIM, GPIO, `SD_MODE`, amplifier state, mixer buffers, decoder internals, hardware callbacks, FAT paths, host paths, or unbounded playback behavior.

---

## Communication Profile Pipeline

Communication profile output must target [[Communication_API_Contract]], not BLE/NINA transport internals.

Required package-facing communication artifacts:

| Artifact | Purpose |
|---|---|
| `communication_context_table` | scene communication contexts, modes, roles, and routes |
| `message_schema_table` | bounded versioned message types and payload schemas |
| `session_policy_table` | session-required admission, optional fallback, timeout, and session-end behavior |
| `comm_rate_limit_table` | send/receive rate and queue limits |
| `comm_diagnostic_labels` | package-facing labels for developer traces |

Rules:

- package authors use sessions, peers, and schema messages, not BLE transport behavior.
- session-required multiplayer/companion packages are valid when declared and bounded.
- every communication scene must declare either fallback/route behavior or session-required admission behavior.
- message size, message queue depth, send rate, receive rate, and processing cost must be bounded.
- HW6 target profiles must reject communication wake behavior until a future measured profile explicitly grants it.
- package-visible disconnect/session events are distinct from Platform BLE/NINA faults.

Tooling must reject communication profiles that reference BLE, NINA, UART, GAP, GATT, SPS, AT commands, pins, bonding storage, flow control, module reset, hardware addresses, or arbitrary byte streams.

---

## Time And Power Intent Pipeline

Time/power and reactive-wait output must target [[Time_And_Power_Intent_API_Contract]], not RTC, clock, STOP, timer, LPBAM, DMA, or PMIC internals.

Required package-facing time/power artifacts:

| Artifact | Purpose |
|---|---|
| `time_power_profile` | calendar requirements, lifecycle policy, wake intents, and cadence hints |
| `schedule_table` | bounded delayed and local-calendar schedule rules |
| `catch_up_policy_table` | bounded missed-event reconciliation behavior |
| `wake_intent_table` | normalized wake intent declarations |
| `scene_cadence_table` | scene cadence, frame, presentation-phase, and latency declarations |
| `reactive_wait_policy_table` | per-state waiting visuals, event interests, schedules, wake intents, and fallbacks |
| `interaction_policy_table` | inactive presentation, meaningful activity, admitted inactive routes, and bounded deferrals |

Rules:

- package authors use PeepOS local calendar/logical time, not RTC hardware.
- authoring blocks compile event handling, settled views, waiting visuals, and schedules into one reactive contract.
- PeepOS inactivity handling is mandatory; gameplay inactivity remains a normal schedule/transition.
- unbounded inactivity deferral is rejected.
- calendar-dependent packages require a target profile that grants `time.calendar`.
- schedules must have bounded table size, bounded catch-up, and declared stale-event behavior.
- sequence/program scenes must declare frame budget, meaningful activity, suspend/resume behavior, and inactive routes.
- state scenes must not poll or remain awake waiting for input; display-only motion belongs in waiting visuals.
- HW6 target profiles must reject communication wake behavior until a future measured profile explicitly grants it.

Tooling must reject time/power profiles that reference RTC registers, SysTick, hardware timers, STOP modes, PLL/clocks, PMIC registers, wake pins, RTOS scheduler internals, or unbounded catch-up behavior.

---

## Diagnostics Profile Pipeline

Diagnostics profile output must target [[Diagnostics_API_Contract]], not Platform debug transports.

Required package-facing diagnostics artifacts:

| Artifact | Purpose |
|---|---|
| `marker_table` | package-local timeline marker IDs |
| `counter_table` | bounded numeric counters |
| `timing_scope_table` | bounded package/runtime timing scopes |
| `trace_value_table` | fixed-schema trace values for dev/twin profiles |
| `warning_code_table` | package/tool warning codes |
| `package_fault_code_table` | package fault codes and lifecycle routes |
| `diagnostic_profile_gates` | build-profile availability and shipping-minimal policy |

Rules:

- package diagnostics explain package behavior, not Platform hardware behavior.
- shipping diagnostics must be explicitly marked and minimal.
- verbose diagnostics are limited to authoring, dev, or digital twin profiles unless release policy allows them.
- diagnostic IDs must be stable and package-local.
- diagnostic payloads and rates must be bounded.
- package fault codes must map to Engine lifecycle policy.

Tooling must reject diagnostics profiles that reference SWD, SWO, UART, USB, BLE, protected storage, hardware registers, RTOS objects, filesystem paths, raw pointers, memory dumps, or unbounded string logging.

---

## Rendering Asset Pipeline

Rendering asset output must target [[Rendering_API_Contract]], not a target-specific display driver.

Source files may include PNG, Aseprite, Tiled, font sources, or other editor-native inputs. Runtime packages must contain compiled PeepOS assets only.

### Immediate STATE Vertical Slice

The first Peep Studio asset path is intentionally limited to masked 1bpp PNG
frames. It must produce deterministic `asset_table`,
`masked_1bpp_sprite_bank`, and `animation_table` records that are consumed by
both `HOST_AUTHORING_PREVIEW` and PeepOS.

For this slice:

- alpha zero is transparent; every nonzero alpha value is opaque.
- a PNG without alpha is fully opaque.
- logical sprite pixels are white or black only.
- frame dimensions, pivots, source rectangles, ordering, durations, and loop
  policy are compiled metadata rather than inferred by firmware.
- waiting visuals are composed from those same assets into final logical 1bpp
  frames before Platform-specific LPBAM preparation.
- preview and firmware must not contain substitute procedural art for a package
  asset that exists in the `.egg`.

Tone/dither inputs, Tiled maps, fonts, fractional transforms, and source-editor
import helpers remain later extensions. Existing reserved asset classes do not
make them dependencies of the masked-1bpp STATE milestone.

### Retained Primitive And Text Expansion

Peep Studio authors retained records rather than immediate firmware draw calls.
The STATE presentation milestone adds package records for line, outline
rectangle, filled rectangle, circle, and ellipse geometry using bounded native
integer coordinates. Package content may use `BACKGROUND`, `SCENE`, and `UI`;
`OVERLAY` remains system-owned.

Exact host preview and firmware must use identical clipping, ink, fill, and
ordering semantics. Private shell/calibration drawing helpers and historical
cursor/marker/diamond proof mappings are not package capabilities.

Initial menu text is compiled on the host into masked 1bpp sprite assets. Formal
runtime font, localization, and mutable-text records remain later extensions.
The compiler must not emit source font paths or require runtime font parsing.

Rendering asset classes in the complete contract are:

| Asset Class | Purpose |
|---|---|
| `masked_1bpp_sprite` | crisp black/white sprite with opacity mask |
| `tone5_masked_sprite` | semantic tone5 sprite with opacity/ownership |
| `tone5_tileset` | tone5 tile graphics for bounded tilemaps |
| `tilemap` | compact bounded map/viewport data |
| `font_1bpp` | crisp monochrome font data |
| `animation_table` | bounded frame/timing table referencing sprite/tile assets |
| `waiting_visual_sequence` | bounded 1bpp visual sequence associated with a settled reactive state |

`tone5` is a coverage model, not a color-depth format.

Source art may be authored as a five-color indexed PNG or equivalent indexed source. Tooling must convert it into validated `tone5_masked` package assets containing logical tone data, masks, and deterministic 1bpp coverage semantics.

Tone values:

| Value | Meaning |
|---|---|
| `transparent` | no ownership |
| `white` | 0% black coverage |
| `light` | about 25% black coverage |
| `mid` | about 50% black coverage |
| `dark` | about 75% black coverage |
| `black` | 100% black coverage |

Tooling may store tone5 using compact implementation formats such as color plane plus mask plane, but the schema must define the semantic tone output.

Integer scaling is the v1 package-facing scaling model. Tooling must validate output bounds, collision/placement metadata, animation frame consistency, dither phase stability, and render cost for each declared scale.

Authoring content targets the bounded `OVERLAY -> UI -> SCENE -> BACKGROUND` retained compositor. `OVERLAY` is Engine/Platform controlled; tools may accept richer source groupings only when they deterministically flatten them into those four runtime layers.

System UI assets are reserved Platform/Engine assets and may remain crisp 1bpp outside package control.

Rendering assets are stored in package chunks defined by [[Package_Blob_Format_Contract]].

---

## Waiting Visual Sequence Pipeline

`waiting_visual_sequence` assets are built from authored state intent before package compilation/export.

Rules:

- each sequence is attached to a settled reactive state/block, not a package-selected hardware mode
- source may be sprites, tone5 assets, tilemaps, UI elements, animations, or direct authored frames
- sequence content resolves to bounded final 1bpp visual states before target-specific Platform packing
- no runtime JSON, PNG, Aseprite, Tiled, or font parsing is allowed during autonomous playback
- sequence size, frame count, cadence, cycle duration, and compiler admission must fit the selected target profile; preferred/fallback and wake/exit bindings are validated through the enclosing reactive-wait policy
- visual playback cannot mutate package variables, fire gameplay transitions, or substitute for logical schedules
- preferred visuals may declare reduced-sequence and hold fallbacks
- HW6 v1 tools treat three global steps as the universally supported waiting-animation form. They preview the deterministic mappings `1 -> 1/1/1`, `2 -> 1/2/1`, `3 -> 1/2/3`, and `4 -> 1/2/3`. A fourth phase and mixed preferred timelines up to twelve steps are marked as requiring target admission.
- export performs one exact preferred admission check and one deterministic guaranteed-mode check. It must not silently pack elements in source order, iteratively remove elements, or expose hardware allocation order as author-visible behavior.
- tooling derives whether `display.waiting_visual_animation` is required or optional from the authored fallback contract
- package-facing assets must not encode SRAM4 addresses, LPBAM descriptors, SPI bytes, Sharp LCD commands, dirty rows, or transfer chunks
- Platform may convert validated content into full frames, logical deltas, hardware row deltas, repeated payloads, or another display-owner format
- package chunks remain portable PeepOS data; hardware playback payloads are Platform/display-owner internals

Normal authoring diagnostics explain budget failures in visual terms such as animated area, frame count, cadence, and simultaneous animated elements. Low-level row/chunk/descriptor details are advanced Platform diagnostics only.

---

## Package Container Rules

The `PeepPkg` container defined in [[Package_Blob_Format_Contract]] is the package compiler output and installer input.

Rules:

- use `PKG1` as the initial package-container magic/version family in examples.
- The canonical installable package filename extension is `.egg`. Editable
  `.peepproj` directories are authoring source and are never installer input.
- FW0 USB staging may classify `.egg` filenames as package candidates after
  MSC reclaim, then run a read-only minimum-envelope validator that requires
  the first four bytes to be `PKG1`. This proves only that the staged file
  begins with the expected package magic; firmware must still validate the
  actual `PeepPkg` header layout, chunk table, integrity fields, compatibility
  schema, and install policy before any real commit.
- FW0 package import scaffolding may present a package-valid prompt and run a storage-owned install stub for one staged package candidate with a valid minimum `PKG1` envelope, but the stub is not an installer. It must not copy, erase, commit, or expose the package to runtime until the real bounded `PeepPkg` validator and install schema exist.
- chunks are addressed by stable package IDs at authoring level and compact chunk indexes/offsets at runtime level.
- every chunk has type, format version, offset, size, alignment, compiler-derived capability metadata, scene references, and integrity metadata.
- per-chunk CRC plus whole-package checksum are required for v1 integrity.
- cryptographic signatures are a future policy placeholder, not a v1 requirement.
- no runtime path may depend on host/editor source files.

---

## Compression And Packing Rules

V1 runtime paths do not allow general-purpose compression.

Allowed packing must be format-specific and bounded:

- bitplanes
- opacity masks
- tone planes
- fixed-layout tilemaps
- bounded ADPCM audio payloads
- simple RLE only where the chunk format declares maximum expansion size and decode budget

Tooling must reject any asset whose decode time, expanded size, or memory requirement is not statically bounded for the selected target profile.

---

## Deterministic Build Rules

- no hidden timestamps in package payload unless explicitly declared
- stable ordering for generated tables/indexes
- reproducible checksums for identical inputs

---

## Validation Steps (Tool Side)

1. schema validation
2. asset bounds and format validation
3. manifest consistency checks
4. scene type and compiler-derived capability validation
5. input map and focus scope validation
6. audio profile and context validation
7. sensor profile and context validation
8. communication profile and message schema validation
9. time/power profile and schedule validation
10. diagnostics profile validation
11. runtime logic, state graph, action table, and scene-frame validation where present
12. internal forbidden hardware, RTOS, filesystem, and Platform-internal API scan
13. deterministic build checks
14. integrity/checksum generation
15. final package compatibility report

Validation failures block package compilation or export.

Warnings that affect runtime safety, determinism, storage integrity, power policy, or capability availability must be treated as errors.

Development profiles may allow placeholders, mocks, warnings, and explicit runtime-safe waivers as defined in [[Game_Authoring_API_Contract]]. They must still block incoherent graphs, unbounded behavior, invalid save schemas, package integrity failures, and unknown scene types.

---

## Integration Rules

Tooling does not:
- emit hardware register assumptions
- embed RTOS queue assumptions
- assume specific peripheral timing implementation

Tooling does:
- target runtime/package contracts only
- emit intent and structured content

---

## Required Artifacts

For each package build retain:
- input manifest and schema versions
- tool version
- command line or orchestration CLI invocation
- generated package checksum
- compatibility report
