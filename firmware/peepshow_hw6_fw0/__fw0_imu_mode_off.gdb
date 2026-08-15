set pagination off
set $status = PS_HW6_RTOS_DebugRequestImuOff()
printf "HW6 LIS2DUX12 OFF/deep-power-down mode queued for thSensor, status=0x%lx. Continue target briefly, then interrupt and source __fw0_imu_mode_prints.gdb.\n", $status