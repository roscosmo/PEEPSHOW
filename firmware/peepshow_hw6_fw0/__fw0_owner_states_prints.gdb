set pagination off
set $sm = &g_ps_hw6_owner_sm_probe
printf "--- HW6 owner state snapshot ---\n"
printf "api/version/status = %u / 0x%x\n", $sm->version, $sm->stop2_last_status
printf "power current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_POWER], $sm->previous_state[PS_HW6_SM_POWER], $sm->requested_state[PS_HW6_SM_POWER], $sm->last_event[PS_HW6_SM_POWER]
printf "audio current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_AUDIO], $sm->previous_state[PS_HW6_SM_AUDIO], $sm->requested_state[PS_HW6_SM_AUDIO], $sm->last_event[PS_HW6_SM_AUDIO]
printf "speaker current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_SPEAKER], $sm->previous_state[PS_HW6_SM_SPEAKER], $sm->requested_state[PS_HW6_SM_SPEAKER], $sm->last_event[PS_HW6_SM_SPEAKER]
printf "joystick current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_JOYSTICK], $sm->previous_state[PS_HW6_SM_JOYSTICK], $sm->requested_state[PS_HW6_SM_JOYSTICK], $sm->last_event[PS_HW6_SM_JOYSTICK]
printf "display current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_DISPLAY], $sm->previous_state[PS_HW6_SM_DISPLAY], $sm->requested_state[PS_HW6_SM_DISPLAY], $sm->last_event[PS_HW6_SM_DISPLAY]
printf "imu current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_IMU], $sm->previous_state[PS_HW6_SM_IMU], $sm->requested_state[PS_HW6_SM_IMU], $sm->last_event[PS_HW6_SM_IMU]
printf "storage current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_STORAGE], $sm->previous_state[PS_HW6_SM_STORAGE], $sm->requested_state[PS_HW6_SM_STORAGE], $sm->last_event[PS_HW6_SM_STORAGE]
printf "flash current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_FLASH], $sm->previous_state[PS_HW6_SM_FLASH], $sm->requested_state[PS_HW6_SM_FLASH], $sm->last_event[PS_HW6_SM_FLASH]
printf "ble current/prev/req/event = %u / %u / %u / %u\n", $sm->current_state[PS_HW6_SM_BLE], $sm->previous_state[PS_HW6_SM_BLE], $sm->requested_state[PS_HW6_SM_BLE], $sm->last_event[PS_HW6_SM_BLE]
printf "post-resume owner stat A/I/D/S/ST/C = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->post_stop_resume_owner_status[1], $sm->post_stop_resume_owner_status[2], $sm->post_stop_resume_owner_status[3], $sm->post_stop_resume_owner_status[4], $sm->post_stop_resume_owner_status[5], $sm->post_stop_resume_owner_status[6]
printf "power states: ACTIVE_LP=2 ACTIVE_RT=3 SLEEP_PREP=4 STOP_RESIDENT=5 WAKE_RESUME=6 SHIP_PREP=8\n"
printf "owners: A=audio I=input D=display S=sensor ST=storage C=comm\n"
printf "state values are raw enum values; use with stop2/post-resume masks to identify which owner rejected resume.\n"