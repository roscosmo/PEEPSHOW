set pagination off
printf "--- HW6 LPBAM payload byte summary ---\n"
printf "cursor row/count col/count = %u / %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_cursor_start_row, g_ps_hw6_owner_probe.display_lpbam_cursor_row_count, g_ps_hw6_owner_probe.display_lpbam_cursor_start_column, g_ps_hw6_owner_probe.display_lpbam_cursor_column_count
printf "payload frames/chunks/bytes = %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_payload_frame_count, g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count, g_ps_hw6_owner_probe.display_lpbam_payload_bytes
printf "frame dirty start/count = %u/%u %u/%u %u/%u %u/%u\n", ps_lpbam_display_dirty_start_row[0], ps_lpbam_display_dirty_row_count[0], ps_lpbam_display_dirty_start_row[1], ps_lpbam_display_dirty_row_count[1], ps_lpbam_display_dirty_start_row[2], ps_lpbam_display_dirty_row_count[2], ps_lpbam_display_dirty_start_row[3], ps_lpbam_display_dirty_row_count[3]
printf "frame chunk0 len = %u / %u / %u / %u\n", ps_lpbam_display_tx_len[0][0], ps_lpbam_display_tx_len[1][0], ps_lpbam_display_tx_len[2][0], ps_lpbam_display_tx_len[3][0]
set $ci = g_ps_hw6_owner_probe.display_lpbam_cursor_start_column >> 3
set $f = 0
while $f < 4
  set $p = ps_lpbam_display_tx[$f][0]
  set $len = ps_lpbam_display_tx_len[$f][0]
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