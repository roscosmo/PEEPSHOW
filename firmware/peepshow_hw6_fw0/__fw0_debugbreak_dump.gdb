set pagination off
set print pretty on
printf "--- HW6 debugbreak dump ---\n"
printf "registers:\n"
info registers r0 r1 r2 r3 r12 sp lr pc xpsr
printf "fault regs CFSR/HFSR/MMFAR/BFAR = 0x%x / 0x%x / 0x%x / 0x%x\n", *(uint32_t*)0xE000ED28, *(uint32_t*)0xE000ED2C, *(uint32_t*)0xE000ED34, *(uint32_t*)0xE000ED38
printf "firmware probe magic/version/phase/complete = 0x%x / %u / 0x%x / %u\n", g_ps_hw6_fw0_probe.magic, g_ps_hw6_fw0_probe.version, g_ps_hw6_fw0_probe.phase, g_ps_hw6_fw0_probe.complete
printf "firmware error count/phase/code = %u / 0x%x / 0x%x\n", g_ps_hw6_fw0_probe.error_count, g_ps_hw6_fw0_probe.error_phase, g_ps_hw6_fw0_probe.error_code
printf "firmware assert count/line = %u / %u\n", g_ps_hw6_fw0_probe.assert_count, g_ps_hw6_fw0_probe.assert_line
p g_ps_hw6_fw0_probe.assert_file
printf "--- backtrace full ---\n"
bt full
printf "--- LPBAM STOP2 probes ---\n"
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_stop2_lpbam_prints.gdb
printf "--- automatic STOP2 probes ---\n"
source G:/PEEPSHOW/firmware/peepshow_hw6_fw0/__fw0_stop2_auto_idle_prints.gdb
printf "--- HAL_DMAEx_List_BuildNode frame 2 node config ---\n"
frame 2
p/x pNodeConfig
p/x pNode
p/x pNodeConfig->NodeType
p/x pNodeConfig->Init.Request
p/x pNodeConfig->Init.BlkHWRequest
p/x pNodeConfig->Init.Direction
p/x pNodeConfig->Init.SrcInc
p/x pNodeConfig->Init.DestInc
p/x pNodeConfig->Init.SrcDataWidth
p/x pNodeConfig->Init.DestDataWidth
p/x pNodeConfig->Init.Priority
p/x pNodeConfig->Init.Mode
p/x pNodeConfig->Init.TransferEventMode
p/x pNodeConfig->Init.SrcBurstLength
p/x pNodeConfig->Init.DestBurstLength
p/x pNodeConfig->Init.TransferAllocatedPort
p/x pNodeConfig->DataHandlingConfig.DataAlignment
p/x pNodeConfig->DataHandlingConfig.DataExchange
p/x pNodeConfig->TriggerConfig.TriggerPolarity
p/x pNodeConfig->TriggerConfig.TriggerMode
p/x pNodeConfig->TriggerConfig.TriggerSelection
p/x pNodeConfig->RepeatBlockConfig.RepeatCount
p/x pNodeConfig->RepeatBlockConfig.SrcAddrOffset
p/x pNodeConfig->RepeatBlockConfig.DestAddrOffset
p/x pNodeConfig->RepeatBlockConfig.BlkSrcAddrOffset
p/x pNodeConfig->RepeatBlockConfig.BlkDestAddrOffset
p/x pNodeConfig->SrcAddress
p/x pNodeConfig->DstAddress
p/x pNodeConfig->DataSize
frame 0
printf "--- end debugbreak dump ---\n"
