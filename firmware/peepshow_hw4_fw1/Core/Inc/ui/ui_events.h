#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  UI_EVT_NONE = 0U,
  UI_EVT_UP = 1U,
  UI_EVT_DOWN = 2U,
  UI_EVT_LEFT = 3U,
  UI_EVT_RIGHT = 4U,
  UI_EVT_SELECT = 5U,
  UI_EVT_BACK = 6U,
  UI_EVT_TICK = 7U,
  UI_EVT_LONG_SELECT = 8U,
  UI_EVT_LONG_BACK = 9U
} ui_evt_t;

typedef enum
{
  UI_INPUT_SOURCE_UNKNOWN = 0U,
  UI_INPUT_SOURCE_BTN_A = 1U,
  UI_INPUT_SOURCE_BTN_B = 2U,
  UI_INPUT_SOURCE_BTN_L = 3U,
  UI_INPUT_SOURCE_BTN_R = 4U,
  UI_INPUT_SOURCE_BTN_BOOT = 5U,
  UI_INPUT_SOURCE_JOY_UP = 6U,
  UI_INPUT_SOURCE_JOY_RIGHT = 7U,
  UI_INPUT_SOURCE_JOY_DOWN = 8U,
  UI_INPUT_SOURCE_JOY_LEFT = 9U
} ui_input_source_t;

typedef enum
{
  UI_INPUT_ACTION_NONE = 0U,
  UI_INPUT_ACTION_BTN_A = 1U,
  UI_INPUT_ACTION_BTN_B = 2U,
  UI_INPUT_ACTION_BTN_L = 3U,
  UI_INPUT_ACTION_BTN_R = 4U,
  UI_INPUT_ACTION_BTN_BOOT = 5U,
  UI_INPUT_ACTION_JOY_UP = 6U,
  UI_INPUT_ACTION_JOY_RIGHT = 7U,
  UI_INPUT_ACTION_JOY_DOWN = 8U,
  UI_INPUT_ACTION_JOY_LEFT = 9U
} ui_input_action_t;

typedef enum
{
  UI_FOOTER_NONE = 0U,
  UI_FOOTER_A_SELECT_B_BACK = 1U,
  UI_FOOTER_A_START_B_BACK = 2U,
  UI_FOOTER_A_TOGGLE_B_BACK = 3U,
  UI_FOOTER_A_RUN_B_BACK = 4U
} ui_footer_t;

typedef enum
{
  UI_EVT_RESULT_NONE = 0U,
  UI_EVT_RESULT_HANDLED = (1UL << 0),
  UI_EVT_RESULT_DIRTY = (1UL << 1),
  UI_EVT_RESULT_REQUEST_EXIT = (1UL << 2),
  UI_EVT_RESULT_CONSUME_BACK = (1UL << 3)
} ui_evt_result_t;

typedef struct
{
  ui_evt_t evt;
  uint32_t action;
  uint32_t source;
  uint32_t tick;
  uint32_t pressed_mask;
} ui_input_evt_t;

#ifdef __cplusplus
}
#endif

#endif

