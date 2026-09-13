set pagination off
printf "--- HW6 one-press TraceX capture ---\n"
p g_ps_object_trace_probe
printf "Trace buffer bytes / running = %u / %u\n", g_ps_hw6_tracex_buffer_bytes, g_ps_hw6_tracex_runtime_enabled
printf "Expected: request/armed/active=0, complete=1, arm/freeze=0, sequence>0, marker_errors=0. Counter before/after must differ.\n"
printf "At unchanged 24 MHz: 24000 timestamp counts = 1 ms. Verify hclk_start/end and clock events; do not apply one scale across clock changes.\n"
printf "Ring wraps are not automatically loss: confirm matching 0x5172 RECEIVE/DONE markers remain in the dump.\n"
printf "0x5170: runtime stage, sequence, candidate token, HCLK. Stage IDs follow __fw0_object_latency_prints.gdb.\n"
printf "0x5171: raster phase, begin=0/end=1, sequence. Phases validate=1 overlap=2 clear=3 draw=4 copy=5 cold=6.\n"
printf "0x5172: arm=1/receive=2/done=3, sequence, HCLK, tick-or-result. DRAW contains another VALIDATE; do not sum nested phases.\n"
printf "0x5173: owner operation, begin=0/end=1, capture sequence, driver status (end: 0=OK; begin: NOT_RUN). PMIC snapshot=1 joystick wake/read/suspend=2/3/4.\n"
printf "Owner markers cover actual calls during this capture, not all background work. Pair by thread, operation and sequence; boundary-truncated calls are not failures.\n"
printf "Thread execution intervals may include unmarked ISRs. Capture does not prove isolated CPU time or physical button-to-panel latency.\n"
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_object_latency_prints.gdb
