set pagination off
printf "--- HW6 single-slot persistent package install ---\n"
if g_ps_storage_package_install_probe.api_version != 3
  printf "Probe mismatch: expected install API 3. Load the matching ELF before interpreting this journal.\n"
else
printf "api/install/status/stage = %lu / %lu / 0x%lx / %lu\n", g_ps_storage_package_install_probe.api_version, g_ps_storage_package_install_probe.install_count, g_ps_storage_package_install_probe.status, g_ps_storage_package_install_probe.stage
printf "package size/hash low/high = %lu / 0x%lx / 0x%lx\n", g_ps_storage_package_install_probe.package_size, g_ps_storage_package_install_probe.package_id_hash_low, g_ps_storage_package_install_probe.package_id_hash_high
printf "source record/slot target record/slot/generation = %lu / %lu / %lu / %lu / %lu\n", g_ps_storage_package_install_probe.source_record, g_ps_storage_package_install_probe.source_slot, g_ps_storage_package_install_probe.target_record, g_ps_storage_package_install_probe.target_slot, g_ps_storage_package_install_probe.target_generation
printf "target package/index address = 0x%lx / 0x%lx\n", g_ps_storage_package_install_probe.target_package_start, g_ps_storage_package_install_probe.target_index_start
printf "pending record/generation/status / retire status = %lu / %lu / 0x%lx / 0x%lx\n", g_ps_storage_package_install_probe.pending_record, g_ps_storage_package_install_probe.pending_generation, g_ps_storage_package_install_probe.pending_status, g_ps_storage_package_install_probe.retire_status
printf "package erase count/status/polls = %lu / 0x%lx / %lu\n", g_ps_storage_package_install_probe.package_erase_count, g_ps_storage_package_install_probe.package_erase_status, g_ps_storage_package_install_probe.package_erase_poll_count
printf "package program status/bytes/pages/failure offset = 0x%lx / %lu / %lu / 0x%lx\n", g_ps_storage_package_install_probe.package_program_status, g_ps_storage_package_install_probe.package_program_bytes, g_ps_storage_package_install_probe.package_program_page_count, g_ps_storage_package_install_probe.package_program_failure_offset
printf "package program block/device init state/status = %lu / %lu / %lu / 0x%lx\n", g_ps_storage_package_install_probe.package_program_block_initialized, g_ps_storage_package_install_probe.package_program_device_initialized, g_ps_storage_package_install_probe.package_program_device_state, g_ps_storage_package_install_probe.package_program_device_status
printf "package verify status bytes/mismatches = 0x%lx / %lu / %lu\n", g_ps_storage_package_install_probe.package_verify_status, g_ps_storage_package_install_probe.package_verify_bytes, g_ps_storage_package_install_probe.package_verify_mismatches
printf "index erase/program/verify status mismatches/crc = 0x%lx / 0x%lx / 0x%lx / %lu / 0x%lx\n", g_ps_storage_package_install_probe.index_erase_status, g_ps_storage_package_install_probe.index_program_status, g_ps_storage_package_install_probe.index_verify_status, g_ps_storage_package_install_probe.index_verify_mismatches, g_ps_storage_package_install_probe.index_crc32
printf "commit program/verify/marker = 0x%lx / 0x%lx / 0x%lx\n", g_ps_storage_package_install_probe.commit_status, g_ps_storage_package_install_probe.commit_verify_status, g_ps_storage_package_install_probe.commit_marker
printf "rescan status selected record/slot/generation = 0x%lx / %lu / %lu / %lu\n", g_ps_storage_package_install_probe.rescan_status, g_ps_storage_package_install_probe.selected_record, g_ps_storage_package_install_probe.selected_slot, g_ps_storage_package_install_probe.selected_generation
printf "scan valid/available package start/size = %lu / %lu / 0x%lx / %lu\n", g_ps_storage_package_index_probe.valid_record_count, g_ps_storage_package_index_probe.installed_available, g_ps_storage_package_index_probe.selected_package_start, g_ps_storage_package_index_probe.selected_package_size
printf "owner storage/flash state OSPI state/error = %lu / %lu / 0x%lx / 0x%lx\n", g_ps_hw6_owner_sm_probe.current_state[7], g_ps_hw6_owner_sm_probe.current_state[8], hospi1.State, hospi1.ErrorCode
printf "stages: IDLE=0 VALIDATE=1 SCAN=2 ERASE_PKG=3 PROGRAM_PKG=4 VERIFY_PKG=5 ERASE_INDEX=6 PROGRAM_INDEX=7 VERIFY_INDEX=8 COMMIT=9 RESCAN=10 COMPLETE=11 PENDING=12 RETIRE=13\n"
printf "expected first install after legacy/empty index: pending record/gen=0/1, target record/slot/gen=1/0/2. Both pending/retire statuses=0; final status=0 stage=11, verify mismatches=0, marker=0x54494d43, available=1.\n"
printf "expected replacement: active slot stays 0 at 0xc0000; final generation normally advances by two (2 then 4). Two valid journal records include PENDING plus VALID, not two installed games. Interrupted replacement may require reinstall.\n"
end
printf "--- end HW6 single-slot persistent package install ---\n"
