#include "ps_storage_joystick_calibration.h"

#include <stddef.h>
#include <string.h>

#include "ps_storage_layout.h"

#define PS_STORAGE_JOYSTICK_CALIBRATION_MAGIC          (0x314C4143UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_FORMAT_VERSION (1U)
#define PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_MARKER  (0x54494D43UL)

#define PS_STORAGE_JOYSTICK_CALIBRATION_MAGIC_OFFSET       (0UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_VERSION_OFFSET     (4UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_BODY_SIZE_OFFSET   (6UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_GENERATION_OFFSET  (8UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_PAYLOAD_OFFSET     (12UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_CRC32_OFFSET       (76UL)
#define PS_STORAGE_JOYSTICK_CALIBRATION_RESERVED_OFFSET    (80UL)

volatile ps_storage_joystick_calibration_probe_t
  g_ps_storage_joystick_calibration_probe =
{
  .api_version = PS_STORAGE_JOYSTICK_CALIBRATION_API_VERSION,
  .status = (uint32_t)PS_STATUS_NOT_INITIALIZED,
  .selected_record = PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION,
  .selection_reason = PS_STORAGE_JOYSTICK_CALIBRATION_REASON_NONE
};

volatile ps_storage_joystick_calibration_save_probe_t
  g_ps_storage_joystick_calibration_save_probe =
{
  .api_version = PS_STORAGE_JOYSTICK_CALIBRATION_API_VERSION,
  .status = (uint32_t)PS_STATUS_NOT_INITIALIZED,
  .stage = PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_IDLE,
  .source_record = PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION,
  .target_record = PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION,
  .selected_record = PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION
};

static uint8_t ps_storage_joystick_calibration_body[
  PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_BODY_SIZE];
static uint8_t ps_storage_joystick_calibration_verify[
  PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_BODY_SIZE];

static uint16_t PS_StorageJoystickCalibration_U16(const uint8_t *bytes)
{
  return (uint16_t)((uint16_t)bytes[0] |
                    ((uint16_t)bytes[1] << 8));
}

static uint32_t PS_StorageJoystickCalibration_U32(const uint8_t *bytes)
{
  return (uint32_t)bytes[0] |
         ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) |
         ((uint32_t)bytes[3] << 24);
}

static int32_t PS_StorageJoystickCalibration_I32(const uint8_t *bytes)
{
  return (int32_t)PS_StorageJoystickCalibration_U32(bytes);
}

static void PS_StorageJoystickCalibration_PutU16(uint8_t *bytes,
                                                 uint16_t value)
{
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
}

static void PS_StorageJoystickCalibration_PutU32(uint8_t *bytes,
                                                 uint32_t value)
{
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
  bytes[2] = (uint8_t)(value >> 16);
  bytes[3] = (uint8_t)(value >> 24);
}

static void PS_StorageJoystickCalibration_PutI32(uint8_t *bytes,
                                                 int32_t value)
{
  PS_StorageJoystickCalibration_PutU32(bytes, (uint32_t)value);
}

static uint32_t PS_StorageJoystickCalibration_Crc32(const uint8_t *body)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t index;

  for (index = 0UL;
       index < PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_BODY_SIZE;
       ++index)
  {
    uint32_t bit;
    uint8_t value = body[index];

    if ((index >= PS_STORAGE_JOYSTICK_CALIBRATION_CRC32_OFFSET) &&
        (index < (PS_STORAGE_JOYSTICK_CALIBRATION_CRC32_OFFSET + 4UL)))
    {
      value = 0U;
    }
    crc ^= value;
    for (bit = 0UL; bit < 8UL; ++bit)
    {
      crc = ((crc & 1UL) != 0UL) ?
              ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

static const ps_storage_region_t *PS_StorageJoystickCalibration_FindRegion(
  void)
{
  const ps_storage_region_t *regions;
  uint32_t count;
  uint32_t index;

  regions = ps_storage_layout_regions(&count);
  for (index = 0UL; index < count; ++index)
  {
    if (regions[index].id == PS_STORAGE_REGION_CALIBRATION)
    {
      return &regions[index];
    }
  }
  return NULL;
}

static uint32_t PS_StorageJoystickCalibration_IsGenerationNewer(
  uint32_t candidate,
  uint32_t current)
{
  uint32_t difference = candidate - current;

  return ((difference != 0UL) && (difference < 0x80000000UL)) ? 1UL : 0UL;
}

static uint32_t PS_StorageJoystickCalibration_PayloadValid(
  const ps_input_joystick_calibration_t *calibration)
{
  int64_t x_span;
  int64_t y_span;

  if ((calibration == NULL) ||
      (calibration->valid != 1UL) ||
      (calibration->transform_valid > 1UL) ||
      (calibration->deadzone_counts < 0) ||
      (calibration->direction_threshold <= 0) ||
      (calibration->direction_threshold > PS_INPUT_JOYSTICK_AXIS_SCALE) ||
      (calibration->direction_release_threshold < 0) ||
      (calibration->direction_release_threshold >=
       calibration->direction_threshold) ||
      (calibration->dominance_hysteresis < 0) ||
      (calibration->dominance_hysteresis > PS_INPUT_JOYSTICK_AXIS_SCALE))
  {
    return 0UL;
  }

  if (calibration->transform_valid != 0UL)
  {
    if ((calibration->min_x >= 0) || (calibration->max_x <= 0) ||
        (calibration->min_y >= 0) || (calibration->max_y <= 0) ||
        (calibration->deadzone_counts >= PS_INPUT_JOYSTICK_AXIS_SCALE))
    {
      return 0UL;
    }
    return 1UL;
  }

  if ((calibration->min_x >= calibration->center_x) ||
      (calibration->max_x <= calibration->center_x) ||
      (calibration->min_y >= calibration->center_y) ||
      (calibration->max_y <= calibration->center_y))
  {
    return 0UL;
  }
  x_span = (int64_t)calibration->max_x -
           (int64_t)calibration->min_x;
  y_span = (int64_t)calibration->max_y -
           (int64_t)calibration->min_y;
  return ((x_span > 0) && (y_span > 0) &&
          (calibration->deadzone_counts < x_span) &&
          (calibration->deadzone_counts < y_span)) ? 1UL : 0UL;
}

static void PS_StorageJoystickCalibration_EncodePayload(
  uint8_t *bytes,
  const ps_input_joystick_calibration_t *calibration)
{
  PS_StorageJoystickCalibration_PutI32(&bytes[0], calibration->center_x);
  PS_StorageJoystickCalibration_PutI32(&bytes[4], calibration->center_y);
  PS_StorageJoystickCalibration_PutI32(&bytes[8], calibration->min_x);
  PS_StorageJoystickCalibration_PutI32(&bytes[12], calibration->max_x);
  PS_StorageJoystickCalibration_PutI32(&bytes[16], calibration->min_y);
  PS_StorageJoystickCalibration_PutI32(&bytes[20], calibration->max_y);
  PS_StorageJoystickCalibration_PutI32(&bytes[24], calibration->deadzone_counts);
  PS_StorageJoystickCalibration_PutI32(&bytes[28], calibration->direction_threshold);
  PS_StorageJoystickCalibration_PutI32(&bytes[32], calibration->direction_release_threshold);
  PS_StorageJoystickCalibration_PutI32(&bytes[36], calibration->dominance_hysteresis);
  PS_StorageJoystickCalibration_PutI32(&bytes[40], calibration->transform_xx_q20);
  PS_StorageJoystickCalibration_PutI32(&bytes[44], calibration->transform_xy_q20);
  PS_StorageJoystickCalibration_PutI32(&bytes[48], calibration->transform_yx_q20);
  PS_StorageJoystickCalibration_PutI32(&bytes[52], calibration->transform_yy_q20);
  PS_StorageJoystickCalibration_PutU32(&bytes[56], calibration->transform_valid);
  PS_StorageJoystickCalibration_PutU32(&bytes[60], calibration->valid);
}

static void PS_StorageJoystickCalibration_DecodePayload(
  const uint8_t *bytes,
  ps_input_joystick_calibration_t *calibration)
{
  calibration->center_x = PS_StorageJoystickCalibration_I32(&bytes[0]);
  calibration->center_y = PS_StorageJoystickCalibration_I32(&bytes[4]);
  calibration->min_x = PS_StorageJoystickCalibration_I32(&bytes[8]);
  calibration->max_x = PS_StorageJoystickCalibration_I32(&bytes[12]);
  calibration->min_y = PS_StorageJoystickCalibration_I32(&bytes[16]);
  calibration->max_y = PS_StorageJoystickCalibration_I32(&bytes[20]);
  calibration->deadzone_counts = PS_StorageJoystickCalibration_I32(&bytes[24]);
  calibration->direction_threshold = PS_StorageJoystickCalibration_I32(&bytes[28]);
  calibration->direction_release_threshold = PS_StorageJoystickCalibration_I32(&bytes[32]);
  calibration->dominance_hysteresis = PS_StorageJoystickCalibration_I32(&bytes[36]);
  calibration->transform_xx_q20 = PS_StorageJoystickCalibration_I32(&bytes[40]);
  calibration->transform_xy_q20 = PS_StorageJoystickCalibration_I32(&bytes[44]);
  calibration->transform_yx_q20 = PS_StorageJoystickCalibration_I32(&bytes[48]);
  calibration->transform_yy_q20 = PS_StorageJoystickCalibration_I32(&bytes[52]);
  calibration->transform_valid = PS_StorageJoystickCalibration_U32(&bytes[56]);
  calibration->valid = PS_StorageJoystickCalibration_U32(&bytes[60]);
}

static void PS_StorageJoystickCalibration_ResetScanProbe(void)
{
  uint32_t scan_count = g_ps_storage_joystick_calibration_probe.scan_count + 1UL;
  uint32_t index;

  (void)memset((void *)&g_ps_storage_joystick_calibration_probe,
               0,
               sizeof(g_ps_storage_joystick_calibration_probe));
  g_ps_storage_joystick_calibration_probe.api_version =
    PS_STORAGE_JOYSTICK_CALIBRATION_API_VERSION;
  g_ps_storage_joystick_calibration_probe.scan_count = scan_count;
  g_ps_storage_joystick_calibration_probe.status =
    (uint32_t)PS_STATUS_INTERNAL_ERROR;
  g_ps_storage_joystick_calibration_probe.selected_record =
    PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION;
  for (index = 0UL;
       index < PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_COUNT;
       ++index)
  {
    g_ps_storage_joystick_calibration_probe.record[index].read_status =
      (uint32_t)PS_STATUS_NOT_INITIALIZED;
  }
}

static void PS_StorageJoystickCalibration_ResetSaveProbe(void)
{
  uint32_t save_count =
    g_ps_storage_joystick_calibration_save_probe.save_count + 1UL;

  (void)memset((void *)&g_ps_storage_joystick_calibration_save_probe,
               0,
               sizeof(g_ps_storage_joystick_calibration_save_probe));
  g_ps_storage_joystick_calibration_save_probe.api_version =
    PS_STORAGE_JOYSTICK_CALIBRATION_API_VERSION;
  g_ps_storage_joystick_calibration_save_probe.save_count = save_count;
  g_ps_storage_joystick_calibration_save_probe.status =
    (uint32_t)PS_STATUS_INTERNAL_ERROR;
  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_VALIDATE;
  g_ps_storage_joystick_calibration_save_probe.source_record =
    PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION;
  g_ps_storage_joystick_calibration_save_probe.target_record =
    PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION;
  g_ps_storage_joystick_calibration_save_probe.selected_record =
    PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION;
  g_ps_storage_joystick_calibration_save_probe.erase_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_joystick_calibration_save_probe.program_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_joystick_calibration_save_probe.verify_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_joystick_calibration_save_probe.commit_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_joystick_calibration_save_probe.commit_verify_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_joystick_calibration_save_probe.rescan_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
}

static void PS_StorageJoystickCalibration_BuildRecord(
  const ps_input_joystick_calibration_t *calibration,
  uint32_t generation)
{
  uint32_t crc;

  (void)memset(ps_storage_joystick_calibration_body,
               0xFF,
               sizeof(ps_storage_joystick_calibration_body));
  PS_StorageJoystickCalibration_PutU32(
    &ps_storage_joystick_calibration_body[
      PS_STORAGE_JOYSTICK_CALIBRATION_MAGIC_OFFSET],
    PS_STORAGE_JOYSTICK_CALIBRATION_MAGIC);
  PS_StorageJoystickCalibration_PutU16(
    &ps_storage_joystick_calibration_body[
      PS_STORAGE_JOYSTICK_CALIBRATION_VERSION_OFFSET],
    PS_STORAGE_JOYSTICK_CALIBRATION_FORMAT_VERSION);
  PS_StorageJoystickCalibration_PutU16(
    &ps_storage_joystick_calibration_body[
      PS_STORAGE_JOYSTICK_CALIBRATION_BODY_SIZE_OFFSET],
    PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_BODY_SIZE);
  PS_StorageJoystickCalibration_PutU32(
    &ps_storage_joystick_calibration_body[
      PS_STORAGE_JOYSTICK_CALIBRATION_GENERATION_OFFSET],
    generation);
  PS_StorageJoystickCalibration_EncodePayload(
    &ps_storage_joystick_calibration_body[
      PS_STORAGE_JOYSTICK_CALIBRATION_PAYLOAD_OFFSET],
    calibration);
  PS_StorageJoystickCalibration_PutU32(
    &ps_storage_joystick_calibration_body[
      PS_STORAGE_JOYSTICK_CALIBRATION_CRC32_OFFSET],
    0UL);
  crc = PS_StorageJoystickCalibration_Crc32(
    ps_storage_joystick_calibration_body);
  PS_StorageJoystickCalibration_PutU32(
    &ps_storage_joystick_calibration_body[
      PS_STORAGE_JOYSTICK_CALIBRATION_CRC32_OFFSET],
    crc);
  g_ps_storage_joystick_calibration_save_probe.record_crc32 = crc;
}

static void PS_StorageJoystickCalibration_DecodeRecord(
  const uint8_t *bytes,
  volatile ps_storage_joystick_calibration_record_probe_t *record)
{
  ps_input_joystick_calibration_t calibration;

  record->magic = PS_StorageJoystickCalibration_U32(
    &bytes[PS_STORAGE_JOYSTICK_CALIBRATION_MAGIC_OFFSET]);
  record->commit_marker = PS_StorageJoystickCalibration_U32(
    &bytes[PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_OFFSET]);
  record->generation = PS_StorageJoystickCalibration_U32(
    &bytes[PS_STORAGE_JOYSTICK_CALIBRATION_GENERATION_OFFSET]);
  record->stored_crc32 = PS_StorageJoystickCalibration_U32(
    &bytes[PS_STORAGE_JOYSTICK_CALIBRATION_CRC32_OFFSET]);
  record->computed_crc32 = PS_StorageJoystickCalibration_Crc32(bytes);
  PS_StorageJoystickCalibration_DecodePayload(
    &bytes[PS_STORAGE_JOYSTICK_CALIBRATION_PAYLOAD_OFFSET],
    &calibration);
  record->calibration = calibration;
}

static void PS_StorageJoystickCalibration_ValidateRecord(
  const uint8_t *bytes,
  volatile ps_storage_joystick_calibration_record_probe_t *record)
{
  uint32_t index;

  if (record->commit_marker !=
      PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_MARKER)
  {
    record->reason =
      PS_STORAGE_JOYSTICK_CALIBRATION_REASON_UNCOMMITTED;
    return;
  }
  if ((record->magic != PS_STORAGE_JOYSTICK_CALIBRATION_MAGIC) ||
      (PS_StorageJoystickCalibration_U16(
         &bytes[PS_STORAGE_JOYSTICK_CALIBRATION_VERSION_OFFSET]) !=
       PS_STORAGE_JOYSTICK_CALIBRATION_FORMAT_VERSION) ||
      (PS_StorageJoystickCalibration_U16(
         &bytes[PS_STORAGE_JOYSTICK_CALIBRATION_BODY_SIZE_OFFSET]) !=
       PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_BODY_SIZE))
  {
    record->reason = PS_STORAGE_JOYSTICK_CALIBRATION_REASON_FORMAT;
    return;
  }
  if (record->stored_crc32 != record->computed_crc32)
  {
    record->reason = PS_STORAGE_JOYSTICK_CALIBRATION_REASON_CRC;
    return;
  }
  if (PS_StorageJoystickCalibration_PayloadValid(
        (const ps_input_joystick_calibration_t *)&record->calibration) == 0UL)
  {
    record->reason = PS_STORAGE_JOYSTICK_CALIBRATION_REASON_BOUNDS;
    return;
  }
  for (index = PS_STORAGE_JOYSTICK_CALIBRATION_RESERVED_OFFSET;
       index < PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_BODY_SIZE;
       ++index)
  {
    if (bytes[index] != 0xFFU)
    {
      record->reason = PS_STORAGE_JOYSTICK_CALIBRATION_REASON_RESERVED;
      return;
    }
  }
  record->valid = 1UL;
  record->reason = PS_STORAGE_JOYSTICK_CALIBRATION_REASON_NONE;
}

ps_status_t PS_StorageJoystickCalibration_Scan(
  ps_storage_flash_block_t *block)
{
  const ps_storage_region_t *region;
  uint8_t bytes[PS_STORAGE_JOYSTICK_CALIBRATION_READ_SIZE];
  uint32_t record_index;
  uint32_t selected =
    PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION;

  PS_StorageJoystickCalibration_ResetScanProbe();
  if ((block == NULL) || (block->initialized == 0UL))
  {
    g_ps_storage_joystick_calibration_probe.status =
      (uint32_t)PS_STATUS_INVALID_ARGUMENT;
    g_ps_storage_joystick_calibration_probe.selection_reason =
      PS_STORAGE_JOYSTICK_CALIBRATION_REASON_ARGUMENT;
    return PS_STATUS_INVALID_ARGUMENT;
  }

  region = PS_StorageJoystickCalibration_FindRegion();
  if ((region == NULL) ||
      (region->length <
       (PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_COUNT *
        PS_STORAGE_JOYSTICK_CALIBRATION_SECTOR_SIZE)) ||
      (region->host_exposed != 0UL))
  {
    g_ps_storage_joystick_calibration_probe.status =
      (uint32_t)PS_STATUS_VERIFY_FAILED;
    g_ps_storage_joystick_calibration_probe.selection_reason =
      PS_STORAGE_JOYSTICK_CALIBRATION_REASON_LAYOUT;
    return PS_STATUS_VERIFY_FAILED;
  }
  g_ps_storage_joystick_calibration_probe.region_start = region->start;
  g_ps_storage_joystick_calibration_probe.region_length = region->length;

  for (record_index = 0UL;
       record_index < PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_COUNT;
       ++record_index)
  {
    ps_status_t status;
    volatile ps_storage_joystick_calibration_record_probe_t *record =
      &g_ps_storage_joystick_calibration_probe.record[record_index];

    status = ps_storage_flash_block_read(
      block,
      region->start +
        (record_index * PS_STORAGE_JOYSTICK_CALIBRATION_SECTOR_SIZE),
      bytes,
      sizeof(bytes));
    record->read_status = (uint32_t)status;
    if (status != PS_STATUS_OK)
    {
      record->reason = PS_STORAGE_JOYSTICK_CALIBRATION_REASON_READ;
      g_ps_storage_joystick_calibration_probe.status = (uint32_t)status;
      g_ps_storage_joystick_calibration_probe.selection_reason =
        PS_STORAGE_JOYSTICK_CALIBRATION_REASON_READ;
      return status;
    }

    PS_StorageJoystickCalibration_DecodeRecord(bytes, record);
    PS_StorageJoystickCalibration_ValidateRecord(bytes, record);
    if (record->valid != 0UL)
    {
      g_ps_storage_joystick_calibration_probe.valid_record_count++;
      if ((selected ==
           PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION) ||
          (PS_StorageJoystickCalibration_IsGenerationNewer(
             record->generation,
             g_ps_storage_joystick_calibration_probe.record[
               selected].generation) != 0UL))
      {
        selected = record_index;
      }
    }
  }

  if ((g_ps_storage_joystick_calibration_probe.valid_record_count == 2UL) &&
      (g_ps_storage_joystick_calibration_probe.record[0].generation ==
       g_ps_storage_joystick_calibration_probe.record[1].generation) &&
      (memcmp(
         (const void *)&g_ps_storage_joystick_calibration_probe.record[0].calibration,
         (const void *)&g_ps_storage_joystick_calibration_probe.record[1].calibration,
         sizeof(ps_input_joystick_calibration_t)) != 0))
  {
    g_ps_storage_joystick_calibration_probe.selection_reason =
      PS_STORAGE_JOYSTICK_CALIBRATION_REASON_CONFLICT;
    g_ps_storage_joystick_calibration_probe.status =
      (uint32_t)PS_STATUS_OK;
    return PS_STATUS_OK;
  }

  if (selected != PS_STORAGE_JOYSTICK_CALIBRATION_INVALID_SELECTION)
  {
    g_ps_storage_joystick_calibration_probe.calibration_available = 1UL;
    g_ps_storage_joystick_calibration_probe.selected_record = selected;
    g_ps_storage_joystick_calibration_probe.selected_generation =
      g_ps_storage_joystick_calibration_probe.record[selected].generation;
    g_ps_storage_joystick_calibration_probe.selected_calibration =
      g_ps_storage_joystick_calibration_probe.record[selected].calibration;
  }
  g_ps_storage_joystick_calibration_probe.selection_reason =
    PS_STORAGE_JOYSTICK_CALIBRATION_REASON_NONE;
  g_ps_storage_joystick_calibration_probe.status =
    (uint32_t)PS_STATUS_OK;
  return PS_STATUS_OK;
}

ps_status_t PS_StorageJoystickCalibration_Save(
  ps_storage_flash_block_t *block,
  const ps_input_joystick_calibration_t *calibration)
{
  const ps_storage_region_t *region;
  ps_status_t status;
  uint32_t target_record;
  uint32_t target_generation;
  uint32_t target_address;
  uint32_t poll_count = 0UL;
  uint32_t mismatches = 0UL;
  uint32_t index;
  uint8_t marker[4];

  PS_StorageJoystickCalibration_ResetSaveProbe();
  if ((block == NULL) || (block->initialized == 0UL) ||
      (PS_StorageJoystickCalibration_PayloadValid(calibration) == 0UL))
  {
    g_ps_storage_joystick_calibration_save_probe.status =
      (uint32_t)PS_STATUS_INVALID_ARGUMENT;
    return PS_STATUS_INVALID_ARGUMENT;
  }
  region = PS_StorageJoystickCalibration_FindRegion();
  if ((region == NULL) ||
      (region->length <
       (PS_STORAGE_JOYSTICK_CALIBRATION_RECORD_COUNT *
        PS_STORAGE_JOYSTICK_CALIBRATION_SECTOR_SIZE)) ||
      (region->host_exposed != 0UL))
  {
    g_ps_storage_joystick_calibration_save_probe.status =
      (uint32_t)PS_STATUS_VERIFY_FAILED;
    return PS_STATUS_VERIFY_FAILED;
  }

  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_SCAN;
  status = PS_StorageJoystickCalibration_Scan(block);
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_joystick_calibration_save_probe.status = (uint32_t)status;
    return status;
  }
  if (g_ps_storage_joystick_calibration_probe.selection_reason ==
      PS_STORAGE_JOYSTICK_CALIBRATION_REASON_CONFLICT)
  {
    g_ps_storage_joystick_calibration_save_probe.status =
      (uint32_t)PS_STATUS_VERIFY_FAILED;
    return PS_STATUS_VERIFY_FAILED;
  }
  g_ps_storage_joystick_calibration_save_probe.source_record =
    g_ps_storage_joystick_calibration_probe.selected_record;
  if (g_ps_storage_joystick_calibration_probe.calibration_available == 0UL)
  {
    target_record = 0UL;
    target_generation = 1UL;
  }
  else
  {
    target_record =
      g_ps_storage_joystick_calibration_probe.selected_record ^ 1UL;
    target_generation =
      g_ps_storage_joystick_calibration_probe.selected_generation + 1UL;
  }
  target_address = region->start +
    (target_record * PS_STORAGE_JOYSTICK_CALIBRATION_SECTOR_SIZE);
  g_ps_storage_joystick_calibration_save_probe.target_record = target_record;
  g_ps_storage_joystick_calibration_save_probe.target_generation =
    target_generation;
  g_ps_storage_joystick_calibration_save_probe.target_address =
    target_address;

  PS_StorageJoystickCalibration_BuildRecord(calibration,
                                            target_generation);
  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_ERASE;
  status = ps_storage_flash_block_erase(
    block,
    target_address / block->geometry.erase_block_size,
    &poll_count);
  g_ps_storage_joystick_calibration_save_probe.erase_status =
    (uint32_t)status;
  g_ps_storage_joystick_calibration_save_probe.erase_poll_count = poll_count;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_joystick_calibration_save_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_PROGRAM;
  status = ps_storage_flash_block_program(
    block,
    target_address,
    ps_storage_joystick_calibration_body,
    sizeof(ps_storage_joystick_calibration_body));
  g_ps_storage_joystick_calibration_save_probe.program_status =
    (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_joystick_calibration_save_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_VERIFY;
  status = ps_storage_flash_block_read(
    block,
    target_address,
    ps_storage_joystick_calibration_verify,
    sizeof(ps_storage_joystick_calibration_verify));
  if (status == PS_STATUS_OK)
  {
    for (index = 0UL;
         index < sizeof(ps_storage_joystick_calibration_verify);
         ++index)
    {
      if (ps_storage_joystick_calibration_verify[index] !=
          ps_storage_joystick_calibration_body[index])
      {
        mismatches++;
      }
    }
    if (mismatches != 0UL)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }
  g_ps_storage_joystick_calibration_save_probe.verify_status =
    (uint32_t)status;
  g_ps_storage_joystick_calibration_save_probe.verify_mismatches =
    mismatches;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_joystick_calibration_save_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_COMMIT;
  PS_StorageJoystickCalibration_PutU32(
    marker,
    PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_MARKER);
  status = ps_storage_flash_block_program(
    block,
    target_address + PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_OFFSET,
    marker,
    sizeof(marker));
  g_ps_storage_joystick_calibration_save_probe.commit_status =
    (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_joystick_calibration_save_probe.status = (uint32_t)status;
    return status;
  }
  status = ps_storage_flash_block_read(
    block,
    target_address + PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_OFFSET,
    ps_storage_joystick_calibration_verify,
    sizeof(marker));
  g_ps_storage_joystick_calibration_save_probe.commit_verify_status =
    (uint32_t)status;
  if (status == PS_STATUS_OK)
  {
    g_ps_storage_joystick_calibration_save_probe.commit_marker =
      PS_StorageJoystickCalibration_U32(
        ps_storage_joystick_calibration_verify);
    if (g_ps_storage_joystick_calibration_save_probe.commit_marker !=
        PS_STORAGE_JOYSTICK_CALIBRATION_COMMIT_MARKER)
    {
      status = PS_STATUS_VERIFY_FAILED;
      g_ps_storage_joystick_calibration_save_probe.commit_verify_status =
        (uint32_t)status;
    }
  }
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_joystick_calibration_save_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_RESCAN;
  status = PS_StorageJoystickCalibration_Scan(block);
  g_ps_storage_joystick_calibration_save_probe.rescan_status =
    (uint32_t)status;
  g_ps_storage_joystick_calibration_save_probe.selected_record =
    g_ps_storage_joystick_calibration_probe.selected_record;
  g_ps_storage_joystick_calibration_save_probe.selected_generation =
    g_ps_storage_joystick_calibration_probe.selected_generation;
  if ((status != PS_STATUS_OK) ||
      (g_ps_storage_joystick_calibration_probe.calibration_available == 0UL) ||
      (g_ps_storage_joystick_calibration_probe.selected_record !=
       target_record) ||
      (g_ps_storage_joystick_calibration_probe.selected_generation !=
       target_generation) ||
      (memcmp(
         (const void *)&g_ps_storage_joystick_calibration_probe.selected_calibration,
         calibration,
         sizeof(*calibration)) != 0))
  {
    status = (status == PS_STATUS_OK) ? PS_STATUS_VERIFY_FAILED : status;
    g_ps_storage_joystick_calibration_save_probe.status =
      (uint32_t)status;
    return status;
  }

  g_ps_storage_joystick_calibration_save_probe.stage =
    PS_STORAGE_JOYSTICK_CALIBRATION_SAVE_STAGE_COMPLETE;
  g_ps_storage_joystick_calibration_save_probe.status =
    (uint32_t)PS_STATUS_OK;
  return PS_STATUS_OK;
}
