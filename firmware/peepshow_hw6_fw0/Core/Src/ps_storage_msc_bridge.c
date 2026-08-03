#include "ps_storage_msc_bridge.h"

#include <string.h>

#include "ps_hw6_rtos_probe.h"

#define PS_STORAGE_MSC_BRIDGE_WAIT_TICKS (1000UL)
#define PS_STORAGE_MSC_BRIDGE_MEDIA_OK   (0UL)
#define PS_STORAGE_MSC_BRIDGE_MEDIA_FAIL (1UL)

volatile ps_storage_msc_bridge_probe_t g_ps_storage_msc_bridge_probe;

static TX_QUEUE *ps_storage_msc_queue;
static TX_MUTEX ps_storage_msc_mutex;
static TX_SEMAPHORE ps_storage_msc_done;
static ps_storage_msc_request_t ps_storage_msc_request;
static uint32_t ps_storage_msc_objects_created;

static UINT PS_StorageMscBridge_CommandToRtos(
  ps_storage_msc_command_t command)
{
  switch (command)
  {
    case PS_STORAGE_MSC_COMMAND_READ:
      return PS_HW6_RTOS_STORAGE_MSC_READ;
    case PS_STORAGE_MSC_COMMAND_WRITE:
      return PS_HW6_RTOS_STORAGE_MSC_WRITE;
    case PS_STORAGE_MSC_COMMAND_FLUSH:
      return PS_HW6_RTOS_STORAGE_MSC_FLUSH;
    case PS_STORAGE_MSC_COMMAND_STATUS:
      return PS_HW6_RTOS_STORAGE_MSC_STATUS;
    default:
      return 0U;
  }
}

static UINT PS_StorageMscBridge_Reject(UINT ux_status,
                                       ULONG *media_status)
{
  if (media_status != UX_NULL)
  {
    *media_status = PS_STORAGE_MSC_BRIDGE_MEDIA_FAIL;
  }
  g_ps_storage_msc_bridge_probe.last_ux_status = ux_status;
  g_ps_storage_msc_bridge_probe.last_media_status =
    PS_STORAGE_MSC_BRIDGE_MEDIA_FAIL;
  return ux_status;
}

static UINT PS_StorageMscBridge_RejectPolicy(ps_storage_msc_command_t command,
                                             ULONG *media_status)
{
  g_ps_storage_msc_bridge_probe.denied_count++;
  switch (command)
  {
    case PS_STORAGE_MSC_COMMAND_READ:
      g_ps_storage_msc_bridge_probe.denied_read_count++;
      break;
    case PS_STORAGE_MSC_COMMAND_WRITE:
      g_ps_storage_msc_bridge_probe.denied_write_count++;
      break;
    case PS_STORAGE_MSC_COMMAND_STATUS:
      g_ps_storage_msc_bridge_probe.denied_status_count++;
      break;
    default:
      break;
  }
  return PS_StorageMscBridge_Reject(UX_ERROR, media_status);
}

UINT PS_StorageMscBridge_Init(TX_QUEUE *storage_queue)
{
  UINT status;

  (void)memset((void *)&g_ps_storage_msc_bridge_probe, 0,
               sizeof(g_ps_storage_msc_bridge_probe));
  g_ps_storage_msc_bridge_probe.api_version =
    PS_STORAGE_MSC_BRIDGE_API_VERSION;
  ps_storage_msc_queue = storage_queue;
  (void)memset(&ps_storage_msc_request, 0,
               sizeof(ps_storage_msc_request));

  status = tx_mutex_create(&ps_storage_msc_mutex,
                           "psStorageMscMutex",
                           TX_NO_INHERIT);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  status = tx_semaphore_create(&ps_storage_msc_done,
                               "psStorageMscDone",
                               0UL);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  ps_storage_msc_objects_created = 1UL;
  g_ps_storage_msc_bridge_probe.initialized = 1UL;
  return TX_SUCCESS;
}

void PS_StorageMscBridge_SetPolicy(uint32_t export_enabled,
                                   uint32_t media_present,
                                   uint32_t write_enabled)
{
  g_ps_storage_msc_bridge_probe.export_enabled =
    (export_enabled != 0UL) ? 1UL : 0UL;
  g_ps_storage_msc_bridge_probe.media_present =
    (media_present != 0UL) ? 1UL : 0UL;
  g_ps_storage_msc_bridge_probe.write_enabled =
    (write_enabled != 0UL) ? 1UL : 0UL;
}

void PS_StorageMscBridge_MarkActivated(void)
{
  g_ps_storage_msc_bridge_probe.activate_count++;
}

void PS_StorageMscBridge_MarkDeactivated(void)
{
  g_ps_storage_msc_bridge_probe.deactivate_count++;
}

UINT PS_StorageMscBridge_Submit(ps_storage_msc_command_t command,
                                uint8_t *data,
                                uint32_t lba,
                                uint32_t block_count,
                                ULONG *media_status)
{
  ULONG message[PS_HW6_RTOS_MESSAGE_WORDS];
  UINT rtos_command = PS_StorageMscBridge_CommandToRtos(command);
  UINT status;

  if (media_status != UX_NULL)
  {
    *media_status = PS_STORAGE_MSC_BRIDGE_MEDIA_FAIL;
  }

  if ((ps_storage_msc_objects_created == 0UL) ||
      (ps_storage_msc_queue == TX_NULL) ||
      (rtos_command == 0U))
  {
    return PS_StorageMscBridge_Reject(UX_ERROR, media_status);
  }
  if (((command == PS_STORAGE_MSC_COMMAND_READ) ||
       (command == PS_STORAGE_MSC_COMMAND_WRITE)) &&
      ((data == UX_NULL) || (block_count == 0UL)))
  {
    return PS_StorageMscBridge_Reject(UX_ERROR, media_status);
  }
  if ((g_ps_storage_msc_bridge_probe.export_enabled == 0UL) ||
      (g_ps_storage_msc_bridge_probe.media_present == 0UL))
  {
    return PS_StorageMscBridge_RejectPolicy(command, media_status);
  }
  if ((command == PS_STORAGE_MSC_COMMAND_WRITE) &&
      (g_ps_storage_msc_bridge_probe.write_enabled == 0UL))
  {
    return PS_StorageMscBridge_RejectPolicy(command, media_status);
  }
  if ((block_count > PS_STORAGE_MSC_BRIDGE_BLOCK_COUNT) ||
      (lba > PS_STORAGE_MSC_BRIDGE_LAST_LBA) ||
      (block_count > (PS_STORAGE_MSC_BRIDGE_BLOCK_COUNT - lba)))
  {
    return PS_StorageMscBridge_Reject(UX_ERROR, media_status);
  }

  status = tx_mutex_get(&ps_storage_msc_mutex, TX_NO_WAIT);
  if (status != TX_SUCCESS)
  {
    g_ps_storage_msc_bridge_probe.busy_count++;
    return PS_StorageMscBridge_Reject(UX_ERROR, media_status);
  }

  while (tx_semaphore_get(&ps_storage_msc_done, TX_NO_WAIT) == TX_SUCCESS)
  {
  }

  ps_storage_msc_request.command = (uint32_t)command;
  ps_storage_msc_request.data = data;
  ps_storage_msc_request.lba = lba;
  ps_storage_msc_request.block_count = block_count;
  ps_storage_msc_request.media_status = PS_STORAGE_MSC_BRIDGE_MEDIA_FAIL;
  ps_storage_msc_request.ux_status = UX_ERROR;
  ps_storage_msc_request.ps_status = 0xFFFFFFFFUL;

  g_ps_storage_msc_bridge_probe.submit_count++;
  g_ps_storage_msc_bridge_probe.last_command = (uint32_t)command;
  g_ps_storage_msc_bridge_probe.last_lba = lba;
  g_ps_storage_msc_bridge_probe.last_block_count = block_count;

  message[0] = PS_HW6_RTOS_STORAGE_MSC_MAGIC;
  message[1] = (ULONG)PS_HW6_RTOS_OWNER_STORAGE;
  message[2] = (ULONG)rtos_command;
  message[3] = PS_HW6_RTOS_STORAGE_MSC_TOKEN;

  status = tx_queue_send(ps_storage_msc_queue, message, TX_NO_WAIT);
  g_ps_storage_msc_bridge_probe.last_tx_status = status;
  if (status == TX_SUCCESS)
  {
    status = tx_semaphore_get(&ps_storage_msc_done,
                              PS_STORAGE_MSC_BRIDGE_WAIT_TICKS);
  }

  if (status == TX_SUCCESS)
  {
    g_ps_storage_msc_bridge_probe.completed_count++;
  }
  else
  {
    g_ps_storage_msc_bridge_probe.timeout_count++;
    ps_storage_msc_request.ux_status = UX_ERROR;
    ps_storage_msc_request.media_status =
      PS_STORAGE_MSC_BRIDGE_MEDIA_FAIL;
  }

  if (media_status != UX_NULL)
  {
    *media_status = ps_storage_msc_request.media_status;
  }
  g_ps_storage_msc_bridge_probe.last_owner_status = status;
  g_ps_storage_msc_bridge_probe.last_ux_status =
    ps_storage_msc_request.ux_status;
  g_ps_storage_msc_bridge_probe.last_media_status =
    (uint32_t)ps_storage_msc_request.media_status;
  g_ps_storage_msc_bridge_probe.last_ps_status =
    ps_storage_msc_request.ps_status;
  if ((ps_storage_msc_request.ux_status == UX_SUCCESS) &&
      (command == PS_STORAGE_MSC_COMMAND_WRITE))
  {
    g_ps_storage_msc_bridge_probe.dirty = 1UL;
  }

  (void)tx_mutex_put(&ps_storage_msc_mutex);
  return ps_storage_msc_request.ux_status;
}

ps_storage_msc_request_t *PS_StorageMscBridge_CurrentRequest(void)
{
  return &ps_storage_msc_request;
}

void PS_StorageMscBridge_Complete(UINT ux_status,
                                  ULONG media_status,
                                  uint32_t ps_status)
{
  ps_storage_msc_request.ux_status = ux_status;
  ps_storage_msc_request.media_status = media_status;
  ps_storage_msc_request.ps_status = ps_status;
  (void)tx_semaphore_put(&ps_storage_msc_done);
}
