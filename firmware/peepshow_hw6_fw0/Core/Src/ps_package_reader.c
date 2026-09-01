#include "ps_package_reader.h"

#include <stddef.h>

volatile ps_package_reader_probe_t g_ps_package_reader_probe =
{
  .api_version = PS_PACKAGE_READER_API_VERSION,
  .last_status = PS_PACKAGE_READER_STATUS_NOT_RUN,
  .reason = PS_PACKAGE_READER_REASON_NONE
};

static ps_status_t PS_PackageReader_Fail(ps_status_t status,
                                         ps_package_reader_reason_t reason)
{
  g_ps_package_reader_probe.last_status = (uint32_t)status;
  g_ps_package_reader_probe.reason = (uint32_t)reason;
  return status;
}

void PS_PackageReader_StorageClear(void)
{
  g_ps_package_reader_probe.clear_count++;
  g_ps_package_reader_probe.available = 0UL;
  g_ps_package_reader_probe.package_start = 0UL;
  g_ps_package_reader_probe.package_size = 0UL;
  g_ps_package_reader_probe.generation = 0UL;
  g_ps_package_reader_probe.last_offset = 0UL;
  g_ps_package_reader_probe.last_length = 0UL;
  g_ps_package_reader_probe.last_status = PS_STATUS_OK;
  g_ps_package_reader_probe.reason = PS_PACKAGE_READER_REASON_NONE;
}

ps_status_t PS_PackageReader_StorageMount(uint32_t package_start,
                                          uint32_t package_size,
                                          uint32_t generation)
{
  if ((package_size == 0UL) || (generation == 0UL) ||
      (package_start > (UINT32_MAX - package_size)))
  {
    return PS_PackageReader_Fail(PS_STATUS_INVALID_ARGUMENT,
                                 PS_PACKAGE_READER_REASON_ARGUMENT);
  }

  g_ps_package_reader_probe.mount_count++;
  g_ps_package_reader_probe.available = 1UL;
  g_ps_package_reader_probe.package_start = package_start;
  g_ps_package_reader_probe.package_size = package_size;
  g_ps_package_reader_probe.generation = generation;
  g_ps_package_reader_probe.last_offset = 0UL;
  g_ps_package_reader_probe.last_length = 0UL;
  g_ps_package_reader_probe.last_status = PS_STATUS_OK;
  g_ps_package_reader_probe.reason = PS_PACKAGE_READER_REASON_NONE;
  return PS_STATUS_OK;
}

ps_status_t PS_PackageReader_StorageReadWindow(
  ps_storage_flash_block_t *block,
  uint32_t package_offset,
  uint8_t *destination,
  uint32_t length)
{
  ps_status_t status;

  if ((block == NULL) || (destination == NULL))
  {
    return PS_PackageReader_Fail(PS_STATUS_INVALID_ARGUMENT,
                                 PS_PACKAGE_READER_REASON_ARGUMENT);
  }
  if (g_ps_package_reader_probe.available == 0UL)
  {
    return PS_PackageReader_Fail(PS_STATUS_NOT_INITIALIZED,
                                 PS_PACKAGE_READER_REASON_UNAVAILABLE);
  }
  if ((length == 0UL) || (length > PS_PACKAGE_READER_WINDOW_BYTES))
  {
    return PS_PackageReader_Fail(PS_STATUS_INVALID_ARGUMENT,
                                 PS_PACKAGE_READER_REASON_WINDOW);
  }
  if ((package_offset > g_ps_package_reader_probe.package_size) ||
      (length > (g_ps_package_reader_probe.package_size - package_offset)))
  {
    return PS_PackageReader_Fail(PS_STATUS_INVALID_ARGUMENT,
                                 PS_PACKAGE_READER_REASON_BOUNDS);
  }

  g_ps_package_reader_probe.last_offset = package_offset;
  g_ps_package_reader_probe.last_length = length;
  status = ps_storage_flash_block_read(
    block,
    g_ps_package_reader_probe.package_start + package_offset,
    destination,
    length);
  g_ps_package_reader_probe.last_status = (uint32_t)status;
  if (status != PS_STATUS_OK)
  {
    g_ps_package_reader_probe.reason = PS_PACKAGE_READER_REASON_STORAGE;
    return status;
  }

  g_ps_package_reader_probe.read_count++;
  g_ps_package_reader_probe.read_bytes += length;
  g_ps_package_reader_probe.reason = PS_PACKAGE_READER_REASON_NONE;
  return PS_STATUS_OK;
}

uint32_t PS_PackageReader_GetDescriptor(ps_package_reader_descriptor_t *descriptor)
{
  if ((descriptor == NULL) ||
      (g_ps_package_reader_probe.available == 0UL))
  {
    return 0UL;
  }

  descriptor->package_start = g_ps_package_reader_probe.package_start;
  descriptor->package_size = g_ps_package_reader_probe.package_size;
  descriptor->generation = g_ps_package_reader_probe.generation;
  return 1UL;
}
