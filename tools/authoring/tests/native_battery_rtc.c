#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_battery_wake.h"

typedef uint32_t UINT;
typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
#define TX_NOT_AVAILABLE 1U
#define TX_TIMER_TICKS_PER_SECOND 100U
#define PS_SCENE_RUNTIME_EVENT_BINDING_MAX 4U
#define PS_SCENE_RUNTIME_INDEX_INVALID UINT32_MAX
#define PS_SCENE_RUNTIME_INTERACTION_TIMEOUT 1U
#define PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE 1U
#define PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT 38U
#define PS_HW6_RTOS_RTC_WAKE_SOURCE_NONE 0U
#define PS_HW6_RTOS_RTC_WAKE_SOURCE_INTERACTION 1U
#define PS_HW6_RTOS_RTC_WAKE_SOURCE_STATE_TIMER 2U
#define PS_HW6_RTOS_RTC_WAKE_SOURCE_BATTERY 3U
#define PS_HW6_RTOS_RTC_UNITS_PER_SECOND 256U
#define PS_HW6_RTOS_RTC_UNITS_PER_DAY (86400U * 256U)
#define PS_HW6_RTOS_RTC_DIV16_COUNTS_PER_SECOND 2048U
#define PS_HW6_RTOS_RTC_DIV16_MAX_MS 32000U
#define RTC_WAKEUPCLOCK_RTCCLK_DIV16 0U
#define RTC_WAKEUPCLOCK_CK_SPRE_16BITS 1U
#define RTC_IRQn 0U
#define RTC_FLAG_WUTF 1U
#define KNOB_POWER_RTC_WAKE_IRQ_PRIORITY 5U
#define KNOB_POWER_BATTERY_MONITOR_PERIOD_MS 1000U
#define KNOB_POWER_BATTERY_SLEEP_CHECK_MS 1800000U
#define PS_HW6_USB_HOST_EVENT_POWER_SNAPSHOT 1U
#define HAL_NVIC_SetPriority(a,b,c) ((void)0)
#define HAL_NVIC_ClearPendingIRQ(a) ((void)0)
#define HAL_NVIC_EnableIRQ(a) ((void)0)
#define HAL_NVIC_DisableIRQ(a) ((void)0)
#define __HAL_RTC_WAKEUPTIMER_GET_FLAG(a,b) rtc_flag
#define __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(a,b) (rtc_flag = 0U)

static uint32_t hrtc, rtc_flag, units, tick, commands, arms, disarms;
static uint32_t samples, ps_power_battery_monitor_period_ticks;
static struct { uint32_t active, force_read; } g_ps_hw6_battery_fault_wait_probe;
static struct { uint32_t request; } g_ps_hw6_battery_fault_test_probe;
static void PS_HW6_BatteryFaultTestRequest(HAL_StatusTypeDef status)
{ (void)status; g_ps_hw6_battery_fault_test_probe.request = 0; }
static struct {uint32_t battery_policy_last_tick, battery_policy_next_tick;}
  g_ps_hw6_owner_sm_probe;
static HAL_StatusTypeDef read_status, arm_status, disarm_status;
static uint32_t ps_runtime_interaction_rtc_armed, ps_runtime_interaction_rtc_irq_expired;
static uint32_t ps_runtime_interaction_rtc_command_queued, ps_runtime_rtc_wake_source;
static uint32_t ps_runtime_rtc_selected_remaining_ticks;
static uint32_t ps_runtime_rtc_state_timer_scene_revision, ps_runtime_state_timer_scene_revision;
static uint32_t ps_runtime_interaction_mode, ps_runtime_interaction_state;
static uint32_t ps_runtime_interaction_deadline_tick, ps_runtime_interaction_epoch;
static uint32_t ps_runtime_interaction_timeout_forced, ps_runtime_interaction_timeout_forced_epoch;
static uint32_t ps_runtime_interaction_rtc_remaining_ticks, ps_runtime_interaction_rtc_armed_epoch;
static uint32_t ps_runtime_interaction_rtc_start_units, ps_runtime_state_timer_paused;
typedef struct {uint32_t active, deadline_tick, stop2_remaining_ticks;} ps_runtime_state_timer_slot_t;
static ps_runtime_state_timer_slot_t ps_runtime_state_timers[PS_SCENE_RUNTIME_EVENT_BINDING_MAX];
#include "rtc_probe.inc"

static uint32_t tx_time_get(void) { return tick; }
static uint32_t PS_HW6_SM_MsToTicks(uint32_t ms) { return (ms + 9U) / 10U; }
static HAL_StatusTypeDef PS_HW6_PowerOwner_RunSnapshot(void)
{ samples++; return HAL_OK; }
static void PS_HW6_SM_UpdateUsbHostAvailability(uint32_t event)
{ assert(event == PS_HW6_USB_HOST_EVENT_POWER_SNAPSHOT); }
static HAL_StatusTypeDef PS_HW6_SM_EvaluateBatteryPolicy(HAL_StatusTypeDef status,
  uint32_t boot_check)
{
  assert(boot_check == 0);
  g_ps_hw6_owner_sm_probe.battery_policy_last_tick = tick;
  PS_BatteryWake_Record(&g_ps_hw6_battery_wake_probe, tick, status == HAL_OK, 0);
  return status;
}
static uint32_t PS_HW6_RTOS_TimeReached(uint32_t now, uint32_t deadline)
{ return (int32_t)(now - deadline) >= 0; }
static UINT PS_HW6_RTOS_RequestRuntimeCommand(uint32_t command)
{
  assert(command == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_TIMEOUT);
  commands++;
  return 0;
}
static uint32_t PS_HW6_RTOS_RuntimeStateTimerNext(uint32_t now,
  uint32_t *remaining, uint32_t *binding)
{
  uint32_t i, next, available = 0;
  *remaining = UINT32_MAX;
  if (ps_runtime_state_timer_paused) { return 0; }
  for (i = 0; i < PS_SCENE_RUNTIME_EVENT_BINDING_MAX; ++i)
  {
    if (!ps_runtime_state_timers[i].active) { continue; }
    next = PS_HW6_RTOS_TimeReached(now, ps_runtime_state_timers[i].deadline_tick) ?
      0 : ps_runtime_state_timers[i].deadline_tick - now;
    if (next < *remaining) { *remaining = next; *binding = i; available = 1; }
  }
  return available;
}
static HAL_StatusTypeDef PS_HW6_RTOS_InteractionRtcReadUnits(uint32_t *value)
{ *value = units; return read_status; }
static HAL_StatusTypeDef HAL_RTCEx_SetWakeUpTimer_IT(uint32_t *rtc,
  uint32_t counter, uint32_t clock, uint32_t clear)
{
  assert(rtc == &hrtc && counter < 65536U && clock <= 1 && clear == 0);
  arms++;
  return arm_status;
}
static HAL_StatusTypeDef HAL_RTCEx_DeactivateWakeUpTimer(uint32_t *rtc)
{ assert(rtc == &hrtc); disarms++; return disarm_status; }
#include "rtc_under_test.inc"

static void reset_fixture(void)
{
  memset(&g_ps_hw6_rtos_probe, 0, sizeof(g_ps_hw6_rtos_probe));
  memset(ps_runtime_state_timers, 0, sizeof(ps_runtime_state_timers));
  tick = 100;
  units = 256000;
  commands = arms = disarms = rtc_flag = 0;
  samples = ps_power_battery_monitor_period_ticks = 0;
  memset(&g_ps_hw6_battery_fault_wait_probe, 0, sizeof(g_ps_hw6_battery_fault_wait_probe));
  g_ps_hw6_battery_wake_test_request_ms = 0;
  g_ps_hw6_battery_fault_test_probe.request = 0;
  g_ps_hw6_owner_sm_probe.battery_policy_last_tick = tick;
  read_status = arm_status = disarm_status = HAL_OK;
  ps_runtime_interaction_rtc_armed = ps_runtime_interaction_rtc_irq_expired = 0;
  ps_runtime_interaction_rtc_command_queued = 0;
  ps_runtime_interaction_mode = ps_runtime_interaction_state = 0;
  ps_runtime_interaction_deadline_tick = 0;
  ps_runtime_interaction_timeout_forced = ps_runtime_interaction_timeout_forced_epoch = 0;
  ps_runtime_interaction_epoch = 1;
  ps_runtime_state_timer_paused = 0;
  ps_runtime_state_timer_scene_revision = 1;
  assert(PS_BatteryWake_Init(&g_ps_hw6_battery_wake_probe, 180000, 6000, 6000));
  PS_BatteryWake_Record(&g_ps_hw6_battery_wake_probe, tick, 1, 0);
}

int main(void)
{
  uint32_t i;
  reset_fixture();
  g_ps_hw6_battery_fault_wait_probe.active = 1;
  PS_BatteryWake_Record(&g_ps_hw6_battery_wake_probe, tick, 1, 1);
  ps_runtime_interaction_mode = ps_runtime_interaction_state = 1;
  ps_runtime_interaction_deadline_tick = tick + 100;
  ps_runtime_state_timers[0].active = 1;
  ps_runtime_state_timers[0].deadline_tick = tick + 200;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  assert(ps_runtime_rtc_wake_source == PS_HW6_RTOS_RTC_WAKE_SOURCE_BATTERY);
  assert(ps_runtime_rtc_selected_remaining_ticks == 6000);
  units += 60 * 256;
  rtc_flag = 1;
  RTC_IRQHandler();
  PS_HW6_RTOS_InteractionStop2TimeoutFinish();
  assert(commands == 0 && ps_runtime_interaction_timeout_forced == 0);
  assert(ps_runtime_interaction_deadline_tick == tick + 100);
  assert(ps_runtime_state_timers[0].deadline_tick == tick + 200);
  assert(g_ps_hw6_battery_wake_probe.pending);
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 1);
  tick += 100;
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 1); /* Fault wait does not read every awake second. */
  g_ps_hw6_battery_fault_wait_probe.force_read = 1;
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 2 && g_ps_hw6_battery_fault_wait_probe.force_read == 0);
  g_ps_hw6_battery_fault_test_probe.request = 1;
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 3 && g_ps_hw6_battery_fault_test_probe.request == 0);

  reset_fixture();
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  assert(ps_runtime_rtc_wake_source == 3 && arms == 1);
  assert(g_ps_hw6_rtos_probe.runtime_interaction_rtc_clock == RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
  assert(g_ps_hw6_rtos_probe.runtime_interaction_rtc_counter == 1799);
  units += 1800 * 256;
  rtc_flag = 1;
  RTC_IRQHandler();
  assert(commands == 0); /* A battery wake must not invent a runtime event. */
  PS_HW6_RTOS_InteractionStop2TimeoutFinish();
  assert(g_ps_hw6_battery_wake_probe.pending && disarms == 1);
  assert(g_ps_hw6_battery_wake_probe.due_wakes == 1 && commands == 0);
  assert(g_ps_hw6_battery_wake_probe.rtc_expiries == 1);
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 1 && !g_ps_hw6_battery_wake_probe.pending);
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 1); /* Same awake tick: normal cadence skip applies again. */

  reset_fixture();
  g_ps_hw6_battery_wake_test_request_ms = 15000;
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 0 && g_ps_hw6_battery_wake_test_request_ms == 0);
  assert(PS_BatteryWake_Remaining(&g_ps_hw6_battery_wake_probe, tick) == 1500);
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  units += 15 * 256;
  rtc_flag = 1; /* Finish also handles a latched flag before ISR service. */
  PS_HW6_RTOS_InteractionStop2TimeoutFinish();
  assert(g_ps_hw6_battery_wake_probe.rtc_expiries == 1 && commands == 0);
  assert(PS_HW6_OwnerStateMachines_RunBatteryMonitor(tick) == HAL_OK);
  assert(samples == 1 && g_ps_hw6_battery_wake_probe.test_arms == 1);
  assert(g_ps_hw6_battery_wake_probe.due_successes -
    g_ps_hw6_battery_wake_probe.test_due_successes_before == 1);

  reset_fixture();
  for (i = 0; i < 3; ++i)
  {
    assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
    units += 100 * 256; /* Unrelated early wakes, with frozen kernel time. */
    PS_HW6_RTOS_InteractionStop2TimeoutFinish();
    assert(PS_BatteryWake_Remaining(&g_ps_hw6_battery_wake_probe, tick) ==
      180000 - (i + 1) * 10000);
  }
  assert(commands == 0);

  reset_fixture();
  ps_runtime_state_timers[0].active = 1;
  ps_runtime_state_timers[0].deadline_tick = tick + 500;
  ps_runtime_interaction_mode = ps_runtime_interaction_state = 1;
  ps_runtime_interaction_deadline_tick = tick + 200;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  assert(ps_runtime_rtc_wake_source == 1);
  units += 2 * 256;
  rtc_flag = 1;
  RTC_IRQHandler();
  PS_HW6_RTOS_InteractionStop2TimeoutFinish();
  assert(commands == 1 && ps_runtime_interaction_timeout_forced);
  assert(ps_runtime_state_timers[0].deadline_tick == tick + 300);
  assert(PS_BatteryWake_Remaining(&g_ps_hw6_battery_wake_probe, tick) == 179800);

  reset_fixture();
  PS_BatteryWake_Shorten(&g_ps_hw6_battery_wake_probe, tick, 500);
  ps_runtime_state_timers[0].active = 1;
  ps_runtime_state_timers[0].deadline_tick = tick + 500;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  assert(ps_runtime_rtc_wake_source == 2); /* Ties preserve the scene timer. */
  units += 5 * 256;
  PS_HW6_RTOS_InteractionStop2TimeoutFinish();
  assert(commands == 1 && g_ps_hw6_battery_wake_probe.pending);

  reset_fixture();
  units = PS_HW6_RTOS_RTC_UNITS_PER_DAY - 256;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  units = 256;
  PS_HW6_RTOS_InteractionStop2TimeoutFinish();
  assert(PS_BatteryWake_Remaining(&g_ps_hw6_battery_wake_probe, tick) == 179800);

  reset_fixture();
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  PS_HW6_RTOS_InteractionStop2TimeoutFinish(); /* Abort before WFI, no elapsed time. */
  assert(disarms == 1 && !g_ps_hw6_battery_wake_probe.tracking);
  assert(PS_BatteryWake_Remaining(&g_ps_hw6_battery_wake_probe, tick) == 180000);

  reset_fixture();
  tick += 180000;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_BUSY);
  assert(commands == 0 && arms == 0 && g_ps_hw6_battery_wake_probe.pending);
  reset_fixture();
  ps_runtime_state_timers[0].active = 1;
  ps_runtime_state_timers[0].deadline_tick = tick;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_BUSY);
  assert(commands == 1 && arms == 0 && !g_ps_hw6_battery_wake_probe.tracking);

  reset_fixture();
  read_status = HAL_ERROR;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_ERROR);
  assert(arms == 0 && g_ps_hw6_battery_wake_probe.pending);
  reset_fixture();
  arm_status = HAL_ERROR;
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_ERROR);
  assert(!ps_runtime_interaction_rtc_armed && !g_ps_hw6_battery_wake_probe.tracking);
  assert(g_ps_hw6_battery_wake_probe.pending);
  reset_fixture();
  assert(PS_HW6_RTOS_InteractionStop2TimeoutPrepare() == HAL_OK);
  read_status = HAL_ERROR;
  PS_HW6_RTOS_InteractionStop2TimeoutFinish();
  assert(g_ps_hw6_battery_wake_probe.clock_failures == 1);
  assert(g_ps_hw6_battery_wake_probe.pending && commands == 0);
  return 0;
}
