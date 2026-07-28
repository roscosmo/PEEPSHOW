set pagination off
delete breakpoints
break HAL_PWREx_EnterSTOP2Mode
commands
silent
printf "\n=== FW0 PHASE6: HALTED AT STOP2 ENTRY ===\n"
printf "The target is stopped at HAL_PWREx_EnterSTOP2Mode(), before WFI enters STOP2.\n"
printf "Prints below are the last software-visible state before debugger contact may be lost.\n"
printf "Type 'continue' only when you want it to enter STOP2 and disconnect.\n\n"
printf "--- stage ---\n"
source __fw0_probe_phase6_stage_prints.gdb
printf "\n--- nina ---\n"
source __fw0_probe_phase6_nina_prints.gdb
printf "\n--- devices ---\n"
source __fw0_probe_phase6_devices_prints.gdb
printf "\n--- mcu/peripherals ---\n"
source __fw0_probe_phase6_mcu_prints.gdb
printf "\n--- lpbam display ---\n"
source __fw0_probe_phase6_lpbam_prints.gdb
printf "\n=== END PRE-STOP2 SNAPSHOT ===\n"
end
printf "Armed breakpoint at HAL_PWREx_EnterSTOP2Mode. Now continue, then press Start.\n"
