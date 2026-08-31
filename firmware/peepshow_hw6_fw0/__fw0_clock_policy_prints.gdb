set pagination off
set $cp = &g_ps_hw6_clock_policy_probe
set $rt = &g_ps_hw6_rtos_probe
printf "--- HW6 clock policy scaffold ---\n"
printf "api/apply/restore/status = %u / %u / %u / 0x%x\n", $cp->api_version, $cp->apply_count, $cp->restore_count, $cp->last_status
printf "request/selected/current/caps = %u / %u / %u / 0x%x\n", $cp->requested_profile, $cp->selected_profile, $cp->current_profile, $cp->active_capabilities
printf "requester active/aggregated = 0x%x / 0x%x\n", $cp->requester_active_mask, $cp->aggregated_capabilities
printf "requester caps P/A/I/D/S/ST/C/UI/RT = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $cp->requester_capabilities[0], $cp->requester_capabilities[1], $cp->requester_capabilities[2], $cp->requester_capabilities[3], $cp->requester_capabilities[4], $cp->requester_capabilities[5], $cp->requester_capabilities[6], $cp->requester_capabilities[7], $cp->requester_capabilities[8]
printf "requester stat P/A/I/D/S/ST/C/UI/RT = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $cp->requester_status[0], $cp->requester_status[1], $cp->requester_status[2], $cp->requester_status[3], $cp->requester_status[4], $cp->requester_status[5], $cp->requester_status[6], $cp->requester_status[7], $cp->requester_status[8]
printf "audio clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->audio_clock_request_count, $rt->audio_clock_release_count, $rt->audio_clock_last_reason, $rt->audio_clock_last_capabilities, $rt->audio_clock_last_status
printf "audio clock sfx/rt/release = 0x%x / 0x%x / 0x%x\n", $rt->audio_clock_reactive_sfx_status, $rt->audio_clock_realtime_status, $rt->audio_clock_release_status
printf "storage clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->storage_clock_request_count, $rt->storage_clock_release_count, $rt->storage_clock_last_reason, $rt->storage_clock_last_capabilities, $rt->storage_clock_last_status
printf "storage clock export/reclaim/flash/attach/post/release = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $rt->storage_clock_export_status, $rt->storage_clock_reclaim_status, $rt->storage_clock_flash_init_status, $rt->storage_clock_attach_status, $rt->storage_clock_post_stop_resume_status, $rt->storage_clock_release_status
printf "runtime clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->runtime_clock_request_count, $rt->runtime_clock_release_count, $rt->runtime_clock_last_reason, $rt->runtime_clock_last_capabilities, $rt->runtime_clock_last_status
printf "runtime clock reactive/realtime/release = 0x%x / 0x%x / 0x%x\n", $rt->runtime_clock_reactive_status, $rt->runtime_clock_realtime_status, $rt->runtime_clock_release_status
printf "ui clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->ui_clock_request_count, $rt->ui_clock_release_count, $rt->ui_clock_last_reason, $rt->ui_clock_last_capabilities, $rt->ui_clock_last_status
printf "ui clock reactive/release = 0x%x / 0x%x\n", $rt->ui_clock_reactive_status, $rt->ui_clock_release_status
printf "display clock req/rel/reason/caps/status = %u / %u / %u / 0x%x / 0x%x\n", $rt->display_clock_request_count, $rt->display_clock_release_count, $rt->display_clock_last_reason, $rt->display_clock_last_capabilities, $rt->display_clock_last_status
printf "display clock transfer/release = 0x%x / 0x%x\n", $rt->display_clock_transfer_status, $rt->display_clock_release_status
printf "domains req/managed/readback = 0x%x / 0x%x / 0x%x\n", $cp->required_domain_mask, $cp->managed_domain_mask, $cp->readback_domain_mask
printf "STOP2 blockers caps/dom/ready/lpbam = 0x%x / 0x%x / %u / %u\n", $cp->stop2_blocker_capabilities, $cp->stop2_blocker_domain_mask, $cp->stop2_ready, $cp->lpbam_stop2_ready
printf "PLL2 autogate en/skip       = %u / %u\n", $cp->pll2_autogate_enabled, $cp->pll2_autogate_skip_count
printf "PLL2 req/out/on/off/status = 0x%x / 0x%x / %u / %u / 0x%x\n", $cp->pll2_required_output_mask, $cp->pll2_output_enabled_mask, $cp->pll2_domain_on_count, $cp->pll2_domain_off_count, $cp->pll2_domain_last_status
printf "PLL2 post-STOP pending/mask/invalidate/rearm attempt/success/status fast-path = %u / 0x%x / %u / %u / %u / 0x%x / %u\n", $cp->pll2_post_stop_rearm_pending, $cp->pll2_post_stop_rearm_pending_domain_mask, $cp->pll2_post_stop_invalidate_count, $cp->pll2_post_stop_rearm_attempt_count, $cp->pll2_post_stop_rearm_success_count, $cp->pll2_post_stop_rearm_status, $cp->pll2_fast_path_count
printf "SAI post-STOP mux handoff count/success/status = %u / %u / 0x%x\n", $cp->sai_mux_handoff_count, $cp->sai_mux_handoff_success_count, $cp->sai_mux_handoff_status
printf "SAI restart park gate/reset/source/kernel restore gate/reset/source/kernel = %u/%u/0x%x/%u %u/%u/0x%x/%u\n", $cp->sai_mux_park_clock_enabled, $cp->sai_mux_park_reset_asserted, $cp->sai_mux_park_source, $cp->sai_mux_park_kernel_hz, $cp->sai_mux_restore_clock_enabled, $cp->sai_mux_restore_reset_asserted, $cp->sai_mux_restore_source, $cp->sai_mux_restore_kernel_hz
printf "SAI active/gate/reset on/off/reset-count/epoch/status = %u / %u / %u / %u / %u / %u / %u / 0x%x\n", $cp->sai_domain_active, $cp->sai_clock_enabled, $cp->sai_reset_asserted, $cp->sai_domain_on_count, $cp->sai_domain_off_count, $cp->sai_reset_count, $cp->sai_grant_epoch, $cp->sai_domain_last_status
printf "STOP2 prepare count/status/physical/fail = %u / 0x%x / %u / 0x%x\n", $cp->stop2_prepare_count, $cp->stop2_prepare_status, $cp->stop2_physical_ready, $cp->stop2_physical_failure_mask
printf "target/sysclk before/after Hz = %u / %u / %u\n", $cp->target_sysclk_hz, $cp->sysclk_before_hz, $cp->sysclk_after_hz
printf "hclk before/after Hz   = %u / %u\n", $cp->hclk_before_hz, $cp->hclk_after_hz
printf "pclk1/2/3 Hz           = %u / %u / %u\n", $cp->pclk1_after_hz, $cp->pclk2_after_hz, $cp->pclk3_after_hz
printf "flash latency          = %u\n", $cp->flash_latency
printf "usb clk/vdd/hsi48/shsi = %u / %u / %u / %u\n", $cp->usb_clock_enabled, $cp->vddusb_enabled, $cp->hsi48_ready, $cp->shsi_ready
printf "pll1/pll2/pll3 ready   = %u / %u / %u\n", $cp->pll1_ready, $cp->pll2_ready, $cp->pll3_ready
printf "kernel usb/sai/ospi Hz = %u / %u / %u\n", $cp->usb_kernel_hz, $cp->sai1_kernel_hz, $cp->ospi_kernel_hz
printf "profile masks supported/scaffold = 0x%x / 0x%x\n", $cp->supported_profile_mask, $cp->scaffold_profile_mask
printf "profiles: UNKNOWN=0 BOOT_RECOVERY=1 REACTIVE_BASE=2 REACTIVE_BURST=3 REALTIME_BALANCED=4 IO_HIGH=5 STOP_PREP=6\n"
printf "caps: USB=0x1 OCTOSPI=0x2 SAI=0x4 DISPLAY=0x8 RT=0x10 REACTIVE=0x20 LPBAM_DISPLAY=0x40\n"
printf "domains: USB=0x1 PLL2_OCTOSPI=0x2 PLL2_SAI=0x4 DISPLAY=0x8 RT=0x10 REACTIVE=0x20 LPBAM_DISPLAY=0x40\n"
printf "stages: IDLE=0 SELECT=1 SYSCLK_IO_HIGH=2 USB_ON=3 RESTORE_BASE=4 USB_OFF=5 SYSTICK=6 COMPLETE=7 REQUESTER=8 RESOLVE=9 PLL2_ON=10 PLL2_OFF=11 PLL2_REARM=12 SAI_ON=13 SAI_OFF=14 STOP_VERIFY=15\n"
printf "STOP2 fail bits: REQUESTERS=0x1 SAI_GATE=0x2 SAI_RESET=0x4 PLL2_RDY=0x8 PLL2_OUT=0x10 PLL3_RDY=0x20 HSI48=0x40 SHSI=0x80 USB_GATE=0x100 VDDUSB=0x200\n"
printf "audio clock reasons: NONE=0 REACTIVE_SFX=1 REALTIME_MIXER=2 RELEASE=3\n"
printf "storage clock reasons: NONE=0 MSC_EXPORT=1 MSC_RECLAIM=2 FLASH_INIT=3 RELEASE=4 ATTACH=5 POST_STOP_RESUME=6\n"
printf "runtime clock reasons: NONE=0 REACTIVE_TXN=1 REALTIME_DEADLINE=2 RELEASE=3\n"
printf "ui clock reasons: NONE=0 REACTIVE_TXN=1 RELEASE=2\n"
printf "display clock reasons: NONE=0 TRANSFER=1 RELEASE=2\n"
printf "status: TX_SUCCESS=0x0 TX_NOT_DONE=0x20 NOT_RUN=0xffffffff\n"
