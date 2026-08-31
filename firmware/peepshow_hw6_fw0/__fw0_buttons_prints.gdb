set pagination off
set $b = &g_ps_input_buttons_probe
set $rt = &g_ps_hw6_rtos_probe
set $o = &g_ps_hw6_owner_probe
printf "--- HW6 A/B/L/R button FSM scaffold ---\n"
printf "api/edges/presses/ignored = %u / %u / %u / %u\n", $b->api_version, $b->isr_edge_count, $b->press_count, $b->ignored_edge_count
printf "pending/last pin/button/event/level/tick = 0x%x / %u / %u / %u / %u / %u\n", $b->pending_mask, $b->last_pin, $b->last_button_id, $b->last_event, $b->last_level, $b->last_tick
printf "ticks deb p/r long rep-start/period stuck chord = %u / %u / %u / %u / %u / %u / %u\n", $b->button_debounce_press_ticks, $b->button_debounce_release_ticks, $b->button_long_press_ticks, $b->button_repeat_start_ticks, $b->button_repeat_period_ticks, $b->button_stuck_ticks, $b->button_chord_window_ticks
printf "counts deb p/r accept p/r long repeat stuck bounce = %u / %u / %u / %u / %u / %u / %u / %u\n", $b->button_debounce_press_count, $b->button_debounce_release_count, $b->button_press_accept_count, $b->button_release_accept_count, $b->button_long_count, $b->button_repeat_count, $b->button_stuck_count, $b->button_bounce_reject_count
printf "logical counts event/p/r/l/rep/ch/stuck = %u / %u / %u / %u / %u / %u / %u\n", $b->logical_event_count, $b->logical_press_count, $b->logical_release_count, $b->logical_long_count, $b->logical_repeat_count, $b->logical_chord_count, $b->logical_stuck_count
printf "logical last event/button/mask/tick/hold = %u / %u / 0x%x / %u / %u\n", $b->logical_last_event, $b->logical_last_button_id, $b->logical_last_mask, $b->logical_last_timestamp, $b->logical_last_hold_ticks
printf "START state/active/hold/menu count/pending = %u / %u / %u / %u / %u\n", $b->start_state, $b->start_active, $b->start_hold_ticks, $b->start_system_menu_count, $b->start_system_menu_pending
printf "START ship prep/warn/imminent/clear/release = %u / %u / %u / %u / %u\n", $b->start_ship_prep_count, $b->start_ship_warning_count, $b->start_ship_imminent_count, $b->start_ship_display_clear_count, $b->start_release_before_ship_count
printf "system menu request/send/tick = %u / 0x%x / %u\n", $rt->start_system_menu_request_count, $rt->start_system_menu_send_status, $rt->start_system_menu_last_tick
printf "system root resume/page/focus = %u / %u / %u\n", g_ps_ui_router_probe.resume_available, g_ps_ui_router_probe.current_page, g_ps_ui_router_probe.focus_index
printf "resume action/count/suspend/resume/render = %u / %u / %u / %u / 0x%x\n", $rt->ui_action_runtime_resume_count, $rt->ui_action_system_menu_enter_count, $rt->runtime_suspend_count, $rt->runtime_resume_count, $rt->runtime_resume_render_status
printf "shipping clear request/send/wait/ack/owner/result = %u / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->start_shipping_display_clear_request_count, $rt->start_shipping_display_clear_send_status, $rt->start_shipping_display_clear_wait_status, $rt->start_shipping_display_clear_ack_flags, $rt->start_shipping_display_clear_owner_status, $rt->start_shipping_display_clear_status
printf "shipping clear owner count/status/tick = %u / 0x%x / %u\n", $o->display_shipping_clear_count, $o->display_shipping_clear_status, $o->display_shipping_clear_tick
printf "input policy api/event/deliv/supp/lock = %u / %u / %u / %u / %u\n", $rt->input_policy_api_version, $rt->input_policy_event_count, $rt->input_policy_deliver_count, $rt->input_policy_suppress_count, $rt->input_policy_lock_active
printf "input policy ui/runtime/overlay = %u / %u / %u\n", $rt->input_policy_ui_deliver_count, $rt->input_policy_runtime_deliver_count, $rt->input_policy_overlay_deliver_count
printf "input policy last event/button/mask/target/reason/status = %u / %u / 0x%x / %u / %u / 0x%x\n", $rt->input_policy_last_event, $rt->input_policy_last_button_id, $rt->input_policy_last_mask, $rt->input_policy_last_target, $rt->input_policy_last_reason, $rt->input_policy_last_status
printf "input policy runtime/life/page/pkg/shutdown = %u / %u / %u / %u / %u\n", $rt->input_policy_last_runtime_class, $rt->input_policy_last_runtime_lifecycle, $rt->input_policy_last_ui_page, $rt->input_policy_last_package_state, $rt->input_policy_last_shutdown_state
printf "runtime input count/button = %u / %u\n", $rt->runtime_input_event_count, $rt->runtime_input_button_count
printf "runtime input last event/button/mask/status/tick = %u / %u / 0x%x / 0x%x / %u\n", $rt->runtime_input_last_event, $rt->runtime_input_last_button_id, $rt->runtime_input_last_mask, $rt->runtime_input_last_status, $rt->runtime_input_last_tick
printf "admission api/req/allow/deny/suspend = %u / %u / %u / %u / %u\n", $rt->admission_api_version, $rt->admission_request_count, $rt->admission_allow_count, $rt->admission_deny_count, $rt->admission_suspend_count
printf "admission last action/result/reason/status/tick = %u / %u / %u / 0x%x / %u\n", $rt->admission_last_action, $rt->admission_last_result, $rt->admission_last_reason, $rt->admission_last_status, $rt->admission_last_tick
printf "admission runtime/life/page/pkg/shutdown/overlay = %u / %u / %u / %u / %u / %u\n", $rt->admission_last_runtime_class, $rt->admission_last_runtime_lifecycle, $rt->admission_last_ui_page, $rt->admission_last_package_state, $rt->admission_last_shutdown_state, $rt->admission_last_overlay_active
printf "state A/B/L/R        = %u / %u / %u / %u\n", $b->button_state[0], $b->button_state[1], $b->button_state[2], $b->button_state[3]
printf "raw A/B/L/R          = %u / %u / %u / %u\n", $b->button_raw_level[0], $b->button_raw_level[1], $b->button_raw_level[2], $b->button_raw_level[3]
printf "press tick A/B/L/R   = %u / %u / %u / %u\n", $b->button_press_tick[0], $b->button_press_tick[1], $b->button_press_tick[2], $b->button_press_tick[3]
printf "release tick A/B/L/R = %u / %u / %u / %u\n", $b->button_release_tick[0], $b->button_release_tick[1], $b->button_release_tick[2], $b->button_release_tick[3]
printf "deadline A/B/L/R     = %u / %u / %u / %u\n", $b->button_deadline_tick[0], $b->button_deadline_tick[1], $b->button_deadline_tick[2], $b->button_deadline_tick[3]
printf "states: DISABLED=0 RELEASED=1 DEBOUNCE_PRESS=2 PRESSED=3 HELD=4 REPEAT=5 DEBOUNCE_RELEASE=6 STUCK=7 ERROR=8\n"
printf "buttons: A=1 B=2 L=3 R=4 START=5; events: PRESS=1 RELEASE=2\n"
printf "logical events: NONE=0 PRESS=1 RELEASE=2 LONG=3 REPEAT=4 CHORD=5 STUCK=6\n"
printf "policy targets: NONE=0 UI=1 RUNTIME=2; reasons: NONE=0 UI_FOCUS=1 RUNTIME_NOT_READY=2 LOCKED=3 UNSUPPORTED_EVENT=4 INVALID=5 SEND_FAILED=6 OVERLAY=7 RUNTIME_FOCUS=8 UNSUPPORTED_CLASS=9\n"
printf "admission actions: NONE=0 MSC_ENTER=1 MSC_EXIT=2 PKG_INSTALL=3 POWER_SHUT_PREP=4 SYSTEM_MENU=7\n"
printf "admission results: DENY=0 ALLOW=1 ALLOW_AFTER_SUSPEND=2 reasons: NONE=0 UI_SHELL=1 OVERLAY=2 RUNTIME_SUSPENDED=3 INSTALLER=4 BUSY=5 SEND_FAILED=6 UNSUPPORTED=7\n"
