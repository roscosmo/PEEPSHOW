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
   a bounded confirmation window before publishing normalized input to the
   input router.
5. STOP2 wake classification accepts two consecutive matching non-neutral
   cardinal samples within at most three reads. If stability is not reached,
   the latest non-neutral sample is the deterministic fallback. Normal awake
   polling remains a single-sample path.
6. Raw magnetic readings are normalized through the active calibration into a
   canonical direction-bit candidate that may contain one horizontal and one
   vertical bit. A separate dominant-axis result provides deterministic
   four-way policy without discarding the canonical diagonal state.
7. The joystick returns to threshold-armed or polling mode according to active
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
last, and performs no post-terminal I2C read. A bounded wake read and settle
must precede quiet-sleep preparation because a previous firmware image may
leave the TMAG in wake-and-sleep mode across an MCU reset. The tuning values are a
`20 ms` wake-and-sleep period and maximum hardware hysteresis code `7`, both
supplied through the knobs system. X/Y field thresholds are derived separately
from the active unit calibration. If that derivation rejects an otherwise valid
calibration, FW0 temporarily retains movement wake with the knob-controlled
fixed threshold baseline instead of silently quiet-sleeping the sensor. The
profile probe preserves the derivation failure reason while reporting the wake
path as armed, so this fallback is distinguishable from a derived profile.
Each threshold-code step corresponds to `256` counts in the 16-bit magnetic
result.

2026-08-25 HW6 target testing accepted fixed threshold code `48` as the
movement-wake bring-up baseline on the tested unit. Four-way visual testing
confirmed that positive and negative X/Y movement each woke the MCU from STOP2
and that neutral returned promptly to STOP2. The aggregate proof recorded
joystick IRQ/enqueue/dequeue counts of
`14/14/14` and `14` joystick-classified STOP2 wakes, with zero coalesces, drops,
or pending events.
All configuration writes and readbacks passed (`write=0xfff`, `verify=0x7ff`),
and the `thSensor` stack retained `2340` bytes of lower margin. Matched five-minute
STOP2 measurements recorded `55 uA` with joystick W&S disabled and `65 uA` with
W&S enabled, so movement wake adds approximately `10 uA` and remains inside the
`100 uA` STOP2 budget. Long-duration neutral false-wake rate remains a separate
measurement.

That fixed code is not population evidence. A later battery-unit test showed
that code `48` could arm successfully while a deliberate RIGHT movement did not
produce a physical `JOY_INT` edge. Unit-to-unit rest-field offset and magnet
recentering therefore have to be characterized before selecting production
threshold policy.

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
- canonical direction mask, including diagonals
- deterministic dominant-axis direction
- active/inactive flag
- calibration-valid flag
- sample age

Raw magnetic values are diagnostics and calibration inputs only. They must not be normal game-facing API.

The canonical direction mask may represent diagonals by setting one horizontal
and one vertical direction bit. Four-way consumers use the separate resolved
direction: retain the previous selected axis while it remains valid, otherwise
select the greater normalized magnitude, with horizontal as the fixed exact-tie
result. STOP2 wake confirmation remains cardinal-only.

TMAG result registers are read as signed diagnostic values before normalization:

- X: `X_Result_MSB:LSB` at `0x12:0x13`
- Y: `Y_Result_MSB:LSB` at `0x14:0x15`
- Z: `Z_Result_MSB:LSB` at `0x16:0x17`

The public joystick API must not expose these raw register values except through diagnostics/calibration tooling.

---

## HW6 FW0 Bring-Up Status

On HW6 unit 001, FW0 has proven TMAG3001 identity, driver-backed owner wake and
sleep cycles, raw/live sample capture, and the first complete guided calibration
path. FW0 now implements the protected A/B calibration-record path and boot
admission policy; save, reset reload, and active-calibration restoration are
target-validated.

Current FW0 calibration status:

- raw X/Y/Z diagnostic sampling works through `thInput`
- bounded REST and full-travel SWEEP raw XYZ CSV capture helpers are target-validated
- a fixed-point normalized joystick API exists for diagnostics and calibration review
- the shell flow captures a bounded set of post-flick neutral returns, then
  UP, RIGHT, DOWN, LEFT, and full travel
- neutral capture requires five deliberate excursion-and-release cycles,
  rejects unstable return windows, and uses the coordinate-wise median of the
  accepted return centroids instead of one initial resting position
- each capture is incremental and owned by `thInput`; progress is published for UI rendering
- opposite cardinal pairs solve a Q20 axis transform without floating point
- the full-travel sweep must reproduce all four cardinal directions before it is accepted
- captured cardinal reach seeds the final envelope so a sparse sweep cannot discard a proven endpoint
- the review screen displays the measured aligned envelope, deadzone, logical
  activation-threshold markers, live aligned marker, and resolved cardinal direction
- normalized classification retains both the canonical four-bit candidate mask
  and a deterministic one-bit dominant-axis result; enter, release, and
  orthogonal-axis switch hysteresis are compile-time knobs
- a held direction remains selected until it falls below the release threshold
  or the orthogonal axis exceeds it by the configured dominance margin
- normal awake input uses a bounded `thInput` sample every
  `KNOB_INPUT_JOYSTICK_AWAKE_POLL_PERIOD_MS`; it runs only in
  `PWR_ACTIVE_LP`/`PWR_ACTIVE_RT`, is disabled throughout calibration and STOP
  transitions, and emits one logical activation per neutral-to-direction or
  direction-switch transition
- movement wake uses `KNOB_INPUT_JOYSTICK_WAKE_CONFIRM_SAMPLES` and
  `KNOB_INPUT_JOYSTICK_WAKE_CONFIRM_STABLE_SAMPLES` to reject the transitional
  vector that can occur while the stick is still travelling after the hardware
  threshold crossing; this confirmation is not applied to awake polling
- awake direction sources are published as `JOY_LEFT`, `JOY_RIGHT`, `JOY_UP`,
  `JOY_DOWN`, `JOY_UP_LEFT`, `JOY_UP_RIGHT`, `JOY_DOWN_LEFT`, and
  `JOY_DOWN_RIGHT`; each STATE declares deterministic `four_way` or canonical
  `eight_way` resolution
- joystick directions use the same `PRESS`, `RELEASE`, one-shot `HOLD`, and
  bounded `REPEAT` lifecycle and timing knobs as A/B/L/R; the shell remains
  deterministic four-way and press-only
- after movement wake, the confirmed canonical candidate and dominant-axis
  result are both retained; the active STATE policy selects the first package
  direction event, so an eight-way STATE does not receive a temporary cardinal
  event before its diagonal
- the shell receives canonical candidate and resolved masks in the same bounded
  input message; the temporary HOME 3x3 diagnostic renders all eight candidate
  directions while an outer marker shows the resolved four-way action. HW6
  target testing visually accepted all four diagonal candidates with zero
  logical drops.
- only a successfully queued direction activation restarts the STOP2 idle
  window; periodic samples and releases are not meaningful activity
- A persists the candidate through `thStorage`; it becomes active only after
  body verification, commit-last publication, and rescan all succeed
- save failure remains on REVIEW with `TRY AGAIN`; B restores the previous
  calibration before persistence begins
- normal joystick polling and routing remain disabled until boot-time record
  resolution completes
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
proven through this acquisition-independent review path. Persistence, awake
cardinal routing, movement-triggered STOP2 wake, four-direction wake
classification, and the matched STOP2 current comparison are target-validated.
Canonical awake diagonal publication and the temporary HOME diagnostic are
target-validated across all four diagonal directions.

### Guided Wake-Threshold Characterization

FW0 provides a temporary, nonpersistent population-characterization path on
the `INPUT TEST` page. It is debugger-armed only and does not change the saved
calibration or the production threshold fallback.

- A valid saved calibration is required so the diagnostic starts from a known
  joystick configuration. Characterization does not reuse its neutral returns.
- Suspend the active package through the system menu before arming the test.
- `thInput` guides CENTER, UP, RELEASE CENTER, RIGHT, RELEASE CENTER, DOWN,
  RELEASE CENTER, LEFT, RELEASE CENTER. The five neutral captures therefore
  come directly from this characterization session.
- At each center prompt, the operator waits for the stick to physically settle
  before pressing A. At each cardinal prompt, the operator holds full travel
  before pressing A. The UI must render `SCANNING` before acquisition proceeds,
  and further A presses are ignored until that capture has completed or failed.
- Every capture samples five bounded raw X/Y observations and rejects excessive
  movement. Center and cardinal captures then perform bounded real TMAG
  wake-and-sleep comparator trials on each axis. Release-center captures finish
  after raw sampling and retain both their centroid and peak absolute sample.
  The acquisition is a deadline-driven, incremental owner state machine; it
  does not sleep or monopolize `thInput` for the whole scan. X trials enable
  only X and Y trials enable only Y, so a strong endpoint on the other axis
  cannot contaminate the result.
- The search records the highest code that asserts at each pose and repeats the
  adjacent boundary observations. It does not claim a full monotonic sweep. A
  measured-axis result at hardware code `127` is accepted and explicitly
  reported as ceiling-censored rather than rejected.
- The candidate X/Y pair is placed above both the measured center assertion
  boundary and every independently captured neutral sample peak. It is feasible
  only if that same pair still covers all four cardinal poses.
- The calibration page and characterization page remain awake; STOP2 is
  deliberately blocked during acquisition.

Use
`__fw0_joystick_wake_characterization_enable.gdb` once to start the guided
flow and `__fw0_joystick_wake_characterization_prints.gdb` once after the
display says `HALT + PRINT`. Results from the PPK2 unit and both battery units
at a common supply voltage are required before this diagnostic can inform a
production threshold decision.

## Calibration Contract

Joystick calibration is required for normal usability.

Rules:

- Calibration lives in the protected calibration storage region.
- Neutral calibration must use an odd, bounded set of stable post-flick return
  centroids; one held resting-position capture is not a valid center estimate.
- The neutral center is the coordinate-wise median of those centroids. After
  the cardinal transform is solved, the deadzone must enclose every accepted
  return centroid plus the configured margin.
- A neutral capture that times out, lacks the required return count, or needs a
  deadzone beyond the accepted aligned limit must fail and remain retryable.
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
- `JOY_INT` does not directly publish direction; it schedules a bounded,
  wake-only stable-direction confirmation window.
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
- `THR_Config_1` and `THR_Config_2` use independently derived X/Y operating
  points, or the proven fixed baseline on both axes when valid-calibration
  derivation rejects.
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
- If calibration is valid but profile derivation rejects, use the explicit fixed
  threshold fallback and retain the rejection reason in telemetry.

Switch mode provides a level output, so the event remains observable while the
MCU wakes. Active-low polarity and the HW6 `JOY_INT` EXTI path must remain
validated against the board circuit and CubeMX configuration.

The fixed field threshold was a hardware proof, not production policy. Firmware
maps the calibrated runtime-neutral envelope back through the saved transform,
then places each X/Y operating point one hardware code above the corresponding
maximum raw-field magnitude. It also reconstructs all four calibrated cardinal
endpoints and requires each endpoint to cross at least one operating point. A
missing, malformed, saturated, or insufficient-range calibration selects quiet
TMAG sleep and button-only wake instead of arming an unsafe field threshold. This
derivation uses the existing persistent calibration format.

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
