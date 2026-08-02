# HardFault quick dump helper for in-session sourcing.
# Usage:
#   source debug_hardfault.gdb
#   ps_hardfault_dump

set pagination off
set confirm off

define ps_hardfault_dump
  printf "== HardFault dump ==\n"

  set $exc_return = (unsigned long)$lr
  set $mspv = (unsigned long)$msp
  set $pspv = (unsigned long)$psp

  if (($exc_return & 4) != 0)
    set $fault_sp = $pspv
    printf "EXC_RETURN=0x%08lx active_sp=PSP sp=0x%08lx\n", $exc_return, $fault_sp
  else
    set $fault_sp = $mspv
    printf "EXC_RETURN=0x%08lx active_sp=MSP sp=0x%08lx\n", $exc_return, $fault_sp
  end

  set $r0s = *((unsigned long*)$fault_sp + 0)
  set $r1s = *((unsigned long*)$fault_sp + 1)
  set $r2s = *((unsigned long*)$fault_sp + 2)
  set $r3s = *((unsigned long*)$fault_sp + 3)
  set $r12s = *((unsigned long*)$fault_sp + 4)
  set $lrs = *((unsigned long*)$fault_sp + 5)
  set $pcs = *((unsigned long*)$fault_sp + 6)
  set $xpsrs = *((unsigned long*)$fault_sp + 7)

  printf "stacked: r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx\n", $r0s, $r1s, $r2s, $r3s
  printf "stacked: r12=0x%08lx lr=0x%08lx pc=0x%08lx xPSR=0x%08lx\n", $r12s, $lrs, $pcs, $xpsrs

  set $cfsr = *((volatile unsigned long*)0xE000ED28)
  set $hfsr = *((volatile unsigned long*)0xE000ED2C)
  set $shcsr = *((volatile unsigned long*)0xE000ED24)
  set $mmfar = *((volatile unsigned long*)0xE000ED34)
  set $bfar = *((volatile unsigned long*)0xE000ED38)

  printf "SCB: CFSR=0x%08lx HFSR=0x%08lx SHCSR=0x%08lx MMFAR=0x%08lx BFAR=0x%08lx\n", $cfsr, $hfsr, $shcsr, $mmfar, $bfar

  printf "stacked-pc symbol: "
  info symbol $pcs

  printf "current frame:\n"
  bt
end

document ps_hardfault_dump
Dump Cortex-M HardFault context and key SCB fault registers.
Use while halted in HardFault_Handler.
end

define ps_hf
  ps_hardfault_dump
end

document ps_hf
Alias for ps_hardfault_dump.
end
