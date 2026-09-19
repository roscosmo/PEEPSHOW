if g_ps_system_time_probe.api_version != 1
  echo Unsupported system-time probe. Check flashed firmware and ELF.\n
else
  if g_ps_system_time_probe.pending != 0 || g_ps_system_time_request != 0
    echo A time request is pending. Resume to complete it; never clear pending manually.\n
  else
    echo Requested local date/time (not written to the physical RTC):\n
    p g_ps_system_time_request_local
    set var g_ps_system_time_request = 2
    echo Queued one SET using those fields. Resume, waking normally if asleep.\n
    echo After a second, halt and source __fw0_system_time_prints.gdb. Invalid dates are rejected.\n
    echo Mapping is retained across qualified warm reset, not shipment/power loss. Shell TIME also uses this service; calendar events remain deferred.\n
  end
end
