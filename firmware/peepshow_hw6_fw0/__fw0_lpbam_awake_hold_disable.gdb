set pagination off
set variable g_ps_hw6_power_stop2_lpbam_awake_hold_enable = 0
printf "--- HW6 awake LPBAM-only hold disabled ---\n"
printf "hold = %u\n", g_ps_hw6_power_stop2_lpbam_awake_hold_enable
printf "Resume execution. The next eligible automatic check may enter real STOP2 with the active LPBAM queue.\n"
