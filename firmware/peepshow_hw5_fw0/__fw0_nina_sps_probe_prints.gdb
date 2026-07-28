printf "\n=== NINA SPS PROBE SUMMARY ===\n"
printf "magic                  = 0x%lx\n", g_ps_nina_sps_probe.magic
printf "phase                  = 0x%lx\n", g_ps_nina_sps_probe.phase
printf "complete               = 0x%lx\n", g_ps_nina_sps_probe.complete
printf "tick_start             = 0x%lx\n", g_ps_nina_sps_probe.tick_start
printf "tick_peer              = 0x%lx\n", g_ps_nina_sps_probe.tick_peer
printf "tick_data_done         = 0x%lx\n", g_ps_nina_sps_probe.tick_data_done
printf "peer_wait_loops        = 0x%lx\n", g_ps_nina_sps_probe.peer_wait_loops
printf "data_wait_loops        = 0x%lx\n", g_ps_nina_sps_probe.data_wait_loops
printf "boot_rx_len            = 0x%lx\n", g_ps_nina_sps_probe.boot_rx_len
printf "at_tx_status           = 0x%lx\n", g_ps_nina_sps_probe.at_tx_status
printf "at_rx_len              = 0x%lx\n", g_ps_nina_sps_probe.at_rx_len
printf "at_ok                  = 0x%lx\n", g_ps_nina_sps_probe.at_ok
printf "phone_rx_len           = 0x%lx\n", g_ps_nina_sps_probe.phone_rx_len
printf "phone_detected         = 0x%lx\n", g_ps_nina_sps_probe.phone_detected
printf "phone_sps_detected     = 0x%lx\n", g_ps_nina_sps_probe.phone_sps_detected
printf "data_mode_tx_status    = 0x%lx\n", g_ps_nina_sps_probe.data_mode_tx_status
printf "data_mode_rx_len       = 0x%lx\n", g_ps_nina_sps_probe.data_mode_rx_len
printf "data_mode_ok           = 0x%lx\n", g_ps_nina_sps_probe.data_mode_ok
printf "hello_tx_status        = 0x%lx\n", g_ps_nina_sps_probe.hello_tx_status
printf "phone_data_rx_len      = 0x%lx\n", g_ps_nina_sps_probe.phone_data_rx_len
printf "phone_echo_tx_count    = 0x%lx\n", g_ps_nina_sps_probe.phone_echo_tx_count
printf "phone_alive_tx_count   = 0x%lx\n", g_ps_nina_sps_probe.phone_alive_tx_count
printf "uart_flow_diag_mode    = 0x%lx  (0 normal RTS/CTS, 1 no-HW-flow PB12 high, 2 no-HW-flow PB12 low)\n", g_ps_nina_sps_probe.uart_flow_diag_mode
printf "uart_reinit_status     = 0x%lx\n", g_ps_nina_sps_probe.uart_reinit_status
printf "uart_state_after       = 0x%lx\n", g_ps_nina_sps_probe.uart_state_after
printf "uart_error_after       = 0x%lx\n", g_ps_nina_sps_probe.uart_error_after
printf "nrst_state_after       = 0x%lx\n", g_ps_nina_sps_probe.nrst_state_after
printf "dtr_state_after        = 0x%lx\n", g_ps_nina_sps_probe.dtr_state_after
printf "dsr_state_after        = 0x%lx\n", g_ps_nina_sps_probe.dsr_state_after
printf "\n--- flow snapshots: 0=after flow diag setup, 1=after AT setup, 2=after peer, 3=after data wait ---\n"
printf "GPIOA MODER: "
p/x g_ps_nina_sps_probe.flow_gpioa_moder
printf "GPIOB MODER: "
p/x g_ps_nina_sps_probe.flow_gpiob_moder
printf "GPIOB ODR:   "
p/x g_ps_nina_sps_probe.flow_gpiob_odr
printf "GPIOB IDR:   "
p/x g_ps_nina_sps_probe.flow_gpiob_idr
printf "GPIOC MODER: "
p/x g_ps_nina_sps_probe.flow_gpioc_moder
printf "GPIOC IDR:   "
p/x g_ps_nina_sps_probe.flow_gpioc_idr
printf "LPUART CR1:  "
p/x g_ps_nina_sps_probe.flow_uart_cr1
printf "LPUART CR2:  "
p/x g_ps_nina_sps_probe.flow_uart_cr2
printf "LPUART CR3:  "
p/x g_ps_nina_sps_probe.flow_uart_cr3
printf "LPUART ISR:  "
p/x g_ps_nina_sps_probe.flow_uart_isr
printf "\n--- plain AT response ---\n"
x/64cb &g_ps_nina_sps_probe.at_rx
printf "\n--- setup command lengths / OK flags ---\n"
p/x g_ps_nina_sps_probe.setup_rx_len
p/x g_ps_nina_sps_probe.setup_ok
printf "\n--- setup[0] AT+UMRS? response ---\n"
x/96cb &g_ps_nina_sps_probe.setup_rx[0]
printf "\n--- phone connect URCs ---\n"
x/128cb &g_ps_nina_sps_probe.phone_rx
printf "\n--- data mode response ---\n"
x/64cb &g_ps_nina_sps_probe.data_mode_rx
printf "\n--- phone payload bytes ---\n"
x/256cb &g_ps_nina_sps_probe.phone_data_rx
printf "=== END NINA SPS PROBE ===\n"
