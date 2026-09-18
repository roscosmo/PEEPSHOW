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
  uint32_t resume_available;
  uint32_t last_button_event;
  uint32_t button_event_count;
  uint32_t last_joystick_event;
  uint32_t joystick_event_count;
  uint32_t joystick_candidate_direction_mask;
  uint32_t joystick_resolved_direction_mask;
  uint32_t joystick_state_update_count;
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
volatile ps_ui_time_probe_t g_ps_ui_time_probe;

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
  g_ps_ui_router_probe.resume_available =
    ps_ui_router_state.resume_available;
  g_ps_ui_router_probe.last_button_event =
    ps_ui_router_state.last_button_event;
  g_ps_ui_router_probe.button_event_count =
    ps_ui_router_state.button_event_count;
  g_ps_ui_router_probe.last_joystick_event =
    ps_ui_router_state.last_joystick_event;
  g_ps_ui_router_probe.joystick_event_count =
    ps_ui_router_state.joystick_event_count;
  g_ps_ui_router_probe.joystick_candidate_direction_mask =
    ps_ui_router_state.joystick_candidate_direction_mask;
  g_ps_ui_router_probe.joystick_resolved_direction_mask =
    ps_ui_router_state.joystick_resolved_direction_mask;
  g_ps_ui_router_probe.joystick_state_update_count =
    ps_ui_router_state.joystick_state_update_count;
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
  if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_TIME) &&
      (g_ps_ui_time_probe.status == PS_UI_TIME_SAVING)) { return 0UL; }
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
    return_page = PS_UI_ROUTER_PAGE_MENU;
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
  if ((page == PS_UI_ROUTER_PAGE_TIME) && (g_ps_ui_time_probe.session == UINT32_MAX))
  { return PS_STATUS_INVALID_STATE; }
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
  if (page == PS_UI_ROUTER_PAGE_TIME)
  {
    g_ps_ui_time_probe.draft = (ps_system_datetime_t){2000U, 1U, 1U, 0U, 0U, 0U};
    g_ps_ui_time_probe.session++;
    g_ps_ui_time_probe.status = PS_UI_TIME_LOADING;
    g_ps_ui_time_probe.result_status = PS_UI_ROUTER_STATUS_NOT_RUN;
    g_ps_ui_time_probe.request = 1UL;
  }
  else { g_ps_ui_time_probe.request = 0UL; }
  return PS_STATUS_OK;
}

uint32_t PS_UIRouter_TakeTimeRequest(uint32_t *session, ps_system_datetime_t *local)
{
  uint32_t operation = g_ps_ui_time_probe.request;
  if ((session == 0) || (local == 0)) { return 0UL; }
  if ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_TIME) ||
      (operation == 0UL)) { return 0UL; }
  *session = g_ps_ui_time_probe.session;
  *local = g_ps_ui_time_probe.draft;
  g_ps_ui_time_probe.request = 0UL;
  return operation;
}

uint32_t PS_UIRouter_CompleteTimeRequest(uint32_t session, uint32_t operation,
  uint32_t status, const ps_system_datetime_t *local)
{
  uint32_t seconds;
  if ((session != g_ps_ui_time_probe.session) ||
      ((operation == 1UL) && (g_ps_ui_time_probe.status != PS_UI_TIME_LOADING)) ||
      ((operation == 2UL) && (g_ps_ui_time_probe.status != PS_UI_TIME_SAVING)) ||
      ((operation != 1UL) && (operation != 2UL))) { return 0UL; }
  if ((status == PS_SYSTEM_TIME_OK) && (PS_SystemTime_Encode(local, &seconds) != PS_SYSTEM_TIME_OK))
  { status = PS_SYSTEM_TIME_ARGUMENT; }
  g_ps_ui_time_probe.result_status = status;
  if (status == PS_SYSTEM_TIME_OK)
  {
    g_ps_ui_time_probe.draft = *local;
    g_ps_ui_time_probe.status = (operation == 1UL) ? PS_UI_TIME_EDIT : PS_UI_TIME_SAVED;
  }
  else if ((operation == 1UL) && ((status == PS_SYSTEM_TIME_UNSET) ||
           (status == PS_SYSTEM_TIME_SOURCE_LOST) || (status == PS_SYSTEM_TIME_RANGE)))
  { g_ps_ui_time_probe.status = PS_UI_TIME_UNSET; }
  else
  { g_ps_ui_time_probe.status = (operation == 1UL) ? PS_UI_TIME_READ_ERROR : PS_UI_TIME_SAVE_ERROR; }
  return (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_TIME) ? 1UL : 0UL;
}

static ps_status_t PS_UIRouter_TimeInput(uint32_t event)
{
  uint32_t field = ps_ui_router_state.focus_index;
  uint32_t value, min = 0UL, max = 59UL, seconds;
  uint32_t forward = (event == PS_UI_ROUTER_EVENT_INPUT_JOY_UP);
  ps_system_datetime_t local = g_ps_ui_time_probe.draft;
  if (g_ps_ui_time_probe.status == PS_UI_TIME_SAVING) { return PS_STATUS_BUSY; }
  if (event == PS_UI_ROUTER_EVENT_INPUT_BTN_B)
  { return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU); }
  if (g_ps_ui_time_probe.status == PS_UI_TIME_LOADING) { return PS_STATUS_BUSY; }
  if (g_ps_ui_time_probe.status == PS_UI_TIME_READ_ERROR)
  {
    if (event != PS_UI_ROUTER_EVENT_INPUT_BTN_A) { return PS_STATUS_INVALID_STATE; }
    g_ps_ui_time_probe.status = PS_UI_TIME_LOADING;
    g_ps_ui_time_probe.request = 1UL;
    return PS_STATUS_OK;
  }
  if (event == PS_UI_ROUTER_EVENT_INPUT_BTN_A)
  {
    if (field == 7UL) { return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU); }
    if (field == 6UL)
    {
      if (PS_SystemTime_Encode(&local, &seconds) != PS_SYSTEM_TIME_OK)
      { return PS_STATUS_INVALID_ARGUMENT; }
      g_ps_ui_time_probe.status = PS_UI_TIME_SAVING;
      g_ps_ui_time_probe.request = 2UL;
    }
    else { ps_ui_router_state.focus_index = field + 1UL; }
    return PS_STATUS_OK;
  }
  if ((event == PS_UI_ROUTER_EVENT_INPUT_JOY_LEFT) ||
      (event == PS_UI_ROUTER_EVENT_INPUT_BTN_L))
  { ps_ui_router_state.focus_index = (field == 0UL) ? 7UL : field - 1UL; return PS_STATUS_OK; }
  if ((event == PS_UI_ROUTER_EVENT_INPUT_JOY_RIGHT) ||
      (event == PS_UI_ROUTER_EVENT_INPUT_BTN_R))
  { ps_ui_router_state.focus_index = (field + 1UL) % 8UL; return PS_STATUS_OK; }
  if ((event != PS_UI_ROUTER_EVENT_INPUT_JOY_UP) &&
      (event != PS_UI_ROUTER_EVENT_INPUT_JOY_DOWN)) { return PS_STATUS_UNSUPPORTED; }
  if (field >= 6UL) { return PS_STATUS_OK; }
  switch (field)
  {
    case 0UL: value = local.year; min = 2000UL; max = 2099UL; break;
    case 1UL: value = local.month; min = 1UL; max = 12UL; break;
    case 2UL:
      value = local.day; min = 1UL; max = 31UL;
      local.day = 31U;
      for (uint32_t i = 0UL; i < 3UL; ++i)
      {
        if (PS_SystemTime_Encode(&local, &seconds) == PS_SYSTEM_TIME_OK) { break; }
        --local.day;
      }
      max = local.day;
      break;
    case 3UL: value = local.hour; max = 23UL; break;
    case 4UL: value = local.minute; break;
    default: value = local.second; break;
  }
  value = forward ? ((value == max) ? min : value + 1UL) : ((value == min) ? max : value - 1UL);
  switch (field)
  {
    case 0UL: local.year = (uint16_t)value; break;
    case 1UL: local.month = (uint8_t)value; break;
    case 2UL: local.day = (uint8_t)value; break;
    case 3UL: local.hour = (uint8_t)value; break;
    case 4UL: local.minute = (uint8_t)value; break;
    default: local.second = (uint8_t)value; break;
  }
  /* A month/year edit clamps the day; at most three decrements are needed. */
  for (uint32_t i = 0UL; i < 3UL; ++i)
  {
    if (PS_SystemTime_Encode(&local, &seconds) == PS_SYSTEM_TIME_OK) { break; }
    --local.day;
  }
  g_ps_ui_time_probe.draft = local;
  if (g_ps_ui_time_probe.status != PS_UI_TIME_UNSET) { g_ps_ui_time_probe.status = PS_UI_TIME_EDIT; }
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
    if (ps_ui_router_state.resume_available != 0UL)
    {
      if (ps_ui_router_state.focus_index == 0UL)
      {
        return PS_UIRouter_RequestAction(
          PS_UI_ROUTER_ACTION_RUNTIME_RESUME);
      }
      if (ps_ui_router_state.focus_index == 1UL)
      {
        return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
      }
      return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_PACKAGE_BROWSER);
    }
    return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
  }
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_MENU)
  {
    if (ps_ui_router_state.focus_index == 0UL)
    {
      return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_TIME);
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
    {
      return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_PACKAGE_BROWSER);
    }
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
    if (ps_ui_router_state.package_state == PS_UI_ROUTER_PACKAGE_NONE)
    {
      if (ps_ui_router_state.focus_index == 0UL)
      {
        return PS_UIRouter_RequestAction(PS_UI_ROUTER_ACTION_MSC_ENTER);
      }
      if (ps_ui_router_state.focus_index == 1UL)
      {
        ps_status_t status = PS_UIRouter_RequestAction(
          PS_UI_ROUTER_ACTION_PACKAGE_SCAN);
        if (status == PS_STATUS_OK)
        {
          PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_CANDIDATE);
        }
        return status;
      }
      return PS_STATUS_INVALID_STATE;
    }
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
    return PS_STATUS_INVALID_STATE;
  }
  return PS_STATUS_INVALID_STATE;
}

static ps_status_t PS_UIRouter_DispatchButtonB(void)
{
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_HOME)
  {
    if (ps_ui_router_state.resume_available != 0UL)
    {
      return PS_UIRouter_RequestAction(
        PS_UI_ROUTER_ACTION_RUNTIME_RESUME);
    }
    return PS_STATUS_OK;
  }
  if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_SHUTDOWN) &&
      ((ps_ui_router_state.shutdown_state ==
        PS_UI_ROUTER_SHUTDOWN_MSC_ERROR) ||
       (ps_ui_router_state.shutdown_state ==
        PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY)))
  {
    return PS_UIRouter_CancelShutdown();
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
      return PS_STATUS_OK;
    }
    PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_NONE);
    return PS_STATUS_OK;
  }
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER)
  {
    return PS_UIRouter_GotoPage(
      (ps_ui_router_state.resume_available != 0UL) ?
      PS_UI_ROUTER_PAGE_HOME : PS_UI_ROUTER_PAGE_MENU);
  }
  if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_MENU) &&
      (ps_ui_router_state.resume_available != 0UL))
  {
    return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_HOME);
  }
  return PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
}

static ps_status_t PS_UIRouter_DispatchButtonL(void)
{
  if ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_HOME) &&
      (ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_MENU) &&
      ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_PACKAGE_BROWSER) ||
       (ps_ui_router_state.package_state != PS_UI_ROUTER_PACKAGE_NONE)))
  {
    return PS_STATUS_INVALID_STATE;
  }
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER)
  {
    ps_ui_router_state.focus_index =
      (ps_ui_router_state.focus_index == 0UL) ? 1UL : 0UL;
  }
  else
  {
    ps_ui_router_state.focus_index =
      (ps_ui_router_state.focus_index == 0UL) ? 2UL :
      (ps_ui_router_state.focus_index - 1UL);
  }
  ps_ui_router_state.transition_count++;
  return PS_STATUS_OK;
}

static ps_status_t PS_UIRouter_DispatchButtonR(void)
{
  if ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_HOME) &&
      (ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_MENU) &&
      ((ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_PACKAGE_BROWSER) ||
       (ps_ui_router_state.package_state != PS_UI_ROUTER_PACKAGE_NONE)))
  {
    return PS_STATUS_INVALID_STATE;
  }
  if (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_PACKAGE_BROWSER)
  {
    ps_ui_router_state.focus_index =
      (ps_ui_router_state.focus_index == 0UL) ? 1UL : 0UL;
  }
  else
  {
    ps_ui_router_state.focus_index =
      (ps_ui_router_state.focus_index >= 2UL) ? 0UL :
      (ps_ui_router_state.focus_index + 1UL);
  }
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
  ps_ui_router_state.shutdown_return_page = PS_UI_ROUTER_PAGE_MENU;
  ps_ui_router_state.package_state = PS_UI_ROUTER_PACKAGE_NONE;
  ps_ui_router_state.package_event_count = 0UL;
  ps_ui_router_state.eggless = 0UL;
  ps_ui_router_state.resume_available = 0UL;
  ps_ui_router_state.last_button_event = 0UL;
  ps_ui_router_state.button_event_count = 0UL;
  ps_ui_router_state.last_joystick_event = 0UL;
  ps_ui_router_state.joystick_event_count = 0UL;
  ps_ui_router_state.joystick_candidate_direction_mask = 0UL;
  ps_ui_router_state.joystick_resolved_direction_mask = 0UL;
  ps_ui_router_state.joystick_state_update_count = 0UL;
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
  g_ps_ui_time_probe = (ps_ui_time_probe_t){0};
  PS_UIRouter_UpdateProbe();
}

ps_status_t PS_UIRouter_RecordJoystickState(
  uint32_t candidate_direction_mask,
  uint32_t resolved_direction_mask)
{
  if (((candidate_direction_mask & ~0x0FUL) != 0UL) ||
      ((candidate_direction_mask & 0x03UL) == 0x03UL) ||
      ((candidate_direction_mask & 0x0CUL) == 0x0CUL) ||
      ((resolved_direction_mask != 0UL) &&
       ((resolved_direction_mask & (resolved_direction_mask - 1UL)) != 0UL)) ||
      ((resolved_direction_mask & ~0x0FUL) != 0UL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  ps_ui_router_state.joystick_candidate_direction_mask =
    candidate_direction_mask;
  ps_ui_router_state.joystick_resolved_direction_mask =
    resolved_direction_mask;
  ps_ui_router_state.joystick_state_update_count++;
  PS_UIRouter_UpdateProbe();
  return PS_STATUS_OK;
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
  if ((entry_page == PS_UI_ROUTER_PAGE_TIME) &&
      (((event >= PS_UI_ROUTER_EVENT_INPUT_BTN_A) && (event <= PS_UI_ROUTER_EVENT_INPUT_BTN_R)) ||
       ((event >= PS_UI_ROUTER_EVENT_INPUT_JOY_LEFT) && (event <= PS_UI_ROUTER_EVENT_INPUT_JOY_DOWN))))
  {
    status = PS_UIRouter_TimeInput(event);
    ps_ui_router_state.last_status = status;
    if (status == PS_STATUS_OK) { ps_ui_router_state.transition_count++; }
    else { ps_ui_router_state.rejected_event_count++; }
    PS_UIRouter_UpdateProbe();
    return status;
  }
  switch (event)
  {
    case PS_UI_ROUTER_EVENT_BOOT_COMPLETE:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
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
    case PS_UI_ROUTER_EVENT_NAV_TIME:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_TIME);
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
    case PS_UI_ROUTER_EVENT_NAV_INPUT_DIAGNOSTIC:
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC);
      break;
    case PS_UI_ROUTER_EVENT_LAUNCH_RUNTIME:
      if (((ps_ui_router_state.shutdown_state ==
            PS_UI_ROUTER_SHUTDOWN_NONE) ||
           (ps_ui_router_state.shutdown_state ==
            PS_UI_ROUTER_SHUTDOWN_CANCELLED)) &&
          ((ps_ui_router_state.current_page ==
            PS_UI_ROUTER_PAGE_BOOTSTRAP) ||
          (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_HOME) ||
          (ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_MENU) ||
          (ps_ui_router_state.current_page ==
           PS_UI_ROUTER_PAGE_PACKAGE_BROWSER)))
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
          ps_ui_router_state.resume_available = 0UL;
        }
      }
      else
      {
        status = PS_STATUS_INVALID_STATE;
      }
      break;
    case PS_UI_ROUTER_EVENT_RUNTIME_RETURNED:
      ps_ui_router_state.eggless = 0UL;
      ps_ui_router_state.resume_available = 0UL;
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
      break;
    case PS_UI_ROUTER_EVENT_RUNTIME_UNAVAILABLE:
      ps_ui_router_state.eggless = 1UL;
      ps_ui_router_state.resume_available = 0UL;
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
      break;
    case PS_UI_ROUTER_EVENT_SYSTEM_MENU_REQUEST:
      status = PS_UIRouter_RequestAction(
        PS_UI_ROUTER_ACTION_SYSTEM_MENU_ENTER);
      break;
    case PS_UI_ROUTER_EVENT_SYSTEM_MENU_ACTIVE:
      ps_ui_router_state.resume_available = 1UL;
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_HOME);
      if (status == PS_STATUS_OK)
      {
        ps_ui_router_state.focus_index = 0UL;
      }
      break;
    case PS_UI_ROUTER_EVENT_SYSTEM_MENU_SHELL:
      ps_ui_router_state.resume_available = 0UL;
      status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
      break;
    case PS_UI_ROUTER_EVENT_SYSTEM_MENU_DISCARD:
      ps_ui_router_state.resume_available = 0UL;
      ps_ui_router_state.transition_count++;
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_SYSTEM_MENU_RESUMED:
      if (ps_ui_router_state.resume_available != 0UL)
      {
        ps_ui_router_state.resume_available = 0UL;
        status = PS_UIRouter_GotoPage(
          PS_UI_ROUTER_PAGE_RUNTIME_HANDOFF);
      }
      else
      {
        status = PS_STATUS_INVALID_STATE;
      }
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
        ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
        ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_BOOTSTRAP;
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
      if ((ps_ui_router_state.current_page ==
           PS_UI_ROUTER_PAGE_CALIBRATION) &&
          (ps_ui_router_state.calibration_page ==
           PS_UI_ROUTER_CAL_JOYSTICK_REVIEW))
      {
        status = PS_UIRouter_GotoPage(PS_UI_ROUTER_PAGE_MENU);
      }
      else
      {
        status = PS_STATUS_INVALID_STATE;
      }
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
    case PS_UI_ROUTER_EVENT_MSC_EXPORT:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_MSC_EXPORT, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_MSC_ACTIVE:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_MSC_ACTIVE, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_MSC_RECLAIM:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_MSC_RECLAIM, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_MSC_DONE:
      if ((ps_ui_router_state.current_page == PS_UI_ROUTER_PAGE_SHUTDOWN) &&
          (ps_ui_router_state.shutdown_state >=
           PS_UI_ROUTER_SHUTDOWN_MSC_EXPORT) &&
          (ps_ui_router_state.shutdown_state <=
           PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY))
      {
        status = PS_UIRouter_CancelShutdown();
        if (status == PS_STATUS_OK)
        {
          ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
        }
      }
      else
      {
        status = PS_STATUS_OK;
      }
      break;
    case PS_UI_ROUTER_EVENT_MSC_ERROR:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_MSC_ERROR, 0UL);
      break;
    case PS_UI_ROUTER_EVENT_MSC_RECOVERY:
      status = PS_UIRouter_ShowShutdown(
        PS_UI_ROUTER_SHUTDOWN_MSC_RECOVERY, 0UL);
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
    case PS_UI_ROUTER_EVENT_PACKAGE_LAUNCH_ERROR:
      if (PS_UIRouter_CanNavigate() == 0UL)
      {
        status = PS_STATUS_INVALID_STATE;
        break;
      }
      if (event == PS_UI_ROUTER_EVENT_PACKAGE_LAUNCH_ERROR)
      {
        ps_ui_router_state.eggless = 1UL;
        ps_ui_router_state.resume_available = 0UL;
        ps_ui_router_state.pending_action = PS_UI_ROUTER_ACTION_NONE;
      }
      ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
      ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
      ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
      ps_ui_router_state.shutdown_countdown_seconds = 0UL;
      ps_ui_router_state.focus_index = 0UL;
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_ERROR);
      status = PS_STATUS_OK;
      break;
    case PS_UI_ROUTER_EVENT_PACKAGE_INSTALL_BEGIN:
      ps_ui_router_state.previous_page = ps_ui_router_state.current_page;
      ps_ui_router_state.current_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.requested_page = PS_UI_ROUTER_PAGE_PACKAGE_BROWSER;
      ps_ui_router_state.nav_state = PS_UI_ROUTER_NAV_FOCUS;
      ps_ui_router_state.modal_state = PS_UI_ROUTER_MODAL_NONE;
      ps_ui_router_state.calibration_page = PS_UI_ROUTER_CAL_NONE;
      ps_ui_router_state.shutdown_state = PS_UI_ROUTER_SHUTDOWN_NONE;
      ps_ui_router_state.shutdown_countdown_seconds = 0UL;
      PS_UIRouter_SetPackageState(PS_UI_ROUTER_PACKAGE_INSTALLING);
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
      status = (ps_ui_router_state.current_page ==
                PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC) ?
               PS_STATUS_OK : PS_UIRouter_DispatchButtonA();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_BTN_B:
      ps_ui_router_state.last_button_event = event;
      ps_ui_router_state.button_event_count++;
      status = (ps_ui_router_state.current_page ==
                PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC) ?
               PS_STATUS_OK : PS_UIRouter_DispatchButtonB();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_BTN_L:
      ps_ui_router_state.last_button_event = event;
      ps_ui_router_state.button_event_count++;
      status = (ps_ui_router_state.current_page ==
                PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC) ?
               PS_STATUS_OK : PS_UIRouter_DispatchButtonL();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_BTN_R:
      ps_ui_router_state.last_button_event = event;
      ps_ui_router_state.button_event_count++;
      status = (ps_ui_router_state.current_page ==
                PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC) ?
               PS_STATUS_OK : PS_UIRouter_DispatchButtonR();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_JOY_LEFT:
    case PS_UI_ROUTER_EVENT_INPUT_JOY_UP:
      ps_ui_router_state.last_joystick_event = event;
      ps_ui_router_state.joystick_event_count++;
      status = (ps_ui_router_state.current_page ==
                PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC) ?
               PS_STATUS_OK : PS_UIRouter_DispatchButtonL();
      break;
    case PS_UI_ROUTER_EVENT_INPUT_JOY_RIGHT:
    case PS_UI_ROUTER_EVENT_INPUT_JOY_DOWN:
      ps_ui_router_state.last_joystick_event = event;
      ps_ui_router_state.joystick_event_count++;
      status = (ps_ui_router_state.current_page ==
                PS_UI_ROUTER_PAGE_INPUT_DIAGNOSTIC) ?
               PS_STATUS_OK : PS_UIRouter_DispatchButtonR();
      break;
    default:
      status = PS_STATUS_UNSUPPORTED;
      break;
  }

  if ((entry_page == PS_UI_ROUTER_PAGE_TIME) &&
      (ps_ui_router_state.current_page != PS_UI_ROUTER_PAGE_TIME) &&
      (g_ps_ui_time_probe.request != 0UL))
  {
    g_ps_ui_time_probe.request = 0UL;
    g_ps_ui_time_probe.status = PS_UI_TIME_READ_ERROR;
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

