set pagination off
printf "--- HW6 V2 LPBAM payload preparation only ---\n"
if g_ps_object_lpbam_prepare_probe.api_version != 1
  printf "Probe mismatch: requires V2 LPBAM preparation API 1. Check the flashed ELF.\n"
else
  printf "queued / request / complete = %u / %u / %u\n", g_ps_object_lpbam_prepare_request, g_ps_object_lpbam_prepare_probe.request_count, g_ps_object_lpbam_prepare_probe.complete_count
  printf "schedule / payload status / reason = 0x%x / 0x%x / %u\n", g_ps_object_lpbam_prepare_probe.schedule_status, g_ps_object_lpbam_prepare_probe.payload_status, g_ps_object_lpbam_prepare_probe.reason
  printf "steps / quantum / first remaining ms / frames composed = %u / %u / %u / %u\n", g_ps_object_lpbam_prepare_probe.step_count, g_ps_object_lpbam_prepare_probe.quantum_ms, g_ps_object_lpbam_prepare_probe.initial_remaining_ms, g_ps_object_lpbam_prepare_probe.frames_composed
  printf "sequence / chunks / payload used / capacity = %u / %u / %u / %u\n", g_ps_object_lpbam_prepare_probe.sequence_used, g_ps_object_lpbam_prepare_probe.chunk_used, g_ps_object_lpbam_prepare_probe.payload_used_bytes, g_ps_object_lpbam_prepare_probe.payload_capacity_bytes
  printf "LPBAM ready / prearmed / active = %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_ready, g_ps_hw6_owner_probe.display_lpbam_prearmed, g_ps_hw6_owner_probe.display_lpbam_active
  printf "display request / complete / status / lease fault = %u / %u / 0x%x / %u\n", g_ps_object_development_probe.render_request, g_ps_object_development_probe.render_complete, g_ps_object_development_probe.render_status, g_ps_object_development_probe.lease_fault
  printf "Expected numbered fixture: request=complete>0, schedule/payload/reason=0/0/0, steps/quantum=4/400, first remaining=1..400, composed=5 (includes wrap), sequence=4, chunks<=18, bytes<=capacity.\n"
  printf "Ready/prearmed/active must stay zero. Successful packing is NOT autonomous playback or timing proof. V2 remains awake-only; do not force STOP2.\n"
  printf "Payload reasons: NONE=0 ARGUMENT=1 SEQUENCE=2 CHUNKS=3 PAYLOAD=4 BUILD=5. A rejected schedule or payload must not change the awake animation.\n"
end
