set pagination off
printf "--- HW6 V2 candidate display rejection test ---\n"
if g_ps_object_candidate_probe.api_version != 1
  printf "NOT queued: candidate probe API 1 required. Check the flashed ELF.\n"
else
  if (g_ps_hw6_rtos_probe.runtime_complete == 0) || (g_ps_object_candidate_probe.leased != 0) || (g_ps_object_candidate_request != 0)
    printf "NOT queued: finish boot or wait for the previous candidate completion. Do not clear a lease manually.\n"
  else
    set g_ps_object_candidate_request = 2
    printf "Queued missing-sprite injection in the PRIVATE candidate catalog only. No active asset or package bytes are modified.\n"
    printf "Resume; wake with A/B if asleep. After two seconds halt and source __fw0_object_candidate_prints.gdb.\n"
    printf "Expect display/raster rejection, completed token and released lease. The existing scene, inputs and low-power animation must still work.\n"
  end
end
