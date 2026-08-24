#ifndef PS_INPUT_JOYSTICK_H
#define PS_INPUT_JOYSTICK_H

#include <stdint.h>

#include "ps_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_INPUT_JOYSTICK_API_VERSION       (2UL)
#define PS_INPUT_JOYSTICK_AXIS_SCALE        (1000)
#define PS_INPUT_JOYSTICK_DIRECTION_LEFT    (1UL << 0)
#define PS_INPUT_JOYSTICK_DIRECTION_RIGHT   (1UL << 1)
#define PS_INPUT_JOYSTICK_DIRECTION_UP      (1UL << 2)
#define PS_INPUT_JOYSTICK_DIRECTION_DOWN    (1UL << 3)

typedef enum
{
  PS_INPUT_JOYSTICK_POLICY_OFF = 0,
  PS_INPUT_JOYSTICK_POLICY_THRESHOLD_ARMED,
  PS_INPUT_JOYSTICK_POLICY_DIRECTION_SAMPLE,
  PS_INPUT_JOYSTICK_POLICY_SLOW_POLL,
  PS_INPUT_JOYSTICK_POLICY_FAST_POLL
} ps_input_joystick_policy_t;

typedef struct
{
  int32_t center_x;
  int32_t center_y;
  int32_t min_x;
  int32_t max_x;
  int32_t min_y;
  int32_t max_y;
  int32_t deadzone_counts;
  int32_t direction_threshold;
  int32_t transform_xx_q20;
  int32_t transform_xy_q20;
  int32_t transform_yx_q20;
  int32_t transform_yy_q20;
  uint32_t transform_valid;
  uint32_t valid;
} ps_input_joystick_calibration_t;

typedef struct
{
  int32_t raw_x;
  int32_t raw_y;
  int32_t raw_z;
  uint32_t conv_status;
  uint32_t sample_tick;
} ps_input_joystick_raw_sample_t;

typedef struct
{
  uint32_t api_version;
  uint32_t policy;
  uint32_t calibration_valid;
  uint32_t active;
  uint32_t direction_mask;
  uint32_t sample_tick;
  uint32_t sample_age_ticks;
  uint32_t update_count;
  uint32_t fault_count;
  uint32_t last_status;
  int32_t raw_x;
  int32_t raw_y;
  int32_t raw_z;
  int32_t delta_x;
  int32_t delta_y;
  int32_t normalized_x;
  int32_t normalized_y;
  uint32_t magnitude;
  uint32_t conv_status;
} ps_input_joystick_state_t;

void PS_InputJoystick_InitState(ps_input_joystick_state_t *state);
ps_status_t PS_InputJoystick_Normalize(
  const ps_input_joystick_calibration_t *calibration,
  const ps_input_joystick_raw_sample_t *sample,
  uint32_t policy,
  uint32_t now_tick,
  ps_input_joystick_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* PS_INPUT_JOYSTICK_H */
