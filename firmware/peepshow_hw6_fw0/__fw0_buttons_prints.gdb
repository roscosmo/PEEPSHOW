set pagination off
set $b = &g_ps_input_buttons_probe
printf "--- HW6 A/B/L/R button FSM scaffold ---\n"
printf "api/edges/presses/ignored = %u / %u / %u / %u\n", $b->api_version, $b->isr_edge_count, $b->press_count, $b->ignored_edge_count
printf "pending/last pin/button/event/level/tick = 0x%x / %u / %u / %u / %u / %u\n", $b->pending_mask, $b->last_pin, $b->last_button_id, $b->last_event, $b->last_level, $b->last_tick
printf "ticks deb p/r long rep-start/period stuck chord = %u / %u / %u / %u / %u / %u / %u\n", $b->button_debounce_press_ticks, $b->button_debounce_release_ticks, $b->button_long_press_ticks, $b->button_repeat_start_ticks, $b->button_repeat_period_ticks, $b->button_stuck_ticks, $b->button_chord_window_ticks
printf "counts deb p/r accept p/r long repeat stuck bounce = %u / %u / %u / %u / %u / %u / %u / %u\n", $b->button_debounce_press_count, $b->button_debounce_release_count, $b->button_press_accept_count, $b->button_release_accept_count, $b->button_long_count, $b->button_repeat_count, $b->button_stuck_count, $b->button_bounce_reject_count
printf "state A/B/L/R        = %u / %u / %u / %u\n", $b->button_state[0], $b->button_state[1], $b->button_state[2], $b->button_state[3]
printf "raw A/B/L/R          = %u / %u / %u / %u\n", $b->button_raw_level[0], $b->button_raw_level[1], $b->button_raw_level[2], $b->button_raw_level[3]
printf "press tick A/B/L/R   = %u / %u / %u / %u\n", $b->button_press_tick[0], $b->button_press_tick[1], $b->button_press_tick[2], $b->button_press_tick[3]
printf "release tick A/B/L/R = %u / %u / %u / %u\n", $b->button_release_tick[0], $b->button_release_tick[1], $b->button_release_tick[2], $b->button_release_tick[3]
printf "deadline A/B/L/R     = %u / %u / %u / %u\n", $b->button_deadline_tick[0], $b->button_deadline_tick[1], $b->button_deadline_tick[2], $b->button_deadline_tick[3]
printf "states: DISABLED=0 RELEASED=1 DEBOUNCE_PRESS=2 PRESSED=3 HELD=4 REPEAT=5 DEBOUNCE_RELEASE=6 STUCK=7 ERROR=8\n"
printf "buttons: A=1 B=2 L=3 R=4 START=5; events: PRESS=1 RELEASE=2\n"