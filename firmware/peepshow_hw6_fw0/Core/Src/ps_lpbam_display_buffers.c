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
uint8_t *ps_lpbam_display_tx[PS_LPBAM_DISPLAY_MAX_CHUNKS];
uint16_t ps_lpbam_display_tx_len[PS_LPBAM_DISPLAY_MAX_CHUNKS];
ps_lpbam_display_sequence_entry_t
  ps_lpbam_display_sequence[PS_LPBAM_DISPLAY_SEQUENCE_MAX];
ps_lpbam_display_admission_t ps_lpbam_display_admission;
uint16_t ps_lpbam_display_active_sequence_count;
uint16_t ps_lpbam_display_active_chunk_count;
uint16_t ps_lpbam_display_payload_wire_bytes;
uint16_t ps_lpbam_display_sequence_start_frame;
uint16_t ps_lpbam_display_queue_start_slot;
static uint8_t ps_lpbam_display_payload_arena[PS_LPBAM_DISPLAY_ARENA_SIZE] PS_SRAM4_BUF_ATTR;
static uint16_t ps_lpbam_display_payload_used PS_SRAM4_BUF_ATTR;
static uint8_t ps_lpbam_display_experiment_variant;
static uint16_t ps_lpbam_display_candidate_rows[DISPLAY_HEIGHT];
static uint8_t ps_lpbam_display_candidate_row_enabled[DISPLAY_HEIGHT];
static uint8_t ps_lpbam_display_dirty_row_list[DISPLAY_HEIGHT];
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

static uint8_t PS_LpbamDisplay_RowIsDirty(
  const uint8_t previous_frame[DISPLAY_HEIGHT][LINE_WIDTH],
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

static HAL_StatusTypeDef PS_LpbamDisplay_AllocPayload(uint16_t max_len,
                                                       uint8_t **out_payload)
{
  uint16_t aligned_used;

  if ((out_payload == NULL) || (max_len == 0U))
  {
    return HAL_ERROR;
  }

  aligned_used = (uint16_t)((ps_lpbam_display_payload_used + 3U) & ~3U);
  if (((uint32_t)aligned_used + (uint32_t)max_len) > PS_LPBAM_DISPLAY_ARENA_SIZE)
  {
    return HAL_ERROR;
  }

  *out_payload = &ps_lpbam_display_payload_arena[aligned_used];
  ps_lpbam_display_payload_used = (uint16_t)(aligned_used + max_len);
  return HAL_OK;
}

static void PS_LpbamDisplay_AppendWireRow(
  uint8_t **write,
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

static HAL_StatusTypeDef PS_LpbamDisplay_FinalizePayload(
  uint8_t *payload,
  uint8_t *write,
  uint16_t row_guard,
  const uint8_t frame[DISPLAY_HEIGHT][LINE_WIDTH],
  uint16_t *out_len)
{
  if ((payload == NULL) || (write == NULL) || (frame == NULL) ||
      (out_len == NULL) || (row_guard == 0U))
  {
    return HAL_ERROR;
  }

  PS_LpbamDisplay_AppendWireRow(&write, row_guard, frame);
  *write++ = 0x00U;
  *write++ = 0x00U;
  *out_len = (uint16_t)(write - payload);

  return ((*out_len) <= PS_LPBAM_DISPLAY_TX_MAX_LEN) ? HAL_OK : HAL_ERROR;
}

static void PS_LpbamDisplay_ResetPayloadState(void)
{
  memset(ps_lpbam_display_tx, 0, sizeof(ps_lpbam_display_tx));
  memset(ps_lpbam_display_tx_len, 0, sizeof(ps_lpbam_display_tx_len));
  memset(ps_lpbam_display_sequence, 0,
         sizeof(ps_lpbam_display_sequence));
  memset(&ps_lpbam_display_admission, 0,
         sizeof(ps_lpbam_display_admission));
  memset(ps_lpbam_display_payload_arena, 0,
         sizeof(ps_lpbam_display_payload_arena));
  ps_lpbam_display_payload_used = 0U;
  ps_lpbam_display_active_sequence_count = 0U;
  ps_lpbam_display_active_chunk_count = 0U;
  ps_lpbam_display_payload_wire_bytes = 0U;
  ps_lpbam_display_queue_start_slot = 0U;
  ps_lpbam_display_admission.sequence_capacity =
    PS_LPBAM_DISPLAY_SEQUENCE_MAX;
  ps_lpbam_display_admission.chunk_capacity =
    PS_LPBAM_DISPLAY_MAX_CHUNKS;
  ps_lpbam_display_admission.payload_capacity_bytes =
    PS_LPBAM_DISPLAY_ARENA_SIZE;
  ps_lpbam_display_admission.api_version =
    PS_LPBAM_DISPLAY_ADMISSION_API_VERSION;
  ps_lpbam_display_admission.status = (uint32_t)HAL_ERROR;
  ps_lpbam_display_admission.reason = PS_LPBAM_ADMISSION_REASON_NONE;
}

static HAL_StatusTypeDef PS_LpbamDisplay_AdmissionFailure(uint32_t reason)
{
  ps_lpbam_display_admission.sequence_used =
    ps_lpbam_display_active_sequence_count;
  ps_lpbam_display_admission.chunk_used =
    ps_lpbam_display_active_chunk_count;
  ps_lpbam_display_admission.payload_wire_bytes =
    ps_lpbam_display_payload_wire_bytes;
  ps_lpbam_display_admission.payload_used_bytes =
    ps_lpbam_display_payload_used;
  ps_lpbam_display_admission.status = (uint32_t)HAL_ERROR;
  ps_lpbam_display_admission.reason = reason;
  return HAL_ERROR;
}

static HAL_StatusTypeDef PS_LpbamDisplay_BuildPayloadBuffersForRows(
  const uint16_t *candidate_rows,
  uint16_t candidate_row_count,
  uint16_t frame_count,
  uint16_t sequence_start_frame)
{
  uint16_t first_candidate_row = 0U;

  if ((candidate_rows == NULL) || (candidate_row_count == 0U) ||
      (frame_count == 0U) ||
      (frame_count > PS_LPBAM_DISPLAY_SEQUENCE_MAX))
  {
    PS_LpbamDisplay_ResetPayloadState();
    return PS_LpbamDisplay_AdmissionFailure(
      (frame_count > PS_LPBAM_DISPLAY_SEQUENCE_MAX) ?
      PS_LPBAM_ADMISSION_REASON_SEQUENCE :
      PS_LPBAM_ADMISSION_REASON_ARGUMENT);
  }

  PS_LpbamDisplay_ResetPayloadState();
  memset(ps_lpbam_display_candidate_row_enabled, 0,
         sizeof(ps_lpbam_display_candidate_row_enabled));

  for (uint16_t i = 0U; i < candidate_row_count; i++)
  {
    uint16_t row = candidate_rows[i];
    if ((row < 1U) || (row > DISPLAY_HEIGHT))
    {
      return PS_LpbamDisplay_AdmissionFailure(
        PS_LPBAM_ADMISSION_REASON_ARGUMENT);
    }

    if (first_candidate_row == 0U)
    {
      first_candidate_row = row;
    }
    ps_lpbam_display_candidate_row_enabled[row - 1U] = 1U;
  }

  ps_lpbam_display_sequence_start_frame =
    (uint16_t)(sequence_start_frame % frame_count);

  for (uint16_t frame = 0U; frame < frame_count; frame++)
  {
    uint16_t previous = (uint16_t)((ps_lpbam_display_sequence_start_frame + frame) %
                                   frame_count);
    uint16_t target = (uint16_t)((previous + 1U) % frame_count);
    uint16_t dirty_rows = 0U;
    uint16_t first_chunk = ps_lpbam_display_active_chunk_count;
    uint16_t dirty_index = 0U;

    memset(ps_lpbam_display_dirty_row_list, 0,
           sizeof(ps_lpbam_display_dirty_row_list));

    for (uint16_t row = 1U; row <= DISPLAY_HEIGHT; row++)
    {
      if ((ps_lpbam_display_candidate_row_enabled[row - 1U] != 0U) &&
          (PS_LpbamDisplay_RowIsDirty(ps_lpbam_display_frames[previous],
                                      ps_lpbam_display_frames[target],
                                      row) != 0U))
      {
        ps_lpbam_display_dirty_row_list[dirty_rows] = (uint8_t)row;
        dirty_rows++;
      }
    }

    if (dirty_rows == 0U)
    {
      ps_lpbam_display_dirty_row_list[0] = (uint8_t)first_candidate_row;
      dirty_rows = 1U;
    }

    while (dirty_index < dirty_rows)
    {
      uint16_t rows_this_payload;
      uint16_t payload_max_len;
      uint8_t *payload = NULL;
      uint8_t *write;
      uint16_t len = 0U;
      uint16_t last_payload_row;

      if (ps_lpbam_display_active_chunk_count >=
          PS_LPBAM_DISPLAY_MAX_CHUNKS)
      {
        return PS_LpbamDisplay_AdmissionFailure(
          PS_LPBAM_ADMISSION_REASON_CHUNKS);
      }

      rows_this_payload = (uint16_t)(dirty_rows - dirty_index);
      if (rows_this_payload > PS_LPBAM_DISPLAY_ROWS)
      {
        rows_this_payload = PS_LPBAM_DISPLAY_ROWS;
      }

      payload_max_len = (uint16_t)(1U + (((uint32_t)rows_this_payload + 1U) *
                                        PS_LPBAM_ROW_WIRE_BYTES) + 2U);
      if (PS_LpbamDisplay_AllocPayload(payload_max_len, &payload) != HAL_OK)
      {
        return PS_LpbamDisplay_AdmissionFailure(
          PS_LPBAM_ADMISSION_REASON_PAYLOAD);
      }

      write = payload;
      *write++ = PS_LPBAM_MLCD_CMD_WRITE;

      for (uint16_t i = 0U; i < rows_this_payload; i++)
      {
        uint16_t row = ps_lpbam_display_dirty_row_list[dirty_index + i];
        PS_LpbamDisplay_AppendWireRow(&write, row,
                                      ps_lpbam_display_frames[target]);
      }

      last_payload_row =
        ps_lpbam_display_dirty_row_list[(uint16_t)(dirty_index +
                                                   rows_this_payload - 1U)];
      if (PS_LpbamDisplay_FinalizePayload(payload, write, last_payload_row,
                                          ps_lpbam_display_frames[target],
                                          &len) != HAL_OK)
      {
        return PS_LpbamDisplay_AdmissionFailure(
          PS_LPBAM_ADMISSION_REASON_BUILD);
      }

      ps_lpbam_display_tx[ps_lpbam_display_active_chunk_count] = payload;
      ps_lpbam_display_tx_len[ps_lpbam_display_active_chunk_count] = len;
      ps_lpbam_display_payload_wire_bytes =
        (uint16_t)(ps_lpbam_display_payload_wire_bytes + len);
      dirty_index = (uint16_t)(dirty_index + rows_this_payload);
      ps_lpbam_display_active_chunk_count++;
    }

    ps_lpbam_display_sequence[frame].first_chunk = first_chunk;
    ps_lpbam_display_sequence[frame].chunk_count =
      (uint16_t)(ps_lpbam_display_active_chunk_count - first_chunk);
    ps_lpbam_display_sequence[frame].dirty_start_row =
      ps_lpbam_display_dirty_row_list[0];
    ps_lpbam_display_sequence[frame].dirty_row_count = dirty_rows;
    ps_lpbam_display_active_sequence_count++;
  }

  memcpy(ps_lpbam_display_frame_a, ps_lpbam_display_frames[0],
         sizeof(ps_lpbam_display_frame_a));
  memcpy(ps_lpbam_display_frame_b, ps_lpbam_display_frames[1],
         sizeof(ps_lpbam_display_frame_b));

  if (ps_lpbam_display_active_sequence_count != frame_count)
  {
    return PS_LpbamDisplay_AdmissionFailure(
      PS_LPBAM_ADMISSION_REASON_BUILD);
  }

  ps_lpbam_display_admission.sequence_used =
    ps_lpbam_display_active_sequence_count;
  ps_lpbam_display_admission.chunk_used =
    ps_lpbam_display_active_chunk_count;
  ps_lpbam_display_admission.payload_wire_bytes =
    ps_lpbam_display_payload_wire_bytes;
  ps_lpbam_display_admission.payload_used_bytes =
    ps_lpbam_display_payload_used;
  ps_lpbam_display_admission.status = (uint32_t)HAL_OK;
  ps_lpbam_display_admission.reason = PS_LPBAM_ADMISSION_REASON_NONE;
  return HAL_OK;
}

static HAL_StatusTypeDef PS_LpbamDisplay_BuildPayloadBuffers(
  uint16_t start_row,
  uint16_t row_count)
{
  if ((start_row < 1U) || (row_count == 0U) ||
      (((uint32_t)start_row + (uint32_t)row_count - 1U) > DISPLAY_HEIGHT))
  {
    return HAL_ERROR;
  }

  for (uint16_t i = 0U; i < row_count; i++)
  {
    ps_lpbam_display_candidate_rows[i] = (uint16_t)(start_row + i);
  }

  return PS_LpbamDisplay_BuildPayloadBuffersForRows(
    ps_lpbam_display_candidate_rows,
    row_count,
    PS_LPBAM_DISPLAY_FRAME_COUNT,
    0U);
}

HAL_StatusTypeDef PS_LpbamDisplay_BuildPatternBuffers(uint16_t start_row,
                                                      uint16_t row_count)
{
  PS_LpbamDisplay_ComposeExperimentFrames();
  return PS_LpbamDisplay_BuildPayloadBuffers(start_row, row_count);
}

HAL_StatusTypeDef PS_LpbamDisplay_BuildPreparedAnimationBuffers(
  const uint16_t *candidate_rows,
  uint16_t candidate_row_count,
  uint16_t frame_count,
  uint16_t sequence_start_frame)
{
  PS_LpbamDisplay_SetExperimentVariant(0xC1U);
  return PS_LpbamDisplay_BuildPayloadBuffersForRows(
    candidate_rows,
    candidate_row_count,
    frame_count,
    sequence_start_frame);
}

