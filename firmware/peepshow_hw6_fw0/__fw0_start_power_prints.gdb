set pagination off

printf "--- HW6 START shipping-prep scaffold ---\n"
set $btn = &g_ps_input_buttons_probe
set $sm = &g_ps_hw6_owner_sm_probe
set $owner = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
set $rtos = &g_ps_hw6_rtos_probe
set $input_now = g_ps_hw6_rtos_probe.owner_last_tick[2]
set $start_live_hold = 0
if ($btn->start_active != 0) && ($btn->start_press_tick != 0)
  set $start_live_hold = $input_now - $btn->start_press_tick
end
printf "button api/edges/presses  = %u / %u / %u\n", $btn->api_version, $btn->isr_edge_count, $btn->press_count
printf "last pin/button/event/lev = %u / %u / %u / %u\n", $btn->last_pin, $btn->last_button_id, $btn->last_event, $btn->last_level
printf "START state/active       = %u / %u\n", $btn->start_state, $btn->start_active
printf "START hold checkpoint/live ticks = %u / %u\n", $btn->start_hold_ticks, $start_live_hold
printf "START pend press/release = %u / %u\n", $btn->start_press_pending, $btn->start_release_pending
printf "START armed/live/next   = %u / %u / %u\n", $btn->start_armed, $btn->start_live_level, $btn->start_next_check_tick
printf "START raw/stable/count  = %u / %u / %u\n", $btn->start_raw_level, $btn->start_stable_level, $btn->start_stable_count
printf "START samples/synth p/r = %u / %u / %u\n", $btn->start_sample_count, $btn->start_synth_press_count, $btn->start_synth_release_count
printf "START input owner tick   = %u\n", $input_now
printf "START checks            = %u\n", $btn->start_checkpoint_count
printf "START press/release tick  = %u / %u\n", $btn->start_press_tick, $btn->start_release_tick
printf "START prep/warn/imm/rel   = %u / %u / %u / %u\n", $btn->start_ship_prep_count, $btn->start_ship_warning_count, $btn->start_ship_imminent_count, $btn->start_release_before_ship_count
printf "START pending/drop        = %u / %u\n", $btn->start_pending_event, $btn->start_pending_drop_count
printf "pending tick/hold ticks   = %u / %u\n", $btn->start_pending_timestamp, $btn->start_pending_hold_ticks
printf "power state/pmic state    = %u / %u\n", $sm->current_state[0], $sm->current_state[1]
printf "power last event/status   = %u / 0x%x\n", $sm->start_power_last_event, $sm->start_power_last_status
printf "power return state       = %u\n", $sm->start_power_return_state
printf "power count/hold ticks/tick = %u / %u / %u\n", $sm->start_power_event_count, $sm->start_power_last_hold_ticks, $sm->start_power_last_tick
printf "power prep/warn/imm/cancel = %u / %u / %u / %u\n", $sm->start_power_ship_prep_count, $sm->start_power_ship_warning_count, $sm->start_power_ship_imminent_count, $sm->start_power_cancel_count
printf "power quiesce count/status/tick = %u / 0x%x / %u\n", $sm->start_power_quiesce_request_count, $sm->start_power_quiesce_last_status, $sm->start_power_quiesce_last_tick
printf "barrier reason/count/status = %u / %u / 0x%x\n", $sm->power_quiesce_reason, $sm->power_quiesce_request_count, $sm->power_quiesce_last_status
printf "barrier ticks start/end = %u / %u\n", $sm->power_quiesce_start_tick, $sm->power_quiesce_end_tick
printf "barrier masks req/send/ack/ok/fail = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_required_mask, $sm->power_quiesce_send_ok_mask, $sm->power_quiesce_ack_ok_mask, $sm->power_quiesce_success_mask, $sm->power_quiesce_failure_mask
printf "barrier owner stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_owner_status[1], $sm->power_quiesce_owner_status[2], $sm->power_quiesce_owner_status[3], $sm->power_quiesce_owner_status[4], $sm->power_quiesce_owner_status[5], $sm->power_quiesce_owner_status[6]
printf "barrier send stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_send_status[1], $sm->power_quiesce_send_status[2], $sm->power_quiesce_send_status[3], $sm->power_quiesce_send_status[4], $sm->power_quiesce_send_status[5], $sm->power_quiesce_send_status[6]
printf "barrier ack stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_ack_status[1], $sm->power_quiesce_ack_status[2], $sm->power_quiesce_ack_status[3], $sm->power_quiesce_ack_status[4], $sm->power_quiesce_ack_status[5], $sm->power_quiesce_ack_status[6]
printf "power sw ship en/req/skip/status/tick = %u / %u / %u / 0x%x / %u\n", $sm->start_power_software_ship_enabled, $sm->start_power_software_ship_request_count, $sm->start_power_software_ship_skipped_count, $sm->start_power_software_ship_last_status, $sm->start_power_software_ship_last_tick
printf "admission req/allow/deny/susp/resume = %u / %u / %u / %u / %u\n", $rtos->admission_request_count, $rtos->admission_allow_count, $rtos->admission_deny_count, $rtos->admission_suspend_count, $rtos->admission_resume_count
printf "admission last action/result/reason/status = %u / %u / %u / 0x%x\n", $rtos->admission_last_action, $rtos->admission_last_result, $rtos->admission_last_reason, $rtos->admission_last_status
printf "admission suspend by-system/action resume reason/status = %u / %u / %u / 0x%x\n", $rtos->admission_runtime_suspended_by_system, $rtos->admission_runtime_suspended_action, $rtos->admission_runtime_resume_reason, $rtos->admission_runtime_resume_status
printf "UI page/shutdown/countdown = %u / %u / %u\n", $ui->current_page, $ui->shutdown_state, $ui->shutdown_countdown_seconds
printf "display page/shutdown/countdown = %u / %u / %u\n", $owner->display_ui_page, $owner->display_ui_shutdown_state, $owner->display_ui_shutdown_countdown_seconds
printf "pmic MR/sw ship status   = 0x%x / 0x%x\n", $owner->power_driver_mr_shipping_mode_status, $owner->power_driver_software_shipping_mode_status
printf "pmic sw ship count/tick = %u / %u\n", $owner->power_software_ship_request_count, $owner->power_software_ship_request_tick
printf "pmic sw ship request    = %u (manual request flag; do not set during normal START tests)\n", g_ps_hw6_pmic_software_ship_request
printf "states: START idle=0 press=1 long=2 prep=3 warn=4 imminent=5 released=6\n"
printf "ui shutdown: NONE=0 PREP=1 WARNING=2 IMMINENT=3 CANCELLED=4 LOW_BOOT=5 LOW_CHARGE=6 FLASH_INIT=7 FLASH_DONE=8 FLASH_ERROR=9 MSC_EXPORT=10 MSC_ACTIVE=11 MSC_RECLAIM=12 MSC_DONE=13 MSC_ERROR=14 MSC_RECOVERY=15\n"
printf "events: prep=1 warning=2 imminent=3 release-before-ship=4\n"
printf "admission actions: MSC_ENTER=1 MSC_EXIT=2 PKG_INSTALL=3 POWER_START_SHUT_PREP=4 POWER_BATT_CRIT_PREP=5 POWER_BOOT_LOW_PREP=6\n"
printf "power: PWR_ACTIVE_LP=2 PWR_ACTIVE_RT=3 PWR_SHIP_PREP=8 PMIC_MONITOR=3 PMIC_SHIP_PENDING=8\n"
