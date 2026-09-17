set pagination off
if (g_ps_object_trace_probe.api_version != 2) || (g_ps_object_latency_probe.api_version != 3)
  printf "NOT armed: reflash the object TraceX build and use its ELF.\n"
else
  if (s_ps_scene_runtime_development_objects == 0) || (g_ps_ui_router_probe.current_page != 6) || (g_ps_hw6_rtos_probe.runtime_lifecycle != 2) || (g_ps_object_trace_probe.request != 0) || (g_ps_object_trace_probe.active != 0) || (g_ps_object_trace_probe.armed != 0) || (g_ps_object_latency_probe.active != 0) || (g_ps_object_latency_probe.request != 0) || (g_ps_object_candidate_probe.leased != 0) || (g_ps_object_development_probe.lease_fault != 0) || (g_ps_object_development_probe.render_request != g_ps_object_development_probe.render_complete)
    printf "NOT armed: run an installed or development V2 object scene; finish outstanding work first. Reset if a previous capture is still armed.\n"
  else
    set g_ps_object_trace_probe.request = 1
    printf "Queued ONE runtime button transaction. Keep the installed egg; no development scene-enable helper is needed.\n"
    printf "For GUI Lobby/Garden: in quiet Lobby resume, wait one second, then press A once to enter Garden.\n"
    printf "Observe the change without halting. Let the timer sound finish, then halt and source __fw0_object_trace_prints.gdb, then __fw0_tracex_dump.gdb.\n"
    printf "The runtime restarts the existing trace ring, checks the cycle counter and freezes after that transaction. Do not halt during the press.\n"
    printf "STOP2 and clock policy are unchanged. If asleep before arming, the request is serviced at runtime receipt of the press.\n"
    printf "Analyse matching RECEIVE/DONE markers only: DWT does not measure sleep duration or pre-runtime wake latency. Tracing changes timing; this is not untraced power evidence.\n"
  end
end
