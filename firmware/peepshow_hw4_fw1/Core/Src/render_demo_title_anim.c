#include "render_demo_title_anim.h"

#include <string.h>

#include "display_renderer.h"
#include "th_mode.h"
#include "ui/ui_test_mimic_autogen.h"

#define ANIM_TEST_RAPID_INVERT_MS (60U)
#define ANIM_TEST_SHAKE_STEP_MS   (45U)
#define ANIM_TEST_PULSE_STEP_MS   (80U)
#define ANIM_TEST_SLIDE_STEP_MS   (16U)
#define ANIM_TEST_SLIDE_STEP_PX   (4)
#define ANIM_TEST_NOISE_STEP_MS   (50U)
#define ANIM_TEST_GLITCH_STEP_MS  (70U)
#define ANIM_TEST_GHOST_STEP_MS   (80U)
#define ANIM_TEST_SHUTTER_STEP_MS (30U)
#define ANIM_TEST_BOUNCE_STEP_MS  (55U)
#define ANIM_TEST_DISSOLVE_STEP_MS (35U)
#define ANIM_TEST_DATAMOSH_STEP_MS (65U)
#define ANIM_TEST_RING_STEP_MS     (40U)
#define ANIM_TEST_CHROMA_STEP_MS   (45U)
#define ANIM_TEST_WAVE_STEP_MS      (32U)
#define ANIM_TEST_EDGE_BURN_STEP_MS (55U)
#define ANIM_TEST_TEAR_STEP_MS      (46U)
#define ANIM_TEST_SNAP_STEP_MS      (80U)
#define ANIM_TEST_TEMP_DITHER_STEP_MS (38U)
#define ANIM_TEST_BLINDS_STEP_MS      (42U)
#define ANIM_TEST_SCANROLL_STEP_MS    (30U)
#define ANIM_TEST_RING_INVERT_STEP_MS (34U)
#define ANIM_TEST_SHEAR_STEP_MS       (45U)
#define ANIM_TEST_SCALE_MIN       (1U)
#define ANIM_TEST_SCALE_MAX       (4U)
#define ANIM_TEST_1BPP_BYTES      ((uint32_t)UI_TEST_MIMIC_H * (uint32_t)UI_TEST_MIMIC_1BPP_STRIDE)
#define ANIM_TEST_2BPP_BYTES      ((uint32_t)UI_TEST_MIMIC_H * (uint32_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE)
#define ANIM_TEST_MASK_BYTES      ((uint32_t)UI_TEST_MIMIC_H * (uint32_t)UI_TEST_MIMIC_1BPP_STRIDE)
#define ANIM_TEST_GLITCH_BANDS    (4U)
#define ANIM_TEST_DISSOLVE_MAX_PHASE (16U)
#define ANIM_TEST_RING_THICKNESS     (3U)
#define ANIM_TEST_SPRITE_FAST_SKIP   (8)
#define ANIM_TEST_SPRITE_COUNT       ((uint8_t)(UI_TEST_MIMIC_SPRITE_COUNT * 2U))

typedef enum
{
  ANIM_TEST_EFFECT_NONE = 0U,
  ANIM_TEST_EFFECT_RAPID_INVERT = 1U,
  ANIM_TEST_EFFECT_SHAKE = 2U,
  ANIM_TEST_EFFECT_PULSE = 3U,
  ANIM_TEST_EFFECT_SLIDE_IN = 4U,
  ANIM_TEST_EFFECT_NOISE = 5U,
  ANIM_TEST_EFFECT_GLITCH_SLICE = 6U,
  ANIM_TEST_EFFECT_GHOST_TRAIL = 7U,
  ANIM_TEST_EFFECT_SHUTTER_WIPE = 8U,
  ANIM_TEST_EFFECT_BOUNCE = 9U,
  ANIM_TEST_EFFECT_DISSOLVE = 10U,
  ANIM_TEST_EFFECT_DATAMOSH = 11U,
  ANIM_TEST_EFFECT_RING_SHOCKWAVE = 12U,
  ANIM_TEST_EFFECT_CHROMA_PHASE = 13U,
  ANIM_TEST_EFFECT_WAVE_WARP = 14U,
  ANIM_TEST_EFFECT_EDGE_BURN = 15U,
  ANIM_TEST_EFFECT_TEAR_DROP = 16U,
  ANIM_TEST_EFFECT_SNAP_GLITCH = 17U,
  ANIM_TEST_EFFECT_TEMPORAL_DITHER = 18U,
  ANIM_TEST_EFFECT_VENETIAN_BLINDS = 19U,
  ANIM_TEST_EFFECT_SCANLINE_ROLL = 20U,
  ANIM_TEST_EFFECT_SHOCK_RING_INVERT = 21U,
  ANIM_TEST_EFFECT_MIRROR_SHEAR = 22U,
  ANIM_TEST_EFFECT_COUNT = 23U
} anim_test_effect_t;

typedef struct
{
  uint8_t initialized;
  uint8_t sprite_idx;
  uint8_t effect_idx;
  uint8_t effect_playing;
  uint8_t invert_phase;
  uint8_t shake_phase;
  uint8_t pulse_phase;
  uint8_t scale;
  int16_t slide_offset_x;
  uint16_t effect_ms_accum;
  uint16_t noise_frame;
  uint8_t glitch_phase;
  uint8_t ghost_phase;
  uint8_t bounce_phase;
  uint16_t shutter_phase;
  uint8_t dissolve_phase;
  uint8_t dissolve_dir;
  uint8_t datamosh_phase;
  uint8_t chroma_phase;
  uint16_t ring_radius;
  uint8_t wave_phase;
  uint8_t edge_phase;
  uint8_t tear_phase;
  uint8_t snap_phase;
  uint8_t temporal_phase;
  uint8_t blinds_phase;
  uint8_t scan_phase;
  uint16_t ring_invert_radius;
  uint8_t shear_phase;
  uint8_t bg_mode;
} render_demo_title_anim_state_t;

static render_demo_title_anim_state_t s_title_anim;
static uint8_t s_anim_noise_1bpp[ANIM_TEST_1BPP_BYTES];
static uint8_t s_anim_noise_2bpp[ANIM_TEST_2BPP_BYTES];
static uint8_t s_anim_mask_temp[ANIM_TEST_MASK_BYTES];

typedef enum
{
  ANIM_TEST_BG_BLACK = 0U,
  ANIM_TEST_BG_WHITE = 1U,
  ANIM_TEST_BG_DITHER50 = 2U,
  ANIM_TEST_BG_COUNT = 3U
} anim_test_bg_mode_t;

static uint8_t RenderDemoTitleAnim_InputIsAction(ULONG event)
{
  return ((event == (ULONG)GAME_RT_INPUT_EVENT_PRESS) ||
          (event == (ULONG)GAME_RT_INPUT_EVENT_REPEAT) ||
          (event == (ULONG)GAME_RT_INPUT_EVENT_LONG)) ? 1U : 0U;
}

static uint32_t RenderDemoTitleAnim_XorShift32(uint32_t x)
{
  if (x == 0UL)
  {
    x = 0xA341316CUL;
  }
  x ^= (x << 13);
  x ^= (x >> 17);
  x ^= (x << 5);
  return x;
}

static uint16_t RenderDemoTitleAnim_CurrentSpriteAssetIndex(void)
{
  if (UI_TEST_MIMIC_SPRITE_COUNT == 0U)
  {
    return 0U;
  }
  return (uint16_t)(((uint16_t)s_title_anim.sprite_idx >> 1U) % (uint16_t)UI_TEST_MIMIC_SPRITE_COUNT);
}

static uint8_t RenderDemoTitleAnim_CurrentSpriteIs2bpp(void)
{
  return ((s_title_anim.sprite_idx & 0x1U) != 0U) ? 1U : 0U;
}

static const ui_test_mimic_sprite_entry_t *RenderDemoTitleAnim_CurrentSprite(void)
{
  return &ui_test_mimic_sprites[RenderDemoTitleAnim_CurrentSpriteAssetIndex()];
}

static void RenderDemoTitleAnim_CycleBgMode(void)
{
  s_title_anim.bg_mode = (uint8_t)((s_title_anim.bg_mode + 1U) % (uint8_t)ANIM_TEST_BG_COUNT);
}

static void RenderDemoTitleAnim_DrawBackground(void)
{
  uint16_t y;
  uint16_t x;

  if (s_title_anim.bg_mode == (uint8_t)ANIM_TEST_BG_WHITE)
  {
    renderClear(RENDER_COLOR_WHITE);
    return;
  }

  renderClear(RENDER_COLOR_BLACK);
  if (s_title_anim.bg_mode != (uint8_t)ANIM_TEST_BG_DITHER50)
  {
    return;
  }

  for (y = 0U; y < (uint16_t)RENDER_HEIGHT; y++)
  {
    for (x = 0U; x < (uint16_t)RENDER_WIDTH; x++)
    {
      if (((x + y) & 0x1U) == 0U)
      {
        renderSetPixel(x, y, RENDER_LAYER_GAME, RENDER_COLOR_WHITE);
      }
    }
  }
}

static uint8_t RenderDemoTitleAnim_PairMaskFromNibble(uint8_t nibble)
{
  uint8_t pair_mask = 0U;
  if ((nibble & 0x1U) != 0U)
  {
    pair_mask |= 0x03U;
  }
  if ((nibble & 0x2U) != 0U)
  {
    pair_mask |= 0x0CU;
  }
  if ((nibble & 0x4U) != 0U)
  {
    pair_mask |= 0x30U;
  }
  if ((nibble & 0x8U) != 0U)
  {
    pair_mask |= 0xC0U;
  }
  return pair_mask;
}

static uint8_t RenderDemoTitleAnim_InvertActive(void)
{
  return ((s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_RAPID_INVERT) &&
          (s_title_anim.effect_playing != 0U) &&
          (s_title_anim.invert_phase != 0U)) ? 1U : 0U;
}

static uint8_t RenderDemoTitleAnim_DrawBaseScale(void)
{
  uint8_t scale = s_title_anim.scale;

  if ((s_title_anim.effect_playing != 0U) &&
      (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_PULSE))
  {
    switch (s_title_anim.pulse_phase & 0x3U)
    {
      case 1U:
      case 3U:
        if (scale < ANIM_TEST_SCALE_MAX)
        {
          scale++;
        }
        break;

      case 2U:
        if (scale < ANIM_TEST_SCALE_MAX)
        {
          scale++;
        }
        if (scale < ANIM_TEST_SCALE_MAX)
        {
          scale++;
        }
        break;

      default:
        break;
    }
  }

  return scale;
}

static uint16_t RenderDemoTitleAnim_EffectiveScale(uint8_t base_scale)
{
  uint16_t scale = (uint16_t)base_scale;

  if (RenderDemoTitleAnim_CurrentSpriteIs2bpp() != 0U)
  {
    scale = (uint16_t)(scale * 2U);
  }
  if (scale == 0U)
  {
    scale = 1U;
  }
  return scale;
}

static uint32_t RenderDemoTitleAnim_DrawWidth(uint16_t effective_scale)
{
  return ((uint32_t)UI_TEST_MIMIC_W * (uint32_t)effective_scale);
}

static uint32_t RenderDemoTitleAnim_DrawHeight(uint16_t effective_scale)
{
  return ((uint32_t)UI_TEST_MIMIC_H * (uint32_t)effective_scale);
}

static void RenderDemoTitleAnim_ComputeCenteredXY(uint16_t effective_scale, uint16_t *x_out, uint16_t *y_out)
{
  uint32_t draw_w = RenderDemoTitleAnim_DrawWidth(effective_scale);
  uint32_t draw_h = RenderDemoTitleAnim_DrawHeight(effective_scale);
  uint16_t x = 0U;
  uint16_t y = 0U;

  if ((x_out == (uint16_t *)0) || (y_out == (uint16_t *)0))
  {
    return;
  }

  if ((uint32_t)RENDER_WIDTH > draw_w)
  {
    x = (uint16_t)(((uint32_t)RENDER_WIDTH - draw_w) / 2U);
  }
  if ((uint32_t)RENDER_HEIGHT > draw_h)
  {
    y = (uint16_t)(((uint32_t)RENDER_HEIGHT - draw_h) / 2U);
  }

  *x_out = x;
  *y_out = y;
}

static void RenderDemoTitleAnim_BuildNoise1bpp(const uint8_t *src_plane, const uint8_t *src_mask)
{
  uint32_t seed = (0x9E3779B9UL ^ (uint32_t)s_title_anim.noise_frame);
  uint32_t i;

  if ((src_plane == (const uint8_t *)0) || (src_mask == (const uint8_t *)0))
  {
    return;
  }

  for (i = 0UL; i < ANIM_TEST_1BPP_BYTES; i++)
  {
    uint8_t rand_mask;
    uint8_t flip_mask;
    seed = RenderDemoTitleAnim_XorShift32(seed + i + 1UL);
    rand_mask = (uint8_t)(seed >> 24);
    flip_mask = (uint8_t)(rand_mask & src_mask[i]);
    s_anim_noise_1bpp[i] = (uint8_t)(src_plane[i] ^ flip_mask);
  }
}

static void RenderDemoTitleAnim_BuildNoise2bpp(const uint8_t *src_plane, const uint8_t *src_mask)
{
  uint32_t seed = (0x7F4A7C15UL ^ (uint32_t)s_title_anim.noise_frame);
  uint32_t y;
  uint32_t x;

  if ((src_plane == (const uint8_t *)0) || (src_mask == (const uint8_t *)0))
  {
    return;
  }

  for (y = 0UL; y < (uint32_t)UI_TEST_MIMIC_H; y++)
  {
    uint32_t color_row = (y * (uint32_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE);
    uint32_t mask_row = (y * (uint32_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE);

    for (x = 0UL; x < (uint32_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE; x++)
    {
      uint32_t idx = color_row + x;
      uint8_t mask_byte = src_mask[mask_row + (x >> 1)];
      uint8_t nibble;
      uint8_t pair_mask;

      seed = RenderDemoTitleAnim_XorShift32(seed + idx + 1UL);

      if (mask_byte == 0U)
      {
        s_anim_noise_2bpp[idx] = src_plane[idx];
        continue;
      }

      nibble = (uint8_t)((seed >> 28) & 0x0FU);
      pair_mask = RenderDemoTitleAnim_PairMaskFromNibble(nibble);
      s_anim_noise_2bpp[idx] = (uint8_t)(src_plane[idx] ^ pair_mask);
    }
  }
}

static void RenderDemoTitleAnim_BuildRowsMask(const uint8_t *src_mask,
                                              uint16_t mask_stride,
                                              uint16_t row_start,
                                              uint16_t row_end_exclusive)
{
  uint16_t y;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if (src_mask == (const uint8_t *)0)
  {
    return;
  }
  if (row_start >= (uint16_t)UI_TEST_MIMIC_H)
  {
    return;
  }
  if (row_end_exclusive > (uint16_t)UI_TEST_MIMIC_H)
  {
    row_end_exclusive = (uint16_t)UI_TEST_MIMIC_H;
  }
  if (row_end_exclusive <= row_start)
  {
    return;
  }

  for (y = row_start; y < row_end_exclusive; y++)
  {
    uint32_t row_off = (uint32_t)y * (uint32_t)mask_stride;
    (void)memcpy(&s_anim_mask_temp[row_off], &src_mask[row_off], (size_t)mask_stride);
  }
}

static void RenderDemoTitleAnim_BuildCheckerMask(const uint8_t *src_mask,
                                                 uint16_t mask_stride,
                                                 uint8_t phase,
                                                 uint8_t parity_seed)
{
  uint16_t y;
  uint16_t x;

  if (src_mask == (const uint8_t *)0)
  {
    (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));
    return;
  }

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    uint8_t pattern = (((uint8_t)(y + phase + parity_seed) & 0x1U) != 0U) ? 0xAAU : 0x55U;
    uint32_t row_off = (uint32_t)y * (uint32_t)mask_stride;
    for (x = 0U; x < mask_stride; x++)
    {
      s_anim_mask_temp[row_off + x] = (uint8_t)(src_mask[row_off + x] & pattern);
    }
  }
}

static uint16_t RenderDemoTitleAnim_ShutterRowsVisible(void)
{
  uint16_t h = (uint16_t)UI_TEST_MIMIC_H;
  uint16_t period = (uint16_t)(h * 2U);
  uint16_t p;

  if (period == 0U)
  {
    return 0U;
  }
  p = (uint16_t)(s_title_anim.shutter_phase % period);
  return (p < h) ? p : (uint16_t)(period - p);
}

static void RenderDemoTitleAnim_BuildDissolveMask(const uint8_t *src_mask,
                                                  uint16_t mask_stride,
                                                  uint8_t phase)
{
  static const uint8_t k_bayer4x4[4][4] = {
    {0U, 8U, 2U, 10U},
    {12U, 4U, 14U, 6U},
    {3U, 11U, 1U, 9U},
    {15U, 7U, 13U, 5U}
  };
  uint16_t y;
  uint16_t x;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if (src_mask == (const uint8_t *)0)
  {
    return;
  }

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    for (x = 0U; x < (uint16_t)UI_TEST_MIMIC_W; x++)
    {
      uint32_t byte_idx = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
      uint8_t bit = (uint8_t)(0x80U >> (x & 0x7U));
      uint8_t threshold = k_bayer4x4[y & 0x3U][x & 0x3U];

      if ((src_mask[byte_idx] & bit) == 0U)
      {
        continue;
      }
      if (threshold > phase)
      {
        continue;
      }
      s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
    }
  }
}

static void RenderDemoTitleAnim_BuildRingMask(const uint8_t *src_mask,
                                              uint16_t mask_stride,
                                              uint16_t radius,
                                              uint16_t thickness)
{
  int16_t cx = (int16_t)(UI_TEST_MIMIC_W / 2U);
  int16_t cy = (int16_t)(UI_TEST_MIMIC_H / 2U);
  int32_t outer = (int32_t)radius + (int32_t)thickness;
  int32_t inner = (int32_t)radius - (int32_t)thickness;
  int32_t outer2;
  int32_t inner2;
  uint16_t y;
  uint16_t x;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if (src_mask == (const uint8_t *)0)
  {
    return;
  }

  if (inner < 0)
  {
    inner = 0;
  }
  outer2 = outer * outer;
  inner2 = inner * inner;

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    for (x = 0U; x < (uint16_t)UI_TEST_MIMIC_W; x++)
    {
      int32_t dx = (int32_t)((int16_t)x - cx);
      int32_t dy = (int32_t)((int16_t)y - cy);
      int32_t d2 = (dx * dx) + (dy * dy);
      uint32_t byte_idx = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
      uint8_t bit = (uint8_t)(0x80U >> (x & 0x7U));

      if ((src_mask[byte_idx] & bit) == 0U)
      {
        continue;
      }
      if ((d2 < inner2) || (d2 > outer2))
      {
        continue;
      }
      s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
    }
  }
}

static void RenderDemoTitleAnim_BuildChroma2bpp(const uint8_t *src_plane, uint8_t phase)
{
  uint32_t i;
  uint8_t mode = (uint8_t)(phase & 0x3U);

  if (src_plane == (const uint8_t *)0)
  {
    return;
  }

  for (i = 0UL; i < ANIM_TEST_2BPP_BYTES; i++)
  {
    uint8_t v = src_plane[i];
    if (mode == 0U)
    {
      s_anim_noise_2bpp[i] = v;
    }
    else if (mode == 1U)
    {
      s_anim_noise_2bpp[i] = (uint8_t)(v ^ 0x55U);
    }
    else if (mode == 2U)
    {
      s_anim_noise_2bpp[i] = (uint8_t)(v ^ 0xAAU);
    }
    else
    {
      s_anim_noise_2bpp[i] = (uint8_t)((v << 2) | (v >> 6));
    }
  }
}

static void RenderDemoTitleAnim_BuildColumnsMask(const uint8_t *src_mask,
                                                 uint16_t mask_stride,
                                                 uint16_t col_start,
                                                 uint16_t col_end_exclusive)
{
  uint16_t y;
  uint16_t x;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if (src_mask == (const uint8_t *)0)
  {
    return;
  }
  if (col_start >= (uint16_t)UI_TEST_MIMIC_W)
  {
    return;
  }
  if (col_end_exclusive > (uint16_t)UI_TEST_MIMIC_W)
  {
    col_end_exclusive = (uint16_t)UI_TEST_MIMIC_W;
  }
  if (col_end_exclusive <= col_start)
  {
    return;
  }

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    for (x = col_start; x < col_end_exclusive; x++)
    {
      uint32_t byte_idx = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
      uint8_t bit = (uint8_t)(0x80U >> (x & 0x7U));
      if ((src_mask[byte_idx] & bit) != 0U)
      {
        s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
      }
    }
  }
}

static void RenderDemoTitleAnim_BuildEdgeBurnMask(const uint8_t *src_mask,
                                                  uint16_t mask_stride,
                                                  uint8_t phase)
{
  uint16_t y;
  uint16_t x;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if (src_mask == (const uint8_t *)0)
  {
    return;
  }

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    for (x = 0U; x < (uint16_t)UI_TEST_MIMIC_W; x++)
    {
      uint32_t byte_idx = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
      uint8_t bit = (uint8_t)(0x80U >> (x & 0x7U));
      uint8_t pixel_on;
      uint8_t edge = 0U;

      pixel_on = ((src_mask[byte_idx] & bit) != 0U) ? 1U : 0U;
      if (pixel_on == 0U)
      {
        continue;
      }

      if ((x == 0U) || (x + 1U >= (uint16_t)UI_TEST_MIMIC_W) ||
          (y == 0U) || (y + 1U >= (uint16_t)UI_TEST_MIMIC_H))
      {
        edge = 1U;
      }
      else
      {
        uint16_t xl = (uint16_t)(x - 1U);
        uint16_t xr = (uint16_t)(x + 1U);
        uint16_t yu = (uint16_t)(y - 1U);
        uint16_t yd = (uint16_t)(y + 1U);
        uint32_t idx_l = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)xl >> 3);
        uint32_t idx_r = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)xr >> 3);
        uint32_t idx_u = ((uint32_t)yu * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
        uint32_t idx_d = ((uint32_t)yd * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
        uint8_t bit_l = (uint8_t)(0x80U >> (xl & 0x7U));
        uint8_t bit_r = (uint8_t)(0x80U >> (xr & 0x7U));
        uint8_t bit_u = (uint8_t)(0x80U >> (x & 0x7U));
        uint8_t bit_d = (uint8_t)(0x80U >> (x & 0x7U));
        if (((src_mask[idx_l] & bit_l) == 0U) ||
            ((src_mask[idx_r] & bit_r) == 0U) ||
            ((src_mask[idx_u] & bit_u) == 0U) ||
            ((src_mask[idx_d] & bit_d) == 0U))
        {
          edge = 1U;
        }
      }

      if (edge != 0U)
      {
        s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
      }
      else
      {
        uint32_t seed = (uint32_t)(x + ((uint16_t)(y * 37U))) ^ (uint32_t)(phase * 13U);
        uint8_t t = (uint8_t)(RenderDemoTitleAnim_XorShift32(seed) & 0x0FU);
        if (t <= phase)
        {
          s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
        }
      }
    }
  }
}

static void RenderDemoTitleAnim_BuildTemporalDitherMask(const uint8_t *src_mask,
                                                        uint16_t mask_stride,
                                                        uint8_t phase)
{
  static const uint8_t k_bayer4x4[16] = {
    0U, 8U, 2U, 10U,
    12U, 4U, 14U, 6U,
    3U, 11U, 1U, 9U,
    15U, 7U, 13U, 5U
  };
  uint8_t p = (uint8_t)(phase & 0x1FU);
  uint8_t level = (p <= 15U) ? p : (uint8_t)(31U - p);
  uint16_t y;
  uint16_t x;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if (src_mask == (const uint8_t *)0)
  {
    return;
  }

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    for (x = 0U; x < (uint16_t)UI_TEST_MIMIC_W; x++)
    {
      uint32_t byte_idx = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
      uint8_t bit = (uint8_t)(0x80U >> (x & 0x7U));
      uint8_t t = k_bayer4x4[((y & 0x3U) << 2) | (x & 0x3U)];

      if ((src_mask[byte_idx] & bit) == 0U)
      {
        continue;
      }
      if (t <= level)
      {
        s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
      }
    }
  }
}

static void RenderDemoTitleAnim_BuildVenetianMask(const uint8_t *src_mask,
                                                  uint16_t mask_stride,
                                                  uint8_t phase)
{
  const uint16_t band_h = 6U;
  uint8_t p = (uint8_t)(phase % 13U);
  uint8_t open = (p <= 6U) ? p : (uint8_t)(12U - p);
  uint16_t y;
  uint16_t x;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if ((src_mask == (const uint8_t *)0) || (open == 0U))
  {
    return;
  }

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    uint16_t local = (uint16_t)(y % band_h);
    uint16_t band = (uint16_t)(y / band_h);
    uint8_t visible = 0U;

    if ((band & 0x1U) == 0U)
    {
      visible = (local < (uint16_t)open) ? 1U : 0U;
    }
    else
    {
      visible = (local >= (uint16_t)(band_h - (uint16_t)open)) ? 1U : 0U;
    }
    if (visible == 0U)
    {
      continue;
    }

    for (x = 0U; x < (uint16_t)UI_TEST_MIMIC_W; x++)
    {
      uint32_t byte_idx = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
      uint8_t bit = (uint8_t)(0x80U >> (x & 0x7U));

      if ((src_mask[byte_idx] & bit) != 0U)
      {
        s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
      }
    }
  }
}

static void RenderDemoTitleAnim_BuildScanlineRollMask(const uint8_t *src_mask,
                                                      uint16_t mask_stride,
                                                      uint8_t phase)
{
  int16_t center = (int16_t)(phase % (uint8_t)UI_TEST_MIMIC_H);
  uint16_t y;
  uint16_t x;

  (void)memset(s_anim_mask_temp, 0, sizeof(s_anim_mask_temp));

  if (src_mask == (const uint8_t *)0)
  {
    return;
  }

  for (y = 0U; y < (uint16_t)UI_TEST_MIMIC_H; y++)
  {
    int16_t dy = (int16_t)y - center;
    if (dy < 0)
    {
      dy = (int16_t)(-dy);
    }

    for (x = 0U; x < (uint16_t)UI_TEST_MIMIC_W; x++)
    {
      uint32_t byte_idx = ((uint32_t)y * (uint32_t)mask_stride) + ((uint32_t)x >> 3);
      uint8_t bit = (uint8_t)(0x80U >> (x & 0x7U));

      if ((src_mask[byte_idx] & bit) == 0U)
      {
        continue;
      }
      if (dy <= 1)
      {
        continue;
      }
      if ((((y + (uint16_t)phase) & 0x1U) != 0U) &&
          (((x + (uint16_t)phase) & 0x3U) != 0U))
      {
        continue;
      }

      s_anim_mask_temp[byte_idx] = (uint8_t)(s_anim_mask_temp[byte_idx] | bit);
    }
  }
}

static void RenderDemoTitleAnim_StopEffect(void)
{
  s_title_anim.effect_playing = 0U;
  s_title_anim.invert_phase = 0U;
  s_title_anim.shake_phase = 0U;
  s_title_anim.pulse_phase = 0U;
  s_title_anim.slide_offset_x = 0;
  s_title_anim.glitch_phase = 0U;
  s_title_anim.ghost_phase = 0U;
  s_title_anim.bounce_phase = 0U;
  s_title_anim.shutter_phase = 0U;
  s_title_anim.dissolve_phase = 0U;
  s_title_anim.dissolve_dir = 1U;
  s_title_anim.datamosh_phase = 0U;
  s_title_anim.chroma_phase = 0U;
  s_title_anim.ring_radius = 0U;
  s_title_anim.wave_phase = 0U;
  s_title_anim.edge_phase = 0U;
  s_title_anim.tear_phase = 0U;
  s_title_anim.snap_phase = 0U;
  s_title_anim.temporal_phase = 0U;
  s_title_anim.blinds_phase = 0U;
  s_title_anim.scan_phase = 0U;
  s_title_anim.ring_invert_radius = 0U;
  s_title_anim.shear_phase = 0U;
  s_title_anim.effect_ms_accum = 0U;
}

static void RenderDemoTitleAnim_StartEffect(void)
{
  uint8_t base_scale;
  uint16_t effective_scale;
  uint32_t draw_w;

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_NONE)
  {
    RenderDemoTitleAnim_StopEffect();
    return;
  }

  RenderDemoTitleAnim_StopEffect();
  s_title_anim.effect_playing = 1U;

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SLIDE_IN)
  {
    base_scale = RenderDemoTitleAnim_DrawBaseScale();
    effective_scale = RenderDemoTitleAnim_EffectiveScale(base_scale);
    draw_w = RenderDemoTitleAnim_DrawWidth(effective_scale);
    s_title_anim.slide_offset_x = (int16_t)(-(int32_t)draw_w - 8);
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_NOISE)
  {
    s_title_anim.noise_frame++;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHUTTER_WIPE)
  {
    s_title_anim.shutter_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_DISSOLVE)
  {
    s_title_anim.dissolve_phase = 0U;
    s_title_anim.dissolve_dir = 1U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_DATAMOSH)
  {
    s_title_anim.datamosh_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_RING_SHOCKWAVE)
  {
    s_title_anim.ring_radius = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_CHROMA_PHASE)
  {
    s_title_anim.chroma_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_WAVE_WARP)
  {
    s_title_anim.wave_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_EDGE_BURN)
  {
    s_title_anim.edge_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_TEAR_DROP)
  {
    s_title_anim.tear_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SNAP_GLITCH)
  {
    s_title_anim.snap_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_TEMPORAL_DITHER)
  {
    s_title_anim.temporal_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_VENETIAN_BLINDS)
  {
    s_title_anim.blinds_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SCANLINE_ROLL)
  {
    s_title_anim.scan_phase = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHOCK_RING_INVERT)
  {
    s_title_anim.ring_invert_radius = 0U;
  }
  else if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_MIRROR_SHEAR)
  {
    s_title_anim.shear_phase = 0U;
  }
}

static void RenderDemoTitleAnim_EnsureInitialized(void)
{
  if (s_title_anim.initialized != 0U)
  {
    return;
  }

  s_title_anim.sprite_idx = 0U;
  s_title_anim.effect_idx = (uint8_t)ANIM_TEST_EFFECT_RAPID_INVERT;
  s_title_anim.effect_playing = 0U;
  s_title_anim.invert_phase = 0U;
  s_title_anim.shake_phase = 0U;
  s_title_anim.pulse_phase = 0U;
  s_title_anim.scale = ANIM_TEST_SCALE_MIN;
  s_title_anim.slide_offset_x = 0;
  s_title_anim.effect_ms_accum = 0U;
  s_title_anim.noise_frame = 0U;
  s_title_anim.glitch_phase = 0U;
  s_title_anim.ghost_phase = 0U;
  s_title_anim.bounce_phase = 0U;
  s_title_anim.shutter_phase = 0U;
  s_title_anim.dissolve_phase = 0U;
  s_title_anim.dissolve_dir = 1U;
  s_title_anim.datamosh_phase = 0U;
  s_title_anim.chroma_phase = 0U;
  s_title_anim.ring_radius = 0U;
  s_title_anim.wave_phase = 0U;
  s_title_anim.edge_phase = 0U;
  s_title_anim.tear_phase = 0U;
  s_title_anim.snap_phase = 0U;
  s_title_anim.temporal_phase = 0U;
  s_title_anim.blinds_phase = 0U;
  s_title_anim.scan_phase = 0U;
  s_title_anim.ring_invert_radius = 0U;
  s_title_anim.shear_phase = 0U;
  s_title_anim.bg_mode = (uint8_t)ANIM_TEST_BG_BLACK;
  s_title_anim.initialized = 1U;
}

static void RenderDemoTitleAnim_CycleSprite(int8_t step)
{
  uint8_t count = (uint8_t)ANIM_TEST_SPRITE_COUNT;
  int16_t idx;
  int16_t delta;

  if ((count == 0U) || (step == 0))
  {
    return;
  }

  idx = (int16_t)s_title_anim.sprite_idx;
  delta = (int16_t)step;
  idx = (int16_t)(idx + delta);
  while (idx < 0)
  {
    idx = (int16_t)(idx + (int16_t)count);
  }
  while (idx >= (int16_t)count)
  {
    idx = (int16_t)(idx - (int16_t)count);
  }
  s_title_anim.sprite_idx = (uint8_t)idx;
}

static void RenderDemoTitleAnim_CycleEffect(int8_t step)
{
  uint8_t idx = s_title_anim.effect_idx;
  uint8_t count = (uint8_t)ANIM_TEST_EFFECT_COUNT;

  if (count == 0U)
  {
    return;
  }

  if (step > 0)
  {
    idx = (uint8_t)((idx + 1U) % count);
  }
  else
  {
    idx = (idx == 0U) ? (uint8_t)(count - 1U) : (uint8_t)(idx - 1U);
  }

  s_title_anim.effect_idx = idx;
  RenderDemoTitleAnim_StopEffect();
}

static const char *RenderDemoTitleAnim_EffectLabel(void)
{
  switch (s_title_anim.effect_idx)
  {
    case (uint8_t)ANIM_TEST_EFFECT_RAPID_INVERT:
      return "FX: RAPID INVERT";
    case (uint8_t)ANIM_TEST_EFFECT_SHAKE:
      return "FX: SHAKE";
    case (uint8_t)ANIM_TEST_EFFECT_PULSE:
      return "FX: PULSE";
    case (uint8_t)ANIM_TEST_EFFECT_SLIDE_IN:
      return "FX: SLIDE-IN";
    case (uint8_t)ANIM_TEST_EFFECT_NOISE:
      return "FX: NOISE";
    case (uint8_t)ANIM_TEST_EFFECT_GLITCH_SLICE:
      return "FX: GLITCH SLICE";
    case (uint8_t)ANIM_TEST_EFFECT_GHOST_TRAIL:
      return "FX: GHOST TRAIL";
    case (uint8_t)ANIM_TEST_EFFECT_SHUTTER_WIPE:
      return "FX: SHUTTER WIPE";
    case (uint8_t)ANIM_TEST_EFFECT_BOUNCE:
      return "FX: BOUNCE";
    case (uint8_t)ANIM_TEST_EFFECT_DISSOLVE:
      return "FX: DISSOLVE";
    case (uint8_t)ANIM_TEST_EFFECT_DATAMOSH:
      return "FX: DATAMOSH";
    case (uint8_t)ANIM_TEST_EFFECT_RING_SHOCKWAVE:
      return "FX: RING SHOCK";
    case (uint8_t)ANIM_TEST_EFFECT_CHROMA_PHASE:
      return "FX: CHROMA PHASE";
    case (uint8_t)ANIM_TEST_EFFECT_WAVE_WARP:
      return "FX: WAVE WARP";
    case (uint8_t)ANIM_TEST_EFFECT_EDGE_BURN:
      return "FX: EDGE BURN";
    case (uint8_t)ANIM_TEST_EFFECT_TEAR_DROP:
      return "FX: TEAR DROP";
    case (uint8_t)ANIM_TEST_EFFECT_SNAP_GLITCH:
      return "FX: SNAP GLITCH";
    case (uint8_t)ANIM_TEST_EFFECT_TEMPORAL_DITHER:
      return "FX: TEMP DITHER";
    case (uint8_t)ANIM_TEST_EFFECT_VENETIAN_BLINDS:
      return "FX: VENETIAN";
    case (uint8_t)ANIM_TEST_EFFECT_SCANLINE_ROLL:
      return "FX: SCAN ROLL";
    case (uint8_t)ANIM_TEST_EFFECT_SHOCK_RING_INVERT:
      return "FX: RING INVERT";
    case (uint8_t)ANIM_TEST_EFFECT_MIRROR_SHEAR:
      return "FX: MIRROR SHEAR";
    default:
      break;
  }
  return "FX: NONE";
}

void RenderDemoTitleAnim_Reset(void)
{
  (void)memset(&s_title_anim, 0, sizeof(s_title_anim));
}

uint8_t RenderDemoTitleAnim_HandleControl(const game_runtime_input_t *input,
                                          uint8_t *request_exit_to_static,
                                          game_runtime_audio_cue_t *audio_cue_out)
{
  if (input == NULL)
  {
    return 0U;
  }

  RenderDemoTitleAnim_EnsureInitialized();

  if (request_exit_to_static != NULL)
  {
    *request_exit_to_static = 0U;
  }
  if (audio_cue_out != NULL)
  {
    *audio_cue_out = GAME_RT_AUDIO_CUE_NONE;
  }

  if (input->source == (ULONG)GAME_RT_INPUT_SRC_BTN_B)
  {
    if (request_exit_to_static != NULL)
    {
      *request_exit_to_static = 1U;
    }
    return 1U;
  }

  if (RenderDemoTitleAnim_InputIsAction(input->event) == 0U)
  {
    return 0U;
  }

  switch (input->source)
  {
    case (ULONG)GAME_RT_INPUT_SRC_BTN_L:
      RenderDemoTitleAnim_CycleSprite((input->event == (ULONG)GAME_RT_INPUT_EVENT_LONG) ?
                                      (int8_t)(-ANIM_TEST_SPRITE_FAST_SKIP) : (int8_t)-1);
      return 1U;

    case (ULONG)GAME_RT_INPUT_SRC_BTN_R:
      RenderDemoTitleAnim_CycleSprite((input->event == (ULONG)GAME_RT_INPUT_EVENT_LONG) ?
                                      (int8_t)ANIM_TEST_SPRITE_FAST_SKIP : (int8_t)1);
      return 1U;

    case (ULONG)GAME_RT_INPUT_SRC_JOY_LEFT:
      RenderDemoTitleAnim_CycleEffect(-1);
      return 1U;

    case (ULONG)GAME_RT_INPUT_SRC_JOY_RIGHT:
      RenderDemoTitleAnim_CycleEffect(1);
      return 1U;

    case (ULONG)GAME_RT_INPUT_SRC_JOY_UP:
      if (s_title_anim.scale < ANIM_TEST_SCALE_MAX)
      {
        s_title_anim.scale++;
      }
      return 1U;

    case (ULONG)GAME_RT_INPUT_SRC_JOY_DOWN:
      if (s_title_anim.scale > ANIM_TEST_SCALE_MIN)
      {
        s_title_anim.scale--;
      }
      return 1U;

    case (ULONG)GAME_RT_INPUT_SRC_BTN_A:
      if (input->event == (ULONG)GAME_RT_INPUT_EVENT_LONG)
      {
        RenderDemoTitleAnim_CycleBgMode();
        return 1U;
      }

      if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_NONE)
      {
        RenderDemoTitleAnim_StopEffect();
      }
      else
      {
        if (s_title_anim.effect_playing != 0U)
        {
          RenderDemoTitleAnim_StopEffect();
        }
        else
        {
          RenderDemoTitleAnim_StartEffect();
        }
      }
      return 1U;

    default:
      break;
  }

  return 0U;
}

void RenderDemoTitleAnim_Update(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms)
{
  (void)sensor_snapshot;
  RenderDemoTitleAnim_EnsureInitialized();

  if (s_title_anim.effect_playing == 0U)
  {
    return;
  }

  s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum + (uint16_t)dt_ms);

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_RAPID_INVERT)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_RAPID_INVERT_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_RAPID_INVERT_MS);
      s_title_anim.invert_phase = (s_title_anim.invert_phase == 0U) ? 1U : 0U;
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHAKE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_SHAKE_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_SHAKE_STEP_MS);
      s_title_anim.shake_phase = (uint8_t)((s_title_anim.shake_phase + 1U) % 5U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_PULSE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_PULSE_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_PULSE_STEP_MS);
      s_title_anim.pulse_phase = (uint8_t)((s_title_anim.pulse_phase + 1U) & 0x3U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SLIDE_IN)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_SLIDE_STEP_MS)
    {
      int16_t next_offset;
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_SLIDE_STEP_MS);

      if (s_title_anim.slide_offset_x >= 0)
      {
        s_title_anim.slide_offset_x = 0;
        s_title_anim.effect_playing = 0U;
        break;
      }

      next_offset = (int16_t)(s_title_anim.slide_offset_x + ANIM_TEST_SLIDE_STEP_PX);
      if (next_offset >= 0)
      {
        s_title_anim.slide_offset_x = 0;
        s_title_anim.effect_playing = 0U;
        break;
      }
      s_title_anim.slide_offset_x = next_offset;
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_NOISE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_NOISE_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_NOISE_STEP_MS);
      s_title_anim.noise_frame++;
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_GLITCH_SLICE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_GLITCH_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_GLITCH_STEP_MS);
      s_title_anim.glitch_phase = (uint8_t)((s_title_anim.glitch_phase + 1U) & 0x7U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_GHOST_TRAIL)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_GHOST_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_GHOST_STEP_MS);
      s_title_anim.ghost_phase = (uint8_t)((s_title_anim.ghost_phase + 1U) & 0x7U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHUTTER_WIPE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_SHUTTER_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_SHUTTER_STEP_MS);
      s_title_anim.shutter_phase++;
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_BOUNCE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_BOUNCE_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_BOUNCE_STEP_MS);
      s_title_anim.bounce_phase = (uint8_t)((s_title_anim.bounce_phase + 1U) & 0x7U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_DISSOLVE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_DISSOLVE_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_DISSOLVE_STEP_MS);
      if (s_title_anim.dissolve_dir != 0U)
      {
        if (s_title_anim.dissolve_phase < ANIM_TEST_DISSOLVE_MAX_PHASE)
        {
          s_title_anim.dissolve_phase++;
        }
        else
        {
          s_title_anim.dissolve_dir = 0U;
        }
      }
      else
      {
        if (s_title_anim.dissolve_phase > 0U)
        {
          s_title_anim.dissolve_phase--;
        }
        else
        {
          s_title_anim.dissolve_dir = 1U;
        }
      }
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_DATAMOSH)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_DATAMOSH_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_DATAMOSH_STEP_MS);
      s_title_anim.datamosh_phase = (uint8_t)((s_title_anim.datamosh_phase + 1U) & 0x0FU);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_RING_SHOCKWAVE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_RING_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_RING_STEP_MS);
      s_title_anim.ring_radius = (uint16_t)(s_title_anim.ring_radius + 2U);
      if (s_title_anim.ring_radius > 40U)
      {
        s_title_anim.ring_radius = 0U;
      }
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_CHROMA_PHASE)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_CHROMA_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_CHROMA_STEP_MS);
      s_title_anim.chroma_phase = (uint8_t)((s_title_anim.chroma_phase + 1U) & 0x7U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_WAVE_WARP)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_WAVE_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_WAVE_STEP_MS);
      s_title_anim.wave_phase = (uint8_t)((s_title_anim.wave_phase + 1U) & 0x7U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_EDGE_BURN)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_EDGE_BURN_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_EDGE_BURN_STEP_MS);
      s_title_anim.edge_phase = (uint8_t)((s_title_anim.edge_phase + 1U) % 20U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_TEAR_DROP)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_TEAR_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_TEAR_STEP_MS);
      s_title_anim.tear_phase = (uint8_t)((s_title_anim.tear_phase + 1U) & 0x0FU);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SNAP_GLITCH)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_SNAP_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_SNAP_STEP_MS);
      s_title_anim.snap_phase = (uint8_t)((s_title_anim.snap_phase + 1U) % 12U);
      if (s_title_anim.snap_phase < 3U)
      {
        s_title_anim.noise_frame++;
      }
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_TEMPORAL_DITHER)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_TEMP_DITHER_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_TEMP_DITHER_STEP_MS);
      s_title_anim.temporal_phase = (uint8_t)((s_title_anim.temporal_phase + 1U) & 0x1FU);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_VENETIAN_BLINDS)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_BLINDS_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_BLINDS_STEP_MS);
      s_title_anim.blinds_phase = (uint8_t)((s_title_anim.blinds_phase + 1U) % 13U);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SCANLINE_ROLL)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_SCANROLL_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_SCANROLL_STEP_MS);
      s_title_anim.scan_phase = (uint8_t)((s_title_anim.scan_phase + 1U) % (uint8_t)UI_TEST_MIMIC_H);
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHOCK_RING_INVERT)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_RING_INVERT_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_RING_INVERT_STEP_MS);
      s_title_anim.ring_invert_radius = (uint16_t)(s_title_anim.ring_invert_radius + 2U);
      if (s_title_anim.ring_invert_radius > 40U)
      {
        s_title_anim.ring_invert_radius = 0U;
      }
    }
    return;
  }

  if (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_MIRROR_SHEAR)
  {
    while (s_title_anim.effect_ms_accum >= ANIM_TEST_SHEAR_STEP_MS)
    {
      s_title_anim.effect_ms_accum = (uint16_t)(s_title_anim.effect_ms_accum - ANIM_TEST_SHEAR_STEP_MS);
      s_title_anim.shear_phase = (uint8_t)((s_title_anim.shear_phase + 1U) & 0x7U);
    }
    return;
  }
}

void RenderDemoTitleAnim_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot)
{
  uint8_t draw_base_scale;
  uint16_t draw_effective_scale;
  uint16_t draw_x = 0U;
  uint16_t draw_y = 0U;
  int32_t draw_x_i32;
  int32_t draw_y_i32;
  int8_t shake_dx = 0;
  int8_t shake_dy = 0;
  int8_t bounce_dy = 0;
  uint8_t glitch_active = 0U;
  uint8_t ghost_active = 0U;
  uint8_t shutter_active = 0U;
  uint8_t dissolve_active = 0U;
  uint8_t datamosh_active = 0U;
  uint8_t ring_active = 0U;
  uint8_t chroma_active = 0U;
  uint8_t wave_active = 0U;
  uint8_t edge_active = 0U;
  uint8_t tear_active = 0U;
  uint8_t snap_active = 0U;
  uint8_t snap_corrupt = 0U;
  uint8_t temporal_active = 0U;
  uint8_t blinds_active = 0U;
  uint8_t scanroll_active = 0U;
  uint8_t ring_invert_active = 0U;
  uint8_t shear_active = 0U;
  uint8_t invert_active;
  const ui_test_mimic_sprite_entry_t *sprite;

  (void)sensor_snapshot;

  RenderDemoTitleAnim_EnsureInitialized();
  sprite = RenderDemoTitleAnim_CurrentSprite();

  draw_base_scale = RenderDemoTitleAnim_DrawBaseScale();
  draw_effective_scale = RenderDemoTitleAnim_EffectiveScale(draw_base_scale);
  invert_active = RenderDemoTitleAnim_InvertActive();

  RenderDemoTitleAnim_ComputeCenteredXY(draw_effective_scale, &draw_x, &draw_y);

  if ((s_title_anim.effect_playing != 0U) &&
      (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHAKE))
  {
    static const int8_t k_shake_offsets[5][2] = {
      {0, 0},
      {1, 0},
      {-1, 0},
      {0, 1},
      {0, -1}
    };
    shake_dx = k_shake_offsets[s_title_anim.shake_phase % 5U][0];
    shake_dy = k_shake_offsets[s_title_anim.shake_phase % 5U][1];
  }

  if ((s_title_anim.effect_playing != 0U) &&
      (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_BOUNCE))
  {
    static const int8_t k_bounce_offsets[8] = {0, -3, -6, -3, 0, 2, 0, -1};
    bounce_dy = k_bounce_offsets[s_title_anim.bounce_phase & 0x7U];
  }

  glitch_active = ((s_title_anim.effect_playing != 0U) &&
                   (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_GLITCH_SLICE)) ? 1U : 0U;
  ghost_active = ((s_title_anim.effect_playing != 0U) &&
                  (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_GHOST_TRAIL)) ? 1U : 0U;
  shutter_active = ((s_title_anim.effect_playing != 0U) &&
                    (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHUTTER_WIPE)) ? 1U : 0U;
  dissolve_active = ((s_title_anim.effect_playing != 0U) &&
                     (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_DISSOLVE)) ? 1U : 0U;
  datamosh_active = ((s_title_anim.effect_playing != 0U) &&
                     (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_DATAMOSH)) ? 1U : 0U;
  ring_active = ((s_title_anim.effect_playing != 0U) &&
                 (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_RING_SHOCKWAVE)) ? 1U : 0U;
  chroma_active = ((s_title_anim.effect_playing != 0U) &&
                   (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_CHROMA_PHASE)) ? 1U : 0U;
  wave_active = ((s_title_anim.effect_playing != 0U) &&
                 (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_WAVE_WARP)) ? 1U : 0U;
  edge_active = ((s_title_anim.effect_playing != 0U) &&
                 (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_EDGE_BURN)) ? 1U : 0U;
  tear_active = ((s_title_anim.effect_playing != 0U) &&
                 (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_TEAR_DROP)) ? 1U : 0U;
  snap_active = ((s_title_anim.effect_playing != 0U) &&
                 (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SNAP_GLITCH)) ? 1U : 0U;
  snap_corrupt = ((snap_active != 0U) && (s_title_anim.snap_phase < 3U)) ? 1U : 0U;
  temporal_active = ((s_title_anim.effect_playing != 0U) &&
                     (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_TEMPORAL_DITHER)) ? 1U : 0U;
  blinds_active = ((s_title_anim.effect_playing != 0U) &&
                   (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_VENETIAN_BLINDS)) ? 1U : 0U;
  scanroll_active = ((s_title_anim.effect_playing != 0U) &&
                     (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SCANLINE_ROLL)) ? 1U : 0U;
  ring_invert_active = ((s_title_anim.effect_playing != 0U) &&
                        (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SHOCK_RING_INVERT)) ? 1U : 0U;
  shear_active = ((s_title_anim.effect_playing != 0U) &&
                  (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_MIRROR_SHEAR)) ? 1U : 0U;

  draw_x_i32 = (int32_t)draw_x + (int32_t)shake_dx;
  draw_y_i32 = (int32_t)draw_y + (int32_t)shake_dy + (int32_t)bounce_dy;

  if ((s_title_anim.effect_playing != 0U) &&
      (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_SLIDE_IN))
  {
    draw_x_i32 += (int32_t)s_title_anim.slide_offset_x;
  }

  if (draw_x_i32 < 0)
  {
    draw_x_i32 = 0;
  }
  if (draw_y_i32 < 0)
  {
    draw_y_i32 = 0;
  }
  if (draw_x_i32 > 65535)
  {
    draw_x_i32 = 65535;
  }
  if (draw_y_i32 > 65535)
  {
    draw_y_i32 = 65535;
  }

  RenderDemoTitleAnim_DrawBackground();
  Render_SetModeIndicator(TH_MODE_REALTIME);

  if (RenderDemoTitleAnim_CurrentSpriteIs2bpp() != 0U)
  {
    const uint8_t *color_plane = (invert_active != 0U)
                                     ? sprite->color_2bpp_inv
                                     : sprite->color_2bpp;
    const uint8_t *mask_plane = sprite->mask_2bpp;

    if ((s_title_anim.effect_playing != 0U) &&
        (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_NOISE))
    {
      RenderDemoTitleAnim_BuildNoise2bpp(color_plane, sprite->mask_2bpp);
      color_plane = s_anim_noise_2bpp;
    }

    if (chroma_active != 0U)
    {
      RenderDemoTitleAnim_BuildChroma2bpp(color_plane, s_title_anim.chroma_phase);
      color_plane = s_anim_noise_2bpp;
    }

    if (edge_active != 0U)
    {
      RenderDemoTitleAnim_BuildEdgeBurnMask(sprite->mask_2bpp,
                                            (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                            s_title_anim.edge_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (shutter_active != 0U)
    {
      uint16_t visible_rows = RenderDemoTitleAnim_ShutterRowsVisible();
      RenderDemoTitleAnim_BuildRowsMask(sprite->mask_2bpp,
                                        (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                        0U,
                                        visible_rows);
      mask_plane = s_anim_mask_temp;
    }
    else if (dissolve_active != 0U)
    {
      RenderDemoTitleAnim_BuildDissolveMask(sprite->mask_2bpp,
                                            (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                            s_title_anim.dissolve_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (ring_active != 0U)
    {
      RenderDemoTitleAnim_BuildRingMask(sprite->mask_2bpp,
                                        (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                        s_title_anim.ring_radius,
                                        ANIM_TEST_RING_THICKNESS);
      mask_plane = s_anim_mask_temp;
    }
    else if (temporal_active != 0U)
    {
      RenderDemoTitleAnim_BuildTemporalDitherMask(sprite->mask_2bpp,
                                                  (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                                  s_title_anim.temporal_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (blinds_active != 0U)
    {
      RenderDemoTitleAnim_BuildVenetianMask(sprite->mask_2bpp,
                                            (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                            s_title_anim.blinds_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (scanroll_active != 0U)
    {
      RenderDemoTitleAnim_BuildScanlineRollMask(sprite->mask_2bpp,
                                                (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                                s_title_anim.scan_phase);
      mask_plane = s_anim_mask_temp;
    }

    if ((snap_active != 0U) && (snap_corrupt != 0U))
    {
      static const int8_t k_snap_offsets[8] = {0, 8, -7, 5, -4, 7, -6, 3};
      const uint8_t *snap_plane = color_plane;
      uint16_t band_h = 6U;
      uint16_t band;

      RenderDemoTitleAnim_BuildNoise2bpp(color_plane, sprite->mask_2bpp);
      snap_plane = s_anim_noise_2bpp;
      RenderDemoTitleAnim_BuildChroma2bpp(snap_plane,
                                          (uint8_t)(s_title_anim.snap_phase + (uint8_t)(s_title_anim.noise_frame & 0x3U)));
      snap_plane = s_anim_noise_2bpp;

      for (band = 0U; band < (uint16_t)ANIM_TEST_GLITCH_BANDS; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == ((uint16_t)ANIM_TEST_GLITCH_BANDS - 1U))
                               ? (uint16_t)UI_TEST_MIMIC_H
                               : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_snap_offsets[(s_title_anim.snap_phase + (uint8_t)(band * 2U)) & 0x7U];
        int32_t band_y = draw_y_i32 + ((((s_title_anim.snap_phase + band) & 0x1U) != 0U) ? 2 : -2);

        if (band_x < 0)
        {
          band_x = 0;
        }
        if (band_y < 0)
        {
          band_y = 0;
        }

        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked2bppScaled((uint16_t)band_x,
                                   (uint16_t)band_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   snap_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   RENDER_2BPP_PRESENT_BAYER2X2,
                                   draw_base_scale);
      }
    }
    else if (tear_active != 0U)
    {
      static const int8_t k_tear_offsets[16] = {0, 1, 3, 2, 0, -1, -3, -2, 0, 1, 2, 0, -2, -1, 0, 2};
      uint16_t col_start;
      uint8_t slice_idx = 0U;
      const uint16_t slice_w = 6U;

      for (col_start = 0U; col_start < (uint16_t)UI_TEST_MIMIC_W; col_start = (uint16_t)(col_start + slice_w))
      {
        uint16_t col_end = (uint16_t)(col_start + slice_w);
        int32_t slice_y = draw_y_i32 + (int32_t)k_tear_offsets[(s_title_anim.tear_phase + (uint8_t)(slice_idx * 3U)) & 0x0FU];

        if (col_end > (uint16_t)UI_TEST_MIMIC_W)
        {
          col_end = (uint16_t)UI_TEST_MIMIC_W;
        }
        if (slice_y < 0)
        {
          slice_y = 0;
        }

        RenderDemoTitleAnim_BuildColumnsMask(mask_plane,
                                             (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                             col_start,
                                             col_end);
        renderBlitMasked2bppScaled((uint16_t)draw_x_i32,
                                   (uint16_t)slice_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   color_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   RENDER_2BPP_PRESENT_BAYER2X2,
                                   draw_base_scale);
        slice_idx++;
      }
    }
    else if (wave_active != 0U)
    {
      static const int8_t k_wave_offsets[8] = {0, 2, 4, 2, 0, -2, -4, -2};
      const uint16_t band_h = 8U;
      uint16_t band;

      for (band = 0U; band < 6U; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == 5U) ? (uint16_t)UI_TEST_MIMIC_H : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_wave_offsets[(s_title_anim.wave_phase + (uint8_t)band) & 0x7U];

        if (band_x < 0)
        {
          band_x = 0;
        }

        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked2bppScaled((uint16_t)band_x,
                                   (uint16_t)draw_y_i32,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   color_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   RENDER_2BPP_PRESENT_BAYER2X2,
                                   draw_base_scale);
      }
    }
    else if (ring_invert_active != 0U)
    {
      const uint8_t *inv_plane = (color_plane == sprite->color_2bpp_inv)
                                     ? sprite->color_2bpp
                                     : sprite->color_2bpp_inv;
      renderBlitMasked2bppScaled((uint16_t)draw_x_i32,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 color_plane,
                                 mask_plane,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 RENDER_2BPP_PRESENT_BAYER2X2,
                                 draw_base_scale);

      RenderDemoTitleAnim_BuildRingMask(mask_plane,
                                        (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                        s_title_anim.ring_invert_radius,
                                        ANIM_TEST_RING_THICKNESS);
      renderBlitMasked2bppScaled((uint16_t)draw_x_i32,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 inv_plane,
                                 s_anim_mask_temp,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 RENDER_2BPP_PRESENT_BAYER2X2,
                                 draw_base_scale);
    }
    else if (shear_active != 0U)
    {
      static const int8_t k_shear_offsets[8] = {0, 2, 4, 3, 0, -3, -4, -2};
      int32_t shear_x = (int32_t)k_shear_offsets[s_title_anim.shear_phase & 0x7U];
      int32_t top_x = draw_x_i32 + shear_x;
      int32_t bot_x = draw_x_i32 - shear_x;
      uint16_t half_h = (uint16_t)(UI_TEST_MIMIC_H / 2U);

      if (top_x < 0)
      {
        top_x = 0;
      }
      if (bot_x < 0)
      {
        bot_x = 0;
      }

      RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                        (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                        0U,
                                        half_h);
      renderBlitMasked2bppScaled((uint16_t)top_x,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 color_plane,
                                 s_anim_mask_temp,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 RENDER_2BPP_PRESENT_BAYER2X2,
                                 draw_base_scale);

      RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                        (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                        half_h,
                                        (uint16_t)UI_TEST_MIMIC_H);
      renderBlitMasked2bppScaled((uint16_t)bot_x,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 color_plane,
                                 s_anim_mask_temp,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 RENDER_2BPP_PRESENT_BAYER2X2,
                                 draw_base_scale);
    }
    else if (datamosh_active != 0U)
    {
      static const int8_t k_mosh_offsets[16] = {0, 6, -5, 3, -2, 7, -7, 2, -1, 5, -4, 4, -6, 1, -3, 0};
      uint16_t band_h = 6U;
      uint16_t band;

      for (band = 0U; band < (uint16_t)ANIM_TEST_GLITCH_BANDS; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == ((uint16_t)ANIM_TEST_GLITCH_BANDS - 1U))
                               ? (uint16_t)UI_TEST_MIMIC_H
                               : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_mosh_offsets[(s_title_anim.datamosh_phase + (uint8_t)(band * 3U)) & 0x0FU];
        int32_t band_y = draw_y_i32 + ((((s_title_anim.datamosh_phase + band) & 0x1U) != 0U) ? 1 : 0);

        if (band_x < 0)
        {
          band_x = 0;
        }
        if (band_y < 0)
        {
          band_y = 0;
        }

        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked2bppScaled((uint16_t)band_x,
                                   (uint16_t)band_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   color_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   RENDER_2BPP_PRESENT_BAYER2X2,
                                   draw_base_scale);
      }
    }
    else if (glitch_active != 0U)
    {
      static const int8_t k_glitch_offsets[8] = {0, 2, -2, 1, -1, 3, -3, 0};
      uint16_t band_h = (uint16_t)(UI_TEST_MIMIC_H / ANIM_TEST_GLITCH_BANDS);
      uint16_t band;

      if (band_h == 0U)
      {
        band_h = 1U;
      }

      for (band = 0U; band < (uint16_t)ANIM_TEST_GLITCH_BANDS; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == ((uint16_t)ANIM_TEST_GLITCH_BANDS - 1U))
                               ? (uint16_t)UI_TEST_MIMIC_H
                               : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_glitch_offsets[(s_title_anim.glitch_phase + (uint8_t)(band * 2U)) & 0x7U];

        if (band_x < 0)
        {
          band_x = 0;
        }
        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked2bppScaled((uint16_t)band_x,
                                   (uint16_t)draw_y_i32,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   color_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   RENDER_2BPP_PRESENT_BAYER2X2,
                                   draw_base_scale);
      }
    }
    else
    {
      if (ghost_active != 0U)
      {
        int32_t ghost_x = draw_x_i32 - (int32_t)(1U + (s_title_anim.ghost_phase & 0x3U));
        int32_t ghost_y = draw_y_i32 + 1;

        if (ghost_x < 0)
        {
          ghost_x = 0;
        }
        if (ghost_y < 0)
        {
          ghost_y = 0;
        }

        RenderDemoTitleAnim_BuildCheckerMask(mask_plane,
                                             (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                             s_title_anim.ghost_phase,
                                             0U);
        renderBlitMasked2bppScaled((uint16_t)ghost_x,
                                   (uint16_t)ghost_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   color_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   RENDER_2BPP_PRESENT_BAYER2X2,
                                   draw_base_scale);

        RenderDemoTitleAnim_BuildCheckerMask(mask_plane,
                                             (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                             (uint8_t)(s_title_anim.ghost_phase + 1U),
                                             1U);
        renderBlitMasked2bppScaled((uint16_t)((ghost_x > 1) ? (ghost_x - 1) : 0),
                                   (uint16_t)(ghost_y + 1),
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   color_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                   (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   RENDER_2BPP_PRESENT_BAYER2X2,
                                   draw_base_scale);
      }

      renderBlitMasked2bppScaled((uint16_t)draw_x_i32,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 color_plane,
                                 mask_plane,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_COLOR_STRIDE,
                                 (uint16_t)UI_TEST_MIMIC_2BPP_MASK_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 RENDER_2BPP_PRESENT_BAYER2X2,
                                 draw_base_scale);
    }
  }
  else
  {
    const uint8_t *on_plane = (invert_active != 0U)
                                   ? sprite->on_1bpp_inv
                                   : sprite->on_1bpp;
    const uint8_t *mask_plane = sprite->mask_1bpp;

    if ((s_title_anim.effect_playing != 0U) &&
        (s_title_anim.effect_idx == (uint8_t)ANIM_TEST_EFFECT_NOISE))
    {
      RenderDemoTitleAnim_BuildNoise1bpp(on_plane, sprite->mask_1bpp);
      on_plane = s_anim_noise_1bpp;
    }

    if (edge_active != 0U)
    {
      RenderDemoTitleAnim_BuildEdgeBurnMask(sprite->mask_1bpp,
                                            (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                            s_title_anim.edge_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (shutter_active != 0U)
    {
      uint16_t visible_rows = RenderDemoTitleAnim_ShutterRowsVisible();
      RenderDemoTitleAnim_BuildRowsMask(sprite->mask_1bpp,
                                        (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                        0U,
                                        visible_rows);
      mask_plane = s_anim_mask_temp;
    }
    else if (dissolve_active != 0U)
    {
      RenderDemoTitleAnim_BuildDissolveMask(sprite->mask_1bpp,
                                            (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                            s_title_anim.dissolve_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (ring_active != 0U)
    {
      RenderDemoTitleAnim_BuildRingMask(sprite->mask_1bpp,
                                        (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                        s_title_anim.ring_radius,
                                        ANIM_TEST_RING_THICKNESS);
      mask_plane = s_anim_mask_temp;
    }
    else if (temporal_active != 0U)
    {
      RenderDemoTitleAnim_BuildTemporalDitherMask(sprite->mask_1bpp,
                                                  (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                                  s_title_anim.temporal_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (blinds_active != 0U)
    {
      RenderDemoTitleAnim_BuildVenetianMask(sprite->mask_1bpp,
                                            (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                            s_title_anim.blinds_phase);
      mask_plane = s_anim_mask_temp;
    }
    else if (scanroll_active != 0U)
    {
      RenderDemoTitleAnim_BuildScanlineRollMask(sprite->mask_1bpp,
                                                (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                                s_title_anim.scan_phase);
      mask_plane = s_anim_mask_temp;
    }

    if ((snap_active != 0U) && (snap_corrupt != 0U))
    {
      static const int8_t k_snap_offsets[8] = {0, 8, -7, 5, -4, 7, -6, 3};
      const uint8_t *snap_plane = on_plane;
      uint16_t band_h = 6U;
      uint16_t band;

      RenderDemoTitleAnim_BuildNoise1bpp(on_plane, sprite->mask_1bpp);
      snap_plane = s_anim_noise_1bpp;

      for (band = 0U; band < (uint16_t)ANIM_TEST_GLITCH_BANDS; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == ((uint16_t)ANIM_TEST_GLITCH_BANDS - 1U))
                               ? (uint16_t)UI_TEST_MIMIC_H
                               : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_snap_offsets[(s_title_anim.snap_phase + (uint8_t)(band * 2U)) & 0x7U];
        int32_t band_y = draw_y_i32 + ((((s_title_anim.snap_phase + band) & 0x1U) != 0U) ? 2 : -2);

        if (band_x < 0)
        {
          band_x = 0;
        }
        if (band_y < 0)
        {
          band_y = 0;
        }

        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked1bppScaled((uint16_t)band_x,
                                   (uint16_t)band_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   snap_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   draw_base_scale);
      }
    }
    else if (tear_active != 0U)
    {
      static const int8_t k_tear_offsets[16] = {0, 1, 3, 2, 0, -1, -3, -2, 0, 1, 2, 0, -2, -1, 0, 2};
      uint16_t col_start;
      uint8_t slice_idx = 0U;
      const uint16_t slice_w = 6U;

      for (col_start = 0U; col_start < (uint16_t)UI_TEST_MIMIC_W; col_start = (uint16_t)(col_start + slice_w))
      {
        uint16_t col_end = (uint16_t)(col_start + slice_w);
        int32_t slice_y = draw_y_i32 + (int32_t)k_tear_offsets[(s_title_anim.tear_phase + (uint8_t)(slice_idx * 3U)) & 0x0FU];

        if (col_end > (uint16_t)UI_TEST_MIMIC_W)
        {
          col_end = (uint16_t)UI_TEST_MIMIC_W;
        }
        if (slice_y < 0)
        {
          slice_y = 0;
        }

        RenderDemoTitleAnim_BuildColumnsMask(mask_plane,
                                             (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                             col_start,
                                             col_end);
        renderBlitMasked1bppScaled((uint16_t)draw_x_i32,
                                   (uint16_t)slice_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   on_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   draw_base_scale);
        slice_idx++;
      }
    }
    else if (wave_active != 0U)
    {
      static const int8_t k_wave_offsets[8] = {0, 2, 4, 2, 0, -2, -4, -2};
      const uint16_t band_h = 8U;
      uint16_t band;

      for (band = 0U; band < 6U; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == 5U) ? (uint16_t)UI_TEST_MIMIC_H : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_wave_offsets[(s_title_anim.wave_phase + (uint8_t)band) & 0x7U];

        if (band_x < 0)
        {
          band_x = 0;
        }

        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked1bppScaled((uint16_t)band_x,
                                   (uint16_t)draw_y_i32,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   on_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   draw_base_scale);
      }
    }
    else if (ring_invert_active != 0U)
    {
      const uint8_t *inv_plane = (on_plane == sprite->on_1bpp_inv)
                                     ? sprite->on_1bpp
                                     : sprite->on_1bpp_inv;
      renderBlitMasked1bppScaled((uint16_t)draw_x_i32,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 on_plane,
                                 mask_plane,
                                 (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 draw_base_scale);

      RenderDemoTitleAnim_BuildRingMask(mask_plane,
                                        (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                        s_title_anim.ring_invert_radius,
                                        ANIM_TEST_RING_THICKNESS);
      renderBlitMasked1bppScaled((uint16_t)draw_x_i32,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 inv_plane,
                                 s_anim_mask_temp,
                                 (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 draw_base_scale);
    }
    else if (shear_active != 0U)
    {
      static const int8_t k_shear_offsets[8] = {0, 2, 4, 3, 0, -3, -4, -2};
      int32_t shear_x = (int32_t)k_shear_offsets[s_title_anim.shear_phase & 0x7U];
      int32_t top_x = draw_x_i32 + shear_x;
      int32_t bot_x = draw_x_i32 - shear_x;
      uint16_t half_h = (uint16_t)(UI_TEST_MIMIC_H / 2U);

      if (top_x < 0)
      {
        top_x = 0;
      }
      if (bot_x < 0)
      {
        bot_x = 0;
      }

      RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                        (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                        0U,
                                        half_h);
      renderBlitMasked1bppScaled((uint16_t)top_x,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 on_plane,
                                 s_anim_mask_temp,
                                 (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 draw_base_scale);

      RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                        (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                        half_h,
                                        (uint16_t)UI_TEST_MIMIC_H);
      renderBlitMasked1bppScaled((uint16_t)bot_x,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 on_plane,
                                 s_anim_mask_temp,
                                 (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 draw_base_scale);
    }
    else if (datamosh_active != 0U)
    {
      static const int8_t k_mosh_offsets[16] = {0, 6, -5, 3, -2, 7, -7, 2, -1, 5, -4, 4, -6, 1, -3, 0};
      uint16_t band_h = 6U;
      uint16_t band;

      for (band = 0U; band < (uint16_t)ANIM_TEST_GLITCH_BANDS; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == ((uint16_t)ANIM_TEST_GLITCH_BANDS - 1U))
                               ? (uint16_t)UI_TEST_MIMIC_H
                               : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_mosh_offsets[(s_title_anim.datamosh_phase + (uint8_t)(band * 3U)) & 0x0FU];
        int32_t band_y = draw_y_i32 + ((((s_title_anim.datamosh_phase + band) & 0x1U) != 0U) ? 1 : 0);

        if (band_x < 0)
        {
          band_x = 0;
        }
        if (band_y < 0)
        {
          band_y = 0;
        }

        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked1bppScaled((uint16_t)band_x,
                                   (uint16_t)band_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   on_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   draw_base_scale);
      }
    }
    else if (glitch_active != 0U)
    {
      static const int8_t k_glitch_offsets[8] = {0, 2, -2, 1, -1, 3, -3, 0};
      uint16_t band_h = (uint16_t)(UI_TEST_MIMIC_H / ANIM_TEST_GLITCH_BANDS);
      uint16_t band;

      if (band_h == 0U)
      {
        band_h = 1U;
      }

      for (band = 0U; band < (uint16_t)ANIM_TEST_GLITCH_BANDS; band++)
      {
        uint16_t row_start = (uint16_t)(band * band_h);
        uint16_t row_end = (band == ((uint16_t)ANIM_TEST_GLITCH_BANDS - 1U))
                               ? (uint16_t)UI_TEST_MIMIC_H
                               : (uint16_t)(row_start + band_h);
        int32_t band_x = draw_x_i32 + (int32_t)k_glitch_offsets[(s_title_anim.glitch_phase + (uint8_t)(band * 2U)) & 0x7U];

        if (band_x < 0)
        {
          band_x = 0;
        }
        RenderDemoTitleAnim_BuildRowsMask(mask_plane,
                                          (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                          row_start,
                                          row_end);
        renderBlitMasked1bppScaled((uint16_t)band_x,
                                   (uint16_t)draw_y_i32,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   on_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   draw_base_scale);
      }
    }
    else
    {
      if (ghost_active != 0U)
      {
        int32_t ghost_x = draw_x_i32 - (int32_t)(1U + (s_title_anim.ghost_phase & 0x3U));
        int32_t ghost_y = draw_y_i32 + 1;

        if (ghost_x < 0)
        {
          ghost_x = 0;
        }
        if (ghost_y < 0)
        {
          ghost_y = 0;
        }

        RenderDemoTitleAnim_BuildCheckerMask(mask_plane,
                                             (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                             s_title_anim.ghost_phase,
                                             0U);
        renderBlitMasked1bppScaled((uint16_t)ghost_x,
                                   (uint16_t)ghost_y,
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   on_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   draw_base_scale);

        RenderDemoTitleAnim_BuildCheckerMask(mask_plane,
                                             (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                             (uint8_t)(s_title_anim.ghost_phase + 1U),
                                             1U);
        renderBlitMasked1bppScaled((uint16_t)((ghost_x > 1) ? (ghost_x - 1) : 0),
                                   (uint16_t)(ghost_y + 1),
                                   (uint16_t)UI_TEST_MIMIC_W,
                                   (uint16_t)UI_TEST_MIMIC_H,
                                   on_plane,
                                   s_anim_mask_temp,
                                   (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                   true,
                                   RENDER_LAYER_GAME,
                                   draw_base_scale);
      }

      renderBlitMasked1bppScaled((uint16_t)draw_x_i32,
                                 (uint16_t)draw_y_i32,
                                 (uint16_t)UI_TEST_MIMIC_W,
                                 (uint16_t)UI_TEST_MIMIC_H,
                                 on_plane,
                                 mask_plane,
                                 (uint16_t)UI_TEST_MIMIC_1BPP_STRIDE,
                                 true,
                                 RENDER_LAYER_GAME,
                                 draw_base_scale);
    }
  }

  renderDrawText(2U, 2U, RenderDemoTitleAnim_EffectLabel(), RENDER_LAYER_UI, RENDER_COLOR_WHITE);
  Render_MarkDirtyAll();
}
