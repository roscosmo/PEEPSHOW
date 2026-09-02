#include "ps_storage_package_index.h"

#include <stddef.h>
#include <string.h>

#include "ps_storage_layout.h"

#define PS_STORAGE_PACKAGE_INDEX_MAGIC          (0x31494745UL)
#define PS_STORAGE_PACKAGE_INDEX_FORMAT_VERSION (1U)
#define PS_STORAGE_PACKAGE_INDEX_COMMIT_MARKER  (0x54494D43UL)
#define PS_STORAGE_PACKAGE_INDEX_MIN_PACKAGE_SIZE (104UL)

#define PS_STORAGE_PACKAGE_INDEX_MAGIC_OFFSET       (0UL)
#define PS_STORAGE_PACKAGE_INDEX_VERSION_OFFSET     (4UL)
#define PS_STORAGE_PACKAGE_INDEX_BODY_SIZE_OFFSET   (6UL)
#define PS_STORAGE_PACKAGE_INDEX_GENERATION_OFFSET  (8UL)
#define PS_STORAGE_PACKAGE_INDEX_SLOT_OFFSET        (12UL)
#define PS_STORAGE_PACKAGE_INDEX_PACKAGE_SIZE_OFFSET (16UL)
#define PS_STORAGE_PACKAGE_INDEX_ID_LOW_OFFSET      (20UL)
#define PS_STORAGE_PACKAGE_INDEX_ID_HIGH_OFFSET     (24UL)
#define PS_STORAGE_PACKAGE_INDEX_FLAGS_OFFSET       (28UL)
#define PS_STORAGE_PACKAGE_INDEX_SHA256_OFFSET      (32UL)
#define PS_STORAGE_PACKAGE_INDEX_CRC32_OFFSET       (64UL)
#define PS_STORAGE_PACKAGE_INDEX_RESERVED_OFFSET    (68UL)

#define PS_STORAGE_PACKAGE_HEADER_SIZE               (64UL)
#define PS_STORAGE_PACKAGE_FOOTER_SIZE               (40UL)
#define PS_STORAGE_PACKAGE_DECLARED_SIZE_OFFSET      (8UL)
#define PS_STORAGE_PACKAGE_FOOTER_OFFSET_OFFSET      (16UL)
#define PS_STORAGE_PACKAGE_ID_HASH_OFFSET             (36UL)
#define PS_STORAGE_PACKAGE_FOOTER_SHA256_OFFSET       (8UL)
#define PS_STORAGE_PACKAGE_VERIFY_BUFFER_SIZE         (256UL)

volatile ps_storage_package_index_probe_t
  g_ps_storage_package_index_probe =
{
  .api_version = PS_STORAGE_PACKAGE_INDEX_API_VERSION,
  .status = (uint32_t)PS_STATUS_NOT_INITIALIZED,
  .selected_record = PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION,
  .selected_slot = PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION,
  .selection_reason = PS_STORAGE_PACKAGE_INDEX_REASON_NONE
};

volatile ps_storage_package_install_probe_t
  g_ps_storage_package_install_probe =
{
  .api_version = PS_STORAGE_PACKAGE_INDEX_API_VERSION,
  .status = (uint32_t)PS_STATUS_NOT_INITIALIZED,
  .stage = PS_STORAGE_PACKAGE_INSTALL_STAGE_IDLE,
  .source_record = PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION,
  .source_slot = PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION,
  .target_record = PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION,
  .target_slot = PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION
};

static uint8_t ps_storage_package_index_body[
  PS_STORAGE_PACKAGE_INDEX_RECORD_BODY_SIZE];
static uint8_t ps_storage_package_index_verify[
  PS_STORAGE_PACKAGE_VERIFY_BUFFER_SIZE];

static uint16_t PS_StoragePackageIndex_U16(const uint8_t *bytes)
{
  return (uint16_t)((uint16_t)bytes[0] |
                    ((uint16_t)bytes[1] << 8));
}

static uint32_t PS_StoragePackageIndex_U32(const uint8_t *bytes)
{
  return (uint32_t)bytes[0] |
         ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) |
         ((uint32_t)bytes[3] << 24);
}

static void PS_StoragePackageIndex_PutU16(uint8_t *bytes, uint16_t value)
{
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
}

static void PS_StoragePackageIndex_PutU32(uint8_t *bytes, uint32_t value)
{
  bytes[0] = (uint8_t)value;
  bytes[1] = (uint8_t)(value >> 8);
  bytes[2] = (uint8_t)(value >> 16);
  bytes[3] = (uint8_t)(value >> 24);
}

static uint32_t PS_StoragePackageIndex_Crc32(const uint8_t *body)
{
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t index;

  for (index = 0UL;
       index < PS_STORAGE_PACKAGE_INDEX_RECORD_BODY_SIZE;
       ++index)
  {
    uint32_t bit;
    uint8_t value = body[index];

    if ((index >= PS_STORAGE_PACKAGE_INDEX_CRC32_OFFSET) &&
        (index < (PS_STORAGE_PACKAGE_INDEX_CRC32_OFFSET + 4UL)))
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

static const ps_storage_region_t *PS_StoragePackageIndex_FindRegion(
  ps_storage_region_id_t id)
{
  const ps_storage_region_t *regions;
  uint32_t count;
  uint32_t index;

  regions = ps_storage_layout_regions(&count);
  for (index = 0UL; index < count; ++index)
  {
    if (regions[index].id == id)
    {
      return &regions[index];
    }
  }
  return NULL;
}

static void PS_StoragePackageIndex_ResetProbe(void)
{
  uint32_t scan_count = g_ps_storage_package_index_probe.scan_count + 1UL;
  uint32_t index;

  (void)memset((void *)&g_ps_storage_package_index_probe,
               0,
               sizeof(g_ps_storage_package_index_probe));
  g_ps_storage_package_index_probe.api_version =
    PS_STORAGE_PACKAGE_INDEX_API_VERSION;
  g_ps_storage_package_index_probe.scan_count = scan_count;
  g_ps_storage_package_index_probe.status =
    (uint32_t)PS_STATUS_INTERNAL_ERROR;
  g_ps_storage_package_index_probe.selected_record =
    PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;
  g_ps_storage_package_index_probe.selected_slot =
    PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;
  for (index = 0UL;
       index < PS_STORAGE_PACKAGE_INDEX_RECORD_COUNT;
       ++index)
  {
    g_ps_storage_package_index_probe.record[index].read_status =
      (uint32_t)PS_STATUS_NOT_INITIALIZED;
  }
}

static uint32_t PS_StoragePackageIndex_IsGenerationNewer(uint32_t candidate,
                                                         uint32_t current)
{
  uint32_t difference = candidate - current;

  return ((difference != 0UL) && (difference < 0x80000000UL)) ? 1UL : 0UL;
}

static uint32_t PS_StoragePackageIndex_RecordsMatch(
  const volatile ps_storage_package_index_record_probe_t *left,
  const volatile ps_storage_package_index_record_probe_t *right)
{
  uint32_t index;

  if ((left->active_slot != right->active_slot) ||
      (left->package_size != right->package_size) ||
      (left->package_id_hash_low != right->package_id_hash_low) ||
      (left->package_id_hash_high != right->package_id_hash_high) ||
      (left->flags != right->flags))
  {
    return 0UL;
  }
  for (index = 0UL; index < 8UL; ++index)
  {
    if (left->package_sha256_words[index] !=
        right->package_sha256_words[index])
    {
      return 0UL;
    }
  }
  return 1UL;
}

static void PS_StoragePackageIndex_ResetInstallProbe(void)
{
  uint32_t install_count =
    g_ps_storage_package_install_probe.install_count + 1UL;

  (void)memset((void *)&g_ps_storage_package_install_probe,
               0,
               sizeof(g_ps_storage_package_install_probe));
  g_ps_storage_package_install_probe.api_version =
    PS_STORAGE_PACKAGE_INDEX_API_VERSION;
  g_ps_storage_package_install_probe.install_count = install_count;
  g_ps_storage_package_install_probe.status =
    (uint32_t)PS_STATUS_INTERNAL_ERROR;
  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_VALIDATE;
  g_ps_storage_package_install_probe.source_record =
    PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;
  g_ps_storage_package_install_probe.source_slot =
    PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;
  g_ps_storage_package_install_probe.target_record =
    PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;
  g_ps_storage_package_install_probe.target_slot =
    PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;
  g_ps_storage_package_install_probe.package_erase_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.package_program_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.package_program_failure_offset =
    PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;
  g_ps_storage_package_install_probe.package_verify_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.index_erase_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.index_program_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.index_verify_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.commit_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.commit_verify_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
  g_ps_storage_package_install_probe.rescan_status =
    (uint32_t)PS_STATUS_NOT_INITIALIZED;
}

static ps_status_t PS_StoragePackageIndex_VerifyBytes(
  ps_storage_flash_block_t *block,
  uint32_t address,
  const uint8_t *expected,
  uint32_t length,
  uint32_t *verified_bytes,
  uint32_t *mismatches)
{
  uint32_t offset;

  *verified_bytes = 0UL;
  *mismatches = 0UL;
  for (offset = 0UL; offset < length; offset += sizeof(ps_storage_package_index_verify))
  {
    ps_status_t status;
    uint32_t index;
    uint32_t chunk = length - offset;

    if (chunk > sizeof(ps_storage_package_index_verify))
    {
      chunk = sizeof(ps_storage_package_index_verify);
    }
    status = ps_storage_flash_block_read(block,
                                         address + offset,
                                         ps_storage_package_index_verify,
                                         chunk);
    if (status != PS_STATUS_OK)
    {
      return status;
    }
    for (index = 0UL; index < chunk; ++index)
    {
      if (ps_storage_package_index_verify[index] != expected[offset + index])
      {
        (*mismatches)++;
      }
    }
    *verified_bytes += chunk;
  }
  return (*mismatches == 0UL) ? PS_STATUS_OK : PS_STATUS_VERIFY_FAILED;
}

static void PS_StoragePackageIndex_BuildRecord(
  const uint8_t *package,
  uint32_t package_size,
  uint32_t footer_offset,
  uint32_t generation,
  uint32_t slot)
{
  uint32_t crc;

  (void)memset(ps_storage_package_index_body,
               0xFF,
               sizeof(ps_storage_package_index_body));
  PS_StoragePackageIndex_PutU32(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_MAGIC_OFFSET],
    PS_STORAGE_PACKAGE_INDEX_MAGIC);
  PS_StoragePackageIndex_PutU16(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_VERSION_OFFSET],
    PS_STORAGE_PACKAGE_INDEX_FORMAT_VERSION);
  PS_StoragePackageIndex_PutU16(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_BODY_SIZE_OFFSET],
    PS_STORAGE_PACKAGE_INDEX_RECORD_BODY_SIZE);
  PS_StoragePackageIndex_PutU32(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_GENERATION_OFFSET],
    generation);
  PS_StoragePackageIndex_PutU32(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_SLOT_OFFSET],
    slot);
  PS_StoragePackageIndex_PutU32(
    &ps_storage_package_index_body[
      PS_STORAGE_PACKAGE_INDEX_PACKAGE_SIZE_OFFSET],
    package_size);
  (void)memcpy(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_ID_LOW_OFFSET],
    &package[PS_STORAGE_PACKAGE_ID_HASH_OFFSET],
    8UL);
  PS_StoragePackageIndex_PutU32(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_FLAGS_OFFSET],
    0UL);
  (void)memcpy(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_SHA256_OFFSET],
    &package[footer_offset + PS_STORAGE_PACKAGE_FOOTER_SHA256_OFFSET],
    32UL);
  PS_StoragePackageIndex_PutU32(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_CRC32_OFFSET],
    0UL);
  crc = PS_StoragePackageIndex_Crc32(ps_storage_package_index_body);
  PS_StoragePackageIndex_PutU32(
    &ps_storage_package_index_body[PS_STORAGE_PACKAGE_INDEX_CRC32_OFFSET],
    crc);
  g_ps_storage_package_install_probe.index_crc32 = crc;
}

static void PS_StoragePackageIndex_DecodeRecord(
  const uint8_t *bytes,
  volatile ps_storage_package_index_record_probe_t *record)
{
  uint32_t index;

  record->magic = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_MAGIC_OFFSET]);
  record->commit_marker = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_COMMIT_OFFSET]);
  record->generation = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_GENERATION_OFFSET]);
  record->active_slot = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_SLOT_OFFSET]);
  record->package_size = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_PACKAGE_SIZE_OFFSET]);
  record->package_id_hash_low = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_ID_LOW_OFFSET]);
  record->package_id_hash_high = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_ID_HIGH_OFFSET]);
  record->flags = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_FLAGS_OFFSET]);
  record->stored_crc32 = PS_StoragePackageIndex_U32(
    &bytes[PS_STORAGE_PACKAGE_INDEX_CRC32_OFFSET]);
  record->computed_crc32 = PS_StoragePackageIndex_Crc32(bytes);
  for (index = 0UL; index < 8UL; ++index)
  {
    record->package_sha256_words[index] = PS_StoragePackageIndex_U32(
      &bytes[PS_STORAGE_PACKAGE_INDEX_SHA256_OFFSET + (index * 4UL)]);
  }
}

static void PS_StoragePackageIndex_ValidateRecord(
  const uint8_t *bytes,
  volatile ps_storage_package_index_record_probe_t *record)
{
  uint32_t index;

  if (record->commit_marker != PS_STORAGE_PACKAGE_INDEX_COMMIT_MARKER)
  {
    record->reason = PS_STORAGE_PACKAGE_INDEX_REASON_UNCOMMITTED;
    return;
  }
  if ((record->magic != PS_STORAGE_PACKAGE_INDEX_MAGIC) ||
      (PS_StoragePackageIndex_U16(
         &bytes[PS_STORAGE_PACKAGE_INDEX_VERSION_OFFSET]) !=
       PS_STORAGE_PACKAGE_INDEX_FORMAT_VERSION) ||
      (PS_StoragePackageIndex_U16(
         &bytes[PS_STORAGE_PACKAGE_INDEX_BODY_SIZE_OFFSET]) !=
       PS_STORAGE_PACKAGE_INDEX_RECORD_BODY_SIZE) ||
      (record->flags != 0UL))
  {
    record->reason = PS_STORAGE_PACKAGE_INDEX_REASON_FORMAT;
    return;
  }
  if (record->stored_crc32 != record->computed_crc32)
  {
    record->reason = PS_STORAGE_PACKAGE_INDEX_REASON_CRC;
    return;
  }
  if ((record->active_slot >= PS_STORAGE_PACKAGE_SLOT_COUNT) ||
      (record->package_size < PS_STORAGE_PACKAGE_INDEX_MIN_PACKAGE_SIZE) ||
      (record->package_size > PS_STORAGE_PACKAGE_SLOT_SIZE))
  {
    record->reason = PS_STORAGE_PACKAGE_INDEX_REASON_BOUNDS;
    return;
  }
  for (index = PS_STORAGE_PACKAGE_INDEX_RESERVED_OFFSET;
       index < PS_STORAGE_PACKAGE_INDEX_RECORD_BODY_SIZE;
       ++index)
  {
    if (bytes[index] != 0xFFU)
    {
      record->reason = PS_STORAGE_PACKAGE_INDEX_REASON_RESERVED;
      return;
    }
  }
  record->valid = 1UL;
  record->reason = PS_STORAGE_PACKAGE_INDEX_REASON_NONE;
}

ps_status_t PS_StoragePackageIndex_Scan(ps_storage_flash_block_t *block)
{
  const ps_storage_region_t *index_region;
  const ps_storage_region_t *package_region;
  uint8_t bytes[PS_STORAGE_PACKAGE_INDEX_READ_SIZE];
  uint32_t record_index;
  uint32_t selected = PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION;

  PS_StoragePackageIndex_ResetProbe();
  if ((block == NULL) || (block->initialized == 0UL))
  {
    g_ps_storage_package_index_probe.status =
      (uint32_t)PS_STATUS_INVALID_ARGUMENT;
    g_ps_storage_package_index_probe.selection_reason =
      PS_STORAGE_PACKAGE_INDEX_REASON_ARGUMENT;
    return PS_STATUS_INVALID_ARGUMENT;
  }

  index_region = PS_StoragePackageIndex_FindRegion(
    PS_STORAGE_REGION_PACKAGE_INDEX);
  package_region = PS_StoragePackageIndex_FindRegion(
    PS_STORAGE_REGION_INSTALLED_PACKAGE);
  if ((index_region == NULL) || (package_region == NULL) ||
      (index_region->length <
       (PS_STORAGE_PACKAGE_INDEX_RECORD_COUNT *
        PS_STORAGE_PACKAGE_INDEX_SECTOR_SIZE)) ||
      (package_region->length !=
       (PS_STORAGE_PACKAGE_SLOT_COUNT * PS_STORAGE_PACKAGE_SLOT_SIZE)))
  {
    g_ps_storage_package_index_probe.status =
      (uint32_t)PS_STATUS_VERIFY_FAILED;
    g_ps_storage_package_index_probe.selection_reason =
      PS_STORAGE_PACKAGE_INDEX_REASON_LAYOUT;
    return PS_STATUS_VERIFY_FAILED;
  }

  g_ps_storage_package_index_probe.index_region_start = index_region->start;
  g_ps_storage_package_index_probe.index_region_length = index_region->length;
  g_ps_storage_package_index_probe.package_region_start = package_region->start;
  g_ps_storage_package_index_probe.package_region_length = package_region->length;

  for (record_index = 0UL;
       record_index < PS_STORAGE_PACKAGE_INDEX_RECORD_COUNT;
       ++record_index)
  {
    ps_status_t status;
    volatile ps_storage_package_index_record_probe_t *record =
      &g_ps_storage_package_index_probe.record[record_index];

    status = ps_storage_flash_block_read(
      block,
      index_region->start +
        (record_index * PS_STORAGE_PACKAGE_INDEX_SECTOR_SIZE),
      bytes,
      sizeof(bytes));
    record->read_status = (uint32_t)status;
    if (status != PS_STATUS_OK)
    {
      record->reason = PS_STORAGE_PACKAGE_INDEX_REASON_READ;
      g_ps_storage_package_index_probe.status = (uint32_t)status;
      g_ps_storage_package_index_probe.selection_reason =
        PS_STORAGE_PACKAGE_INDEX_REASON_READ;
      return status;
    }

    PS_StoragePackageIndex_DecodeRecord(bytes, record);
    PS_StoragePackageIndex_ValidateRecord(bytes, record);
    if (record->valid != 0UL)
    {
      g_ps_storage_package_index_probe.valid_record_count++;
      if ((selected == PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION) ||
          (PS_StoragePackageIndex_IsGenerationNewer(
             record->generation,
             g_ps_storage_package_index_probe.record[selected].generation) !=
           0UL))
      {
        selected = record_index;
      }
    }
  }

  if ((g_ps_storage_package_index_probe.valid_record_count == 2UL) &&
      (g_ps_storage_package_index_probe.record[0].generation ==
       g_ps_storage_package_index_probe.record[1].generation) &&
      (PS_StoragePackageIndex_RecordsMatch(
         &g_ps_storage_package_index_probe.record[0],
         &g_ps_storage_package_index_probe.record[1]) == 0UL))
  {
    g_ps_storage_package_index_probe.selection_reason =
      PS_STORAGE_PACKAGE_INDEX_REASON_CONFLICT;
    g_ps_storage_package_index_probe.status = (uint32_t)PS_STATUS_OK;
    return PS_STATUS_OK;
  }

  if (selected != PS_STORAGE_PACKAGE_INDEX_INVALID_SELECTION)
  {
    volatile ps_storage_package_index_record_probe_t *record =
      &g_ps_storage_package_index_probe.record[selected];

    g_ps_storage_package_index_probe.installed_available = 1UL;
    g_ps_storage_package_index_probe.selected_record = selected;
    g_ps_storage_package_index_probe.selected_slot = record->active_slot;
    g_ps_storage_package_index_probe.selected_generation = record->generation;
    g_ps_storage_package_index_probe.selected_package_start =
      package_region->start +
      (record->active_slot * PS_STORAGE_PACKAGE_SLOT_SIZE);
    g_ps_storage_package_index_probe.selected_package_size =
      record->package_size;
  }
  g_ps_storage_package_index_probe.selection_reason =
    PS_STORAGE_PACKAGE_INDEX_REASON_NONE;
  g_ps_storage_package_index_probe.status = (uint32_t)PS_STATUS_OK;
  return PS_STATUS_OK;
}

ps_status_t PS_StoragePackageIndex_InstallValidated(
  ps_storage_flash_block_t *block,
  const uint8_t *package,
  uint32_t package_size)
{
  const ps_storage_region_t *index_region;
  const ps_storage_region_t *package_region;
  ps_status_t status;
  uint32_t footer_offset;
  uint32_t target_record;
  uint32_t target_slot;
  uint32_t target_generation;
  uint32_t target_package_start;
  uint32_t target_index_start;
  uint32_t erase_index;
  uint32_t erase_count;
  uint32_t poll_count;
  uint32_t program_offset;
  uint32_t verified_bytes;
  uint32_t mismatches;
  uint8_t marker[4];

  PS_StoragePackageIndex_ResetInstallProbe();
  g_ps_storage_package_install_probe.package_size = package_size;
  if ((block == NULL) || (block->initialized == 0UL) ||
      (package == NULL) ||
      (package_size < (PS_STORAGE_PACKAGE_HEADER_SIZE +
                       PS_STORAGE_PACKAGE_FOOTER_SIZE)) ||
      (package_size > PS_STORAGE_PACKAGE_SLOT_SIZE) ||
      (memcmp(package, "PKG1", 4UL) != 0) ||
      (PS_StoragePackageIndex_U32(
         &package[PS_STORAGE_PACKAGE_DECLARED_SIZE_OFFSET]) != package_size))
  {
    g_ps_storage_package_install_probe.status =
      (uint32_t)PS_STATUS_INVALID_ARGUMENT;
    return PS_STATUS_INVALID_ARGUMENT;
  }
  footer_offset = PS_StoragePackageIndex_U32(
    &package[PS_STORAGE_PACKAGE_FOOTER_OFFSET_OFFSET]);
  if ((footer_offset != (package_size - PS_STORAGE_PACKAGE_FOOTER_SIZE)) ||
      (memcmp(&package[footer_offset], "END1", 4UL) != 0))
  {
    g_ps_storage_package_install_probe.status =
      (uint32_t)PS_STATUS_VERIFY_FAILED;
    return PS_STATUS_VERIFY_FAILED;
  }
  g_ps_storage_package_install_probe.package_id_hash_low =
    PS_StoragePackageIndex_U32(&package[PS_STORAGE_PACKAGE_ID_HASH_OFFSET]);
  g_ps_storage_package_install_probe.package_id_hash_high =
    PS_StoragePackageIndex_U32(&package[PS_STORAGE_PACKAGE_ID_HASH_OFFSET + 4UL]);

  index_region = PS_StoragePackageIndex_FindRegion(
    PS_STORAGE_REGION_PACKAGE_INDEX);
  package_region = PS_StoragePackageIndex_FindRegion(
    PS_STORAGE_REGION_INSTALLED_PACKAGE);
  if ((index_region == NULL) || (package_region == NULL))
  {
    g_ps_storage_package_install_probe.status =
      (uint32_t)PS_STATUS_VERIFY_FAILED;
    return PS_STATUS_VERIFY_FAILED;
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_SCAN;
  status = PS_StoragePackageIndex_Scan(block);
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }
  g_ps_storage_package_install_probe.source_record =
    g_ps_storage_package_index_probe.selected_record;
  g_ps_storage_package_install_probe.source_slot =
    g_ps_storage_package_index_probe.selected_slot;
  if (g_ps_storage_package_index_probe.installed_available == 0UL)
  {
    target_record = 0UL;
    target_slot = 0UL;
    target_generation = 1UL;
  }
  else
  {
    target_record = g_ps_storage_package_index_probe.selected_record ^ 1UL;
    target_slot = g_ps_storage_package_index_probe.selected_slot ^ 1UL;
    target_generation =
      g_ps_storage_package_index_probe.selected_generation + 1UL;
  }
  target_package_start = package_region->start +
                         (target_slot * PS_STORAGE_PACKAGE_SLOT_SIZE);
  target_index_start = index_region->start +
                       (target_record * PS_STORAGE_PACKAGE_INDEX_SECTOR_SIZE);
  g_ps_storage_package_install_probe.target_record = target_record;
  g_ps_storage_package_install_probe.target_slot = target_slot;
  g_ps_storage_package_install_probe.target_generation = target_generation;
  g_ps_storage_package_install_probe.target_package_start =
    target_package_start;
  g_ps_storage_package_install_probe.target_index_start = target_index_start;

  erase_count = (package_size + block->geometry.erase_block_size - 1UL) /
                block->geometry.erase_block_size;
  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_ERASE_PACKAGE;
  for (erase_index = 0UL; erase_index < erase_count; ++erase_index)
  {
    poll_count = 0UL;
    status = ps_storage_flash_block_erase(
      block,
      (target_package_start / block->geometry.erase_block_size) + erase_index,
      &poll_count);
    g_ps_storage_package_install_probe.package_erase_count++;
    g_ps_storage_package_install_probe.package_erase_poll_count += poll_count;
    g_ps_storage_package_install_probe.package_erase_status =
      (uint32_t)status;
    if (status != PS_STATUS_OK)
    {
      g_ps_storage_package_install_probe.status = (uint32_t)status;
      return status;
    }
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_PROGRAM_PACKAGE;
  for (program_offset = 0UL;
       program_offset < package_size;
       program_offset += block->geometry.program_page_size)
  {
    uint32_t program_length = package_size - program_offset;

    if (program_length > block->geometry.program_page_size)
    {
      program_length = block->geometry.program_page_size;
    }
    g_ps_storage_package_install_probe.package_program_block_initialized =
      block->initialized;
    g_ps_storage_package_install_probe.package_program_device_initialized =
      (block->flash != NULL) ? block->flash->initialized : 0UL;
    g_ps_storage_package_install_probe.package_program_device_state =
      (block->flash != NULL) ? (uint32_t)block->flash->state : 0UL;
    g_ps_storage_package_install_probe.package_program_device_status =
      (block->flash != NULL) ? block->flash->last_status :
      (uint32_t)PS_STATUS_INVALID_ARGUMENT;
    status = ps_storage_flash_block_program(
      block,
      target_package_start + program_offset,
      &package[program_offset],
      program_length);
    g_ps_storage_package_install_probe.package_program_status =
      (uint32_t)status;
    if (status != PS_STATUS_OK)
    {
      g_ps_storage_package_install_probe.package_program_failure_offset =
        program_offset;
      g_ps_storage_package_install_probe.package_program_block_initialized =
        block->initialized;
      g_ps_storage_package_install_probe.package_program_device_initialized =
        (block->flash != NULL) ? block->flash->initialized : 0UL;
      g_ps_storage_package_install_probe.package_program_device_state =
        (block->flash != NULL) ? (uint32_t)block->flash->state : 0UL;
      g_ps_storage_package_install_probe.package_program_device_status =
        (block->flash != NULL) ? block->flash->last_status :
        (uint32_t)PS_STATUS_INVALID_ARGUMENT;
      g_ps_storage_package_install_probe.status = (uint32_t)status;
      return status;
    }
    g_ps_storage_package_install_probe.package_program_bytes +=
      program_length;
    g_ps_storage_package_install_probe.package_program_page_count++;
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_VERIFY_PACKAGE;
  status = PS_StoragePackageIndex_VerifyBytes(block,
                                              target_package_start,
                                              package,
                                              package_size,
                                              &verified_bytes,
                                              &mismatches);
  g_ps_storage_package_install_probe.package_verify_status =
    (uint32_t)status;
  g_ps_storage_package_install_probe.package_verify_bytes = verified_bytes;
  g_ps_storage_package_install_probe.package_verify_mismatches = mismatches;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }

  PS_StoragePackageIndex_BuildRecord(package,
                                     package_size,
                                     footer_offset,
                                     target_generation,
                                     target_slot);
  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_ERASE_INDEX;
  poll_count = 0UL;
  status = ps_storage_flash_block_erase(
    block,
    target_index_start / block->geometry.erase_block_size,
    &poll_count);
  g_ps_storage_package_install_probe.index_erase_status = (uint32_t)status;
  g_ps_storage_package_install_probe.index_erase_poll_count = poll_count;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_PROGRAM_INDEX;
  status = ps_storage_flash_block_program(block,
                                          target_index_start,
                                          ps_storage_package_index_body,
                                          sizeof(ps_storage_package_index_body));
  g_ps_storage_package_install_probe.index_program_status = (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_VERIFY_INDEX;
  status = PS_StoragePackageIndex_VerifyBytes(
    block,
    target_index_start,
    ps_storage_package_index_body,
    sizeof(ps_storage_package_index_body),
    &verified_bytes,
    &mismatches);
  g_ps_storage_package_install_probe.index_verify_status = (uint32_t)status;
  g_ps_storage_package_install_probe.index_verify_mismatches = mismatches;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_COMMIT;
  PS_StoragePackageIndex_PutU32(marker,
                                PS_STORAGE_PACKAGE_INDEX_COMMIT_MARKER);
  status = ps_storage_flash_block_program(
    block,
    target_index_start + PS_STORAGE_PACKAGE_INDEX_COMMIT_OFFSET,
    marker,
    sizeof(marker));
  g_ps_storage_package_install_probe.commit_status = (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }
  status = ps_storage_flash_block_read(
    block,
    target_index_start + PS_STORAGE_PACKAGE_INDEX_COMMIT_OFFSET,
    ps_storage_package_index_verify,
    sizeof(marker));
  g_ps_storage_package_install_probe.commit_verify_status = (uint32_t)status;
  if (status == PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.commit_marker =
      PS_StoragePackageIndex_U32(ps_storage_package_index_verify);
    if (g_ps_storage_package_install_probe.commit_marker !=
        PS_STORAGE_PACKAGE_INDEX_COMMIT_MARKER)
    {
      status = PS_STATUS_VERIFY_FAILED;
      g_ps_storage_package_install_probe.commit_verify_status =
        (uint32_t)status;
    }
  }
  if (status != PS_STATUS_OK)
  {
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_RESCAN;
  status = PS_StoragePackageIndex_Scan(block);
  g_ps_storage_package_install_probe.rescan_status = (uint32_t)status;
  g_ps_storage_package_install_probe.selected_record =
    g_ps_storage_package_index_probe.selected_record;
  g_ps_storage_package_install_probe.selected_slot =
    g_ps_storage_package_index_probe.selected_slot;
  g_ps_storage_package_install_probe.selected_generation =
    g_ps_storage_package_index_probe.selected_generation;
  if ((status != PS_STATUS_OK) ||
      (g_ps_storage_package_index_probe.installed_available == 0UL) ||
      (g_ps_storage_package_index_probe.selected_record != target_record) ||
      (g_ps_storage_package_index_probe.selected_slot != target_slot) ||
      (g_ps_storage_package_index_probe.selected_generation !=
       target_generation) ||
      (g_ps_storage_package_index_probe.selected_package_size != package_size))
  {
    status = (status == PS_STATUS_OK) ? PS_STATUS_VERIFY_FAILED : status;
    g_ps_storage_package_install_probe.status = (uint32_t)status;
    return status;
  }

  g_ps_storage_package_install_probe.stage =
    PS_STORAGE_PACKAGE_INSTALL_STAGE_COMPLETE;
  g_ps_storage_package_install_probe.status = (uint32_t)PS_STATUS_OK;
  return PS_STATUS_OK;
}
