#ifndef PS_STORAGE_FLASH_BLOCK_H
#define PS_STORAGE_FLASH_BLOCK_H

#include <stdint.h>

#include "ps_dev_at25sl128a.h"
#include "ps_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_STORAGE_FLASH_BLOCK_API_VERSION (1UL)
#define PS_STORAGE_FLASH_BLOCK_PATTERN_BASE (0x5AU)

typedef struct
{
  uint32_t total_size;
  uint32_t erase_block_size;
  uint32_t program_page_size;
  uint32_t logical_block_size;
  uint32_t logical_block_count;
} ps_storage_flash_block_geometry_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t operation_count;
  uint32_t last_status;
  ps_dev_at25sl128a_t *flash;
  ps_storage_flash_block_geometry_t geometry;
} ps_storage_flash_block_t;

typedef struct
{
  ps_status_t status;
  uint32_t address;
  uint32_t block_index;
  uint32_t length;
  uint32_t geometry_total_size;
  uint32_t geometry_erase_block_size;
  uint32_t geometry_program_page_size;
  uint32_t geometry_logical_block_count;
  uint32_t erase_status;
  uint32_t erase_poll_count;
  uint32_t blank_read_status;
  uint32_t blank_read_count;
  uint32_t blank_mismatch_count;
  uint8_t blank_first16[16];
  uint32_t program_status;
  uint32_t program_page_count;
  uint32_t program_last_poll_count;
  uint32_t verify_read_status;
  uint32_t verify_read_count;
  uint32_t verify_mismatch_count;
  uint8_t verify_first16[16];
  uint32_t cleanup_status;
  uint32_t cleanup_poll_count;
  uint32_t cleanup_read_status;
  uint32_t cleanup_mismatch_count;
  uint8_t cleanup_first16[16];
  uint32_t ospi_state_after;
  uint32_t ospi_error_after;
} ps_storage_flash_block_test_result_t;

ps_status_t ps_storage_flash_block_init(
  ps_storage_flash_block_t *block,
  ps_dev_at25sl128a_t *flash);

ps_status_t ps_storage_flash_block_get_geometry(
  const ps_storage_flash_block_t *block,
  ps_storage_flash_block_geometry_t *geometry);

ps_status_t ps_storage_flash_block_read(ps_storage_flash_block_t *block,
                                        uint32_t address,
                                        uint8_t *data,
                                        uint32_t length);

ps_status_t ps_storage_flash_block_program(ps_storage_flash_block_t *block,
                                           uint32_t address,
                                           const uint8_t *data,
                                           uint32_t length);

ps_status_t ps_storage_flash_block_erase(ps_storage_flash_block_t *block,
                                         uint32_t block_index,
                                         uint32_t *poll_count);

ps_status_t ps_storage_flash_block_verify_erased(
  ps_storage_flash_block_t *block,
  uint32_t block_index,
  uint32_t *mismatch_count);

ps_status_t ps_storage_flash_block_run_scratch_test(
  ps_storage_flash_block_t *block,
  uint32_t block_index,
  ps_storage_flash_block_test_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* PS_STORAGE_FLASH_BLOCK_H */