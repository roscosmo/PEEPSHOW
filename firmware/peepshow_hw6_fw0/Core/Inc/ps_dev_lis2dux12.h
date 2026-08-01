#ifndef PS_DEV_LIS2DUX12_H
#define PS_DEV_LIS2DUX12_H

#include <stdint.h>

#include "ps_hw_i2c3.h"
#include "ps_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_LIS2DUX12_API_VERSION      (1UL)
#define PS_DEV_LIS2DUX12_REGISTER_COUNT   (11UL)

typedef enum
{
  PS_DEV_LIS2DUX12_STATE_UNINITIALIZED = 0,
  PS_DEV_LIS2DUX12_STATE_READY,
  PS_DEV_LIS2DUX12_STATE_LOW_RATE,
  PS_DEV_LIS2DUX12_STATE_SUSPENDED,
  PS_DEV_LIS2DUX12_STATE_FAULT
} ps_dev_lis2dux12_state_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t operation_count;
  uint32_t state;
  uint32_t last_status;
  uint8_t address_7bit;
} ps_dev_lis2dux12_t;

typedef struct
{
  ps_status_t status;
  uint32_t whoami_hal_status;
  uint32_t whoami_hal_error;
  uint8_t whoami;
  uint32_t identity_match;
  uint8_t register_address[PS_DEV_LIS2DUX12_REGISTER_COUNT];
  uint8_t register_before[PS_DEV_LIS2DUX12_REGISTER_COUNT];
  uint8_t register_after[PS_DEV_LIS2DUX12_REGISTER_COUNT];
  uint32_t snapshot_ok_mask;
  uint32_t write_ok_mask;
  uint32_t verify_ok_mask;
  uint8_t deep_power_down_value;
  ps_status_t deep_power_down_status;
  uint32_t terminal_deep_power_down_committed;
  uint32_t post_deep_power_down_read_omitted;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_lis2dux12_stabilize_result_t;

typedef struct
{
  ps_status_t status;
  uint32_t wake_probe_hal_status;
  uint32_t wake_probe_hal_error;
  uint32_t wake_probe_accepted;
  ps_status_t whoami_status;
  uint8_t whoami;
  ps_status_t mode_status;
  uint8_t active_ctrl5;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_lis2dux12_wake_result_t;

typedef struct
{
  ps_status_t status;
  ps_status_t mode_status;
  ps_status_t deep_power_down_status;
  uint32_t terminal_deep_power_down_committed;
  uint32_t post_deep_power_down_read_omitted;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_lis2dux12_suspend_result_t;

ps_status_t ps_dev_lis2dux12_init(ps_dev_lis2dux12_t *device,
                                  uint8_t address_7bit);
ps_status_t ps_dev_lis2dux12_stabilize_suspended(
  ps_dev_lis2dux12_t *device,
  ps_dev_lis2dux12_stabilize_result_t *result);
ps_status_t ps_dev_lis2dux12_wake_low_rate(
  ps_dev_lis2dux12_t *device,
  ps_dev_lis2dux12_wake_result_t *result);
ps_status_t ps_dev_lis2dux12_suspend(
  ps_dev_lis2dux12_t *device,
  ps_dev_lis2dux12_suspend_result_t *result);
uint8_t ps_dev_lis2dux12_diagnostic_register(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_LIS2DUX12_H */
