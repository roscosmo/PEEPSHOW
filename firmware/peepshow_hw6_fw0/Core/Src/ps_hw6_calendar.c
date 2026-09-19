#include "ps_hw6_calendar.h"
#include "ps_hw6_calendar_runtime.h"
#include "tx_api.h"

volatile ps_hw6_calendar_probe_t g_ps_calendar_probe = {.api_version = 1U};
static ps_calendar_timer_t ps_calendar_timer;
static uint32_t ps_calendar_tick;

static void PS_HW6_Calendar_Publish(void)
{
  g_ps_calendar_probe.configured = ps_calendar_timer.configured;
  g_ps_calendar_probe.armed = ps_calendar_timer.armed;
  g_ps_calendar_probe.generation = ps_calendar_timer.generation;
  g_ps_calendar_probe.deadline_seconds = ps_calendar_timer.deadline_seconds;
  g_ps_calendar_probe.tick_deadline = ps_calendar_tick;
}

static void PS_HW6_Calendar_Update(uint32_t operation, uint32_t value)
{
  ps_system_time_snapshot_t now = {0};
  ps_system_time_status_t status;
  ps_calendar_result_t result;
  uint32_t seconds, occurrence = 0U;
  uint32_t tick = (uint32_t)tx_time_get();
  uint64_t remaining_ms, ticks;
  if ((operation == 0U) && !PS_HW6_CalendarRuntime_Ready()) { return; }
  g_ps_calendar_probe.sample_count++;
  status = PS_HW6_Calendar_Read(&now);
  g_ps_calendar_probe.time_status = (uint32_t)status;
  if ((operation == PS_CALENDAR_ONCE) || (operation == PS_CALENDAR_DAILY))
  {
    result = PS_CalendarTimer_Configure(&ps_calendar_timer,
      (ps_calendar_kind_t)operation, value, status, &now);
  }
  else if (operation == 4U)
  { result = PS_CalendarTimer_Rebase(&ps_calendar_timer, status, &now); }
  else
  {
    result = PS_CalendarTimer_Fire(&ps_calendar_timer, ps_calendar_timer.generation,
      status, &now, &occurrence);
  }
  g_ps_calendar_probe.status = (uint32_t)result;
  if (result == PS_CALENDAR_DUE)
  {
    /* Bench sink: a consumed occurrence, not a package handler or panel draw. */
    g_ps_calendar_probe.occurrence = occurrence;
    g_ps_calendar_probe.delivered_registration = g_ps_calendar_probe.registration;
    g_ps_calendar_probe.delivered_generation = now.generation;
    g_ps_calendar_probe.delivered++;
    PS_HW6_CalendarRuntime_Due(occurrence, now.generation);
  }
  if ((ps_calendar_timer.armed != 0U) && (status == PS_SYSTEM_TIME_OK) &&
      (PS_SystemTime_Encode(&now.local, &seconds) == PS_SYSTEM_TIME_OK))
  {
    remaining_ms = (uint64_t)ps_calendar_timer.deadline_seconds * 1000ULL -
      ((uint64_t)seconds * 1000ULL + now.millisecond);
    /* Bound the signed tick horizon. Long dates are resampled on existing
     * battery wakes; this cap is an arithmetic window, not a polling period. */
    if (remaining_ms > 86400000ULL) { remaining_ms = 86400000ULL; }
    ticks = (remaining_ms * TX_TIMER_TICKS_PER_SECOND + 999ULL) / 1000ULL;
    if (ticks > INT32_MAX) { ticks = INT32_MAX; }
    ps_calendar_tick = tick + (uint32_t)ticks;
  }
  PS_HW6_Calendar_Publish();
}

uint32_t PS_HW6_Calendar_Remaining(uint32_t now_tick)
{
  int32_t remaining;
  if ((ps_calendar_timer.armed == 0U) || !PS_HW6_CalendarRuntime_Ready())
  { return UINT32_MAX; }
  remaining = (int32_t)(ps_calendar_tick - now_tick);
  return (remaining > 0) ? (uint32_t)remaining : 0U;
}

void PS_HW6_Calendar_Service(uint32_t now_tick, uint32_t allowed)
{
  uint32_t request = g_ps_calendar_probe.request;
  uint32_t value = g_ps_calendar_probe.value;
  g_ps_calendar_probe.request = 0U;
  if ((request != 0U) && PS_HW6_CalendarRuntime_Bound() && allowed)
  { g_ps_calendar_probe.status = PS_CALENDAR_ARGUMENT; return; }
  if ((allowed == 0U) || (request == 3U))
  {
    PS_CalendarTimer_Cancel(&ps_calendar_timer);
    g_ps_calendar_probe.status = PS_CALENDAR_IDLE;
    PS_HW6_Calendar_Publish();
    return;
  }
  if (request != 0U)
  {
    if ((request > 2U) || (g_ps_calendar_probe.registration == UINT32_MAX))
    { g_ps_calendar_probe.status = PS_CALENDAR_ARGUMENT; return; }
    /* No queued delivery survives replacement: the bench sink is synchronous. */
    if (((request == 2U) && (value >= 86400U)) ||
        ((request == 1U) && (value >= 3155760000UL)))
    { g_ps_calendar_probe.status = PS_CALENDAR_ARGUMENT; return; }
    g_ps_calendar_probe.registration++;
    PS_HW6_Calendar_Update(request, value);
  }
  else if (PS_HW6_Calendar_Remaining(now_tick) == 0U)
  { PS_HW6_Calendar_Update(0U, 0U); }
}

void PS_HW6_Calendar_TimeChanged(void)
{
  PS_HW6_CalendarRuntime_TimeChanged();
  if (ps_calendar_timer.configured != 0U) { PS_HW6_Calendar_Update(4U, 0U); }
}

ps_calendar_result_t PS_HW6_Calendar_Register(ps_calendar_kind_t kind, uint32_t value)
{
  if (g_ps_calendar_probe.registration == UINT32_MAX) { return PS_CALENDAR_ARGUMENT; }
  g_ps_calendar_probe.registration++;
  PS_HW6_Calendar_Update((uint32_t)kind, value);
  return (ps_calendar_result_t)g_ps_calendar_probe.status;
}

void PS_HW6_Calendar_Cancel(void)
{
  PS_CalendarTimer_Cancel(&ps_calendar_timer);
  PS_HW6_Calendar_Publish();
}

uint32_t PS_HW6_Calendar_Prepare(uint32_t now_tick)
{
  (void)now_tick;
  if (ps_calendar_timer.armed != 0U) { PS_HW6_Calendar_Update(0U, 0U); }
  /* Abort this sleep attempt, but do not use zero as the owner's receive wait:
   * lower-priority runtime must get CPU time to claim/complete the work. */
  if (PS_HW6_CalendarRuntime_NeedsService()) { return 0U; }
  return PS_HW6_Calendar_Remaining((uint32_t)tx_time_get());
}

void PS_HW6_Calendar_Finish(void)
{
  /* Kernel ticks may stop in STOP2. Re-anchor from the raw RTC after any wake. */
  if (ps_calendar_timer.armed != 0U) { PS_HW6_Calendar_Update(0U, 0U); }
}
