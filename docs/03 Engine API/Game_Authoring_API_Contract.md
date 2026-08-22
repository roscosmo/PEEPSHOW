# Game Authoring API Contract

This document defines the Engine-facing contract for game-authoring tools, package content, and runtime-safe game behavior.

Game-authoring tools target this contract and the package contracts. They must not target Platform hardware abstractions, RTOS internals, CubeMX output, STM32 HAL/LL, middleware internals, or Reference Game-specific implementation details.

Editable project/source files are governed by [[Authoring_Project_Schema_Contract]]. Compiled package/runtime output remains governed by this contract, [[Runtime_Logic_State_API_Contract]], and the package/tooling contracts.

Related:

- [[Engine_API_Index]]
- [[Runtime_Host_Contract]]
- [[Runtime_Host_Internal_State_Machines]]
- [[Runtime_Logic_State_API_Contract]]
- [[Scene_Runtime_and_Interaction_Model]]
- [[Authoring_Project_Schema_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Target_Profile_Schema_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[Package_Contract]]
- [[Package_Manager_State_Machine]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Authority_and_Invariants]]
- [[Architecture_and_Boundaries]]
- [[Game_Documentation_Boundary]]

---

## Scope

Defines:

- the game/tool-facing authoring surface
- the runtime-safe Engine request surface available to hosted games
- package validation requirements before compilation/export
- compiler-derived capability model
- safe hooks for rendering, input, audio, assets, saves, time, sensors, communication, power intent, and diagnostics
- safe hooks for runtime logic, state graphs, action tables, and realtime scene logic

Does not define:

- Platform hardware policy
- HAL, LL, CubeMX, DMA, pin, clock, storage-owner, filesystem, or RTOS details
- Reference Game mechanics or content
- low-level asset file formats beyond Engine-visible requirements
- Platform owner-thread state machines

---

## Layer Position

```text
Game-authoring tools
        |
validated authoring data and package sources
        |
package compiler / asset pipeline
        |
validated package
        |
Engine runtime hosts
        |
Platform capability APIs
        |
Platform owner threads and hardware policy
```

The authoring layer expresses content and intent.

The Engine validates and runs reusable game abstractions.

The Platform decides hardware behavior.

---

## Core Rule

Game-authoring tools must validate all content before package compilation or export.

No tool may emit a compiled package, generated runtime module, asset blob, state table, or installable artifact unless the validation gate passes.

Install-time validation remains mandatory, but it is a second gate. It is not a substitute for tool-side validation.

Game developers work inside the PeepOS sandbox. Low-level concepts such as HAL, GPIO, DMA, RTOS objects, buses, registers, filesystems, and Platform internals should be unrepresentable in normal game tools.

---

## General-Purpose Runtime Principle

PeepOS authoring APIs are genre-agnostic.

The API exposes reusable low-power handheld runtime primitives, not Reference Game mechanics and not a pet-game-specific API.

Valid PeepOS content may include, but is not limited to:

- virtual pets
- clocks and ambient toys
- Game & Watch style games
- puzzle games
- turn-based RPGs
- text adventures
- visual novels
- card or board games
- step/light/motion-reactive games
- multiplayer experiments
- music and BBB toys
- utility or diagnostic-style packages where allowed by package policy

Scene types describe bounded runtime behavior, not genre.

The Reference Game is a proof-of-capability package and showcase. It must be built from the same public PeepOS primitives available to other packages.

Reference Game needs may request new primitives. Accepted primitives must be reusable beyond the Reference Game and documented as Engine/API capabilities before game implementation depends on them.

No Reference Game-only hidden API path is allowed.

Tool UX may include templates for pets, maps, microgames, clocks, dialogue, or other patterns. Templates must compile down to the same general PeepOS primitives and package schemas available to all packages.

---

## Authoring Reuse Model

PeepOS authoring tools may support reusable source-level systems. These are authoring-source concepts, not firmware objects and not new scene types.

Canonical authoring terms:

| Term | Meaning |
|---|---|
| `Template` | complete starter package or package fragment intended for customization |
| `Authoring Kit` | reusable gameplay system composed of behavior graphs, prefabs, assets, content parameters, save/schema additions, service requirements and fallbacks, validation rules, and optional diagnostics |
| `Prefab` | reusable actor/entity setup with visual assets, local parameters, and attached behavior graph references |
| `Behavior Graph` | visual event/state/action logic that compiles into bounded runtime logic tables |
| `Behavior Macro` | reusable bounded graph fragment used inside behavior graphs |

Examples:

| Pattern | Preferred Name |
|---|---|
| pet starter with evolution, stats, art slots, and triggers | Virtual Pet Template |
| reusable evolution rules and stat thresholds | Evolution Authoring Kit |
| reusable merchant interaction and item purchase flow | Shop Authoring Kit |
| configured merchant actor with sprite, dialogue hook, and shop reference | Merchant Prefab |
| reusable locked-door interaction branch | Behavior Macro |

Authoring Kits are the canonical name for reusable gameplay systems. Do not use `module` as the general authoring name for gameplay reuse. Hardware and firmware modules remain Platform terminology, not package scene types.

Compile boundary:

- templates, Authoring Kits, prefabs, behavior graphs, and behavior macros exist in authoring source.
- they may be imported, inspected, customized, parameterized, versioned, and validated by tools.
- they compile into package manifests, scenes, state/action/guard tables, presentation data, asset tables, content parameter schemas, save/settings schemas, diagnostics, and compatibility reports.
- they do not install as independent firmware components.
- they must not expose Platform internals, hardware policy, RTOS ownership, raw storage, HAL/LL, DMA, filesystem paths, or Platform knobs.

An Authoring Kit may compile into one or more scenes. Every generated scene declares exactly one canonical scene type.

Example:

```text
Virtual Pet Template
  Authoring Kits:
    Pet Stats Kit
    Evolution Kit
    Feeding Kit
    Sleep/Idle Kit

  Prefabs:
    Pet Actor Prefab
    Food Item Prefab

  Compiled scenes:
    ambient_pet: STATE_SCENE
    care_interaction: STATE_SCENE
    intro_animation: SEQUENCE_SCENE
    play_microgame: PROGRAM_SCENE
```

This keeps template-driven authoring productive while preserving the Platform -> Engine -> Packages boundary.

---

## Tooling Boundary

PeepOS game tools may expose and produce:

- manifests
- templates and template metadata
- Authoring Kits and kit metadata
- prefabs
- asset tables
- scene graphs
- behavior graphs and behavior macros
- state graphs
- animation tables
- input action maps
- audio cue tables
- BBB pattern tables
- save schemas
- content parameter schemas
- compiler capability-derivation metadata
- wake-intent declarations
- cadence hints
- localized text tables
- bounded script or action tables where a runtime host explicitly allows them

PeepOS game tools must not expose:

- hardware pins, ports, buses, registers, interrupts, DMA, clocks, or sleep modes
- STM32 HAL/LL, CubeMX, ThreadX, FileX, LevelX, USBX, or Platform-internal names
- Platform knobs or `platform.knobs.*` paths as editable package controls
- raw filesystem paths for runtime use
- direct storage-region addresses
- raw memory pointers or function pointers
- host-specific private struct layouts
- unbounded script constructs

The package compiler and internal verifier must also ensure generated artifacts do not contain:

- direct HAL, LL, RTOS, middleware, or Platform-internal API references
- raw filesystem paths for runtime use
- direct storage-region addresses
- raw peripheral register writes
- Platform knob writes or generated Platform knob references
- CubeMX pin, DMA, or clock assumptions
- unbounded scripts
- host-specific private struct layouts unless defined by a versioned package schema

---

## Validation Gate

The toolchain validation gate runs before package compilation/export.

Validation has four layers.

### Authoring Validation

Authoring validation is user-facing and must speak in PeepOS concepts only.

It checks:

- scene entry point exists
- scene/state graph is structurally valid
- transition targets exist
- action lists and expressions are bounded
- asset references resolve or have approved placeholders
- selected scene type supports the authored features
- compiler-derived capabilities match authored feature and service use
- input action map is valid
- audio cue and BBB pattern bounds are valid
- save fields have a schema and version
- cadence, wake intent, sensor use, and communication use are within declared limits
- required fallbacks exist for unavailable optional capabilities

Validation failures block compilation/export.

Warnings that affect runtime safety, determinism, storage integrity, power policy, or capability availability must become errors for normal packages.

### Compiler Validation

Compiler validation checks deterministic package generation:

- manifest schema validity
- asset format and bounds
- stable generated table ordering
- checksums and integrity metadata
- package format version
- scene-type and execution-semantic compatibility
- compatibility report generation

Compiler validation failures block compiled output.

### Internal Safety Verification

Internal safety verification is a hidden guardrail for toolchain bugs, corrupted artifacts, malicious packages, and future advanced tooling.

It checks generated output for forbidden internals such as HAL, LL, RTOS, filesystem, Platform hardware abstraction, raw register, and storage-region references.

Normal PeepOS game developers should not see these names in routine authoring errors. If a normal authoring flow produces an internal-forbidden-token error, treat that as a toolchain defect or corrupted artifact.

### Firmware Install Validation

Firmware install validation runs on-device before install/activation.

It verifies:

- package integrity
- schema compatibility
- scene-type and execution-semantic compatibility
- required capability compatibility
- asset table integrity
- save schema declaration
- install transaction safety

Authoring validation output must include exact rejection reasons that can be surfaced to tool users in PeepOS terms.

Internal verifier output may include low-level detail for toolchain developers, but it should not be the normal author-facing explanation.

---

## Validation Severity

Validation results must use severity levels.

| Severity | Meaning | Blocks Authoring Preview | Blocks Dev Package | Blocks Shipping Package |
|---|---|---:|---:|---:|
| `fatal` | package/source cannot be parsed or is structurally incoherent | Yes | Yes | Yes |
| `error` | runtime safety, schema, bounds, or required capability violation | Yes unless mocked by profile | Yes | Yes |
| `warning` | incomplete or suspicious but runtime-safe | No | No unless profile escalates | Yes unless waived |
| `advisory` | polish, optimization, style, or unused content | No | No | No |
| `waived` | known warning accepted under an explicit waiver | No | No | No if release policy allows the waiver |

Developer-facing errors must use PeepOS authoring language.

Examples:

| Condition | User-Facing Severity |
|---|---|
| missing entry scene | `fatal` or `error` |
| unresolved required sprite without placeholder policy | `error` |
| placeholder art | `advisory` |
| missing optional SFX with fallback | `warning` or `advisory` |
| save variable without schema | `error` |
| action graph may loop without a bounded exit or tick budget | `error` |
| unused asset | `advisory` |
| optional sensor feature has no content fallback | `error` |

---

## Build Profiles

Validation policy is profile-aware.

| Profile | Purpose | Validation Behavior |
|---|---|---|
| `authoring_preview` | editor preview, simulator, and quick iteration | allows placeholders, mocks, and warnings; blocks incoherent graphs and unbounded behavior |
| `dev_package` | test package on device or runtime host | allows warnings and explicit waivers; blocks runtime safety errors |
| `hardware_bringup` | Platform-owned hardware testing | outside normal game package path; must not be treated as game content |
| `release_candidate` | final compatibility and polish pass | warnings must be resolved or explicitly waived |
| `shipping` | normal user-installable package | no unresolved errors, no unresolved safety warnings, no unknown schema issues |

Dev and preview profiles must be productive. They may run unfinished but safe content.

No profile may allow unbounded runtime behavior, missing required entry points, invalid save schema, unknown scene type, or package integrity failure.

Simulator and digital-twin preview modes use the same PeepOS authoring hooks. The active host digital twin is implemented only after HW6 Platform behavior is validated and documented; before that point, simulator mocks are development conveniences and must not be treated as hardware evidence.

---

## Waivers

Waivers are allowed only for issues that are runtime-safe.

Rules:

- waivers must record issue ID or stable validation code
- waivers must record reason, author, date, and intended removal condition
- waivers may not suppress `fatal` findings
- waivers may not suppress hard runtime safety errors
- shipping waiver policy must be explicit per package profile
- waiver state is included in compatibility reports

Valid waiver examples:

- placeholder art accepted for a dev package
- optional SFX missing while silent cue fallback is declared
- temporary text warning accepted for a release candidate

Invalid waiver examples:

- no entry scene
- unbounded action graph
- save write without schema
- package integrity failure
- required capability unavailable with no fallback

---

## Placeholder And Mock Policy

Authoring tools may support placeholders and mock capabilities for preview and dev workflows.

Allowed:

- placeholder sprites
- placeholder text
- silent replacement for missing optional SFX
- simulator-only mock light, step, motion, or communication events
- dummy save records in simulator preview

Rules:

- placeholders must be explicit in the validation report
- required runtime assets need either real content or an approved placeholder policy
- mock capabilities are not proof of hardware support
- mock capability use must not be exported as measured bring-up evidence
- shipping packages must not depend on simulator-only mocks

---

## Runtime Safety Rules

All game-facing runtime behavior must be:

- bounded
- deterministic
- rejectable
- suspendable
- resumable or cleanly stoppable
- versioned where persisted
- independent of direct hardware access

Rules:

- no game-created threads
- no direct RTOS object ownership
- no direct HAL/LL access
- no direct Platform hardware abstraction access
- no raw filesystem access during active runtime
- no runtime heap dependency unless a specific host contract later approves a bounded allocator
- no unbounded loops, recursion, retries, waits, or queues
- no transient pointer ownership across API boundaries
- no function pointers in package data
- no blocking calls without an explicit timeout
- all dynamic capability contexts must have declared lifecycle and fallback policy

The Engine or Platform may reject invalid package output during validation when it exceeds declared bounds, current mode policy, or available resources.

---

## Scene Runtime Model

Authoring tools define package scenes. Each scene has one fixed scene type and execution semantic:

| Scene Type | Execution | Authoring Use | Safety Model |
|---|---|---|---|
| `STATE_SCENE` | `REACTIVE` | state/event-driven experiences, dialogue, menus, choices, pet rooms, clocks, and turn-based interactions | bounded event transactions that settle and yield between admitted events |
| `SEQUENCE_SCENE` | `REALTIME` | bounded data-driven animation, cinematics, timed prompts, and audio/visual timelines | fixed tracks, duration/end marker, frame budget, and declared routes |
| `PROGRAM_SCENE` | `REALTIME` | programmable frame-paced games and interactions | sandbox instruction, memory, stack, event, and frame budgets plus declared routes |

`SHELL`, `PACKAGE`, and `INSTALLER` are system host identities. `SHELL` and `INSTALLER` are Platform-owned; package authors define scenes inside the mounted `PACKAGE` host.

Tools must validate that authored content uses only the features allowed by each declared scene type.

Packages may contain multiple scenes.

Example:

```text
package:
  entry_scene: ambient_pet

  scenes:
    ambient_pet:      STATE_SCENE
    dialogue_flow:    STATE_SCENE
    intro_animation:  SEQUENCE_SCENE
    battle_microgame: PROGRAM_SCENE
```

The entry scene is the package's normal entry point.

Transitions between scenes are Engine-managed and must be declared and validated. The package remains mounted across scene transitions.

This keeps frame-paced behavior scoped to the scene that needs it. A package may spend most of its life in reactive state scenes, briefly enter a sequence or program scene, then return to a settled state scene.

Scene transition forms:

- replace the current scene with a declared target
- push a declared scene onto a bounded return stack
- pop back to the previous scene
- exit to PeepOS shell through an approved system route

Tools must not expose arbitrary jumps to undeclared scene IDs. `TRANSITION` is a bounded graph/action construct, not a scene.

---

## Reactive Authoring Model And Power Compliance

PeepOS authoring blocks express gameplay semantics. They do not expose a separate low-power graph that designers must wire around normal gameplay.

Menus, dialogue, choices, inventory, shops, pet states, turn-based encounters, map inspection, clocks, and similar Authoring Kits normally compile into `STATE_SCENE` blocks. A reactive block performs bounded work when an admitted event arrives, settles its state and presentation, publishes its next wait contract, and yields. PeepOS sleeps automatically between transactions.

A world-oriented Authoring Kit may compile tilemaps, entity definitions and
instances, tags, reusable behaviors, collision data, camera policy, and a turn
controller into the same `STATE_SCENE`. Ordinary movement, combat, collection,
and world-property changes remain inside that scene. A timed interactive action
may push a declared `PROGRAM_SCENE` and consume its bounded result when the
state scene resumes. `REALTIME_SCENE` is obsolete terminology and must not be
emitted by tools. See [[State_Scene_World_Entity_and_Turn_Contract]].

A Menu block, for example, compiles the following behavior without exposing hardware mechanics:

```text
enter or receive input
  -> update selection/state
  -> render settled menu
  -> select waiting visual for that menu state
  -> publish accepted input and timeout events
  -> yield
```

The menu remains mounted while waiting. Its cursor/background may continue animating through a Platform-selected autonomous backend, but no package logic runs until the next event.

### Execution Semantics

| Semantic | Designer Meaning | Runtime Behavior |
|---|---|---|
| `REACTIVE` | respond to input, schedules, sensors, and lifecycle events | bounded transaction, then immediate yield/sleep |
| `REALTIME` | run a bounded sequence timeline or programmable frame-paced scene | continuous admitted frame loop with declared budgets and inactive/end/failure routes |

`STATIC` is no longer an execution-mode token. The term remains valid for static art, static frames, and one-shot display updates where accurate.

For the HW6 v1 profile, state-scene waiting presentation uses a `250 ms` phase quantum. Authored phase durations are integer multiples of that quantum; a two-frame animation with one quantum per phase has a `500 ms` full cycle. This is presentation timing, not a package logic tick. Awake rendering and autonomous playback consume one shared presentation epoch so changing backend does not restart or duplicate a phase. Multiple elements share one combined timeline: two-phase and three-phase elements, for example, repeat over six steps. Updating selection or other content inside the same presentation preserves its step and deadline; entering a different state or presentation rebases to that presentation's settled step.

The authoring tool must preview both the preferred timeline and the target's
deterministic three-step result. It presents a visual budget indicator derived
from the selected target profile, not LPBAM rows, chunks, descriptors, or SRAM
addresses. Preferred content beyond the guaranteed three global steps remains
valid only when the compiled scene fits the preferred target budget and should
be labeled as requiring design consideration.

### Reactive Block Output

Templates and Authoring Kits may expose friendly controls, but compile to the common contract:

```text
reactive_block:
  entry_actions[]
  event_handlers[]
  state_transitions[]
  settled_view_ref
  waiting_visual_ref
  waiting_visual_fallback_ref
  event_interests[]
  schedules[]
  gameplay_timeout_transitions[]
  interaction_context_ref
  bounds
```

Rules:

- designers do not place sleep, STOP, DMA, LPBAM, or wake-pin blocks
- custom code handles a bounded event and returns control to PeepOS
- a designer-authored gameplay inactivity timer is a normal schedule and state transition, such as `explore -> pet_idle`
- an `interaction_context_ref` may select only predeclared meaningful-activity sources or bounded deferrals from the package interaction policy; it cannot change timeout, route, or activation semantics
- waiting visuals describe intended appearance while the block waits
- Platform/tooling derive hold, autonomous playback, reduced animation, or another admitted backend
- visual motion while waiting cannot mutate game variables or advance committed state
- every preferred waiting visual declares a reduced visual or hold fallback unless the target profile makes the preferred capability mandatory and available

### System Interaction State

PeepOS always owns `ACTIVE`/`INACTIVE` interaction state and the inactivity timeout. Authoring exposes only package presentation and routing policy:

```text
interaction_policy:
  meaningful_activity_sources[]
  inactive_route             # preserve_scene, transition_to_scene, exit_to_shell
  inactive_target_scene
  inactive_waiting_visual
  bounded_deferrals[]
```

Rules:

- interaction-state handling is a PeepOS system overlay, not ordinary package input code
- packages cannot disable inactivity handling or author the timeout
- while `INACTIVE`, only a target-owned system activation gesture restores normal interaction
- HW6 initially uses Start; a future target profile may admit another button or a classified chord such as `L+R`
- the activation gesture is consumed by PeepOS and does not also activate a package action
- the package receives symbolic `DEVICE_INACTIVE` and `DEVICE_ACTIVE` lifecycle events
- inactive routes are limited to preserving the current scene, transitioning to a declared scene, or exiting to shell
- bounded cinematics or required sequences may defer inactivity until completion
- unbounded deferral is forbidden
- declared gyro or other admitted control activity may count as meaningful activity
- passive animation and cosmetic waiting visuals do not count as activity
- if inactivity fires during a sequence or program scene, the scene follows its declared suspend/inactive route before the inactive wait is established

### Power Compliance

Rules:

- every scene declares one scene type
- every state-scene block that can settle resolves a reactive wait contract
- every package declares an entry scene
- state scenes do not poll or remain awake waiting for input
- sequence and program scenes declare frame budgets, meaningful activity, suspend/resume behavior, and inactive routes
- PeepOS inactivity handling is always present
- inactivity deferrals are statically bounded
- packages express waiting visual, event, schedule, wake, latency, and activity intent only
- Platform may clamp cadence and select a different admitted waiting-visual backend

Power-facing authoring primitives:

| Primitive | Purpose |
|---|---|
| `reactive_wait` | declares accepted events, schedules, waiting visual, wake intent, and fallback for a settled state |
| `waiting_visual` | describes cosmetic display behavior while waiting; never names LPBAM |
| `gameplay_timeout_transition` | normal designer-authored delayed state transition |
| `interaction_policy` | inactive presentation, meaningful activity, inactive route, and bounded deferral |
| `realtime_activity` | declares meaningful active work and frame-paced admission |
| `latency_tolerance` | requests response characteristics without selecting sleep hardware |
| `inactive_route` | declared route from realtime execution to a state scene or shell |
| `capability_context` | bounded temporary high-duty behavior; not a hardware command |

Tools must not expose STOP level, clocks, LPBAM setup, RTC programming, DMA, display transfer internals, peripheral power state, SRAM4 placement, or wake-pin configuration.

Profile-dependent behavior:

| Target Profile | Tool Behavior |
|---|---|
| `HW6_PENDING_VALIDATION` | model reactive/waiting-visual behavior but report hardware-dependent limits as provisional |
| `HW6_VALIDATED_BASELINE` | compile reactive blocks; use hold or measured wake/update/return waiting visuals |
| `HW6_VALIDATED_LPBAM` | compile eligible waiting visuals for autonomous playback and enforce measured sequence budgets |
| `HOST_AUTHORING_PREVIEW` | preview preferred and fallback waiting visuals with explicit compatibility warnings |
| `HOST_DIGITAL_TWIN_HW6` | mirror measured HW6 reactive, interaction-state, and autonomous-display behavior |

---

## Package Manifest Requirements

Every package manifest must declare:

- package ID
- name
- package version
- package format version
- entry scene
- scenes
- required capabilities
- optional capabilities
- wake intents
- cadence hints
- latency tolerance
- asset table
- save schema version
- storage write budget
- compatibility constraints

The manifest is authoritative for runtime admission. The compiler derives its capability declarations from scene types, nodes, properties, bindings, service use, and fallbacks. Runtime code may request less than the manifest declares, but it must not request capabilities that were not derived and validated at package or scene scope.

---

## Capability Derivation Model

Capabilities are abstract Engine-visible requirements generated by the compiler. Normal authors choose services and behavior, not capability flags.

Canonical capability names live in [[PeepOS_Capability_Registry]].

Examples:

| Capability | Meaning |
|---|---|
| `display.mono_canvas` | can render to the logical monochrome display surface |
| `input.buttons` | can receive logical button actions |
| `input.encoder` | can receive logical encoder deltas |
| `input.joystick_vector` | can receive normalized joystick vector/action data |
| `audio.music` | can use symbolic music cues |
| `audio.sfx` | can use symbolic SFX cues |
| `audio.bbb` | can use bounded BBB tone/pattern cues |
| `audio.timeline` | can use symbolic audio timeline events for diagnostics, replay, or package logic where supported |
| `sensor.light` | can consume resolved ambient-light value and band |
| `sensor.light_stream` | can use bounded active light sampling contexts where supported |
| `sensor.imu_steps` | can consume step session totals and deltas |
| `sensor.imu_events` | can consume motion, tap, shake, tilt, or orientation events where supported |
| `sensor.imu_motion_snapshot` | can consume normalized motion/orientation snapshots |
| `sensor.imu_motion_stream` | can use bounded higher-rate motion contexts for realtime gameplay |
| `comm.multiplayer` | can use generic multiplayer sessions and bounded messages |
| `comm.companion` | can use companion-app sessions and bounded messages |
| `comm.session_required` | can declare scenes that require an active communication session |
| `comm.message_schema` | can declare bounded versioned message schemas |
| `save.records` | can read/write package save records through Engine APIs |
| `time.calendar` | can read valid PeepOS local date/time where the target profile grants it |
| `time.delayed_event` | can schedule bounded package events after a duration |
| `time.calendar_schedule` | can schedule bounded package events against local calendar rules |
| `time.frame_delta` | can consume runtime host frame delta in sequence or program scenes |

Capability names are not hardware names. They must not include pin numbers, DMA channels, HAL handles, or device register names. Advanced tools may show why a capability was derived and help authors design an optional fallback, but cannot require manual capability bookkeeping.

---

## Target Profiles

Tools validate packages against a target profile.

Target profiles define which capabilities and limits are available for a given target.

Required profile families:

- `HW6_PENDING_VALIDATION`
- `HW6_VALIDATED_BASELINE`
- `HW6_VALIDATED_LPBAM`
- `HOST_AUTHORING_PREVIEW`
- `HOST_DIGITAL_TWIN_HW6`

Rules:

- `HW6_PENDING_VALIDATION` is useful for tool design but not shipping authority
- hardware-derived profiles require evidence in [[Brought_Up_Tracker]]
- `HOST_DIGITAL_TWIN_HW6` is derived from measured HW6 behavior after Platform validation
- package compatibility reports must list the target profile used for validation
- packages may declare profile-specific fallbacks

Target profile fields are defined in [[PeepOS_Capability_Registry]] and [[Digital_Twin_Host_Runtime_Contract]].

---

## Authoring Tool Families

Game tools should expose high-level creation workflows that compile to general PeepOS primitives.

Recommended tool families:

| Tool Family | Compiles To | Notes |
|---|---|---|
| state scene/block editor | state graph, bounded actions, reactive waits, schedules, waiting visuals, and interaction behavior | default authoring path for state-based package behavior |
| sequence editor | bounded visual/audio tracks, markers, controls, and scene-end route | data-driven realtime presentation without arbitrary program logic |
| program scene editor | sandbox program plus declared memory/instruction/frame budgets and routes | programmable realtime behavior |
| scene editor | scene graph, draw commands, asset references, input maps | used by reactive settled views and realtime scenes |
| tile/map importer | bounded tilemap assets, viewport metadata, collision/data tables | may import from tools such as Tiled, but runtime output must be bounded |
| animation editor | sprite/frame animation tables and optional waiting-visual sequence candidates | continued waiting motion is target-profile gated |
| dialogue/text editor | text tables and state graph actions | no filesystem paths at runtime |
| audio cue editor | music/SFX/BBB cue tables | validates duration, format, and voice bounds |
| save schema editor | save records, defaults, migrations | required before save writes |
| input map editor | actions, focus scopes, bindings | maps to logical input only |

External formats are import sources, not runtime APIs.

For example, Tiled maps may be imported, but the compiler must produce bounded PeepOS tilemap/data-table assets. A package must not stream or parse arbitrary Tiled files at runtime.

---

## Scene And State Graph Hooks

Detailed runtime logic behavior is defined in [[Runtime_Logic_State_API_Contract]].

Tools may author:

- scenes
- scene entry and exit actions
- state graph nodes
- transitions
- guarded transitions
- timers
- event reactions
- bounded action lists
- local variables with declared type and bounds

Runtime rules:

- every graph has one declared entry point
- every transition target must exist
- transition evaluation must be bounded
- action list length must be bounded
- timers must declare their timebase and maximum duration
- graph-local variables must have fixed size and declared reset/persist behavior
- invalid transition or missing asset routes to host fault handling

State graphs may express game logic. They may not express hardware policy.

`SEQUENCE_SCENE` accepts bounded timeline tracks and symbolic actions but no arbitrary per-frame program. `PROGRAM_SCENE` may use the approved sandbox instruction model. Both must declare frame budgets, meaningful-activity rules, suspend/resume behavior, bounded inactivity deferrals where used, and inactive routing before validation can accept them.

---

## Script And Action Table Rules

Script and action table behavior is governed by [[Runtime_Logic_State_API_Contract]].

If a runtime host supports scripted logic, the script system must be explicitly bounded.

Allowed forms:

- declarative action tables
- finite state graph actions
- bounded expression evaluation
- host-approved bytecode with instruction and stack limits

Required limits:

- maximum instruction count per tick/event
- maximum stack depth
- maximum local variable storage
- maximum action table length
- no recursion unless statically bounded and validated
- no dynamic code loading after package validation
- no direct calls to Platform hardware APIs

Script validation must run before package compilation/export.

---

## Rendering Hooks

Game-facing rendering is through Engine drawing abstractions.

The detailed rendering contract lives in [[Rendering_API_Contract]].

Tools may author:

- masked 1bpp sprites
- tone5 masked sprites
- tilemaps
- tilesets
- frame animations
- text labels
- fonts
- UI panels
- simple shape primitives where supported
- integer scale factors
- bounded waiting-visual sequence candidates
- scene-local draw ordering

Runtime may request:

- draw masked 1bpp sprite or frame
- draw tone5 sprite or frame
- draw integer-scaled sprite
- draw tile region
- draw text from validated text table
- play animation by ID
- present frame or scene update

Rules:

- assets are referenced by ID, not filesystem path
- draw command count is bounded per frame/event
- target canvas profile must be declared by capability, not hardware peripheral name
- Engine and Platform detect changed display regions internally where useful
- no package may control SPI, DMA, EXTCOMIN, display voltage translation, or display sleep policy
- `tone5` is a semantic coverage model, not native display color
- integer scaling is the v1 scaling model for package-facing scaled sprites
- visual layer order is `OVERLAY -> UI -> SCENE -> BACKGROUND`
- `OVERLAY` is Engine/Platform controlled; packages may supply validated style assets but cannot suppress mandatory system information
- authoring tools place content into the four retained logical layers and validate each target profile's retained-memory/object limits
- system UI may use baked crisp 1bpp assets outside the package-authored scene surface
- waiting-visual motion uses validated final visual states only; it does not run arbitrary package rendering or gameplay logic while the reactive host is yielded

For the HW6 target profile, the expected primary display capability is a logical monochrome canvas matching the Platform display contract.

---

## Variable And Property Hooks

Authoring tools expose typed property paths while compiled packages use stable numeric IDs:

| Namespace | Ownership | Examples |
|---|---|---|
| `system.*` | PeepOS, read-only | time, uptime, battery state of charge, step count |
| `game.*` | package | score, health, current room, persistent progress |
| `scene.*` | active scene | selected item, page, local state |
| `entity.*` | scene object | position, sprite/frame, visibility, animation state |

Property access compiles to bounded table operations, not runtime string reflection. Service-backed `system.*` reads cause the compiler to derive their capability and event requirements.

---

## Input Hooks

Game-facing input is logical and focus-routed.

The detailed input/focus contract lives in [[Input_Focus_API_Contract]].

Tools may author:

- action names
- action maps
- focus scopes
- button bindings
- chord bindings
- hold bindings
- repeat behavior requests
- encoder delta bindings
- joystick vector or direction bindings
- low-power wake input intents
- fallback bindings for unavailable optional inputs

Runtime may consume:

- action pressed/released
- action repeated
- action held
- chord action
- encoder delta action
- normalized joystick vector/action
- input focus gained/lost

Rules:

- Platform does not assign universal accept/back/action meanings.
- Engine focus maps logical Platform input into package actions.
- Tools may declare preferred bindings, but shell/platform policy may reserve or override system-critical inputs.
- `BTN_BOOT` is never normal game input.
- Start shipping intent is power policy, not game input.
- raw GPIO, EXTI, timer counter, or ADC input is forbidden.
- raw joystick magnetic readings and encoder hardware counters are forbidden.
- input focus must be released or transferred during scene transitions.
- wake input is delivered through normal resume/lifecycle flow before package actions.

---

## Audio Hooks

Detailed audio API behavior is defined in [[Audio_API_Contract]].

Game-facing audio is symbolic and creatively open. PeepOS does not require packages to remain semantically complete when muted.

Tools may author:

- music cues
- SFX cues
- BBB patterns
- BBB tones
- BBB sweeps
- cue priorities
- cue groups
- volume defaults
- loop flags
- fade hints
- audio contexts
- symbolic timeline markers

Runtime may request:

- play/stop/pause/resume music cue
- play SFX cue
- stop SFX group
- play BBB pattern
- play bounded BBB tone
- play bounded BBB sweep
- set bus volume intent
- set mute intent
- consume symbolic cue timeline events where supported

Rules:

- audio assets must be prevalidated and package-contained
- SFX and music formats must match Engine/Platform accepted formats
- BBB sequence duration, step count, frequency range, and repeat count are bounded
- physical output may be muted, suppressed, faded, ducked, stopped, or quarantined by PeepOS policy
- audio-centric gameplay is allowed
- no game code may control SAI, DMA, LPTIM, `SD_MODE`, or amplifier state
- no FileX/FAT streaming in active runtime loops
- package logic must not depend on DMA callbacks, buffer refill timing, SAI completion, or LPTIM interrupts

---

## Asset Hooks

Game runtime accesses assets by ID through Engine/package APIs.

Asset classes may include:

- image/sprite assets
- tilemap assets
- animation tables
- audio assets
- BBB pattern assets
- text/localization assets
- graph/state table assets
- data tables

Rules:

- every asset has a type, ID, size, checksum, and compatibility metadata
- asset references must resolve at validation time
- asset bounds must match the declared scene type and selected target profile
- missing, corrupt, or incompatible assets must reject install or route to runtime fault handling
- runtime code must not access host-visible staging paths directly
- active runtime reads use package-safe asset APIs only

---

## Save Data Hooks

Save data is schema-driven and package-owned through Engine save APIs.

The detailed save/settings contract lives in [[Package_Save_Settings_API_Contract]].

Tools may author:

- save schema version
- record types
- default values
- migration declarations
- maximum record sizes
- write frequency assumptions
- reset/erase behavior

Runtime may request:

- read save record
- write save record
- enumerate package-owned save keys where allowed
- migrate save record through approved migration path
- reset package-owned save data through explicit user/system flow

Rules:

- saves are not direct filesystem files
- saves are not host-writable staging content
- writes are bounded and power-safe through Platform storage
- schema changes require versioning
- failed write must preserve the previous valid record where possible
- high-frequency writes may be clamped or rejected
- package settings use the same package-owned schema discipline
- package code must handle save/settings API failure
- no save/settings API may mutate Platform settings, calibration, BLE bonding, install metadata, or fault logs

---

## Time, Cadence, And Wake Intent Hooks

Detailed time and power intent API behavior is defined in [[Time_And_Power_Intent_API_Contract]].

Games express logical schedules, reactive waits, meaningful activity, and realtime cadence. Platform owns physical timing and sleep policy.

Tools may author:

- calendar-time requirements
- delayed event and local-calendar schedule requests
- reactive wait/event interests
- gameplay-timeout transitions
- waiting-visual timing intent
- latency tolerance
- symbolic wake intent
- catch-up policy
- meaningful-activity sources
- interaction policy, inactive route, and bounded inactivity deferrals
- realtime target cadence and frame budget

Runtime may:

- schedule an event after a bounded delay
- schedule an event against a local-calendar rule
- publish the next reactive wait contract
- request realtime frame pacing
- mark admitted meaningful activity
- declare temporary wake intent
- read PeepOS local calendar time
- consume elapsed suspend/resume time

Rules:

- Platform may clamp, coalesce, delay, or reject cadence requests
- no package may directly program RTC, SysTick, timers, STOP mode, or clocks
- no package may set, correct, or resync RTC/calendar time
- timing knobs must use documented timebase domains
- sequence and program scenes must have explicit frame budgets, meaningful-activity rules, suspend/resume behavior, and inactive routes
- state-scene content must tolerate missed or delayed schedules where policy requires it
- package-authored gameplay inactivity is a normal schedule, not the system interaction-state timer
- calendar-dependent packages may assume valid PeepOS time after system setup/admission
- missed schedule catch-up must be bounded
---

## Sensor Hooks

Detailed sensor API behavior is defined in [[Sensor_API_Contract]].

Game-facing sensor data is normalized, resolved, and capability-gated.

Tools may author:

- sensor capability requirements
- sensor context declarations
- sample cadence hints
- event interests
- calibration dependency declarations
- optional content fallback behavior for target-profile portability

Runtime may consume:

- resolved ambient-light value
- ambient-light band
- IMU step session delta or total snapshot
- motion/tap/shake/tilt/orientation events where supported
- normalized motion/orientation snapshots or streams where supported
- normalized joystick vector through input APIs

Rules:

- raw ADC, raw I2C registers, raw magnetic diagnostic values, and raw IMU configuration are not normal game APIs
- Platform may internally clamp sensor rate and duration while preserving the package-facing contract
- required sensor primitive failure is handled by Platform/Engine fault logging and lifecycle policy
- optional sensor features require declared content fallback behavior
- sensor streaming contexts are bounded
- sensor contexts must not change Platform sleep policy directly

---

## Communication Hooks

Detailed communication API behavior is defined in [[Communication_API_Contract]].

Communication is generic, transport-agnostic, and capability-gated.

Tools may author:

- multiplayer capability requirement
- companion-app capability requirement
- session role intent
- message schema
- maximum message size
- rate limits
- `optional` or `session_required` context behavior
- timeout and session-end route behavior

Runtime may request:

- advertise session
- join session
- leave session
- send bounded message
- receive bounded message/event
- query communication capability state

Rules:

- no package may control BLE hardware, NINA pins, UART, bonding storage, or BLE command protocol
- messages must have fixed maximum size and schema version
- each communication scene must declare either fallback/route behavior or session-required admission behavior
- peer disconnects, session closes, and message timeouts are package-visible session events
- BLE/NINA/UART faults are Platform/Engine diagnostics, not normal gameplay branches
- pairing/bonding is Platform-owned
- HW6 communication cannot wake the device unless a future measured profile explicitly grants that capability
- Platform may reject invalid communication use during validation or admission where it would violate power, storage, or realtime policy

---

## Power Intent Hooks

Detailed time and power intent API behavior is defined in [[Time_And_Power_Intent_API_Contract]].

Games and tools express scene, waiting, activity, and interaction intent only.

Tools may author:

- scene type; execution semantic is derived from it
- reactive wait contracts
- waiting visuals and reduced/hold fallbacks
- logical schedules and gameplay-timeout transitions
- latency tolerance and symbolic wake intents
- meaningful-activity sources
- interaction policy and admitted inactive route
- bounded inactivity deferrals
- realtime cadence, frame budget, suspend/resume behavior, and inactive/failure routing

Runtime may publish:

- the next reactive wait contract
- meaningful activity from a declared source
- realtime work pending
- bounded inactivity-deferral completion
- temporary capability need

Scenes may declare temporary capability contexts through Engine APIs, such as high-rate sensor sampling, step-counter session behavior, audio activity, communication session activity, or realtime activity. These are validated PeepOS contexts, not direct hardware control.

Rules:

- state scenes yield as soon as bounded event work settles
- waiting for input is not an awake runtime mode
- packages cannot disable system inactivity handling
- only admitted meaningful activity refreshes the interaction window
- the target-owned activation gesture is consumed by PeepOS; the package receives `DEVICE_ACTIVE`, not the physical gesture as an action
- inactive routes preserve the current scene, transition to a declared scene, or exit to shell
- inactivity deferral is always bounded; unbounded deferral is forbidden
- package-authored gameplay inactivity is a normal schedule/scene transition, separate from system interaction state
- Platform chooses sleep class, clock profile, waiting-visual backend, and wake-source wiring
- Platform owns quiesce/resume sequencing
- invalid context use is caught before package compilation/export where possible
- if a required context cannot be maintained at runtime, Platform/Engine handles fault logging and lifecycle policy
---

## Content Parameter Hooks

Packages may define content parameters through package schemas.

Content parameters are package-authored values used for balancing or authored behavior. They are not Platform knobs.

Allowed content parameter examples:

- pet hunger or energy rates
- encounter weights
- animation timing
- dialogue timing
- puzzle constants
- package-local difficulty defaults

Rules:

- content parameters live in package source data, package schemas, or generated package content.
- content parameters may be edited by normal game-authoring tools.
- content parameters may be previewed in the digital twin.
- content parameters must compile into package data or package-owned settings.
- content parameters must not mutate Platform knobs, Platform settings, hardware policy, sleep policy, storage policy, sensor policy, PMIC policy, or communication policy.
- any hardware-facing intent must be expressed through target profile validation and bounded capability requests.

---

## Package Settings Hooks

Packages may define package-owned settings through schemas.

The detailed save/settings contract lives in [[Package_Save_Settings_API_Contract]].

Allowed package settings examples:

- difficulty
- text speed
- package-local sound preference
- package-local input preference
- accessibility preference local to the package

Rules:

- package settings are not Platform settings
- PeepOS may render and edit package settings through system UI
- package settings are stored through package save/settings APIs
- packages may not directly mutate Platform settings, hardware policy, sleep policy, storage policy, sensor policy, PMIC policy, or communication policy
- hardware-affecting behavior is expressed through bounded capability contexts, not direct settings mutation

---

## Diagnostics Hooks

Detailed diagnostics API behavior is defined in [[Diagnostics_API_Contract]].

Game-facing diagnostics are bounded and optional.

Runtime may emit:

- package event markers
- package fault codes
- counters
- lightweight timing markers
- compatibility report references
- bounded trace values in dev/twin profiles

Rules:

- diagnostic output is rate-limited
- diagnostics do not own SWD, SWO, USB, UART, storage, or BLE
- package diagnostics must not expose protected Platform storage
- package diagnostics explain package behavior, not Platform hardware behavior
- shipping packages may retain minimal package fault codes and selected bounded counters
- production builds may compile out verbose package diagnostics while preserving fault classification

---

## Forbidden APIs

The following are never part of the game-authoring or game-runtime surface and should be unrepresentable in normal PeepOS tools:

- STM32 HAL or LL APIs
- CubeMX generated handles
- Platform `ps_hw_*` APIs
- GPIO, EXTI, DMA, ADC, SPI, I2C, SAI, UART, OCTOSPI, RTC, or timer control
- ThreadX object creation or direct queue/event ownership
- FileX, LevelX, USBX, or raw flash access
- direct mount, unmount, or host-export operations
- direct sleep, clock, reset, watchdog, or PMIC control
- raw BLE/NINA commands or bonding storage
- raw device register read/write
- arbitrary host filesystem paths

Any package or generated output containing forbidden tokens or imports must fail internal safety verification before compilation/export.

Normal authoring tools should report problems in PeepOS terms, such as missing scene entry, asset too large for profile, save field missing schema, or capability fallback missing. They should not expose low-level names to game developers.

---

## Versioning And Compatibility

Versioned contracts are required for:

- package format
- manifest schema
- asset table schema
- scene/state graph schema
- input map schema
- save schema
- optional script/bytecode format
- message schema

Rules:

- major version mismatch rejects install or compilation
- minor version mismatch is allowed only when backward-compatible
- tool output records schema versions and tool version
- runtime fault reports include package ID and relevant schema versions

---

## Required Toolchain Checks

Every package build must produce:

- validation report
- compatibility report
- `PeepPkg` package blob
- selected build profile
- selected target profile
- waiver list
- schema version list
- required and optional capability list
- power compliance summary
- asset inventory and checksums
- save schema summary
- scene-type and execution-semantic summary
- generated package checksum

User-facing reports must use PeepOS authoring terms.

Internal verifier reports may include low-level forbidden-token diagnostics for toolchain developers, but these should normally be treated as tool/compiler defects rather than ordinary game-author mistakes.

The package compiler must refuse to emit installable output when:

- required capability is unknown or unavailable for the target profile
- graph validation fails
- asset bounds are exceeded
- save schema is invalid
- power compliance validation fails
- forbidden API access is detected
- scene-type or execution-semantic rules are violated
- deterministic build checks fail
- runtime safety warnings remain unresolved

---

## Validation Cases

1. valid `STATE_SCENE` package validates, compiles, installs, and runs without hardware assumptions.
2. normal PeepOS authoring UI provides no path to hardware, RTOS, filesystem, or Platform-internal concepts.
3. generated artifact containing HAL, RTOS, GPIO, DMA, filesystem, or `ps_hw_*` tokens fails internal safety verification.
4. package requesting undeclared capability at runtime is rejected by Engine.
5. package with unresolved required asset ID fails authoring validation unless an approved placeholder policy applies.
6. package with placeholder art can run in `authoring_preview` or `dev_package` profile.
7. package with unbounded graph loop or action list fails validation in every profile.
8. package with oversized audio, BBB, save, communication, or render command data fails validation.
9. scene type/content mismatch fails validation before package compilation/export.
10. install-time validation rejects a package whose manifest/checksum was corrupted after tool validation.
11. optional sensor or communication feature validates only when declared fallback behavior exists.
12. suspend/resume during active package behavior preserves host and package state consistency.
13. save write failure preserves previous valid save record where possible.
14. sensor streaming context above the target profile limit fails validation before package compilation/export.
15. waived warning appears in the compatibility report with reason and removal condition.
16. sequence or program scene without meaningful-activity rules and an inactive route fails power compliance validation.
17. package requiring `display.waiting_visual_animation` fails shipping validation unless the selected target profile grants it.
18. package that attempts to keep realtime cadence without admitted meaningful work or through unbounded inactivity deferral fails power compliance validation.
19. package that requires continued waiting-visual motion validates only against a target profile that grants `display.waiting_visual_animation`.
20. package that exposes or requires display transfer/internal update-region control fails internal safety verification.
21. package attempt to disable system inactivity handling fails validation.
22. interaction policy without an admitted inactive route fails validation.
23. the target-owned activation gesture is consumed and cannot also activate a package action.
24. any unbounded inactivity deferral fails validation.
25. package-authored system inactivity timeout or physical activation gesture fails validation.

---

## Rule

Game-authoring tools create validated content and intent.

Engine runtime hosts execute reusable bounded abstractions.

Platform owns hardware policy.
