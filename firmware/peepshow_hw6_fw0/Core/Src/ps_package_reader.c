#include "ps_package_reader.h"

#include <stddef.h>

volatile ps_package_reader_probe_t g_ps_package_reader_probe =
{
  .api_version = PS_PACKAGE_READER_API_VERSION,
  .last_status = PS_PACKAGE_READER_STATUS_NOT_RUN,
  .request_status = PS_PACKAGE_READER_STATUS_NOT_RUN,
  .reason = PS_PACKAGE_READER_REASON_NONE
};

typedef enum
{
  PS_PACKAGE_READER_REQUEST_IDLE = 0,
  PS_PACKAGE_READER_REQUEST_QUEUED,
  PS_PACKAGE_READER_REQUEST_IN_FLIGHT,
  PS_PACKAGE_READER_REQUEST_COMPLETE
} ps_package_reader_request_state_t;

static volatile ps_package_reader_request_state_t s_ps_package_reader_request_state;
static uint8_t * volatile s_ps_package_reader_request_destination;
static volatile uint32_t s_ps_package_reader_request_offset;
static volatile uint32_t s_ps_package_reader_request_length;
static volatile ps_status_t s_ps_package_reader_request_status;

static ps_status_t PS_PackageReader_Fail(ps_status_t status,
                                         ps_package_reader_reason_t reason)
{
  g_ps_package_reader_probe.last_status = (uint32_t)status;
  g_ps_package_reader_probe.reason = (uint32_t)reason;
  return status;
}

void PS_PackageReader_StorageClear(void)
{
  s_ps_package_reader_request_state = PS_PACKAGE_READER_REQUEST_IDLE;
  s_ps_package_reader_request_destination = NULL;
  s_ps_package_reader_request_offset = 0UL;
  s_ps_package_reader_request_length = 0UL;
  s_ps_package_reader_request_status = PS_STATUS_OK;
  g_ps_package_reader_probe.clear_count++;
  g_ps_package_reader_probe.available = 0UL;
  g_ps_package_reader_probe.package_start = 0UL;
  g_ps_package_reader_probe.package_size = 0UL;
  g_ps_package_reader_probe.generation = 0UL;
  g_ps_package_reader_probe.last_offset = 0UL;
  g_ps_package_reader_probe.last_length = 0UL;
  g_ps_package_reader_probe.last_status = PS_STATUS_OK;
  g_ps_package_reader_probe.reason = PS_PACKAGE_READER_REASON_NONE;
  g_ps_package_reader_probe.request_pending = 0UL;
  g_ps_package_reader_probe.request_offset = 0UL;
  g_ps_package_reader_probe.request_length = 0UL;
  g_ps_package_reader_probe.request_status = PS_STATUS_OK;
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

ps_status_t PS_PackageReader_RuntimeBeginWindowRead(
  uint32_t package_offset,
  uint8_t *destination,
  uint32_t length)
{
  if (s_ps_package_reader_request_state == PS_PACKAGE_READER_REQUEST_COMPLETE)
  {
    s_ps_package_reader_request_state = PS_PACKAGE_READER_REQUEST_IDLE;
  }
  if (s_ps_package_reader_request_state != PS_PACKAGE_READER_REQUEST_IDLE)
  {
    return PS_PackageReader_Fail(PS_STATUS_BUSY,
                                 PS_PACKAGE_READER_REASON_BUSY);
  }
  if (destination == NULL)
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

  s_ps_package_reader_request_destination = destination;
  s_ps_package_reader_request_offset = package_offset;
  s_ps_package_reader_request_length = length;
  s_ps_package_reader_request_status = PS_STATUS_INTERNAL_ERROR;
  s_ps_package_reader_request_state = PS_PACKAGE_READER_REQUEST_QUEUED;
  g_ps_package_reader_probe.runtime_request_count++;
  g_ps_package_reader_probe.request_pending = 1UL;
  g_ps_package_reader_probe.request_offset = package_offset;
  g_ps_package_reader_probe.request_length = length;
  g_ps_package_reader_probe.request_status = PS_PACKAGE_READER_STATUS_NOT_RUN;
  return PS_STATUS_OK;
}

uint32_t PS_PackageReader_StorageTakeWindowRead(
  uint32_t *package_offset,
  uint8_t **destination,
  uint32_t *length)
{
  if ((package_offset == NULL) || (destination == NULL) || (length == NULL) ||
      (s_ps_package_reader_request_state !=
       PS_PACKAGE_READER_REQUEST_QUEUED))
  {
    return 0UL;
  }

  *package_offset = s_ps_package_reader_request_offset;
  *destination = s_ps_package_reader_request_destination;
  *length = s_ps_package_reader_request_length;
  s_ps_package_reader_request_state = PS_PACKAGE_READER_REQUEST_IN_FLIGHT;
  g_ps_package_reader_probe.storage_service_count++;
  return 1UL;
}

void PS_PackageReader_StorageCompleteWindowRead(ps_status_t status)
{
  if (s_ps_package_reader_request_state != PS_PACKAGE_READER_REQUEST_IN_FLIGHT)
  {
    return;
  }

  s_ps_package_reader_request_status = status;
  s_ps_package_reader_request_state = PS_PACKAGE_READER_REQUEST_COMPLETE;
  g_ps_package_reader_probe.complete_count++;
  g_ps_package_reader_probe.request_pending = 0UL;
  g_ps_package_reader_probe.request_status = (uint32_t)status;
}

ps_status_t PS_PackageReader_RuntimeFinishWindowRead(void)
{
  ps_status_t status;

  if (s_ps_package_reader_request_state != PS_PACKAGE_READER_REQUEST_COMPLETE)
  {
    return PS_PackageReader_Fail(PS_STATUS_BUSY,
                                 PS_PACKAGE_READER_REASON_BUSY);
  }

  status = s_ps_package_reader_request_status;
  s_ps_package_reader_request_state = PS_PACKAGE_READER_REQUEST_IDLE;
  s_ps_package_reader_request_destination = NULL;
  s_ps_package_reader_request_offset = 0UL;
  s_ps_package_reader_request_length = 0UL;
  g_ps_package_reader_probe.request_status = (uint32_t)status;
  return status;
}

void PS_PackageReader_RuntimeCancelWindowRead(void)
{
  if (s_ps_package_reader_request_state != PS_PACKAGE_READER_REQUEST_QUEUED)
  {
    return;
  }

  s_ps_package_reader_request_state = PS_PACKAGE_READER_REQUEST_IDLE;
  s_ps_package_reader_request_destination = NULL;
  s_ps_package_reader_request_offset = 0UL;
  s_ps_package_reader_request_length = 0UL;
  s_ps_package_reader_request_status = PS_STATUS_OK;
  g_ps_package_reader_probe.request_pending = 0UL;
  g_ps_package_reader_probe.request_status = PS_STATUS_OK;
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
