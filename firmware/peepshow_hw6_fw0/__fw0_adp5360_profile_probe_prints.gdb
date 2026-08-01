set pagination off
echo HW6 ADP5360 profile probe\n
echo magic = 
p/x g_ps_hw6_adp5360_profile_probe.magic
echo version = 
p/x g_ps_hw6_adp5360_profile_probe.version
echo phase = 
p/x g_ps_hw6_adp5360_profile_probe.phase
echo complete = 
p/x g_ps_hw6_adp5360_profile_probe.complete
echo success = 
p/x g_ps_hw6_adp5360_profile_probe.success
echo skipped = 
p/x g_ps_hw6_adp5360_profile_probe.skipped
echo blocked_phase = 
p/x g_ps_hw6_adp5360_profile_probe.blocked_phase
echo duration_ticks = 
p/d g_ps_hw6_adp5360_profile_probe.duration_ticks

echo \n--- safety guards ---\n
echo guard_required_mask = 
p/x g_ps_hw6_adp5360_profile_probe.guard_required_mask
echo guard_pass_mask = 
p/x g_ps_hw6_adp5360_profile_probe.guard_pass_mask
echo guard_read_ok_mask = 
p/x g_ps_hw6_adp5360_profile_probe.guard_read_ok_mask
echo guard_address = 
p/x g_ps_hw6_adp5360_profile_probe.guard_address
echo guard_value = 
p/x g_ps_hw6_adp5360_profile_probe.guard_value
echo guard_status = 
p/x g_ps_hw6_adp5360_profile_probe.guard_status
echo guard_error = 
p/x g_ps_hw6_adp5360_profile_probe.guard_error
echo prewrite_pgood_status = 
p/x g_ps_hw6_adp5360_profile_probe.prewrite_pgood_status
echo prewrite_pgood_error = 
p/x g_ps_hw6_adp5360_profile_probe.prewrite_pgood_error
echo prewrite_pgood_value = 
p/x g_ps_hw6_adp5360_profile_probe.prewrite_pgood_value
echo postwrite_pgood_status = 
p/x g_ps_hw6_adp5360_profile_probe.postwrite_pgood_status
echo postwrite_pgood_error = 
p/x g_ps_hw6_adp5360_profile_probe.postwrite_pgood_error
echo postwrite_pgood_value = 
p/x g_ps_hw6_adp5360_profile_probe.postwrite_pgood_value
echo postwrite_vbus_absent = 
p/x g_ps_hw6_adp5360_profile_probe.postwrite_vbus_absent

echo \n--- transaction masks ---\n
echo snapshot_ok_mask = 
p/x g_ps_hw6_adp5360_profile_probe.snapshot_ok_mask
echo write_attempted_count = 
p/d g_ps_hw6_adp5360_profile_probe.write_attempted_count
echo write_ok_mask = 
p/x g_ps_hw6_adp5360_profile_probe.write_ok_mask
echo verify_ok_mask = 
p/x g_ps_hw6_adp5360_profile_probe.verify_ok_mask
echo candidate_match_mask = 
p/x g_ps_hw6_adp5360_profile_probe.candidate_match_mask
echo restore_attempted = 
p/x g_ps_hw6_adp5360_profile_probe.restore_attempted
echo restore_attempted_count = 
p/d g_ps_hw6_adp5360_profile_probe.restore_attempted_count
echo restore_ok_mask = 
p/x g_ps_hw6_adp5360_profile_probe.restore_ok_mask
echo restore_verify_ok_mask = 
p/x g_ps_hw6_adp5360_profile_probe.restore_verify_ok_mask
echo restore_match_mask = 
p/x g_ps_hw6_adp5360_profile_probe.restore_match_mask

echo \n--- register values ---\n
echo register_address = 
p/x g_ps_hw6_adp5360_profile_probe.register_address
echo original_value = 
p/x g_ps_hw6_adp5360_profile_probe.original_value
echo candidate_value = 
p/x g_ps_hw6_adp5360_profile_probe.candidate_value
echo candidate_readback = 
p/x g_ps_hw6_adp5360_profile_probe.candidate_readback
echo restored_readback = 
p/x g_ps_hw6_adp5360_profile_probe.restored_readback

echo \n--- per-register HAL status ---\n
echo snapshot_status = 
p/x g_ps_hw6_adp5360_profile_probe.snapshot_status
echo write_status = 
p/x g_ps_hw6_adp5360_profile_probe.write_status
echo verify_status = 
p/x g_ps_hw6_adp5360_profile_probe.verify_status
echo restore_status = 
p/x g_ps_hw6_adp5360_profile_probe.restore_status
echo restore_verify_status = 
p/x g_ps_hw6_adp5360_profile_probe.restore_verify_status
echo snapshot_error = 
p/x g_ps_hw6_adp5360_profile_probe.snapshot_error
echo write_error = 
p/x g_ps_hw6_adp5360_profile_probe.write_error
echo verify_error = 
p/x g_ps_hw6_adp5360_profile_probe.verify_error
echo restore_error = 
p/x g_ps_hw6_adp5360_profile_probe.restore_error
echo restore_verify_error = 
p/x g_ps_hw6_adp5360_profile_probe.restore_verify_error

echo \n--- final restored state ---\n
echo final_fault_status = 
p/x g_ps_hw6_adp5360_profile_probe.final_fault_status
echo final_fault_error = 
p/x g_ps_hw6_adp5360_profile_probe.final_fault_error
echo final_fault_value = 
p/x g_ps_hw6_adp5360_profile_probe.final_fault_value
echo final_fault_clear = 
p/x g_ps_hw6_adp5360_profile_probe.final_fault_clear
echo final_pgood_status = 
p/x g_ps_hw6_adp5360_profile_probe.final_pgood_status
echo final_pgood_error = 
p/x g_ps_hw6_adp5360_profile_probe.final_pgood_error
echo final_pgood_value = 
p/x g_ps_hw6_adp5360_profile_probe.final_pgood_value
echo final_vbus_absent = 
p/x g_ps_hw6_adp5360_profile_probe.final_vbus_absent

if g_ps_hw6_adp5360_profile_probe.success == 1
  echo \nPASS: candidate matched and all tested registers restored exactly.\n
else
  if g_ps_hw6_adp5360_profile_probe.skipped == 1
    echo \nBLOCKED SAFELY: no candidate writes were permitted.\n
  else
    echo \nFAIL: candidate or restoration evidence is incomplete.\n
  end
end
echo === END HW6 FW0 ADP5360 REVERSIBLE PROFILE PROBE ===\n
