# TMAG3001 Joystick Bring-up Runbook

This runbook records the measured HW5 procedure for bringing up the `TMAG3001A1YBGR` hall-effect joystick.

> [!important] HW6 reuse
> HW6 identity, owner-routed terminal inactive entry, and the HW6-native
> TMAG3001 input-driver lifecycle path now pass on unit 001. Bounded FW0 REST
> and full-travel SWEEP raw XYZ CSV captures also pass on unit 001. A FW0
> Z-high magnetic-range diagnostic also validates `Sensor_Config_2`
> override/restore, but Z remains pinned near the negative measurement limit.
> Threshold interrupt, calibration, event wake, and current measurements still
> require HW6 evidence in [[HW6_Brought_Up_Tracker]]. The failed HW5 component
> incident is diagnostic history, not evidence about an HW6 unit.

Related:

- [[Joystick_Hall_Input_Contract]]
- [[Brought_Up_Tracker]]
- [[Debug_Workflows]]

---

## Scope

This runbook covers:

- I2C probe at address `0x34`
- identity/status readback
- threshold interrupt configuration
- `JOY_INT` validation on `PC11` / `EXTI11`
- cardinal direction sampling
- calibration capture and validation
- safe-mode calibration fallback

---

## Preconditions

- `I2C3` bus on `PC0` SCL and `PC1` SDA validated
- `JOY_INT` on `PC11` / `EXTI11` mapped and interrupt path validated
- `thInput` owns joystick policy
- fallback controls are available for safe-mode calibration

---

## Baseline Datasheet Sequence

Initial bring-up uses absolute magnetic threshold detection, not wake-on-change.

Reason: TMAG3001 wake-on-change magnetic mode monitors only the first enabled magnetic axis according to `MAG_CH_EN`. A joystick must wake reliably from X or Y movement, so the baseline uses B-field thresholds across the required axes and then classifies direction from a bounded sample read.

Known datasheet values:

- I2C address: `0x34`
- `Device_ID`: `0x0D`
- `Manufacturer_ID_LSB`: `0x0E`, expected `0x49`
- `Manufacturer_ID_MSB`: `0x0F`, expected `0x54`
- `Device_Config_1`: `0x00`
- `Device_Config_2`: `0x01`
- `Sensor_Config_1`: `0x02`
- `Sensor_Config_2`: `0x03`
- `THR_Config_1`: `0x04`
- `THR_Config_2`: `0x05`
- `THR_Config_3`: `0x06`
- `Sensor_Config_3`: `0x07`
- `INT_Config_1`: `0x08`
- `Sensor_Config_4`: `0x09`
- `Sensor_Config_5`: `0x0A`
- `Sensor_Config_6`: `0x0B`
- X result: `0x12:0x13`
- Y result: `0x14:0x15`
- Z result: `0x16:0x17`
- `Conv_Status`: `0x18`
- `Device_Status`: `0x1C`

Baseline configuration policy:

- use `THR_SEL = 2h` for B-field thresholds
- keep `WOC_SEL = 0h`
- keep angle mode disabled for threshold wake
- use TMAG `INT` pin, not SCL interrupt mode
- use latched interrupt for bring-up so wake events are not missed
- leave exact threshold values, hysteresis, range, sleep interval, and axis mapping to measured calibration

---

## Command Sequence Ledger

Record exact transactions here once measured.

| Step | Operation | Register / Address | Value | Delay | Expected Readback | Notes |
|---|---|---|---|---|---|---|
| 1 | probe | `0x34` | I2C address transaction | N/A | ACK | Repaired-board HW5 read-only probe passed on 2026-06-16: `0x34` ACKed, `0x35..0x37` NACKed, and full bus scan found `0x18`, `0x34`, and `0x46`. Earlier damaged-unit scan on 2026-05-30 found only `0x18` and `0x46`. |
| 2 | identity read | `Device_ID` / `0x0D` | read | N/A | version bits consistent with A1 device | Repaired-board readback returned `0x00`; record exact device-version interpretation once the TMAG3001 driver/datasheet mapping is implemented. |
| 3 | manufacturer ID read | `0x0E`, `0x0F` | read | N/A | `0x49`, `0x54` | Repaired-board readback returned `0x49`, `0x54`, confirming TI device family. |
| 4 | status read/clear | `Conv_Status` / `0x18`, `Device_Status` / `0x1C` | read only | N/A | no unexpected faults | Repaired-board readback returned `Conv_Status=0x10`, `Device_Status=0x10`; no clear/write was attempted in the read-only identity probe. |
| 5 | configure averaging/read mode | `Device_Config_1` / `0x00` | experimental `0x00` | `10 ms` after full active setup | readback `0x00` | 2026-06-16 repaired-board active captures wrote/read this value with HAL status `0`; the later TI-driver path used `TMAG3001setSampleRate(0)`. Exact final averaging policy remains pending. |
| 6 | configure range/polarity | `Sensor_Config_2` / `0x03` | experimental `0x00` | `10 ms` after full active setup | readback `0x00` | 2026-06-16 repaired-board active captures wrote/read this value with HAL status `0`; the later TI-driver path used `TMAG3001setRanges(0, 0)`. HW6 FW0 Z-high diagnostic later changed `Sensor_Config_2` `0x0 -> 0x1 -> 0x0` with status `0x0`; the override/restore path works, but Z remains saturated, so this is not final policy. Final range/polarity policy remains pending. |
| 7 | configure threshold type | `Sensor_Config_3` / `0x07` | `THR_SEL = 2h`, `WOC_SEL = 0h` | TBD | TBD | B-field thresholds, wake-on-change disabled |
| 8 | configure low thresholds | `THR_Config_1..3` / `0x04..0x06` | measured X/Y/Z low thresholds | TBD | TBD | values come from calibration/bring-up |
| 9 | configure high thresholds | `Sensor_Config_4..6` / `0x09..0x0B` | measured X/Y/Z high thresholds | TBD | TBD | valid when `THR_SEL = 2h` and angle mode disabled |
| 10 | configure INT output | `INT_Config_1` / `0x08` | `Threshold_INT = 1`, `INT_Mode = 1h`, latched | TBD | TBD | use TMAG `INT`; do not use SCL interrupt |
| 11 | enter threshold-armed mode | `Device_Config_2` / `0x01` and `Sensor_Config_1` / `0x02` | experimental active capture used `Device_Config_2=0x02`, `Sensor_Config_1=0x70` | `10 ms` after writes | readback `0x02`, `0x70` | Active continuous/sample mode only, not threshold-armed mode. 2026-06-16 repaired-board active captures wrote/read both values with HAL status `0`; the later TI-driver path used `TMAG3001enableMagChannels(MAG_CH_EN_XYZ)` and `TMAG3001enterContinuousMeasureMode()`. Final wake-up/sleep and threshold mode remain pending. |
| 12 | bounded sample read | `0x12..0x18` | read X/Y/Z and status | read only after experimental active setup | read succeeds and raw values are captured | Repaired-board read-only result-window probe returned HAL status `0`, bytes `{0x00,0x00,0x00,0x00,0x00,0x00,0x10}`, decoded `X=0`, `Y=0`, `Z=0`, and window `Conv_Status=0x10`. After raw experimental active setup on 2026-06-16, capture completed `200/200` samples with `read_error_count=0`, `i2c_error_after=0`, and non-zero changing XYZ sample points, for example sample 0 `X=0xDA40`, `Y=0xCE10`, `Z=0x8170`, sample 80 `X=0x37F0`, `Y=0x00D0`, `Z=0xE050`, and sample 199 `X=0x0450`, `Y=0xF0E0`, `Z=0x0060`. After TI-driver active setup on 2026-06-16, capture again completed `200/200` samples with `read_error_count=0`, `i2c_error_after=0`, and non-zero changing XYZ sample points, for example sample 0 `X=0xD060`, `Y=0xCD80`, `Z=0xCE20`, sample 80 `X=0x05C0`, `Y=0xF140`, `Z=0x0090`, and sample 199 `X=0x0560`, `Y=0xF140`, `Z=0x0080`. HW6 FW0 evidence on 2026-08-14 adds bounded owner-routed CSV captures: REST/flick `256/256` samples with zero errors, full-travel SWEEP `512/512` samples with zero errors, SWEEP `X=-24368..27632`, `Y=-28832..21232`, `Z=-32576..-32528`, and max Z delta `48`. A follow-up Z-high range capture completed `512/512` samples with zero errors and verified `Sensor_Config_2` override/restore `0x0 -> 0x1 -> 0x0`, but Z stayed pinned at `-32592..-32560` with max delta `32`. Exact signed scaling, axis polarity/order, neutral/range calibration, and threshold behavior remain pending; Z-based wake-on-change is not accepted for HW6 FW0 without new evidence. |
| 13 | interrupt clear validation | TMAG addressed by valid I2C access | read or write | TBD | `JOY_INT` deasserts | exact clear behavior must be measured |
| 14 | enter inactive sleep baseline | `Device_Config_2` / `0x01` | low-current standby bits verified first, then `Operating_Mode=1h` as the terminal write | device timing only | no post-write I2C read | TMAG3001 recognizes its address while asleep, wakes, and deliberately NACKs. A readback probe therefore both creates a false error and leaves the device awake. Validate readable configuration before this write; afterward use the successful terminal write plus current evidence. |

---

## Validation Procedure

1. Probe `0x34` and record ACK/failure.
2. Read identity/status registers and record values.
3. Configure threshold interrupt behavior.
4. Confirm `JOY_INT` fires on joystick movement.
5. Read the device after interrupt and classify cardinal direction bits.
6. Confirm wake-on-change is not required for normal joystick wake.
7. Run center/range calibration and store calibration data.
8. Reboot with valid calibration and confirm normal input starts.
9. Reboot with missing/invalid calibration and confirm safe-mode calibration starts.
10. Validate encoder and L/R navigation in joystick safe mode.
11. Force or simulate an I2C/config fault and validate recovery or safe mode.
12. For an inactive-sleep test, perform no TMAG-addressed I2C transaction after the terminal sleep write until an intentional wake is required.

---

## HW6 FW0 Raw XYZ Capture Helpers

The current diagnostic path is owner-routed through `thInput`; it must not be
used as a production joystick API. It exists to collect calibration and
threshold-planning data in CSV-compatible form.

REST/flick capture:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_rest_start.gdb
```

Continue the target, repeatedly flick/release the joystick and let it settle,
then interrupt after the display cue completes. Expected runtime is about
5 seconds, with a 15 second hard timeout.

Full-travel SWEEP capture:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_start.gdb
```

Continue the target, move through full travel, circles, edges, and corners, then
interrupt after the display cue completes. Expected runtime is about 10 seconds,
with a 15 second hard timeout.

Z-high range SWEEP capture:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_zhigh_start.gdb
```

Continue the target, move through full travel, circles, edges, and corners, then
interrupt after the display cue completes. Expected runtime is about 10 seconds,
with a 15 second hard timeout. This diagnostic temporarily sets the Z magnetic
range bit in `Sensor_Config_2` and must restore the register before returning
TMAG3001 to sleep.

After any capture:

```gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_prints.gdb
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_dump_csv.gdb
```

Expected successful REST evidence is `capture mode/status = 1 / 0x0`,
`period/requested/capacity = 1 / 256 / 512`, and
`records success/error = 256 / 256 / 0`. Expected successful SWEEP evidence is
`capture mode/status = 2 / 0x0`, `period/requested/capacity = 1 / 512 / 512`,
and `records success/error = 512 / 512 / 0`. Expected successful Z-high SWEEP
evidence is `capture mode/status = 3 / 0x0`, `records success/error = 512 / 512 / 0`,
`sensor cfg2 stat/restore stat = 0x0 / 0x0`,
`sensor cfg2 before/active/restore = 0x0 / 0x1 / 0x0`, and
`range override mask/value/applied = 0x1 / 0x1 / 1`.

The dump helper writes mode-specific files:

- `firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_rest_capture.csv`
- `firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_capture.csv`
- `firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_sweep_zrange_capture.csv`

The first REST diagnostic initially exposed a `thInput` stack overflow. The
current FW0 diagnostic budget is `KNOB_RTOS_INPUT_STACK_BYTES = 1536`; do not
raise this casually because a `4096` byte input stack exhausted the current
ThreadX byte-pool budget before all owner threads could be created.

---

## Bring-Up Decisions To Measure

The following values must be measured on HW6 hardware and recorded before the joystick contract is considered implementation-ready:

- physical X/Y/Z sign mapping for the mounted sensor and magnet
- whether Z is needed for wake/classification or diagnostics only
- chosen `MAG_CH_EN` value
- selected X/Y and Z magnetic ranges
- neutral center and usable travel ranges
- threshold counts for each direction
- hysteresis setting that avoids chatter near center
- wake/sleep interval that balances response and current
- final `JOY_INT` polarity and CubeMX EXTI edge selection
- whether latched interrupt remains final policy or only bring-up policy

---

## Evidence

Every successful validation must link evidence from [[Brought_Up_Tracker]].

- 2026-07-31: `EV-HW6-20260731-P5-OWNERS-004` proves identity, readable
  pre-terminal configuration, successful terminal `Operating_Mode=1h`, and a
  clean I2C controller state without a wake-causing readback.
- 2026-08-01: both lifecycle-v3 cycles passed the expected first-address wake
  failure, bounded identity retry, `Sensor_Config_1=0x70`, continuous mode, and
  terminal sleep write. The overall run failed only on the unrelated IMU wake
  path; exact results are preserved in `EV-HW6-20260801-P5-OWNERS-005`.
- 2026-08-01: `EV-HW6-20260801-P5-INPUT-007` proves the driver-backed
  `thInput` TMAG3001 lifecycle path. The run passed top-level owner success
  `1/1`, masks `0x7F/0x00`, two active/inactive state masks
  `0x3FF/0x3FF`, driver `API/init/state/ops/last = 1/0/3/5/0`, identity
  `00/49/54`, active `Sensor_Config_1=0x70`, active
  `Device_Config_2=0x02`, expected first wake status `5`, retry `0`, and
  terminal sleep recommit.
- 2026-08-14: `EV-HW6-20260814-P4-TMAGXYZ-061` proves bounded FW0
  owner-routed REST and full-travel SWEEP raw XYZ CSV capture. REST/flick
  completed `256/256` samples with zero errors; SWEEP completed `512/512`
  samples with zero errors and measured `X=-24368..27632`,
  `Y=-28832..21232`, `Z=-32576..-32528`, max Z delta `48`.
  This supports calibration and threshold planning only.
- 2026-08-14: `EV-HW6-20260814-P4-TMAGZRANGE-062` proves the FW0
  diagnostic Z-range override and restore path. Z-high SWEEP completed
  `512/512` samples with zero errors, changed `Sensor_Config_2`
  `0x0 -> 0x1 -> 0x0`, and reported applied mask/value `0x1/0x1`.
  Z still measured only `-32592..-32560`, so Z-based wake-on-change remains
  rejected for HW6 FW0 unless new mechanical or sensor-range evidence changes it.

Do not mark joystick threshold wake, direction classification, calibration, or
inactive current known-good without measured target-specific evidence.
