#ifndef PS_DEV_TMAG3001_H
#define PS_DEV_TMAG3001_H

#include <stdint.h>

#include "ps_hw_i2c3.h"
#include "ps_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_TMAG3001_API_VERSION (9UL)

#define PS_DEV_TMAG3001_SENSOR_CONFIG2_X_Y_RANGE_MASK (0x02U)
#define PS_DEV_TMAG3001_SENSOR_CONFIG2_Z_RANGE_MASK   (0x01U)
#define PS_DEV_TMAG3001_DEVICE_STATUS_THR_CROSS_MASK  (0x01U)
#define PS_DEV_TMAG3001_DEVICE_STATUS_INT_RB_MASK     (0x10U)
#define PS_DEV_TMAG3001_SENSOR_CONFIG2_X_Y_HIGH_RANGE (0x02U)
#define PS_DEV_TMAG3001_SENSOR_CONFIG2_Z_HIGH_RANGE   (0x01U)

typedef enum
{
  PS_DEV_TMAG3001_STATE_UNINITIALIZED = 0,
  PS_DEV_TMAG3001_STATE_READY,
  PS_DEV_TMAG3001_STATE_ACTIVE,
  PS_DEV_TMAG3001_STATE_SUSPENDED,
  PS_DEV_TMAG3001_STATE_FAULT,
  PS_DEV_TMAG3001_STATE_WAKE_SLEEP
} ps_dev_tmag3001_state_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t operation_count;
  uint32_t state;
  uint32_t last_status;
  uint8_t address_7bit;
} ps_dev_tmag3001_t;

typedef struct
{
  ps_status_t status;
  ps_status_t ready_status;
  ps_status_t identity_status;
  uint8_t device_id;
  uint8_t manufacturer_lsb;
  uint8_t manufacturer_msb;
  uint32_t identity_match;
  uint8_t sensor_config1_before;
  uint8_t sensor_config1_after;
  uint8_t device_config2_before;
  uint8_t device_config2_after;
  uint8_t device_config2_sleep;
  uint32_t write_ok_mask;
  uint32_t verify_ok_mask;
  ps_status_t sensor_config1_verify_status;
  ps_status_t device_config2_verify_status;
  ps_status_t sleep_write_status;
  uint32_t terminal_sleep_committed;
  uint32_t post_sleep_read_omitted;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_tmag3001_stabilize_result_t;

typedef struct
{
  ps_status_t status;
  ps_status_t wake_probe_status;
  ps_status_t wake_retry_status;
  ps_status_t active_status;
  ps_status_t sensor_config2_status;
  ps_status_t sensor_config2_restore_status;
  ps_status_t preclear_device_status_read_status;
  uint8_t preclear_device_status;
  uint8_t active_sensor_config1;
  uint8_t sensor_config2_before;
  uint8_t active_sensor_config2;
  uint8_t sensor_config2_restore;
  uint8_t active_device_config2;
  uint32_t range_override_mask;
  uint32_t range_override_value;
  uint32_t range_override_applied;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_tmag3001_wake_result_t;

typedef struct
{
  ps_status_t status;
  ps_status_t sleep_status;
  uint32_t terminal_sleep_committed;
  uint32_t post_sleep_read_omitted;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_tmag3001_suspend_result_t;

typedef struct
{
  ps_status_t status;
  ps_status_t ready_status;
  ps_status_t identity_status;
  uint8_t device_id;
  uint8_t manufacturer_lsb;
  uint8_t manufacturer_msb;
  uint32_t identity_match;
  uint8_t sensor_config1_before;
  uint8_t sensor_config1_after;
  uint8_t int_config1_before;
  uint8_t int_config1_target;
  uint8_t int_config1_after;
  uint8_t device_config2_before;
  uint8_t device_config2_after;
  uint8_t device_config2_sleep;
  uint32_t write_ok_mask;
  uint32_t verify_ok_mask;
  ps_status_t sensor_config1_verify_status;
  ps_status_t int_config1_verify_status;
  ps_status_t device_config2_verify_status;
  ps_status_t sleep_write_status;
  uint32_t terminal_sleep_committed;
  uint32_t post_sleep_read_omitted;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_tmag3001_sleep_audit_result_t;

typedef struct
{
  ps_status_t status;
  ps_status_t identity_status;
  ps_status_t terminal_write_status;
  uint8_t device_id;
  uint8_t manufacturer_lsb;
  uint8_t manufacturer_msb;
  uint8_t sleep_period_code;
  uint8_t field_threshold_code;
  uint8_t field_hysteresis_code;
  uint8_t sensor_config1_target;
  uint8_t sensor_config1_after;
  uint8_t sensor_config2_target;
  uint8_t sensor_config2_after;
  uint8_t sensor_config3_target;
  uint8_t sensor_config3_after;
  uint8_t threshold_x_after;
  uint8_t threshold_y_after;
  uint8_t threshold_z_after;
  uint8_t threshold_x_high_after;
  uint8_t threshold_y_high_after;
  uint8_t threshold_z_high_after;
  uint8_t int_config1_target;
  uint8_t int_config1_after;
  uint8_t device_config2_standby;
  uint8_t device_config2_after;
  uint8_t device_config2_wake_sleep;
  uint32_t identity_match;
  uint32_t write_ok_mask;
  uint32_t verify_ok_mask;
  uint32_t terminal_write_committed;
  uint32_t post_terminal_read_omitted;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_tmag3001_wake_sleep_result_t;

typedef struct
{
  ps_status_t status;
  int16_t x;
  int16_t y;
  int16_t z;
  uint8_t conv_status;
  uint8_t magnitude_result;
  uint8_t device_status;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_tmag3001_raw_sample_t;

ps_status_t ps_dev_tmag3001_init(ps_dev_tmag3001_t *device,
                                 uint8_t address_7bit);
ps_status_t ps_dev_tmag3001_stabilize_suspended(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_stabilize_result_t *result);
ps_status_t ps_dev_tmag3001_wake_continuous(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_wake_result_t *result);
ps_status_t ps_dev_tmag3001_wake_continuous_with_range(
  ps_dev_tmag3001_t *device,
  uint8_t range_override_mask,
  uint8_t range_override_value,
  ps_dev_tmag3001_wake_result_t *result);
ps_status_t ps_dev_tmag3001_set_sensor_config2(
  ps_dev_tmag3001_t *device,
  uint8_t value,
  uint8_t *readback);
ps_status_t ps_dev_tmag3001_suspend(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_suspend_result_t *result);
ps_status_t ps_dev_tmag3001_prepare_sleep(
  ps_dev_tmag3001_t *device,
  uint8_t int_config1_target,
  ps_dev_tmag3001_sleep_audit_result_t *result);
ps_status_t ps_dev_tmag3001_prepare_sleep_audit(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_sleep_audit_result_t *result);
ps_status_t ps_dev_tmag3001_prepare_wake_sleep_omnipolar_xy(
  ps_dev_tmag3001_t *device,
  uint8_t sleep_period_code,
  uint8_t field_threshold_code,
  uint8_t field_hysteresis_code,
  ps_dev_tmag3001_wake_sleep_result_t *result);
ps_status_t ps_dev_tmag3001_read_raw_sample(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_raw_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_TMAG3001_H */
