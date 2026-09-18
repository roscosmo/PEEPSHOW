#ifndef PS_HW6_CALENDAR_H
#define PS_HW6_CALENDAR_H
#include "ps_calendar_timer.h"

typedef struct
{
  uint32_t api_version, request, value, registration, status;
  uint32_t configured, armed, generation, deadline_seconds;
  uint32_t tick_deadline, sample_count, time_status;
  uint32_t delivered, occurrence, delivered_registration, delivered_generation;
  uint32_t rtc_selections, rtc_expiries;
} ps_hw6_calendar_probe_t;
extern volatile ps_hw6_calendar_probe_t g_ps_calendar_probe;
/* thPower only. Mailbox request: ONCE=1 DAILY=2 CANCEL=3. No package API yet. */
void PS_HW6_Calendar_Service(uint32_t now_tick, uint32_t allowed);
void PS_HW6_Calendar_TimeChanged(void);
uint32_t PS_HW6_Calendar_Remaining(uint32_t now_tick);
uint32_t PS_HW6_Calendar_Prepare(uint32_t now_tick);
void PS_HW6_Calendar_Finish(void);
/* Adapter supplied by the power-owned system-time service. */
ps_system_time_status_t PS_HW6_Calendar_Read(ps_system_time_snapshot_t *snapshot);
#endif
