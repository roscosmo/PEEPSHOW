#include "ps_lpbam_display_buffers.h"
#include <string.h>

#define PS_LPBAM_MLCD_CMD_WRITE 0x01U
#define PS_LPBAM_ROW_WIRE_BYTES (1U + LINE_WIDTH + 1U)
#define PS_LPBAM_ALIGN4(value) (((value) + 3U) & ~3U)
#define PS_LPBAM_SPATIAL_PAYLOAD_BYTES \
  (1U + ((PS_LPBAM_DISPLAY_SPATIAL_ROWS + 1U) * \
         PS_LPBAM_ROW_WIRE_BYTES) + 2U)
#define PS_LPBAM_SPATIAL_SLOT_BYTES \
  PS_LPBAM_ALIGN4(PS_LPBAM_SPATIAL_PAYLOAD_BYTES)
#define PS_LPBAM_DISPLAY_BANK_BYTES \
  (PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT * PS_LPBAM_SPATIAL_SLOT_BYTES)
#define PS_LPBAM_DISPLAY_ARENA_SIZE \
  (PS_LPBAM_DISPLAY_GUARANTEED_FRAME_COUNT * PS_LPBAM_DISPLAY_BANK_BYTES)

#if defined(__GNUC__)
#define PS_SRAM4_BUF_ATTR __attribute__((section(".sram4"))) __attribute__((aligned(4)))
#else
#define PS_SRAM4_BUF_ATTR
#endif

uint8_t ps_lpbam_display_frame_a[DISPLAY_HEIGHT][LINE_WIDTH];
uint8_t ps_lpbam_display_frame_b[DISPLAY_HEIGHT][LINE_WIDTH];
uint8_t *ps_lpbam_display_tx[PS_LPBAM_DISPLAY_MAX_CHUNKS];
uint8_t *ps_lpbam_display_payload_slot[PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
uint16_t ps_lpbam_display_tx_len[PS_LPBAM_DISPLAY_MAX_CHUNKS];
uint16_t ps_lpbam_display_payload_slot_capacity[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
uint16_t ps_lpbam_display_payload_slot_len[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
uint8_t ps_lpbam_display_payload_slot_band[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
uint8_t ps_lpbam_display_payload_slot_occupied[
  PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT];
uint8_t ps_lpbam_display_tx_payload_slot[PS_LPBAM_DISPLAY_MAX_CHUNKS];
ps_lpbam_display_sequence_entry_t
  ps_lpbam_display_sequence[PS_LPBAM_DISPLAY_SEQUENCE_MAX];
ps_lpbam_display_admission_t ps_lpbam_display_admission;
uint16_t ps_lpbam_display_active_sequence_count;
uint16_t ps_lpbam_display_active_chunk_count;
uint16_t ps_lpbam_display_payload_wire_bytes;
uint16_t ps_lpbam_display_sequence_start_frame;
uint16_t ps_lpbam_display_queue_start_slot;
static uint8_t ps_lpbam_display_payload_arena[PS_LPBAM_DISPLAY_ARENA_SIZE] PS_SRAM4_BUF_ATTR;
static uint8_t ps_lpbam_display_payload_scratch[
  PS_LPBAM_DISPLAY_TRANSACTION_MAX_LEN];
static uint16_t ps_lpbam_display_payload_used;
static uint8_t ps_lpbam_display_experiment_variant;
static uint16_t ps_lpbam_display_candidate_rows[DISPLAY_HEIGHT];
static uint8_t ps_lpbam_display_candidate_row_enabled[DISPLAY_HEIGHT];
static uint8_t ps_lpbam_display_dirty_row_list[DISPLAY_HEIGHT];
static uint16_t ps_lpbam_display_expected_sequence_count;
static uint16_t ps_lpbam_display_first_candidate_row;
#if defined(__GNUC__)
__attribute__((weak))
#endif
void PS_LpbamDisplay_ComposeExperimentFrames(void)
{
  memset(ps_lpbam_display_frame_a, 0x00,
         sizeof(ps_lpbam_display_frame_a));
  memset(ps_lpbam_display_frame_b, 0xFF,
         sizeof(ps_lpbam_display_frame_b));
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

static uint16_t PS_LpbamDisplay_BandStartRow(uint8_t band)
{
  return (uint16_t)(
    ((uint16_t)band * PS_LPBAM_DISPLAY_SPATIAL_ROWS) + 1U);
}

static uint16_t PS_LpbamDisplay_BandRowCount(uint8_t band)
{
  uint16_t start_row = PS_LpbamDisplay_BandStartRow(band);
  uint16_t remaining;

  if (start_row > DISPLAY_HEIGHT)
  {
    return 0U;
  }
  remaining = (uint16_t)(DISPLAY_HEIGHT - start_row + 1U);
  return (remaining > PS_LPBAM_DISPLAY_SPATIAL_ROWS) ?
    PS_LPBAM_DISPLAY_SPATIAL_ROWS : remaining;
}

static void PS_LpbamDisplay_InitializePayloadSlots(void)
{
  uint16_t offset = 0U;

  for (uint16_t slot = 0U;
       slot < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT;
       ++slot)
  {
    uint16_t capacity = (uint16_t)PS_LPBAM_SPATIAL_SLOT_BYTES;

    ps_lpbam_display_payload_slot[slot] =
      &ps_lpbam_display_payload_arena[offset];
    ps_lpbam_display_payload_slot_capacity[slot] = capacity;
    offset = (uint16_t)(offset + capacity);
  }
}

static int16_t PS_LpbamDisplay_FindPayloadSlot(uint8_t band,
                                               const uint8_t *payload,
                                               uint16_t len)
{
  int16_t free_preferred = -1;
  int16_t free_fallback = -1;

  for (uint16_t slot = 0U;
       slot < PS_LPBAM_DISPLAY_PAYLOAD_SLOT_COUNT;
       ++slot)
  {
    if (ps_lpbam_display_payload_slot_occupied[slot] != 0U)
    {
      if ((ps_lpbam_display_payload_slot_band[slot] == band) &&
          (ps_lpbam_display_payload_slot_len[slot] == len) &&
          (memcmp(ps_lpbam_display_payload_slot[slot], payload, len) == 0))
      {
        return (int16_t)slot;
      }
      continue;
    }

    if (ps_lpbam_display_payload_slot_capacity[slot] < len)
    {
      continue;
    }
    if ((slot % PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT) == band)
    {
      if (free_preferred < 0)
      {
        free_preferred = (int16_t)slot;
      }
    }
    else if (free_fallback < 0)
    {
      free_fallback = (int16_t)slot;
    }
  }

  return (free_preferred >= 0) ? free_preferred : free_fallback;
}

static HAL_StatusTypeDef PS_LpbamDisplay_StorePayload(
  uint8_t band,
  const uint8_t *payload,
  uint16_t len,
  uint8_t **out_payload,
  uint8_t *out_slot)
{
  int16_t slot;

  if ((payload == NULL) || (out_payload == NULL) || (out_slot == NULL) ||
      (len == 0U))
  {
    return HAL_ERROR;
  }

  slot = PS_LpbamDisplay_FindPayloadSlot(band, payload, len);
  if (slot < 0)
  {
    return HAL_ERROR;
  }

  if (ps_lpbam_display_payload_slot_occupied[(uint16_t)slot] == 0U)
  {
    memcpy(ps_lpbam_display_payload_slot[(uint16_t)slot], payload, len);
    ps_lpbam_display_payload_slot_occupied[(uint16_t)slot] = 1U;
    ps_lpbam_display_payload_slot_band[(uint16_t)slot] = band;
    ps_lpbam_display_payload_slot_len[(uint16_t)slot] = len;
    ps_lpbam_display_payload_used = (uint16_t)(
      ps_lpbam_display_payload_used +
      ps_lpbam_display_payload_slot_capacity[(uint16_t)slot]);
  }

  *out_payload = ps_lpbam_display_payload_slot[(uint16_t)slot];
  *out_slot = (uint8_t)slot;
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

  return ((*out_len) <= PS_LPBAM_DISPLAY_TRANSACTION_MAX_LEN) ?
    HAL_OK : HAL_ERROR;
}

static void PS_LpbamDisplay_ResetPayloadState(void)
{
  memset(ps_lpbam_display_tx, 0, sizeof(ps_lpbam_display_tx));
  memset(ps_lpbam_display_tx_len, 0, sizeof(ps_lpbam_display_tx_len));
  memset(ps_lpbam_display_tx_payload_slot, 0xFF,
         sizeof(ps_lpbam_display_tx_payload_slot));
  memset(ps_lpbam_display_payload_slot_len, 0,
         sizeof(ps_lpbam_display_payload_slot_len));
  memset(ps_lpbam_display_payload_slot_band, 0xFF,
         sizeof(ps_lpbam_display_payload_slot_band));
  memset(ps_lpbam_display_payload_slot_occupied, 0,
         sizeof(ps_lpbam_display_payload_slot_occupied));
  memset(ps_lpbam_display_sequence, 0,
         sizeof(ps_lpbam_display_sequence));
  memset(&ps_lpbam_display_admission, 0,
         sizeof(ps_lpbam_display_admission));
  memset(ps_lpbam_display_payload_arena, 0,
         sizeof(ps_lpbam_display_payload_arena));
  PS_LpbamDisplay_InitializePayloadSlots();
  ps_lpbam_display_payload_used = 0U;
  ps_lpbam_display_active_sequence_count = 0U;
  ps_lpbam_display_active_chunk_count = 0U;
  ps_lpbam_display_payload_wire_bytes = 0U;
  ps_lpbam_display_queue_start_slot = 0U;
  ps_lpbam_display_expected_sequence_count = 0U;
  ps_lpbam_display_first_candidate_row = 0U;
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

HAL_StatusTypeDef PS_LpbamDisplay_BeginPreparedAnimation(
  const uint16_t *candidate_rows,
  uint16_t candidate_row_count,
  uint16_t sequence_count,
  uint16_t sequence_start_frame)
{
  if ((candidate_rows == NULL) || (candidate_row_count == 0U) ||
      (candidate_row_count > DISPLAY_HEIGHT) ||
      (sequence_count == 0U) ||
      (sequence_count > PS_LPBAM_DISPLAY_SEQUENCE_MAX))
  {
    PS_LpbamDisplay_ResetPayloadState();
    return PS_LpbamDisplay_AdmissionFailure(
      (sequence_count > PS_LPBAM_DISPLAY_SEQUENCE_MAX) ?
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

    if (ps_lpbam_display_first_candidate_row == 0U)
    {
      ps_lpbam_display_first_candidate_row = row;
    }
    ps_lpbam_display_candidate_row_enabled[row - 1U] = 1U;
  }

  ps_lpbam_display_expected_sequence_count = sequence_count;
  ps_lpbam_display_sequence_start_frame =
    (uint16_t)(sequence_start_frame % sequence_count);
  PS_LpbamDisplay_SetExperimentVariant(0xC1U);
  return HAL_OK;
}

HAL_StatusTypeDef PS_LpbamDisplay_AppendPreparedTransition(
  const uint8_t previous_frame[DISPLAY_HEIGHT][LINE_WIDTH],
  const uint8_t target_frame[DISPLAY_HEIGHT][LINE_WIDTH])
{
  uint16_t dirty_rows = 0U;
  uint16_t first_chunk = ps_lpbam_display_active_chunk_count;
  uint16_t sequence = ps_lpbam_display_active_sequence_count;
  uint8_t dirty_band[PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT] = {0};

  if ((previous_frame == NULL) || (target_frame == NULL) ||
      (ps_lpbam_display_expected_sequence_count == 0U) ||
      (sequence >= ps_lpbam_display_expected_sequence_count) ||
      (sequence >= PS_LPBAM_DISPLAY_SEQUENCE_MAX))
  {
    return PS_LpbamDisplay_AdmissionFailure(
      PS_LPBAM_ADMISSION_REASON_SEQUENCE);
  }

  memset(ps_lpbam_display_dirty_row_list, 0,
         sizeof(ps_lpbam_display_dirty_row_list));

  for (uint16_t row = 1U; row <= DISPLAY_HEIGHT; row++)
  {
    if ((ps_lpbam_display_candidate_row_enabled[row - 1U] != 0U) &&
        (PS_LpbamDisplay_RowIsDirty(previous_frame,
                                    target_frame,
                                    row) != 0U))
    {
      ps_lpbam_display_dirty_row_list[dirty_rows] = (uint8_t)row;
      dirty_band[(row - 1U) / PS_LPBAM_DISPLAY_SPATIAL_ROWS] = 1U;
      dirty_rows++;
    }
  }

  if (dirty_rows == 0U)
  {
    ps_lpbam_display_dirty_row_list[0] =
      (uint8_t)ps_lpbam_display_first_candidate_row;
    dirty_band[(ps_lpbam_display_first_candidate_row - 1U) /
               PS_LPBAM_DISPLAY_SPATIAL_ROWS] = 1U;
    dirty_rows = 1U;
  }

  for (uint8_t band = 0U;
       band < PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT;
       ++band)
  {
    uint16_t rows_this_chunk;
    uint16_t start_row;
    uint16_t last_row;
    uint8_t *payload = NULL;
    uint8_t *write;
    uint16_t len = 0U;
    uint8_t payload_slot = 0xFFU;

    if (dirty_band[band] == 0U)
    {
      continue;
    }

    if (ps_lpbam_display_active_chunk_count >=
        PS_LPBAM_DISPLAY_MAX_CHUNKS)
    {
      return PS_LpbamDisplay_AdmissionFailure(
        PS_LPBAM_ADMISSION_REASON_CHUNKS);
    }

    start_row = PS_LpbamDisplay_BandStartRow(band);
    rows_this_chunk = PS_LpbamDisplay_BandRowCount(band);
    write = ps_lpbam_display_payload_scratch;
    *write++ = PS_LPBAM_MLCD_CMD_WRITE;
    for (uint16_t i = 0U; i < rows_this_chunk; ++i)
    {
      uint16_t row = (uint16_t)(start_row + i);
      PS_LpbamDisplay_AppendWireRow(&write, row, target_frame);
    }

    last_row = (uint16_t)(start_row + rows_this_chunk - 1U);
    if (PS_LpbamDisplay_FinalizePayload(
          ps_lpbam_display_payload_scratch,
          write,
          last_row,
          target_frame,
          &len) != HAL_OK)
    {
      return PS_LpbamDisplay_AdmissionFailure(
        PS_LPBAM_ADMISSION_REASON_BUILD);
    }
    if (PS_LpbamDisplay_StorePayload(
          band,
          ps_lpbam_display_payload_scratch,
          len,
          &payload,
          &payload_slot) != HAL_OK)
    {
      return PS_LpbamDisplay_AdmissionFailure(
        PS_LPBAM_ADMISSION_REASON_PAYLOAD);
    }

    ps_lpbam_display_tx[ps_lpbam_display_active_chunk_count] =
      payload;
    ps_lpbam_display_tx_len[ps_lpbam_display_active_chunk_count] =
      len;
    ps_lpbam_display_tx_payload_slot[
      ps_lpbam_display_active_chunk_count] = payload_slot;
    ps_lpbam_display_payload_wire_bytes =
      (uint16_t)(ps_lpbam_display_payload_wire_bytes + len);
    ps_lpbam_display_active_chunk_count++;
  }

  ps_lpbam_display_sequence[sequence].first_chunk = first_chunk;
  ps_lpbam_display_sequence[sequence].chunk_count =
    (uint16_t)(ps_lpbam_display_active_chunk_count - first_chunk);
  ps_lpbam_display_sequence[sequence].dirty_start_row =
    PS_LpbamDisplay_BandStartRow((uint8_t)(
      (ps_lpbam_display_dirty_row_list[0] - 1U) /
      PS_LPBAM_DISPLAY_SPATIAL_ROWS));
  ps_lpbam_display_sequence[sequence].dirty_row_count = 0U;
  for (uint8_t band = 0U;
       band < PS_LPBAM_DISPLAY_SPATIAL_CHUNK_COUNT;
       ++band)
  {
    if (dirty_band[band] != 0U)
    {
      ps_lpbam_display_sequence[sequence].dirty_row_count = (uint16_t)(
        ps_lpbam_display_sequence[sequence].dirty_row_count +
        PS_LpbamDisplay_BandRowCount(band));
    }
  }
  ps_lpbam_display_active_sequence_count++;
  return HAL_OK;
}

HAL_StatusTypeDef PS_LpbamDisplay_FinishPreparedAnimation(void)
{
  if ((ps_lpbam_display_expected_sequence_count == 0U) ||
      (ps_lpbam_display_active_sequence_count !=
       ps_lpbam_display_expected_sequence_count))
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

  if (PS_LpbamDisplay_BeginPreparedAnimation(
        ps_lpbam_display_candidate_rows,
        row_count,
        PS_LPBAM_DISPLAY_PATTERN_SEQUENCE_COUNT,
        0U) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if ((PS_LpbamDisplay_AppendPreparedTransition(
         ps_lpbam_display_frame_a,
         ps_lpbam_display_frame_b) != HAL_OK) ||
      (PS_LpbamDisplay_AppendPreparedTransition(
         ps_lpbam_display_frame_b,
         ps_lpbam_display_frame_a) != HAL_OK) ||
      (PS_LpbamDisplay_AppendPreparedTransition(
         ps_lpbam_display_frame_a,
         ps_lpbam_display_frame_b) != HAL_OK) ||
      (PS_LpbamDisplay_AppendPreparedTransition(
         ps_lpbam_display_frame_b,
         ps_lpbam_display_frame_a) != HAL_OK))
  {
    return HAL_ERROR;
  }

  return PS_LpbamDisplay_FinishPreparedAnimation();
}

HAL_StatusTypeDef PS_LpbamDisplay_BuildPatternBuffers(uint16_t start_row,
                                                      uint16_t row_count)
{
  PS_LpbamDisplay_ComposeExperimentFrames();
  return PS_LpbamDisplay_BuildPayloadBuffers(start_row, row_count);
}

