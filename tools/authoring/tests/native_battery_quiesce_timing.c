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
static volatile PS_HW6_BatteryQuiesceTimingProbe g_ps_hw6_battery_quiesce_first_failure_probe;
static struct {
  uint32_t power_quiesce_owner_status[7];
  uint32_t current_state[1];
  uint32_t joystick_calibration_persistent_boot_resolved;
  uint32_t joystick_driver_state, joystick_driver_last_status;
  uint32_t joystick_ready_status, joystick_identity_status;
  uint32_t joystick_sleep_write_status, joystick_terminal_sleep_committed;
  uint32_t joystick_i2c_error_after;
} g_ps_hw6_owner_sm_probe;
enum { PS_HW6_SM_JOYSTICK = 0 };
static uint32_t ps_power_boot_done, ps_joystick_calibration_boot_load_started;
static struct { uint32_t requester_status[7]; } g_ps_hw6_clock_policy_probe;
static uint32_t ps_display_clock_wait_active, ps_display_power_barrier_active;
static uint32_t ps_storage_clock_wait_active, ps_storage_clock_wait_start_tick;
static uint32_t ps_storage_clock_wait_capabilities;
static UINT ps_storage_clock_handoff_result;
static uint32_t ps_storage_power_barrier_active, ps_storage_power_barrier_drop_clock_request;
static UINT ps_storage_power_barrier_clock_status;
static uint32_t ps_display_power_barrier_drop_clock_request;
static uint32_t ps_event_groups[1];
static uint32_t now, inject_clock_wait, clock_delay, clock_result, clock_send_result;
static uint32_t owner_records, owner_bad, clock_calls;
static uint32_t grant_status, release_status, grant_calls, release_calls, clock_ack;
static uint32_t display_send_status, display_wait_status;
static uint32_t barrier_in_clock_wait;
static uint32_t storage_grant_status, storage_release_status, storage_grants, storage_releases;
static uint32_t storage_send_status, storage_wait_status, storage_work;
static uint32_t storage_request_caps, storage_grant_held, storage_pending;
static uint32_t storage_expected_barrier_status, storage_expected_clock_status;
static HAL_StatusTypeDef PS_HW6_RTOS_RunPowerQuiesceBarrier(uint32_t);
static UINT PS_HW6_RTOS_RequestPowerClockProfile(uint32_t, uint32_t, uint32_t);
static uint32_t tx_time_get(void) { return now; }
static ULONG PS_HW6_RTOS_ClockAckFlag(uint32_t owner)
{ return (owner < 7U) ? 1UL << (16U + owner) : 0U; }
static UINT tx_event_flags_set(uint32_t *group, ULONG flags, UINT option)
{
  (void)group;
  assert(option == TX_OR);
  assert(flags == PS_HW6_RTOS_ClockAckFlag(PS_HW6_RTOS_OWNER_DISPLAY) ||
         flags == PS_HW6_RTOS_ClockAckFlag(PS_HW6_RTOS_OWNER_STORAGE));
  if (flags == PS_HW6_RTOS_ClockAckFlag(PS_HW6_RTOS_OWNER_DISPLAY))
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
  assert(owner == PS_HW6_RTOS_OWNER_DISPLAY || owner == PS_HW6_RTOS_OWNER_STORAGE);
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
  if (flags == PS_HW6_RTOS_ClockAckFlag(PS_HW6_RTOS_OWNER_STORAGE))
  {
    assert(ps_storage_clock_wait_active == 1U);
    assert(ps_storage_clock_wait_start_tick == now);
    assert(ps_storage_clock_wait_capabilities == storage_request_caps);
    storage_pending = 1U;
    assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == storage_expected_barrier_status);
    assert(g_ps_hw6_battery_quiesce_timing_probe.storage_clock_wait_at_begin == 1U);
    assert((clock_ack & flags) != 0U);
    assert(ps_storage_clock_handoff_result == storage_expected_clock_status);
    clock_ack &= ~flags;
    storage_pending = 0U;
    *actual = flags;
    return TX_SUCCESS;
  }
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
  if ((flags == PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_STORAGE)) && storage_pending)
  {
    /* Pending calibration resumes only after the clock reply, then parks. */
    assert(clock_ack & PS_HW6_RTOS_ClockAckFlag(PS_HW6_RTOS_OWNER_STORAGE));
    assert(ps_storage_power_barrier_active && storage_grant_held);
    assert(PS_HW6_RTOS_RequestPowerClockProfile(5U, 0U, 1U) == TX_SUCCESS);
    if (ps_storage_clock_handoff_result == TX_SUCCESS) storage_work++;
    assert(PS_HW6_RTOS_RequestPowerClockProfile(5U, 0U, 0U) == TX_SUCCESS);
    assert(storage_grant_held); /* A queued release must not park clocks early. */
    assert(PS_HW6_RTOS_RequestPowerClockProfile(5U, 0U, 2U) == TX_QUEUE_ERROR);
  }
  if ((flags == PS_HW6_RTOS_ACK_OWNER(PS_HW6_RTOS_OWNER_STORAGE)) && storage_wait_status)
  {
    now += wait;
    return storage_wait_status;
  }
  now += 2U;
  for (uint32_t owner = 1U; owner < 7U; ++owner)
  {
    if (flags == PS_HW6_RTOS_ACK_OWNER(owner))
      g_ps_hw6_owner_sm_probe.power_quiesce_owner_status[owner] = HAL_OK;
  }
  *actual = flags;
  return TX_SUCCESS;
}
static uint32_t PS_HW6_RTOS_PowerQuiesceNeedsStorageClock(void) { return 0U; }
static UINT PS_HW6_RTOS_ApplyStorageClockCapabilitiesFromPower(uint32_t reason, uint32_t caps)
{
  UINT status;
  if (caps)
  {
    assert(reason == PS_HW6_RTOS_STORAGE_CLOCK_REASON_POWER_QUIESCE && caps == 1U);
    storage_grants++;
    status = storage_grant_status;
    storage_grant_held = status == TX_SUCCESS;
  }
  else
  {
    assert(reason == PS_HW6_RTOS_STORAGE_CLOCK_REASON_RELEASE);
    storage_releases++;
    status = storage_release_status;
    if (status == TX_SUCCESS) storage_grant_held = 0U;
  }
  g_ps_hw6_clock_policy_probe.requester_status[5] = status;
  return status;
}
static void PS_HW6_OwnerStateMachines_BeginPowerQuiesce(uint32_t reason)
{
  (void)reason;
  owner_records = owner_bad = 0U;
  memset(g_ps_hw6_owner_sm_probe.power_quiesce_owner_status, 0xff,
         sizeof(g_ps_hw6_owner_sm_probe.power_quiesce_owner_status));
}
static UINT PS_HW6_RTOS_SendPowerQuiesceCommand(uint32_t owner, uint32_t reason)
{
  (void)reason;
  if (owner == PS_HW6_RTOS_OWNER_STORAGE) return storage_send_status;
  return owner == PS_HW6_RTOS_OWNER_DISPLAY ? display_send_status : TX_SUCCESS;
}
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
  PS_HW6_BatteryQuiesceTimingProbe first_failure;
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
  assert(g_ps_hw6_battery_quiesce_first_failure_probe.sequence == 0U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.api_version == 3U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.status == HAL_OK);
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
  ps_power_boot_done = 1U;
  ps_joystick_calibration_boot_load_started = 1U;
  g_ps_hw6_owner_sm_probe.joystick_driver_last_status = 9U;
  g_ps_hw6_owner_sm_probe.joystick_ready_status = 8U;
  g_ps_hw6_owner_sm_probe.joystick_identity_status = 7U;
  g_ps_hw6_owner_sm_probe.joystick_sleep_write_status = UINT32_MAX;
  grant_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(2U) == HAL_ERROR);
  assert(owner_bad == 1U && ps_display_power_barrier_active == 0U);
  first_failure = g_ps_hw6_battery_quiesce_first_failure_probe;
  assert(first_failure.sequence == 3U && first_failure.active == 0U);
  assert(first_failure.status == HAL_ERROR);
  assert(first_failure.display_clock_grant_status == TX_QUEUE_ERROR);
  assert(first_failure.send_status[3] == TX_QUEUE_ERROR);
  assert(first_failure.ack_flags[3] == 0U);
  assert(first_failure.owner_status[3] == UINT32_MAX);
  assert(first_failure.owner_status[2] == HAL_OK);
  assert(first_failure.power_boot_done == 1U && first_failure.calibration_load_started == 1U);
  assert(first_failure.calibration_boot_resolved == 0U);
  assert(first_failure.input_driver_status == 9U && first_failure.input_ready_status == 8U);
  assert(first_failure.input_identity_status == 7U && first_failure.input_sleep_write_status == UINT32_MAX);
  g_ps_hw6_owner_sm_probe.joystick_driver_last_status = 0U;
  g_ps_hw6_owner_sm_probe.joystick_calibration_persistent_boot_resolved = 1U;
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
  assert(g_ps_hw6_battery_quiesce_timing_probe.status == HAL_OK);
  assert(memcmp((const void *)&g_ps_hw6_battery_quiesce_first_failure_probe,
                &first_failure, sizeof(first_failure)) == 0);
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(PS_HW6_POWER_QUIESCE_REASON_SLEEP_PREP) == HAL_OK);
  assert(memcmp((const void *)&g_ps_hw6_battery_quiesce_first_failure_probe,
                &first_failure, sizeof(first_failure)) == 0);

  /* Simulate a new boot: ACK timeout freezes the pending owner action. */
  memset((void *)&g_ps_hw6_battery_quiesce_first_failure_probe, 0,
         sizeof(g_ps_hw6_battery_quiesce_first_failure_probe));
  display_wait_status = TX_NO_EVENTS;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_ERROR);
  assert(g_ps_hw6_battery_quiesce_first_failure_probe.ack_status[3] == TX_NO_EVENTS);
  assert(g_ps_hw6_battery_quiesce_first_failure_probe.owner_status[3] == UINT32_MAX);
  first_failure = g_ps_hw6_battery_quiesce_first_failure_probe;
  display_wait_status = TX_SUCCESS;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_OK);
  assert(g_ps_hw6_battery_quiesce_timing_probe.owner_status[3] == HAL_OK);
  assert(memcmp((const void *)&g_ps_hw6_battery_quiesce_first_failure_probe,
                &first_failure, sizeof(first_failure)) == 0);

  /* The observer still retains a genuine clock timeout outside the handoff. */
  g_ps_hw6_battery_quiesce_timing_probe.active = 1U;
  clock_send_result = TX_SUCCESS;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(3U, 0U, 0U) == TX_NO_EVENTS);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_completions == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_failures == 1U);
  assert(g_ps_hw6_battery_quiesce_timing_probe.display_clock_last_elapsed_ticks == 1000U);
  g_ps_hw6_battery_quiesce_timing_probe.active = 0U;
  uint32_t calls_before = clock_calls;
  uint32_t tick_before = now;
  storage_request_caps = 1U;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(5U, 0U, storage_request_caps) == TX_SUCCESS);
  assert(now - tick_before == 12U); /* Real owner ACKs, no 1000-tick timeout. */
  assert(clock_calls == calls_before + 1U && storage_work == 1U);
  assert(ps_storage_clock_wait_active == 0U);
  assert(ps_storage_power_barrier_active == 0U && storage_grant_held == 0U);
  assert(ps_storage_power_barrier_drop_clock_request == 1U);
  assert(PS_HW6_RTOS_ConsumeStorageBarrierClockRequest(3U) == 0U);
  assert(PS_HW6_RTOS_ConsumeStorageBarrierClockRequest(5U) == 1U);
  assert(PS_HW6_RTOS_ConsumeStorageBarrierClockRequest(5U) == 0U);

  /* Failed direct grants reply with failure, even after successful cleanup. */
  storage_expected_barrier_status = HAL_ERROR;
  storage_expected_clock_status = storage_grant_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(5U, 0U, 1U) == TX_QUEUE_ERROR);
  assert(g_ps_hw6_battery_quiesce_timing_probe.owner_status[5] == UINT32_MAX);
  assert(g_ps_hw6_battery_quiesce_timing_probe.ack_flags[5] == 0U);
  assert(storage_work == 1U && ps_storage_power_barrier_active == 0U);
  assert(PS_HW6_RTOS_ConsumeStorageBarrierClockRequest(5U) == 1U);
  storage_grant_status = TX_SUCCESS;
  storage_expected_barrier_status = HAL_OK;
  storage_request_caps = 3U; /* The flash grant does not authorize MSC. */
  assert(PS_HW6_RTOS_RequestPowerClockProfile(5U, 0U, 3U) == TX_QUEUE_ERROR);
  assert(storage_work == 1U);
  assert(PS_HW6_RTOS_ConsumeStorageBarrierClockRequest(5U) == 1U);

  storage_send_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_ERROR);
  assert(ps_storage_power_barrier_active == 0U && storage_grant_held == 0U);
  storage_send_status = TX_SUCCESS;
  storage_wait_status = TX_NO_EVENTS;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_ERROR);
  assert(ps_storage_power_barrier_active == 0U);
  storage_wait_status = TX_SUCCESS;
  storage_release_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_ERROR);
  assert(ps_storage_power_barrier_active == 0U);
  storage_release_status = TX_SUCCESS;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(3U) == HAL_OK);
  assert(storage_grants == storage_releases && storage_grant_held == 0U);
  uint32_t grants_before = storage_grants;
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(1U) == HAL_OK);
  assert(PS_HW6_RTOS_RunPowerQuiesceBarrier(4U) == HAL_OK);
  assert(storage_grants == grants_before);
  clock_send_result = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RequestPowerClockProfile(5U, 0U, 3U) == TX_QUEUE_ERROR);
  assert(ps_storage_clock_wait_active == 0U);
  return 0;
}
