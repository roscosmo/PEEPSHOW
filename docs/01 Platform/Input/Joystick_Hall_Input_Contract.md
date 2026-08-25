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
- low-power bring-up policy: X/Y omnipolar magnetic switch through TMAG `INT`

Per [[Platform_Hardware_Abstraction_Contract]], the joystick driver uses the public 7-bit address `0x34`; STM32 HAL shifted-address handling is hidden inside the `ps_hw_i2c3` layer.

---

## Ownership

- `thSensor` owns TMAG state, configuration, I2C transactions, sample acquisition, recovery, normalization, and publication into the ordered input path.
- `thInput` owns routing policy and delivery of normalized joystick events to Shell or Runtime.
- I2C transactions must be serialized with other `I2C3` users.
- ISRs enqueue `JOY_INT` events only.
- Engine and Reference Game code consume normalized joystick events or snapshots only.
- Game code must not read TMAG registers directly.

---

## Usage Model

The joystick is a primary input device, but the Platform must remain usable without it through safe-mode navigation.

Low-power input model:

1. Configure TMAG wake-and-sleep mode with X/Y enabled and an omnipolar
   magnetic switch threshold on both axes.
2. Enter `JOY_THRESHOLD_ARMED` while the joystick path sleeps between sensor
   conversions.
3. The active-low `JOY_INT` wakes/notifies when either X or Y crosses its
   positive or negative operating threshold.
4. The ISR latches and queues work only; `thSensor` wakes the TMAG and performs
   a bounded I2C read before publishing normalized input to the input router.
5. Raw magnetic readings are normalized through the active calibration into one
   dominant cardinal direction.
6. The joystick returns to threshold-armed or polling mode according to active
   policy.

TMAG3001 magnetic wake-on-change only monitors the first enabled axis according
to `MAG_CH_EN`, so it cannot represent arbitrary two-axis joystick movement.
Angle wake-on-change was also rejected on HW6 because the calculated angle is
unstable near neutral where X/Y field magnitude is small; an `18 degree` setting
caused repeated neutral wakes. Omnipolar switch mode monitors both enabled axes,
uses the same operating point for positive and negative fields, and keeps a
level output asserted until all enabled axes fall below the release point.
Firmware still owns calibrated cardinal classification after wake; the hardware
switch is only a wake request.

During STOP2 quiesce, `thSensor` clears stale joystick EXTI state, verifies all
non-terminal TMAG configuration writes, writes wake-and-sleep operating mode
last, and performs no post-terminal I2C read. The initial tuning values are a
`20 ms` wake-and-sleep period, field-threshold code `48`, and maximum hardware
hysteresis code `7`, all supplied through the knobs system. Threshold code `48`
corresponds to `12288` counts in the 16-bit magnetic result. These remain
bring-up values until response, neutral residency, and current are measured on
target.

2026-08-25 HW6 target testing accepted this configuration as the movement-wake
baseline. Four-way visual testing confirmed that positive and negative X/Y
movement each wake the MCU from STOP2 and that neutral returns promptly to
STOP2. The aggregate proof recorded joystick IRQ/enqueue/dequeue counts of
`14/14/14` and `14` joystick-classified STOP2 wakes, with zero coalesces, drops,
or pending events.
All configuration writes and readbacks passed (`write=0xfff`, `verify=0x7ff`),
and the `thSensor` stack retained `2340` bytes of lower margin. Long-duration
neutral false-wake rate and current remain separate measurements.

Final STOP admission must fail if a joystick event is latched in software, the
input queue is non-empty, or active-low `JOY_INT` on `PC11` is already asserted.
This closes the interval between arming the TMAG and executing WFI.

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
- normal awake input uses a bounded `thInput` sample every
  `KNOB_INPUT_JOYSTICK_AWAKE_POLL_PERIOD_MS`; it runs only in
  `PWR_ACTIVE_LP`/`PWR_ACTIVE_RT`, is disabled throughout calibration and STOP
  transitions, and emits one logical activation per neutral-to-direction or
  direction-switch transition
- awake cardinal sources are published as distinct `JOY_LEFT`, `JOY_RIGHT`,
  `JOY_UP`, and `JOY_DOWN` actions; holding a direction does not repeat it
- only a successfully queued direction activation restarts the STOP2 idle
  window; periodic samples and releases are not meaningful activity
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
This means Z-based wake-on-change is still not accepted for HW6 FW0. X/Y CORDIC
angle wake-on-change was subsequently proven to assert the physical interrupt
path but was rejected because neutral angle noise repeatedly woke STOP2. The
current bring-up strategy is the X/Y omnipolar field switch. The diagnostic path runs
inside `thSensor`, uses bounded ThreadX sleeps and a hard timeout, and writes CSV
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
proven through this acquisition-independent review path. Persistence remains a
separate bring-up milestone. Awake routing and movement-triggered STOP2 wake use
the same normalized routing path and remain provisional until target wake,
ordering, regression, and current evidence is captured.

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
- `JOY_THRESHOLD_ARMED` uses the X/Y omnipolar field switch; it does not publish a direction directly.
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

Baseline omnipolar field-switch policy:

- `Sensor_Config_1.MAG_CH_EN` enables the axes needed for joystick classification.
- `Sensor_Config_1.SLEEPTIME` controls wake/sleep interval in `Operating_Mode = 3h`.
- `Sensor_Config_2` preserves range selection while disabling angle calculation,
  gain selection, threshold direction override, and active-high polarity.
- `Sensor_Config_3.THR_SEL = 2h` selects per-axis magnetic field thresholds.
- `Sensor_Config_3.WOC_SEL = 0h` disables relative wake-on-change.
- `THR_Config_1` and `THR_Config_2` set the same provisional X/Y operating point.
- `THR_Config_3 = 0h` disables the Z threshold.
- `Sensor_Config_4`, `Sensor_Config_5`, and `Sensor_Config_6` are cleared so
  high/tamper thresholds are disabled.
- `INT_Config_1.INT_Mode = 6h` selects omnipolar switch mode through `INT`.
- `Device_Config_2.THR_HYST` sets the release margin; bring-up starts at code `7`.

STOP2 baseline park policy:

- Enable X/Y and select the configured wake-and-sleep period.
- Configure active-low omnipolar switch output on the `INT` pin.
- Write `Device_Config_2.Operating_Mode = Wake-and-Sleep` last.
- Do not read TMAG registers after the terminal write unless deliberately waking it.
- If calibration is missing, use the non-interactive quiet-sleep fallback and retain L/R plus A/B navigation.

Switch mode provides a level output, so the event remains observable while the
MCU wakes. Active-low polarity and the HW6 `JOY_INT` EXTI path must remain
validated against the board circuit and CubeMX configuration.

The fixed field threshold is a hardware proof, not final policy. Production must
derive X/Y operating points from calibration, validate that neutral plus margin
is below both operating points and that all four cardinal endpoints cross them,
and fall back to button-only wake if a unit has insufficient field margin.

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
