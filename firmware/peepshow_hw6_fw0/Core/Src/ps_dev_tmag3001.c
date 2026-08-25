#include "ps_dev_tmag3001.h"

#include <string.h>

#include "tx_api.h"

#define PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS  (200UL)
#define PS_DEV_TMAG3001_MAX_LEASE_MS        (250UL)
#define PS_DEV_TMAG3001_TRANSFER_TIMEOUT_MS (50UL)
#define PS_DEV_TMAG3001_WAKE_SETTLE_TICKS   (1UL)
#define PS_DEV_TMAG3001_SAMPLE_SETTLE_TICKS (1UL)

#define PS_DEV_TMAG3001_REG_DEVICE_CONFIG2   (0x01U)
#define PS_DEV_TMAG3001_REG_SENSOR_CONFIG1   (0x02U)
#define PS_DEV_TMAG3001_REG_SENSOR_CONFIG2   (0x03U)
#define PS_DEV_TMAG3001_REG_THR_CONFIG1      (0x04U)
#define PS_DEV_TMAG3001_REG_THR_CONFIG2      (0x05U)
#define PS_DEV_TMAG3001_REG_THR_CONFIG3      (0x06U)
#define PS_DEV_TMAG3001_REG_SENSOR_CONFIG3   (0x07U)
#define PS_DEV_TMAG3001_REG_INT_CONFIG1      (0x08U)
#define PS_DEV_TMAG3001_REG_SENSOR_CONFIG4   (0x09U)
#define PS_DEV_TMAG3001_REG_SENSOR_CONFIG5   (0x0AU)
#define PS_DEV_TMAG3001_REG_SENSOR_CONFIG6   (0x0BU)
#define PS_DEV_TMAG3001_REG_DEVICE_ID        (0x0DU)
#define PS_DEV_TMAG3001_REG_MANUFACTURER_LSB (0x0EU)
#define PS_DEV_TMAG3001_REG_MANUFACTURER_MSB (0x0FU)
#define PS_DEV_TMAG3001_REG_X_RESULT_MSB    (0x12U)
#define PS_DEV_TMAG3001_REG_DEVICE_STATUS   (0x1CU)
#define PS_DEV_TMAG3001_SAMPLE_WINDOW_LEN    (11U)

#define PS_DEV_TMAG3001_MANUFACTURER_LSB     (0x49U)
#define PS_DEV_TMAG3001_MANUFACTURER_MSB     (0x54U)
#define PS_DEV_TMAG3001_MAG_CHANNEL_MASK     (0xF0U)
#define PS_DEV_TMAG3001_LOW_NOISE_MASK       (0x10U)
#define PS_DEV_TMAG3001_OPERATING_MODE_MASK  (0x03U)
#define PS_DEV_TMAG3001_OPERATING_STANDBY    (0x00U)
#define PS_DEV_TMAG3001_OPERATING_SLEEP      (0x01U)
#define PS_DEV_TMAG3001_OPERATING_CONTINUOUS (0x02U)
#define PS_DEV_TMAG3001_OPERATING_WAKE_SLEEP (0x03U)
#define PS_DEV_TMAG3001_ACTIVE_CHANNELS      (0x70U)
#define PS_DEV_TMAG3001_WAKE_CHANNELS        (0x30U)
#define PS_DEV_TMAG3001_SLEEP_TIME_MASK      (0x0FU)
#define PS_DEV_TMAG3001_SENSOR_CONFIG2_RANGE_MASK (0x03U)
#define PS_DEV_TMAG3001_SENSOR_CONFIG3_FIELD_THRESHOLD (0x20U)
#define PS_DEV_TMAG3001_INT_CONFIG1_OMNIPOLAR_SWITCH (0x18U)
#define PS_DEV_TMAG3001_THRESHOLD_HYSTERESIS_MASK (0xE0U)
#define PS_DEV_TMAG3001_THRESHOLD_HYSTERESIS_SHIFT (5U)
#define PS_DEV_TMAG3001_INT_CONFIG1_DISABLED     (0x01U)

#define PS_DEV_TMAG3001_WRITE_REQUIRED_MASK  (0x07UL)
#define PS_DEV_TMAG3001_VERIFY_REQUIRED_MASK (0x03UL)
#define PS_DEV_TMAG3001_SLEEP_AUDIT_WRITE_MASK  (0x0FUL)
#define PS_DEV_TMAG3001_SLEEP_AUDIT_VERIFY_MASK (0x07UL)
#define PS_DEV_TMAG3001_WAKE_SLEEP_WRITE_MASK   (0x0FFFUL)
#define PS_DEV_TMAG3001_WAKE_SLEEP_VERIFY_MASK  (0x07FFUL)

typedef struct
{
  ps_hw_i2c3_transfer_result_t last_transfer;
} ps_dev_tmag3001_transport_t;

static ps_status_t ps_dev_tmag3001_finish(
  ps_dev_tmag3001_t *device,
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
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
  }
  return operation_status;
}

static ps_status_t ps_dev_tmag3001_read(
  ps_dev_tmag3001_t *device,
  const ps_hw_i2c3_lease_t *lease,
  ps_dev_tmag3001_transport_t *transport,
  uint8_t reg,
  uint8_t *value)
{
  if ((device == NULL) || (lease == NULL) || (transport == NULL) ||
      (value == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  transport->last_transfer = ps_hw_i2c3_mem_read(
    lease,
    device->address_7bit,
    reg,
    value,
    1U,
    PS_DEV_TMAG3001_TRANSFER_TIMEOUT_MS);
  return transport->last_transfer.status;
}

static ps_status_t ps_dev_tmag3001_write(
  ps_dev_tmag3001_t *device,
  const ps_hw_i2c3_lease_t *lease,
  ps_dev_tmag3001_transport_t *transport,
  uint8_t reg,
  uint8_t value)
{
  if ((device == NULL) || (lease == NULL) || (transport == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  transport->last_transfer = ps_hw_i2c3_mem_write(
    lease,
    device->address_7bit,
    reg,
    &value,
    1U,
    PS_DEV_TMAG3001_TRANSFER_TIMEOUT_MS);
  return transport->last_transfer.status;
}

static ps_status_t ps_dev_tmag3001_write_verify(
  ps_dev_tmag3001_t *device,
  const ps_hw_i2c3_lease_t *lease,
  ps_dev_tmag3001_transport_t *transport,
  uint8_t reg,
  uint8_t target,
  uint8_t *readback)
{
  ps_status_t status;
  uint8_t value = 0U;

  status = ps_dev_tmag3001_write(
    device, lease, transport, reg, target);
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device, lease, transport, reg, &value);
  }
  if (readback != NULL)
  {
    *readback = value;
  }
  if ((status == PS_STATUS_OK) && (value != target))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }
  return status;
}

static ps_status_t ps_dev_tmag3001_probe_identity(
  ps_dev_tmag3001_t *device,
  const ps_hw_i2c3_lease_t *lease,
  ps_dev_tmag3001_transport_t *transport,
  uint8_t *device_id,
  uint8_t *manufacturer_lsb,
  uint8_t *manufacturer_msb,
  uint32_t *identity_match)
{
  ps_status_t status;

  status = ps_dev_tmag3001_read(
    device, lease, transport, PS_DEV_TMAG3001_REG_DEVICE_ID, device_id);
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      lease,
      transport,
      PS_DEV_TMAG3001_REG_MANUFACTURER_LSB,
      manufacturer_lsb);
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      lease,
      transport,
      PS_DEV_TMAG3001_REG_MANUFACTURER_MSB,
      manufacturer_msb);
  }

  *identity_match =
    ((status == PS_STATUS_OK) &&
     (*manufacturer_lsb == PS_DEV_TMAG3001_MANUFACTURER_LSB) &&
     (*manufacturer_msb == PS_DEV_TMAG3001_MANUFACTURER_MSB)) ? 1UL : 0UL;
  if ((status == PS_STATUS_OK) && (*identity_match == 0UL))
  {
    status = PS_STATUS_IDENTITY_MISMATCH;
  }
  return status;
}

static void ps_dev_tmag3001_set_last_hal(
  const ps_dev_tmag3001_transport_t *transport,
  uint32_t *last_hal_status,
  uint32_t *last_hal_error)
{
  *last_hal_status = transport->last_transfer.hal_status;
  *last_hal_error = transport->last_transfer.hal_error;
}

ps_status_t ps_dev_tmag3001_init(ps_dev_tmag3001_t *device,
                                 uint8_t address_7bit)
{
  if ((device == NULL) || (address_7bit > 0x7FU))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(device, 0, sizeof(*device));
  device->api_version = PS_DEV_TMAG3001_API_VERSION;
  device->address_7bit = address_7bit;
  device->state = PS_DEV_TMAG3001_STATE_READY;
  device->last_status = PS_STATUS_OK;
  device->initialized = 1U;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_tmag3001_stabilize_suspended(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_stabilize_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_tmag3001_transport_t transport;
  ps_status_t status;
  uint8_t value = 0U;
  uint8_t sensor_config1 = 0U;
  uint8_t device_config2 = 0U;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->ready_status = PS_STATUS_INTERNAL_ERROR;
  result->identity_status = PS_STATUS_INTERNAL_ERROR;
  result->sensor_config1_verify_status = PS_STATUS_INTERNAL_ERROR;
  result->device_config2_verify_status = PS_STATUS_INTERNAL_ERROR;
  result->sleep_write_status = PS_STATUS_INTERNAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_INPUT,
    PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS,
    PS_DEV_TMAG3001_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    result->status = acquire_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
    return result->status;
  }
  (void)memset(&transport, 0, sizeof(transport));

  status = ps_dev_tmag3001_probe_identity(
    device,
    &lease,
    &transport,
    &result->device_id,
    &result->manufacturer_lsb,
    &result->manufacturer_msb,
    &result->identity_match);
  result->ready_status = status;
  result->identity_status = status;

  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      &sensor_config1);
  }
  if (status == PS_STATUS_OK)
  {
    result->sensor_config1_before = sensor_config1;
    sensor_config1 &= (uint8_t)~PS_DEV_TMAG3001_MAG_CHANNEL_MASK;
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      sensor_config1);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 0U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      &value);
    result->sensor_config1_verify_status = status;
    if ((status == PS_STATUS_OK) && (value == sensor_config1))
    {
      result->sensor_config1_after = value;
      result->verify_ok_mask |= 1UL << 0U;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &device_config2);
  }
  if (status == PS_STATUS_OK)
  {
    result->device_config2_before = device_config2;
    device_config2 =
      (uint8_t)((device_config2 &
                 (uint8_t)~(PS_DEV_TMAG3001_LOW_NOISE_MASK |
                            PS_DEV_TMAG3001_OPERATING_MODE_MASK)) |
                PS_DEV_TMAG3001_OPERATING_STANDBY);
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      device_config2);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 1U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &value);
    result->device_config2_verify_status = status;
    if ((status == PS_STATUS_OK) && (value == device_config2))
    {
      result->device_config2_after = value;
      result->verify_ok_mask |= 1UL << 1U;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

  result->device_config2_sleep =
    (uint8_t)(device_config2 | PS_DEV_TMAG3001_OPERATING_SLEEP);
  result->post_sleep_read_omitted = 1UL;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      result->device_config2_sleep);
    result->sleep_write_status = status;
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 2U;
      result->terminal_sleep_committed = 1UL;
    }
  }

  if ((status == PS_STATUS_OK) &&
      ((result->write_ok_mask != PS_DEV_TMAG3001_WRITE_REQUIRED_MASK) ||
       (result->verify_ok_mask != PS_DEV_TMAG3001_VERIFY_REQUIRED_MASK) ||
       (result->terminal_sleep_committed == 0UL)))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

  ps_dev_tmag3001_set_last_hal(
    &transport, &result->last_hal_status, &result->last_hal_error);
  status = ps_dev_tmag3001_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_TMAG3001_STATE_SUSPENDED;
  }
  return result->status;
}

ps_status_t ps_dev_tmag3001_wake_continuous(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_wake_result_t *result)
{
  return ps_dev_tmag3001_wake_continuous_with_range(device, 0U, 0U, result);
}

ps_status_t ps_dev_tmag3001_wake_continuous_with_range(
  ps_dev_tmag3001_t *device,
  uint8_t range_override_mask,
  uint8_t range_override_value,
  ps_dev_tmag3001_wake_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_tmag3001_transport_t transport;
  ps_status_t status;
  uint8_t value = 0U;
  uint8_t device_id = 0U;
  uint8_t manufacturer_lsb = 0U;
  uint8_t manufacturer_msb = 0U;
  uint32_t identity_match = 0UL;
  uint8_t sensor_config1 = 0U;
  uint8_t sensor_config2 = 0U;
  uint8_t device_config2 = 0U;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->active_status = PS_STATUS_INTERNAL_ERROR;
  result->sensor_config2_status = PS_STATUS_INTERNAL_ERROR;
  result->sensor_config2_restore_status = PS_STATUS_OK;
  result->preclear_device_status_read_status = PS_STATUS_INTERNAL_ERROR;
  result->range_override_mask = (uint32_t)range_override_mask;
  result->range_override_value = (uint32_t)range_override_value;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  if ((device->state != PS_DEV_TMAG3001_STATE_SUSPENDED) &&
      (device->state != PS_DEV_TMAG3001_STATE_WAKE_SLEEP))
  {
    result->status = PS_STATUS_INVALID_STATE;
    device->last_status = (uint32_t)result->status;
    return result->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_INPUT,
    PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS,
    PS_DEV_TMAG3001_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    result->status = acquire_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
    return result->status;
  }
  (void)memset(&transport, 0, sizeof(transport));

  result->wake_probe_status = ps_dev_tmag3001_read(
    device,
    &lease,
    &transport,
    PS_DEV_TMAG3001_REG_DEVICE_STATUS,
    &result->preclear_device_status);
  result->preclear_device_status_read_status = result->wake_probe_status;
  tx_thread_sleep(PS_DEV_TMAG3001_WAKE_SETTLE_TICKS);
  status = ps_dev_tmag3001_probe_identity(
    device,
    &lease,
    &transport,
    &device_id,
    &manufacturer_lsb,
    &manufacturer_msb,
    &identity_match);
  result->wake_retry_status = status;

  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_INT_CONFIG1,
      PS_DEV_TMAG3001_INT_CONFIG1_DISABLED,
      &value);
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      &sensor_config1);
  }
  if (status == PS_STATUS_OK)
  {
    sensor_config1 =
      (uint8_t)((sensor_config1 & (uint8_t)~PS_DEV_TMAG3001_MAG_CHANNEL_MASK) |
                PS_DEV_TMAG3001_ACTIVE_CHANNELS);
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      sensor_config1);
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG2,
      &sensor_config2);
    result->sensor_config2_status = status;
  }
  if (status == PS_STATUS_OK)
  {
    result->sensor_config2_before = sensor_config2;
    if (range_override_mask != 0U)
    {
      sensor_config2 =
        (uint8_t)((sensor_config2 & (uint8_t)~range_override_mask) |
                  (range_override_value & range_override_mask));
      status = ps_dev_tmag3001_write(
        device,
        &lease,
        &transport,
        PS_DEV_TMAG3001_REG_SENSOR_CONFIG2,
        sensor_config2);
      if (status == PS_STATUS_OK)
      {
        result->range_override_applied = 1UL;
      }
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG2,
      &value);
    result->sensor_config2_status = status;
    if ((status == PS_STATUS_OK) && (value == sensor_config2))
    {
      result->active_sensor_config2 = value;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &device_config2);
  }
  if (status == PS_STATUS_OK)
  {
    device_config2 =
      (uint8_t)((device_config2 &
                 (uint8_t)~(PS_DEV_TMAG3001_LOW_NOISE_MASK |
                            PS_DEV_TMAG3001_OPERATING_MODE_MASK)) |
                PS_DEV_TMAG3001_OPERATING_CONTINUOUS);
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      device_config2);
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      &value);
    if ((status == PS_STATUS_OK) && (value == sensor_config1))
    {
      result->active_sensor_config1 = value;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &value);
    if ((status == PS_STATUS_OK) && (value == device_config2))
    {
      result->active_device_config2 = value;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

  result->active_status = status;
  ps_dev_tmag3001_set_last_hal(
    &transport, &result->last_hal_status, &result->last_hal_error);
  status = ps_dev_tmag3001_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_TMAG3001_STATE_ACTIVE;
  }
  return result->status;
}

ps_status_t ps_dev_tmag3001_set_sensor_config2(
  ps_dev_tmag3001_t *device,
  uint8_t value,
  uint8_t *readback)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_tmag3001_transport_t transport;
  ps_status_t status;
  uint8_t verify_value = 0U;

  if (device == NULL)
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (readback != NULL)
  {
    *readback = 0U;
  }
  if (device->initialized == 0U)
  {
    device->last_status = (uint32_t)PS_STATUS_NOT_INITIALIZED;
    return PS_STATUS_NOT_INITIALIZED;
  }
  if (device->state != PS_DEV_TMAG3001_STATE_ACTIVE)
  {
    device->last_status = (uint32_t)PS_STATUS_INVALID_STATE;
    return PS_STATUS_INVALID_STATE;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_INPUT,
    PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS,
    PS_DEV_TMAG3001_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    device->last_status = (uint32_t)acquire_result.status;
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
    return acquire_result.status;
  }
  (void)memset(&transport, 0, sizeof(transport));

  status = ps_dev_tmag3001_write(
    device,
    &lease,
    &transport,
    PS_DEV_TMAG3001_REG_SENSOR_CONFIG2,
    value);
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG2,
      &verify_value);
  }
  if (readback != NULL)
  {
    *readback = verify_value;
  }
  if ((status == PS_STATUS_OK) && (verify_value != value))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }
  status = ps_dev_tmag3001_finish(device, &lease, status);
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_TMAG3001_STATE_ACTIVE;
  }
  return status;
}


ps_status_t ps_dev_tmag3001_prepare_sleep(
  ps_dev_tmag3001_t *device,
  uint8_t int_config1_target,
  ps_dev_tmag3001_sleep_audit_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_tmag3001_transport_t transport;
  ps_status_t status;
  uint8_t value = 0U;
  uint8_t sensor_config1 = 0U;
  uint8_t device_config2 = 0U;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->ready_status = PS_STATUS_INTERNAL_ERROR;
  result->identity_status = PS_STATUS_INTERNAL_ERROR;
  result->sensor_config1_verify_status = PS_STATUS_INTERNAL_ERROR;
  result->int_config1_verify_status = PS_STATUS_INTERNAL_ERROR;
  result->device_config2_verify_status = PS_STATUS_INTERNAL_ERROR;
  result->sleep_write_status = PS_STATUS_INTERNAL_ERROR;
  result->int_config1_target = int_config1_target;

  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_INPUT,
    PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS,
    PS_DEV_TMAG3001_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    result->status = acquire_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
    return result->status;
  }
  (void)memset(&transport, 0, sizeof(transport));

  status = ps_dev_tmag3001_probe_identity(
    device,
    &lease,
    &transport,
    &result->device_id,
    &result->manufacturer_lsb,
    &result->manufacturer_msb,
    &result->identity_match);
  result->ready_status = status;
  result->identity_status = status;

  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      &sensor_config1);
  }
  if (status == PS_STATUS_OK)
  {
    result->sensor_config1_before = sensor_config1;
    sensor_config1 &= (uint8_t)~PS_DEV_TMAG3001_MAG_CHANNEL_MASK;
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      sensor_config1);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 0U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      &value);
    result->sensor_config1_verify_status = status;
    if ((status == PS_STATUS_OK) && (value == sensor_config1))
    {
      result->sensor_config1_after = value;
      result->verify_ok_mask |= 1UL << 0U;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_INT_CONFIG1,
      &value);
  }
  if (status == PS_STATUS_OK)
  {
    result->int_config1_before = value;
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_INT_CONFIG1,
      result->int_config1_target);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 1U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_INT_CONFIG1,
      &value);
    result->int_config1_verify_status = status;
    if ((status == PS_STATUS_OK) && (value == result->int_config1_target))
    {
      result->int_config1_after = value;
      result->verify_ok_mask |= 1UL << 1U;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &device_config2);
  }
  if (status == PS_STATUS_OK)
  {
    result->device_config2_before = device_config2;
    device_config2 =
      (uint8_t)((device_config2 &
                 (uint8_t)~(PS_DEV_TMAG3001_LOW_NOISE_MASK |
                            PS_DEV_TMAG3001_OPERATING_MODE_MASK)) |
                PS_DEV_TMAG3001_OPERATING_STANDBY);
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      device_config2);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 2U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &value);
    result->device_config2_verify_status = status;
    if ((status == PS_STATUS_OK) && (value == device_config2))
    {
      result->device_config2_after = value;
      result->verify_ok_mask |= 1UL << 2U;
    }
    else if (status == PS_STATUS_OK)
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }

  result->device_config2_sleep =
    (uint8_t)(device_config2 | PS_DEV_TMAG3001_OPERATING_SLEEP);
  result->post_sleep_read_omitted = 1UL;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      result->device_config2_sleep);
    result->sleep_write_status = status;
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 3U;
      result->terminal_sleep_committed = 1UL;
    }
  }

  if ((status == PS_STATUS_OK) &&
      ((result->write_ok_mask != PS_DEV_TMAG3001_SLEEP_AUDIT_WRITE_MASK) ||
       (result->verify_ok_mask != PS_DEV_TMAG3001_SLEEP_AUDIT_VERIFY_MASK) ||
       (result->terminal_sleep_committed == 0UL)))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

  ps_dev_tmag3001_set_last_hal(
    &transport, &result->last_hal_status, &result->last_hal_error);
  status = ps_dev_tmag3001_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_TMAG3001_STATE_SUSPENDED;
  }
  return result->status;
}

ps_status_t ps_dev_tmag3001_prepare_sleep_audit(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_sleep_audit_result_t *result)
{
  return ps_dev_tmag3001_prepare_sleep(
    device,
    PS_DEV_TMAG3001_INT_CONFIG1_DISABLED,
    result);
}

ps_status_t ps_dev_tmag3001_read_raw_sample(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_raw_sample_t *sample)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_tmag3001_transport_t transport;
  uint8_t data[PS_DEV_TMAG3001_SAMPLE_WINDOW_LEN];
  ps_status_t status;

  if ((device == NULL) || (sample == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(sample, 0, sizeof(*sample));
  sample->status = PS_STATUS_INTERNAL_ERROR;
  if (device->initialized == 0U)
  {
    sample->status = PS_STATUS_NOT_INITIALIZED;
    return sample->status;
  }
  if (device->state != PS_DEV_TMAG3001_STATE_ACTIVE)
  {
    sample->status = PS_STATUS_INVALID_STATE;
    device->last_status = (uint32_t)sample->status;
    return sample->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_INPUT,
    PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS,
    PS_DEV_TMAG3001_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    sample->status = acquire_result.status;
    device->last_status = (uint32_t)sample->status;
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
    return sample->status;
  }
  (void)memset(&transport, 0, sizeof(transport));
  (void)memset(data, 0, sizeof(data));
  tx_thread_sleep(PS_DEV_TMAG3001_SAMPLE_SETTLE_TICKS);

  transport.last_transfer = ps_hw_i2c3_mem_read(
    &lease,
    device->address_7bit,
    PS_DEV_TMAG3001_REG_X_RESULT_MSB,
    data,
    PS_DEV_TMAG3001_SAMPLE_WINDOW_LEN,
    PS_DEV_TMAG3001_TRANSFER_TIMEOUT_MS);
  status = transport.last_transfer.status;
  if (status == PS_STATUS_OK)
  {
    sample->x = (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
    sample->y = (int16_t)(((uint16_t)data[2] << 8) | (uint16_t)data[3]);
    sample->z = (int16_t)(((uint16_t)data[4] << 8) | (uint16_t)data[5]);
    sample->conv_status = data[6];
    sample->magnitude_result = data[9];
    sample->device_status = data[10];
  }

  ps_dev_tmag3001_set_last_hal(
    &transport, &sample->last_hal_status, &sample->last_hal_error);
  status = ps_dev_tmag3001_finish(device, &lease, status);
  sample->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_TMAG3001_STATE_ACTIVE;
  }
  return sample->status;
}

ps_status_t ps_dev_tmag3001_prepare_wake_sleep_omnipolar_xy(
  ps_dev_tmag3001_t *device,
  uint8_t sleep_period_code,
  uint8_t field_threshold_code,
  uint8_t field_hysteresis_code,
  ps_dev_tmag3001_wake_sleep_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_tmag3001_transport_t transport;
  ps_status_t status;
  uint8_t device_config2 = 0U;
  uint8_t sensor_config2 = 0U;

  if ((device == NULL) || (result == NULL) ||
      (sleep_period_code > 0x0CU) ||
      (field_threshold_code == 0U) ||
      (field_threshold_code > 0x7FU) ||
      (field_hysteresis_code > 0x07U))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->identity_status = PS_STATUS_INTERNAL_ERROR;
  result->terminal_write_status = PS_STATUS_INTERNAL_ERROR;
  result->sleep_period_code = sleep_period_code;
  result->field_threshold_code = field_threshold_code;
  result->field_hysteresis_code = field_hysteresis_code;
  result->sensor_config1_target =
    (uint8_t)(PS_DEV_TMAG3001_WAKE_CHANNELS |
              (sleep_period_code & PS_DEV_TMAG3001_SLEEP_TIME_MASK));
  result->sensor_config3_target =
    PS_DEV_TMAG3001_SENSOR_CONFIG3_FIELD_THRESHOLD;
  result->int_config1_target =
    PS_DEV_TMAG3001_INT_CONFIG1_OMNIPOLAR_SWITCH;

  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  if (device->state == PS_DEV_TMAG3001_STATE_FAULT)
  {
    result->status = PS_STATUS_INVALID_STATE;
    device->last_status = (uint32_t)result->status;
    return result->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_INPUT,
    PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS,
    PS_DEV_TMAG3001_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    result->status = acquire_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
    return result->status;
  }
  (void)memset(&transport, 0, sizeof(transport));

  (void)ps_dev_tmag3001_read(
    device,
    &lease,
    &transport,
    PS_DEV_TMAG3001_REG_DEVICE_ID,
    &result->device_id);
  tx_thread_sleep(PS_DEV_TMAG3001_WAKE_SETTLE_TICKS);
  status = ps_dev_tmag3001_probe_identity(
    device,
    &lease,
    &transport,
    &result->device_id,
    &result->manufacturer_lsb,
    &result->manufacturer_msb,
    &result->identity_match);
  result->identity_status = status;

  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG2,
      &sensor_config2);
  }
  if (status == PS_STATUS_OK)
  {
    result->sensor_config2_target =
      (uint8_t)(sensor_config2 & PS_DEV_TMAG3001_SENSOR_CONFIG2_RANGE_MASK);
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      result->sensor_config1_target,
      &result->sensor_config1_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 0U;
      result->verify_ok_mask |= 1UL << 0U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG2,
      result->sensor_config2_target,
      &result->sensor_config2_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 1U;
      result->verify_ok_mask |= 1UL << 1U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG3,
      result->sensor_config3_target,
      &result->sensor_config3_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 2U;
      result->verify_ok_mask |= 1UL << 2U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_THR_CONFIG1,
      field_threshold_code,
      &result->threshold_x_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 3U;
      result->verify_ok_mask |= 1UL << 3U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_THR_CONFIG2,
      field_threshold_code,
      &result->threshold_y_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 4U;
      result->verify_ok_mask |= 1UL << 4U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_THR_CONFIG3,
      0U,
      &result->threshold_z_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 5U;
      result->verify_ok_mask |= 1UL << 5U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG4,
      0U,
      &result->threshold_x_high_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 6U;
      result->verify_ok_mask |= 1UL << 6U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG5,
      0U,
      &result->threshold_y_high_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 7U;
      result->verify_ok_mask |= 1UL << 7U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG6,
      0U,
      &result->threshold_z_high_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 8U;
      result->verify_ok_mask |= 1UL << 8U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_INT_CONFIG1,
      result->int_config1_target,
      &result->int_config1_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 9U;
      result->verify_ok_mask |= 1UL << 9U;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &device_config2);
  }
  if (status == PS_STATUS_OK)
  {
    result->device_config2_standby =
      (uint8_t)((device_config2 &
                 (uint8_t)~(PS_DEV_TMAG3001_THRESHOLD_HYSTERESIS_MASK |
                            PS_DEV_TMAG3001_LOW_NOISE_MASK |
                            PS_DEV_TMAG3001_OPERATING_MODE_MASK)) |
                (uint8_t)(field_hysteresis_code <<
                          PS_DEV_TMAG3001_THRESHOLD_HYSTERESIS_SHIFT) |
                PS_DEV_TMAG3001_OPERATING_STANDBY);
    status = ps_dev_tmag3001_write_verify(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      result->device_config2_standby,
      &result->device_config2_after);
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 10U;
      result->verify_ok_mask |= 1UL << 10U;
    }
  }

  result->device_config2_wake_sleep =
    (uint8_t)(result->device_config2_standby |
              PS_DEV_TMAG3001_OPERATING_WAKE_SLEEP);
  result->post_terminal_read_omitted = 1UL;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      result->device_config2_wake_sleep);
    result->terminal_write_status = status;
    if (status == PS_STATUS_OK)
    {
      result->write_ok_mask |= 1UL << 11U;
      result->terminal_write_committed = 1UL;
    }
  }

  if ((status == PS_STATUS_OK) &&
      ((result->write_ok_mask != PS_DEV_TMAG3001_WAKE_SLEEP_WRITE_MASK) ||
       (result->verify_ok_mask != PS_DEV_TMAG3001_WAKE_SLEEP_VERIFY_MASK) ||
       (result->terminal_write_committed == 0UL)))
  {
    status = PS_STATUS_VERIFY_FAILED;
  }

  ps_dev_tmag3001_set_last_hal(
    &transport, &result->last_hal_status, &result->last_hal_error);
  status = ps_dev_tmag3001_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_TMAG3001_STATE_WAKE_SLEEP;
  }
  return result->status;
}

ps_status_t ps_dev_tmag3001_suspend(
  ps_dev_tmag3001_t *device,
  ps_dev_tmag3001_suspend_result_t *result)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_dev_tmag3001_transport_t transport;
  ps_status_t status;
  uint8_t value = 0U;
  uint8_t sensor_config1 = 0U;
  uint8_t device_config2 = 0U;

  if ((device == NULL) || (result == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(result, 0, sizeof(*result));
  result->status = PS_STATUS_INTERNAL_ERROR;
  result->sleep_status = PS_STATUS_INTERNAL_ERROR;
  if (device->initialized == 0U)
  {
    result->status = PS_STATUS_NOT_INITIALIZED;
    return result->status;
  }
  if (device->state != PS_DEV_TMAG3001_STATE_ACTIVE)
  {
    result->status = PS_STATUS_INVALID_STATE;
    device->last_status = (uint32_t)result->status;
    return result->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_INPUT,
    PS_DEV_TMAG3001_ACQUIRE_TIMEOUT_MS,
    PS_DEV_TMAG3001_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    result->status = acquire_result.status;
    device->last_status = (uint32_t)result->status;
    device->state = PS_DEV_TMAG3001_STATE_FAULT;
    return result->status;
  }
  (void)memset(&transport, 0, sizeof(transport));

  status = ps_dev_tmag3001_read(
    device,
    &lease,
    &transport,
    PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
    &sensor_config1);
  if (status == PS_STATUS_OK)
  {
    sensor_config1 &= (uint8_t)~PS_DEV_TMAG3001_MAG_CHANNEL_MASK;
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      sensor_config1);
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_SENSOR_CONFIG1,
      &value);
    if ((status == PS_STATUS_OK) && (value != sensor_config1))
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &device_config2);
  }
  if (status == PS_STATUS_OK)
  {
    device_config2 =
      (uint8_t)(device_config2 &
                (uint8_t)~(PS_DEV_TMAG3001_LOW_NOISE_MASK |
                           PS_DEV_TMAG3001_OPERATING_MODE_MASK));
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      device_config2);
  }
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_read(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      &value);
    if ((status == PS_STATUS_OK) && (value != device_config2))
    {
      status = PS_STATUS_VERIFY_FAILED;
    }
  }
  result->post_sleep_read_omitted = 1UL;
  if (status == PS_STATUS_OK)
  {
    status = ps_dev_tmag3001_write(
      device,
      &lease,
      &transport,
      PS_DEV_TMAG3001_REG_DEVICE_CONFIG2,
      (uint8_t)(device_config2 | PS_DEV_TMAG3001_OPERATING_SLEEP));
    result->sleep_status = status;
    if (status == PS_STATUS_OK)
    {
      result->terminal_sleep_committed = 1UL;
    }
  }

  ps_dev_tmag3001_set_last_hal(
    &transport, &result->last_hal_status, &result->last_hal_error);
  status = ps_dev_tmag3001_finish(device, &lease, status);
  result->status = status;
  if (status == PS_STATUS_OK)
  {
    device->state = PS_DEV_TMAG3001_STATE_SUSPENDED;
  }
  return result->status;
}
