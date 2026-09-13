set pagination off
if (g_ps_object_trace_probe.api_version != 1) || (g_ps_object_latency_probe.api_version != 3)
  printf "NOT armed: reflash the object TraceX build and use its ELF.\n"
else
  if (s_ps_scene_runtime_development_objects == 0) || (g_ps_ui_router_probe.current_page != 6) || (g_ps_object_lpbam_probe.enabled != 0) || (g_ps_object_trace_probe.active != 0) || (g_ps_object_trace_probe.armed != 0) || (g_ps_object_latency_probe.active != 0) || (g_ps_object_latency_probe.request != 0) || (g_ps_object_candidate_probe.leased != 0) || (g_ps_object_development_probe.render_request != g_ps_object_development_probe.render_complete)
    printf "NOT armed: run the AWAKE HOME/AWAY fixture; finish outstanding work first. Reset if a previous capture is still armed.\n"
  else
    set g_ps_object_trace_probe.request = 1
    printf "Queued one awake TraceX capture. Resume, wait one second, then press L/R once to CHANGE selection.\n"
    printf "After the move, halt and source __fw0_object_trace_prints.gdb, then __fw0_tracex_dump.gdb.\n"
    printf "The runtime restarts the existing trace ring, checks the cycle counter and freezes after that transaction. Do not halt during the press.\n"
    printf "Tracing changes timing. This run is not STOP2 or untraced power evidence.\n"
  end
end
