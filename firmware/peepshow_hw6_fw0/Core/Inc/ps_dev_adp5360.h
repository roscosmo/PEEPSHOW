#ifndef PS_DEV_ADP5360_H
#define PS_DEV_ADP5360_H

#include <stdint.h>

#include "ps_hw_i2c3.h"
#include "ps_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PS_DEV_ADP5360_API_VERSION       (4UL)
#define PS_DEV_ADP5360_POWER_REGISTER_COUNT (7UL)
#define PS_DEV_ADP5360_FUEL_REGISTER_COUNT  (5UL)

typedef enum
{
  PS_DEV_ADP5360_STATE_UNINITIALIZED = 0,
  PS_DEV_ADP5360_STATE_READY,
  PS_DEV_ADP5360_STATE_MONITOR,
  PS_DEV_ADP5360_STATE_FAULT
} ps_dev_adp5360_state_t;

typedef enum
{
  PS_DEV_ADP5360_FUNCTION_MFD = 0,
  PS_DEV_ADP5360_FUNCTION_CHARGER,
  PS_DEV_ADP5360_FUNCTION_FUEL_GAUGE,
  PS_DEV_ADP5360_FUNCTION_REGULATOR,
  PS_DEV_ADP5360_FUNCTION_COUNT
} ps_dev_adp5360_function_t;

typedef struct
{
  uint32_t api_version;
  uint32_t initialized;
  uint32_t operation_count;
  uint32_t state;
  uint32_t last_status;
  uint8_t address_7bit;
} ps_dev_adp5360_t;

typedef struct
{
  ps_status_t status;
  uint32_t function_ready_mask;
  uint32_t acquire_status;
  uint32_t acquire_tx_status;
  uint32_t release_status;
  uint32_t release_tx_status;
  uint8_t register_address[PS_DEV_ADP5360_POWER_REGISTER_COUNT];
  uint8_t register_value[PS_DEV_ADP5360_POWER_REGISTER_COUNT];
  ps_status_t register_status[PS_DEV_ADP5360_POWER_REGISTER_COUNT];
  uint32_t register_hal_status[PS_DEV_ADP5360_POWER_REGISTER_COUNT];
  uint32_t register_hal_error[PS_DEV_ADP5360_POWER_REGISTER_COUNT];
  uint32_t read_ok_mask;
  uint32_t expected_match_mask;
  uint32_t identity_match;
  uint32_t rails_ready;
  uint32_t fault_clear;
  uint32_t charger_status1_status;
  uint32_t charger_status2_status;
  uint32_t charger_status1_hal_status;
  uint32_t charger_status1_hal_error;
  uint32_t charger_status2_hal_status;
  uint32_t charger_status2_hal_error;
  uint8_t charger_status1;
  uint8_t charger_status2;
  uint32_t charger_monitor_read_ok_mask;
  uint32_t charger_mode;
  uint32_t charger_status;
  uint32_t charger_charge_type;
  uint32_t charger_health;
  uint32_t battery_status;
  uint32_t battery_thermal_status;
  uint32_t battery_present;
  uint32_t vbus_ok;
  uint32_t battery_ok;
  uint32_t charge_complete;
  uint8_t fuel_register_address[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint8_t fuel_register_value[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  ps_status_t fuel_register_status[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t fuel_register_hal_status[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t fuel_register_hal_error[PS_DEV_ADP5360_FUEL_REGISTER_COUNT];
  uint32_t fuel_read_ok_mask;
  uint32_t fuel_soc_percent;
  uint32_t fuel_vbat_mv;
  uint8_t fuel_vbat_h;
  uint8_t fuel_vbat_l;
  uint32_t regulator_read_ok_mask;
  uint8_t regulator_buck_cfg;
  uint8_t regulator_buck_output;
  uint8_t regulator_buckbst_cfg;
  uint8_t regulator_buckbst_output;
  uint32_t regulator_vout1_ok;
  uint32_t regulator_vout2_ok;
  uint32_t regulator_battery_ok;
  uint32_t last_hal_status;
  uint32_t last_hal_error;
} ps_dev_adp5360_power_snapshot_t;

ps_status_t ps_dev_adp5360_init(ps_dev_adp5360_t *device,
                                uint8_t address_7bit);
ps_status_t ps_dev_adp5360_enable_mr_shipping_mode(
  ps_dev_adp5360_t *device);
ps_status_t ps_dev_adp5360_read_power_snapshot(
  ps_dev_adp5360_t *device,
  ps_dev_adp5360_power_snapshot_t *snapshot);
uint8_t ps_dev_adp5360_power_register(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* PS_DEV_ADP5360_H */
