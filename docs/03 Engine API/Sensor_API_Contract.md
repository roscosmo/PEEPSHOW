# Sensor API Contract

This document defines the Engine-facing sensor API for PeepOS packages and game-development tools.

The sensor API exposes stable PeepOS primitives. It does not expose ADC, GPIO, I2C, interrupts, sensor registers, sensor power state, calibration storage, sleep mode, or wake-pin configuration.

Related:

- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
- [[Sensors_Index]]
- [[Light_Sensor_Contract]]
- [[IMU_Contract]]
- [[PMIC_and_Power_Contract]]
- [[Power_and_Sleep_Policy]]
- [[Time_And_Power_Intent_API_Contract]]
- [[Brought_Up_Tracker]]

---

## Purpose

Game packages should treat sensors as PeepOS primitives, not hardware devices.

For a target profile that grants the corresponding capability:

- ambient light is a PeepOS light primitive.
- step counting is a PeepOS step primitive.
- IMU events are PeepOS motion/orientation primitives.
- higher-rate motion input is a PeepOS realtime sensor context where the target profile allows it.

A sensor fault is a Platform health event. It is not normal gameplay logic.

---

## Ownership Boundary

The Platform owns:

- hardware probe and configuration
- ADC, I2C, GPIO, EXTI, and interrupt handling
- sensor power state
- sample cadence implementation
- calibration and filtering
- sensor wake policy
- fault detection and recovery
- degraded-capability reporting

The Engine owns:

- compiler-derived package sensor capability declarations
- scene sensor context admission
- stable game-facing sensor primitives
- tool-time validation rules
- lifecycle behavior when a required sensor primitive cannot be maintained
- developer diagnostics and fault trace presentation

Packages own:

- selection of sensor primitives used by their scenes
- gameplay use of resolved sensor values and events
- optional content fallbacks where the package is intentionally portable across profiles

Packages do not own hardware behavior.

---

## Core Rules

- Game code must not request ADC, GPIO, I2C, register, interrupt, or sensor power behavior.
- Normal gameplay APIs must not expose hardware rejection, register faults, ADC timeouts, I2C errors, or low-level sensor health codes.
- Sensor mode and cadence validity is checked before package compilation/export.
- If a scene declares a sensor context that is valid for the selected target profile, PeepOS is responsible for providing it under nominal hardware conditions.
- If a required sensor primitive fails at runtime, Platform/Engine handles it through fault logging, degraded capability state, and package lifecycle policy.
- Optional sensor behavior exists for content design and target-profile portability, not as routine hardware-failure handling.
- Sensor diagnostics may expose detailed fault information to developer tools and logs, but not as normal package gameplay state.

---

## Target Sensor Assumption

The selected target profile defines which sensor primitives are available for package use. A generic API contract is not a hardware grant.

HW6 retains its IMU path but has no ambient-light sensor. Its target profiles must set `sensors.light_supported = false` and omit `sensor.light` and `sensor.light_stream`. A package that requires light sensing must be rejected for HW6 unless it declares and validates an admitted fallback.

Before target validation completes, capabilities may be `CONTRACTED` but not yet shipping-authoritative. After validation, target behavior must be represented by the measured target profile.

The concept of a sensor being unavailable is still useful for:

- pre-bring-up development profiles
- digital twin fault injection
- targets with different fitted sensor sets
- physical hardware fault diagnosis
- developer diagnostics

It is not a normal game-logic branch.

---

## Capability Model

Canonical names live in [[PeepOS_Capability_Registry]].

Sensor capabilities are abstract PeepOS features:

| Capability | Meaning |
|---|---|
| `sensor.light` | resolved ambient-light value and band |
| `sensor.light_stream` | bounded active light sampling context where supported |
| `sensor.imu_steps` | step session and step count snapshots/deltas |
| `sensor.imu_events` | normalized motion, tap, shake, tilt, or orientation events |
| `sensor.imu_motion_snapshot` | low-rate normalized motion/orientation snapshot |
| `sensor.imu_motion_stream` | bounded higher-rate motion context for realtime gameplay |

Capability names must not include part numbers, pins, registers, addresses, ADC channels, EXTI lines, or HAL names.

---

## Initial HW6 Product Scope

As of 2026-09-09, the next device-input increments are read-only battery SOC,
package-session steps, and stable orientation. These remain contracted and
unexposed; this decision does not enable a target-profile capability or add
an executable event binding. Battery SOC belongs to `power.battery_soc`, not
the IMU; it is included here to keep observation and event semantics aligned.

| Priority | Package-facing behavior | Initial wake policy |
|---|---|---|
| 1. Battery SOC | Valid normalized percentage, then rising/falling threshold events | Use OS power snapshots; no extra package wake source |
| 2. Package-session steps | Session count/delta and bounded milestone events | Preserve admitted counting through STOP2; no per-step MCU wake |
| 3. Stable orientation | Current resolved pose and qualified pose-change events | Observe by default; STOP2 wake requires a separate measured grant |

The larger `sensor.imu_events` capability is not permission to use every
feature of the fitted sensor. Profiles must enumerate the supported event
interests and contexts. Taps of every multiplicity, shake, generic motion
wake, free-fall, activity/inactivity, significant motion, embedded relative
tilt, and continuous tilt/motion streaming are deferred. They must not be
enabled implicitly alongside steps or orientation. Keychain transport is
expected normal behavior, not automatically meaningful user interaction.

### Observation Is Not Wake Permission

- Reading a cached snapshot neither enables a detector nor grants wake.
  Fresh sampling and retained detection require their own admitted context.
- An event interest declares what package logic consumes. A wake intent
  separately asks whether that source may wake the MCU; it does not override
  OS interaction, safety, or package-suspension policy.
- Observation-only contexts may update on existing wake opportunities and
  coalesce to the latest resolved value. They must not promise delivery of
  every physical transition that occurred during sleep. A profile must state
  freshness and maximum delivery latency before such a context is exposed.
- A hardware interrupt that schedules `thSensor` is not yet a qualified
  package event, and a sensor wake does not itself activate a package or
  refresh the interaction timeout.
- Explicit package suspension stops its handlers and withdraws its wake
  interests and unnecessary sampling requests. Other admitted OS consumers
  remain intact. Resume reconciles snapshots without replaying an unbounded
  event history; replacement discards events belonging to the old activation.
- Detector power, interrupt routing, filtering, stability, hysteresis and
  cadence remain Platform-owned. Firmware tuning uses the knobs system;
  tools expose only the logical options admitted by the target profile.

`thSensor` owns IMU work, `thPower` owns fuel-gauge snapshots and wake admission,
and `thRuntime` consumes bounded snapshots/events. Packages never read a
peripheral or reconfigure its interrupt path.

### Battery SOC Semantics

The planned values are `device.battery.soc_percent` and
`device.battery.soc_valid`; the planned binding is
`device.battery.soc_threshold`, as listed by the target profile. These names
remain unavailable until the shared implementation and target proof exist.

- Percentage is a normalized fuel estimate, not an exact remaining-runtime
  promise. Validity reflects the OS freshness/quality policy; an invalid
  reading is not converted to zero or used to evaluate a threshold.
- Threshold events compare consecutive valid, OS-filtered observations in
  the declared rising or falling direction. Initial subscription, resume or
  recovery after an invalid interval establishes a baseline without inventing
  a crossing. Content can inspect the current value for an already-low case.
- Repeated readings on the same side do not repeat the event. OS-owned
  hysteresis controls rearming; numeric policy must be specified and tested
  before exposure. Delivery uses normal power monitoring, not a new package
  polling loop or a claim of hardware SOC-threshold wake.
- Battery health faults, PMIC/charger control, USB/VBUS state and shutdown
  decisions remain OS-only. A gameplay threshold cannot delay or suppress
  protective power behavior.

### Stable Orientation Semantics

The first orientation primitive is a coarse gravity-relative device pose,
not continuous steering, compass heading, or the embedded relative-tilt
algorithm. The six physical faces must be mapped to the assembled device's
screen/body axes and verified on target before fixing author-facing enums.

- Publish a current qualified pose with validity/age. Ambiguous, moving or
  stale data must not be presented as a newly confirmed pose.
- A changed pose must satisfy the admitted stability/dwell and hysteresis
  policy. Emit once for each qualified change; repeated samples in the same
  pose do not retrigger. Initial acquisition supplies a snapshot, not a
  fabricated change event.
- Package logic may select which entered poses matter. Any future
  scene-owned handler remains independent of menu selection/state lifetime;
  orientation must not be implemented as a forced input or focus movement.
- Stable package events do not prove low wake cost. A coarse hardware
  orientation interrupt may wake the MCU before software qualification.
  Wake-capable admission must measure those physical interrupts, including
  rejected candidates, and show that carried keychain motion is acceptable.
- Start with observation and qualified event delivery. Promote orientation
  wake only after validating the retained detector, interrupt clear/rearm,
  STOP2 operation, latency and current consumption. Deferred tap or generic
  motion wake must not be used as an undocumented substitute.

### Exposure And Implementation Order

Implement SOC snapshots/thresholds first, then embedded step counting and
session reconciliation, then orientation snapshots/qualified changes. Each
is a separate increment; orientation wake is a later measured promotion,
not a prerequisite for orientation observation.

Before exposing each increment, agree concrete payloads, bounded delivery
and suspension rules, implement owner/runtime support and shared
schema/service/compiler/preview behavior, and record target evidence. A
placeholder's `stop2_wake` flag alone is not proof of a working wake source
or of delivery at the exact instant a threshold is reached.

The existing scoped-timer GUI work can proceed independently. Peep Studio
must not infer sensor support from this document, a broad capability name,
or the sensor datasheet. It must continue to discover executable sources
from the selected profile and service.

---

## Sensor Profile Schema

The shapes and names below describe the broader contracted API, not the
currently executable HW6 package format. The initial HW6 subset and its
exposure conditions are defined above.

Packages declare sensor use as package data.

Conceptual schema:

```text
sensor_profile:
  contexts[]:
    context_id
    scene_ref
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

Allowed modes:

| Mode | Meaning |
|---|---|
| `snapshot` | consume latest resolved value when package logic runs |
| `one_shot` | Platform may take a bounded sample and publish a resolved value |
| `low_rate_periodic` | Platform may maintain low-cadence snapshots/events |
| `event_interest` | Platform may publish normalized events when policy supports them |
| `bounded_stream` | Platform may maintain higher-rate data for a bounded active context |
| `step_session` | package-local step baseline and deltas over Platform step counter |

Rules:

- each context belongs to one or more declared scenes.
- context cadence must be valid for the scene type and target profile.
- context duration must be bounded unless the mode is explicitly low-power safe.
- optional capabilities must declare content fallback behavior.
- required capability failure at runtime is handled by Platform/Engine lifecycle policy, not gameplay branching.
- sensor wake intents are intent only; Platform decides whether and how to arm wake behavior.

---

## Game-Facing Values

Normal package code consumes resolved PeepOS values.

Ambient light:

```text
light.level_0_100
light.band
light.band_changed event
```

Light bands:

```text
UNKNOWN
DARK
DIM
NORMAL
BRIGHT
SATURATED
```

For normal HW5 runtime, `UNKNOWN` is a Platform diagnostic or startup/default state, not a game-authored hardware failure branch.

Steps:

```text
steps.total
steps.session_delta
steps.milestone event
```

IMU events:

```text
motion event
tap event
shake event
tilt event
orientation event
```

Motion snapshots or streams:

```text
motion.vector
motion.orientation
motion.activity_class
```

Exact payload fields must match the measured HW5 sensor behavior and must not promise unsupported LIS2DUX12TR channels.

---

## Step Counter Semantics

The package API must not reset or reconfigure the hardware step counter directly.

Game-facing step behavior is session based:

```text
step_session_begin(session_id)
step_session_reset_baseline(session_id)
step_session_delta(session_id)
step_total_snapshot()
```

Rules:

- session baselines are package/Engine state, not hardware counter resets.
- Platform owns embedded step-counter activation and retention policy.
- step counting must not wake the MCU for every step during normal low-power operation.
- if the step primitive fails at runtime, Platform/Engine logs the fault and applies lifecycle/degraded-capability policy.

For the initial HW6 package-session subset:

- The session baseline/count survives ordinary state, selection and scene
  changes. Stop, replacement or reset ends the live session; this is not a
  daily counter or an implicit reset-persistent total.
- The initial package-facing operation is observing steps since that session
  began, plus declared milestones. The generic session/reset functions above
  are conceptual, not additional executable commands granted by this plan.
- Temporary suspension preserves session identity but does not itself grant
  background counting. The measured context must explicitly specify whether
  counting continues while suspended and how gaps are represented. Handlers
  never execute while the package is suspended.
- The Platform reconciles the embedded counter using bounded scheduled reads
  or existing wake opportunities. The profile must bound observation latency
  and prevent ambiguous counter wrap. Reset/recovery must not be mistaken
  for a huge positive step delta or silently claimed as measured steps.
- Milestones fire once per declared threshold per session when a valid
  observed count crosses it. Batched counts may cross several milestones;
  delivery is bounded and deterministic in threshold then binding order,
  with stale-owner checks between handlers. This is not an interrupt at the
  exact Nth physical step or a stream of reconstructed per-step events.

---

## Scene Type Rules

| Scene Type | Sensor Behavior |
|---|---|
| `STATE_SCENE` | snapshots, one-shot samples, low-rate periodic contexts, step sessions, bounded event interests, and wake intents; no active high-rate stream while yielded |
| `SEQUENCE_SCENE` | bounded streams allowed only when required by declared timeline tracks and valid for the target profile |
| `PROGRAM_SCENE` | bounded motion/light streams allowed when declared and valid for the target profile |

Waiting visuals do not execute arbitrary package sensor logic. Platform may keep approved sensor-event or wake policy active if the selected target profile supports it.

System inactivity policy always applies. Declared gyro or other admitted sensor activity may refresh the interaction window only under the active scene policy; a package must not keep realtime sensor streaming or inactivity deferral alive indefinitely.

---

## Tool-Time Validation

Tooling must validate sensor use before package compilation/export.

Reject:

- raw ADC, GPIO, I2C, EXTI, register, address, or HAL references.
- direct sensor power, calibration, or wake-pin control.
- high-rate streaming in `STATE_SCENE`.
- unbounded sensor streaming.
- per-step MCU wake requirements.
- sensor cadence above the selected target profile limit.
- required sensor capability not present in the selected target profile.
- optional sensor feature without declared content fallback.
- sensor context not tied to a declared scene.
- wake intent unsupported by the selected target profile.

Authoring tools should explain failures in PeepOS terms, such as:

```text
Motion stream is not valid in STATE_SCENE.
Use SEQUENCE_SCENE or PROGRAM_SCENE for this interaction, or lower the sensor mode to event_interest.
```

They should not expose register, ADC, interrupt, or driver details to normal game authors.

---

## Target Profile Sensor Contexts

Target profiles publish measured sensor contexts.

A target profile sensor context describes what PeepOS can provide to packages for a capability, scene type, and power class.

Conceptual shape is defined in [[Target_Profile_Schema_Contract]] and includes:

```text
sensor_context:
  context_id
  capability
  scene_types[]
  power_class
  grant_status
  sample_rate_hz_min
  sample_rate_hz_max
  event_rate_hz_max
  wake_capable
  continuous_in_sleep
  mcu_wake_required
  duration_ms_max
  evidence_ref
```

Rules:

- package tools validate package `sensor_profile.contexts[]` against target profile `sensor_contexts[]`.
- rates are not sensor-wide facts; they belong to a measured context.
- Platform sampling cadence and package-visible event cadence are separate.
- wake-capable sensor behavior must be evidence-backed before shipping profiles grant it.
- high-duty streaming contexts must be scene-type bounded and duration bounded.

---

## Runtime Fault Handling

Runtime sensor faults are handled outside normal gameplay APIs.

If a required sensor primitive cannot be maintained:

1. Platform records the sensor owner fault.
2. Engine records affected package ID, scene, and sensor context.
3. package state is preserved where possible.
4. Engine applies lifecycle policy, such as suspend, exit to shell, or route to a declared safe state scene.
5. developer diagnostics receive a fault trace.

Normal package gameplay code must not branch on low-level sensor fault causes.

---

## Developer Diagnostics

Diagnostic traces may include:

- package ID
- scene ID
- sensor context ID
- requested PeepOS capability
- resolved Platform cadence/mode
- fault class
- Platform owner state
- lifecycle action taken
- timestamp or replay tick

Diagnostics are for tooling, logs, and bring-up. They are not part of the normal game API.

---

## Digital Twin Requirements

The digital twin must use the same sensor contract as the hardware runtime.

Twin sensor sources may include:

- scripted ambient-light tracks
- step-count tracks
- motion/tap/shake/tilt/orientation event traces
- deterministic motion stream traces
- fault-injection traces

Rules:

- normal `HOST_DIGITAL_TWIN_HW6` profiles assume nominal sensor owner health unless fault injection is enabled.
- fault injection tests Platform/Engine lifecycle and diagnostics, not ordinary gameplay error branches.
- twin replay must produce deterministic package-visible sensor values/events.
- twin profiles must be derived from measured HW5 behavior after hardware validation.
- twin traces are not HW5 bring-up evidence.

---

## Validation Cases

1. package using `sensor.light` receives resolved light level and band without ADC/GPIO exposure.
2. package using step sessions can reset package baseline without resetting hardware counter.
3. `STATE_SCENE` package with high-rate motion stream fails tool validation.
4. sequence/program scene with bounded motion stream validates only when target profile grants `sensor.imu_motion_stream`.
5. per-step MCU wake requirement fails validation.
6. sensor wake intent is accepted only when the target profile supports it.
7. required sensor primitive fault at runtime is logged and handled through lifecycle policy.
8. normal game-facing API does not expose I2C, ADC, register, or driver fault codes.
9. digital twin replay produces the same package-visible sensor event sequence for a fixed trace.
10. digital twin sensor fault injection records diagnostics without treating the injected fault as hardware bring-up evidence.

### Initial HW6 Acceptance Evidence

These are pending tests, not claims of existing support:

1. Default-off and observation-only contexts do not enable unrelated gesture
   detectors or sensor wake routes; suspension removes only the package's
   requests, leaving other admitted consumers intact.
2. SOC startup, stale/invalid samples, repeated same-side values, crossings
   and hysteresis rearming produce deterministic values/events without
   changing OS power protection.
3. Step totals survive scene changes and admitted STOP2 retention; counter
   wrap, recovery and suspension reconcile without invented steps or
   duplicate milestones. Measure latency and prove no per-step MCU wakes.
4. All six orientation poses match device axes; ambiguous angles, repeated
   samples and carried-motion traces do not produce duplicate qualified
   changes. Initial acquisition and resume do not fabricate transitions.
5. Before granting orientation wake, measure raw interrupt count as well as
   qualified event count, STOP2 residency, return-to-sleep, delivery latency
   and current on resting and normally carried units. Software event
   suppression alone is not evidence of avoided hardware wakes.
6. Export rejects deferred event types and ungranted wake intents; deterministic
   preview traces match firmware semantics. GUI timer support remains usable
   while these sensor capabilities are still unavailable.
