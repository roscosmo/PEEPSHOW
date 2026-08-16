#include "display_renderer.h"

#include <string.h>

#include "ps_ui_router.h"

#define DISPLAY_RENDERER_TEXT_SCALE (2U)

static uint8_t s_display_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static uint32_t s_rotate_ccw;

static void DisplayRenderer_FillStats(display_renderer_stats_t *stats,
                                      uint32_t black_pixels)
{
  if (stats == NULL)
  {
    return;
  }

  stats->width = DISPLAY_WIDTH;
  stats->height = DISPLAY_HEIGHT;
  stats->framebuffer_hash = DisplayRenderer_FramebufferHash();
  stats->black_pixels = black_pixels;
}

void DisplayRenderer_ClearWhite(void)
{
  (void)memset(s_display_framebuffer, 0xFF,
               sizeof(s_display_framebuffer));
}

const uint8_t *DisplayRenderer_GetBuffer(void)
{
  return s_display_framebuffer;
}

static uint32_t DisplayRenderer_SetBlack(uint16_t x, uint16_t y)
{
  uint16_t panel_x = x;
  uint16_t panel_y = y;
  uint32_t index;
  uint8_t mask;

  if (s_rotate_ccw != 0UL)
  {
    if ((x >= DISPLAY_RENDERER_WIDTH) || (y >= DISPLAY_RENDERER_HEIGHT))
    {
      return 0UL;
    }
    panel_x = y;
    panel_y = (uint16_t)(DISPLAY_RENDERER_WIDTH - 1U - x);
  }

  if ((panel_x >= DISPLAY_WIDTH) || (panel_y >= DISPLAY_HEIGHT))
  {
    return 0UL;
  }

  index = ((uint32_t)panel_y * LINE_WIDTH) + ((uint32_t)panel_x >> 3U);
  mask = (uint8_t)(1U << (panel_x & 7U));
  if ((s_display_framebuffer[index] & mask) == 0U)
  {
    return 0UL;
  }

  s_display_framebuffer[index] &= (uint8_t)~mask;
  return 1UL;
}

static uint32_t DisplayRenderer_HorizontalLine(uint16_t x0,
                                               uint16_t x1,
                                               uint16_t y)
{
  uint32_t count = 0UL;
  uint16_t x;

  for (x = x0; x <= x1; ++x)
  {
    count += DisplayRenderer_SetBlack(x, y);
  }
  return count;
}

static uint32_t DisplayRenderer_VerticalLine(uint16_t x,
                                             uint16_t y0,
                                             uint16_t y1)
{
  uint32_t count = 0UL;
  uint16_t y;

  for (y = y0; y <= y1; ++y)
  {
    count += DisplayRenderer_SetBlack(x, y);
  }
  return count;
}

static uint32_t DisplayRenderer_FilledRect(uint16_t x0,
                                           uint16_t y0,
                                           uint16_t width,
                                           uint16_t height)
{
  uint32_t count = 0UL;
  uint16_t y;

  for (y = y0; y < (uint16_t)(y0 + height); ++y)
  {
    count += DisplayRenderer_HorizontalLine(
      x0, (uint16_t)(x0 + width - 1U), y);
  }
  return count;
}

uint32_t DisplayRenderer_FramebufferHash(void)
{
  uint32_t hash = 2166136261UL;
  uint16_t i;

  for (i = 0U; i < DISPLAY_RENDERER_BUFFER_SIZE; ++i)
  {
    hash ^= s_display_framebuffer[i];
    hash *= 16777619UL;
  }
  return hash;
}

static uint32_t DisplayRenderer_GlyphRows(char glyph, uint8_t rows[7])
{
  static const uint8_t blank[7] =
  {
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
  };
  static const uint8_t glyphs[36][7] =
  {
    {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U},
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU},
    {0x0EU, 0x11U, 0x10U, 0x10U, 0x10U, 0x11U, 0x0EU},
    {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU},
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU},
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U},
    {0x0EU, 0x11U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0FU},
    {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U},
    {0x0EU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU},
    {0x01U, 0x01U, 0x01U, 0x01U, 0x11U, 0x11U, 0x0EU},
    {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U},
    {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU},
    {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U},
    {0x11U, 0x19U, 0x15U, 0x13U, 0x11U, 0x11U, 0x11U},
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU},
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U},
    {0x0EU, 0x11U, 0x11U, 0x11U, 0x15U, 0x12U, 0x0DU},
    {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U},
    {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU},
    {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U},
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU},
    {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0AU, 0x04U},
    {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU},
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x0AU, 0x11U, 0x11U},
    {0x11U, 0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U},
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x1FU},
    {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU},
    {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU},
    {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU},
    {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU},
    {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U},
    {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU},
    {0x07U, 0x08U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU},
    {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U},
    {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU},
    {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x02U, 0x1CU}
  };
  const uint8_t *src = blank;
  uint16_t i;

  if ((glyph >= 'A') && (glyph <= 'Z'))
  {
    src = glyphs[(uint32_t)(glyph - 'A')];
  }
  else if ((glyph >= '0') && (glyph <= '9'))
  {
    src = glyphs[26U + (uint32_t)(glyph - '0')];
  }
  else if (glyph != ' ')
  {
    return 0UL;
  }

  for (i = 0U; i < 7U; ++i)
  {
    rows[i] = src[i];
  }
  return 1UL;
}

static uint32_t DisplayRenderer_DrawGlyph(uint16_t x,
                                          uint16_t y,
                                          char glyph,
                                          uint16_t scale)
{
  uint8_t rows[7];
  uint32_t count = 0UL;
  uint16_t row;
  uint16_t col;
  uint16_t sx;
  uint16_t sy;

  if (DisplayRenderer_GlyphRows(glyph, rows) == 0UL)
  {
    return 0UL;
  }

  for (row = 0U; row < 7U; ++row)
  {
    for (col = 0U; col < 5U; ++col)
    {
      if ((rows[row] & (uint8_t)(1U << (4U - col))) != 0U)
      {
        for (sy = 0U; sy < scale; ++sy)
        {
          for (sx = 0U; sx < scale; ++sx)
          {
            count += DisplayRenderer_SetBlack(
              (uint16_t)(x + (col * scale) + sx),
              (uint16_t)(y + (row * scale) + sy));
          }
        }
      }
    }
  }
  return count;
}

static uint16_t DisplayRenderer_TextWidth(const char *text, uint16_t scale)
{
  uint16_t count = 0U;

  while ((text != NULL) && (text[count] != '\0'))
  {
    ++count;
  }
  return (uint16_t)(count * 6U * scale);
}

static uint32_t DisplayRenderer_DrawText(uint16_t x,
                                         uint16_t y,
                                         const char *text,
                                         uint16_t scale)
{
  uint32_t black_pixels = 0UL;
  uint16_t index = 0U;

  while ((text != NULL) && (text[index] != '\0'))
  {
    black_pixels += DisplayRenderer_DrawGlyph(
      (uint16_t)(x + (index * 6U * scale)), y, text[index], scale);
    ++index;
  }
  return black_pixels;
}

static uint32_t DisplayRenderer_DrawCenteredText(uint16_t y,
                                                 const char *text,
                                                 uint16_t scale)
{
  uint16_t width = DisplayRenderer_TextWidth(text, scale);
  uint16_t x = 0U;

  if (width < DISPLAY_RENDERER_WIDTH)
  {
    x = (uint16_t)((DISPLAY_RENDERER_WIDTH - width) / 2U);
  }
  return DisplayRenderer_DrawText(x, y, text, scale);
}

static const char *DisplayRenderer_ShutdownCountdownLine(
  uint32_t countdown_seconds)
{
  if (countdown_seconds == 3UL)
  {
    return "POWER OFF IN 3";
  }
  if (countdown_seconds == 2UL)
  {
    return "POWER OFF IN 2";
  }
  if (countdown_seconds == 1UL)
  {
    return "POWER OFF IN 1";
  }
  return "PREPARING";
}
static void DisplayRenderer_UIStrings(uint32_t page,
                                      uint32_t calibration_page,
                                      uint32_t focus_index,
                                      uint32_t shutdown_state,
                                      uint32_t shutdown_countdown_seconds,
                                      const char **title,
                                      const char **line1,
                                      const char **line2)
{
  *title = "PEEPSHOW";
  *line1 = "HOME";
  *line2 = "MENU A";

  switch (page)
  {
    case PS_UI_ROUTER_PAGE_HOME:
      break;
    case PS_UI_ROUTER_PAGE_MENU:
      *title = "SYSTEM";
      if (focus_index == 0UL)
      {
        *line1 = "SETTINGS";
        *line2 = "CAL INPUT";
      }
      else if (focus_index == 1UL)
      {
        *line1 = "CAL INPUT";
        *line2 = "PACKAGES";
      }
      else
      {
        *line1 = "PACKAGES";
        *line2 = "SETTINGS";
      }
      break;
    case PS_UI_ROUTER_PAGE_SETTINGS:
      *title = "SETTINGS";
      *line1 = "INPUT";
      *line2 = "DISPLAY";
      break;
    case PS_UI_ROUTER_PAGE_CALIBRATION:
      *title = "CALIBRATION";
      if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL)
      {
        *line1 = "STICK CENTER";
        *line2 = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_RIGHT)
      {
        *line1 = "MOVE RIGHT";
        *line2 = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_CIRCLE)
      {
        *line1 = "MAKE CIRCLES";
        *line2 = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_REVIEW)
      {
        *line1 = "REVIEW";
        *line2 = "PRESS A";
      }
      else
      {
        *line1 = "INPUT";
        *line2 = "JOYSTICK A";
      }
      break;
    case PS_UI_ROUTER_PAGE_PACKAGE_BROWSER:
      if (focus_index == PS_UI_ROUTER_PACKAGE_CANDIDATE)
      {
        *title = "PACKAGE";
        *line1 = "FOUND";
        *line2 = "WAIT";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_VALID)
      {
        *title = "PACKAGE";
        *line1 = "VALID";
        *line2 = "A INSTALL";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_INSTALLING)
      {
        *title = "PACKAGE";
        *line1 = "INSTALL STUB";
        *line2 = "WAIT";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_INSTALLED)
      {
        *title = "PACKAGE";
        *line1 = "STUB DONE";
        *line2 = "B BACK";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_ERROR)
      {
        *title = "PACKAGE";
        *line1 = "PKG ERROR";
        *line2 = "SEE GDB";
      }
      else
      {
        *title = "USB";
        *line1 = "TRANSFER";
        *line2 = "START A";
      }
      break;
    case PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF:
      *title = "RUNTIME";
      *line1 = "HANDOFF";
      *line2 = "WAIT";
      break;
    case PS_UI_ROUTER_PAGE_ERROR:
      *title = "ERROR";
      *line1 = "SHELL FAULT";
      *line2 = "RECOVER";
      break;
    case PS_UI_ROUTER_PAGE_SHUTDOWN:
      if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_BOOT)
      {
        *title = "LOW BATTERY";
        *line1 = "CHARGE DEVICE";
        *line2 = "NO RUNTIME";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE)
      {
        *title = "LOW BATTERY";
        *line1 = "CHARGING";
        *line2 = "PLEASE WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_INIT)
      {
        *title = "FLASH INIT";
        *line1 = "USB STAGING";
        *line2 = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_DONE)
      {
        *title = "FLASH INIT";
        *line1 = "USB STAGING";
        *line2 = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_ERROR)
      {
        *title = "FLASH INIT";
        *line1 = "USB STAGING";
        *line2 = "ERROR";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_EXPORT)
      {
        *title = "USB MSC";
        *line1 = "EXPORT";
        *line2 = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_ACTIVE)
      {
        *title = "USB MSC";
        *line1 = "ACTIVE";
        *line2 = "EJECT FIRST";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_RECLAIM)
      {
        *title = "USB MSC";
        *line1 = "RECLAIM";
        *line2 = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_DONE)
      {
        *title = "USB MSC";
        *line1 = "RECLAIM";
        *line2 = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_ERROR)
      {
        *title = "USB MSC";
        *line1 = "ERROR";
        *line2 = "SEE GDB";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY)
      {
        *title = "USB MSC";
        *line1 = "MSC NEEDS";
        *line2 = "FLASH INIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_REST)
      {
        *title = "JOYSTICK REST";
        *line1 = "FLICK";
        *line2 = "RELEASE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_SWEEP)
      {
        *title = "JOYSTICK SWEEP";
        *line1 = "FULL";
        *line2 = "TRAVEL";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_DONE)
      {
        *title = "JOYSTICK XYZ";
        *line1 = "CAPTURE";
        *line2 = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_ERROR)
      {
        *title = "JOYSTICK XYZ";
        *line1 = "CAPTURE";
        *line2 = "ERROR";
      }
      else
      {
        *title = "SHUTDOWN";
        *line1 = DisplayRenderer_ShutdownCountdownLine(
          shutdown_countdown_seconds);
        *line2 = (shutdown_state == PS_UI_ROUTER_SHUTDOWN_CANCELLED) ?
          "CANCELLED" : "HOLD START";
      }
      break;
    default:
      *title = "BOOT";
      *line1 = "STARTING";
      *line2 = "WAIT";
      break;
  }
}
void DisplayRenderer_PrepareUIPage(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds,
  display_renderer_stats_t *stats)
{
  const char *title;
  const char *line1;
  const char *line2;
  uint32_t black_pixels = 0UL;

  DisplayRenderer_ClearWhite();
  s_rotate_ccw = 1UL;
  DisplayRenderer_UIStrings(page,
                            calibration_page,
                            focus_index,
                            shutdown_state,
                            shutdown_countdown_seconds,
                            &title,
                            &line1,
                            &line2);

  black_pixels += DisplayRenderer_HorizontalLine(
    0U, (uint16_t)(DISPLAY_RENDERER_WIDTH - 1U), 0U);
  black_pixels += DisplayRenderer_HorizontalLine(
    0U, (uint16_t)(DISPLAY_RENDERER_WIDTH - 1U),
    (uint16_t)(DISPLAY_RENDERER_HEIGHT - 1U));
  black_pixels += DisplayRenderer_VerticalLine(
    0U, 0U, (uint16_t)(DISPLAY_RENDERER_HEIGHT - 1U));
  black_pixels += DisplayRenderer_VerticalLine(
    (uint16_t)(DISPLAY_RENDERER_WIDTH - 1U), 0U,
    (uint16_t)(DISPLAY_RENDERER_HEIGHT - 1U));
  black_pixels += DisplayRenderer_HorizontalLine(
    8U, (uint16_t)(DISPLAY_RENDERER_WIDTH - 9U), 28U);
  black_pixels += DisplayRenderer_DrawCenteredText(
    8U, title, DISPLAY_RENDERER_TEXT_SCALE);
  black_pixels += DisplayRenderer_DrawCenteredText(
    58U, line1, DISPLAY_RENDERER_TEXT_SCALE);
  black_pixels += DisplayRenderer_DrawCenteredText(
    94U, line2, DISPLAY_RENDERER_TEXT_SCALE);
  black_pixels += DisplayRenderer_FilledRect(76U, 132U, 16U, 6U);
  s_rotate_ccw = 0UL;

  DisplayRenderer_FillStats(stats, black_pixels);
}

void DisplayRenderer_PreparePattern(display_renderer_stats_t *stats)
{
  uint32_t black_pixels = 0UL;
  uint16_t i;

  DisplayRenderer_ClearWhite();
  s_rotate_ccw = 0UL;

  for (i = 0U; i < 2U; ++i)
  {
    black_pixels += DisplayRenderer_HorizontalLine(
      i, (uint16_t)(DISPLAY_WIDTH - 1U - i), i);
    black_pixels += DisplayRenderer_HorizontalLine(
      i, (uint16_t)(DISPLAY_WIDTH - 1U - i),
      (uint16_t)(DISPLAY_HEIGHT - 1U - i));
    black_pixels += DisplayRenderer_VerticalLine(
      i, i, (uint16_t)(DISPLAY_HEIGHT - 1U - i));
    black_pixels += DisplayRenderer_VerticalLine(
      (uint16_t)(DISPLAY_WIDTH - 1U - i), i,
      (uint16_t)(DISPLAY_HEIGHT - 1U - i));
  }

  black_pixels += DisplayRenderer_HorizontalLine(
    8U, (uint16_t)(DISPLAY_WIDTH - 9U), DISPLAY_HEIGHT / 2U);
  black_pixels += DisplayRenderer_VerticalLine(
    DISPLAY_WIDTH / 2U, 8U, (uint16_t)(DISPLAY_HEIGHT - 9U));

  black_pixels += DisplayRenderer_FilledRect(8U, 8U, 10U, 10U);
  black_pixels += DisplayRenderer_HorizontalLine(
    (uint16_t)(DISPLAY_WIDTH - 19U), (uint16_t)(DISPLAY_WIDTH - 9U), 8U);
  black_pixels += DisplayRenderer_VerticalLine(
    (uint16_t)(DISPLAY_WIDTH - 9U), 8U, 18U);
  for (i = 0U; i < 11U; ++i)
  {
    black_pixels += DisplayRenderer_SetBlack(
      (uint16_t)(8U + i), (uint16_t)(DISPLAY_HEIGHT - 19U + i));
    black_pixels += DisplayRenderer_SetBlack(
      (uint16_t)(18U - i), (uint16_t)(DISPLAY_HEIGHT - 19U + i));
  }
  for (i = 0U; i < 12U; ++i)
  {
    if ((i & 1U) == 0U)
    {
      black_pixels += DisplayRenderer_FilledRect(
        (uint16_t)(DISPLAY_WIDTH - 20U + i),
        (uint16_t)(DISPLAY_HEIGHT - 20U), 1U, 12U);
    }
  }

  DisplayRenderer_FillStats(stats, black_pixels);
}
