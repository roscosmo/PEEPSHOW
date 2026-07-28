# Live Tuning and Platform Knobs Contract

This document defines how PeepShow development tools may inspect and change live-safe Platform knobs without reflashing firmware.

Live tuning is a PeepOS developer workflow. It is not raw memory poking and it is not a normal game/package API.

Related:

- [[Development_Tooling_Index]]
- [[Knobs_and_Tuning_Contract]]
- [[Content_Parameter_Schema_Contract]]
- [[USB_Development_Mode_Contract]]
- [[Debug_and_Observability]]
- [[Debug_Workflows]]
- [[Authority_and_Invariants]]
- [[RTOS_Ownership_and_Queue_Topology]]
- [[Brought_Up_Tracker]]

---

## Scope

Defines:

- live-safe tuning model
- generated tuning registry requirements
- owner-routed apply behavior
- session overlays and persistence rules
- host tooling behavior
- validation and evidence requirements

Does not define:

- the exact CDC packet format
- host GUI implementation
- firmware generator implementation details
- package balancing schemas beyond the boundary with package tooling

---

## Core Principle

The authoritative Platform knob source remains the repository knob schema and JSON.

```text
config/knobs.json
config/knobs.schema.json
    |
tools/gen_knobs.py
    |
generated firmware constants
generated live-tuning metadata
generated host/editor metadata
```

Live tuning may temporarily override approved Platform knob values during a developer session.

A useful live Platform value becomes permanent only when the source knob file or schema is updated and rebuilt through the normal pipeline.

Package-facing content values are separate. They become permanent only when package source content, package schemas, package settings, or package-owned generated data are updated through package tooling.

---

## Non-Goals

Live tuning must not:

- mutate arbitrary memory
- bypass owner-thread requests
- edit generated headers directly
- silently change firmware defaults
- patch Platform policy in shipping builds
- expose hardware controls to package/game tools
- expose Platform knobs as normal package/game controls
- replace measured bring-up evidence

GDB memory writes may remain an emergency/debugger technique, but they are not the supported live-tuning workflow.

---

## Naming And Visibility

Use these names consistently:

| Name | Owner | Tool Visibility | Purpose |
|---|---|---|---|
| Platform knob | PeepOS Platform | PeepOS developer tools only | firmware/platform tuning |
| development tuning overlay | PeepOS developer workflow | PeepOS developer tools only | temporary session override of approved live-safe Platform knobs |
| target profile | Platform/Engine | read-only to package tools | published capability and limit set |
| content parameter | package/game author | editable by package authoring tools | balancing and authored package behavior |
| package setting | package/user through Engine APIs | editable where package schema allows | package-local runtime/user preference |
| capability request | package runtime through Engine APIs | callable through Engine APIs | bounded request for hardware-backed behavior |

Rules:

- normal game-authoring tools must not list or edit Platform knobs.
- package tools may read target profiles but must not mutate them.
- content parameters must be stored in package source data or package-generated content, not `config/knobs.json`.
- package settings must use [[Package_Save_Settings_API_Contract]], not Platform settings or Platform knobs.
- capability requests are API calls with Platform-owned grant/clamp behavior, not hidden knob writes.

Suggested namespaces:

```text
platform.knobs.power.static_timeout_ms
platform.knobs.display.stop_max_fps
dev_overlay.platform.knobs.display.stop_max_fps
target_profile.display.width
target_profile.power.stop_max_fps
package.content.pet.hunger_decay_rate
package.settings.difficulty
```

---

## Tuning Classes

Every Platform knob should declare one tuning class.

| Class | Meaning | Live Editable |
|---|---|---|
| `compile_time` | memory layout, stack sizes, queue depths, object counts, static hardware choices | no |
| `boot_applied` | defaults applied during init or boot | no, or reboot-required |
| `runtime_live_safe` | can be changed at a safe runtime boundary through owner validation | yes in developer mode |
| `protected_policy` | power, storage, PMIC, safety, or architecture policy with high risk | no, except explicit bring-up firmware policy |

Rules:

- Platform live tools may only expose `runtime_live_safe` Platform knobs by default.
- `protected_policy` values require explicit bring-up firmware or maintainer-only policy before any live change path exists.
- `compile_time` values never become live-editable because they affect static allocation, object shape, or build-time layout.
- `boot_applied` values may be staged for next boot only if the owner subsystem supports a safe staged update.

Package content parameters are not a Platform knob class. They are package-authored values and are governed by package schemas.

---

## Platform Live Tuning Metadata

Every live-editable Platform knob must have generated metadata.

Required fields:

```text
platform_live_knob:
  id
  path
  owner
  class
  type
  min/max or enum values
  default_value
  current_value_policy
  apply_policy
  persistence_policy
  build_profile_gate
  target_profile_gate
  units
  timebase_domain
  description
  safety_notes
```

Rules:

- `path` is stable and human-readable.
- `owner` names the Platform owner that validates and applies the value.
- `type` must be explicit.
- numeric ranges must be explicit.
- enum values must be explicit.
- timing values must declare their timebase domain.
- descriptions are host/editor metadata, not firmware logic.
- metadata must be generated from the same source as the compile-time Platform knob contract.

---

## Owner-Routed Apply Model

Platform live tuning commands flow through owner requests.

```text
host tool
    |
CDC dev protocol or digital twin control adapter
    |
live tuning registry
    |
owner-routed apply request
    |
owner validates, clamps, rejects, stages, or applies
    |
structured result + telemetry
```

Owners may return:

| Result | Meaning |
|---|---|
| `applied` | value is active now |
| `staged` | value will apply at the declared safe boundary |
| `clamped` | value was adjusted to the owner-approved range |
| `rejected` | value is invalid or unsafe |
| `requires_reboot` | value can only apply after reboot/reinit |
| `unsupported` | selected build/target/profile does not allow this Platform knob |

Rules:

- command handlers must not write the target variable behind the owner.
- owner apply functions must run at a safe boundary.
- owner apply functions must be bounded.
- changes must be observable through response or telemetry.
- failed apply must leave the previous value valid.

---

## Apply Policies

Allowed apply policies:

| Apply Policy | Meaning |
|---|---|
| `immediate_if_idle` | apply now only if owner is idle |
| `next_event` | apply before next owner event dispatch |
| `next_frame` | apply at next frame boundary |
| `next_static_update` | apply at next static/low-power update boundary |
| `next_unit_transition` | apply when runtime unit changes |
| `next_boot` | stage for next boot only |
| `rebuild_required` | not live-editable |

Rules:

- display/render values should apply at render or frame boundaries.
- input filter values should apply between classifier windows or after input owner quiesce.
- audio values should apply at cue/buffer-safe boundaries.
- sensor values should apply through sensor context boundaries.
- storage and power protected policy values must not be live-edited without explicit bring-up policy.

---

## Session Overlays

Live tuning edits are session overlays unless explicitly persisted.

Overlay records should include:

```text
tuning_overlay:
  firmware_commit
  knobs_hash
  board_revision
  target_profile
  changed_values[]
  timestamp
  tool_version
```

Rules:

- overlays must record the base knob-set hash.
- overlays must be exportable as a developer artifact.
- overlays must not silently become firmware defaults.
- overlays used during bring-up evidence must be recorded with the test result.
- applying an overlay to a mismatched firmware/knob hash should warn or reject.

---

## Persistence Policy

Persistence is optional and owner-specific.

Allowed persistence forms:

- session-only overlay
- staged next-boot development setting
- exported patch for `config/knobs.json`

Rules:

- Platform persistent writes route through `thStorage`.
- persistence must be power-fail safe.
- protected Platform settings, calibration, bonding records, installed indexes, and fault logs are not edited through generic live tuning.
- generated headers are never edited by live tools.
- host tools should make source-update/export explicit.

---

## Content Parameters

Content parameters are package-authored values for balancing and package behavior. They are separate from Platform knobs.

Examples:

- Reference Game balancing values
- animation timing for package content
- pet hunger/energy rates
- encounter weights
- dialogue timing
- package-local difficulty defaults

Rules:

- content parameters must use Engine/package schemas.
- content parameters may be previewed through the digital twin or package authoring tools where allowed.
- content parameters must compile into package data or package-owned settings.
- content parameters must not mutate Platform settings, Platform knobs, or hardware policy.
- content parameter commands and UI must not use the Platform `knob` namespace.

---

## CDC Command Shape

The USB CDC developer protocol is defined in [[USB_Development_Mode_Contract]].

Expected Platform live-tuning commands:

```text
platform.knob.list
platform.knob.describe path
platform.knob.get path
platform.knob.set path value
platform.knob.apply path
platform.knob.stage path value
platform.knob.overlay.export
platform.knob.overlay.clear
```

Rules:

- commands operate on stable `platform.knobs.*` paths, not raw addresses.
- values are encoded by schema.
- every command has bounded payload size.
- invalid values are rejected before owner apply where possible.
- owner apply still validates the value.

---

## Digital Twin Behavior

The digital twin should use the same live-tuning metadata where practical.

Rules:

- twin Platform live tuning follows the same class and bounds rules.
- twin may provide faster UI iteration, but it must still model owner-routed apply semantics.
- twin-only changes are not hardware evidence.
- replay captures must record any active tuning overlay.

---

## Evidence Requirements

Any validation or bring-up run using live tuning must record:

- firmware commit
- board revision
- knobs hash
- active tuning overlay
- tool version
- active USB personality, if CDC was used
- whether instrumentation or live tuning may affect timing/power

Do not compare current, wake timing, or STOP behavior against baseline runs unless the active tuning overlay and instrumentation state are known.

---

## Forbidden Live Tuning Targets

The normal live-tuning surface must not expose:

- stack sizes
- queue depths
- RTOS object counts
- flash region layout
- filesystem mount policy
- installed package index internals
- PMIC safety thresholds except explicit bring-up firmware policy
- clock tree or PLL changes
- DMA descriptors
- SRAM4 buffer placement
- LPBAM descriptors
- raw sensor calibration storage
- BLE bonding storage
- persistent fault log internals
- raw memory addresses or pointers

---

## Validation Cases

1. generated live-tuning registry matches `config/knobs.json` and schema metadata.
2. `runtime_live_safe` Platform knob can be listed, described, set, and applied through owner request.
3. out-of-range live value is clamped or rejected according to metadata.
4. non-live-safe Platform knob is not exposed in normal live tuning and is rejected if requested directly.
5. owner apply failure preserves previous valid value.
6. staged next-boot value does not silently alter compile-time defaults.
7. overlay export records firmware commit, knobs hash, board revision, changed values, and tool version.
8. live-tuned validation evidence records active overlay and USB personality.
9. digital twin replay records and reapplies the same tuning overlay for deterministic runs.
10. raw memory address tuning command is unavailable.
11. content parameters are not listed through Platform knob commands.

---

## Rule

Live tuning is generated, typed, bounded, and owner-routed.

It helps find values quickly, but the repository source remains the authority.
