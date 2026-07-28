set pagination off
set confirm off
target extended-remote localhost:61234
monitor halt
p/x g_ps_phase3_audio_probe
x/96xb &g_ps_phase3_audio_probe
p/x hsai_BlockA1.State
p/x hsai_BlockA1.ErrorCode
p/x hsai_BlockA1.hdmatx
p/x handle_GPDMA1_Channel3.State
p/x handle_GPDMA1_Channel3.ErrorCode
p/x SAI1_Block_A->CR1
p/x SAI1_Block_A->CR2
p/x SAI1_Block_A->FRCR
p/x SAI1_Block_A->SLOTR
p/x SAI1_Block_A->SR
p/x GPDMA1_Channel3->CCR
p/x GPDMA1_Channel3->CBR1
p/x GPDMA1_Channel3->CSR
p/x GPIOC->ODR
detach
quit
