set $rt = &g_ps_hw6_rtos_probe
set $ow = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
printf "--- HW6 LPBAM rearm debug ---\n"
printf "rtos/owner api = %u / %u\n", $rt->version, $ow->version
printf "backend override var = %u\n", g_ps_hw6_power_stop2_display_backend_override
printf "ui page/nav/request/render = %u / %u / %u / %u\n", $ui->current_page, $ui->nav_state, g_ps_ui_router_request, $ow->display_ui_render_count
printf "display complete/success/status/page = %u / %u / 0x%x / %u\n", $ow->display_complete, $ow->display_success, $ow->display_ui_status, $ow->display_ui_page
printf "blink visible/next/suppressed = %u / %u / %u\n", ps_display_blink_visible, ps_display_blink_next_tick, ps_display_blink_stop2_suppressed
printf "edge private pending/target/phase/render/page = %u / %u / %u / %u / %u\n", ps_stop2_lpbam_edge_request_pending, ps_stop2_lpbam_edge_target_tick, ps_stop2_lpbam_edge_start_phase, ps_stop2_lpbam_edge_render_count, ps_stop2_lpbam_edge_page
printf "edge probe state/req/run/status = %u / %u / %u / 0x%x\n", $rt->stop2_lpbam_edge_state, $rt->stop2_lpbam_edge_request_count, $rt->stop2_lpbam_edge_run_count, $rt->stop2_lpbam_edge_status
printf "edge probe req/target/run/ready tick = %u / %u / %u / %u\n", $rt->stop2_lpbam_edge_request_tick, $rt->stop2_lpbam_edge_target_tick, $rt->stop2_lpbam_edge_run_tick, $rt->stop2_lpbam_edge_ready_tick
printf "edge probe phase/miss = %u / %u\n", $rt->stop2_lpbam_edge_start_phase, $rt->stop2_lpbam_edge_miss_count
printf "lpbam ready/page/render/status/active = %u / %u / %u / 0x%x / %u\n", $ow->display_lpbam_ready, $ow->display_lpbam_ready_page, $ow->display_lpbam_ready_render_count, $ow->display_lpbam_status, $ow->display_lpbam_active
printf "lpbam prep count/tick/status fill/start = %u / %u / 0x%x / 0x%x / 0x%x\n", $ow->display_lpbam_prepare_count, $ow->display_lpbam_prepare_tick, $ow->display_lpbam_prepare_status, $ow->display_lpbam_fill_status, $ow->display_lpbam_start_status
printf "lpbam abort count/tick/status = %u / %u / 0x%x\n", $ow->display_lpbam_abort_count, $ow->display_lpbam_abort_tick, $ow->display_lpbam_abort_status
printf "auto enabled/check/entry/skip/status = %u / %u / %u / %u / 0x%x\n", $rt->stop2_auto_enabled, $rt->stop2_auto_check_count, $rt->stop2_auto_entry_count, $rt->stop2_auto_skip_count, $rt->stop2_auto_last_status
printf "auto idle start/live/required = %u / %u / %u\n", $rt->stop2_auto_idle_start_tick, $rt->stop2_auto_idle_ticks, $rt->stop2_auto_required_idle_ticks
printf "auto block/pending/queue = 0x%x / 0x%x / 0x%x\n", $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask, $rt->stop2_auto_queue_pending_mask
printf "input pending/start/logical/policy = 0x%x / %u / %u / %u\n", g_ps_input_buttons_probe.pending_mask, g_ps_input_buttons_probe.start_active, g_ps_input_buttons_probe.logical_event_count, $rt->input_policy_event_count
printf "clock caps/dom/readback = 0x%x / 0x%x / 0x%x\n", $rt->stop2_eligibility_clock_capabilities, $rt->stop2_eligibility_clock_domains, $rt->stop2_eligibility_readback_domains
printf "expected latch failure: suppressed=1, pending=0 or edge state cleared, req>run, auto block has 0x2000\n"
printf "--- end HW6 LPBAM rearm debug ---\n"
