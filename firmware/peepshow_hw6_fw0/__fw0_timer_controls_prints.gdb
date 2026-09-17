set pagination off
printf "--- HW6 installed timer controls fixture ---\n"
if g_ps_hw6_rtos_probe.version != 82 || g_ps_scene_runtime_probe.api_version != 22
  printf "Probe mismatch: expected RTOS/scene APIs 82/22. Check the loaded ELF.\n"
else
  if s_ps_installed_object_size != 3356 || s_ps_object_snapshot.count != 10
    printf "This helper expects the 3356-byte TIMER 10S egg with ten objects. Install it and select PLAY first.\n"
  else
    printf "scene/state/state activation / lifecycle = %u / %u / %u / %u\n", g_ps_scene_runtime_probe.scene_id, g_ps_scene_runtime_probe.state_id, s_ps_scene_runtime_state_activation, g_ps_hw6_rtos_probe.runtime_lifecycle
    printf "guard variable / last guard fill / last DONE fill = %d / %u / %u\n", s_ps_object_graph.variables[0], (s_ps_object_snapshot.objects[0].effective.flags & 1), (s_ps_object_snapshot.objects[1].effective.flags & 1)
    printf "timer configured/active/delay ticks / paused / paused remaining ticks = %u / %u / %u / %u / %u\n", ps_runtime_state_timers[4].configured, ps_runtime_state_timers[4].active, ps_runtime_state_timers[4].delay_ticks, ps_runtime_state_timer_paused, ps_runtime_state_timers[4].paused_remaining_ticks
    printf "timer deadline tick / rejected pending = %u / %u\n", ps_runtime_state_timers[4].deadline_tick, ps_runtime_state_timers[4].rejected_pending
    printf "timer due/applied/ignored/errors / RTC selects = %u / %u / %u / %u / %u\n", g_ps_hw6_rtos_probe.runtime_state_timer_due_count, g_ps_hw6_rtos_probe.runtime_state_timer_applied_count, g_ps_hw6_rtos_probe.runtime_state_timer_ignored_count, g_ps_hw6_rtos_probe.runtime_state_timer_error_count, g_ps_hw6_rtos_probe.runtime_state_timer_rtc_select_count
    printf "A Start, B Restart, L Cancel clear DONE. R toggles GUARD without changing the deadline. Filled GUARD permits DONE on expiry.\n"
    printf "Delay is 1000 ticks at 100 Hz. Paused remaining is meaningful only while paused; divide by 100 for seconds. Raw deadlines are rebased through STOP2, so do not compare them across sleep.\n"
    printf "Fresh entry: GUARD=1, DONE=0, timer active=0. False guard: one due/ignored increment, active=0, DONE=0; enabling GUARD alone must not retry.\n"
    printf "Counters are cumulative; compare before/after. HOLD START opens shell. Its pause must preserve remaining duration, not restart ten seconds. Do not halt during a timed observation.\n"
  end
end
