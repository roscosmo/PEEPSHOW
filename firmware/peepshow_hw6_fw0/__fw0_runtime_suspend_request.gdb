set pagination off
set var g_ps_hw6_runtime_suspend_request = 1
printf "HW6 runtime suspend request queued for thRuntime. Continue target, then interrupt and source __fw0_runtime_state_prints.gdb and __fw0_clock_policy_prints.gdb. Realtime suspend should clear REALTIME_DEADLINE and make STOP2 eligible when no other blockers are active.\n"