set pagination off
set $b = &g_ps_input_buttons_probe
set $rt = &g_ps_hw6_rtos_probe
printf "--- HW6 A/B/L/R button FSM scaffold ---\n"
printf "api/edges/presses/ignored = %u / %u / %u / %u\n", $b->api_version, $b->isr_edge_count, $b->press_count, $b->ignored_edge_count
printf "pending/last pin/button/event/level/tick = 0x%x / %u / %u / %u / %u / %u\n", $b->pending_mask, $b->last_pin, $b->last_button_id, $b->last_event, $b->last_level, $b->last_tick
printf "ticks deb p/r long rep-start/period stuck chord = %u / %u / %u / %u / %u / %u / %u\n", $b->button_debounce_press_ticks, $b->button_debounce_release_ticks, $b->button_long_press_ticks, $b->button_repeat_start_ticks, $b->button_repeat_period_ticks, $b->button_stuck_ticks, $b->button_chord_window_ticks
printf "counts deb p/r accept p/r long repeat stuck bounce = %u / %u / %u / %u / %u / %u / %u / %u\n", $b->button_debounce_press_count, $b->button_debounce_release_count, $b->button_press_accept_count, $b->button_release_accept_count, $b->button_long_count, $b->button_repeat_count, $b->button_stuck_count, $b->button_bounce_reject_count
printf "logical counts event/p/r/l/rep/ch/stuck = %u / %u / %u / %u / %u / %u / %u\n", $b->logical_event_count, $b->logical_press_count, $b->logical_release_count, $b->logical_long_count, $b->logical_repeat_count, $b->logical_chord_count, $b->logical_stuck_count
printf "logical last event/button/mask/tick/hold = %u / %u / 0x%x / %u / %u\n", $b->logical_last_event, $b->logical_last_button_id, $b->logical_last_mask, $b->logical_last_timestamp, $b->logical_last_hold_ticks
printf "input policy api/event/deliv/supp/lock = %u / %u / %u / %u / %u\n", $rt->input_policy_api_version, $rt->input_policy_event_count, $rt->input_policy_deliver_count, $rt->input_policy_suppress_count, $rt->input_policy_lock_active
printf "input policy last event/button/mask/target/reason/status = %u / %u / 0x%x / %u / %u / 0x%x\n", $rt->input_policy_last_event, $rt->input_policy_last_button_id, $rt->input_policy_last_mask, $rt->input_policy_last_target, $rt->input_policy_last_reason, $rt->input_policy_last_status
printf "input policy runtime/life/page/pkg/shutdown = %u / %u / %u / %u / %u\n", $rt->input_policy_last_runtime_class, $rt->input_policy_last_runtime_lifecycle, $rt->input_policy_last_ui_page, $rt->input_policy_last_package_state, $rt->input_policy_last_shutdown_state
printf "state A/B/L/R        = %u / %u / %u / %u\n", $b->button_state[0], $b->button_state[1], $b->button_state[2], $b->button_state[3]
printf "raw A/B/L/R          = %u / %u / %u / %u\n", $b->button_raw_level[0], $b->button_raw_level[1], $b->button_raw_level[2], $b->button_raw_level[3]
printf "press tick A/B/L/R   = %u / %u / %u / %u\n", $b->button_press_tick[0], $b->button_press_tick[1], $b->button_press_tick[2], $b->button_press_tick[3]
printf "release tick A/B/L/R = %u / %u / %u / %u\n", $b->button_release_tick[0], $b->button_release_tick[1], $b->button_release_tick[2], $b->button_release_tick[3]
printf "deadline A/B/L/R     = %u / %u / %u / %u\n", $b->button_deadline_tick[0], $b->button_deadline_tick[1], $b->button_deadline_tick[2], $b->button_deadline_tick[3]
printf "states: DISABLED=0 RELEASED=1 DEBOUNCE_PRESS=2 PRESSED=3 HELD=4 REPEAT=5 DEBOUNCE_RELEASE=6 STUCK=7 ERROR=8\n"
printf "buttons: A=1 B=2 L=3 R=4 START=5; events: PRESS=1 RELEASE=2\n"
printf "logical events: NONE=0 PRESS=1 RELEASE=2 LONG=3 REPEAT=4 CHORD=5 STUCK=6\n"
printf "policy targets: NONE=0 UI=1 RUNTIME=2; reasons: NONE=0 UI_FOCUS=1 RUNTIME_NOT_READY=2 LOCKED=3 UNSUPPORTED=4 INVALID=5 SEND_FAILED=6\n"
