#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "knobs_autogen.h"
#include "ps_battery_wake.h"

#undef KNOB_POWER_CRITICAL_SOFTWARE_SHIP_ENABLE
#undef KNOB_POWER_BOOT_LOW_BATTERY_SHIP_ENABLE
#define KNOB_POWER_CRITICAL_SOFTWARE_SHIP_ENABLE TEST_SHIP_ENABLED
#define KNOB_POWER_BOOT_LOW_BATTERY_SHIP_ENABLE TEST_SHIP_ENABLED
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
#include "battery_declarations.inc"
static volatile PS_HW6_BatteryShutdownProbe g_ps_hw6_battery_shutdown_probe;
static uint32_t ps_power_battery_owns_ship_prep;
static uint32_t ps_power_boot_restart_gate_pending;
static uint32_t ps_power_boot_restart_gate_blocked;
static uint32_t ps_power_battery_monitor_period_ticks = 100U;
static uint32_t g_ps_hw6_pmic_software_ship_request;
static uint32_t now, admission_calls, quiesce_calls;
static HAL_StatusTypeDef admission_status, quiesce_status;

static uint32_t tx_time_get(void) { return now; }
static uint32_t PS_HW6_SM_MsToTicks(uint32_t ms) { return (ms + 9U) / 10U; }
static HAL_StatusTypeDef PS_HW6_RequestPowerAdmission(uint32_t reason)
{
  assert(reason != 0U);
  admission_calls++;
  return admission_status;
}
static HAL_StatusTypeDef PS_HW6_RequestPowerQuiesce(uint32_t reason)
{
  assert(reason != 0U);
  quiesce_calls++;
  now += 7U;
  return quiesce_status;
}
static HAL_StatusTypeDef PS_HW6_SM_Transition(uint32_t machine, uint32_t event,
                                            HAL_StatusTypeDef status)
{
  (void)status;
  if (machine == PS_HW6_SM_POWER)
  {
    if (event == PWR_EV_SHIP_REQUEST)
      g_ps_hw6_owner_sm_probe.current_state[machine] = PWR_SHIP_PREP;
    else if (event == PWR_EV_LP_REQUEST)
      g_ps_hw6_owner_sm_probe.current_state[machine] = 0U;
  }
  return HAL_OK;
}
#include "battery_policy.inc"

static void reset(uint32_t boot)
{
  memset(&g_ps_hw6_owner_sm_probe, 0, sizeof(g_ps_hw6_owner_sm_probe));
  memset(&g_ps_hw6_owner_probe, 0, sizeof(g_ps_hw6_owner_probe));
  PS_HW6_BatteryShutdownReset();
  ps_power_battery_owns_ship_prep = 0U;
  ps_power_boot_restart_gate_pending = boot;
  ps_power_boot_restart_gate_blocked = 0U;
  g_ps_hw6_pmic_software_ship_request = 0U;
  now = 1000U;
  admission_calls = quiesce_calls = 0U;
  admission_status = quiesce_status = HAL_OK;
  g_ps_hw6_owner_probe.power_fuel_read_ok_mask = PS_HW6_BATTERY_FUEL_VBAT_OK_MASK;
  g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3200U;
  (void)PS_BatteryWake_Init(&g_ps_hw6_battery_wake_probe, 180000U, 6000U, 6000U);
}

int main(void)
{
  uint32_t boot, i;
  for (boot = 0U; boot <= 1U; boot++)
  {
    reset(boot);
    admission_status = HAL_BUSY;
    assert(PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, boot) == HAL_OK);
    assert(admission_calls == 1U && quiesce_calls == 0U);
    assert(ps_power_battery_owns_ship_prep == 1U);
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 0U);
    for (i = 0U; i < 20U; i++)
      (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(admission_calls == 1U);
    now = g_ps_hw6_battery_shutdown_probe.next_tick;
    admission_status = HAL_OK;
    quiesce_status = HAL_TIMEOUT;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(admission_calls == 2U && quiesce_calls == 1U);
    assert(g_ps_hw6_battery_shutdown_probe.last_status == HAL_TIMEOUT);
    assert(g_ps_hw6_battery_shutdown_probe.next_tick == now + 100U);
    now += 100U;
    quiesce_status = HAL_OK;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_ERROR, 0U);
    assert(admission_calls == 2U); /* Never retry using an invalid sample. */
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 0U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(admission_calls == 2U);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3200U;
    g_ps_hw6_owner_probe.power_fuel_read_ok_mask = 0U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(admission_calls == 2U);
    g_ps_hw6_owner_probe.power_fuel_read_ok_mask = PS_HW6_BATTERY_FUEL_VBAT_OK_MASK;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(admission_calls == 3U && quiesce_calls == 2U);
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 1U);
    assert(g_ps_hw6_battery_shutdown_probe.exhausted == 0U);
    assert(g_ps_hw6_pmic_software_ship_request == TEST_SHIP_ENABLED);
    for (i = 0U; i < 10U; i++)
    {
      now += 200U;
      (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    }
    assert(admission_calls == 3U);
    assert(g_ps_hw6_owner_sm_probe.battery_policy_software_ship_request_count == TEST_SHIP_ENABLED);
    assert(g_ps_hw6_owner_sm_probe.battery_policy_software_ship_skipped_count == !TEST_SHIP_ENABLED);

    reset(boot);
    quiesce_status = HAL_ERROR;
    for (i = 0U; i < 10U; i++)
    {
      (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
      now += 200U;
    }
    assert(admission_calls == 3U && quiesce_calls == 3U);
    assert(g_ps_hw6_battery_shutdown_probe.exhausted == 1U);
    assert(g_ps_hw6_battery_shutdown_probe.last_status == HAL_ERROR);
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 0U);
    assert(g_ps_hw6_pmic_software_ship_request == 0U);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3400U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_battery_shutdown_probe.exhausted == 1U);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3700U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_battery_shutdown_probe.attempts == 0U);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3200U;
    quiesce_status = HAL_OK;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 1U);
  }
  reset(1U);
  admission_status = HAL_ERROR;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 1U);
  g_ps_hw6_owner_probe.power_vbus_ok = 1U;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  assert(g_ps_hw6_battery_shutdown_probe.attempts == 0U);
  g_ps_hw6_owner_probe.power_vbus_ok = 0U;
  admission_status = HAL_OK;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  assert(g_ps_hw6_battery_shutdown_probe.prepared == 1U);
  assert(g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_block_count == 2U);

  reset(0U);
  now = UINT32_MAX - 50U;
  admission_status = HAL_ERROR;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  now += 99U;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  assert(admission_calls == 1U);
  now++;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  assert(admission_calls == 2U);
  return 0;
}
