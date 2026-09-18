echo --- HW6 owner-routed local time ---\n
printf "API / debug request / operation (READ=1 SET=2) = %u / %u / %u\n", g_ps_system_time_probe.api_version, g_ps_system_time_request, g_ps_system_time_probe.operation
printf "request / complete / pending / send status / rejected messages = %u / %u / %u / 0x%x / %u\n", g_ps_system_time_probe.request, g_ps_system_time_probe.complete, g_ps_system_time_probe.pending, g_ps_system_time_probe.send_status, g_ps_system_time_probe.rejected_messages
if g_ps_system_time_probe.request == 0 || g_ps_system_time_probe.request != g_ps_system_time_probe.complete || g_ps_system_time_request != 0 || g_ps_system_time_probe.send_status != 0
  echo No completed result for the latest request. Queue success alone does not prove a time read or set.\n
else
  printf "local status / RTC read status = %u / 0x%x\n", g_ps_system_time_probe.result.status, g_ps_system_time_probe.result.source_status
  echo Raw elapsed-source milliseconds at completion:\n
  p/u g_ps_system_time_probe.result.source_ms
  if g_ps_system_time_probe.result.status == 0
    printf "local = %04u-%02u-%02u %02u:%02u:%02u.%03u\n", g_ps_system_time_probe.result.snapshot.local.year, g_ps_system_time_probe.result.snapshot.local.month, g_ps_system_time_probe.result.snapshot.local.day, g_ps_system_time_probe.result.snapshot.local.hour, g_ps_system_time_probe.result.snapshot.local.minute, g_ps_system_time_probe.result.snapshot.local.second, g_ps_system_time_probe.result.snapshot.millisecond
    printf "weekday (Monday=1) / generation = %u / %u\n", g_ps_system_time_probe.result.snapshot.weekday, g_ps_system_time_probe.result.snapshot.generation
  end
end
echo Local status: OK=0 UNSET=1 ARGUMENT=2 SOURCE_LOST=3 RANGE=4 GENERATION_EXHAUSTED=5.\n
echo This is the last requested snapshot, not a live clock. Run the READ helper again for a fresh sample.\n
echo SET changes only the local mapping. Relative timers, RTC wake deadlines and raw RTC are not edited.\n
echo A completed request can remain pending briefly until UI consumes it. Never clear its lease manually.\n
echo Reset deliberately returns local time to UNSET; retention and calendar scheduling are not implemented.\n
