# Joystick Hall Input Contract

This document defines the Platform contract for the retained HW6 hall-effect joystick. Electrical behavior and calibration remain pending HW6 revalidation.

Related:

- [[Input_Index]]
- [[Subsystem_State_Machines]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Wake_Sources]]
- [[TMAG3001_Joystick_Bring-up_Runbook]]
- [[Storage_and_Installer_Contract]]

---

## Hardware

Part: `TMAG3001A1YBGR`.

Confirmed configuration:

- I2C address: `0x34`
- shared I2C bus: `I2C3` on `PC0` SCL and `PC1` SDA
- threshold interrupt: `JOY_INT` on `PC11` / `EXTI11`
- default low-power interrupt policy: absolute magnetic threshold detection through TMAG `INT`

Per [[Platform_Hardware_Abstraction_Contract]], the joystick driver uses the public 7-bit address `0x34`; STM32 HAL shifted-address handling is hidden inside the `ps_hw_i2c3` layer.

---

## Ownership

- `thInput` owns joystick device policy, threshold configuration, calibration use, sample requests, normalization, and logical event publication.
- I2C transactions must be serialized with other `I2C3` users.
- ISRs enqueue `JOY_INT` events only.
- Engine and Reference Game code consume normalized joystick events or snapshots only.
- Game code must not read TMAG registers directly.

---

## Usage Model

The joystick is a primary input device, but the Platform must remain usable without it through safe-mode navigation.

Low-power input model:

1. Configure TMAG absolute magnetic threshold detection.
2. Enter `JOY_THRESHOLD_ARMED` while the joystick path sleeps between events.
3. `JOY_INT` wakes/notifies when threshold is crossed.
4. `thInput` performs a bounded I2C read.
5. Raw magnetic readings are normalized into cardinal direction bits and a normalized vector.
6. The joystick returns to threshold-armed or polling mode according to active policy.

Wake-on-change is not the default joystick wake policy. TMAG3001 wake-on-change magnetic mode only monitors the first enabled magnetic axis according to `MAG_CH_EN`, which is not a good fit for a two-axis joystick that must wake reliably from X or Y movement.

Absolute magnetic thresholds are the baseline because they can be configured across multiple axes, then firmware can read the result registers and classify direction using calibration.

STOP2 baseline current policy is stricter than the normal low-power input model: until threshold and wake-and-sleep behavior are measured, `thInput` must terminally park TMAG3001 in sleep during STOP2 quiesce, write sleep last, and avoid any post-sleep I2C read that would immediately wake the part. The STOP2 quiesce implementation must keep the `INT_Config_1` target as a policy value, not bake in a permanent no-wake setting, so future wake-and-sleep or threshold-armed STOP2 modes can be selected without changing the driver sequencing.

Realtime model:

- use `JOY_FAST_POLL` for gameplay or calibration that needs continuous feedback
- use `JOY_SLOW_POLL` for menus, shell, and coarse interaction
- stop polling explicitly when the focus owner no longer needs it

---

## Public Data Contract

Public Engine/UI data is normalized only.

Allowed public fields:

- normalized X/Y vector
- deadzone-applied X/Y vector
- magnitude
- cardinal direction mask
- active/inactive flag
- calibration-valid flag
- sample age

Raw magnetic values are diagnostics and calibration inputs only. They must not be normal game-facing API.

Cardinal direction mask may represent diagonals by setting more than one direction bit.

TMAG result registers are read as signed diagnostic values before normalization:

- X: `X_Result_MSB:LSB` at `0x12:0x13`
- Y: `Y_Result_MSB:LSB` at `0x14:0x15`
- Z: `Z_Result_MSB:LSB` at `0x16:0x17`

The public joystick API must not expose these raw register values except through diagnostics/calibration tooling.

---

## HW6 FW0 Bring-Up Status

On HW6 unit 001, FW0 has proven TMAG3001 identity, driver-backed owner wake and
sleep cycles, raw/live sample capture, and the first complete guided calibration
path. Calibration remains volatile until the protected-record persistence path
is implemented, so it is not yet accepted as boot-time production policy.

Current FW0 calibration status:

- raw X/Y/Z diagnostic sampling works through `thInput`
- bounded REST and full-travel SWEEP raw XYZ CSV capture helpers are target-validated
- a fixed-point normalized joystick API exists for diagnostics and calibration review
- the shell flow captures neutral, UP, RIGHT, DOWN, LEFT, and full travel
- each capture is incremental and owned by `thInput`; progress is published for UI rendering
- opposite cardinal pairs solve a Q20 axis transform without floating point
- the full-travel sweep must reproduce all four cardinal directions before it is accepted
- captured cardinal reach seeds the final envelope so a sparse sweep cannot discard a proven endpoint
- the review screen displays the measured aligned envelope, deadzone, logical
  activation-threshold markers, live aligned marker, and resolved cardinal direction
- normalized cardinal classification is deterministic and dominant-axis only;
  enter, release, and orthogonal-axis switch hysteresis are compile-time knobs
- a held direction remains selected until it falls below the release threshold
  or the orthogonal axis exceeds it by the configured dominance margin
- A applies the volatile candidate and B restores the previous calibration
- L/R plus A/B remain the required fallback controls while calibration is
  missing or invalid

2026-08-14 HW6 FW0 diagnostic captures record the current measured raw range
needed for calibration and threshold planning. The REST/flick capture completed
`256/256` samples with no read errors, and the full-travel SWEEP capture
completed `512/512` samples with no read errors. The normal SWEEP range was
`X=-24368..27632`, `Y=-28832..21232`, and `Z=-32576..-32528`; the Z axis was
effectively pinned over this capture, with maximum observed absolute delta `48`.
A follow-up Z-high range diagnostic validated the `Sensor_Config_2` override and
restore path (`before/active/restore = 0x0/0x1/0x0`, `512/512` samples, no read
errors), but Z remained pinned at `-32592..-32560` with maximum delta `32`.
This means Z-based wake-on-change is still not accepted for HW6 FW0; the
baseline wake strategy remains calibrated X/Y absolute magnetic thresholds unless
new mechanical or sensor-range evidence changes that. The diagnostic path runs
inside `thInput`, uses bounded ThreadX sleeps and a hard timeout, and writes CSV
files for offline plotting. This is diagnostic evidence only; it does not
validate final calibration, threshold interrupt values, production wake policy,
or joystick current.

During this work, a REST capture exposed a `thInput` stack overflow while
running the TMAG3001 read/sleep path. FW0 now uses a dedicated
`KNOB_RTOS_INPUT_STACK_BYTES` budget of `1536` bytes. A larger `4096` byte
diagnostic stack did not fit the current ThreadX byte-pool budget, so input
stack sizing remains measured but provisional.

Debugger-only one-position captures are not sufficient calibration evidence for
this joystick. The accepted path is the on-device guided flow: neutral, four
confirmed cardinals, full-travel sweep, then live visual review. The calibration
page must remain awake throughout this sequence. Dominant-axis hysteresis is
proven through this acquisition-independent review path. Persistence, normal
awake input routing, and movement-triggered wake remain separate bring-up
milestones.

## Calibration Contract

Joystick calibration is required for normal usability.

Rules:

- Calibration lives in the protected calibration storage region.
- If no valid joystick calibration is found, normal shell/game input must not start.
- Missing or invalid calibration routes to safe-mode calibration.
- Safe-mode calibration must be navigable without the joystick.
- Encoder and L/R buttons are approved fallback navigation inputs for joystick safe mode.
- After successful calibration, normal input policy may resume.

---

## Failure Policy

Joystick failure is major but recoverable through safe mode.

On joystick fault or invalid calibration:

- do not launch normal shell/game input that depends on joystick
- enter joystick safe mode or broader storage/input safe mode as appropriate
- allow encoder and L/R buttons for navigation until recalibration or recovery succeeds
- keep A/B/START policy available where safe
- publish an input fault for diagnostics

---

## Joystick FSM

| State | Meaning |
|---|---|
| `JOY_OFF` | TMAG joystick is off, idle, or in its lowest-power state. No samples or events are expected. |
| `JOY_PROBE` | Firmware checks that `TMAG3001A1YBGR` responds at I2C address `0x34`. |
| `JOY_CONFIG` | Firmware applies thresholds, filters, interrupt routing, and sample behavior. |
| `JOY_CAL_REQUIRED` | No valid calibration is available, so normal joystick use is blocked. |
| `JOY_CENTER_CAL` | Firmware captures or updates neutral center and range calibration. |
| `JOY_THRESHOLD_ARMED` | Low-power movement-detect mode. `JOY_INT` is armed and the device sleeps between readings. |
| `JOY_WAKE_PENDING` | `JOY_INT` fired; `thInput` is scheduling a bounded sample to classify movement. |
| `JOY_DIRECTION_SAMPLE` | Firmware reads the TMAG device to determine cardinal direction bits and vector. |
| `JOY_SLOW_POLL` | Low-rate polling for shell/menu/coarse input. |
| `JOY_FAST_POLL` | Higher-rate polling for realtime gameplay or calibration feedback. |
| `JOY_NORMALIZE` | Raw diagnostic readings are converted to normalized vector/direction output. |
| `JOY_SAFE_MODE` | Joystick unavailable or uncalibrated; fallback controls are used for recovery/calibration. |
| `JOY_SUSPENDED` | Temporarily parked for sleep, mode transition, or bus recovery. |
| `JOY_RECOVERING` | Firmware is retrying after an I2C, interrupt, configuration, or calibration fault. |
| `JOY_ERROR` | Joystick cannot be trusted after bounded recovery. Safe mode remains active. |

Rules:

- `JOY_OFF` is the default unless input policy requests the joystick.
- `JOY_THRESHOLD_ARMED` is the preferred low-power interactive state.
- `JOY_THRESHOLD_ARMED` uses absolute magnetic threshold detection, not wake-on-change.
- `JOY_INT` does not directly publish direction; it schedules a bounded read.
- `JOY_NORMALIZE` is the only path to public joystick data.
- Missing calibration enters `JOY_CAL_REQUIRED` or `JOY_SAFE_MODE`, not normal runtime.
- `JOY_ERROR` must preserve fallback navigation.

---

## TMAG3001 Register Policy

Known identity/status registers:

- `Device_ID`: `0x0D`
- `Manufacturer_ID_LSB`: `0x0E`, expected `0x49`
- `Manufacturer_ID_MSB`: `0x0F`, expected `0x54`
- `Conv_Status`: `0x18`
- `Device_Status`: `0x1C`

Baseline threshold-wake policy:

- `Sensor_Config_1.MAG_CH_EN` enables the axes needed for joystick classification.
- `Sensor_Config_1.SLEEPTIME` controls wake/sleep interval in `Operating_Mode = 3h`.
- `Sensor_Config_2` owns interrupt polarity, threshold direction, and range selection.
- `Sensor_Config_3.THR_SEL = 2h` selects B-field thresholds.
- `Sensor_Config_3.WOC_SEL = 0h` disables wake-on-change for the default joystick wake policy.
- `THR_Config_1`, `THR_Config_2`, and `THR_Config_3` hold low thresholds for X/Y/Z.
- `Sensor_Config_4`, `Sensor_Config_5`, and `Sensor_Config_6` hold high thresholds for X/Y/Z when `THR_SEL = 2h` and angle mode is disabled.
- `INT_Config_1.Threshold_INT = 1` enables threshold interrupt response.
- `INT_Config_1.INT_Mode = 1h` routes the interrupt through the TMAG `INT` pin.

STOP2 baseline park policy:

- Clear active magnetic channels before terminal sleep.
- Set the selected STOP2 `INT_Config_1` policy target before terminal sleep.
- Write `Device_Config_2.Operating_Mode = Sleep` last.
- Do not read TMAG registers after the sleep write unless deliberately waking it.
- The current quiet baseline target is `INT_Config_1 = 0x01`; future threshold or wake-and-sleep targets may replace this value after validation.

Bring-up should initially use a latched interrupt so a short threshold event cannot be missed while the MCU wakes. Final polarity and edge configuration must be validated against the HW6 `JOY_INT` circuit and CubeMX EXTI settings.

The exact threshold counts, range settings, hysteresis, sleep interval, and axis mapping are calibration/bring-up outputs, not assumptions.

---

## Validation Cases

1. I2C probe at address `0x34`
2. identity/status register readback
3. threshold interrupt configuration
4. `JOY_INT` wake/notification on `PC11` / `EXTI11`
5. bounded read after threshold interrupt
6. cardinal direction classification
7. normalized vector output after calibration
8. missing calibration routes to safe-mode calibration
9. encoder and L/R navigation works in joystick safe mode
10. I2C/config fault routes to recovery or safe mode
