# LIS2DUX12 IMU Bring-up Runbook

This runbook records the measured HW5 procedure for bringing up the `LIS2DUX12TR` IMU, embedded smart functions, and step counter.

> [!important] HW6 reuse
> HW6 identity and owner-routed terminal deep-power-down entry now pass on unit
> 001. Lifecycle-v5 also proves the I2C deep-power-down wake NACK, identity
> retry, low-rate `CTRL5=0x10`, and return to deep-power-down across two
> owner-routed cycles. Embedded-function, interrupt/event, current, step-counter
> retention, and active sensing measurements still require HW6 evidence in
> [[HW6_Brought_Up_Tracker]]. HW5 register values are starting hypotheses only.

Related:

- [[IMU_Contract]]
- [[Brought_Up_Tracker]]
- [[Debug_Workflows]]
- [[Power_Validation]]

---

## Scope

This runbook covers:

- I2C probe at address `0x18`
- identity/status readback
- embedded smart-function enable sequence
- step-counter enable sequence
- interrupt validation on `MPU_INT` / `PB14`
- low-power step-counter retention validation
- recovery after bus or device fault

---

## Preconditions

- `I2C3` bus on `PC0` SCL and `PC1` SDA validated
- `MPU_INT` on `PB14` / `EXTI14` mapped to LIS2DUX12 `INT1`
- `thSensor` owns all IMU transactions
- power policy can hold the sleep floor required by active embedded functions

---

## Baseline Datasheet Sequence

Initial bring-up uses the ST operation-manual step-counter sequence before any power optimization.

This baseline is intentionally not the final low-power configuration. It exists to prove I2C access, embedded-function access, step-counter operation, `INT1` routing, and readback behavior using a known-good setup. After the baseline passes, run a separate optimization pass to measure lower-power modes and polling-only sleep behavior.

Known manual values:

- `WHO_AM_I` register: `0x0F`
- expected `WHO_AM_I`: `0x47`
- `CTRL4 = 0x10`: enable embedded functions
- `FUNC_CFG_ACCESS.EMB_FUNC_REG_ACCESS = 1`: access embedded-function registers
- `PAGE_RW = 0x40`: embedded advanced feature write mode
- `PAGE_SEL = 0x01`: select page 0
- `PAGE_ADDR = 0x5D`: select `PEDO_CMD_REG`
- `PAGE_VALUE = 0x04`: enable pedometer false-positive rejection
- `PAGE_RW = 0x00`: disable write mode
- `EMB_FUNC_EN_A = 0x08`: enable pedometer
- `EMB_FUNC_EN_B = 0x10`: enable pedometer false-positive rejection block
- `EMB_FUNC_INT1 = 0x08`: route step-detection interrupt to `INT1` for bring-up validation only
- `FUNC_CFG_ACCESS.EMB_FUNC_REG_ACCESS = 0`: return to main register page
- `MD1_CFG = 0x01`: route embedded-function interrupt to `INT1` for bring-up validation only
- `CTRL5 = 0x61`: turn on accelerometer at 25 Hz, +/-4 g
- step count readback: `STEP_COUNTER_H:STEP_COUNTER_L` from embedded registers `0x29:0x28`
- step counter reset: set `PEDO_RST_STEP` in `EMB_FUNC_SRC`

Normal firmware policy is polling-only for steps. Do not keep per-step `INT1` wake enabled in the final low-power runtime path.

---

## Command Sequence Ledger

Record exact transactions here once measured.

| Step | Operation | Register / Address | Value | Delay | Expected Readback | Notes |
|---|---|---|---|---|---|---|
| 1 | wake/probe | `0x18` | I2C address transaction | up to 25 ms after first NACK from deep power-down | ACK | HW5 read-only probe passed: `0x18` ACKed and alternate `0x19` NACKed |
| 2 | identity read | `WHO_AM_I` / `0x0F` | read | TBD | `0x47` | HW5 read-only probe passed: driver and raw reads returned `0x47`; raw `STATUS=0x02`, `CTRL1=0x10`, `CTRL4=0x00`, `MD1_CFG=0x00` |
| 3 | enable embedded functions | `CTRL4` | `0x10` | TBD | TBD | enables embedded-function block |
| 4 | enter embedded register page | `FUNC_CFG_ACCESS` | set `EMB_FUNC_REG_ACCESS = 1` | TBD | write-only while enabled | access embedded-function registers |
| 5 | select advanced write mode | `PAGE_RW` | `0x40` | TBD | TBD | select write operation mode |
| 6 | select advanced page | `PAGE_SEL` | `0x01` | TBD | TBD | select page 0 |
| 7 | select pedometer command register | `PAGE_ADDR` | `0x5D` | TBD | TBD | selects `PEDO_CMD_REG` |
| 8 | enable false-positive rejection | `PAGE_VALUE` | `0x04` | TBD | TBD | `FP_REJECTION_EN = 1` |
| 9 | leave advanced write mode | `PAGE_RW` | `0x00` | TBD | TBD | disables write operation mode |
| 10 | enable pedometer | `EMB_FUNC_EN_A` | `0x08` | TBD | TBD | enables step detector/counter |
| 11 | enable rejection block | `EMB_FUNC_EN_B` | `0x10` | TBD | TBD | manual labels this as MLC-backed false-positive rejection |
| 12 | route step IRQ for bring-up | `EMB_FUNC_INT1` | `0x08` | TBD | TBD | bring-up validation only, not final low-power policy |
| 13 | return to main page | `FUNC_CFG_ACCESS` | set `EMB_FUNC_REG_ACCESS = 0` | TBD | TBD | disable embedded register access |
| 14 | route embedded interrupt to INT1 | `MD1_CFG` | `0x01` | TBD | TBD | bring-up validation only |
| 15 | start accelerometer baseline | `CTRL5` | `0x61` | per ODR/write timing | TBD | 25 Hz, +/-4 g baseline |
| 16 | read step count | `STEP_COUNTER_H:L` / `0x29:0x28` | read | TBD | monotonically increasing during walk test | read through embedded-function register access |
| 17 | reset step count | `EMB_FUNC_SRC.PEDO_RST_STEP` | set bit | TBD | bit auto-clears | validates reset path |
| 18 | enter inactive deep-power-down baseline | `SLEEP` / `0x3D` | set `DEEP_PD=1` only after all other shutdown registers are read back | device timing only | no post-write register read | In deep power-down the register map is inaccessible. The first I2C address transaction starts a wake, returns NACK, and can take up to 25 ms. Treat the successful terminal write as the final bus operation and use current evidence for the retained low-power state. |

---

## Validation Procedure

1. Probe `0x18` and record ACK/failure.
2. Read identity/status registers and record values.
3. Apply embedded-function enable sequence.
4. Apply step-counter enable sequence.
5. Confirm step count changes during physical step test.
6. Enter the selected low-power mode that should preserve embedded functions.
7. Confirm step count continues or is retained according to polling policy.
8. Disable per-step interrupt wake for normal low-power runtime.
9. Trigger motion/tap/shake/tilt events and record interrupt behavior separately from step counting.
10. Force or simulate an I2C fault and validate recovery.
11. For a deep-power-down test, perform no LIS-addressed transaction after setting `DEEP_PD`; a readback is a wake command, not verification.

---

## Optimization Pass

After the baseline passes, measure and document the lowest-power configuration that preserves embedded step counting.

The optimization pass must record:

- selected accelerometer ODR and full-scale range
- whether false-positive rejection remains enabled
- MCU sleep class used while step counting is active
- whether I2C3 state is retained or reinitialized across sleep
- measured step-count retention or continued accumulation
- current draw compared with the `CTRL5 = 0x61` baseline
- final polling cadence for step snapshots

Do not skip this pass. The baseline sequence proves functionality; it does not prove the final ULP policy.

---

## Evidence

Every successful validation must link evidence from [[Brought_Up_Tracker]].

- 2026-07-31: `EV-HW6-20260731-P5-OWNERS-004` proves identity, all readable
  shutdown-register writes, exact pre-terminal verification mask `0x3FF`, and
  successful terminal `SLEEP.DEEP_PD=1` with a clean I2C controller state.
- 2026-08-01: the lifecycle-v3 run recorded `HAL_ERROR` at the expected
  first-address wake-NACK point, but did not retain the raw AF error. Firmware
  also incorrectly used the SPI-only
  `EN_DEVICE_CONFIG.SOFT_PD` exit sequence and converted that NACK into
  `IMU_ERROR`. Both cycle transport paths completed; the IMU alone missed the
  active and inactive boundary masks. This diagnostic is preserved in
  `EV-HW6-20260801-P5-OWNERS-005`.
- 2026-08-01: lifecycle-v5 passed on `HW6-UNIT-001` with the official
  LIS2DUX12 driver path. Both cycles accepted the expected I2C
  deep-power-down wake NACK (`status/error/accepted = 1 / 4 / 1`), retried to
  `WHO_AM_I=0x47`, configured low-rate active mode with `CTRL5=0x10`, and
  returned to terminal deep-power-down. Baseline masks were
  `snapshot/write/verify = 0x7FF / 0x7FF / 0x3FF`, and driver
  `api/init/state/ops/last = 1 / 0 / 3 / 5 / 0`. Evidence is preserved in
  `EV-HW6-20260801-P5-OWNERS-006`.

Do not mark active sensing, embedded functions, wake, current, or the step
counter known-good without measured target-specific evidence.
