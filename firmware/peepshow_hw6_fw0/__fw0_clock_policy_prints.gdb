set pagination off
set $cp = &g_ps_hw6_clock_policy_probe
set $rt = &g_ps_hw6_rtos_probe
printf "--- HW6 clock policy scaffold ---\n"
printf "api/apply/restore/status = %u / %u / %u / 0x%x\n", $cp->api_version, $cp->apply_count, $cp->restore_count, $cp->last_status
printf "request/selected/current/caps = %u / %u / %u / 0x%x\n", $cp->requested_profile, $cp->selected_profile, $cp->current_profile, $cp->active_capabilities
printf "requester active/aggregated = 0x%x / 0x%x\n", $cp->requester_active_mask, $cp->aggregated_capabilities
printf "requester caps P/A/I/D/S/ST/C/UI/RT = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $cp->requester_capabilities[0], $cp->requester_capabilities[1], $cp->requester_capabilities[2], $cp->requester_capabilities[3], $cp->requester_capabilities[4], $cp->requester_capabilities[5], $cp->requester_capabilities[6], $cp->requester_capabilities[7], $cp->requester_capabilities[8]
printf "requester stat P/A/I/D/S/ST/C/UI/RT = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $cp->requester_status[0], $cp->requester_status[1], $cp->requester_status[2], $cp->requester_status[3], $cp->requester_status[4], $cp->requester_status[5], $cp->requester_status[6], $cp->requester_status[7], $cp->requester_status[8]
printf "storage clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->storage_clock_request_count, $rt->storage_clock_release_count, $rt->storage_clock_last_reason, $rt->storage_clock_last_capabilities, $rt->storage_clock_last_status
printf "storage clock export/reclaim/flash/release = 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->storage_clock_export_status, $rt->storage_clock_reclaim_status, $rt->storage_clock_flash_init_status, $rt->storage_clock_release_status
printf "runtime clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->runtime_clock_request_count, $rt->runtime_clock_release_count, $rt->runtime_clock_last_reason, $rt->runtime_clock_last_capabilities, $rt->runtime_clock_last_status
printf "runtime clock reactive/realtime/release = 0x%x / 0x%x / 0x%x\n", $rt->runtime_clock_reactive_status, $rt->runtime_clock_realtime_status, $rt->runtime_clock_release_status
printf "ui clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->ui_clock_request_count, $rt->ui_clock_release_count, $rt->ui_clock_last_reason, $rt->ui_clock_last_capabilities, $rt->ui_clock_last_status
printf "ui clock reactive/release = 0x%x / 0x%x\n", $rt->ui_clock_reactive_status, $rt->ui_clock_release_status
printf "domains req/managed/readback = 0x%x / 0x%x / 0x%x\n", $cp->required_domain_mask, $cp->managed_domain_mask, $cp->readback_domain_mask
printf "STOP2 blockers caps/dom/ready/lpbam = 0x%x / 0x%x / %u / %u\n", $cp->stop2_blocker_capabilities, $cp->stop2_blocker_domain_mask, $cp->stop2_ready, $cp->lpbam_stop2_ready
printf "PLL2 autogate en/skip       = %u / %u\n", $cp->pll2_autogate_enabled, $cp->pll2_autogate_skip_count
printf "PLL2 req/out/on/off/status = 0x%x / 0x%x / %u / %u / 0x%x\n", $cp->pll2_required_output_mask, $cp->pll2_output_enabled_mask, $cp->pll2_domain_on_count, $cp->pll2_domain_off_count, $cp->pll2_domain_last_status
printf "target/sysclk before/after Hz = %u / %u / %u\n", $cp->target_sysclk_hz, $cp->sysclk_before_hz, $cp->sysclk_after_hz
printf "hclk before/after Hz   = %u / %u\n", $cp->hclk_before_hz, $cp->hclk_after_hz
printf "pclk1/2/3 Hz           = %u / %u / %u\n", $cp->pclk1_after_hz, $cp->pclk2_after_hz, $cp->pclk3_after_hz
printf "flash latency          = %u\n", $cp->flash_latency
printf "usb clk/vdd/hsi48      = %u / %u / %u\n", $cp->usb_clock_enabled, $cp->vddusb_enabled, $cp->hsi48_ready
printf "pll1/pll2 ready        = %u / %u\n", $cp->pll1_ready, $cp->pll2_ready
printf "kernel usb/sai/ospi Hz = %u / %u / %u\n", $cp->usb_kernel_hz, $cp->sai1_kernel_hz, $cp->ospi_kernel_hz
printf "profile masks supported/scaffold = 0x%x / 0x%x\n", $cp->supported_profile_mask, $cp->scaffold_profile_mask
printf "profiles: UNKNOWN=0 BOOT_RECOVERY=1 REACTIVE_BASE=2 REACTIVE_BURST=3 REALTIME_BALANCED=4 IO_HIGH=5 STOP_PREP=6\n"
printf "caps: USB=0x1 OCTOSPI=0x2 SAI=0x4 DISPLAY=0x8 RT=0x10 REACTIVE=0x20 LPBAM_DISPLAY=0x40\n"
printf "domains: USB=0x1 PLL2_OCTOSPI=0x2 PLL2_SAI=0x4 DISPLAY=0x8 RT=0x10 REACTIVE=0x20 LPBAM_DISPLAY=0x40\n"
printf "stages: IDLE=0 SELECT=1 SYSCLK_IO_HIGH=2 USB_ON=3 RESTORE_BASE=4 USB_OFF=5 SYSTICK=6 COMPLETE=7 REQUESTER=8 RESOLVE=9 PLL2_ON=10 PLL2_OFF=11\n"
printf "storage clock reasons: NONE=0 MSC_EXPORT=1 MSC_RECLAIM=2 FLASH_INIT=3 RELEASE=4\n"
printf "runtime clock reasons: NONE=0 REACTIVE_TXN=1 REALTIME_DEADLINE=2 RELEASE=3\n"
printf "ui clock reasons: NONE=0 REACTIVE_TXN=1 RELEASE=2\n"
printf "status: TX_SUCCESS=0x0 TX_NOT_DONE=0x20 NOT_RUN=0xffffffff\n"
