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
