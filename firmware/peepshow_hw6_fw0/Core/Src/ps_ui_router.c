#include "ps_ui_router.h"

#include "ps_hw6_trace.h"

typedef struct
{
  uint32_t current_page;
  uint32_t previous_page;
  uint32_t requested_page;
  uint32_t nav_state;
  uint32_t modal_state;
  uint32_t calibration_page;
  uint32_t focus_index;
  uint32_t shutdown_state;
  uint32_t shutdown_countdown_seconds;
  uint32_t shutdown_event_count;
  uint32_t shutdown_return_page;
  uint32_t package_state;
  uint32_t package_event_count;
  uint32_t eggless;
  uint32_t last_button_event;
  uint32_t button_event_count;
  uint32_t pending_action;
  uint32_t last_action;
  uint32_t action_request_count;
  uint32_t action_take_count;
  uint32_t last_event;
  uint32_t transition_count;
  uint32_t rejected_event_count;
  uint32_t last_status;
} ps_ui_router_state_t;

volatile ps_ui_router_probe_t g_ps_ui_router_probe;
volatile uint32_t g_ps_ui_router_request;
volatile uint32_t g_ps_ui_router_request_event;

static ps_ui_router_state_t ps_ui_router_state;

static void PS_UIRouter_UpdateProbe(void)
{
  g_ps_ui_router_probe.api_version = PS_UI_ROUTER_API_VERSION;
  g_ps_ui_router_probe.current_page = ps_ui_router_state.current_page;
  g_ps_ui_router_probe.previous_page = ps_ui_router_state.previous_page;
  g_ps_ui_router_probe.requested_page = ps_ui_router_state.requested_page;
  g_ps_ui_router_probe.nav_state = ps_ui_router_state.nav_state;
  g_ps_ui_router_probe.modal_state = ps_ui_router_state.modal_state;
  g_ps_ui_router_probe.calibration_page =
    ps_ui_router_state.calibration_page;
  g_ps_ui_router_probe.focus_index = ps_ui_router_state.focus_index;
  g_ps_ui_router_probe.shutdown_state =
    ps_ui_router_state.shutdown_state;
  g_ps_ui_router_probe.shutdown_countdown_seconds =
    ps_ui_router_state.shutdown_countdown_seconds;
  g_ps_ui_router_probe.shutdown_event_count =
    ps_ui_router_state.shutdown_event_count;
  g_ps_ui_router_probe.shutdown_return_page =
    ps_ui_router_state.shutdown_return_page;
  g_ps_ui_router_probe.package_state = ps_ui_router_state.package_state;
  g_ps_ui_router_probe.package_event_count =
    ps_ui_router_state.package_event_count;
  g_ps_ui_router_probe.eggless = ps_ui_router_state.eggless;
  g_ps_ui_router_probe.last_button_event =
    ps_ui_router_state.last_button_event;
  g_ps_ui_router_probe.button_event_count =
    ps_ui_router_state.button_event_count;
  g_ps_ui_router_probe.pending_action =
    ps_ui_router_state.pending_action;
  g_ps_ui_router_probe.last_action = ps_ui_router_state.last_action;
  g_ps_ui_router_probe.action_request_count =
    ps_ui_router_state.action_request_count;
  g_ps_ui_router_probe.action_take_count =
    ps_ui_router_state.action_take_count;
  g_ps_ui_router_probe.last_event = ps_ui_router_state.last_event;
  g_ps_ui_router_probe.transition_count =
    ps_ui_router_state.transition_count;
  g_ps_ui_router_probe.rejected_event_count =
    ps_ui_router_state.rejected_event_count;
  g_ps_ui_router_probe.last_status = ps_ui_router_state.last_status;
}

static uint32_t PS_UIRouter_CanNavigate(void)
{
  if ((ps_ui_router_state.nav_state == PS_UI_ROUTER_NAV_TEXT_ENTRY) ||
      (ps_ui_router_state.nav_state == PS_UI_ROUTER_NAV_NUMERIC_ENTRY) ||
      (ps_ui_router_state.nav_state == PS_UI_ROUTER_NAV_MODAL_LOCK) ||
      (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_ERROR))
  {
    return 0UL;
  }
  return 1UL;
}

static ps_status_t PS_UIRouter_ShowShutdown(uint32_t shutdown_state,
                                           uint32_t countdown_seconds)
{
  if (ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_SHUTDOWN)
  {
    ps_ui_router_state.shutdown_return_page =
      ps_ui_router_state.current_page;
    ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
    ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_SHUTDOWN;
    ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_SHUTDOWN;
  }

  ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_MODAL_LOCK;
  ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_DIALOG;
  ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
  ps_ui_router_state.focus_index = 0UL;
  ps_ui_router_state.shutdown_state = shutdown_state;
  ps_ui_router_state.shutdown_countdown_seconds = countdown_seconds;
  ps_ui_router_state.shutdown_event_count++;
  ps_ui_router_state.transition_count++;
  return PS_STATUS_OK;
}

static ps_status_t PS_UIRouter_CancelShutdown(void)
{
  uint32_t return_page = ps_ui_router_state.shutdown_return_page;

  if ((ps_ui_router_state.shutdown_state ==
       PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_BOOT) ||
      (ps_ui_router_state.shutdown_state ==
       PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE))
  {
    return PS_STATUS_INVALID_STATE;
  }

  if ((return_page == PS_UI_ROUTER_PAGE_BOOTSTRAP) ||
      (return_page == PS_UI_ROUTER_PAGE_ERROR) ||
      (return_page == PS_UI_ROUTER_PAGE_SHUTDOWN))
  {
    return_page = PS_UI_ROUTER_PAGE_HOME;
  }

  ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
  ps_ui_router_state.current_page = return_page;
  ps_ui_router_state.requested_page = return_page;
  ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
  ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
  ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_CANCELLED;
  ps_ui_router_state.shutdown_countdown_seconds = 0UL;
  ps_ui_router_state.shutdown_event_count++;
  ps_ui_router_state.transition_count++;
  return PS_STATUS_OK;
}

static ps_status_t PS_UIRouter_ShowLowBatteryBootBlock(void)
{
  ps_status_t status = PS_UIRouter_ShowShutdown(
    PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_BOOT, 0UL);

  ps_ui_router_state.shutdown_return_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
  return status;
}

static ps_status_t PS_UIRouter_ShowLowBatteryChargeRecovery(void)
{
  ps_status_t status = PS_UIRouter_ShowShutdown(
    PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE, 0UL);

  ps_ui_router_state.shutdown_return_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
  return status;
}

static ps_status_t PS_UIRouter_GotoPage(uint32_t page)
{
  if (PS_UIRouter_CanNavigate() == 0UL)
  {
    return PS_STATUS_INVALID_STATE;
  }

  ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
  ps_ui_router_state.requested_page = page;
  ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_TRANSITION_LOCK;
  ps_ui_router_state.current_page = page;
  ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
  ps_ui_router_state.transition_count++;
  if ((page == PS_UI_ROUTER_PAGE_HOME) ||
      (page == PS_UI_ROUTER_PAGE_MENU))
  {
    ps_ui_router_state.focus_index = 1UL;
  }
  else
  {
    ps_ui_router_state.focus_index = 0UL;
  }
  if (page != PS_UI_ROUTER_PAGE_CALIBRATION)
  {
    ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
  }
  return PS_STATUS_OK;
}

static ps_status_t PS_UIRouter_StartJoystickCalibration(void)
{
  if (ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_CALIBRATION)
  {
    return PS_STATUS_INVALID_STATE;
  }
  ps_ui_router_state.calibration_page =
    PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL;
  ps_ui_router_state.transition_count++;
  return PS_STATUS_OK;
}

static ps_status_t PS_UIRouter_AdvanceJoystickCalibration(uint32_t from_page,
                                                          uint32_t to_page)
{
  if ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_CALIBRATION) ||
      (ps_ui_router_state.calibration_page != from_page))
  {
    return PS_STATUS_INVALID_STATE;
  }
  ps_ui_router_state.calibration_page = to_page;
  ps_ui_router_state.transition_count++;
  return PS_STATUS_OK;
}

static void PS_UIRouter_SetPackageState(uint32_t package_state)
{
  ps_ui_router_state.package_state = package_state;
  ps_ui_router_state.package_event_count++;
  ps_ui_router_state.transition_count++;
}

static ps_status_t PS_UIRouter_RequestAction(uint32_t action)
{
  if (action == (uint32_t)PS_UI_ROUTER_ACTION_NONE)
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (ps_ui_router_state.pending_action !=
      (uint32_t)PS_UI_ROUTER_ACTION_NONE)
  {
    return PS_STATUS_BUSY;
  }

  ps_ui_router_state.pending_action = action;
  ps_ui_router_state.last_action = action;
  ps_ui_router_state.action_request_count++;
  return PS_STATUS_OK;
}

static ps_status_t PS_UIRouter_DispatchButtonA(void)
{
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_HOME)
  {
    return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
  }
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_MENU)
  {
    if (ps_ui_router_state.focus_index == 0UL)
    {
      return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_SETTINGS);
    }
    if (ps_ui_router_state.focus_index == 1UL)
    {
      ps_status_t status = PS_UIRouter_GotoPage(
        PS_UI_ROUTER_PAGE_CALIBRATION);
      if (status == PS_STATUS_OK)
      {
        ps_ui_router_state.calibration_page =
          PS_UI_ROUTER_CAL_INPUT_ROOT;
      }
      return status;
    }
    return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_PACKAGE_BROWSER);
  }
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_CALIBRATION)
  {
    if (ps_ui_router_state.calibration_page == PS_UI_ROUTER_CAL_INPUT_ROOT)
    {
      return PS_UIRouter_StartJoystickCalibration();
    }
    if (ps_ui_router_state.calibration_page ==
        PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL)
    {
      return PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL,
        PS_UI_ROUTER_CAL_JOYSTICK_UP);
    }
    if (ps_ui_router_state.calibration_page ==
        PS_UI_ROUTER_CAL_JOYSTICK_UP)
    {
      return PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_UP,
        PS_UI_ROUTER_CAL_JOYSTICK_RIGHT);
    }
    if (ps_ui_router_state.calibration_page ==
        PS_UI_ROUTER_CAL_JOYSTICK_RIGHT)
    {
      return PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_RIGHT,
        PS_UI_ROUTER_CAL_JOYSTICK_DOWN);
    }
    if (ps_ui_router_state.calibration_page ==
        PS_UI_ROUTER_CAL_JOYSTICK_DOWN)
    {
      return PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_DOWN,
        PS_UI_ROUTER_CAL_JOYSTICK_LEFT);
    }
    if (ps_ui_router_state.calibration_page ==
        PS_UI_ROUTER_CAL_JOYSTICK_LEFT)
    {
      return PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_LEFT,
        PS_UI_ROUTER_CAL_JOYSTICK_SWEEP);
    }
    if (ps_ui_router_state.calibration_page ==
        PS_UI_ROUTER_CAL_JOYSTICK_SWEEP)
    {
      return PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_SWEEP,
        PS_UI_ROUTER_CAL_JOYSTICK_REVIEW);
    }
    if (ps_ui_router_state.calibration_page ==
        PS_UI_ROUTER_CAL_JOYSTICK_REVIEW)
    {
      return PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_REVIEW,
        PS_UI_ROUTER_CAL_INPUT_ROOT);
    }
  }
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER)
  {
    if (ps_ui_router_state.package_state == PS_UI_ROUTER_PACKAGE_INSTALLED)
    {
      return PS_UIRouter_RequestAction(
        PS_UI_ROUTER_ACTION_PACKAGE_LAUNCH);
    }
    if (ps_ui_router_state.package_state == PS_UI_ROUTER_PACKAGE_VALID)
    {
      ps_status_t status = PS_UIRouter_RequestAction(
        PS_UI_ROUTER_ACTION_PACKAGE_INSTALL_STUB);
      if (status == PS_STATUS_OK)
      {
        PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_INSTALLING);
      }
      return status;
    }
    if (ps_ui_router_state.package_state == PS_UI_ROUTER_PACKAGE_INSTALLING)
    {
      return PS_STATUS_BUSY;
    }
    return PS_UIRouter_RequestAction(PS_UI_ROUTER_ACTION_MSC_ENTER);
  }
  return PS_STATUS_INVALID_STATE;
}

static ps_status_t PS_UIRouter_DispatchButtonB(void)
{
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_HOME)
  {
    return PS_STATUS_OK;
  }
  if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_CALIBRATION) &&
      (ps_ui_router_state.calibration_page != PS_UI_ROUTER_CAL_INPUT_ROOT))
  {
    ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_INPUT_ROOT;
    ps_ui_router_state.transition_count++;
    return PS_STATUS_OK;
  }
  if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER) &&
      (ps_ui_router_state.package_state != PS_UI_ROUTER_PACKAGE_NONE))
  {
    if ((ps_ui_router_state.package_state == PS_UI_ROUTER_PACKAGE_VALID) ||
        (ps_ui_router_state.package_state == PS_UI_ROUTER_PACKAGE_INSTALLED) ||
        (ps_ui_router_state.package_state == PS_UI_ROUTER_PACKAGE_ERROR))
    {
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_NONE);
      return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
    }
    PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_NONE);
    return PS_STATUS_OK;
  }
  return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
}

static ps_status_t PS_UIRouter_DispatchButtonL(void)
{
  if ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_HOME) &&
      (ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_MENU))
  {
    return PS_STATUS_INVALID_STATE;
  }
  ps_ui_router_state.focus_index =
    (ps_ui_router_state.focus_index == 0UL) ? 2UL :
    (ps_ui_router_state.focus_index - 1UL);
  ps_ui_router_state.transition_count++;
  return PS_STATUS_OK;
}

static ps_status_t PS_UIRouter_DispatchButtonR(void)
{
  if ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_HOME) &&
      (ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_MENU))
  {
    return PS_STATUS_INVALID_STATE;
  }
  ps_ui_router_state.focus_index =
    (ps_ui_router_state.focus_index >= 2UL) ? 0UL :
    (ps_ui_router_state.focus_index + 1UL);
  ps_ui_router_state.transition_count++;
  return PS_STATUS_OK;
}

void PS_UIRouter_Init(void)
{
  ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
  ps_ui_router_state.previous_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
  ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
  ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_IDLE;
  ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
  ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
  ps_ui_router_state.focus_index = 0UL;
  ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
  ps_ui_router_state.shutdown_countdown_seconds = 0UL;
  ps_ui_router_state.shutdown_event_count = 0UL;
  ps_ui_router_state.shutdown_return_page = PS_UI_ROUTER_PAGE_HOME;
  ps_ui_router_state.package_state = PS_UI_ROUTER_PACKAGE_NONE;
  ps_ui_router_state.package_event_count = 0UL;
  ps_ui_router_state.eggless = 0UL;
  ps_ui_router_state.last_button_event = 0UL;
  ps_ui_router_state.button_event_count = 0UL;
  ps_ui_router_state.pending_action = PS_UI_ROUTER_ACTION_NONE;
  ps_ui_router_state.last_action = PS_UI_ROUTER_ACTION_NONE;
  ps_ui_router_state.action_request_count = 0UL;
  ps_ui_router_state.action_take_count = 0UL;
  ps_ui_router_state.last_event = 0UL;
  ps_ui_router_state.transition_count = 0UL;
  ps_ui_router_state.rejected_event_count = 0UL;
  ps_ui_router_state.last_status = PS_UI_ROUTER_STATUS_NOT_RUN;
  g_ps_ui_router_request = 0UL;
  g_ps_ui_router_request_event = 0UL;
  PS_UIRouter_UpdateProbe();
}

uint32_t PS_UIRouter_TakeAction(void)
{
  uint32_t action = ps_ui_router_state.pending_action;

  if (action != (uint32_t)PS_UI_ROUTER_ACTION_NONE)
  {
    ps_ui_router_state.pending_action = PS_UI_ROUTER_ACTION_NONE;
    ps_ui_router_state.action_take_count++;
    PS_UIRouter_UpdateProbe();
  }
  return action;
}

ps_status_t PS_UIRouter_Dispatch(uint32_t event)
{
  ps_status_t status = PS_STATUS_UNSUPPORTED;
  uint32_t entry_page = ps_ui_router_state.current_page;

  ps_ui_router_state.last_event = event;
  switch (event)
  {
    case PS_UI_ROUTER_EVENT_BOOT_COMPLETE:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_HOME);
      break;
    case PS_UI_ROUTER_EVENT_NAV_HOME:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_HOME);
      break;
    case PS_UI_ROUTER_EVENT_NAV_MENU:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
      break;
    case PS_UI_ROUTER_EVENT_NAV_SETTINGS:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_SETTINGS);
      break;
    case PS_UI_ROUTER_EVENT_NAV_CALIBRATION:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_CALIBRATION);
      if (status == PS_STATUS_OK)
      {
        ps_ui_router_state.calibration_page =
          PS_UI_ROUTER_CAL_INPUT_ROOT;
      }
      break;
    case PS_UI_ROUTER_EVENT_NAV_PACKAGES:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_PACKAGE_BROWSER);
      break;
    case PS_UI_ROUTER_EVENT_LAUNCH_RUNTIME:
      if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_HOME) ||
          (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_MENU) ||
          (ps_ui_router_state.current_page ==
           PS_UI_ROUTER_PAGE_PACKAGE_BROWSER))
      {
        status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF);
        if (status == PS_STATUS_OK)
        {
          if (ps_ui_router_state.package_state ==
              PS_UI_ROUTER_PACKAGE_INSTALLING)
          {
            PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_INSTALLED);
          }
          ps_ui_router_state.eggless = 0UL;
        }
      }
      else
      {
        status = PS_STATUS_INVALID_STATE;
      }
      break;
    case PS_UI_ROUTER_EVENT_RUNTIME_RETURNED:
      ps_ui_router_state.eggless = 0UL;
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_HOME);
      break;
    case PS_UI_ROUTER_EVENT_RUNTIME_UNAVAILABLE:
      ps_ui_router_state.eggless = 1UL;
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_HOME);
      break;
    case PS_UI_ROUTER_EVENT_SHELL_FAULT:
      ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_ERROR;
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_ERROR;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_MODAL_LOCK;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_DIALOG;
      ps_ui_router_state.transition_count++;
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_RECOVER_OK:
      if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_ERROR) ||
          ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_SHUTDOWN) &&
           ((ps_ui_router_state.shutdown_state ==
             PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_BOOT) ||
            (ps_ui_router_state.shutdown_state ==
             PS_UI_ROUTER_SHUTDOWN_LOW_BATTERY_CHARGE))))
      {
        ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
        ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_HOME;
        ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_HOME;
        ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
        ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
        ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
        ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
        ps_ui_router_state.shutdown_countdown_seconds = 0UL;
        ps_ui_router_state.transition_count++;
        status = PS_STATUS_OK;
      }
      else
      {
        status = PS_STATUS_INVALID_STATE;
      }
      break;

    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_START:
      status = PS_UIRouter_StartJoystickCalibration();
      break;
    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_NEUTRAL_ACCEPT:
      status = PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_NEUTRAL,
        PS_UI_ROUTER_CAL_JOYSTICK_UP);
      break;
    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_UP_ACCEPT:
      status = PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_UP,
        PS_UI_ROUTER_CAL_JOYSTICK_RIGHT);
      break;
    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_RIGHT_ACCEPT:
      status = PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_RIGHT,
        PS_UI_ROUTER_CAL_JOYSTICK_DOWN);
      break;
    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_DOWN_ACCEPT:
      status = PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_DOWN,
        PS_UI_ROUTER_CAL_JOYSTICK_LEFT);
      break;
    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_LEFT_ACCEPT:
      status = PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_LEFT,
        PS_UI_ROUTER_CAL_JOYSTICK_SWEEP);
      break;
    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_SWEEP_ACCEPT:
      status = PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_SWEEP,
        PS_UI_ROUTER_CAL_JOYSTICK_REVIEW);
      break;
    case PS_UI_ROUTER_EVENT_CAL_JOYSTICK_REVIEW_ACCEPT:
      status = PS_UIRouter_AdvanceJoystickCalibration(
        PS_UI_ROUTER_CAL_JOYSTICK_REVIEW,
        PS_UI_ROUTER_CAL_INPUT_ROOT);
      break;
    case PS_UI_ROUTER_EVENT_PAGE_TRANSITION_END:
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_SHUTDOWN_PREP:
      status = PS_UIRouter_ShowShutdown(PS_UI_ROUTER_SHUTDOWN_PREP, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_SHUTDOWN_WARNING:
      status = PS_UIRouter_ShowShutdown(PS_UI_ROUTER_SHUTDOWN_WARNING, 3UL);
      break;
    case PS_UI_ROUTER_EVENT_SHUTDOWN_IMMINENT:
      status = PS_UIRouter_ShowShutdown(PS_UI_ROUTER_SHUTDOWN_IMMINENT, 1UL);
      break;
    case PS_UI_ROUTER_EVENT_SHUTDOWN_CANCEL:
      status = PS_UIRouter_CancelShutdown();
      break;
    case PS_UI_ROUTER_EVENT_LOW_BATTERY_BOOT_BLOCK:
      status = PS_UIRouter_ShowLowBatteryBootBlock();
      break;
    case PS_UI_ROUTER_EVENT_LOW_BATTERY_CHARGE_RECOVERY:
      status = PS_UIRouter_ShowLowBatteryChargeRecovery();
      break;
    case PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_REST:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_REST, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_SWEEP:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_SWEEP, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_DONE:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_DONE, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_JOYSTICK_XYZ_ERROR:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_JOYSTICK_XYZ_ERROR, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_PACKAGE_CANDIDATE_FOUND:
      ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
      ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
      ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
      ps_ui_router_state.shutdown_countdown_seconds = 0UL;
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_CANDIDATE);
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_PACKAGE_VALID_FOUND:
      ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
      ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
      ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
      ps_ui_router_state.shutdown_countdown_seconds = 0UL;
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_VALID);
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_PACKAGE_VALIDATE_ERROR:
      ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_DIALOG;
      ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
      ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
      ps_ui_router_state.shutdown_countdown_seconds = 0UL;
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_ERROR);
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_DONE:
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_INSTALLED);
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_STUB_ERROR:
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_DIALOG;
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_ERROR);
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_PACKAGE_CLEAR:
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_NONE);
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_INPUT_BTN_A:
      ps_ui_router_state.last_button_event = event;
      ps_ui_router_state.button_event_count++;
      status = PS_UIRouter_DispatchButtonA();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_BTN_B:
      ps_ui_router_state.last_button_event = event;
      ps_ui_router_state.button_event_count++;
      status = PS_UIRouter_DispatchButtonB();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_BTN_L:
      ps_ui_router_state.last_button_event = event;
      ps_ui_router_state.button_event_count++;
      status = PS_UIRouter_DispatchButtonL();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_BTN_R:
      ps_ui_router_state.last_button_event = event;
      ps_ui_router_state.button_event_count++;
      status = PS_UIRouter_DispatchButtonR();
      break;
    default:
      status = PS_STATUS_UNSUPPORTED;
      break;
  }

  if (status != PS_STATUS_OK)
  {
    ps_ui_router_state.rejected_event_count++;
  }
  ps_ui_router_state.last_status = (uint32_t)status;
  PS_HW6_TraceUiDispatch(event,
                         entry_page,
                         ps_ui_router_state.current_page,
                         (uint32_t)status);
  PS_UIRouter_UpdateProbe();
  return status;
}

