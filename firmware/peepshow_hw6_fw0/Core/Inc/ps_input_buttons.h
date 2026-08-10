#ifndef PS_INPUT_BUTTONS_H
#define PS_INPUT_BUTTONS_H

#include <stdint.h>

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_INPUT_BUTTONS_API_VERSION (1UL)

typedef enum
{
  PS_INPUT_BUTTON_ID_NONE = 0,
  PS_INPUT_BUTTON_ID_A,
  PS_INPUT_BUTTON_ID_B,
  PS_INPUT_BUTTON_ID_L,
  PS_INPUT_BUTTON_ID_R
} ps_input_button_id_t;

typedef enum
{
  PS_INPUT_BUTTON_EVENT_NONE = 0,
  PS_INPUT_BUTTON_EVENT_PRESS
} ps_input_button_event_t;

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
} ps_input_buttons_probe_t;

extern volatile ps_input_buttons_probe_t g_ps_input_buttons_probe;

void PS_InputButtons_Init(void);
void PS_InputButtons_RecordExti(uint16_t gpio_pin, GPIO_PinState level);
uint32_t PS_InputButtons_TakePress(ps_input_button_id_t *button_id,
                                   uint32_t *timestamp);

#ifdef __cplusplus
}
#endif

#endif /* PS_INPUT_BUTTONS_H */