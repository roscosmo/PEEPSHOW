#include "display_renderer.h"
#include "LS013B7DH05.h"
#include "font8x8_basic.h"
#include <string.h>

#if defined(__GNUC__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#elif defined(__ICCARM__)
  #define SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
  #define SRAM4_BUF_ATTR
#endif

/* Panel-native packing */
#define BYTES_PER_ROW   (LINE_WIDTH)
#define TOTAL_BYTES     (BUFFER_LENGTH)

/* Dirty tracking (kept as-is: 1-based physical rows). */
#define DIRTY_WORD_COUNT ((DISPLAY_HEIGHT + 31u) / 32u)

/*
 * Plane convention used here:
 *   *_on bit = 1 -> WHITE pixel
 *            = 0 -> BLACK pixel
 *   *_op bit = 1 -> layer owns pixel (opaque)
 *            = 0 -> transparent
 *
 * This matches the panel buffer convention used elsewhere in the project
 * (0 bits drawn as black, 1 bits drawn as white), so flush needs no inversion.
 */
static uint8_t s_bg_on[TOTAL_BYTES];
static uint8_t s_bg_op[TOTAL_BYTES];
static uint8_t s_game_on[TOTAL_BYTES];
static uint8_t s_game_op[TOTAL_BYTES];
static uint8_t s_ui_on[TOTAL_BYTES];
static uint8_t s_ui_op[TOTAL_BYTES];

/* Composed panel-native framebuffer (used by LCD_Flush*()). */
static uint8_t s_framebuffer[BUFFER_LENGTH] SRAM4_BUF_ATTR;

static uint32_t s_dirty_mask[DIRTY_WORD_COUNT];
static th_mode_t s_mode_current = (th_mode_t)0xFFu;
static bool s_clip_enabled = false;
static uint16_t s_clip_x0 = 0u;
static uint16_t s_clip_y0 = 0u;
static uint16_t s_clip_x1 = 0u;
static uint16_t s_clip_y1 = 0u;
/* Blit hardening counters for debugger inspection. */
static volatile uint32_t g_render_blit_invalid_arg_count = 0u;
static volatile uint32_t g_render_blit_stride_reject_count = 0u;
static volatile uint32_t g_render_blit_bounds_break_count = 0u;

/* ------------------------------- Dirty bits -------------------------------- */
static void dirty_set_row(uint16_t row)
{
    if ((row == 0u) || (row > DISPLAY_HEIGHT)) {
        return;
    }
    uint32_t idx = (uint32_t)(row - 1u);
    s_dirty_mask[idx / 32u] |= (1u << (idx % 32u));
}

static void dirty_clear_row(uint16_t row)
{
    if ((row == 0u) || (row > DISPLAY_HEIGHT)) {
        return;
    }
    uint32_t idx = (uint32_t)(row - 1u);
    s_dirty_mask[idx / 32u] &= ~(1u << (idx % 32u));
}

static bool dirty_is_set(uint16_t row)
{
    if ((row == 0u) || (row > DISPLAY_HEIGHT)) {
        return false;
    }
    uint32_t idx = (uint32_t)(row - 1u);
    return (s_dirty_mask[idx / 32u] & (1u << (idx % 32u))) != 0u;
}

static void dirty_clear_all(void)
{
    memset(s_dirty_mask, 0, sizeof(s_dirty_mask));
}

static inline uint32_t dirty_last_word_mask(void)
{
    const uint32_t rem = (uint32_t)DISPLAY_HEIGHT & 31u;
    return (rem == 0u) ? 0xFFFFFFFFu : ((1u << rem) - 1u);
}

static uint16_t dirty_count_rows(void)
{
    uint32_t total = 0u;
    const uint32_t last_mask = dirty_last_word_mask();

    for (uint32_t i = 0u; i < (uint32_t)DIRTY_WORD_COUNT; i++) {
        uint32_t w = s_dirty_mask[i];
        if (i == ((uint32_t)DIRTY_WORD_COUNT - 1u)) {
            w &= last_mask;
        }
        total += (uint32_t)__builtin_popcount(w);
    }

    return (uint16_t)total;
}

static inline bool render_in_clip(uint16_t x, uint16_t y)
{
    if (!s_clip_enabled) {
        return true;
    }
    return (x >= s_clip_x0) && (x < s_clip_x1) && (y >= s_clip_y0) && (y < s_clip_y1);
}

/* ------------------------- Logical->physical mapping ------------------------ */
static inline bool map_xy(uint16_t x_screen, uint16_t y_screen,
                          uint16_t *out_x_phys, uint16_t *out_y_phys)
{
    if ((x_screen >= RENDER_WIDTH) || (y_screen >= RENDER_HEIGHT)) {
        return false;
    }

    /* 270 deg logical: phys_y = x_screen, phys_x = (DISPLAY_WIDTH - 1) - y_screen */
    *out_y_phys = x_screen;
    *out_x_phys = (uint16_t)((DISPLAY_WIDTH - 1u) - y_screen);
    return true;
}

static inline void idx_mask(uint16_t x_phys, uint16_t y_phys,
                            uint32_t *out_idx, uint8_t *out_mask)
{
    uint32_t idx = ((uint32_t)y_phys * BYTES_PER_ROW) + ((uint32_t)x_phys >> 3);
    uint8_t  mask = (uint8_t)(1u << (x_phys & 7u));
    *out_idx = idx;
    *out_mask = mask;
}

static inline uint8_t *plane_on_ptr(render_layer_t layer)
{
    switch (layer) {
        case RENDER_LAYER_BG:   return s_bg_on;
        case RENDER_LAYER_GAME: return s_game_on;
        case RENDER_LAYER_UI:   return s_ui_on;
        default:                return s_bg_on;
    }
}

static inline uint8_t *plane_op_ptr(render_layer_t layer)
{
    switch (layer) {
        case RENDER_LAYER_BG:   return s_bg_op;
        case RENDER_LAYER_GAME: return s_game_op;
        case RENDER_LAYER_UI:   return s_ui_op;
        default:                return s_bg_op;
    }
}

static inline void plane_write_span_phys(uint16_t y_phys,
                                        uint16_t x_phys0, uint16_t x_phys1,
                                        render_layer_t layer,
                                        render_color_t color)
{
    if (y_phys >= DISPLAY_HEIGHT) {
        return;
    }
    if (x_phys0 > x_phys1) {
        uint16_t t = x_phys0;
        x_phys0 = x_phys1;
        x_phys1 = t;
    }
    if (x_phys0 >= DISPLAY_WIDTH) {
        return;
    }
    if (x_phys1 >= DISPLAY_WIDTH) {
        x_phys1 = (uint16_t)(DISPLAY_WIDTH - 1u);
    }

    uint32_t base = (uint32_t)y_phys * BYTES_PER_ROW;
    uint8_t *on = plane_on_ptr(layer) + base;
    uint8_t *op = plane_op_ptr(layer) + base;

    uint16_t b0 = (uint16_t)(x_phys0 >> 3);
    uint16_t b1 = (uint16_t)(x_phys1 >> 3);

    uint8_t start_mask = (uint8_t)(0xFFu << (x_phys0 & 7u));
    uint8_t end_mask   = (uint8_t)(0xFFu >> (7u - (x_phys1 & 7u)));

    if (b0 == b1) {
        uint8_t m = (uint8_t)(start_mask & end_mask);
        op[b0] |= m;
        if (color == RENDER_COLOR_BLACK) {
            on[b0] &= (uint8_t)~m;
        } else {
            on[b0] |= m;
        }
    } else {
        op[b0] |= start_mask;
        if (color == RENDER_COLOR_BLACK) {
            on[b0] &= (uint8_t)~start_mask;
        } else {
            on[b0] |= start_mask;
        }

        for (uint16_t b = (uint16_t)(b0 + 1u); b < b1; b++) {
            op[b] = 0xFFu;
            on[b] = (color == RENDER_COLOR_BLACK) ? 0x00u : 0xFFu;
        }

        op[b1] |= end_mask;
        if (color == RENDER_COLOR_BLACK) {
            on[b1] &= (uint8_t)~end_mask;
        } else {
            on[b1] |= end_mask;
        }
    }

    dirty_set_row((uint16_t)(y_phys + 1u));
}



/* ------------------------------ Composition -------------------------------- */
static void compose_row_to_fb(uint16_t y_phys)
{
    uint32_t base = (uint32_t)y_phys * BYTES_PER_ROW;

    uint8_t *dst = &s_framebuffer[base];
    const uint8_t *bg_on = &s_bg_on[base];
    const uint8_t *game_on = &s_game_on[base];
    const uint8_t *game_op = &s_game_op[base];
    const uint8_t *ui_on = &s_ui_on[base];
    const uint8_t *ui_op = &s_ui_op[base];

    for (uint16_t b = 0u; b < BYTES_PER_ROW; b++) {
        uint8_t out = bg_on[b];

        uint8_t gop = game_op[b];
        out = (uint8_t)((out & (uint8_t)~gop) | (game_on[b] & gop));

        uint8_t uop = ui_op[b];
        out = (uint8_t)((out & (uint8_t)~uop) | (ui_on[b] & uop));

        dst[b] = out;
    }
}

static void compose_all_rows_to_fb(void)
{
    for (uint16_t y = 0u; y < DISPLAY_HEIGHT; y++) {
        compose_row_to_fb(y);
    }
}

/* --------------------------------- Public --------------------------------- */
void Render_Init(void)
{
    /* Default baseline: white BG, opaque; GAME/UI transparent. */
    renderClear(RENDER_COLOR_WHITE);

    /* Composed output starts all-white. */
    memset(s_framebuffer, 0xFF, sizeof(s_framebuffer));

    dirty_clear_all();
    s_mode_current = (th_mode_t)0xFFu;
    s_clip_enabled = false;
}

const uint8_t *Render_GetBuffer(void)
{
    return s_framebuffer;
}

void Render_WriteBgRowSpanPhys(uint16_t y_phys,
                               uint16_t x_phys0, uint16_t x_phys1,
                               const uint8_t *row_bytes)
{
    if (row_bytes == NULL) {
        return;
    }
    if (y_phys >= DISPLAY_HEIGHT) {
        return;
    }

    if (x_phys0 > x_phys1) {
        uint16_t t = x_phys0;
        x_phys0 = x_phys1;
        x_phys1 = t;
    }
    if (x_phys0 >= DISPLAY_WIDTH) {
        return;
    }
    if (x_phys1 >= DISPLAY_WIDTH) {
        x_phys1 = (uint16_t)(DISPLAY_WIDTH - 1u);
    }

    uint32_t base = (uint32_t)y_phys * BYTES_PER_ROW;
    uint8_t *on = &s_bg_on[base];
    uint8_t *op = &s_bg_op[base];

    uint16_t b0 = (uint16_t)(x_phys0 >> 3);
    uint16_t b1 = (uint16_t)(x_phys1 >> 3);

    uint8_t start_mask = (uint8_t)(0xFFu << (x_phys0 & 7u));
    uint8_t end_mask = (uint8_t)(0xFFu >> (7u - (x_phys1 & 7u)));

    if (b0 == b1) {
        uint8_t m = (uint8_t)(start_mask & end_mask);
        on[b0] = (uint8_t)((on[b0] & (uint8_t)~m) | (row_bytes[b0] & m));
        op[b0] |= m;
    } else {
        on[b0] = (uint8_t)((on[b0] & (uint8_t)~start_mask) | (row_bytes[b0] & start_mask));
        op[b0] |= start_mask;

        for (uint16_t b = (uint16_t)(b0 + 1u); b < b1; b++) {
            on[b] = row_bytes[b];
            op[b] = 0xFFu;
        }

        on[b1] = (uint8_t)((on[b1] & (uint8_t)~end_mask) | (row_bytes[b1] & end_mask));
        op[b1] |= end_mask;
    }

    dirty_set_row((uint16_t)(y_phys + 1u));
}

void Render_SetClipRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint32_t x1 = (uint32_t)x + (uint32_t)w;
    uint32_t y1 = (uint32_t)y + (uint32_t)h;

    if ((w == 0u) || (h == 0u)) {
        s_clip_enabled = false;
        return;
    }

    if (x >= RENDER_WIDTH) {
        s_clip_enabled = false;
        return;
    }
    if (y >= RENDER_HEIGHT) {
        s_clip_enabled = false;
        return;
    }

    if (x1 > RENDER_WIDTH) {
        x1 = RENDER_WIDTH;
    }
    if (y1 > RENDER_HEIGHT) {
        y1 = RENDER_HEIGHT;
    }

    if ((x1 <= x) || (y1 <= y)) {
        s_clip_enabled = false;
        return;
    }

    s_clip_x0 = x;
    s_clip_y0 = y;
    s_clip_x1 = (uint16_t)x1;
    s_clip_y1 = (uint16_t)y1;
    s_clip_enabled = true;
}

void Render_ClearClip(void)
{
    s_clip_enabled = false;
}

void renderClear(render_color_t bg_color)
{
    const uint8_t bg_on_fill = (bg_color == RENDER_COLOR_BLACK) ? 0x00u : 0xFFu;

    memset(s_bg_on, bg_on_fill, sizeof(s_bg_on));
    memset(s_bg_op, 0xFFu,     sizeof(s_bg_op));

    memset(s_game_op, 0x00u, sizeof(s_game_op));
    memset(s_ui_op,   0x00u, sizeof(s_ui_op));

    /* on planes don't matter where op=0, but keep deterministic. */
    memset(s_game_on, 0xFFu, sizeof(s_game_on));
    memset(s_ui_on,   0xFFu, sizeof(s_ui_on));

    Render_MarkDirtyAll();
}

void renderSetPixel(uint16_t x, uint16_t y, render_layer_t layer, render_color_t color)
{
    if (!render_in_clip(x, y)) {
        return;
    }

    uint16_t x_phys = 0u, y_phys = 0u;
    if (!map_xy(x, y, &x_phys, &y_phys)) {
        return;
    }

    uint32_t idx = 0u;
    uint8_t mask = 0u;
    idx_mask(x_phys, y_phys, &idx, &mask);

    uint8_t *on = plane_on_ptr(layer);
    uint8_t *op = plane_op_ptr(layer);

    if (color == RENDER_COLOR_TRANSPARENT) {
        op[idx] &= (uint8_t)~mask;
    } else {
        op[idx] |= mask;
        if (color == RENDER_COLOR_BLACK) {
            on[idx] &= (uint8_t)~mask;
        } else {
            on[idx] |= mask;
        }
    }

    dirty_set_row((uint16_t)(y_phys + 1u));
}

static inline uint8_t read_src_bit(const uint8_t *src_row, uint16_t x, bool leftmost_is_msb)
{
    uint8_t byte = src_row[x >> 3];
    uint8_t bit  = (uint8_t)(x & 7u);
    return leftmost_is_msb ? (uint8_t)((byte >> (7u - bit)) & 1u)
                           : (uint8_t)((byte >> bit) & 1u);
}

void renderBlit1bpp(uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h,
                    const uint8_t *src, uint16_t src_stride_bytes,
                    bool leftmost_is_msb,
                    render_layer_t layer,
                    render_color_t one_bits_color)
{
    uint16_t min_src_stride;
    bool one_bits_black;

    if (!src || (w == 0u) || (h == 0u)) {
        g_render_blit_invalid_arg_count++;
        return;
    }
    if ((one_bits_color != RENDER_COLOR_BLACK) && (one_bits_color != RENDER_COLOR_WHITE)) {
        g_render_blit_invalid_arg_count++;
        return;
    }

    min_src_stride = (uint16_t)((w + 7u) >> 3);
    if ((src_stride_bytes == 0u) || (src_stride_bytes < min_src_stride)) {
        g_render_blit_stride_reject_count++;
        return;
    }

    one_bits_black = (one_bits_color == RENDER_COLOR_BLACK);

    /* Source bit selection for a fixed source column sx */
    for (uint16_t sx = 0u; sx < w; sx++) {

        /* Screen X for this source column */
        uint32_t x_screen_u32 = (uint32_t)x + (uint32_t)sx;
        if (x_screen_u32 >= RENDER_WIDTH) {
            continue;
        }
        if (s_clip_enabled) {
            if ((x_screen_u32 < s_clip_x0) || (x_screen_u32 >= s_clip_x1)) {
                continue;
            }
        }

        /* Rotation mapping:
           phys_y = x_screen
           phys_x = (DISPLAY_WIDTH - 1) - y_screen
         */
        uint16_t y_phys = (uint16_t)x_screen_u32;
        if (y_phys >= DISPLAY_HEIGHT) {
            continue;
        }

        /* Determine sy range (clip + screen bounds) once per column */
        uint32_t sy0 = 0u;
        uint32_t sy1 = (uint32_t)h; /* exclusive */

        /* Clamp to screen bottom */
        if ((uint32_t)y >= RENDER_HEIGHT) {
            continue;
        }
        {
            uint32_t max_h = (uint32_t)RENDER_HEIGHT - (uint32_t)y;
            if (sy1 > max_h) {
                sy1 = max_h;
            }
        }

        /* Clamp to clip rect (Y only; X already handled above) */
        if (s_clip_enabled) {
            if ((uint32_t)y < (uint32_t)s_clip_y0) {
                uint32_t d = (uint32_t)s_clip_y0 - (uint32_t)y;
                if (d > sy0) sy0 = d;
            }
            {
                uint32_t clip_y1 = (uint32_t)s_clip_y1;
                uint32_t y1_screen = (uint32_t)y + sy1;
                if (y1_screen > clip_y1) {
                    uint32_t new_sy1 = (clip_y1 > (uint32_t)y) ? (clip_y1 - (uint32_t)y) : 0u;
                    if (new_sy1 < sy1) sy1 = new_sy1;
                }
            }
        }

        if (sy0 >= sy1) {
            continue;
        }

        /* Destination row pointers (single physical row) */
        uint32_t base = (uint32_t)y_phys * BYTES_PER_ROW;
        uint8_t *on = plane_on_ptr(layer) + base;
        uint8_t *op = plane_op_ptr(layer) + base;

        /* Precompute how to read the source bit for this sx */
        uint16_t src_byte_ix = (uint16_t)(sx >> 3);
        uint8_t  src_bit = (uint8_t)(sx & 7u);
        uint8_t  src_mask = leftmost_is_msb ? (uint8_t)(1u << (7u - src_bit))
                                            : (uint8_t)(1u << src_bit);
        const uint8_t *src_col = src + ((uint32_t)sy0 * (uint32_t)src_stride_bytes) + src_byte_ix;

        /* Starting y_screen and corresponding x_phys */
        uint32_t y_screen_u32 = (uint32_t)y + sy0;
        uint16_t x_phys = (uint16_t)((DISPLAY_WIDTH - 1u) - (uint16_t)y_screen_u32);

        /* Track byte/bit for x_phys as we decrement x_phys each step */
        uint16_t dst_byte_ix = (uint16_t)(x_phys >> 3);
        uint8_t  dst_bit_ix  = (uint8_t)(x_phys & 7u);
        uint8_t  dst_mask    = (uint8_t)(1u << dst_bit_ix);

        bool row_dirty = false;
        uint32_t run = sy1 - sy0;
        while (run != 0u) {
            run--;

            /* Read source bit: only one byte load + mask */
            if ((*src_col & src_mask) != 0u) {

                /* Apply to destination planes */
                op[dst_byte_ix] |= dst_mask;

                if (one_bits_black) {
                    on[dst_byte_ix] &= (uint8_t)~dst_mask;
                } else {
                    on[dst_byte_ix] |= dst_mask;
                }

                row_dirty = true;
            }
            src_col += src_stride_bytes;

            /* Advance to next y_screen (sy+1) => x_phys decrements by 1 */
            if (dst_mask == 0x01u) {
                /* bit 0 -> next is bit 7 in previous byte */
                if (dst_byte_ix == 0u) {
                    if (run != 0u) {
                        g_render_blit_bounds_break_count++;
                    }
                    break;
                }
                dst_byte_ix--;
                dst_mask = 0x80u;
            } else {
                dst_mask >>= 1;
            }
        }

        if (row_dirty) {
            dirty_set_row((uint16_t)(y_phys + 1u));
        }
    }
}


void renderBlit1bppScaled(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          const uint8_t *src, uint16_t src_stride_bytes,
                          bool leftmost_is_msb,
                          render_layer_t layer,
                          render_color_t one_bits_color,
                          uint8_t scale)
{
    if (!src || (w == 0u) || (h == 0u)) {
        return;
    }
    if ((one_bits_color != RENDER_COLOR_BLACK) && (one_bits_color != RENDER_COLOR_WHITE)) {
        return;
    }
    if ((scale == 0u) || (scale > 8u)) {
        return;
    }

    for (uint16_t sy = 0u; sy < h; sy++) {

        /* Screen-space Y rectangle for this source row */
        uint32_t y0 = (uint32_t)y + (uint32_t)sy * (uint32_t)scale;
        uint32_t y1 = y0 + (uint32_t)scale; /* exclusive */

        if (y0 >= RENDER_HEIGHT) {
            break; /* further sy only increases y */
        }
        if (y1 > RENDER_HEIGHT) {
            y1 = RENDER_HEIGHT;
        }

        if (s_clip_enabled) {
            if (y1 <= s_clip_y0 || y0 >= s_clip_y1) {
                continue;
            }
            if (y0 < s_clip_y0) y0 = s_clip_y0;
            if (y1 > s_clip_y1) y1 = s_clip_y1;
            if (y1 <= y0) {
                continue;
            }
        }

        const uint8_t *src_row = src + ((uint32_t)sy * (uint32_t)src_stride_bytes);

        for (uint16_t sx = 0u; sx < w; sx++) {

            /* Read source bit (same semantics as your unscaled blit) */
            uint16_t src_byte_ix = (uint16_t)(sx >> 3);
            uint8_t  bit = (uint8_t)(sx & 7u);
            uint8_t  src_mask = leftmost_is_msb ? (uint8_t)(1u << (7u - bit))
                                                : (uint8_t)(1u << bit);

            if ((src_row[src_byte_ix] & src_mask) == 0u) {
                continue;
            }

            /* Screen-space X rectangle for this source column */
            uint32_t x0 = (uint32_t)x + (uint32_t)sx * (uint32_t)scale;
            uint32_t x1 = x0 + (uint32_t)scale; /* exclusive */

            if (x0 >= RENDER_WIDTH) {
                continue;
            }
            if (x1 > RENDER_WIDTH) {
                x1 = RENDER_WIDTH;
            }

            if (s_clip_enabled) {
                if (x1 <= s_clip_x0 || x0 >= s_clip_x1) {
                    continue;
                }
                if (x0 < s_clip_x0) x0 = s_clip_x0;
                if (x1 > s_clip_x1) x1 = s_clip_x1;
                if (x1 <= x0) {
                    continue;
                }
            }

            /* Map the clipped screen rect to a physical rect.
             *
             * Mapping (same as elsewhere):
             *   phys_y = x_screen
             *   phys_x = (DISPLAY_WIDTH - 1) - y_screen
             *
             * Screen rect:
             *   x in [x0, x1)  -> phys_y in [x0, x1)
             *   y in [y0, y1)  -> phys_x in [W-1-(y1-1), W-1-y0]
             */
            uint16_t phys_y0 = (uint16_t)x0;
            uint16_t phys_y1 = (uint16_t)(x1 - 1u);

            uint16_t y_screen_lo = (uint16_t)y0;
            uint16_t y_screen_hi = (uint16_t)(y1 - 1u);

            uint16_t phys_x_hi = (uint16_t)((DISPLAY_WIDTH - 1u) - y_screen_lo);
            uint16_t phys_x_lo = (uint16_t)((DISPLAY_WIDTH - 1u) - y_screen_hi);

            /* Fill the physical rectangle as horizontal spans, one per physical row */
            for (uint16_t py = phys_y0; py <= phys_y1; py++) {
                plane_write_span_phys(py, phys_x_lo, phys_x_hi, layer, one_bits_color);
            }
        }
    }
}

void renderBlitMasked1bpp(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          const uint8_t *src_on,
                          const uint8_t *src_mask,
                          uint16_t src_stride_bytes,
                          bool leftmost_is_msb,
                          render_layer_t layer)
{
    uint16_t min_src_stride;

    if (!src_on || !src_mask || (w == 0u) || (h == 0u)) {
        g_render_blit_invalid_arg_count++;
        return;
    }

    min_src_stride = (uint16_t)((w + 7u) >> 3);
    if ((src_stride_bytes == 0u) || (src_stride_bytes < min_src_stride)) {
        g_render_blit_stride_reject_count++;
        return;
    }

    for (uint16_t sx = 0u; sx < w; sx++) {

        /* screen x for this source column */
        uint32_t x_screen = (uint32_t)x + (uint32_t)sx;
        if (x_screen >= RENDER_WIDTH) {
            continue;
        }
        if (s_clip_enabled) {
            if ((x_screen < s_clip_x0) || (x_screen >= s_clip_x1)) {
                continue;
            }
        }

        /* mapping: phys_y = x_screen */
        uint16_t y_phys = (uint16_t)x_screen;
        if (y_phys >= DISPLAY_HEIGHT) {
            continue;
        }

        /* compute sy range once */
        if ((uint32_t)y >= RENDER_HEIGHT) {
            continue;
        }

        uint32_t sy0 = 0u;
        uint32_t sy1 = (uint32_t)h; /* exclusive */

        /* clamp to screen */
        {
            uint32_t max_h = (uint32_t)RENDER_HEIGHT - (uint32_t)y;
            if (sy1 > max_h) sy1 = max_h;
        }

        /* clamp to clip rect (Y only) */
        if (s_clip_enabled) {
            if ((uint32_t)y < (uint32_t)s_clip_y0) {
                uint32_t d = (uint32_t)s_clip_y0 - (uint32_t)y;
                if (d > sy0) sy0 = d;
            }
            {
                uint32_t clip_y1 = (uint32_t)s_clip_y1;
                uint32_t y1_screen = (uint32_t)y + sy1;
                if (y1_screen > clip_y1) {
                    uint32_t new_sy1 = (clip_y1 > (uint32_t)y) ? (clip_y1 - (uint32_t)y) : 0u;
                    if (new_sy1 < sy1) sy1 = new_sy1;
                }
            }
        }

        if (sy0 >= sy1) {
            continue;
        }

        /* destination row pointers */
        uint32_t base = (uint32_t)y_phys * BYTES_PER_ROW;
        uint8_t *dst_on = plane_on_ptr(layer) + base;
        uint8_t *dst_op = plane_op_ptr(layer) + base;

        /* precompute how to read src bit for this sx */
        uint16_t src_byte_ix = (uint16_t)(sx >> 3);
        uint8_t  bit = (uint8_t)(sx & 7u);
        uint8_t  src_bitmask = leftmost_is_msb ? (uint8_t)(1u << (7u - bit))
                                               : (uint8_t)(1u << bit);

        /* starting y_screen => starting x_phys */
        uint32_t y_screen0 = (uint32_t)y + sy0;
        uint16_t x_phys = (uint16_t)((DISPLAY_WIDTH - 1u) - (uint16_t)y_screen0);

        uint16_t dst_byte_ix = (uint16_t)(x_phys >> 3);
        uint8_t  dst_mask = (uint8_t)(1u << (x_phys & 7u));
        const uint8_t *row_mask_col = src_mask + ((uint32_t)sy0 * (uint32_t)src_stride_bytes) + src_byte_ix;
        const uint8_t *row_on_col = src_on + ((uint32_t)sy0 * (uint32_t)src_stride_bytes) + src_byte_ix;

        bool row_dirty = false;
        uint32_t run = sy1 - sy0;

        while (run != 0u) {
            run--;
            if ((*row_mask_col & src_bitmask) != 0u) {

                /* opaque pixel: set op bit */
                dst_op[dst_byte_ix] |= dst_mask;

                /* choose black/white from src_on */
                if ((*row_on_col & src_bitmask) != 0u) {
                    dst_on[dst_byte_ix] |= dst_mask;   /* white */
                } else {
                    dst_on[dst_byte_ix] &= (uint8_t)~dst_mask; /* black */
                }

                row_dirty = true;
            }
            row_mask_col += src_stride_bytes;
            row_on_col += src_stride_bytes;

            /* advance: next sy => x_phys-- */
            if (dst_mask == 0x01u) {
                if (dst_byte_ix == 0u) {
                    if (run != 0u) {
                        g_render_blit_bounds_break_count++;
                    }
                    break;
                }
                dst_byte_ix--;
                dst_mask = 0x80u;
            } else {
                dst_mask >>= 1;
            }
        }

        if (row_dirty) {
            dirty_set_row((uint16_t)(y_phys + 1u));
        }
    }
}



void Render_DrawSprite(const sprite1_t *spr,
                       uint16_t x, uint16_t y,
                       render_layer_t layer,
                       uint8_t scale)
{
    if (spr == NULL || spr->on == NULL || spr->mask == NULL) {
        return;
    }

    if (scale <= 1u) {
        renderBlitMasked1bpp(x, y,
                             spr->w, spr->h,
                             spr->on, spr->mask,
                             spr->stride,
                             spr->leftmost_is_msb,
                             layer);
    } else {
        renderBlitMasked1bppScaled(x, y,
                                   spr->w, spr->h,
                                   spr->on, spr->mask,
                                   spr->stride,
                                   spr->leftmost_is_msb,
                                   layer,
                                   scale);
    }
}





void renderFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    render_layer_t layer, render_color_t color)
{
    if ((w == 0u) || (h == 0u)) {
        return;
    }
    if ((color != RENDER_COLOR_BLACK) && (color != RENDER_COLOR_WHITE)) {
        return;
    }

    uint32_t x0 = x;
    uint32_t y0 = y;
    uint32_t x1 = (uint32_t)x + (uint32_t)w; /* exclusive */
    uint32_t y1 = (uint32_t)y + (uint32_t)h; /* exclusive */

    if (x0 >= RENDER_WIDTH || y0 >= RENDER_HEIGHT) {
        return;
    }
    if (x1 > RENDER_WIDTH)  { x1 = RENDER_WIDTH; }
    if (y1 > RENDER_HEIGHT) { y1 = RENDER_HEIGHT; }

    if (s_clip_enabled) {
        if (x0 < s_clip_x0) x0 = s_clip_x0;
        if (y0 < s_clip_y0) y0 = s_clip_y0;
        if (x1 > s_clip_x1) x1 = s_clip_x1;
        if (y1 > s_clip_y1) y1 = s_clip_y1;
        if ((x1 <= x0) || (y1 <= y0)) {
            return;
        }
    }

    const uint16_t y_screen0 = (uint16_t)y0;
    const uint16_t y_screen1 = (uint16_t)(y1 - 1u);

    uint16_t x_phys_lo = (uint16_t)((DISPLAY_WIDTH - 1u) - y_screen1);
    uint16_t x_phys_hi = (uint16_t)((DISPLAY_WIDTH - 1u) - y_screen0);

    for (uint32_t xs = x0; xs < x1; xs++) {
        uint16_t y_phys = (uint16_t)xs;
        plane_write_span_phys(y_phys, x_phys_lo, x_phys_hi, layer, color);
    }
}

static inline int32_t i32_abs(int32_t v)
{
    return (v < 0) ? -v : v;
}

static inline int32_t i32_sign(int32_t v)
{
    return (v > 0) ? 1 : (v < 0) ? -1 : 0;
}

static inline void swap_i32(int32_t *a, int32_t *b)
{
    int32_t t = *a;
    *a = *b;
    *b = t;
}

static inline void render_set_pixel_signed(int32_t x, int32_t y,
                                           render_layer_t layer, render_color_t color)
{
    if ((x < 0) || (y < 0)) {
        return;
    }
    if (((uint32_t)x >= RENDER_WIDTH) || ((uint32_t)y >= RENDER_HEIGHT)) {
        return;
    }
    renderSetPixel((uint16_t)x, (uint16_t)y, layer, color);
}

static void render_draw_hspan(int32_t y, int32_t x0, int32_t x1,
                              render_layer_t layer, render_color_t color)
{
    if ((y < 0) || (y >= (int32_t)RENDER_HEIGHT)) {
        return;
    }
    if (x0 > x1) {
        swap_i32(&x0, &x1);
    }
    if ((x1 < 0) || (x0 >= (int32_t)RENDER_WIDTH)) {
        return;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 >= (int32_t)RENDER_WIDTH) {
        x1 = (int32_t)RENDER_WIDTH - 1;
    }

    renderFillRect((uint16_t)x0, (uint16_t)y, (uint16_t)(x1 - x0 + 1), 1u, layer, color);
}


static void render_draw_vspan(int32_t x, int32_t y0, int32_t y1,
                              render_layer_t layer, render_color_t color)
{
    if ((x < 0) || (x >= (int32_t)RENDER_WIDTH)) {
        return;
    }
    if (y0 > y1) {
        swap_i32(&y0, &y1);
    }
    if ((y1 < 0) || (y0 >= (int32_t)RENDER_HEIGHT)) {
        return;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (y1 >= (int32_t)RENDER_HEIGHT) {
        y1 = (int32_t)RENDER_HEIGHT - 1;
    }

    renderFillRect((uint16_t)x, (uint16_t)y0, 1u, (uint16_t)(y1 - y0 + 1), layer, color);
}


static void render_draw_line_thin(int32_t x0, int32_t y0,
                                  int32_t x1, int32_t y1,
                                  render_layer_t layer, render_color_t color)
{
    int32_t dx = i32_abs(x1 - x0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = -i32_abs(y1 - y0);
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    for (;;) {
        render_set_pixel_signed(x0, y0, layer, color);
        if ((x0 == x1) && (y0 == y1)) {
            break;
        }
        int32_t e2 = err << 1;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void render_draw_line_outward(int32_t x0, int32_t y0,
                                     int32_t x1, int32_t y1,
                                     render_layer_t layer, render_color_t color,
                                     uint16_t thickness)
{
    if (thickness == 0u) {
        return;
    }

    if (thickness <= 1u) {
        render_draw_line_thin(x0, y0, x1, y1, layer, color);
        return;
    }

    int32_t r_lo = (int32_t)(thickness - 1u) / 2;
    int32_t r_hi = (int32_t)thickness / 2;

    int32_t dx = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = (y0 < y1) ? (y1 - y0) : (y0 - y1);
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx - dy;

    if (dx >= dy) {
        for (;;) {
            render_draw_vspan(x0, y0 - r_lo, y0 + r_hi, layer, color);
            if ((x0 == x1) && (y0 == y1)) {
                break;
            }
            int32_t e2 = err * 2;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    } else {
        for (;;) {
            render_draw_hspan(y0, x0 - r_lo, x0 + r_hi, layer, color);
            if ((x0 == x1) && (y0 == y1)) {
                break;
            }
            int32_t e2 = err * 2;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static void render_draw_circle_outline(int32_t cx, int32_t cy, int32_t radius,
                                       render_layer_t layer, render_color_t color)
{
    int32_t x = radius;
    int32_t y = 0;
    int32_t err = 1 - x;

    while (x >= y) {
        render_set_pixel_signed(cx + x, cy + y, layer, color);
        render_set_pixel_signed(cx + y, cy + x, layer, color);
        render_set_pixel_signed(cx - y, cy + x, layer, color);
        render_set_pixel_signed(cx - x, cy + y, layer, color);
        render_set_pixel_signed(cx - x, cy - y, layer, color);
        render_set_pixel_signed(cx - y, cy - x, layer, color);
        render_set_pixel_signed(cx + y, cy - x, layer, color);
        render_set_pixel_signed(cx + x, cy - y, layer, color);

        y++;
        if (err < 0) {
            err += (2 * y) + 1;
        } else {
            x--;
            err += (2 * (y - x)) + 1;
        }
    }
}

static void render_draw_circle_fill(int32_t cx, int32_t cy, int32_t radius,
                                    render_layer_t layer, render_color_t color)
{
    int32_t x = radius;
    int32_t y = 0;
    int32_t err = 1 - x;

    while (x >= y) {
        render_draw_hspan(cy + y, cx - x, cx + x, layer, color);
        render_draw_hspan(cy - y, cx - x, cx + x, layer, color);
        render_draw_hspan(cy + x, cx - y, cx + y, layer, color);
        render_draw_hspan(cy - x, cx - y, cx + y, layer, color);

        y++;
        if (err < 0) {
            err += (2 * y) + 1;
        } else {
            x--;
            err += (2 * (y - x)) + 1;
        }
    }
}

static void render_draw_triangle_fill(int32_t x0, int32_t y0,
                                      int32_t x1, int32_t y1,
                                      int32_t x2, int32_t y2,
                                      render_layer_t layer, render_color_t color)
{
    if (y0 > y1) {
        swap_i32(&y0, &y1);
        swap_i32(&x0, &x1);
    }
    if (y1 > y2) {
        swap_i32(&y1, &y2);
        swap_i32(&x1, &x2);
    }
    if (y0 > y1) {
        swap_i32(&y0, &y1);
        swap_i32(&x0, &x1);
    }

    if (y0 == y2) {
        int32_t minx = x0;
        int32_t maxx = x0;
        if (x1 < minx) {
            minx = x1;
        }
        if (x2 < minx) {
            minx = x2;
        }
        if (x1 > maxx) {
            maxx = x1;
        }
        if (x2 > maxx) {
            maxx = x2;
        }
        render_draw_hspan(y0, minx, maxx, layer, color);
        return;
    }

    int32_t dx01 = x1 - x0;
    int32_t dy01 = y1 - y0;
    int32_t dx02 = x2 - x0;
    int32_t dy02 = y2 - y0;
    int32_t dx12 = x2 - x1;
    int32_t dy12 = y2 - y1;

    int32_t sa = 0;
    int32_t sb = 0;

    int32_t y;
    int32_t last = (y1 == y2) ? y1 : (y1 - 1);

    for (y = y0; y <= last; y++) {
        int32_t a = x0 + ((dy01 != 0) ? (sa / dy01) : 0);
        int32_t b = x0 + ((dy02 != 0) ? (sb / dy02) : 0);
        sa += dx01;
        sb += dx02;
        if (a > b) {
            swap_i32(&a, &b);
        }
        render_draw_hspan(y, a, b, layer, color);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
        int32_t a = x1 + ((dy12 != 0) ? (sa / dy12) : 0);
        int32_t b = x0 + ((dy02 != 0) ? (sb / dy02) : 0);
        sa += dx12;
        sb += dx02;
        if (a > b) {
            swap_i32(&a, &b);
        }
        render_draw_hspan(y, a, b, layer, color);
    }
}

void renderDrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                    render_layer_t layer, render_color_t color, uint16_t thickness)
{
    if (thickness == 0u) {
        return;
    }
    render_draw_line_outward((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1,
                             layer, color, thickness);
}

void renderDrawRectOutline(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           render_layer_t layer, render_color_t color, uint16_t thickness)
{
    if ((w == 0u) || (h == 0u) || (thickness == 0u)) {
        return;
    }

    uint16_t x1 = (uint16_t)(x + w - 1u);
    uint16_t y1 = (uint16_t)(y + h - 1u);

    renderDrawLine(x, y, x1, y, layer, color, thickness);
    renderDrawLine(x1, y, x1, y1, layer, color, thickness);
    renderDrawLine(x1, y1, x, y1, layer, color, thickness);
    renderDrawLine(x, y1, x, y, layer, color, thickness);
}

void renderDrawCircle(uint16_t cx, uint16_t cy, uint16_t radius,
                      render_layer_t layer, render_color_t color,
                      bool filled, uint16_t thickness)
{
    if (!filled && (thickness == 0u)) {
        return;
    }

    if (radius == 0u) {
        renderSetPixel(cx, cy, layer, color);
        return;
    }

    if (filled) {
        render_draw_circle_fill((int32_t)cx, (int32_t)cy, (int32_t)radius, layer, color);
    }

    for (uint16_t i = 0u; i < thickness; i++) {
        render_draw_circle_outline((int32_t)cx, (int32_t)cy,
                                   (int32_t)radius + (int32_t)i, layer, color);
    }
}

void renderDrawTriangle(uint16_t x0, uint16_t y0,
                        uint16_t x1, uint16_t y1,
                        uint16_t x2, uint16_t y2,
                        render_layer_t layer, render_color_t color,
                        bool filled, uint16_t thickness)
{
    if (!filled && (thickness == 0u)) {
        return;
    }

    int32_t xi0 = (int32_t)x0;
    int32_t yi0 = (int32_t)y0;
    int32_t xi1 = (int32_t)x1;
    int32_t yi1 = (int32_t)y1;
    int32_t xi2 = (int32_t)x2;
    int32_t yi2 = (int32_t)y2;

    if (filled) {
        render_draw_triangle_fill(xi0, yi0, xi1, yi1, xi2, yi2, layer, color);
    }

    if (thickness == 0u) {
        return;
    }

    int32_t area2 = (xi1 - xi0) * (yi2 - yi0) - (xi2 - xi0) * (yi1 - yi0);
    if (area2 > 0) {
        renderDrawLine(x1, y1, x0, y0, layer, color, thickness);
        renderDrawLine(x2, y2, x1, y1, layer, color, thickness);
        renderDrawLine(x0, y0, x2, y2, layer, color, thickness);
    } else {
        renderDrawLine(x0, y0, x1, y1, layer, color, thickness);
        renderDrawLine(x1, y1, x2, y2, layer, color, thickness);
        renderDrawLine(x2, y2, x0, y0, layer, color, thickness);
    }
}

void Render_SetModeIndicator(th_mode_t mode)
{
    s_mode_current = mode;
}


void Render_MarkDirtyAll(void)
{
    for (uint32_t i = 0u; i < (uint32_t)DIRTY_WORD_COUNT; i++) {
        s_dirty_mask[i] = 0xFFFFFFFFu;
    }
    s_dirty_mask[DIRTY_WORD_COUNT - 1u] &= dirty_last_word_mask();
}

void Render_MarkDirtyList(const uint16_t *rows, uint16_t row_count)
{
    if ((rows == NULL) || (row_count == 0u)) {
        return;
    }

    for (uint16_t i = 0u; i < row_count; i++) {
        dirty_set_row(rows[i]);
    }
}

bool Render_TakeDirtyRows(uint16_t *rows, uint16_t max_rows,
                          uint16_t *out_count, bool *out_full)
{
    static const uint16_t kDirtyFullThreshold = (DISPLAY_HEIGHT / 2u);

    if ((rows == NULL) || (out_count == NULL) || (out_full == NULL)) {
        return false;
    }

    uint16_t total = dirty_count_rows();
    if (total == 0u) {
        return false;
    }

    if ((total >= kDirtyFullThreshold) || (total > max_rows)) {
        compose_all_rows_to_fb();
        dirty_clear_all();
        *out_count = 0u;
        *out_full = true;
        return true;
    }

    uint16_t count = 0u;
    const uint32_t last_mask = dirty_last_word_mask();

    for (uint32_t wi = 0u; wi < (uint32_t)DIRTY_WORD_COUNT; wi++) {
        uint32_t w = s_dirty_mask[wi];
        if (wi == ((uint32_t)DIRTY_WORD_COUNT - 1u)) {
            w &= last_mask;
        }

        while (w != 0u) {
            uint32_t bit = (uint32_t)__builtin_ctz(w);
            uint16_t row = (uint16_t)(wi * 32u + bit + 1u);
            uint16_t y_phys = (uint16_t)(row - 1u);

            compose_row_to_fb(y_phys);
            rows[count++] = row;

            w &= (w - 1u);
        }

        s_dirty_mask[wi] = 0u;
    }

    *out_count = count;
    *out_full = false;
    return true;
}

void renderDrawChar(uint16_t x, uint16_t y, char c,
                    render_layer_t layer, render_color_t color)
{
    if ((color != RENDER_COLOR_BLACK) && (color != RENDER_COLOR_WHITE)) {
        return;
    }

    uint8_t ch = (uint8_t)c;
    if ((ch < FONT8X8_START_CHAR) || (ch > FONT8X8_END_CHAR)) {
        ch = (uint8_t)' ';
    }

    const uint8_t *glyph = font8x8_basic[ch];
    renderBlit1bpp(x, y, FONT8X8_WIDTH, FONT8X8_HEIGHT,
                   glyph, 1u, false, layer, color);
}

void renderDrawText(uint16_t x, uint16_t y, const char *text,
                    render_layer_t layer, render_color_t color)
{
    if (text == NULL) {
        return;
    }

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + FONT8X8_HEIGHT);
            if (cursor_y >= RENDER_HEIGHT) {
                break;
            }
            continue;
        }
        if (*p == '\r') {
            continue;
        }

        if ((uint16_t)(cursor_x + FONT8X8_WIDTH) > RENDER_WIDTH) {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + FONT8X8_HEIGHT);
            if (cursor_y >= RENDER_HEIGHT) {
                break;
            }
        }

        renderDrawChar(cursor_x, cursor_y, *p, layer, color);
        cursor_x = (uint16_t)(cursor_x + FONT8X8_WIDTH);
    }
}

void renderDrawCharScaled(uint16_t x, uint16_t y, char c,
                          render_layer_t layer, render_color_t color,
                          uint8_t scale)
{
    if ((color != RENDER_COLOR_BLACK) && (color != RENDER_COLOR_WHITE)) {
        return;
    }
    if ((scale == 0u) || (scale > 8u)) {
        return;
    }

    uint8_t ch = (uint8_t)c;
    if ((ch < FONT8X8_START_CHAR) || (ch > FONT8X8_END_CHAR)) {
        ch = (uint8_t)' ';
    }

    const uint8_t *glyph = font8x8_basic[ch];
    renderBlit1bppScaled(x, y, FONT8X8_WIDTH, FONT8X8_HEIGHT,
                         glyph, 1u, false, layer, color, scale);
}

void renderDrawTextScaled(uint16_t x, uint16_t y, const char *text,
                          render_layer_t layer, render_color_t color,
                          uint8_t scale)
{
    if (text == NULL) {
        return;
    }
    if ((scale == 0u) || (scale > 8u)) {
        return;
    }

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    uint16_t step_x = (uint16_t)(FONT8X8_WIDTH * scale);
    uint16_t step_y = (uint16_t)(FONT8X8_HEIGHT * scale);

    for (const char *p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + step_y);
            if (cursor_y >= RENDER_HEIGHT) {
                break;
            }
            continue;
        }
        if (*p == '\r') {
            continue;
        }

        if ((uint16_t)(cursor_x + step_x) > RENDER_WIDTH) {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + step_y);
            if (cursor_y >= RENDER_HEIGHT) {
                break;
            }
        }

        renderDrawCharScaled(cursor_x, cursor_y, *p, layer, color, scale);
        cursor_x = (uint16_t)(cursor_x + step_x);
    }
}
