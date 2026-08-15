set pagination off
set var g_ps_hw6_ble_sleep_dsr_deasserted = 0
set var g_ps_hw6_power_stop2_controlled_entry_request = 1
printf "HW6 controlled STOP2 entry DSR ASSERTED polarity test queued. This WILL enter STOP2 if eligibility is clear. Continue target, wake with START, reconnect if needed, then source __fw0_stop2_controlled_entry_prints.gdb. This is diagnostic only; normal STOP2 uses DSR DEASSERTED.\n"