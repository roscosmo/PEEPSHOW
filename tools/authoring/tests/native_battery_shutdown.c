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
static HAL_StatusTypeDef ship_status;
static uint32_t ship_calls;

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
static HAL_StatusTypeDef PS_HW6_PowerOwner_EnterSoftwareShipmentMode(void)
{
  ship_calls++;
  now += 3U;
  return ship_status;
}
typedef struct
{
  uint32_t from_state, event, to_state;
} PS_HW6_StateTransition;
#include "battery_transitions.inc"

static HAL_StatusTypeDef PS_HW6_SM_Transition(uint32_t machine, uint32_t event,
                                            HAL_StatusTypeDef status)
{
  const PS_HW6_StateTransition *table;
  uint32_t count, i;
  (void)status;
  if (machine == PS_HW6_SM_POWER)
  {
    table = ps_power_transitions;
    count = sizeof(ps_power_transitions) / sizeof(ps_power_transitions[0]);
  }
  else
  {
    assert(machine == PS_HW6_SM_PMIC);
    table = ps_pmic_transitions;
    count = sizeof(ps_pmic_transitions) / sizeof(ps_pmic_transitions[0]);
  }
  for (i = 0U; i < count; i++)
  {
    if ((table[i].from_state == g_ps_hw6_owner_sm_probe.current_state[machine]) &&
        (table[i].event == event))
    {
      g_ps_hw6_owner_sm_probe.current_state[machine] = table[i].to_state;
      return HAL_OK;
    }
  }
  return HAL_ERROR;
}
#include "battery_policy.inc"

static void reset(uint32_t boot)
{
  memset(&g_ps_hw6_owner_sm_probe, 0, sizeof(g_ps_hw6_owner_sm_probe));
  memset(&g_ps_hw6_owner_probe, 0, sizeof(g_ps_hw6_owner_probe));
  PS_HW6_BatteryShutdownReset();
  g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER] = PWR_ACTIVE_LP;
  g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] = PMIC_MONITOR;
  ps_power_battery_owns_ship_prep = 0U;
  ps_power_boot_restart_gate_pending = boot;
  ps_power_boot_restart_gate_blocked = 0U;
  g_ps_hw6_pmic_software_ship_request = 0U;
  now = 1000U;
  admission_calls = quiesce_calls = 0U;
  ship_calls = 0U;
  ship_status = HAL_OK;
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
    assert(g_ps_hw6_battery_shutdown_probe.ship_pending == TEST_SHIP_ENABLED);
    assert(g_ps_hw6_pmic_software_ship_request == 0U);
    for (i = 0U; i < 10U; i++)
    {
      now += 200U;
      (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    }
    assert(admission_calls == 3U);
    assert(g_ps_hw6_owner_sm_probe.battery_policy_software_ship_request_count == TEST_SHIP_ENABLED);
    assert(g_ps_hw6_owner_sm_probe.battery_policy_software_ship_skipped_count == !TEST_SHIP_ENABLED);

    /* Gated-off successful preparation must not leave PMIC pending on recovery. */
    g_ps_hw6_pmic_software_ship_request = 0U;
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3800U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] == PMIC_MONITOR);
    assert(ps_power_battery_owns_ship_prep == 0U);

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
  g_ps_hw6_owner_probe.power_charger_status = PS_HW6_CHARGER_STATUS_CHARGING;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  assert(g_ps_hw6_battery_shutdown_probe.attempts == 0U);
  assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] == PMIC_CHARGING);
  g_ps_hw6_owner_probe.power_vbus_ok = 0U;
  admission_status = HAL_OK;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  assert(g_ps_hw6_battery_shutdown_probe.prepared == 1U);
  assert(g_ps_hw6_owner_sm_probe.battery_policy_boot_restart_block_count == 2U);

  /* Healthy and boot-charge reads must not cancel START-only preparation. */
  for (boot = 0U; boot <= 1U; boot++)
  {
    reset(boot);
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER] = PWR_SHIP_PREP;
    g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] = PMIC_SHIP_PENDING;
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = (boot != 0U) ? 3400U : 3800U;
    g_ps_hw6_owner_probe.power_vbus_ok = boot;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER] == PWR_SHIP_PREP);
    assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_PMIC] == PMIC_SHIP_PENDING);
    assert(admission_calls == 0U && quiesce_calls == 0U);
  }

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

  for (boot = 0U; boot <= 1U; boot++)
  {
    reset(boot);
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, boot);
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    assert(ship_calls == TEST_SHIP_ENABLED);
    assert(g_ps_hw6_battery_shutdown_probe.ship_pending == 0U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_attempts == TEST_SHIP_ENABLED);
    for (i = 0U; i < 10U; i++)
    {
      now += 200U;
      (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
      PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    }
    assert(ship_calls == TEST_SHIP_ENABLED); /* HAL_OK is not a retry trigger. */

    /* Recovery cancels only battery requests, never a manual/START request. */
    reset(boot);
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, boot);
    g_ps_hw6_pmic_software_ship_request = 1U;
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3800U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_pending == 0U);
    ship_status = HAL_ERROR;
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    assert(ship_calls == 1U && g_ps_hw6_pmic_software_ship_request == 0U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_attempts == 0U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_failures == 0U);

    if (TEST_SHIP_ENABLED == 0U)
      continue;

    reset(boot);
    now = UINT32_MAX - 50U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, boot);
    assert(g_ps_hw6_owner_sm_probe.battery_policy_software_ship_last_status ==
           PS_HW6_OWNER_SM_STATUS_NOT_RUN);
    ship_status = HAL_TIMEOUT;
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 0U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_failures == 1U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_first_failure == HAL_TIMEOUT);
    assert(g_ps_hw6_battery_shutdown_probe.ship_last_status == HAL_TIMEOUT);
    assert(g_ps_hw6_owner_sm_probe.battery_policy_software_ship_last_status == HAL_TIMEOUT);
    assert(g_ps_hw6_battery_shutdown_probe.next_tick == now + 100U);
    now += 99U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    assert(ship_calls == 1U && admission_calls == 1U);
    now++;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_ERROR, 0U);
    assert(admission_calls == 1U); /* Due is insufficient without a fresh valid read. */
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 0U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(admission_calls == 1U);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3200U;
    quiesce_status = HAL_ERROR;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    assert(ship_calls == 1U && admission_calls == 2U);
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 0U);
    now = g_ps_hw6_battery_shutdown_probe.next_tick;
    quiesce_status = HAL_OK;
    ship_status = HAL_OK;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    assert(admission_calls == 3U && ship_calls == 2U);
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 1U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_last_status == HAL_OK);
    assert(g_ps_hw6_battery_shutdown_probe.ship_first_failure == HAL_TIMEOUT);

    reset(boot);
    ship_status = HAL_BUSY;
    for (i = 0U; i < 10U; i++)
    {
      (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, boot);
      PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
      now += 200U;
    }
    assert(ship_calls == 3U && admission_calls == 3U && quiesce_calls == 3U);
    assert(g_ps_hw6_battery_shutdown_probe.prepared == 0U);
    assert(g_ps_hw6_battery_shutdown_probe.exhausted == 1U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_failures == 3U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_last_status == HAL_BUSY);
    assert(g_ps_hw6_battery_shutdown_probe.ship_pending == 0U);
    assert(g_ps_hw6_owner_sm_probe.current_state[PS_HW6_SM_POWER] == PWR_SHIP_PREP);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3400U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_battery_shutdown_probe.exhausted == 1U);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3800U;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    assert(g_ps_hw6_battery_shutdown_probe.exhausted == 0U);
    assert(g_ps_hw6_battery_shutdown_probe.ship_attempts == 0U);
    g_ps_hw6_owner_probe.power_fuel_vbat_mv = 3200U;
    ship_status = HAL_OK;
    (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
    PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
    assert(ship_calls == 4U);
  }

  /* A valid boot charger recovery also cancels a queued battery request. */
  reset(1U);
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 1U);
  g_ps_hw6_owner_probe.power_vbus_ok = 1U;
  (void)PS_HW6_SM_EvaluateBatteryPolicy(HAL_OK, 0U);
  PS_HW6_OwnerStateMachines_ProcessSoftwareShipment();
  assert(ship_calls == 0U && g_ps_hw6_battery_shutdown_probe.ship_pending == 0U);
  return 0;
}
