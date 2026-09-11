set pagination off
set print pretty off
printf "--- HW6 persistent package index scan ---\n"
if g_ps_storage_package_index_probe.api_version != 3
  printf "Probe mismatch: expected index API 3. Load the matching ELF before interpreting this journal.\n"
else
printf "api/scan/status = %lu / %lu / 0x%lx\n", g_ps_storage_package_index_probe.api_version, g_ps_storage_package_index_probe.scan_count, g_ps_storage_package_index_probe.status
printf "index start/length package start/length = 0x%lx / %lu / 0x%lx / %lu\n", g_ps_storage_package_index_probe.index_region_start, g_ps_storage_package_index_probe.index_region_length, g_ps_storage_package_index_probe.package_region_start, g_ps_storage_package_index_probe.package_region_length
printf "valid/available selected record/slot/generation = %lu / %lu / %lu / %lu / %lu\n", g_ps_storage_package_index_probe.valid_record_count, g_ps_storage_package_index_probe.installed_available, g_ps_storage_package_index_probe.selected_record, g_ps_storage_package_index_probe.selected_slot, g_ps_storage_package_index_probe.selected_generation
printf "selected package start/size reason = 0x%lx / %lu / %lu\n", g_ps_storage_package_index_probe.selected_package_start, g_ps_storage_package_index_probe.selected_package_size, g_ps_storage_package_index_probe.selection_reason
printf "record 0 read/valid/reason magic/marker/gen/slot/size crc stored/computed = 0x%lx / %lu / %lu / 0x%lx / 0x%lx / %lu / %lu / %lu / 0x%lx / 0x%lx\n", g_ps_storage_package_index_probe.record[0].read_status, g_ps_storage_package_index_probe.record[0].valid, g_ps_storage_package_index_probe.record[0].reason, g_ps_storage_package_index_probe.record[0].magic, g_ps_storage_package_index_probe.record[0].commit_marker, g_ps_storage_package_index_probe.record[0].generation, g_ps_storage_package_index_probe.record[0].active_slot, g_ps_storage_package_index_probe.record[0].package_size, g_ps_storage_package_index_probe.record[0].stored_crc32, g_ps_storage_package_index_probe.record[0].computed_crc32
printf "record 1 read/valid/reason magic/marker/gen/slot/size crc stored/computed = 0x%lx / %lu / %lu / 0x%lx / 0x%lx / %lu / %lu / %lu / 0x%lx / 0x%lx\n", g_ps_storage_package_index_probe.record[1].read_status, g_ps_storage_package_index_probe.record[1].valid, g_ps_storage_package_index_probe.record[1].reason, g_ps_storage_package_index_probe.record[1].magic, g_ps_storage_package_index_probe.record[1].commit_marker, g_ps_storage_package_index_probe.record[1].generation, g_ps_storage_package_index_probe.record[1].active_slot, g_ps_storage_package_index_probe.record[1].package_size, g_ps_storage_package_index_probe.record[1].stored_crc32, g_ps_storage_package_index_probe.record[1].computed_crc32
printf "record 0 format/state; record 1 format/state = %lu / %lu ; %lu / %lu\n", g_ps_storage_package_index_probe.record[0].format_version, g_ps_storage_package_index_probe.record[0].transaction_state, g_ps_storage_package_index_probe.record[1].format_version, g_ps_storage_package_index_probe.record[1].transaction_state
printf "reasons: NONE=0 ARGUMENT=1 LAYOUT=2 READ=3 UNCOMMITTED=4 FORMAT=5 CRC=6 BOUNDS=7 RESERVED=8 CONFLICT=9 LEGACY=10 TRANSACTION=11 PENDING=12\n"
printf "expected erased index: valid/available=0/0 and both records reason=4\n"
printf "format=2 states PENDING=1 VALID=2 FAILED=3. Only the newest committed VALID permits launch. Two journal records do not mean two package slots.\n"
printf "expected installed: available=1 slot=0 address=0xc0000 size<=5242880, selected format/state=2/2 and matching CRCs. Legacy-only or newest PENDING/FAILED means shell/reinstall, not package launch.\n"
end
printf "--- end HW6 persistent package index scan ---\n"
