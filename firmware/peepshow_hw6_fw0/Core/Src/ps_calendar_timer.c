#include "ps_calendar_timer.h"
#include <stddef.h>
#include <string.h>

#define PS_CALENDAR_DAY_SECONDS (86400UL)
#define PS_CALENDAR_END_SECONDS (3155760000ULL)

static uint32_t PS_CalendarTimer_Snapshot(const ps_system_time_snapshot_t *now,
  uint32_t *seconds)
{
  return ((now != NULL) && (now->generation != 0U) &&
    (now->millisecond < 1000U) &&
    (PS_SystemTime_Encode(&now->local, seconds) == PS_SYSTEM_TIME_OK)) ? 1U : 0U;
}

ps_system_time_status_t PS_CalendarTimer_ResolveOnce(
  ps_calendar_once_rule_t rule, uint32_t time_of_day, uint32_t day_offset,
  ps_system_time_status_t time_status, const ps_system_time_snapshot_t *now,
  uint32_t *deadline)
{
  uint32_t seconds;
  uint64_t resolved;
  if ((deadline == NULL) || (time_of_day >= PS_CALENDAR_DAY_SECONDS) ||
      ((rule != PS_CALENDAR_TODAY_OFFSET) &&
       (rule != PS_CALENDAR_NEXT_OCCURRENCE)) ||
      ((rule == PS_CALENDAR_NEXT_OCCURRENCE) && (day_offset != 0U)))
  { return PS_SYSTEM_TIME_ARGUMENT; }
  if (time_status != PS_SYSTEM_TIME_OK) { return time_status; }
  if (PS_CalendarTimer_Snapshot(now, &seconds) == 0U)
  { return PS_SYSTEM_TIME_ARGUMENT; }
  resolved = ((uint64_t)(seconds / PS_CALENDAR_DAY_SECONDS) + day_offset) *
    PS_CALENDAR_DAY_SECONDS + time_of_day;
  if ((rule == PS_CALENDAR_NEXT_OCCURRENCE) && (resolved <= seconds))
  { resolved += PS_CALENDAR_DAY_SECONDS; }
  if (resolved >= PS_CALENDAR_END_SECONDS) { return PS_SYSTEM_TIME_RANGE; }
  *deadline = (uint32_t)resolved;
  return PS_SYSTEM_TIME_OK;
}

static ps_calendar_result_t PS_CalendarTimer_Next(ps_calendar_timer_t *timer,
  uint32_t seconds)
{
  uint64_t next;
  timer->armed = 0U;
  if (timer->consumed != 0U) { return PS_CALENDAR_IDLE; }
  if (timer->kind == PS_CALENDAR_ONCE)
  {
    if (timer->value <= seconds)
    { timer->consumed = 1U; return PS_CALENDAR_IDLE; }
    next = timer->value;
  }
  else
  {
    uint32_t floor = seconds;
    if ((timer->delivered != 0U) && (timer->last_delivered_seconds > floor))
    { floor = timer->last_delivered_seconds; }
    next = (uint64_t)(floor / PS_CALENDAR_DAY_SECONDS) * PS_CALENDAR_DAY_SECONDS + timer->value;
    if (next <= floor) { next += PS_CALENDAR_DAY_SECONDS; }
    if (next >= PS_CALENDAR_END_SECONDS) { return PS_CALENDAR_IDLE; }
  }
  timer->deadline_seconds = (uint32_t)next;
  timer->armed = 1U;
  return PS_CALENDAR_ARMED;
}

void PS_CalendarTimer_Cancel(ps_calendar_timer_t *timer)
{
  if (timer != NULL) { (void)memset(timer, 0, sizeof(*timer)); }
}

ps_calendar_result_t PS_CalendarTimer_Rebase(ps_calendar_timer_t *timer,
  ps_system_time_status_t time_status, const ps_system_time_snapshot_t *now)
{
  uint32_t seconds;
  if (timer == NULL) { return PS_CALENDAR_ARGUMENT; }
  if (time_status != PS_SYSTEM_TIME_OK)
  { timer->armed = 0U; timer->generation = 0U; return PS_CALENDAR_IDLE; }
  if (PS_CalendarTimer_Snapshot(now, &seconds) == 0U) { return PS_CALENDAR_ARGUMENT; }
  timer->generation = now->generation;
  if (timer->configured == 0U) { return PS_CALENDAR_IDLE; }
  return PS_CalendarTimer_Next(timer, seconds);
}

ps_calendar_result_t PS_CalendarTimer_Configure(ps_calendar_timer_t *timer,
  ps_calendar_kind_t kind, uint32_t value, ps_system_time_status_t time_status,
  const ps_system_time_snapshot_t *now)
{
  ps_calendar_timer_t candidate = {0};
  ps_calendar_result_t result;
  if ((timer == NULL) || ((kind != PS_CALENDAR_ONCE) && (kind != PS_CALENDAR_DAILY)) ||
      ((kind == PS_CALENDAR_ONCE) && ((uint64_t)value >= PS_CALENDAR_END_SECONDS)) ||
      ((kind == PS_CALENDAR_DAILY) && (value >= PS_CALENDAR_DAY_SECONDS)))
  { return PS_CALENDAR_ARGUMENT; }
  candidate.configured = 1U;
  candidate.kind = (uint32_t)kind;
  candidate.value = value;
  result = PS_CalendarTimer_Rebase(&candidate, time_status, now);
  if (result != PS_CALENDAR_ARGUMENT) { *timer = candidate; }
  return result;
}

ps_calendar_result_t PS_CalendarTimer_Fire(ps_calendar_timer_t *timer,
  uint32_t wake_generation, ps_system_time_status_t time_status,
  const ps_system_time_snapshot_t *now, uint32_t *occurrence)
{
  uint32_t seconds, due;
  if ((timer == NULL) || (occurrence == NULL)) { return PS_CALENDAR_ARGUMENT; }
  if (time_status != PS_SYSTEM_TIME_OK)
  { return PS_CalendarTimer_Rebase(timer, time_status, now); }
  if (PS_CalendarTimer_Snapshot(now, &seconds) == 0U) { return PS_CALENDAR_ARGUMENT; }
  if (timer->generation != now->generation)
  {
    (void)PS_CalendarTimer_Rebase(timer, time_status, now);
    return PS_CALENDAR_REBASED;
  }
  if (wake_generation != timer->generation) { return PS_CALENDAR_STALE; }
  if (timer->armed == 0U) { return PS_CALENDAR_IDLE; }
  if (seconds < timer->deadline_seconds) { return PS_CALENDAR_ARMED; }
  due = timer->deadline_seconds;
  timer->armed = 0U;
  timer->delivered = 1U;
  timer->last_delivered_seconds = due;
  if (timer->kind == PS_CALENDAR_ONCE) { timer->consumed = 1U; }
  else { (void)PS_CalendarTimer_Next(timer, seconds); }
  *occurrence = due;
  return PS_CALENDAR_DUE;
}
