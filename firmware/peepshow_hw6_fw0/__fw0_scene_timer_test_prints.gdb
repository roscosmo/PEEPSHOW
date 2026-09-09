set pagination off
printf "--- HW6 scene-owned timer test result ---\n"
if $_isvoid($ps_scene_timer_test_armed)
  printf "No test recorded in this debugger session. Source __fw0_scene_timer_test_enable.gdb first.\n"
else
  if $ps_scene_timer_test_armed != 1
    printf "The test was not armed.\n"
  else
    if (g_ps_scene_runtime_probe.api_version != 21) || (g_ps_hw6_rtos_probe.version != 82)
      printf "Probe API mismatch; do not interpret these results.\n"
    else
      if (g_ps_scene_runtime_probe.active == 0) || (s_ps_scene_runtime_scene_activation != $ps_scene_timer_test_owner)
        printf "The scene was replaced or stopped; its timer was cancelled. Re-arm in the scene being tested.\n"
      else
        printf "test counter (expected 1) = %d\n", s_ps_scene_runtime_variables[$ps_scene_timer_test_variable]
        printf "state activations during test = %u\n", s_ps_scene_runtime_state_activation - $ps_scene_timer_test_state
        printf "test slot configured/active/scope/deadline = %u / %u / %u / %u\n", ps_runtime_state_timers[$ps_scene_timer_test_binding].configured, ps_runtime_state_timers[$ps_scene_timer_test_binding].active, ps_runtime_state_timers[$ps_scene_timer_test_binding].scope, ps_runtime_state_timers[$ps_scene_timer_test_binding].deadline_tick
        printf "timer applied/error deltas = %u / %u\n", g_ps_hw6_rtos_probe.runtime_state_timer_applied_count - $ps_scene_timer_test_applied, g_ps_hw6_rtos_probe.runtime_state_timer_error_count - $ps_scene_timer_test_errors
        printf "RTC timer selections during test = %u\n", g_ps_hw6_rtos_probe.runtime_state_timer_rtc_select_count - $ps_scene_timer_test_rtc
        printf "Counter=1 proves the handler executed, not just that thRuntime woke. Active=0 proves the one-shot was consumed.\n"
        printf "State activations count transitions, not render updates. RTC selections>0 shows a timer wake was scheduled, not by itself a physical STOP2 power measurement.\n"
      end
    end
  end
end
