#ifndef UI_ROUTER_H
#define UI_ROUTER_H

#include "tx_api.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_PAGE_HOME = 0U,
  UI_PAGE_MENU = 1U,
  UI_PAGE_JOY_CAL = 2U,
  UI_PAGE_BATT_STATS = 3U,
  UI_PAGE_COMMUNICATIONS = 4U,
  UI_PAGE_JOY_CURSOR = 5U,
  UI_PAGE_JOY_TARGET = 6U,
  UI_PAGE_LIS2 = 7U,
  UI_PAGE_LIS2_STEPS = 8U,
  UI_PAGE_MENU_INPUT = 9U,
  UI_PAGE_RTC_SET = 10U,
  UI_PAGE_SEED = 11U,
  UI_PAGE_SLEEP = 12U,
  UI_PAGE_SOUND = 13U,
  UI_PAGE_STORAGE = 14U,
  UI_PAGE_MENU_INPUT_SUB = 15U,
  UI_PAGE_MENU_SENSORS_SUB = 16U,
  UI_PAGE_MENU_POWER_TIME_SUB = 17U,
  UI_PAGE_MENU_SYSTEM_SUB = 18U,
  UI_PAGE_DISPLAY_STRESS = 19U,
  UI_PAGE_COUNT
} ui_page_id_t;

typedef enum
{
  UI_ACTION_NONE = 0U,
  UI_ACTION_CONFIRM = 1U,
  UI_ACTION_CANCEL = 2U,
  UI_ACTION_LEFT = 3U,
  UI_ACTION_RIGHT = 4U,
  UI_ACTION_MENU = 5U,
  UI_ACTION_UP = 6U,
  UI_ACTION_DOWN = 7U
} ui_action_id_t;

typedef enum
{
  UI_EVENT_PRESS = 1U,
  UI_EVENT_RELEASE = 2U,
  UI_EVENT_REPEAT = 3U,
  UI_EVENT_LONG = 4U
} ui_event_id_t;

typedef enum
{
  UI_JOY_CAL_STAGE_IDLE = 0U,
  UI_JOY_CAL_STAGE_NEUTRAL = 1U,
  UI_JOY_CAL_STAGE_UP = 2U,
  UI_JOY_CAL_STAGE_RIGHT = 3U,
  UI_JOY_CAL_STAGE_DOWN = 4U,
  UI_JOY_CAL_STAGE_LEFT = 5U,
  UI_JOY_CAL_STAGE_SWEEP = 6U,
  UI_JOY_CAL_STAGE_DONE = 7U,
  UI_JOY_CAL_STAGE_ERROR = 8U
} ui_joy_cal_stage_t;

typedef enum
{
  UI_JOY_DIR_NEUTRAL = 0U,
  UI_JOY_DIR_RIGHT = 1U,
  UI_JOY_DIR_UPRIGHT = 2U,
  UI_JOY_DIR_UP = 3U,
  UI_JOY_DIR_UPLEFT = 4U,
  UI_JOY_DIR_LEFT = 5U,
  UI_JOY_DIR_DOWNLEFT = 6U,
  UI_JOY_DIR_DOWN = 7U,
  UI_JOY_DIR_DOWNRIGHT = 8U
} ui_joy_dir_t;

typedef struct
{
  ULONG action;
  ULONG source;
  ULONG event;
  ULONG tick;
  ULONG pressed_mask;
} ui_action_evt_t;

typedef struct
{
  ULONG stage;
  float progress;
  LONG last_error;
  ULONG save_pending;
  ULONG save_ok_count;
  ULONG save_fail_count;
  ULONG load_ok_count;
  ULONG load_fail_count;
} ui_joy_cal_status_t;

typedef struct
{
  ULONG dir;
  ULONG input_mask;
  ULONG deadzone_enabled;
  ULONG invert_x;
  ULONG invert_y;
  float nx;
  float ny;
  float r_abs_mT;
  float center_x_mT;
  float center_y_mT;
  float span_x_mT;
  float span_y_mT;
  float rotation_deg;
  float threshold_x_mT;
  float threshold_y_mT;
  float deadzone_mT;
} ui_joy_live_status_t;

typedef struct
{
  ULONG fsm_state;
  ULONG fsm_fail_count;
  ULONG fsm_recovery_attempts;
  LONG fsm_last_error;
  ULONG stream_enabled;
  ULONG profile_requested;
  ULONG profile_applied;
  ULONG addr;
  ULONG whoami;
  ULONG status;
  ULONG sample_count;
  ULONG fail_count;
  ULONG last_sample_tick;
  LONG last_error;
  LONG x_raw;
  LONG y_raw;
  LONG z_raw;
  ULONG step_enabled;
  ULONG step_count;
  ULONG step_detected;
  ULONG tilt_detected;
  ULONG sigmot_detected;
} ui_lis_live_status_t;

typedef struct
{
  ULONG fsm_state;
  ULONG fsm_fail_count;
  ULONG fsm_recovery_attempts;
  LONG fsm_last_error;
  ULONG sample_count;
  ULONG fail_count;
  ULONG last_sample_tick;
  LONG last_error;
  LONG last_transport_error;
  ULONG charging_enabled_cfg;
  ULONG charging_active;
  ULONG battery_soc_percent;
  ULONG battery_soc_raw;
  ULONG battery_health_state;
  ULONG battery_health_reason;
  ULONG charger_state;
  ULONG battery_uv;
  ULONG battery_ov;
  ULONG vbat_mV;
  ULONG vbat_raw;
  ULONG fault_raw;
  ULONG status2_raw;
  ULONG pgood_raw;
} ui_pmic_live_status_t;

typedef struct
{
  ULONG valid;
  ULONG quality_ok;
  float span_ratio;
  float axis_error;
  float dir_norm_min;
  float dir_norm_max;
} ui_joy_cal_quality_t;

typedef struct
{
  ULONG mode_flags;
  ULONG last_input_source;
  ULONG last_input_action;
  ULONG last_input_edge;
  ui_joy_cal_status_t joy_cal_status;
  ULONG joy_cal_active;
  ui_joy_cal_quality_t joy_cal_quality;
  ui_joy_live_status_t joy_live;
  ui_pmic_live_status_t pmic_live;
  ui_lis_live_status_t lis_live;
} ui_router_state_t;

typedef struct
{
  UINT (*joy_cal_start)(void);
  UINT (*joy_cal_save)(void);
  UINT (*joy_cal_cancel)(void);
} ui_router_api_t;

typedef struct ui_page_vtable_s
{
  const char *name;
  void (*on_enter)(void);
  uint8_t (*on_action)(const ui_action_evt_t *evt);
  void (*on_tick)(void);
  void (*on_render)(void);
  void (*on_exit)(void);
} ui_page_vtable_t;

void UiRouter_Init(const ui_router_api_t *api);
void UiRouter_UpdateState(const ui_router_state_t *state);
void UiRouter_Tick(void);
uint8_t UiRouter_HandleAction(const ui_action_evt_t *evt);
void UiRouter_Render(void);
void UiRouter_RequestPage(ui_page_id_t page);
ui_page_id_t UiRouter_GetCurrentPage(void);
uint8_t UiRouter_IsDirty(void);
void UiRouter_ClearDirty(void);
void UiRouter_MarkDirty(void);
const ui_router_state_t *UiRouter_GetState(void);
const ui_router_api_t *UiRouter_GetApi(void);
void UiRouter_RenderFooterHints(const char *line1, const char *line2);
void UiRouter_RenderInputMonitor(uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif

#endif
