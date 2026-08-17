#include "display_renderer.h"

#include <string.h>

#include "ps_ui_router.h"

#define DISPLAY_RENDERER_TEXT_SCALE         (2U)
#define DISPLAY_RENDERER_LIST_ROW_COUNT     (3U)
#define DISPLAY_RENDERER_LIST_TEXT_SCALE    (2U)
#define DISPLAY_RENDERER_LIST_TITLE_Y       (7U)
#define DISPLAY_RENDERER_LIST_DIVIDER_Y     (27U)
#define DISPLAY_RENDERER_LIST_ROW_Y0        (43U)
#define DISPLAY_RENDERER_LIST_ROW_STEP      (30U)
#define DISPLAY_RENDERER_LIST_CURSOR_X      (8U)
#define DISPLAY_RENDERER_LIST_TEXT_X        (26U)
#define DISPLAY_RENDERER_LIST_CURSOR_WIDTH  (8U)
#define DISPLAY_RENDERER_LIST_CURSOR_HEIGHT (16U)

static uint8_t s_display_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static uint8_t s_display_committed_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static uint16_t s_display_dirty_rows[DISPLAY_RENDERER_DIRTY_ROW_MAX];
static uint8_t s_display_dirty_row_marks[DISPLAY_RENDERER_DIRTY_ROW_MAX];
static uint16_t s_display_dirty_row_count;
static uint32_t s_display_committed_valid;
static uint32_t s_rotate_ccw;
static display_renderer_panel_region_t s_lpbam_cursor_panel_region;
static uint32_t s_lpbam_cursor_panel_region_valid;

static void DisplayRenderer_ResetDirtyRows(void)
{
  (void)memset(s_display_dirty_rows, 0, sizeof(s_display_dirty_rows));
  (void)memset(s_display_dirty_row_marks, 0,
               sizeof(s_display_dirty_row_marks));
  s_display_dirty_row_count = 0U;
}

static void DisplayRenderer_MarkPanelRowDirty(uint16_t panel_y)
{
  uint16_t panel_row;
  uint16_t insert_at;
  uint16_t i;

  if (panel_y >= DISPLAY_HEIGHT)
  {
    return;
  }
  if (s_display_dirty_row_marks[panel_y] != 0U)
  {
    return;
  }
  if (s_display_dirty_row_count >= DISPLAY_RENDERER_DIRTY_ROW_MAX)
  {
    return;
  }

  panel_row = (uint16_t)(panel_y + 1U);
  insert_at = 0U;
  while ((insert_at < s_display_dirty_row_count) &&
         (s_display_dirty_rows[insert_at] < panel_row))
  {
    ++insert_at;
  }
  for (i = s_display_dirty_row_count; i > insert_at; --i)
  {
    s_display_dirty_rows[i] = s_display_dirty_rows[i - 1U];
  }
  s_display_dirty_rows[insert_at] = panel_row;
  s_display_dirty_row_marks[panel_y] = 1U;
  ++s_display_dirty_row_count;
}

static void DisplayRenderer_MarkAllRowsDirty(void)
{
  uint16_t row;

  for (row = 0U; row < DISPLAY_HEIGHT; ++row)
  {
    DisplayRenderer_MarkPanelRowDirty(row);
  }
}

static void DisplayRenderer_ComputeDirtyRowsFromCommitted(void)
{
  uint16_t row;
  uint32_t row_offset;

  DisplayRenderer_ResetDirtyRows();
  if (s_display_committed_valid == 0UL)
  {
    DisplayRenderer_MarkAllRowsDirty();
    return;
  }

  for (row = 0U; row < DISPLAY_HEIGHT; ++row)
  {
    row_offset = (uint32_t)row * LINE_WIDTH;
    if (memcmp(&s_display_framebuffer[row_offset],
               &s_display_committed_framebuffer[row_offset],
               LINE_WIDTH) != 0)
    {
      DisplayRenderer_MarkPanelRowDirty(row);
    }
  }
}

static void DisplayRenderer_InvalidateLpbamCursorRegion(void)
{
  (void)memset(&s_lpbam_cursor_panel_region, 0,
               sizeof(s_lpbam_cursor_panel_region));
  s_lpbam_cursor_panel_region_valid = 0UL;
}

static void DisplayRenderer_RecordLpbamCursorRegion(uint32_t row)
{
  uint16_t logical_x0;
  uint16_t logical_x1;
  uint16_t logical_y0;
  uint16_t logical_y1;
  uint16_t panel_x0;
  uint16_t panel_x1;
  uint16_t panel_y0;
  uint16_t panel_y1;

  if (row >= DISPLAY_RENDERER_LIST_ROW_COUNT)
  {
    DisplayRenderer_InvalidateLpbamCursorRegion();
    return;
  }

  logical_x0 = DISPLAY_RENDERER_LIST_CURSOR_X;
  logical_x1 = (uint16_t)(logical_x0 +
                          DISPLAY_RENDERER_LIST_CURSOR_WIDTH - 1U);
  logical_y0 = (uint16_t)(DISPLAY_RENDERER_LIST_ROW_Y0 +
                          (row * DISPLAY_RENDERER_LIST_ROW_STEP));
  logical_y1 = (uint16_t)(logical_y0 +
                          DISPLAY_RENDERER_LIST_CURSOR_HEIGHT - 1U);

  if (s_rotate_ccw != 0UL)
  {
    panel_x0 = logical_y0;
    panel_x1 = logical_y1;
    panel_y0 = (uint16_t)(DISPLAY_RENDERER_WIDTH - 1U - logical_x1);
    panel_y1 = (uint16_t)(DISPLAY_RENDERER_WIDTH - 1U - logical_x0);
  }
  else
  {
    panel_x0 = logical_x0;
    panel_x1 = logical_x1;
    panel_y0 = logical_y0;
    panel_y1 = logical_y1;
  }

  s_lpbam_cursor_panel_region.start_row = (uint16_t)(panel_y0 + 1U);
  s_lpbam_cursor_panel_region.row_count =
    (uint16_t)(panel_y1 - panel_y0 + 1U);
  s_lpbam_cursor_panel_region.start_column = panel_x0;
  s_lpbam_cursor_panel_region.column_count =
    (uint16_t)(panel_x1 - panel_x0 + 1U);
  s_lpbam_cursor_panel_region_valid = 1UL;
}

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
  stats->dirty_row_count = s_display_dirty_row_count;
  stats->dirty_first_row = (s_display_dirty_row_count == 0U) ? 0UL :
    (uint32_t)s_display_dirty_rows[0];
  stats->dirty_last_row = (s_display_dirty_row_count == 0U) ? 0UL :
    (uint32_t)s_display_dirty_rows[s_display_dirty_row_count - 1U];
}

void DisplayRenderer_ClearWhite(void)
{
  DisplayRenderer_ResetDirtyRows();
  (void)memset(s_display_framebuffer, 0xFF,
               sizeof(s_display_framebuffer));
  DisplayRenderer_InvalidateLpbamCursorRegion();
}

const uint8_t *DisplayRenderer_GetBuffer(void)
{
  return s_display_framebuffer;
}

uint32_t DisplayRenderer_GetDirtyRows(const uint16_t **rows)
{
  if (rows != NULL)
  {
    *rows = s_display_dirty_rows;
  }
  return (uint32_t)s_display_dirty_row_count;
}

void DisplayRenderer_CommitPresentedFrame(void)
{
  (void)memcpy(s_display_committed_framebuffer,
               s_display_framebuffer,
               sizeof(s_display_committed_framebuffer));
  s_display_committed_valid = 1UL;
}

uint32_t DisplayRenderer_GetLpbamCursorPanelRegion(
  display_renderer_panel_region_t *region)
{
  if ((region == NULL) || (s_lpbam_cursor_panel_region_valid == 0UL))
  {
    return 0UL;
  }

  *region = s_lpbam_cursor_panel_region;
  return 1UL;
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

typedef struct
{
  const char *title;
  const char *rows[DISPLAY_RENDERER_LIST_ROW_COUNT];
  uint32_t selected_row;
} display_renderer_list_t;

static void DisplayRenderer_ListInit(display_renderer_list_t *list)
{
  uint32_t i;

  list->title = "PEEPSHOW";
  for (i = 0U; i < DISPLAY_RENDERER_LIST_ROW_COUNT; ++i)
  {
    list->rows[i] = "";
  }
  list->selected_row = 0UL;
}

static void DisplayRenderer_UIList(uint32_t page,
                                   uint32_t calibration_page,
                                   uint32_t focus_index,
                                   uint32_t shutdown_state,
                                   uint32_t shutdown_countdown_seconds,
                                   display_renderer_list_t *list)
{
  DisplayRenderer_ListInit(list);
  list->rows[0] = "HOME";
  list->rows[1] = "MENU";
  list->rows[2] = "PACKAGES";

  switch (page)
  {
    case PS_UI_ROUTER_PAGE_HOME:
      list->selected_row = (focus_index >= DISPLAY_RENDERER_LIST_ROW_COUNT) ?
        1UL : focus_index;
      break;
    case PS_UI_ROUTER_PAGE_MENU:
      list->title = "SYSTEM";
      list->rows[0] = "SETTINGS";
      list->rows[1] = "CAL INPUT";
      list->rows[2] = "PACKAGES";
      list->selected_row = (focus_index >= DISPLAY_RENDERER_LIST_ROW_COUNT) ?
        0UL : focus_index;
      break;
    case PS_UI_ROUTER_PAGE_SETTINGS:
      list->title = "SETTINGS";
      list->rows[0] = "INPUT";
      list->rows[1] = "DISPLAY";
      list->rows[2] = "BACK";
      break;
    case PS_UI_ROUTER_PAGE_CALIBRATION:
      list->title = "CALIBRATION";
      if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL)
      {
        list->rows[0] = "STICK CENTER";
        list->rows[1] = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_RIGHT)
      {
        list->rows[0] = "MOVE RIGHT";
        list->rows[1] = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_CIRCLE)
      {
        list->rows[0] = "MAKE CIRCLES";
        list->rows[1] = "PRESS A";
      }
      else if (calibration_page == PS_UI_ROUTER_CAL_JOYSTICK_REVIEW)
      {
        list->rows[0] = "REVIEW";
        list->rows[1] = "PRESS A";
      }
      else
      {
        list->rows[0] = "INPUT";
        list->rows[1] = "JOYSTICK A";
      }
      list->rows[2] = "B BACK";
      break;
    case PS_UI_ROUTER_PAGE_PACKAGE_BROWSER:
      if (focus_index == PS_UI_ROUTER_PACKAGE_CANDIDATE)
      {
        list->title = "PACKAGE";
        list->rows[0] = "FOUND";
        list->rows[1] = "WAIT";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_VALID)
      {
        list->title = "PACKAGE";
        list->rows[0] = "VALID";
        list->rows[1] = "A INSTALL";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_INSTALLING)
      {
        list->title = "PACKAGE";
        list->rows[0] = "INSTALL STUB";
        list->rows[1] = "WAIT";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_INSTALLED)
      {
        list->title = "PACKAGE";
        list->rows[0] = "STUB DONE";
        list->rows[1] = "B BACK";
      }
      else if (focus_index == PS_UI_ROUTER_PACKAGE_ERROR)
      {
        list->title = "PACKAGE";
        list->rows[0] = "PKG ERROR";
        list->rows[1] = "SEE GDB";
      }
      else
      {
        list->title = "USB";
        list->rows[0] = "TRANSFER";
        list->rows[1] = "START A";
      }
      list->rows[2] = "B BACK";
      break;
    case PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF:
      list->title = "RUNTIME";
      list->rows[0] = "HANDOFF";
      list->rows[1] = "WAIT";
      break;
    case PS_UI_ROUTER_PAGE_ERROR:
      list->title = "ERROR";
      list->rows[0] = "SHELL FAULT";
      list->rows[1] = "RECOVER";
      break;
    case PS_UI_ROUTER_PAGE_SHUTDOWN:
      if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_BOOT)
      {
        list->title = "LOW BATTERY";
        list->rows[0] = "CHARGE DEVICE";
        list->rows[1] = "NO RUNTIME";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE)
      {
        list->title = "LOW BATTERY";
        list->rows[0] = "CHARGING";
        list->rows[1] = "PLEASE WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_INIT)
      {
        list->title = "FLASH INIT";
        list->rows[0] = "USB STAGING";
        list->rows[1] = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_DONE)
      {
        list->title = "FLASH INIT";
        list->rows[0] = "USB STAGING";
        list->rows[1] = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_FLASH_ERROR)
      {
        list->title = "FLASH INIT";
        list->rows[0] = "USB STAGING";
        list->rows[1] = "ERROR";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_EXPORT)
      {
        list->title = "USB MSC";
        list->rows[0] = "EXPORT";
        list->rows[1] = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_ACTIVE)
      {
        list->title = "USB MSC";
        list->rows[0] = "ACTIVE";
        list->rows[1] = "EJECT FIRST";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_RECLAIM)
      {
        list->title = "USB MSC";
        list->rows[0] = "RECLAIM";
        list->rows[1] = "WAIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_DONE)
      {
        list->title = "USB MSC";
        list->rows[0] = "RECLAIM";
        list->rows[1] = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_ERROR)
      {
        list->title = "USB MSC";
        list->rows[0] = "ERROR";
        list->rows[1] = "SEE GDB";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY)
      {
        list->title = "USB MSC";
        list->rows[0] = "MSC NEEDS";
        list->rows[1] = "FLASH INIT";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_REST)
      {
        list->title = "JOYSTICK REST";
        list->rows[0] = "FLICK";
        list->rows[1] = "RELEASE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_SWEEP)
      {
        list->title = "JOYSTICK SWEEP";
        list->rows[0] = "FULL";
        list->rows[1] = "TRAVEL";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_DONE)
      {
        list->title = "JOYSTICK XYZ";
        list->rows[0] = "CAPTURE";
        list->rows[1] = "DONE";
      }
      else if (shutdown_state == PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_ERROR)
      {
        list->title = "JOYSTICK XYZ";
        list->rows[0] = "CAPTURE";
        list->rows[1] = "ERROR";
      }
      else
      {
        list->title = "SHUTDOWN";
        list->rows[0] = DisplayRenderer_ShutdownCountdownLine(
          shutdown_countdown_seconds);
        list->rows[1] = (shutdown_state == PS_UI_ROUTER_SHUTDOWN_CANCELLED) ?
          "CANCELLED" : "HOLD START";
      }
      break;
    default:
      list->title = "BOOT";
      list->rows[0] = "STARTING";
      list->rows[1] = "WAIT";
      break;
  }
}

static uint32_t DisplayRenderer_DrawListCursor(uint32_t row, uint32_t visible)
{
  uint16_t y;

  if ((visible == 0UL) || (row >= DISPLAY_RENDERER_LIST_ROW_COUNT))
  {
    return 0UL;
  }

  y = (uint16_t)(DISPLAY_RENDERER_LIST_ROW_Y0 +
                 (row * DISPLAY_RENDERER_LIST_ROW_STEP));
  DisplayRenderer_RecordLpbamCursorRegion(row);
  return DisplayRenderer_FilledRect(DISPLAY_RENDERER_LIST_CURSOR_X,
                                    y,
                                    DISPLAY_RENDERER_LIST_CURSOR_WIDTH,
                                    DISPLAY_RENDERER_LIST_CURSOR_HEIGHT);
}

static uint32_t DisplayRenderer_DrawList(const display_renderer_list_t *list)
{
  uint32_t black_pixels = 0UL;
  uint32_t row;

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
    8U, (uint16_t)(DISPLAY_RENDERER_WIDTH - 9U),
    DISPLAY_RENDERER_LIST_DIVIDER_Y);
  black_pixels += DisplayRenderer_DrawCenteredText(
    DISPLAY_RENDERER_LIST_TITLE_Y, list->title, DISPLAY_RENDERER_TEXT_SCALE);

  for (row = 0U; row < DISPLAY_RENDERER_LIST_ROW_COUNT; ++row)
  {
    black_pixels += DisplayRenderer_DrawText(
      DISPLAY_RENDERER_LIST_TEXT_X,
      (uint16_t)(DISPLAY_RENDERER_LIST_ROW_Y0 +
                 (row * DISPLAY_RENDERER_LIST_ROW_STEP)),
      list->rows[row],
      DISPLAY_RENDERER_LIST_TEXT_SCALE);
  }

  black_pixels += DisplayRenderer_DrawListCursor(list->selected_row, 1UL);
  return black_pixels;
}

void DisplayRenderer_PrepareUIPage(
  uint32_t page,
  uint32_t calibration_page,
  uint32_t focus_index,
  uint32_t shutdown_state,
  uint32_t shutdown_countdown_seconds,
  display_renderer_stats_t *stats)
{
  display_renderer_list_t list;
  uint32_t black_pixels = 0UL;

  DisplayRenderer_ClearWhite();
  s_rotate_ccw = 1UL;
  DisplayRenderer_UIList(page,
                         calibration_page,
                         focus_index,
                         shutdown_state,
                         shutdown_countdown_seconds,
                         &list);
  black_pixels += DisplayRenderer_DrawList(&list);
  s_rotate_ccw = 0UL;

  DisplayRenderer_ComputeDirtyRowsFromCommitted();
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

  DisplayRenderer_ComputeDirtyRowsFromCommitted();
  DisplayRenderer_FillStats(stats, black_pixels);
}
