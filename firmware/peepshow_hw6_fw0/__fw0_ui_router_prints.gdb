set pagination off

set $ui = &g_ps_ui_router_probe
set $ow = &g_ps_hw6_owner_probe
set $btn = &g_ps_input_buttons_probe

printf "\n--- HW6 UI router state ---\n"
printf "api/status/event       = %u / 0x%x / %u\n", $ui->api_version, $ui->last_status, $ui->last_event
printf "page prev/current/req  = %u / %u / %u\n", $ui->previous_page, $ui->current_page, $ui->requested_page
printf "nav/modal/cal/focus   = %u / %u / %u / %u\n", $ui->nav_state, $ui->modal_state, $ui->calibration_page, $ui->focus_index
printf "transitions/rejected  = %u / %u\n", $ui->transition_count, $ui->rejected_event_count
printf "button event/count    = %u / %u\n", $ui->last_button_event, $ui->button_event_count
printf "request/event flags   = %u / %u\n", g_ps_ui_router_request, g_ps_ui_router_request_event
printf "display ui req/render = %u / %u\n", $ow->display_ui_request_count, $ow->display_ui_render_count
printf "display ui page/cal   = %u / %u\n", $ow->display_ui_page, $ow->display_ui_calibration_page
printf "display status/hash   = 0x%x / 0x%x\n", $ow->display_ui_status, $ow->display_framebuffer_hash
printf "input edge/press      = %u / %u\n", $btn->isr_edge_count, $btn->press_count
printf "input pending/button  = 0x%x / %u\n", $btn->pending_mask, $btn->last_button_id
printf "pages: BOOT=0 HOME=1 MENU=2 SETTINGS=3 CAL=4 PACKAGES=5 RUNTIME=6 ERROR=7\n"
printf "cal: NONE=0 INPUT_ROOT=1 JOY_NEUTRAL=2 JOY_RIGHT=3 JOY_CIRCLE=4 JOY_REVIEW=5\n"
printf "events: BOOT_HOME=1 NAV_MENU=3 NAV_CAL=5 JOY_START=11 NEUTRAL=12 RIGHT=13 CIRCLE=14 REVIEW=15 BTN_A=17 BTN_B=18 BTN_L=19 BTN_R=20\n"
