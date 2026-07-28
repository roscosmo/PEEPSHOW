# Boot and Fault Supervisor State Machine

This document defines the active HW6 boot sequence, recovery routing, and global fault supervision contract. HW5 boot evidence is historical and may guide, but cannot replace, HW6 validation.

The boot and fault supervisor is Platform-owned. It may start the Engine and Reference Game only after required Platform invariants are satisfied.

Related:

- [[Authority_and_Invariants]]
- [[Subsystem_State_Machines]]
- [[Peripheral_Robustness_Contract]]
- [[Storage_and_Installer_Contract]]
- [[Platform_Firmware_Update_and_Development_Security_Policy]]
- [[Joystick_Hall_Input_Contract]]
- [[Display_and_Rendering_Contract]]
- [[Debug_and_Observability]]
- [[Brought_Up_Tracker]]

---

## Scope

Defines:

- deterministic boot phase progression
- first-boot and missing-settings behavior
- safe-mode routing
- global fault classification
- persistent fault evidence policy
- fatal fault handling
- reset-cause and watchdog-hardening policy

Does not define:

- per-peripheral recovery internals
- Reference Game launch policy beyond Platform readiness
- MCU ROM bootloader behavior once `BOOT0` is sampled high
- final secure boot, Platform update slot, or production lock-down design

---

## Boot Principles

- Normal boot must not reach normal shell, Engine runtime, or Reference Game runtime until required settings and calibration records are valid.
- Missing settings are not fatal, but they force safe-mode setup/calibration before normal functionality.
- Missing joystick calibration forces joystick calibration safe mode.
- First boot ignores START shipping intent because there is no user data to preserve yet.
- `BTN_BOOT` / `PH3-BOOT0` is the MCU hardware bootloader pin. If it is held high early enough to enter ROM bootloader mode, application firmware does not run and cannot classify it.
- If application firmware is running, `BTN_BOOT` remains reserved for maintenance/recovery policy only and is never normal Engine/Game input.
- Boot must be useful without the Reference Game installed.
- Watchdog enforcement is deferred until release hardening; boot must still capture reset cause and preserve enough evidence to diagnose unexpected resets where possible.

---

## Boot Phase FSM

| State | Meaning |
|---|---|
| `BOOT_RESET_ENTRY` | First application-controlled phase after reset vector. Capture reset reason and minimal crash/reset context. |
| `BOOT_SAFE_GPIO` | Put Platform-controlled outputs into safe defaults before peripheral owners are allowed to act. |
| `BOOT_CLOCKS` | Start and validate required clock sources and clock-tree assumptions, including the `PC14` LSE MEMS oscillator path. |
| `BOOT_KERNEL_START` | Start RTOS/thread infrastructure and core queues without launching normal runtime behavior. |
| `BOOT_PLATFORM_PROBE` | Start/probe required Platform owner threads and required hardware capabilities. |
| `BOOT_STORAGE_LOAD` | Mount/probe protected storage and load settings, calibration, bonding records, and installed package metadata. |
| `BOOT_RECOVERY_CHECK` | Decide normal boot, first-boot setup, safe-mode calibration, installer/recovery, or fatal fault routing. |
| `BOOT_SAFE_MODE` | Minimal controlled mode for calibration, recovery, diagnostics, or storage/settings repair. |
| `BOOT_RUNTIME_READY` | Platform and Engine prerequisites are satisfied; normal shell/runtime may start. |
| `BOOT_FAULT_LATCHED` | Boot encountered a fatal condition and has latched diagnostic evidence as far as possible. |

## Boot Events

| Event | Source | Meaning |
|---|---|---|
| `EV_RESET_VECTOR` | reset handler | application firmware entered after reset |
| `EV_WATCHDOG_RESET_DETECTED` | reset handler / boot supervisor | previous reset appears watchdog-driven where hardware evidence supports that classification |
| `EV_SAFE_GPIO_DONE` | boot supervisor | safe defaults applied |
| `EV_CLOCKS_OK` | clock init | required clocks validated |
| `EV_CLOCKS_FAIL` | clock init | required clocks failed |
| `EV_KERNEL_OK` | RTOS init | kernel primitives and queues ready |
| `EV_KERNEL_FAIL` | RTOS init | kernel setup failed |
| `EV_PLATFORM_REQUIRED_OK` | owner threads | required Platform owners reached usable state |
| `EV_PLATFORM_REQUIRED_FAIL` | owner threads | required Platform owner failed |
| `EV_STORAGE_READY` | `thStorage` | protected storage and required records are readable |
| `EV_STORAGE_MISSING_SETTINGS` | `thStorage` | settings are absent or invalid |
| `EV_CAL_MISSING` | `thStorage` / `thInput` | required calibration is absent or invalid |
| `EV_FIRST_BOOT_DETECTED` | boot supervisor | no valid prior setup record exists |
| `EV_RECOVERY_REQUIRED` | boot supervisor | installer, calibration, diagnostic, or repair path required |
| `EV_SAFE_MODE_READY` | safe-mode shell | recovery/calibration UI is usable |
| `EV_RUNTIME_ALLOWED` | boot supervisor | normal shell/runtime prerequisites are satisfied |
| `EV_FATAL_BOOT_FAULT` | any required owner | fatal boot fault detected |

## Boot Transition Rules

| Current | Event | Next | Required Action |
|---|---|---|---|
| `BOOT_RESET_ENTRY` | `EV_RESET_VECTOR` | `BOOT_SAFE_GPIO` | capture reset reason; do minimal debug marker output if possible |
| `BOOT_SAFE_GPIO` | `EV_SAFE_GPIO_DONE` | `BOOT_CLOCKS` | apply safe pin defaults and defer peripheral-specific enables to owners |
| `BOOT_CLOCKS` | `EV_CLOCKS_OK` | `BOOT_KERNEL_START` | publish clock profile and RTC assumptions |
| `BOOT_CLOCKS` | `EV_CLOCKS_FAIL` | `BOOT_FAULT_LATCHED` | capture clock fault and use easiest available diagnostic output |
| `BOOT_KERNEL_START` | `EV_KERNEL_OK` | `BOOT_PLATFORM_PROBE` | start owner threads in dependency order |
| `BOOT_KERNEL_START` | `EV_KERNEL_FAIL` | `BOOT_FAULT_LATCHED` | capture kernel fault before normal services start |
| `BOOT_PLATFORM_PROBE` | `EV_PLATFORM_REQUIRED_OK` | `BOOT_STORAGE_LOAD` | proceed only after required Platform capabilities are usable or safely degraded |
| `BOOT_PLATFORM_PROBE` | `EV_PLATFORM_REQUIRED_FAIL` | `BOOT_SAFE_MODE` or `BOOT_FAULT_LATCHED` | route by fault class; display failure is fatal unless another diagnostic path is intentionally selected for bring-up |
| `BOOT_STORAGE_LOAD` | `EV_STORAGE_READY` | `BOOT_RECOVERY_CHECK` | validate settings, calibration, bonding, and package metadata |
| `BOOT_STORAGE_LOAD` | `EV_STORAGE_MISSING_SETTINGS` | `BOOT_SAFE_MODE` | start first-boot setup/calibration path |
| `BOOT_STORAGE_LOAD` | `EV_CAL_MISSING` | `BOOT_SAFE_MODE` | start safe-mode calibration path |
| `BOOT_RECOVERY_CHECK` | `EV_FIRST_BOOT_DETECTED` | `BOOT_SAFE_MODE` | ignore START shipping intent and run first-boot setup/calibration |
| `BOOT_RECOVERY_CHECK` | `EV_RECOVERY_REQUIRED` | `BOOT_SAFE_MODE` | enter installer/recovery/diagnostic/calibration flow |
| `BOOT_RECOVERY_CHECK` | `EV_RUNTIME_ALLOWED` | `BOOT_RUNTIME_READY` | allow normal shell/runtime launch |
| `BOOT_SAFE_MODE` | `EV_SAFE_MODE_READY` | `BOOT_SAFE_MODE` | remain in safe mode until required setup, calibration, or repair succeeds |
| any boot state | `EV_FATAL_BOOT_FAULT` | `BOOT_FAULT_LATCHED` | preserve diagnostics using best available channel |

Invalid transitions must be rejected and logged if logging is available.

---

## First-Boot And Missing-Settings Policy

First boot is the case where protected storage has no valid setup/settings record.

First boot must:

- enter safe-mode setup/calibration before normal shell/runtime
- ignore START shipping intent because there is nothing to save or back up yet
- create required settings records through `thStorage`
- create required calibration records before normal input is considered usable
- record setup completion only after required records are valid

If settings are missing or invalid after a non-first boot:

- normal shell/runtime must not start
- safe-mode repair/setup must start
- existing valid calibration or bonding records may be preserved if independently valid

If joystick calibration is missing or invalid:

- normal joystick input must not start
- the device enters joystick calibration safe mode
- calibration safe mode must be navigable with encoder and L/R button fallback controls

---

## BOOT0 / Maintenance Button Policy

`BTN_BOOT` is physically `PH3-BOOT0`.

Rules:

- Holding `BTN_BOOT` high during MCU boot may force STM32 ROM bootloader entry before application firmware runs.
- Application firmware must not claim it can intercept that early bootloader path.
- If application firmware is running and later sees `BTN_BOOT`, it may classify it only as maintenance/recovery input.
- `BTN_BOOT` must never be exposed to Engine or Reference Game input mapping.

---

## Display Failure During Boot

Display is required for normal standalone operation.

If display init fails during boot:

- classify as fatal for normal operation
- emit diagnostic output through the easiest available bring-up channel, such as debugger, SWO, serial, or fault log, depending on what is alive
- do not launch normal shell/runtime blindly
- preserve enough evidence to diagnose the failure when storage or debugger output is available

A deliberate blind recovery mode may be added later, but it must be documented as a separate recovery policy before implementation.

---

## Fault Supervisor FSM

| State | Meaning |
|---|---|
| `FAULT_HEALTHY` | no active fault affects system capability |
| `FAULT_CAPTURE` | fault source, subsystem state, event, reset context, and timestamp are captured |
| `FAULT_CLASSIFY` | fault is classified by severity and required route |
| `FAULT_RECOVERING` | bounded subsystem or system recovery attempt is active |
| `FAULT_DEGRADED` | system continues with an unavailable non-critical capability |
| `FAULT_SAFE_MODE` | normal operation blocked; minimal recovery/calibration/diagnostic UI active |
| `FAULT_FATAL` | unrecoverable or required capability failure; halt, controlled reset, or latched fault behavior |

## Fault Classes

| Class | Meaning | Examples | Required Route |
|---|---|---|---|
| informational | noteworthy but does not affect capability | transient rejected request, invalid transition | log if available |
| degraded | optional capability unavailable | BLE failure, IMU failure, light sensor failure | continue with capability disabled |
| major recoverable | important subsystem failed but recovery may work | audio fault, external flash transient, I2C bus recovery | bounded recovery, then safe/degraded/fatal by subsystem policy |
| safe-mode required | normal operation is unsafe or unusable | missing settings, missing joystick calibration, storage/settings unavailable | `FAULT_SAFE_MODE` |
| fatal | required standalone capability unavailable or unrecoverable invariant broken | display failure, unrecoverable boot clock/kernel fault | `FAULT_FATAL` |

## Fault Events

| Event | Source | Meaning |
|---|---|---|
| `EV_FAULT_RAISED` | any owner thread | subsystem reports fault |
| `EV_FAULT_CAPTURED` | fault supervisor | diagnostic record captured |
| `EV_FAULT_CLASSIFIED_DEGRADED` | fault supervisor | continue with disabled optional capability |
| `EV_FAULT_CLASSIFIED_RECOVERABLE` | fault supervisor | bounded recovery should run |
| `EV_FAULT_CLASSIFIED_SAFE_MODE` | fault supervisor | safe mode is required |
| `EV_FAULT_CLASSIFIED_FATAL` | fault supervisor | fatal route required |
| `EV_RECOVERY_OK` | recovery owner | recovery succeeded |
| `EV_RECOVERY_FAIL` | recovery owner | current recovery attempt failed |
| `EV_RECOVERY_EXHAUSTED` | recovery owner | retry budget exhausted |
| `EV_FAULT_CLEARED` | fault supervisor | fault condition has been cleared and acknowledged |

## Fault Transition Rules

| Current | Event | Next | Required Action |
|---|---|---|---|
| `FAULT_HEALTHY` | `EV_FAULT_RAISED` | `FAULT_CAPTURE` | snapshot subsystem, state, event, and fault context |
| `FAULT_CAPTURE` | `EV_FAULT_CAPTURED` | `FAULT_CLASSIFY` | decide severity using subsystem policy |
| `FAULT_CLASSIFY` | `EV_FAULT_CLASSIFIED_DEGRADED` | `FAULT_DEGRADED` | disable affected capability and publish health |
| `FAULT_CLASSIFY` | `EV_FAULT_CLASSIFIED_RECOVERABLE` | `FAULT_RECOVERING` | start bounded recovery |
| `FAULT_CLASSIFY` | `EV_FAULT_CLASSIFIED_SAFE_MODE` | `FAULT_SAFE_MODE` | stop normal runtime and enter recovery UI |
| `FAULT_CLASSIFY` | `EV_FAULT_CLASSIFIED_FATAL` | `FAULT_FATAL` | preserve evidence and enter fatal safe state |
| `FAULT_RECOVERING` | `EV_RECOVERY_OK` | `FAULT_HEALTHY` or `FAULT_DEGRADED` | clear or downgrade fault according to restored capability |
| `FAULT_RECOVERING` | `EV_RECOVERY_FAIL` | `FAULT_CAPTURE` | capture failed attempt and reclassify |
| `FAULT_RECOVERING` | `EV_RECOVERY_EXHAUSTED` | `FAULT_SAFE_MODE` or `FAULT_FATAL` | escalate according to subsystem criticality |
| `FAULT_DEGRADED` | `EV_FAULT_CLEARED` | `FAULT_HEALTHY` | restore health only after owner confirms capability recovered |
| `FAULT_SAFE_MODE` | `EV_FAULT_CLEARED` | `FAULT_HEALTHY` | allow normal boot/runtime only after required records/capabilities validate |

---

## Persistent Fault Evidence

Fault evidence should be stored in the protected external-flash fault-log ring once `thStorage` is available.

Persistent fault record should include:

- reset reason
- boot phase
- fault class
- subsystem ID
- subsystem state
- triggering event
- hardware status snapshot where cheap and safe
- timestamp or monotonic boot counter if available
- recovery attempt count
- reset-cause classification, including watchdog reset where available

Rules:

- fault logs are not host-writable and are not directly host-exposed
- fault log writes are append/ring style and must preserve the previous valid record on failed update
- early boot faults before storage is available must use the easiest available diagnostic output
- debugger/SWO/serial output is acceptable during bring-up when storage is not yet available
- fault logging must not block fault containment

---

## Watchdog Policy

Watchdog enablement is not required for early bring-up or Platform freeze.

The physical reset path remains the primary development recovery method until reset-cause logging, storage durability, sleep/resume behavior, and update/recovery flows are proven.

If watchdog support is enabled for release hardening:

- a supervisor-owned policy must decide when the watchdog is refreshed.
- owner threads report health to the supervisor; they do not independently refresh the watchdog.
- watchdog reset cause must be recorded where hardware evidence allows it.
- boot must route through normal recovery and package/save reconciliation after watchdog reset.
- watchdog behavior must be validated with low-power residency, storage writes, package saves, installer state, and Platform update state.

Do not enable watchdog policy as a workaround for unresolved owner-thread hangs during bring-up. Diagnose the hang first.

---

## Supervisor Responsibilities

- Own global boot progress state publication.
- Aggregate subsystem fault signals and apply escalation policy.
- Trigger safe-mode routing when normal operation is unsafe or setup is incomplete.
- Prevent normal shell/runtime launch until required settings and calibration are valid.
- Preserve fault evidence through `thStorage` when storage is available.
- Expose boot/fault state to diagnostics and [[Brought_Up_Tracker]] evidence.

---

## Validation Cases

1. successful first application boot enters setup/calibration safe mode
2. valid settings and calibration allow `BOOT_RUNTIME_READY`
3. missing settings enter safe-mode setup
4. missing joystick calibration enters joystick calibration safe mode
5. START shipping intent is ignored during first boot
6. `BTN_BOOT` hardware ROM bootloader limitation is documented and not claimed as firmware-handled early boot input
7. display init failure produces the easiest available diagnostic evidence and does not launch normal runtime
8. non-critical BLE/IMU/light fault can degrade without fatal boot
9. storage-backed fault record is written after storage is available
10. early boot fault emits debugger/SWO/serial evidence if available
11. bounded recovery exhaustion escalates by subsystem criticality
12. watchdog reset cause is captured where available before watchdog release enablement
13. watchdog release policy does not corrupt save, settings, installer, update, or package metadata during reset recovery
