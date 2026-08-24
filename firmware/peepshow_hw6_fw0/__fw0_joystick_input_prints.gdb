set pagination off

set $sm = &g_ps_hw6_owner_sm_probe
set $rt = &g_ps_hw6_rtos_probe

printf "\n--- HW6 joystick normalized input state ---\n"
printf "input api/policy/status   = %u / %u / 0x%x\n", $sm->joystick_input_api_version, $sm->joystick_input_policy, $sm->joystick_input_last_status
printf "cal/active/candidate/resolved/mag = %u / %u / 0x%x / 0x%x / %u\n", $sm->joystick_input_calibration_valid, $sm->joystick_input_active, $sm->joystick_input_candidate_direction_mask, $sm->joystick_input_direction_mask, $sm->joystick_input_magnitude
printf "direction changes/press/release/switch = %u / %u / %u / %u\n", $sm->joystick_input_direction_change_count, $sm->joystick_input_direction_press_count, $sm->joystick_input_direction_release_count, $sm->joystick_input_direction_switch_count
printf "enter/release/dominance  = %d / %d / %d\n", $sm->joystick_calibration_direction_threshold, $sm->joystick_calibration_direction_release_threshold, $sm->joystick_calibration_dominance_hysteresis
printf "norm X/Y                 = %d / %d\n", $sm->joystick_input_normalized_x, $sm->joystick_input_normalized_y
printf "delta X/Y                = %d / %d\n", $sm->joystick_input_delta_x, $sm->joystick_input_delta_y
printf "raw X/Y/Z conv           = %d / %d / %d / 0x%02x\n", $sm->joystick_input_raw_x, $sm->joystick_input_raw_y, $sm->joystick_input_raw_z, $sm->joystick_input_conv_status
printf "sample tick/age/updates  = %u / %u / %u\n", $sm->joystick_input_sample_tick, $sm->joystick_input_sample_age_ticks, $sm->joystick_input_update_count
printf "live req/status/count/err = %u / 0x%x / %u / %u\n", $sm->joystick_live_request_count, $sm->joystick_live_status, $sm->joystick_live_sample_count, $sm->joystick_live_error_count
printf "card req/status          = %u / 0x%x\n", $sm->joystick_cardinal_request_count, $sm->joystick_cardinal_status
printf "card failures stage/driver/HAL/error = %u / %u / 0x%x / 0x%x / 0x%x\n", $sm->joystick_cardinal_failure_count, $sm->joystick_cardinal_last_failure_stage, $sm->joystick_cardinal_last_driver_status, $sm->joystick_cardinal_last_hal_status, $sm->joystick_cardinal_last_hal_error
printf "recovery attempt/success/fail/status = %u / %u / %u / 0x%x\n", $sm->joystick_recovery_attempt_count, $sm->joystick_recovery_success_count, $sm->joystick_recovery_failure_count, $sm->joystick_recovery_last_status
printf "owner/driver state       = %u / %u\n", $sm->current_state[5], $sm->joystick_driver_state
printf "awake poll count/error/next/status = %u / %u / %u / 0x%x\n", $rt->joystick_awake_poll_count, $rt->joystick_awake_poll_error_count, $rt->joystick_awake_poll_next_tick, $rt->joystick_awake_poll_last_status
printf "logical change/activate/release/switch/drop = %u / %u / %u / %u / %u\n", $rt->joystick_logical_change_count, $rt->joystick_logical_activation_count, $rt->joystick_logical_release_count, $rt->joystick_logical_switch_count, $rt->joystick_logical_drop_count
printf "logical last direction/source/target/status/tick = 0x%x / %u / %u / 0x%x / %u\n", $rt->joystick_logical_last_direction_mask, $rt->joystick_logical_last_source, $rt->joystick_logical_last_target, $rt->joystick_logical_last_status, $rt->joystick_logical_last_tick
printf "UI joystick event/count = %u / %u\n", g_ps_ui_router_probe.last_joystick_event, g_ps_ui_router_probe.joystick_event_count
printf "request flags live/card  = %u / %u\n", g_ps_hw6_joystick_live_request, g_ps_hw6_joystick_cardinal_request
printf "failure stages: PREPARE=1 ARM=2 INTERRUPT=3 WAKE=4 READ=5 SAMPLE_FSM=6 NORMALIZE=7 SUSPEND=8 FINISH_FSM=9\n"
printf "dir bits: LEFT=0x1 RIGHT=0x2 UP=0x4 DOWN=0x8\n"
printf "logical IDs: JOY_LEFT=6 JOY_RIGHT=7 JOY_UP=8 JOY_DOWN=9; targets NONE=0 UI=1 RUNTIME=2\n"
printf "expected steady awake: owner/driver=5/3, poll status=0, resolved direction zero or one bit; a recovered transient increments failure and recovery success without leaving owner=14 or driver=4\n"
