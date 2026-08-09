set pagination off

set var g_ps_hw6_storage_usb_reclaim_request = 1

printf "HW6 USB MSC reclaim request queued for thStorage. Continue the target.\n"
printf "Use only after the host has ejected/unmounted the MSC volume and the cable is detached.\n"
printf "PWR_DBG pulses while thStorage stops USB, closes export, and parks the PCD.\n"
