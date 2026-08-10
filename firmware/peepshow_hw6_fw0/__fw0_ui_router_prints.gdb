set pagination off

set $ui = &g_ps_ui_router_probe
set $ow = &g_ps_hw6_owner_probe
set $btn = &g_ps_input_buttons_probe
set $sm = &g_ps_hw6_owner_sm_probe

printf "\n--- HW6 UI router state ---\n"
printf "api/status/event       = %u / 0x%x / %u\n", $ui->api_version, $ui->last_status, $ui->last_event
printf "page prev/current/req  = %u / %u / %u\n", $ui->previous_page, $ui->current_page, $ui->requested_page
printf "nav/modal/cal/focus   = %u / %u / %u / %u\n", $ui->nav_state, $ui->modal_state, $ui->calibration_page, $ui->focus_index
printf "shutdown state/cd/ret/count = %u / %u / %u / %u\n", $ui->shutdown_state, $ui->shutdown_countdown_seconds, $ui->shutdown_return_page, $ui->shutdown_event_count
printf "transitions/rejected  = %u / %u\n", $ui->transition_count, $ui->rejected_event_count
printf "button event/count    = %u / %u\n", $ui->last_button_event, $ui->button_event_count
printf "request/event flags   = %u / %u\n", g_ps_ui_router_request, g_ps_ui_router_request_event
printf "display ui req/render = %u / %u\n", $ow->display_ui_request_count, $ow->display_ui_render_count
printf "display ui page/cal/focus = %u / %u / %u\n", $ow->display_ui_page, $ow->display_ui_calibration_page, $ow->display_ui_focus_index
printf "display shutdown/cd   = %u / %u\n", $ow->display_ui_shutdown_state, $ow->display_ui_shutdown_countdown_seconds
printf "display complete/success = %u / %u\n", $ow->display_complete, $ow->display_success
printf "display status/hash   = 0x%x / 0x%x\n", $ow->display_ui_status, $ow->display_framebuffer_hash
printf "input edge/press      = %u / %u\n", $btn->isr_edge_count, $btn->press_count
printf "input pending/button  = 0x%x / %u\n", $btn->pending_mask, $btn->last_button_id
printf "joystick cal req/page/status = %u / %u / 0x%x\n", $sm->joystick_calibration_capture_request_count, $sm->joystick_calibration_capture_page, $sm->joystick_calibration_capture_status
printf "joystick cal valid/dz/thr = %u / %d / %d\n", $sm->joystick_calibration_active_valid, $sm->joystick_calibration_deadzone_counts, $sm->joystick_calibration_direction_threshold
printf "joystick cal center X/Y = %d / %d\n", $sm->joystick_calibration_center_x, $sm->joystick_calibration_center_y
printf "joystick cal min X/Y   = %d / %d\n", $sm->joystick_calibration_min_x, $sm->joystick_calibration_min_y
printf "joystick cal max X/Y   = %d / %d\n", $sm->joystick_calibration_max_x, $sm->joystick_calibration_max_y
printf "joystick sample count/errors = %u / %u\n", $sm->joystick_sample_count, $sm->joystick_sample_error_count
printf "joystick sample min X/Y = %d / %d\n", $sm->joystick_sample_min_x, $sm->joystick_sample_min_y
printf "joystick sample max X/Y = %d / %d\n", $sm->joystick_sample_max_x, $sm->joystick_sample_max_y
printf "pages: BOOT=0 HOME=1 MENU=2 SETTINGS=3 CAL=4 PACKAGES=5 RUNTIME=6 ERROR=7 SHUTDOWN=8\n"
printf "cal: NONE=0 INPUT_ROOT=1 JOY_NEUTRAL=2 JOY_RIGHT=3 JOY_CIRCLE=4 JOY_REVIEW=5\n"
printf "events: BOOT_HOME=1 NAV_MENU=3 NAV_CAL=5 JOY_START=11 NEUTRAL=12 RIGHT=13 CIRCLE=14 REVIEW=15 BTN_A=17 BTN_B=18 BTN_L=19 BTN_R=20 SHUT_PREP=21 WARN=22 IMM=23 CANCEL=24\n"
