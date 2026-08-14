set $rt = &g_ps_hw6_rtos_probe
set $ow = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 display LPBAM STOP2 admission handshake ---\n"
printf "rtos/owner api = %u / %u\n", $rt->version, $ow->version
printf "ui page/render = %u / %u\n", $ui->current_page, $ow->display_ui_render_count
printf "display backend req/selected/status/held = %u / %u / 0x%x / %u\n", $rt->stop2_display_wait_backend_requested, $rt->stop2_display_wait_backend_selected, $rt->stop2_display_wait_backend_status, $rt->stop2_display_wait_backend_held_ready
printf "display ready/page/render/status = %u / %u / %u / 0x%x\n", $ow->display_lpbam_ready, $ow->display_lpbam_ready_page, $ow->display_lpbam_ready_render_count, $ow->display_lpbam_status
printf "display prep count/tick/status = %u / %u / 0x%x\n", $ow->display_lpbam_prepare_count, $ow->display_lpbam_prepare_tick, $ow->display_lpbam_prepare_status
printf "display debug force ready count = %u\n", $ow->display_lpbam_debug_force_ready_count
printf "display abort count/tick/status = %u / %u / 0x%x\n", $ow->display_lpbam_abort_count, $ow->display_lpbam_abort_tick, $ow->display_lpbam_abort_status
printf "display clear count/reason = %u / %u\n", $ow->display_lpbam_clear_count, $ow->display_lpbam_clear_reason
printf "rtos prep count/tick/send/wait/ack = %u / %u / 0x%x / 0x%x / 0x%x\n", $rt->stop2_lpbam_prepare_request_count, $rt->stop2_lpbam_prepare_last_tick, $rt->stop2_lpbam_prepare_send_status, $rt->stop2_lpbam_prepare_wait_status, $rt->stop2_lpbam_prepare_ack_flags
printf "rtos prep owner/ready/clear = 0x%x / %u / %u\n", $rt->stop2_lpbam_prepare_owner_status, $rt->stop2_lpbam_prepare_ready_after, $rt->stop2_lpbam_prepare_display_clear_count
printf "rtos abort count/tick/send/wait/ack/owner = %u / %u / 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->stop2_lpbam_abort_request_count, $rt->stop2_lpbam_abort_last_tick, $rt->stop2_lpbam_abort_send_status, $rt->stop2_lpbam_abort_wait_status, $rt->stop2_lpbam_abort_ack_flags, $rt->stop2_lpbam_abort_owner_status
printf "late abort test count/tick/status/block = %u / %u / 0x%x / 0x%x\n", $rt->stop2_lpbam_abort_late_test_count, $rt->stop2_lpbam_abort_late_test_tick, $rt->stop2_lpbam_abort_late_test_status, $rt->stop2_lpbam_abort_late_test_blocker_mask
printf "late blocker count = %u\n", $rt->stop2_lpbam_late_blocker_count
printf "auto block/pending = 0x%x / 0x%x\n", $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask
printf "owner stop2 count/start/wake/end = %u / %u / %u / %u\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick
printf "blockers: INPUT=0x800 LPBAM_NOT_READY=0x2000; pending LPBAM=0x2; status: HAL_OK=0x0 HAL_ERROR=0x1 UNAVAILABLE=0xfffffffe NOT_RUN=0xffffffff\n"
printf "display backends: NONE=0 HELD_FRAME=1 LPBAM=2\n"
