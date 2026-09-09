set pagination off
set input-radix 10
echo \n--- HW6 package / MSC workflow ---\n
if g_ps_package_workflow_probe.api_version != 1
  echo Workflow probe API mismatch. Use the ELF matching the flashed firmware.\n
else
  if g_ps_package_workflow_probe.sequence == 0
    echo No shell workflow recorded since boot; timing fields are not a completed-action result.\n
  end
  printf "api/sequence/action/active = %u / %u / %u / %u\n", g_ps_package_workflow_probe.api_version, g_ps_package_workflow_probe.sequence, g_ps_package_workflow_probe.action, g_ps_package_workflow_probe.active
  printf "phase/status/terminal event = %u / 0x%x / %u\n", g_ps_package_workflow_probe.phase, g_ps_package_workflow_probe.status, g_ps_package_workflow_probe.terminal_event
  printf "ticks accepted/displayed/work/end = %u / %u / %u / %u\n", g_ps_package_workflow_probe.accepted_tick, g_ps_package_workflow_probe.displayed_tick, g_ps_package_workflow_probe.work_start_tick, g_ps_package_workflow_probe.end_tick
  printf "display status/send failures/duplicates = 0x%x / %u / %u\n", g_ps_package_workflow_probe.display_status, g_ps_package_workflow_probe.display_send_failures, g_ps_package_workflow_probe.duplicate_count
  printf "preflight count/status/scene/reason = %u / 0x%x / %u / %u\n", g_ps_package_workflow_probe.validation_count, g_ps_package_workflow_probe.validation_status, g_ps_package_workflow_probe.validation_scene, g_ps_package_workflow_probe.validation_reason
  printf "preflight pending/reserved = %u\n", ps_package_validation_busy
  if g_ps_package_workflow_probe.tick_hz != 0
    if g_ps_package_workflow_probe.display_status == 0
      printf "accepted-to-notice ms = %llu\n", ((unsigned long long)(unsigned int)(g_ps_package_workflow_probe.displayed_tick - g_ps_package_workflow_probe.accepted_tick) * 1000) / g_ps_package_workflow_probe.tick_hz
    end
    set $wf_phase = 1
    while $wf_phase < 16
      if g_ps_package_workflow_probe.phase_count[$wf_phase] != 0
        printf "phase %u: completed ms / visits / HCLK Hz / OSPI policy Hz = %llu / %u / %u / %u\n", $wf_phase, ((unsigned long long)g_ps_package_workflow_probe.phase_ticks[$wf_phase] * 1000) / g_ps_package_workflow_probe.tick_hz, g_ps_package_workflow_probe.phase_count[$wf_phase], g_ps_package_workflow_probe.phase_cpu_hz[$wf_phase], g_ps_package_workflow_probe.phase_ospi_hz[$wf_phase]
      end
      set $wf_phase = $wf_phase + 1
    end
  end
  echo Actions: MSC_ENTER=1 MSC_EXIT=2 INSTALL=3 SCAN=4 LAUNCH=5.\n
  echo Phases: STARTING=1 PREPARING=2 SCANNING=3 READING=4 VALIDATING=5 ERASE=6 WRITE=7 VERIFY=8 COMMIT=9 LOAD=10 LAUNCH=11 EXPORT=12 RECLAIM=13 DONE=14 ERROR=15.\n
  echo Preflight reasons: HEADER_CRC=3 DIGEST=4 CHUNK=5 CHUNK_CRC_OR_RESIDENCY=6 GRAPH=10 RENDER=11 WAITING=12 CAPACITY=13 UNSUPPORTED=14 HASH=15 ASSET=16 AUDIO=17.\n
  echo Expected: display status=0 before work starts; final active/status=0/0 on success. Invalid scenes fail preflight before VALID or flash commit.\n
  echo Durations are wall time, not CPU utilization. HCLK is sampled at phase entry; OSPI is the clock-policy readback. DONE does not prove the final panel transfer.\n
end
echo --- end package / MSC workflow ---\n
