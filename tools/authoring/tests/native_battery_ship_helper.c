#include <stdint.h>
#include "ps_hw6_owner_state_machines.h"
#include "ps_hw6_owner_services.h"
#include "ps_battery_wake.h"
#include "timing_probe.inc"

/* Host-only memory fixture. There is no PMIC driver or request consumer. */
volatile uint32_t g_ps_hw6_pmic_software_ship_request;
volatile PS_HW6_BatteryShutdownProbe g_ps_hw6_battery_shutdown_probe = {
  .api_version = 2U, .reason = 3U, .attempts = 1U, .prepared = 1U
};
volatile PS_HW6_BatteryFaultWaitProbe g_ps_hw6_battery_fault_wait_probe = {.api_version = 1U};
volatile PS_HW6_BatteryFaultTestProbe g_ps_hw6_battery_fault_test_probe = {.api_version = 2U};
volatile ps_battery_wake_t g_ps_hw6_battery_wake_probe;
volatile PS_HW6_OwnerStateMachineProbe g_ps_hw6_owner_sm_probe = {
  .magic = PS_HW6_OWNER_SM_PROBE_MAGIC,
  .version = PS_HW6_OWNER_SM_PROBE_VERSION,
  .current_state = {8U, 8U},
  .battery_policy_state = 5U,
  .battery_policy_fuel_ok = 1U,
  .battery_policy_battery_present = 1U,
  .battery_policy_vbat_mv = 3373U,
  .battery_policy_restart_allow_mv = 3600U,
  .battery_policy_critical_mv = 3300U,
  .battery_policy_boot_restart_gate_blocked = 1U,
  .power_quiesce_reason = 3U,
  .power_quiesce_required_mask = 0x7eU,
  .power_quiesce_send_ok_mask = 0x7eU,
  .power_quiesce_ack_ok_mask = 0x7eU,
  .power_quiesce_success_mask = 0x7eU
};
volatile PS_HW6_OwnerProbe g_ps_hw6_owner_probe = {
  .magic = PS_HW6_OWNER_PROBE_MAGIC,
  .version = PS_HW6_OWNER_PROBE_VERSION,
  .power_vbus_agree = 1U
};
volatile PS_HW6_BatteryQuiesceTimingProbe g_ps_hw6_battery_quiesce_timing_probe = {
  .api_version = 3U, .sequence = 1U, .reason = 3U
};
volatile struct {uint32_t runtime_complete, runtime_lifecycle;} g_ps_hw6_rtos_probe = {1U, 0U};

int main(void)
{
  return 0;
}
