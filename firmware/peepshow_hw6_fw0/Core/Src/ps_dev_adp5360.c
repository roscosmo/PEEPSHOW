#include "ps_dev_adp5360.h"

#include <string.h>

#define PS_DEV_ADP5360_ACQUIRE_TIMEOUT_MS  (200UL)
#define PS_DEV_ADP5360_MAX_LEASE_MS        (250UL)
#define PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS (50UL)
#define PS_DEV_ADP5360_ALL_POWER_MASK      \
  ((1UL << PS_DEV_ADP5360_POWER_REGISTER_COUNT) - 1UL)
#define PS_DEV_ADP5360_ALL_FUEL_MASK       \
  ((1UL << PS_DEV_ADP5360_FUEL_REGISTER_COUNT) - 1UL)

#define PS_DEV_ADP5360_REG_ID              (0x00U)
#define PS_DEV_ADP5360_REG_BUCK_CFG        (0x29U)
#define PS_DEV_ADP5360_REG_BUCK_OUTPUT     (0x2AU)
#define PS_DEV_ADP5360_REG_BUCKBST_CFG     (0x2BU)
#define PS_DEV_ADP5360_REG_BUCKBST_OUTPUT  (0x2CU)
#define PS_DEV_ADP5360_REG_SUPERVISORY     (0x2DU)
#define PS_DEV_ADP5360_REG_FAULT_STATUS    (0x2EU)
#define PS_DEV_ADP5360_REG_PGOOD_STATUS    (0x2FU)
#define PS_DEV_ADP5360_REG_CHARGER_STATUS_1 (0x08U)
#define PS_DEV_ADP5360_REG_CHARGER_STATUS_2 (0x09U)
#define PS_DEV_ADP5360_REG_FUEL_CFG        (0x20U)
#define PS_DEV_ADP5360_REG_FUEL_SOC        (0x21U)
#define PS_DEV_ADP5360_REG_FUEL_VBAT_H     (0x25U)
#define PS_DEV_ADP5360_REG_FUEL_VBAT_L     (0x26U)
#define PS_DEV_ADP5360_REG_FUEL_MODE       (0x27U)
#define PS_DEV_ADP5360_REG_SOC_RESET       (0x28U)
#define PS_DEV_ADP5360_FUEL_SOC_LOW_11     (0x40U)
#define PS_DEV_ADP5360_FUEL_SLEEP_CURR_10  (0x10U)
#define PS_DEV_ADP5360_FUEL_SLEEP_TIME_1   (0x00U)
#define PS_DEV_ADP5360_FUEL_ACTIVE_MODE    (0x00U)
#define PS_DEV_ADP5360_FUEL_ENABLE         (0x01U)
#define PS_DEV_ADP5360_SOC_RESET_REFRESH   (0x80U)
#define PS_DEV_ADP5360_REG_SHIPMENT        (0x36U)

#define PS_DEV_ADP5360_EXPECTED_ID         (0x10U)
#define PS_DEV_ADP5360_RAIL_PGOOD_MASK     (0x03U)
#define PS_DEV_ADP5360_CHARGER_MODE_MASK   (0x07U)
#define PS_DEV_ADP5360_BATTERY_STATUS_MASK (0x07U)
#define PS_DEV_ADP5360_BATTERY_THERM_MASK  (0xE0U)
#define PS_DEV_ADP5360_BATTERY_THERM_SHIFT (5U)
#define PS_DEV_ADP5360_PGOOD_BATOK         (0x04U)
#define PS_DEV_ADP5360_PGOOD_VBUSOK        (0x08U)
#define PS_DEV_ADP5360_PGOOD_CHG_COMPLETE  (0x10U)
#define PS_DEV_ADP5360_FAULT_TEMP_SHUTDOWN (0x01U)
#define PS_DEV_ADP5360_FAULT_WATCHDOG      (0x04U)
#define PS_DEV_ADP5360_FAULT_CHG_OV        (0x10U)
#define PS_DEV_ADP5360_CHARGER_MONITOR_MASK (0x03UL)
#define PS_DEV_ADP5360_SOC_PERCENT_MASK    (0x7FU)
#define PS_DEV_ADP5360_REGULATOR_MASK      (0x1FUL)
#define PS_DEV_ADP5360_SUPERVISORY_EN_MR_SD (0x02U)


typedef enum
{
  PS_DEV_ADP5360_CHARGER_STATUS_NOT_CHARGING = 0,
  PS_DEV_ADP5360_CHARGER_STATUS_CHARGING,
  PS_DEV_ADP5360_CHARGER_STATUS_FULL,
  PS_DEV_ADP5360_CHARGER_STATUS_DISCHARGING,
  PS_DEV_ADP5360_CHARGER_STATUS_UNKNOWN
} ps_dev_adp5360_charger_status_t;

typedef enum
{
  PS_DEV_ADP5360_CHARGE_TYPE_NONE = 0,
  PS_DEV_ADP5360_CHARGE_TYPE_TRICKLE,
  PS_DEV_ADP5360_CHARGE_TYPE_FAST,
  PS_DEV_ADP5360_CHARGE_TYPE_LONGLIFE,
  PS_DEV_ADP5360_CHARGE_TYPE_UNKNOWN
} ps_dev_adp5360_charge_type_t;

typedef enum
{
  PS_DEV_ADP5360_CHARGER_HEALTH_GOOD = 0,
  PS_DEV_ADP5360_CHARGER_HEALTH_COLD,
  PS_DEV_ADP5360_CHARGER_HEALTH_COOL,
  PS_DEV_ADP5360_CHARGER_HEALTH_WARM,
  PS_DEV_ADP5360_CHARGER_HEALTH_HOT,
  PS_DEV_ADP5360_CHARGER_HEALTH_WATCHDOG_TIMER_EXPIRE,
  PS_DEV_ADP5360_CHARGER_HEALTH_OVERVOLTAGE,
  PS_DEV_ADP5360_CHARGER_HEALTH_OVERHEAT,
  PS_DEV_ADP5360_CHARGER_HEALTH_SAFETY_TIMER_EXPIRE,
  PS_DEV_ADP5360_CHARGER_HEALTH_NO_BATTERY
} ps_dev_adp5360_charger_health_t;
static const uint8_t ps_dev_adp5360_power_registers
  [PS_DEV_ADP5360_POWER_REGISTER_COUNT] =
{
  PS_DEV_ADP5360_REG_ID,
  PS_DEV_ADP5360_REG_BUCK_CFG,
  PS_DEV_ADP5360_REG_BUCK_OUTPUT,
  PS_DEV_ADP5360_REG_BUCKBST_CFG,
  PS_DEV_ADP5360_REG_BUCKBST_OUTPUT,
  PS_DEV_ADP5360_REG_FAULT_STATUS,
  PS_DEV_ADP5360_REG_PGOOD_STATUS
};

static const uint8_t ps_dev_adp5360_power_expected
  [PS_DEV_ADP5360_POWER_REGISTER_COUNT] =
{
  PS_DEV_ADP5360_EXPECTED_ID,
  0x31U,
  0x18U,
  0x18U,
  0x13U,
  0x00U,
  0x07U
};


static uint32_t ps_dev_adp5360_decode_charger_status(uint32_t mode)
{
  switch (mode)
  {
    case 0U:
      return PS_DEV_ADP5360_CHARGER_STATUS_NOT_CHARGING;

    case 1U:
    case 2U:
    case 3U:
      return PS_DEV_ADP5360_CHARGER_STATUS_CHARGING;

    case 4U:
      return PS_DEV_ADP5360_CHARGER_STATUS_FULL;

    case 5U:
    case 6U:
    case 7U:
      return PS_DEV_ADP5360_CHARGER_STATUS_DISCHARGING;

    default:
      return PS_DEV_ADP5360_CHARGER_STATUS_UNKNOWN;
  }
}

static uint32_t ps_dev_adp5360_decode_charge_type(uint32_t mode)
{
  switch (mode)
  {
    case 0U:
    case 4U:
      return PS_DEV_ADP5360_CHARGE_TYPE_NONE;

    case 1U:
      return PS_DEV_ADP5360_CHARGE_TYPE_TRICKLE;

    case 2U:
      return PS_DEV_ADP5360_CHARGE_TYPE_FAST;

    case 3U:
      return PS_DEV_ADP5360_CHARGE_TYPE_LONGLIFE;

    default:
      return PS_DEV_ADP5360_CHARGE_TYPE_UNKNOWN;
  }
}

static uint32_t ps_dev_adp5360_decode_charger_health(
  uint32_t status1,
  uint32_t status2,
  uint32_t fault_status)
{
  uint32_t thermal;

  thermal = (status2 & PS_DEV_ADP5360_BATTERY_THERM_MASK) >>
    PS_DEV_ADP5360_BATTERY_THERM_SHIFT;
  switch (thermal)
  {
    case 1U:
      return PS_DEV_ADP5360_CHARGER_HEALTH_COLD;

    case 2U:
      return PS_DEV_ADP5360_CHARGER_HEALTH_COOL;

    case 3U:
      return PS_DEV_ADP5360_CHARGER_HEALTH_WARM;

    case 4U:
      return PS_DEV_ADP5360_CHARGER_HEALTH_HOT;

    default:
      break;
  }

  if ((fault_status & PS_DEV_ADP5360_FAULT_WATCHDOG) != 0U)
  {
    return PS_DEV_ADP5360_CHARGER_HEALTH_WATCHDOG_TIMER_EXPIRE;
  }
  if ((fault_status & PS_DEV_ADP5360_FAULT_CHG_OV) != 0U)
  {
    return PS_DEV_ADP5360_CHARGER_HEALTH_OVERVOLTAGE;
  }
  if ((fault_status & PS_DEV_ADP5360_FAULT_TEMP_SHUTDOWN) != 0U)
  {
    return PS_DEV_ADP5360_CHARGER_HEALTH_OVERHEAT;
  }
  if ((status1 & PS_DEV_ADP5360_CHARGER_MODE_MASK) == 6U)
  {
    return PS_DEV_ADP5360_CHARGER_HEALTH_SAFETY_TIMER_EXPIRE;
  }
  if ((status2 & PS_DEV_ADP5360_BATTERY_STATUS_MASK) == 1U)
  {
    return PS_DEV_ADP5360_CHARGER_HEALTH_NO_BATTERY;
  }
  return PS_DEV_ADP5360_CHARGER_HEALTH_GOOD;
}
static const uint8_t ps_dev_adp5360_fuel_registers
  [PS_DEV_ADP5360_FUEL_REGISTER_COUNT] =
{
  PS_DEV_ADP5360_REG_FUEL_CFG,
  PS_DEV_ADP5360_REG_FUEL_SOC,
  PS_DEV_ADP5360_REG_FUEL_VBAT_H,
  PS_DEV_ADP5360_REG_FUEL_VBAT_L,
  PS_DEV_ADP5360_REG_FUEL_MODE
};
static ps_status_t ps_dev_adp5360_finish(
  ps_dev_adp5360_t *device,
  ps_hw_i2c3_lease_t *lease,
  ps_dev_adp5360_power_snapshot_t *snapshot,
  ps_status_t operation_status)
{
  ps_hw_i2c3_lease_result_t release_result;

  release_result = ps_hw_i2c3_release(lease);
  snapshot->release_status = (uint32_t)release_result.status;
  snapshot->release_tx_status = release_result.tx_status;
  if ((operation_status == PS_STATUS_OK) &&
      (release_result.status != PS_STATUS_OK))
  {
    operation_status = release_result.status;
  }

  snapshot->status = operation_status;
  device->last_status = (uint32_t)operation_status;
  device->state = (operation_status == PS_STATUS_OK) ?
    PS_DEV_ADP5360_STATE_MONITOR : PS_DEV_ADP5360_STATE_FAULT;
  return operation_status;
}

ps_status_t ps_dev_adp5360_init(ps_dev_adp5360_t *device,
                                uint8_t address_7bit)
{
  if ((device == NULL) || (address_7bit > 0x7FU))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }

  (void)memset(device, 0, sizeof(*device));
  device->api_version = PS_DEV_ADP5360_API_VERSION;
  device->address_7bit = address_7bit;
  device->state = PS_DEV_ADP5360_STATE_READY;
  device->last_status = PS_STATUS_OK;
  device->initialized = 1U;
  return PS_STATUS_OK;
}

ps_status_t ps_dev_adp5360_enable_mr_shipping_mode(
  ps_dev_adp5360_t *device)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_hw_i2c3_lease_result_t release_result;
  ps_hw_i2c3_transfer_result_t transfer_result;
  ps_status_t status;
  uint8_t value = 0U;

  if (device == NULL)
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (device->initialized == 0U)
  {
    return PS_STATUS_NOT_INITIALIZED;
  }

  device->operation_count++;
  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_POWER,
    PS_DEV_ADP5360_ACQUIRE_TIMEOUT_MS,
    PS_DEV_ADP5360_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    device->last_status = (uint32_t)acquire_result.status;
    device->state = PS_DEV_ADP5360_STATE_FAULT;
    return acquire_result.status;
  }

  transfer_result = ps_hw_i2c3_mem_read(
    &lease,
    device->address_7bit,
    PS_DEV_ADP5360_REG_SUPERVISORY,
    &value,
    1U,
    PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
  status = transfer_result.status;

  if (status == PS_STATUS_OK)
  {
    value |= PS_DEV_ADP5360_SUPERVISORY_EN_MR_SD;
    transfer_result = ps_hw_i2c3_mem_write(
      &lease,
      device->address_7bit,
      PS_DEV_ADP5360_REG_SUPERVISORY,
      &value,
      1U,
      PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
    status = transfer_result.status;
  }

  release_result = ps_hw_i2c3_release(&lease);
  if ((status == PS_STATUS_OK) &&
      (release_result.status != PS_STATUS_OK))
  {
    status = release_result.status;
  }

  device->last_status = (uint32_t)status;
  device->state = (status == PS_STATUS_OK) ?
    PS_DEV_ADP5360_STATE_MONITOR : PS_DEV_ADP5360_STATE_FAULT;
  return status;
}

ps_status_t ps_dev_adp5360_prepare_fuel_gauge(
  ps_dev_adp5360_t *device)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_hw_i2c3_lease_result_t release_result;
  ps_hw_i2c3_transfer_result_t transfer_result;
  ps_status_t status;
  uint8_t value;

  if (device == NULL)
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (device->initialized == 0U)
  {
    return PS_STATUS_NOT_INITIALIZED;
  }

  device->operation_count++;
  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_POWER,
    PS_DEV_ADP5360_ACQUIRE_TIMEOUT_MS,
    PS_DEV_ADP5360_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    device->last_status = (uint32_t)acquire_result.status;
    device->state = PS_DEV_ADP5360_STATE_FAULT;
    return acquire_result.status;
  }

  value = (uint8_t)(PS_DEV_ADP5360_FUEL_SOC_LOW_11 |
                    PS_DEV_ADP5360_FUEL_SLEEP_CURR_10 |
                    PS_DEV_ADP5360_FUEL_SLEEP_TIME_1 |
                    PS_DEV_ADP5360_FUEL_ACTIVE_MODE |
                    PS_DEV_ADP5360_FUEL_ENABLE);
  transfer_result = ps_hw_i2c3_mem_write(
    &lease,
    device->address_7bit,
    PS_DEV_ADP5360_REG_FUEL_MODE,
    &value,
    1U,
    PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
  status = transfer_result.status;

  if (status == PS_STATUS_OK)
  {
    value = PS_DEV_ADP5360_SOC_RESET_REFRESH;
    transfer_result = ps_hw_i2c3_mem_write(
      &lease,
      device->address_7bit,
      PS_DEV_ADP5360_REG_SOC_RESET,
      &value,
      1U,
      PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
    status = transfer_result.status;
  }

  if (status == PS_STATUS_OK)
  {
    value = 0U;
    transfer_result = ps_hw_i2c3_mem_write(
      &lease,
      device->address_7bit,
      PS_DEV_ADP5360_REG_SOC_RESET,
      &value,
      1U,
      PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
    status = transfer_result.status;
  }

  release_result = ps_hw_i2c3_release(&lease);
  if ((status == PS_STATUS_OK) &&
      (release_result.status != PS_STATUS_OK))
  {
    status = release_result.status;
  }

  device->last_status = (uint32_t)status;
  device->state = (status == PS_STATUS_OK) ?
    PS_DEV_ADP5360_STATE_MONITOR : PS_DEV_ADP5360_STATE_FAULT;
  return status;
}

ps_status_t ps_dev_adp5360_enter_shipment_mode(
  ps_dev_adp5360_t *device)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_hw_i2c3_lease_result_t release_result;
  ps_hw_i2c3_transfer_result_t transfer_result;
  ps_status_t status;
  uint8_t value = 1U;

  if (device == NULL)
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  if (device->initialized == 0U)
  {
    return PS_STATUS_NOT_INITIALIZED;
  }

  device->operation_count++;
  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_POWER,
    PS_DEV_ADP5360_ACQUIRE_TIMEOUT_MS,
    PS_DEV_ADP5360_MAX_LEASE_MS,
    &lease);
  if (acquire_result.status != PS_STATUS_OK)
  {
    device->last_status = (uint32_t)acquire_result.status;
    device->state = PS_DEV_ADP5360_STATE_FAULT;
    return acquire_result.status;
  }

  transfer_result = ps_hw_i2c3_mem_write(
    &lease,
    device->address_7bit,
    PS_DEV_ADP5360_REG_SHIPMENT,
    &value,
    1U,
    PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
  status = transfer_result.status;

  release_result = ps_hw_i2c3_release(&lease);
  if ((status == PS_STATUS_OK) &&
      (release_result.status != PS_STATUS_OK))
  {
    status = release_result.status;
  }

  device->last_status = (uint32_t)status;
  device->state = (status == PS_STATUS_OK) ?
    PS_DEV_ADP5360_STATE_MONITOR : PS_DEV_ADP5360_STATE_FAULT;
  return status;
}

ps_status_t ps_dev_adp5360_read_power_snapshot(
  ps_dev_adp5360_t *device,
  ps_dev_adp5360_power_snapshot_t *snapshot)
{
  ps_hw_i2c3_lease_t lease;
  ps_hw_i2c3_lease_result_t acquire_result;
  ps_hw_i2c3_transfer_result_t status1_result;
  ps_hw_i2c3_transfer_result_t status2_result;
  ps_status_t status = PS_STATUS_OK;
  uint32_t index;

  if ((device == NULL) || (snapshot == NULL))
  {
    return PS_STATUS_INVALID_ARGUMENT;
  }
  (void)memset(snapshot, 0, sizeof(*snapshot));
  snapshot->status = PS_STATUS_INTERNAL_ERROR;
  snapshot->acquire_status = PS_STATUS_INTERNAL_ERROR;
  snapshot->release_status = PS_STATUS_INTERNAL_ERROR;
  for (index = 0U; index < PS_DEV_ADP5360_POWER_REGISTER_COUNT; ++index)
  {
    snapshot->register_address[index] =
      ps_dev_adp5360_power_registers[index];
    snapshot->register_status[index] = PS_STATUS_INTERNAL_ERROR;
  }
  for (index = 0U; index < PS_DEV_ADP5360_FUEL_REGISTER_COUNT; ++index)
  {
    snapshot->fuel_register_address[index] =
      ps_dev_adp5360_fuel_registers[index];
    snapshot->fuel_register_status[index] = PS_STATUS_INTERNAL_ERROR;
  }

  if (device->initialized == 0U)
  {
    snapshot->status = PS_STATUS_NOT_INITIALIZED;
    return snapshot->status;
  }
  device->operation_count++;

  acquire_result = ps_hw_i2c3_acquire(
    PS_HW_I2C3_CLIENT_POWER,
    PS_DEV_ADP5360_ACQUIRE_TIMEOUT_MS,
    PS_DEV_ADP5360_MAX_LEASE_MS,
    &lease);
  snapshot->acquire_status = (uint32_t)acquire_result.status;
  snapshot->acquire_tx_status = acquire_result.tx_status;
  if (acquire_result.status != PS_STATUS_OK)
  {
    snapshot->status = acquire_result.status;
    device->last_status = (uint32_t)acquire_result.status;
    device->state = PS_DEV_ADP5360_STATE_FAULT;
    return snapshot->status;
  }

  snapshot->function_ready_mask =
    (1UL << PS_DEV_ADP5360_FUNCTION_MFD) |
    (1UL << PS_DEV_ADP5360_FUNCTION_CHARGER) |
    (1UL << PS_DEV_ADP5360_FUNCTION_FUEL_GAUGE) |
    (1UL << PS_DEV_ADP5360_FUNCTION_REGULATOR);

  for (index = 0U; index < PS_DEV_ADP5360_POWER_REGISTER_COUNT; ++index)
  {
    ps_hw_i2c3_transfer_result_t transfer_result;
    uint8_t value = 0U;

    transfer_result = ps_hw_i2c3_mem_read(
      &lease,
      device->address_7bit,
      snapshot->register_address[index],
      &value,
      1U,
      PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
    snapshot->register_value[index] = value;
    snapshot->register_status[index] = transfer_result.status;
    snapshot->register_hal_status[index] = transfer_result.hal_status;
    snapshot->register_hal_error[index] = transfer_result.hal_error;
    snapshot->last_hal_status = transfer_result.hal_status;
    snapshot->last_hal_error = transfer_result.hal_error;

    if (transfer_result.status == PS_STATUS_OK)
    {
      snapshot->read_ok_mask |= 1UL << index;
      if (value == ps_dev_adp5360_power_expected[index])
      {
        snapshot->expected_match_mask |= 1UL << index;
      }
    }
    else if (status == PS_STATUS_OK)
    {
      status = transfer_result.status;
    }
  }

  status1_result = ps_hw_i2c3_mem_read(
    &lease,
    device->address_7bit,
    PS_DEV_ADP5360_REG_CHARGER_STATUS_1,
    &snapshot->charger_status1,
    1U,
    PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
  snapshot->charger_status1_status = (uint32_t)status1_result.status;
  snapshot->charger_status1_hal_status = status1_result.hal_status;
  snapshot->charger_status1_hal_error = status1_result.hal_error;
  snapshot->last_hal_status = status1_result.hal_status;
  snapshot->last_hal_error = status1_result.hal_error;
  if (status1_result.status == PS_STATUS_OK)
  {
    snapshot->charger_monitor_read_ok_mask |= 1UL;
  }

  status2_result = ps_hw_i2c3_mem_read(
    &lease,
    device->address_7bit,
    PS_DEV_ADP5360_REG_CHARGER_STATUS_2,
    &snapshot->charger_status2,
    1U,
    PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
  snapshot->charger_status2_status = (uint32_t)status2_result.status;
  snapshot->charger_status2_hal_status = status2_result.hal_status;
  snapshot->charger_status2_hal_error = status2_result.hal_error;
  snapshot->last_hal_status = status2_result.hal_status;
  snapshot->last_hal_error = status2_result.hal_error;
  if (status2_result.status == PS_STATUS_OK)
  {
    snapshot->charger_monitor_read_ok_mask |= 2UL;
  }

  snapshot->charger_mode =
    snapshot->charger_status1 & PS_DEV_ADP5360_CHARGER_MODE_MASK;
  snapshot->charger_status =
    ps_dev_adp5360_decode_charger_status(snapshot->charger_mode);
  snapshot->charger_charge_type =
    ps_dev_adp5360_decode_charge_type(snapshot->charger_mode);
  snapshot->battery_status =
    snapshot->charger_status2 & PS_DEV_ADP5360_BATTERY_STATUS_MASK;
  snapshot->battery_thermal_status =
    (snapshot->charger_status2 & PS_DEV_ADP5360_BATTERY_THERM_MASK) >>
    PS_DEV_ADP5360_BATTERY_THERM_SHIFT;
  snapshot->battery_present = (snapshot->battery_status == 1U) ? 0UL : 1UL;
  snapshot->vbus_ok =
    ((snapshot->register_value[6] & PS_DEV_ADP5360_PGOOD_VBUSOK) != 0U) ?
    1UL : 0UL;
  snapshot->battery_ok =
    ((snapshot->register_value[6] & PS_DEV_ADP5360_PGOOD_BATOK) != 0U) ?
    1UL : 0UL;
  snapshot->charge_complete =
    ((snapshot->register_value[6] & PS_DEV_ADP5360_PGOOD_CHG_COMPLETE) !=
     0U) ? 1UL : 0UL;
  snapshot->charger_health = ps_dev_adp5360_decode_charger_health(
    snapshot->charger_status1,
    snapshot->charger_status2,
    snapshot->register_value[5]);
  for (index = 0U; index < PS_DEV_ADP5360_FUEL_REGISTER_COUNT; ++index)
  {
    ps_hw_i2c3_transfer_result_t fuel_result;
    uint8_t value = 0U;

    fuel_result = ps_hw_i2c3_mem_read(
      &lease,
      device->address_7bit,
      snapshot->fuel_register_address[index],
      &value,
      1U,
      PS_DEV_ADP5360_TRANSFER_TIMEOUT_MS);
    snapshot->fuel_register_value[index] = value;
    snapshot->fuel_register_status[index] = fuel_result.status;
    snapshot->fuel_register_hal_status[index] = fuel_result.hal_status;
    snapshot->fuel_register_hal_error[index] = fuel_result.hal_error;
    snapshot->last_hal_status = fuel_result.hal_status;
    snapshot->last_hal_error = fuel_result.hal_error;
    if (fuel_result.status == PS_STATUS_OK)
    {
      snapshot->fuel_read_ok_mask |= 1UL << index;
    }
    else if (status == PS_STATUS_OK)
    {
      status = fuel_result.status;
    }
  }

  snapshot->fuel_soc_percent =
    snapshot->fuel_register_value[1] & PS_DEV_ADP5360_SOC_PERCENT_MASK;
  snapshot->fuel_vbat_h = snapshot->fuel_register_value[2];
  snapshot->fuel_vbat_l = snapshot->fuel_register_value[3];
  snapshot->fuel_vbat_mv =
    ((uint32_t)snapshot->fuel_vbat_h << 5U) |
    ((uint32_t)snapshot->fuel_vbat_l >> 3U);
  snapshot->identity_match =
    (snapshot->register_value[0] == PS_DEV_ADP5360_EXPECTED_ID) ? 1UL : 0UL;
  snapshot->fault_clear =
    (snapshot->register_value[5] == 0x00U) ? 1UL : 0UL;
  snapshot->rails_ready =
    ((snapshot->register_value[6] & PS_DEV_ADP5360_RAIL_PGOOD_MASK) ==
     PS_DEV_ADP5360_RAIL_PGOOD_MASK) ? 1UL : 0UL;
  snapshot->regulator_buck_cfg = snapshot->register_value[1];
  snapshot->regulator_buck_output = snapshot->register_value[2];
  snapshot->regulator_buckbst_cfg = snapshot->register_value[3];
  snapshot->regulator_buckbst_output = snapshot->register_value[4];
  snapshot->regulator_vout1_ok =
    ((snapshot->register_value[6] & 0x01U) != 0U) ? 1UL : 0UL;
  snapshot->regulator_vout2_ok =
    ((snapshot->register_value[6] & 0x02U) != 0U) ? 1UL : 0UL;
  snapshot->regulator_battery_ok =
    ((snapshot->register_value[6] & PS_DEV_ADP5360_PGOOD_BATOK) != 0U) ?
    1UL : 0UL;
  snapshot->regulator_read_ok_mask =
    ((snapshot->read_ok_mask & PS_DEV_ADP5360_REGULATOR_MASK) ==
     PS_DEV_ADP5360_REGULATOR_MASK) ? 0x1FUL : 0UL;

  if ((snapshot->read_ok_mask != PS_DEV_ADP5360_ALL_POWER_MASK) ||
      (snapshot->expected_match_mask != PS_DEV_ADP5360_ALL_POWER_MASK) ||
      (snapshot->fuel_read_ok_mask != PS_DEV_ADP5360_ALL_FUEL_MASK))
  {
    status = (status == PS_STATUS_OK) ?
      PS_STATUS_VERIFY_FAILED : status;
  }

  return ps_dev_adp5360_finish(device, &lease, snapshot, status);
}

uint8_t ps_dev_adp5360_power_register(uint32_t index)
{
  if (index >= PS_DEV_ADP5360_POWER_REGISTER_COUNT)
  {
    return 0U;
  }
  return ps_dev_adp5360_power_registers[index];
}
