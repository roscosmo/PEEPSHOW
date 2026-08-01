#include "ps_dev_lis2dux12.h"

#include <string.h>

#include "lis2dux12_reg.h"
#include "tx_api.h"

#define PS_DEV_LIS2DUX12_ACQUIRE_TIMEOUT_MS  (200UL)
#define PS_DEV_LIS2DUX12_MAX_LEASE_MS        (250UL)
#define PS_DEV_LIS2DUX12_TRANSFER_TIMEOUT_MS (50UL)
#define PS_DEV_LIS2DUX12_WAKE_SETTLE_MS      (30UL)
#define PS_DEV_LIS2DUX12_PRETERMINAL_COUNT   \
  (PS_DEV_LIS2DUX12_REGISTER_COUNT - 1UL)
#define PS_DEV_LIS2DUX12_ALL_REGISTER_MASK   \
  ((1UL << PS_DEV_LIS2DUX12_REGISTER_COUNT) - 1UL)
#define PS_DEV_LIS2DUX12_PRETERMINAL_MASK    \
  ((1UL << PS_DEV_LIS2DUX12_PRETERMINAL_COUNT) - 1UL)

typedef struct
{
  ps_dev_lis2dux12_t *device;
  ps_hw_i2c3_lease_t *lease;
  ps_hw_i2c3_transfer_result_t last_transfer;
} ps_dev_lis2dux12_transport_t;

static const uint8_t ps_dev_lis2dux12_registers
  [PS_DEV_LIS2DUX12_REGISTER_COUNT] =
{
  LIS2DUX12_FIFO_CTRL,
  LIS2DUX12_CTRL2,
  LIS2DUX12_CTRL3,
  LIS2DUX12_INTERRUPT_CFG,
  LIS2DUX12_MD1_CFG,
  LIS2DUX12_MD2_CFG,
  LIS2DUX12_SELF_TEST,
  LIS2DUX12_CTRL4,
  LIS2DUX12_CTRL5,
  LIS2DUX12_CTRL1,
  LIS2DUX12_SLEEP
};

static ULONG ps_dev_lis2dux12_ms_to_ticks(uint32_t milliseconds)
{
  uint64_t scaled;

  scaled = ((uint64_t)milliseconds * TX_TIMER_TICKS_PER_SECOND) + 999ULL;
  scaled /= 1000ULL;
  return (scaled == 0ULL) ? 1UL : (ULONG)scaled;
}

static int32_t ps_dev_lis2dux12_read(void *handle,
                                     uint8_t reg,
                                     uint8_t *data,
                                     uint16_t length)
{
  ps_dev_lis2dux12_transport_t *transport =
    (ps_dev_lis2dux12_transport_t *)handle;

  if ((transport == NULL) || (transport->device == NULL) ||
      (transport->lease == NULL))
  {
    return -1;
  }

  transport->last_transfer = ps_hw_i2c3_mem_read(
    transport->lease,
    transport->device->address_7bit,
    reg,
    data,
    length,
    PS_DEV_LIS2DUX12_TRANSFER_TIMEOUT_MS);
  return (transport->last_transfer.status == PS_STATUS_OK) ? 0 : -1;
}

static int32_t ps_dev_lis2dux12_write(void *handle,
                                      uint8_t reg,
                                      const uint8_t *data,
                                      uint16_t length)
{
  ps_dev_lis2dux12_transport_t *transport =
    (ps_dev_lis2dux12_transport_t *)handle;

  if ((transport == NULL) || (transport->device == NULL) ||
      (transport->lease == NULL))
  {
    return -1;
  }

  transport->last_transfer = ps_hw_i2c3_mem_write(
    transport->lease,
    transport->device->address_7bit,
    reg,
    data,
    length,
    PS_DEV_LIS2DUX12_TRANSFER_TIMEOUT_MS);
  return (transport->last_transfer.status == PS_STATUS_OK) ? 0 : -1;
}

static void ps_dev_lis2dux12_prepare_context(
  ps_dev_lis2dux12_t *device,
  ps_hw_i2c3_lease_t *lease,
  ps_dev_lis2dux12_transport_t *transport,
  stmdev_ctx_t *context)
{
  (void)memset(transport, 0, sizeof(*transport));
  transport->device = device;
  transport->lease = lease;
  transport->last_transfer.status = PS_STATUS_OK;
  transport->last_transfer.hal_status = HAL_OK;
  transport->last_transfer.hal_error = HAL_I2C_ERROR_NONE;

  (void)memset(context, 0, sizeof(*context));
  context->read_reg = ps_dev_lis2dux12_read;
  context->write_reg = ps_dev_lis2dux12_write;
  context->handle = transport;
}

static ps_status_t ps_dev_lis2dux12_vendor_status(
  int32_t vendor_status,
  const ps_dev_lis2dux12_transport_t *transport)
{
  if (vendor_status == 0)
  {
    return PS_STATUS_OK;
  }
  if ((transport != NULL) &&
      (transport->last_transfer.status != PS_STATUS_OK))
  {
    return transport->last_transfer.status;
  }
  return PS_STATUS_INTERNAL_ERROR;
}

static ps_status_t ps_dev_lis2dux12_finish(
  ps_dev_lis2dux12_t *device,
  ps_hw_i2c3_lease_t *lease,
  ps_status_t operation_status)
{
  ps_hw_i2c3_lease_result_t release_result;

  release_result = ps_hw_i2c3_release(lease);
  if ((operation_status == PS_STATUS_OK) &&
      (release_result.status != PS_STATUS_OK))
  {
    operation_status = release_result.status;
  }
  device->last_status = (uint32_t)operation_status;
  if (operation_status != PS_STATUS_OK)
  {
    device->state = PS_DEV_LIS2DUX12_STATE_FAULT;
  }
  return operation_status;
}

static uint32_t ps_dev_lis2dux12_register_matches(
  uint32_t index,
  uint8_t before,
  uint8_t after)
{
  switch (index)
  {
    case 0U:
    case 1U:
    case 2U:
    case 3U:
    case 4U:
    case 5U:
      return (after == 0U) ? 1UL : 0UL;

    case 6U:
      return ((after & 0x01U) != 0U) ? 1UL : 0UL;

    case 7U:
      return ((after & 0x18U) == 0U) ? 1UL : 0UL;

    case 8U:
      return (after == 0U) ? 1UL : 0UL;

    case 9U:
      return (after == (uint8_t)(before & 0x10U)) ? 1UL : 0UL;

    default:
      return 0UL;
  }
}

ps_status_t ps_dev_lis2dux12_init(ps_dev_lis2dux12_t *device,
                                  uint8_t address_7bit)
{
  if ((device == NULL) || (address_7bit > 0x7FU))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(device, 0, sizeof(*device));
  device->api_version = PS_DEV_LIS2DUX12_API_VERSION;
  device->address_7bit = address_7bit;
  device->state = PS_DEV_LIS2DUX12_STATE_READY;
  device->last_status = PS_STATUS_OK;
  device->initialized = 1U;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_lis2dux12_stabilize_suspended(
  ps_dev_lis2dux12_t *device,
  ps_dev_lis2dux12_stabilize_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_lis2dux12_transport_t transport;
  stmdev_ctx_t context;
  lis2dux12_md_t mode =
  {
    .odr = LIS2DUX12_OFF,
    .fs = LIS2DUX12_2g,
    .bw = LIS2DUX12_ODR_div_2
  };
  ps_status_t status = PS_STATUS_OK;
  uint8_t value = 0U;
  uint32_t index;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->deep_power_down_status = PS_STATUS_INTERNAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  device->operation_count++;

  for (index = 0U; index < PS_DEV_LIS2DUX12_REGISTER_COUNT; ++index)
  {
    result->register_address[index] =
      ps_dev_lis2dux12_registers[index];
  }

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_SENSOR,
    PS_DEV_LIS2DUX12_ACQUIRE_TIMEOUT_MS,
    PS_DEV_LIS2DUX12_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    result->status = acquire_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_LIS2DUX12_STATE_FAULT;
    return result->status;
  }
  ps_dev_lis2dux12_prepare_context(device, &lease, &transport, &context);

  status = ps_dev_lis2dux12_vendor_status(
    lis2dux12_device_id_get(&context, &result->whoami),
    &transport);
  result->whoami_hal_status = transport.last_transfer.hal_status;
  result->whoami_hal_error = transport.last_transfer.hal_error;
  result->identity_match =
    ((status == PS_STATUS_OK) && (result->whoami == LIS2DUX12_ID)) ?
      1UL : 0UL;
  if ((status == PS_STATUS_OK) && (result->identity_match == 0UL))
  {
    status = PS_STATUS_IDENTITY_MISMATCH;
  }

  for (index = 0U;
       (index < PS_DEV_LIS2DUX12_REGISTER_COUNT) &&
       (status == PS_STATUS_OK);
       ++index)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_read_reg(&context,
                         ps_dev_lis2dux12_registers[index],
                         &result->register_before[index],
                         1U),
      &transport);
    if (status == PS_STATUS_OK)
    {
      result->snapshot_ok_mask |= 1UL << index;
    }
  }

  value = 0U;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_write_reg(&context, LIS2DUX12_FIFO_CTRL, &value, 1U),
      &transport);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 0U;
    }
  }
  for (index = 1U; (index <= 5U) && (status == PS_STATUS_OK); ++index)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_write_reg(&context,
                          ps_dev_lis2dux12_registers[index],
                          &value,
                          1U),
      &transport);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << index;
    }
  }

  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_temp_disable_set(&context, PROPERTY_ENABLE),
      &transport);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 6U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_embedded_state_set(&context, PROPERTY_DISABLE),
      &transport);
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_read_reg(&context, LIS2DUX12_CTRL4, &value, 1U),
      &transport);
  }
  if (status == PS_STATUS_OK)
  {
    value &= (uint8_t)~0x18U;
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_write_reg(&context, LIS2DUX12_CTRL4, &value, 1U),
      &transport);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 7U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_mode_set(&context, &mode),
      &transport);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= (1UL << 2U) | (1UL << 8U);
    }
  }
  if (status == PS_STATUS_OK)
  {
    value = (uint8_t)(result->register_before[9] & 0x10U);
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_write_reg(&context, LIS2DUX12_CTRL1, &value, 1U),
      &transport);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 9U;
    }
  }

  for (index = 0U;
       (index < PS_DEV_LIS2DUX12_PRETERMINAL_COUNT) &&
       (status == PS_STATUS_OK);
       ++index)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_read_reg(&context,
                         ps_dev_lis2dux12_registers[index],
                         &result->register_after[index],
                         1U),
      &transport);
    if ((status == PS_STATUS_OK) &&
        (ps_dev_lis2dux12_register_matches(
           index,
           result->register_before[index],
           result->register_after[index]) != 0UL))
    {
      result->verify_ok_mask |= 1UL << index;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

  result->deep_power_down_value =
    (uint8_t)(result->register_before[10] | 0x01U);
  result->post_deep_power_down_read_omitted = 1UL;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_enter_deep_power_down(&context, PROPERTY_ENABLE),
      &transport);
    result->deep_power_down_status = status;
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 10U;
      result->terminal_deep_power_down_committed = 1UL;
    }
  }

  if ((status == PS_STATUS_OK) &&
      ((result->snapshot_ok_mask !=
        PS_DEV_LIS2DUX12_ALL_REGISTER_MASK) ||
       (result->write_ok_mask !=
        PS_DEV_LIS2DUX12_ALL_REGISTER_MASK) ||
       (result->verify_ok_mask !=
        PS_DEV_LIS2DUX12_PRETERMINAL_MASK) ||
       (result->terminal_deep_power_down_committed == 0UL)))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

  result->last_hal_status = transport.last_transfer.hal_status;
  result->last_hal_error = transport.last_transfer.hal_error;
  status = ps_dev_lis2dux12_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_LIS2DUX12_STATE_SUSPENDED;
  }
  return result->status;
}

ps_status_t ps_dev_lis2dux12_wake_low_rate(
  ps_dev_lis2dux12_t *device,
  ps_dev_lis2dux12_wake_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t lease_result;
  ps_hw_i2c3_transfer_result_t wake_probe;
  ps_dev_lis2dux12_transport_t transport;
  stmdev_ctx_t context;
  lis2dux12_md_t mode =
  {
    .odr = LIS2DUX12_1Hz6_ULP,
    .fs = LIS2DUX12_2g,
    .bw = LIS2DUX12_ODR_div_2
  };
  ps_status_t status;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->whoami_status = PS_STATUS_INTERNAL_ERROR;
  result->mode_status = PS_STATUS_INTERNAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  if (device->state != PS_DEV_LIS2DUX12_STATE_SUSPENDED)
  {
    result->status = PS_STATUS_INVALID_STATE;
    device->last_status = (uint32_t)result->status;
    return result->status;
  }
  device->operation_count++;

  lease_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_SENSOR,
    PS_DEV_LIS2DUX12_ACQUIRE_TIMEOUT_MS,
    PS_DEV_LIS2DUX12_MAX_LEASE_MS,
    &lease);
  if (lease_result.status != PS_STATUS_OK)
  {
    result->status = lease_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_LIS2DUX12_STATE_FAULT;
    return result->status;
  }

  wake_probe = ps_hw_i2c3_probe_address(
    &lease,
    device->address_7bit,
    1U,
    PS_DEV_LIS2DUX12_TRANSFER_TIMEOUT_MS);
  result->wake_probe_hal_status = wake_probe.hal_status;
  result->wake_probe_hal_error = wake_probe.hal_error;
  result->wake_probe_accepted =
    (wake_probe.status == PS_STATUS_EXPECTED_NACK) ? 1UL : 0UL;
  status = (result->wake_probe_accepted != 0UL) ?
    PS_STATUS_OK : PS_STATUS_IO_ERROR;
  status = ps_dev_lis2dux12_finish(device, &lease, status);
  if (status != PS_STATUS_OK)
  {
    result->status = status;
    return result->status;
  }

  tx_thread_sleep(
    ps_dev_lis2dux12_ms_to_ticks(PS_DEV_LIS2DUX12_WAKE_SETTLE_MS));

  lease_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_SENSOR,
    PS_DEV_LIS2DUX12_ACQUIRE_TIMEOUT_MS,
    PS_DEV_LIS2DUX12_MAX_LEASE_MS,
    &lease);
  if (lease_result.status != PS_STATUS_OK)
  {
    result->status = lease_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_LIS2DUX12_STATE_FAULT;
    return result->status;
  }
  ps_dev_lis2dux12_prepare_context(device, &lease, &transport, &context);

  status = ps_dev_lis2dux12_vendor_status(
    lis2dux12_device_id_get(&context, &result->whoami),
    &transport);
  result->whoami_status = status;
  if ((status == PS_STATUS_OK) && (result->whoami != LIS2DUX12_ID))
  {
    status = PS_STATUS_IDENTITY_MISMATCH;
    result->whoami_status = status;
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_mode_set(&context, &mode),
      &transport);
    result->mode_status = status;
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_read_reg(&context,
                         LIS2DUX12_CTRL5,
                         &result->active_ctrl5,
                         1U),
      &transport);
    if ((status == PS_STATUS_OK) &&
        (((result->active_ctrl5 >> 4U) & 0x0FU) !=
         (uint8_t)LIS2DUX12_1Hz6_ULP))
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

  result->last_hal_status = transport.last_transfer.hal_status;
  result->last_hal_error = transport.last_transfer.hal_error;
  status = ps_dev_lis2dux12_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_LIS2DUX12_STATE_LOW_RATE;
  }
  return result->status;
}

ps_status_t ps_dev_lis2dux12_suspend(
  ps_dev_lis2dux12_t *device,
  ps_dev_lis2dux12_suspend_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_lis2dux12_transport_t transport;
  stmdev_ctx_t context;
  lis2dux12_md_t mode =
  {
    .odr = LIS2DUX12_OFF,
    .fs = LIS2DUX12_2g,
    .bw = LIS2DUX12_ODR_div_2
  };
  ps_status_t status;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->mode_status = PS_STATUS_INTERNAL_ERROR;
  result->deep_power_down_status = PS_STATUS_INTERNAL_ERROR;
  result->post_deep_power_down_read_omitted = 1UL;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  if (device->state != PS_DEV_LIS2DUX12_STATE_LOW_RATE)
  {
    result->status = PS_STATUS_INVALID_STATE;
    device->last_status = (uint32_t)result->status;
    return result->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_SENSOR,
    PS_DEV_LIS2DUX12_ACQUIRE_TIMEOUT_MS,
    PS_DEV_LIS2DUX12_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    result->status = acquire_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_LIS2DUX12_STATE_FAULT;
    return result->status;
  }
  ps_dev_lis2dux12_prepare_context(device, &lease, &transport, &context);

  status = ps_dev_lis2dux12_vendor_status(
    lis2dux12_mode_set(&context, &mode),
    &transport);
  result->mode_status = status;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_lis2dux12_vendor_status(
      lis2dux12_enter_deep_power_down(&context, PROPERTY_ENABLE),
      &transport);
    result->deep_power_down_status = status;
    if (status == PS_STATUS_OK)
    {
      result->terminal_deep_power_down_committed = 1UL;
    }
  }

  result->last_hal_status = transport.last_transfer.hal_status;
  result->last_hal_error = transport.last_transfer.hal_error;
  status = ps_dev_lis2dux12_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_LIS2DUX12_STATE_SUSPENDED;
  }
  return result->status;
}

uint8_t ps_dev_lis2dux12_diagnostic_register(uint32_t index)
{
  if (index >= PS_DEV_LIS2DUX12_REGISTER_COUNT)
  {
    return 0U;
  }
  return ps_dev_lis2dux12_registers[index];
}
