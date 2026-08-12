set pagination off
set $cp = &g_ps_hw6_clock_policy_probe
printf "--- HW6 clock policy scaffold ---\n"
printf "api/apply/restore/status = %u / %u / %u / 0x%x\n", $cp->api_version, $cp->apply_count, $cp->restore_count, $cp->last_status
printf "request/selected/current/caps = %u / %u / %u / 0x%x\n", $cp->requested_profile, $cp->selected_profile, $cp->current_profile, $cp->active_capabilities
printf "stage/tick usb on/off = %u / %u / %u / %u\n", $cp->last_stage, $cp->last_tick, $cp->usb_domain_on_count, $cp->usb_domain_off_count
printf "sysclk before/after Hz = %u / %u\n", $cp->sysclk_before_hz, $cp->sysclk_after_hz
printf "hclk before/after Hz   = %u / %u\n", $cp->hclk_before_hz, $cp->hclk_after_hz
printf "pclk1/2/3 Hz           = %u / %u / %u\n", $cp->pclk1_after_hz, $cp->pclk2_after_hz, $cp->pclk3_after_hz
printf "flash latency          = %u\n", $cp->flash_latency
printf "usb clk/vdd/hsi48      = %u / %u / %u\n", $cp->usb_clock_enabled, $cp->vddusb_enabled, $cp->hsi48_ready
printf "pll1/pll2 ready        = %u / %u\n", $cp->pll1_ready, $cp->pll2_ready
printf "kernel usb/sai/ospi Hz = %u / %u / %u\n", $cp->usb_kernel_hz, $cp->sai1_kernel_hz, $cp->ospi_kernel_hz
printf "profiles: UNKNOWN=0 BOOT_RECOVERY=1 REACTIVE_BASE=2 REACTIVE_BURST=3 REALTIME_BALANCED=4 IO_HIGH=5 STOP_PREP=6\n"
printf "caps: USB=0x1 OCTOSPI=0x2 SAI=0x4 DISPLAY=0x8 RT=0x10 REACTIVE=0x20\n"
printf "stages: IDLE=0 SELECT=1 SYSCLK_IO_HIGH=2 USB_ON=3 RESTORE_BASE=4 USB_OFF=5 SYSTICK=6 COMPLETE=7\n"
printf "status: TX_SUCCESS=0x0 TX_NOT_DONE=0x20 NOT_RUN=0xffffffff\n"