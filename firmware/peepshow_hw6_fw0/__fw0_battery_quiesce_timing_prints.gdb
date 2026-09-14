set pagination off
printf "--- HW6 battery preparation timing and first failure ---\n"
if g_ps_hw6_battery_quiesce_timing_probe.api_version != 3 || g_ps_hw6_battery_quiesce_first_failure_probe.api_version != 3
  printf "Probe mismatch: reflash and load the matching ELF for timing API 3.\n"
else
  set $qslot = 0
  while $qslot < 2
    if $qslot == 0
      printf "LATEST battery barrier:\n"
      set $qt = &g_ps_hw6_battery_quiesce_timing_probe
    else
      printf "FIRST FAILED battery barrier since reset:\n"
      set $qt = &g_ps_hw6_battery_quiesce_first_failure_probe
    end
    if $qt->sequence == 0
      printf "No record.\n"
    else
  printf "sequence/active/reason start/end ticks = %u / %u / %u / %u / %u\n", $qt->sequence, $qt->active, $qt->reason, $qt->start_tick, $qt->end_tick
  printf "owner send/done ticks: AUDIO %u/%u INPUT %u/%u DISPLAY %u/%u\n", $qt->send_tick[1], $qt->done_tick[1], $qt->send_tick[2], $qt->done_tick[2], $qt->send_tick[3], $qt->done_tick[3]
  printf "owner send/done ticks: SENSOR %u/%u STORAGE %u/%u COMM %u/%u\n", $qt->send_tick[4], $qt->done_tick[4], $qt->send_tick[5], $qt->done_tick[5], $qt->send_tick[6], $qt->done_tick[6]
  printf "display clock waiting at barrier begin / display dispatch = %u / %u\n", $qt->display_clock_wait_at_begin, $qt->display_clock_wait_at_send
  printf "display clock completions/failures during barrier / last wait status/capabilities/elapsed ticks = %u / %u / 0x%x / 0x%x / %u\n", $qt->display_clock_completions, $qt->display_clock_failures, $qt->display_clock_last_status, $qt->display_clock_last_capabilities, $qt->display_clock_last_elapsed_ticks
      printf "barrier status; display clock grant/release; storage needed/grant/release = 0x%x ; 0x%x / 0x%x ; %u / 0x%x / 0x%x\n", $qt->status, $qt->display_clock_grant_status, $qt->display_clock_release_status, $qt->storage_clock_required, $qt->storage_clock_grant_status, $qt->storage_clock_release_status
      printf "Owners: 1 AUDIO, 2 INPUT, 3 DISPLAY, 4 SENSOR, 5 STORAGE, 6 COMM.\n"
      printf "at barrier begin: power boot done/calibration load started/resolved = %u / %u / %u\n", $qt->power_boot_done, $qt->calibration_load_started, $qt->calibration_boot_resolved
      printf "storage clock waiting begin/send/done; request start tick/capabilities = %u / %u / %u ; %u / 0x%x\n", $qt->storage_clock_wait_at_begin, $qt->storage_clock_wait_at_send, $qt->storage_clock_wait_at_done, $qt->storage_clock_wait_start_tick, $qt->storage_clock_wait_capabilities
      printf "input owner state before/after; driver state before/after/status = %u / %u ; %u / %u / 0x%x\n", $qt->input_state_before, $qt->input_state_after, $qt->input_driver_state_before, $qt->input_driver_state_after, $qt->input_driver_status
      printf "input ready/identity/sleep-write status terminal-committed/I2C-error = 0x%x / 0x%x / 0x%x / %u / 0x%x\n", $qt->input_ready_status, $qt->input_identity_status, $qt->input_sleep_write_status, $qt->input_terminal_sleep_committed, $qt->input_i2c_error
      set $qo = 1
      while $qo <= 6
        printf "owner %u send/ACK/flags/action = 0x%x / 0x%x / 0x%x / 0x%x\n", $qo, $qt->send_status[$qo], $qt->ack_status[$qo], $qt->ack_flags[$qo], $qt->owner_status[$qo]
        set $qo = $qo + 1
      end
    end
    set $qslot = $qslot + 1
  end
  printf "Latest is replaced by each battery barrier. First failure survives retries, recovery and ordinary sleep until reset. Admission failures before a barrier are not captured here.\n"
  printf "Status 0xffffffff means NOT_RUN. Storage needed=0 means its grant/release were skipped. ACK alone is not successful owner work: require action=0 and the expected ACK bit. Later ACKs do not rewrite the frozen first failure.\n"
  printf "Storage waiting=1 marks an outstanding clock transaction, not a failed clock. Input fields copy driver probes at the owner wait boundary; without an ACK they may be incomplete or describe earlier work. No extra sensor reads are performed.\n"
  printf "At the current 100 Hz kernel rate, 100 ticks=1 second. Owner intervals include queue waits and scheduling, not isolated subsystem CPU time. Zero timestamps can also be a real counter value.\n"
  printf "Clock failure counts are queue/ACK failures, not voltage or RCC verdicts. The last clock wait may have begun before the barrier; successes can overwrite its last-status field.\n"
  printf "Do not halt during preparation. Use the battery power print for actual owner ACK/result and shipment gates. Timing records alone do not prove physical shutdown.\n"
end
