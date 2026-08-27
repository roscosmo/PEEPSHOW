set pagination off
set $b = &g_ps_input_buttons_probe
set $rt = &g_ps_hw6_rtos_probe
set $ui = &g_ps_ui_router_probe
set $ow = &g_ps_hw6_owner_probe
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 ordered input and STOP2 veto ---\n"
printf "api rtos/buttons = %u / %u\n", $rt->version, $b->api_version
printf "raw ISR edges send/drop/process/recover = %u / %u / %u / %u / %u\n", $b->isr_edge_count, $b->raw_edge_send_count, $b->raw_edge_drop_count, $b->raw_edge_process_count, $b->raw_edge_recovery_count
printf "raw driver last status/tick = 0x%x / %u\n", $b->raw_edge_last_status, $b->raw_edge_last_timestamp
printf "raw queue enq/deq/drop/high/receive = %u / %u / %u / %u / %u\n", $rt->input_raw_enqueue_count, $rt->input_raw_dequeue_count, $rt->input_raw_drop_count, $rt->input_raw_queue_high_water, $rt->queue_receive_count[2]
printf "raw queue last status/button/active/tick = 0x%x / %u / %u / %u\n", $rt->input_raw_last_send_status, $rt->input_raw_last_button_id, $rt->input_raw_last_active, $rt->input_raw_last_timestamp
printf "FSM state A/B/L/R = %u / %u / %u / %u\n", $b->button_state[0], $b->button_state[1], $b->button_state[2], $b->button_state[3]
printf "FSM raw A/B/L/R = %u / %u / %u / %u\n", $b->button_raw_level[0], $b->button_raw_level[1], $b->button_raw_level[2], $b->button_raw_level[3]
printf "logical event/press/release/pending = %u / %u / %u / 0x%x\n", $b->logical_event_count, $b->logical_press_count, $b->logical_release_count, $b->pending_mask
printf "policy event/deliver/suppress last status = %u / %u / %u / 0x%x\n", $rt->input_policy_event_count, $rt->input_policy_deliver_count, $rt->input_policy_suppress_count, $rt->input_policy_last_status
printf "policy source buttons/START/combined = %u / %u / %u\n", $b->logical_event_count, $rt->runtime_interaction_package_start_count, $b->logical_event_count + $rt->runtime_interaction_package_start_count
printf "UI page/focus/button events/last event = %u / %u / %u / %u\n", $ui->current_page, $ui->focus_index, $ui->button_event_count, $ui->last_button_event
printf "display UI req/render blink req/render/phase = %u / %u / %u / %u / %u\n", $ow->display_ui_request_count, $ow->display_ui_render_count, $ow->display_blink_request_count, $ow->display_blink_render_count, $ow->display_blink_phase
printf "final input check/veto/status = %u / %u / 0x%x\n", $rt->stop2_final_input_check_count, $rt->stop2_final_input_veto_count, $rt->stop2_final_input_last_status
printf "final input enq/deq/queue GPIOA/B = %u / %u / 0x%x / 0x%x / 0x%x\n", $rt->stop2_final_input_enqueue_count, $rt->stop2_final_input_dequeue_count, $rt->stop2_final_input_queue_mask, $rt->stop2_final_input_gpioa_idr, $rt->stop2_final_input_gpiob_idr
printf "auto checks/entries/skips block/pending/queue = %u / %u / %u / 0x%x / 0x%x / 0x%x\n", $rt->stop2_auto_check_count, $rt->stop2_auto_entry_count, $rt->stop2_auto_skip_count, $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask, $rt->stop2_auto_queue_pending_mask
printf "owner STOP2 count/start/WFI/wake/end/status = %u / %u / %u / %u / %u / 0x%x\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wfi_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick, $sm->stop2_last_status
printf "expected: queue enq=deq, drops=0, logical presses match UI actions, final successful snapshot queue=0/status=0\n"
printf "buttons: A=1 B=2 L=3 R=4; states: RELEASED=1 DEBOUNCE_PRESS=2 PRESSED=3 HELD=4 REPEAT=5 DEBOUNCE_RELEASE=6\n"
printf "--- end ordered input and STOP2 veto ---\n"
