#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_scene_render_model.h"
#include "ps_hw6_object_development.h"
typedef uint32_t UINT;
typedef uint32_t ULONG;
#define TX_SUCCESS 0U
#define TX_CALLER_ERROR 1U
#define TX_AND_CLEAR 1U
#define TX_NO_WAIT 0U
#define TX_TIMER_TICKS_PER_SECOND 100U
#define PS_HW6_RTOS_EVENT_DEBUG_INDEX 3U
#define PS_HW6_RTOS_OBJECT_DISPLAY_ACK (1U << 13)
#define PS_HW6_RTOS_OBJECT_DISPLAY_MAGIC 0x4F424A32U
#define PS_HW6_RTOS_OWNER_DISPLAY 3U
#define PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS 1000U
#define PS_HW6_RTOS_STATUS_NOT_RUN 0xFFFFFFFFU
#define PS_HW6_RTOS_MESSAGE_WORDS 4U
#define HAL_OK 0U
static uint32_t ps_event_groups[4], ps_queues[9];
volatile ps_hw6_object_development_probe_t g_ps_object_development_probe;
static ps_scene_render_model_t ps_object_display_lease;
static ps_object_waiting_program_t ps_object_waiting_lease;
static uint32_t ps_object_waiting_lease_request;
static uint32_t ps_object_waiting_deadline;
static uint64_t ps_object_missing_consumed;
volatile ps_hw6_object_lpbam_probe_t g_ps_object_lpbam_probe;
volatile uint32_t g_ps_object_lpbam_prepare_request;
volatile ps_hw6_object_lpbam_prepare_probe_t g_ps_object_lpbam_prepare_probe;
static uint32_t schedule_result, builds;
static uint32_t PS_SceneRuntime_BuildDevelopmentWaiting(ps_object_waiting_program_t *program)
{
  builds++;
  program->step_count = 4;
  program->quantum_ms = 400;
  program->initial_remaining_ms = 150;
  return schedule_result;
}
static uint32_t ps_object_last_tick, ps_object_tick_fraction;
static uint32_t ps_object_clock_activation = 42, ps_object_rtc_valid;
static struct { uint32_t runtime_active_unit_id; } g_ps_hw6_rtos_probe;
static struct { uint32_t scene_id; } g_ps_scene_runtime_probe;
static uint32_t now, queue_result, wait_result, render_result, mismatch;
static uint32_t advance_calls, project_calls, send_calls, clock_elapsed;
static uint32_t next_duration = 250;
static uint32_t tx_time_get(void) { return now; }
static uint32_t PS_HW6_RTOS_MsToTicks(uint32_t ms) { return (ms + 9) / 10; }
static uint32_t PS_SceneRuntime_SceneActivation(void) { return 42; }
static uint32_t PS_SceneRuntime_AdvanceDevelopmentObjects(uint32_t ms)
{ advance_calls++; clock_elapsed += ms; return 0; }
static uint32_t PS_SceneRuntime_ProjectDevelopmentObjects(ps_scene_render_model_t *model, uint32_t *ms)
{ project_calls++; model->state_id = 2; model->elements[0].asset_id = project_calls; *ms = next_duration; return 0; }
static UINT tx_queue_send(uint32_t *queue, const ULONG *message, ULONG wait)
{
  assert(queue == &ps_queues[3] && wait == TX_NO_WAIT);
  assert(message[0] == PS_HW6_RTOS_OBJECT_DISPLAY_MAGIC && message[1] == 3 && message[3] == 42);
  send_calls++;
  return queue_result;
}
static UINT tx_event_flags_get(uint32_t *group, ULONG flags, UINT op, ULONG *actual, ULONG wait)
{
  assert(group == &ps_event_groups[3] && flags == PS_HW6_RTOS_OBJECT_DISPLAY_ACK && op == TX_AND_CLEAR);
  *actual = flags;
  if (wait == TX_NO_WAIT) { return 0; }
  assert(wait == PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  if (wait_result == 0)
  {
    g_ps_object_development_probe.render_complete = g_ps_object_development_probe.render_request - mismatch;
    g_ps_object_development_probe.render_status = render_result;
  }
  return wait_result;
}
#include "object_handoff_under_test.inc"

int main(void)
{
  now = 100;
  assert(PS_HW6_RTOS_ObjectPresent() == 0);
  assert(clock_elapsed == 1000 && g_ps_object_development_probe.next_tick == 125);
  now = 113;
  assert(PS_HW6_RTOS_ObjectPresent() == 0);
  assert(clock_elapsed == 1130 && g_ps_object_development_probe.state_id == 2);
  next_duration = 0;
  assert(PS_HW6_RTOS_ObjectPresent() == 0 && g_ps_object_development_probe.next_tick == 0);
  assert(builds == 0);
  g_ps_object_lpbam_prepare_request = 1;
  assert(PS_HW6_RTOS_ObjectPresent() == 0);
  assert(builds == 1 && g_ps_object_lpbam_prepare_request == 0);
  assert(ps_object_waiting_lease_request == g_ps_object_development_probe.render_request);
  assert(g_ps_object_lpbam_prepare_probe.quantum_ms == 400);
  assert(g_ps_object_lpbam_prepare_probe.initial_remaining_ms == 150);
  assert(PS_HW6_RTOS_ObjectPresent() == 0 && ps_object_waiting_lease_request == 0);
  schedule_result = PS_OBJECT_WAITING_CAPACITY;
  g_ps_object_lpbam_prepare_request = 1;
  assert(PS_HW6_RTOS_ObjectPresent() == 0 && ps_object_waiting_lease_request == 0);
  assert(g_ps_object_lpbam_prepare_probe.complete_count == 2 && builds == 2);
  queue_result = 9;
  assert(PS_HW6_RTOS_ObjectPresent() == 9 && g_ps_object_development_probe.lease_fault == 0);
  queue_result = 0;
  render_result = 1;
  assert(PS_HW6_RTOS_ObjectPresent() == TX_CALLER_ERROR);
  render_result = 0;
  mismatch = 1;
  assert(PS_HW6_RTOS_ObjectPresent() == TX_CALLER_ERROR);
  mismatch = 0;
  wait_result = 7;
  assert(PS_HW6_RTOS_ObjectPresent() == 7 && g_ps_object_development_probe.lease_fault == 1);
  {
    uint32_t a = advance_calls, p = project_calls, s = send_calls;
    ps_scene_render_model_t saved = ps_object_display_lease;
    ps_object_waiting_program_t waiting_saved = ps_object_waiting_lease;
    g_ps_object_lpbam_prepare_request = 1;
    wait_result = 0;
    now += 200;
    assert(PS_HW6_RTOS_ObjectPresent() == TX_CALLER_ERROR);
    assert(a == advance_calls && p == project_calls && s == send_calls);
    assert(memcmp(&saved, &ps_object_display_lease, sizeof(saved)) == 0);
    assert(memcmp(&waiting_saved, &ps_object_waiting_lease, sizeof(waiting_saved)) == 0);
    assert(g_ps_object_lpbam_prepare_request == 1 && builds == 2);
  }
  return 0;
}
