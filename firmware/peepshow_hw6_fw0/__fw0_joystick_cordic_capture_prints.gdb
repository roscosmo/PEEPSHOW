set pagination off

set $sm = &g_ps_hw6_owner_sm_probe

printf "\n--- HW6 TMAG3001 hardware CORDIC magnitude ---\n"
printf "owner/driver api = %u / %u\n", $sm->version, $sm->joystick_driver_api_version
printf "live request/status/samples/errors = %u / 0x%x / %u / %u\n", $sm->joystick_live_request_count, $sm->joystick_live_status, $sm->joystick_live_sample_count, $sm->joystick_live_error_count
printf "CORDIC samples/current/min/max = %u / %u / %u / %u\n", $sm->joystick_cordic_sample_count, $sm->joystick_cordic_magnitude_result, $sm->joystick_cordic_magnitude_min, $sm->joystick_cordic_magnitude_max
printf "STOP field threshold/hysteresis code and THR samples = %u / %u / %u\n", $sm->joystick_wake_field_threshold_code, $sm->joystick_wake_field_hysteresis_code, $sm->joystick_cordic_threshold_cross_count
printf "device status/THR_Cross/INT_RB/JOY_INT pin = 0x%x / %u / %u / %u\n", $sm->joystick_cordic_device_status, $sm->joystick_cordic_device_status & 1, $sm->joystick_cordic_int_readback, $sm->joystick_cordic_int_pin_level
printf "raw X/Y/Z software magnitude = %d / %d / %d / %u\n", $sm->joystick_input_raw_x, $sm->joystick_input_raw_y, $sm->joystick_input_raw_z, $sm->joystick_input_magnitude
printf "expected: owner/driver api=70/8, live status=0, samples>0; CORDIC telemetry remains diagnostic while STOP2 wake uses the X/Y omnipolar field switch\n"
printf "--- end TMAG3001 hardware CORDIC magnitude ---\n"
