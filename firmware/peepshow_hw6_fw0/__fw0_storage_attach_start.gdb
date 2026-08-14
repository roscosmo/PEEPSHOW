set pagination off
set $status = PS_HW6_RTOS_DebugRequestStorageAttach()
printf "HW6 non-destructive storage attach/check queued for thStorage, status=0x%lx. Continue the target.\n", $status
printf "This does not erase or format flash. It wakes/probes flash, validates the layout, then returns flash to deep power-down.\n"
printf "After it runs, interrupt and source __fw0_storage_attach_prints.gdb.\n"