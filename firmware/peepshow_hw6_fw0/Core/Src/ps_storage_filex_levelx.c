#include "ps_storage_filex_levelx.h"

#include <stdint.h>
#include <string.h>

#include "fx_api.h"
#include "lx_api.h"

#define PS_STORAGE_FXLX_TEST_LENGTH          (0x00100000UL)
#define PS_STORAGE_FXLX_SECTOR_SIZE          (512UL)
#define PS_STORAGE_FXLX_FAKE_BASE            (0x10000000UL)
#define PS_STORAGE_FXLX_MEDIA_MEMORY_SIZE    (4096UL)
#define PS_STORAGE_FXLX_FILE_PAYLOAD_SIZE    (256UL)
#define PS_STORAGE_FXLX_NOT_RUN              (0xFFFFFFFFUL)
#define PS_STORAGE_FXLX_VOLUME_NAME          "PSHW6"
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
  return PS_StorageFxLx_StatusToFx(status);
}

static UINT PS_StorageFxLx_SystemError(UINT error_code)
{
  if (ps_storage_fxlx_result != NULL)
  {
    ps_storage_fxlx_result->lx_driver_last_status = error_code;
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

ps_status_t ps_storage_filex_levelx_run_smoke(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  ps_storage_filex_levelx_smoke_result_t *result)
{
  UINT status;
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
  result->test_length = PS_StorageFxLx_Min(region->length,
                                           PS_STORAGE_FXLX_TEST_LENGTH);
  result->erase_block_size = block->geometry.erase_block_size;
  result->logical_sector_size = PS_STORAGE_FXLX_SECTOR_SIZE;
  result->logical_sector_count = result->test_length /
                                 PS_STORAGE_FXLX_SECTOR_SIZE;

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
  (void)memset(&ps_storage_fxlx_nor, 0, sizeof(ps_storage_fxlx_nor));
  (void)memset(&ps_storage_fxlx_media, 0, sizeof(ps_storage_fxlx_media));
  (void)memset(&ps_storage_fxlx_file, 0, sizeof(ps_storage_fxlx_file));
  (void)memset(ps_storage_fxlx_media_memory, 0,
               sizeof(ps_storage_fxlx_media_memory));
  (void)memset(ps_storage_fxlx_read_buffer, 0,
               sizeof(ps_storage_fxlx_read_buffer));
  PS_StorageFxLx_FillPayload();

  status = lx_nor_flash_initialize();
  result->lx_initialize_status = status;
  if (status != LX_SUCCESS)
  {
    result->status = PS_STATUS_INTERNAL_ERROR;
    return result->status;
  }

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
                           1U,
                           32U,
                           0U,
                           result->logical_sector_count,
                           PS_STORAGE_FXLX_SECTOR_SIZE,
                           1U,
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
  return result->status;
}
