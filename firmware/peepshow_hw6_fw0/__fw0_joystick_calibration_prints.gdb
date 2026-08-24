set pagination off

set $sm = &g_ps_hw6_owner_sm_probe
set $rt = &g_ps_hw6_rtos_probe

printf "\n--- HW6 staged joystick calibration ---\n"
printf "owner api / UI page/calibration = %u / %u / %u\n", $sm->version, g_ps_ui_router_probe.current_page, g_ps_ui_router_probe.calibration_page
printf "capture count/page/status ticks = %u / %u / 0x%x / %u..%u\n", $sm->joystick_calibration_capture_request_count, $sm->joystick_calibration_capture_page, $sm->joystick_calibration_capture_status, $sm->joystick_calibration_capture_start_tick, $sm->joystick_calibration_capture_end_tick
printf "capture active/progress/samples/errors/next = %u / %u / %u / %u / %u\n", $sm->joystick_calibration_capture_active, $sm->joystick_calibration_capture_progress_per_mille, $sm->joystick_calibration_capture_sample_count, $sm->joystick_calibration_capture_error_count, $sm->joystick_calibration_capture_next_tick
printf "session/valid/transform/commit/cancel = %u / %u / %u / %u / %u\n", $sm->joystick_calibration_session_active, $sm->joystick_calibration_active_valid, $sm->joystick_calibration_transform_valid, $sm->joystick_calibration_commit_count, $sm->joystick_calibration_cancel_count
printf "center raw X/Y = %d / %d\n", $sm->joystick_calibration_center_x, $sm->joystick_calibration_center_y
printf "cardinal UP X/Y = %d / %d\n", $sm->joystick_calibration_cardinal_x[0], $sm->joystick_calibration_cardinal_y[0]
printf "cardinal RIGHT X/Y = %d / %d\n", $sm->joystick_calibration_cardinal_x[1], $sm->joystick_calibration_cardinal_y[1]
printf "cardinal DOWN X/Y = %d / %d\n", $sm->joystick_calibration_cardinal_x[2], $sm->joystick_calibration_cardinal_y[2]
printf "cardinal LEFT X/Y = %d / %d\n", $sm->joystick_calibration_cardinal_x[3], $sm->joystick_calibration_cardinal_y[3]
printf "transform Q20 xx/xy/yx/yy = %d / %d / %d / %d\n", $sm->joystick_calibration_transform_xx_q20, $sm->joystick_calibration_transform_xy_q20, $sm->joystick_calibration_transform_yx_q20, $sm->joystick_calibration_transform_yy_q20
printf "sweep aligned X min/max Y min/max = %d / %d / %d / %d\n", $sm->joystick_calibration_sweep_min_x, $sm->joystick_calibration_sweep_max_x, $sm->joystick_calibration_sweep_min_y, $sm->joystick_calibration_sweep_max_y
printf "sweep coverage mask = 0x%x (expected 0xf)\n", $sm->joystick_calibration_sweep_coverage_mask
printf "deadzone/enter/release/dominance = %d / %d / %d / %d\n", $sm->joystick_calibration_deadzone_counts, $sm->joystick_calibration_direction_threshold, $sm->joystick_calibration_direction_release_threshold, $sm->joystick_calibration_dominance_hysteresis
printf "review count/status/tick = %u / 0x%x / %u\n", $sm->joystick_calibration_review_count, $sm->joystick_calibration_review_status, $sm->joystick_calibration_review_tick
printf "live raw X/Y normalized X/Y = %d / %d / %d / %d\n", $sm->joystick_input_raw_x, $sm->joystick_input_raw_y, $sm->joystick_input_normalized_x, $sm->joystick_input_normalized_y
printf "live candidate/resolved/active = 0x%x / 0x%x / %u\n", $sm->joystick_input_candidate_direction_mask, $sm->joystick_input_direction_mask, $sm->joystick_input_active
printf "direction changes/press/release/switch = %u / %u / %u / %u\n", $sm->joystick_input_direction_change_count, $sm->joystick_input_direction_press_count, $sm->joystick_input_direction_release_count, $sm->joystick_input_direction_switch_count
printf "STOP2 auto entries/block/pending = %u / 0x%x / 0x%x\n", $rt->stop2_auto_entry_count, $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask
printf "stages: ROOT=1 NEUTRAL=2 RIGHT=3 SWEEP=4 REVIEW=5 UP=6 DOWN=7 LEFT=8\n"
printf "expected review: owner api=64 session/valid/transform=1/1/1 coverage=0xf, resolved direction is zero or exactly one bit, review count advances, STOP2 entry count remains unchanged while calibration is open\n"
printf "--- end staged joystick calibration ---\n"
