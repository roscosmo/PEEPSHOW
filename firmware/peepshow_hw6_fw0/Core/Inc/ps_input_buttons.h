#ifndef PS_INPUT_BUTTONS_H
#define PS_INPUT_BUTTONS_H

#include <stdint.h>

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_INPUT_BUTTONS_API_VERSION (10UL)
#define PS_INPUT_BUTTON_PHYSICAL_COUNT (4UL)

typedef enum
{
  PS_INPUT_BUTTON_ID_NONE = 0,
  PS_INPUT_BUTTON_ID_A,
  PS_INPUT_BUTTON_ID_B,
  PS_INPUT_BUTTON_ID_L,
  PS_INPUT_BUTTON_ID_R,
  PS_INPUT_BUTTON_ID_START
} ps_input_button_id_t;

typedef enum
{
  PS_INPUT_BUTTON_EVENT_NONE = 0,
  PS_INPUT_BUTTON_EVENT_PRESS,
  PS_INPUT_BUTTON_EVENT_RELEASE
} ps_input_button_event_t;

typedef enum
{
  PS_INPUT_BUTTON_LOGICAL_EVENT_NONE = 0,
  PS_INPUT_BUTTON_LOGICAL_EVENT_PRESS,
  PS_INPUT_BUTTON_LOGICAL_EVENT_RELEASE,
  PS_INPUT_BUTTON_LOGICAL_EVENT_LONG_PRESS,
  PS_INPUT_BUTTON_LOGICAL_EVENT_REPEAT,
  PS_INPUT_BUTTON_LOGICAL_EVENT_CHORD,
  PS_INPUT_BUTTON_LOGICAL_EVENT_STUCK
} ps_input_button_logical_event_t;

typedef enum
{
  PS_INPUT_BUTTON_STATE_DISABLED = 0,
  PS_INPUT_BUTTON_STATE_RELEASED,
  PS_INPUT_BUTTON_STATE_DEBOUNCE_PRESS,
  PS_INPUT_BUTTON_STATE_PRESSED,
  PS_INPUT_BUTTON_STATE_HELD,
  PS_INPUT_BUTTON_STATE_REPEAT,
  PS_INPUT_BUTTON_STATE_DEBOUNCE_RELEASE,
  PS_INPUT_BUTTON_STATE_STUCK,
  PS_INPUT_BUTTON_STATE_ERROR
} ps_input_button_state_t;

typedef enum
{
  PS_INPUT_START_STATE_IDLE = 0,
  PS_INPUT_START_STATE_NORMAL_PRESS,
  PS_INPUT_START_STATE_LONG_PRESS,
  PS_INPUT_START_STATE_SHIP_PREP,
  PS_INPUT_START_STATE_SHIP_WARNING,
  PS_INPUT_START_STATE_SHIP_IMMINENT,
  PS_INPUT_START_STATE_RELEASED
} ps_input_start_state_t;

typedef enum
{
  PS_INPUT_START_POWER_EVENT_NONE = 0,
  PS_INPUT_START_POWER_EVENT_SHIP_PREP,
  PS_INPUT_START_POWER_EVENT_SHIP_WARNING,
  PS_INPUT_START_POWER_EVENT_SHIP_IMMINENT,
  PS_INPUT_START_POWER_EVENT_RELEASED_BEFORE_SHIP
} ps_input_start_power_event_t;

typedef struct
{
  ps_input_button_logical_event_t event;
  ps_input_button_id_t button_id;
  uint32_t button_mask;
  uint32_t timestamp;
  uint32_t hold_ticks;
} ps_input_button_logical_record_t;

typedef struct
{
  uint32_t api_version;
  uint32_t isr_edge_count;
  uint32_t press_count;
  uint32_t ignored_edge_count;
  uint32_t pending_mask;
  uint32_t last_pin;
  uint32_t last_button_id;
  uint32_t last_event;
  uint32_t last_level;
  uint32_t last_tick;
  uint32_t button_state[PS_INPUT_BUTTON_PHYSICAL_COUNT];
  uint32_t button_raw_level[PS_INPUT_BUTTON_PHYSICAL_COUNT];
  uint32_t button_press_tick[PS_INPUT_BUTTON_PHYSICAL_COUNT];
  uint32_t button_release_tick[PS_INPUT_BUTTON_PHYSICAL_COUNT];
  uint32_t button_deadline_tick[PS_INPUT_BUTTON_PHYSICAL_COUNT];
  uint32_t button_debounce_press_ticks;
  uint32_t button_debounce_release_ticks;
  uint32_t button_long_press_ticks;
  uint32_t button_repeat_start_ticks;
  uint32_t button_repeat_period_ticks;
  uint32_t button_stuck_ticks;
  uint32_t button_chord_window_ticks;
  uint32_t button_debounce_press_count;
  uint32_t button_debounce_release_count;
  uint32_t button_press_accept_count;
  uint32_t button_release_accept_count;
  uint32_t button_long_count;
  uint32_t button_repeat_count;
  uint32_t button_stuck_count;
  uint32_t button_bounce_reject_count;
  uint32_t raw_edge_send_count;
  uint32_t raw_edge_drop_count;
  uint32_t raw_edge_process_count;
  uint32_t raw_edge_recovery_count;
  uint32_t raw_edge_last_status;
  uint32_t raw_edge_last_timestamp;
  uint32_t logical_event_count;
  uint32_t logical_press_count;
  uint32_t logical_release_count;
  uint32_t logical_long_count;
  uint32_t logical_repeat_count;
  uint32_t logical_chord_count;
  uint32_t logical_stuck_count;
  uint32_t logical_last_event;
  uint32_t logical_last_button_id;
  uint32_t logical_last_mask;
  uint32_t logical_last_timestamp;
  uint32_t logical_last_hold_ticks;
  uint32_t start_state;
  uint32_t start_active;
  uint32_t start_press_pending;
  uint32_t start_release_pending;
  uint32_t start_armed;
  uint32_t start_live_level;
  uint32_t start_raw_level;
  uint32_t start_stable_level;
  uint32_t start_stable_count;
  uint32_t start_sample_count;
  uint32_t start_synth_press_count;
  uint32_t start_next_check_tick;
  uint32_t start_checkpoint_count;
  uint32_t start_synth_release_count;
  uint32_t start_press_tick;
  uint32_t start_release_tick;
  uint32_t start_hold_ticks;
  uint32_t start_ship_prep_count;
  uint32_t start_ship_warning_count;
  uint32_t start_ship_imminent_count;
  uint32_t start_release_before_ship_count;
  uint32_t start_pending_event;
  uint32_t start_pending_timestamp;
  uint32_t start_pending_hold_ticks;
  uint32_t start_pending_drop_count;
} ps_input_buttons_probe_t;

typedef uint32_t (*ps_input_buttons_raw_edge_sink_t)(
  ps_input_button_id_t button_id,
  uint32_t active,
  uint32_t timestamp);

extern volatile ps_input_buttons_probe_t g_ps_input_buttons_probe;

void PS_InputButtons_Init(void);
void PS_InputButtons_SetRawEdgeSink(
  ps_input_buttons_raw_edge_sink_t sink);
void PS_InputButtons_RecordExti(uint16_t gpio_pin, GPIO_PinState level);
void PS_InputButtons_ProcessRawEdge(ps_input_button_id_t button_id,
                                    uint32_t active,
                                    uint32_t timestamp);
void PS_InputButtons_ReconcileLiveLevels(uint32_t now_tick);
uint32_t PS_InputButtons_NextWaitTicks(uint32_t now_tick,
                                      uint32_t maximum_wait_ticks);
uint32_t PS_InputButtons_Stop2Ready(void);
uint32_t PS_InputButtons_StartCheckDue(uint32_t now_tick);
void PS_InputButtons_PollStart(uint32_t now_tick);
uint32_t PS_InputButtons_ButtonsCheckDue(uint32_t now_tick);
void PS_InputButtons_PollButtons(uint32_t now_tick);
uint32_t PS_InputButtons_TakeLogicalEvent(
  ps_input_button_logical_record_t *record);
uint32_t PS_InputButtons_TakePress(ps_input_button_id_t *button_id,
                                   uint32_t *timestamp);
uint32_t PS_InputButtons_TakeStartPowerEvent(
  ps_input_start_power_event_t *event,
  uint32_t *timestamp,
  uint32_t *hold_ticks);

#ifdef __cplusplus
}
#endif

#endif /* PS_INPUT_BUTTONS_H */
