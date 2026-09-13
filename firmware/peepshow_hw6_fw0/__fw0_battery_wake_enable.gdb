set pagination off
printf "--- HW6 quiet battery wake test ---\n"
if g_ps_hw6_battery_wake_probe.api_version != 1 || g_ps_hw6_battery_wake_probe.configured != 1
  printf "NOT armed: requires the matching battery-wake firmware and completed power-owner initialization.\n"
else
  set var g_ps_hw6_battery_wake_test_request_ms = 15000
  printf "Queued one deadline shortening to 15 seconds. No voltage, charger, or shutdown settings are changed.\n"
  printf "Use a settled HOME scene after its 2-second reveal, or a settled shell page. Do not use the AWAKE ONLY development test.\n"
  printf "Resume and leave all buttons/joystick alone for 20 seconds. The battery wake is quiet: no new screen or sound is expected.\n"
  printf "Then wake normally if needed, halt, and source __fw0_battery_wake_prints.gdb. Reconnect without resetting if GDB disconnects.\n"
  printf "A fresh battery reading before expiry can legitimately replace the test deadline. Zero expiry delta is not a pass.\n"
end
