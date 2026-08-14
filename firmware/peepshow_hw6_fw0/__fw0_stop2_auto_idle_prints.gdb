set pagination off
set $rt = &g_ps_hw6_rtos_probe
set $sm = &g_ps_hw6_owner_sm_probe
set $ow = &g_ps_hw6_owner_probe
set $ui = &g_ps_ui_router_probe
set $btn = &g_ps_input_buttons_probe
set $clk = &g_ps_hw6_clock_policy_probe
printf "--- HW6 automatic STOP2 idle admission scaffold ---\n"
printf "rtos api/runtime/boot = %u / %u / %u\n", $rt->version, $rt->runtime_complete, $rt->boot_power_done
printf "auto enabled/check/entry/skip = %u / %u / %u / %u\n", $rt->stop2_auto_enabled, $rt->stop2_auto_check_count, $rt->stop2_auto_entry_count, $rt->stop2_auto_skip_count
printf "auto status/tick/next = 0x%x / %u / %u\n", $rt->stop2_auto_last_status, $rt->stop2_auto_last_tick, $rt->stop2_auto_next_tick
printf "auto idle start/live/required ticks = %u / %u / %u\n", $rt->stop2_auto_idle_start_tick, $rt->stop2_auto_idle_ticks, $rt->stop2_auto_required_idle_ticks
printf "auto block/pending/queue = 0x%x / 0x%x / 0x%x\n", $rt->stop2_auto_blocker_mask, $rt->stop2_auto_pending_mask, $rt->stop2_auto_queue_pending_mask
printf "auto elig/entry status = 0x%x / 0x%x\n", $rt->stop2_auto_eligibility_status, $rt->stop2_auto_entry_status
printf "elig ready/block/pending = %u / 0x%x / 0x%x\n", $rt->stop2_eligibility_ready, $rt->stop2_eligibility_blocker_mask, $rt->stop2_eligibility_pending_mask
printf "power state/pmic/battery = %u / %u / %u\n", $rt->stop2_eligibility_power_state, $rt->stop2_eligibility_pmic_state, $rt->stop2_eligibility_battery_policy
printf "runtime class/exec/life/caps = %u / %u / %u / 0x%x\n", $rt->runtime_current_class, $rt->runtime_execution, $rt->runtime_lifecycle, $rt->runtime_active_capabilities
printf "ui page/nav/modal/pkg/shutdown/pending = %u / %u / %u / %u / %u / %u\n", $ui->current_page, $ui->nav_state, $ui->modal_state, $ui->package_state, $ui->shutdown_state, $ui->pending_action
printf "display ui req/render/page/status = %u / %u / %u / 0x%x\n", $ow->display_ui_request_count, $ow->display_ui_render_count, $ow->display_ui_page, $ow->display_ui_status
printf "display complete/success = %u / %u\n", $ow->display_complete, $ow->display_success
printf "input pending/start/logical/policy = 0x%x / %u / %u / %u\n", $btn->pending_mask, $btn->start_active, $btn->logical_event_count, $rt->input_policy_event_count
printf "clock caps/dom/readback/lpbam = 0x%x / 0x%x / 0x%x / %u\n", $clk->stop2_blocker_capabilities, $clk->stop2_blocker_domain_mask, $clk->readback_domain_mask, $clk->lpbam_stop2_ready
printf "owner stop2 count/start/wake/end = %u / %u / %u / %u\n", $sm->stop2_request_count, $sm->stop2_start_tick, $sm->stop2_wake_tick, $sm->stop2_end_tick
printf "blockers: BOOT=0x1 POWER=0x2 PMIC=0x4 BATT=0x8 CLOCK_CAP=0x10 CLOCK_READBACK=0x20 DISABLED=0x40 RUNTIME=0x80 UI=0x100 DISPLAY=0x200 STORAGE_USB=0x400 INPUT=0x800 QUEUE=0x1000\n"
printf "pending: OWNER_QUIESCE=0x1 LPBAM=0x2 IDLE_WINDOW=0x4\n"
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 NOT_RUN=0xffffffff\n"