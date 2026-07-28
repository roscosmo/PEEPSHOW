printf "\n=== NINA NFC PROBE SUMMARY ===\n"
printf "magic                  = 0x%lx\n", g_ps_nina_nfc_probe.magic
printf "phase                  = 0x%lx\n", g_ps_nina_nfc_probe.phase
printf "complete               = 0x%lx\n", g_ps_nina_nfc_probe.complete
printf "tick_start             = 0x%lx\n", g_ps_nina_nfc_probe.tick_start
printf "tick_read_event        = 0x%lx\n", g_ps_nina_nfc_probe.tick_read_event
printf "boot_rx_len            = 0x%lx\n", g_ps_nina_nfc_probe.boot_rx_len
printf "at_tx_status           = 0x%lx\n", g_ps_nina_nfc_probe.at_tx_status
printf "at_rx_len              = 0x%lx\n", g_ps_nina_nfc_probe.at_rx_len
printf "at_ok                  = 0x%lx\n", g_ps_nina_nfc_probe.at_ok
printf "enable_query_tx_status = 0x%lx\n", g_ps_nina_nfc_probe.enable_query_tx_status
printf "enable_query_rx_len    = 0x%lx\n", g_ps_nina_nfc_probe.enable_query_rx_len
printf "enable_query_ok        = 0x%lx\n", g_ps_nina_nfc_probe.enable_query_ok
printf "uri_set_tx_status      = 0x%lx\n", g_ps_nina_nfc_probe.uri_set_tx_status
printf "uri_set_rx_len         = 0x%lx\n", g_ps_nina_nfc_probe.uri_set_rx_len
printf "uri_set_ok             = 0x%lx\n", g_ps_nina_nfc_probe.uri_set_ok
printf "uri_query_tx_status    = 0x%lx\n", g_ps_nina_nfc_probe.uri_query_tx_status
printf "uri_query_rx_len       = 0x%lx\n", g_ps_nina_nfc_probe.uri_query_rx_len
printf "uri_query_ok           = 0x%lx\n", g_ps_nina_nfc_probe.uri_query_ok
printf "enable_uri_tx_status   = 0x%lx\n", g_ps_nina_nfc_probe.enable_uri_tx_status
printf "enable_uri_rx_len      = 0x%lx\n", g_ps_nina_nfc_probe.enable_uri_rx_len
printf "enable_uri_ok          = 0x%lx\n", g_ps_nina_nfc_probe.enable_uri_ok
printf "enable_verify_tx_status= 0x%lx\n", g_ps_nina_nfc_probe.enable_verify_tx_status
printf "enable_verify_rx_len   = 0x%lx\n", g_ps_nina_nfc_probe.enable_verify_rx_len
printf "enable_verify_ok       = 0x%lx\n", g_ps_nina_nfc_probe.enable_verify_ok
printf "read_event_rx_len      = 0x%lx\n", g_ps_nina_nfc_probe.read_event_rx_len
printf "read_event_detected    = 0x%lx\n", g_ps_nina_nfc_probe.read_event_detected
printf "read_event_wait_loops  = 0x%lx\n", g_ps_nina_nfc_probe.read_event_wait_loops
printf "uart_state_after       = 0x%lx\n", g_ps_nina_nfc_probe.uart_state_after
printf "uart_error_after       = 0x%lx\n", g_ps_nina_nfc_probe.uart_error_after
printf "nrst_state_after       = 0x%lx\n", g_ps_nina_nfc_probe.nrst_state_after
printf "\n--- enable query response ---\n"
x/96cb &g_ps_nina_nfc_probe.enable_query_rx
printf "\n--- URI set response ---\n"
x/96cb &g_ps_nina_nfc_probe.uri_set_rx
printf "\n--- URI query response ---\n"
x/128cb &g_ps_nina_nfc_probe.uri_query_rx
printf "\n--- enable URI response ---\n"
x/96cb &g_ps_nina_nfc_probe.enable_uri_rx
printf "\n--- enable verify response ---\n"
x/96cb &g_ps_nina_nfc_probe.enable_verify_rx
printf "\n--- NFC read event bytes ---\n"
x/128cb &g_ps_nina_nfc_probe.read_event_rx
printf "=== END NINA NFC PROBE ===\n"