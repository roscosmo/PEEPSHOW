#include "ps_battery_wake.h"
#include <stddef.h>

volatile ps_battery_wake_t g_ps_hw6_battery_wake_probe;
volatile uint32_t g_ps_hw6_battery_wake_test_request_ms;

uint32_t PS_BatteryWake_Init(volatile ps_battery_wake_t *state,
  uint32_t normal_ticks, uint32_t warning_ticks, uint32_t retry_ticks)
{
  if (state == NULL) { return 0UL; }
  *state = (ps_battery_wake_t){.api_version = 1UL};
  if ((normal_ticks == 0UL) || (normal_ticks > 0x7fffffffUL) ||
      (warning_ticks == 0UL) || (warning_ticks > normal_ticks) ||
      (retry_ticks == 0UL) || (retry_ticks > normal_ticks))
  { return 0UL; }
  state->configured = 1UL;
  state->normal_ticks = normal_ticks;
  state->warning_ticks = warning_ticks;
  state->retry_ticks = retry_ticks;
  return 1UL;
}

uint32_t PS_BatteryWake_Remaining(volatile ps_battery_wake_t *state, uint32_t now)
{
  uint32_t remaining;
  if (state == NULL) { return 0UL; }
  remaining = state->deadline_tick - now;
  if ((state->configured == 0UL) || (state->has_deadline == 0UL) ||
      (state->pending != 0UL) || (remaining == 0UL) ||
      (remaining > 0x7fffffffUL))
  {
    state->pending = 1UL;
    return 0UL;
  }
  return remaining;
}

void PS_BatteryWake_Record(volatile ps_battery_wake_t *state,
  uint32_t now, uint32_t sample_ok, uint32_t warning)
{
  uint32_t remaining;
  uint32_t next;
  if ((state == NULL) || (state->configured == 0UL)) { return; }
  remaining = PS_BatteryWake_Remaining(state, now);
  if (state->pending != 0UL)
  {
    state->due_checks++;
    if (sample_ok != 0UL) { state->due_successes++; }
  }
  state->attempts++;
  state->last_sample_ok = sample_ok;
  if (sample_ok != 0UL)
  {
    state->successes++;
    next = (warning != 0UL) ? state->warning_ticks : state->normal_ticks;
  }
  else
  {
    state->failures++;
    next = state->retry_ticks;
    /* Repeated failures before the retry deadline must not push it out. */
    if ((remaining != 0UL) && (remaining < next)) { next = remaining; }
  }
  state->deadline_tick = now + next;
  state->has_deadline = 1UL;
  state->pending = 0UL;
}

uint32_t PS_BatteryWake_Prepare(volatile ps_battery_wake_t *state, uint32_t now)
{
  uint32_t remaining = PS_BatteryWake_Remaining(state, now);
  if (state != NULL)
  {
    state->sleep_remaining_ticks = remaining;
    state->tracking = (remaining != 0UL);
  }
  return remaining;
}

void PS_BatteryWake_Finish(volatile ps_battery_wake_t *state,
  uint32_t now, uint32_t elapsed_ticks, uint32_t clock_ok)
{
  uint32_t remaining;
  if ((state == NULL) || (state->tracking == 0UL)) { return; }
  state->tracking = 0UL;
  state->last_elapsed_ticks = elapsed_ticks;
  remaining = state->sleep_remaining_ticks;
  if ((clock_ok == 0UL) || (elapsed_ticks >= remaining))
  {
    state->deadline_tick = now;
    state->pending = 1UL;
    state->due_wakes++;
    if (clock_ok == 0UL) { state->clock_failures++; }
  }
  else
  {
    state->deadline_tick = now + remaining - elapsed_ticks;
  }
}

void PS_BatteryWake_Shorten(volatile ps_battery_wake_t *state,
  uint32_t now, uint32_t ticks)
{
  uint32_t remaining = PS_BatteryWake_Remaining(state, now);
  if ((state != NULL) && (ticks != 0UL) && (ticks < remaining))
  {
    state->deadline_tick = now + ticks;
    state->test_arms++;
    state->test_rtc_expiries_before = state->rtc_expiries;
    state->test_due_successes_before = state->due_successes;
  }
}
