printf "--- HW6 TMAG3001 raw XYZ capture scaffold ---\n"
printf "owner magic/version = 0x%lx / %lu\n", g_ps_hw6_owner_sm_probe.magic, g_ps_hw6_owner_sm_probe.version
printf "input stack cfg/size/ptr/high = %lu / %lu / 0x%lx / 0x%lx\n", g_ps_hw6_rtos_probe.thread_stack_config_bytes[2], g_ps_hw6_rtos_probe.thread_stack_size[2], g_ps_hw6_rtos_probe.thread_stack_ptr[2], g_ps_hw6_rtos_probe.thread_stack_highest_ptr[2]
printf "request mode/count = %lu / %lu\n", g_ps_hw6_joystick_xyz_capture_mode, g_ps_hw6_joystick_xyz_capture_request
printf "capture mode/status = %lu / 0x%lx\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_mode, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_status
printf "capture ticks start/end = %lu / %lu\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_start_tick, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_end_tick
printf "period/requested/capacity = %lu / %lu / %lu\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_period_ticks, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_requested_samples, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_capacity
printf "timeout ticks/count = %lu / %lu\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_timeout_ticks, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_timeout_count
printf "records success/error = %lu / %lu / %lu\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_count, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_success_count, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_error_count
printf "stabilize/wake/sleep = 0x%lx / 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_stabilize_status, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_wake_status, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sleep_status
printf "sensor cfg2 stat/restore stat = 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_status, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_restore_status
printf "sensor cfg2 before/active/restore = 0x%lx / 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_before, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_active, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_sensor_config2_restore
printf "range override mask/value/applied = 0x%lx / 0x%lx / %lu\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_mask, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_value, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_range_override_applied
printf "first x/y/z = %ld / %ld / %ld\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_x, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_y, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_first_z
printf "min x/y/z   = %ld / %ld / %ld\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_x, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_y, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_min_z
printf "max x/y/z   = %ld / %ld / %ld\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_x, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_y, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_z
printf "last x/y/z  = %ld / %ld / %ld\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_x, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_y, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_z
printf "max abs delta z / last conv/read = %lu / 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.joystick_xyz_capture_max_abs_delta_z, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_conv_status, g_ps_hw6_owner_sm_probe.joystick_xyz_capture_last_read_status
printf "driver state/status ops = %lu / 0x%lx / %lu\n", g_ps_hw6_owner_sm_probe.joystick_driver_state, g_ps_hw6_owner_sm_probe.joystick_driver_last_status, g_ps_hw6_owner_sm_probe.joystick_driver_operation_count
printf "modes: NONE=0 REST=1 SWEEP=2 SWEEP_Z_HIGH=3; status: HAL_OK=0x0 HAL_ERROR=0x1 NOT_RUN=0xffffffff\n"
printf "Dump CSV with: source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_joystick_xyz_dump_csv.gdb\n"