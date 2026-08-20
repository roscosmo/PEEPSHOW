set $rt = &g_ps_hw6_rtos_probe
set $ow = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
printf "--- HW6 HardFault dump ---\n"
printf "core registers:\n"
info registers r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 r10 r11 r12 sp lr pc xpsr msp psp primask basepri faultmask control
printf "fault regs CFSR/HFSR/DFSR/AFSR/MMFAR/BFAR = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", *(uint32_t*)0xE000ED28, *(uint32_t*)0xE000ED2C, *(uint32_t*)0xE000ED30, *(uint32_t*)0xE000ED3C, *(uint32_t*)0xE000ED34, *(uint32_t*)0xE000ED38
printf "SHCSR/ICSR/VTOR = 0x%x / 0x%x / 0x%x\n", *(uint32_t*)0xE000ED24, *(uint32_t*)0xE000ED04, *(uint32_t*)0xE000ED08
printf "backtrace follows:\n"
bt full
printf "rtos/owner api = %u / %u\n", $rt->version, $ow->version
printf "rtos lpbam edge state/req/run/status = %u / %u / %u / 0x%x\n", $rt->stop2_lpbam_edge_state, $rt->stop2_lpbam_edge_request_count, $rt->stop2_lpbam_edge_run_count, $rt->stop2_lpbam_edge_status
printf "rtos lpbam edge req/target/run/ready tick = %u / %u / %u / %u\n", $rt->stop2_lpbam_edge_request_tick, $rt->stop2_lpbam_edge_target_tick, $rt->stop2_lpbam_edge_run_tick, $rt->stop2_lpbam_edge_ready_tick
printf "rtos prep count/tick/send/wait/ack/owner/ready = %u / %u / 0x%x / 0x%x / 0x%x / 0x%x / %u\n", $rt->stop2_lpbam_prepare_request_count, $rt->stop2_lpbam_prepare_last_tick, $rt->stop2_lpbam_prepare_send_status, $rt->stop2_lpbam_prepare_wait_status, $rt->stop2_lpbam_prepare_ack_flags, $rt->stop2_lpbam_prepare_owner_status, $rt->stop2_lpbam_prepare_ready_after
printf "display prep/status/fill/clock/link/start = %u / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $ow->display_lpbam_prepare_count, $ow->display_lpbam_prepare_status, $ow->display_lpbam_fill_status, $ow->display_lpbam_clock_status, $ow->display_lpbam_link_status, $ow->display_lpbam_start_status
printf "display cursor row/count col/count = %u / %u / %u / %u\n", $ow->display_lpbam_cursor_start_row, $ow->display_lpbam_cursor_row_count, $ow->display_lpbam_cursor_start_column, $ow->display_lpbam_cursor_column_count
printf "display blink req/render/tick/phase/status = %u / %u / %u / %u / 0x%x\n", $ow->display_blink_request_count, $ow->display_blink_render_count, $ow->display_blink_tick, $ow->display_blink_phase, $ow->display_blink_status
printf "display payload frames/chunks/bytes = %u / %u / %u\n", $ow->display_lpbam_payload_frame_count, $ow->display_lpbam_payload_chunk_count, $ow->display_lpbam_payload_bytes
printf "display admission api/status/reason sequence/chunks/payload = %u/0x%x/%u %u/%u %u/%u %u/%u\n", $ow->display_lpbam_admission_api_version, $ow->display_lpbam_admission_status, $ow->display_lpbam_admission_reason, ps_lpbam_display_admission.sequence_used, ps_lpbam_display_admission.sequence_capacity, ps_lpbam_display_admission.chunk_used, ps_lpbam_display_admission.chunk_capacity, ps_lpbam_display_admission.payload_used_bytes, ps_lpbam_display_admission.payload_capacity_bytes
printf "sequence first/count dirty start/count = %u/%u %u/%u | %u/%u %u/%u | %u/%u %u/%u | %u/%u %u/%u\n", ps_lpbam_display_sequence[0].first_chunk, ps_lpbam_display_sequence[0].chunk_count, ps_lpbam_display_sequence[0].dirty_start_row, ps_lpbam_display_sequence[0].dirty_row_count, ps_lpbam_display_sequence[1].first_chunk, ps_lpbam_display_sequence[1].chunk_count, ps_lpbam_display_sequence[1].dirty_start_row, ps_lpbam_display_sequence[1].dirty_row_count, ps_lpbam_display_sequence[2].first_chunk, ps_lpbam_display_sequence[2].chunk_count, ps_lpbam_display_sequence[2].dirty_start_row, ps_lpbam_display_sequence[2].dirty_row_count, ps_lpbam_display_sequence[3].first_chunk, ps_lpbam_display_sequence[3].chunk_count, ps_lpbam_display_sequence[3].dirty_start_row, ps_lpbam_display_sequence[3].dirty_row_count
printf "auto block/pending = 0x%x / 0x%x\n", $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask
printf "--- end HW6 HardFault dump ---\n"
