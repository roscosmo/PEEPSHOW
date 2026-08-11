set pagination off
printf "--- HW6 TraceX app marker scaffold ---\n"
printf "trace api/count ok/skip/err = %u / %u / %u / %u / %u\n", g_ps_hw6_trace_probe.api_version, g_ps_hw6_trace_probe.insert_count, g_ps_hw6_trace_probe.success_count, g_ps_hw6_trace_probe.skipped_count, g_ps_hw6_trace_probe.error_count
printf "last event/info/status      = 0x%x / %u / %u / %u / %u / 0x%x\n", g_ps_hw6_trace_probe.last_event_id, g_ps_hw6_trace_probe.last_info1, g_ps_hw6_trace_probe.last_info2, g_ps_hw6_trace_probe.last_info3, g_ps_hw6_trace_probe.last_info4, g_ps_hw6_trace_probe.last_status
printf "event ids owner/reject/ui/button/start/pmic/sleep = 0x5101 / 0x5102 / 0x5110 / 0x5120 / 0x5130 / 0x5140 / 0x5150\n"
printf "sleep stages prep/enter/wake/recover = 1 / 2 / 3 / 4\n"
printf "status: TX_SUCCESS=0x0 DISABLED=0xfffffffe NOT_RUN=0xffffffff\n"