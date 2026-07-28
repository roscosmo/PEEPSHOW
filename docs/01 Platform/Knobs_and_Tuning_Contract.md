# Platform Knobs and Compile-Time Tuning Contract

This document defines the Platform knob system contract for PeepOS firmware tunables.

A Platform knob is a PeepOS implementation value used to tune firmware, hardware policy, resource sizing, timing, diagnostics, or owner-subsystem behavior. It is not part of the public game-authoring API.

Package/game-facing values use different names:

| Name | Owner | Purpose |
|---|---|---|
| Platform knob | PeepOS Platform | internal firmware/platform tuning |
| development tuning overlay | PeepOS developer workflow | temporary override of approved live-safe Platform knobs |
| target profile | Platform/Engine | read-only published capability and limit set for package validation |
| content parameter | package/game author | package-authored balancing or content behavior value |
| package setting | package/user through Engine APIs | package-local user/runtime preference |
| capability request | package runtime through Engine APIs | bounded request for hardware-backed behavior |

Regular game-authoring tools must not expose Platform knobs as editable controls. They may read target profiles, edit package content parameters, edit package settings, and issue bounded capability requests.

---

## Pipeline (Authoritative)

```
config/knobs.json
  -> tools/gen_knobs.py
  -> Core/Inc/knobs_autogen.h
```

Rules:
- firmware includes only generated header
- firmware never parses JSON at runtime
- Platform knob changes require regenerate plus rebuild

This is the authoritative compile-time path.

Live tuning, where allowed, is a developer-only extension defined by [[Live_Tuning_And_Knobs_Contract]] and exposed through [[USB_Development_Mode_Contract]] or the digital twin. It must use generated metadata and owner-routed apply requests, not raw memory pokes.

---

## Required Files

- `config/knobs.json` source of truth
- `config/knobs.schema.json` validation and editor metadata
- `tools/gen_knobs.py` generator
- `Core/Inc/knobs_autogen.h` generated output

Generated header is never manually edited.

---

## Platform Knob Schema Model

Every Platform knob entry should carry enough metadata for code generation, validation, live-safe developer tooling, and evidence traceability.

Conceptual schema:

```text
platform_knob:
  id
  path
  owner
  class
  type
  default_value
  min
  max
  enum_values
  units
  timebase_domain
  build_profile_gate
  target_profile_gate
  live_editable
  apply_policy
  persistence_policy
  reboot_required
  evidence_relevance
  description
  safety_notes
```

Required fields:

| Field | Meaning |
|---|---|
| `id` | stable generated identifier |
| `path` | stable human-readable path, for example `platform.knobs.input.button_debounce_ms` |
| `owner` | Platform subsystem that validates and consumes the value |
| `class` | one of the tuning classes below |
| `type` | explicit scalar/enum/array type |
| `default_value` | generated firmware default |
| `units` | physical or logical units where applicable |
| `timebase_domain` | required for timing knobs |
| `build_profile_gate` | allowed build profiles |
| `target_profile_gate` | allowed target profiles |
| `live_editable` | true only for `runtime_live_safe` values |
| `apply_policy` | safe boundary for live apply |
| `persistence_policy` | session-only, staged, source-only, or rebuild-required |

Rules:

- paths must start with `platform.knobs.`.
- package tools must not use Platform knob paths.
- numeric knobs must declare bounds.
- enum knobs must declare allowed values.
- timing knobs must declare a timebase domain.
- live-editable knobs must name an owner and apply policy.
- `protected_policy` knobs must document safety notes.
- generated metadata may be exposed to PeepOS developer tools but not normal game-authoring tools.

---

## Namespaces

Recommended top-level namespaces:

| Namespace | Owner Area |
|---|---|
| `platform.knobs.power.*` | sleep, wake, PMIC, battery, timeout policy |
| `platform.knobs.display.*` | display owner policy, cadence caps, transfer policy |
| `platform.knobs.render.*` | Platform renderer defaults and diagnostics |
| `platform.knobs.input.*` | debounce, repeat, classifier policy |
| `platform.knobs.sensor.*` | Platform sensor owner policy and filtering |
| `platform.knobs.audio.*` | mixer, BBB, output policy |
| `platform.knobs.storage.*` | owner policy, retries, staging limits |
| `platform.knobs.comm.*` | communication owner policy |
| `platform.knobs.debug.*` | telemetry, trace, fault capture, evidence builds |
| `platform.knobs.memory.*` | static budget values and generated resource limits |

Rules:

- namespaces describe Platform owner areas, not hardware part names.
- avoid leaking implementation details into package-facing names.
- content parameters must use `package.content.*`, not `platform.knobs.*`.

---

## Timebase Domain Contract

All Platform timing knobs must declare:
- authored domain
- runtime compare domain

Allowed domains:
- `threadx`
- `hal_ms`
- `knob_rtos_tick_hz`

Any conversion must happen once at an explicit boundary.

---

## Adding a New Platform Knob

1. add key to `config/knobs.json`
2. add schema entry with type and constraints
3. regenerate header
4. consume generated macro in firmware
5. document effect and owner subsystem

---

## Platform Knob Hygiene Rules

- no duplicated tunables for same behavior
- no stale/unused knob keys
- names must include subsystem context
- tuning knobs must not bypass ownership boundaries
- Platform knob names must not be used as package/content API names

---

## Visibility And Authority

Platform knobs are private to PeepOS Platform development.

Rules:

- Platform knobs live in the Platform knob schema and generated firmware outputs.
- Platform knobs are not authored by package developers.
- Platform knobs are not exposed by normal game-authoring tools.
- game-authoring tools may consume read-only target profile data derived from measured Platform behavior.
- package balancing values must be modeled as content parameters, not Platform knobs.
- package preferences must be modeled as package settings, not Platform knobs.
- hardware-affecting behavior must be expressed as bounded capability requests, not settings or knob writes.

---

## Tuning Classes

Detailed live tuning behavior is defined in [[Live_Tuning_And_Knobs_Contract]].

Every Platform knob should declare a tuning class.

| Class | Meaning | Live Editable |
|---|---|---|
| `compile_time` | memory layout, stack/queue depth, object counts, static hardware configuration | no |
| `boot_applied` | defaults loaded or applied during boot/init | no, or reboot-required |
| `runtime_live_safe` | value can be changed at a safe runtime boundary through owner validation | yes in developer mode |
| `protected_policy` | power, storage, PMIC, safety, or architecture policy with high risk | no, except explicit bring-up firmware policy |

Rules:

- only `runtime_live_safe` Platform knobs may be changed through developer CDC live tuning.
- live-editable Platform knobs must declare owner subsystem, type, bounds, apply timing, persistence behavior, and reset/reboot requirement.
- owner subsystem validates, clamps, rejects, or applies live changes.
- compile-time and protected-policy knobs must not be exposed as normal live controls.

Content parameters may be previewed by development tools, but they are package data. They do not live in `config/knobs.json` and they do not mutate Platform behavior.

---

## Determinism Rules

Platform knobs must not:
- add random timing behavior
- introduce unbounded retries
- silently change architectural contracts

---

## Version and Traceability

Track a knob-set stamp for each bring-up and validation run:
- firmware commit
- knobs hash or version
- board revision

Store stamps in validation evidence records.
