set pagination off
printf "--- HW6 V2 LPBAM payload preparation only ---\n"
if g_ps_object_lpbam_prepare_probe.api_version != 1
  printf "NOT queued: this helper requires V2 LPBAM preparation API 1. Check the flashed ELF.\n"
else
  if (s_ps_scene_runtime_development_objects == 0) || (g_ps_hw6_rtos_probe.runtime_lifecycle != 2) || (g_ps_ui_router_probe.current_page != 6)
    printf "NOT queued: launch the development fixture with __fw0_object_scene_awake_enable.gdb from the shell first, then halt while the fixture is running.\n"
  else
    if (g_ps_object_development_probe.lease_fault != 0) || (g_ps_hw6_owner_probe.display_lpbam_active != 0) || (g_ps_hw6_owner_probe.display_lpbam_prearmed != 0)
      printf "NOT queued: display lease fault or autonomous playback active. Reset and launch the awake fixture again.\n"
    else
      set g_ps_object_lpbam_prepare_request = 1
      printf "Queued. Resume for one second, halt, then source __fw0_object_scene_lpbam_prepare_prints.gdb.\n"
      printf "The display owner packs the current complete scene into LPBAM payloads once. No DMA start, STOP2 entry, installation or flash write is requested.\n"
      printf "Animation and A/B must remain normal afterward. Repeat after moving the lower square to exercise a different state.\n"
    end
  end
end
