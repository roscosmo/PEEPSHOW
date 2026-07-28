# Rotary Encoder Bring-up Runbook

This runbook records the measured HW5 procedure for the rotary encoder path.

> [!warning] Retired hardware only
> The rotary encoder, `ENC_EN`, and TIM2 encoder path are absent from HW6. This runbook is not part of HW6 arrival or revalidation and must not be applied to the HW6 IOC. Its results remain historical evidence in [[HW5_Brought_Up_Tracker]].

Related:

- [[Rotary_Encoder_Input_Contract]]
- [[HW5_Wake_Sources]]
- [[HW5_Power_Rails]]
- [[Brought_Up_Tracker]]

---

## Scope

This runbook covers:

- `TIM2` encoder mode on `PA5` / `PA1`
- `PB4` `ENC_EN` encoder power enable
- direction and delta classification
- acceleration classification
- off/unpowered behavior
- optional wake behavior through `TIM2_IRQn` when armed
- verification that the rotary path emits deltas only, not navigation actions or button/select events

---

## CubeMX Baseline

Current `.ioc` baseline:

- `PA5` is `TIM2_CH1`
- `PA1` is `TIM2_CH2`
- `TIM2.EncoderMode = TIM_ENCODERMODE_TI12`
- `TIM2_IRQn` is enabled
- `PB4` / `ENC_EN` is `LPGPIO_Output`

Bring-up must use the timer encoder path. Do not replace the hardware validation with ad hoc GPIO edge counting unless explicitly debugging a timer failure.

`ENC_EN` is active high:

- low: encoder circuit unpowered/off
- high: encoder circuit powered/on

Firmware safe default is low/off.

---

## Baseline State Sequence

Use this sequence for first bring-up:

1. Boot with `ENC_EN` low/off.
2. Start `TIM2` encoder mode with the counter reset to a neutral baseline.
3. Verify no encoder events are emitted while `ENC_EN` is low.
4. Assert `ENC_EN`.
5. Wait the measured settle interval before trusting counter movement.
6. Reset the TIM2 counter baseline after settle.
7. Rotate one detent clockwise and record signed delta.
8. Rotate one detent counter-clockwise and record signed delta.
9. Rotate multiple slow detents and record event cadence.
10. Rotate rapidly and record acceleration classification behavior.
11. Disable `ENC_EN`, re-enable it, and verify no stale delta is emitted.

The encoder event contract is signed delta only. Navigation, value adjustment, pet interaction, and game use are mappings performed by the active input focus above the Platform.

---

## Command / Configuration Ledger

Record exact configuration and measured values here once HW5 is available.

| Step | Operation | Peripheral / Pin | Value | Delay | Expected Result | Notes |
|---|---|---|---|---|---|---|
| 1 | safe default check | `PB4` / `ENC_EN` | low | N/A | encoder circuit off | no events should be emitted |
| 2 | timer init check | `TIM2` | encoder mode `TI12` | N/A | counter can track A/B phase | from CubeMX baseline |
| 3 | IRQ path check | `TIM2_IRQn` | enabled | N/A | activity can reach `thInput` | do not publish raw ISR events |
| 4 | power enable | `PB4` / `ENC_EN` | high | measured settle | encoder circuit on | settle time becomes `KNOB_INPUT_ENC_SETTLE_MS` |
| 5 | baseline reset | `TIM2` counter | reset/read baseline | after settle | no stale delta | required after each enable/resume |
| 6 | clockwise test | encoder rotation | one or more detents | N/A | signed delta recorded | sign convention must be documented |
| 7 | counter-clockwise test | encoder rotation | one or more detents | N/A | opposite signed delta recorded | sign convention must be documented |
| 8 | fast rotation test | encoder rotation | rapid detents | N/A | acceleration class recorded | validates event burst limits |
| 9 | disable test | `PB4` / `ENC_EN` | low | N/A | no movement events | confirms powered-off behavior |
| 10 | wake-armed test | `ENC_WAKE_ARMED` | policy-defined | sleep-class dependent | activity detected only in supported sleep class | optional until sleep/wake pass |

---

## Validation Procedure

1. Confirm `ENC_EN` active-high behavior: low powers encoder circuit off, high powers encoder circuit on.
2. Confirm no encoder activity is reported while encoder is disabled.
3. Enable encoder and validate settle time.
4. Validate clockwise delta sign.
5. Validate counter-clockwise delta sign.
6. Validate slow and fast rotation classification.
7. Validate no stale delta is emitted after disable/re-enable.
8. Validate `ENC_WAKE_ARMED` behavior only if the selected sleep class supports it.
9. Measure current impact of keeping encoder armed if possible.
10. Confirm the rotary path never emits a press/select action.
11. Confirm all navigation mapping is performed by input focus, not the encoder driver.

---

## Bring-Up Decisions To Measure

The following values must be measured before implementation is frozen:

- clockwise sign convention
- counter-clockwise sign convention
- detent-to-counter-delta ratio
- required settle time after `ENC_EN`
- whether TIM2 input filtering is needed for this encoder/circuit
- slow polling cadence for shell/menu use
- acceleration threshold and window
- idle timeout before returning to `ENC_IDLE`
- whether `TIM2_IRQn` gives reliable activity detection in the selected sleep class
- current cost of keeping `ENC_EN` asserted in `ENC_WAKE_ARMED`
- selected behavior when the requested sleep class cannot support encoder wake

---

## Evidence Requirements

Record in [[Brought_Up_Tracker]]:

- active-high enable observation
- delta sign test results
- acceleration behavior notes
- disable/no-stale-event test result
- wake result if attempted
- current measurement if available

Encoder events are deltas only. Navigation mapping belongs above Platform.
