#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ps_ui_router.h"
#include "ps_hw6_rtos_probe.h"
#include "ps_input_buttons.h"
#include "ps_input_joystick.h"
#include "ps_input_logical.h"
#include "ps_scene_runtime.h"
#include "ps_package_source.h"
#include "ps_package_workflow.h"
#include "shell_constants.inc"

#define TX_SUCCESS 0U
#define TX_NOT_AVAILABLE 1U
#define TX_NOT_DONE 2U
#define TX_NO_INSTANCE 3U
#define TX_PTR_ERROR 4U
#define TX_QUEUE_ERROR 5U

volatile PS_HW6_RTOS_Probe g_ps_hw6_rtos_probe;
volatile ps_scene_runtime_probe_t g_ps_scene_runtime_probe;
volatile ps_package_workflow_probe_t g_ps_package_workflow_probe;
volatile ps_package_source_probe_t g_ps_package_source_probe;
volatile uint32_t g_ps_package_source_override;
static struct { uint32_t usb_host_msc_active; } g_ps_hw6_owner_sm_probe;
static struct { uint32_t export_enabled; } g_ps_storage_msc_bridge_probe;
static uint32_t ps_runtime_interaction_state, ps_runtime_interaction_cue_active;
static uint32_t ps_runtime_interaction_activation_active, ps_input_activity_generation;
static uint32_t ps_package_launch_fault_pending, ps_runtime_package_replace_waiting_for_audio;
static uint32_t runtime_sends, ui_sends, queue_status, last_ui_status, last_ui_event;
static uint32_t scene_active, failure, render_status, storage_loads;
static UINT PS_HW6_RTOS_StopPackageSfx(void) { return TX_SUCCESS; }
static UINT PS_HW6_RTOS_OpenPackageSfx(void) { return TX_SUCCESS; }

enum { FAIL_NONE, FAIL_STORAGE, FAIL_READER, FAIL_SCENE, NO_PACKAGE, FAIL_RELEASE };
static uint32_t PS_HW6_RTOS_RouterEventForLogicalSource(uint32_t source);
static uint32_t PS_HW6_RTOS_LogicalSourceForJoystickDirection(uint32_t mask);

void PS_HW6_TraceUiDispatch(uint32_t event, uint32_t from, uint32_t to, uint32_t status)
{ (void)from; (void)to; last_ui_event = event; last_ui_status = status; }
static ULONG tx_time_get(void) { return 123U; }
static UINT PS_HW6_RTOS_SendUiLogicalPress(uint32_t source)
{
  if (queue_status != TX_SUCCESS) { return queue_status; }
  ui_sends++;
  /* Deliver the queued event to the real thUI router in this host harness. */
  (void)PS_UIRouter_Dispatch(PS_HW6_RTOS_RouterEventForLogicalSource(source));
  return TX_SUCCESS;
}
static UINT PS_HW6_RTOS_SendUiJoystickState(uint32_t candidate, uint32_t resolved, uint32_t dispatch)
{
  if (queue_status != TX_SUCCESS) { return queue_status; }
  ui_sends++;
  assert(PS_UIRouter_RecordJoystickState(candidate, resolved) == PS_STATUS_OK);
  if (dispatch)
  {
    (void)PS_UIRouter_Dispatch(PS_HW6_RTOS_RouterEventForLogicalSource(
      PS_HW6_RTOS_LogicalSourceForJoystickDirection(resolved)));
  }
  return TX_SUCCESS;
}
static UINT PS_HW6_RTOS_SendRuntimeLogicalEvent(uint32_t event, uint32_t source, uint32_t mask)
{ (void)event; (void)source; (void)mask; runtime_sends++; return TX_SUCCESS; }
static UINT PS_HW6_RTOS_RequestRuntimeCommand(uint32_t command)
{ assert(command == PS_HW6_RTOS_COMMAND_RUNTIME_INTERACTION_CUE); runtime_sends++; return TX_SUCCESS; }
static uint32_t PS_HW6_RTOS_UiMscExportActive(void)
{ return g_ps_hw6_owner_sm_probe.usb_host_msc_active; }
static void PS_HW6_RTOS_HandleUiRouterAction(uint32_t action)
{ assert(action == PS_UI_ROUTER_ACTION_MSC_EXIT); g_ps_hw6_rtos_probe.ui_action_send_status = TX_SUCCESS; }
static uint32_t PS_HW6_OwnerStateMachines_JoystickWakeCharacterizationActive(void) { return 0U; }
static void PS_HW6_RTOS_SendCurrentUiRenderCommand(void) {}
static void PS_HW6_RTOS_RequestStop2AutoCheckNow(uint32_t tick) { (void)tick; }
uint32_t PS_SceneRuntime_StateSceneActive(void) { return scene_active; }
uint32_t PS_SceneRuntime_JoystickPolicy(void) { return PS_SCENE_RUNTIME_JOYSTICK_EIGHT_WAY; }
uint32_t PS_SceneRuntime_StateFocusIndex(void) { return 0U; }
void PS_SceneRuntime_ExitStateScene(void) { scene_active = 0U; }
uint32_t PS_SceneRuntime_EnterStateScene(void)
{
  if (failure >= FAIL_SCENE)
  {
    g_ps_scene_runtime_probe.activation_status = (failure == FAIL_SCENE) ?
      1U : PS_SCENE_RUNTIME_STATUS_NO_PACKAGE;
    return PS_SCENE_RUNTIME_INDEX_INVALID;
  }
  scene_active = 1U;
  return 0U;
}
static void PS_HW6_RTOS_RuntimeInteractionEnd(void)
{
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_NONE;
  ps_runtime_interaction_cue_active = ps_runtime_interaction_activation_active = 0U;
}
static void PS_HW6_RTOS_RuntimeInteractionBegin(uint32_t tick)
{ (void)tick; ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_ACTIVE; }
static UINT PS_HW6_RTOS_RequestStoragePackageLoadAndWait(void)
{ storage_loads++; return (failure == FAIL_STORAGE) ? TX_NOT_DONE : TX_SUCCESS; }
static UINT PS_HW6_RTOS_RuntimeVerifyInstalledPackageReader(void)
{ return (failure == FAIL_READER) ? TX_NOT_DONE : TX_SUCCESS; }
static UINT PS_HW6_RTOS_RequestRuntimeClockCapabilities(uint32_t reason, uint32_t capabilities)
{
  assert(reason == PS_HW6_RTOS_RUNTIME_CLOCK_REASON_RELEASE && capabilities == 0);
  return (failure == FAIL_RELEASE) ? TX_NOT_DONE : TX_SUCCESS;
}
static UINT PS_HW6_RTOS_SendDisplayUiRenderCommand(uint32_t page, uint32_t cal,
  uint32_t focus, uint32_t shutdown, uint32_t countdown)
{ (void)page; (void)cal; (void)focus; (void)shutdown; (void)countdown; return render_status; }
void PS_HW6_RTOS_PackageProgress(uint32_t phase) { (void)phase; }

#include "shell_under_test.inc"

static void reset(void)
{
  memset((void *)&g_ps_hw6_rtos_probe, 0, sizeof(g_ps_hw6_rtos_probe));
  memset((void *)&g_ps_scene_runtime_probe, 0, sizeof(g_ps_scene_runtime_probe));
  memset((void *)&g_ps_package_workflow_probe, 0, sizeof(g_ps_package_workflow_probe));
  memset((void *)&g_ps_package_source_probe, 0, sizeof(g_ps_package_source_probe));
  PS_UIRouter_Init();
  PS_HW6_RTOS_RuntimeInteractionEnd();
  g_ps_package_source_override = PS_PACKAGE_SOURCE_OVERRIDE_DEFAULT;
  ps_package_launch_fault_pending = ps_runtime_package_replace_waiting_for_audio = 0U;
  g_ps_hw6_owner_sm_probe.usb_host_msc_active = 0U;
  g_ps_storage_msc_bridge_probe.export_enabled = 0U;
  ui_sends = runtime_sends = scene_active = storage_loads = failure = 0U;
  queue_status = render_status = TX_SUCCESS;
}
static void button(uint32_t source, uint32_t event)
{
  ps_input_button_logical_record_t record = {0};
  record.button_id = source;
  record.event = event;
  record.button_mask = 1U << (source - 1U);
  record.timestamp = 123U;
  (void)PS_HW6_RTOS_DeliverInputLogicalEvent(&record);
}
static void press(uint32_t source) { button(source, PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS); }
static void joystick(uint32_t direction)
{
  (void)PS_HW6_RTOS_DeliverJoystickLogicalEvent(PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS,
    direction, direction, direction, 123U);
}
static void assert_package_tools_reachable(void)
{
  uint32_t before;
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_SYSTEM_MENU_SHELL) == PS_STATUS_OK);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_MENU);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_L);
  assert(g_ps_ui_router_probe.focus_index == 0);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_SETTINGS);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_B);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_MENU);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_L);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_R);
  assert(g_ps_ui_router_probe.focus_index == 1);
  before = ui_sends;
  joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(ui_sends == before + 1 && last_ui_status == PS_STATUS_OK);
  assert(g_ps_ui_router_probe.focus_index == 2);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_B);
  assert(g_ps_ui_router_probe.package_state == PS_UI_ROUTER_PACKAGE_NONE);
  /* B can leave an already-clear browser, so enter it again explicitly. */
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_NAV_PACKAGES) == PS_STATUS_OK);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(PS_UIRouter_TakeAction() == PS_UI_ROUTER_ACTION_MSC_ENTER);
  joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(PS_UIRouter_TakeAction() == PS_UI_ROUTER_ACTION_PACKAGE_SCAN);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_PACKAGE_VALID_FOUND) == PS_STATUS_OK);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(PS_UIRouter_TakeAction() == PS_UI_ROUTER_ACTION_PACKAGE_INSTALL_STUB);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_DONE) == PS_STATUS_OK);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(PS_UIRouter_TakeAction() == PS_UI_ROUTER_ACTION_PACKAGE_LAUNCH);
  assert(runtime_sends == 0);
}

int main(void)
{
  uint32_t lifecycle, render_in_place, fail, before;
  /* Reproduce the target's MENU + LP_GRAPH/ERROR, including a missing runtime. */
  for (lifecycle = PS_HW6_RUNTIME_LIFECYCLE_NONE; lifecycle <= PS_HW6_RUNTIME_LIFECYCLE_ERROR; lifecycle++)
  {
    reset();
    PS_HW6_RTOS_RuntimeSetState(lifecycle ? PS_HW6_RUNTIME_CLASS_LP_GRAPH : PS_HW6_RUNTIME_CLASS_NONE,
      PS_HW6_RUNTIME_EXEC_REACTIVE, lifecycle);
    ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE;
    assert_package_tools_reachable();
    assert(PS_HW6_RTOS_JoystickLogicalPolicy() == PS_SCENE_RUNTIME_JOYSTICK_FOUR_WAY);
  }
  /* Boot and in-place PLAY must both unwind failed activation to SHELL/RUNNING. */
  for (render_in_place = 0; render_in_place <= 1; render_in_place++)
  {
    for (fail = FAIL_STORAGE; fail <= FAIL_RELEASE; fail++)
    {
      reset(); failure = fail;
      assert(PS_HW6_RTOS_RuntimePackageActivateStub(PS_HW6_RUNTIME_CLASS_LP_GRAPH,
        PS_HW6_RUNTIME_EXEC_REACTIVE, 0, TX_SUCCESS, 1, render_in_place) != PS_STATUS_OK);
      assert(g_ps_hw6_rtos_probe.runtime_current_class == PS_HW6_RUNTIME_CLASS_SHELL);
      assert(g_ps_hw6_rtos_probe.runtime_lifecycle == PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
      assert(g_ps_hw6_rtos_probe.runtime_active_package_id == 0 && !scene_active);
      assert(ps_runtime_interaction_state == PS_HW6_RUNTIME_INTERACTION_STATE_NONE);
      assert(!g_ps_hw6_rtos_probe.input_policy_lock_active);
      if (ps_package_launch_fault_pending)
      {
        assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_PACKAGE_LAUNCH_ERROR) == PS_STATUS_OK);
        assert(g_ps_ui_router_probe.package_state == PS_UI_ROUTER_PACKAGE_ERROR);
      }
      else
      {
        assert(fail == NO_PACKAGE && !render_in_place && g_ps_ui_router_request);
        assert(PS_UIRouter_Dispatch(g_ps_ui_router_request_event) == PS_STATUS_OK);
      }
      assert(g_ps_ui_router_probe.eggless && !g_ps_ui_router_probe.resume_available);
      before = storage_loads;
      assert_package_tools_reachable();
      assert(storage_loads == before); /* Navigation never retries the failed egg. */
    }
  }
  reset(); render_status = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_RuntimePackageActivateStub(PS_HW6_RUNTIME_CLASS_LP_GRAPH,
    PS_HW6_RUNTIME_EXEC_REACTIVE, 0, TX_SUCCESS, 1, 1) != PS_STATUS_OK);
  assert(!scene_active && ps_package_launch_fault_pending);
  reset();
  assert(PS_HW6_RTOS_RuntimePackageActivateStub(PS_HW6_RUNTIME_CLASS_LP_GRAPH,
    PS_HW6_RUNTIME_EXEC_REACTIVE, 0, TX_NOT_AVAILABLE, 1, 0) != PS_STATUS_OK);
  assert(g_ps_hw6_rtos_probe.runtime_current_class == PS_HW6_RUNTIME_CLASS_SHELL);

  reset();
  PS_HW6_RTOS_RuntimeSetState(PS_HW6_RUNTIME_CLASS_INSTALLER, PS_HW6_RUNTIME_EXEC_REACTIVE,
    PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  PS_HW6_RTOS_RuntimeErrorInstaller();
  assert(g_ps_hw6_rtos_probe.runtime_lifecycle == PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  assert(g_ps_hw6_rtos_probe.runtime_current_class == PS_HW6_RUNTIME_CLASS_SHELL);
  assert(g_ps_hw6_rtos_probe.runtime_last_event == PS_HW6_RUNTIME_EVENT_INSTALLER_ERROR);
  assert(g_ps_hw6_rtos_probe.runtime_last_status == PS_STATUS_INTERNAL_ERROR);
  assert_package_tools_reachable();

  reset();
  PS_HW6_RTOS_RuntimeSetState(PS_HW6_RUNTIME_CLASS_LP_GRAPH, PS_HW6_RUNTIME_EXEC_REACTIVE,
    PS_HW6_RUNTIME_LIFECYCLE_SUSPENDED);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_SYSTEM_MENU_ACTIVE) == PS_STATUS_OK);
  assert(g_ps_ui_router_probe.resume_available);
  joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(g_ps_ui_router_probe.focus_index == 1);
  joystick(PS_INPUT_JOYSTICK_DIRECTION_UP);
  assert(g_ps_ui_router_probe.focus_index == 0);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(PS_UIRouter_TakeAction() == PS_UI_ROUTER_ACTION_RUNTIME_RESUME);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_PACKAGE_LAUNCH_ERROR) == PS_STATUS_OK);
  assert(!g_ps_ui_router_probe.resume_available);
  assert(g_ps_ui_router_probe.nav_state == PS_UI_ROUTER_NAV_FOCUS);

  reset();
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_LAUNCH_RUNTIME) == PS_STATUS_OK);
  PS_HW6_RTOS_RuntimeSetState(PS_HW6_RUNTIME_CLASS_LP_GRAPH, PS_HW6_RUNTIME_EXEC_REACTIVE,
    PS_HW6_RUNTIME_LIFECYCLE_RUNNING);
  scene_active = 1;
  assert(PS_HW6_RTOS_JoystickLogicalPolicy() == PS_SCENE_RUNTIME_JOYSTICK_EIGHT_WAY);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  button(PS_INPUT_LOGICAL_SOURCE_BUTTON_A, PS_INPUT_BUTTON_LOGICAL_EVENT_RELEASE);
  joystick(PS_INPUT_JOYSTICK_DIRECTION_UP);
  assert(runtime_sends == 3 && ui_sends == 0);
  ps_runtime_interaction_state = PS_HW6_RUNTIME_INTERACTION_STATE_INACTIVE;
  joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(runtime_sends == 3 && ui_sends == 0);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  assert(runtime_sends == 4 && ui_sends == 0);
  PS_HW6_RTOS_RuntimeSetState(PS_HW6_RUNTIME_CLASS_LP_GRAPH, PS_HW6_RUNTIME_EXEC_REACTIVE,
    PS_HW6_RUNTIME_LIFECYCLE_ERROR);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A);
  joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(runtime_sends == 4 && ui_sends == 0); /* Failed game page is not shell focus. */

  reset();
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_SYSTEM_MENU_SHELL) == PS_STATUS_OK);
  g_ps_hw6_rtos_probe.input_policy_lock_active = 1;
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A); joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(ui_sends == 0 && runtime_sends == 0);
  g_ps_hw6_rtos_probe.input_policy_lock_active = 0;
  g_ps_package_workflow_probe.active = 1;
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A); joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(ui_sends == 0 && runtime_sends == 0);
  g_ps_package_workflow_probe.active = 0;
  queue_status = TX_QUEUE_ERROR;
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_A); joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(ui_sends == 0 && g_ps_ui_router_probe.button_event_count == 0);
  assert(g_ps_hw6_rtos_probe.input_policy_last_reason == PS_HW6_RTOS_INPUT_POLICY_REASON_SEND_FAILED);
  queue_status = TX_SUCCESS;
  button(PS_INPUT_LOGICAL_SOURCE_BUTTON_A, PS_INPUT_BUTTON_LOGICAL_EVENT_RELEASE);
  assert(ui_sends == 0); /* Shell acts on presses, not releases. */

  reset();
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_LOW_BATTERY_BOOT_BLOCK) == PS_STATUS_OK);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_PACKAGE_LAUNCH_ERROR) == PS_STATUS_INVALID_STATE);
  press(PS_INPUT_LOGICAL_SOURCE_BUTTON_B); joystick(PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_SHUTDOWN);
  assert(g_ps_ui_router_probe.nav_state == PS_UI_ROUTER_NAV_MODAL_LOCK);
  assert(runtime_sends == 0);
  reset();
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_SHELL_FAULT) == PS_STATUS_OK);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_PACKAGE_LAUNCH_ERROR) == PS_STATUS_INVALID_STATE);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_INPUT_BTN_B) == PS_STATUS_INVALID_STATE);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_ERROR);
  puts("shell navigation and recovery checks passed");
  return 0;
}
