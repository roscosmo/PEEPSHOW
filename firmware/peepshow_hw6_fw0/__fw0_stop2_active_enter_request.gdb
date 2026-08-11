set pagination off
set var g_ps_hw6_power_stop2_active_enter_request = 1
printf "HW6 STOP2 active-owner ENTER request queued for thPower. This WILL enter STOP2 only if prep-ready is 1. Continue target, press START briefly to wake when the debugger disconnects or the display goes quiet, reconnect if needed, interrupt, then source __fw0_stop2_power_prints.gdb.\n"