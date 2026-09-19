set pagination off
echo --- HW6 calendar-to-V2 handler result ---\n
p g_ps_calendar_runtime_probe
p g_ps_calendar_probe
printf "STOP2 entries / scene activation / state activation = %u / %u / %u\n", g_ps_hw6_rtos_probe.stop2_auto_entry_count, s_ps_scene_runtime_scene_activation, s_ps_scene_runtime_state_activation
echo Setup status=0 means registered. Applied counts completed handler/presentation work, not measured current or proof of visible pixels.\n
echo Delivery EMPTY/PENDING/CLAIMED/APPLIED/IGNORED/FAILED/INVALIDATED=0/1/2/3/4/5/6.\n
echo Require applied increment once, failed=0, pending_ack=0, delivery_state=3, a calendar RTC expiry, and the observed square fill.\n
echo Scene replacement cancels this bench registration. HOLD START defers calendar delivery until resume; it does not move the deadline.\n
echo Cancel with set var g_ps_calendar_runtime_probe.request = 4, then resume. Restore preferred time through shell TIME.\n
echo Bench-only binding to an action-start handler; no calendar export capability is advertised yet.\n
