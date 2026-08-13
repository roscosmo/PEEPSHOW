# Button Input Contract

This document defines active-target button ownership, electrical behavior, classification, and state machines. The retained HW6 button paths require HW6 revalidation.

Buttons are Platform input devices. Platform emits raw logical button events and chord masks. UI, Engine, and Reference Game code map those events contextually.

No button is a universal accept/back/action key at the Platform layer.

## Hardware Path

| Button | MCU Pin | CubeMX Signal | Electrical Behavior | Owner | Normal Game Input |
|---|---|---|---|---|---|
| `BTN_START` | `PA4` | `EXTI4` | pullup, button pulls down through BAT54 into ADP5360 `MR` path, active low | `thInput` / `thPower` policy | yes |
| `BTN_A` | `PB5` | `EXTI5` | pulldown, 0.1 uF debounce cap, active high | `thInput` | yes |
| `BTN_B` | `PB6` | `EXTI6` | pulldown, 0.1 uF debounce cap, active high | `thInput` | yes |
| `BTN_L` | `PB7` | `EXTI7` | pulldown, 0.1 uF debounce cap, active high | `thInput` | yes |
| `BTN_R` | `PB8` | `EXTI8` | pulldown, 0.1 uF debounce cap, active high | `thInput` | yes |
| `BTN_BOOT` | `PH3-BOOT0` | `EXTI3` when application is running | BOOT0 pin, pulldown, active high; can force ROM bootloader before firmware runs | ROM bootloader before app, then `thInput` maintenance policy | no |

## Ownership

- `thInput` owns EXTI event capture, debounce, hold timing, repeat timing, and chord classification.
- `thPower` consumes system-owned Start shipping/power intent and shipping-mode warning events; ordinary package Start actions remain focus-routed when no system overlay consumes them.
- `BTN_BOOT` is hardware `BOOT0`. If sampled high at reset, STM32 ROM bootloader may run before application firmware can classify it. If application firmware is running, `BTN_BOOT` is reserved for system maintenance/recovery behavior and must not be exposed as normal Engine/Game input.
- UI, Engine, and Reference Game code consume logical button events and chord masks only.

## HW6 FW0 Bring-Up Status

On HW6 unit 001, FW0 has proven the application-visible A/B/L/R/Start button
edge path and the first UI consumption path:

- raw EXTI/debounce probing recorded A/B/L/R and Start press/release events
  returning to inactive state
- `thInput` publishes generic button impulses, not universal accept/back actions
- `thInput` now has an explicit logical-event and delivery-policy scaffold: physical A/B/L/R accepted presses become generic logical press records, then policy forwards allowed records without assigning UI meaning
- `thUI` currently maps `BTN_A`, `BTN_B`, `BTN_L`, and `BTN_R` according to
  shell context for menu navigation and calibration screens
- A/B/L/R navigation was physically confirmed through the UI: L/R changed focus
  and A/B entered/exited pages
- L/R are approved fallback navigation controls while joystick calibration is
  missing or invalid

FW0 A/B/L/R handling is now an EXTI-assisted, ThreadX-tick-owned physical FSM.
The EXTI path only records raw press/release edges and debug timestamps;
`thInput` owns debounce deadlines, accepted press impulses, release completion,
and future long/repeat/stuck classification. `HAL_GetTick()` must not be used
as the authoritative timing source for A/B/L/R classification. Quick taps are
explicitly supported: if a release edge arrives while press debounce is still
pending, the release is latched and the tap emits exactly one generic press
impulse when the debounce interval expires, then returns to released.

FW0 API version 9 adds the first logical input policy scaffold. `PS_InputButtons_TakeLogicalEvent()` currently emits validated physical A/B/L/R presses as `PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS` records with raw button IDs and masks. `thInput` then records policy context and forwards only allowed generic press records to `thUI`. The default policy is unlocked and UI-focused while the runtime host is active; it records suppression reasons for runtime-not-ready, lock-active, unsupported event, invalid button, and queue-send failure. This scaffold does not map A/B/L/R to accept/back/up/down, and it does not yet publish long, repeat, chord, or stuck logical events.

Validated HW6 unit 001 FW0 evidence for API version 8: after physical A/B/L/R
navigation, `__fw0_buttons_prints.gdb` reported `api/edges/presses/ignored =
8 / 18 / 7 / 3`, `counts deb p/r accept p/r long repeat stuck bounce = 7 / 0 /
7 / 7 / 0 / 0 / 0 / 0`, all A/B/L/R states returned to `RELEASED`, and the UI
reacted as expected. The non-zero ignored edge count is allowed for extra
release/bounce edges after a tap has already been latched and completed.

Validated HW6 unit 001 FW0 evidence for API version 9: after physical A/B/L/R navigation, `__fw0_buttons_prints.gdb` reported `api/edges/presses/ignored = 9 / 20 / 7 / 3`, `logical counts event/p/r/l/rep/ch/stuck = 7 / 7 / 0 / 0 / 0 / 0 / 0`, and `input policy api/event/deliv/supp/lock = 1 / 7 / 7 / 0 / 0`. The last policy record was a generic UI-focused press with `target/reason/status = 1 / 1 / 0x0`; all A/B/L/R physical FSM states returned to `RELEASED`; and no policy suppression occurred during normal shell navigation. This validates the current scaffold: physical accepted presses are converted into generic logical press records, then forwarded through the delivery policy without assigning platform-level button meaning.

This is not the complete button contract. Non-START long press, repeat, chord,
stuck button, wake-from-low-power, and `BTN_BOOT` remain open. START
shipping-prep/warning/imminent timing has an FW0 scaffold validated on HW6
unit 001, but final product behavior is still governed by
[[PMIC_and_Power_Contract]].

## Start Button / ADP5360 Shipping Mode

`BTN_START` is electrically tied into the ADP5360 `MR` path.

Hardware behavior:

- short and normal long presses are firmware-observable button input
- holding Start for the ADP5360 shipping threshold enters shipping mode
- in shipping mode, pressing Start for the required wake duration exits shipping mode

Contract:

- Firmware may detect Start hold duration and present countdown/warning UX.
- Firmware should begin save/quiesce preparation before the shipping threshold, not only near the final threshold.
- Firmware must not assume it can prevent shipping mode once the hardware threshold is reached.
- Game/save systems must treat Start shipping intent as a serious power-loss-adjacent event.

Timing details:

- ADP5360 shipping-mode threshold: approximately 12 s by hardware design.
- ADP5360 shipping-mode exit press: approximately 200 ms by hardware design.
- Firmware warning/quiesce lead time is policy-defined and must be controlled by Platform knobs.

## Required Knobs

| Knob | Purpose |
|---|---|
| `KNOB_INPUT_BTN_DEBOUNCE_PRESS_MS` | debounce interval after active edge |
| `KNOB_INPUT_BTN_DEBOUNCE_RELEASE_MS` | debounce interval after release edge |
| `KNOB_INPUT_BTN_LONG_PRESS_MS` | normal long-press threshold |
| `KNOB_INPUT_BTN_REPEAT_START_MS` | hold duration before repeat events begin |
| `KNOB_INPUT_BTN_REPEAT_PERIOD_MS` | repeat event cadence |
| `KNOB_INPUT_BTN_STUCK_MS` | stuck-button fault threshold |
| `KNOB_INPUT_CHORD_WINDOW_MS` | window for combining button presses into a chord |
| `KNOB_INPUT_START_LONG_PRESS_MS` | Start-hold duration where firmware leaves the normal-press window |
| `KNOB_INPUT_START_STABLE_SAMPLES` | consecutive PA4 live-level samples required before START level is treated as stable |
| `KNOB_INPUT_START_SHIP_PREP_MS` | Start-hold duration where save/quiesce preparation begins |
| `KNOB_INPUT_START_SHIP_WARN_MS` | Start-hold duration where visible warning/countdown begins |
| `KNOB_INPUT_START_SHIP_IMMINENT_MS` | Start-hold duration where shipping-mode entry is considered imminent |

The ship-prep, warning, and imminent thresholds must be below the hardware shipping-mode threshold.

## Per-Button Physical State Machine

Each physical button has an independent FSM.

| State | Meaning |
|---|---|
| `BTN_DISABLED` | button ignored by policy, except hardware wake if explicitly armed |
| `BTN_RELEASED` | stable inactive state |
| `BTN_DEBOUNCE_PRESS` | active edge received; waiting for debounce confirmation |
| `BTN_PRESSED` | stable active press confirmed |
| `BTN_HELD` | press duration exceeded long-press threshold |
| `BTN_REPEAT` | repeat events active while held, if enabled by policy |
| `BTN_DEBOUNCE_RELEASE` | release edge received; waiting for debounce confirmation |
| `BTN_STUCK` | button active longer than stuck threshold |
| `BTN_ERROR` | impossible transition, GPIO fault, or classifier inconsistency |

## Per-Button Events

| Event | Source | Meaning |
|---|---|---|
| `EV_BTN_EDGE_ACTIVE` | EXTI / input scan | electrical active edge observed |
| `EV_BTN_EDGE_INACTIVE` | EXTI / input scan | electrical release edge observed |
| `EV_BTN_DEBOUNCE_DONE` | `thInput` timer | debounce interval elapsed |
| `EV_BTN_LONG_THRESHOLD` | `thInput` timer | normal long-press threshold reached |
| `EV_BTN_REPEAT_THRESHOLD` | `thInput` timer | repeat may begin |
| `EV_BTN_REPEAT_TICK` | `thInput` timer | repeat event due |
| `EV_BTN_STUCK_THRESHOLD` | `thInput` timer | held too long for normal behavior |
| `EV_BTN_DISABLE_REQ` | input/power policy | disable button events |
| `EV_BTN_ENABLE_REQ` | input/power policy | enable button events |
| `EV_BTN_FAULT` | `thInput` | GPIO/classifier fault |
| `EV_RECOVER_OK` | `thInput` | recovery completed |

## Physical Transition Rules

| Current | Event | Next | Required Action |
|---|---|---|---|
| `BTN_RELEASED` | `EV_BTN_EDGE_ACTIVE` | `BTN_DEBOUNCE_PRESS` | latch active edge and start press debounce timer in `thInput` tick domain |
| `BTN_DEBOUNCE_PRESS` | `EV_BTN_EDGE_INACTIVE` | `BTN_DEBOUNCE_PRESS` | latch quick-tap release; do not cancel the pending press |
| `BTN_DEBOUNCE_PRESS` | `EV_BTN_DEBOUNCE_DONE` and active | `BTN_PRESSED` | emit down/press candidate to classifier |
| `BTN_DEBOUNCE_PRESS` | `EV_BTN_DEBOUNCE_DONE` and release latched | `BTN_RELEASED` | emit one press candidate plus release completion for a quick tap |
| `BTN_DEBOUNCE_PRESS` | `EV_BTN_DEBOUNCE_DONE` and inactive with no latched release | `BTN_RELEASED` | reject bounce |
| `BTN_PRESSED` | `EV_BTN_LONG_THRESHOLD` | `BTN_HELD` | emit long-press candidate if policy allows |
| `BTN_HELD` | `EV_BTN_REPEAT_THRESHOLD` | `BTN_REPEAT` | start repeat cadence if repeat enabled |
| `BTN_REPEAT` | `EV_BTN_REPEAT_TICK` | `BTN_REPEAT` | emit repeat candidate if focus accepts repeats |
| `BTN_PRESSED` / `BTN_HELD` / `BTN_REPEAT` | `EV_BTN_EDGE_INACTIVE` | `BTN_DEBOUNCE_RELEASE` | start release debounce timer |
| `BTN_DEBOUNCE_RELEASE` | `EV_BTN_DEBOUNCE_DONE` and inactive | `BTN_RELEASED` | emit release and final duration |
| `BTN_DEBOUNCE_RELEASE` | `EV_BTN_DEBOUNCE_DONE` and active | previous active state | reject release bounce |
| any active state | `EV_BTN_STUCK_THRESHOLD` | `BTN_STUCK` | emit stuck fault, suppress normal repeats |
| any state | `EV_BTN_DISABLE_REQ` | `BTN_DISABLED` | suppress logical events |
| `BTN_DISABLED` | `EV_BTN_ENABLE_REQ` | `BTN_RELEASED` | re-sample inactive/active level |
| any state | `EV_BTN_FAULT` | `BTN_ERROR` | publish input fault |
| `BTN_ERROR` | `EV_RECOVER_OK` | `BTN_RELEASED` | clear only after GPIO validation |

Invalid transitions must be rejected and logged.

## Button Classifier State Machine

The classifier turns physical button events into logical events and chord masks.

| State | Meaning |
|---|---|
| `BTN_CLASSIFIER_IDLE` | no pending button classification |
| `BTN_CLASSIFIER_SINGLE_PENDING` | one press is pending chord-window expiry |
| `BTN_CLASSIFIER_CHORD_PENDING` | multiple buttons observed inside chord window |
| `BTN_CLASSIFIER_CHORD_ACTIVE` | chord event emitted, waiting for release |
| `BTN_CLASSIFIER_REPEAT_ACTIVE` | repeat events active for one or more held buttons |
| `BTN_CLASSIFIER_LOCKED` | classifier suppressed by modal/power/maintenance policy |

Classifier rules:

- Chords are emitted as raw button masks.
- Chords must not be mapped to actions in Platform.
- Repeat events are generated by Platform but may be ignored by focus/context policy.
- `BTN_BOOT` may participate only in application-visible system maintenance/recovery classification, not Engine/Game input. Firmware cannot classify the early ROM bootloader path.

## Start Shipping Overlay

`BTN_START` has an additional policy overlay because it shares intent with ADP5360 `MR` behavior.

| Overlay State | Meaning |
|---|---|
| `START_IDLE` | Start not pressed |
| `START_NORMAL_PRESS` | normal press window |
| `START_LONG_PRESS` | normal long-press window |
| `START_SHIP_PREP` | firmware should begin save/quiesce preparation |
| `START_SHIP_WARNING` | firmware should present warning/countdown if display policy allows |
| `START_SHIP_IMMINENT` | hardware shipping-mode threshold is approaching |
| `START_RELEASED` | Start released before hardware shipping-mode entry |

Overlay events:

- `EV_START_PRESS`
- `EV_START_LONG_THRESHOLD`
- `EV_START_SHIP_PREP_THRESHOLD`
- `EV_START_SHIP_WARN_THRESHOLD`
- `EV_START_SHIP_IMMINENT_THRESHOLD`
- `EV_START_RELEASE`

Published events:

- `INPUT_START_SHIP_PREP`
- `INPUT_START_SHIP_WARNING`
- `INPUT_START_SHIP_IMMINENT`
- `INPUT_START_RELEASED_BEFORE_SHIP`

These events are power/save intent. They are not game input.

### HW6 FW0 Start Overlay Scaffold Status

Current FW0 scaffolding admits `BTN_START` into the EXTI-backed input path and keeps it out of the ordinary A/B/L/R UI button queue. The EXTI handler only latches press/release edges; `thInput` consumes those latches on its ThreadX tick and also samples the live PA4 level on the bounded input-owner heartbeat. START press acceptance is edge-assisted when PA4 is raw low, missed press edges are recovered from a debounced stable-low PA4 sample, and release is accepted only after the debounced stable-high PA4 level confirms it. `thInput` arms START hold checkpoints from the same ThreadX tick domain, tracks the `START_*` overlay states, and publishes shipping-prep, warning, imminent, and release-before-ship events to `thPower`. `HAL_GetTick()` may be used for debug timestamps on edges, but it is not authoritative for START hold classification.

`thPower` treats those events as lifecycle intent: it records debugger-visible counters, enters existing `PWR_SHIP_PREP` / `PMIC_SHIP_PENDING` scaffold states, calls a no-op save/quiesce placeholder on ship-prep, and cancels back toward the prior active power state on release before hardware shipment entry. `thUI` now has a scaffold `SHUTDOWN` page for prep/warning/imminent states and release-cancel routing back to the prior page. This is not final product behavior: it does not yet save real state, persist settings, enter STOP, prove first-boot policy, or automatically enter ADP5360 software shipment unless the protected power knob explicitly enables that request.

Validated HW6 unit 001 FW0 evidence: a 5 s hold reached `START_SHIP_PREP` with checkpoint/live ticks `500/525`; a 9-10 s hold reached `START_SHIP_WARNING` with `900/1075`; a >11 s hold reached `START_SHIP_IMMINENT` with `1100/1675`; raw/stable PA4 was `0/0` during sustained holds; and `thPower` recorded prep/warning/imminent counters `1/1/1`. A later validated scaffold capture showed the user-visible `PREPARING` and `POWER OFF IN 3/2/1` screens, release before shipment returning to HOME, input prep/warn/imminent/release counters `1/1/1/1`, power prep/warn/imminent/cancel counters `1/1/1/1`, one no-op quiesce hook call, and software shipment remaining gated off with enable/request/skip `0/0/1`. The ADP5360 hardware shipment path was confirmed separately by holding START/MR past the hardware threshold.

The required knob names above are authoritative. HW6 FW0 sources knob values from `firmware/peepshow_hw6_fw0/config/knobs.json`, validates them with `config/knobs.schema.json`, and generates `Core/Inc/knobs_autogen.h` through `tools/gen_knobs.py`. The START overlay scaffold consumes generated START macros. The A/B/L/R physical button FSM consumes the generic debounce, long-press, repeat, stuck, and chord-window knobs. API version 9 adds the logical press record and delivery-policy probe, but long/repeat/stuck/chord publication remains scaffolded until the classifier emits those logical event types and the focus policy consumes them.

Current generated defaults preserve the validated START scaffold shape while allowing timing adjustment: generic press/release debounce `20/20 ms`, generic long press `1000 ms`, generic repeat start/period `500/150 ms`, generic stuck threshold `30000 ms`, chord window `80 ms`, START long press `1000 ms`, ship-prep `5000 ms`, warning `9000 ms`, imminent `11000 ms`, START stable-level acceptance `2` samples, and software shipment request enable `false`.

## Logical Event Model

Platform may publish:

- `INPUT_BUTTON_DOWN`
- `INPUT_BUTTON_UP`
- `INPUT_BUTTON_PRESS`
- `INPUT_BUTTON_LONG_PRESS`
- `INPUT_BUTTON_REPEAT`
- `INPUT_BUTTON_CHORD`
- `INPUT_BUTTON_STUCK`
- `INPUT_BUTTON_MAINTENANCE`
- `INPUT_START_SHIP_PREP`
- `INPUT_START_SHIP_WARNING`
- `INPUT_START_SHIP_IMMINENT`

Payload should include:

- button ID
- active level
- duration in ms
- repeat count
- chord mask
- timestamp
- source, such as normal, wake, maintenance, or power intent

Exact payload shape belongs in [[Interface_Control_Document]].

## Wake Policy

| Button | Wake Policy |
|---|---|
| `BTN_START` | normal focus-routed input when admitted; system-owned unlock, shipping, and power intent take precedence |
| `BTN_A` / `BTN_B` / `BTN_L` / `BTN_R` | normal declared reactive wake/input sources while unlocked |
| `BTN_BOOT` | ROM bootloader before app if sampled high at reset; otherwise application-visible maintenance/recovery only, not normal input |

`BTN_START` shipping intent must be routed to power/save policy early enough for state preservation work.

## PeepOS Input Lock Overlay

Automatic input locking is optional package policy enforced by PeepOS. When the overlay is active:

- only `BTN_START` is armed as the normal interaction wake source
- A/B/L/R, encoder, and joystick interaction are not delivered as package input
- the Start press that unlocks the device is consumed by the system overlay
- normal package focus is restored only after wake/resume and unlock routing complete
- Engine receives symbolic `DEVICE_LOCKED` and `DEVICE_UNLOCKED` lifecycle events rather than the consumed raw unlock press
- Start long-hold/shipping behavior remains Platform-owned and takes precedence over package bindings

The package chooses whether automatic locking is enabled and chooses its declared lock route. The Platform owns the wake mask, suppression, debounce, and unlock ordering.

## Mode Behavior

| Mode | Button Policy |
|---|---|
| `SHELL` | normal Start/A/B/L/R input; maintenance handling for application-visible `BTN_BOOT` |
| `LP_GRAPH` | declared focus/wake set is armed while unlocked; Start-only behavior applies only under the system lock overlay |
| `LP_MODULE` | declared focus/wake set and repeat policy while unlocked |
| `RT_SCENE` | focus-controlled button set; repeats/chords allowed if requested |
| `INSTALLER` | local navigation subset only; Start power intent remains active |
| LOCKED overlay | Start-only wake/unlock; all other normal package interaction suppressed |

## Validation Cases

1. active-high buttons debounce correctly, survive quick taps, and emit press/release duration
2. active-low Start debounce correctly emits normal press/long press
3. repeat generation can be enabled and ignored by focus policy
4. chord window emits raw chord masks without Platform action mapping
5. `BTN_BOOT` is excluded from Engine/Game input and early BOOT0 ROM entry is not claimed as firmware-handled
6. Start shipping-prep event fires before warning/imminent thresholds
7. Start release before hardware shipping threshold cancels firmware countdown
8. stuck button suppresses repeat spam and emits fault
9. wake from Start works from supported low-power modes
10. optional wake buttons obey Platform policy
11. locked overlay arms only Start for normal interaction wake
12. unlock Start press is consumed and does not leak into the package action stream

Related:

- [[Input_Index]]
- [[Subsystem_State_Machines]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Wake_Sources]]
- [[Power_and_Sleep_Policy]]
