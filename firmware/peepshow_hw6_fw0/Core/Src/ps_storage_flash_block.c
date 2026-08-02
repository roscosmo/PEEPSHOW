#include "ps_storage_flash_block.h"

#include <string.h>

static uint8_t ps_storage_flash_block_tx[PS_DEV_AT25SL128A_PAGE_SIZE];
static uint8_t ps_storage_flash_block_rx[PS_DEV_AT25SL128A_PAGE_SIZE];

static uint8_t PS_StorageFlashBlock_PatternByte(uint32_t offset)
{
  return (uint8_t)(PS_STORAGE_FLASH_BLOCK_PATTERN_BASE ^
                   (uint8_t)offset ^
                   (uint8_t)(offset >> 4));
}

static uint32_t PS_StorageFlashBlock_CountBlankMismatches(
  const uint8_t *data,
  uint32_t length)
{
  uint32_t mismatches = 0UL;
  uint32_t index;

  for (index = 0UL; index < length; ++index)
  {
    if (data[index] != 0xFFU)
    {
      mismatches++;
    }
  }
  return mismatches;
}

static uint32_t PS_StorageFlashBlock_CountPatternMismatches(
  const uint8_t *data,
  uint32_t length,
  uint32_t base_offset)
{
  uint32_t mismatches = 0UL;
  uint32_t index;

  for (index = 0UL; index < length; ++index)
  {
    if (data[index] != PS_StorageFlashBlock_PatternByte(base_offset + index))
    {
      mismatches++;
    }
  }
  return mismatches;
}

static void PS_StorageFlashBlock_FillPattern(uint8_t *data,
                                             uint32_t length,
                                             uint32_t base_offset)
{
  uint32_t index;

  for (index = 0UL; index < length; ++index)
  {
    data[index] = PS_StorageFlashBlock_PatternByte(base_offset + index);
  }
}

static void PS_StorageFlashBlock_CopyFirst16(uint8_t *destination,
                                             const uint8_t *source,
                                             uint32_t length)
{
  uint32_t index;
  uint32_t copy_length = (length < 16UL) ? length : 16UL;

  for (index = 0UL; index < copy_length; ++index)
  {
    destination[index] = source[index];
  }
}

static ps_status_t PS_StorageFlashBlock_ReadAndCountBlank(
  ps_storage_flash_block_t *block,
  uint32_t address,
  uint32_t length,
  uint32_t *read_count,
  uint32_t *mismatch_count,
  uint8_t *first16)
{
  ps_dev_at25sl128a_io_result_t io_result;
  ps_status_t status = PS_STATUS_OK;
  uint32_t offset;
  uint32_t chunk;

  *read_count = 0UL;
  *mismatch_count = 0UL;
  for (offset = 0UL; offset < length; offset += PS_DEV_AT25SL128A_PAGE_SIZE)
  {
    chunk = length - offset;
    if (chunk > PS_DEV_AT25SL128A_PAGE_SIZE)
    {
      chunk = PS_DEV_AT25SL128A_PAGE_SIZE;
    }
    (void)memset(ps_storage_flash_block_rx,
                 0x00,
                 PS_DEV_AT25SL128A_PAGE_SIZE);
    status = ps_dev_at25sl128a_read(block->flash,
                                        address + offset,
                                        ps_storage_flash_block_rx,
                                        chunk,
                                        &io_result);
    if (status != PS_STATUS_OK)
    {
      return status;
    }
    if (offset == 0UL)
    {
      PS_StorageFlashBlock_CopyFirst16(first16,
                                       ps_storage_flash_block_rx,
                                       chunk);
    }
    *read_count += 1UL;
    *mismatch_count += PS_StorageFlashBlock_CountBlankMismatches(
      ps_storage_flash_block_rx,
      chunk);
  }
  return status;
}

static ps_status_t PS_StorageFlashBlock_ReadAndCountPattern(
  ps_storage_flash_block_t *block,
  uint32_t address,
  uint32_t length,
  uint32_t *read_count,
  uint32_t *mismatch_count,
  uint8_t *first16)
{
  ps_dev_at25sl128a_io_result_t io_result;
  ps_status_t status = PS_STATUS_OK;
  uint32_t offset;
  uint32_t chunk;

  *read_count = 0UL;
  *mismatch_count = 0UL;
  for (offset = 0UL; offset < length; offset += PS_DEV_AT25SL128A_PAGE_SIZE)
  {
    chunk = length - offset;
    if (chunk > PS_DEV_AT25SL128A_PAGE_SIZE)
    {
      chunk = PS_DEV_AT25SL128A_PAGE_SIZE;
    }
    (void)memset(ps_storage_flash_block_rx,
                 0x00,
                 PS_DEV_AT25SL128A_PAGE_SIZE);
    status = ps_dev_at25sl128a_read(block->flash,
                                        address + offset,
                                        ps_storage_flash_block_rx,
                                        chunk,
                                        &io_result);
    if (status != PS_STATUS_OK)
    {
      return status;
    }
    if (offset == 0UL)
    {
      PS_StorageFlashBlock_CopyFirst16(first16,
                                       ps_storage_flash_block_rx,
                                       chunk);
    }
    *read_count += 1UL;
    *mismatch_count += PS_StorageFlashBlock_CountPatternMismatches(
      ps_storage_flash_block_rx,
      chunk,
      offset);
  }
  return status;
}

ps_status_t ps_storage_flash_block_init(
  ps_storage_flash_block_t *block,
  ps_dev_at25sl128a_t *flash)
{
  if ((block == NULL) || (flash == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(block, 0, sizeof(*block));
  block->api_version = PS_STORAGE_FLASH_BLOCK_API_VERSION;
  block->flash = flash;
  block->geometry.total_size = PS_DEV_AT25SL128A_TOTAL_SIZE;
  block->geometry.erase_block_size = PS_DEV_AT25SL128A_SECTOR_SIZE;
  block->geometry.program_page_size = PS_DEV_AT25SL128A_PAGE_SIZE;
  block->geometry.logical_block_size = PS_DEV_AT25SL128A_SECTOR_SIZE;
  block->geometry.logical_block_count =
    PS_DEV_AT25SL128A_TOTAL_SIZE / PS_DEV_AT25SL128A_SECTOR_SIZE;
  block->initialized = 1UL;
  block->last_status = (uint32_t)PS_STATUS_OK;
  return PS_STATUS_OK;
}

ps_status_t ps_storage_flash_block_get_geometry(
  const ps_storage_flash_block_t *block,
  ps_storage_flash_block_geometry_t *geometry)
{
  if ((block == NULL) || (geometry == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (block->initialized == 0UL)
  {
    return PS_STATUS_NOT_INITIALIZED;
  }

  *geometry = block->geometry;
  return PS_STATUS_OK;
}

ps_status_t ps_storage_flash_block_run_scratch_test(
  ps_storage_flash_block_t *block,
  uint32_t block_index,
  ps_storage_flash_block_test_result_t *result)
{
  ps_dev_at25sl128a_io_result_t io_result;
  ps_status_t status = PS_STATUS_OK;
  uint32_t offset;

  if ((block == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->erase_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->blank_read_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->program_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->verify_read_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->cleanup_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  result->cleanup_read_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
  if (block->initialized == 0UL)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  if (block_index >= block->geometry.logical_block_count)
  {
    result->status = PS_STATUS_INVALID_ARGUMENT;
    return result->status;
  }

  block->operation_count++;
  result->block_index = block_index;
  result->address = block_index * block->geometry.logical_block_size;
  result->length = block->geometry.logical_block_size;
  result->geometry_total_size = block->geometry.total_size;
  result->geometry_erase_block_size = block->geometry.erase_block_size;
  result->geometry_program_page_size = block->geometry.program_page_size;
  result->geometry_logical_block_count =
    block->geometry.logical_block_count;

  status = ps_dev_at25sl128a_erase_4k(block->flash,
                                      result->address,
                                      &io_result);
  result->erase_status = (uint32_t)status;
  result->erase_poll_count = io_result.flash_poll_count;
  if (status != PS_STATUS_OK)
  {
    goto block_done;
  }

  status = PS_StorageFlashBlock_ReadAndCountBlank(
    block,
    result->address,
    result->length,
    &result->blank_read_count,
    &result->blank_mismatch_count,
    result->blank_first16);
  result->blank_read_status = (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    goto cleanup;
  }
  if (result->blank_mismatch_count != 0UL)
  {
    status = PS_STATUS_VERIFY_FAILED;
    goto cleanup;
  }

  for (offset = 0UL; offset < result->length;
       offset += PS_DEV_AT25SL128A_PAGE_SIZE)
  {
    PS_StorageFlashBlock_FillPattern(ps_storage_flash_block_tx,
                                     PS_DEV_AT25SL128A_PAGE_SIZE,
                                     offset);
    status = ps_dev_at25sl128a_program_page(
      block->flash,
      result->address + offset,
      ps_storage_flash_block_tx,
      PS_DEV_AT25SL128A_PAGE_SIZE,
      &io_result);
    result->program_status = (uint32_t)status;
    result->program_last_poll_count = io_result.flash_poll_count;
    if (status != PS_STATUS_OK)
    {
      goto cleanup;
    }
    result->program_page_count++;
  }

  status = PS_StorageFlashBlock_ReadAndCountPattern(
    block,
    result->address,
    result->length,
    &result->verify_read_count,
    &result->verify_mismatch_count,
    result->verify_first16);
  result->verify_read_status = (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    goto cleanup;
  }
  if (result->verify_mismatch_count != 0UL)
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

cleanup:
  {
    ps_status_t cleanup_status = ps_dev_at25sl128a_erase_4k(
      block->flash,
      result->address,
      &io_result);
    result->cleanup_status = (uint32_t)cleanup_status;
    result->cleanup_poll_count = io_result.flash_poll_count;
    if ((cleanup_status != PS_STATUS_OK) && (status == PS_STATUS_OK))
    {
      status = cleanup_status;
    }
  }
  if (result->cleanup_status == (uint32_t)PS_STATUS_OK)
  {
    ps_status_t cleanup_read_status = PS_StorageFlashBlock_ReadAndCountBlank(
      block,
      result->address,
      result->length,
      &result->blank_read_count,
      &result->cleanup_mismatch_count,
      result->cleanup_first16);
    result->cleanup_read_status = (uint32_t)cleanup_read_status;
    if ((cleanup_read_status != PS_STATUS_OK) &&
        (status == PS_STATUS_OK))
    {
      status = cleanup_read_status;
    }
    if ((result->cleanup_mismatch_count != 0UL) &&
        (status == PS_STATUS_OK))
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

block_done:
  result->ospi_state_after = (uint32_t)HAL_OSPI_GetState(block->flash->ospi);
  result->ospi_error_after = HAL_OSPI_GetError(block->flash->ospi);
  if ((status == PS_STATUS_OK) &&
      (result->ospi_error_after != HAL_OSPI_ERROR_NONE))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }
  result->status = status;
  block->last_status = (uint32_t)status;
  return result->status;
}