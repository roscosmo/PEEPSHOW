set pagination off
if g_ps_object_latency_probe.api_version != 3
  printf "NOT armed: flash the latency build and use its ELF.\n"
else
  if (g_ps_object_latency_probe.active != 0) || (g_ps_object_candidate_probe.leased != 0) || (g_ps_object_development_probe.lease_fault != 0) || (g_ps_object_development_probe.render_request != g_ps_object_development_probe.render_complete)
    printf "NOT armed: resume until the current transaction completes. Never clear a lease manually.\n"
  else
    if (s_ps_scene_runtime_development_objects == 0) || (g_ps_ui_router_probe.current_page != 6)
      printf "NOT armed: launch an installed or development V2 object scene and halt while playing.\n"
    else
      set g_ps_object_latency_probe.request = 1
      printf "Armed ONE runtime button press. Resume, press one button that changes scene or selection, then halt after the visible change.\n"
      printf "For GUI Lobby/Garden: A in Lobby measures entry; B in Garden measures return. Do not halt during audio playback.\n"
      printf "Releases and animation will not overwrite it. Source __fw0_object_latency_prints.gdb.\n"
      printf "Measurement begins at runtime receipt: physical edge, debounce and pre-runtime wake time are excluded. Do not halt during the press.\n"
    end
  end
end
