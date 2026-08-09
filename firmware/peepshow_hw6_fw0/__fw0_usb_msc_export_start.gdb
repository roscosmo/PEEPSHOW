set pagination off

set var g_ps_hw6_storage_usb_export_request = 1

printf "HW6 USB MSC export request queued for thStorage. Continue the target.\n"
printf "Use after lifecycle completes with the board powered from battery; do not source the board from PPK while USB host VBUS is attached.\n"
printf "PH1/PWR_DBG pulses during export if passively probed; it is not required for this battery-powered USB host test.\n"
