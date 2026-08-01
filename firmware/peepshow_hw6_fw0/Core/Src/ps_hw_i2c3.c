#include "ps_hw_i2c3.h"

#include <limits.h>
#include <string.h>

#define PS_HW_I2C3_LEASE_MAGIC          (0x4932434CUL)
#define PS_HW_I2C3_LEGACY_MAX_LEASE_MS  (250UL)

static TX_MUTEX ps_hw_i2c3_mutex;
static I2C_HandleTypeDef *ps_hw_i2c3_handle;
static uint32_t ps_hw_i2c3_initialized;
static uint32_t ps_hw_i2c3_next_token;
static uint32_t ps_hw_i2c3_active_token;

static ULONG ps_hw_i2c3_ms_to_ticks(uint32_t milliseconds)
{
  uint64_t scaled;

  if (milliseconds == 0U)
  {
    return TX_NO_WAIT;
  }

  scaled = ((uint64_t)milliseconds * TX_TIMER_TICKS_PER_SECOND) + 999ULL;
  scaled /= 1000ULL;
  if (scaled > (uint64_t)ULONG_MAX)
  {
    return TX_WAIT_FOREVER;
  }

  return (ULONG)scaled;
}

static uint32_t ps_hw_i2c3_ticks_to_ms(ULONG ticks)
{
  uint64_t scaled;

  if (ticks == TX_WAIT_FOREVER)
  {
    return UINT32_MAX;
  }

  scaled = ((uint64_t)ticks * 1000ULL) +
           (uint64_t)TX_TIMER_TICKS_PER_SECOND - 1ULL;
  scaled /= (uint64_t)TX_TIMER_TICKS_PER_SECOND;
  if (scaled > UINT32_MAX)
  {
    return UINT32_MAX;
  }

  return (uint32_t)scaled;
}

static ps_status_t ps_hw_i2c3_map_hal_status(HAL_StatusTypeDef status)
{
  switch (status)
  {
    case HAL_OK:
      return PS_STATUS_OK;

    case HAL_BUSY:
      return PS_STATUS_BUSY;

    case HAL_TIMEOUT:
      return PS_STATUS_TIMEOUT;

    case HAL_ERROR:
    default:
      return PS_STATUS_IO_ERROR;
  }
}

static ps_status_t ps_hw_i2c3_validate_lease(
  const ps_hw_i2c3_lease_t *lease)
{
  ULONG elapsed;

  if ((ps_hw_i2c3_initialized == 0U) || (ps_hw_i2c3_handle == NULL))
  {
    return PS_STATUS_NOT_INITIALIZED;
  }
  if ((lease == NULL) ||
      (lease->magic != PS_HW_I2C3_LEASE_MAGIC) ||
      (lease->active == 0U) ||
      (lease->token == 0U) ||
      (lease->token != ps_hw_i2c3_active_token))
  {
    return PS_STATUS_INVALID_STATE;
  }

  elapsed = tx_time_get() - lease->acquired_tick;
  if (elapsed > lease->max_lease_ticks)
  {
    return PS_STATUS_LEASE_EXPIRED;
  }

  return PS_STATUS_OK;
}

UINT PS_HW_I2C3_Init(I2C_HandleTypeDef *handle)
{
  UINT status;

  if (handle == NULL)
  {
    return TX_PTR_ERROR;
  }

  status = tx_mutex_create(&ps_hw_i2c3_mutex, "mtxI2C3", TX_INHERIT);
  if (status == TX_SUCCESS)
  {
    ps_hw_i2c3_handle = handle;
    ps_hw_i2c3_next_token = 0U;
    ps_hw_i2c3_active_token = 0U;
    ps_hw_i2c3_initialized = 1U;
  }

  return status;
}

ps_hw_i2c3_lease_result_t ps_hw_i2c3_acquire(
  ps_hw_i2c3_client_t owner,
  uint32_t acquire_timeout_ms,
  uint32_t max_lease_ms,
  ps_hw_i2c3_lease_t *lease)
{
  ps_hw_i2c3_lease_result_t result =
  {
    .status = PS_STATUS_INTERNAL_ERROR,
    .tx_status = TX_NOT_DONE
  };
  ULONG max_lease_ticks;

  if ((lease == NULL) || (owner >= PS_HW_I2C3_CLIENT_COUNT) ||
      (max_lease_ms == 0U))
  {
    result.status = PS_STATUS_INVALID_ARGUMENT;
    result.tx_status = TX_PTR_ERROR;
    return result;
  }
  (void)memset(lease, 0, sizeof(*lease));
  if ((ps_hw_i2c3_initialized == 0U) || (ps_hw_i2c3_handle == NULL))
  {
    result.status = PS_STATUS_NOT_INITIALIZED;
    result.tx_status = TX_PTR_ERROR;
    return result;
  }

  result.tx_status = tx_mutex_get(
    &ps_hw_i2c3_mutex,
    (acquire_timeout_ms == UINT32_MAX) ?
      TX_WAIT_FOREVER : ps_hw_i2c3_ms_to_ticks(acquire_timeout_ms));
  if (result.tx_status != TX_SUCCESS)
  {
    result.status = (acquire_timeout_ms == 0U) ?
      PS_STATUS_BUSY : PS_STATUS_TIMEOUT;
    return result;
  }

  max_lease_ticks = ps_hw_i2c3_ms_to_ticks(max_lease_ms);
  if (max_lease_ticks == TX_NO_WAIT)
  {
    max_lease_ticks = 1UL;
  }
  ps_hw_i2c3_next_token++;
  if (ps_hw_i2c3_next_token == 0U)
  {
    ps_hw_i2c3_next_token = 1U;
  }
  ps_hw_i2c3_active_token = ps_hw_i2c3_next_token;
  lease->magic = PS_HW_I2C3_LEASE_MAGIC;
  lease->token = ps_hw_i2c3_active_token;
  lease->owner = (uint32_t)owner;
  lease->acquired_tick = tx_time_get();
  lease->max_lease_ticks = max_lease_ticks;
  lease->active = 1U;
  result.status = PS_STATUS_OK;
  return result;
}

ps_hw_i2c3_lease_result_t ps_hw_i2c3_release(
  ps_hw_i2c3_lease_t *lease)
{
  ps_hw_i2c3_lease_result_t result =
  {
    .status = PS_STATUS_INVALID_STATE,
    .tx_status = TX_NOT_DONE
  };
  ps_status_t lease_status;

  lease_status = ps_hw_i2c3_validate_lease(lease);
  if ((lease_status != PS_STATUS_OK) &&
      (lease_status != PS_STATUS_LEASE_EXPIRED))
  {
    return result;
  }

  ps_hw_i2c3_active_token = 0U;
  lease->active = 0U;
  result.tx_status = tx_mutex_put(&ps_hw_i2c3_mutex);
  if (result.tx_status != TX_SUCCESS)
  {
    result.status = PS_STATUS_INTERNAL_ERROR;
  }
  else
  {
    result.status = lease_status;
  }
  return result;
}

ps_hw_i2c3_transfer_result_t ps_hw_i2c3_probe_address(
  const ps_hw_i2c3_lease_t *lease,
  uint8_t address_7bit,
  uint32_t trials,
  uint32_t transfer_timeout_ms)
{
  ps_hw_i2c3_transfer_result_t result =
  {
    .status = PS_STATUS_INTERNAL_ERROR,
    .hal_status = HAL_ERROR,
    .hal_error = HAL_I2C_ERROR_NONE,
    .bytes_requested = 0U
  };
  HAL_StatusTypeDef status;

  result.status = ps_hw_i2c3_validate_lease(lease);
  if (result.status != PS_STATUS_OK)
  {
    return result;
  }
  if ((address_7bit > 0x7FU) || (trials == 0U))
  {
    result.status = PS_STATUS_INVALID_ARGUMENT;
    return result;
  }

  status = HAL_I2C_IsDeviceReady(
    ps_hw_i2c3_handle,
    (uint16_t)((uint16_t)address_7bit << 1U),
    trials,
    transfer_timeout_ms);
  result.hal_status = (uint32_t)status;
  result.hal_error = HAL_I2C_GetError(ps_hw_i2c3_handle);
  result.status = ps_hw_i2c3_map_hal_status(status);
  if ((status == HAL_ERROR) &&
      ((result.hal_error & HAL_I2C_ERROR_AF) != 0U))
  {
    result.status = PS_STATUS_EXPECTED_NACK;
  }
  return result;
}

ps_hw_i2c3_transfer_result_t ps_hw_i2c3_mem_read(
  const ps_hw_i2c3_lease_t *lease,
  uint8_t address_7bit,
  uint8_t register_address,
  uint8_t *data,
  uint16_t length,
  uint32_t transfer_timeout_ms)
{
  ps_hw_i2c3_transfer_result_t result =
  {
    .status = PS_STATUS_INTERNAL_ERROR,
    .hal_status = HAL_ERROR,
    .hal_error = HAL_I2C_ERROR_NONE,
    .bytes_requested = length
  };
  HAL_StatusTypeDef status;

  result.status = ps_hw_i2c3_validate_lease(lease);
  if (result.status != PS_STATUS_OK)
  {
    return result;
  }
  if ((address_7bit > 0x7FU) || (data == NULL) || (length == 0U))
  {
    result.status = PS_STATUS_INVALID_ARGUMENT;
    return result;
  }

  status = HAL_I2C_Mem_Read(
    ps_hw_i2c3_handle,
    (uint16_t)((uint16_t)address_7bit << 1U),
    register_address,
    I2C_MEMADD_SIZE_8BIT,
    data,
    length,
    transfer_timeout_ms);
  result.hal_status = (uint32_t)status;
  result.hal_error = HAL_I2C_GetError(ps_hw_i2c3_handle);
  result.status = ps_hw_i2c3_map_hal_status(status);
  return result;
}

ps_hw_i2c3_transfer_result_t ps_hw_i2c3_mem_write(
  const ps_hw_i2c3_lease_t *lease,
  uint8_t address_7bit,
  uint8_t register_address,
  const uint8_t *data,
  uint16_t length,
  uint32_t transfer_timeout_ms)
{
  ps_hw_i2c3_transfer_result_t result =
  {
    .status = PS_STATUS_INTERNAL_ERROR,
    .hal_status = HAL_ERROR,
    .hal_error = HAL_I2C_ERROR_NONE,
    .bytes_requested = length
  };
  HAL_StatusTypeDef status;

  result.status = ps_hw_i2c3_validate_lease(lease);
  if (result.status != PS_STATUS_OK)
  {
    return result;
  }
  if ((address_7bit > 0x7FU) || (data == NULL) || (length == 0U))
  {
    result.status = PS_STATUS_INVALID_ARGUMENT;
    return result;
  }

  status = HAL_I2C_Mem_Write(
    ps_hw_i2c3_handle,
    (uint16_t)((uint16_t)address_7bit << 1U),
    register_address,
    I2C_MEMADD_SIZE_8BIT,
    (uint8_t *)data,
    length,
    transfer_timeout_ms);
  result.hal_status = (uint32_t)status;
  result.hal_error = HAL_I2C_GetError(ps_hw_i2c3_handle);
  result.status = ps_hw_i2c3_map_hal_status(status);
  return result;
}

PS_HW_I2C3_Result PS_HW_I2C3_ReadRegister(uint8_t address_7bit,
                                           uint8_t register_address,
                                           uint8_t *value,
                                           ULONG lease_wait_ticks,
                                           uint32_t transfer_timeout_ms)
{
  PS_HW_I2C3_Result result =
  {
    .acquire_status = TX_NOT_DONE,
    .transfer_status = HAL_ERROR,
    .transfer_error = HAL_I2C_ERROR_NONE,
    .release_status = TX_NOT_DONE
  };
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t lease_result;
  ps_hw_i2c3_transfer_result_t transfer_result;

  if (value == NULL)
  {
    result.acquire_status = TX_PTR_ERROR;
    return result;
  }

  lease_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_DIAGNOSTIC,
    ps_hw_i2c3_ticks_to_ms(lease_wait_ticks),
    PS_HW_I2C3_LEGACY_MAX_LEASE_MS,
    &lease);
  result.acquire_status = lease_result.tx_status;
  if (lease_result.status != PS_STATUS_OK)
  {
    return result;
  }

  transfer_result = ps_hw_i2c3_mem_read(
    &lease,
    address_7bit,
    register_address,
    value,
    1U,
    transfer_timeout_ms);
  result.transfer_status = transfer_result.hal_status;
  result.transfer_error = transfer_result.hal_error;
  lease_result = ps_hw_i2c3_release(&lease);
  result.release_status = lease_result.tx_status;

  return result;
}

PS_HW_I2C3_Result PS_HW_I2C3_WriteRegister(uint8_t address_7bit,
                                            uint8_t register_address,
                                            uint8_t value,
                                            ULONG lease_wait_ticks,
                                            uint32_t transfer_timeout_ms)
{
  PS_HW_I2C3_Result result =
  {
    .acquire_status = TX_NOT_DONE,
    .transfer_status = HAL_ERROR,
    .transfer_error = HAL_I2C_ERROR_NONE,
    .release_status = TX_NOT_DONE
  };
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t lease_result;
  ps_hw_i2c3_transfer_result_t transfer_result;

  lease_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_DIAGNOSTIC,
    ps_hw_i2c3_ticks_to_ms(lease_wait_ticks),
    PS_HW_I2C3_LEGACY_MAX_LEASE_MS,
    &lease);
  result.acquire_status = lease_result.tx_status;
  if (lease_result.status != PS_STATUS_OK)
  {
    return result;
  }

  transfer_result = ps_hw_i2c3_mem_write(
    &lease,
    address_7bit,
    register_address,
    &value,
    1U,
    transfer_timeout_ms);
  result.transfer_status = transfer_result.hal_status;
  result.transfer_error = transfer_result.hal_error;
  lease_result = ps_hw_i2c3_release(&lease);
  result.release_status = lease_result.tx_status;

  return result;
}
