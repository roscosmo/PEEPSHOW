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

The baseline runtime compositor has three fixed physical layers.

Visual order, top to bottom:

1. `UI`
2. `GAME`
3. `BG`

Composition order is implementation-defined as long as the visible result is equivalent to:

```text
BG base
GAME replaces BG where GAME owns pixels
UI replaces GAME/BG where UI owns pixels
```

Each runtime layer has ownership/opacity semantics. A transparent pixel does not overwrite lower layers.

Authoring tools may expose more logical layers, but the compiler must flatten, merge, or schedule those layers into the bounded runtime compositor model.

System UI is Platform/Engine-owned and may use reserved crisp 1bpp assets. Package UI defaults to `masked_1bpp`; `tone5` package UI is allowed only when explicitly authored and within render budget.

System UI is reserved PeepOS behavior for setup, calibration, package management, diagnostics, errors, shipping mode, and related system flows. It is not a package-authored game layer.

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

The Engine may also support bounded procedural surfaces for advanced packages. Procedural surfaces must declare dimensions, memory budget, operation budget, runtime-class limits, and fallback behavior before package compilation.

---

## Tilemap And Viewport Model

Tilemaps are package assets, not runtime editor files.

Rules:

- external Tiled or map-editor files are import sources only.
- package tilemaps are compiled bounded binary assets.
- tilesets declare pixel model, tile size, and allowed scale.
- tilemap dimensions, layers, collision/data tables, and viewport bounds are validated before package compilation.
- runtime may draw a tilemap viewport or region; it must not parse JSON or stream arbitrary files.

---

## Execution And Presentation Rules

| Execution Semantic | Rendering Rule |
|---|---|
| `REACTIVE` | bounded drawing/presentation occurs during an admitted event transaction; after the settled view and waiting visual are established, the runtime yields |
| `REALTIME` | frame-paced drawing occurs within the declared frame budget |

A settled reactive state declares a waiting visual:

- `hold`: preserve the committed frame without package drawing
- `sequence`: bounded cosmetic motion derived from authored frames/sprites/UI elements
- `reduced_sequence`: optional lower-complexity fallback
- `hold_fallback`: preserve the settled frame when animation cannot be admitted

Waiting visual intent does not select an autonomous backend. The Platform may compile it for LPBAM/LPDMA, use a measured wake/update/return path, reduce it, or hold the frame according to target-profile grants and declared fallback.

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
- packages describe preferred and fallback appearance; tools derive whether `display.waiting_visual_animation` is required or optional

---

## Validation Requirements

Rendering validation must check:

- asset pixel model is known and supported by the target profile.
- sprite, tile, font, and animation bounds fit package and runtime-unit budgets.
- tone5 assets have deterministic coverage output.
- integer scale factors fit output bounds and render budget.
- draw command count fits the runtime class.
- runtime compositor layer use fits the fixed layer model.
- tilemap dimensions, viewport bounds, and layer flattening are valid.
- text layout cannot overflow declared bounds unless clipping/wrapping policy is declared.
- `RT_SCENE` frame rendering fits frame budget.
- reactive rendering establishes its next waiting visual and yields after the event transaction settles.
- waiting visual sequences are precomposed, bounded, and validated against the target profile.
- no package artifact exposes panel-native framebuffer, changed-row transfer control, SRAM4 placement, SPI, DMA, LPBAM, EXTCOMIN, or display power policy.

---

## Digital Twin Requirements

The digital twin must use the same package assets, renderer semantics, tone5 coverage rules, layer order, and runtime mode rules as the device contract.

It may render to a host window or image buffer, but screenshots and frame checksums must be derived from the same logical canvas semantics used by the package runtime.

---

Related:

- [[Game_Authoring_API_Contract]]
- [[PeepOS_Capability_Registry]]
- [[Package_Contract]]
- [[Asset_Pipeline_and_Package_Tooling_Contract]]
- [[Display_and_Rendering_Contract]]
- [[Digital_Twin_Host_Runtime_Contract]]
