#include "ps_storage_layout.h"

#include <stddef.h>
#include <string.h>

#define PS_STORAGE_LAYOUT_TOTAL_SIZE       (0x01000000UL)
#define PS_STORAGE_LAYOUT_ERASE_SIZE       (0x00001000UL)
#define PS_STORAGE_LAYOUT_SCRATCH_START    (0x00FFF000UL)
#define PS_STORAGE_LAYOUT_SCRATCH_LENGTH   (0x00001000UL)

static const ps_storage_region_t ps_storage_regions[PS_STORAGE_LAYOUT_REGION_COUNT] =
{
  {PS_STORAGE_REGION_SETTINGS,          0x00000000UL, 0x00010000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_BONDING,           0x00010000UL, 0x00010000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_CALIBRATION,       0x00020000UL, 0x00010000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_SAVE_DATA,         0x00030000UL, 0x00080000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_PACKAGE_INDEX,     0x000B0000UL, 0x00010000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_INSTALLED_PACKAGE, 0x000C0000UL, 0x00A00000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_USB_STAGING,       0x00AC0000UL, 0x00500000UL, 1UL, 0UL},
  {PS_STORAGE_REGION_FAULT_LOG,         0x00FC0000UL, 0x00030000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_RESERVED_TAIL,     0x00FF0000UL, 0x0000F000UL, 0UL, 0UL},
  {PS_STORAGE_REGION_BRINGUP_SCRATCH,   0x00FFF000UL, 0x00001000UL, 0UL, 1UL}
};

const ps_storage_region_t *ps_storage_layout_regions(uint32_t *count)
{
  if (count != NULL)
  {
    *count = PS_STORAGE_LAYOUT_REGION_COUNT;
  }
  return ps_storage_regions;
}

ps_status_t ps_storage_layout_validate(
  const ps_storage_flash_block_geometry_t *geometry,
  ps_storage_layout_validation_t *result)
{
  uint32_t index;
  uint32_t previous_end = 0UL;

  if ((geometry == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(result, 0, sizeof(*result));
  result->api_version = PS_STORAGE_LAYOUT_API_VERSION;
  result->region_count = PS_STORAGE_LAYOUT_REGION_COUNT;
  result->total_size = geometry->total_size;
  result->erase_block_size = geometry->erase_block_size;
  result->scratch_region_index = PS_STORAGE_LAYOUT_REGION_COUNT;

  if ((geometry->total_size != PS_STORAGE_LAYOUT_TOTAL_SIZE) ||
      (geometry->erase_block_size != PS_STORAGE_LAYOUT_ERASE_SIZE) ||
      (geometry->erase_block_size == 0UL))
  {
    result->range_error_count++;
  }
  if (geometry->erase_block_size == 0UL)
  {
    result->status = PS_STATUS_VERIFY_FAILED;
    return result->status;
  }

  for (index = 0UL; index < PS_STORAGE_LAYOUT_REGION_COUNT; ++index)
  {
    const ps_storage_region_t *region = &ps_storage_regions[index];
    uint32_t end = region->start + region->length;

    if (((region->start % geometry->erase_block_size) != 0UL) ||
        ((region->length % geometry->erase_block_size) != 0UL))
    {
      result->alignment_error_count++;
    }
    if ((region->length == 0UL) || (end > geometry->total_size) ||
        (end < region->start))
    {
      result->range_error_count++;
    }
    if ((index != 0UL) && (region->start < previous_end))
    {
      result->overlap_error_count++;
    }
    if (region->host_exposed != 0UL)
    {
      result->host_exposed_mask |= 1UL << index;
    }
    else
    {
      result->protected_mask |= 1UL << index;
    }
    if (region->id == PS_STORAGE_REGION_BRINGUP_SCRATCH)
    {
      result->scratch_region_index = index;
      result->scratch_start = region->start;
      result->scratch_length = region->length;
    }
    previous_end = end;
    result->layout_end = end;
  }

  if ((result->scratch_start != PS_STORAGE_LAYOUT_SCRATCH_START) ||
      (result->scratch_length != PS_STORAGE_LAYOUT_SCRATCH_LENGTH))
  {
    result->range_error_count++;
  }
  if (result->layout_end != geometry->total_size)
  {
    result->range_error_count++;
  }

  result->status =
    ((result->alignment_error_count == 0UL) &&
     (result->overlap_error_count == 0UL) &&
     (result->range_error_count == 0UL)) ? PS_STATUS_OK :
                                           PS_STATUS_VERIFY_FAILED;
  return result->status;
}