#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ps_hw6_trace.h"

typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { void *Bus; } LS013B7DH05;
#define DISPLAY_HEIGHT 6U
#define LINE_WIDTH 2U
#define LCD_DMA_MAX_ROWS_PER_TRANSFER 2U
#define DISPLAY_RENDERER_DIRTY_ROW_MAX DISPLAY_HEIGHT
#define TXBUF_MAX (2U + DISPLAY_HEIGHT * (LINE_WIDTH + 2U))
#define MLCD_CMD_WRITE 1U
#define PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS 250U
#define PS_HW6_DISPLAY_DRIVER_STATE_FAULT 1U
#define PS_HW6_DISPLAY_DRIVER_STATE_HOLD 2U
static uint8_t txBuf[TXBUF_MAX], framebuffer[DISPLAY_HEIGHT * LINE_WIDTH];
static uint16_t dirty_rows[] = {1, 2, 6};
static uint32_t dirty_count, commits, starts, tick, wait_reads;
static HAL_StatusTypeDef start_status, wait_status;
static bool g_dma_done;
static struct { HAL_StatusTypeDef last; } g_chain;
static uint32_t records[96][3], record_count;
static uint32_t ps_hw6_display_driver_initialized, ps_hw6_display_driver_state;
static uint32_t ps_hw6_display_driver_last_status, ps_hw6_display_driver_operation_count;
static uint32_t hspi3;
static LS013B7DH05 ps_hw6_display;

void PS_HW6_TraceObjectPanel(uint32_t stage, uint32_t end, uint32_t value)
{
  assert(record_count < 96);
  records[record_count][0] = stage;
  records[record_count][1] = end;
  records[record_count++][2] = value;
}

static uint32_t HAL_GetTick(void)
{
  wait_reads++;
  if ((wait_status != HAL_TIMEOUT) && (wait_reads == 3)) { g_dma_done = true; }
  return tick++;
}

static HAL_StatusTypeDef lcd_dma_start(LS013B7DH05 *dev, const uint8_t *buf, uint32_t len)
{
  assert(dev == &ps_hw6_display && buf == txBuf);
  assert(len == 6 || len == 10);
  assert(buf[0] == MLCD_CMD_WRITE && buf[len - 1] == 0);
  for (uint32_t offset = 1; offset < len - 1; offset += LINE_WIDTH + 2)
  {
    uint32_t row = buf[offset];
    assert(row >= 1 && row <= DISPLAY_HEIGHT);
    assert(memcmp(buf + offset + 1, framebuffer + (row - 1) * LINE_WIDTH, LINE_WIDTH) == 0);
    assert(buf[offset + LINE_WIDTH + 1] == 0);
  }
  starts++;
  g_chain.last = wait_status;
  wait_reads = 0;
  g_dma_done = false;
  return start_status;
}

static uint32_t DisplayRenderer_GetDirtyRows(const uint16_t **rows)
{ *rows = dirty_rows; return dirty_count; }
static const uint8_t *DisplayRenderer_GetBuffer(void) { return framebuffer; }
static void DisplayRenderer_CommitPresentedFrame(void) { commits++; }
#include "panel_under_test.inc"

static void reset(void)
{
  record_count = 0; commits = 0; starts = 0; tick = 0;
  start_status = HAL_OK; wait_status = HAL_OK; g_dma_done = true;
  ps_hw6_display_driver_initialized = 1;
  ps_hw6_display.Bus = &hspi3;
  dirty_count = 3; dirty_rows[0] = 1;
  for (uint32_t index = 0; index < sizeof(framebuffer); ++index) { framebuffer[index] = (uint8_t)index; }
}

static uint32_t pairs(uint32_t stage, uint32_t expected_status)
{
  uint32_t open = 0, count = 0;
  for (uint32_t index = 0; index < record_count; ++index)
  {
    if (records[index][0] != stage) { continue; }
    if (records[index][1] == 0) { assert(!open); open = 1; }
    else { assert(open && records[index][2] == expected_status); open = 0; count++; }
  }
  assert(!open);
  return count;
}

int main(void)
{
  uint32_t init, result;
  reset();
  assert(PS_HW6_DisplayOwner_PresentRendererRows(&init, &result) == HAL_OK);
  assert(init == HAL_OK && result == HAL_OK && starts == 2 && commits == 1);
  assert(records[0][0] == PS_TRACE_PANEL_TRANSFER && records[0][2] == 3);
  assert(pairs(PS_TRACE_PANEL_TRANSFER, HAL_OK) == 1);
  assert(pairs(PS_TRACE_PANEL_WIRE, HAL_OK) == 2);
  assert(pairs(PS_TRACE_PANEL_DMA_START, HAL_OK) == 2);
  assert(pairs(PS_TRACE_PANEL_DMA_WAIT, HAL_OK) == 2);

  reset(); dirty_count = 0;
  assert(PS_HW6_DisplayOwner_PresentRendererRows(&init, &result) == HAL_OK);
  assert(record_count == 0 && starts == 0 && commits == 1);

  reset(); dirty_rows[0] = 0;
  assert(PS_HW6_DisplayOwner_PresentRendererRows(&init, &result) == HAL_ERROR);
  assert(starts == 0 && commits == 0);
  assert(pairs(PS_TRACE_PANEL_WIRE, HAL_ERROR) == 1);
  assert(pairs(PS_TRACE_PANEL_TRANSFER, HAL_ERROR) == 1);
  assert(pairs(PS_TRACE_PANEL_DMA_START, HAL_OK) == 0);

  reset(); start_status = HAL_BUSY;
  assert(PS_HW6_DisplayOwner_PresentRendererRows(&init, &result) == HAL_BUSY);
  assert(starts == 1 && commits == 0);
  assert(pairs(PS_TRACE_PANEL_DMA_START, HAL_BUSY) == 1);
  assert(pairs(PS_TRACE_PANEL_DMA_WAIT, HAL_OK) == 0);
  assert(pairs(PS_TRACE_PANEL_TRANSFER, HAL_BUSY) == 1);

  reset(); wait_status = HAL_TIMEOUT;
  assert(PS_HW6_DisplayOwner_PresentRendererRows(&init, &result) == HAL_TIMEOUT);
  assert(starts == 1 && commits == 0 && tick > PS_HW6_DISPLAY_PRESENT_TIMEOUT_MS);
  assert(pairs(PS_TRACE_PANEL_DMA_WAIT, HAL_TIMEOUT) == 1);
  assert(pairs(PS_TRACE_PANEL_TRANSFER, HAL_TIMEOUT) == 1);

  reset(); wait_status = HAL_ERROR;
  assert(PS_HW6_DisplayOwner_PresentRendererRows(&init, &result) == HAL_ERROR);
  assert(starts == 1 && commits == 0);
  assert(pairs(PS_TRACE_PANEL_DMA_WAIT, HAL_ERROR) == 1);
  assert(pairs(PS_TRACE_PANEL_TRANSFER, HAL_ERROR) == 1);
  return 0;
}
