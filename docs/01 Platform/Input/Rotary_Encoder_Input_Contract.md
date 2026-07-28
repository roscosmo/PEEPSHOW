# Rotary Encoder Input Contract

This document defines the retired HW5 rotary encoder boundary and required state machine for targets that physically provide an encoder.

## Target Applicability

- HW5: historical implementation contract.
- HW6: not applicable. The encoder, `TIM2` encoder path, encoder channels, and `ENC_EN` were removed.
- HW6 target profiles must set `input.encoder_supported = false`, block capability `input.encoder`, and must not publish encoder wake intent.

The abstract `input.encoder` capability may remain in Engine contracts for other targets. This document does not grant that capability to HW6.

The rotary encoder is a Platform input device. It produces logical input events for Platform shell and Engine input focus systems.

It must not expose timer counters, GPIO control, or hardware policy to Engine or Reference Game code.

## Hardware Path

| Signal | MCU Pin | CubeMX Signal | Owner |
|---|---|---|---|
| Encoder A | `PA5` | `TIM2_CH1` | `thInput` |
| Encoder B | `PA1` | `TIM2_CH2` | `thInput` |
| Encoder enable | `PB4` | `ENC_EN` / `LPGPIO_Output` | `thInput` |

The fitted encoder is a TTC green rotary encoder. The exact manufacturer model number is currently unknown.

The HW5 encoder is rotation-only for Platform purposes. It does not define a press/select input. Button semantics come from the dedicated button inputs.

## Ownership

- `thInput` owns `TIM2` encoder mode and `ENC_EN`.
- Other layers consume logical encoder events only.
- Engine and Reference Game code must not directly read timer counters or toggle `ENC_EN`.
- Power policy may request encoder enable/disable, but `thInput` applies the transition.

## Current CubeMX Baseline

- `TIM2` is configured in encoder mode `TIM_ENCODERMODE_TI12`.
- `ENC_EN` is a low-power GPIO output.
- `TIM2_IRQn` is enabled in NVIC and is the intended encoder activity interrupt path.
- Deep-sleep wake behavior depends on the selected sleep class and TIM2 clock/interrupt behavior, and must be proven during bring-up.

## Required Knobs

The following values must become compile-time knobs before implementation:

| Knob | Purpose |
|---|---|
| `KNOB_INPUT_ENC_SETTLE_MS` | time after `ENC_EN` before counter sampling is trusted |
| `KNOB_INPUT_ENC_SAMPLE_MS` | normal counter polling cadence |
| `KNOB_INPUT_ENC_IDLE_TIMEOUT_MS` | time without delta before returning to idle |
| `KNOB_INPUT_ENC_ACCEL_WINDOW_MS` | window used for acceleration classification |
| `KNOB_INPUT_ENC_ACCEL_THRESHOLD` | delta threshold for accelerated events |
| `KNOB_INPUT_ENC_WAKE_ARM_MAX_MS` | maximum time the encoder may remain wake-armed without renewed policy |

`ENC_EN` is active high. Firmware safe default is low/off. `thInput` must only assert it when encoder input or explicit wake-armed policy is requested.

## Logical Events

`thInput` publishes encoder-specific logical events.

- `INPUT_ENC_DELTA`
- `INPUT_ENC_CCW_FAST`
- `INPUT_ENC_CW_FAST`
- `INPUT_ENC_ACTIVITY`
- `INPUT_ENC_IDLE`
- `INPUT_ENC_FAULT`

`INPUT_ENC_DELTA` payload should include:

- signed delta or direction plus step count
- acceleration flag or acceleration class
- timestamp
- source mode, such as normal or wake-armed

The Platform must not map encoder rotation directly to navigation left/right. Current input focus decides whether a delta means navigation, value adjustment, gameplay control, or another action.

Exact payload shape belongs in [[Interface_Control_Document]].

No universal action may be assigned to clockwise or counter-clockwise rotation at the Platform layer.

## State Machine

| State | Meaning |
|---|---|
| `ENC_OFF` | encoder supply/control path disabled; timer counter ignored |
| `ENC_POWER_SETTLE` | `ENC_EN` asserted; waiting for powered encoder path to settle |
| `ENC_IDLE` | encoder enabled and sampled, no active movement |
| `ENC_TRACKING` | movement detected; deltas converted to encoder delta events |
| `ENC_ACCEL` | movement exceeds acceleration threshold; accelerated delta events emitted |
| `ENC_WAKE_ARMED` | encoder intentionally powered and armed as an activity/wake source under Platform sleep policy |
| `ENC_SUSPENDED` | quiesced for sleep or mode policy when wake-armed behavior is not allowed |
| `ENC_ERROR` | timer, GPIO, wake-arm, or impossible-counter behavior detected |

## Events

| Event | Source | Meaning |
|---|---|---|
| `EV_ENC_ENABLE_REQ` | power/input policy | enable encoder path |
| `EV_ENC_DISABLE_REQ` | power/input policy | disable encoder path |
| `EV_ENC_WAKE_ARM_REQ` | power/input policy | keep encoder powered for activity/wake detection |
| `EV_ENC_WAKE_DISARM_REQ` | power/input policy | leave wake-armed mode |
| `EV_ENC_SETTLE_EXPIRED` | `thInput` timer | hardware settle window elapsed |
| `EV_ENC_COUNTER_DELTA` | `TIM2_IRQn` / `thInput` poll | counter changed |
| `EV_ENC_ACCEL_DETECTED` | `thInput` classifier | delta exceeded fast threshold |
| `EV_ENC_ACTIVITY_TIMEOUT` | `thInput` timer | movement stopped |
| `EV_QUIESCE` | `thPower` | prepare for sleep/mode transition |
| `EV_RESUME` | `thPower` | resume from sleep/mode transition |
| `EV_ENC_FAULT` | `thInput` | timer/GPIO/counter fault |
| `EV_RECOVER_OK` | `thInput` | recovery completed |

## Transition Rules

| Current | Event | Next | Required Action |
|---|---|---|---|
| `ENC_OFF` | `EV_ENC_ENABLE_REQ` | `ENC_POWER_SETTLE` | assert `ENC_EN`, reset counter baseline, start settle timer |
| `ENC_OFF` | `EV_ENC_WAKE_ARM_REQ` | `ENC_POWER_SETTLE` | assert `ENC_EN`, mark wake-arm request, reset counter baseline, start settle timer |
| `ENC_POWER_SETTLE` | `EV_ENC_SETTLE_EXPIRED` | `ENC_IDLE` | enable TIM2 activity path, publish ready health |
| `ENC_IDLE` | `EV_ENC_COUNTER_DELTA` | `ENC_TRACKING` | classify direction, emit logical event |
| `ENC_TRACKING` | `EV_ENC_COUNTER_DELTA` | `ENC_TRACKING` | emit bounded logical events |
| `ENC_TRACKING` | `EV_ENC_ACCEL_DETECTED` | `ENC_ACCEL` | emit accelerated logical event |
| `ENC_ACCEL` | `EV_ENC_COUNTER_DELTA` | `ENC_ACCEL` | continue fast classification while threshold holds |
| `ENC_ACCEL` | `EV_ENC_ACTIVITY_TIMEOUT` | `ENC_IDLE` | emit idle event |
| `ENC_TRACKING` | `EV_ENC_ACTIVITY_TIMEOUT` | `ENC_IDLE` | emit idle event |
| `ENC_IDLE` | `EV_ENC_WAKE_ARM_REQ` | `ENC_WAKE_ARMED` | keep `ENC_EN` asserted, arm TIM2 activity policy, publish wake-armed status |
| `ENC_WAKE_ARMED` | `EV_ENC_COUNTER_DELTA` | `ENC_TRACKING` | wake/activity detected, re-baseline as needed, emit encoder delta only after validation |
| `ENC_WAKE_ARMED` | `EV_ENC_WAKE_DISARM_REQ` | `ENC_IDLE` | leave wake-armed policy and continue normal enabled behavior |
| any enabled state | `EV_ENC_DISABLE_REQ` | `ENC_OFF` | stop sampling, disable `ENC_EN`, clear transient deltas |
| any enabled state | `EV_QUIESCE` | `ENC_SUSPENDED` | stop sampling and place hardware in policy-defined sleep state |
| `ENC_SUSPENDED` | `EV_RESUME` | `ENC_POWER_SETTLE` | re-enable and re-baseline through settle path |
| any state | `EV_ENC_FAULT` | `ENC_ERROR` | disable output events, publish fault |
| `ENC_ERROR` | `EV_RECOVER_OK` | `ENC_OFF` | clear fault only after timer/GPIO validation |

Invalid transitions must be rejected and logged.

## Mode Behavior

| Mode | Encoder Policy |
|---|---|
| `SHELL` | enabled when interactive UI expects rotary navigation |
| `LP_GRAPH` | off unless Engine capability request or wake policy explicitly arms it |
| `LP_MODULE` | policy-defined by Engine focus request; wake-armed only by explicit policy |
| `RT_SCENE` | enabled only while active scene requests encoder focus |
| `INSTALLER` | disabled unless installer UI explicitly requires local navigation |

The encoder may enter `ENC_WAKE_ARMED` when requested by input focus, UI, game, or wake policy.

`ENC_WAKE_ARMED` is a real policy state, not a bring-up workaround. It requires the Platform to choose a sleep class where TIM2 encoder activity can be observed. STOP-compatible behavior must be validated on HW5 hardware before documenting the encoder as a deep-sleep wake source.

## Power Rules

- `ENC_EN` must default low/off.
- Encoder hardware is powered only while requested.
- `thInput` must quiesce sampling before STOP entry unless Platform policy enters `ENC_WAKE_ARMED`.
- `thInput` must re-baseline after wake before emitting events.
- No clock or timer dependency may be assumed valid until `EV_RESUME` completes.

## Validation Cases

1. enable/settle/idle sequence produces no false movement events
2. clockwise and counter-clockwise movement map to signed encoder delta events
3. fast rotation maps to accelerated delta events without unbounded event bursts
4. quiesce/resume does not emit stale deltas
5. disable path leaves `ENC_EN` low/off
6. wake-armed mode detects activity through the intended TIM2 path in the selected sleep class
7. unsupported sleep depth is rejected while encoder wake is requested
8. timer/counter fault routes to `ENC_ERROR` and recovers through `ENC_OFF`
9. no press/select event is emitted by the rotary encoder path

Related:

- [[Input_Index]]
- [[Subsystem_State_Machines]]
- [[HW5_Pin_Ownership_Matrix]]
- [[HW5_Power_Rails]]
- [[Power_and_Sleep_Policy]]
