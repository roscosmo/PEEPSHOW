printf "--- HW6 display DMA and SRAM split ---\n"
printf "display driver api = %lu\n", g_ps_hw6_owner_probe.display_driver_api_version
printf "SPI3 hdmatx/GPDMA/LPDMA = %p / %p / %p\n", hspi3.hdmatx, &handle_GPDMA1_Channel0, &handle_LPDMA1_Channel0
printf "SPI3 hdmatx instance/GPDMA instance/LPDMA instance = %p / %p / %p\n", hspi3.hdmatx->Instance, handle_GPDMA1_Channel0.Instance, handle_LPDMA1_Channel0.Instance
printf "awake DMA state/error = %lu / 0x%lx\n", g_ps_hw6_owner_probe.display_dma_state_after, g_ps_hw6_owner_probe.display_dma_error_after
printf "display complete/success/status = %lu / %lu / 0x%lx\n", g_ps_hw6_owner_probe.display_complete, g_ps_hw6_owner_probe.display_success, g_ps_hw6_owner_probe.display_present_status
printf "awake txBuf address/bytes = %p / %lu\n", &txBuf, sizeof(txBuf)
printf "LPBAM arena address/bytes = %p / %lu\n", &ps_lpbam_display_payload_arena, sizeof(ps_lpbam_display_payload_arena)
printf "LPBAM queue address = %p\n", &Queue1_Q
printf "expected: hdmatx equals GPDMA, not LPDMA; txBuf is outside 0x28000000..0x28003fff; LPBAM arena and queue are inside that range\n"
printf "--- end HW6 display DMA and SRAM split ---\n"
