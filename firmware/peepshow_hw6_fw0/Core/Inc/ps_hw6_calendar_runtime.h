#ifndef PS_HW6_CALENDAR_RUNTIME_H
#define PS_HW6_CALENDAR_RUNTIME_H
#include "ps_calendar_transport.h"

typedef struct
{
  uint32_t api_version, request, binding, time_of_day, day_offset;
  uint32_t registration, scene, setup_status, active;
  uint32_t notifications, claims, grants, applied, ignored, failed, stale;
  uint32_t power_send_failures, runtime_send_failures, pending_ack;
  uint32_t delivery_state, delivery_sequence, occurrence, clock_generation;
} ps_hw6_calendar_runtime_probe_t;
extern volatile ps_hw6_calendar_runtime_probe_t g_ps_calendar_runtime_probe;

/* Power and runtime entry points are each called only by that owner. */
void PS_HW6_CalendarRuntime_PowerMessage(const ps_calendar_message_t *message);
void PS_HW6_CalendarRuntime_RuntimeMessage(const ps_calendar_message_t *message);
void PS_HW6_CalendarRuntime_PowerService(uint32_t allowed);
void PS_HW6_CalendarRuntime_RuntimeService(void);
/* Power-only scheduler sink. */
uint32_t PS_HW6_CalendarRuntime_Ready(void);
uint32_t PS_HW6_CalendarRuntime_NeedsService(void);
uint32_t PS_HW6_CalendarRuntime_Bound(void);
void PS_HW6_CalendarRuntime_Due(uint32_t occurrence, uint32_t generation);
void PS_HW6_CalendarRuntime_TimeChanged(void);
/* Platform adapters: no pointers are transferred through queues. Send returns
 * zero only on successful non-blocking enqueue. Scene zero means unavailable. */
uint32_t PS_HW6_CalendarRuntime_Send(uint32_t to_power,
  const ps_calendar_message_t *message);
uint32_t PS_HW6_CalendarRuntime_Scene(void);
uint32_t PS_HW6_CalendarRuntime_Running(void);
uint32_t PS_HW6_CalendarRuntime_BindingValid(uint32_t binding);
ps_calendar_delivery_state_t PS_HW6_CalendarRuntime_Apply(uint32_t binding);
#endif
