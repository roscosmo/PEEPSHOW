set pagination off
printf "--- HW6 LPBAM permanent SRAM4 payload layout ---\n"
printf "variant/animation/source = %u / %u / %u\n", g_display_renderer_waiting_test_variant, g_ps_hw6_owner_probe.display_lpbam_animation_id, g_ps_hw6_owner_probe.display_lpbam_source_primitive_id
printf "admission api/status/reason = %u / 0x%x / %u\n", ps_lpbam_display_admission.api_version, ps_lpbam_display_admission.status, ps_lpbam_display_admission.reason
printf "sequence/chunks/payload wire/used/capacity = %u/%u %u/%u %u/%u/%u\n", ps_lpbam_display_admission.sequence_used, ps_lpbam_display_admission.sequence_capacity, ps_lpbam_display_admission.chunk_used, ps_lpbam_display_admission.chunk_capacity, ps_lpbam_display_admission.payload_wire_bytes, ps_lpbam_display_admission.payload_used_bytes, ps_lpbam_display_admission.payload_capacity_bytes
printf "guaranteed frames/spatial chunks/payload slots = %u / %u / %u\n", 3, 6, 18
set $slot = 0
set $occupied = 0
while $slot < 18
  printf "slot %02u ptr/capacity/len/band/occupied = %p / %u / %u / %u / %u\n", $slot, ps_lpbam_display_payload_slot[$slot], ps_lpbam_display_payload_slot_capacity[$slot], ps_lpbam_display_payload_slot_len[$slot], ps_lpbam_display_payload_slot_band[$slot], ps_lpbam_display_payload_slot_occupied[$slot]
  if ps_lpbam_display_payload_slot_occupied[$slot] != 0
    set $occupied = $occupied + 1
  end
  set $slot = $slot + 1
end
printf "occupied payload slots = %u\n", $occupied
set $step = 0
while $step < ps_lpbam_display_active_sequence_count
  printf "step %u first/count dirty start/count = %u/%u %u/%u\n", $step, ps_lpbam_display_sequence[$step].first_chunk, ps_lpbam_display_sequence[$step].chunk_count, ps_lpbam_display_sequence[$step].dirty_start_row, ps_lpbam_display_sequence[$step].dirty_row_count
  set $chunk = 0
  while $chunk < ps_lpbam_display_sequence[$step].chunk_count
    set $index = ps_lpbam_display_sequence[$step].first_chunk + $chunk
    printf "  transaction %u slot/len = %u/%u\n", $index, ps_lpbam_display_tx_payload_slot[$index], ps_lpbam_display_tx_len[$index]
    set $chunk = $chunk + 1
  end
  set $step = $step + 1
end
printf "queue nodes/start status = %u / 0x%x\n", g_ps_hw6_owner_probe.display_lpbam_queue_node_count, g_ps_hw6_owner_probe.display_lpbam_start_status
printf "expected: api/status/reason=4/0/0 sequence=3/12 chunks=18/18 payload=10494/10512/10512 occupied=18 queue nodes=126\n"
printf "expected: each step has six transactions of 583 bytes; every slot capacity is 584 bytes\n"
printf "--- end HW6 LPBAM permanent SRAM4 payload layout ---\n"
