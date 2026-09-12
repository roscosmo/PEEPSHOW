set pagination off
if g_ps_object_latency_probe.api_version != 1
  printf "NOT armed: flash the latency build and use its ELF.\n"
else
  if (g_ps_object_latency_probe.active != 0) || (g_ps_object_candidate_probe.leased != 0) || (g_ps_object_development_probe.lease_fault != 0) || (g_ps_object_development_probe.render_request != g_ps_object_development_probe.render_complete)
    printf "NOT armed: resume until the current transaction completes. Never clear a lease manually.\n"
  else
    if (s_ps_scene_runtime_development_objects == 0) || (g_ps_ui_router_probe.current_page != 6)
      printf "NOT armed: launch the HOME/AWAY scene and halt while playing.\n"
    else
      set g_ps_object_latency_probe.request = 1
      printf "Armed ONE runtime button press. Resume, press L or R to CHANGE selection, then halt after the visible move.\n"
      printf "Releases and animation will not overwrite it. Source __fw0_object_latency_prints.gdb.\n"
      printf "Measurement begins at runtime receipt: physical edge, debounce and pre-runtime wake time are excluded. Do not halt during the press.\n"
    end
  end
end
