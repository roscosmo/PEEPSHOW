set pagination off
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 NINA/BLE comm mode scaffold ---\n"
printf "owner api/current/prev/req/event = %u / %u / %u / %u / %u\n", $sm->version, $sm->current_state[PS_HW6_SM_BLE], $sm->previous_state[PS_HW6_SM_BLE], $sm->requested_state[PS_HW6_SM_BLE], $sm->last_event[PS_HW6_SM_BLE]
printf "mode count/request/active/status/tick = %u / %u / %u / 0x%x / %u\n", $sm->ble_mode_request_count, $sm->ble_mode_requested, $sm->ble_mode_active, $sm->ble_mode_last_status, $sm->ble_mode_last_tick
printf "placeholder/shutdown reset/dsr highz = %u / %u / %u\n", $sm->ble_mode_placeholder, $sm->ble_shutdown_reset_asserted, $sm->ble_dsr_highz_configured
printf "nrst before/released/after = %u / %u / %u\n", $sm->ble_nrst_before, $sm->ble_nrst_released, $sm->ble_nrst_after
printf "dsr before/target/after = %u / %u / %u\n", $sm->ble_dsr_host_control_before, $sm->ble_dsr_sleep_target_level, $sm->ble_dsr_host_control_after
printf "dsr sleep deasserted override/readback before/after = %u / %u / %u\n", g_ps_hw6_ble_sleep_dsr_deasserted, $sm->ble_dsr_before_sleep_level, $sm->ble_dsr_after_sleep_level
printf "dsr ticks assert/deassert settle start/end/ticks = %u / %u / %u / %u / %u\n", $sm->ble_dsr_assert_tick, $sm->ble_dsr_deassert_tick, $sm->ble_stop_settle_start_tick, $sm->ble_stop_settle_end_tick, $sm->ble_stop_settle_ticks
printf "identity status/rx len = 0x%x / %u\n", $sm->ble_identity_status, $sm->ble_identity_rx_len
printf "identity response: "
x/s &$sm->ble_identity_response[0]
printf "uart deinit/state/error = 0x%x / 0x%x / 0x%x\n", $sm->ble_uart_deinit_status, $sm->ble_uart_state_after, $sm->ble_uart_error_after
printf "command count req/attempt/skip/ok/err = %u / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->ble_command_count, $sm->ble_command_required_mask, $sm->ble_command_attempted_mask, $sm->ble_command_skipped_mask, $sm->ble_command_ok_mask, $sm->ble_command_error_mask
printf "command tx status 0..7 = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->ble_command_tx_status[0], $sm->ble_command_tx_status[1], $sm->ble_command_tx_status[2], $sm->ble_command_tx_status[3], $sm->ble_command_tx_status[4], $sm->ble_command_tx_status[5], $sm->ble_command_tx_status[6], $sm->ble_command_tx_status[7]
printf "command rx len 0..7 = %u / %u / %u / %u / %u / %u / %u / %u\n", $sm->ble_command_rx_len[0], $sm->ble_command_rx_len[1], $sm->ble_command_rx_len[2], $sm->ble_command_rx_len[3], $sm->ble_command_rx_len[4], $sm->ble_command_rx_len[5], $sm->ble_command_rx_len[6], $sm->ble_command_rx_len[7]
printf "command 7 is optional identity query AT+UBTLEDIS?; it is not required for BLE sleep setup\n"
printf "states: OFF=0 RESET=1 BOOT=2 CONFIG=3 IDLE=4 ADVERTISING=5 CONNECTED=6 PAIRING=7 TRANSFER=8 SUSPENDING=9 SUSPENDED=10 ERROR=11\n"
printf "modes: RESET_HELD=0 SLEEP_SYSTEM_OFF=1 SEARCHING=2 PAIRING=3 CONNECTED=4; searching/pairing/connected are placeholders for now\n"
printf "DSR: ASSERTED=0, DEASSERTED=1; AT&D4 should enter sleep on ASSERTED->DEASSERTED\n"
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 UNAVAILABLE=0xfffffffe NOT_RUN=0xffffffff\n"