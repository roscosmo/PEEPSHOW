# IMU Contract

This document defines the Platform contract for the retained HW6 IMU. Electrical behavior, low-power mode, and wake behavior remain pending HW6 revalidation.

Related:

- [[Sensors_Index]]
- [[Subsystem_State_Machines]]
- [[Power_and_Sleep_Policy]]
- [[HW6_Pin_Ownership_Matrix]]
- [[HW6_Wake_Sources]]
- [[LIS2DUX12_IMU_Bring-up_Runbook]]

---

## Hardware

Part: `LIS2DUX12TR`.

Confirmed configuration:

- I2C address: `0x18`
- shared I2C bus: `I2C3` on `PC0` SCL and `PC1` SDA
- interrupt: `MPU_INT` on `PB14` / `EXTI14`, wired to LIS2DUX12 `INT1`
- supports embedded smart functions
- step counter is an important low-power function

Per [[Platform_Hardware_Abstraction_Contract]], the IMU driver uses the public 7-bit address `0x18`; STM32 HAL shifted-address handling is hidden inside the `ps_hw_i2c3` layer.

---

## Ownership

- `thSensor` owns IMU probing, configuration, embedded-function enable, interrupt handling, sampling, event publication, and recovery.
- ISRs enqueue interrupt events only.
- The Engine and Reference Game request capabilities or consume events/snapshots.
- Game code must not write IMU registers directly.

---

## Usage Model

The IMU supports:

- wake-on-motion
- step counting
- tap/shake gestures
- tilt/orientation gestures
- realtime game input
- diagnostics and bring-up sampling

The IMU defaults fully off unless a system, Engine, Reference Game, or diagnostics mode requests it.

Failure is non-critical and recoverable. Features depending on motion input must degrade cleanly when IMU data is unavailable.

---

## Public Data Contract

The Platform may publish both raw-ish motion data and normalized events.

Raw/sample data:

- timestamped sensor samples
- sample rate
- range/config metadata
- validity and age

Normalized event data:

- step count delta and total snapshot
- motion wake event
- tap/shake event
- tilt/orientation event
- activity classification where supported

The final API must match the physical channels exposed by `LIS2DUX12TR`; do not promise unsupported channels at the Engine API level.

---

## Embedded Step Counter Power Rule

When the step counter is active, the Platform must put the device into the lowest-power sleep mode that preserves the IMU embedded functions, interrupt path, and required state retention.

The absolute deepest sleep mode is not allowed while step counting is required if it would stop embedded functions or lose step-counter state.

`IMU_STEP_COUNTER_ACTIVE` therefore publishes a power floor to `thPower`.

Step counting is polled by the Platform. `MPU_INT` must not wake the MCU for every detected step during normal low-power operation.

The IMU interrupt path remains available for motion, tap/shake, tilt/orientation, diagnostics, and future policy-defined events, but step detection interrupts are bring-up/debug only unless a later measured requirement explicitly changes this contract.

The exact MCU sleep class, IMU register mode, and wake path must be proven on HW6 and recorded in [[HW6_Brought_Up_Tracker]].

---

## IMU FSM

| State | Meaning |
|---|---|
| `IMU_OFF` | IMU is in the lowest-power off/power-down state. No events or samples are expected. |
| `IMU_PROBE` | Firmware checks that `LIS2DUX12TR` responds at I2C address `0x18`. |
| `IMU_CONFIG` | Firmware applies requested range, filters, sample rate, interrupt routing, and feature configuration. |
| `IMU_EMBEDDED_FUNC_ENABLE` | Firmware runs the required embedded smart-function enable sequence. |
| `IMU_STEP_COUNTER_ACTIVE` | Embedded step counter is running, polled by the Platform, and publishing a power floor to preserve it. |
| `IMU_EVENT_ARMED` | Low-power interrupt/event mode for motion, tap/shake, tilt/orientation, or wake gestures. |
| `IMU_LOW_RATE_SAMPLE` | Periodic low-rate sampling for non-realtime features or diagnostics. |
| `IMU_STREAMING` | Higher-rate continuous sampling for realtime gameplay or diagnostics. |
| `IMU_SUSPENDED` | IMU is temporarily parked for sleep transition, mode transition, or bus ownership change. |
| `IMU_RECOVERING` | Firmware is retrying or reinitializing after a bus, register, interrupt, or configuration fault. |
| `IMU_ERROR` | IMU cannot be trusted. Platform degrades motion features and reports a non-critical sensor fault. |

Rules:

- `IMU_OFF` is the default when no owner requests IMU capability.
- `IMU_EMBEDDED_FUNC_ENABLE` must follow the documented bring-up sequence before step counting is trusted.
- `IMU_STEP_COUNTER_ACTIVE` must not be collapsed into generic off/sleep behavior.
- `IMU_STEP_COUNTER_ACTIVE` must not arm per-step MCU wake by default.
- `IMU_STREAMING` must be bounded by power policy and explicit stop conditions.
- All register access is serialized by `thSensor`.

---

## Bring-Up Sequence Requirement

The register sequence for embedded smart functions and step counter enable is operational bring-up knowledge.

It must be recorded in [[LIS2DUX12_IMU_Bring-up_Runbook]] with:

- I2C transaction order
- register addresses and values
- required delays
- expected readbacks
- failure observations
- power-mode assumptions
- step-counter validation evidence

Do not mark embedded functions or step counting known-good from configuration alone.

---

## Validation Cases

1. I2C probe at address `0x18`
2. identity/status register readback
3. embedded smart-function enable sequence
4. step-counter activation and readback
5. step-counter retention across the selected low-power mode
6. motion/tap/shake/tilt interrupt path from `MPU_INT` on `PB14`
7. low-rate sample mode
8. streaming sample mode
9. recovery after I2C/register fault
10. graceful degradation when IMU is unavailable
