# Rendering API Contract

This document defines the Engine-facing rendering model used by packages, tools, runtime hosts, and the digital twin.

It does not define the target display driver, SPI transfer format, DMA policy, LPBAM setup, EXTCOMIN behavior, or panel-native row packing. Those belong to [[Display_and_Rendering_Contract]].

---

## Boundary

Packages and tools may use:

- logical monochrome canvas coordinates
- sprites, masks, tone assets, tilemaps, fonts, shapes, and animation tables
- fixed runtime compositor layers
- integer sprite scaling
- scene/frame presentation requests
- bounded waiting-visual sequence assets

Packages and tools must not use:

- panel-native framebuffer addresses
- physical LCD row numbers
- Sharp LCD line command bytes
- changed-row or transfer-region control
- SRAM4 placement decisions
- SPI, DMA, LPDMA, LPBAM, EXTCOMIN, or level-translator control

The Engine may implement changed-region tracking internally. The Platform chooses the physical display transfer method.

---

## Logical Canvas

The package-facing canvas is the logical PeepOS display surface.

| Field | HW6 Logical Surface |
|---|---|
| Logical width | `168` |
| Logical height | `144` |
| Origin | top-left |
| X axis | right |
| Y axis | down |
| Final output | 1-bit monochrome |

The logical canvas is not the panel-native framebuffer. The Platform maps the logical canvas to the native Sharp Memory LCD orientation.

---

## Pixel And Tone Models

The final LCD output is always 1-bit.

Package assets may use these Engine pixel models:

| Pixel Model | Meaning |
|---|---|
| `masked_1bpp` | black/white pixels with explicit opacity mask |
| `tone5_masked` | semantic tone pixels with explicit opacity/ownership |
| `precomposed_1bpp` | validated final monochrome frame data for fixed display sequences |

`tone5` is a semantic coverage model, not native display color and not a color-depth format.

The first Peep Studio STATE vertical slice implements `masked_1bpp` and
`precomposed_1bpp` only. `tone5_masked` remains a reserved Engine model and is
not a dependency of that slice. Additional fixed tone/dither import models must
receive versioned package semantics before tools or firmware advertise them.

`tone5` values:

| Value | Meaning |
|---|---|
| `transparent` | no ownership; lower layer shows through |
| `white` | 0% black coverage |
| `light` | about 25% black coverage |
| `mid` | about 50% black coverage |
| `dark` | about 75% black coverage |
| `black` | 100% black coverage |

The asset pipeline may pack `tone5_masked` however it chooses, including compact color-plane plus mask-plane representations. The package-facing contract is the semantic tone model.

Source art may be authored as a five-color indexed PNG. The asset pipeline converts that source into validated `tone5_masked` package assets containing logical tone data and explicit ownership/mask semantics.

---

## Coverage Rendering

`tone5` assets are resolved to 1-bit output using deterministic coverage patterns.

Rules:

- integer scale is the v1 scaling model.
- each tone source pixel expands to an `N x N` output cell when scaled by integer `N`.
- the renderer fills the cell with a deterministic black-pixel coverage pattern matching the requested tone.
- dither/coverage phase must be stable across frames unless an animation deliberately changes it.
- target profiles and package validation define maximum scale, maximum output bounds, and per-frame/per-event render cost.
- fractional scaling is not part of the v1 contract.

At `2x`, `tone5` behaves as a 2x2 virtual-pixel coverage model. Larger integer scales use the same coverage principle over a larger cell.

---

## Runtime Compositor Layers

The runtime compositor has four retained logical layers.

Visual order, top to bottom:

1. `OVERLAY`
2. `UI`
3. `SCENE`
4. `BACKGROUND`

Composition is implementation-defined as long as each owned pixel on a higher
layer replaces lower-layer output. A transparent pixel does not overwrite lower
layers.

Rules:

- retained layer planes live outside the Platform autonomous-display SRAM arena
- `BACKGROUND` may be opaque; transparent layers retain explicit ownership/mask data
- changed content is tracked per layer and composed into final changed panel rows internally
- authoring tools may expose more source layers only when tooling can flatten them into this bounded model
- `OVERLAY` is Engine/Platform controlled
- packages may provide validated overlay style assets, including inactive-state styling, but may not suppress mandatory system warnings
- system setup, calibration, package management, diagnostics, faults, shipping state, and interaction-state cues may use the reserved overlay path
- package UI defaults to `masked_1bpp`; `tone5` UI is allowed only when explicitly authored and within budget

---

## Render Primitives

The Engine may expose these package-facing primitives:

- draw masked 1bpp sprite
- draw tone5 masked sprite
- draw integer-scaled sprite
- draw tilemap region or viewport
- draw text from validated font/text tables
- draw simple bounded shapes
- play frame animation by ID
- present scene or frame update
- request static, realtime, or low-power presentation intent through power policy

The Engine may also support bounded procedural surfaces for advanced packages. Procedural surfaces must declare dimensions, memory budget, operation budget, scene-type limits, and fallback behavior before package compilation.

---

## Tilemap And Viewport Model

Tilemaps are package assets, not runtime editor files.

Rules:

- external Tiled or map-editor files are import sources only.
- package tilemaps are compiled bounded binary assets.
- tilesets declare pixel model, tile size, and allowed scale.
- tilemap dimensions, layers, collision/data tables, and viewport bounds are validated before package compilation.
- runtime may draw a tilemap viewport or region; it must not parse JSON or stream arbitrary files.

For a world-enabled `STATE_SCENE`, world entities are not themselves retained
render elements. The runtime applies camera projection and viewport culling,
then emits a bounded render model for visible content. A tilemap viewport is a
bounded region command or prepared surface operation rather than one element per
tile. One entity may emit zero, one, or several elements according to its visual
definition. World-entity capacity and render-element capacity are separate
target-profile limits. See [[State_Scene_World_Entity_and_Turn_Contract]].

---

## Execution And Presentation Rules

| Execution Semantic | Rendering Rule |
|---|---|
| `STATE_SCENE / REACTIVE` | bounded drawing occurs during an admitted event transaction; after the settled view and waiting presentation are established, the runtime yields |
| `SEQUENCE_SCENE / REALTIME` | validated timeline frames are composed and presented at the declared FPS |
| `PROGRAM_SCENE / REALTIME` | sandboxed frame logic submits bounded drawing within the declared frame budget |

A settled `STATE_SCENE` declares a waiting visual:

- `hold`: preserve the committed frame without package drawing
- `sequence`: bounded cosmetic motion derived from authored frames/sprites/UI elements
- `reduced_sequence`: optional lower-complexity fallback
- `hold_fallback`: preserve the settled frame when animation cannot be admitted

Waiting visual intent does not select an autonomous backend. The Platform may compile it for LPBAM/LPDMA, use a measured wake/update/return path, reduce it, or hold the frame according to target-profile grants and declared fallback.

For the HW6 v1 compiler profile, authors may use up to four phases per element and twelve combined preferred timeline steps. The target also supplies a guaranteed three-step reduction, so packages do not need to encode a hardware-specific emergency sequence. It maps one-phase elements to `1/1/1`, two-phase elements to `1/2/1`, three-phase elements to `1/2/3`, and four-phase elements to `1/2/3`. Tools must preview this result and warn that a fourth phase or a more complex mixed timeline is preferred-only. An explicitly authored reduced sequence may be stricter, but cannot exceed the target's guaranteed bounds.

Every waiting presentation also resolves a backend-neutral presentation
timeline. The timeline owns the epoch, phase index, and next deadline. Changing
between awake rendering and an autonomous backend must preserve that timeline.
Re-rendering changed content inside the same presentation also preserves the
current combined step and deadline. A scene-state content change does not by
itself create a new presentation identity. Only an incompatible presentation
identity or an explicit authored rebase policy rebases to its declared settled
step.

---

## Waiting Visual Sequences

`waiting_visual_sequence` is the package/tool-facing visual asset used by reactive states while waiting.

Rules:

- source may come from sprites, tone5 assets, animations, tilemaps, UI elements, or direct authored frames
- tooling resolves each sequence to bounded final 1bpp visual states before package compilation/export
- no arbitrary game logic, tilemap renderer, text layout, or sprite compositor runs while a waiting sequence is being replayed autonomously
- playback does not mutate package state or generate gameplay transitions
- the Platform may store admitted sequence content as full frames, logical deltas, hardware row deltas, repeated payloads, or another display-owner format
- SRAM4 placement and LPDMA/LPBAM payload format are Platform internals
- sequence frame count, cadence, cycle duration, and target-compiler admission are capped by the selected target profile; wake/exit behavior comes from the enclosing reactive wait
- phase durations are integer multiples of the target presentation quantum; the HW6 v1 quantum is `250 ms`
- all elements advance on one combined timeline; differing local phase counts
  are represented by each element's phase index at every combined step
- backend handoff starts with the phase after the committed physical frame and must not rebase the presentation epoch
- same-presentation event updates redraw the new settled content at the current
  combined step and preserve the next deadline
- renderers must distinguish content revision from timeline revision so dirty
  rows can change without resetting otherwise compatible animation
- wake from a target-generated reduced sequence presents the current reduced phase unchanged, preserves the remaining quantum, and then resumes the preferred timeline at its corresponding next phase
- packages describe preferred and fallback appearance; tools derive whether `display.waiting_visual_animation` is required or optional

The HW6 FW0 compiled-in `STATE_SCENE` proof implements this distinction: each
accepted L/R action advances scene content while retaining one six-step
presentation identity, allowing the existing awake/LPBAM scheduler to preserve
the current combined frame and absolute deadline. The proof does not yet define
the package serialization or authoring-tool schema for that scene.

The next HW6 FW0 slice resolves one bounded render model per scene render. The
model carries numeric scene, state, visual-binding, text, focus, content-revision,
timeline-revision, and waiting-policy fields. `thDisplay` translates its numeric
text IDs through the compiled target catalog, renders the awake list, then uses
the same model to publish the waiting visual. A missing, invalid, or stale model
fails closed rather than combining static content and autonomous animation from
different revisions. This is the package-facing rendering boundary shape; the
current compiled catalog is not yet a package asset loader. The next vertical
slice replaces that visual catalog with validated `asset_table`,
`masked_1bpp_sprite_bank`, and `animation_table` records. The selected-scene
host preview and firmware renderer must consume those same package records and
produce the same logical `168 x 144` 1bpp framebuffer for the same state,
explicit input, and explicit time.

---

## Validation Requirements

Rendering validation must check:

- asset pixel model is known and supported by the target profile.
- sprite, tile, font, and animation bounds fit package and scene budgets.
- tone5 assets have deterministic coverage output.
- integer scale factors fit output bounds and render budget.
- draw command count fits the scene type.
- runtime compositor layer use fits the fixed layer model.
- tilemap dimensions, viewport bounds, and layer flattening are valid.
- text layout cannot overflow declared bounds unless clipping/wrapping policy is declared.
- `SEQUENCE_SCENE` timeline rendering fits its FPS, duration, track, and frame budgets.
- `PROGRAM_SCENE` rendering fits its program and frame budgets.
- reactive rendering establishes its next waiting visual and yields after the event transaction settles.
- waiting visual sequences are precomposed, bounded, and validated against the target profile.
- no package artifact exposes panel-native framebuffer, changed-row transfer control, SRAM4 placement, SPI, DMA, LPBAM, EXTCOMIN, or display power policy.

---

## Digital Twin Requirements

The digital twin must use the same package assets, renderer semantics, tone5
coverage rules, layer order, scene rules, presentation epoch, and phase-deadline
rules as the device contract.

It may render to a host window or image buffer, but screenshots and frame checksums must be derived from the same logical canvas semantics used by the package runtime.

---

Related:

- [[Scene_Runtime_and_Interaction_Model]]
- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Display_and_Rendering_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
