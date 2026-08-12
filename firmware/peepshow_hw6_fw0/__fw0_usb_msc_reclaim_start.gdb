set pagination off

set $status = PS_HW6_RTOS_RequestUsbMscExit()
printf "HW6 USB MSC exit service queued for thStorage, status=0x%x. Continue the target.\n", $status
printf "Use only after the host has ejected/unmounted the MSC volume and the cable is detached.\n"
printf "PWR_DBG pulses while thStorage stops USB, closes export, and parks the PCD.\n"