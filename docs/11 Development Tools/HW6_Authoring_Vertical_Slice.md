# HW6 Authoring Vertical Slice

Status: `accepted_design`

Implementation status: `partial`

The V1 `.peepproj` STATE subset, semantic validator, normalized intermediate,
deterministic binary `.egg` compiler, independent host package reader, and the
first bounded HW6 embedded-package STATE decoder are implemented. The embedded
reader, hardware SHA-256 integrity path, sparse retained STATE scene, mixed
2-phase/3-phase waiting visuals, STOP2 continuity, and input transitions have
passed initial target validation. External-flash installation and activation,
target-profile closure, authoring preview/editor surfaces, SEQUENCE, PROGRAM,
and end-to-end HW6 evidence remain open.

Target status: `HW6_PENDING_VALIDATION` until measured HW6 evidence is frozen into a shipping-authoritative target profile.

This document defines the first end-to-end proof of the PeepShow game-authoring workflow. It is a validation specification, not a new Engine API contract.

Related:

- [[Development_Tooling_Index]]
- [[Authoring_Tool_Architecture]]
- [[Authoring_Project_Schema_Contract]]
- [[Game_Authoring_API_Contract]]
- [[Package_Contract]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Package_Compatibility_Report_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Reference_Game_Index]]
- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[Evidence_Artifact_Convention]]
- [[HW6_Brought_Up_Tracker]]

---

## Purpose

The vertical slice must prove one continuous path:

```text
game author
  -> authoring project
  -> preview and validation
  -> deterministic package compilation
  -> compatibility report
  -> PeepPkg installation
  -> PeepOS runtime execution
  -> measured HW6 behavior and power evidence
```

The proof is complete only when the same authored project reaches HW6 without:

- hand-editing compiled package data
- manually patching generated assets
- adding Reference Game-only Engine calls
- adding package-specific Platform behavior
- exposing hardware, RTOS, LPBAM, DMA, SRAM, clock, pin, or power controls to the author

The slice demonstrates representative PeepOS execution classes and transitions. It is not intended to demonstrate the full Reference Game.

---

## Proof Package

The proof package is a deliberately small Reference Game package centered on one slime character.

It contains:

1. a reactive ambient state
2. an animated waiting visual where the target profile grants it
3. system inactivity routing with a preserved package scene
4. consumed Start activation behavior
5. a reactive care menu
6. one bounded feed interaction with state mutation, SFX, and save
7. one short program-scene play microgame
8. a result state and return to reactive waiting

The package may use Reference Game art and language, but every behavior must be authored through public, reusable PeepOS primitives.

---

## Package Scenes

The minimum scene set is:

| Scene ID | Scene Type | Responsibility |
|---|---|---|
| `ambient_pet` | `STATE_SCENE` | show the slime, run bounded ambient state changes, publish the next wait contract, and yield |
| `care_menu` | `STATE_SCENE` | present menu state, process one symbolic menu action, settle the next view, and yield |
| `feed_interaction` | `STATE_SCENE` | perform the bounded feed transaction across admitted events, request SFX/save work, handle the result, and return |
| `play_microgame` | `PROGRAM_SCENE` | run a short frame-paced sandboxed interaction with declared budgets and meaningful activity |
| `result_state` | `STATE_SCENE` | display the result, commit bounded state changes if needed, and route back to ambient waiting |

Rules:

- every transition is declared in the authoring project.
- every reactive path settles a view, publishes a wait contract, and yields.
- `play_microgame` declares an inactive/failure route to `result_state` or `ambient_pet`.
- no state scene spins or keeps the CPU awake while waiting for package input.
- the program scene has a finite test scenario even though the generic `PROGRAM_SCENE` contract is activity-bounded rather than duration-bounded.

---

## Experience Flow

### Boot And Restore

1. PeepOS validates and loads the package.
2. The package reads its schema-versioned save record.
3. Missing or invalid package save data follows the declared default/failure route.
4. The package enters `ambient_pet`.
5. `ambient_pet` settles its presentation and yields.

### Ambient Reactive Wait

The slime remains visible while package logic is yielded.

Where `display.waiting_visual_animation` is granted, a bounded authored waiting sequence may continue through the Platform waiting-visual backend. Where it is unavailable, the same authored wait contract uses its declared static or reduced fallback.

The author expresses the desired waiting visual and fallback. The author does not configure LPBAM, transfer chunks, rows, descriptors, retained memory, clocks, or STOP mode.

### Care Menu

A symbolic package action opens `care_menu`.

Each menu action is one bounded reactive transaction:

```text
admitted input
  -> evaluate guards
  -> update menu or package state
  -> request presentation
  -> publish next wait contract
  -> yield
```

The menu may continue a profile-admitted waiting visual while yielded.

### Feed Interaction

The feed action enters `feed_interaction`.

The interaction must:

- validate the selected item/action
- update one bounded package-owned stat
- request one symbolic SFX cue
- request one schema-versioned package save write
- handle save success and save failure through authored routes
- return to a settled `STATE_SCENE`

The interaction must not access storage, audio hardware, or display hardware directly.

If save or audio completion is asynchronous, the interaction may span multiple admitted Engine events. Each event transaction remains bounded and the runtime yields between events; authored logic never blocks waiting for an owner result.

### Realtime Play

The play action enters `play_microgame`.

The microgame must:

- use target-profile-granted logical input actions
- declare target FPS and frame budget
- declare meaningful-activity sources
- declare audio behavior if audio is enabled
- declare suspend/resume behavior
- declare inactive and failure routes to `result_state` or `ambient_pet`
- terminate through a deterministic test result route

The first slice should use a small, visually obvious interaction rather than a complex game system. Its purpose is to prove realtime admission, frame pacing, input, presentation, optional audio, and return to reactive execution.

### Result And Return

The result state displays a clear outcome, performs any bounded score/save transaction, and returns to `ambient_pet`.

The return is not complete until the ambient presentation and wait backend are stable and package logic has yielded.

### System Interaction State

This proof package chooses `preserve_scene` as its inactive route.

For this slice:

- the target/system policy owns the inactivity timeout.
- the package emits meaningful-activity intent through the public contract.
- while `INACTIVE`, package actions other than the target-owned activation gesture are not delivered.
- Start is consumed by PeepOS as the initial HW6 activation gesture.
- the physical Start press is not also delivered to package logic.
- the package observes symbolic `DEVICE_INACTIVE` and `DEVICE_ACTIVE` lifecycle events.
- activation returns to the preserved state scene and establishes its next wait contract.

This is one package-policy choice for the proof. The generic authoring contract allows another admitted inactive route, while target policy may later admit another activation button or chord. Packages cannot disable inactivity handling or choose the physical activation gesture.

---

## Authoring Workflow To Prove

The minimum author workflow is:

1. create a project from the vertical-slice template
2. set package identity and select the HW6 target profile
3. import sprite and audio source assets
4. bind the slime prefab and content parameters
5. author the ambient and menu behavior graphs
6. configure the bounded feed interaction
7. configure the program microgame scene and inactive/failure routes
8. define save fields and defaults
9. configure the interaction policy and waiting-visual fallback
10. run authoring validation
11. run the logical preview with deterministic fake input/time
12. build the package twice and verify deterministic output
13. inspect the compatibility report
14. install the package through the normal package path
15. execute the deterministic HW6 evidence scenario

The GUI and CLI must call the same schema, validator, and compiler operations.

---

## Minimum Tool Surfaces

The first visual tool needs only the surfaces required to complete the workflow:

| Surface | Minimum responsibility |
|---|---|
| project and asset browser | project metadata, target selection, assets, schemas, and stable IDs |
| scene/prefab editor | place the slime and bind authored presentation assets |
| behavior graph | author states, events, guards, actions, waits, and scene transitions |
| property inspector | edit typed parameters, bounds, fallbacks, save fields, and service intent; show compiler-derived capabilities |
| preview/build panel | inject deterministic events, show logical output, validate, build, and display compatibility results |

The first slice does not require a complete general-purpose game IDE. The source schema and headless toolchain remain authoritative over editor convenience.

---

## Required Build Artifacts

One successful build retains:

- authored `.peepproj` source
- normalized validated intermediate representation
- compiled assets
- compiled scene and logic tables
- save/settings schema summary
- compatibility report
- deterministic build report and content hashes
- installable `PeepPkg`
- canonical installable filename ending in `.egg`
- tool and schema version manifest

Two builds from identical semantic source, target profile, tool versions, and build profile must produce identical installable output. Editor layout-only changes must not change the package hash.

---

## Capability And Fallback Proof

The slice must prove both acceptance and rejection behavior.

Required positive cases:

- logical buttons validate
- profile-granted display paths validate
- profile-granted joystick input validates if selected for the microgame
- profile-granted speaker SFX validates if enabled
- package save records validate within profile limits
- a granted waiting-visual sequence validates

Required negative or fallback cases:

- `input.encoder` is rejected for every HW6 profile
- `sensor.light` and `sensor.light_stream` are rejected for every HW6 profile
- `audio.bbb` is rejected for every HW6 profile
- a waiting visual that exceeds the selected profile is rejected or resolves through its declared fallback
- a sequence or program scene without frame budget, meaningful activity, suspend/resume policy, or inactive route is rejected
- an interaction policy without an admitted inactive route is rejected
- an unbounded reactive action or inactivity deferral is rejected
- shipping export against `HW6_PENDING_VALIDATION` is rejected

Normal GUI errors must explain these failures in PeepOS authoring terms rather than exposing hardware implementation details.

---

## HW6 Validation Bootstrap

The initial HW6 proof cannot use a shipping-authoritative LPBAM profile before HW6 evidence exists.

Use this sequence:

1. author and preview against `HW6_PENDING_VALIDATION`
2. compile a bounded development/evidence build that clearly records pending capability use
3. run the target-qualified waiting-visual and power procedures on HW6
4. record accepted evidence through [[HW6_Brought_Up_Tracker]]
5. publish the resulting measured facts in `HW6_VALIDATED_BASELINE` and, if accepted, `HW6_VALIDATED_LPBAM`
6. rebuild the same semantic authoring project against the validated profile
7. require normal shipping validation and firmware install validation to pass

The evidence build does not grant a shipping capability. Only the reviewed target profile does.

---

## Deterministic Device Scenario

The device evidence run uses one fixed package build and one scripted/manual action sequence:

1. cold boot and package restore
2. settle in ambient reactive wait
3. allow system inactivity to occur
4. press Start once and verify consumed activation
5. open the care menu
6. execute one feed interaction
7. return to ambient wait
8. enter the realtime microgame
9. run the fixed realtime input sequence
10. exit through the result state
11. return to ambient wait
12. allow system inactivity to occur again

The input sequence, source voltage, firmware build, package hash, target profile, instrumentation profile, and PPK2 configuration must be recorded with the evidence.

---

## Power Evidence Matrix

Numerical HW6 limits remain `pending_validation` until they are measured, reviewed, and frozen through [[HW6_Brought_Up_Tracker]] and the target profile. The slice defines measurement windows now so later results are comparable.

| Scenario | Window | Required functional proof | Required measurements |
|---|---|---|---|
| boot and restore | power applied to ambient wait stable | package validates, save/default route completes, ambient view settles | boot time, peak current, charge, energy |
| ambient reactive wait | stable yielded interval | package logic is yielded; selected waiting visual or fallback is correct | settled current distribution, average current, waiting cadence where applicable |
| inactive transition | last meaningful activity to inactive wait stable | inactive route occurs once; package scene/state is preserved | transition latency, charge, energy, inactive settled current |
| Start activation | admitted Start edge to next reactive wait stable | Start is consumed; one `DEVICE_ACTIVE` event; no duplicate package action | response latency, presentation latency, charge, energy, return-to-wait latency |
| care-menu action | admitted action to menu wait stable | exactly one state/view transaction and yield | response latency, active duration, charge, energy |
| feed interaction | admitted feed action to reactive wait stable | stat result, SFX request, save result/failure route, and return are correct | transaction latency, save latency, peak current, charge, energy |
| state to program | play action to first deadline-valid realtime frame | scene transition and input focus are correct | transition latency, charge, energy |
| realtime active | fixed microgame interval | correct frames/input/audio; no missed required behavior | average/peak current, frame-time distribution, worst frame time, deadline misses, audio underruns, headroom |
| program to state | terminal realtime event to ambient wait stable | result route completes and reactive wait is re-established | transition latency, charge, energy, return-to-wait latency |
| inactive wait | stable inactive interval | only the target-owned activation gesture can restore package interaction | settled current distribution, average current, unintended wake count |

For short transactions:

```text
charge_mC = integral(current_mA * time_s)
energy_mJ = integral(voltage_V * current_mA * time_s)
```

Steady current alone is not sufficient for reactive or transition acceptance. Transaction energy and deadline correctness are the decision metrics.

---

## PWR_DBG Correlation Profile

`PWR_DBG` remains Platform-owned dev-only instrumentation. The package never reads or writes it.

For this vertical-slice evidence procedure only, the recommended interval convention is:

```text
low  = package runtime settled in a reactive or inactive wait
high = admitted reactive transaction or PROGRAM_SCENE execution active
```

Boot, save, display, audio, scene transition, interaction-state, and wake boundaries must also be mirrored into the structured Platform marker stream defined by [[Power_Measurement_and_Trace_Correlation_Runbook]]. If a test needs pulse semantics instead, that run records the alternate polarity/meaning in its evidence manifest.

Final current evidence must be repeated with unnecessary trace/debug overhead disabled.

---

## Acceptance Gates

### Authoring Gate

- the project can be created and edited without raw package-data editing.
- all authored objects use stable IDs.
- the required runtime flow is visible in authoring terms.
- preview uses the same normalized logic representation accepted by the compiler.
- removed HW6 capabilities fail validation with clear messages.

### Compiler Gate

- validation runs before installable export.
- output is deterministic.
- compatibility reporting is complete.
- no host source paths or editor-only state affect runtime output.
- no forbidden Platform or hardware reference appears in package output.

### Runtime Gate

- the package installs through the normal package path.
- all scene transitions execute through public Engine contracts.
- state scenes yield whenever waiting.
- Start activation is consumed exactly once.
- program execution meets declared deadlines and returns through its declared route.
- save success and failure paths are both testable.

### HW6 Evidence Gate

- every matrix scenario has a reproducible evidence artifact.
- Platform markers and PPK2 capture are correlated.
- current, charge, energy, latency, and deadline results are recorded where required.
- instrumentation-minimized power captures exist.
- accepted measured limits are promoted through the HW6 tracker and target profile rather than copied directly from a development run.

---

## Explicit Non-Goals

The first vertical slice does not include:

- arbitrary package scripts or native code
- a general dialogue system
- maps, quests, shops, NPC pathing, inventory depth, or combat systems
- BLE or NFC gameplay
- multiplayer
- a complete audio sequencer
- an Authoring Kit marketplace
- a measured HW6 digital twin beyond the initial authoring preview
- final editor visual design
- production firmware update or bootloader work

These exclusions keep the proof focused on the authoring/runtime/power architecture.

---

## Implementation Order

1. freeze the canonical scene, interaction, rendering, and evidence contracts
2. migrate the firmware runtime identity to system host, scene type/ID, execution semantic, and lifecycle without changing validated owner boundaries
3. implement one retained `250 ms` presentation timeline shared by awake rendering, held frames, LPBAM preparation, STOP entry, wake, and resume; use the current cursor as the first continuity proof and preserve the validated SPI/LPDMA queue mechanics
4. implement the four retained compositor layers and panel-native dirty-row composition above the low-level display driver
5. implement `STATE_SCENE` bounded event/guard/action execution, wait contracts, scene transitions inside one mounted package, and namespace-backed variables
6. implement mandatory `ACTIVE`/`INACTIVE` interaction state, RTC-backed inactivity, declared inactive routes, and target-owned activation gestures; HW6 starts with Start while the policy remains button/chord capable
7. implement the headless project loader, normalized model, validator, deterministic package compiler, and compiler-derived capability closure for the state-scene slice
8. implement `HOST_AUTHORING_PREVIEW` against the same normalized/compiled scene and presentation semantics using [[Authoring_Tool_Architecture]]; do not label it HW6 evidence
9. implement only the Electron/React surfaces needed to author, preview, validate, and build the STATE portion of this package, with the Python service remaining authoritative
10. install and run the authored STATE package against HW6 so editor iteration no longer requires embedding package bytes in firmware
11. implement `SEQUENCE_SCENE`, then `PROGRAM_SCENE`, with realtime budgets, input routes, suspend/resume behavior, and required state-scene/shell routes; extend the same editor architecture for each type
12. measure representative reactive and realtime workloads, admit intermediate PLL/clock operating points one at a time behind Platform capability resolution, then capture and review the complete power evidence matrix; packages continue to request semantics and deadlines, never MHz

The slice is not complete merely because the game appears on the display. Completion requires the full author-to-package-to-device path and the associated HW6 evidence.
