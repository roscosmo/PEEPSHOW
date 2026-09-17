#include <assert.h>
#include <stdint.h>

typedef uint32_t ULONG;
#define TX_NO_WAIT 0U
#define PS_HW6_RTOS_HEARTBEAT_TICKS 10U
#define PS_HW6_RTOS_OWNER_POWER 0U
#define PS_HW6_RTOS_OWNER_AUDIO 1U
#define PS_HW6_RTOS_OWNER_INPUT 2U
#define PS_HW6_RTOS_OWNER_DISPLAY 3U
#define PS_HW6_RTOS_OWNER_SENSOR 4U
#define PS_HW6_RTOS_OWNER_STORAGE 5U
#define PS_HW6_RTOS_OWNER_COMM 6U
#define PS_HW6_RTOS_OWNER_RUNTIME 8U
#define PS_HW6_RUNTIME_LIFECYCLE_RUNNING 2U
#define PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE 1U
#define PS_UI_ROUTER_PAGE_CALIBRATION 9U
#define PS_UI_ROUTER_CAL_JOYSTICK_REVIEW 7U

static struct { uint32_t active; } g_ps_hw6_battery_fault_wait_probe;
static struct {
  uint32_t runtime_lifecycle, display_deadline_wait_count;
  uint32_t display_deadline_due_count, display_deadline_wait_last_ticks;
} g_ps_hw6_rtos_probe;
static struct { uint32_t current_page, calibration_page; } g_ps_ui_router_probe;
static struct { uint32_t joystick_calibration_session_active; } g_ps_hw6_owner_sm_probe;
static struct { uint32_t enabled, publish_status; } g_ps_object_lpbam_probe;
static struct { uint32_t next_tick; } g_ps_object_development_probe;
static uint32_t ps_joystick_calibration_review_next_tick, ps_joystick_awake_poll_next_tick;
static uint32_t ps_runtime_interaction_state, ps_runtime_interaction_deadline_tick;
static uint32_t ps_runtime_interaction_cue_active, ps_runtime_interaction_cue_deadline_tick;
static uint32_t ps_runtime_interaction_activation_active, ps_runtime_interaction_activation_deadline_tick;
static uint32_t ps_display_blink_next_tick, ps_display_blink_stop2_suppressed;
static uint32_t buttons_wait = PS_HW6_RTOS_HEARTBEAT_TICKS;
static uint32_t timer_active, timer_remaining, timer_queries, objects_active, audio_active;

static uint32_t PS_InputButtons_NextWaitTicks(uint32_t now, uint32_t maximum)
{ (void)now; assert(buttons_wait <= maximum); return buttons_wait; }
static uint32_t PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureActive(void) { return 0; }
static uint32_t PS_HW6_OwnerStateMachines_JoystickCalibrationCaptureNextTick(void) { return 0; }
static uint32_t PS_HW6_OwnerStateMachines_JoystickWakeCharacterizationCaptureActive(void) { return 0; }
static uint32_t PS_HW6_OwnerStateMachines_JoystickWakeCharacterizationCaptureNextTick(void) { return 0; }
static uint32_t PS_SceneRuntime_DevelopmentObjectsActive(void) { return objects_active; }
static uint32_t PS_HW6_AudioOwner_SfxActive(void) { return audio_active; }
static uint32_t PS_HW6_RTOS_RuntimeStateTimerNext(uint32_t now, uint32_t *remaining,
                                               uint32_t *binding)
{
  (void)now;
  timer_queries++;
  *remaining = timer_remaining;
  *binding = 0;
  return timer_active;
}
#include "owner_wait.inc"

int main(void)
{
  uint32_t i, queries;
  const uint32_t ordinary_owners[] = {PS_HW6_RTOS_OWNER_POWER, PS_HW6_RTOS_OWNER_SENSOR,
    PS_HW6_RTOS_OWNER_STORAGE, PS_HW6_RTOS_OWNER_COMM};

  /* Reproduce the bench deadline immediately preceding the failed barrier. */
  ps_display_blink_next_tick = 663;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_DISPLAY, 664) == TX_NO_WAIT);
  assert(g_ps_hw6_rtos_probe.display_deadline_due_count == 1);
  g_ps_hw6_battery_fault_wait_probe.active = 1;
  for (i = 0; i < 100; ++i)
  {
    assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_DISPLAY, 664 + i) ==
           PS_HW6_RTOS_HEARTBEAT_TICKS);
  }
  assert(g_ps_hw6_rtos_probe.display_deadline_due_count == 1);
  assert(g_ps_hw6_rtos_probe.display_deadline_wait_last_ticks == PS_HW6_RTOS_HEARTBEAT_TICKS);
  assert(ps_display_blink_next_tick == 663); /* Suppression does not rewrite playback. */

  objects_active = 1;
  g_ps_hw6_rtos_probe.runtime_lifecycle = PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
  g_ps_object_development_probe.next_tick = 663;
  timer_active = 1;
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE;
  ps_runtime_interaction_deadline_tick = 663;
  ps_runtime_interaction_cue_active = ps_runtime_interaction_activation_active = 1;
  ps_runtime_interaction_cue_deadline_tick = ps_runtime_interaction_activation_deadline_tick = 663;
  queries = timer_queries;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_RUNTIME, 664) ==
         PS_HW6_RTOS_HEARTBEAT_TICKS);
  assert(timer_queries == queries);

  /* Button debounce/release scheduling and audio cleanup retain their deadlines. */
  buttons_wait = 2;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_INPUT, 664) == 2);
  buttons_wait = 0;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_INPUT, 664) == TX_NO_WAIT);
  audio_active = 1;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_AUDIO, 664) == TX_NO_WAIT);
  for (i = 0; i < sizeof(ordinary_owners) / sizeof(ordinary_owners[0]); ++i)
  { assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(ordinary_owners[i], 664) == PS_HW6_RTOS_HEARTBEAT_TICKS); }

  g_ps_hw6_battery_fault_wait_probe.active = 0;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_DISPLAY, 664) == TX_NO_WAIT);
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_RUNTIME, 664) == TX_NO_WAIT);
  ps_display_blink_next_tick = 669;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_DISPLAY, 664) == 5);
  ps_display_blink_stop2_suppressed = 1;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_DISPLAY, 664) == PS_HW6_RTOS_HEARTBEAT_TICKS);
  objects_active = 0;
  ps_runtime_interaction_state = 0;
  ps_runtime_interaction_cue_active = ps_runtime_interaction_activation_active = 0;
  timer_remaining = 3;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_RUNTIME, 664) == 3);
  timer_active = 0;
  assert(PS_HW6_RTOS_OwnerReceiveWaitTicks(PS_HW6_RTOS_OWNER_RUNTIME, 664) == PS_HW6_RTOS_HEARTBEAT_TICKS);
  return 0;
}
