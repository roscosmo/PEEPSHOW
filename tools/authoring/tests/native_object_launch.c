#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ps_hw6_object_development.h"
#include "ps_ui_router.h"

#define TX_SUCCESS 0U
#define HAL_ERROR 1U
#define HAL_BUSY 2U
#define PS_HW6_RTOS_OWNER_DISPLAY 3U
#define PS_HW6_RUNTIME_CLASS_LP_GRAPH 2U
#define PS_HW6_RUNTIME_EXEC_REACTIVE 1U
#define PS_HW6_RUNTIME_LIFECYCLE_RUNNING 2U

volatile ps_hw6_object_development_probe_t g_ps_object_development_probe;
volatile uint32_t g_ps_object_development_request;
volatile ps_ui_router_probe_t g_ps_ui_router_probe;
volatile uint32_t g_ps_ui_router_request, g_ps_ui_router_request_event;
const uint8_t g_ps_object_development_egg[] = {0};
const uint32_t g_ps_object_development_egg_size = 1;
static struct { uint32_t active; } g_ps_package_workflow_probe;
static struct { uint32_t display_complete; } g_ps_hw6_owner_probe;
static struct { uint32_t scene_id; } g_ps_scene_runtime_probe;
static struct {
  uint32_t runtime_active_capabilities, runtime_active_package_id;
  uint32_t runtime_active_unit_id, runtime_lifecycle;
} g_ps_hw6_rtos_probe;
static struct { uint32_t tx_queue_enqueued; } ps_queues[9];
static uint32_t ps_audio_sfx_pending, ps_audio_sfx_clock_held;
static uint32_t ps_object_launching, ps_object_last_tick, ps_object_tick_fraction;
static uint32_t active, enter_status, present_status, presentations, failures;
static uint32_t timer_syncs;
static uint32_t PS_HW6_RTOS_SystemOverlayActive(void) { return 0; }
static uint32_t PS_HW6_AudioOwner_SfxActive(void) { return 0; }
static void PS_HW6_RTOS_RuntimeInteractionEnd(void) {}
static void PS_SceneRuntime_ExitStateScene(void) { active = 0; }
static uint32_t PS_SceneRuntime_DevelopmentObjectsActive(void) { return active; }
static uint32_t tx_time_get(void) { return 100; }
static uint32_t PS_SceneRuntime_EnterDevelopmentObjects(const uint8_t *egg, uint32_t size)
{
  assert(egg == g_ps_object_development_egg && size == 1);
  active = (enter_status == 0);
  return enter_status;
}
static void PS_HW6_RTOS_RuntimeSetState(uint32_t cls, uint32_t exec, uint32_t life)
{
  assert(cls == PS_HW6_RUNTIME_CLASS_LP_GRAPH && exec == PS_HW6_RUNTIME_EXEC_REACTIVE);
  g_ps_hw6_rtos_probe.runtime_lifecycle = life;
}
static uint32_t PS_HW6_RTOS_ObjectPresent(void)
{
  assert(active && g_ps_ui_router_request == 0);
  assert(timer_syncs == 1);
  presentations++;
  g_ps_object_development_probe.next_tick = 125;
  return present_status;
}
static uint32_t PS_HW6_RTOS_RuntimePackageReplacementFail(void)
{ failures++; active = 0; return 0; }
static void PS_HW6_RTOS_RuntimeStateTimersSync(uint32_t tick, uint32_t force)
{ assert(active && tick == 100 && force == 1); timer_syncs++; }

/* The old launch path calls this restricted helper and fails. */
uint32_t PS_HW6_RTOS_SendUiLifecycleEvent(uint32_t event)
{ assert(event == PS_UI_ROUTER_EVENT_LAUNCH_RUNTIME); return 1; }

#include "object_launch_under_test.inc"

static void reset(void)
{
  memset((void *)&g_ps_object_development_probe, 0, sizeof(g_ps_object_development_probe));
  memset(&g_ps_hw6_rtos_probe, 0, sizeof(g_ps_hw6_rtos_probe));
  g_ps_ui_router_probe.current_page = PS_UI_ROUTER_PAGE_HOME;
  g_ps_hw6_owner_probe.display_complete = 1;
  g_ps_scene_runtime_probe.scene_id = 42;
  g_ps_object_development_request = 1;
  g_ps_ui_router_request = 0;
  g_ps_ui_router_request_event = 0;
  active = enter_status = present_status = presentations = failures = 0;
  timer_syncs = 0;
}

int main(void)
{
  reset();
  PS_HW6_RTOS_ObjectService(100);
  assert(g_ps_object_development_probe.launch_status == 0);
  assert(active && presentations == 1 && failures == 0);
  assert(g_ps_ui_router_request == 1);
  assert(g_ps_ui_router_request_event == PS_UI_ROUTER_EVENT_LAUNCH_RUNTIME);
  assert(g_ps_hw6_rtos_probe.runtime_active_unit_id == 42);
  assert(g_ps_object_development_request == 0 && ps_object_launching == 0);
  g_ps_ui_router_request = 0;
  PS_HW6_RTOS_ObjectService(124);
  assert(presentations == 1);
  PS_HW6_RTOS_ObjectService(125);
  assert(presentations == 2 && active && failures == 0);

  reset();
  present_status = 1;
  PS_HW6_RTOS_ObjectService(100);
  assert(g_ps_object_development_probe.launch_status == HAL_ERROR);
  assert(presentations == 1 && failures == 1 && !active);
  assert(g_ps_ui_router_request == 0 && ps_object_launching == 0);

  reset();
  enter_status = 1;
  PS_HW6_RTOS_ObjectService(100);
  assert(presentations == 0 && failures == 1 && !active);
  assert(g_ps_ui_router_request == 0);

  reset();
  g_ps_object_development_probe.lease_fault = 1;
  PS_HW6_RTOS_ObjectService(100);
  assert(g_ps_object_development_probe.admission_blockers == 1);
  assert(presentations == 0 && failures == 0 && !active);
  assert(g_ps_ui_router_request == 0);
  return 0;
}
