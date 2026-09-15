set pagination off
set input-radix 10
printf "--- HW6 battery fault-wait bench test ---\n"
printf "BENCH ONLY: cell and device USB disconnected; isolated PPK2 battery-input supply.\n"
set $fault_test_ok = 1
if g_ps_hw6_owner_sm_probe.magic != 0x48364653 || g_ps_hw6_owner_sm_probe.version != 85 || g_ps_hw6_battery_shutdown_probe.api_version != 2 || g_ps_hw6_battery_fault_wait_probe.api_version != 1 || g_ps_hw6_battery_fault_test_probe.api_version != 1
  printf "NOT armed: firmware/probe mismatch. Flash the matching normal Debug build.\n"
  set $fault_test_ok = 0
end
if g_ps_hw6_owner_sm_probe.battery_policy_critical_ship_enabled != 0 || g_ps_hw6_owner_sm_probe.battery_policy_boot_ship_enabled != 0 || g_ps_hw6_owner_sm_probe.start_power_software_ship_enabled != 0
  printf "NOT armed: ALL shipment gates must be OFF. Do NOT use BATTERY SHUTDOWN TEST.\n"
  set $fault_test_ok = 0
end
if g_ps_hw6_rtos_probe.runtime_complete != 1 || g_ps_hw6_battery_shutdown_probe.prepared != 1 || g_ps_hw6_battery_shutdown_probe.exhausted != 0 || g_ps_hw6_owner_sm_probe.current_state[0] != 8
  printf "NOT armed: let low-battery preparation finish first, then halt. For the boot case, boot at 3.4 V and wait for LOW BATTERY.\n"
  set $fault_test_ok = 0
end
if g_ps_hw6_battery_fault_test_probe.accepted != 0 || g_ps_hw6_battery_fault_test_probe.request != 0 || g_ps_hw6_battery_fault_wait_probe.active != 0
  printf "NOT armed: a fault/test is already active or recorded. Reset for a new run; do not reset to retrieve an existing run.\n"
  set $fault_test_ok = 0
end
if g_ps_hw6_owner_probe.power_vbus_ok != 0 || g_ps_hw6_owner_probe.power_mcu_vbus_present != 0 || g_ps_hw6_owner_probe.power_vbus_agree != 1 || g_ps_hw6_pmic_software_ship_request != 0 || g_ps_hw6_owner_probe.power_software_ship_request_count != 0
  printf "NOT armed: VBUS must be absent and no shipment request may be pending or previously attempted.\n"
  set $fault_test_ok = 0
end
if $fault_test_ok != 0
  set var g_ps_hw6_battery_fault_test_probe.request = 1
  printf "Queued. thPower rechecks a fresh sample, then injects failed preparation returns AFTER real successful owner quiesce.\n"
  printf "No PMIC shipment write or bus fault is injected. The normal bounded retries must exhaust and enter checked fault STOP2.\n"
  printf "Resume at 3.4 V, leave buttons released and observe current for about 25 seconds. Battery RTC checks use a 15-second maximum interval in this test.\n"
  printf "Then tap A once. The FIRST button wake opens a 60-second RUNNING-TIME awake inspection window; RTC wakes do not.\n"
  printf "Reattach with HW6 FW0: Attach with ST-LINK if needed, WITHOUT reset/reflash. Halt and source __fw0_battery_fault_test_prints.gdb.\n"
  printf "Halting freezes the inspection countdown. High current in that window is intentional; measure fault-sleep current BEFORE the button wake.\n"
end
