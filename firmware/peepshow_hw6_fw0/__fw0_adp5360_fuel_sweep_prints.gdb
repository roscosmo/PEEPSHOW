set pagination off
echo === HW6 FW0 ADP5360 FUEL-GAUGE SWEEP ===\n
echo magic =\n
p/x g_ps_hw6_adp5360_fuel_probe.magic
echo version =\n
p/x g_ps_hw6_adp5360_fuel_probe.version
echo phase =\n
p/x g_ps_hw6_adp5360_fuel_probe.phase
echo complete =\n
p/x g_ps_hw6_adp5360_fuel_probe.complete
echo success =\n
p/x g_ps_hw6_adp5360_fuel_probe.success
echo skipped =\n
p/x g_ps_hw6_adp5360_fuel_probe.skipped
echo blocked_phase =\n
p/x g_ps_hw6_adp5360_fuel_probe.blocked_phase
echo duration_ticks =\n
p/d g_ps_hw6_adp5360_fuel_probe.duration_ticks

echo \n--- safety guards ---\n
echo guard_required_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.guard_required_mask
echo guard_pass_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.guard_pass_mask
echo guard_read_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.guard_read_ok_mask
echo guard_address =\n
p/x g_ps_hw6_adp5360_fuel_probe.guard_address
echo guard_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.guard_value
echo guard_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.guard_status
echo guard_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.guard_error
echo prewrite_pgood_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.prewrite_pgood_status
echo prewrite_pgood_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.prewrite_pgood_error
echo prewrite_pgood_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.prewrite_pgood_value
echo postwrite_pgood_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.postwrite_pgood_status
echo postwrite_pgood_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.postwrite_pgood_error
echo postwrite_pgood_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.postwrite_pgood_value
echo postwrite_vbus_absent =\n
p/x g_ps_hw6_adp5360_fuel_probe.postwrite_vbus_absent

echo \n--- temporary monitor configuration ---\n
echo register_address =\n
p/x g_ps_hw6_adp5360_fuel_probe.register_address
echo original_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.original_value
echo candidate_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.candidate_value
echo candidate_readback =\n
p/x g_ps_hw6_adp5360_fuel_probe.candidate_readback
echo snapshot_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.snapshot_ok_mask
echo write_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.write_ok_mask
echo verify_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.verify_ok_mask
echo candidate_match_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.candidate_match_mask
echo snapshot_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.snapshot_status
echo write_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.write_status
echo verify_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.verify_status
echo snapshot_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.snapshot_error
echo write_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.write_error
echo verify_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.verify_error

echo \n--- SOC refresh ---\n
echo reset_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.reset_value
echo reset_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.reset_status
echo reset_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.reset_error
echo reset_write_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.reset_write_ok_mask

echo \n--- 13 samples over 12 seconds ---\n
echo sample_count =\n
p/d g_ps_hw6_adp5360_fuel_probe.sample_count
echo sample_tick =\n
p/d g_ps_hw6_adp5360_fuel_probe.sample_tick
echo sample_soc_percent =\n
p/d g_ps_hw6_adp5360_fuel_probe.sample_soc
echo sample_vbat_mv =\n
p/d g_ps_hw6_adp5360_fuel_probe.sample_vbat_mv
echo sample_vbat_h =\n
p/x g_ps_hw6_adp5360_fuel_probe.sample_vbat_h
echo sample_vbat_l =\n
p/x g_ps_hw6_adp5360_fuel_probe.sample_vbat_l
echo sample_soc_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.sample_soc_ok_mask
echo sample_vbat_h_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.sample_vbat_h_ok_mask
echo sample_vbat_l_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.sample_vbat_l_ok_mask
echo first_sample_failure_index =\n
p/x g_ps_hw6_adp5360_fuel_probe.first_sample_failure_index
echo first_sample_failure_register =\n
p/x g_ps_hw6_adp5360_fuel_probe.first_sample_failure_register
echo first_sample_failure_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.first_sample_failure_status
echo first_sample_failure_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.first_sample_failure_error

echo \n--- exact restoration and final state ---\n
echo restore_attempted =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_attempted
echo restore_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_ok_mask
echo restore_verify_ok_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_verify_ok_mask
echo restore_match_mask =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_match_mask
echo restored_readback =\n
p/x g_ps_hw6_adp5360_fuel_probe.restored_readback
echo restore_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_status
echo restore_verify_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_verify_status
echo restore_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_error
echo restore_verify_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.restore_verify_error
echo final_fault_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_fault_status
echo final_fault_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_fault_error
echo final_fault_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_fault_value
echo final_fault_clear =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_fault_clear
echo final_pgood_status =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_pgood_status
echo final_pgood_error =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_pgood_error
echo final_pgood_value =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_pgood_value
echo final_vbus_absent =\n
p/x g_ps_hw6_adp5360_fuel_probe.final_vbus_absent

if g_ps_hw6_adp5360_fuel_probe.success == 1
  echo PASS: fuel-gauge samples captured and monitor configuration restored exactly.\n
else
  if g_ps_hw6_adp5360_fuel_probe.skipped == 1
    echo SKIPPED: a safety guard or prewrite snapshot blocked the test.\n
  else
    echo FAIL: inspect blocked_phase, masks, statuses, and restoration fields above.\n
  end
end
echo === END HW6 FW0 ADP5360 FUEL-GAUGE SWEEP ===\n
