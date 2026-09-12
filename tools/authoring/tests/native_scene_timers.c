#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ps_scene_runtime.h"
#include "timer_probe_under_test.inc"

#define PS_HW6_RUNTIME_CLASS_LP_GRAPH 2U
#define PS_HW6_RUNTIME_LIFECYCLE_RUNNING 2U

uint32_t PS_SceneRuntime_DevelopmentObjectsActive(void) { return 0; }
uint32_t PS_SceneRuntime_ObjectReplacementRejected(void) { return 0; }
static uint32_t tx_time_get(void) { assert(0); return 0; }
static uint32_t PS_HW6_RTOS_ObjectAdvance(uint32_t now) { (void)now; assert(0); return 1; }
static void PS_HW6_RTOS_RuntimePackageReplacementFail(void) { assert(0); }

static uint32_t scene_active;
static uint32_t scene_activation;
static uint32_t state_activation;
static uint32_t enabled[16], scopes[16], starts[16], delays[16];
static uint32_t command_binding[32], command_kind[32], command_count, command_take;
static uint32_t delivered[32], delivered_count;
static uint32_t cancel_second;

uint32_t PS_SceneRuntime_StateSceneActive(void) { return scene_active; }
uint32_t PS_SceneRuntime_SceneActivation(void) { return scene_activation; }
uint32_t PS_SceneRuntime_StateActivation(void) { return state_activation; }
uint32_t PS_SceneRuntime_TimerConfiguration(uint32_t i, uint32_t *scope,
                                           uint32_t *start, uint32_t *delay)
{
  *scope = scopes[i]; *start = starts[i]; *delay = delays[i];
  return enabled[i];
}
uint32_t PS_SceneRuntime_TakeTimerAction(uint32_t *binding, uint32_t *kind)
{
  if (command_take == command_count) { return 0; }
  *binding = command_binding[command_take]; *kind = command_kind[command_take++];
  return 1;
}
static void Command(uint32_t binding, uint32_t kind)
{
  assert(command_count < 32);
  command_binding[command_count] = binding;
  command_kind[command_count++] = kind;
}
uint32_t PS_SceneRuntime_HandleStateSceneEvent(uint32_t binding)
{
  assert(delivered_count < 32);
  delivered[delivered_count++] = binding;
  if (cancel_second && binding == 0)
  {
    Command(1, PS_SCENE_RUNTIME_ACTION_CANCEL_TIMER);
  }
  return PS_SCENE_RUNTIME_INPUT_APPLIED;
}
static uint32_t PS_HW6_RTOS_CompleteStateSceneEvent(uint32_t result) { return result; }
static uint32_t PS_HW6_RTOS_MsToTicks(uint32_t ms) { return (ms + 9U) / 10U; }
static uint32_t PS_HW6_RTOS_TimeReached(uint32_t now, uint32_t deadline)
{
  return (int32_t)(now - deadline) >= 0;
}

#include "scene_timers_under_test.inc"

static void Reset(void)
{
  memset(&g_ps_hw6_rtos_probe, 0, sizeof(g_ps_hw6_rtos_probe));
  memset(enabled, 0, sizeof(enabled));
  memset(scopes, 0, sizeof(scopes));
  memset(starts, 0, sizeof(starts));
  memset(delays, 0, sizeof(delays));
  scene_active = 1; scene_activation = 1; state_activation = 1;
  command_count = 0; command_take = 0; delivered_count = 0; cancel_second = 0;
  g_ps_hw6_rtos_probe.runtime_current_class = PS_HW6_RUNTIME_CLASS_LP_GRAPH;
  g_ps_hw6_rtos_probe.runtime_lifecycle = PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
  PS_HW6_RTOS_RuntimeStateTimersClear();
  enabled[0] = 1; scopes[0] = PS_SCENE_RUNTIME_TIMER_SCENE; delays[0] = 1000;
}

int main(void)
{
  Reset();
  enabled[1] = 1; scopes[1] = PS_SCENE_RUNTIME_TIMER_STATE_ENTRY; delays[1] = 1500;
  PS_HW6_RTOS_RuntimeStateTimersSync(100, 0);
  assert(ps_runtime_state_timers[0].deadline_tick == 200);
  state_activation++;
  enabled[1] = 0;
  PS_HW6_RTOS_RuntimeStateTimersSync(140, 0);
  assert(ps_runtime_state_timers[0].deadline_tick == 200);
  assert(ps_runtime_state_timers[1].active == 0);
  PS_HW6_RTOS_RuntimeStateTimersService(200);
  PS_HW6_RTOS_RuntimeStateTimersService(500);
  assert(delivered_count == 1 && delivered[0] == 0);
  state_activation++;
  enabled[1] = 1;
  PS_HW6_RTOS_RuntimeStateTimersSync(600, 0);
  assert(ps_runtime_state_timers[0].active == 0);
  assert(ps_runtime_state_timers[1].deadline_tick == 750);

  Reset();
  starts[0] = PS_SCENE_RUNTIME_TIMER_START_ACTION;
  PS_HW6_RTOS_RuntimeStateTimersSync(100, 0);
  assert(ps_runtime_state_timers[0].active == 0);
  Command(0, PS_SCENE_RUNTIME_ACTION_START_TIMER);
  PS_HW6_RTOS_RuntimeStateTimersSync(200, 0);
  Command(0, PS_SCENE_RUNTIME_ACTION_START_TIMER);
  PS_HW6_RTOS_RuntimeStateTimersSync(250, 0);
  assert(ps_runtime_state_timers[0].deadline_tick == 300);
  Command(0, PS_SCENE_RUNTIME_ACTION_RESTART_TIMER);
  PS_HW6_RTOS_RuntimeStateTimersSync(299, 0);
  assert(ps_runtime_state_timers[0].deadline_tick == 399);
  Command(0, PS_SCENE_RUNTIME_ACTION_CANCEL_TIMER);
  PS_HW6_RTOS_RuntimeStateTimersService(500);
  assert(delivered_count == 0);

  Reset();
  PS_HW6_RTOS_RuntimeStateTimersSync(100, 0);
  PS_HW6_RTOS_RuntimeStateTimersPause(140);
  PS_HW6_RTOS_RuntimeStateTimersService(1000);
  assert(delivered_count == 0);
  PS_HW6_RTOS_RuntimeStateTimersResume(1000);
  PS_HW6_RTOS_RuntimeStateTimersService(1059);
  assert(delivered_count == 0);
  PS_HW6_RTOS_RuntimeStateTimersService(1060);
  assert(delivered_count == 1);

  Reset();
  PS_HW6_RTOS_RuntimeStateTimersSync(100, 0);
  scene_activation++; state_activation++;
  PS_HW6_RTOS_RuntimeStateTimersSync(199, 0);
  assert(ps_runtime_state_timers[0].deadline_tick == 299);
  scene_active = 0;
  PS_HW6_RTOS_RuntimeStateTimersSync(200, 0);
  assert(ps_runtime_state_timers[0].configured == 0);

  Reset();
  enabled[1] = 1; scopes[1] = PS_SCENE_RUNTIME_TIMER_SCENE; delays[1] = 500;
  PS_HW6_RTOS_RuntimeStateTimersSync(100, 0);
  PS_HW6_RTOS_RuntimeStateTimersService(300);
  assert(delivered_count == 2 && delivered[0] == 1 && delivered[1] == 0);

  Reset();
  enabled[1] = 1; scopes[1] = PS_SCENE_RUNTIME_TIMER_SCENE; delays[1] = 500;
  PS_HW6_RTOS_RuntimeStateTimersSync(100, 0);
  PS_HW6_RTOS_RuntimeStateTimersPause(250);
  PS_HW6_RTOS_RuntimeStateTimersResume(1000);
  PS_HW6_RTOS_RuntimeStateTimersService(1000);
  assert(delivered_count == 2 && delivered[0] == 1 && delivered[1] == 0);

  Reset();
  enabled[1] = 1; scopes[1] = PS_SCENE_RUNTIME_TIMER_SCENE; delays[1] = 1000;
  PS_HW6_RTOS_RuntimeStateTimersSync(100, 0);
  cancel_second = 1;
  PS_HW6_RTOS_RuntimeStateTimersService(200);
  assert(delivered_count == 1 && delivered[0] == 0);

  Reset();
  delays[0] = 320;
  PS_HW6_RTOS_RuntimeStateTimersSync(0xFFFFFFF0U, 0);
  PS_HW6_RTOS_RuntimeStateTimersService(15);
  assert(delivered_count == 0);
  PS_HW6_RTOS_RuntimeStateTimersService(16);
  assert(delivered_count == 1);

  puts("native firmware timer scheduling checks passed");
  return 0;
}
