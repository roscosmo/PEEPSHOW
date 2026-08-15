set pagination off
set var g_ps_hw6_ble_sleep_dsr_deasserted = 1
set $status = PS_HW6_RTOS_DebugRequestCommBleStop()
printf "HW6 NINA/BLE DSR Sleep/System-OFF mode queued for thComm with DSR DEASSERTED sleep target, status=0x%lx. Continue target briefly, then interrupt and source __fw0_comm_ble_prints.gdb.\n", $status