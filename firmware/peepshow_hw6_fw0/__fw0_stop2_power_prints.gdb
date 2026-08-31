set pagination off
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 STOP2 START-wake scaffold ---\n"
printf "api/status          = %u / 0x%x\n", $sm->version, $sm->stop2_last_status
printf "stop2 count/start/wake/end = %u / %u / %u / %u\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick
printf "stop2 quiesce/enter/clock prepare/restore/recover = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->stop2_quiesce_status, $sm->stop2_enter_status, $sm->stop2_clock_prepare_status, $sm->stop2_clock_restore_status, $sm->stop2_recover_status
printf "clock STOP2 prepare count/status/physical/fail = %u / 0x%x / %u / 0x%x\n", g_ps_hw6_clock_policy_probe.stop2_prepare_count, g_ps_hw6_clock_policy_probe.stop2_prepare_status, g_ps_hw6_clock_policy_probe.stop2_physical_ready, g_ps_hw6_clock_policy_probe.stop2_physical_failure_mask
printf "stop2 expected wake pin = 0x%x\n", $sm->stop2_expected_wake_pin
printf "stop2 IDR before/after = 0x%x / 0x%x\n", $sm->stop2_wake_start_idr, $sm->stop2_wake_end_idr
printf "power state/last event = %u / %u\n", $sm->current_state[PS_HW6_SM_POWER], $sm->last_event[PS_HW6_SM_POWER]
printf "active prep count/status/ready/cycle = %u / 0x%x / %u / %u\n", $sm->stop2_active_prep_request_count, $sm->stop2_active_prep_last_status, $sm->stop2_active_prep_ready, $sm->stop2_active_prep_cycle_index
printf "active expected prep owner/state = 0x5f / 0x27f\n"
printf "active expected post noop/action = 0x2a / 0x54\n"
printf "active prep ticks start/end = %u / %u\n", $sm->stop2_active_prep_start_tick, $sm->stop2_active_prep_end_tick
printf "active enter count/gate = %u / 0x%x\n", $sm->stop2_active_enter_request_count, $sm->stop2_active_enter_gate_status
printf "cycle0 resume ok/fail = 0x%x / 0x%x\n", $sm->cycle_resume_success_mask[0], $sm->cycle_resume_failure_mask[0]
printf "cycle0 quiesce ok/fail = 0x%x / 0x%x\n", $sm->cycle_quiesce_success_mask[0], $sm->cycle_quiesce_failure_mask[0]
printf "cycle0 state active/inactive = 0x%x / 0x%x\n", $sm->cycle_active_state_match_mask[0], $sm->cycle_inactive_state_match_mask[0]
printf "cycle0 resume stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->cycle_action_status[0][0][1], $sm->cycle_action_status[0][0][2], $sm->cycle_action_status[0][0][3], $sm->cycle_action_status[0][0][4], $sm->cycle_action_status[0][0][5], $sm->cycle_action_status[0][0][6]
printf "cycle0 quiesce stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->cycle_action_status[0][1][1], $sm->cycle_action_status[0][1][2], $sm->cycle_action_status[0][1][3], $sm->cycle_action_status[0][1][4], $sm->cycle_action_status[0][1][5], $sm->cycle_action_status[0][1][6]
printf "barrier reason/count/status = %u / %u / 0x%x\n", $sm->power_quiesce_reason, $sm->power_quiesce_request_count, $sm->power_quiesce_last_status
printf "barrier ticks start/end = %u / %u\n", $sm->power_quiesce_start_tick, $sm->power_quiesce_end_tick
printf "barrier masks req/send/ack/ok/fail = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_required_mask, $sm->power_quiesce_send_ok_mask, $sm->power_quiesce_ack_ok_mask, $sm->power_quiesce_success_mask, $sm->power_quiesce_failure_mask
printf "barrier owner stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_owner_status[1], $sm->power_quiesce_owner_status[2], $sm->power_quiesce_owner_status[3], $sm->power_quiesce_owner_status[4], $sm->power_quiesce_owner_status[5], $sm->power_quiesce_owner_status[6]
printf "barrier send stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_send_status[1], $sm->power_quiesce_send_status[2], $sm->power_quiesce_send_status[3], $sm->power_quiesce_send_status[4], $sm->power_quiesce_send_status[5], $sm->power_quiesce_send_status[6]
printf "barrier ack stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->power_quiesce_ack_status[1], $sm->power_quiesce_ack_status[2], $sm->power_quiesce_ack_status[3], $sm->power_quiesce_ack_status[4], $sm->power_quiesce_ack_status[5], $sm->power_quiesce_ack_status[6]
printf "post-resume count/status = %u / 0x%x\n", $sm->post_stop_resume_request_count, $sm->post_stop_resume_last_status
printf "post-resume ticks start/end = %u / %u\n", $sm->post_stop_resume_start_tick, $sm->post_stop_resume_end_tick
printf "post-resume masks req/send/ack/ok/fail = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->post_stop_resume_required_mask, $sm->post_stop_resume_send_ok_mask, $sm->post_stop_resume_ack_ok_mask, $sm->post_stop_resume_success_mask, $sm->post_stop_resume_failure_mask
printf "post-resume noop/action masks = 0x%x / 0x%x\n", $sm->post_stop_resume_noop_mask, $sm->post_stop_resume_action_mask
printf "post-resume owner stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->post_stop_resume_owner_status[1], $sm->post_stop_resume_owner_status[2], $sm->post_stop_resume_owner_status[3], $sm->post_stop_resume_owner_status[4], $sm->post_stop_resume_owner_status[5], $sm->post_stop_resume_owner_status[6]
printf "post-resume send stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->post_stop_resume_send_status[1], $sm->post_stop_resume_send_status[2], $sm->post_stop_resume_send_status[3], $sm->post_stop_resume_send_status[4], $sm->post_stop_resume_send_status[5], $sm->post_stop_resume_send_status[6]
printf "post-resume ack stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->post_stop_resume_ack_status[1], $sm->post_stop_resume_ack_status[2], $sm->post_stop_resume_ack_status[3], $sm->post_stop_resume_ack_status[4], $sm->post_stop_resume_ack_status[5], $sm->post_stop_resume_ack_status[6]
printf "storage clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", g_ps_hw6_rtos_probe.storage_clock_request_count, g_ps_hw6_rtos_probe.storage_clock_release_count, g_ps_hw6_rtos_probe.storage_clock_last_reason, g_ps_hw6_rtos_probe.storage_clock_last_capabilities, g_ps_hw6_rtos_probe.storage_clock_last_status
printf "manual request flags base/full/prep/enter = %u / %u / %u / %u\n", g_ps_hw6_power_stop2_request, g_ps_hw6_power_stop2_active_resume_request, g_ps_hw6_power_stop2_active_prep_request, g_ps_hw6_power_stop2_active_enter_request
printf "states: PWR_ACTIVE_LP=2 PWR_ACTIVE_RT=3 PWR_SLEEP_PREP=4 PWR_STOP_RESIDENT=5 PWR_WAKE_RESUME=6 PWR_FORCED_SLEEP=7 PWR_SHIP_PREP=8\n"
printf "events: LP_REQUEST=5 SLEEP_REQUEST=6 STOP_ENTERED=7 WAKE=8\n"
printf "staged active test: source active_prep helper, verify ready=1, then source active_enter helper and wake with START\n"
