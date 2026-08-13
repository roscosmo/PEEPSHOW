set pagination off
set var g_ps_hw6_runtime_resume_request = 1
printf "HW6 runtime resume request queued for thRuntime. Continue target, then interrupt and source __fw0_runtime_state_prints.gdb and __fw0_clock_policy_prints.gdb. Realtime resume should restore REALTIME_DEADLINE and block STOP2 again.\n"