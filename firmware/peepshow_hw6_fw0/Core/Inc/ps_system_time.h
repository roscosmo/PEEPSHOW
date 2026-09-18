#ifndef PS_SYSTEM_TIME_H
#define PS_SYSTEM_TIME_H

#include <stdint.h>

/* Single-owner, peripheral-free core. Values are local civil time, not UTC.
 * This representation is limited to 2000..2099, matching the RTC year range. */
typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} ps_system_datetime_t;

typedef enum
{
  PS_SYSTEM_TIME_OK = 0,
  PS_SYSTEM_TIME_UNSET,
  PS_SYSTEM_TIME_ARGUMENT,
  PS_SYSTEM_TIME_SOURCE_LOST,
  PS_SYSTEM_TIME_RANGE,
  PS_SYSTEM_TIME_GENERATION_EXHAUSTED
} ps_system_time_status_t;

typedef struct
{
  uint64_t anchor_source_ms;
  uint64_t last_source_ms;
  uint32_t anchor_local_seconds;
  uint32_t generation;
  ps_system_time_status_t status;
} ps_system_time_t;

typedef struct
{
  ps_system_datetime_t local;
  uint32_t generation;
  uint16_t millisecond;
  uint8_t weekday; /* Monday=1, Sunday=7. */
} ps_system_time_snapshot_t;

void PS_SystemTime_Init(ps_system_time_t *clock);
ps_system_time_status_t PS_SystemTime_Encode(const ps_system_datetime_t *local,
  uint32_t *seconds);
ps_system_time_status_t PS_SystemTime_Decode(uint32_t seconds,
  ps_system_datetime_t *local);
/* source_ms must come from one continuous elapsed-time lifetime. Set changes
 * only this local mapping, never the source or any relative-timer deadline. */
ps_system_time_status_t PS_SystemTime_Set(ps_system_time_t *clock,
  const ps_system_datetime_t *local, uint64_t source_ms);
/* Read on explicit demand. On failure output is unchanged. Detected source
 * regression/range exhaustion invalidates the mapping until explicitly set. */
ps_system_time_status_t PS_SystemTime_Read(ps_system_time_t *clock,
  uint64_t source_ms, ps_system_time_snapshot_t *snapshot);
/* Call on source loss, reset or an unverified elapsed-time lifetime change. */
void PS_SystemTime_Invalidate(ps_system_time_t *clock);

#endif
