set pagination off

printf "--- HW6 TraceX scaffold ---\n"
printf "enable status/runtime = 0x%x / %u\n", g_ps_hw6_tracex_enable_status, g_ps_hw6_tracex_runtime_enabled
printf "buffer addr/bytes     = 0x%x / %u\n", g_ps_hw6_tracex_buffer_address, g_ps_hw6_tracex_buffer_bytes
printf "registry entries      = %u\n", g_ps_hw6_tracex_registry_entries
printf "tx header/current     = 0x%x / 0x%x\n", (unsigned int)_tx_trace_header_ptr, (unsigned int)_tx_trace_buffer_current_ptr
printf "tx start/end          = 0x%x / 0x%x\n", (unsigned int)_tx_trace_buffer_start_ptr, (unsigned int)_tx_trace_buffer_end_ptr
printf "tx registry total/avail = %u / %u\n", _tx_trace_total_registry_entries, _tx_trace_available_registry_entries
printf "status: TX_SUCCESS=0x0 TX_FEATURE_NOT_ENABLED=0xff\n"
