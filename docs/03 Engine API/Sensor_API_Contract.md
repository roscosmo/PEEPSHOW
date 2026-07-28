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
- [[Power_and_Sleep_Policy]]
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

- package sensor capability declarations
- runtime-unit sensor context admission
- stable game-facing sensor primitives
- tool-time validation rules
- lifecycle behavior when a required sensor primitive cannot be maintained
- developer diagnostics and fault trace presentation

Packages own:

- declaration of which sensor primitives their runtime units use
- gameplay use of resolved sensor values and events
- optional content fallbacks where the package is intentionally portable across profiles

Packages do not own hardware behavior.

---

## Core Rules

- Game code must not request ADC, GPIO, I2C, register, interrupt, or sensor power behavior.
- Normal gameplay APIs must not expose hardware rejection, register faults, ADC timeouts, I2C errors, or low-level sensor health codes.
- Sensor mode and cadence validity is checked before package compilation/export.
- If a runtime unit declares a sensor context that is valid for the selected target profile, PeepOS is responsible for providing it under nominal hardware conditions.
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

## Sensor Profile Schema

Packages declare sensor use as package data.

Conceptual schema:

```text
sensor_profile:
  contexts[]:
    context_id
    runtime_unit_ref
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

- each context belongs to one or more declared runtime units.
- context cadence must be valid for the runtime class and target profile.
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

---

## Runtime Class Rules

| Runtime Class | Sensor Behavior |
|---|---|
| `LP_GRAPH` | snapshots, step sessions, low-rate events, and wake intents only; no active high-rate stream |
| `LP_MODULE` | snapshots, one-shot samples, low-rate periodic contexts, step sessions, and bounded event interests |
| `RT_SCENE` | bounded motion/light streams allowed when declared and valid for target profile |

Waiting visuals do not execute arbitrary package sensor logic. Platform may keep approved sensor-event or wake policy active if the selected target profile supports it.

Automatic input locking applies when enabled by the package. Declared gyro or other admitted sensor activity may refresh that lock only under the active block policy; a package must not keep realtime sensor streaming or lock deferral alive indefinitely.

---

## Tool-Time Validation

Tooling must validate sensor use before package compilation/export.

Reject:

- raw ADC, GPIO, I2C, EXTI, register, address, or HAL references.
- direct sensor power, calibration, or wake-pin control.
- high-rate streaming in `LP_GRAPH`.
- unbounded sensor streaming.
- per-step MCU wake requirements.
- sensor cadence above the selected target profile limit.
- required sensor capability not present in the selected target profile.
- optional sensor feature without declared content fallback.
- sensor context not tied to a declared runtime unit.
- wake intent unsupported by the selected target profile.

Authoring tools should explain failures in PeepOS terms, such as:

```text
Motion stream is not valid in LP_GRAPH.
Use RT_SCENE for this minigame or lower the sensor mode to event_interest.
```

They should not expose register, ADC, interrupt, or driver details to normal game authors.

---

## Target Profile Sensor Contexts

Target profiles publish measured sensor contexts.

A target profile sensor context describes what PeepOS can provide to packages for a capability, runtime class, and power class.

Conceptual shape is defined in [[Target_Profile_Schema_Contract]] and includes:

```text
sensor_context:
  context_id
  capability
  runtime_classes[]
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
- high-duty streaming contexts must be runtime-class bounded and duration bounded.

---

## Runtime Fault Handling

Runtime sensor faults are handled outside normal gameplay APIs.

If a required sensor primitive cannot be maintained:

1. Platform records the sensor owner fault.
2. Engine records affected package ID, runtime unit, and sensor context.
3. package state is preserved where possible.
4. Engine applies lifecycle policy, such as suspend, exit to shell, or route to a declared safe runtime unit.
5. developer diagnostics receive a fault trace.

Normal package gameplay code must not branch on low-level sensor fault causes.

---

## Developer Diagnostics

Diagnostic traces may include:

- package ID
- runtime unit ID
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
3. `LP_GRAPH` package with high-rate motion stream fails tool validation.
4. `RT_SCENE` package with bounded motion stream validates only when target profile grants `sensor.imu_motion_stream`.
5. per-step MCU wake requirement fails validation.
6. sensor wake intent is accepted only when the target profile supports it.
7. required sensor primitive fault at runtime is logged and handled through lifecycle policy.
8. normal game-facing API does not expose I2C, ADC, register, or driver fault codes.
9. digital twin replay produces the same package-visible sensor event sequence for a fixed trace.
10. digital twin sensor fault injection records diagnostics without treating the injected fault as hardware bring-up evidence.
