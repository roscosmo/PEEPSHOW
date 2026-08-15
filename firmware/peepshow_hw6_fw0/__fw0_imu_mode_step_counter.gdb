set pagination off
set $status = PS_HW6_RTOS_DebugRequestImuStepCounter()
printf "HW6 LIS2DUX12 STEP-COUNTER placeholder mode queued for thSensor, status=0x%lx. Continue target briefly, then interrupt and source __fw0_imu_mode_prints.gdb.\n", $status