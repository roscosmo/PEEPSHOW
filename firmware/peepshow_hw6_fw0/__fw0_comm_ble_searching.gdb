set pagination off
set $status = PS_HW6_RTOS_DebugRequestCommBleSearching()
printf "HW6 NINA/BLE SEARCHING placeholder mode queued for thComm, status=0x%lx. Continue target briefly, then interrupt and source __fw0_comm_ble_prints.gdb.\n", $status