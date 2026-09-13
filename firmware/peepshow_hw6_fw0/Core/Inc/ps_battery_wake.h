#ifndef PS_BATTERY_WAKE_H
#define PS_BATTERY_WAKE_H

#include <stdint.h>

/* thPower-owned deadline. RTC elapsed time rebases it after each sleep attempt. */
typedef struct
{
  uint32_t api_version;
  uint32_t configured;
  uint32_t has_deadline;
  uint32_t normal_ticks;
  uint32_t warning_ticks;
  uint32_t retry_ticks;
  uint32_t deadline_tick;
  uint32_t pending;
  uint32_t tracking;
  uint32_t sleep_remaining_ticks;
  uint32_t last_elapsed_ticks;
  uint32_t attempts;
  uint32_t successes;
  uint32_t failures;
  uint32_t last_sample_ok;
  uint32_t rtc_selections;
  uint32_t rtc_expiries;
  uint32_t due_wakes;
  uint32_t due_checks;
  uint32_t due_successes;
  uint32_t clock_failures;
  uint32_t test_arms;
  uint32_t test_rtc_expiries_before;
  uint32_t test_due_successes_before;
} ps_battery_wake_t;

uint32_t PS_BatteryWake_Init(volatile ps_battery_wake_t *state,
  uint32_t normal_ticks, uint32_t warning_ticks, uint32_t retry_ticks);
uint32_t PS_BatteryWake_Remaining(volatile ps_battery_wake_t *state, uint32_t now);
void PS_BatteryWake_Record(volatile ps_battery_wake_t *state,
  uint32_t now, uint32_t sample_ok, uint32_t warning);
uint32_t PS_BatteryWake_Prepare(volatile ps_battery_wake_t *state, uint32_t now);
void PS_BatteryWake_Finish(volatile ps_battery_wake_t *state,
  uint32_t now, uint32_t elapsed_ticks, uint32_t clock_ok);
void PS_BatteryWake_Shorten(volatile ps_battery_wake_t *state,
  uint32_t now, uint32_t ticks);

extern volatile ps_battery_wake_t g_ps_hw6_battery_wake_probe;
extern volatile uint32_t g_ps_hw6_battery_wake_test_request_ms;

#endif
