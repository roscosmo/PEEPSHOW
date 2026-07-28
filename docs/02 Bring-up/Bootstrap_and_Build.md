# Bootstrap and Build

This document defines the initial project setup and the first bring-up sequence for new hardware.

---

## Toolchain Prerequisites

- STM32CubeMX (for `.ioc` generation)
- ARM GNU toolchain (`arm-none-eabi-gcc`)
- CMake (3.25+ recommended)
- Ninja
- ST-Link tooling or OpenOCD
- Python 3.11+ for local generators/validators

---

## One-Time Project Bootstrap

1. Confirm the active bring-up CubeMX project path from [[FW0_Phased_CubeMX_Bring-up_Plan]].
2. Validate pin map and peripheral clock selections against schematic.
3. Generate or regenerate firmware code into the documented firmware workspace with CMake support enabled.
4. Keep `/docs` as the Obsidian vault and architecture source of truth.
5. Define initial knobs and schemas in the documented configuration location before feature work depends on them.
6. Commit a "hardware skeleton only" checkpoint before subsystem feature work.

---

## Build Configuration Baseline

Required build types:
- `Debug`
- `Release`

Required outputs:
- `.elf`
- `.bin`
- map file

No dynamic allocation is allowed unless explicitly approved and documented.

---

## Example Configure/Build Commands

The recommended long-term entry point for repeatable workflows is [[Dev_Orchestration_CLI_Contract]].

Manual commands remain valid during bootstrap and when diagnosing the orchestration layer.

Agent-run builds must follow [[Bounded_Build_Flash_Debug_Runbook]].

During hardware bring-up, use `fw0` from [[FW0_Phased_CubeMX_Bring-up_Plan]] unless the user explicitly promotes or replaces the firmware target.

```powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

Optional release:

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

---

## Flash and Debug (Manual Template)

Use your project-standard toolchain command templates, for example:

```powershell
st-flash --reset write build/debug/peepos.bin 0x08000000
```

or:

```powershell
openocd -f interface/stlink.cfg -f target/stm32u5x.cfg
```

Keep actual production commands in [[Debug_Workflows]].

Agent-run flash and debugger inspection must use the bounded workflow in [[Bounded_Build_Flash_Debug_Runbook]].

Once the orchestration CLI exists, equivalent commands should be available through documented `peep build`, `peep flash`, and `peep debug` workflows while still printing the underlying tool invocation or artifact summary.

---

## Bring-Up Phases

Use the phase order from [[Brought_Up_Tracker]]:

1. Phase 0 - Arrival, power, and clock stability.
2. Phase 1 - Display validation.
3. Phase 2 - Storage validation.
4. Phase 3 - Audio validation.
5. Phase 4 - Input and sensors.
6. Phase 5 - RTOS owner integration.
7. Phase 6 - Sleep and wake validation.
8. Phase 7 - Installer and transport mode.
9. Phase 8 - Runtime host lifecycle.
10. Phase 9 - Platform freeze checks.
11. Phase 10 - Digital twin parity.

Do not enable deep sleep before power, clocks, owner-thread quiesce, and wake classification prerequisites are documented by the relevant runbooks.

CubeMX feature enablement is phased by [[FW0_Phased_CubeMX_Bring-up_Plan]]. Do not enable full `fw1` middleware merely because the reference `.ioc` contains it.

---

## Build and Bring-Up Evidence Required

For each phase capture:
- date and board revision
- firmware commit ID
- command line or orchestration CLI invocation
- what was tested
- pass/fail criteria
- evidence artifact path (SWO log, scope capture, host logs)

Store evidence in the live tracker [[Brought_Up_Tracker]].
