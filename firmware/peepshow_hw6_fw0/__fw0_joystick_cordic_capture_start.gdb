set pagination off

set var g_ps_hw6_owner_sm_probe.joystick_cordic_sample_count = 0
set var g_ps_hw6_owner_sm_probe.joystick_cordic_magnitude_result = 0
set var g_ps_hw6_owner_sm_probe.joystick_cordic_magnitude_min = 0
set var g_ps_hw6_owner_sm_probe.joystick_cordic_magnitude_max = 0
set var g_ps_hw6_owner_sm_probe.joystick_cordic_device_status = 0
set var g_ps_hw6_owner_sm_probe.joystick_cordic_threshold_cross_count = 0
set var g_ps_hw6_owner_sm_probe.joystick_cordic_int_readback = 0
set var g_ps_hw6_owner_sm_probe.joystick_cordic_int_pin_level = 0
set var g_ps_hw6_joystick_live_request = 1

printf "HW6 TMAG3001 CORDIC capture queued for thSensor. Continue and sweep the joystick fully during the live window.\n"
printf "Then halt and source __fw0_joystick_cordic_capture_prints.gdb.\n"
