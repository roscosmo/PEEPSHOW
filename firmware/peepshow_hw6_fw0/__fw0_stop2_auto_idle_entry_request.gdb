set pagination off
set var g_ps_hw6_power_stop2_auto_idle_entry_request = 1
printf "HW6 automatic STOP2 held-frame entry request queued for thPower. This WILL enter STOP2 if eligibility is clear; it bypasses the compile-time auto-enable knob for this explicit test and treats the idle window as already satisfied. Continue target, wait for STOP2, press START briefly to wake, reconnect if needed, then source __fw0_stop2_auto_idle_prints.gdb and __fw0_stop2_controlled_entry_prints.gdb.\n"
