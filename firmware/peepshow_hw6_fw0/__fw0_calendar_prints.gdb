set pagination off
echo --- HW6 calendar wake bench ---\n
p g_ps_calendar_probe
p g_ps_time_retention_probe
printf "STOP2 entries / RTC source / arm status = %u / %u / 0x%x\n", g_ps_hw6_rtos_probe.stop2_auto_entry_count, g_ps_hw6_rtos_probe.runtime_rtc_wake_source, g_ps_hw6_rtos_probe.runtime_interaction_rtc_arm_status
echo RTC source NONE/INTERACTION/STATE/BATTERY/CALENDAR = 0/1/2/3/4.\n
echo Calendar status IDLE/ARMED/DUE/REBASED/ARGUMENT/STALE = 0/1/2/3/4/5. time_status=0 means valid.\n
echo Require a new delivered occurrence, matching delivered_registration/generation, and increased RTC expiry count for asleep proof.\n
echo Delivery records actual calendar deadline consumption, not game actions, rendering or measured current.\n
echo Daily remains armed for tomorrow. Counts are cumulative; compare with the pre-test values.\n
echo Cancel with: set var g_ps_calendar_probe.request = 3, then resume. No lease may be cleared manually.\n
