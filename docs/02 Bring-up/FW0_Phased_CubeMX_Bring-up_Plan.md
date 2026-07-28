# FW0 Phased CubeMX Bring-up Plan

This document defines the minimal CubeMX and firmware generation path for HW5 hardware bring-up.

> [!warning] Retired plan
> This plan applies only to retired HW5. It must not be used to generate or flash HW6. The repository-contained safe HW6 arrival target is still a blocker in [[HW6_Hardware_Documentation_Readiness]]; use [[HW6_Arrival_Phase0_Checklist]] and [[HW6_Revalidation_Matrix]] for current planning.

`fw0` is the active bring-up firmware target.

`fw1` remains the full/reference PeepOS target until the phased bring-up firmware reaches that scope.

Related:

- [[Bootstrap_and_Build]]
- [[Brought_Up_Tracker]]
- [[HW5_Arrival_Phase0_Checklist]]
- [[ADP5360_Power_Bring-up_Runbook]]
- [[HW5_Hardware_Documentation_Readiness]]
- [[HW5_CubeMX_Pin_Map]]
- [[HW5_Pin_Ownership_Matrix]]
- [[HW5_Power_Rails]]
- [[CubeMX_Configuration_Checklist]]
- [[Bounded_Build_Flash_Debug_Runbook]]

---

## Purpose

The master/full `.ioc` contains the intended final HW5 peripheral and middleware configuration.

That is not the right first image to flash onto fresh hardware.

Early bring-up should use the smallest firmware profile that proves the next hardware dependency. This avoids confusing first hardware faults with middleware, storage, USB, display, audio, BLE, RTOS, package runtime, or sleep behavior.

---

## Firmware Targets

| Target | Path | Role | Bring-up Status |
|---|---|---|---|
| `fw0` | `firmware/peepshow_hw5_fw0/peepshow_hw5_fw0.ioc` | active phased HW5 bring-up firmware | create/use during hardware bring-up |
| `fw1` | `firmware/peepshow_hw5_fw1/peepshow_hw5_fw1.ioc` | full/reference PeepOS target | reference only until phased bring-up catches up |

Rules:

- `fw0` is the only active firmware target during early hardware bring-up.
- `fw1` must not be flashed just because it exists.
- `fw1` may be used as reference for final intended pin/peripheral coverage.
- when `fw0` reaches full PeepOS hardware scope, decide whether to promote/rename `fw0` into `fw1`, archive `fw0`, or replace the old `fw1`.
- this decision is deferred until measured bring-up evidence exists.

---

## Core Rule

Do not enable a CubeMX peripheral, middleware stack, DMA channel, interrupt, or generated init path in `fw0` until the current bring-up phase needs it.

Do not add ThreadX, FileX, LevelX, USBX, display transactions, audio output, BLE/NINA release, storage writes, STOP modes, or package/runtime code to `fw0` before the relevant phase.

---

## Agent Workflow

For each bring-up phase, the coding agent should:

1. read this plan and the relevant subsystem runbook.
2. inspect the current `fw0` `.ioc`.
3. tell the user exactly what to configure in CubeMX for the next phase.
4. wait for the user to perform or approve CubeMX changes where GUI work is needed.
5. inspect generated output after generation.
6. implement only the smallest firmware code needed for the current test.
7. run bounded build only when asked or clearly approved.
8. ask before any flash, reset, debugger attach, or hardware command.
9. record results only when measured evidence is provided or the user asks to update docs.

The agent should not continue broad passive checklist work when the next phase requires firmware/CubeMX setup.

---

## Phase 0A: Safe Generated Baseline

Goal:

```text
Generate and build the smallest recoverable HW5 firmware image.
```

Enable/configure:

- STM32U575RITx target package matching HW5
- SWD debug
- conservative clock baseline sufficient for boot and I2C
- GPIO safe defaults required to avoid external hardware hazards
- no RTOS
- no middleware
- no USB device stack
- no storage stack
- no display transactions
- no audio output
- no BLE/NINA release
- no sleep/STOP

Required safe defaults:

- display level translator disabled where applicable
- speaker amplifier disabled
- NINA held reset or left in documented safe state
- NINA `SW1`, `SW2`, `DTR`, and `DSR` high-Z/no-pull unless explicitly needed later
- external flash chip select inactive
- no peripheral enable pin driven active unless the current test requires it

Firmware goal:

- boot
- preserve debug attach/recovery
- optionally expose a small debugger-visible status struct

Exit evidence:

- generated project path recorded
- bounded build passes
- flash/debug attach only after user approval
- no unexpected current/thermal behavior after flash

---

## Phase 0B: ADP5360 I2C Probe

Goal:

```text
Prove PMIC I2C communication and basic status readback.
```

Enable/configure:

- `I2C3`
- `PC0` SCL
- `PC1` SDA
- timing suitable for conservative standard-mode or safe initial I2C operation
- optional `PMIC_INT` input/EXTI only if needed for this phase

Do not enable:

- charger policy automation beyond explicit test code
- low/critical battery enforcement
- shipping-mode automation
- USB/MSC behavior
- sleep/STOP

Firmware goal:

- initialize I2C3
- probe ADP5360 at public 7-bit address `0x46`
- read basic PMIC/fuel/charger/fault status registers selected from the runbook
- store results in a debugger-visible struct or emit through an approved minimal diagnostic path

Exit evidence:

- ADP5360 ACK/NACK result
- register readback values
- I2C address convention confirmed
- current impact noted from PPK2 where practical

---

## Phase 0C: Controlled Rail Configuration

Goal:

```text
Configure only the PMIC rail settings needed for safe continued bring-up.
```

Preconditions:

- Phase 0B ADP5360 I2C readback works.
- current/thermal behavior remains safe.
- intended rail targets are reviewed against [[HW5_Power_Rails]] and [[PMIC_and_Power_Contract]].

Enable/configure:

- only ADP5360 register writes required for rail setup
- readback verification after every write
- bounded failure path if write/readback mismatch occurs

Do not enable:

- real-cell charging unless explicitly reviewed safe
- critical-battery ISOFET-disconnect tests without recovery plan
- START shipping threshold crossing
- unrelated peripherals

Firmware goal:

- write minimal PMIC rail configuration
- read back configuration
- report rail/status result

Exit evidence:

- PPK2 voltage/current setup
- rail measurements after configuration
- ADP5360 write/readback log
- no thermal anomaly

---

## Phase 1: Display Minimum

Goal:

```text
Validate the display electrical path and a blocking static pattern.
```

Enable only after PMIC/rails are safe.

Enable/configure only what [[LS013B7DH05_Display_Bring-up_Runbook]] needs first:

- display GPIO safe enable path
- SPI/display bus baseline
- EXTCOMIN or equivalent required output
- blocking transfer path first

Do not enable:

- full renderer
- DMA display transfer
- LPBAM
- runtime compositor
- package rendering

Add DMA/LPBAM only after static blocking display behavior is proven.

---

## Phase 2: External Flash Minimum

Goal:

```text
Read flash ID, then prove scratch erase/program/readback.
```

Enable/configure only:

- OCTOSPI1 conservative baseline
- flash GPIOs
- polling command path first
- scratch region only

Do not enable:

- FileX
- LevelX
- USB MSC
- package installer
- runtime asset loading

Add DMA, LevelX/FileX, and installer behavior only after raw flash operations are proven.

---

## Phase 3: Audio Minimum

Goal:

```text
Validate safe audio outputs one path at a time.
```

Start with the least risky output path required by [[Audio_Output_Bring-up_Runbook]].

Do not enable:

- mixer
- DMA audio streaming
- package audio runtime

until basic enable/disable, BBB, and/or amp behavior is proven.

---

## Phase 4: Input And Sensors

Goal:

```text
Validate physical inputs and sensor identities/normalized readings.
```

Add one input/sensor family at a time:

1. buttons
2. rotary encoder
3. light sensor
4. IMU
5. joystick/hall sensor

Do not enable all EXTI and sensor logic at once if a narrower test is available.

---

## Phase 5: RTOS Owner Integration

Goal:

```text
Move from bare/minimal test code into Platform owner-thread architecture.
```

Add:

- ThreadX
- owner thread skeletons
- queues/events for already-proven peripherals
- owner health/status reporting

Do not add package/runtime behavior yet.

---

## Phase 6: Sleep And Wake

Goal:

```text
Validate quiesce, sleep entry, wake classification, and resume.
```

Add:

- sleep policy scaffolding
- wake source configuration only for already-proven sources
- PPK2 current measurement scenarios
- Tracealyzer/SWO evidence where useful

Do not enable LPBAM/autonomous display until the display and sleep preconditions are proven.

---

## Phase 7: Installer And Transport

Goal:

```text
Validate USB data-host detection, MSC staging/export, and storage ownership.
```

Add only after storage owner and flash basics are proven:

- USB device stack
- USB host/enumeration detection policy
- MSC staging/export path
- CDC developer personality if explicitly phased

Do not expose MSC from VBUS alone.

Do not enable composite MSC+CDC in v1 unless a later contract requires it.

---

## Phase 8: Runtime Host Lifecycle

Goal:

```text
Validate Engine runtime host lifecycle on proven Platform services.
```

Add:

- runtime host lifecycle
- package validation/install path
- package asset access through safe APIs
- save/settings APIs

Do not implement digital twin as hardware evidence.

---

## CubeMX Change Discipline

Before changing `fw0` `.ioc`, identify:

1. phase
2. specific peripheral/middleware being added
3. runbook that requires it
4. expected generated files affected
5. safe default state after reset
6. evidence required before proceeding

After generation:

1. inspect pin modes for hazards
2. inspect clocks for unintended changes
3. inspect middleware added by CubeMX
4. inspect init order for external hardware risks
5. build with bounded wrapper
6. ask before flash/debug hardware action

---

## Rule

`fw0` grows from proven hardware evidence.

`fw1` remains reference/full-intent until `fw0` is ready to become the full PeepOS firmware target.
