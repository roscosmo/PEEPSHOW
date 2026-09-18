#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ps_system_time.h"

int main(void)
{
  ps_system_time_t clock, before;
  ps_system_time_snapshot_t snapshot, untouched;
  ps_system_datetime_t date = {2000, 1, 1, 0, 0, 0}, decoded;
  uint32_t seconds, day, generation;
  memset(&snapshot, 0xA5, sizeof(snapshot));
  untouched = snapshot;
  PS_SystemTime_Init(&clock);
  assert(PS_SystemTime_Read(&clock, 100, &snapshot) == PS_SYSTEM_TIME_UNSET);
  assert(memcmp(&snapshot, &untouched, sizeof(snapshot)) == 0);
  assert(PS_SystemTime_Set(&clock, &date, 100) == PS_SYSTEM_TIME_OK);
  assert(PS_SystemTime_Read(&clock, 100, &snapshot) == PS_SYSTEM_TIME_OK);
  assert(snapshot.weekday == 6 && snapshot.generation == 1);

  /* Every representable date, including every month boundary and leap day. */
  for (day = 0; day < 36525; ++day)
  {
    seconds = day * 86400U + 86399U;
    assert(PS_SystemTime_Decode(seconds, &date) == PS_SYSTEM_TIME_OK);
    assert(PS_SystemTime_Encode(&date, &generation) == PS_SYSTEM_TIME_OK);
    assert(seconds == generation);
  }
  assert(date.year == 2099 && date.month == 12 && date.day == 31);
  assert(PS_SystemTime_Decode(3155760000U, &decoded) == PS_SYSTEM_TIME_RANGE);
  assert(PS_SystemTime_Decode(UINT32_MAX, &decoded) == PS_SYSTEM_TIME_RANGE);

  date = (ps_system_datetime_t){2024, 2, 28, 23, 59, 59};
  assert(PS_SystemTime_Set(&clock, &date, 1000) == PS_SYSTEM_TIME_OK);
  assert(PS_SystemTime_Read(&clock, 2500, &snapshot) == PS_SYSTEM_TIME_OK);
  assert(snapshot.local.day == 29 && snapshot.local.hour == 0 && snapshot.millisecond == 500);
  assert(PS_SystemTime_Read(&clock, 86402500, &snapshot) == PS_SYSTEM_TIME_OK);
  assert(snapshot.local.month == 3 && snapshot.local.day == 1);
  date = (ps_system_datetime_t){2025, 2, 29, 0, 0, 0};
  before = clock;
  assert(PS_SystemTime_Set(&clock, &date, 1) == PS_SYSTEM_TIME_ARGUMENT);
  assert(memcmp(&clock, &before, sizeof(clock)) == 0);
  date = (ps_system_datetime_t){2100, 1, 1, 0, 0, 0};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  date = (ps_system_datetime_t){1999, 12, 31, 0, 0, 0};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  date = (ps_system_datetime_t){2026, 4, 31, 0, 0, 0};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  date = (ps_system_datetime_t){2026, 0, 1, 0, 0, 0};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  date = (ps_system_datetime_t){2026, 13, 1, 0, 0, 0};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  date = (ps_system_datetime_t){2026, 1, 1, 24, 0, 0};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  date = (ps_system_datetime_t){2026, 1, 1, 0, 60, 0};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  date = (ps_system_datetime_t){2026, 1, 1, 0, 0, 60};
  assert(PS_SystemTime_Encode(&date, &seconds) == PS_SYSTEM_TIME_ARGUMENT);

  /* Local adjustments change generation, not the supplied elapsed clock. */
  date = (ps_system_datetime_t){2026, 12, 31, 23, 59, 59};
  generation = clock.generation;
  assert(PS_SystemTime_Set(&clock, &date, 90000000) == PS_SYSTEM_TIME_OK);
  assert(clock.generation == generation + 1);
  assert(PS_SystemTime_Read(&clock, 90001000, &snapshot) == PS_SYSTEM_TIME_OK);
  assert(snapshot.local.year == 2027 && snapshot.local.month == 1 && snapshot.local.day == 1);
  date.year = 2020;
  assert(PS_SystemTime_Set(&clock, &date, 90002000) == PS_SYSTEM_TIME_OK);
  assert(PS_SystemTime_Read(&clock, 90003000, &snapshot) == PS_SYSTEM_TIME_OK);
  assert(snapshot.local.year == 2021 && snapshot.local.second == 0);
  untouched = snapshot;
  generation = clock.generation;
  assert(PS_SystemTime_Read(&clock, 90002999, &snapshot) == PS_SYSTEM_TIME_SOURCE_LOST);
  assert(clock.generation == generation + 1);
  assert(memcmp(&snapshot, &untouched, sizeof(snapshot)) == 0);
  assert(PS_SystemTime_Read(&clock, 90004000, &snapshot) == PS_SYSTEM_TIME_SOURCE_LOST);
  PS_SystemTime_Invalidate(&clock);
  assert(clock.generation == generation + 1);

  date = (ps_system_datetime_t){2099, 12, 31, 23, 59, 59};
  assert(PS_SystemTime_Set(&clock, &date, 0) == PS_SYSTEM_TIME_OK);
  assert(PS_SystemTime_Read(&clock, 999, &snapshot) == PS_SYSTEM_TIME_OK);
  assert(PS_SystemTime_Read(&clock, 1000, &snapshot) == PS_SYSTEM_TIME_RANGE);
  assert(PS_SystemTime_Set(&clock, &date, 0) == PS_SYSTEM_TIME_OK);
  assert(PS_SystemTime_Read(&clock, UINT64_MAX, &snapshot) == PS_SYSTEM_TIME_RANGE);
  assert(PS_SystemTime_Set(&clock, &date, UINT64_MAX - 999) == PS_SYSTEM_TIME_OK);
  assert(PS_SystemTime_Read(&clock, UINT64_MAX, &snapshot) == PS_SYSTEM_TIME_OK);
  clock.generation = UINT32_MAX;
  before = clock;
  assert(PS_SystemTime_Set(&clock, &date, 0) == PS_SYSTEM_TIME_GENERATION_EXHAUSTED);
  assert(memcmp(&clock, &before, sizeof(clock)) == 0);
  PS_SystemTime_Invalidate(&clock);
  assert(clock.generation == UINT32_MAX && clock.status == PS_SYSTEM_TIME_SOURCE_LOST);
  assert(PS_SystemTime_Read(NULL, 0, &snapshot) == PS_SYSTEM_TIME_ARGUMENT);
  assert(PS_SystemTime_Read(&clock, 0, NULL) == PS_SYSTEM_TIME_ARGUMENT);
  assert(PS_SystemTime_Set(NULL, &date, 0) == PS_SYSTEM_TIME_ARGUMENT);
  assert(PS_SystemTime_Set(&clock, NULL, 0) == PS_SYSTEM_TIME_ARGUMENT);
  assert(PS_SystemTime_Encode(NULL, &seconds) == PS_SYSTEM_TIME_ARGUMENT);
  assert(PS_SystemTime_Decode(0, NULL) == PS_SYSTEM_TIME_ARGUMENT);
  puts("system time checks passed: calendar, adjustments, validity and source loss");
  return 0;
}
