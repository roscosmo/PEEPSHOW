#ifndef UI_RUNTIME_CONTEXT_H
#define UI_RUNTIME_CONTEXT_H

#include "tx_api.h"
#include "ui/ui_events.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_ACTION_NONE = 0U,
  UI_ACTION_BTN_A = 1U,
  UI_ACTION_BTN_B = 2U,
  UI_ACTION_BTN_L = 3U,
  UI_ACTION_BTN_R = 4U,
  UI_ACTION_BTN_BOOT = 5U,
  UI_ACTION_JOY_UP = 6U,
  UI_ACTION_JOY_RIGHT = 7U,
  UI_ACTION_JOY_DOWN = 8U,
  UI_ACTION_JOY_LEFT = 9U
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
  ULONG sample_count;
  ULONG last_sample_tick;
  LONG last_error;
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
  ULONG fault_raw;
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
  ULONG user_master_pct;
  ULONG user_music_pct;
  ULONG user_sfx_pct;
  ULONG user_ui_pct;
} ui_audio_live_status_t;

typedef struct
{
  ULONG mode_flags;
  ULONG last_input_source;
  ULONG last_input_action;
  ULONG last_input_edge;
  ULONG pet_state;
  ULONG pet_tick_count;
  ULONG pet_wake_count;
  ULONG pet_last_action;
  ULONG pet_hunger_pct;
  ULONG pet_energy_pct;
  ULONG pet_mood_pct;
  ULONG stop_select_active;
  ULONG stop_select_last_input_tick;
  ui_joy_cal_status_t joy_cal_status;
  ULONG joy_cal_active;
  ui_joy_cal_quality_t joy_cal_quality;
  ui_joy_live_status_t joy_live;
  ui_audio_live_status_t audio_live;
  ui_pmic_live_status_t pmic_live;
  ui_lis_live_status_t lis_live;
} ui_router_state_t;

typedef struct
{
  UINT (*joy_cal_start)(void);
  UINT (*joy_cal_save)(void);
  UINT (*joy_cal_cancel)(void);
} ui_router_api_t;

void UiRuntimeContext_Init(const ui_router_api_t *api);
void UiRuntimeContext_UpdateState(const ui_router_state_t *state);
const ui_router_state_t *UiRuntimeContext_GetState(void);
const ui_router_api_t *UiRuntimeContext_GetApi(void);
void UiRuntimeContext_RenderFooterHints(const char *line1, const char *line2);
void UiRuntimeContext_RenderInputMonitor(uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif

#endif

