set pagination off

printf "--- HW6 TraceX snapshot dump ---\n"
printf "enable status/runtime = 0x%x / %u\n", g_ps_hw6_tracex_enable_status, g_ps_hw6_tracex_runtime_enabled
printf "buffer addr/bytes     = 0x%x / %u\n", g_ps_hw6_tracex_buffer_address, g_ps_hw6_tracex_buffer_bytes
printf "registry total/avail = %u / %u\n", _tx_trace_total_registry_entries, _tx_trace_available_registry_entries
set $ps_hw6_tracex_base = (char *)g_ps_hw6_tracex_buffer_address
set $ps_hw6_tracex_end = $ps_hw6_tracex_base + g_ps_hw6_tracex_buffer_bytes
set $ps_hw6_tracex_can_dump = (g_ps_hw6_tracex_enable_status == 0)
set $ps_hw6_tracex_can_dump = $ps_hw6_tracex_can_dump && (g_ps_hw6_tracex_runtime_enabled == 1)
set $ps_hw6_tracex_can_dump = $ps_hw6_tracex_can_dump && (g_ps_hw6_tracex_buffer_address != 0)
set $ps_hw6_tracex_can_dump = $ps_hw6_tracex_can_dump && (g_ps_hw6_tracex_buffer_bytes != 0)
if $ps_hw6_tracex_can_dump
  dump binary memory G:/PEEPSHOW/firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot.trx $ps_hw6_tracex_base $ps_hw6_tracex_end
  printf "dumped latest TraceX buffer to G:/PEEPSHOW/firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot.trx\n"
  shell powershell -NoProfile -Command "$src='G:/PEEPSHOW/firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot.trx'; $stamp=Get-Date -Format 'yyyyMMdd_HHmmss'; $dst='G:/PEEPSHOW/firmware/peepshow_hw6_fw0/TraceFiles/__fw0_tracex_snapshot_' + $stamp + '.trx'; Copy-Item -LiteralPath $src -Destination $dst; Write-Host ('timestamped TraceX snapshot: ' + $dst)"
else
  printf "SKIPPED: TraceX is not enabled or buffer metadata is invalid.\n"
end
printf "status: TX_SUCCESS=0x0 TX_FEATURE_NOT_ENABLED=0xff\n"