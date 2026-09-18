#include <assert.h>
#include <stdint.h>
#include <string.h>

#define DISPLAY_HEIGHT 168U
#define LINE_WIDTH 18U
#define DISPLAY_RENDERER_DIRTY_ROW_MAX DISPLAY_HEIGHT
#define PS_TRACE_PANEL_DIRTY_ROWS 6U
static uint16_t s_display_dirty_rows[DISPLAY_HEIGHT];
static uint8_t s_display_dirty_row_marks[DISPLAY_HEIGHT];
static uint16_t s_display_dirty_row_count;
static uint32_t s_display_committed_valid;
static uint8_t s_display_framebuffer[DISPLAY_HEIGHT * LINE_WIDTH];
static uint8_t s_display_committed_framebuffer[DISPLAY_HEIGHT * LINE_WIDTH];
static uint8_t expected[DISPLAY_HEIGHT];

static void PS_HW6_TraceObjectPanel(uint32_t stage, uint32_t end, uint32_t value)
{ (void)stage; (void)end; (void)value; }

#include "dirty_rows_under_test.inc"

static void check(void)
{
  uint16_t count = 0U;
  for (uint16_t row = 0U; row < DISPLAY_HEIGHT; ++row)
  {
    assert(s_display_dirty_row_marks[row] == expected[row]);
    if (expected[row] != 0U)
    {
      assert(s_display_dirty_rows[count++] == row + 1U);
    }
  }
  assert(s_display_dirty_row_count == count);
  for (; count < DISPLAY_HEIGHT; ++count)
  {
    assert(s_display_dirty_rows[count] == 0U);
  }
}

int main(void)
{
  uint8_t pixels_before[sizeof(s_display_framebuffer)];
  uint8_t committed_before[sizeof(s_display_committed_framebuffer)];
  for (uint16_t mode = 0U; mode < 3U; ++mode)
  {
    DisplayRenderer_ResetDirtyRows();
    memset(expected, 0, sizeof(expected));
    check();
    DisplayRenderer_MarkPanelRowDirty(DISPLAY_HEIGHT);
    DisplayRenderer_MarkPanelRowDirty(UINT16_MAX);
    check();
    for (uint16_t index = 0U; index < DISPLAY_HEIGHT; ++index)
    {
      uint16_t row = (mode == 0U) ? index : (mode == 1U) ?
        (uint16_t)(DISPLAY_HEIGHT - 1U - index) :
        (uint16_t)((index * 17U) % DISPLAY_HEIGHT);
      expected[row] = 1U;
      DisplayRenderer_MarkPanelRowDirty(row);
      check();
      DisplayRenderer_MarkPanelRowDirty(row);
      check();
    }
    DisplayRenderer_MarkAllRowsDirty();
    check();
  }

  /* Exercise the capacity guard even when the requested row is unmarked. */
  s_display_dirty_row_marks[0] = 0U;
  DisplayRenderer_MarkPanelRowDirty(0U);
  assert(s_display_dirty_row_count == DISPLAY_HEIGHT);
  assert(s_display_dirty_row_marks[0] == 0U);
  s_display_dirty_row_marks[0] = 1U;
  check();

  for (uint16_t row = 0U; row < DISPLAY_HEIGHT; ++row)
  {
    for (uint16_t col = 0U; col < LINE_WIDTH; ++col)
    {
      s_display_committed_framebuffer[row * LINE_WIDTH + col] = (uint8_t)(row + col);
    }
  }
  memcpy(s_display_framebuffer, s_display_committed_framebuffer, sizeof(s_display_framebuffer));
  s_display_committed_valid = 1U;
  memset(expected, 0, sizeof(expected));
  DisplayRenderer_ComputeDirtyRowsFromCommitted();
  check();
  for (uint16_t row = 0U; row < DISPLAY_HEIGHT; row += 3U)
  {
    expected[row] = 1U;
    s_display_framebuffer[row * LINE_WIDTH + row % LINE_WIDTH] ^= 0x80U;
  }
  memcpy(pixels_before, s_display_framebuffer, sizeof(pixels_before));
  memcpy(committed_before, s_display_committed_framebuffer, sizeof(committed_before));
  DisplayRenderer_ComputeDirtyRowsFromCommitted();
  check();
  s_display_committed_valid = 0U;
  memset(expected, 1, sizeof(expected));
  DisplayRenderer_ComputeDirtyRowsFromCommitted();
  check();
  assert(memcmp(pixels_before, s_display_framebuffer, sizeof(pixels_before)) == 0);
  assert(memcmp(committed_before, s_display_committed_framebuffer, sizeof(committed_before)) == 0);
  return 0;
}
