set pagination off

set $sm = &g_ps_hw6_owner_sm_probe

printf "\n--- HW6 joystick normalized input state ---\n"
printf "input api/policy/status   = %u / %u / 0x%x\n", $sm->joystick_input_api_version, $sm->joystick_input_policy, $sm->joystick_input_last_status
printf "cal/active/dir/mag       = %u / %u / 0x%x / %u\n", $sm->joystick_input_calibration_valid, $sm->joystick_input_active, $sm->joystick_input_direction_mask, $sm->joystick_input_magnitude
printf "norm X/Y                 = %d / %d\n", $sm->joystick_input_normalized_x, $sm->joystick_input_normalized_y
printf "delta X/Y                = %d / %d\n", $sm->joystick_input_delta_x, $sm->joystick_input_delta_y
printf "raw X/Y/Z conv           = %d / %d / %d / 0x%02x\n", $sm->joystick_input_raw_x, $sm->joystick_input_raw_y, $sm->joystick_input_raw_z, $sm->joystick_input_conv_status
printf "sample tick/age/updates  = %u / %u / %u\n", $sm->joystick_input_sample_tick, $sm->joystick_input_sample_age_ticks, $sm->joystick_input_update_count
printf "live req/status/count/err = %u / 0x%x / %u / %u\n", $sm->joystick_live_request_count, $sm->joystick_live_status, $sm->joystick_live_sample_count, $sm->joystick_live_error_count
printf "card req/status          = %u / 0x%x\n", $sm->joystick_cardinal_request_count, $sm->joystick_cardinal_status
printf "request flags live/card  = %u / %u\n", g_ps_hw6_joystick_live_request, g_ps_hw6_joystick_cardinal_request
printf "dir bits: LEFT=0x1 RIGHT=0x2 UP=0x4 DOWN=0x8\n"
