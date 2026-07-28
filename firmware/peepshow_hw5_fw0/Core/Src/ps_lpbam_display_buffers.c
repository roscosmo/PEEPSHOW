#include "ps_lpbam_display_buffers.h"
#include <string.h>

#define PS_LPBAM_MLCD_CMD_WRITE 0x01U
#define PS_LPBAM_ROW_WIRE_BYTES (1U + LINE_WIDTH + 1U)
#define PS_LPBAM_DISPLAY_ARENA_SIZE 9216U

#if defined(__GNUC__)
#define PS_SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
#define PS_SRAM4_BUF_ATTR
#endif

uint8_t ps_lpbam_display_frames[PS_LPBAM_DISPLAY_FRAME_COUNT][DISPLAY_HEIGHT][LINE_WIDTH];
uint8_t ps_lpbam_display_frame_a[DISPLAY_HEIGHT][LINE_WIDTH];
uint8_t ps_lpbam_display_frame_b[DISPLAY_HEIGHT][LINE_WIDTH];
uint8_t *ps_lpbam_display_tx[PS_LPBAM_DISPLAY_FRAME_COUNT][PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS];
uint16_t ps_lpbam_display_tx_len[PS_LPBAM_DISPLAY_FRAME_COUNT][PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS] PS_SRAM4_BUF_ATTR;
uint16_t ps_lpbam_display_frame_len PS_SRAM4_BUF_ATTR;
uint16_t ps_lpbam_display_active_frame_count PS_SRAM4_BUF_ATTR;
uint16_t ps_lpbam_display_active_chunk_count[PS_LPBAM_DISPLAY_FRAME_COUNT] PS_SRAM4_BUF_ATTR;
uint16_t ps_lpbam_display_dirty_start_row[PS_LPBAM_DISPLAY_FRAME_COUNT] PS_SRAM4_BUF_ATTR;
uint16_t ps_lpbam_display_dirty_row_count[PS_LPBAM_DISPLAY_FRAME_COUNT] PS_SRAM4_BUF_ATTR;
uint16_t ps_lpbam_display_sequence_start_frame PS_SRAM4_BUF_ATTR;
uint16_t ps_lpbam_display_queue_start_slot PS_SRAM4_BUF_ATTR;
static uint8_t ps_lpbam_display_payload_arena[PS_LPBAM_DISPLAY_ARENA_SIZE] PS_SRAM4_BUF_ATTR;
static uint16_t ps_lpbam_display_payload_used PS_SRAM4_BUF_ATTR;
static uint8_t ps_lpbam_display_experiment_variant;

#if defined(__GNUC__)
__attribute__((weak))
#endif
void PS_LpbamDisplay_ComposeExperimentFrames(void)
{
  memset(ps_lpbam_display_frames[0], 0x00, sizeof(ps_lpbam_display_frames[0]));
  memset(ps_lpbam_display_frames[1], 0xFF, sizeof(ps_lpbam_display_frames[1]));
  memset(ps_lpbam_display_frames[2], 0x00, sizeof(ps_lpbam_display_frames[2]));
  memset(ps_lpbam_display_frames[3], 0xFF, sizeof(ps_lpbam_display_frames[3]));
  memcpy(ps_lpbam_display_frame_a, ps_lpbam_display_frames[0], sizeof(ps_lpbam_display_frame_a));
  memcpy(ps_lpbam_display_frame_b, ps_lpbam_display_frames[1], sizeof(ps_lpbam_display_frame_b));
}

void PS_LpbamDisplay_SetExperimentVariant(uint8_t variant)
{
  ps_lpbam_display_experiment_variant = variant;
}

uint8_t PS_LpbamDisplay_GetExperimentVariant(void)
{
  return ps_lpbam_display_experiment_variant;
}

void PS_LpbamDisplay_SetSequenceStartFrame(uint16_t frame_index)
{
  ps_lpbam_display_sequence_start_frame = (uint16_t)(frame_index % PS_LPBAM_DISPLAY_FRAME_COUNT);
}

void PS_LpbamDisplay_SetQueueStartSlot(uint16_t frame_slot)
{
  ps_lpbam_display_queue_start_slot = (uint16_t)(frame_slot % PS_LPBAM_DISPLAY_FRAME_COUNT);
}

static uint8_t PS_LpbamDisplay_RowIsDirty(const uint8_t previous_frame[DISPLAY_HEIGHT][LINE_WIDTH],
                                          const uint8_t target_frame[DISPLAY_HEIGHT][LINE_WIDTH],
                                          uint16_t row)
{
#if PS_LPBAM_DISPLAY_UPDATE_MODE == PS_LPBAM_DISPLAY_UPDATE_MODE_PARTIAL_DIFF
  return (memcmp(previous_frame[row - 1U], target_frame[row - 1U], LINE_WIDTH) != 0) ? 1U : 0U;
#else
  (void)previous_frame;
  (void)target_frame;
  (void)row;
  return 1U;
#endif
}

static HAL_StatusTypeDef PS_LpbamDisplay_AllocPayload(uint16_t max_len, uint8_t **out_payload)
{
  if ((out_payload == NULL) || (max_len == 0U))
  {
    return HAL_ERROR;
  }

  uint16_t aligned_used = (uint16_t)((ps_lpbam_display_payload_used + 3U) & ~3U);
  if (((uint32_t)aligned_used + (uint32_t)max_len) > PS_LPBAM_DISPLAY_ARENA_SIZE)
  {
    return HAL_ERROR;
  }

  *out_payload = &ps_lpbam_display_payload_arena[aligned_used];
  ps_lpbam_display_payload_used = (uint16_t)(aligned_used + max_len);
  return HAL_OK;
}

static void PS_LpbamDisplay_AppendWireRow(uint8_t **write,
                                          uint16_t row,
                                          const uint8_t frame[DISPLAY_HEIGHT][LINE_WIDTH])
{
  uint8_t *w = *write;
  *w++ = (uint8_t)row;
  memcpy(w, frame[row - 1U], LINE_WIDTH);
  w += LINE_WIDTH;
  *w++ = 0x00U;
  *write = w;
}

static HAL_StatusTypeDef PS_LpbamDisplay_FinalizePayload(uint8_t *payload,
                                                         uint8_t *write,
                                                         uint16_t row_guard,
                                                         const uint8_t frame[DISPLAY_HEIGHT][LINE_WIDTH],
                                                         uint16_t *out_len)
{
  if ((payload == NULL) || (write == NULL) || (frame == NULL) || (out_len == NULL) || (row_guard == 0U))
  {
    return HAL_ERROR;
  }

  PS_LpbamDisplay_AppendWireRow(&write, row_guard, frame);
  *write++ = 0x00U;
  *write++ = 0x00U;
  *out_len = (uint16_t)(write - payload);

  return ((*out_len) <= PS_LPBAM_DISPLAY_TX_MAX_LEN) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef PS_LpbamDisplay_BuildPatternBuffers(uint16_t start_row,
                                                      uint16_t row_count)
{
  if ((start_row < 1U) || (row_count == 0U) ||
      (((uint32_t)start_row + (uint32_t)row_count - 1U) > DISPLAY_HEIGHT))
  {
    return HAL_ERROR;
  }

  memset(ps_lpbam_display_tx, 0, sizeof(ps_lpbam_display_tx));
  memset(ps_lpbam_display_tx_len, 0, sizeof(ps_lpbam_display_tx_len));
  memset(ps_lpbam_display_active_chunk_count, 0, sizeof(ps_lpbam_display_active_chunk_count));
  memset(ps_lpbam_display_dirty_start_row, 0, sizeof(ps_lpbam_display_dirty_start_row));
  memset(ps_lpbam_display_dirty_row_count, 0, sizeof(ps_lpbam_display_dirty_row_count));
  memset(ps_lpbam_display_payload_arena, 0, sizeof(ps_lpbam_display_payload_arena));
  ps_lpbam_display_payload_used = 0U;
  ps_lpbam_display_active_frame_count = 0U;
  ps_lpbam_display_frame_len = 0U;
  ps_lpbam_display_queue_start_slot = 0U;

  PS_LpbamDisplay_ComposeExperimentFrames();

  uint16_t final_row = (uint16_t)((uint32_t)start_row + (uint32_t)row_count - 1U);

  for (uint16_t frame = 0U; frame < PS_LPBAM_DISPLAY_FRAME_COUNT; frame++)
  {
    uint16_t previous = (uint16_t)((ps_lpbam_display_sequence_start_frame + frame) % PS_LPBAM_DISPLAY_FRAME_COUNT);
    uint16_t target = (uint16_t)((previous + 1U) % PS_LPBAM_DISPLAY_FRAME_COUNT);
    uint8_t dirty_row_list[DISPLAY_HEIGHT] = {0};
    uint16_t dirty_rows = 0U;

    for (uint16_t row = start_row; row <= final_row; row++)
    {
      if (PS_LpbamDisplay_RowIsDirty(ps_lpbam_display_frames[previous], ps_lpbam_display_frames[target], row) != 0U)
      {
        dirty_row_list[dirty_rows] = (uint8_t)row;
        dirty_rows++;
      }
    }

    if (dirty_rows == 0U)
    {
      dirty_row_list[0] = (uint8_t)start_row;
      dirty_rows = 1U;
    }

    uint16_t chunk_count = 0U;
    uint16_t dirty_index = 0U;
    while (dirty_index < dirty_rows)
    {
      if (chunk_count >= PS_LPBAM_DISPLAY_SEQUENCE_CHUNKS)
      {
        return HAL_ERROR;
      }

      uint16_t rows_this_payload = (uint16_t)(dirty_rows - dirty_index);
      if (rows_this_payload > PS_LPBAM_DISPLAY_ROWS)
      {
        rows_this_payload = PS_LPBAM_DISPLAY_ROWS;
      }

      uint16_t payload_max_len = (uint16_t)(1U + (((uint32_t)rows_this_payload + 1U) * PS_LPBAM_ROW_WIRE_BYTES) + 2U);
      uint8_t *payload = NULL;
      if (PS_LpbamDisplay_AllocPayload(payload_max_len, &payload) != HAL_OK)
      {
        return HAL_ERROR;
      }

      uint8_t *write = payload;
      *write++ = PS_LPBAM_MLCD_CMD_WRITE;

      for (uint16_t i = 0U; i < rows_this_payload; i++)
      {
        uint16_t row = dirty_row_list[dirty_index + i];
        PS_LpbamDisplay_AppendWireRow(&write, row, ps_lpbam_display_frames[target]);
      }

      uint16_t last_payload_row = dirty_row_list[(uint16_t)(dirty_index + rows_this_payload - 1U)];
      uint16_t len = 0U;
      if (PS_LpbamDisplay_FinalizePayload(payload, write, last_payload_row, ps_lpbam_display_frames[target], &len) != HAL_OK)
      {
        return HAL_ERROR;
      }

      ps_lpbam_display_tx[frame][chunk_count] = payload;
      ps_lpbam_display_tx_len[frame][chunk_count] = len;
      ps_lpbam_display_frame_len = (uint16_t)(ps_lpbam_display_frame_len + len);
      dirty_index = (uint16_t)(dirty_index + rows_this_payload);
      chunk_count++;
    }

    ps_lpbam_display_active_chunk_count[frame] = chunk_count;
    ps_lpbam_display_dirty_start_row[frame] = dirty_row_list[0];
    ps_lpbam_display_dirty_row_count[frame] = dirty_rows;
    ps_lpbam_display_active_frame_count++;
  }

  memcpy(ps_lpbam_display_frame_a, ps_lpbam_display_frames[0], sizeof(ps_lpbam_display_frame_a));
  memcpy(ps_lpbam_display_frame_b, ps_lpbam_display_frames[1], sizeof(ps_lpbam_display_frame_b));

  return (ps_lpbam_display_active_frame_count == PS_LPBAM_DISPLAY_FRAME_COUNT) ? HAL_OK : HAL_ERROR;
}