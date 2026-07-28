# Game Documentation Boundary

Game documentation may advance during hardware bring-up.

Game implementation may not drive platform architecture.

## Allowed In Game Notes

Game notes may define:

- desired player experience
- mechanics
- creatures and characters
- mood, pacing, and balancing
- encounters
- UI flow intent
- required abstract capabilities
- content and asset needs

Game notes may reference:

- [[Display_and_Rendering_Contract]]
- [[Power_and_Sleep_Policy]]
- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Runtime_Host_Contract]]
- [[Package_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[HW6_Hardware_Revision_Contract]]

## Not Allowed In Game Notes

Game notes may not define:

- peripheral ownership
- RTOS thread topology
- sleep-mode policy
- clock policy
- storage ownership
- DMA policy
- CubeMX pin choices
- hardware fault handling
- direct hardware register behavior

## Capability Request Flow

If a game idea needs a platform capability:

1. describe the gameplay intent in a Game or Reference Game note
2. link the relevant Platform or Engine note
3. create or update an Engine API requirement note
4. let Platform authority decide the hardware policy

The Reference Game expresses intent.

The Engine defines reusable APIs.

The Platform owns hardware behavior.

The Reference Game is not authoritative over PeepOS content style, genre, mechanics, or API semantics. It is a showcase and proof-of-capability package built from public PeepOS primitives.
