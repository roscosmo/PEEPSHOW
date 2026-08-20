# Content Parameter Schema Contract

This document defines package-authored content parameters.

Content parameters are the game/package side of tuning. They are used for balancing, authored behavior, animation timing, encounter weights, package-local defaults, and other package-owned values.

Content parameters are not Platform knobs and must not mutate Platform behavior.

Related:

- [[Game_Authoring_API_Contract]]
- [[Package_Contract]]
- [[Package_Save_Settings_API_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Target_Profile_Schema_Contract]]
- [[Live_Tuning_And_Knobs_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]

---

## Boundary

Content parameters may control:

- package balancing
- template editable slots
- Authoring Kit customization values
- prefab local defaults
- authored state/scene behavior
- package-local timing
- animation timing
- encounter and table weights
- dialogue pacing
- package-local difficulty defaults
- scene-local constants

Content parameters must not control:

- Platform knobs
- Platform settings
- sleep policy
- display transfer policy
- dirty-region behavior
- hardware sensor modes directly
- audio hardware state directly
- storage wear policy
- BLE/NINA/communication hardware policy
- RTOS objects, stack sizes, queues, clocks, DMA, or memory layout

Hardware-facing needs must be represented as target-profile validation and bounded capability requests.

---

## Schema Shape

Conceptual schema:

```text
content_parameters:
  schema_id
  schema_version
  package_id
  target_profile_ref
  groups[]
  parameters[]
  validation_rules[]
  export_policy
```

Parameter shape:

```text
content_parameter:
  id
  path
  group
  value_type
  default_value
  allowed_values
  min
  max
  units
  precision
  scene_scope
  target_profile_gate
  package_setting_binding
  save_binding
  rebuild_required
  description
  authoring_ui_hint
```

Rules:

- `path` is stable inside the package, for example `package.content.pet.hunger_decay_rate`.
- `id` is stable for compiled package references.
- parameter defaults must validate against the schema.
- parameter values must be serializable into package data or package-owned settings.
- parameters must not reference Platform knob paths.

---

## Allowed Types

Initial value types:

| Type | Use |
|---|---|
| `bool` | feature flag or local behavior toggle |
| `int` | bounded integer count, weight, step, or index |
| `fixed` | deterministic fractional value, stored as fixed-point |
| `enum` | symbolic option |
| `duration_ms` | package-local timing value |
| `rate_per_period` | bounded decay/spawn/progression rate |
| `weight_table` | bounded weighted choice table |
| `curve_table` | bounded authored curve with fixed sample count |
| `id_ref` | reference to package-local asset/state/string/table ID |
| `small_struct` | schema-defined bounded structured value |

Rules:

- floating point values should be authored as `fixed` or converted to deterministic fixed-point during packaging.
- `weight_table` totals must be bounded and deterministic.
- `id_ref` values must resolve during package validation.
- `small_struct` requires explicit max byte size and field schema.
- arbitrary JSON blobs are not content parameters.

---

## Scope And Binding

Content parameters may be scoped to:

- whole package
- scene
- template instance
- Authoring Kit instance
- prefab instance
- state graph
- scene
- asset table
- animation table
- encounter table
- dialogue table
- reference-game content area

Optional bindings:

| Binding | Meaning |
|---|---|
| `package_data` | compiled into immutable package data |
| `package_setting_default` | provides default for a package-owned setting |
| `save_record_default` | provides default for a package-owned save field |
| `digital_twin_override` | may be overridden in twin/dev preview only |

Rules:

- package settings are user/runtime preferences; content parameters are authored defaults/data.
- if a value must change persistently at runtime, it becomes package save/settings data.
- development previews may temporarily override content parameters, but exported packages must compile explicit values.

---

## Authoring Tool Behavior

Authoring tools may expose content parameters as:

- numeric fields
- sliders
- toggles
- enum selectors
- table editors
- curve editors
- asset/state/string reference pickers

Tools must:

- validate ranges before export
- validate template slot values
- validate Authoring Kit and prefab parameter bindings
- validate target profile gates
- validate referenced IDs
- show whether a parameter compiles into package data or a package setting default
- keep Platform knobs hidden
- keep target profiles read-only

Tools must not:

- expose `platform.knobs.*`
- write `config/knobs.json`
- emit direct Platform API calls from content parameters
- infer hardware behavior from content parameter names

---

## Digital Twin Preview

The digital twin may preview content parameter changes.

Rules:

- preview overrides are session-local unless exported through package tooling.
- replay captures must record active content parameter overrides.
- content parameter overrides are package/Engine evidence only.
- content parameter preview must still pass target profile validation.
- preview cannot grant capabilities that the target profile blocks.

---

## Package Export

Package export must include:

- content parameter schema version
- resolved parameter values
- parameter hash
- target profile used for validation
- compatibility report

Rules:

- unresolved parameters block export.
- out-of-range parameters block export or are clamped only when schema explicitly allows clamping.
- target-profile-incompatible parameters block export.
- removed or renamed parameters require schema migration or explicit reset behavior when bound to saves/settings.

---

## Examples

```text
package.content.pet.hunger_decay_rate
package.content.pet.energy_recovery_rate
package.content.template.virtual_pet.evolution_threshold
package.content.kit.shop.default_price_multiplier
package.content.prefab.merchant.dialogue_ref
package.content.animation.idle_frame_ms
package.content.encounters.slime_spawn_weight
package.content.dialogue.typewriter_delay_ms
package.content.puzzle.move_limit_default
```

These are package values.

They must not become:

```text
platform.knobs.power.static_timeout_ms
platform.knobs.display.stop_max_fps
platform.knobs.sensor.imu_owner_poll_ms
```

---

## Validation Cases

1. parameter outside declared range fails package validation.
2. parameter referencing missing asset/state/string ID fails validation.
3. package export records content parameter hash and schema version.
4. content parameter cannot reference `platform.knobs.*`.
5. content parameter requiring blocked target profile capability fails validation.
6. digital twin replay records active content parameter overrides.
7. runtime-persistent value is represented as package save/settings data, not a Platform knob.

---

## Rule

Content parameters are package-owned authored values.

They are the safe game-development tuning surface, while Platform knobs remain PeepOS-internal.
