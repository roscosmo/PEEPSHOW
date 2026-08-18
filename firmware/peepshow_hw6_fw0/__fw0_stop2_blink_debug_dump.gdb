set $rt = &g_ps_hw6_rtos_probe
set $sm = &g_ps_hw6_owner_sm_probe
set $ow = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
set $btn = &g_ps_input_buttons_probe
set $clk = &g_ps_hw6_clock_policy_probe
printf "--- HW6 STOP2 blink debug dump ---\n"
printf "backtrace follows:\n"
bt
printf "rtos api/runtime/boot/idlepark = %u / %u / %u / %u\n", $rt->version, $rt->runtime_complete, $rt->boot_power_done, $rt->boot_idle_peripheral_park_done
printf "owner api = %u\n", $ow->version
printf "auto enabled/check/entry/skip = %u / %u / %u / %u\n", $rt->stop2_auto_enabled, $rt->stop2_auto_check_count, $rt->stop2_auto_entry_count, $rt->stop2_auto_skip_count
printf "auto status/tick/next = 0x%x / %u / %u\n", $rt->stop2_auto_last_status, $rt->stop2_auto_last_tick, $rt->stop2_auto_next_tick
printf "auto idle start/live/required ticks = %u / %u / %u\n", $rt->stop2_auto_idle_start_tick, $rt->stop2_auto_idle_ticks, $rt->stop2_auto_required_idle_ticks
printf "auto block/pending/queue = 0x%x / 0x%x / 0x%x\n", $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask, $rt->stop2_auto_queue_pending_mask
printf "auto clock-release recheck count/tick/requester = %u / %u / %u\n", $rt->stop2_auto_clock_release_recheck_count, $rt->stop2_auto_clock_release_recheck_tick, $rt->stop2_auto_clock_release_recheck_requester
printf "auto display-clock idle preserve count/tick = %u / %u\n", $rt->stop2_auto_clock_idle_preserve_count, $rt->stop2_auto_clock_idle_preserve_tick
printf "auto elig/entry status = 0x%x / 0x%x\n", $rt->stop2_auto_eligibility_status, $rt->stop2_auto_entry_status
printf "stop2 blink handoff count/status/send/wait/ack/owner = %u / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_blink_handoff_request_count, $rt->stop2_blink_handoff_status, $rt->stop2_blink_handoff_send_status, $rt->stop2_blink_handoff_wait_status, $rt->stop2_blink_handoff_ack_flags, $rt->stop2_blink_handoff_owner_status
printf "elig ready/block/pending = %u / 0x%x / 0x%x\n", $rt->stop2_eligibility_ready, $rt->stop2_eligibility_blocker_mask, $rt->stop2_eligibility_pending_mask
printf "ui page/nav/modal/pkg/shutdown/pending = %u / %u / %u / %u / %u / %u\n", $ui->current_page, $ui->nav_state, $ui->modal_state, $ui->package_state, $ui->shutdown_state, $ui->pending_action
printf "display ui req/render/page/status = %u / %u / %u / 0x%x\n", $ow->display_ui_request_count, $ow->display_ui_render_count, $ow->display_ui_page, $ow->display_ui_status
printf "display blink req/render/tick/phase/status = %u / %u / %u / %u / 0x%x\n", $ow->display_blink_request_count, $ow->display_blink_render_count, $ow->display_blink_tick, $ow->display_blink_phase, $ow->display_blink_status
printf "display complete/success dirty first/last = %u / %u / %u / %u / %u\n", $ow->display_complete, $ow->display_success, $ow->display_dirty_row_count, $ow->display_dirty_first_row, $ow->display_dirty_last_row
printf "display primitive id/prev/current = %u / %u / %u\n", $ow->display_renderer_primitive_id, $ow->display_renderer_previous_focus_row, $ow->display_renderer_current_focus_row
printf "display ui primitive id/prev/current = %u / %u / %u\n", $ow->display_ui_primitive_id, $ow->display_ui_previous_focus_row, $ow->display_ui_current_focus_row
printf "display lpbam ready/page/status active/clear/reason = %u / %u / 0x%x / %u / %u / %u\n", $ow->display_lpbam_ready, $ow->display_lpbam_ready_page, $ow->display_lpbam_status, $ow->display_lpbam_active, $ow->display_lpbam_clear_count, $ow->display_lpbam_clear_reason
printf "display lpbam animation/source/focus = %u / %u / %u\n", $ow->display_lpbam_animation_id, $ow->display_lpbam_source_primitive_id, $ow->display_lpbam_focus_row
printf "display clocks req/rel/last reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->display_clock_request_count, $rt->display_clock_release_count, $rt->display_clock_last_reason, $rt->display_clock_last_capabilities, $rt->display_clock_last_status
printf "clock requester caps P/A/I/D/S/St/C/UI/Rt = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $clk->requester_capabilities[0], $clk->requester_capabilities[1], $clk->requester_capabilities[2], $clk->requester_capabilities[3], $clk->requester_capabilities[4], $clk->requester_capabilities[5], $clk->requester_capabilities[6], $clk->requester_capabilities[7], $clk->requester_capabilities[8]
printf "owner stop2 count/start/wake/end/last = %u / %u / %u / %u / 0x%x\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick, $sm->stop2_last_status
printf "owner stop2 quiesce/enter/clock/recover/gpio park/restore = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_quiesce_status, $sm->stop2_enter_status, $sm->stop2_clock_restore_status, $sm->stop2_recover_status, $sm->stop2_gpio_park_status, $sm->stop2_gpio_restore_status
printf "power quiesce status required/send/ack/success/fail = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_last_status, $sm->power_quiesce_required_mask, $sm->power_quiesce_send_ok_mask, $sm->power_quiesce_ack_ok_mask, $sm->power_quiesce_success_mask, $sm->power_quiesce_failure_mask
printf "wake count/tick/source/primary = %u / %u / 0x%x / %u\n", $rt->stop2_wake_classify_count, $rt->stop2_wake_classify_tick, $rt->stop2_wake_source_mask, $rt->stop2_wake_primary_cause
printf "wake GPIOA/B/C before = 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_gpioa_before_idr, $rt->stop2_wake_gpiob_before_idr, $rt->stop2_wake_gpioc_before_idr
printf "wake GPIOA/B/C after = 0x%x / 0x%x / 0x%x\n", $rt->stop2_wake_gpioa_after_idr, $rt->stop2_wake_gpiob_after_idr, $rt->stop2_wake_gpioc_after_idr
printf "input pending/start/logical/policy = 0x%x / %u / %u / %u\n", $btn->pending_mask, $btn->start_active, $btn->logical_event_count, $rt->input_policy_event_count
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 UNAVAILABLE=0xfffffffe NOT_RUN=0xffffffff\n"
printf "--- end HW6 STOP2 blink debug dump ---\n"
