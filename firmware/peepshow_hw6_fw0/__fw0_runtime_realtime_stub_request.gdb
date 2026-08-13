set pagination off
set var g_ps_hw6_runtime_realtime_stub_request = 1
printf "HW6 runtime realtime package stub request queued for thRuntime. Continue target, then interrupt and source __fw0_runtime_state_prints.gdb and __fw0_clock_policy_prints.gdb. This should keep REALTIME_DEADLINE active until runtime return.\n"