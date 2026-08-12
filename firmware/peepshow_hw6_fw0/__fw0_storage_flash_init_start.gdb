set pagination off
set $status = PS_HW6_RTOS_DebugRequestStorageFlashInit()
printf "HW6 storage flash init command queued for thStorage, status=0x%lx. Continue the target.\n", $status
printf "This is destructive for the USB staging region; use only when intentionally provisioning flash.\n"
printf "With SWO enabled, wait for DON or ERR before interrupting and sourcing __fw0_storage_flash_init_prints.gdb.\n"