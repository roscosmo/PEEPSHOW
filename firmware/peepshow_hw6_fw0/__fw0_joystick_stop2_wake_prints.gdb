set pagination off

set $sm = &g_ps_hw6_owner_sm_probe
set $rt = &g_ps_hw6_rtos_probe

printf "\n--- HW6 joystick movement wake from STOP2 ---\n"
printf "api rtos/owner/driver = %u / %u / %u\n", $rt->version, $sm->version, $sm->joystick_driver_api_version
printf "ThreadX pool available before/after fragments = %u / %u / %u\n", $rt->pool_available_before, $rt->pool_available_after, $rt->pool_fragments_after
printf "sensor stack bytes/start/end/ptr/lower margin = %u / 0x%x / 0x%x / 0x%x / %u\n", $rt->thread_stack_config_bytes[4], $rt->thread_stack_start[4], $rt->thread_stack_end[4], $rt->thread_stack_ptr[4], $rt->thread_stack_ptr[4] - $rt->thread_stack_start[4]
printf "wake/sleep arm count/status = %u / 0x%x\n", $sm->joystick_wake_sleep_arm_count, $sm->joystick_wake_sleep_arm_status
printf "STOP2 expected wake mask / GPIOC wake/park masks = 0x%x / 0x%x / 0x%x\n", $sm->stop2_expected_wake_pin, $sm->stop2_gpio_wake_mask[2], $sm->stop2_gpio_park_mask[2]
printf "wake period ms/code field threshold/hysteresis = %u / %u / %u / %u\n", $sm->joystick_wake_sleep_period_ms, $sm->joystick_wake_sleep_period_code, $sm->joystick_wake_field_threshold_code, $sm->joystick_wake_field_hysteresis_code
printf "wake registers sensor1/2/3 threshold X/Y/Z = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->joystick_wake_sensor_config1, $sm->joystick_wake_sensor_config2, $sm->joystick_wake_sensor_config3, $sm->joystick_wake_threshold_x, $sm->joystick_wake_threshold_y, $sm->joystick_wake_threshold_z
printf "wake registers high X/Y/Z int/device2 = 0x%x / 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->joystick_wake_threshold_x_high, $sm->joystick_wake_threshold_y_high, $sm->joystick_wake_threshold_z_high, $sm->joystick_wake_int_config1, $sm->joystick_wake_device_config2
printf "wake write/verify masks HAL/error = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->joystick_wake_write_ok_mask, $sm->joystick_wake_verify_ok_mask, $sm->joystick_wake_last_hal_status, $sm->joystick_wake_last_hal_error
printf "wake pre-clear pin/read/status/THR/INT = %u / 0x%x / 0x%x / %u / %u\n", $sm->joystick_wake_preclear_int_pin_level, $sm->joystick_wake_preclear_read_status, $sm->joystick_wake_preclear_device_status, $sm->joystick_wake_preclear_threshold_cross, $sm->joystick_wake_preclear_int_readback
printf "wake direction capture count/status/tick/mask/pending/consume = %u / 0x%x / %u / 0x%x / %u / %u\n", $sm->joystick_wake_direction_capture_count, $sm->joystick_wake_direction_capture_status, $sm->joystick_wake_direction_capture_tick, $sm->joystick_wake_direction_capture_mask, $sm->joystick_wake_direction_capture_pending, $sm->joystick_wake_direction_capture_consume_count
printf "wake direction raw X/Y normalized X/Y = %d / %d / %d / %d\n", $sm->joystick_wake_direction_capture_raw_x, $sm->joystick_wake_direction_capture_raw_y, $sm->joystick_wake_direction_capture_normalized_x, $sm->joystick_wake_direction_capture_normalized_y
printf "JOY IRQ count/enq/coalesce/deq/drop/pending/send = %u / %u / %u / %u / %u / %u / 0x%x\n", $rt->joystick_irq_count, $rt->joystick_irq_enqueue_count, $rt->joystick_irq_coalesce_count, $rt->joystick_irq_dequeue_count, $rt->joystick_irq_drop_count, $rt->joystick_irq_pending_count, $rt->joystick_irq_last_send_status
printf "STOP wake classify/source/primary/joystick = %u / 0x%x / %u / %u\n", $rt->stop2_wake_classify_count, $rt->stop2_wake_source_mask, $rt->stop2_wake_primary_cause, $rt->stop2_wake_joystick_count
printf "final input check/veto/status pending/GPIOC = %u / %u / 0x%x / %u / 0x%x\n", $rt->stop2_final_input_check_count, $rt->stop2_final_input_veto_count, $rt->stop2_final_input_last_status, $rt->stop2_final_joystick_pending_count, $rt->stop2_final_input_gpioc_idr
printf "cardinal req/status/fail stage/driver = %u / 0x%x / %u / %u / 0x%x\n", $sm->joystick_cardinal_request_count, $sm->joystick_cardinal_status, $sm->joystick_cardinal_failure_count, $sm->joystick_cardinal_last_failure_stage, $sm->joystick_cardinal_last_driver_status
printf "wake confirm requests/samples/stable/fallback/direction/status = %u / %u / %u / %u / 0x%x / 0x%x\n", $sm->joystick_wake_confirm_request_count, $sm->joystick_wake_confirm_sample_count, $sm->joystick_wake_confirm_stable_count, $sm->joystick_wake_confirm_fallback_count, $sm->joystick_wake_confirm_direction_mask, $sm->joystick_wake_confirm_status
printf "wake confirmed LEFT/RIGHT/UP/DOWN = %u / %u / %u / %u\n", $sm->joystick_wake_confirm_left_count, $sm->joystick_wake_confirm_right_count, $sm->joystick_wake_confirm_up_count, $sm->joystick_wake_confirm_down_count
printf "resolved direction/magnitude = 0x%x / %u\n", $sm->joystick_input_direction_mask, $sm->joystick_input_magnitude
printf "TMAG CORDIC samples/current/min/max = %u / %u / %u / %u\n", $sm->joystick_cordic_sample_count, $sm->joystick_cordic_magnitude_result, $sm->joystick_cordic_magnitude_min, $sm->joystick_cordic_magnitude_max
printf "TMAG status/THR samples/INT readback/pin = 0x%x / %u / %u / %u\n", $sm->joystick_cordic_device_status, $sm->joystick_cordic_threshold_cross_count, $sm->joystick_cordic_int_readback, $sm->joystick_cordic_int_pin_level
printf "logical activate/release/switch/drop = %u / %u / %u / %u\n", $rt->joystick_logical_activation_count, $rt->joystick_logical_release_count, $rt->joystick_logical_switch_count, $rt->joystick_logical_drop_count
printf "logical last direction/source/target/status/tick = 0x%x / %u / %u / 0x%x / %u\n", $rt->joystick_logical_last_direction_mask, $rt->joystick_logical_last_source, $rt->joystick_logical_last_target, $rt->joystick_logical_last_status, $rt->joystick_logical_last_tick
printf "logical STOP2 latch reset request/consume/pending/previous/tick = %u / %u / %u / 0x%x / %u\n", $rt->joystick_logical_stop2_reset_request_count, $rt->joystick_logical_stop2_reset_consume_count, $rt->joystick_logical_stop2_reset_pending, $rt->joystick_logical_stop2_reset_previous_direction, $rt->joystick_logical_stop2_reset_tick
printf "logical wake capture available/publish/direction/status/tick = %u / %u / 0x%x / 0x%x / %u\n", $rt->joystick_logical_wake_capture_available_count, $rt->joystick_logical_wake_capture_publish_count, $rt->joystick_logical_wake_capture_direction, $rt->joystick_logical_wake_capture_status, $rt->joystick_logical_wake_capture_tick
if (($rt->stop2_wake_source_mask & 0x4) && ($sm->joystick_wake_direction_capture_tick >= $sm->stop2_wake_tick) && ($rt->joystick_logical_wake_capture_tick >= $sm->joystick_wake_direction_capture_tick))
  printf "latency wake-to-end/capture/logical capture-to-logical ticks = %u / %u / %u / %u\n", $sm->stop2_end_tick - $sm->stop2_wake_tick, $sm->joystick_wake_direction_capture_tick - $sm->stop2_wake_tick, $rt->joystick_logical_wake_capture_tick - $sm->stop2_wake_tick, $rt->joystick_logical_wake_capture_tick - $sm->joystick_wake_direction_capture_tick
else
  printf "latency wake-to-end/capture/logical capture-to-logical ticks = N/A (latest STOP2 wake is not the matching joystick capture)\n"
end
printf "STOP2 auto checks/entries/skips = %u / %u / %u\n", $rt->stop2_auto_check_count, $rt->stop2_auto_entry_count, $rt->stop2_auto_skip_count
printf "dir bits: LEFT=0x1 RIGHT=0x2 UP=0x4 DOWN=0x8; wake source JOYSTICK=0x4; primary JOYSTICK=3\n"
printf "expected omnipolar switch: APIs=64/73/9; neutral remains in STOP2, cardinal movement asserts JOY_INT, IRQ enq=deq with no drops, wake confirm uses <=3 samples and reaches 2 stable samples for a held cardinal, wake source includes 0x4, primary=3, and one logical activation is delivered per movement wake\n"
printf "expected registers: sensor1/2/3=0x34/0x00/0x20 threshold X/Y/Z=0x30/0x30/0 high X/Y/Z=0/0/0 int=0x18 device2=0xe3 write/verify=0xfff/0x7ff\n"
printf "--- end joystick movement wake ---\n"
