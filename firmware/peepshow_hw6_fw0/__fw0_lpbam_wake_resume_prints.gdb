set pagination off
printf "--- HW6 LPBAM reverse handoff ---\n"
printf "rtos/owner api = %u / %u\n", g_ps_hw6_rtos_probe.version, g_ps_hw6_owner_probe.version
printf "owner snapshot status/state sequence/frame/phase = 0x%x / %u / %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_wake_snapshot_status, g_ps_hw6_owner_probe.display_lpbam_wake_progress_state, g_ps_hw6_owner_probe.display_lpbam_wake_sequence_index, g_ps_hw6_owner_probe.display_lpbam_wake_sequence_frame, g_ps_hw6_owner_probe.display_lpbam_wake_phase
printf "owner node index/count = %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_wake_node_index, g_ps_hw6_owner_probe.display_lpbam_wake_node_count
printf "owner LPTIM count/period render/status = %u / %u / 0x%x / 0x%x\n", g_ps_hw6_owner_probe.display_lpbam_wake_lptim_count, g_ps_hw6_owner_probe.display_lpbam_wake_lptim_period, g_ps_hw6_owner_probe.display_lpbam_wake_render_status, g_ps_hw6_owner_probe.display_lpbam_abort_status
printf "rtos resume count/status/phase/remaining/deadline = %u / 0x%x / %u / %u / %u\n", g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_count, g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_status, g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_phase, g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_remaining_ticks, g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_deadline_tick
printf "blink phase/next/suppressed = %u / %u / %u\n", ps_display_blink_visible, ps_display_blink_next_tick, ps_display_blink_stop2_suppressed
printf "abort resume phase/status/tick = %u / 0x%x / %u\n", g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_phase, g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_status, g_ps_hw6_rtos_probe.stop2_lpbam_abort_resume_tick
printf "input enq/deq/drop logical/policy = %u / %u / %u / %u / %u\n", g_ps_hw6_rtos_probe.input_raw_enqueue_count, g_ps_hw6_rtos_probe.input_raw_dequeue_count, g_ps_hw6_rtos_probe.input_raw_drop_count, g_ps_input_buttons_probe.logical_event_count, g_ps_hw6_rtos_probe.input_policy_event_count
printf "STOP2 entry/status block/pending = %u / 0x%x / 0x%x / 0x%x\n", g_ps_hw6_rtos_probe.stop2_auto_entry_count, g_ps_hw6_owner_sm_probe.stop2_last_status, g_ps_hw6_rtos_probe.stop2_auto_blocker_mask, g_ps_hw6_rtos_probe.stop2_auto_pending_mask
printf "progress states: INVALID=0 WAITING=1 TRANSFERRING=2; expected snapshot/render/resume/abort status=0, remaining=1..25, input enq=deq drops=0\n"
printf "--- end HW6 LPBAM reverse handoff ---\n"
