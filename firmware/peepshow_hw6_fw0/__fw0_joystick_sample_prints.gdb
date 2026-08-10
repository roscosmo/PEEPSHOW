set pagination off

set $sm = &g_ps_hw6_owner_sm_probe

printf "\n--- HW6 joystick raw sweep probe ---\n"
printf "request/start/end/status = %u / %u / %u / 0x%x\n", $sm->joystick_sample_request_count, $sm->joystick_sample_start_tick, $sm->joystick_sample_end_tick, $sm->joystick_sample_status
printf "stabilize/wake/read/sleep = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->joystick_sample_stabilize_status, $sm->joystick_sample_wake_status, $sm->joystick_sample_read_status, $sm->joystick_sample_sleep_status
printf "samples/errors/conv       = %u / %u / 0x%02x\n", $sm->joystick_sample_count, $sm->joystick_sample_error_count, $sm->joystick_sample_conv_status
printf "center status/conv       = 0x%x / 0x%02x\n", $sm->joystick_sample_center_status, $sm->joystick_sample_center_conv_status
printf "center X/Y/Z             = %d / %d / %d\n", $sm->joystick_sample_center_x, $sm->joystick_sample_center_y, $sm->joystick_sample_center_z
printf "first sweep X/Y/Z        = %d / %d / %d\n", $sm->joystick_sample_first_x, $sm->joystick_sample_first_y, $sm->joystick_sample_first_z
printf "min X/Y/Z                = %d / %d / %d\n", $sm->joystick_sample_min_x, $sm->joystick_sample_min_y, $sm->joystick_sample_min_z
printf "max X/Y/Z                = %d / %d / %d\n", $sm->joystick_sample_max_x, $sm->joystick_sample_max_y, $sm->joystick_sample_max_z
printf "last X/Y/Z               = %d / %d / %d\n", $sm->joystick_sample_x, $sm->joystick_sample_y, $sm->joystick_sample_z
printf "driver state/ops/last    = %u / %u / 0x%x\n", $sm->joystick_driver_state, $sm->joystick_driver_operation_count, $sm->joystick_driver_last_status
printf "request flag             = %u (expected 0 after thInput handles it)\n", g_ps_hw6_joystick_sample_request
