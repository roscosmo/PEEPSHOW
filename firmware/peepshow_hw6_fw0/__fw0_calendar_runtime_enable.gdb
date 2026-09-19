set pagination off
echo --- HW6 calendar-to-V2 handler bench ---\n
if g_ps_calendar_runtime_probe.api_version != 1 || g_ps_system_time_probe.api_version != 1
  echo Not queued: firmware/probe mismatch.\n
else
  if g_ps_system_time_probe.pending != 0 || g_ps_system_time_request != 0 || g_ps_calendar_runtime_probe.active != 0 || g_ps_calendar_runtime_probe.pending_ack != 0 || g_ps_calendar_runtime_probe.request != 0
    echo Not queued: a request or registration is active. Cancel with g_ps_calendar_runtime_probe.request=4 and resume before rearming.\n
  else
    if g_ps_egg_validation_probe.package_size != 1004
      echo Not queued: install the 1004-byte calendar_dispatch.egg first.\n
    else
      set var g_ps_system_time_request_local.year = 2026
      set var g_ps_system_time_request_local.month = 9
      set var g_ps_system_time_request_local.day = 19
      set var g_ps_system_time_request_local.hour = 23
      set var g_ps_system_time_request_local.minute = 59
      set var g_ps_system_time_request_local.second = 40
      set var g_ps_calendar_runtime_probe.binding = 1
      set var g_ps_calendar_runtime_probe.time_of_day = 0
      set var g_ps_calendar_runtime_probe.day_offset = 0
      set var g_ps_system_time_request = 2
      set var g_ps_calendar_runtime_probe.request = 1
      echo Queued local SET to 2026-09-19 23:59:40 and daily midnight handler. This changes saved local time.\n
      echo Resume in the installed Calendar scene and leave controls alone for 25 seconds. The empty square must fill once.\n
      echo A clears it without rearming. It must stay empty afterward. No audio is expected.\n
      echo Wake normally, halt, then source __fw0_calendar_runtime_prints.gdb. Do not change low-power debug bits.\n
    end
  end
end
