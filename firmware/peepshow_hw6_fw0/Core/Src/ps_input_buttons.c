#include "ps_input_buttons.h"

#include "tx_api.h"

#define PS_INPUT_BUTTON_MASK_A (1UL << 0U)
#define PS_INPUT_BUTTON_MASK_B (1UL << 1U)
#define PS_INPUT_BUTTON_MASK_L (1UL << 2U)
#define PS_INPUT_BUTTON_MASK_R (1UL << 3U)

volatile ps_input_buttons_probe_t g_ps_input_buttons_probe;

static volatile uint32_t ps_input_buttons_pending_mask;
static volatile uint32_t ps_input_buttons_timestamp[4];

static uint32_t PS_InputButtons_MaskForPin(uint16_t gpio_pin,
                                           ps_input_button_id_t *button_id)
{
  if (gpio_pin == BTN_A_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_A;
    return PS_INPUT_BUTTON_MASK_A;
  }
  if (gpio_pin == BTN_B_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_B;
    return PS_INPUT_BUTTON_MASK_B;
  }
  if (gpio_pin == BTN_L_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_L;
    return PS_INPUT_BUTTON_MASK_L;
  }
  if (gpio_pin == BTN_R_Pin)
  {
    *button_id = PS_INPUT_BUTTON_ID_R;
    return PS_INPUT_BUTTON_MASK_R;
  }

  *button_id = PS_INPUT_BUTTON_ID_NONE;
  return 0UL;
}

void PS_InputButtons_Init(void)
{
  uint32_t i;

  g_ps_input_buttons_probe.api_version = PS_INPUT_BUTTONS_API_VERSION;
  g_ps_input_buttons_probe.isr_edge_count = 0UL;
  g_ps_input_buttons_probe.press_count = 0UL;
  g_ps_input_buttons_probe.ignored_edge_count = 0UL;
  g_ps_input_buttons_probe.pending_mask = 0UL;
  g_ps_input_buttons_probe.last_pin = 0UL;
  g_ps_input_buttons_probe.last_button_id = PS_INPUT_BUTTON_ID_NONE;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_NONE;
  g_ps_input_buttons_probe.last_level = 0UL;
  g_ps_input_buttons_probe.last_tick = 0UL;
  ps_input_buttons_pending_mask = 0UL;
  for (i = 0UL; i < 4UL; ++i)
  {
    ps_input_buttons_timestamp[i] = 0UL;
  }
}

void PS_InputButtons_RecordExti(uint16_t gpio_pin, GPIO_PinState level)
{
  ps_input_button_id_t button_id;
  uint32_t mask = PS_InputButtons_MaskForPin(gpio_pin, &button_id);

  g_ps_input_buttons_probe.isr_edge_count++;
  g_ps_input_buttons_probe.last_pin = gpio_pin;
  g_ps_input_buttons_probe.last_button_id = button_id;
  g_ps_input_buttons_probe.last_level = (level == GPIO_PIN_SET) ? 1UL : 0UL;
  g_ps_input_buttons_probe.last_tick = HAL_GetTick();

  if ((mask == 0UL) || (level != GPIO_PIN_SET))
  {
    g_ps_input_buttons_probe.ignored_edge_count++;
    return;
  }

  ps_input_buttons_timestamp[(uint32_t)button_id - 1UL] =
    g_ps_input_buttons_probe.last_tick;
  ps_input_buttons_pending_mask |= mask;
  g_ps_input_buttons_probe.pending_mask = ps_input_buttons_pending_mask;
}

uint32_t PS_InputButtons_TakePress(ps_input_button_id_t *button_id,
                                   uint32_t *timestamp)
{
  uint32_t primask;
  uint32_t pending;
  uint32_t mask = 0UL;
  uint32_t index = 0UL;

  if ((button_id == 0) || (timestamp == 0))
  {
    return 0UL;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  pending = ps_input_buttons_pending_mask;
  if (pending == 0UL)
  {
    if (primask == 0UL)
    {
      __enable_irq();
    }
    return 0UL;
  }

  if ((pending & PS_INPUT_BUTTON_MASK_A) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_A;
    index = 0UL;
    *button_id = PS_INPUT_BUTTON_ID_A;
  }
  else if ((pending & PS_INPUT_BUTTON_MASK_B) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_B;
    index = 1UL;
    *button_id = PS_INPUT_BUTTON_ID_B;
  }
  else if ((pending & PS_INPUT_BUTTON_MASK_L) != 0UL)
  {
    mask = PS_INPUT_BUTTON_MASK_L;
    index = 2UL;
    *button_id = PS_INPUT_BUTTON_ID_L;
  }
  else
  {
    mask = PS_INPUT_BUTTON_MASK_R;
    index = 3UL;
    *button_id = PS_INPUT_BUTTON_ID_R;
  }

  ps_input_buttons_pending_mask &= ~mask;
  *timestamp = ps_input_buttons_timestamp[index];
  g_ps_input_buttons_probe.pending_mask = ps_input_buttons_pending_mask;
  if (primask == 0UL)
  {
    __enable_irq();
  }

  g_ps_input_buttons_probe.last_button_id = *button_id;
  g_ps_input_buttons_probe.last_event = PS_INPUT_BUTTON_EVENT_PRESS;
  g_ps_input_buttons_probe.press_count++;
  return 1UL;
}
