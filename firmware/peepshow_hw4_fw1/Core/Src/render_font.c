#include "render_font.h"
#include "font8x8_basic.h"
#include "font_assets_autogen.h"

static const render_font_t k_render_font_builtin_8x8 =
{
    .glyph_w = FONT8X8_WIDTH,
    .glyph_h = FONT8X8_HEIGHT,
    .first_char = FONT8X8_START_CHAR,
    .last_char = FONT8X8_END_CHAR,
    .glyph_row_stride_bytes = 1u,
    .leftmost_is_msb = false,
    .glyph_data = (const uint8_t *)font8x8_basic
};

static const render_font_t *s_render_font_default = &g_render_font_default_asset;
static const render_font_family_t *s_render_font_family_default = &g_render_font_family_default_asset;

static bool RenderFont_IsValid(const render_font_t *font)
{
    if (font == 0) {
        return false;
    }
    if ((font->glyph_data == 0) ||
        (font->glyph_w == 0u) ||
        (font->glyph_h == 0u) ||
        (font->glyph_row_stride_bytes == 0u) ||
        (font->first_char > font->last_char)) {
        return false;
    }
    return true;
}

const render_font_t *RenderFont_GetBuiltIn8x8(void)
{
    return &k_render_font_builtin_8x8;
}

const render_font_t *RenderFont_GetDefault(void)
{
    if (s_render_font_default == 0) {
        return &k_render_font_builtin_8x8;
    }
    return s_render_font_default;
}

void RenderFont_SetDefault(const render_font_t *font)
{
    if (!RenderFont_IsValid(font)) {
        return;
    }

    s_render_font_default = font;
}

void RenderFont_ResetDefault(void)
{
    s_render_font_default = &g_render_font_default_asset;
}

const uint8_t *RenderFont_GetGlyph(const render_font_t *font, uint8_t ch)
{
    uint32_t glyph_span;
    uint32_t glyph_index;

    if (font == 0) {
        return 0;
    }
    if (!RenderFont_IsValid(font)) {
        return 0;
    }

    if (ch < font->first_char || ch > font->last_char) {
        ch = (uint8_t)' ';
        if (ch < font->first_char || ch > font->last_char) {
            ch = font->first_char;
        }
    }

    glyph_span = (uint32_t)font->glyph_h * (uint32_t)font->glyph_row_stride_bytes;
    glyph_index = (uint32_t)(ch - font->first_char);
    return font->glyph_data + (glyph_index * glyph_span);
}

const render_font_family_t *RenderFont_GetDefaultFamily(void)
{
    if (s_render_font_family_default == 0) {
        return &g_render_font_family_default_asset;
    }
    return s_render_font_family_default;
}

void RenderFont_SetDefaultFamily(const render_font_family_t *family)
{
    if (family == 0) {
        return;
    }
    s_render_font_family_default = family;
}

void RenderFont_ResetDefaultFamily(void)
{
    s_render_font_family_default = &g_render_font_family_default_asset;
}

const render_font_t *RenderFont_SelectFromFamily(const render_font_family_t *family,
                                                 uint8_t ch,
                                                 uint8_t style_flags)
{
    const render_font_family_t *fam = family;
    const render_font_t *font = 0;

    if (fam == 0) {
        fam = RenderFont_GetDefaultFamily();
    }

    if (fam != 0) {
        if (((style_flags & RENDER_TEXT_STYLE_TINY) != 0u) && RenderFont_IsValid(fam->tiny)) {
            font = fam->tiny;
        }

        if ((font == 0) && ((style_flags & RENDER_TEXT_STYLE_ITALIC) != 0u)) {
            if ((ch >= (uint8_t)'A') && (ch <= (uint8_t)'Z') && RenderFont_IsValid(fam->italic_upper)) {
                font = fam->italic_upper;
            } else if ((ch >= (uint8_t)'a') && (ch <= (uint8_t)'z') && RenderFont_IsValid(fam->italic_lower)) {
                font = fam->italic_lower;
            } else if (RenderFont_IsValid(fam->italic_upper)) {
                font = fam->italic_upper;
            } else if (RenderFont_IsValid(fam->italic_lower)) {
                font = fam->italic_lower;
            }
        }

        if ((font == 0) && ((style_flags & RENDER_TEXT_STYLE_BOLD) != 0u) && RenderFont_IsValid(fam->bold)) {
            font = fam->bold;
        }

        if ((font == 0) && RenderFont_IsValid(fam->regular)) {
            font = fam->regular;
        }
    }

    if (!RenderFont_IsValid(font)) {
        font = RenderFont_GetDefault();
    }
    if (!RenderFont_IsValid(font)) {
        font = RenderFont_GetBuiltIn8x8();
    }
    return font;
}
