#include "ps_input_joystick.h"

static int32_t PS_InputJoystick_Abs32(int32_t value)
{
  return (value < 0) ? -value : value;
}

static int32_t PS_InputJoystick_ClampAxis(int32_t value)
{
  if (value > PS_INPUT_JOYSTICK_AXIS_SCALE)
  {
    return PS_INPUT_JOYSTICK_AXIS_SCALE;
  }
  if (value < -PS_INPUT_JOYSTICK_AXIS_SCALE)
  {
    return -PS_INPUT_JOYSTICK_AXIS_SCALE;
  }
  return value;
}

static int32_t PS_InputJoystick_NormalizeAxis(int32_t delta,
                                             int32_t negative_span,
                                             int32_t positive_span,
                                             int32_t deadzone)
{
  int32_t span;
  int32_t magnitude;
  int32_t normalized;

  magnitude = PS_InputJoystick_Abs32(delta);
  if (magnitude <= deadzone)
  {
    return 0;
  }

  span = (delta < 0) ? negative_span : positive_span;
  if (span <= deadzone)
  {
    return 0;
  }

  magnitude -= deadzone;
  normalized = (magnitude * PS_INPUT_JOYSTICK_AXIS_SCALE) /
               (span - deadzone);
  normalized = PS_InputJoystick_ClampAxis(normalized);
  return (delta < 0) ? -normalized : normalized;
}

void PS_InputJoystick_InitState(ps_input_joystick_state_t *state)
{
  if (state == (ps_input_joystick_state_t *)0)
  {
    return;
  }

  state->api_version = PS_INPUT_JOYSTICK_API_VERSION;
  state->policy = PS_INPUT_JOYSTICK_POLICY_OFF;
  state->calibration_valid = 0UL;
  state->active = 0UL;
  state->direction_mask = 0UL;
  state->sample_tick = 0UL;
  state->sample_age_ticks = 0UL;
  state->update_count = 0UL;
  state->fault_count = 0UL;
  state->last_status = (uint32_t)PS_STATUS_OK;
  state->raw_x = 0;
  state->raw_y = 0;
  state->raw_z = 0;
  state->delta_x = 0;
  state->delta_y = 0;
  state->normalized_x = 0;
  state->normalized_y = 0;
  state->magnitude = 0UL;
  state->conv_status = 0UL;
}

ps_status_t PS_InputJoystick_Normalize(
  const ps_input_joystick_calibration_t *calibration,
  const ps_input_joystick_raw_sample_t *sample,
  uint32_t policy,
  uint32_t now_tick,
  ps_input_joystick_state_t *state)
{
  int32_t negative_x_span;
  int32_t positive_x_span;
  int32_t negative_y_span;
  int32_t positive_y_span;
  uint32_t magnitude_x;
  uint32_t magnitude_y;
  uint32_t direction_mask;

  if ((calibration == (const ps_input_joystick_calibration_t *)0) ||
      (sample == (const ps_input_joystick_raw_sample_t *)0) ||
      (state == (ps_input_joystick_state_t *)0))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  state->policy = policy;
  state->sample_tick = sample->sample_tick;
  state->sample_age_ticks = now_tick - sample->sample_tick;
  state->raw_x = sample->raw_x;
  state->raw_y = sample->raw_y;
  state->raw_z = sample->raw_z;
  state->conv_status = sample->conv_status;
  state->calibration_valid = calibration->valid;

  if (calibration->valid == 0UL)
  {
    state->active = 0UL;
    state->direction_mask = 0UL;
    state->delta_x = 0;
    state->delta_y = 0;
    state->normalized_x = 0;
    state->normalized_y = 0;
    state->magnitude = 0UL;
    state->fault_count++;
    state->last_status = (uint32_t)PS_STATUS_INVALID_STATE;
    return PS_STATUS_INVALID_STATE;
  }

  negative_x_span = calibration->center_x - calibration->min_x;
  positive_x_span = calibration->max_x - calibration->center_x;
  negative_y_span = calibration->center_y - calibration->min_y;
  positive_y_span = calibration->max_y - calibration->center_y;
  if ((negative_x_span <= 0) || (positive_x_span <= 0) ||
      (negative_y_span <= 0) || (positive_y_span <= 0))
  {
    state->fault_count++;
    state->last_status = (uint32_t)PS_STATUS_INVALID_ARGUMENT;
    return PS_STATUS_INVALID_ARGUMENT;
  }

  state->delta_x = sample->raw_x - calibration->center_x;
  state->delta_y = sample->raw_y - calibration->center_y;
  state->normalized_x = PS_InputJoystick_NormalizeAxis(
    state->delta_x, negative_x_span, positive_x_span,
    calibration->deadzone_counts);
  state->normalized_y = PS_InputJoystick_NormalizeAxis(
    state->delta_y, negative_y_span, positive_y_span,
    calibration->deadzone_counts);

  magnitude_x = (uint32_t)PS_InputJoystick_Abs32(state->normalized_x);
  magnitude_y = (uint32_t)PS_InputJoystick_Abs32(state->normalized_y);
  state->magnitude = (magnitude_x > magnitude_y) ? magnitude_x : magnitude_y;

  direction_mask = 0UL;
  if (state->normalized_x <= -calibration->direction_threshold)
  {
    direction_mask |= PS_INPUT_JOYSTICK_DIRECTION_LEFT;
  }
  if (state->normalized_x >= calibration->direction_threshold)
  {
    direction_mask |= PS_INPUT_JOYSTICK_DIRECTION_RIGHT;
  }
  if (state->normalized_y <= -calibration->direction_threshold)
  {
    direction_mask |= PS_INPUT_JOYSTICK_DIRECTION_UP;
  }
  if (state->normalized_y >= calibration->direction_threshold)
  {
    direction_mask |= PS_INPUT_JOYSTICK_DIRECTION_DOWN;
  }

  state->direction_mask = direction_mask;
  state->active = (direction_mask != 0UL) ? 1UL : 0UL;
  state->update_count++;
  state->last_status = (uint32_t)PS_STATUS_OK;
  return PS_STATUS_OK;
}
