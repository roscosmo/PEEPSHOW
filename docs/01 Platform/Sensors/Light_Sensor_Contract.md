# Light Sensor Contract

This document defines the retired HW5 ambient light sensor boundary and required state machine for targets that physically provide the sensor.

## Target Applicability

- HW5: historical implementation contract.
- HW6: not applicable. The TEMT6000, `PHOT_EN`, ADC input, and ADC1 configuration were removed.
- HW6 target profiles must set `sensors.light_supported = false` and must not grant `sensor.light` or `sensor.light_stream`.

The abstract light-sensor capabilities may remain in Engine contracts for other targets. This document does not grant them to HW6.

The light sensor is a Platform sensor. It uses a `TEMT6000X01` ambient-light phototransistor feeding `ADC1_IN4`.

It provides a normalized ambient-light value to Engine and Reference Game consumers through sensor snapshots or events.

It must not expose ADC control, enable GPIO, or sampling policy to Engine or Reference Game code.

## Hardware Path

| Signal | MCU Pin | CubeMX Signal | Owner |
|---|---|---|---|
| Enable | `PC2` | `PHOT_EN` / `LPGPIO_Output` | `thSensor` |
| Analog sample | `PC3` | `PHOT_ADC` / `ADC1_IN4` | `thSensor` |

## Ownership

- `thSensor` owns `PHOT_EN`, `ADC1`, and ambient-light sampling.
- Other layers consume a normalized ambient-light snapshot or event.
- Engine and Reference Game code must not directly enable the sensor or sample ADC.
- Power policy and Engine may request ambient-light cadence, but `thSensor` applies and clamps the hardware behavior.

## Current CubeMX Baseline

- `ADC1` has one conversion configured.
- `PHOT_ADC` is `ADC1_IN4`.
- `PHOT_EN` is `LPGPIO_Output`.
- ADC interrupt and DMA are intentionally not enabled yet.
- No light-threshold wake interrupt is assigned.

## Intended Uses

The light sensor is used for:

- game input, including bounded realtime minigames
- low-rate system display policy, such as reducing display refresh rate in darkness
- diagnostics and calibration

There is no display backlight on the Sharp Memory LCD, so this sensor does not drive backlight brightness.

## Required Knobs

The following values must become compile-time knobs before implementation:

| Knob | Purpose |
|---|---|
| `KNOB_SENSOR_LIGHT_SETTLE_MS` | time after `PHOT_EN` before ADC sample is trusted |
| `KNOB_SENSOR_LIGHT_SAMPLE_COUNT` | number of ADC samples per published reading |
| `KNOB_SENSOR_LIGHT_SAMPLE_SPACING_MS` | delay between samples in a burst |
| `KNOB_SENSOR_LIGHT_SYSTEM_PERIOD_MS` | normal low-rate system sampling cadence |
| `KNOB_SENSOR_LIGHT_STREAM_MAX_HZ` | maximum Platform-approved realtime sampling rate |
| `KNOB_SENSOR_LIGHT_STREAM_MAX_MS` | maximum continuous streaming lease duration |
| `KNOB_SENSOR_LIGHT_DELTA_THRESHOLD` | minimum filtered change needed to publish an event |

`PHOT_EN` is active high. Firmware safe default is low/off. `thSensor` must only assert it for bounded one-shot, periodic, or streaming leases.

## Published Data

`thSensor` publishes a Platform-owned snapshot.

Normal Engine and Reference Game consumers use:

- `normalized_0_100`
- `light_band`
- `sample_age_ms`
- `valid`
- `mode`

Raw ADC values are diagnostics only and should be limited to bring-up, calibration, or debug builds.

The normal game-facing value is normalized to `0-100` using calibration and filtering owned by the Platform.

Suggested light bands:

- `LIGHT_BAND_UNKNOWN`
- `LIGHT_BAND_DARK`
- `LIGHT_BAND_DIM`
- `LIGHT_BAND_NORMAL`
- `LIGHT_BAND_BRIGHT`
- `LIGHT_BAND_SATURATED`

Exact payload shape belongs in [[Interface_Control_Document]].

## Request Model

Engine and Reference Game code may request:

- latest snapshot
- one-shot sample
- bounded sample cadence for active gameplay

Platform may clamp or reject requested sample rates and durations.

Conceptual request shape:

```text
ambient_light_request(rate_hz, reason, max_duration_ms)
```

Examples:

- low-rate system display policy: approximately sub-Hz to 1 Hz
- active minigame feedback: bounded realtime cadence approved by Platform
- one-shot calibration sample: immediate sample burst, then off

## State Machine

| State | Meaning |
|---|---|
| `LIGHT_OFF` | sensor disabled; ADC path inactive |
| `LIGHT_SETTLING` | `PHOT_EN` asserted; waiting for analog path to settle |
| `LIGHT_SINGLE_SAMPLE` | one on-demand ADC sample burst in progress |
| `LIGHT_PERIODIC_WAIT` | low-frequency system cadence mode, normally off between samples |
| `LIGHT_STREAMING` | bounded higher-rate gameplay sampling mode |
| `LIGHT_PROCESSING` | samples are reduced, calibrated, normalized, and banded |
| `LIGHT_PUBLISH` | new snapshot/event is published if threshold rules allow |
| `LIGHT_SUSPENDED` | quiesced for sleep or mode transition |
| `LIGHT_ERROR` | ADC, GPIO, timeout, or impossible-value fault detected |

## Events

| Event | Source | Meaning |
|---|---|---|
| `EV_LIGHT_ONESHOT_REQ` | system / Engine request | take one reading |
| `EV_LIGHT_PERIODIC_REQ` | system policy | enter low-rate periodic mode |
| `EV_LIGHT_STREAM_REQ` | Engine gameplay request | enter bounded realtime sampling mode |
| `EV_LIGHT_STOP_REQ` | system / Engine request | stop sampling |
| `EV_LIGHT_SETTLE_DONE` | `thSensor` timer | settle interval elapsed |
| `EV_LIGHT_PERIOD_ELAPSED` | `thSensor` timer | low-rate periodic sample due |
| `EV_LIGHT_ADC_DONE` | `thSensor` / ADC poll | ADC sample burst completed |
| `EV_LIGHT_PROCESS_DONE` | `thSensor` | normalized value ready |
| `EV_LIGHT_PUBLISH_DONE` | `thSensor` | snapshot/event publication complete |
| `EV_LIGHT_NO_CHANGE` | `thSensor` | filtered delta below publish threshold |
| `EV_LIGHT_STREAM_EXPIRED` | `thSensor` timer | streaming lease expired |
| `EV_QUIESCE` | `thPower` | prepare for sleep/mode transition |
| `EV_RESUME` | `thPower` | resume from sleep/mode transition |
| `EV_LIGHT_FAULT` | `thSensor` | ADC/GPIO/timeout fault |
| `EV_RECOVER_OK` | `thSensor` | recovery completed |

## Transition Rules

| Current | Event | Next | Required Action |
|---|---|---|---|
| `LIGHT_OFF` | `EV_LIGHT_ONESHOT_REQ` | `LIGHT_SETTLING` | assert `PHOT_EN`, mark one-shot mode, start settle timer |
| `LIGHT_OFF` | `EV_LIGHT_PERIODIC_REQ` | `LIGHT_PERIODIC_WAIT` | schedule low-rate system cadence |
| `LIGHT_OFF` | `EV_LIGHT_STREAM_REQ` | `LIGHT_SETTLING` | clamp request, assert `PHOT_EN`, mark stream mode, start settle timer |
| `LIGHT_PERIODIC_WAIT` | `EV_LIGHT_PERIOD_ELAPSED` | `LIGHT_SETTLING` | assert `PHOT_EN`, mark periodic sample |
| `LIGHT_SETTLING` | `EV_LIGHT_SETTLE_DONE` | `LIGHT_SINGLE_SAMPLE` | start bounded ADC sample burst |
| `LIGHT_SINGLE_SAMPLE` | `EV_LIGHT_ADC_DONE` | `LIGHT_PROCESSING` | stop ADC conversion path until next sample |
| `LIGHT_STREAMING` | `EV_LIGHT_ADC_DONE` | `LIGHT_PROCESSING` | keep `PHOT_EN` active while stream lease remains valid |
| `LIGHT_PROCESSING` | `EV_LIGHT_PROCESS_DONE` | `LIGHT_PUBLISH` | compare against last published value and request mode |
| `LIGHT_PROCESSING` | `EV_LIGHT_NO_CHANGE` | mode-dependent | return to `LIGHT_OFF`, `LIGHT_PERIODIC_WAIT`, or `LIGHT_STREAMING` without event spam |
| `LIGHT_PUBLISH` | `EV_LIGHT_PUBLISH_DONE` | mode-dependent | update snapshot; return to `LIGHT_OFF`, `LIGHT_PERIODIC_WAIT`, or `LIGHT_STREAMING` |
| `LIGHT_STREAMING` | `EV_LIGHT_STREAM_EXPIRED` | `LIGHT_OFF` | deassert `PHOT_EN`, publish idle/lease-ended status |
| any active state | `EV_LIGHT_STOP_REQ` | `LIGHT_OFF` | abort/finish bounded work, deassert `PHOT_EN` |
| any active state | `EV_QUIESCE` | `LIGHT_SUSPENDED` | abort/finish bounded work, deassert `PHOT_EN` unless policy explicitly allows retention |
| `LIGHT_SUSPENDED` | `EV_RESUME` | `LIGHT_OFF` | resume idle; wait for next request |
| any state | `EV_LIGHT_FAULT` | `LIGHT_ERROR` | deassert `PHOT_EN`, publish sensor fault |
| `LIGHT_ERROR` | `EV_RECOVER_OK` | `LIGHT_OFF` | clear fault only after ADC/GPIO validation |

Invalid transitions must be rejected and logged.

## Mode Behavior

| Mode | Light Sensor Policy |
|---|---|
| `SHELL` | low-cadence system sampling allowed for display refresh policy and diagnostics |
| `STATE_SCENE` | latest snapshot, one-shot, or low-rate event sampling; no continuous awake polling while yielded |
| `SEQUENCE_SCENE` | bounded streaming only when the declared timeline requires it and frame/audio timing remains valid |
| `PROGRAM_SCENE` | clamped streaming allowed for active gameplay when declared capabilities and frame/audio budgets permit it |
| `INSTALLER` | disabled unless diagnostics explicitly request it |

The light sensor is not currently a wake source.

## Power Rules

- `PHOT_EN` must default low/off.
- Low-rate sampling should be burst-based: enable, settle, sample, process, publish, disable.
- Streaming mode may keep `PHOT_EN` active for a bounded lease.
- The sensor must not remain powered indefinitely after a failed sample.
- Deep sleep must not be blocked waiting for a non-critical ambient-light sample.
- ADC interrupt and DMA are not required for first bring-up. Polling is acceptable until measured cadence or CPU cost requires otherwise.

## Failure Policy

The light sensor is non-critical.

On failure:

- publish `valid = false`
- use last-good or default midpoint normalized value where a value is required
- publish `LIGHT_BAND_UNKNOWN`
- keep Platform display policy functional with conservative defaults
- do not block boot, shell, storage, display, or game runtime

## Validation Cases

1. disabled state keeps `PHOT_EN` low/off
2. one-shot enable/settle/sample/off sequence completes within a bounded time
3. periodic mode samples at low cadence and powers down between readings
4. streaming mode provides bounded realtime feedback and expires cleanly
5. normalized 0-100 output tracks raw ADC changes after calibration
6. large ambient change publishes a new snapshot/event
7. small changes below threshold do not spam events
8. quiesce during settle/sample/stream leaves sensor off
9. ADC timeout, stuck-at-rail, or impossible calibration routes to `LIGHT_ERROR`

Related:

- [[Sensors_Index]]
- [[Subsystem_State_Machines]]
- [[HW5_Pin_Ownership_Matrix]]
- [[HW5_Power_Rails]]
- [[Power_and_Sleep_Policy]]
