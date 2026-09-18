if g_ps_system_time_probe.api_version != 1
  echo Unsupported system-time probe. Check flashed firmware and ELF.\n
else
  if g_ps_system_time_probe.pending != 0 || g_ps_system_time_request != 0
    echo A time request is pending. Resume to complete it; never clear pending manually.\n
  else
    set var g_ps_system_time_request = 1
    echo Queued one local-time READ. Resume, waking normally if asleep.\n
    echo After a second, halt and source __fw0_system_time_prints.gdb. No periodic time polling is enabled.\n
  end
end
