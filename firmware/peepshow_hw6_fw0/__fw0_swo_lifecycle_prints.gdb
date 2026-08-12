printf "--- HW6 SWO lifecycle marker status ---\n"
printf "trace api/count ok/skip/err = %u / %u / %u / %u / %u\n", g_ps_hw6_trace_probe.api_version, g_ps_hw6_trace_probe.insert_count, g_ps_hw6_trace_probe.success_count, g_ps_hw6_trace_probe.skipped_count, g_ps_hw6_trace_probe.error_count
printf "swo emit/drop/disabled = %u / %u / %u\n", g_ps_hw6_trace_probe.swo_emit_count, g_ps_hw6_trace_probe.swo_drop_count, g_ps_hw6_trace_probe.swo_disabled_count
printf "swo last token/status = 0x%x / 0x%x\n", g_ps_hw6_trace_probe.swo_last_token, g_ps_hw6_trace_probe.swo_last_status
printf "swo last text         = %c%c%c\n", g_ps_hw6_trace_probe.swo_last_token & 0xff, (g_ps_hw6_trace_probe.swo_last_token >> 8) & 0xff, (g_ps_hw6_trace_probe.swo_last_token >> 16) & 0xff
printf "tokens: BTD boot done, RDY storage ready, REQ flash init requested, WAK flash wake OK, LAY layout OK\n"
printf "tokens: ERS erase start, FMT FAT format start, DON flash init done, ERR error\n"
printf "tokens: EXP MSC export start, MOK MSC media open OK, REC MSC recovery/init required, REL reclaim start, RDN reclaim done\n"
printf "status: 0x0 emitted, 0xfffffffe knob disabled, 0xfffffffd SWO not ready, 0xffffffff not run\n"