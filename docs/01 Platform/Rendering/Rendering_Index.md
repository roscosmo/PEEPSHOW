# Rendering Index

This section defines Sharp Memory LCD ownership, framebuffer semantics, present behavior, changed-region policy, low-power display behavior, DMA-safe buffer placement, and LPBAM display-animation experiments.

## Core Notes

- [[Display_and_Rendering_Contract]]
- [[Subsystem_State_Machines]]
- [[Power_and_Sleep_Policy]]
- [[HW6_Hardware_Revision_Contract]]

## Boundary

The Engine may request scene/frame presentation and rendering capabilities.

The Platform owns the display peripheral, level translation, EXTCOMIN behavior, transfer method, DMA/LPBAM policy, and low-power display behavior.
