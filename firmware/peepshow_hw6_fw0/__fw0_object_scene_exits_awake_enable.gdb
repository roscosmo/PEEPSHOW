source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_scene_exits_enable.gdb
if g_ps_object_development_request == 4
  set g_ps_object_development_request = 3
  printf "OVERRIDE: this run is AWAKE ONLY. Same HOME/AWAY content, no automatic STOP2. Do not force STOP2. Reset before changing test mode.\n"
end
