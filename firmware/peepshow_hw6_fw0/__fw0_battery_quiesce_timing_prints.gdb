set pagination off
printf "--- HW6 battery preparation timing ---\n"
set $qt = &g_ps_hw6_battery_quiesce_timing_probe
if $qt->api_version != 1
  printf "Probe mismatch: use the matching flashed ELF.\n"
else
  printf "sequence/active/reason start/end ticks = %u / %u / %u / %u / %u\n", $qt->sequence, $qt->active, $qt->reason, $qt->start_tick, $qt->end_tick
  printf "owner send/done ticks: AUDIO %u/%u INPUT %u/%u DISPLAY %u/%u\n", $qt->send_tick[1], $qt->done_tick[1], $qt->send_tick[2], $qt->done_tick[2], $qt->send_tick[3], $qt->done_tick[3]
  printf "owner send/done ticks: SENSOR %u/%u STORAGE %u/%u COMM %u/%u\n", $qt->send_tick[4], $qt->done_tick[4], $qt->send_tick[5], $qt->done_tick[5], $qt->send_tick[6], $qt->done_tick[6]
  printf "display clock waiting at barrier begin / display dispatch = %u / %u\n", $qt->display_clock_wait_at_begin, $qt->display_clock_wait_at_send
  printf "display clock completions/failures during barrier / last wait status/capabilities/elapsed ticks = %u / %u / 0x%x / 0x%x / %u\n", $qt->display_clock_completions, $qt->display_clock_failures, $qt->display_clock_last_status, $qt->display_clock_last_capabilities, $qt->display_clock_last_elapsed_ticks
  printf "Only battery-critical/boot-low barriers are retained; each retry replaces the previous record. sequence=0 means none recorded. active=1 means incomplete.\n"
  printf "At the current 100 Hz kernel rate, 100 ticks=1 second. Owner intervals include queue waits and scheduling, not isolated subsystem CPU time. Zero timestamps can also be a real counter value.\n"
  printf "Clock failure counts are queue/ACK failures, not voltage or RCC verdicts. The last clock wait may have begun before the barrier; successes can overwrite its last-status field.\n"
  printf "Do not halt during preparation. Use the battery power print for actual owner ACK/result and shipment gates. Timing records alone do not prove physical shutdown.\n"
end
