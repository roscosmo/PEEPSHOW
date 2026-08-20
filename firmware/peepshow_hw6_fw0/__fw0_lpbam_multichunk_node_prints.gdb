set pagination off
printf "--- HW6 LPBAM multi-chunk trigger layout ---\n"
printf "active chunks/queue nodes = %u / %u\n", ps_lpbam_display_active_chunk_count, Queue1_Q.NodeNumber
set $chunk = 0
set $triggered = 0
while $chunk < ps_lpbam_display_active_chunk_count
  set $d = &Queue1_Q_DisplayBuf_Desc[$chunk]
  set $config_ctr2 = $d->pNodes[0].LinkRegisters[1]
  set $trigm = ($config_ctr2 >> 14) & 0x3
  set $trigsel = ($config_ctr2 >> 16) & 0x3f
  set $trigpol = ($config_ctr2 >> 24) & 0x3
  printf "chunk %u len=%u config trigger mode/selection/polarity = %u/%u/%u\n", $chunk, ps_lpbam_display_tx_len[$chunk], $trigm, $trigsel, $trigpol
  if $trigpol != 0
    set $triggered = $triggered + 1
  end
  set $chunk = $chunk + 1
end
printf "triggered chunk count = %u\n", $triggered
printf "expected: chunks 0 and 4 are triggered; all six continuation chunks are masked; triggered count=2\n"
printf "--- end HW6 LPBAM multi-chunk trigger layout ---\n"
