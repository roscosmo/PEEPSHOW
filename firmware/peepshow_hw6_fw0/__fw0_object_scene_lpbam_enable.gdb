set pagination off
printf "--- HW6 development V2 autonomous object scene ---\n"
if (*(unsigned int *)(g_ps_object_development_egg + 36) == 0xb9c7cf95) && (*(unsigned int *)(g_ps_object_development_egg + 40) == 0x2f3539de)
  printf "NOT armed: this build contains HOME/AWAY. Use __fw0_object_scene_exits_enable.gdb instead.\n"
else
if (g_ps_object_lpbam_probe.api_version != 1) || (g_ps_object_development_probe.api_version != 4) || (g_ps_scene_runtime_probe.api_version != 22)
  printf "NOT armed: this helper requires LPBAM/development/scene APIs 1/4/22. Check the flashed ELF.\n"
else
  if (g_ps_object_development_probe.lease_fault != 0) || (g_ps_audio_package_probe.fault != 0)
    printf "NOT armed: reset after the display/audio fault.\n"
  else
    if (g_ps_hw6_rtos_probe.runtime_complete == 0) || ((g_ps_ui_router_probe.current_page != 1) && (g_ps_ui_router_probe.current_page != 2))
      printf "NOT armed: finish boot, HOLD START to open the shell MENU, then halt.\n"
    else
      set g_ps_object_development_request = 2
      printf "Queued. Resume normally. The installed package is unchanged.\n"
      printf "Dual-animation OS fixture: top digits 1,2,3,4 advance every 400 ms. The four-cell indicator below advances left-to-right every 800 ms.\n"
      printf "Each indicator position spans two digit frames. Their full combined pattern repeats after 3.2 seconds. Middle marker slots are B (left) and A (right).\n"
      printf "The marker starts in B. A moves it once into A; B moves it once into B. Pressing the already-selected button does nothing.\n"
      printf "The bottom slot fills once after 2 seconds, then stays fixed. The outlines and labels must never move.\n"
      printf "After each A/B change release everything: low-power animation must return without restarting either clip or shifting the marker/labels/outlines.\n"
      printf "HOLD START opens the shell; this development session stays awake while shell-suspended. Reset ends it.\n"
      printf "Do not force manual STOP2 or halt at the sleep edge. Observe first; halt after waking with A/B and source __fw0_object_scene_lpbam_prints.gdb.\n"
      printf "A debugger disconnect is not a reset: reconnect without resetting or reflashing to retain these results.\n"
    end
  end
end
end
