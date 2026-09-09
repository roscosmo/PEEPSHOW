#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ps_package_workflow.h"
#include "ps_ui_router.h"
#include "ps_hw6_rtos_probe.h"
#include "workflow_constants.inc"

#define TX_SUCCESS 0U
#define TX_NOT_AVAILABLE 1U
#define TX_NOT_DONE 2U
#define TX_NO_EVENTS 3U
#define TX_QUEUE_ERROR 4U
#define TX_NO_WAIT 0U
#define TX_AND_CLEAR 1U
#define TX_TIMER_TICKS_PER_SECOND 100U
#define TX_INTERRUPT_SAVE_AREA
#define TX_DISABLE
#define TX_RESTORE
#define HAL_OK 0U
#define HAL_ERROR 1U
#define PS_HW6_RTOS_DISPLAY_CLOCK_TRANSFER_CAPABILITIES 1U
#define PS_HW6_RTOS_UI_CLOCK_REACTIVE_CAPABILITIES 1U
typedef uint32_t HAL_StatusTypeDef;

static struct { ULONG words[32][4]; uint32_t count; } ps_queues[9];
static uint32_t ps_event_groups[4];
static struct { uint32_t ospi_kernel_hz; } g_ps_hw6_clock_policy_probe;
volatile PS_HW6_RTOS_Probe g_ps_hw6_rtos_probe;
volatile ps_package_workflow_probe_t g_ps_package_workflow_probe;
static const uint8_t *ps_package_validation_blob;
static uint32_t ps_package_validation_size;
static volatile uint32_t ps_package_validation_busy, ps_package_validation_status;
static volatile uint32_t ps_package_workflow_notice_pending, ps_package_workflow_finished;
static volatile uint32_t ps_package_launch_fault_pending;
static uint32_t tick, cpu_hz, fail_owner, display_result, admission_result;
static uint32_t prepare_count, storage_count, installer_errors, renders, last_event;
static uint32_t runtime_send_result, validation_wait_result;

static void PS_HW6_RTOS_PackageWorkflowFinish(UINT status);
static void PS_HW6_RTOS_PackageWorkflowStorage(void);
static ULONG tx_time_get(void) { return tick; }
static uint32_t HAL_RCC_GetHCLKFreq(void) { return cpu_hz; }
static UINT tx_queue_send(void *queue, ULONG *message, ULONG wait)
{
  uint32_t owner = (uint32_t)message[1];
  assert(owner < 9 && queue == &ps_queues[owner] && wait == TX_NO_WAIT);
  if (owner == fail_owner) { return TX_QUEUE_ERROR; }
  assert(ps_queues[owner].count < 32);
  memcpy(ps_queues[owner].words[ps_queues[owner].count++], message, 4 * sizeof(ULONG));
  return TX_SUCCESS;
}
static UINT tx_event_flags_get(void *group, ULONG mask, UINT mode, ULONG *actual, ULONG wait)
{
  assert(group == &ps_event_groups[PS_HW6_RTOS_EVENT_DEBUG_INDEX]);
  assert(mask == PS_HW6_RTOS_PACKAGE_VALIDATE_ACK && mode == TX_AND_CLEAR);
  *actual = 0;
  if (wait == TX_NO_WAIT) { return TX_NO_EVENTS; }
  assert(wait == PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS);
  if (validation_wait_result == TX_SUCCESS)
  {
    ps_package_validation_busy = 0;
    ps_package_validation_status = 0;
  }
  return validation_wait_result;
}
static UINT PS_HW6_RTOS_RequestRuntimeCommand(uint32_t command)
{
  assert(command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_VALIDATE);
  return runtime_send_result;
}
static UINT PS_HW6_RTOS_RequestDisplayClockCapabilities(uint32_t reason, uint32_t caps)
{ (void)reason; (void)caps; return TX_SUCCESS; }
static UINT PS_HW6_RTOS_RequestUiClockCapabilities(uint32_t reason, uint32_t caps)
{ (void)reason; (void)caps; return TX_SUCCESS; }
static HAL_StatusTypeDef PS_HW6_DisplayOwner_RenderUI(uint32_t page, uint32_t cal,
  uint32_t focus, uint32_t shutdown, uint32_t seconds)
{
  assert(page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER && cal == PS_UI_ROUTER_CAL_NONE);
  assert(focus >= PS_PACKAGE_WORKFLOW_DISPLAY_BASE && shutdown == 0 && seconds == 0);
  renders++;
  tick += 3;
  return display_result;
}
static void PS_HW6_RTOS_ResetDisplayCursorBlink(uint32_t now) { (void)now; }
static uint32_t PS_HW6_RTOS_AdmissionActionForUiRouterAction(uint32_t action)
{ return (action == PS_UI_ROUTER_ACTION_PACKAGE_LAUNCH) ? 0U : action; }
static UINT PS_HW6_RTOS_AdmitSystemAction(uint32_t action)
{ assert(action != 0); prepare_count++; return admission_result; }
static void PS_HW6_RTOS_RuntimeEnterInstaller(void)
{ g_ps_hw6_rtos_probe.runtime_current_class = PS_HW6_RUNTIME_CLASS_INSTALLER; }
static void PS_HW6_RTOS_RuntimeErrorInstaller(void)
{
  installer_errors++;
  g_ps_hw6_rtos_probe.runtime_current_class = PS_HW6_RUNTIME_CLASS_SHELL;
  g_ps_hw6_rtos_probe.runtime_lifecycle = PS_HW6_RUNTIME_LIFECYCLE_RUNNING;
}
static UINT PS_HW6_RTOS_SendUiLifecycleEvent(uint32_t event)
{ assert(event == PS_UI_ROUTER_EVENT_SYSTEM_MENU_DISCARD); return TX_SUCCESS; }
static void PS_HW6_RTOS_HandleRuntimeCommand(uint32_t command)
{
  assert(command == PS_HW6_RTOS_COMMAND_RUNTIME_PACKAGE_REPLACE);
  PS_HW6_RTOS_PackageWorkflowFinish(TX_SUCCESS);
}
static UINT storage_work(uint32_t event)
{
  assert(g_ps_package_workflow_probe.display_status == HAL_OK);
  assert(g_ps_package_workflow_probe.work_start_tick >= g_ps_package_workflow_probe.displayed_tick);
  storage_count++;
  PS_HW6_RTOS_PackageProgress(PS_PACKAGE_WORKFLOW_READING);
  tick += 17;
  PS_HW6_RTOS_PackageProgress(PS_PACKAGE_WORKFLOW_VALIDATING);
  tick += 9;
  g_ps_package_workflow_probe.terminal_event = event;
  return TX_SUCCESS;
}
static UINT PS_HW6_RTOS_RunStorageUsbExportRequest(void)
{ return storage_work(PS_UI_ROUTER_EVENT_MSC_ACTIVE); }
static UINT PS_HW6_RTOS_RunStorageUsbReclaimRequest(void)
{ return storage_work(PS_UI_ROUTER_EVENT_MSC_DONE); }
static UINT PS_HW6_RTOS_RunStoragePackageScanRequest(void)
{ return storage_work(PS_UI_ROUTER_EVENT_PACKAGE_VALID_FOUND); }
static UINT PS_HW6_RTOS_RunStoragePackageInstallStubRequest(void)
{ return storage_work(PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_DONE); }
void PS_HW6_TraceUiDispatch(uint32_t event, uint32_t from, uint32_t to, uint32_t status)
{ (void)from; (void)to; (void)status; last_event = event; }
static void PS_HW6_RTOS_SendCurrentUiRenderCommand(void) { renders++; }

#include "workflow_under_test.inc"

static void reset(void)
{
  memset((void *)&g_ps_package_workflow_probe, 0, sizeof(g_ps_package_workflow_probe));
  memset((void *)&g_ps_hw6_rtos_probe, 0, sizeof(g_ps_hw6_rtos_probe));
  PS_UIRouter_Init();
  memset(ps_queues, 0, sizeof(ps_queues));
  ps_package_workflow_notice_pending = ps_package_workflow_finished = 0;
  ps_package_launch_fault_pending = ps_package_validation_busy = 0;
  tick = 100; cpu_hz = 24000000; g_ps_hw6_clock_policy_probe.ospi_kernel_hz = 48000000;
  fail_owner = 99; display_result = HAL_OK; admission_result = TX_SUCCESS;
  prepare_count = storage_count = installer_errors = renders = last_event = 0;
  runtime_send_result = validation_wait_result = TX_SUCCESS;
}
static void deliver(uint32_t owner)
{
  ULONG message[4];
  assert(ps_queues[owner].count > 0);
  memcpy(message, ps_queues[owner].words[0], sizeof(message));
  ps_queues[owner].count--;
  memmove(ps_queues[owner].words, ps_queues[owner].words[1],
          ps_queues[owner].count * sizeof(message));
  PS_HW6_RTOS_PackageWorkflowHandleMessage(owner, message);
}
int main(void)
{
  ULONG stale[4];
  uint32_t action;
  static const uint8_t candidate[] = {1, 2};
  for (action = PS_UI_ROUTER_ACTION_MSC_ENTER; action <= PS_UI_ROUTER_ACTION_PACKAGE_LAUNCH; action++)
  {
    reset();
    assert(PS_HW6_RTOS_PackageWorkflowBegin(action) == TX_SUCCESS);
    assert(renders == 0 && prepare_count == 0 && storage_count == 0);
    assert(PS_HW6_RTOS_PackageWorkflowBegin(action) == TX_NOT_AVAILABLE);
    assert(g_ps_package_workflow_probe.duplicate_count == 1);
    deliver(PS_HW6_RTOS_OWNER_DISPLAY);
    assert(renders == 1 && prepare_count == 0 && storage_count == 0);
    deliver(PS_HW6_RTOS_OWNER_RUNTIME);
    if (action != PS_UI_ROUTER_ACTION_PACKAGE_LAUNCH) { deliver(PS_HW6_RTOS_OWNER_STORAGE); }
    assert(ps_package_workflow_finished && g_ps_package_workflow_probe.active);
    PS_HW6_RTOS_PackageWorkflowServiceUi();
    assert(!g_ps_package_workflow_probe.active && last_event != 0);
    if (action != PS_UI_ROUTER_ACTION_PACKAGE_LAUNCH)
    {
      assert(storage_count == 1);
      assert(g_ps_package_workflow_probe.phase_ticks[PS_PACKAGE_WORKFLOW_READING] == 17);
      assert(g_ps_package_workflow_probe.phase_ticks[PS_PACKAGE_WORKFLOW_VALIDATING] == 9);
      assert(g_ps_package_workflow_probe.phase_cpu_hz[PS_PACKAGE_WORKFLOW_READING] == cpu_hz);
    }
  }
  reset();
  fail_owner = PS_HW6_RTOS_OWNER_DISPLAY;
  assert(PS_HW6_RTOS_PackageWorkflowBegin(PS_UI_ROUTER_ACTION_PACKAGE_SCAN) == TX_QUEUE_ERROR);
  PS_HW6_RTOS_PackageWorkflowServiceUi();
  assert(!g_ps_package_workflow_probe.active && last_event == PS_UI_ROUTER_EVENT_PACKAGE_VALIDATE_ERROR);
  assert(!prepare_count && !storage_count);

  reset();
  display_result = HAL_ERROR;
  assert(PS_HW6_RTOS_PackageWorkflowBegin(PS_UI_ROUTER_ACTION_PACKAGE_SCAN) == TX_SUCCESS);
  deliver(PS_HW6_RTOS_OWNER_DISPLAY);
  PS_HW6_RTOS_PackageWorkflowServiceUi();
  assert(!g_ps_package_workflow_probe.active && !prepare_count && !storage_count);

  reset();
  assert(PS_HW6_RTOS_PackageWorkflowBegin(PS_UI_ROUTER_ACTION_PACKAGE_SCAN) == TX_SUCCESS);
  memcpy(stale, ps_queues[PS_HW6_RTOS_OWNER_DISPLAY].words[0], sizeof(stale));
  tick += PS_HW6_RTOS_OWNER_ACK_WAIT_TICKS;
  PS_HW6_RTOS_PackageWorkflowServiceUi();
  assert(!g_ps_package_workflow_probe.active && g_ps_package_workflow_probe.status == TX_NO_EVENTS);
  assert(PS_HW6_RTOS_PackageWorkflowBegin(PS_UI_ROUTER_ACTION_PACKAGE_SCAN) == TX_SUCCESS);
  PS_HW6_RTOS_PackageWorkflowHandleMessage(PS_HW6_RTOS_OWNER_DISPLAY, stale);
  assert(renders == 1 && !prepare_count && !storage_count);

  reset();
  assert(PS_HW6_RTOS_PackageWorkflowBegin(PS_UI_ROUTER_ACTION_PACKAGE_INSTALL_STUB) == TX_SUCCESS);
  deliver(PS_HW6_RTOS_OWNER_DISPLAY);
  fail_owner = PS_HW6_RTOS_OWNER_STORAGE;
  deliver(PS_HW6_RTOS_OWNER_RUNTIME);
  PS_HW6_RTOS_PackageWorkflowServiceUi();
  assert(installer_errors == 1 && !g_ps_package_workflow_probe.active && !storage_count);

  reset();
  fail_owner = PS_HW6_RTOS_OWNER_UI;
  assert(PS_HW6_RTOS_PackageWorkflowBegin(PS_UI_ROUTER_ACTION_PACKAGE_SCAN) == TX_SUCCESS);
  deliver(PS_HW6_RTOS_OWNER_DISPLAY);
  deliver(PS_HW6_RTOS_OWNER_RUNTIME);
  deliver(PS_HW6_RTOS_OWNER_STORAGE);
  PS_HW6_RTOS_PackageWorkflowServiceUi();
  assert(!g_ps_package_workflow_probe.active && last_event == PS_UI_ROUTER_EVENT_PACKAGE_VALID_FOUND);

  reset();
  runtime_send_result = TX_QUEUE_ERROR;
  assert(PS_HW6_RTOS_ValidatePackage(candidate, sizeof(candidate)) != 0);
  assert(!PS_HW6_RTOS_PackageValidationBusy());
  runtime_send_result = TX_SUCCESS; validation_wait_result = TX_NO_EVENTS;
  assert(PS_HW6_RTOS_ValidatePackage(candidate, sizeof(candidate)) != 0);
  assert(PS_HW6_RTOS_PackageValidationBusy());
  assert(ps_package_validation_blob == candidate && ps_package_validation_size == sizeof(candidate));
  assert(PS_HW6_RTOS_PackageWorkflowBegin(PS_UI_ROUTER_ACTION_PACKAGE_SCAN) == TX_NOT_AVAILABLE);
  ps_package_validation_busy = 0; validation_wait_result = TX_SUCCESS;
  assert(PS_HW6_RTOS_ValidatePackage(candidate, sizeof(candidate)) == 0);
  assert(!PS_HW6_RTOS_PackageValidationBusy());

  reset();
  ps_package_launch_fault_pending = 1;
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_LOW_BATTERY_BOOT_BLOCK) == PS_STATUS_OK);
  last_event = 0;
  PS_HW6_RTOS_PackageWorkflowServiceUi();
  assert(last_event == 0 && ps_package_launch_fault_pending);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_RECOVER_OK) == PS_STATUS_OK);
  PS_HW6_RTOS_PackageWorkflowServiceUi();
  assert(last_event == PS_UI_ROUTER_EVENT_PACKAGE_LAUNCH_ERROR && !ps_package_launch_fault_pending);
  assert(g_ps_ui_router_probe.current_page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER);
  assert(g_ps_ui_router_probe.package_state == PS_UI_ROUTER_PACKAGE_ERROR);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_INPUT_BTN_B) == PS_STATUS_OK);
  assert(g_ps_ui_router_probe.package_state == PS_UI_ROUTER_PACKAGE_NONE);
  assert(PS_UIRouter_Dispatch(PS_UI_ROUTER_EVENT_INPUT_BTN_A) == PS_STATUS_OK);
  assert(PS_UIRouter_TakeAction() == PS_UI_ROUTER_ACTION_MSC_ENTER);
  puts("workflow ordering, timeout, completion and reservation checks passed");
  return 0;
}
