set pagination off
printf "--- HW6 LPBAM streaming compiler ---\n"
printf "admission api/status/reason = %u / 0x%x / %u\n", ps_lpbam_display_admission.api_version, ps_lpbam_display_admission.status, ps_lpbam_display_admission.reason
printf "sequence/chunks/payload = %u/%u %u/%u %u/%u\n", ps_lpbam_display_admission.sequence_used, ps_lpbam_display_admission.sequence_capacity, ps_lpbam_display_admission.chunk_used, ps_lpbam_display_admission.chunk_capacity, ps_lpbam_display_admission.payload_used_bytes, ps_lpbam_display_admission.payload_capacity_bytes
printf "payload wire bytes = %u\n", ps_lpbam_display_admission.payload_wire_bytes
printf "scratch A/B addresses and bytes = %p / %p / %u\n", ps_lpbam_display_frame_a, ps_lpbam_display_frame_b, sizeof(ps_lpbam_display_frame_a)
set $step = 0
while $step < ps_lpbam_display_active_sequence_count
  printf "step %u first chunk/count dirty start/count = %u/%u %u/%u\n", $step, ps_lpbam_display_sequence[$step].first_chunk, ps_lpbam_display_sequence[$step].chunk_count, ps_lpbam_display_sequence[$step].dirty_start_row, ps_lpbam_display_sequence[$step].dirty_row_count
  set $chunk = 0
  while $chunk < ps_lpbam_display_sequence[$step].chunk_count
    set $index = ps_lpbam_display_sequence[$step].first_chunk + $chunk
    printf "  chunk %u pool index/len = %u/%u\n", $chunk, $index, ps_lpbam_display_tx_len[$index]
    set $chunk = $chunk + 1
  end
  set $step = $step + 1
end
printf "owner phases/steps/cadence/start = %u / %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_phase_count, g_ps_hw6_owner_probe.display_lpbam_sequence_frame_count, g_ps_hw6_owner_probe.display_lpbam_cadence_ms, g_ps_hw6_owner_probe.display_lpbam_sequence_start_frame
printf "owner elements count/id0/phases0 = %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_element_count, g_ps_hw6_owner_probe.display_lpbam_element_id[0], g_ps_hw6_owner_probe.display_lpbam_element_phase_count[0]
printf "expected cursor: admission 3/0/0, sequence 4/12, chunks 4/12, payload 575/9216, four one-chunk steps\n"
printf "--- end HW6 LPBAM streaming compiler ---\n"
