set pagination off
printf "--- HW6 LPBAM compiler admission ---\n"
printf "api/status/reason = %u / 0x%x / %u\n", ps_lpbam_display_admission.api_version, ps_lpbam_display_admission.status, ps_lpbam_display_admission.reason
printf "sequence used/capacity = %u / %u\n", ps_lpbam_display_admission.sequence_used, ps_lpbam_display_admission.sequence_capacity
printf "chunks used/capacity = %u / %u\n", ps_lpbam_display_admission.chunk_used, ps_lpbam_display_admission.chunk_capacity
printf "payload wire/used/capacity bytes = %u / %u / %u\n", ps_lpbam_display_admission.payload_wire_bytes, ps_lpbam_display_admission.payload_used_bytes, ps_lpbam_display_admission.payload_capacity_bytes
printf "sequence first/count dirty start/count = %u/%u %u/%u | %u/%u %u/%u | %u/%u %u/%u | %u/%u %u/%u\n", ps_lpbam_display_sequence[0].first_chunk, ps_lpbam_display_sequence[0].chunk_count, ps_lpbam_display_sequence[0].dirty_start_row, ps_lpbam_display_sequence[0].dirty_row_count, ps_lpbam_display_sequence[1].first_chunk, ps_lpbam_display_sequence[1].chunk_count, ps_lpbam_display_sequence[1].dirty_start_row, ps_lpbam_display_sequence[1].dirty_row_count, ps_lpbam_display_sequence[2].first_chunk, ps_lpbam_display_sequence[2].chunk_count, ps_lpbam_display_sequence[2].dirty_start_row, ps_lpbam_display_sequence[2].dirty_row_count, ps_lpbam_display_sequence[3].first_chunk, ps_lpbam_display_sequence[3].chunk_count, ps_lpbam_display_sequence[3].dirty_start_row, ps_lpbam_display_sequence[3].dirty_row_count
printf "first four chunk lengths = %u / %u / %u / %u\n", ps_lpbam_display_tx_len[0], ps_lpbam_display_tx_len[1], ps_lpbam_display_tx_len[2], ps_lpbam_display_tx_len[3]
printf "owner payload sequence/chunks/wire bytes = %u / %u / %u\n", g_ps_hw6_owner_probe.display_lpbam_payload_frame_count, g_ps_hw6_owner_probe.display_lpbam_payload_chunk_count, g_ps_hw6_owner_probe.display_lpbam_payload_bytes
printf "reasons: NONE=0 ARGUMENT=1 SEQUENCE=2 CHUNKS=3 PAYLOAD=4 BUILD=5; HAL_OK=0x0\n"
printf "expected cursor: api/status/reason=1/0/0 sequence=4/4 chunks=4/12 payload=572/575/9216 entries=0/1,1/1,2/1,3/1 lengths=143 each\n"
printf "--- end HW6 LPBAM compiler admission ---\n"
