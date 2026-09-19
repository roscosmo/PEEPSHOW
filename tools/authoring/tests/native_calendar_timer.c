#include <assert.h>
#include <string.h>
#include "ps_calendar_timer.h"

static ps_system_time_snapshot_t sample(uint16_t y, uint8_t m, uint8_t d,
  uint8_t h, uint8_t minute, uint8_t s, uint32_t generation)
{
  ps_system_time_snapshot_t result = {0};
  result.local = (ps_system_datetime_t){y, m, d, h, minute, s};
  result.generation = generation;
  return result;
}
static uint32_t seconds(ps_system_time_snapshot_t value)
{
  uint32_t result;
  assert(PS_SystemTime_Encode(&value.local, &result) == PS_SYSTEM_TIME_OK);
  return result;
}

static void resolve_once_tests(void)
{
  ps_system_time_snapshot_t now = sample(2028, 2, 28, 13, 0, 0, 1);
  ps_calendar_timer_t timer = {0};
  uint32_t deadline = UINT32_MAX, saved;
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 43200, 0,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_OK);
  assert(deadline == seconds(sample(2028, 2, 28, 12, 0, 0, 1)));
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_ONCE, deadline,
    PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_IDLE);
  assert(timer.consumed && !timer.armed);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 43200, 1,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_OK);
  assert(deadline == seconds(sample(2028, 2, 29, 12, 0, 0, 1)));
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_ONCE, deadline,
    PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  saved = deadline;
  now.local.day = 29; now.local.hour = 10; now.generation++;
  assert(PS_CalendarTimer_Rebase(&timer, PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  assert(timer.deadline_seconds == saved);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 43200, 1,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_OK);
  assert(deadline == seconds(sample(2028, 3, 1, 12, 0, 0, 1)));
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_NEXT_OCCURRENCE, 43200, 0,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_OK);
  assert(deadline == saved);
  now.local.hour = 12;
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_NEXT_OCCURRENCE, 43200, 0,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_OK);
  assert(deadline == saved + 86400U);
  now.millisecond = 999;
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_NEXT_OCCURRENCE, 43200, 0,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_OK);
  assert(deadline == saved + 86400U);
  now = sample(2026, 12, 31, 23, 0, 0, 1);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 0, 3,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_OK);
  assert(deadline == seconds(sample(2027, 1, 3, 0, 0, 0, 1)));
  saved = deadline;
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 0, UINT32_MAX,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_RANGE);
  assert(deadline == saved);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_NEXT_OCCURRENCE, 0, 1,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_ARGUMENT);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 86400, 0,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_ARGUMENT);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 0, 0,
    PS_SYSTEM_TIME_UNSET, NULL, &deadline) == PS_SYSTEM_TIME_UNSET);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_TODAY_OFFSET, 0, 0,
    PS_SYSTEM_TIME_SOURCE_LOST, NULL, &deadline) == PS_SYSTEM_TIME_SOURCE_LOST);
  now = sample(2099, 12, 31, 23, 59, 59, 1);
  assert(PS_CalendarTimer_ResolveOnce(PS_CALENDAR_NEXT_OCCURRENCE, 0, 0,
    PS_SYSTEM_TIME_OK, &now, &deadline) == PS_SYSTEM_TIME_RANGE);
  assert(deadline == saved);
}

int main(void)
{
  ps_calendar_timer_t timer = {0}, saved;
  ps_system_time_snapshot_t now = sample(2026, 9, 19, 23, 59, 50, 1);
  uint32_t due, original;
  resolve_once_tests();
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_DAILY, 0U,
    PS_SYSTEM_TIME_UNSET, NULL) == PS_CALENDAR_IDLE);
  assert(timer.configured == 1U && timer.armed == 0U);
  assert(PS_CalendarTimer_Rebase(&timer, PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  original = timer.deadline_seconds;
  assert(original == seconds(now) + 10U);
  due = UINT32_MAX;
  assert(PS_CalendarTimer_Fire(&timer, 1U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_ARMED);
  assert(due == UINT32_MAX);
  now = sample(2026, 9, 20, 0, 0, 0, 1);
  assert(PS_CalendarTimer_Fire(&timer, 1U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_DUE);
  assert(due == original && timer.deadline_seconds == original + 86400U);
  assert(PS_CalendarTimer_Fire(&timer, 1U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_ARMED);
  /* Backward edit does not repeat midnight already delivered. */
  now = sample(2026, 9, 19, 23, 59, 55, 2);
  due = UINT32_MAX;
  assert(PS_CalendarTimer_Fire(&timer, 1U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_REBASED);
  assert(due == UINT32_MAX && timer.deadline_seconds == original + 86400U);
  assert(PS_CalendarTimer_Fire(&timer, 1U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_STALE);
  /* Forward edit skips crossed occurrences, even if an old RTC wake arrives. */
  now = sample(2026, 9, 25, 12, 0, 0, 3);
  assert(PS_CalendarTimer_Fire(&timer, 2U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_REBASED);
  assert(timer.deadline_seconds == seconds(now) + 43200U);
  assert(due == UINT32_MAX);
  /* Ordinary late wake delivers the scheduled occurrence once, no backlog. */
  original = timer.deadline_seconds;
  now = sample(2026, 9, 28, 12, 0, 0, 3);
  assert(PS_CalendarTimer_Fire(&timer, 3U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_DUE);
  assert(due == original && timer.deadline_seconds == seconds(now) + 43200U);
  assert(PS_CalendarTimer_Fire(&timer, 3U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_ARMED);

  /* A selected one-shot deadline remains fixed and is delivered only once. */
  original = seconds(now) + 10U;
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_ONCE, original,
    PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  now.local.second = 11U;
  assert(PS_CalendarTimer_Fire(&timer, 3U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_DUE);
  assert(due == original && timer.consumed == 1U && timer.armed == 0U);
  now.local.second = 0U;
  now.generation++;
  assert(PS_CalendarTimer_Rebase(&timer, PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_IDLE);
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_ONCE, original,
    PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  now.local.second = 11U;
  now.generation++;
  due = UINT32_MAX;
  assert(PS_CalendarTimer_Fire(&timer, 4U, PS_SYSTEM_TIME_OK, &now, &due) == PS_CALENDAR_REBASED);
  assert(timer.consumed == 1U && due == UINT32_MAX);

  /* Invalid time disarms without inventing a date; recovery recalculates. */
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_DAILY, 0U,
    PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  assert(PS_CalendarTimer_Rebase(&timer, PS_SYSTEM_TIME_SOURCE_LOST, NULL) == PS_CALENDAR_IDLE);
  assert(timer.armed == 0U && timer.configured == 1U);
  now = sample(2028, 2, 28, 23, 59, 59, 6);
  assert(PS_CalendarTimer_Rebase(&timer, PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  assert(timer.deadline_seconds == seconds(sample(2028, 2, 29, 0, 0, 0, 6)));
  now = sample(2099, 12, 31, 23, 59, 59, 7);
  assert(PS_CalendarTimer_Rebase(&timer, PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_IDLE);
  assert(timer.armed == 0U);
  saved = timer;
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_DAILY, 86400U,
    PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARGUMENT);
  assert(memcmp(&timer, &saved, sizeof(timer)) == 0);
  now.millisecond = 1000U;
  assert(PS_CalendarTimer_Rebase(&timer, PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARGUMENT);
  assert(memcmp(&timer, &saved, sizeof(timer)) == 0);
  PS_CalendarTimer_Cancel(&timer);
  assert(timer.configured == 0U && timer.armed == 0U);
  now = sample(2026, 9, 19, 0, 0, 0, 8);
  assert(PS_CalendarTimer_Configure(&timer, PS_CALENDAR_DAILY, 0U,
    PS_SYSTEM_TIME_OK, &now) == PS_CALENDAR_ARMED);
  assert(timer.deadline_seconds == seconds(now) + 86400U);
  return 0;
}
