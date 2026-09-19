#include <assert.h>
#include "ps_hw6_calendar.h"
#define TX_TIMER_TICKS_PER_SECOND 100U
static uint32_t tick, reads;
static ps_system_time_snapshot_t snapshot = {
  .local = {2026, 9, 19, 23, 59, 45}, .generation = 1U};
static ps_system_time_status_t read_status = PS_SYSTEM_TIME_OK;
static uint32_t tx_time_get(void) { return tick; }
uint32_t PS_HW6_CalendarRuntime_Ready(void) { return 1U; }
uint32_t PS_HW6_CalendarRuntime_Bound(void) { return 0U; }
uint32_t PS_HW6_CalendarRuntime_NeedsService(void) { return 0U; }
void PS_HW6_CalendarRuntime_Due(uint32_t occurrence, uint32_t generation)
{ (void)occurrence; (void)generation; }
void PS_HW6_CalendarRuntime_TimeChanged(void) { }
ps_system_time_status_t PS_HW6_Calendar_Read(ps_system_time_snapshot_t *out)
{ reads++; *out = snapshot; return read_status; }
#include "calendar_owner.inc"
int main(void)
{
  uint32_t i, before, delivered;
  g_ps_calendar_probe.request = 2U;
  PS_HW6_Calendar_Service(tick, 1U);
  assert(g_ps_calendar_probe.armed == 1U && reads == 1U);
  assert(PS_HW6_Calendar_Remaining(tick) == 1500U);
  before = reads;
  for (i = 0U; i < 1400U; ++i)
  { tick = i; PS_HW6_Calendar_Service(tick, 1U); }
  assert(reads == before); /* Awake deadline checks do not read/poll RTC. */
  snapshot.local.second = 59U;
  snapshot.millisecond = 900U;
  assert(PS_HW6_Calendar_Prepare(tick) == 10U);
  /* RTC advances while kernel ticks remain stopped. */
  snapshot.local.day = 20U;
  snapshot.local.hour = 0U; snapshot.local.minute = 0U; snapshot.local.second = 0U;
  snapshot.millisecond = 0U;
  PS_HW6_Calendar_Finish();
  assert(g_ps_calendar_probe.delivered == 1U);
  assert(PS_HW6_Calendar_Remaining(tick) == 8640000U);
  PS_HW6_Calendar_Finish();
  assert(g_ps_calendar_probe.delivered == 1U);
  snapshot.local.day = 25U; snapshot.generation++;
  PS_HW6_Calendar_TimeChanged();
  assert(g_ps_calendar_probe.delivered == 1U);
  assert(g_ps_calendar_probe.generation == 2U);
  read_status = PS_SYSTEM_TIME_SOURCE_LOST;
  PS_HW6_Calendar_Finish();
  assert(g_ps_calendar_probe.armed == 0U);
  before = reads;
  PS_HW6_Calendar_Prepare(tick); PS_HW6_Calendar_Finish();
  assert(reads == before);
  read_status = PS_SYSTEM_TIME_OK; snapshot.generation++;
  PS_HW6_Calendar_TimeChanged();
  assert(g_ps_calendar_probe.armed == 1U);
  /* Cancel/replacement cannot retain a pending calendar occurrence. */
  g_ps_calendar_probe.request = 3U;
  PS_HW6_Calendar_Service(tick, 1U);
  assert(PS_HW6_Calendar_Remaining(tick) == UINT32_MAX);
  assert(g_ps_calendar_probe.configured == 0U);
  g_ps_calendar_probe.value = 86400U; g_ps_calendar_probe.request = 2U;
  PS_HW6_Calendar_Service(tick, 1U);
  assert(g_ps_calendar_probe.status == PS_CALENDAR_ARGUMENT);
  g_ps_calendar_probe.value = 1U; g_ps_calendar_probe.request = 2U;
  PS_HW6_Calendar_Service(tick, 1U);
  assert(PS_HW6_Calendar_Remaining(tick) == 100U);
  delivered = g_ps_calendar_probe.delivered;
  tick += 100U; snapshot.local.second = 1U;
  PS_HW6_Calendar_Service(tick, 1U);
  assert(g_ps_calendar_probe.delivered == delivered + 1U);
  PS_HW6_Calendar_Service(tick, 0U);
  assert(g_ps_calendar_probe.configured == 0U && g_ps_calendar_probe.armed == 0U);
  return 0;
}
