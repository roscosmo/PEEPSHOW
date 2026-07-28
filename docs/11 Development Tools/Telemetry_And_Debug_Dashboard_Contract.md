# Telemetry and Debug Dashboard Contract

This document defines the structured telemetry model and host dashboard expectations for PeepShow development.

The dashboard is a development tool. It observes PeepOS state, decodes structured events, displays timelines/panels, and may invoke approved developer commands. It must not become a hidden Platform control plane or package/game API.

Related:

- [[Development_Tooling_Index]]
- [[Debug_and_Observability]]
- [[Debug_Workflows]]
- [[USB_Development_Mode_Contract]]
- [[Live_Tuning_And_Knobs_Contract]]
- [[Tracealyzer_Snapshot_Evidence_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Diagnostics_API_Contract]]
- [[Runtime_Host_Contract]]
- [[Brought_Up_Tracker]]
- [[Validation_Plan]]

---

## Scope

Defines:

- structured telemetry event model
- telemetry source paths
- dashboard panel expectations
- capture/export artifact rules
- rate limits and build-profile gates
- dashboard control boundary
- evidence requirements

Does not define:

- exact GUI framework
- exact binary packet layout
- exact SWO or CDC driver implementation
- Tracealyzer file format
- package-facing diagnostics API internals

---

## Core Principle

Telemetry is structured state evidence, not arbitrary logging.

```text
Platform / Engine / package diagnostics / digital twin
    |
structured telemetry events
    |
SWO, CDC dev mode, capture export, or twin adapter
    |
host decoder
    |
dashboard timeline and panels
```

Rules:

- event schemas are versioned.
- payloads are bounded.
- event IDs are stable.
- high-rate streams are filterable.
- protected data is never exposed.
- control commands use documented dev protocols only.

---

## Source Paths

Telemetry may come from several sources.

| Source | Direction | Purpose |
|---|---|---|
| SWO structured events | device to host | low-overhead observation during bring-up/debug |
| USB CDC developer telemetry | bidirectional dev mode | state query, subscriptions, captures, live-safe control responses |
| staged export artifact | device to host after copy/export | bounded logs, screenshots, summaries, captured telemetry files |
| Tracealyzer snapshot | debugger capture | RTOS scheduling and owner-thread ordering evidence |
| digital twin telemetry | host internal | deterministic package/runtime replay and dashboard development |

Rules:

- SWO is observation-first.
- CDC is the structured developer control and telemetry path, only in developer USB personality.
- MSC staging/export may carry copied artifacts but is not a live telemetry transport.
- Tracealyzer snapshots complement dashboard telemetry; they do not replace Platform/Engine state events.
- the digital twin should emit the same dashboard-facing event schemas where practical.

---

## Event Envelope

Every telemetry event should fit a common conceptual envelope.

```text
telemetry_event:
  schema_version
  event_id
  event_class
  source
  sequence
  timestamp
  timebase_domain
  severity
  build_profile
  target_profile
  payload
```

Required properties:

- `event_id` is stable and machine-readable.
- `event_class` groups related events.
- `source` identifies Platform owner, Engine service, runtime host, package diagnostics, or digital twin component.
- `sequence` is monotonic per source or per transport where possible.
- `timestamp` declares a timebase domain.
- `payload` is schema-bounded.

Allowed timebase domains:

- `threadx`
- `hal_ms`
- `rtc_calendar`
- `trace_timestamp`
- `host_twin`
- `wall_clock_host`

Rules:

- payloads must not contain raw pointers, raw memory dumps, protected storage addresses, hardware register dumps, arbitrary filesystem paths, or package private data.
- string payloads use stable IDs or bounded static strings.
- event loss must be detectable where practical through sequence gaps or overflow markers.

---

## Core Event Classes

The dashboard should understand these event classes.

| Event Class | Purpose |
|---|---|
| `system.state_vector` | current public Platform/Engine state summary |
| `mode.transition` | system and runtime mode changes |
| `owner.state` | Platform owner FSM state transition |
| `owner.health` | owner alive/fault/degraded status |
| `queue.status` | queue depth, overflow, rejected request, timeout |
| `power.intent` | Engine/runtime power intent publication |
| `sleep.quiesce` | sleep preparation and owner acknowledgement |
| `sleep.enter` | attempted sleep entry |
| `wake.reason` | normalized wake cause and resume route |
| `runtime.lifecycle` | runtime host mount/start/suspend/resume/stop/unmount |
| `runtime.logic` | state graph/unit transition and budget events |
| `display.frame` | frame present request/result, checksum, dirty summary |
| `display.flush` | display transfer begin/end/fault summary |
| `input.event` | logical input/action summary |
| `sensor.summary` | normalized sensor event, snapshot, or health summary |
| `audio.status` | cue/BBB request, queue, underrun, suppression summary |
| `storage.owner` | storage mount/export/reclaim/install transitions |
| `usb.personality` | MSC or CDC personality selection and state |
| `communication.session` | abstract comm session state and message summary |
| `package.install` | package stage/validate/commit/rollback |
| `package.diagnostic` | bounded package diagnostics through [[Diagnostics_API_Contract]] |
| `live_tuning` | overlay, apply, clamp, reject, persist events |
| `capture.marker` | start/stop/checkpoint for capture artifacts |
| `fault.path` | fault classification and safe route |

Rules:

- event classes are development-facing, not package APIs.
- package diagnostics may be included only after Engine validation and rate limiting.
- hardware faults are Platform events, not package events.
- high-rate event classes must support filtering and sampling.

---

## State Vector

The dashboard should be able to show the current contract-visible state vector.

Conceptual state:

```text
peepos_state_vector:
  system_mode
  power_state
  sleep_class
  runtime_class
  runtime_unit
  runtime_lifecycle_state
  display_state
  audio_state
  input_state
  sensor_state
  storage_state
  usb_personality
  communication_state
  wake_reason
  active_capabilities
  clamped_requests
  active_tuning_overlay
  fault_summary
```

Rules:

- fields must use public state identifiers from Platform/Engine contracts.
- private owner internals may be summarized only through approved public states.
- the digital twin and hardware backend should expose the same state vector fields where practical.

---

## Dashboard Panels

The v1 dashboard should be designed around panels, not freeform logs.

Expected panels:

| Panel | Purpose |
|---|---|
| State Vector | current Platform/Engine mode, runtime, owner health, USB personality |
| Timeline | ordered telemetry, package diagnostics, and capture markers |
| Power/Wake | quiesce, sleep entry, wake reason, resume route, cadence clamps |
| Owner Threads | owner FSM states, queue summaries, health/fault status |
| Display | frame/flush timing, dirty rows/regions, checksum, waiting-visual/compiled-slice state |
| Runtime | runtime class/unit, lifecycle, logic state, frame/budget warnings |
| Input | logical actions, focus scope, wake input path |
| Sensors | normalized sensor events/snapshots, context state, health |
| Audio | active cues, BBB patterns, queue/decode status, suppression/faults |
| Storage/USB | storage owner FSM, install flow, MSC/CDC personality, staging state |
| Communication | abstract session state, peers, message counts, timeouts |
| Live Tuning | active Platform knob overlay, live-safe Platform knobs, applied/clamped/rejected results |
| Virtual Display | latest frame, screenshot, or frame checksum where available |
| Evidence | capture metadata, artifact paths, test case, pass/fail notes |

Rules:

- panels may be hidden or unavailable when the active source does not provide relevant events.
- dashboard UI must distinguish live hardware, captured hardware, and digital twin sources.
- dashboard must label trace-enabled/instrumented runs.

---

## Transport Policies

### SWO

SWO is preferred for low-overhead structured observation.

Rules:

- event encoding should be compact.
- high-rate event classes must be filterable.
- SWO loss/overflow should be visible through counters or markers where practical.
- SWO must not be required for normal product use.

### CDC Developer Mode

CDC telemetry is available only in developer USB personality.

Rules:

- CDC subscriptions are explicit.
- command responses and telemetry share the same structured schema discipline.
- CDC telemetry must be rate-limited and disableable.
- CDC must not run concurrently with MSC in v1.

### Staged Export

Captured telemetry may be copied into staging/export storage by firmware.

Rules:

- protected logs are copied as bounded summaries.
- internal protected storage is never directly exposed.
- exports must include metadata and schema versions.

### Digital Twin

The digital twin should emit the same telemetry schema for package/runtime behavior.

Rules:

- twin telemetry is deterministic for a fixed replay input.
- twin telemetry is not hardware bring-up evidence.
- dashboards must clearly label twin source data.

---

## Capture Artifacts

Telemetry captures should be portable artifacts.

Conceptual capture bundle:

```text
peep_capture:
  capture_manifest
  telemetry_events
  state_vector_snapshots
  frame_checksums_or_images
  package_diagnostics
  live_tuning_overlay
  source_profile
  schema_versions
```

Manifest fields:

- capture ID
- source type: hardware live, hardware exported, digital twin, Tracealyzer-linked
- board revision
- firmware commit
- knobs hash/version
- package ID/checksum where applicable
- target profile
- build profile
- telemetry schema version
- active live-tuning overlay
- active trace/instrumentation profile
- start/stop reason
- artifact paths

Rules:

- captures must be deterministic to parse.
- schema versions must be included.
- event streams must not depend on host-local absolute paths.
- captured screenshots/frames are optional and bounded.
- captures used as evidence are linked from [[Brought_Up_Tracker]].

---

## Control Boundary

The dashboard may expose developer controls only by calling existing contracts.

Allowed controls:

- start/stop telemetry subscription through [[USB_Development_Mode_Contract]]
- start/stop capture through [[USB_Development_Mode_Contract]]
- apply live-safe Platform tuning through [[Live_Tuning_And_Knobs_Contract]]
- upload package through [[USB_Development_Mode_Contract]]
- run digital twin replay/fault-injection profiles

Disallowed controls:

- raw memory read/write as a normal workflow
- raw flash read/write
- direct filesystem paths into firmware
- direct owner FSM mutation
- HAL/LL/RTOS handle access
- direct sleep, clock, DMA, PMIC, or peripheral control
- bypassing package install validation

Dashboard controls must show accepted, rejected, clamped, staged, failed, or completed status.

---

## Rate Limits And Build Profiles

Telemetry availability is build/profile dependent.

| Profile | Telemetry Behavior |
|---|---|
| bring-up trace | verbose owner and transition events allowed |
| dev firmware | dashboard events, CDC telemetry, live tuning allowed where gated |
| package dev | package diagnostics and runtime events allowed within bounds |
| release candidate | selected diagnostics and evidence events only |
| shipping | minimal fault/state evidence only unless policy changes |

Rules:

- high-rate display/input/sensor/audio events require explicit subscription.
- telemetry must degrade gracefully when a transport cannot keep up.
- release builds must compile out or disable verbose telemetry paths.
- dashboard should show when events are filtered, sampled, dropped, or unavailable.

---

## Evidence Use

Dashboard telemetry can support bring-up and validation evidence when the capture is bounded, versioned, and linked.

Telemetry can prove:

- state transition ordering
- owner request and response flow
- runtime lifecycle behavior
- package diagnostics and package fault routing
- live tuning apply/clamp/reject behavior
- digital twin replay behavior

Telemetry cannot prove by itself:

- current draw
- physical wake latency
- electrical signal integrity
- flash media reliability
- display waveform correctness
- BLE RF/module behavior

Physical claims still require measured evidence from the active hardware target, currently HW6.

---

## Validation Cases

1. host decoder accepts a capture manifest and telemetry stream with matching schema versions.
2. dashboard shows source type clearly: hardware live, hardware exported, or digital twin.
3. state vector panel updates from `system.state_vector` events.
4. owner FSM events appear in timeline and owner panel.
5. sleep/wake scenario shows quiesce, sleep entry, wake reason, and resume route.
6. display scenario shows frame/flush summary and checksum or screenshot where available.
7. CDC telemetry subscription is rejected when not in developer USB personality.
8. dashboard live tuning uses [[Live_Tuning_And_Knobs_Contract]] and reports applied/clamped/rejected status.
9. dashboard package upload uses [[USB_Development_Mode_Contract]] and cannot bypass package validation.
10. captured telemetry used as evidence records firmware commit, board revision, knobs hash, schema versions, source profile, and artifact path.
11. digital twin telemetry replay is deterministic for a fixed replay input.
12. protected storage, raw memory, HAL handles, RTOS objects, and arbitrary filesystem paths are not present in telemetry payloads.

---

## Rule

The dashboard observes structured PeepOS telemetry and invokes documented developer commands.

It does not own Platform state.
