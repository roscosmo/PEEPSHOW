#ifndef PS_DEV_TMAG3001_H
#define PS_DEV_TMAG3001_H

#include <stdint.h>

#include "ps_hw_i2c3.h"
#include "ps_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_TMAG3001_API_VERSION (1UL)

typedef enum
{
  PS_DEV_TMAG3001_STATE_UNINITIALIZED = 0,
  PS_DEV_TMAG3001_STATE_READY,
  PS_DEV_TMAG3001_STATE_ACTIVE,
  PS_DEV_TMAG3001_STATE_SUSPENDED,
  PS_DEV_TMAG3001_STATE_FAULT
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
  uint8_t active_sensor_config1;
  uint8_t active_device_config2;
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
  int16_t x;
  int16_t y;
  int16_t z;
  uint8_t conv_status;
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
ps_status_t ps_dev_tmag3001_suspend(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_suspend_result_t *result);
ps_status_t ps_dev_tmag3001_read_raw_sample(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_raw_sample_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_TMAG3001_H */
