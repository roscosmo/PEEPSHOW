set pagination off
set var g_ps_hw6_ble_sleep_dsr_deasserted = 0
set $status = PS_HW6_RTOS_DebugRequestCommBleStop()
printf "HW6 NINA/BLE DSR ASSERTED polarity test queued for thComm, status=0x%lx. Continue target briefly, then interrupt and source __fw0_comm_ble_prints.gdb. This is diagnostic only; normal sleep uses DEASSERTED.\n", $status