set pagination off
printf "--- HW6 V2 candidate owner-queue check ---\n"
if g_ps_object_candidate_probe.api_version != 2
  printf "NOT queued: candidate probe API 2 required. Check the flashed ELF.\n"
else
  if (g_ps_hw6_rtos_probe.runtime_complete == 0) || (g_ps_object_candidate_probe.leased != 0) || (g_ps_object_candidate_request != 0)
    printf "NOT queued: finish boot or wait for the previous candidate completion. Do not clear a lease manually.\n"
  else
    set g_ps_object_candidate_request = 1
    printf "Queued normal candidate check. Resume; wake with A/B if currently asleep. After two seconds, halt and source __fw0_object_candidate_prints.gdb.\n"
    printf "This checks the embedded dual-animation candidate, not the installed egg. No scene replacement, panel transfer, install or flash write.\n"
    printf "The running scene and A/B behaviour must remain intact; normal low-power animation must return afterward.\n"
  end
end
