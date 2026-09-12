set pagination off
define ps_object_latency_row
  if ($ps_latency->valid[$arg0] != 0) && ($ps_latency->valid[$arg1] != 0)
    printf "%llu ms; HCLK start/end %u/%u Hz\n", ((unsigned long long)(unsigned int)($ps_latency->tick[$arg1] - $ps_latency->tick[$arg0]) * 1000) / $ps_latency->tick_hz, $ps_latency->hclk_hz[$arg0], $ps_latency->hclk_hz[$arg1]
  else
    printf "NOT RECORDED\n"
  end
end
printf "--- HW6 one-press object latency ---\n"
set $ps_latency = &g_ps_object_latency_probe
if ($ps_latency->api_version != 1) || ($ps_latency->complete == 0) || ($ps_latency->active != 0) || ($ps_latency->tick_hz == 0)
  printf "No completed capture: request/active/complete = %u/%u/%u. Use __fw0_object_latency_enable.gdb, resume, change selection once, then halt.\n", $ps_latency->request, $ps_latency->active, $ps_latency->complete
else
  printf "sequence/button/scene before/after LPBAM/WFI baseline = %u/%u/%u/%u %u/%u\n", $ps_latency->sequence, $ps_latency->button, $ps_latency->scene_before, $ps_latency->scene_after, $ps_latency->lp_enabled, $ps_latency->wfi_returns
  printf "event result/status/panel status candidate token/count render token = %u/0x%x/0x%x %u/%u %u\n", $ps_latency->event_result, $ps_latency->status, $ps_latency->panel_status, $ps_latency->candidate_token, $ps_latency->candidate_count, $ps_latency->render_token
  printf "tick frequency = %u Hz; zero ms means below tick resolution, not free work.\n", $ps_latency->tick_hz
  printf "Metadata cache hits/misses since boot = %u/%u (cumulative, not this capture alone).\n", ps_candidate_cache_hits, ps_candidate_cache_misses
  printf "Runtime advance: "
  ps_object_latency_row 0 1
  printf "Private copy: "
  ps_object_latency_row 2 3
  printf "Runtime clock/setup: "
  ps_object_latency_row 3 4
  printf "Package decode/validation: "
  ps_object_latency_row 4 5
  printf "Graph/schedule: "
  ps_object_latency_row 5 6
  printf "Runtime clock release: "
  ps_object_latency_row 6 7
  printf "Candidate queue: "
  ps_object_latency_row 8 9
  printf "Display clock request: "
  ps_object_latency_row 9 10
  printf "Candidate raster/packing: "
  ps_object_latency_row 10 11
  printf "Display clock release: "
  ps_object_latency_row 11 12
  printf "Admission total (overlaps above): "
  ps_object_latency_row 2 13
  printf "Runtime receipt to event commit: "
  ps_object_latency_row 0 14
  printf "Presentation projection/schedule: "
  ps_object_latency_row 15 16
  printf "Presentation queue/clock: "
  ps_object_latency_row 16 17
  printf "Panel render/transfer: "
  ps_object_latency_row 17 18
  printf "Playback publication: "
  ps_object_latency_row 18 19
  printf "Runtime receipt to panel completion: "
  ps_object_latency_row 0 18
  printf "Runtime transaction total: "
  ps_object_latency_row 0 20
  printf "One captured press only. Missing stages can mean ignored input or failure; require a real selection change and status/panel=0.\n"
  printf "Pre-runtime wake, debounce and input queue latency are excluded. WFI baseline is cumulative, not proof this press woke STOP2. HCLK is a stage-boundary sample, not a trace.\n"
  printf "Do not sum overlapping totals. Compare awake and autonomous runs without halting during the transaction.\n"
end
