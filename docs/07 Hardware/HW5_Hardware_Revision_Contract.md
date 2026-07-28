# HW5 Hardware Revision Contract

This document is the hardware contract for the HW5 target board.
Complete this document immediately after schematic and PCB pinout are frozen.

> [!warning] Retired target
> HW5 is retired. This contract is immutable historical context and does not authorize HW6 implementation or evidence. Use [[HW6_Hardware_Revision_Contract]] for active work.

Focused hardware notes:

- [[HW5_Part_Tokens]]
- [[HW5_CubeMX_Pin_Map]]
- [[HW5_Pin_Ownership_Matrix]]
- [[HW5_Clock_Tree_Contract]]
- [[HW5_DMA_Map]]
- [[HW5_Power_Rails]]
- [[HW5_Wake_Sources]]
- [[CubeMX_Configuration_Checklist]]

---

## Required Inputs

- schematic revision ID
- PCB revision ID
- BOM revision ID
- CubeMX `.ioc` revision ID
- bring-up board count and serial identifiers

---

## Canonical Part Tokens

Canonical tokens live in [[HW5_Part_Tokens]].

Use those tokens in hardware notes, bring-up logs, firmware comments, issue titles, and debug traces.

---

## Pin Ownership Matrix (Required)

For each peripheral define:
- MCU instance/pins
- owning thread
- DMA channel (if used)
- wake capability
- safe reset state

The historical HW5 matrix lives in [[HW5_Pin_Ownership_Matrix]].

The raw CubeMX pin extraction lives in [[HW5_CubeMX_Pin_Map]].

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

The historical HW5 DMA map lives in [[HW5_DMA_Map]].

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

The historical HW5 rail/control table lives in [[HW5_Power_Rails]].

---

## Wake Source Contract

List all wake-capable sources and owner handling path.

The historical HW5 wake-source table lives in [[HW5_Wake_Sources]].

---

## Sign-Off Checklist

Hardware contract is complete only when:
1. pin ownership matrix is complete
2. clock and DMA mapping are complete
3. wake-source map is complete
4. safe defaults are validated on target hardware
5. `.ioc` and this doc agree exactly
