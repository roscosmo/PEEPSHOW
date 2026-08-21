set pagination off
printf "--- HW6 LPBAM admission fallback ---\n"
printf "rtos/owner api = %u / %u\n", g_ps_hw6_rtos_probe.version, g_ps_hw6_owner_probe.version
printf "variant = %u\n", g_display_renderer_waiting_test_variant
printf "admission api/status/reason sequence/chunks/payload = %u / 0x%x / %u %u/%u %u/%u %u/%u\n", ps_lpbam_display_admission.api_version, ps_lpbam_display_admission.status, ps_lpbam_display_admission.reason, ps_lpbam_display_admission.sequence_used, ps_lpbam_display_admission.sequence_capacity, ps_lpbam_display_admission.chunk_used, ps_lpbam_display_admission.chunk_capacity, ps_lpbam_display_admission.payload_used_bytes, ps_lpbam_display_admission.payload_capacity_bytes
printf "display prepare/status/ready/active = 0x%x / 0x%x / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_prepare_status, g_ps_hw6_owner_probe.display_lpbam_status, g_ps_hw6_owner_probe.display_lpbam_ready, g_ps_hw6_owner_probe.display_lpbam_active
printf "backend requested/selected/status/held = %u / %u / 0x%x / %u\n", g_ps_hw6_rtos_probe.stop2_display_wait_backend_requested, g_ps_hw6_rtos_probe.stop2_display_wait_backend_selected, g_ps_hw6_rtos_probe.stop2_display_wait_backend_status, g_ps_hw6_rtos_probe.stop2_display_wait_backend_held_ready
printf "auto entry/block/pending = %u / 0x%x / 0x%x\n", g_ps_hw6_rtos_probe.stop2_auto_entry_count, g_ps_hw6_rtos_probe.stop2_auto_blocker_mask, g_ps_hw6_rtos_probe.stop2_auto_pending_mask
printf "owner STOP2 count/status = %u / 0x%x\n", g_ps_hw6_owner_sm_probe.stop2_request_count, g_ps_hw6_owner_sm_probe.stop2_last_status
printf "expected: rtos/owner api=53/28 admission status=1 reason=3 (CHUNKS), chunks=18/18, requested/selected=2/1, held=1, LPBAM ready/active=0/0, STOP2 count increments\n"
printf "--- end HW6 LPBAM admission fallback ---\n"
