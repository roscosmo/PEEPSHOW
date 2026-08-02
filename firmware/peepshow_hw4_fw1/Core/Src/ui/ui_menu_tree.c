#include "ui/ui_menu_tree.h"

static const ui_menu_item_t s_home_items[] =
{
  {"START GAME", UI_PAGE_HOME},
  {"OPTIONS", UI_PAGE_MENU}
};

static const ui_menu_item_t s_system_menu_items[] =
{
  {"INPUT", UI_PAGE_MENU_INPUT_SUB},
  {"SENSORS", UI_PAGE_MENU_SENSORS_SUB},
  {"POWER/TIME", UI_PAGE_MENU_POWER_TIME_SUB},
  {"SYSTEM", UI_PAGE_MENU_SYSTEM_SUB},
  {"BACK", UI_PAGE_HOME}
};

static const ui_menu_item_t s_input_submenu_items[] =
{
  {"JOYSTICK CAL", UI_PAGE_JOY_CAL},
  {"MENU INPUT", UI_PAGE_MENU_INPUT},
  {"JOY CURSOR", UI_PAGE_JOY_CURSOR},
  {"JOY TARGET", UI_PAGE_JOY_TARGET},
  {"BACK", UI_PAGE_MENU}
};

static const ui_menu_item_t s_sensors_submenu_items[] =
{
  {"LIS2", UI_PAGE_LIS2},
  {"LIS2 STEPS", UI_PAGE_LIS2_STEPS},
  {"BACK", UI_PAGE_MENU}
};

static const ui_menu_item_t s_power_time_submenu_items[] =
{
  {"BATT STATS", UI_PAGE_BATT_STATS},
  {"RTC SET", UI_PAGE_RTC_SET},
  {"SLEEP", UI_PAGE_SLEEP},
  {"SEED", UI_PAGE_SEED},
  {"BACK", UI_PAGE_MENU}
};

static const ui_menu_item_t s_system_submenu_items[] =
{
  {"STORAGE", UI_PAGE_STORAGE},
  {"SOUND", UI_PAGE_SOUND},
  {"COMMUNICATIONS", UI_PAGE_COMMUNICATIONS},
  {"BACK", UI_PAGE_MENU}
};

const ui_menu_item_t *UiMenuTree_GetHomeItems(ULONG *count_out)
{
  if (count_out != TX_NULL)
  {
    *count_out = (ULONG)(sizeof(s_home_items) / sizeof(s_home_items[0]));
  }
  return s_home_items;
}

const ui_menu_item_t *UiMenuTree_GetSystemMenuItems(ULONG *count_out)
{
  if (count_out != TX_NULL)
  {
    *count_out = (ULONG)(sizeof(s_system_menu_items) / sizeof(s_system_menu_items[0]));
  }
  return s_system_menu_items;
}

const ui_menu_item_t *UiMenuTree_GetInputSubmenuItems(ULONG *count_out)
{
  if (count_out != TX_NULL)
  {
    *count_out = (ULONG)(sizeof(s_input_submenu_items) / sizeof(s_input_submenu_items[0]));
  }
  return s_input_submenu_items;
}

const ui_menu_item_t *UiMenuTree_GetSensorsSubmenuItems(ULONG *count_out)
{
  if (count_out != TX_NULL)
  {
    *count_out = (ULONG)(sizeof(s_sensors_submenu_items) / sizeof(s_sensors_submenu_items[0]));
  }
  return s_sensors_submenu_items;
}

const ui_menu_item_t *UiMenuTree_GetPowerTimeSubmenuItems(ULONG *count_out)
{
  if (count_out != TX_NULL)
  {
    *count_out = (ULONG)(sizeof(s_power_time_submenu_items) / sizeof(s_power_time_submenu_items[0]));
  }
  return s_power_time_submenu_items;
}

const ui_menu_item_t *UiMenuTree_GetSystemSubmenuItems(ULONG *count_out)
{
  if (count_out != TX_NULL)
  {
    *count_out = (ULONG)(sizeof(s_system_submenu_items) / sizeof(s_system_submenu_items[0]));
  }
  return s_system_submenu_items;
}
