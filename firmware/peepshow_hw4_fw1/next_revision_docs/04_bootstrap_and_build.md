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

1. Create a CubeMX project for the target board.
2. Validate pin map and peripheral clock selections against schematic.
3. Generate code into the repository with CMake support enabled.
4. Copy baseline docs from this pack into `/docs`.
5. Define initial knobs and schemas in `/config`.
6. Commit a "hardware skeleton only" checkpoint before feature work.

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

Keep actual production commands in `24_debug_workflows.md`.

---

## Bring-Up Phases

1. Phase 0 - Clock and reset stability.
2. Phase 1 - GPIO and always-on rails.
3. Phase 2 - Display bring-up (blocking path first).
4. Phase 3 - External flash and storage path.
5. Phase 4 - Audio path and DMA.
6. Phase 5 - Input and sensor validation.
7. Phase 6 - ThreadX owner-thread integration.
8. Phase 7 - STOP2 and wake classification.
9. Phase 8 - Installer path and storage ownership handoff.

Do not enable deep sleep before phases 0-6 are stable.

---

## Build and Bring-Up Evidence Required

For each phase capture:
- date and board revision
- firmware commit ID
- what was tested
- pass/fail criteria
- evidence artifact path (SWO log, scope capture, host logs)

Store evidence in live tracker `brought_up.md` created from `23_brought_up_tracker_template.md`.
