#include "display_renderer.h"

#include <string.h>

#include "knobs_autogen.h"
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

typedef struct
{
  const char *title;
  const char *rows[DISPLAY_RENDERER_LIST_ROW_COUNT];
  uint32_t selected_row;
} display_renderer_list_t;

static uint8_t s_display_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static uint8_t s_display_committed_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static uint8_t s_display_cursor_base_framebuffer[DISPLAY_RENDERER_BUFFER_SIZE];
static uint16_t s_display_dirty_rows[DISPLAY_RENDERER_DIRTY_ROW_MAX];
static uint8_t s_display_dirty_row_marks[DISPLAY_RENDERER_DIRTY_ROW_MAX];
static uint16_t
  s_display_waiting_candidate_rows[DISPLAY_RENDERER_DIRTY_ROW_MAX];
static uint8_t
  s_display_waiting_candidate_row_marks[DISPLAY_RENDERER_DIRTY_ROW_MAX];
static display_renderer_waiting_animation_t s_display_waiting_animation;
static display_renderer_waiting_animation_t
  s_display_waiting_guaranteed_animation;
static const display_renderer_waiting_animation_t
  *s_display_selected_waiting_animation = &s_display_waiting_animation;
static uint16_t s_display_dirty_row_count;
static uint32_t s_display_committed_valid;
static uint32_t s_display_cursor_base_valid;
static display_renderer_list_t s_display_committed_list;
static display_renderer_list_t s_display_pending_list;
static uint32_t s_display_committed_list_valid;
static uint32_t s_display_pending_list_valid;
static uint32_t s_display_pending_list_invalidates;
static uint32_t s_rotate_ccw;
static display_renderer_panel_region_t s_lpbam_cursor_panel_region;
static uint32_t s_lpbam_cursor_panel_region_valid;
volatile uint32_t g_display_renderer_waiting_test_variant;

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

static uint32_t DisplayRenderer_CountBlackPixels(void)
{
  uint32_t count = 0UL;
  uint16_t i;
  uint8_t bit;

  for (i = 0U; i < DISPLAY_RENDERER_BUFFER_SIZE; ++i)
  {
    for (bit = 0U; bit < 8U; ++bit)
    {
      if ((s_display_framebuffer[i] & (uint8_t)(1U << bit)) == 0U)
      {
        ++count;
      }
    }
  }
  return count;
}

static void DisplayRenderer_SetPanelPixelWhiteInBuffer(
  uint8_t *framebuffer,
  uint16_t panel_x,
  uint16_t panel_y)
{
  uint32_t index;
  uint8_t mask;

  if ((framebuffer == NULL) || (panel_x >= DISPLAY_WIDTH) ||
      (panel_y >= DISPLAY_HEIGHT))
  {
    return;
  }

  index = ((uint32_t)panel_y * LINE_WIDTH) + ((uint32_t)panel_x >> 3U);
  mask = (uint8_t)(1U << (panel_x & 7U));
  framebuffer[index] |= mask;
}

static void DisplayRenderer_SetPanelPixelWhite(uint16_t panel_x,
                                               uint16_t panel_y)
{
  DisplayRenderer_SetPanelPixelWhiteInBuffer(
    s_display_framebuffer, panel_x, panel_y);
}

static void DisplayRenderer_ClearPanelRegion(
  const display_renderer_panel_region_t *region)
{
  uint16_t row;
  uint16_t column;
  uint16_t row_end;
  uint16_t column_end;

  if ((region == NULL) || (region->row_count == 0U) ||
      (region->column_count == 0U) || (region->start_row == 0U))
  {
    return;
  }

  row_end = (uint16_t)(region->start_row + region->row_count - 1U);
  column_end = (uint16_t)(region->start_column + region->column_count);
  if ((row_end > DISPLAY_HEIGHT) || (column_end > DISPLAY_WIDTH))
  {
    return;
  }

  for (row = region->start_row; row <= row_end; ++row)
  {
    for (column = region->start_column; column < column_end; ++column)
    {
      DisplayRenderer_SetPanelPixelWhite(column, (uint16_t)(row - 1U));
    }
  }
}

static void DisplayRenderer_HollowPanelRegionInBuffer(
  uint8_t *framebuffer,
  const display_renderer_panel_region_t *region)
{
  uint16_t row;
  uint16_t column;
  uint16_t row_end;
  uint16_t column_end;

  if ((framebuffer == NULL) || (region == NULL) ||
      (region->row_count <= 2U) ||
      (region->column_count <= 2U) || (region->start_row == 0U))
  {
    return;
  }

  row_end = (uint16_t)(region->start_row + region->row_count - 1U);
  column_end = (uint16_t)(region->start_column + region->column_count);
  if ((row_end > DISPLAY_HEIGHT) || (column_end > DISPLAY_WIDTH))
  {
    return;
  }

  for (row = (uint16_t)(region->start_row + 1U); row < row_end; ++row)
  {
    for (column = (uint16_t)(region->start_column + 1U);
         column < (uint16_t)(column_end - 1U);
         ++column)
    {
      DisplayRenderer_SetPanelPixelWhiteInBuffer(
        framebuffer, column, (uint16_t)(row - 1U));
    }
  }
}

static void DisplayRenderer_InvalidateLpbamCursorRegion(void)
{
  (void)memset(&s_lpbam_cursor_panel_region, 0,
               sizeof(s_lpbam_cursor_panel_region));
  s_lpbam_cursor_panel_region_valid = 0UL;
}

static uint32_t DisplayRenderer_GetListCursorPanelRegion(
  uint32_t row,
  display_renderer_panel_region_t *region)
{
  uint16_t logical_x0;
  uint16_t logical_x1;
  uint16_t logical_y0;
  uint16_t logical_y1;
  uint16_t panel_x0;
  uint16_t panel_x1;
  uint16_t panel_y0;
  uint16_t panel_y1;

  if ((region == NULL) || (row >= DISPLAY_RENDERER_LIST_ROW_COUNT))
  {
    return 0UL;
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

  region->start_row = (uint16_t)(panel_y0 + 1U);
  region->row_count = (uint16_t)(panel_y1 - panel_y0 + 1U);
  region->start_column = panel_x0;
  region->column_count = (uint16_t)(panel_x1 - panel_x0 + 1U);
  return 1UL;
}

static void DisplayRenderer_RecordLpbamCursorRegion(uint32_t row)
{
  if (DisplayRenderer_GetListCursorPanelRegion(
        row, &s_lpbam_cursor_panel_region) == 0UL)
  {
    DisplayRenderer_InvalidateLpbamCursorRegion();
    return;
  }
  s_lpbam_cursor_panel_region_valid = 1UL;
}

static void DisplayRenderer_ClearListCursor(uint32_t row)
{
  display_renderer_panel_region_t region;

  if (DisplayRenderer_GetListCursorPanelRegion(row, &region) != 0UL)
  {
    DisplayRenderer_ClearPanelRegion(&region);
  }
}

static void DisplayRenderer_RecordCursorBaseFrame(void)
{
  if (s_lpbam_cursor_panel_region_valid == 0UL)
  {
    s_display_cursor_base_valid = 0UL;
    return;
  }

  (void)memcpy(s_display_cursor_base_framebuffer,
               s_display_framebuffer,
               sizeof(s_display_cursor_base_framebuffer));
  s_display_cursor_base_valid = 1UL;
}

static void DisplayRenderer_FillStats(display_renderer_stats_t *stats,
                                      uint32_t black_pixels,
                                      uint32_t primitive_id,
                                      uint32_t previous_focus_row,
                                      uint32_t current_focus_row)
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
  stats->primitive_id = primitive_id;
  stats->previous_focus_row = previous_focus_row;
  stats->current_focus_row = current_focus_row;
}

void DisplayRenderer_ClearWhite(void)
{
  DisplayRenderer_ResetDirtyRows();
  (void)memset(s_display_framebuffer, 0xFF,
               sizeof(s_display_framebuffer));
  DisplayRenderer_InvalidateLpbamCursorRegion();
  s_display_cursor_base_valid = 0UL;
  s_display_pending_list_valid = 0UL;
  s_display_pending_list_invalidates = 1UL;
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

  if (s_display_pending_list_valid != 0UL)
  {
    s_display_committed_list = s_display_pending_list;
    s_display_committed_list_valid = 1UL;
  }
  else if (s_display_pending_list_invalidates != 0UL)
  {
    (void)memset(&s_display_committed_list, 0,
                 sizeof(s_display_committed_list));
    s_display_committed_list_valid = 0UL;
  }
  s_display_pending_list_valid = 0UL;
  s_display_pending_list_invalidates = 0UL;
}

static uint32_t DisplayRenderer_ApplyCursorBlinkPhase(
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  if ((destination == NULL) ||
      (destination_size < DISPLAY_RENDERER_BUFFER_SIZE) ||
      (s_lpbam_cursor_panel_region_valid == 0UL) ||
      (phase >= 2UL))
  {
    return 0UL;
  }

  if (phase == 0UL)
  {
    DisplayRenderer_HollowPanelRegionInBuffer(
      destination, &s_lpbam_cursor_panel_region);
  }

  return 1UL;
}

static uint32_t DisplayRenderer_CopyCursorBlinkFrame(
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  if ((destination == NULL) ||
      (destination_size < DISPLAY_RENDERER_BUFFER_SIZE) ||
      (s_display_cursor_base_valid == 0UL))
  {
    return 0UL;
  }

  (void)memcpy(destination,
               s_display_cursor_base_framebuffer,
               DISPLAY_RENDERER_BUFFER_SIZE);
  return DisplayRenderer_ApplyCursorBlinkPhase(
    phase, destination, destination_size);
}

static uint32_t DisplayRenderer_ApplyMultichunkTestPhase(
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  uint16_t row;

  if ((destination == NULL) ||
      (destination_size < DISPLAY_RENDERER_BUFFER_SIZE) ||
      (phase >= 4UL))
  {
    return 0UL;
  }

  for (row = 1U; row <= DISPLAY_HEIGHT; row = (uint16_t)(row + 2U))
  {
    destination[((uint32_t)(row - 1U) * LINE_WIDTH) + phase] ^= 0xFFU;
  }

  return 1UL;
}

static uint32_t DisplayRenderer_ApplyThreePhaseTestPhase(
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  uint16_t row;

  if ((destination == NULL) ||
      (destination_size < DISPLAY_RENDERER_BUFFER_SIZE) ||
      (phase >= 3UL))
  {
    return 0UL;
  }

  for (row = 1U; row <= 8U; ++row)
  {
    uint32_t offset = ((uint32_t)(row - 1U) * LINE_WIDTH) + phase;
    destination[offset] ^= 0xFFU;
  }
  return 1UL;
}

static uint32_t DisplayRenderer_ApplyFullFrameTestPhase(
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  if ((destination == NULL) ||
      (destination_size < DISPLAY_RENDERER_BUFFER_SIZE) ||
      (phase >= 4UL))
  {
    return 0UL;
  }

  if (phase < 2UL)
  {
    (void)memset(destination, (phase == 0UL) ? 0x00 : 0xFF,
                 DISPLAY_RENDERER_BUFFER_SIZE);
  }
  else
  {
    for (uint16_t row = 0U; row < DISPLAY_HEIGHT; ++row)
    {
      (void)memset(&destination[(uint32_t)row * LINE_WIDTH],
                   (phase == 2UL) ?
                     (((row & 1U) == 0U) ? 0xAA : 0x55) :
                     0xF0,
                   LINE_WIDTH);
    }
  }
  return 1UL;
}

static uint32_t DisplayRenderer_ComposeCursorWaitingPhase(
  const void *context,
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  (void)context;
  return DisplayRenderer_ApplyCursorBlinkPhase(
    phase, destination, destination_size);
}

static uint32_t DisplayRenderer_ComposeMultichunkTestPhase(
  const void *context,
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  (void)context;
  return DisplayRenderer_ApplyMultichunkTestPhase(
    phase, destination, destination_size);
}

static uint32_t DisplayRenderer_ComposeThreePhaseTestPhase(
  const void *context,
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  (void)context;
  return DisplayRenderer_ApplyThreePhaseTestPhase(
    phase, destination, destination_size);
}

static uint32_t DisplayRenderer_ComposeFullFrameTestPhase(
  const void *context,
  uint32_t phase,
  uint8_t *destination,
  uint32_t destination_size)
{
  (void)context;
  return DisplayRenderer_ApplyFullFrameTestPhase(
    phase, destination, destination_size);
}

static uint32_t DisplayRenderer_AddWaitingCandidateRow(
  uint16_t row,
  uint16_t *candidate_count)
{
  uint16_t insert_at;
  uint16_t i;

  if ((candidate_count == NULL) || (row < 1U) ||
      (row > DISPLAY_HEIGHT) ||
      (*candidate_count >= DISPLAY_RENDERER_DIRTY_ROW_MAX))
  {
    return 0UL;
  }

  if (s_display_waiting_candidate_row_marks[row - 1U] == 0U)
  {
    insert_at = 0U;
    while ((insert_at < *candidate_count) &&
           (s_display_waiting_candidate_rows[insert_at] < row))
    {
      ++insert_at;
    }
    for (i = *candidate_count; i > insert_at; --i)
    {
      s_display_waiting_candidate_rows[i] =
        s_display_waiting_candidate_rows[i - 1U];
    }
    s_display_waiting_candidate_row_marks[row - 1U] = 1U;
    s_display_waiting_candidate_rows[insert_at] = row;
    (*candidate_count)++;
  }
  return 1UL;
}

uint32_t DisplayRenderer_PrepareCursorBlinkFrame(
  uint32_t visible,
  display_renderer_stats_t *stats)
{
  if (DisplayRenderer_CopyCursorBlinkFrame(
        visible,
        s_display_framebuffer,
        sizeof(s_display_framebuffer)) == 0UL)
  {
    DisplayRenderer_ResetDirtyRows();
    DisplayRenderer_FillStats(stats,
                            DisplayRenderer_CountBlackPixels(),
                            DISPLAY_RENDERER_PRIMITIVE_CURSOR_BLINK,
                            DISPLAY_RENDERER_ROW_NONE,
                            s_display_committed_list_valid == 0UL ?
                              DISPLAY_RENDERER_ROW_NONE :
                              s_display_committed_list.selected_row);
    return 0UL;
  }

  DisplayRenderer_ComputeDirtyRowsFromCommitted();
  DisplayRenderer_FillStats(stats,
                            DisplayRenderer_CountBlackPixels(),
                            DISPLAY_RENDERER_PRIMITIVE_CURSOR_BLINK,
                            DISPLAY_RENDERER_ROW_NONE,
                            s_display_committed_list_valid == 0UL ?
                              DISPLAY_RENDERER_ROW_NONE :
                              s_display_committed_list.selected_row);
  return 1UL;
}

const display_renderer_waiting_animation_t *DisplayRenderer_GetWaitingAnimation(
  uint32_t sequence_start_frame,
  uint32_t next_deadline_tick)
{
  display_renderer_waiting_animation_t *animation =
    &s_display_waiting_animation;
  display_renderer_waiting_element_t *cursor_element;
  display_renderer_waiting_element_t *test_element;
  uint16_t candidate_count = 0U;
  uint32_t frame;
  uint16_t row;

  (void)memset(animation, 0, sizeof(*animation));
  s_display_selected_waiting_animation = animation;
  (void)memset(s_display_waiting_candidate_row_marks, 0,
               sizeof(s_display_waiting_candidate_row_marks));

  if ((s_display_committed_valid == 0UL) ||
      (s_display_cursor_base_valid == 0UL) ||
      (s_lpbam_cursor_panel_region_valid == 0UL) ||
      (s_display_committed_list_valid == 0UL) ||
      (s_lpbam_cursor_panel_region.row_count == 0U) ||
      (s_lpbam_cursor_panel_region.row_count >
       DISPLAY_RENDERER_DIRTY_ROW_MAX))
  {
    return NULL;
  }

  animation->animation_id = DISPLAY_RENDERER_ANIMATION_CURSOR_BLINK;
  animation->source_primitive_id = DISPLAY_RENDERER_PRIMITIVE_CURSOR_BLINK;
  animation->focus_row = s_display_committed_list.selected_row;
  animation->phase_count = 2UL;
  animation->sequence_frame_count = 4UL;
  animation->cadence_ms = (uint32_t)KNOB_DISPLAY_CURSOR_BLINK_PERIOD_MS;
  animation->next_deadline_tick = next_deadline_tick;
  animation->element_count = 1UL;
  animation->panel_bounds = s_lpbam_cursor_panel_region;

  cursor_element = &animation->elements[0];
  cursor_element->element_id = DISPLAY_RENDERER_WAITING_ELEMENT_CURSOR;
  cursor_element->source_primitive_id =
    DISPLAY_RENDERER_PRIMITIVE_CURSOR_BLINK;
  cursor_element->phase_count = 2UL;
  cursor_element->panel_bounds = s_lpbam_cursor_panel_region;
  cursor_element->compose_phase =
    DisplayRenderer_ComposeCursorWaitingPhase;
  cursor_element->compose_context = NULL;
  cursor_element->sequence_phase[0] = 0UL;
  cursor_element->sequence_phase[1] = 1UL;
  cursor_element->sequence_phase[2] = 0UL;
  cursor_element->sequence_phase[3] = 1UL;

  for (row = 0U;
       row < s_lpbam_cursor_panel_region.row_count;
       ++row)
  {
    if (DisplayRenderer_AddWaitingCandidateRow(
          (uint16_t)(s_lpbam_cursor_panel_region.start_row + row),
          &candidate_count) == 0UL)
    {
      return NULL;
    }
  }

  if (g_display_renderer_waiting_test_variant == 1UL)
  {
    animation->animation_id =
      DISPLAY_RENDERER_ANIMATION_COMPOSITE_TEST;
    animation->source_primitive_id = DISPLAY_RENDERER_PRIMITIVE_PATTERN;
    animation->phase_count = 4UL;
    animation->element_count = 2UL;
    animation->panel_bounds.start_row = 1U;
    animation->panel_bounds.row_count = (uint16_t)(DISPLAY_HEIGHT - 1U);
    animation->panel_bounds.start_column = 0U;
    animation->panel_bounds.column_count =
      (uint16_t)(s_lpbam_cursor_panel_region.start_column +
                 s_lpbam_cursor_panel_region.column_count);

    test_element = &animation->elements[1];
    test_element->element_id =
      DISPLAY_RENDERER_WAITING_ELEMENT_MULTICHUNK_TEST;
    test_element->source_primitive_id = DISPLAY_RENDERER_PRIMITIVE_PATTERN;
    test_element->phase_count = 4UL;
    test_element->panel_bounds.start_row = 1U;
    test_element->panel_bounds.row_count =
      (uint16_t)(DISPLAY_HEIGHT - 1U);
    test_element->panel_bounds.start_column = 0U;
    test_element->panel_bounds.column_count = 32U;
    test_element->compose_phase =
      DisplayRenderer_ComposeMultichunkTestPhase;
    test_element->compose_context = NULL;
    test_element->sequence_phase[0] = 0UL;
    test_element->sequence_phase[1] = 1UL;
    test_element->sequence_phase[2] = 2UL;
    test_element->sequence_phase[3] = 3UL;

    for (row = 1U; row <= DISPLAY_HEIGHT; row = (uint16_t)(row + 2U))
    {
      if (DisplayRenderer_AddWaitingCandidateRow(
            row, &candidate_count) == 0UL)
      {
        return NULL;
      }
    }
  }

  if (g_display_renderer_waiting_test_variant == 5UL)
  {
    (void)memset(s_display_waiting_candidate_row_marks, 0,
                 sizeof(s_display_waiting_candidate_row_marks));
    candidate_count = 0U;
    animation->animation_id = DISPLAY_RENDERER_ANIMATION_COMPOSITE_TEST;
    animation->source_primitive_id = DISPLAY_RENDERER_PRIMITIVE_PATTERN;
    animation->phase_count = 6UL;
    animation->sequence_frame_count = 6UL;
    animation->element_count = 2UL;
    animation->panel_bounds.start_row = 1U;
    animation->panel_bounds.row_count = DISPLAY_HEIGHT;
    animation->panel_bounds.start_column = 0U;
    animation->panel_bounds.column_count = DISPLAY_WIDTH;

    cursor_element->sequence_phase[0] = 0UL;
    cursor_element->sequence_phase[1] = 1UL;
    cursor_element->sequence_phase[2] = 0UL;
    cursor_element->sequence_phase[3] = 1UL;
    cursor_element->sequence_phase[4] = 0UL;
    cursor_element->sequence_phase[5] = 1UL;

    test_element = &animation->elements[1];
    test_element->element_id =
      DISPLAY_RENDERER_WAITING_ELEMENT_THREE_PHASE_TEST;
    test_element->source_primitive_id = DISPLAY_RENDERER_PRIMITIVE_PATTERN;
    test_element->phase_count = 3UL;
    test_element->panel_bounds.start_row = 1U;
    test_element->panel_bounds.row_count = 8U;
    test_element->panel_bounds.start_column = 0U;
    test_element->panel_bounds.column_count = 24U;
    test_element->compose_phase = DisplayRenderer_ComposeThreePhaseTestPhase;
    test_element->compose_context = NULL;
    test_element->sequence_phase[0] = 0UL;
    test_element->sequence_phase[1] = 1UL;
    test_element->sequence_phase[2] = 2UL;
    test_element->sequence_phase[3] = 0UL;
    test_element->sequence_phase[4] = 1UL;
    test_element->sequence_phase[5] = 2UL;

    for (row = 1U; row <= 8U; ++row)
    {
      if (DisplayRenderer_AddWaitingCandidateRow(
            row, &candidate_count) == 0UL)
      {
        return NULL;
      }
    }
    for (row = 0U;
         row < s_lpbam_cursor_panel_region.row_count;
         ++row)
    {
      if (DisplayRenderer_AddWaitingCandidateRow(
            (uint16_t)(s_lpbam_cursor_panel_region.start_row + row),
            &candidate_count) == 0UL)
      {
        return NULL;
      }
    }
  }

  if ((g_display_renderer_waiting_test_variant == 2UL) ||
      (g_display_renderer_waiting_test_variant == 3UL) ||
      (g_display_renderer_waiting_test_variant == 4UL) ||
      (g_display_renderer_waiting_test_variant == 6UL))
  {
    animation->animation_id =
      DISPLAY_RENDERER_ANIMATION_FULL_FRAME_TEST;
    animation->source_primitive_id = DISPLAY_RENDERER_PRIMITIVE_PATTERN;
    animation->phase_count =
      (g_display_renderer_waiting_test_variant == 4UL) ? 3UL :
      ((g_display_renderer_waiting_test_variant == 3UL) ? 4UL :
      ((g_display_renderer_waiting_test_variant == 6UL) ? 5UL : 2UL));
    animation->sequence_frame_count =
      (g_display_renderer_waiting_test_variant == 3UL) ? 4UL :
      ((g_display_renderer_waiting_test_variant == 4UL) ? 3UL :
      ((g_display_renderer_waiting_test_variant == 6UL) ? 1UL : 2UL));
    animation->element_count =
      (g_display_renderer_waiting_test_variant == 3UL) ? 2UL : 1UL;
    animation->panel_bounds.start_row = 1U;
    animation->panel_bounds.row_count = DISPLAY_HEIGHT;
    animation->panel_bounds.start_column = 0U;
    animation->panel_bounds.column_count = DISPLAY_WIDTH;

    test_element = &animation->elements[0];
    (void)memset(test_element, 0, sizeof(*test_element));
    test_element->element_id =
      DISPLAY_RENDERER_WAITING_ELEMENT_FULL_FRAME_TEST;
    test_element->source_primitive_id = DISPLAY_RENDERER_PRIMITIVE_PATTERN;
    test_element->phase_count = animation->phase_count;
    test_element->compose_phase =
      DisplayRenderer_ComposeFullFrameTestPhase;
    test_element->compose_context = NULL;
    test_element->sequence_phase[0] = 0UL;
    test_element->sequence_phase[1] = 1UL;
    test_element->sequence_phase[2] = 0UL;
    test_element->sequence_phase[3] = 1UL;
    if (g_display_renderer_waiting_test_variant == 3UL)
    {
      test_element->sequence_phase[2] = 2UL;
      test_element->sequence_phase[3] = 3UL;
    }
    if (g_display_renderer_waiting_test_variant == 4UL)
    {
      test_element->sequence_phase[2] = 2UL;
    }
    test_element->panel_bounds = animation->panel_bounds;

    (void)memset(s_display_waiting_candidate_row_marks, 0,
                 sizeof(s_display_waiting_candidate_row_marks));
    candidate_count = 0U;
    for (row = 1U; row <= DISPLAY_HEIGHT; ++row)
    {
      if (DisplayRenderer_AddWaitingCandidateRow(
            row, &candidate_count) == 0UL)
      {
        return NULL;
      }
    }
    cursor_element = test_element;
    if (g_display_renderer_waiting_test_variant == 3UL)
    {
      cursor_element = &animation->elements[1];
      cursor_element->element_id = DISPLAY_RENDERER_WAITING_ELEMENT_CURSOR;
      cursor_element->source_primitive_id =
        DISPLAY_RENDERER_PRIMITIVE_CURSOR_BLINK;
      cursor_element->phase_count = 2UL;
      cursor_element->panel_bounds = s_lpbam_cursor_panel_region;
      cursor_element->compose_phase =
        DisplayRenderer_ComposeCursorWaitingPhase;
      cursor_element->compose_context = NULL;
      cursor_element->sequence_phase[0] = 0UL;
      cursor_element->sequence_phase[1] = 1UL;
      cursor_element->sequence_phase[2] = 0UL;
      cursor_element->sequence_phase[3] = 1UL;
    }
  }

  animation->candidate_rows = s_display_waiting_candidate_rows;
  animation->candidate_row_count = candidate_count;
  for (frame = 0UL; frame < animation->sequence_frame_count; ++frame)
  {
    animation->sequence_phase[frame] =
      cursor_element->sequence_phase[frame];
  }

  if (sequence_start_frame >= animation->sequence_frame_count)
  {
    sequence_start_frame = 0UL;
  }
  animation->sequence_start_frame = sequence_start_frame;
  animation->current_phase =
    cursor_element->sequence_phase[sequence_start_frame];

  return animation;
}

uint32_t DisplayRenderer_ValidateWaitingAnimation(
  const display_renderer_waiting_animation_t *animation)
{
  uint32_t element_index;
  uint32_t sequence_frame;
  uint16_t candidate_index;
  uint16_t previous_row = 0U;

  if ((animation == NULL) ||
      (animation->phase_count == 0UL) ||
      (animation->phase_count > DISPLAY_RENDERER_WAITING_PHASE_MAX) ||
      (animation->sequence_frame_count == 0UL) ||
      (animation->sequence_frame_count >
       DISPLAY_RENDERER_WAITING_SEQUENCE_MAX) ||
      (animation->sequence_start_frame >=
       animation->sequence_frame_count) ||
      (animation->current_phase >= animation->phase_count) ||
      (animation->cadence_ms == 0UL) ||
      (animation->element_count == 0UL) ||
      (animation->element_count > DISPLAY_RENDERER_WAITING_ELEMENT_MAX) ||
      (animation->candidate_rows == NULL) ||
      (animation->candidate_row_count == 0U) ||
      (animation->candidate_row_count > DISPLAY_RENDERER_DIRTY_ROW_MAX))
  {
    return 0UL;
  }

  for (candidate_index = 0U;
       candidate_index < animation->candidate_row_count;
       ++candidate_index)
  {
    uint16_t row = animation->candidate_rows[candidate_index];
    if ((row < 1U) || (row > DISPLAY_HEIGHT) ||
        ((candidate_index != 0U) && (row <= previous_row)))
    {
      return 0UL;
    }
    previous_row = row;
  }

  for (element_index = 0UL;
       element_index < animation->element_count;
       ++element_index)
  {
    const display_renderer_waiting_element_t *element =
      &animation->elements[element_index];
    uint32_t row_end = (uint32_t)element->panel_bounds.start_row +
                       (uint32_t)element->panel_bounds.row_count - 1UL;
    uint32_t column_end =
      (uint32_t)element->panel_bounds.start_column +
      (uint32_t)element->panel_bounds.column_count;

    if ((element->element_id == 0UL) ||
        (element->phase_count == 0UL) ||
        (element->phase_count >
         DISPLAY_RENDERER_WAITING_ELEMENT_PHASE_MAX) ||
        (element->compose_phase == NULL) ||
        (element->panel_bounds.start_row == 0U) ||
        (element->panel_bounds.row_count == 0U) ||
        (element->panel_bounds.column_count == 0U) ||
        (row_end > DISPLAY_HEIGHT) ||
        (column_end > DISPLAY_WIDTH))
    {
      return 0UL;
    }

    for (sequence_frame = 0UL;
         sequence_frame < animation->sequence_frame_count;
         ++sequence_frame)
    {
      if (element->sequence_phase[sequence_frame] >= element->phase_count)
      {
        return 0UL;
      }
    }
  }

  return 1UL;
}

const display_renderer_waiting_animation_t *
DisplayRenderer_GetGuaranteedWaitingAnimation(
  const display_renderer_waiting_animation_t *preferred)
{
  display_renderer_waiting_animation_t *guaranteed =
    &s_display_waiting_guaranteed_animation;
  uint32_t element_index;
  uint32_t frame;
  uint32_t start_found = 0UL;

  if (DisplayRenderer_ValidateWaitingAnimation(preferred) == 0UL)
  {
    return NULL;
  }

  (void)memcpy(guaranteed, preferred, sizeof(*guaranteed));
  guaranteed->phase_count = DISPLAY_RENDERER_WAITING_GUARANTEED_STEPS;
  guaranteed->sequence_frame_count =
    DISPLAY_RENDERER_WAITING_GUARANTEED_STEPS;

  for (element_index = 0UL;
       element_index < guaranteed->element_count;
       ++element_index)
  {
    display_renderer_waiting_element_t *element =
      &guaranteed->elements[element_index];

    element->sequence_phase[0] = 0UL;
    element->sequence_phase[1] =
      (element->phase_count >= 2UL) ? 1UL : 0UL;
    element->sequence_phase[2] =
      (element->phase_count >= 3UL) ? 2UL : 0UL;
    for (frame = DISPLAY_RENDERER_WAITING_GUARANTEED_STEPS;
         frame < DISPLAY_RENDERER_WAITING_SEQUENCE_MAX;
         ++frame)
    {
      element->sequence_phase[frame] = 0UL;
    }
  }

  for (frame = 0UL;
       frame < DISPLAY_RENDERER_WAITING_GUARANTEED_STEPS;
       ++frame)
  {
    guaranteed->sequence_phase[frame] =
      guaranteed->elements[0].sequence_phase[frame];
    if ((start_found == 0UL) &&
        (guaranteed->sequence_phase[frame] == preferred->current_phase))
    {
      guaranteed->sequence_start_frame = frame;
      start_found = 1UL;
    }
  }
  for (frame = DISPLAY_RENDERER_WAITING_GUARANTEED_STEPS;
       frame < DISPLAY_RENDERER_WAITING_SEQUENCE_MAX;
       ++frame)
  {
    guaranteed->sequence_phase[frame] = 0UL;
  }

  if ((start_found == 0UL) ||
      (DisplayRenderer_ValidateWaitingAnimation(guaranteed) == 0UL))
  {
    (void)memset(guaranteed, 0, sizeof(*guaranteed));
    return NULL;
  }
  return guaranteed;
}

uint32_t DisplayRenderer_SelectWaitingAnimation(
  const display_renderer_waiting_animation_t *animation)
{
  if (DisplayRenderer_ValidateWaitingAnimation(animation) == 0UL)
  {
    return 0UL;
  }
  s_display_selected_waiting_animation = animation;
  return 1UL;
}

const display_renderer_waiting_animation_t *
DisplayRenderer_GetSelectedWaitingAnimation(void)
{
  return (DisplayRenderer_ValidateWaitingAnimation(
            s_display_selected_waiting_animation) != 0UL) ?
         s_display_selected_waiting_animation : NULL;
}

uint32_t DisplayRenderer_ResumePreferredWaitingAnimation(
  uint32_t selected_sequence_frame,
  uint32_t *preferred_sequence_frame,
  uint32_t *preferred_sequence_count)
{
  const display_renderer_waiting_animation_t *selected =
    s_display_selected_waiting_animation;
  const display_renderer_waiting_animation_t *preferred =
    &s_display_waiting_animation;
  uint32_t candidate_frame;
  uint32_t element_index;

  if ((preferred_sequence_frame == NULL) ||
      (preferred_sequence_count == NULL) ||
      (DisplayRenderer_ValidateWaitingAnimation(selected) == 0UL) ||
      (DisplayRenderer_ValidateWaitingAnimation(preferred) == 0UL) ||
      (selected_sequence_frame >= selected->sequence_frame_count) ||
      (selected->element_count != preferred->element_count))
  {
    return 0UL;
  }

  if (selected == preferred)
  {
    *preferred_sequence_frame = selected_sequence_frame;
    *preferred_sequence_count = preferred->sequence_frame_count;
    return 1UL;
  }

  for (candidate_frame = 0UL;
       candidate_frame < preferred->sequence_frame_count;
       ++candidate_frame)
  {
    uint32_t matches = 1UL;

    for (element_index = 0UL;
         element_index < preferred->element_count;
         ++element_index)
    {
      if ((selected->elements[element_index].element_id !=
           preferred->elements[element_index].element_id) ||
          (selected->elements[element_index].sequence_phase[
             selected_sequence_frame] !=
           preferred->elements[element_index].sequence_phase[
             candidate_frame]))
      {
        matches = 0UL;
        break;
      }
    }
    if (matches != 0UL)
    {
      s_display_selected_waiting_animation = preferred;
      *preferred_sequence_frame = candidate_frame;
      *preferred_sequence_count = preferred->sequence_frame_count;
      return 1UL;
    }
  }

  return 0UL;
}

uint32_t DisplayRenderer_CopyWaitingAnimationFrame(
  const display_renderer_waiting_animation_t *animation,
  uint32_t sequence_frame,
  uint8_t *destination,
  uint32_t destination_size)
{
  uint32_t element_index;
  uint32_t phase;

  if ((DisplayRenderer_ValidateWaitingAnimation(animation) == 0UL) ||
      (sequence_frame >= animation->sequence_frame_count) ||
      (destination == NULL) ||
      (destination_size < DISPLAY_RENDERER_BUFFER_SIZE) ||
      (s_display_cursor_base_valid == 0UL))
  {
    return 0UL;
  }

  (void)memcpy(destination,
               s_display_cursor_base_framebuffer,
               DISPLAY_RENDERER_BUFFER_SIZE);
  for (element_index = 0UL;
       element_index < animation->element_count;
       ++element_index)
  {
    const display_renderer_waiting_element_t *element =
      &animation->elements[element_index];

    phase = element->sequence_phase[sequence_frame];
    if ((element->phase_count == 0UL) ||
        (element->phase_count >
         DISPLAY_RENDERER_WAITING_ELEMENT_PHASE_MAX) ||
        (phase >= element->phase_count) ||
        (element->compose_phase == NULL))
    {
      return 0UL;
    }

    if (element->compose_phase(element->compose_context,
                               phase,
                               destination,
                               destination_size) == 0UL)
    {
      return 0UL;
    }
  }

  return 1UL;
}

uint32_t DisplayRenderer_PrepareWaitingAnimationFrame(
  uint32_t sequence_frame,
  display_renderer_stats_t *stats)
{
  const display_renderer_waiting_animation_t *animation =
    s_display_selected_waiting_animation;

  if (DisplayRenderer_CopyWaitingAnimationFrame(
        animation,
        sequence_frame,
        s_display_framebuffer,
        sizeof(s_display_framebuffer)) == 0UL)
  {
    DisplayRenderer_ResetDirtyRows();
    return 0UL;
  }

  DisplayRenderer_ComputeDirtyRowsFromCommitted();
  DisplayRenderer_FillStats(
    stats,
    DisplayRenderer_CountBlackPixels(),
    animation->source_primitive_id,
    DISPLAY_RENDERER_ROW_NONE,
    s_display_committed_list_valid == 0UL ?
      DISPLAY_RENDERER_ROW_NONE : s_display_committed_list.selected_row);
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

static uint32_t DisplayRenderer_TextEquals(const char *left,
                                           const char *right)
{
  if (left == right)
  {
    return 1UL;
  }
  if ((left == NULL) || (right == NULL))
  {
    return 0UL;
  }
  return (strcmp(left, right) == 0) ? 1UL : 0UL;
}

static uint32_t DisplayRenderer_ListVisualContentMatches(
  const display_renderer_list_t *left,
  const display_renderer_list_t *right)
{
  uint32_t row;

  if ((left == NULL) || (right == NULL))
  {
    return 0UL;
  }
  if (DisplayRenderer_TextEquals(left->title, right->title) == 0UL)
  {
    return 0UL;
  }

  for (row = 0U; row < DISPLAY_RENDERER_LIST_ROW_COUNT; ++row)
  {
    if (DisplayRenderer_TextEquals(left->rows[row], right->rows[row]) == 0UL)
    {
      return 0UL;
    }
  }
  return 1UL;
}

static void DisplayRenderer_SetPendingList(const display_renderer_list_t *list)
{
  if (list != NULL)
  {
    s_display_pending_list = *list;
    s_display_pending_list_valid = 1UL;
    s_display_pending_list_invalidates = 0UL;
  }
}

static uint32_t DisplayRenderer_ListFocusPrimitiveEligible(
  const display_renderer_list_t *list)
{
  if ((list == NULL) || (s_display_committed_valid == 0UL) ||
      (s_display_committed_list_valid == 0UL) ||
      (s_display_cursor_base_valid == 0UL))
  {
    return 0UL;
  }
  if (DisplayRenderer_ListVisualContentMatches(
        &s_display_committed_list, list) == 0UL)
  {
    return 0UL;
  }
  if (s_display_committed_list.selected_row == list->selected_row)
  {
    return 0UL;
  }
  if ((s_display_committed_list.selected_row >=
       DISPLAY_RENDERER_LIST_ROW_COUNT) ||
      (list->selected_row >= DISPLAY_RENDERER_LIST_ROW_COUNT))
  {
    return 0UL;
  }
  return 1UL;
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
  uint32_t primitive_id = DISPLAY_RENDERER_PRIMITIVE_LIST_FULL;
  uint32_t previous_focus_row = DISPLAY_RENDERER_ROW_NONE;

  DisplayRenderer_UIList(page,
                         calibration_page,
                         focus_index,
                         shutdown_state,
                         shutdown_countdown_seconds,
                         &list);

  s_rotate_ccw = 1UL;
  if (DisplayRenderer_ListFocusPrimitiveEligible(&list) != 0UL)
  {
    primitive_id = DISPLAY_RENDERER_PRIMITIVE_LIST_FOCUS;
    previous_focus_row = s_display_committed_list.selected_row;
    (void)memcpy(s_display_framebuffer,
                 s_display_cursor_base_framebuffer,
                 sizeof(s_display_framebuffer));
    DisplayRenderer_ClearListCursor(previous_focus_row);
    (void)DisplayRenderer_DrawListCursor(list.selected_row, 1UL);
    DisplayRenderer_RecordCursorBaseFrame();
    DisplayRenderer_ComputeDirtyRowsFromCommitted();
    black_pixels = DisplayRenderer_CountBlackPixels();
  }
  else
  {
    DisplayRenderer_ClearWhite();
    s_rotate_ccw = 1UL;
    black_pixels += DisplayRenderer_DrawList(&list);
    DisplayRenderer_RecordCursorBaseFrame();
    DisplayRenderer_ComputeDirtyRowsFromCommitted();
  }
  s_rotate_ccw = 0UL;
  DisplayRenderer_SetPendingList(&list);

  DisplayRenderer_FillStats(stats,
                            black_pixels,
                            primitive_id,
                            previous_focus_row,
                            list.selected_row);
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
  DisplayRenderer_FillStats(stats,
                            black_pixels,
                            DISPLAY_RENDERER_PRIMITIVE_PATTERN,
                            DISPLAY_RENDERER_ROW_NONE,
                            DISPLAY_RENDERER_ROW_NONE);
}
