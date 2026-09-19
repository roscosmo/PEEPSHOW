#ifndef PS_HW6_TIME_RETENTION_H
#define PS_HW6_TIME_RETENTION_H

#include "ps_system_time.h"

/* Boot-only, before ThreadX. Preserve is called after HAL RTC initialization. */
uint32_t PS_HW6_TimeRetention_BootPreserve(void);
/* Completes cold initialization with a nonzero raw year; returns 1 on success. */
uint32_t PS_HW6_TimeRetention_BootFresh(void);
/* thPower only after startup. Invalid mappings clear the committed record. */
void PS_HW6_TimeRetention_Restore(ps_system_time_t *clock);
uint32_t PS_HW6_TimeRetention_Store(const ps_system_time_t *clock);

typedef struct
{
  uint32_t reset_flags;
  uint32_t source_preserved;
  uint32_t mapping_restored;
  uint32_t writes;
  uint32_t failures;
} ps_hw6_time_retention_probe_t;
extern volatile ps_hw6_time_retention_probe_t g_ps_time_retention_probe;
#endif
