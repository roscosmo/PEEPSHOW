#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "LS013B7DH05.h"
#include "render_font.h"
#include "th_mode.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RENDER_LAYER_BG = 0u,
    RENDER_LAYER_GAME = 1u,
    RENDER_LAYER_UI = 2u
} render_layer_t;

typedef enum {
    RENDER_COLOR_BLACK = 0u,
    RENDER_COLOR_WHITE = 1u,
    RENDER_COLOR_TRANSPARENT = 2u
} render_color_t;

typedef struct
{
    uint16_t w;
    uint16_t h;
    uint16_t stride;              /* bytes per source row */
    const uint8_t *on;            /* 1 = white, 0 = black */
    const uint8_t *mask;          /* 1 = opaque, 0 = transparent */
    bool leftmost_is_msb;         /* source bit order */
} sprite1_t;

typedef struct
{
    uint16_t w;
    uint16_t h;
    uint16_t color_stride;        /* bytes per source row for 2bpp color plane */
    uint16_t mask_stride;         /* bytes per source row for 1bpp mask plane (0 -> fully opaque) */
    const uint8_t *color2bpp;     /* packed 2bpp, pixels per byte: [7:6][5:4][3:2][1:0] */
    const uint8_t *mask;          /* optional 1bpp mask: 1=opaque, 0=transparent */
    bool leftmost_is_msb;         /* bit order for both color groups and mask bits */
} sprite2_t;

typedef enum
{
    RENDER_2BPP_PRESENT_BINARY_CLAMP = 0u,
    RENDER_2BPP_PRESENT_BAYER2X2 = 1u
} render_2bpp_present_t;


#define RENDER_WIDTH  (DISPLAY_HEIGHT)
#define RENDER_HEIGHT (DISPLAY_WIDTH)

void Render_Init(void);
const uint8_t *Render_GetBuffer(void);

void Render_WriteBgRowSpanPhys(uint16_t y_phys,
                               uint16_t x_phys0, uint16_t x_phys1,
                               const uint8_t *row_bytes);

void Render_SetClipRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void Render_ClearClip(void);
bool Render_ShiftLayer(render_layer_t layer, int16_t dx, int16_t dy, render_color_t fill_color);

void renderClear(render_color_t bg_color);
void renderSetPixel(uint16_t x, uint16_t y, render_layer_t layer, render_color_t color);

/*
 * 1bpp blit:
 *   - reads bits from src
 *   - writes only where src bit == 1
 *   - treats 0 bits as transparent (does not modify destination)
 */
void renderBlit1bpp(uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h,
                    const uint8_t *src, uint16_t src_stride_bytes,
                    bool leftmost_is_msb,
                    render_layer_t layer,
                    render_color_t one_bits_color);

void renderBlit1bppScaled(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          const uint8_t *src, uint16_t src_stride_bytes,
                          bool leftmost_is_msb,
                          render_layer_t layer,
                          render_color_t one_bits_color,
                          uint8_t scale);

/*
 * Masked 1bpp blit (true transparency support):
 *   - src_on:   1 bits are drawn as one_bits_color
 *   - src_mask: 1 bits are written (opaque), 0 bits are transparent
 *
 * Asset pipeline can generate these two planes from PNG (black/white/transparent).
 */
void renderBlitMasked1bpp(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          const uint8_t *src_on,
                          const uint8_t *src_mask,
                          uint16_t src_stride_bytes,
                          bool leftmost_is_msb,
                          render_layer_t layer);
void renderBlitMasked1bppFastNoClip(uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h,
                                    const uint8_t *src_on,
                                    const uint8_t *src_mask,
                                    uint16_t src_stride_bytes,
                                    bool leftmost_is_msb,
                                    render_layer_t layer);

void renderBlitMasked1bppScaled(uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h,
                                const uint8_t *src_on,
                                const uint8_t *src_mask,
                                uint16_t src_stride_bytes,
                                bool leftmost_is_msb,
                                render_layer_t layer,
                                uint8_t scale);

void renderBlit2bpp(uint16_t x, uint16_t y,
                    uint16_t w, uint16_t h,
                    const uint8_t *src_color2bpp, uint16_t src_color_stride_bytes,
                    bool leftmost_is_msb,
                    render_layer_t layer,
                    render_2bpp_present_t present_mode);

void renderBlit2bppScaled(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          const uint8_t *src_color2bpp, uint16_t src_color_stride_bytes,
                          bool leftmost_is_msb,
                          render_layer_t layer,
                          render_2bpp_present_t present_mode,
                          uint8_t scale);

void renderBlitMasked2bpp(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          const uint8_t *src_color2bpp,
                          const uint8_t *src_mask,
                          uint16_t src_color_stride_bytes,
                          uint16_t src_mask_stride_bytes,
                          bool leftmost_is_msb,
                          render_layer_t layer,
                          render_2bpp_present_t present_mode);

void renderBlitMasked2bppScaled(uint16_t x, uint16_t y,
                                uint16_t w, uint16_t h,
                                const uint8_t *src_color2bpp,
                                const uint8_t *src_mask,
                                uint16_t src_color_stride_bytes,
                                uint16_t src_mask_stride_bytes,
                                bool leftmost_is_msb,
                                render_layer_t layer,
                                render_2bpp_present_t present_mode,
                                uint8_t scale);

void Render_DrawSprite(const sprite1_t *spr,
                       uint16_t x, uint16_t y,
                       render_layer_t layer,
                       uint8_t scale);

void Render_DrawSprite2(const sprite2_t *spr,
                        uint16_t x, uint16_t y,
                        render_layer_t layer,
                        render_2bpp_present_t present_mode,
                        uint8_t scale);



void renderFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    render_layer_t layer, render_color_t color);
void renderClearRectTransparent(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                render_layer_t layer);

void renderDrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                    render_layer_t layer, render_color_t color, uint16_t thickness);

void renderDrawRectOutline(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           render_layer_t layer, render_color_t color, uint16_t thickness);

void renderDrawCircle(uint16_t cx, uint16_t cy, uint16_t radius,
                      render_layer_t layer, render_color_t color,
                      bool filled, uint16_t thickness);

void renderDrawTriangle(uint16_t x0, uint16_t y0,
                        uint16_t x1, uint16_t y1,
                        uint16_t x2, uint16_t y2,
                        render_layer_t layer, render_color_t color,
                        bool filled, uint16_t thickness);

void renderDrawChar(uint16_t x, uint16_t y, char c,
                    render_layer_t layer, render_color_t color);

void renderDrawText(uint16_t x, uint16_t y, const char *text,
                    render_layer_t layer, render_color_t color);

void renderDrawCharScaled(uint16_t x, uint16_t y, char c,
                          render_layer_t layer, render_color_t color,
                          uint8_t scale);

void renderDrawTextScaled(uint16_t x, uint16_t y, const char *text,
                          render_layer_t layer, render_color_t color,
                          uint8_t scale);

void renderDrawCharFont(uint16_t x, uint16_t y, char c,
                        render_layer_t layer, render_color_t color,
                        const render_font_t *font);

void renderDrawTextFont(uint16_t x, uint16_t y, const char *text,
                        render_layer_t layer, render_color_t color,
                        const render_font_t *font);

void renderDrawCharScaledFont(uint16_t x, uint16_t y, char c,
                              render_layer_t layer, render_color_t color,
                              uint8_t scale,
                              const render_font_t *font);

void renderDrawTextScaledFont(uint16_t x, uint16_t y, const char *text,
                              render_layer_t layer, render_color_t color,
                              uint8_t scale,
                              const render_font_t *font);

void renderDrawTextRich(uint16_t x, uint16_t y, const char *text,
                        render_layer_t layer, render_color_t color);

void renderDrawTextRichScaled(uint16_t x, uint16_t y, const char *text,
                              render_layer_t layer, render_color_t color,
                              uint8_t scale,
                              const render_font_family_t *family);

void Render_SetModeIndicator(th_mode_t mode);

void Render_MarkDirtyAll(void);
void Render_MarkDirtyList(const uint16_t *rows, uint16_t row_count);

bool Render_TakeDirtyRows(uint16_t *rows, uint16_t max_rows,
                          uint16_t *out_count, bool *out_full);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_RENDERER_H */
