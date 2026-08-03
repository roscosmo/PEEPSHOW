#ifndef PS_STORAGE_LAYOUT_H
#define PS_STORAGE_LAYOUT_H

#include <stdint.h>

#include "ps_status.h"
#include "ps_storage_flash_block.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_STORAGE_LAYOUT_API_VERSION (1UL)
#define PS_STORAGE_LAYOUT_REGION_COUNT (10UL)

typedef enum
{
  PS_STORAGE_REGION_SETTINGS = 0,
  PS_STORAGE_REGION_BONDING,
  PS_STORAGE_REGION_CALIBRATION,
  PS_STORAGE_REGION_SAVE_DATA,
  PS_STORAGE_REGION_PACKAGE_INDEX,
  PS_STORAGE_REGION_INSTALLED_PACKAGE,
  PS_STORAGE_REGION_USB_STAGING,
  PS_STORAGE_REGION_FAULT_LOG,
  PS_STORAGE_REGION_RESERVED_TAIL,
  PS_STORAGE_REGION_BRINGUP_SCRATCH
} ps_storage_region_id_t;

typedef struct
{
  ps_storage_region_id_t id;
  uint32_t start;
  uint32_t length;
  uint32_t host_exposed;
  uint32_t writable_in_bringup;
} ps_storage_region_t;

typedef struct
{
  ps_status_t status;
  uint32_t api_version;
  uint32_t region_count;
  uint32_t total_size;
  uint32_t erase_block_size;
  uint32_t layout_end;
  uint32_t alignment_error_count;
  uint32_t overlap_error_count;
  uint32_t range_error_count;
  uint32_t host_exposed_mask;
  uint32_t protected_mask;
  uint32_t scratch_region_index;
  uint32_t scratch_start;
  uint32_t scratch_length;
} ps_storage_layout_validation_t;

const ps_storage_region_t *ps_storage_layout_regions(uint32_t *count);

ps_status_t ps_storage_layout_validate(
  const ps_storage_flash_block_geometry_t *geometry,
  ps_storage_layout_validation_t *result);

#ifdef __cplusplus
}
#endif

#endif /* PS_STORAGE_LAYOUT_H */