set pagination off
printf "--- HW6 LPBAM composite animation compiler ---\n"
printf "variant/animation/source = %u / %u / %u\n", g_display_renderer_waiting_test_variant, g_ps_hw6_owner_probe.display_lpbam_animation_id, g_ps_hw6_owner_probe.display_lpbam_source_primitive_id
printf "elements count ids phase counts = %u / %u,%u / %u,%u\n", g_ps_hw6_owner_probe.display_lpbam_element_count, g_ps_hw6_owner_probe.display_lpbam_element_id[0], g_ps_hw6_owner_probe.display_lpbam_element_id[1], g_ps_hw6_owner_probe.display_lpbam_element_phase_count[0], g_ps_hw6_owner_probe.display_lpbam_element_phase_count[1]
printf "element 0 phases = %u / %u / %u / %u\n", s_display_waiting_animation.elements[0].sequence_phase[0], s_display_waiting_animation.elements[0].sequence_phase[1], s_display_waiting_animation.elements[0].sequence_phase[2], s_display_waiting_animation.elements[0].sequence_phase[3]
printf "element 1 phases = %u / %u / %u / %u\n", s_display_waiting_animation.elements[1].sequence_phase[0], s_display_waiting_animation.elements[1].sequence_phase[1], s_display_waiting_animation.elements[1].sequence_phase[2], s_display_waiting_animation.elements[1].sequence_phase[3]
printf "admission api/status/reason = %u / 0x%x / %u\n", ps_lpbam_display_admission.api_version, ps_lpbam_display_admission.status, ps_lpbam_display_admission.reason
printf "sequence/chunks/payload wire/used/capacity = %u/%u %u/%u %u/%u/%u\n", ps_lpbam_display_admission.sequence_used, ps_lpbam_display_admission.sequence_capacity, ps_lpbam_display_admission.chunk_used, ps_lpbam_display_admission.chunk_capacity, ps_lpbam_display_admission.payload_wire_bytes, ps_lpbam_display_admission.payload_used_bytes, ps_lpbam_display_admission.payload_capacity_bytes
set $step = 0
while $step < ps_lpbam_display_active_sequence_count
  printf "step %u first/count dirty start/count = %u/%u %u/%u\n", $step, ps_lpbam_display_sequence[$step].first_chunk, ps_lpbam_display_sequence[$step].chunk_count, ps_lpbam_display_sequence[$step].dirty_start_row, ps_lpbam_display_sequence[$step].dirty_row_count
  set $chunk = 0
  while $chunk < ps_lpbam_display_sequence[$step].chunk_count
    set $index = ps_lpbam_display_sequence[$step].first_chunk + $chunk
    printf "  chunk %u pool index/len = %u/%u\n", $chunk, $index, ps_lpbam_display_tx_len[$index]
    set $chunk = $chunk + 1
  end
  set $step = $step + 1
end
printf "queue nodes/start status = %u / 0x%x\n", g_ps_hw6_owner_probe.display_lpbam_queue_node_count, g_ps_hw6_owner_probe.display_lpbam_start_status
printf "SPI AUTOCR/TRIGEN = 0x%x / %u\n", hspi3.Instance->AUTOCR, (hspi3.Instance->AUTOCR >> 21) & 0x1
printf "expected: variant/program=1/2 elements=2 ids=1,2 phases=2,4 admission=3/0/0 sequence=4/12 chunks=8/12 payload=7144/7151/9216 queue nodes=56 AUTOCR TRIGEN=0\n"
printf "expected: every step combines cursor and bar in two transactions of 983 and 803 bytes\n"
printf "--- end HW6 LPBAM composite animation compiler ---\n"
