# Reference Game

This section is for designing the PeepShow reference game.

Keep it concrete:

- rules
- loops
- inputs
- state
- maps
- encounters
- content
- constraints

No pitch copy.

No fake certainty.

## Start

- [[HW6_Authoring_Vertical_Slice]]
- [[Scratchpad]]
- [[Core_Loop]]
- [[Game_State]]
- [[Input_Verbs]]
- [[Content_Index]]
- [[Open_Questions]]

## Mechanics

- [[Core_Loop]]
- [[Game_State]]
- [[Input_Verbs]]
- [[Time_And_Schedules]]
- [[Combat]]
- [[Forms]]
- [[Microgames]]

## Content

- [[Content_Index]]
- [[Slime]]
- [[NPCs]]
- [[Enemies]]
- [[Places]]
- [[Items]]
- [[Rituals]]

## Maps

- [[Map_Notes]]

## Boundary

Game docs can request Engine or Platform capabilities.

Game docs do not define hardware behavior.

The Reference Game is a proof-of-capability package for PeepOS.

Its first bounded proof package is specified by [[HW6_Authoring_Vertical_Slice]]. That slice is intentionally smaller than the full Reference Game and exists to validate the public authoring, package, runtime, and HW6 evidence path.

It must be built from the same public primitives, package contracts, and authoring tools available to other packages.

It may request new capabilities, but accepted capabilities must be reusable beyond the Reference Game and documented through [[Game_Authoring_API_Contract]] and [[PeepOS_Capability_Registry]].

No Reference Game-only hidden API path is allowed.

## Authoring Reuse

Reference Game features should be framed as reusable PeepOS authoring patterns where possible.

Use `Authoring Kit` for reusable gameplay systems, not `module`.

Likely Reference Game Authoring Kits include:

- Dialogue Kit
- NPC Kit
- Shop Kit
- Inventory Kit
- Quest Flag Kit
- Pathing Kit
- Encounter Kit
- Microgame Kit

These kits must compile through the same package contracts, scene types, content parameters, save/settings schemas, capability requirements, and compatibility reports available to other packages.

Reference Game-specific content may instantiate or customize these kits, but it must not define private Engine APIs or Platform behavior.
