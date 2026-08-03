set pagination off

set var g_ps_hw6_owner_sm_start_request = 1

tbreak PS_HW6_RTOS_OwnerEntry
commands
  silent
  set var g_ps_hw6_owner_sm_start_request = 1
  printf "HW6 baseline stabilization plus two owner lifecycle cycles requested after RTOS owner startup.\n"
  printf "PWR_DBG is high only while the bounded workflow runs.\n"
  printf "Expected physical result: three display-card presents and three ~750 ms tones total.\n"
  continue
end

printf "HW6 owner lifecycle start requested and startup fallback armed. Continue the target.\n"
