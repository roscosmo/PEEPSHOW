# Hardware Asset Export Guide

This folder stores visual assets used by hardware canvases and documentation.

The canonical canvas entry point is [[HW5_Hardware_Visual_Map]].

## Folder Layout

| Folder | Use |
|---|---|
| `schematic/` | SVG/PDF exports of schematic pages or subsystem plots |
| `pcb/` | SVG/PDF/PNG exports of PCB layers, assembly plots, placement views, routing views |
| `housing/` | enclosure sketches, dimensions, fit checks, mechanical drawings |
| `photos/` | board photos, bring-up probe photos, assembly photos |
| `reference-renders/` | combined board/enclosure renders or annotated overview images |

## Naming Convention

Use names that preserve revision and purpose:

```text
hw5_schematic_power_revA.svg
hw5_pcb_top_assembly_revA.svg
hw5_housing_button_clearance_revA.svg
hw5_board_photo_bringup_001.jpg
```

## Rules

- Keep large generated visuals in `docs/assets/hardware/hw5/`.
- Link visuals from canvases or notes.
- Do not treat a canvas annotation as authoritative unless the relevant hardware note is updated.
- If a visual identifies a pin, rail, wake source, or measured bring-up result, update [[Hardware_Index]] or [[Brought_Up_Tracker]] links accordingly.
