set pagination off
echo --- HW6 calendar midnight wake bench ---\n
if g_ps_calendar_probe.api_version != 1 || g_ps_system_time_probe.api_version != 1
  echo Not queued: firmware/probe mismatch.\n
else
  if g_ps_system_time_probe.pending != 0 || g_ps_system_time_request != 0 || g_ps_calendar_probe.request != 0
    echo Not queued: a time/calendar request is outstanding. Resume and allow it to complete.\n
  else
    set $calendar_delivered_before = g_ps_calendar_probe.delivered
    set $calendar_expiries_before = g_ps_calendar_probe.rtc_expiries
    set var g_ps_system_time_request_local.year = 2026
    set var g_ps_system_time_request_local.month = 9
    set var g_ps_system_time_request_local.day = 19
    set var g_ps_system_time_request_local.hour = 23
    set var g_ps_system_time_request_local.minute = 59
    set var g_ps_system_time_request_local.second = 40
    set var g_ps_calendar_probe.value = 0
    set var g_ps_calendar_probe.request = 2
    set var g_ps_system_time_request = 2
    echo Queued local SET to 2026-09-19 23:59:40 and one daily midnight registration. This CHANGES saved local time.\n
    echo Use a settled installed scene that normally enters STOP2, not an awake-only development session.\n
    echo Resume and leave controls alone for 25 seconds. No screen or sound change is expected.\n
    echo Then wake normally, halt, and source __fw0_calendar_prints.gdb. Do not reset or change low-power debug bits.\n
    echo This bench latches an occurrence in thPower, not a game handler. Restore your preferred time afterward.\n
  end
end
