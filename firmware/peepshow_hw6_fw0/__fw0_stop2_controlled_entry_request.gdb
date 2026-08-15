set pagination off
set var g_ps_hw6_ble_sleep_dsr_deasserted = 1
set var g_ps_hw6_power_stop2_controlled_entry_request = 1
printf "HW6 controlled STOP2 entry request queued for thPower. This WILL enter STOP2 if eligibility is clear. Continue target, wait for STOP2, press START briefly to wake, reconnect if needed, then source __fw0_stop2_controlled_entry_prints.gdb.\n"