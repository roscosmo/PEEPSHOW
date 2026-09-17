set pagination off
set input-radix 10
printf "--- HW6 battery fault-wait one-shot owner refusal ---\n"
printf "BENCH ONLY: cell and device USB disconnected; isolated PPK2 battery-input supply.\n"
set $fault_test_ok = 1
if g_ps_hw6_owner_sm_probe.magic != 0x48364653 || g_ps_hw6_owner_sm_probe.version != 85 || g_ps_hw6_battery_shutdown_probe.api_version != 2 || g_ps_hw6_battery_fault_wait_probe.api_version != 1 || g_ps_hw6_battery_fault_test_probe.api_version != 2
  printf "NOT armed: firmware/probe mismatch. Flash the matching normal Debug build.\n"
  set $fault_test_ok = 0
end
if g_ps_hw6_owner_sm_probe.battery_policy_critical_ship_enabled != 0 || g_ps_hw6_owner_sm_probe.battery_policy_boot_ship_enabled != 0 || g_ps_hw6_owner_sm_probe.start_power_software_ship_enabled != 0
  printf "NOT armed: ALL shipment gates must be OFF. Do NOT use BATTERY SHUTDOWN TEST.\n"
  set $fault_test_ok = 0
end
if g_ps_hw6_rtos_probe.runtime_complete != 1 || g_ps_hw6_battery_shutdown_probe.prepared != 1 || g_ps_hw6_battery_shutdown_probe.exhausted != 0 || g_ps_hw6_owner_sm_probe.current_state[0] != 8
  printf "NOT armed: boot at 3.4 V and let LOW BATTERY preparation finish, then halt.\n"
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
  set var g_ps_hw6_battery_fault_test_probe.request = 2
  printf "Queued mode 2. Three preparation failures lead to fault wait, then SENSOR reports ONE synthetic refusal after real successful quiesce.\n"
  printf "The first sleep MUST be refused. Awake current for about 60 seconds is intentional; the existing retry backoff is unchanged. Battery reads remain scheduled.\n"
  printf "Resume at 3.4 V, leave buttons released and observe for about 90 seconds. The next full barrier must succeed before STOP2 resumes; subsequent battery wakes use 15 seconds.\n"
  printf "Then tap A once for the 60-second inspection window, attach WITHOUT reset/reflash if needed, halt and source __fw0_battery_fault_test_prints.gdb and __fw0_battery_quiesce_timing_prints.gdb.\n"
  printf "The FIRST FAILED barrier must retain SENSOR action=1 with a real ACK, not an ACK timeout. No bus is disconnected and no PMIC shipment call is made.\n"
end
