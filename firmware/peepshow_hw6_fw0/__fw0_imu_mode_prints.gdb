set pagination off
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 LIS2DUX12 IMU mode scaffold ---\n"
printf "owner api/current/prev/req/event = %u / %u / %u / %u / %u\n", $sm->version, $sm->current_state[PS_HW6_SM_IMU], $sm->previous_state[PS_HW6_SM_IMU], $sm->requested_state[PS_HW6_SM_IMU], $sm->last_event[PS_HW6_SM_IMU]
printf "mode count/request/active/status/tick = %u / %u / %u / 0x%x / %u\n", $sm->imu_mode_request_count, $sm->imu_mode_requested, $sm->imu_mode_active, $sm->imu_mode_last_status, $sm->imu_mode_last_tick
printf "placeholder/power-floor/wake-source = %u / %u / %u\n", $sm->imu_mode_placeholder, $sm->imu_mode_power_floor, $sm->imu_mode_wake_source_enabled
printf "driver api/state/status/ops = %u / %u / 0x%x / %u\n", $sm->imu_driver_api_version, $sm->imu_driver_state, $sm->imu_driver_last_status, $sm->imu_driver_operation_count
printf "identity status/whoami/match = 0x%x / 0x%x / %u\n", $sm->imu_whoami_status, $sm->imu_whoami, $sm->imu_identity_match
printf "deep power value/write/committed/omitted = 0x%x / 0x%x / %u / %u\n", $sm->imu_deep_power_down_value, $sm->imu_deep_power_down_write_status, $sm->imu_terminal_deep_power_down_committed, $sm->imu_post_deep_power_down_read_omitted
printf "i2c state/error = 0x%x / 0x%x\n", $sm->imu_i2c_state_after, $sm->imu_i2c_error_after
printf "cycle wake status/error/accepted = 0x%x / 0x%x / %u\n", $sm->imu_cycle_wake_probe_status[0], $sm->imu_cycle_wake_probe_error[0], $sm->imu_cycle_wake_probe_accepted[0]
printf "cycle whoami/active ctrl5/status/sleep = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->imu_cycle_whoami[0], $sm->imu_cycle_active_ctrl5[0], $sm->imu_cycle_active_status[0], $sm->imu_cycle_sleep_status[0]
printf "states: OFF=0 PROBE=1 CONFIG=2 EMBED=3 STEP=4 EVENT=5 LOW_RATE=6 STREAM=7 SUSPENDED=8 RECOVERING=9 ERROR=10\n"
printf "modes: OFF=0 LOW_RATE=1 EVENT_ARMED=2 STEP_COUNTER=3 STREAMING=4; event/step/stream are placeholders for now\n"
printf "status: HAL_OK=0x0 HAL_ERROR=0x1 UNAVAILABLE=0xfffffffe NOT_RUN=0xffffffff\n"