set pagination off
printf "--- HW6 development HOME/AWAY scene exits ---\n"
if (g_ps_object_development_probe.api_version != 4) || (g_ps_scene_runtime_probe.api_version != 22) || (g_ps_object_candidate_probe.api_version != 2) || (g_ps_object_lpbam_probe.api_version != 1)
  printf "NOT armed: expected development/scene/candidate/LPBAM APIs 4/22/2/1. Flash this build and use its ELF.\n"
else
  if (g_ps_object_development_egg_size != 3440) || (*(unsigned int *)(g_ps_object_development_egg + 36) != 0xb9c7cf95) || (*(unsigned int *)(g_ps_object_development_egg + 40) != 0x2f3539de)
    printf "NOT armed: the linked egg is not the OS HOME/AWAY fixture. Build with --scene-exits and flash that build.\n"
  else
    if (g_ps_object_development_probe.lease_fault != 0) || (g_ps_audio_package_probe.fault != 0) || (g_ps_object_candidate_probe.leased != 0)
      printf "NOT armed: resolve the display/audio fault or outstanding candidate first. Do not clear a lease manually.\n"
    else
      if (g_ps_hw6_rtos_probe.runtime_complete == 0) || ((g_ps_ui_router_probe.current_page != 1) && (g_ps_ui_router_probe.current_page != 2))
        printf "NOT armed: finish boot, HOLD START to open the shell MENU, then halt.\n"
      else
        set g_ps_object_development_request = 4
        printf "Queued. Resume normally. No installation or flash write is requested.\n"
        printf "HOME and AWAY labels identify the scenes. Digits loop 1-2-3-4 at 400 ms in both. L/R selects the left/right outlined slot without restarting digits.\n"
        printf "A goes HOME -> AWAY. B goes AWAY -> HOME. A in AWAY and B in HOME do nothing. Every scene entry restarts at digit 1 with the marker left.\n"
        printf "HOME's bottom slot fills after 2 seconds. AWAY's bottom slot stays empty; AWAY automatically returns HOME after 6 seconds even while using L/R.\n"
        printf "First leave HOME quickly with A: its old 2-second timer must not fill AWAY's slot or return early. Then let AWAY return automatically. HOME starts with an empty slot again.\n"
        printf "Also try B for an early return. Release everything between actions so automatic STOP2 can run; cadence and slot positions must stay correct.\n"
        printf "HOLD START opens the shell and suspends this development session awake. Reset ends it. Do not force manual STOP2.\n"
        printf "Observe first, wake with L/R to halt, then source __fw0_object_scene_exits_prints.gdb. If GDB disconnects, reconnect without reset/reflash.\n"
      end
    end
  end
end
