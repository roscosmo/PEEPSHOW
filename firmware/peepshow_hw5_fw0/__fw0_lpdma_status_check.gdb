set pagination off
set confirm off
target remote localhost:61234
monitor halt
info registers
p/x g_ps_phase1_display_invert_probe
p/x hspi3.State
p/x hspi3.ErrorCode
p/x hspi3.Init
p/x handle_LPDMA1_Channel0.State
p/x handle_LPDMA1_Channel0.ErrorCode
p/x handle_LPDMA1_Channel0.Init
p/x hspi3.hdmatx
p/x hspi3.hdmatx->State
p/x hspi3.hdmatx->ErrorCode
p/x SPI3->CR1
p/x SPI3->CR2
p/x SPI3->SR
p/x SPI3->CFG1
p/x SPI3->CFG2
p/x LPDMA1_Channel0->CCR
p/x LPDMA1_Channel0->CTR1
p/x LPDMA1_Channel0->CTR2
p/x LPDMA1_Channel0->CBR1
p/x LPDMA1_Channel0->CSR
detach
quit
