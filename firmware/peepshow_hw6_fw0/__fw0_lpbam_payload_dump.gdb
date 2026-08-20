set pagination off
printf "--- HW6 LPBAM payload byte summary ---\n"
printf "cursor row/count col/count = %u / %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_cursor_start_row, g_ps_hw6_owner_probe.display_lpbam_cursor_row_count, g_ps_hw6_owner_probe.display_lpbam_cursor_start_column, g_ps_hw6_owner_probe.display_lpbam_cursor_column_count
printf "payload frames/chunks/bytes = %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_payload_frame_count, g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count, g_ps_hw6_owner_probe.display_lpbam_payload_bytes
printf "admission api/status/reason sequence/chunks/payload = %u/0x%x/%u %u/%u %u/%u %u/%u\n", ps_lpbam_display_admission.api_version, ps_lpbam_display_admission.status, ps_lpbam_display_admission.reason, ps_lpbam_display_admission.sequence_used, ps_lpbam_display_admission.sequence_capacity, ps_lpbam_display_admission.chunk_used, ps_lpbam_display_admission.chunk_capacity, ps_lpbam_display_admission.payload_used_bytes, ps_lpbam_display_admission.payload_capacity_bytes
printf "sequence first/count dirty start/count = %u/%u %u/%u | %u/%u %u/%u | %u/%u %u/%u | %u/%u %u/%u\n", ps_lpbam_display_sequence[0].first_chunk, ps_lpbam_display_sequence[0].chunk_count, ps_lpbam_display_sequence[0].dirty_start_row, ps_lpbam_display_sequence[0].dirty_row_count, ps_lpbam_display_sequence[1].first_chunk, ps_lpbam_display_sequence[1].chunk_count, ps_lpbam_display_sequence[1].dirty_start_row, ps_lpbam_display_sequence[1].dirty_row_count, ps_lpbam_display_sequence[2].first_chunk, ps_lpbam_display_sequence[2].chunk_count, ps_lpbam_display_sequence[2].dirty_start_row, ps_lpbam_display_sequence[2].dirty_row_count, ps_lpbam_display_sequence[3].first_chunk, ps_lpbam_display_sequence[3].chunk_count, ps_lpbam_display_sequence[3].dirty_start_row, ps_lpbam_display_sequence[3].dirty_row_count
set $ci = g_ps_hw6_owner_probe.display_lpbam_cursor_start_column >> 3
set $f = 0
while $f < 4
  set $chunk = ps_lpbam_display_sequence[$f].first_chunk
  set $p = ps_lpbam_display_tx[$chunk]
  set $len = ps_lpbam_display_tx_len[$chunk]
  printf "frame %u ptr=%p len=%u cursor_byte=%u\n", $f, $p, $len, $ci
  if $p != 0
    printf "  command=0x%02x\n", ((unsigned char *)$p)[0]
    set $r = 0
    while $r < 9
      set $off = 1 + ($r * 20)
      if ($off + 19) < $len
        set $sum = 0
        set $b = 0
        while $b < 18
          set $sum = $sum + ((unsigned char *)$p)[$off + 1 + $b]
          set $b = $b + 1
        end
        printf "  rec %u addr=%u sum=0x%04x b[%u..%u]=%02x %02x %02x %02x dummy=%02x\n", $r, ((unsigned char *)$p)[$off], $sum, $ci, $ci + 3, ((unsigned char *)$p)[$off + 1 + $ci], ((unsigned char *)$p)[$off + 2 + $ci], ((unsigned char *)$p)[$off + 3 + $ci], ((unsigned char *)$p)[$off + 4 + $ci], ((unsigned char *)$p)[$off + 19]
      end
      set $r = $r + 1
    end
  end
  set $f = $f + 1
end
printf "--- end HW6 LPBAM payload byte summary ---\n"
