set pagination off

set $ui = &g_ps_ui_router_probe
set $ow = &g_ps_hw6_owner_probe
set $btn = &g_ps_input_buttons_probe
set $sm = &g_ps_hw6_owner_sm_probe
set $rt = &g_ps_hw6_rtos_probe

printf "\n--- HW6 UI router state ---\n"
printf "api/status/event       = %u / 0x%x / %u\n", $ui->api_version, $ui->last_status, $ui->last_event
printf "page prev/current/req  = %u / %u / %u\n", $ui->previous_page, $ui->current_page, $ui->requested_page
printf "nav/modal/cal/focus   = %u / %u / %u / %u\n", $ui->nav_state, $ui->modal_state, $ui->calibration_page, $ui->focus_index
printf "shutdown state/cd/ret/count = %u / %u / %u / %u\n", $ui->shutdown_state, $ui->shutdown_countdown_seconds, $ui->shutdown_return_page, $ui->shutdown_event_count
printf "package state/count   = %u / %u\n", $ui->package_state, $ui->package_event_count
printf "transitions/rejected  = %u / %u\n", $ui->transition_count, $ui->rejected_event_count
printf "button event/count    = %u / %u\n", $ui->last_button_event, $ui->button_event_count
printf "router action pend/last/req/take = %u / %u / %u / %u\n", $ui->pending_action, $ui->last_action, $ui->action_request_count, $ui->action_take_count
printf "rtos ui action last/count/status = %u / %u / 0x%x\n", $rt->ui_action_last, $rt->ui_action_count, $rt->ui_action_send_status
printf "rtos msc enter/exit/intercept/unsup = %u / %u / %u / %u\n", $rt->ui_action_msc_enter_count, $rt->ui_action_msc_exit_count, $rt->ui_action_msc_exit_intercept_count, $rt->ui_action_unsupported_count
printf "rtos package install action = %u\n", $rt->ui_action_package_install_stub_count
printf "runtime class/exec/life = %u / %u / %u\n", $rt->runtime_current_class, $rt->runtime_execution, $rt->runtime_lifecycle
printf "runtime prev/return/page = %u / %u / %u\n", $rt->runtime_previous_class, $rt->runtime_return_class, $rt->runtime_return_page
printf "runtime event/count/status = %u / %u / 0x%x\n", $rt->runtime_last_event, $rt->runtime_event_count, $rt->runtime_last_status
printf "runtime install enter/done/err = %u / %u / %u\n", $rt->runtime_installer_enter_count, $rt->runtime_installer_complete_count, $rt->runtime_installer_error_count
printf "usb availability ext/data/avail/active = %u / %u / %u / %u\n", $sm->usb_host_external_power_present, $sm->usb_host_data_seen, $sm->usb_host_msc_available, $sm->usb_host_msc_active
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
printf "ui shutdown: NONE=0 PREP=1 WARNING=2 IMMINENT=3 CANCELLED=4 LOW_BOOT=5 LOW_CHARGE=6 FLASH_INIT=7 FLASH_DONE=8 FLASH_ERROR=9 MSC_EXPORT=10 MSC_ACTIVE=11 MSC_RECLAIM=12 MSC_DONE=13 MSC_ERROR=14 MSC_RECOVERY=15\n"
printf "package: NONE=0 CANDIDATE=1 VALID=2 INSTALLING=3 INSTALLED=4 CANCELLED=5 ERROR=6\n"
printf "cal: NONE=0 INPUT_ROOT=1 JOY_NEUTRAL=2 JOY_RIGHT=3 JOY_CIRCLE=4 JOY_REVIEW=5\n"
printf "events: BOOT_HOME=1 NAV_MENU=3 NAV_CAL=5 JOY_START=11 NEUTRAL=12 RIGHT=13 CIRCLE=14 REVIEW=15 BTN_A=17 BTN_B=18 BTN_L=19 BTN_R=20 SHUT_PREP=21 WARN=22 IMM=23 CANCEL=24 PKG_FOUND=27 PKG_VALID=28 PKG_VALIDATE_ERR=29 PKG_DONE=30 PKG_ERR=31 PKG_CLEAR=32\n"
printf "runtime classes: NONE=0 SHELL=1 LP_GRAPH=2 LP_MODULE=3 RT_SCENE=4 INSTALLER=5\n"
printf "runtime exec: NONE=0 REACTIVE=1 REALTIME=2 lifecycle: NONE=0 MOUNTED=1 RUNNING=2 SUSPENDED=3 STOPPING=4 ERROR=5\n"
printf "runtime events: BOOT_SHELL=1 INSTALLER_ENTER=2 INSTALLER_COMPLETE=3 INSTALLER_ERROR=4 PKG_ACTIVATE_STUB=5 PKG_RETURN=6 SUSPEND=7 RESUME=8\n"
printf "actions: NONE=0 MSC_ENTER=1 MSC_EXIT=2 PACKAGE_INSTALL_STUB=3\n"
