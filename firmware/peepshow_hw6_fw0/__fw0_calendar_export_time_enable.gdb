set pagination off
echo --- HW6 exported calendar fixture: set clock only ---\n
if g_ps_system_time_probe.api_version != 1 || g_ps_egg_validation_probe.package_size != 1000
  echo Not queued: flash current firmware and install the 1000-byte calendar_export.egg first.\n
else
  if g_ps_system_time_probe.pending != 0 || g_ps_system_time_request != 0
    echo Not queued: a time request is pending. Resume and retry.\n
  else
    set var g_ps_system_time_request_local.year = 2026
    set var g_ps_system_time_request_local.month = 9
    set var g_ps_system_time_request_local.day = 19
    set var g_ps_system_time_request_local.hour = 23
    set var g_ps_system_time_request_local.minute = 59
    set var g_ps_system_time_request_local.second = 40
    set var g_ps_system_time_request = 2
    echo Queued local SET to 2026-09-19 23:59:40. This changes saved system time.\n
    echo No calendar registration is injected. The installed egg owns its daily schedule.\n
    echo Resume untouched for 25 seconds: the square must fill once. A clears it without rearming.\n
    echo Then wake normally, halt and source __fw0_calendar_runtime_prints.gdb.\n
  end
end
