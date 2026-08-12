#ifndef PS_STORAGE_FILEX_LEVELX_H
#define PS_STORAGE_FILEX_LEVELX_H

#include <stdint.h>

#include "ps_status.h"
#include "ps_storage_flash_block.h"
#include "ps_storage_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_STORAGE_FILEX_LEVELX_API_VERSION (1UL)
#define PS_STORAGE_FILEX_LEVELX_MSC_PROBE_API_VERSION (3UL)

typedef struct
{
  ps_status_t status;
  uint32_t api_version;
  uint32_t region_id;
  uint32_t region_start;
  uint32_t region_length;
  uint32_t test_start;
  uint32_t test_length;
  uint32_t erase_block_size;
  uint32_t logical_sector_size;
  uint32_t logical_sector_count;
  uint32_t preformat_erase_status;
  uint32_t preformat_erase_block_count;
  uint32_t preformat_erase_failed_block;
  uint32_t preformat_erase_last_poll_count;
  uint32_t lx_initialize_status;
  uint32_t lx_open_status;
  uint32_t fx_format_status;
  uint32_t fx_open_status;
  uint32_t file_create_status;
  uint32_t file_open_status;
  uint32_t file_write_status;
  uint32_t file_seek_status;
  uint32_t file_read_status;
  uint32_t file_close_status;
  uint32_t fx_flush_status;
  uint32_t fx_close_status;
  uint32_t lx_close_status;
  uint32_t bytes_written;
  uint32_t bytes_read;
  uint32_t verify_mismatch_count;
  uint32_t boot_read_first16[16];
  uint32_t boot_bytes_per_sector;
  uint32_t boot_sectors_per_cluster;
  uint32_t boot_reserved_sectors;
  uint32_t boot_number_of_fats;
  uint32_t boot_root_entries;
  uint32_t boot_total_sectors;
  uint32_t boot_sectors_per_fat;
  uint32_t boot_signature;
  uint32_t read_first16[16];
  uint32_t lx_driver_read_count;
  uint32_t lx_driver_write_count;
  uint32_t lx_driver_erase_count;
  uint32_t lx_driver_verify_count;
  uint32_t lx_driver_last_status;
  uint32_t fx_driver_read_count;
  uint32_t fx_driver_write_count;
  uint32_t fx_driver_flush_count;
  uint32_t fx_driver_abort_count;
  uint32_t fx_driver_init_count;
  uint32_t fx_driver_uninit_count;
  uint32_t fx_driver_release_count;
  uint32_t fx_driver_last_request;
  uint32_t fx_driver_last_status;
} ps_storage_filex_levelx_smoke_result_t;

typedef struct
{
  uint32_t api_version;
  uint32_t active;
  uint32_t open_count;
  uint32_t close_count;
  uint32_t last_stage;
  uint32_t status;
  uint32_t validate_status;
  uint32_t export_length;
  uint32_t region_id;
  uint32_t region_start;
  uint32_t region_length;
  uint32_t already_open;
  uint32_t invalid_media_detected;
  uint32_t recovery_required_count;
  uint32_t recovery_lx_open_status;
  uint32_t recovery_driver_status;
  uint32_t lx_initialize_status;
  uint32_t lx_open_status;
  uint32_t lx_close_status;
  uint32_t lx_driver_read_count;
  uint32_t lx_driver_write_count;
  uint32_t lx_driver_erase_count;
  uint32_t lx_driver_verify_count;
  uint32_t lx_driver_last_status;
  uint32_t block_last_status;
  uint32_t flash_state;
  uint32_t flash_last_status;
  uint32_t ospi_state_after;
  uint32_t ospi_error_after;
  uint32_t nor_state;
} ps_storage_filex_levelx_msc_probe_t;

extern volatile ps_storage_filex_levelx_msc_probe_t
  g_ps_storage_filex_levelx_msc_probe;

ps_status_t ps_storage_filex_levelx_run_smoke(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  ps_storage_filex_levelx_smoke_result_t *result);
ps_status_t ps_storage_filex_levelx_initialize_usb_staging(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region,
  ps_storage_filex_levelx_smoke_result_t *result);

ps_status_t ps_storage_filex_levelx_msc_open(
  ps_storage_flash_block_t *block,
  const ps_storage_region_t *region);
ps_status_t ps_storage_filex_levelx_msc_close(void);
ps_status_t ps_storage_filex_levelx_msc_read(uint32_t lba,
                                             uint32_t block_count,
                                             uint8_t *data);
ps_status_t ps_storage_filex_levelx_msc_write(uint32_t lba,
                                              uint32_t block_count,
                                              const uint8_t *data);
uint32_t ps_storage_filex_levelx_msc_is_open(void);

#ifdef __cplusplus
}
#endif

#endif /* PS_STORAGE_FILEX_LEVELX_H */
