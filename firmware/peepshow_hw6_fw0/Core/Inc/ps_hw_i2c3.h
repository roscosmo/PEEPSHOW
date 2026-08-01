#ifndef PS_HW_I2C3_H
#define PS_HW_I2C3_H

#include <stdint.h>

#include "ps_status.h"
#include "stm32u5xx_hal.h"
#include "tx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint32_t acquire_status;
  uint32_t transfer_status;
  uint32_t transfer_error;
  uint32_t release_status;
} PS_HW_I2C3_Result;

typedef enum
{
  PS_HW_I2C3_CLIENT_POWER = 0,
  PS_HW_I2C3_CLIENT_INPUT,
  PS_HW_I2C3_CLIENT_SENSOR,
  PS_HW_I2C3_CLIENT_DIAGNOSTIC,
  PS_HW_I2C3_CLIENT_COUNT
} ps_hw_i2c3_client_t;

typedef struct
{
  uint32_t magic;
  uint32_t token;
  uint32_t owner;
  ULONG acquired_tick;
  ULONG max_lease_ticks;
  uint32_t active;
} ps_hw_i2c3_lease_t;

typedef struct
{
  ps_status_t status;
  uint32_t tx_status;
} ps_hw_i2c3_lease_result_t;

typedef struct
{
  ps_status_t status;
  uint32_t hal_status;
  uint32_t hal_error;
  uint32_t bytes_requested;
} ps_hw_i2c3_transfer_result_t;

UINT PS_HW_I2C3_Init(I2C_HandleTypeDef *handle);

ps_hw_i2c3_lease_result_t ps_hw_i2c3_acquire(
  ps_hw_i2c3_client_t owner,
  uint32_t acquire_timeout_ms,
  uint32_t max_lease_ms,
  ps_hw_i2c3_lease_t *lease);
ps_hw_i2c3_lease_result_t ps_hw_i2c3_release(
  ps_hw_i2c3_lease_t *lease);
ps_hw_i2c3_transfer_result_t ps_hw_i2c3_probe_address(
  const ps_hw_i2c3_lease_t *lease,
  uint8_t address_7bit,
  uint32_t trials,
  uint32_t transfer_timeout_ms);
ps_hw_i2c3_transfer_result_t ps_hw_i2c3_mem_read(
  const ps_hw_i2c3_lease_t *lease,
  uint8_t address_7bit,
  uint8_t register_address,
  uint8_t *data,
  uint16_t length,
  uint32_t transfer_timeout_ms);
ps_hw_i2c3_transfer_result_t ps_hw_i2c3_mem_write(
  const ps_hw_i2c3_lease_t *lease,
  uint8_t address_7bit,
  uint8_t register_address,
  const uint8_t *data,
  uint16_t length,
  uint32_t transfer_timeout_ms);

/* Compatibility helpers for probes not yet migrated to a device driver. */
PS_HW_I2C3_Result PS_HW_I2C3_ReadRegister(uint8_t address_7bit,
                                           uint8_t register_address,
                                           uint8_t *value,
                                           ULONG lease_wait_ticks,
                                           uint32_t transfer_timeout_ms);
PS_HW_I2C3_Result PS_HW_I2C3_WriteRegister(uint8_t address_7bit,
                                            uint8_t register_address,
                                            uint8_t value,
                                            ULONG lease_wait_ticks,
                                            uint32_t transfer_timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* PS_HW_I2C3_H */
