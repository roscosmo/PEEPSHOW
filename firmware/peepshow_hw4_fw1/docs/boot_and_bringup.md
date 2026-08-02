# Boot and Bring-Up

Authoritative specification for boot sequencing, hardware initialization,
ThreadX startup, and phased system bring-up in PeepShow V5.

This document defines the order of initialization, subsystem validation
strategy, and constraints required before enabling low-power modes.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- Reset-to-main boot flow
- Clock initialization policy
- ThreadX initialization sequence
- Peripheral bring-up ordering
- Phased development model
- STOP2 enablement criteria

Does NOT define:
- Runtime mode policy (see power_management.md)
- Thread responsibilities (see rtos_architecture.md)

---

## Design Principles

- Hardware first, features later.
- Verify each subsystem independently.
- STOP2 is introduced only after stability.
- No dynamic allocation at runtime.
- Deterministic object creation at init.

---

## Boot Sequence Overview

Reset → HAL_Init() → SystemClock_Config() → main() → MX init → ThreadX start

High-level order:

1. Reset handler
2. HAL_Init
3. System clock configuration
4. Peripheral clock setup
5. Static memory initialization
6. MX_ThreadX_Init
7. ThreadX kernel start

After ThreadX starts:
- No further RTOS object creation is allowed.

---

## Clock Initialization

Initial clock state must be conservative and stable.

Boot clock policy:

- Use PLL1 for SYSCLK.
- Select stable HCLK (e.g., 80 MHz initial).
- Configure MSIK and LSE early.
- Do not enable PLL2 until required by subsystem.

SysTick must be correctly initialized after final SYSCLK selection.

If SYSCLK changes later:
- SysTick must be reprogrammed immediately.

---

## Peripheral Bring-Up Order

Bring-up order must follow dependency layering:

1. GPIO (including level translators)
2. LSE (RTC source)
3. SPI3 (display)
4. I2C (sensors + PMIC)
5. OCTOSPI (flash interface)
6. SAI1 (audio, without enabling amp)
7. LPUART1 (radio)

Each peripheral must be validated independently before integration.

---

## ThreadX Initialization

All RTOS objects must be created during:

MX_ThreadX_Init()

This includes:

- Threads
- Queues
- Event flag groups
- Semaphores
- Byte pools (if used, but dynamic allocation discouraged)

Rules:

- No RTOS object creation after scheduler start.
- Stack sizes must be defined explicitly.
- Thread priorities must be fixed at compile time.

---

## Phased Bring-Up Model

Development proceeds in phases.

### Phase 0 – Power + Clock Stability

Goals:
- Confirm stable SYSCLK.
- Confirm LSE running.
- Confirm SysTick increments.
- Confirm no unexpected faults.

STOP2 not yet enabled.

---

### Phase 1 – Display Validation

Goals:
- Initialize SPI3.
- Perform blocking full-screen test pattern.
- Verify CS polarity and bit ordering.
- Confirm correct orientation.

DMA introduced only after blocking path is verified.

---

### Phase 2 – Storage Validation

Goals:
- Initialize OCTOSPI.
- Read JEDEC ID.
- Validate raw read/write.
- Initialize LevelX + FileX.
- Mount and format FAT volume.

Do not enable FLASHING mode yet.

---

### Phase 3 – Audio Validation

Goals:
- Enable PLL2P.
- Configure SAI1.
- Generate test tone or play embedded clip.
- Validate DMA refill path.
- Confirm amplifier enable sequencing.

STOP2 not yet enabled.

---

### Phase 4 – Input + Sensors

Goals:
- Validate EXTI button wake.
- Validate joystick read.
- Validate I2C sensors.
- Confirm raw event routing via thInput.

---

### Phase 5 – RTOS Integration

Goals:
- Validate thread communication.
- Confirm queue topology.
- Confirm display thread ownership.
- Confirm storage thread isolation.

---

### Phase 6 – STOP2 Introduction

Preconditions:

- All awake-mode paths stable.
- No DMA faults.
- No HardFault events.
- No uncontrolled polling loops.

Introduce STOP2 with:

- Debug-in-low-power enabled.
- SWO markers around STOP entry/exit.

---

### Phase 7 – FLASHING Mode

Goals:
- Unmount FileX cleanly.
- Enable USB MSC.
- Validate host mount.
- Validate safe eject.
- Validate remount and install workflow.

No rendering or audio active during FLASHING.

---

## STOP2 Enablement Criteria

STOP2 must not be enabled until:

- All DMA channels validated.
- Flash deep power-down tested.
- Audio quiesce confirmed.
- Display flush safe.
- No ISR busy loops.

STOP2 must be entered only via thPower.

---

## HardFault Policy During Bring-Up

Any HardFault must:

- Capture stacked registers.
- Capture SCB fault registers.
- Emit concise SWO event.
- Halt or reset safely.

HardFaults must not be ignored.

---

## Deterministic Initialization Rules

- All global state initialized explicitly.
- No uninitialized static data relied upon.
- No runtime memory allocation.
- No background tasks started before validation.

---

## Forbidden Patterns

- Enabling STOP2 before awake paths are stable.
- Debugging without HardFault capture.
- Creating RTOS objects after scheduler start.
- Mixing blocking and DMA display paths without isolation.
- Enabling amplifier before valid audio data.

---

## Integration Notes

- Debug-in-low-power must be enabled during STOP testing.
- Connect-under-reset should be used for early boot faults.
- SWD frequency may need reduction during initial bring-up.
- SysTick must remain valid after clock transitions.

---

Last updated: 2026-02-18
