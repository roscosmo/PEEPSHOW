# HW6 Authoring Vertical Slice

Status: `accepted_design`

Implementation status: `pending`

Target status: `HW6_PENDING_VALIDATION` until measured HW6 evidence is frozen into a shipping-authoritative target profile.

This document defines the first end-to-end proof of the PeepShow game-authoring workflow. It is a validation specification, not a new Engine API contract.

Related:

- [[Development_Tooling_Index]]
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
3. automatic input locking with a preserved package state
4. consumed Start unlock behavior
5. a reactive care menu
6. one bounded feed interaction with state mutation, SFX, and save
7. one short realtime play microgame
8. a result state and return to reactive waiting

The package may use Reference Game art and language, but every behavior must be authored through public, reusable PeepOS primitives.

---

## Runtime Units

The minimum runtime-unit set is:

| Unit ID | Runtime class | Responsibility |
|---|---|---|
| `ambient_pet` | `LP_GRAPH` | show the slime, run bounded ambient state changes, publish the next wait contract, and yield |
| `care_menu` | `LP_GRAPH` | present menu state, process one symbolic menu action, settle the next view, and yield |
| `feed_interaction` | `LP_MODULE` | perform the bounded feed transaction, request SFX/save work, handle the result, and return |
| `play_microgame` | `RT_SCENE` | run a short frame-paced interaction with declared budgets and meaningful activity |
| `result_state` | `LP_GRAPH` | display the result, commit bounded state changes if needed, and route back to ambient waiting |

Rules:

- every transition is declared in the authoring project.
- every reactive path settles a view, publishes a wait contract, and yields.
- `play_microgame` declares a reactive fallback to `result_state` or `ambient_pet`.
- no runtime unit spins or keeps the CPU awake while waiting for package input.
- the realtime scene has a finite test scenario even though the generic `RT_SCENE` contract is activity-bounded rather than duration-bounded.

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
- return to a reactive unit

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
- declare a reactive fallback
- terminate through a deterministic test result route

The first slice should use a small, visually obvious interaction rather than a complex game system. Its purpose is to prove realtime admission, frame pacing, input, presentation, optional audio, and return to reactive execution.

### Result And Return

The result state displays a clear outcome, performs any bounded score/save transaction, and returns to `ambient_pet`.

The return is not complete until the ambient presentation and wait backend are stable and package logic has yielded.

### Automatic Input Lock

This proof package enables automatic input locking and chooses `preserve_state` as its lock route.

For this slice:

- the target/system policy owns the inactivity timeout.
- the package emits meaningful-activity intent through the public contract.
- when locked, package actions other than the admitted system unlock route are not delivered.
- Start is consumed by PeepOS as the unlock action.
- the physical Start press is not also delivered to package logic.
- the package observes symbolic `DEVICE_LOCKED` and `DEVICE_UNLOCKED` lifecycle events.
- unlock returns to the preserved reactive state and establishes its next wait contract.

This is one package-policy choice for the proof. The generic authoring contract still allows a package to disable automatic locking or select another admitted lock route.

---

## Authoring Workflow To Prove

The minimum author workflow is:

1. create a project from the vertical-slice template
2. set package identity and select the HW6 target profile
3. import sprite and audio source assets
4. bind the slime prefab and content parameters
5. author the ambient and menu behavior graphs
6. configure the bounded feed interaction
7. configure the realtime microgame scene and fallback
8. define save fields and defaults
9. configure the input-lock policy and waiting-visual fallback
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
| behavior graph | author states, events, guards, actions, waits, and runtime-unit transitions |
| property inspector | edit typed parameters, bounds, fallbacks, save fields, and capability intent |
| preview/build panel | inject deterministic events, show logical output, validate, build, and display compatibility results |

The first slice does not require a complete general-purpose game IDE. The source schema and headless toolchain remain authoritative over editor convenience.

---

## Required Build Artifacts

One successful build retains:

- authored `.peepproj` source
- normalized validated intermediate representation
- compiled assets
- compiled runtime-unit and logic tables
- save/settings schema summary
- compatibility report
- deterministic build report and content hashes
- installable `PeepPkg`
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
- a realtime scene without frame budget, meaningful activity, suspend/resume policy, or reactive fallback is rejected
- an enabled lock policy without an admitted route is rejected
- an unbounded reactive action or lock deferral is rejected
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
3. allow automatic lock to occur
4. press Start once and verify consumed unlock
5. open the care menu
6. execute one feed interaction
7. return to ambient wait
8. enter the realtime microgame
9. run the fixed realtime input sequence
10. exit through the result state
11. return to ambient wait
12. allow lock to occur again

The input sequence, source voltage, firmware build, package hash, target profile, instrumentation profile, and PPK2 configuration must be recorded with the evidence.

---

## Power Evidence Matrix

Numerical HW6 limits remain `pending_validation` until they are measured, reviewed, and frozen through [[HW6_Brought_Up_Tracker]] and the target profile. The slice defines measurement windows now so later results are comparable.

| Scenario | Window | Required functional proof | Required measurements |
|---|---|---|---|
| boot and restore | power applied to ambient wait stable | package validates, save/default route completes, ambient view settles | boot time, peak current, charge, energy |
| ambient reactive wait | stable yielded interval | package logic is yielded; selected waiting visual or fallback is correct | settled current distribution, average current, waiting cadence where applicable |
| lock transition | last meaningful activity to locked wait stable | lock route occurs once; package state is preserved | transition latency, charge, energy, locked settled current |
| Start unlock | admitted Start edge to next reactive wait stable | Start is consumed; one `DEVICE_UNLOCKED` event; no duplicate package action | response latency, presentation latency, charge, energy, return-to-wait latency |
| care-menu action | admitted action to menu wait stable | exactly one state/view transaction and yield | response latency, active duration, charge, energy |
| feed interaction | admitted feed action to reactive wait stable | stat result, SFX request, save result/failure route, and return are correct | transaction latency, save latency, peak current, charge, energy |
| reactive to realtime | play action to first deadline-valid realtime frame | runtime transition and input focus are correct | transition latency, charge, energy |
| realtime active | fixed microgame interval | correct frames/input/audio; no missed required behavior | average/peak current, frame-time distribution, worst frame time, deadline misses, audio underruns, headroom |
| realtime to reactive | terminal realtime event to ambient wait stable | result route completes and reactive wait is re-established | transition latency, charge, energy, return-to-wait latency |
| locked wait | stable locked interval | only the admitted system unlock route can wake package interaction | settled current distribution, average current, unintended wake count |

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
low  = package runtime settled in a reactive or locked wait
high = admitted reactive transaction or RT_SCENE execution active
```

Boot, save, display, audio, runtime transition, lock, and wake boundaries must also be mirrored into the structured Platform marker stream defined by [[Power_Measurement_and_Trace_Correlation_Runbook]]. If a test needs pulse semantics instead, that run records the alternate polarity/meaning in its evidence manifest.

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
- all runtime-unit transitions execute through public Engine contracts.
- reactive units yield whenever waiting.
- Start unlock is consumed exactly once.
- realtime execution meets declared deadlines and returns through its fallback route.
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
- a full digital twin
- final editor visual design
- production firmware update or bootloader work

These exclusions keep the proof focused on the authoring/runtime/power architecture.

---

## Implementation Order

1. freeze this behavior and evidence matrix
2. implement the headless project loader, normalized model, validator, and deterministic package compiler
3. implement the preview runtime against the normalized model
4. implement only the editor surfaces needed to author this package
5. implement the public PeepOS runtime/package path needed by the slice
6. execute HW6 bring-up and publish measured target-profile facts
7. build, install, and run the package against the validated profile
8. capture and review the complete power evidence matrix

The slice is not complete merely because the game appears on the display. Completion requires the full author-to-package-to-device path and the associated HW6 evidence.
