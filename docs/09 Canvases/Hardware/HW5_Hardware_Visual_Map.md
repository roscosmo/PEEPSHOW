# HW5 Hardware Visual Map

This note is the companion index for the Obsidian canvas:

- [HW5_Hardware_Visual_Map.canvas](HW5_Hardware_Visual_Map.canvas)

Use the canvas for SVG plots, PCB views, housing sketches, and annotation layers.

## Authoritative Links

- [[Hardware_Index]]
- [[HW5_Hardware_Revision_Contract]]
- [[HW5_CubeMX_Pin_Map]]
- [[HW5_Pin_Ownership_Matrix]]
- [[HW5_Power_Rails]]
- [[HW5_Wake_Sources]]
- [[HW5_DMA_Map]]
- [[Brought_Up_Tracker]]

## Visual Asset Paths

- schematic SVG/PDF exports: `docs/assets/hardware/hw5/schematic/`
- PCB SVG/PDF/PNG exports: `docs/assets/hardware/hw5/pcb/`
- housing drawings/dimensions: `docs/assets/hardware/hw5/housing/`
- board photos: `docs/assets/hardware/hw5/photos/`
- combined renders/mockups: `docs/assets/hardware/hw5/reference-renders/`

## Suggested Canvas Layers

| Layer | Purpose |
|---|---|
| schematic | electrical signal grouping and subsystem boundaries |
| PCB | connector location, routing-sensitive areas, test points |
| housing | clearances, buttons, openings, speaker/buzzer/acoustic paths |
| bring-up | probes, jumpers, test order, known-good evidence |
| firmware ownership | owner thread and contract links |

## Annotation Rule

If a canvas note discovers a real hardware requirement, copy it back into the relevant authoritative note.
