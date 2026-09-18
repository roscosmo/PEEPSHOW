#ifndef PS_HW6_SYSTEM_TIME_H
#define PS_HW6_SYSTEM_TIME_H

#include "ps_system_time.h"

#define PS_HW6_SYSTEM_TIME_READ (1UL)
#define PS_HW6_SYSTEM_TIME_SET (2UL)
#define PS_HW6_SYSTEM_TIME_MAGIC (0x54494D45UL)

typedef struct
{
  uint32_t status; /* ps_system_time_status_t; snapshot valid only for OK. */
  uint32_t source_status; /* HAL status; no RTC write is performed. */
  uint64_t source_ms;
  ps_system_time_snapshot_t snapshot;
} ps_hw6_system_time_result_t;

typedef struct
{
  uint32_t api_version;
  uint32_t request;
  uint32_t complete;
  uint32_t pending;
  uint32_t operation;
  uint32_t send_status;
  uint32_t rejected_messages;
  ps_hw6_system_time_result_t result;
} ps_hw6_system_time_probe_t;

extern volatile ps_hw6_system_time_probe_t g_ps_system_time_probe;
/* Development mailbox only: thUI copies the request into qSysEvents. */
extern volatile uint32_t g_ps_system_time_request;
extern volatile ps_system_datetime_t g_ps_system_time_request_local;

/* thUI only, non-blocking ThreadX status. TX_SUCCESS means queued, not set.
 * One outstanding request; Take releases it only on matching completion.
 * No manual lease clearing on timeout: a late SET may still execute. */
uint32_t PS_HW6_SystemTime_Request(uint32_t operation,
  const ps_system_datetime_t *local, uint32_t *token);
uint32_t PS_HW6_SystemTime_Take(uint32_t token, ps_hw6_system_time_result_t *result);

#endif
