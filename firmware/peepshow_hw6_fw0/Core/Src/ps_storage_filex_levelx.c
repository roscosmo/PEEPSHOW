#include "ps_storage_filex_levelx.h"

#include <stdint.h>
#include <string.h>

#include "fx_api.h"
#include "lx_api.h"
#include "ps_storage_msc_bridge.h"
#include "ps_hw6_trace.h"

#define PS_STORAGE_FXLX_TEST_LENGTH          (0x00500000UL)
#define PS_STORAGE_FXLX_SECTOR_SIZE          (512UL)
#define PS_STORAGE_FXLX_FAKE_BASE            (0x10000000UL)
#define PS_STORAGE_FXLX_MEDIA_MEMORY_SIZE    (4096UL)
#define PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE    (256UL)
#define PS_STORAGE_FXLX_NOT_RUN              (0xFFFFFFFFUL)
#define PS_STORAGE_FXLX_VOLUME_NAME          "PEEPSHOW   "
#define PS_STORAGE_FXLX_MSC_STAGE_IDLE       (0UL)
#define PS_STORAGE_FXLX_MSC_STAGE_VALIDATE   (1UL)
#define PS_STORAGE_FXLX_MSC_STAGE_OPENED_OLD (2UL)
#define PS_STORAGE_FXLX_MSC_STAGE_LX_INIT    (3UL)
#define PS_STORAGE_FXLX_MSC_STAGE_LX_OPEN    (4UL)
#define PS_STORAGE_FXLX_MSC_STAGE_OPENED     (5UL)
#define PS_STORAGE_FXLX_MSC_STAGE_CLOSE      (6UL)
#define PS_STORAGE_FXLX_MSC_STAGE_FAULT      (7UL)
#define PS_STORAGE_FXLX_NUM_FATS             (2U)
#define PS_STORAGE_FXLX_DIR_ENTRIES          (256U)
#define PS_STORAGE_FXLX_SECTORS_PER_CLUSTER  (8U)
#define PS_STORAGE_FXLX_FILE_NAME            "HW6_FXLX.TXT"

static ps_storage_flash_block_t *ps_storage_fxlx_block;
static uint32_t ps_storage_fxlx_start;
static uint32_t ps_storage_fxlx_length;
static LX_NOR_FLASH ps_storage_fxlx_nor;
static FX_MEDIA ps_storage_fxlx_media;
static FX_FILE ps_storage_fxlx_file;
static ULONG ps_storage_fxlx_sector_buffer[LX_NOR_SECTOR_SIZE];
static UCHAR ps_storage_fxlx_media_memory[PS_STORAGE_FXLX_MEDIA_MEMORY_SIZE];
static UCHAR ps_storage_fxlx_write_buffer[PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE];
static UCHAR ps_storage_fxlx_read_buffer[PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE];
static ps_storage_filex_levelx_smoke_result_t *ps_storage_fxlx_result;
static uint32_t ps_storage_fxlx_levelx_initialized;
static uint32_t ps_storage_fxlx_msc_opened;

volatile ps_storage_filex_levelx_msc_probe_t
  g_ps_storage_filex_levelx_msc_probe;

static void PS_StorageFxLx_RecordMscHardware(
  ps_storage_flash_block_t *block)
{
  volatile ps_storage_filex_levelx_msc_probe_t *probe =
    &g_ps_storage_filex_levelx_msc_probe;

  if (block != NULL)
  {
    probe->block_last_status = block->last_status;
    if (block->flash != NULL)
    {
      probe->flash_state = block->flash->state;
      probe->flash_last_status = block->flash->last_status;
      if (block->flash->ospi != NULL)
      {
        probe->ospi_state_after =
          (uint32_t)HAL_OSPI_GetState(block->flash->ospi);
        probe->ospi_error_after = HAL_OSPI_GetError(block->flash->ospi);
      }
    }
  }
  probe->nor_state = (uint32_t)ps_storage_fxlx_nor.lx_nor_flash_state;
}

static void PS_StorageFxLx_ResetMscProbe(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region)
{
  uint32_t open_count = g_ps_storage_filex_levelx_msc_probe.open_count;
  uint32_t close_count = g_ps_storage_filex_levelx_msc_probe.close_count;
  volatile ps_storage_filex_levelx_msc_probe_t *probe =
    &g_ps_storage_filex_levelx_msc_probe;

  (void)memset((void *)probe, 0, sizeof(*probe));
  probe->api_version = PS_STORAGE_FILEX_LEVELX_MSC_PROBE_API_VERSION;
  probe->open_count = open_count + 1UL;
  probe->close_count = close_count;
  probe->status = (uint32_t)PS_STATUS_INTERNAL_ERROR;
  probe->validate_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->recovery_lx_open_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->recovery_driver_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->lx_initialize_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->lx_open_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->lx_close_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->lx_driver_last_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->block_last_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->flash_last_status = PS_STORAGE_FXLX_NOT_RUN;
  probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_VALIDATE;

  if (region != NULL)
  {
    probe->region_id = (uint32_t)region->id;
    probe->region_start = region->start;
    probe->region_length = region->length;
  }
  PS_StorageFxLx_RecordMscHardware(block);
}

static uint32_t PS_StorageFxLx_Min(uint32_t a, uint32_t b)
{
  return (a < b) ? a : b;
}

static ps_status_t PS_StorageFxLx_CheckAddress(ULONG *flash_address,
                                               ULONG words,
                                               uint32_t *address,
                                               uint32_t *length)
{
  uintptr_t base = (uintptr_t)PS_STORAGE_FXLX_FAKE_BASE;
  uintptr_t current = (uintptr_t)flash_address;
  uintptr_t byte_length = ((uintptr_t)words) * sizeof(ULONG);
  uintptr_t byte_offset;

  if ((ps_storage_fxlx_block == NULL) || (address == NULL) ||
      (length == NULL) || (current < base) ||
      (byte_length > (uintptr_t)UINT32_MAX))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  byte_offset = current - base;
  if ((byte_offset > (uintptr_t)ps_storage_fxlx_length) ||
      (byte_length > ((uintptr_t)ps_storage_fxlx_length - byte_offset)))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  *address = ps_storage_fxlx_start + (uint32_t)byte_offset;
  *length = (uint32_t)byte_length;
  return PS_STATUS_OK;
}

static UINT PS_StorageFxLx_StatusToFx(ps_status_t status)
{
  return (status == PS_STATUS_OK) ? FX_SUCCESS : FX_IO_ERROR;
}

static ps_status_t PS_StorageFxLx_StatusFromLx(UINT status)
{
  return (status == LX_SUCCESS) ? PS_STATUS_OK : PS_STATUS_IO_ERROR;
}

static uint32_t PS_StorageFxLx_ExportLength(
  const ps_storage_region_t *region)
{
  return PS_StorageFxLx_Min(region->length, PS_STORAGE_FXLX_TEST_LENGTH);
}

static ps_status_t PS_StorageFxLx_ValidateExport(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  uint32_t *export_length)
{
  uint32_t length;

  if ((block == NULL) || (region == NULL) || (export_length == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  length = PS_StorageFxLx_ExportLength(region);
  if ((block->initialized == 0UL) ||
      (region->id != PS_STORAGE_REGION_USB_STAGING) ||
      (region->host_exposed == 0UL) ||
      (length < (128UL * 1024UL)) ||
      ((region->start % block->geometry.erase_block_size) != 0UL) ||
      ((length % block->geometry.erase_block_size) != 0UL) ||
      ((length / PS_STORAGE_FXLX_SECTOR_SIZE) !=
       PS_STORAGE_MSC_BRIDGE_BLOCK_COUNT))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  *export_length = length;
  return PS_STATUS_OK;
}

static ps_status_t PS_StorageFxLx_EraseExportRange(
  ps_storage_flash_block_t *block,
  uint32_t start,
  uint32_t length,
  ps_storage_filex_levelx_smoke_result_t *result)
{
  ps_status_t status = PS_STATUS_OK;
  uint32_t first_block;
  uint32_t block_count;
  uint32_t block_ix;
  uint32_t poll_count = 0UL;

  if (result != NULL)
  {
    result->preformat_erase_status = PS_STATUS_INTERNAL_ERROR;
    result->preformat_erase_block_count = 0UL;
    result->preformat_erase_failed_block = PS_STORAGE_FXLX_NOT_RUN;
    result->preformat_erase_last_poll_count = 0UL;
  }

  if ((block == NULL) || (block->initialized == 0UL) ||
      (block->geometry.logical_block_size == 0UL) || (length == 0UL) ||
      ((start % block->geometry.logical_block_size) != 0UL) ||
      ((length % block->geometry.logical_block_size) != 0UL))
  {
    status = PS_STATUS_INVALID_ARGUMENT;
    if (result != NULL)
    {
      result->preformat_erase_status = (uint32_t)status;
    }
    return status;
  }

  first_block = start / block->geometry.logical_block_size;
  block_count = length / block->geometry.logical_block_size;
  if ((first_block > block->geometry.logical_block_count) ||
      (block_count > (block->geometry.logical_block_count - first_block)))
  {
    status = PS_STATUS_INVALID_ARGUMENT;
    if (result != NULL)
    {
      result->preformat_erase_status = (uint32_t)status;
    }
    return status;
  }

  if (result != NULL)
  {
    result->preformat_erase_block_count = block_count;
  }

  for (block_ix = 0UL; block_ix < block_count; ++block_ix)
  {
    poll_count = 0UL;
    status = ps_storage_flash_block_erase(block,
                                          first_block + block_ix,
                                          &poll_count);
    if (result != NULL)
    {
      result->preformat_erase_last_poll_count = poll_count;
    }
    if (status != PS_STATUS_OK)
    {
      if (result != NULL)
      {
        result->preformat_erase_status = (uint32_t)status;
        result->preformat_erase_failed_block = first_block + block_ix;
      }
      return status;
    }
  }

  if (result != NULL)
  {
    result->preformat_erase_status = PS_STATUS_OK;
  }
  return PS_STATUS_OK;
}

static UINT PS_StorageFxLx_Read(ULONG *flash_address,
                                ULONG *destination,
                                ULONG words)
{
  ps_status_t status;
  uint32_t address;
  uint32_t length;

  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_read_count++;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_read_count++;
  }
  status = PS_StorageFxLx_CheckAddress(flash_address, words, &address, &length);
  if (status == PS_STATUS_OK)
  {
    status = ps_storage_flash_block_read(ps_storage_fxlx_block,
                                         address,
                                         (uint8_t *)destination,
                                         length);
  }
  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_last_status = (uint32_t)status;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_last_status =
      (uint32_t)status;
    PS_StorageFxLx_RecordMscHardware(ps_storage_fxlx_block);
  }
  return PS_StorageFxLx_StatusToFx(status);
}

static UINT PS_StorageFxLx_Write(ULONG *flash_address,
                                 ULONG *source,
                                 ULONG words)
{
  ps_status_t status;
  uint32_t address;
  uint32_t length;

  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_write_count++;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_write_count++;
  }
  status = PS_StorageFxLx_CheckAddress(flash_address, words, &address, &length);
  if (status == PS_STATUS_OK)
  {
    status = ps_storage_flash_block_program(ps_storage_fxlx_block,
                                            address,
                                            (const uint8_t *)source,
                                            length);
  }
  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_last_status = (uint32_t)status;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_last_status =
      (uint32_t)status;
    PS_StorageFxLx_RecordMscHardware(ps_storage_fxlx_block);
  }
  return PS_StorageFxLx_StatusToFx(status);
}

static UINT PS_StorageFxLx_BlockErase(ULONG block, ULONG erase_count)
{
  ps_status_t status;
  uint32_t physical_block;
  uint32_t poll_count;

  (void)erase_count;
  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_erase_count++;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_erase_count++;
  }
  if ((ps_storage_fxlx_block == NULL) ||
      (block >= (ps_storage_fxlx_length /
                 ps_storage_fxlx_block->geometry.logical_block_size)))
  {
    status = PS_STATUS_INVALID_ARGUMENT;
  }
  else
  {
    physical_block = (ps_storage_fxlx_start /
                      ps_storage_fxlx_block->geometry.logical_block_size) +
                     (uint32_t)block;
    status = ps_storage_flash_block_erase(ps_storage_fxlx_block,
                                          physical_block,
                                          &poll_count);
  }
  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_last_status = (uint32_t)status;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_last_status =
      (uint32_t)status;
    PS_StorageFxLx_RecordMscHardware(ps_storage_fxlx_block);
  }
  return PS_StorageFxLx_StatusToFx(status);
}

static UINT PS_StorageFxLx_BlockErasedVerify(ULONG block)
{
  ps_status_t status;
  uint32_t physical_block;
  uint32_t mismatch_count;

  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_verify_count++;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_verify_count++;
  }
  if ((ps_storage_fxlx_block == NULL) ||
      (block >= (ps_storage_fxlx_length /
                 ps_storage_fxlx_block->geometry.logical_block_size)))
  {
    status = PS_STATUS_INVALID_ARGUMENT;
  }
  else
  {
    physical_block = (ps_storage_fxlx_start /
                      ps_storage_fxlx_block->geometry.logical_block_size) +
                     (uint32_t)block;
    status = ps_storage_flash_block_verify_erased(ps_storage_fxlx_block,
                                                  physical_block,
                                                  &mismatch_count);
  }
  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_last_status = (uint32_t)status;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_last_status =
      (uint32_t)status;
    PS_StorageFxLx_RecordMscHardware(ps_storage_fxlx_block);
  }
  return PS_StorageFxLx_StatusToFx(status);
}

static UINT PS_StorageFxLx_SystemError(UINT error_code)
{
  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_last_status = error_code;
  }
  if (g_ps_storage_filex_levelx_msc_probe.active != 0UL)
  {
    g_ps_storage_filex_levelx_msc_probe.lx_driver_last_status = error_code;
  }
  return LX_SUCCESS;
}

static UINT PS_StorageFxLx_NorInit(LX_NOR_FLASH *nor_flash)
{
  nor_flash->lx_nor_flash_base_address = (ULONG *)PS_STORAGE_FXLX_FAKE_BASE;
  nor_flash->lx_nor_flash_total_blocks =
    ps_storage_fxlx_length / ps_storage_fxlx_block->geometry.erase_block_size;
  nor_flash->lx_nor_flash_words_per_block =
    ps_storage_fxlx_block->geometry.erase_block_size / sizeof(ULONG);
  nor_flash->lx_nor_flash_driver_read = PS_StorageFxLx_Read;
  nor_flash->lx_nor_flash_driver_write = PS_StorageFxLx_Write;
  nor_flash->lx_nor_flash_driver_block_erase = PS_StorageFxLx_BlockErase;
  nor_flash->lx_nor_flash_driver_block_erased_verify =
    PS_StorageFxLx_BlockErasedVerify;
  nor_flash->lx_nor_flash_driver_system_error = PS_StorageFxLx_SystemError;
  nor_flash->lx_nor_flash_sector_buffer = ps_storage_fxlx_sector_buffer;
  return LX_SUCCESS;
}

static void PS_StorageFxLx_MediaDriver(FX_MEDIA *media)
{
  UINT status = FX_SUCCESS;
  ULONG sector;
  ULONG sectors;
  UCHAR *buffer;

  if ((media == NULL) || (ps_storage_fxlx_result == NULL))
  {
    return;
  }

  ps_storage_fxlx_result->fx_driver_last_request = media->fx_media_driver_request;
  sector = (ULONG)media->fx_media_driver_logical_sector;
  sectors = media->fx_media_driver_sectors;
  buffer = media->fx_media_driver_buffer;
  if ((media->fx_media_driver_request == FX_DRIVER_BOOT_READ) ||
      (media->fx_media_driver_request == FX_DRIVER_BOOT_WRITE))
  {
    sector = 0UL;
    sectors = 1UL;
  }

  switch (media->fx_media_driver_request)
  {
    case FX_DRIVER_INIT:
      ps_storage_fxlx_result->fx_driver_init_count++;
      media->fx_media_driver_free_sector_update = FX_TRUE;
      break;

    case FX_DRIVER_UNINIT:
      ps_storage_fxlx_result->fx_driver_uninit_count++;
      break;

    case FX_DRIVER_READ:
    case FX_DRIVER_BOOT_READ:
      ps_storage_fxlx_result->fx_driver_read_count++;
      while ((sectors != 0UL) && (status == FX_SUCCESS))
      {
        status = lx_nor_flash_sector_read(&ps_storage_fxlx_nor,
                                          sector,
                                          buffer);
        if ((status == FX_SUCCESS) &&
            (media->fx_media_driver_request == FX_DRIVER_BOOT_READ))
        {
          uint32_t index;
          for (index = 0UL; index < 16UL; ++index)
          {
            ps_storage_fxlx_result->boot_read_first16[index] = buffer[index];
          }
          ps_storage_fxlx_result->boot_bytes_per_sector =
            (uint32_t)buffer[11] | ((uint32_t)buffer[12] << 8);
          ps_storage_fxlx_result->boot_sectors_per_cluster = buffer[13];
          ps_storage_fxlx_result->boot_reserved_sectors =
            (uint32_t)buffer[14] | ((uint32_t)buffer[15] << 8);
          ps_storage_fxlx_result->boot_number_of_fats = buffer[16];
          ps_storage_fxlx_result->boot_root_entries =
            (uint32_t)buffer[17] | ((uint32_t)buffer[18] << 8);
          ps_storage_fxlx_result->boot_total_sectors =
            (uint32_t)buffer[19] | ((uint32_t)buffer[20] << 8);
          ps_storage_fxlx_result->boot_sectors_per_fat =
            (uint32_t)buffer[22] | ((uint32_t)buffer[23] << 8);
          ps_storage_fxlx_result->boot_signature =
            (uint32_t)buffer[510] | ((uint32_t)buffer[511] << 8);
        }
        sector++;
        buffer += PS_STORAGE_FXLX_SECTOR_SIZE;
        sectors--;
      }
      break;

    case FX_DRIVER_WRITE:
    case FX_DRIVER_BOOT_WRITE:
      ps_storage_fxlx_result->fx_driver_write_count++;
      while ((sectors != 0UL) && (status == FX_SUCCESS))
      {
        status = lx_nor_flash_sector_write(&ps_storage_fxlx_nor,
                                           sector,
                                           buffer);
        sector++;
        buffer += PS_STORAGE_FXLX_SECTOR_SIZE;
        sectors--;
      }
      break;

    case FX_DRIVER_RELEASE_SECTORS:
      ps_storage_fxlx_result->fx_driver_release_count++;
      while ((sectors != 0UL) && (status == FX_SUCCESS))
      {
        status = lx_nor_flash_sector_release(&ps_storage_fxlx_nor, sector);
        sector++;
        sectors--;
      }
      break;

    case FX_DRIVER_FLUSH:
      ps_storage_fxlx_result->fx_driver_flush_count++;
      break;

    case FX_DRIVER_ABORT:
      ps_storage_fxlx_result->fx_driver_abort_count++;
      break;

    default:
      status = FX_IO_ERROR;
      break;
  }

  media->fx_media_driver_status = status;
  ps_storage_fxlx_result->fx_driver_last_status = status;
}

static void PS_StorageFxLx_FillPayload(void)
{
  uint32_t index;

  for (index = 0UL; index < PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE; ++index)
  {
    ps_storage_fxlx_write_buffer[index] =
      (UCHAR)('A' + (uint8_t)(index % 26UL));
  }
  ps_storage_fxlx_write_buffer[0] = 'H';
  ps_storage_fxlx_write_buffer[1] = 'W';
  ps_storage_fxlx_write_buffer[2] = '6';
  ps_storage_fxlx_write_buffer[3] = '-';
  ps_storage_fxlx_write_buffer[4] = 'F';
  ps_storage_fxlx_write_buffer[5] = 'X';
  ps_storage_fxlx_write_buffer[6] = 'L';
  ps_storage_fxlx_write_buffer[7] = 'X';
}

static uint32_t PS_StorageFxLx_CountMismatches(uint32_t length)
{
  uint32_t index;
  uint32_t mismatches = 0UL;

  for (index = 0UL; index < length; ++index)
  {
    if (ps_storage_fxlx_read_buffer[index] != ps_storage_fxlx_write_buffer[index])
    {
      mismatches++;
    }
  }
  return mismatches;
}

static uint32_t PS_StorageFxLx_MscOpenFoundInvalidMedia(void)
{
  uint32_t open_status = g_ps_storage_filex_levelx_msc_probe.lx_open_status;
  uint32_t driver_status =
    g_ps_storage_filex_levelx_msc_probe.lx_driver_last_status;

  return ((open_status == LX_SYSTEM_INVALID_FORMAT) ||
          (open_status == LX_SYSTEM_INVALID_BLOCK) ||
          (open_status == LX_SYSTEM_INVALID_SECTOR_MAP) ||
          ((open_status == LX_ERROR) &&
           ((driver_status == LX_SYSTEM_INVALID_FORMAT) ||
            (driver_status == LX_SYSTEM_INVALID_BLOCK) ||
            (driver_status == LX_SYSTEM_INVALID_SECTOR_MAP)))) ?
         1UL : 0UL;
}

static void PS_StorageFxLx_InitFormatResult(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  uint32_t export_length,
  ps_storage_filex_levelx_smoke_result_t *result)
{
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->api_version = PS_STORAGE_FILEX_LEVELX_API_VERSION;
  result->region_id = (uint32_t)region->id;
  result->region_start = region->start;
  result->region_length = region->length;
  result->test_start = region->start;
  result->test_length = export_length;
  result->erase_block_size = block->geometry.erase_block_size;
  result->logical_sector_size = PS_STORAGE_FXLX_SECTOR_SIZE;
  result->logical_sector_count = export_length / PS_STORAGE_FXLX_SECTOR_SIZE;
  result->preformat_erase_status = PS_STORAGE_FXLX_NOT_RUN;
  result->preformat_erase_block_count = 0UL;
  result->preformat_erase_failed_block = PS_STORAGE_FXLX_NOT_RUN;
  result->preformat_erase_last_poll_count = 0UL;
  result->lx_initialize_status = PS_STORAGE_FXLX_NOT_RUN;
  result->lx_open_status = PS_STORAGE_FXLX_NOT_RUN;
  result->fx_format_status = FX_IO_ERROR;
  result->fx_open_status = FX_IO_ERROR;
  result->file_create_status = PS_STORAGE_FXLX_NOT_RUN;
  result->file_open_status = PS_STORAGE_FXLX_NOT_RUN;
  result->file_write_status = PS_STORAGE_FXLX_NOT_RUN;
  result->file_seek_status = PS_STORAGE_FXLX_NOT_RUN;
  result->file_read_status = PS_STORAGE_FXLX_NOT_RUN;
  result->file_close_status = PS_STORAGE_FXLX_NOT_RUN;
  result->fx_flush_status = FX_IO_ERROR;
  result->fx_close_status = FX_IO_ERROR;
  result->lx_close_status = PS_STORAGE_FXLX_NOT_RUN;
}

static ps_status_t PS_StorageFxLx_MscOpenAttempt(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  uint32_t export_length)
{
  UINT status;
  ps_status_t ps_status;
  volatile ps_storage_filex_levelx_msc_probe_t *probe =
    &g_ps_storage_filex_levelx_msc_probe;

  if ((ps_storage_fxlx_msc_opened != 0UL) ||
      (ps_storage_fxlx_nor.lx_nor_flash_state == LX_NOR_FLASH_OPENED))
  {
    ps_storage_fxlx_msc_opened = 1UL;
    probe->already_open = 1UL;
    probe->active = 1UL;
    probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_OPENED_OLD;
    probe->status = (uint32_t)PS_STATUS_OK;
    PS_StorageFxLx_RecordMscHardware(block);
    return PS_STATUS_OK;
  }

  if (ps_storage_fxlx_levelx_initialized == 0UL)
  {
    probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_LX_INIT;
    status = lx_nor_flash_initialize();
    probe->lx_initialize_status = (uint32_t)status;
    if (status != LX_SUCCESS)
    {
      ps_status = PS_StorageFxLx_StatusFromLx(status);
      probe->active = 0UL;
      probe->status = (uint32_t)ps_status;
      probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_FAULT;
      PS_StorageFxLx_RecordMscHardware(block);
      return ps_status;
    }
    ps_storage_fxlx_levelx_initialized = 1UL;
  }
  else
  {
    probe->lx_initialize_status = LX_SUCCESS;
  }

  ps_storage_fxlx_block = block;
  ps_storage_fxlx_start = region->start;
  ps_storage_fxlx_length = export_length;
  ps_storage_fxlx_result = NULL;
  (void)memset(&ps_storage_fxlx_nor, 0, sizeof(ps_storage_fxlx_nor));

  probe->active = 1UL;
  probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_LX_OPEN;
  status = lx_nor_flash_open(&ps_storage_fxlx_nor,
                             (CHAR *)"at25-usb-stage",
                             PS_StorageFxLx_NorInit);
  probe->lx_open_status = (uint32_t)status;
  if (status != LX_SUCCESS)
  {
    ps_storage_fxlx_block = NULL;
    ps_storage_fxlx_msc_opened = 0UL;
    ps_status = PS_StorageFxLx_StatusFromLx(status);
    probe->active = 0UL;
    probe->status = (uint32_t)ps_status;
    probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_FAULT;
    PS_StorageFxLx_RecordMscHardware(block);
    return ps_status;
  }

  ps_storage_fxlx_msc_opened = 1UL;
  probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_OPENED;
  probe->status = (uint32_t)PS_STATUS_OK;
  PS_StorageFxLx_RecordMscHardware(block);
  return PS_STATUS_OK;
}

ps_status_t ps_storage_filex_levelx_initialize_usb_staging(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  ps_storage_filex_levelx_smoke_result_t *result)
{
  UINT status;
  ps_status_t ps_status;
  uint32_t export_length;

  if ((block == NULL) || (region == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  export_length = PS_StorageFxLx_ExportLength(region);
  PS_StorageFxLx_InitFormatResult(block, region, export_length, result);
  ps_status = PS_StorageFxLx_ValidateExport(block, region, &export_length);
  result->test_length = export_length;
  result->logical_sector_count = export_length / PS_STORAGE_FXLX_SECTOR_SIZE;
  if (ps_status != PS_STATUS_OK)
  {
    result->status = ps_status;
    return ps_status;
  }

  ps_storage_fxlx_block = block;
  ps_storage_fxlx_start = region->start;
  ps_storage_fxlx_length = export_length;
  ps_storage_fxlx_result = result;
  ps_storage_fxlx_msc_opened = 0UL;
  (void)memset(&ps_storage_fxlx_nor, 0, sizeof(ps_storage_fxlx_nor));
  (void)memset(&ps_storage_fxlx_media, 0, sizeof(ps_storage_fxlx_media));
  (void)memset(&ps_storage_fxlx_file, 0, sizeof(ps_storage_fxlx_file));
  (void)memset(ps_storage_fxlx_media_memory, 0,
               sizeof(ps_storage_fxlx_media_memory));
  (void)memset(ps_storage_fxlx_read_buffer, 0,
               sizeof(ps_storage_fxlx_read_buffer));

  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_FLASH_ERASE_START);
  ps_status = PS_StorageFxLx_EraseExportRange(block,
                                              result->test_start,
                                              result->test_length,
                                              result);
  if (ps_status != PS_STATUS_OK)
  {
    result->status = ps_status;
    goto init_done;
  }

  status = lx_nor_flash_initialize();
  result->lx_initialize_status = status;
  if (status != LX_SUCCESS)
  {
    result->status = PS_STATUS_IO_ERROR;
    goto init_done;
  }
  ps_storage_fxlx_levelx_initialized = 1UL;

  status = lx_nor_flash_open(&ps_storage_fxlx_nor,
                             (CHAR *)"at25-usb-stage",
                             PS_StorageFxLx_NorInit);
  result->lx_open_status = status;
  if (status != LX_SUCCESS)
  {
    result->status = PS_STATUS_IO_ERROR;
    goto init_done;
  }

  PS_HW6_TraceSwoLifecycle(PS_HW6_TRACE_SWO_FLASH_FORMAT_START);
  status = fx_media_format(&ps_storage_fxlx_media,
                           PS_StorageFxLx_MediaDriver,
                           NULL,
                           ps_storage_fxlx_media_memory,
                           sizeof(ps_storage_fxlx_media_memory),
                           (CHAR *)PS_STORAGE_FXLX_VOLUME_NAME,
                           PS_STORAGE_FXLX_NUM_FATS,
                           PS_STORAGE_FXLX_DIR_ENTRIES,
                           0U,
                           result->logical_sector_count,
                           PS_STORAGE_FXLX_SECTOR_SIZE,
                           PS_STORAGE_FXLX_SECTORS_PER_CLUSTER,
                           1U,
                           1U);
  result->fx_format_status = status;
  if (status != FX_SUCCESS)
  {
    result->lx_close_status = lx_nor_flash_close(&ps_storage_fxlx_nor);
    result->status = PS_STATUS_IO_ERROR;
    goto init_done;
  }

  status = fx_media_open(&ps_storage_fxlx_media,
                         (CHAR *)"pshw6-media",
                         PS_StorageFxLx_MediaDriver,
                         NULL,
                         ps_storage_fxlx_media_memory,
                         sizeof(ps_storage_fxlx_media_memory));
  result->fx_open_status = status;
  if (status == FX_SUCCESS)
  {
    result->fx_flush_status = fx_media_flush(&ps_storage_fxlx_media);
    result->fx_close_status = fx_media_close(&ps_storage_fxlx_media);
  }
  result->lx_close_status = lx_nor_flash_close(&ps_storage_fxlx_nor);

  if ((result->fx_open_status == FX_SUCCESS) &&
      (result->fx_flush_status == FX_SUCCESS) &&
      (result->fx_close_status == FX_SUCCESS) &&
      (result->lx_close_status == LX_SUCCESS))
  {
    result->status = PS_STATUS_OK;
  }
  else
  {
    result->status = PS_STATUS_IO_ERROR;
  }

init_done:
  (void)memset(&ps_storage_fxlx_nor, 0, sizeof(ps_storage_fxlx_nor));
  (void)memset(&ps_storage_fxlx_media, 0, sizeof(ps_storage_fxlx_media));
  ps_storage_fxlx_block = NULL;
  ps_storage_fxlx_result = NULL;
  ps_storage_fxlx_msc_opened = 0UL;
  return result->status;
}

ps_status_t ps_storage_filex_levelx_msc_open(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region)
{
  uint32_t export_length;
  ps_status_t ps_status;
  volatile ps_storage_filex_levelx_msc_probe_t *probe =
    &g_ps_storage_filex_levelx_msc_probe;

  PS_StorageFxLx_ResetMscProbe(block, region);
  probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_VALIDATE;
  ps_status = PS_StorageFxLx_ValidateExport(block, region, &export_length);
  probe->validate_status = (uint32_t)ps_status;
  probe->status = (uint32_t)ps_status;
  if (ps_status != PS_STATUS_OK)
  {
    probe->active = 0UL;
    probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_FAULT;
    PS_StorageFxLx_RecordMscHardware(block);
    return ps_status;
  }
  probe->export_length = export_length;

  ps_status = PS_StorageFxLx_MscOpenAttempt(block, region, export_length);
  if ((ps_status != PS_STATUS_OK) &&
      (PS_StorageFxLx_MscOpenFoundInvalidMedia() != 0UL))
  {
    probe->invalid_media_detected = 1UL;
    probe->recovery_required_count++;
    probe->recovery_lx_open_status = probe->lx_open_status;
    probe->recovery_driver_status = probe->lx_driver_last_status;
    probe->status = (uint32_t)PS_STATUS_RECOVERY_REQUIRED;
    return PS_STATUS_RECOVERY_REQUIRED;
  }
  return ps_status;
}

ps_status_t ps_storage_filex_levelx_msc_close(void)
{
  UINT status;
  ps_status_t ps_status;
  ps_storage_flash_block_t *block = ps_storage_fxlx_block;
  volatile ps_storage_filex_levelx_msc_probe_t *probe =
    &g_ps_storage_filex_levelx_msc_probe;

  probe->api_version = PS_STORAGE_FILEX_LEVELX_MSC_PROBE_API_VERSION;
  probe->close_count++;
  probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_CLOSE;
  probe->lx_close_status = PS_STORAGE_FXLX_NOT_RUN;

  if ((ps_storage_fxlx_msc_opened == 0UL) &&
      (ps_storage_fxlx_nor.lx_nor_flash_state != LX_NOR_FLASH_OPENED))
  {
    probe->active = 0UL;
    probe->status = (uint32_t)PS_STATUS_OK;
    probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_IDLE;
    PS_StorageFxLx_RecordMscHardware(block);
    return PS_STATUS_OK;
  }

  status = lx_nor_flash_close(&ps_storage_fxlx_nor);
  probe->lx_close_status = (uint32_t)status;
  if (status != LX_SUCCESS)
  {
    ps_status = PS_StorageFxLx_StatusFromLx(status);
    probe->status = (uint32_t)ps_status;
    PS_StorageFxLx_RecordMscHardware(block);
    return ps_status;
  }

  (void)memset(&ps_storage_fxlx_nor, 0, sizeof(ps_storage_fxlx_nor));
  ps_storage_fxlx_block = NULL;
  ps_storage_fxlx_result = NULL;
  ps_storage_fxlx_msc_opened = 0UL;
  probe->active = 0UL;
  probe->status = (uint32_t)PS_STATUS_OK;
  probe->last_stage = PS_STORAGE_FXLX_MSC_STAGE_IDLE;
  PS_StorageFxLx_RecordMscHardware(block);
  return PS_STATUS_OK;
}

ps_status_t ps_storage_filex_levelx_msc_read(uint32_t lba,
                                             uint32_t block_count,
                                             uint8_t *data)
{
  UINT status = LX_SUCCESS;
  uint32_t block_ix;
  uint32_t logical_sector_count;

  if ((data == NULL) || (block_count == 0UL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if ((ps_storage_fxlx_msc_opened == 0UL) ||
      (ps_storage_fxlx_nor.lx_nor_flash_state != LX_NOR_FLASH_OPENED))
  {
    return PS_STATUS_INVALID_STATE;
  }

  logical_sector_count = ps_storage_fxlx_length / PS_STORAGE_FXLX_SECTOR_SIZE;
  if ((lba >= logical_sector_count) ||
      (block_count > (logical_sector_count - lba)))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  for (block_ix = 0UL; block_ix < block_count; ++block_ix)
  {
    status = lx_nor_flash_sector_read(&ps_storage_fxlx_nor,
                                      (ULONG)lba + block_ix,
                                      data + (block_ix *
                                              PS_STORAGE_FXLX_SECTOR_SIZE));
    if (status != LX_SUCCESS)
    {
      break;
    }
  }

  return PS_StorageFxLx_StatusFromLx(status);
}

ps_status_t ps_storage_filex_levelx_msc_write(uint32_t lba,
                                              uint32_t block_count,
                                              const uint8_t *data)
{
  UINT status = LX_SUCCESS;
  uint32_t block_ix;
  uint32_t logical_sector_count;

  if ((data == NULL) || (block_count == 0UL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if ((ps_storage_fxlx_msc_opened == 0UL) ||
      (ps_storage_fxlx_nor.lx_nor_flash_state != LX_NOR_FLASH_OPENED))
  {
    return PS_STATUS_INVALID_STATE;
  }

  logical_sector_count = ps_storage_fxlx_length / PS_STORAGE_FXLX_SECTOR_SIZE;
  if ((lba >= logical_sector_count) ||
      (block_count > (logical_sector_count - lba)))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  for (block_ix = 0UL; block_ix < block_count; ++block_ix)
  {
    status = lx_nor_flash_sector_write(&ps_storage_fxlx_nor,
                                       (ULONG)lba + block_ix,
                                       (VOID *)(data + (block_ix *
                                                PS_STORAGE_FXLX_SECTOR_SIZE)));
    if (status != LX_SUCCESS)
    {
      break;
    }
  }

  return PS_StorageFxLx_StatusFromLx(status);
}

uint32_t ps_storage_filex_levelx_msc_is_open(void)
{
  return ((ps_storage_fxlx_msc_opened != 0UL) &&
          (ps_storage_fxlx_nor.lx_nor_flash_state == LX_NOR_FLASH_OPENED)) ?
         1UL : 0UL;
}

ps_status_t ps_storage_filex_levelx_run_smoke(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  ps_storage_filex_levelx_smoke_result_t *result)
{
  UINT status;
  ps_status_t erase_status;
  ULONG actual_read = 0UL;
  uint32_t index;

  if ((block == NULL) || (region == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->api_version = PS_STORAGE_FILEX_LEVELX_API_VERSION;
  result->region_id = (uint32_t)region->id;
  result->region_start = region->start;
  result->region_length = region->length;
  result->test_start = region->start;
  result->test_length = PS_StorageFxLx_ExportLength(region);
  result->erase_block_size = block->geometry.erase_block_size;
  result->logical_sector_size = PS_STORAGE_FXLX_SECTOR_SIZE;
  result->logical_sector_count = result->test_length /
                                 PS_STORAGE_FXLX_SECTOR_SIZE;
  result->preformat_erase_status = PS_STORAGE_FXLX_NOT_RUN;
  result->preformat_erase_block_count = 0UL;
  result->preformat_erase_failed_block = PS_STORAGE_FXLX_NOT_RUN;
  result->preformat_erase_last_poll_count = 0UL;

  result->lx_initialize_status = PS_STORAGE_FXLX_NOT_RUN;
  result->lx_open_status = PS_STORAGE_FXLX_NOT_RUN;
  result->fx_format_status = FX_IO_ERROR;
  result->fx_open_status = FX_IO_ERROR;
  result->file_create_status = FX_IO_ERROR;
  result->file_open_status = FX_IO_ERROR;
  result->file_write_status = FX_IO_ERROR;
  result->file_seek_status = FX_IO_ERROR;
  result->file_read_status = FX_IO_ERROR;
  result->file_close_status = FX_IO_ERROR;
  result->fx_flush_status = FX_IO_ERROR;
  result->fx_close_status = FX_IO_ERROR;
  result->lx_close_status = PS_STORAGE_FXLX_NOT_RUN;

  if ((block->initialized == 0UL) || (region->id != PS_STORAGE_REGION_USB_STAGING) ||
      (region->host_exposed == 0UL) || (result->test_length < (128UL * 1024UL)) ||
      ((region->start % block->geometry.erase_block_size) != 0UL) ||
      ((result->test_length % block->geometry.erase_block_size) != 0UL))
  {
    result->status = PS_STATUS_INVALID_ARGUMENT;
    return result->status;
  }

  ps_storage_fxlx_block = block;
  ps_storage_fxlx_start = result->test_start;
  ps_storage_fxlx_length = result->test_length;
  ps_storage_fxlx_result = result;
  ps_storage_fxlx_msc_opened = 0UL;
  (void)memset(&ps_storage_fxlx_nor, 0, sizeof(ps_storage_fxlx_nor));
  (void)memset(&ps_storage_fxlx_media, 0, sizeof(ps_storage_fxlx_media));
  (void)memset(&ps_storage_fxlx_file, 0, sizeof(ps_storage_fxlx_file));
  (void)memset(ps_storage_fxlx_media_memory, 0,
               sizeof(ps_storage_fxlx_media_memory));
  (void)memset(ps_storage_fxlx_read_buffer, 0,
               sizeof(ps_storage_fxlx_read_buffer));
  PS_StorageFxLx_FillPayload();

  erase_status = PS_StorageFxLx_EraseExportRange(block,
                                                 result->test_start,
                                                 result->test_length,
                                                 result);
  if (erase_status != PS_STATUS_OK)
  {
    result->status = erase_status;
    return result->status;
  }

  status = lx_nor_flash_initialize();
  result->lx_initialize_status = status;
  if (status != LX_SUCCESS)
  {
    result->status = PS_STATUS_INTERNAL_ERROR;
    return result->status;
  }
  ps_storage_fxlx_levelx_initialized = 1UL;

  status = lx_nor_flash_open(&ps_storage_fxlx_nor,
                             (CHAR *)"at25-usb-stage",
                             PS_StorageFxLx_NorInit);
  result->lx_open_status = status;
  if (status != LX_SUCCESS)
  {
    result->status = PS_STATUS_IO_ERROR;
    return result->status;
  }

  status = fx_media_format(&ps_storage_fxlx_media,
                           PS_StorageFxLx_MediaDriver,
                           NULL,
                           ps_storage_fxlx_media_memory,
                           sizeof(ps_storage_fxlx_media_memory),
                           (CHAR *)PS_STORAGE_FXLX_VOLUME_NAME,
                           PS_STORAGE_FXLX_NUM_FATS,
                           PS_STORAGE_FXLX_DIR_ENTRIES,
                           0U,
                           result->logical_sector_count,
                           PS_STORAGE_FXLX_SECTOR_SIZE,
                           PS_STORAGE_FXLX_SECTORS_PER_CLUSTER,
                           1U,
                           1U);
  result->fx_format_status = status;
  if (status != FX_SUCCESS)
  {
    result->lx_close_status = lx_nor_flash_close(&ps_storage_fxlx_nor);
    result->status = PS_STATUS_IO_ERROR;
    return result->status;
  }

  status = fx_media_open(&ps_storage_fxlx_media,
                         (CHAR *)"pshw6-media",
                         PS_StorageFxLx_MediaDriver,
                         NULL,
                         ps_storage_fxlx_media_memory,
                         sizeof(ps_storage_fxlx_media_memory));
  result->fx_open_status = status;
  if (status != FX_SUCCESS)
  {
    result->lx_close_status = lx_nor_flash_close(&ps_storage_fxlx_nor);
    result->status = PS_STATUS_IO_ERROR;
    return result->status;
  }

  status = fx_file_create(&ps_storage_fxlx_media,
                          (CHAR *)PS_STORAGE_FXLX_FILE_NAME);
  result->file_create_status = status;
  if (status != FX_SUCCESS)
  {
    goto close_media;
  }

  status = fx_file_open(&ps_storage_fxlx_media,
                        &ps_storage_fxlx_file,
                        (CHAR *)PS_STORAGE_FXLX_FILE_NAME,
                        FX_OPEN_FOR_WRITE);
  result->file_open_status = status;
  if (status != FX_SUCCESS)
  {
    goto close_media;
  }

  status = fx_file_write(&ps_storage_fxlx_file,
                         ps_storage_fxlx_write_buffer,
                         PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE);
  result->file_write_status = status;
  if (status == FX_SUCCESS)
  {
    result->bytes_written = PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE;
  }

  result->file_close_status = fx_file_close(&ps_storage_fxlx_file);
  result->fx_flush_status = fx_media_flush(&ps_storage_fxlx_media);
  if ((status != FX_SUCCESS) || (result->file_close_status != FX_SUCCESS) ||
      (result->fx_flush_status != FX_SUCCESS))
  {
    goto close_media;
  }

  status = fx_file_open(&ps_storage_fxlx_media,
                        &ps_storage_fxlx_file,
                        (CHAR *)PS_STORAGE_FXLX_FILE_NAME,
                        FX_OPEN_FOR_READ);
  result->file_open_status = status;
  if (status != FX_SUCCESS)
  {
    goto close_media;
  }

  status = fx_file_seek(&ps_storage_fxlx_file, 0UL);
  result->file_seek_status = status;
  if (status == FX_SUCCESS)
  {
    status = fx_file_read(&ps_storage_fxlx_file,
                          ps_storage_fxlx_read_buffer,
                          PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE,
                          &actual_read);
  }
  result->file_read_status = status;
  result->bytes_read = (uint32_t)actual_read;
  result->file_close_status = fx_file_close(&ps_storage_fxlx_file);

  result->verify_mismatch_count = PS_StorageFxLx_CountMismatches(
    PS_StorageFxLx_Min(result->bytes_read, PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE));
  for (index = 0UL; index < 16UL; ++index)
  {
    result->read_first16[index] = ps_storage_fxlx_read_buffer[index];
  }

close_media:
  result->fx_close_status = fx_media_close(&ps_storage_fxlx_media);
  result->lx_close_status = lx_nor_flash_close(&ps_storage_fxlx_nor);

  if ((result->lx_initialize_status == LX_SUCCESS) &&
      (result->lx_open_status == LX_SUCCESS) &&
      (result->fx_format_status == FX_SUCCESS) &&
      (result->fx_open_status == FX_SUCCESS) &&
      (result->file_create_status == FX_SUCCESS) &&
      (result->file_open_status == FX_SUCCESS) &&
      (result->file_write_status == FX_SUCCESS) &&
      (result->file_seek_status == FX_SUCCESS) &&
      (result->file_read_status == FX_SUCCESS) &&
      (result->file_close_status == FX_SUCCESS) &&
      (result->fx_flush_status == FX_SUCCESS) &&
      (result->fx_close_status == FX_SUCCESS) &&
      (result->lx_close_status == LX_SUCCESS) &&
      (result->bytes_written == PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE) &&
      (result->bytes_read == PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE) &&
      (result->verify_mismatch_count == 0UL))
  {
    result->status = PS_STATUS_OK;
  }
  else
  {
    result->status = PS_STATUS_VERIFY_FAILED;
  }

  ps_storage_fxlx_block = NULL;
  ps_storage_fxlx_result = NULL;
  ps_storage_fxlx_msc_opened = 0UL;
  return result->status;
}
