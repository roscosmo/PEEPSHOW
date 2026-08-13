set pagination off
set $cp = &g_ps_hw6_clock_policy_probe
set $rt = &g_ps_hw6_rtos_probe
printf "--- HW6 audio SAI clock requester scaffold ---\n"
printf "rtos version/runtime    = %u / %u\n", $rt->version, $rt->runtime_complete
printf "audio clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->audio_clock_request_count, $rt->audio_clock_release_count, $rt->audio_clock_last_reason, $rt->audio_clock_last_capabilities, $rt->audio_clock_last_status
printf "audio clock sfx/rt/release status = 0x%x / 0x%x / 0x%x\n", $rt->audio_clock_reactive_sfx_status, $rt->audio_clock_realtime_status, $rt->audio_clock_release_status
printf "request flags req/release = %u / %u\n", g_ps_hw6_audio_clock_probe_request, g_ps_hw6_audio_clock_probe_release_request
printf "requester active/aggregated = 0x%x / 0x%x\n", $cp->requester_active_mask, $cp->aggregated_capabilities
printf "requester caps audio/runtime = 0x%x / 0x%x\n", $cp->requester_capabilities[1], $cp->requester_capabilities[8]
printf "requester stat audio/runtime = 0x%x / 0x%x\n", $cp->requester_status[1], $cp->requester_status[8]
printf "domains req/managed/readback = 0x%x / 0x%x / 0x%x\n", $cp->required_domain_mask, $cp->managed_domain_mask, $cp->readback_domain_mask
printf "STOP2 blockers caps/dom/ready/lpbam = 0x%x / 0x%x / %u / %u\n", $cp->stop2_blocker_capabilities, $cp->stop2_blocker_domain_mask, $cp->stop2_ready, $cp->lpbam_stop2_ready
printf "PLL2 req/out/on/off/status = 0x%x / 0x%x / %u / %u / 0x%x\n", $cp->pll2_required_output_mask, $cp->pll2_output_enabled_mask, $cp->pll2_domain_on_count, $cp->pll2_domain_off_count, $cp->pll2_domain_last_status
printf "kernel usb/sai/ospi Hz = %u / %u / %u\n", $cp->usb_kernel_hz, $cp->sai1_kernel_hz, $cp->ospi_kernel_hz
printf "profiles: UNKNOWN=0 BOOT_RECOVERY=1 REACTIVE_BASE=2 REACTIVE_BURST=3 REALTIME_BALANCED=4 IO_HIGH=5 STOP_PREP=6\n"
printf "caps: USB=0x1 OCTOSPI=0x2 SAI=0x4 DISPLAY=0x8 RT=0x10 REACTIVE=0x20 LPBAM_DISPLAY=0x40\n"
printf "audio clock reasons: NONE=0 REACTIVE_SFX=1 REALTIME_MIXER=2 RELEASE=3\n"
printf "status: TX_SUCCESS=0x0 TX_NOT_DONE=0x20 NOT_RUN=0xffffffff\n"