# Hardware Revision Contract

This document is the hardware contract for the target board.
Complete this document immediately after schematic and PCB pinout are frozen.

---

## Required Inputs

- schematic revision ID
- PCB revision ID
- BOM revision ID
- CubeMX `.ioc` revision ID
- bring-up board count and serial identifiers

---

## Canonical Part Tokens

Define canonical tokens and use them everywhere:

| Function | Canonical Token | Part Number | Notes |
|---|---|---|---|
| MCU | `MCU_MAIN` | TBD | |
| Display | `DISPLAY_PANEL` | TBD | |
| External flash | `FLASH_EXT` | TBD | |
| Audio amp/codec | `AUDIO_OUT` | TBD | |
| Hall/joystick | `JOY_SENSOR` | TBD | |
| IMU | `IMU_SENSOR` | TBD | |
| PMIC/fuel | `PMIC_MAIN` | TBD | |
| Radio (optional) | `RADIO_MAIN` | TBD | |

---

## Pin Ownership Matrix (Required)

For each peripheral define:
- MCU instance/pins
- owning thread
- DMA channel (if used)
- wake capability
- safe reset state

Template:

| Peripheral | Instance | Pins | Owner Thread | DMA | Wake Source | Safe Default |
|---|---|---|---|---|---|---|
| Display bus | TBD | TBD | `thDisplay` | TBD | No | Disabled |
| Audio bus | TBD | TBD | `thAudio` | TBD | No | Amp off |
| Storage flash | TBD | TBD | `thStorage` | TBD | No | Idle |
| Sensor I2C | TBD | TBD | `thSensor` | N/A | Optional | Bus idle |
| Input GPIO/EXTI | TBD | TBD | `thInput` | N/A | Yes | Pull configured |

---

## Clock Tree Contract

Must be explicitly documented:
- base SYSCLK profile
- boosted profile(s)
- kernel clock sources for display, audio, storage, sensors, USB
- STOP-safe clock assumptions

Rules:
- kernel clocks required for deterministic peripheral timing must not depend on ad hoc runtime changes
- profile changes must not violate active transfer safety

---

## DMA and Memory Placement Contract

For each DMA path define:
- controller/channel
- source and destination memory regions
- alignment requirements
- STOP compatibility

Template:

| Path | DMA | Buffer Region | Alignment | STOP-safe |
|---|---|---|---|---|
| Display flush | TBD | TBD | TBD | TBD |
| Audio TX | TBD | TBD | TBD | TBD |
| Storage transfer | TBD | TBD | TBD | TBD |

---

## Power Rails and Enables

Document:
- translator enables
- sensor rails
- amp enable/shutdown pin behavior
- flash deep power-down requirements

Each control pin needs:
- active polarity
- boot default state
- mode-transition behavior

---

## Wake Source Contract

List all wake-capable sources and owner handling path:

| Wake Source | Electrical Path | Owner | Debounce/Filter | Allowed Modes |
|---|---|---|---|---|
| RTC alarm | TBD | `thPower` | N/A | all low-power modes |
| Button EXTI | TBD | `thInput` | required | policy-defined |
| Sensor IRQ | TBD | `thSensor` | required | policy-defined |
| USB attach | TBD | `thStorage`/`thPower` | required | installer flow |

---

## Sign-Off Checklist

Hardware contract is complete only when:
1. pin ownership matrix is complete
2. clock and DMA mapping are complete
3. wake-source map is complete
4. safe defaults are validated on target hardware
5. `.ioc` and this doc agree exactly
