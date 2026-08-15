set pagination off
set $status = PS_HW6_RTOS_DebugRequestImuLowRate()
printf "HW6 LIS2DUX12 LOW-RATE sample mode queued for thSensor, status=0x%lx. Continue target briefly, then interrupt and source __fw0_imu_mode_prints.gdb.\n", $status