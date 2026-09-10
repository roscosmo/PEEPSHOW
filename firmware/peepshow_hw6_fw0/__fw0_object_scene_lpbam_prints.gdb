set pagination off
printf "--- HW6 development V2 autonomous object scene ---\n"
if g_ps_object_lpbam_probe.api_version != 1
  printf "Probe mismatch: expected object LPBAM API 1.\n"
else
  printf "enabled / fault / launch status / scene active / development = %u / %u / 0x%x / %u / %u\n", g_ps_object_lpbam_probe.enabled, g_ps_object_lpbam_probe.fault, g_ps_object_development_probe.launch_status, g_ps_scene_runtime_probe.active, s_ps_scene_runtime_development_objects
  printf "publish count/status / sleep barrier = %u / 0x%x / %u\n", g_ps_object_lpbam_probe.publish_count, g_ps_object_lpbam_probe.publish_status, g_ps_object_lpbam_probe.barrier
  printf "schedule steps/quantum/residual ms = %u / %u / %u\n", g_ps_object_lpbam_prepare_probe.step_count, g_ps_object_lpbam_prepare_probe.quantum_ms, g_ps_object_lpbam_prepare_probe.initial_remaining_ms
  printf "payload status/reason steps/chunks/bytes = 0x%x / %u / %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_fill_status, ps_lpbam_display_admission.reason, g_ps_hw6_owner_probe.display_lpbam_payload_frame_count, g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count, ps_lpbam_display_admission.payload_used_bytes
  printf "LPBAM ready/prearmed/active commit count/status = %u / %u / %u / %u / 0x%x\n", g_ps_hw6_owner_probe.display_lpbam_ready, g_ps_hw6_owner_probe.display_lpbam_prearmed, g_ps_hw6_owner_probe.display_lpbam_active, g_ps_hw6_owner_probe.display_lpbam_commit_count, g_ps_hw6_owner_probe.display_lpbam_commit_status
  printf "first interval ticks / CCR1 / wake edge count = %u / %u / %u\n", g_ps_object_lpbam_probe.commit_remaining_ticks, g_ps_object_lpbam_probe.compare_count, g_ps_object_lpbam_probe.wake_compare_count
  printf "automatic attempts baseline/current / V2 WFI returns = %u / %u / %u\n", g_ps_object_lpbam_probe.entry_baseline, g_ps_hw6_rtos_probe.stop2_auto_entry_count, g_ps_object_lpbam_probe.physical_count
  printf "STOP2 blockers/pending / entry status = 0x%x / 0x%x / 0x%x\n", g_ps_hw6_rtos_probe.stop2_auto_blocker_mask, g_ps_hw6_rtos_probe.stop2_auto_pending_mask, g_ps_hw6_rtos_probe.stop2_auto_entry_status
  printf "sleep measured/reconciled status / latest ms / total missing ms = %u / %u / 0x%x / %u / %llu\n", g_ps_object_lpbam_probe.sleep_count, g_ps_object_lpbam_probe.reconciled_count, g_ps_object_lpbam_probe.clock_status, g_ps_object_lpbam_probe.sleep_ms, g_ps_object_lpbam_probe.missing_ms
  printf "wake snapshot/render/map/resume status frame/remaining ticks = 0x%x / 0x%x / 0x%x / 0x%x / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_wake_snapshot_status, g_ps_hw6_owner_probe.display_lpbam_wake_render_status, g_ps_hw6_owner_probe.display_lpbam_wake_preferred_map_status, g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_status, g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_sequence_frame, g_ps_hw6_rtos_probe.stop2_lpbam_wake_resume_remaining_ticks
  printf "timer due/applied/error RTC selections = %u / %u / %u / %u\n", g_ps_hw6_rtos_probe.runtime_state_timer_due_count, g_ps_hw6_rtos_probe.runtime_state_timer_applied_count, g_ps_hw6_rtos_probe.runtime_state_timer_error_count, g_ps_hw6_rtos_probe.runtime_state_timer_rtc_select_count
  printf "last object snapshot phase/remaining/marker x / reveal visible = %u / %u / %d / %u\n", s_ps_object_snapshot.objects[0].step, s_ps_object_snapshot.objects[0].remaining_ms, s_ps_object_snapshot.objects[1].effective.x, (s_ps_object_snapshot.objects[2].effective.flags & 1)
  if s_ps_object_snapshot.count > 8
    printf "last slow indicator phase/remaining ms / animation visible = %u / %u / %u\n", s_ps_object_snapshot.objects[8].step, s_ps_object_snapshot.objects[8].remaining_ms, s_ps_object_snapshot.objects[8].animation_visible
  end
  printf "display request/complete/result/fault = %u / %u / 0x%x / %u\n", g_ps_object_development_probe.render_request, g_ps_object_development_probe.render_complete, g_ps_object_development_probe.render_status, g_ps_object_development_probe.lease_fault
  printf "Expected after wake: enabled=1 fault=0, publish/wake statuses=0, V2 WFI returns>0, measured=reconciled, barrier=0. Current payload/commit fields can reset to NOT_RUN after a redraw. Limits: 12 steps, 18 chunks, 10512 bytes.\n"
  printf "Structured fixture: marker x=32(B) or 120(A), y=68; timer square x=76,y=116. Only the marker moves on A/B, once. All outlines and labels stay fixed.\n"
  printf "Current dual fixture: expected steps/quantum=8/400 and chunks/bytes=16/9344. Digits advance every 400 ms, the four-cell indicator every 800 ms; neither restarts on A/B or timer reveal.\n"
  printf "Request/commit counters alone do not prove low-power drawing. Require visible 1-2-3-4 cadence while asleep, low-current residency, A/B continuity, and the timer reveal.\n"
  printf "Snapshots describe the last runtime projection, not live autonomous frames. Active/ready can be zero after wake. A halted in-flight render is not a completed-render failure.\n"
end
