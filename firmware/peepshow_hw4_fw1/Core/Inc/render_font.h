#ifndef RENDER_FONT_H
#define RENDER_FONT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint8_t glyph_w;
    uint8_t glyph_h;
    uint8_t first_char;
    uint8_t last_char;
    uint8_t glyph_row_stride_bytes;
    bool leftmost_is_msb;
    const uint8_t *glyph_data;
} render_font_t;

typedef struct
{
    const render_font_t *regular;
    const render_font_t *bold;
    const render_font_t *italic_lower;
    const render_font_t *italic_upper;
    const render_font_t *tiny;
} render_font_family_t;

enum
{
    RENDER_TEXT_STYLE_BOLD = (1u << 0),
    RENDER_TEXT_STYLE_ITALIC = (1u << 1),
    RENDER_TEXT_STYLE_TINY = (1u << 2)
};

const render_font_t *RenderFont_GetBuiltIn8x8(void);

const render_font_t *RenderFont_GetDefault(void);
void RenderFont_SetDefault(const render_font_t *font);
void RenderFont_ResetDefault(void);

const uint8_t *RenderFont_GetGlyph(const render_font_t *font, uint8_t ch);

const render_font_family_t *RenderFont_GetDefaultFamily(void);
void RenderFont_SetDefaultFamily(const render_font_family_t *family);
void RenderFont_ResetDefaultFamily(void);
const render_font_t *RenderFont_SelectFromFamily(const render_font_family_t *family,
                                                 uint8_t ch,
                                                 uint8_t style_flags);

#endif /* RENDER_FONT_H */
