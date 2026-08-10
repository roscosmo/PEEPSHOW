set pagination off

printf "--- HW6 START shipping-prep scaffold ---\n"
set $btn = &g_ps_input_buttons_probe
set $sm = &g_ps_hw6_owner_sm_probe
set $owner = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
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
printf "power sw ship en/req/skip/status/tick = %u / %u / %u / 0x%x / %u\n", $sm->start_power_software_ship_enabled, $sm->start_power_software_ship_request_count, $sm->start_power_software_ship_skipped_count, $sm->start_power_software_ship_last_status, $sm->start_power_software_ship_last_tick
printf "UI page/shutdown/countdown = %u / %u / %u\n", $ui->current_page, $ui->shutdown_state, $ui->shutdown_countdown_seconds
printf "display page/shutdown/countdown = %u / %u / %u\n", $owner->display_ui_page, $owner->display_ui_shutdown_state, $owner->display_ui_shutdown_countdown_seconds
printf "pmic MR/sw ship status   = 0x%x / 0x%x\n", $owner->power_driver_mr_shipping_mode_status, $owner->power_driver_software_shipping_mode_status
printf "pmic sw ship count/tick = %u / %u\n", $owner->power_software_ship_request_count, $owner->power_software_ship_request_tick
printf "pmic sw ship request    = %u (manual request flag; do not set during normal START tests)\n", g_ps_hw6_pmic_software_ship_request
printf "states: START idle=0 press=1 long=2 prep=3 warn=4 imminent=5 released=6\n"
printf "ui shutdown: NONE=0 PREP=1 WARNING=2 IMMINENT=3 CANCELLED=4\n"
printf "events: prep=1 warning=2 imminent=3 release-before-ship=4\n"
printf "power: PWR_ACTIVE_LP=2 PWR_ACTIVE_RT=3 PWR_SHIP_PREP=8 PMIC_MONITOR=3 PMIC_SHIP_PENDING=8\n"
