#include "ps_system_time.h"

#include <stddef.h>
#include <string.h>

/* Exclusive endpoint: 2100-01-01. These are format limits, not tuning knobs. */
#define PS_SYSTEM_TIME_END_SECONDS (3155760000UL)

static uint32_t PS_SystemTime_MonthDays(uint32_t year, uint32_t month)
{
  static const uint8_t days[12] = {31U,28U,31U,30U,31U,30U,31U,31U,30U,31U,30U,31U};
  return days[month - 1UL] + (((month == 2UL) && ((year % 4UL) == 0UL)) ? 1UL : 0UL);
}

void PS_SystemTime_Init(ps_system_time_t *clock)
{
  if (clock != NULL)
  {
    (void)memset(clock, 0, sizeof(*clock));
    clock->status = PS_SYSTEM_TIME_UNSET;
  }
}

ps_system_time_status_t PS_SystemTime_Encode(const ps_system_datetime_t *local,
  uint32_t *seconds)
{
  uint32_t year, month, days;
  if ((local == NULL) || (seconds == NULL) ||
      (local->year < 2000U) || (local->year > 2099U) ||
      (local->month < 1U) || (local->month > 12U) ||
      (local->day < 1U) || (local->hour > 23U) ||
      (local->minute > 59U) || (local->second > 59U))
  { return PS_SYSTEM_TIME_ARGUMENT; }
  if (local->day > PS_SystemTime_MonthDays(local->year, local->month))
  { return PS_SYSTEM_TIME_ARGUMENT; }
  year = (uint32_t)local->year - 2000UL;
  days = year * 365UL + (year + 3UL) / 4UL;
  for (month = 1UL; month < local->month; ++month)
  { days += PS_SystemTime_MonthDays(local->year, month); }
  days += (uint32_t)local->day - 1UL;
  *seconds = days * 86400UL + (uint32_t)local->hour * 3600UL +
    (uint32_t)local->minute * 60UL + local->second;
  return PS_SYSTEM_TIME_OK;
}

ps_system_time_status_t PS_SystemTime_Decode(uint32_t seconds,
  ps_system_datetime_t *local)
{
  ps_system_datetime_t result = {0};
  uint32_t days, year, month, span;
  if (local == NULL) { return PS_SYSTEM_TIME_ARGUMENT; }
  if (seconds >= PS_SYSTEM_TIME_END_SECONDS) { return PS_SYSTEM_TIME_RANGE; }
  days = seconds / 86400UL;
  for (year = 2000UL; year < 2100UL; ++year)
  {
    span = ((year % 4UL) == 0UL) ? 366UL : 365UL;
    if (days < span) { break; }
    days -= span;
  }
  for (month = 1UL; month < 12UL; ++month)
  {
    span = PS_SystemTime_MonthDays(year, month);
    if (days < span) { break; }
    days -= span;
  }
  result.year = (uint16_t)year;
  result.month = (uint8_t)month;
  result.day = (uint8_t)(days + 1UL);
  result.hour = (uint8_t)((seconds % 86400UL) / 3600UL);
  result.minute = (uint8_t)((seconds % 3600UL) / 60UL);
  result.second = (uint8_t)(seconds % 60UL);
  *local = result;
  return PS_SYSTEM_TIME_OK;
}

void PS_SystemTime_Invalidate(ps_system_time_t *clock)
{
  if (clock == NULL) { return; }
  if (clock->status == PS_SYSTEM_TIME_OK)
  {
    if (clock->generation != UINT32_MAX) { ++clock->generation; }
  }
  clock->status = PS_SYSTEM_TIME_SOURCE_LOST;
}

ps_system_time_status_t PS_SystemTime_Set(ps_system_time_t *clock,
  const ps_system_datetime_t *local, uint64_t source_ms)
{
  uint32_t seconds;
  if ((clock == NULL) || (PS_SystemTime_Encode(local, &seconds) != PS_SYSTEM_TIME_OK))
  { return PS_SYSTEM_TIME_ARGUMENT; }
  if (clock->generation == UINT32_MAX) { return PS_SYSTEM_TIME_GENERATION_EXHAUSTED; }
  clock->anchor_source_ms = source_ms;
  clock->last_source_ms = source_ms;
  clock->anchor_local_seconds = seconds;
  ++clock->generation;
  clock->status = PS_SYSTEM_TIME_OK;
  return PS_SYSTEM_TIME_OK;
}

ps_system_time_status_t PS_SystemTime_Read(ps_system_time_t *clock,
  uint64_t source_ms, ps_system_time_snapshot_t *snapshot)
{
  ps_system_time_snapshot_t result = {0};
  uint64_t local_ms, elapsed_ms;
  if ((clock == NULL) || (snapshot == NULL)) { return PS_SYSTEM_TIME_ARGUMENT; }
  if (clock->status != PS_SYSTEM_TIME_OK) { return clock->status; }
  if (source_ms < clock->last_source_ms)
  {
    PS_SystemTime_Invalidate(clock);
    return PS_SYSTEM_TIME_SOURCE_LOST;
  }
  elapsed_ms = source_ms - clock->anchor_source_ms;
  local_ms = (uint64_t)clock->anchor_local_seconds * 1000ULL;
  if (elapsed_ms >= (uint64_t)PS_SYSTEM_TIME_END_SECONDS * 1000ULL - local_ms)
  {
    PS_SystemTime_Invalidate(clock);
    clock->status = PS_SYSTEM_TIME_RANGE;
    return PS_SYSTEM_TIME_RANGE;
  }
  local_ms += elapsed_ms;
  (void)PS_SystemTime_Decode((uint32_t)(local_ms / 1000ULL), &result.local);
  result.millisecond = (uint16_t)(local_ms % 1000ULL);
  result.weekday = (uint8_t)(((local_ms / 86400000ULL) + 5ULL) % 7ULL + 1ULL);
  result.generation = clock->generation;
  clock->last_source_ms = source_ms;
  *snapshot = result;
  return PS_SYSTEM_TIME_OK;
}
