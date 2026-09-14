set pagination off
set input-radix 10
printf "--- HW6 prepared-battery one-shot physical shipment test ---\n"
printf "BENCH ONLY: cell and device USB disconnected; isolated PPK2 battery-input supply. Keep voltage unchanged until shutdown.\n"
set $ship_ok = 1
set $sm = &g_ps_hw6_owner_sm_probe
set $owner = &g_ps_hw6_owner_probe
set $prep = &g_ps_hw6_battery_shutdown_probe
set $timing = &g_ps_hw6_battery_quiesce_timing_probe
if $sm->magic != 0x48364653 || $sm->version != 85 || $owner->magic != 0x48364f57 || $owner->version != 46 || $prep->api_version != 1 || $timing->api_version != 3
  printf "NOT armed: mismatched firmware/probes. Use the matching ELF.\n"
  set $ship_ok = 0
end
if g_ps_hw6_rtos_probe.runtime_complete != 1 || $sm->current_state[0] != 8 || $sm->current_state[1] != 8
  printf "NOT armed: boot must be complete and battery policy must own shipment preparation.\n"
  set $ship_ok = 0
end
if $sm->battery_policy_critical_ship_enabled != 0 || $sm->battery_policy_boot_ship_enabled != 0
  printf "NOT armed: this procedure requires both automatic battery shipment gates OFF.\n"
  set $ship_ok = 0
end
if $sm->battery_policy_last_snapshot_status != 0 || $sm->battery_policy_fuel_ok != 1 || $sm->battery_policy_battery_present != 1 || $sm->battery_policy_vbat_mv == 0
  printf "NOT armed: a valid battery reading is required. Resume at the test voltage first.\n"
  set $ship_ok = 0
end
if $sm->battery_policy_vbus_ok != 0 || $owner->power_vbus_ok != 0 || $owner->power_mcu_vbus_present != 0 || $owner->power_vbus_agree != 1
  printf "NOT armed: USB/VBUS must be absent with agreeing PMIC/MCU detection.\n"
  set $ship_ok = 0
end
if $prep->reason == 3
  if $sm->battery_policy_state != 5 || $sm->battery_policy_boot_restart_gate_blocked != 1 || $sm->battery_policy_vbat_mv >= $sm->battery_policy_restart_allow_mv
    printf "NOT armed: no current low-voltage boot block.\n"
    set $ship_ok = 0
  end
else
  if $prep->reason != 2 || $sm->battery_policy_state != 4 || $sm->battery_policy_vbat_mv > $sm->battery_policy_critical_mv
    printf "NOT armed: no current critical-battery preparation.\n"
    set $ship_ok = 0
  end
end
if $prep->attempts == 0 || $prep->prepared != 1 || $prep->exhausted != 0 || $prep->last_status != 0 || $prep->next_tick != 0
  printf "NOT armed: battery preparation must have completed successfully.\n"
  set $ship_ok = 0
end
if $sm->battery_policy_quiesce_last_status != 0 || $sm->power_quiesce_reason != $prep->reason || $sm->power_quiesce_last_status != 0 || $sm->power_quiesce_required_mask != 0x7e || $sm->power_quiesce_send_ok_mask != 0x7e || $sm->power_quiesce_ack_ok_mask != 0x7e || $sm->power_quiesce_success_mask != 0x7e || $sm->power_quiesce_failure_mask != 0
  printf "NOT armed: the matching owner barrier must have all successful ACKs and actions.\n"
  set $ship_ok = 0
end
set $ship_owner = 1
while $ship_owner <= 6
  if $sm->power_quiesce_owner_status[$ship_owner] != 0 || $sm->power_quiesce_send_status[$ship_owner] != 0 || $sm->power_quiesce_ack_status[$ship_owner] != 0
    printf "NOT armed: owner %u has an unsuccessful send, ACK or action.\n", $ship_owner
    set $ship_ok = 0
  end
  set $ship_owner = $ship_owner + 1
end
if $timing->sequence == 0 || $timing->active != 0 || $timing->reason != $prep->reason || $timing->status != 0 || $timing->display_clock_failures != 0 || $timing->display_clock_grant_status != 0 || $timing->display_clock_release_status != 0 || $timing->storage_clock_grant_status != 0 || $timing->storage_clock_release_status != 0
  printf "NOT armed: incomplete preparation timing or a failed clock handoff.\n"
  set $ship_ok = 0
end
if $owner->power_driver_mr_shipping_mode_status != 0 || $owner->power_driver_fuel_gauge_prepare_status != 0
  printf "NOT armed: PMIC boot preparation did not succeed.\n"
  set $ship_ok = 0
end
if g_ps_hw6_pmic_software_ship_request != 0 || $owner->power_software_ship_request_count != 0 || $sm->battery_policy_software_ship_request_count != 0
  printf "NOT armed: shipment is already queued or has been attempted since reset. Do not retry blindly.\n"
  set $ship_ok = 0
end
if $ship_ok != 0
  set var g_ps_hw6_pmic_software_ship_request = 1
  printf "Queued ONE physical shipment request after successful battery preparation at %u mV.\n", $sm->battery_policy_vbat_mv
  printf "Resume immediately without changing supply or connecting USB. thPower performs the existing PMIC write; no peripheral call is made by GDB.\n"
  printf "Observe supply current and accessible system rail. Debugger loss alone is not proof; the display may retain its image.\n"
  printf "Automatic gates remain OFF, including after restart. This is a manual final-step test, not automatic discharge-protection proof.\n"
end
