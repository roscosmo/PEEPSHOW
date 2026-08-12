set pagination off

set $status = PS_HW6_RTOS_DebugRequestUsbExport()
printf "HW6 USB MSC export command queued for thStorage, status=0x%x. Continue the target.\n", $status
printf "Use after lifecycle completes with the board powered from battery; do not source the board from PPK while USB host VBUS is attached.\n"
printf "PH1/PWR_DBG pulses during export if passively probed; it is not required for this battery-powered USB host test.\n"