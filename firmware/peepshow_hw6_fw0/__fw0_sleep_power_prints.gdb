set pagination off
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 sleep-prep scaffold ---\n"
printf "api/status          = %u / 0x%x\n", $sm->version, $sm->sleep_prep_last_status
printf "sleep count/start/end = %u / %u / %u\n", $sm->sleep_prep_request_count, $sm->sleep_prep_start_tick, $sm->sleep_prep_end_tick
printf "sleep quiesce/recover/stop-skip = 0x%x / 0x%x / %u\n", $sm->sleep_prep_quiesce_status, $sm->sleep_prep_recover_status, $sm->sleep_prep_stop_entry_skipped
printf "power state/last event = %u / %u\n", $sm->current_state[0], $sm->last_event[0]
printf "barrier reason/count/status = %u / %u / 0x%x\n", $sm->power_quiesce_reason, $sm->power_quiesce_request_count, $sm->power_quiesce_last_status
printf "barrier ticks start/end = %u / %u\n", $sm->power_quiesce_start_tick, $sm->power_quiesce_end_tick
printf "barrier masks req/send/ack/ok/fail = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_required_mask, $sm->power_quiesce_send_ok_mask, $sm->power_quiesce_ack_ok_mask, $sm->power_quiesce_success_mask, $sm->power_quiesce_failure_mask
printf "barrier owner stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_owner_status[1], $sm->power_quiesce_owner_status[2], $sm->power_quiesce_owner_status[3], $sm->power_quiesce_owner_status[4], $sm->power_quiesce_owner_status[5], $sm->power_quiesce_owner_status[6]
printf "barrier send stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_send_status[1], $sm->power_quiesce_send_status[2], $sm->power_quiesce_send_status[3], $sm->power_quiesce_send_status[4], $sm->power_quiesce_send_status[5], $sm->power_quiesce_send_status[6]
printf "barrier ack stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_ack_status[1], $sm->power_quiesce_ack_status[2], $sm->power_quiesce_ack_status[3], $sm->power_quiesce_ack_status[4], $sm->power_quiesce_ack_status[5], $sm->power_quiesce_ack_status[6]
printf "manual request flag = %u\n", g_ps_hw6_power_sleep_prep_request
printf "states: PWR_ACTIVE_LP=2 PWR_ACTIVE_RT=3 PWR_SLEEP_PREP=4 PWR_STOP_RESIDENT=5 PWR_WAKE_RESUME=6 PWR_FORCED_SLEEP=7 PWR_SHIP_PREP=8\n"
printf "quiesce reasons: START=1 BATT_CRIT=2 BOOT_LOW=3 SLEEP_PREP=4\n"