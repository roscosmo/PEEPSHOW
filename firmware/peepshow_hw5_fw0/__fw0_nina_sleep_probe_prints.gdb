printf "\n--- RTOS sanity ---\n"
printf "threadx_magic          = 0x%x\n", g_ps_phase5_threadx_probe.magic
printf "threadx_complete       = 0x%x\n", g_ps_phase5_threadx_probe.complete
printf "threadx_start_mask     = 0x%x\n", g_ps_phase5_threadx_probe.start_mask
printf "comm_create_status     = 0x%x\n", g_ps_phase5_threadx_probe.create_status[4]
printf "comm_heartbeat         = 0x%x\n", g_ps_phase5_threadx_probe.heartbeat[4]
printf "comm_last_time         = 0x%x\n", g_ps_phase5_threadx_probe.last_time[4]
printf "\n=== NINA SLEEP PROBE SUMMARY ===\n"
printf "magic                  = 0x%x\n", g_ps_nina_sleep_probe.magic
printf "phase                  = 0x%x\n", g_ps_nina_sleep_probe.phase
printf "complete               = 0x%x\n", g_ps_nina_sleep_probe.complete
printf "tick_start             = 0x%x\n", g_ps_nina_sleep_probe.tick_start
printf "tick_ustop_enter       = 0x%x\n", g_ps_nina_sleep_probe.tick_ustop_enter
printf "tick_ustop_done        = 0x%x\n", g_ps_nina_sleep_probe.tick_ustop_done
printf "tick_dtr_stop_enter    = 0x%x\n", g_ps_nina_sleep_probe.tick_dtr_stop_enter
printf "tick_dtr_stop_done     = 0x%x\n", g_ps_nina_sleep_probe.tick_dtr_stop_done
printf "dtr_stop_hold_active   = 0x%x\n", g_ps_nina_sleep_probe.dtr_stop_hold_active
printf "dtr_stop_hold_loops    = 0x%x\n", g_ps_nina_sleep_probe.dtr_stop_hold_loops
printf "reset_hold_active      = 0x%x\n", g_ps_nina_sleep_probe.reset_attrib_hold_active
printf "reset_hold_loops       = 0x%x\n", g_ps_nina_sleep_probe.reset_attrib_hold_loops
printf "reset_tick_enter       = 0x%x\n", g_ps_nina_sleep_probe.reset_attrib_tick_enter
printf "reset_tick_done        = 0x%x\n", g_ps_nina_sleep_probe.reset_attrib_tick_done
printf "reset_nrst_state       = 0x%x\n", g_ps_nina_sleep_probe.reset_attrib_nrst_state

printf "\n--- command status ---\n"
printf "boot_rx_len            = 0x%x\n", g_ps_nina_sleep_probe.boot_rx_len
printf "at_tx_status           = 0x%x\n", g_ps_nina_sleep_probe.at_tx_status
printf "at_rx_len              = 0x%x\n", g_ps_nina_sleep_probe.at_rx_len
printf "at_ok                  = 0x%x\n", g_ps_nina_sleep_probe.at_ok
printf "upwrreg_query_tx       = 0x%x\n", g_ps_nina_sleep_probe.upwrreg_query_tx_status
printf "upwrreg_query_rx_len   = 0x%x\n", g_ps_nina_sleep_probe.upwrreg_query_rx_len
printf "upwrreg_query_ok       = 0x%x\n", g_ps_nina_sleep_probe.upwrreg_query_ok
printf "pwrmng_min_tx          = 0x%x\n", g_ps_nina_sleep_probe.pwrmng_min_tx_status
printf "pwrmng_min_rx_len      = 0x%x\n", g_ps_nina_sleep_probe.pwrmng_min_rx_len
printf "pwrmng_min_ok          = 0x%x\n", g_ps_nina_sleep_probe.pwrmng_min_ok
printf "pwrmng_max_tx          = 0x%x\n", g_ps_nina_sleep_probe.pwrmng_max_tx_status
printf "pwrmng_max_rx_len      = 0x%x\n", g_ps_nina_sleep_probe.pwrmng_max_rx_len
printf "pwrmng_max_ok          = 0x%x\n", g_ps_nina_sleep_probe.pwrmng_max_ok
printf "bt_disc_off_tx         = 0x%x\n", g_ps_nina_sleep_probe.bt_discoverable_off_tx_status
printf "bt_disc_off_rx_len     = 0x%x\n", g_ps_nina_sleep_probe.bt_discoverable_off_rx_len
printf "bt_disc_off_ok         = 0x%x\n", g_ps_nina_sleep_probe.bt_discoverable_off_ok
printf "bt_conn_off_tx         = 0x%x\n", g_ps_nina_sleep_probe.bt_connectable_off_tx_status
printf "bt_conn_off_rx_len     = 0x%x\n", g_ps_nina_sleep_probe.bt_connectable_off_rx_len
printf "bt_conn_off_ok         = 0x%x\n", g_ps_nina_sleep_probe.bt_connectable_off_ok
printf "bt_pair_off_tx         = 0x%x\n", g_ps_nina_sleep_probe.bt_pairing_off_tx_status
printf "bt_pair_off_rx_len     = 0x%x\n", g_ps_nina_sleep_probe.bt_pairing_off_rx_len
printf "bt_pair_off_ok         = 0x%x\n", g_ps_nina_sleep_probe.bt_pairing_off_ok
printf "ustop_tx_status        = 0x%x\n", g_ps_nina_sleep_probe.ustop_tx_status
printf "ustop_rx_len           = 0x%x\n", g_ps_nina_sleep_probe.ustop_rx_len
printf "ustop_ok               = 0x%x\n", g_ps_nina_sleep_probe.ustop_ok
printf "ustop_startup_seen     = 0x%x\n", g_ps_nina_sleep_probe.ustop_startup_seen
printf "post_ustop_at_tx       = 0x%x\n", g_ps_nina_sleep_probe.post_ustop_at_tx_status
printf "post_ustop_at_rx_len   = 0x%x\n", g_ps_nina_sleep_probe.post_ustop_at_rx_len
printf "post_ustop_at_ok       = 0x%x\n", g_ps_nina_sleep_probe.post_ustop_at_ok
printf "dtr_asserted_before    = 0x%x\n", g_ps_nina_sleep_probe.dtr_asserted_before_set_state
printf "dtr3_set_tx_status     = 0x%x\n", g_ps_nina_sleep_probe.dtr_uartoff_set_tx_status
printf "dtr3_set_rx_len        = 0x%x\n", g_ps_nina_sleep_probe.dtr_uartoff_set_rx_len
printf "dtr3_set_ok            = 0x%x\n", g_ps_nina_sleep_probe.dtr_uartoff_set_ok
printf "dtr3_deasserted_state  = 0x%x\n", g_ps_nina_sleep_probe.dtr3_deasserted_state
printf "dtr3_at_high_tx        = 0x%x\n", g_ps_nina_sleep_probe.dtr3_at_while_deasserted_tx_status
printf "dtr3_at_high_rx_len    = 0x%x\n", g_ps_nina_sleep_probe.dtr3_at_while_deasserted_rx_len
printf "dtr3_at_high_ok        = 0x%x\n", g_ps_nina_sleep_probe.dtr3_at_while_deasserted_ok
printf "dtr3_wake_asserted     = 0x%x\n", g_ps_nina_sleep_probe.dtr3_wake_asserted_state
printf "dtr3_wake_at_tx        = 0x%x\n", g_ps_nina_sleep_probe.dtr3_post_wake_at_tx_status
printf "dtr3_wake_at_rx_len    = 0x%x\n", g_ps_nina_sleep_probe.dtr3_post_wake_at_rx_len
printf "dtr3_wake_at_ok        = 0x%x\n", g_ps_nina_sleep_probe.dtr3_post_wake_at_ok
printf "dtr_set_tx_status      = 0x%x\n", g_ps_nina_sleep_probe.dtr_set_tx_status
printf "dtr_set_rx_len         = 0x%x\n", g_ps_nina_sleep_probe.dtr_set_rx_len
printf "dtr_set_ok             = 0x%x\n", g_ps_nina_sleep_probe.dtr_set_ok
printf "dtr_deasserted_state   = 0x%x\n", g_ps_nina_sleep_probe.dtr_deasserted_state
printf "dtr_wake_asserted      = 0x%x\n", g_ps_nina_sleep_probe.dtr_wake_asserted_state
printf "dtr_wake_rx_len        = 0x%x\n", g_ps_nina_sleep_probe.dtr_wake_rx_len
printf "dtr_wake_startup_seen  = 0x%x\n", g_ps_nina_sleep_probe.dtr_wake_startup_seen
printf "post_dtr_at_tx         = 0x%x\n", g_ps_nina_sleep_probe.post_dtr_at_tx_status
printf "post_dtr_at_rx_len     = 0x%x\n", g_ps_nina_sleep_probe.post_dtr_at_rx_len
printf "post_dtr_at_ok         = 0x%x\n", g_ps_nina_sleep_probe.post_dtr_at_ok

printf "\n--- final lines ---\n"
printf "gpioa_moder_after      = 0x%x\n", g_ps_nina_sleep_probe.gpioa_moder_after
printf "gpiob_moder_after      = 0x%x\n", g_ps_nina_sleep_probe.gpiob_moder_after
printf "gpioc_moder_after      = 0x%x\n", g_ps_nina_sleep_probe.gpioc_moder_after
printf "gpioc_odr_after        = 0x%x\n", g_ps_nina_sleep_probe.gpioc_odr_after
printf "gpioc_idr_after        = 0x%x\n", g_ps_nina_sleep_probe.gpioc_idr_after
printf "uart_state_after       = 0x%x\n", g_ps_nina_sleep_probe.uart_state_after
printf "uart_error_after       = 0x%x\n", g_ps_nina_sleep_probe.uart_error_after
printf "nrst_state_after       = 0x%x\n", g_ps_nina_sleep_probe.nrst_state_after
printf "dtr_state_after        = 0x%x\n", g_ps_nina_sleep_probe.dtr_state_after
printf "dsr_state_after        = 0x%x\n", g_ps_nina_sleep_probe.dsr_state_after

printf "\n--- boot rx ---\n"
x/64cb &g_ps_nina_sleep_probe.boot_rx
printf "\n--- AT rx ---\n"
x/64cb &g_ps_nina_sleep_probe.at_rx
printf "\n--- UPWRREG? rx ---\n"
x/96cb &g_ps_nina_sleep_probe.upwrreg_query_rx
printf "\n--- UPWRMNG min rx ---\n"
x/64cb &g_ps_nina_sleep_probe.pwrmng_min_rx
printf "\n--- UPWRMNG max rx ---\n"
x/64cb &g_ps_nina_sleep_probe.pwrmng_max_rx
printf "\n--- UBTDM=1 rx ---\n"
x/64cb &g_ps_nina_sleep_probe.bt_discoverable_off_rx
printf "\n--- UBTCM=1 rx ---\n"
x/64cb &g_ps_nina_sleep_probe.bt_connectable_off_rx
printf "\n--- UBTPM=1 rx ---\n"
x/64cb &g_ps_nina_sleep_probe.bt_pairing_off_rx
printf "\n--- USTOP rx ---\n"
x/128cb &g_ps_nina_sleep_probe.ustop_rx
printf "\n--- post USTOP AT rx ---\n"
x/64cb &g_ps_nina_sleep_probe.post_ustop_at_rx
printf "\n--- AT&D3 rx ---\n"
x/64cb &g_ps_nina_sleep_probe.dtr_uartoff_set_rx
printf "\n--- AT while DSR deasserted rx ---\n"
x/64cb &g_ps_nina_sleep_probe.dtr3_at_while_deasserted_rx
printf "\n--- AT after DSR asserted rx ---\n"
x/64cb &g_ps_nina_sleep_probe.dtr3_post_wake_at_rx
printf "\n--- AT&D4 rx ---\n"
x/64cb &g_ps_nina_sleep_probe.dtr_set_rx
printf "\n--- DSR wake rx ---\n"
x/128cb &g_ps_nina_sleep_probe.dtr_wake_rx
printf "\n--- post DSR AT rx ---\n"
x/64cb &g_ps_nina_sleep_probe.post_dtr_at_rx
printf "=== END NINA SLEEP PROBE ===\n"