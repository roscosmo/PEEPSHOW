# Display and Rendering

Authoritative specification for the Sharp Memory LCD pipeline, renderer
bitplane model, composition rules, and flush behavior in PeepShow V5.

This document defines buffer formats, layer semantics, dirty tracking,
and the SPI/DMA update contract.

If implementation differs from this document, the implementation is wrong.

---

## Scope

Defines:
- Panel geometry and update stream format
- SPI electrical/transaction requirements
- DMA constraints (SRAM regions, chunking)
- Renderer layer model (bitplanes + opacity planes)
- Composition order and pixel semantics
- Dirty-row tracking and partial update strategy
- Logical-to-physical coordinate mapping
- 2bpp world surface format (4-tone + transparency)
- 1× binary clamp present mode
- 2× Bayer zoom present mode

Does NOT define:
- Thread ownership or queues (see rtos_architecture.md)
- Clock policy (see power_management.md)
- Asset formats (see asset_pipeline.md)

Reference implementation:
- LS013B7DH05 driver: /mnt/data/LS013B7DH05.c
- Renderer/compositor: /mnt/data/display_renderer.c

---

## Display Hardware Model (Sharp Memory LCD)

Panel class:
- LS013B7DH05
- Native resolution: 144 x 168

Panel update stream format (EXTCOM handled externally):
- WRITE_CMD = 0x01
- For each updated gate line:
  - Gate address byte (1..168)
  - Line data bytes (panel byte order)
  - Dummy byte 0x00 (8 clocks)
- End of stream:
  - Dummy byte 0x00 (extra 8 clocks; total 16 clocks after last line)

Row addressing is 1-based and must remain 1-based across the entire pipeline.

---

## SPI Contract (Non-Negotiable)

SPI configuration:
- 8-bit transfers
- CPOL = 0
- CPHA = 1Edge
- FirstBit = LSB first (matches Sharp MIP examples and current driver intent)

Chip select behavior:
- CS is ACTIVE HIGH
- CS must remain HIGH for the entire update stream
- CS must go LOW only after the final dummy byte is clocked

Violations of CS hold timing will produce corrupted/partial updates.

---

## Buffer Format (Panel-Native)

Critical rule:
- The framebuffer is stored in panel byte order already.
- Flush must NOT apply bit reversal (no rev8).

Pixel convention (panel-native):
- Bit = 1 means WHITE
- Bit = 0 means BLACK

This convention is preserved through:
- Layer planes
- Compositor output
- Flush stream

No inversion is performed during flush.

---

## Memory and DMA Constraints

LPDMA constraints:
- Any SPI DMA source buffer must be placed in SRAM4.
- Buffers used for DMA are aligned to 4 bytes.

Renderer output framebuffer:
- Stored in SRAM4 (aligned)
- Used directly by LCD flush APIs

If DMA source is placed outside DMA-accessible memory:
- DMA may transmit zeros/garbage
- Display may appear blank
- Faults may occur depending on bus configuration

---

## Flush Strategy (Driver)

The driver supports:
- Blocking flush (HAL_SPI_Transmit) with chunking
- DMA flush (HAL_SPI_Transmit_DMA) with chunk chaining

Chunking constraint:
- Practical HAL TSIZE chunk max: 255 bytes per call

Full-screen stream size:
- 1 + 168 * (addr + data + dummy) + 1
- data bytes per row: LINE_WIDTH bytes
- Total full stream is on the order of a few kilobytes

DMA flush model:
- One flush in flight at a time
- Driver maintains an internal chain state:
  - device pointer
  - current pointer
  - remaining length
- HAL_SPI_TxCpltCallback advances the chain
- Completion triggers LCD_FlushDmaDoneCallback (weak hook)
- Errors trigger LCD_FlushDmaErrorCallback (weak hook)

Optional wait helper:
- LCD_FlushDMA_WaitWFI(timeout_ms) waits for completion using WFI

---

## Partial Updates (Row Flush)

The driver supports row-based partial updates via:
- LCD_FlushRows (blocking)
- LCD_FlushRows_DMA (DMA)

Row list rules:
- Rows are gate addresses (1..DISPLAY_HEIGHT)
- Rows are 1-based, not 0-based
- Row count must be bounded and validated

Row update stream format:
- CMD_WRITE then per-row [addr][line_data][dummy], final dummy.

---

## Renderer Architecture

The renderer is panel-native and produces a composed framebuffer suitable for flush.

### Layer Model

Renderer maintains three logical layers:
- BG
- GAME
- UI

Each layer is represented by two planes:
- ON plane: pixel color bits (1=white, 0=black)
- OP plane: opacity bits (1=opaque, 0=transparent)

Planes:
- bg_on, bg_op
- game_on, game_op
- ui_on, ui_op

Plane convention:
- *_on bit = 1 -> WHITE pixel, 0 -> BLACK pixel
- *_op bit = 1 -> layer owns pixel (opaque), 0 -> transparent

This matches the panel convention and avoids inversion at flush time.

---

## Composition Rules (Authoritative)

Composition order:
1. Start with BG color bits
2. Apply GAME where GAME opacity is 1
3. Apply UI where UI opacity is 1

Per-byte composition for a given row:
- out = bg_on
- out = (out & ~game_op) | (game_on & game_op)
- out = (out & ~ui_op) | (ui_on & ui_op)

BG is treated as the baseline.
GAME and UI are alpha-over masks (1-bit opacity).

Opaque pixels overwrite; transparent pixels do nothing.

---

## Dirty Row Tracking

Dirty tracking is row-granular.

Dirty mask:
- Bitset over DISPLAY_HEIGHT rows
- Rows are tracked as 1-based physical rows
- Internally uses a 32-bit word array

Rules:
- Any drawing operation that modifies pixels in a physical row must set that row dirty.
- Dirty is set using y_phys + 1 (physical row index is 0-based in math but stored as 1-based gate row).

Dirty operations:
- dirty_set_row(row_1_based)
- dirty_clear_row(row_1_based)
- dirty_count_rows()

Partial update policy:
- Compose and flush only dirty rows when practical
- Full compose/flush may be used when dirty count exceeds a threshold (policy decision belongs above the renderer)

Renderer provides:
- compose_row_to_fb(y_phys)
- compose_all_rows_to_fb()

---

# World Surface (2bpp + Mask) and Present Modes

This section defines the optional 4-tone world rendering path.

This does NOT change the panel framebuffer format.
The panel remains strictly 1bpp.

The 2bpp surface is an internal intermediate representation only.

---

## World Surface Format (2bpp)

Pixel levels:
- 0 = white
- 1 = light grey
- 2 = dark grey
- 3 = black

Each pixel uses 2 bits.
Four pixels per byte:

- bits 7:6 = pixel 0
- bits 5:4 = pixel 1
- bits 3:2 = pixel 2
- bits 1:0 = pixel 3

Stride must be 4-byte aligned.

---

## Transparency Model (Sprites and Bitmaps)

All bitmaps (tiles, sprites, images) use:

- 2bpp color plane
- 1bpp mask plane

Mask semantics:
- mask bit = 1 → pixel exists
- mask bit = 0 → transparent

Transparency is mask-only.
Color level 0 is a valid white pixel.

---

## Present Modes

### Mode A: 1× Binary Clamp Present

World resolution:
- Equal to panel resolution

Mapping:
- level 0,1 → WHITE
- level 2,3 → BLACK

Formal rule:

panel_bit = (level >= 2) ? 0 : 1

Where:
- 1 = white
- 0 = black

No spatial scaling occurs.

---

### Mode B: 2× Bayer Dither Zoom Present

World resolution:
- (DISPLAY_WIDTH / 2) × (DISPLAY_HEIGHT / 2)

Each world pixel expands to a 2×2 block in the panel framebuffer.

Bayer 2×2 matrix:

0 2
3 1

For each world pixel level L:
- threshold = bayer[dx][dy]
- panel_pixel = (L > threshold) ? BLACK : WHITE

Effect:
- 4-tone appearance
- Visible 2× zoom
- World viewport reduced to half width and half height

---

## Coordinate System and Rotation

The renderer uses a logical coordinate space distinct from physical panel addressing.

Mapping used:
- phys_y = x_screen
- phys_x = (DISPLAY_WIDTH - 1) - y_screen

Rules:
- All high-level drawing APIs accept logical x/y.
- Only the low-level bit operations use physical x/y.
- Physical addressing uses BYTES_PER_ROW = LINE_WIDTH and bit mask (1 << (x_phys & 7)).

---

## Clipping

Renderer supports an optional clip rectangle:
- When disabled: all pixels draw
- When enabled: only pixels within [x0,x1) and [y0,y1) draw

Clip is evaluated in logical coordinates.

---

## Invariants (Do Not Violate)

- Framebuffer is panel byte order. No rev8 in flush.
- Bit 1 means white, bit 0 means black everywhere in the pipeline.
- CS is active-high and must remain high for the full update stream.
- SPI DMA source buffers must reside in SRAM4.
- Only one flush may be in flight at a time.
- Dirty rows are tracked as 1-based gate rows.
- Composition order is BG then GAME then UI.
- OP plane is opacity, not color.
- World2bpp surface is never sent directly to the panel.

---

## Integration Notes

- thDisplay owns SPI3 and all flush operations (see rtos_architecture.md).
- Renderer produces a composed SRAM4 framebuffer suitable for DMA.
- Higher-level code should:
  - write into planes or world2bpp surface
  - mark dirty rows
  - compose dirty rows to framebuffer
  - request flush of those rows through display thread

---

Last updated: 2026-02-27