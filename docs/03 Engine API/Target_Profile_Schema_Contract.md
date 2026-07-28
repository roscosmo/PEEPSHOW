# Target Profile Schema Contract

This document defines the schema for PeepOS target profiles.

A target profile is the read-only bridge between measured Platform behavior and package/game tooling. It tells tools what capabilities, limits, cadence rules, and compatibility constraints exist for a specific target.

Target profiles are not Platform knobs. Package tools may read them for validation, but may not edit them.

Related:

- [[PeepOS_Capability_Registry]]
- [[Game_Authoring_API_Contract]]
- [[Package_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[HW6_Brought_Up_Tracker]]
- [[Validation_Plan]]
- [[Power_and_Sleep_Policy]]
- [[Power_Measurement_and_Trace_Correlation_Runbook]]
- [[Display_and_Rendering_Contract]]
- [[Sensor_API_Contract]]
- [[Audio_API_Contract]]
- [[Communication_API_Contract]]

---

## Purpose

Target profiles allow tools to answer:

- which runtime classes are available
- which Engine-visible capabilities are granted
- which capabilities are optional or blocked
- what display, cadence, memory, audio, input, sensor, communication, save, and diagnostics limits apply
- whether a package is compatible with a target
- whether a feature is provisional, measured, simulated, or unavailable

They must not expose pins, registers, DMA channels, clocks, RTOS objects, HAL names, filesystem paths, flash offsets, or Platform knobs.

---

## Required Initial Profiles

| Profile | Authority | Shipping Use | Purpose |
|---|---|---|---|
| `HW6_PENDING_VALIDATION` | HW6 contracts, imported IOC, and provisional assumptions | no | active design-time profile before measured HW6 evidence |
| `HW6_VALIDATED_BASELINE` | measured HW6 evidence | yes after freeze | future normal HW6 profile without continued waiting-visual animation support |
| `HW6_VALIDATED_LPBAM` | measured HW6 evidence | yes after freeze | future HW6 profile with the revalidated LPBAM waiting-visual animation backend |
| `HOST_AUTHORING_PREVIEW` | tooling model | no | editor preview with mocks/placeholders and compatibility warnings |
| `HOST_DIGITAL_TWIN_HW6` | measured HW6 profile mirrored by twin | no hardware authority | future digital twin profile derived from validated HW6 behavior |
| `HW5_VALIDATED_BASELINE` | retired measured HW5 evidence | no new shipping use | historical package compatibility only |
| `HW5_VALIDATED_LPBAM` | retired measured HW5 evidence | no new shipping use | historical package compatibility only |
| `HOST_DIGITAL_TWIN_HW5` | retired measured HW5 profile | no hardware authority | historical twin compatibility only |

Rules:

- `HW6_PENDING_VALIDATION` may unblock tool design but must not be shipping authority.
- hardware-derived HW6 profiles require evidence links in [[HW6_Brought_Up_Tracker]].
- `HOST_DIGITAL_TWIN_HW6` must be derived from a measured HW6 profile, not invented by host tooling.
- retired HW5 profiles remain immutable historical inputs and do not grant HW6 behavior.
- if measured waiting-visual backend evidence is missing, `display.waiting_visual_animation` remains unavailable for shipping packages.

---

## Schema Shape

Conceptual schema:

```text
target_profile:
  profile_id
  profile_version
  profile_status
  profile_family
  source_authority
  platform_contract_revision
  engine_contract_revision
  hardware_revision
  firmware_commit
  knobs_hash
  evidence_refs[]
  generated_at

  capabilities[]
  runtime
  logic
  display
  rendering
  power
  input
  sensors
  audio
  communication
  time
  save_storage
  diagnostics
  package_limits
  compatibility_rules[]
```

Required profile statuses:

| Status | Meaning |
|---|---|
| `pending_validation` | intended behavior, no target-qualified evidence yet |
| `hw_validated` | measured on the named hardware target and linked to evidence |
| `host_preview` | editor/simulator preview profile |
| `host_twin` | host profile derived from measured hardware behavior |
| `deprecated` | profile exists for compatibility reports only |

---

## Capability Grants

Each capability grant uses canonical names from [[PeepOS_Capability_Registry]].

```text
capability_grant:
  name
  grant_status
  may_be_required_by_package
  fallback_required_if_used
  runtime_classes[]
  constraints_ref
  evidence_ref
  notes
```

Allowed grant statuses:

| Status | Meaning |
|---|---|
| `granted` | package may require the capability in this profile |
| `optional` | package may use it only with validated fallback behavior |
| `blocked` | package must not require or use it in this profile |
| `experimental` | dev/preview only; not shipping-authoritative |
| `pending_validation` | tools may model it but must warn and prevent shipping reliance |

Rules:

- required package capabilities must resolve to `granted`.
- optional capabilities require package fallback behavior.
- `pending_validation` capabilities must fail shipping package export.
- profile validation must report every blocked or degraded capability used by a package.

---

## Runtime And Logic Fields

Required runtime fields:

```text
runtime:
  classes[]              # LP_GRAPH, LP_MODULE, RT_SCENE, plus Platform-owned SHELL/INSTALLER where relevant
  default_package_class
  allowed_transitions[]
  reactive_wait_required
  runtime_units_max
  runtime_unit_nesting_depth_max
  runtime_transition_chain_max
```

Required logic fields:

```text
logic:
  state_count_max
  transition_count_max
  guard_count_max
  action_count_max
  action_steps_per_event_max
  local_event_queue_depth_max
  calendar_schedule_count_max
  catch_up_events_per_resume_max
  unbounded_loops_allowed = false
```

Rules:

- `RT_SCENE` units must declare frame budget, meaningful-activity rules, suspend behavior, resume behavior, and a reactive fallback route.
- `LP_GRAPH` must not request high-frequency polling.
- `LP_MODULE` must declare an approved module type.
- `reactive_wait_required` means every state/block that can settle must resolve an event/schedule/waiting-visual contract so the host can yield without an awake input-wait loop.
- `runtime_unit_nesting_depth_max` limits how many package runtime units may be nested or suspended behind each other.
- `runtime_transition_chain_max` limits how many state/runtime transitions may execute from one event before the runtime must yield or report validation failure.
- neither field describes ThreadX stack memory.

---

## Display And Rendering Fields

Required display fields:

```text
display:
  logical_surface:
    width
    height
    logical_pixel_model    # mono_1bpp
    orientation
  native_panel_diagnostics:
    visible_to_package_tools = false
    width
    height
    native_pixel_model      # panel_native_1bpp
  static_hold_supported
  dirty_tracking_internal = true
  waiting_visual_animation:
    grant_status           # blocked, pending_validation, granted, experimental
    authored_frame_count_max
    cadence_hz_max
    cycle_duration_ms_max
    compiler_profile_id
    compiler_admission_required = true
    compatibility_reporting = abstract_utilization
    precomposed_or_tool_resolved_1bpp = true
    evidence_ref
```

Required rendering fields:

```text
rendering:
  layer_order_top_to_bottom[] = [UI, GAME, BG]
  masked_1bpp_supported
  tone5:
    supported
    source_model = 5_color_indexed_png
    runtime_asset_model = tone5_masked
    output_model = deterministic_1bpp_coverage
    coverage_model
    scale_integer_only = true
    integer_scale_max
    deterministic_phase_required = true
  tilemap_viewport_supported
  waiting_visual_sequence_assets:
    supported
    pixel_model = precomposed_1bpp
    final_visual_states_required = true
    continued_motion_requires_display_grant = true
  system_ui_reserved = true
```

Rules:

- package tools must not expose dirty-row controls.
- package tools author against `display.logical_surface`, not `display.native_panel_diagnostics`.
- native panel diagnostics are Platform reporting metadata only. They are hidden from normal package tools and must not expose panel command bytes, row packing, SRAM4 buffers, or transfer policy.
- tone5 is a semantic coverage model, not native display color.
- tone5 source art may be authored as a five-color indexed PNG; tooling converts it into validated `tone5_masked` package assets containing the logical tone data and masks used by the renderer.
- tone5 output must be deterministic 1bpp coverage on every backend, including the digital twin.
- waiting visual sequences must resolve to bounded final 1bpp visual states before target-specific Platform compilation.
- waiting visual sequence assets may exist when continued motion while yielded is blocked; tools then require a reduced sequence, hold fallback, or a profile that grants `display.waiting_visual_animation`.
- `authored_frame_count_max` is a portable authoring bound, not a guarantee that every sequence with that many frames fits the target compiler; admission remains content-dependent.
- `cycle_duration_ms_max` limits one authored visual loop; it does not limit how long a reactive state may remain waiting or how long an admitted loop may repeat.
- `compiler_profile_id` selects a versioned Platform-owned admission model. Normal package tools receive pass/fail, abstract utilization, and fallback results; they do not receive panel-row, transfer-chunk, descriptor, SRAM4, or LPBAM limits.
- system UI is reserved PeepOS behavior for setup, calibration, package management, diagnostics, errors, shipping mode, and related system flows. It is not a package-authored game layer.

---

## Power And Time Fields

Required fields:

```text
power:
  reactive_sleep_immediate = true
  realtime_requires_admitted_work = true
  input_lock:
    package_may_disable = true
    timeout_ms_default
    timeout_ms_min
    timeout_ms_max
    bounded_deferral_ms_max
    unlock_action = START
    unlock_press_consumed = true
    lock_routes[] = [preserve_state, transition_to, exit_to_shell]
    meaningful_activity_sources[]
  communication_wake_supported
  interactive_session_wait:
    supported
    awake_grace_ms_max
    remote_activity_refresh_supported
  wake_intents_supported[]
  lifecycle_wake_reasons[]
  latency_classes[]
  cadence:
    reactive_scheduled_event_hz_max
    reactive_input_response_latency_ms_max
    realtime_target_fps
    realtime_frame_budget_ms
  estimates:
    supported
    source = measured_platform_profile
    confidence_class
    exposes_raw_measurements = false
    evidence_refs[]

time:
  calendar_read_supported
  calendar_set_by_package_allowed = false
  delayed_event_supported
  delayed_events_max
  calendar_schedule_supported
  calendar_schedules_max
  rtc_wake_intent_supported
  missed_event_policy
  missed_event_catchup_max
```

Rules:

- packages may read valid PeepOS calendar time where granted
- packages may not set RTC/calendar time
- reactive runtimes yield immediately after bounded event work settles
- packages may enable or disable automatic input locking; target profiles do not override this invariant
- enabled lock policy uses the active target/system timeout, must fit profile deferral bounds, and must use an admitted lock route
- unlock Start is consumed; Engine emits `DEVICE_UNLOCKED` instead of delivering a package Start action
- meaningful activity must come from admitted declared sources; cosmetic animation cannot refresh the lock timer
- package gameplay inactivity is a normal schedule/transition and does not alter system lock policy
- `RT_SCENE` requires a reactive fallback and bounded deferral declarations where used
- static/one-shot display terminology does not define CPU residency
- baseline reactive waits that require MCU wake/update/return are modeled separately from waiting visuals that continue autonomously
- power estimates are derived from measured Platform profiles and are advisory unless a later contract makes them normative
- reactive/realtime clock and voltage operating points remain internal Platform facts; profiles publish only derived latency, cadence, workload, compatibility, and estimate limits
- a package cannot require a literal operating point, and changing the internal Platform point must not change Engine semantic behavior
- package tools may consume estimate summaries and compatibility warnings, but not raw power traces or Platform operation-cost tables
- package wake behavior uses `wake_intents_supported[]`; hardware wake details remain Platform/HW documentation
- delayed/calendar event limits are package schedule limits, not direct RTC alarm ownership
- communication wake is blocked for HW6 profiles unless a future measured HW6 profile grants it

---

## Input, Sensor, Audio, And Communication Fields

Required input fields:

```text
input:
  package_inputs[]
  dev_only_inputs[]
  system_override_actions[]
  encoder_supported
  joystick_vector_supported
  joystick_direction_supported
  chords_supported
  hold_repeat_supported
  low_power_wake_intents[]
```

Required sensor fields:

```text
sensors:
  light_supported
  imu_supported
  sensor_contexts[]:
    context_id
    capability
    runtime_classes[]
    power_class
    grant_status
    may_be_required_by_package
    fallback_required_if_used
    sample_rate_hz_min
    sample_rate_hz_max
    event_rate_hz_max
    wake_capable
    continuous_in_sleep
    mcu_wake_required
    duration_ms_max
    evidence_ref
```

Required audio fields:

```text
audio:
  music_supported
  sfx_supported
  bbb_supported
  timeline_supported
  voice_count_max
  sample_rate_hz
  muted_output_valid = true
```

Required communication fields:

```text
communication:
  multiplayer_supported
  companion_supported
  local_loopback_supported
  session_required_units_supported
  message_payload_bytes_max
  message_rate_max
  wake_supported = false
```

Rules:

- every HW6 profile sets `input.encoder_supported = false` and blocks `input.encoder`
- every HW6 profile sets `sensors.light_supported = false` and blocks `sensor.light` and `sensor.light_stream`
- every HW6 profile sets `audio.bbb_supported = false` and blocks `audio.bbb`
- absent HW6 hardware cannot be restored by package fallback, host preview behavior, or inherited HW5 evidence

- `package_inputs[]` lists normal logical package inputs, including Start where the target profile grants it.
- `package_inputs[]` entries are logical names, not GPIO, EXTI, pin, or board-signal names.
- `dev_only_inputs[]` lists abstract inputs available only on development hardware/profiles. Package behavior using these inputs must be stripped, nulled, or rejected on non-dev profiles.
- a physical development-only boot/debug signal may map to an abstract dev-only input, but the target profile must not expose the physical pin or boot-mode detail to normal package tools.
- `system_override_actions[]` lists Platform-owned override paths that can preempt normal input routing, such as shipping-mode entry, recovery, installer entry, or dev-mode entry.
- shipping-mode intent is a Platform override path, not the same thing as a development boot/debug input.
- Start is a normal package input unless Platform has already entered a system override path.
- package-visible sensor behavior is normalized; hardware faults are Platform/Engine diagnostics.
- sensor limits are exposed as measured PeepOS sensor contexts, not flat sensor-wide rate limits.
- sensor context IDs are abstract PeepOS names, not hardware part, register, interrupt, ADC, I2C, or pin names.
- `sample_rate_hz_*` describes Platform sampling cadence; `event_rate_hz_max` describes package-visible event delivery.
- `wake_capable`, `continuous_in_sleep`, and `mcu_wake_required` must be measured or explicitly marked pending before shipping use.
- high-duty sensor contexts must declare runtime classes and bounded duration.
- audio-centric packages are allowed; mute is user/platform policy, not package validation failure.
- communication sessions are abstract and must not expose BLE/NINA/UART terms to packages.

---

## Save, Diagnostics, And Package Limits

Required fields:

```text
save_storage:
  save_records_supported
  package_settings_supported
  record_bytes_max
  package_save_bytes_max
  package_settings_bytes_max
  writes_per_period_max
  write_period_ms
  write_on_suspend_supported
  transactional_write_supported
  preserve_previous_valid_record = true
  schema_migration_supported
  write_failure_result_required = true
  package_visible_results[]       # success, deferred, rejected_budget, rejected_schema, unavailable, failed_preserved

diagnostics:
  package_markers_supported
  package_counters_supported
  timing_scopes_supported
  trace_values_supported
  package_fault_codes_supported
  shipping_minimal_faults_supported
  event_rate_max
  payload_bytes_max
  marker_count_max
  counter_count_max
  timing_scope_count_max
  trace_value_count_max

package_limits:
  package_bytes_max
  asset_bytes_max
  runtime_ram_bytes_max
  runtime_unit_count_max
  save_settings_bytes_max
  diagnostics_table_bytes_max
  content_parameter_count_max
  content_parameter_blob_bytes_max
  string_table_bytes_max
  waiting_visual_sequence_asset_bytes_max
```

Rules:

- save/settings limits describe Engine APIs, not filesystem access, flash offsets, erase pages, raw storage regions, or Platform settings.
- package save records and package-owned settings are schema-versioned records, not files.
- package-owned settings may influence package logic only. They must not mutate PeepOS knobs, Platform settings, calibration, BLE bonding, install metadata, power policy, or hardware policy.
- save writes may be deferred, clamped, rejected by budget, rejected by schema, unavailable, or failed while preserving the previous valid record.
- save write failure is a package-visible persistence result that package logic and tools must model. The underlying storage or hardware fault remains Platform diagnostics.
- package tools must validate save schemas, defaults, migration policy, write policy, and fallback behavior before export.
- package diagnostics are bounded records and do not own debug transports, dashboard export, Tracealyzer/SWO, USB CDC, BLE, UART, protected storage, or fault-log storage.
- shipping diagnostics must be minimal and explicitly profile-gated.
- package limits are target-profile abstractions, not memory-map facts. They must not expose SRAM bank names, linker sections, flash offsets, raw heap regions, or DMA buffer addresses.
- profile package limits must be enforced before package compilation/export.

---

## Evidence And Change Control

Hardware-derived target profiles must record:

- hardware target
- board revision
- board ID/serial where available
- assembly/rework state
- firmware commit
- Platform contract revision
- knobs hash/version
- evidence artifact IDs
- validation cases covered
- date/time and maintainer

Profile changes require:

1. update profile source data
2. update compatibility validation expectations
3. update digital twin profile import if host behavior changes
4. link measured evidence for hardware-derived changes
5. revalidate packages that depend on changed limits or grants

---

## Validation Cases

1. package requiring a blocked capability fails validation.
2. package using optional capability without fallback fails validation.
3. shipping export fails against `HW6_PENDING_VALIDATION`.
4. `display.waiting_visual_animation` is unavailable unless profile evidence grants it.
5. target profile contains no HAL, pin, DMA, register, RTOS object, raw filesystem, flash-offset, or Platform knob names.
6. `HOST_DIGITAL_TWIN_HW6` cannot be generated before its source HW6 profile has evidence.
7. changing profile limits invalidates stale compatibility reports.
8. package tools can read target profiles but cannot edit them.
9. dev-only inputs are stripped, nulled, or rejected on non-dev target profiles.
10. system override actions are not delivered as normal package input after Platform override handling begins.
11. package rendering validation uses `display.logical_surface`; `display.native_panel_diagnostics` is not package-authorable.
12. tone5 source art is valid only after tooling converts it to deterministic `tone5_masked` package assets.
13. `rendering.waiting_visual_sequence_assets.supported` does not imply continued motion while yielded; that still requires `display.waiting_visual_animation.grant_status = granted`.
14. package save/settings validation rejects records that exceed schema, size, migration, or write-budget limits before export.
15. package-visible write failure uses bounded persistence results; underlying storage faults remain Platform diagnostics.
16. package diagnostics cannot request debug transports, protected storage, or dashboard export ownership.
17. package limits expose abstract compatibility budgets, not SRAM banks, linker sections, flash offsets, heap regions, or DMA buffers.
18. interactive communication wait validates against profile peer-wait support and grace limits without granting communication wake.
19. package-authored input-lock timeout or unlock action fails validation.
20. every HW6 profile blocks `input.encoder`, `sensor.light`, `sensor.light_stream`, and `audio.bbb` regardless of inherited HW5 profile data.

---

## Rule

Target profiles publish Platform capability and limit facts to Engine/package tools.

They are read-only from game tooling and evidence-backed when hardware-derived.
