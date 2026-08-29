set pagination off

set $sm = &g_ps_hw6_owner_sm_probe
set $rt = &g_ps_hw6_rtos_probe
set $scan = &g_ps_storage_joystick_calibration_probe
set $save = &g_ps_storage_joystick_calibration_save_probe

printf "\n--- HW6 persistent joystick calibration ---\n"
printf "api rtos/owner/record = %u / %u / %u\n", $rt->version, $sm->version, $scan->api_version
printf "display stack bytes/start/end/ptr/lower margin = %u / 0x%x / 0x%x / 0x%x / %u\n", $rt->thread_stack_config_bytes[3], $rt->thread_stack_start[3], $rt->thread_stack_end[3], $rt->thread_stack_ptr[3], $rt->thread_stack_ptr[3] - $rt->thread_stack_start[3]
printf "boot resolved load count/status/available/generation = %u / %u / 0x%x / %u / %u\n", $sm->joystick_calibration_persistent_boot_resolved, $sm->joystick_calibration_persistent_load_count, $sm->joystick_calibration_persistent_load_status, $sm->joystick_calibration_persistent_load_available, $sm->joystick_calibration_persistent_load_generation
printf "apply count/status active valid/transform = %u / 0x%x / %u / %u\n", $sm->joystick_calibration_persistent_apply_count, $sm->joystick_calibration_persistent_apply_status, $sm->joystick_calibration_active_valid, $sm->joystick_calibration_transform_valid
printf "save request/pending/status/generation commit count = %u / %u / 0x%x / %u / %u\n", $sm->joystick_calibration_persistent_save_request_count, $sm->joystick_calibration_persistent_save_pending, $sm->joystick_calibration_persistent_save_status, $sm->joystick_calibration_persistent_save_generation, $sm->joystick_calibration_commit_count
printf "scan count/status region start/length = %u / 0x%x / 0x%x / %u\n", $scan->scan_count, $scan->status, $scan->region_start, $scan->region_length
printf "scan valid/available/selected/generation/reason = %u / %u / %u / %u / %u\n", $scan->valid_record_count, $scan->calibration_available, $scan->selected_record, $scan->selected_generation, $scan->selection_reason
printf "record 0 read/valid/reason magic/marker/generation crc stored/computed = 0x%x / %u / %u / 0x%x / 0x%x / %u / 0x%x / 0x%x\n", $scan->record[0].read_status, $scan->record[0].valid, $scan->record[0].reason, $scan->record[0].magic, $scan->record[0].commit_marker, $scan->record[0].generation, $scan->record[0].stored_crc32, $scan->record[0].computed_crc32
printf "record 1 read/valid/reason magic/marker/generation crc stored/computed = 0x%x / %u / %u / 0x%x / 0x%x / %u / 0x%x / 0x%x\n", $scan->record[1].read_status, $scan->record[1].valid, $scan->record[1].reason, $scan->record[1].magic, $scan->record[1].commit_marker, $scan->record[1].generation, $scan->record[1].stored_crc32, $scan->record[1].computed_crc32
printf "save count/status/stage source/target/generation/address = %u / 0x%x / %u / %u / %u / %u / 0x%x\n", $save->save_count, $save->status, $save->stage, $save->source_record, $save->target_record, $save->target_generation, $save->target_address
printf "save erase/program/verify mismatches = 0x%x / 0x%x / 0x%x / %u\n", $save->erase_status, $save->program_status, $save->verify_status, $save->verify_mismatches
printf "save commit/verify/marker rescan selected/generation = 0x%x / 0x%x / 0x%x / 0x%x / %u / %u\n", $save->commit_status, $save->commit_verify_status, $save->commit_marker, $save->rescan_status, $save->selected_record, $save->selected_generation
printf "selected center X/Y extents X min/max Y min/max = %d / %d / %d / %d / %d / %d\n", $scan->selected_calibration.center_x, $scan->selected_calibration.center_y, $scan->selected_calibration.min_x, $scan->selected_calibration.max_x, $scan->selected_calibration.min_y, $scan->selected_calibration.max_y
printf "active center X/Y extents X min/max Y min/max = %d / %d / %d / %d / %d / %d\n", $sm->joystick_calibration_center_x, $sm->joystick_calibration_center_y, $sm->joystick_calibration_sweep_min_x, $sm->joystick_calibration_sweep_max_x, $sm->joystick_calibration_sweep_min_y, $sm->joystick_calibration_sweep_max_y
printf "UI page/calibration/session input lock = %u / %u / %u / %u\n", g_ps_ui_router_probe.current_page, g_ps_ui_router_probe.calibration_page, $sm->joystick_calibration_session_active, $rt->input_policy_lock_active
printf "reasons: NONE=0 ARGUMENT=1 LAYOUT=2 READ=3 UNCOMMITTED=4 FORMAT=5 CRC=6 BOUNDS=7 RESERVED=8 CONFLICT=9\n"
printf "save stages: IDLE=0 VALIDATE=1 SCAN=2 ERASE=3 PROGRAM=4 VERIFY=5 COMMIT=6 RESCAN=7 COMPLETE=8\n"
printf "expected erased boot: resolved=1 load/apply status=0 available=0 active valid=0 records reason=4 and UI enters joystick CENTER\n"
printf "expected first save: display stack bytes=3072; save status=0 stage=8 target=0 generation=1 valid/available=1/1 selected=0; UI returns HOME/CAL_NONE with session/input lock=0/0; after reset load/apply status=0 and active equals selected\n"
printf "expected second save: source=0 target=1 generation=2 valid=2 selected=1; UI returns HOME/CAL_NONE, input lock and save pending return to zero\n"
printf "--- end persistent joystick calibration ---\n"
