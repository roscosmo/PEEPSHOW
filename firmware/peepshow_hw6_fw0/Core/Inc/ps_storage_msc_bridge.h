#ifndef PS_STORAGE_MSC_BRIDGE_H
#define PS_STORAGE_MSC_BRIDGE_H

#include <stdint.h>

#include "tx_api.h"
#include "ux_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_STORAGE_MSC_BRIDGE_API_VERSION (3UL)
#define PS_STORAGE_MSC_BRIDGE_BLOCK_SIZE  (512UL)
#define PS_STORAGE_MSC_BRIDGE_BLOCK_COUNT (10240UL)
#define PS_STORAGE_MSC_BRIDGE_LAST_LBA    \
  (PS_STORAGE_MSC_BRIDGE_BLOCK_COUNT - 1UL)

typedef enum
{
  PS_STORAGE_MSC_COMMAND_READ = 1,
  PS_STORAGE_MSC_COMMAND_WRITE,
  PS_STORAGE_MSC_COMMAND_FLUSH,
  PS_STORAGE_MSC_COMMAND_STATUS
} ps_storage_msc_command_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t export_enabled;
  uint32_t media_present;
  uint32_t write_enabled;
  uint32_t dirty;
  uint32_t submit_count;
  uint32_t completed_count;
  uint32_t timeout_count;
  uint32_t busy_count;
  uint32_t denied_count;
  uint32_t denied_read_count;
  uint32_t denied_write_count;
  uint32_t denied_status_count;
  uint32_t activate_count;
  uint32_t deactivate_count;
  uint32_t read_count;
  uint32_t write_count;
  uint32_t flush_count;
  uint32_t status_count;
  uint32_t fast_status_count;
  uint32_t last_command;
  uint32_t last_lba;
  uint32_t last_block_count;
  uint32_t last_tx_status;
  uint32_t last_owner_status;
  uint32_t last_ux_status;
  uint32_t last_media_status;
  uint32_t last_ps_status;
} ps_storage_msc_bridge_probe_t;

typedef struct
{
  uint32_t command;
  uint8_t *data;
  uint32_t lba;
  uint32_t block_count;
  ULONG media_status;
  UINT ux_status;
  uint32_t ps_status;
} ps_storage_msc_request_t;

extern volatile ps_storage_msc_bridge_probe_t g_ps_storage_msc_bridge_probe;

UINT PS_StorageMscBridge_Init(TX_QUEUE *storage_queue);
void PS_StorageMscBridge_SetPolicy(uint32_t export_enabled,
                                   uint32_t media_present,
                                   uint32_t write_enabled);
void PS_StorageMscBridge_MarkActivated(void);
void PS_StorageMscBridge_MarkDeactivated(void);
UINT PS_StorageMscBridge_Submit(ps_storage_msc_command_t command,
                                uint8_t *data,
                                uint32_t lba,
                                uint32_t block_count,
                                ULONG *media_status);
UINT PS_StorageMscBridge_Status(ULONG *media_status);
ps_storage_msc_request_t *PS_StorageMscBridge_CurrentRequest(void);
void PS_StorageMscBridge_Complete(UINT ux_status,
                                  ULONG media_status,
                                  uint32_t ps_status);

#ifdef __cplusplus
}
#endif

#endif /* PS_STORAGE_MSC_BRIDGE_H */
