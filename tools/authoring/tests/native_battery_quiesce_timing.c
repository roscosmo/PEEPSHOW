#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef uint32_t UINT;
typedef uint32_t ULONG;
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
enum { TX_SUCCESS = 0, TX_QUEUE_ERROR = 9, TX_NO_EVENTS = 7,
       TX_AND_CLEAR = 1, TX_OR = 2, TX_NO_WAIT = 0 };
enum { PS_HW6_RTOS_OWNER_POWER, PS_HW6_RTOS_OWNER_AUDIO, PS_HW6_RTOS_OWNER_INPUT,
       PS_HW6_RTOS_OWNER_DISPLAY, PS_HW6_RTOS_OWNER_SENSOR, PS_HW6_RTOS_OWNER_STORAGE,
       PS_HW6_RTOS_OWNER_COMM, PS_HW6_OWNER_SM_PHYSICAL_OWNER_COUNT };
enum { PS_HW6_POWER_QUIESCE_REASON_BATTERY_CRITICAL = 2,
       PS_HW6_POWER_QUIESCE_REASON_BOOT_LOW_BATTERY = 3,
       PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP = 4,
       PS_HW6_RTOS_EVENT_DEBUG_INDEX = 0,
       PS_HW6_RTOS_STORAGE_CLOCK_REASON_POWER_QUIESCE = 8,
       PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE = 0,
       PS_HW6_RTOS_STORAGE_CLOCK_FLASH_CAPABILITIES = 1,
       PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER = 1,
       PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE = 0,
       PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES = 3,
       PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS = 1000 };
#define PS_HW6_RTOS_STATUS_NOT_RUN UINT32_MAX
#define PS_HW6_RTOS_ACK_OWNER(owner) (1UL << (owner))
#include "timing_probe.inc"
static volatile PS_HW6_BatteryQuiesceTimingProbe g_ps_hw6_battery_quiesce_timing_probe;
static struct { uint32_t requester_status[7]; } g_ps_hw6_clock_policy_probe;
static uint32_t ps_display_clock_wait_active, ps_display_power_barrier_active;
static uint32_t ps_display_power_barrier_drop_clock_request;
static uint32_t ps_event_groups[1];
static uint32_t now, inject_clock_wait, clock_delay, clock_result, clock_send_result;
static uint32_t owner_records, owner_bad, clock_calls;
static uint32_t grant_status, release_status, grant_calls, release_calls, clock_ack;
static uint32_t display_send_status, display_wait_status;
static uint32_t barrier_in_clock_wait;
static HAL_StatusTypeDef PS_HW6_RTOS_RunPowerQuiesceBarrier(uint32_t);
static UINT PS_HW6_RTOS_RequestPowerClockProfile(uint32_t, uint32_t, uint32_t);
static uint32_t tx_time_get(void) { return now; }
static ULONG PS_HW6_RTOS_ClockAckFlag(uint32_t owner)
{ return (owner < 7U) ? 1UL << (16U + owner) : 0U; }
static UINT tx_event_flags_set(uint32_t *group, ULONG flags, UINT option)
{
  (void)group;
  assert(option == TX_OR);
  assert(flags == PS_HW6_RTOS_ClockAckFlag(PS_HW6_RTOS_OWNER_DISPLAY));
  assert(g_ps_hw6_clock_policy_probe.requester_status[3] == TX_SUCCESS);
  clock_ack |= flags;
  return TX_SUCCESS;
}
static UINT PS_HW6_RTOS_ApplyDisplayClockCapabilitiesDirect(uint32_t reason, uint32_t caps)
{
  UINT status;
  if (caps != 0U)
  {
    assert(reason == PS_HW6_RTOS_DISPLAY_CLOCK_REASON_TRANSFER);
    assert(caps == PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES);
    grant_calls++;
    status = grant_status;
  }
  else
  {
    assert(reason == PS_HW6_RTOS_DISPLAY_CLOCK_REASON_RELEASE);
    release_calls++;
    status = release_status;
  }
  g_ps_hw6_clock_policy_probe.requester_status[3] = status;
  return status;
}
static UINT PS_HW6_RTOS_SendClockProfileCommand(uint32_t owner, uint32_t profile, uint32_t caps)
{
  (void)profile; (void)caps;
  assert(owner == PS_HW6_RTOS_OWNER_DISPLAY);
  clock_calls++;
  return clock_send_result;
}
static UINT tx_event_flags_get(uint32_t *group, ULONG flags, UINT option,
                               ULONG *actual, ULONG wait)
{
  (void)group; (void)option;
  *actual = 0U;
  if (wait == 0U) return TX_NO_EVENTS;
  assert(wait == 1000U);
  if (flags == PS_HW6_RTOS_ClockAckFlag(PS_HW6_RTOS_OWNER_DISPLAY))
  {
    if (barrier_in_clock_wait)
    {
      barrier_in_clock_wait = 0U;
      assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(PS_HW6_POWER_QUIESCE_REASON_BATTERY_CRITICAL) == HAL_OK);
      assert(clock_ack == flags);
      *actual = clock_ack;
      clock_ack = 0U;
      return TX_SUCCESS;
    }
    now += clock_delay;
    if (clock_result == TX_SUCCESS) *actual = flags;
    return clock_result;
  }
  if ((flags == PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_DISPLAY)) && inject_clock_wait)
  {
    assert(PS_HW6_RTOS_RequestPowerClockProfile(PS_HW6_RTOS_OWNER_DISPLAY, 0U, 0U) == TX_SUCCESS);
  }
  if ((flags == PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_DISPLAY)) && display_wait_status)
  {
    now += wait;
    return display_wait_status;
  }
  now += 2U;
  *actual = flags;
  return TX_SUCCESS;
}
static uint32_t PS_HW6_RTOS_PowerQuiesceNeedsStorageClock(void) { return 0U; }
static UINT PS_HW6_RTOS_ApplyStorageClockCapabilitiesFromPower(uint32_t reason, uint32_t caps)
{ (void)reason; (void)caps; return TX_SUCCESS; }
static void PS_HW6_OwnerStateMachines_BeginPowerQuiesce(uint32_t reason)
{ (void)reason; owner_records = owner_bad = 0U; }
static UINT PS_HW6_RTOS_SendPowerQuiesceCommand(uint32_t owner, uint32_t reason)
{ (void)reason; return owner == PS_HW6_RTOS_OWNER_DISPLAY ? display_send_status : TX_SUCCESS; }
static void PS_HW6_OwnerStateMachines_RecordPowerQuiesceCommand(uint32_t owner,
    UINT send_status, UINT wait_status, uint32_t flags)
{
  owner_records++;
  if (send_status || wait_status || flags != PS_HW6_RTOS_ACK_OWNER(owner)) owner_bad++;
}
static HAL_StatusTypeDef PS_HW6_OwnerStateMachines_EndPowerQuiesce(void)
{ return (owner_records == 6U && owner_bad == 0U) ? HAL_OK : HAL_ERROR; }
#include "timing_functions.inc"

int main(void)
{
  PS_HW6_BatteryQuiesceTimingProbe saved;
  now = 100U;
  inject_clock_wait = 1U;
  clock_delay = 1000U;
  clock_result = TX_NO_EVENTS;
  clock_send_result = TX_SUCCESS;
  /* Display's real request is outstanding when power begins preparation. */
  barrier_in_clock_wait = 1U;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(3U, 0U, 0U) == TX_SUCCESS);
  assert(owner_records == 6U && clock_calls == 1U);
  assert(ps_display_power_barrier_drop_clock_request == 1U);
  assert(ps_display_power_barrier_active == 0U && ps_display_clock_wait_active == 0U);
  assert(grant_calls == 1U && release_calls == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.sequence == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.active == 0U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_wait_at_begin == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_wait_at_send == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_failures == 0U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.done_tick[3] -
         g_ps_hw6_battery_quiesce_timing_probe.send_tick[3] == 2U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.end_tick -
         g_ps_hw6_battery_quiesce_timing_probe.start_tick == 12U);
  saved = g_ps_hw6_battery_quiesce_timing_probe;
  inject_clock_wait = 0U;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP) == HAL_OK);
  assert(memcmp((const void *)&g_ps_hw6_battery_quiesce_timing_probe, &saved, sizeof(saved)) == 0);
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(1U) == HAL_OK); /* START unchanged. */
  assert(grant_calls == 1U && release_calls == 1U);
  clock_send_result = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(3U, 0U, 0U) == TX_QUEUE_ERROR);
  assert(memcmp((const void *)&g_ps_hw6_battery_quiesce_timing_probe, &saved, sizeof(saved)) == 0);
  ps_display_power_barrier_active = 1U;
  g_ps_hw6_clock_policy_probe.requester_status[3] = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(3U, 0U, 0U) == TX_QUEUE_ERROR);
  ps_display_power_barrier_active = 0U;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(PS_HW6_POWER_QUIESCE_REASON_BOOT_LOW_BATTERY) == HAL_OK);
  assert(g_ps_hw6_battery_quiesce_timing_probe.sequence == 2U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_completions == 0U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_last_status == UINT32_MAX);

  /* Grant failure cannot dispatch display or manufacture its owner ACK. */
  grant_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(2U) == HAL_ERROR);
  assert(owner_bad == 1U && ps_display_power_barrier_active == 0U);
  grant_status = TX_SUCCESS;
  display_send_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(2U) == HAL_ERROR);
  assert(owner_bad == 1U && ps_display_power_barrier_active == 0U);
  display_send_status = TX_SUCCESS;
  display_wait_status = TX_NO_EVENTS;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_ERROR);
  assert(owner_bad == 1U && ps_display_power_barrier_active == 0U);
  display_wait_status = TX_SUCCESS;
  release_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_ERROR);
  assert(owner_bad == 0U && ps_display_power_barrier_active == 0U);
  release_status = TX_SUCCESS;
  assert(grant_calls == release_calls);
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_OK);

  /* The observer still retains a genuine clock timeout outside the handoff. */
  g_ps_hw6_battery_quiesce_timing_probe.active = 1U;
  clock_send_result = TX_SUCCESS;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(3U, 0U, 0U) == TX_NO_EVENTS);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_completions == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_failures == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_last_elapsed_ticks == 1000U);
  return 0;
}
