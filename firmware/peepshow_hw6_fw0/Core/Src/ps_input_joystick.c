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

static uint32_t PS_InputJoystick_DirectionForAxis(int32_t value,
                                                  uint32_t negative_direction,
                                                  uint32_t positive_direction)
{
  return (value < 0) ? negative_direction : positive_direction;
}

static uint32_t PS_InputJoystick_SelectDominantDirection(
  int32_t normalized_x,
  int32_t normalized_y,
  uint32_t previous_direction,
  int32_t enter_threshold,
  int32_t release_threshold,
  int32_t dominance_hysteresis)
{
  int32_t magnitude_x = PS_InputJoystick_Abs32(normalized_x);
  int32_t magnitude_y = PS_InputJoystick_Abs32(normalized_y);
  uint32_t horizontal_direction = PS_InputJoystick_DirectionForAxis(
    normalized_x,
    PS_INPUT_JOYSTICK_DIRECTION_LEFT,
    PS_INPUT_JOYSTICK_DIRECTION_RIGHT);
  uint32_t vertical_direction = PS_InputJoystick_DirectionForAxis(
    normalized_y,
    PS_INPUT_JOYSTICK_DIRECTION_UP,
    PS_INPUT_JOYSTICK_DIRECTION_DOWN);
  uint32_t previous_horizontal = previous_direction &
    (PS_INPUT_JOYSTICK_DIRECTION_LEFT |
     PS_INPUT_JOYSTICK_DIRECTION_RIGHT);
  uint32_t previous_vertical = previous_direction &
    (PS_INPUT_JOYSTICK_DIRECTION_UP |
     PS_INPUT_JOYSTICK_DIRECTION_DOWN);

  if ((previous_horizontal != 0UL) &&
      (previous_horizontal == horizontal_direction) &&
      (magnitude_x >= release_threshold))
  {
    if ((magnitude_y >= enter_threshold) &&
        (magnitude_y > (magnitude_x + dominance_hysteresis)))
    {
      return vertical_direction;
    }
    return previous_horizontal;
  }
  if ((previous_vertical != 0UL) &&
      (previous_vertical == vertical_direction) &&
      (magnitude_y >= release_threshold))
  {
    if ((magnitude_x >= enter_threshold) &&
        (magnitude_x > (magnitude_y + dominance_hysteresis)))
    {
      return horizontal_direction;
    }
    return previous_vertical;
  }

  if ((magnitude_x < enter_threshold) &&
      (magnitude_y < enter_threshold))
  {
    return 0UL;
  }
  if (magnitude_y > magnitude_x)
  {
    return vertical_direction;
  }
  return horizontal_direction;
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
  state->candidate_direction_mask = 0UL;
  state->direction_mask = 0UL;
  state->direction_change_count = 0UL;
  state->direction_press_count = 0UL;
  state->direction_release_count = 0UL;
  state->direction_switch_count = 0UL;
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
  uint32_t previous_direction;
  uint32_t resolved_direction;
  int64_t transformed_x;
  int64_t transformed_y;

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
    state->candidate_direction_mask = 0UL;
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

  state->delta_x = sample->raw_x - calibration->center_x;
  state->delta_y = sample->raw_y - calibration->center_y;
  if (calibration->transform_valid != 0UL)
  {
    transformed_x =
      ((int64_t)calibration->transform_xx_q20 * state->delta_x) +
      ((int64_t)calibration->transform_xy_q20 * state->delta_y);
    transformed_y =
      ((int64_t)calibration->transform_yx_q20 * state->delta_x) +
      ((int64_t)calibration->transform_yy_q20 * state->delta_y);
    state->delta_x = (int32_t)(transformed_x >> 20);
    state->delta_y = (int32_t)(transformed_y >> 20);
    negative_x_span = -calibration->min_x;
    positive_x_span = calibration->max_x;
    negative_y_span = -calibration->min_y;
    positive_y_span = calibration->max_y;
  }
  else
  {
    negative_x_span = calibration->center_x - calibration->min_x;
    positive_x_span = calibration->max_x - calibration->center_x;
    negative_y_span = calibration->center_y - calibration->min_y;
    positive_y_span = calibration->max_y - calibration->center_y;
  }
  if ((negative_x_span <= 0) || (positive_x_span <= 0) ||
      (negative_y_span <= 0) || (positive_y_span <= 0) ||
      (calibration->direction_threshold <= 0) ||
      (calibration->direction_threshold > PS_INPUT_JOYSTICK_AXIS_SCALE) ||
      (calibration->direction_release_threshold < 0) ||
      (calibration->direction_release_threshold >=
       calibration->direction_threshold) ||
      (calibration->dominance_hysteresis < 0))
  {
    state->fault_count++;
    state->last_status = (uint32_t)PS_STATUS_INVALID_ARGUMENT;
    return PS_STATUS_INVALID_ARGUMENT;
  }

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

  previous_direction = state->direction_mask;
  resolved_direction = PS_InputJoystick_SelectDominantDirection(
    state->normalized_x,
    state->normalized_y,
    previous_direction,
    calibration->direction_threshold,
    calibration->direction_release_threshold,
    calibration->dominance_hysteresis);
  state->candidate_direction_mask = direction_mask;
  state->direction_mask = resolved_direction;
  state->active = (resolved_direction != 0UL) ? 1UL : 0UL;
  if (resolved_direction != previous_direction)
  {
    state->direction_change_count++;
    if (previous_direction == 0UL)
    {
      state->direction_press_count++;
    }
    else if (resolved_direction == 0UL)
    {
      state->direction_release_count++;
    }
    else
    {
      state->direction_switch_count++;
    }
  }
  state->update_count++;
  state->last_status = (uint32_t)PS_STATUS_OK;
  return PS_STATUS_OK;
}
