set pagination off

printf "=== HW6 FW0 HARDFAULT / FAULT CONTEXT PRINT ===\n"
printf "Use while halted in HardFault_Handler, MemManage_Handler, BusFault_Handler, UsageFault_Handler, or Default_Handler. This script is read-only.\n"

printf "\n--- current CPU context ---\n"
printf "pc/lr/sp/xpsr/control/active = 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / %u\n", $pc, $lr, $sp, $xpsr, $control, $xpsr & 0x1ff
printf "msp/psp/primask/basepri/faultmask = 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x\n", $msp, $psp, $primask, $basepri, $faultmask
info symbol $pc
info symbol $lr
bt

printf "\n--- SCB fault registers ---\n"
printf "ICSR  = 0x%08x  active exception = %u\n", *(uint32_t*)0xE000ED04, (*(uint32_t*)0xE000ED04) & 0x1ff
printf "SHCSR = 0x%08x\n", *(uint32_t*)0xE000ED24
printf "CFSR  = 0x%08x  MMFSR=0x%02x BFSR=0x%02x UFSR=0x%04x\n", *(uint32_t*)0xE000ED28, (*(uint32_t*)0xE000ED28) & 0xff, ((*(uint32_t*)0xE000ED28) >> 8) & 0xff, ((*(uint32_t*)0xE000ED28) >> 16) & 0xffff
printf "HFSR  = 0x%08x\n", *(uint32_t*)0xE000ED2C
printf "DFSR  = 0x%08x\n", *(uint32_t*)0xE000ED30
printf "MMFAR = 0x%08x\n", *(uint32_t*)0xE000ED34
printf "BFAR  = 0x%08x\n", *(uint32_t*)0xE000ED38
printf "AFSR  = 0x%08x\n", *(uint32_t*)0xE000ED3C
printf "CCR   = 0x%08x\n", *(uint32_t*)0xE000ED14

printf "\n--- decoded CFSR bits ---\n"
set $cfsr = *(uint32_t*)0xE000ED28
printf "MMFSR IACCVIOL/DACCVIOL/MUNSTKERR/MSTKERR/MLSPERR/MMARVALID = %u %u %u %u %u %u\n", ($cfsr >> 0) & 1, ($cfsr >> 1) & 1, ($cfsr >> 3) & 1, ($cfsr >> 4) & 1, ($cfsr >> 5) & 1, ($cfsr >> 7) & 1
printf "BFSR  IBUSERR/PRECISERR/IMPRECISERR/UNSTKERR/STKERR/LSPERR/BFARVALID = %u %u %u %u %u %u %u\n", ($cfsr >> 8) & 1, ($cfsr >> 9) & 1, ($cfsr >> 10) & 1, ($cfsr >> 11) & 1, ($cfsr >> 12) & 1, ($cfsr >> 13) & 1, ($cfsr >> 15) & 1
printf "UFSR  UNDEFINSTR/INVSTATE/INVPC/NOCP/STKOF/UNALIGNED/DIVBYZERO = %u %u %u %u %u %u %u\n", ($cfsr >> 16) & 1, ($cfsr >> 17) & 1, ($cfsr >> 18) & 1, ($cfsr >> 19) & 1, ($cfsr >> 20) & 1, ($cfsr >> 24) & 1, ($cfsr >> 25) & 1

printf "\n--- exception stacked frame ---\n"
if ($lr & 4)
  set $fault_sp = $psp
  printf "stack used before exception = PSP\n"
else
  set $fault_sp = $msp
  printf "stack used before exception = MSP\n"
end
printf "fault_sp = 0x%08x\n", $fault_sp
x/8wx $fault_sp
printf "stacked r0/r1/r2/r3/r12/lr/pc/xpsr = 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x\n", *(uint32_t*)($fault_sp + 0), *(uint32_t*)($fault_sp + 4), *(uint32_t*)($fault_sp + 8), *(uint32_t*)($fault_sp + 12), *(uint32_t*)($fault_sp + 16), *(uint32_t*)($fault_sp + 20), *(uint32_t*)($fault_sp + 24), *(uint32_t*)($fault_sp + 28)
set $stacked_pc = *(uint32_t*)($fault_sp + 24)
set $stacked_lr = *(uint32_t*)($fault_sp + 20)
printf "stacked pc symbol: "
info symbol $stacked_pc
printf "stacked lr symbol: "
info symbol $stacked_lr
if (($stacked_pc >= 0x08000000) && ($stacked_pc < 0x08200000))
  printf "instructions at stacked pc:\n"
  x/8i $stacked_pc - 12
else
  printf "stacked pc is outside flash, disassembly skipped.\n"
end

printf "\n--- MSP frame candidate ---\n"
x/12wx $msp
printf "MSP candidate r0/r1/r2/r3/r12/lr/pc/xpsr = 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x\n", *(uint32_t*)($msp + 0), *(uint32_t*)($msp + 4), *(uint32_t*)($msp + 8), *(uint32_t*)($msp + 12), *(uint32_t*)($msp + 16), *(uint32_t*)($msp + 20), *(uint32_t*)($msp + 24), *(uint32_t*)($msp + 28)
printf "MSP candidate pc symbol: "
set $msp_pc = *(uint32_t*)($msp + 24)
info symbol $msp_pc

printf "\n--- PSP frame candidate + surrounding stack ---\n"
x/24wx $psp
printf "PSP candidate r0/r1/r2/r3/r12/lr/pc/xpsr = 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x\n", *(uint32_t*)($psp + 0), *(uint32_t*)($psp + 4), *(uint32_t*)($psp + 8), *(uint32_t*)($psp + 12), *(uint32_t*)($psp + 16), *(uint32_t*)($psp + 20), *(uint32_t*)($psp + 24), *(uint32_t*)($psp + 28)
printf "PSP candidate pc symbol: "
set $psp_pc = *(uint32_t*)($psp + 24)
info symbol $psp_pc

printf "\n--- owner workflow breadcrumb ---\n"
set $sm = &g_ps_hw6_owner_sm_probe
printf "owner magic/version/phase complete/success = 0x%x / 0x%x / 0x%x  %u / %u\n", $sm->magic, $sm->version, $sm->phase, $sm->complete, $sm->success
printf "owners required/completed/success/failure = 0x%x / 0x%x / 0x%x / 0x%x\n", $sm->required_owner_mask, $sm->completed_owner_mask, $sm->success_owner_mask, $sm->failure_owner_mask
printf "owner action start/end/status storage = %u / %u / 0x%x\n", $sm->owner_action_start_tick[5], $sm->owner_action_end_tick[5], $sm->owner_action_status[5]
printf "FSM storage/flash current = %u / %u\n", $sm->current_state[7], $sm->current_state[8]
printf "AT25 scratch status/address/len = %u / 0x%08x / %u\n", $sm->flash_scratch_status, $sm->flash_scratch_address, $sm->flash_scratch_length
printf "AT25 scratch cleanup mismatch/OSPI = %u / 0x%x / 0x%x\n", $sm->flash_scratch_cleanup_blank_mismatch_count, $sm->flash_scratch_ospi_state_after, $sm->flash_scratch_ospi_error_after
printf "block API/init/ops/last = %u / %u / %u / %u\n", $sm->flash_block_api_version, $sm->flash_block_init_status, $sm->flash_block_operation_count, $sm->flash_block_last_status
printf "block test status/index/address/len = %u / %u / 0x%08x / %u\n", $sm->flash_block_test_status, $sm->flash_block_test_index, $sm->flash_block_test_address, $sm->flash_block_test_length
printf "block erase/blank/program/verify/cleanup = %u / %u / %u / %u / %u\n", $sm->flash_block_erase_status, $sm->flash_block_blank_read_status, $sm->flash_block_program_status, $sm->flash_block_verify_read_status, $sm->flash_block_cleanup_status

printf "\n--- storage block private state ---\n"
printf "ps_flash_block addr/api/init/ops/last = 0x%08x / %u / %u / %u / %u\n", &ps_flash_block, ps_flash_block.api_version, ps_flash_block.initialized, ps_flash_block.operation_count, ps_flash_block.last_status
printf "ps_flash_block flash ptr/geometry = 0x%08x / %u / %u / %u / %u\n", ps_flash_block.flash, ps_flash_block.geometry.total_size, ps_flash_block.geometry.erase_block_size, ps_flash_block.geometry.program_page_size, ps_flash_block.geometry.logical_block_count
printf "block tx/rx addrs = 0x%08x / 0x%08x\n", &ps_storage_flash_block_tx, &ps_storage_flash_block_rx
printf "block tx first32:\n"
x/32xb &ps_storage_flash_block_tx
printf "block rx first32:\n"
x/32xb &ps_storage_flash_block_rx

printf "\n--- OSPI and DMA state ---\n"
printf "hospi1 State/Error/hdma/Instance = 0x%x / 0x%x / 0x%08x / 0x%08x\n", hospi1.State, hospi1.ErrorCode, hospi1.hdma, hospi1.Instance
printf "GPDMA ch4 RX State/Error/Instance = 0x%x / 0x%x / 0x%08x\n", handle_GPDMA1_Channel4.State, handle_GPDMA1_Channel4.ErrorCode, handle_GPDMA1_Channel4.Instance
printf "GPDMA ch5 TX State/Error/Instance = 0x%x / 0x%x / 0x%08x\n", handle_GPDMA1_Channel5.State, handle_GPDMA1_Channel5.ErrorCode, handle_GPDMA1_Channel5.Instance

printf "\n--- recent owner transition trace ---\n"
set $trace_count = $sm->trace_count
set $trace_index = ($sm->trace_write_index + 128 - $trace_count) % 128
set $i = 0
while $i < $trace_count
  printf "%u %u %u %u %u %x\n", $sm->trace[$trace_index].tick, $sm->trace[$trace_index].state_machine_id, $sm->trace[$trace_index].from_state, $sm->trace[$trace_index].event, $sm->trace[$trace_index].to_state, $sm->trace[$trace_index].action_status
  set $trace_index = ($trace_index + 1) % 128
  set $i = $i + 1
end

printf "=== END HW6 FW0 HARDFAULT / FAULT CONTEXT PRINT ===\n"