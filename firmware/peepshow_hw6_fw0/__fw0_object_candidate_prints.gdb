set pagination off
printf "--- HW6 V2 candidate owner-queue result ---\n"
if g_ps_object_candidate_probe.api_version != 1
  printf "Probe mismatch: candidate API 1 required. Do not interpret other fields.\n"
else
  printf "request/mode/queued/leased/status = %u / %u / %u / %u / 0x%x\n", g_ps_object_candidate_probe.request_id, g_ps_object_candidate_probe.mode, g_ps_object_candidate_request, g_ps_object_candidate_probe.leased, g_ps_object_candidate_probe.status
  printf "refused/late completions = %u / %u\n", g_ps_object_candidate_probe.refused, g_ps_object_candidate_probe.late_completions
  printf "profile/reason/loader graph/schedule steps/quantum = 0x%x / %u / %u / 0x%x / 0x%x / %u / %u\n", g_ps_object_candidate_probe.profile_status, g_ps_object_candidate_probe.profile_reason, g_ps_object_candidate_probe.loader_reason, g_ps_object_candidate_probe.graph_status, g_ps_object_candidate_probe.schedule_status, g_ps_object_candidate_probe.steps, g_ps_object_candidate_probe.quantum_ms
  printf "queue/wait display started/complete/status = 0x%x / 0x%x / %u / %u / 0x%x\n", g_ps_object_candidate_probe.queue_status, g_ps_object_candidate_probe.wait_status, g_ps_object_candidate_probe.display_started, g_ps_object_candidate_probe.display_complete, g_ps_object_candidate_probe.display_status
  printf "runtime clock request/release display clock request/release = 0x%x / 0x%x / 0x%x / 0x%x\n", g_ps_object_candidate_probe.runtime_clock_status, g_ps_object_candidate_probe.runtime_clock_release_status, g_ps_object_candidate_probe.clock_status, g_ps_object_candidate_probe.clock_release_status
  printf "projection/raster/failed step/composed payload status/reason/chunks/bytes = 0x%x / 0x%x / %u / %u / 0x%x / %u / %u / %u\n", g_ps_object_candidate_probe.projection_status, g_ps_object_candidate_probe.raster_status, g_ps_object_candidate_probe.failed_step, g_ps_object_candidate_probe.frames_composed, g_ps_object_candidate_probe.payload_status, g_ps_object_candidate_probe.payload_reason, g_ps_object_candidate_probe.chunks, g_ps_object_candidate_probe.bytes
  printf "Normal dual candidate: status/profile/graph/schedule/queue/wait/display=0, steps/quantum=8/400, composed=9, chunks/bytes=16/9344.\n"
  printf "Missing-sprite mode 2: profile/graph/schedule=0, display/raster/status=1, payload reason BUILD=5. It is intentional rejection, not a broken running scene.\n"
  printf "Both: request=display complete, leased=0, all four clock statuses=0. NOT_RUN=0xffffffff. Lease stays set after timeout until matching completion; never clear it manually.\n"
  printf "Display status 2 means busy/prearmed: no raster work or clock change was attempted. Wake normally and request again.\n"
  printf "Started only proves dispatch. Completed raster/payload work proves the check ran, not that anything was drawn. Confirm the original scene, A/B and low-power playback remain normal. No install/export support is enabled.\n"
end
