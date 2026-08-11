set pagination off
set var g_ps_hw6_power_stop2_active_resume_request = 1
printf "HW6 STOP2 active-owner LEGACY one-shot request queued for thPower. Prefer the staged helpers: __fw0_stop2_active_prep_request.gdb first, then __fw0_stop2_active_enter_request.gdb. This one-shot helper WILL enter STOP2 without a debug-visible midpoint.\n"