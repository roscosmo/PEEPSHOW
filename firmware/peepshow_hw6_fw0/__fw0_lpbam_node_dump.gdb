set pagination off
printf "--- HW6 LPBAM DMA/SPI framing dump ---\n"
printf "queue head/first/node count = %p / %p / %u\n", Queue1_Q.Head, Queue1_Q.FirstCircularNode, Queue1_Q.NodeNumber
printf "SPI3 live CR1/CR2/CFG1/CFG2/AUTOCR/SR = 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x\n", hspi3.Instance->CR1, hspi3.Instance->CR2, hspi3.Instance->CFG1, hspi3.Instance->CFG2, hspi3.Instance->AUTOCR, hspi3.Instance->SR
printf "SPI3 decoded CFG1.MBR=%u CFG2.SSOE/SSIOP/SSOM=%u/%u/%u AUTOCR=0x%08x\n", (hspi3.Instance->CFG1 >> 28) & 7, (hspi3.Instance->CFG2 >> 29) & 1, (hspi3.Instance->CFG2 >> 28) & 1, (hspi3.Instance->CFG2 >> 30) & 1, hspi3.Instance->AUTOCR
printf "SPI3 HAL Init Direction/Prescaler/SSIdleness = 0x%08x / 0x%08x / 0x%08x\n", hspi3.Init.Direction, hspi3.Init.BaudRatePrescaler, hspi3.Init.MasterSSIdleness
printf "LPTIM1 CR/CFGR/CCMR1/ARR/CMP = 0x%08x / 0x%08x / 0x%08x / %u / %u\n", hlptim1.Instance->CR, hlptim1.Instance->CFGR, hlptim1.Instance->CCMR1, hlptim1.Instance->ARR, hlptim1.Instance->CCR1
printf "cursor chunk len = %u / %u / %u / %u\n", ps_lpbam_display_tx_len[0][0], ps_lpbam_display_tx_len[1][0], ps_lpbam_display_tx_len[2][0], ps_lpbam_display_tx_len[3][0]
printf "descriptor staged SPI writes, chunk 0 each frame: pReg0=CR1 disable, pReg1=CR2 TSIZE, pReg2=IFCR clear, pReg3=CR1 enable, pReg4=CR1 CSTART\n"
set $f = 0
while $f < 4
  set $d = &Queue1_Q_DisplayBuf_Desc[$f][0]
  set $cr2 = $d->pReg[1]
  set $cfg2_live = hspi3.Instance->CFG2
  printf "frame %u regs disable/tsize/ifcr/enable/start = 0x%08x / 0x%08x / 0x%08x / 0x%08x / 0x%08x\n", $f, $d->pReg[0], $d->pReg[1], $d->pReg[2], $d->pReg[3], $d->pReg[4]
  printf "frame %u decoded TSIZE=%u enable.SPE=%u start.CSTART=%u live CFG2 SSOE/SSIOP/SSOM=%u/%u/%u\n", $f, $cr2 & 0xffff, $d->pReg[3] & 1, ($d->pReg[4] >> 9) & 1, ($cfg2_live >> 29) & 1, ($cfg2_live >> 28) & 1, ($cfg2_live >> 30) & 1
  set $f = $f + 1
end
printf "node fields: ctr1 ctr2 cbr1 csar cdar cllr; bndt=CBR1[15:0], trigm=CTR2[15:14], trigsel=CTR2[21:16], trigpol=CTR2[25:24]\n"
set $node = Queue1_Q.Head
set $i = 0
while ($node != 0) && ($i < Queue1_Q.NodeNumber) && ($i < 30)
  set $ctr1 = ((DMA_NodeTypeDef *)$node)->LinkRegisters[0]
  set $ctr2 = ((DMA_NodeTypeDef *)$node)->LinkRegisters[1]
  set $cbr1 = ((DMA_NodeTypeDef *)$node)->LinkRegisters[2]
  set $csar = ((DMA_NodeTypeDef *)$node)->LinkRegisters[3]
  set $cdar = ((DMA_NodeTypeDef *)$node)->LinkRegisters[4]
  set $cllr = ((DMA_NodeTypeDef *)$node)->LinkRegisters[5]
  set $nodeinfo = ((DMA_NodeTypeDef *)$node)->NodeInfo
  set $bndt = $cbr1 & 0xffff
  set $req = $ctr2 & 0x7f
  set $trigm = ($ctr2 >> 14) & 0x3
  set $trigsel = ($ctr2 >> 16) & 0x3f
  set $trigpol = ($ctr2 >> 24) & 0x3
  set $sdw = $ctr1 & 0x3
  set $ddw = ($ctr1 >> 16) & 0x3
  printf "node %02u @%p info=0x%08x ctr1=0x%08x ctr2=0x%08x cbr1=0x%08x csar=%p cdar=%p cllr=0x%08x bndt=%u req=%u width=%u/%u trig=%u/%u/%u\n", $i, $node, $nodeinfo, $ctr1, $ctr2, $cbr1, (void *)$csar, (void *)$cdar, $cllr, $bndt, $req, $sdw, $ddw, $trigm, $trigsel, $trigpol
  set $next = ($cllr & 0x0000fffc) | ((unsigned int)$node & 0xffff0000)
  if ($next == (unsigned int)$node)
    set $node = 0
  else
    set $node = (DMA_NodeTypeDef *)$next
  end
  set $i = $i + 1
end
printf "expected per 183-byte frame: one triggered config group, then bndt=180 width=2/2 body and bndt=3 width=0/0 tail under the same SPI TSIZE/CSTART.\n"
printf "NSS focus: CFG2 SSOE=1 means hardware NSS output, SSIOP=1 means active high, SSOM=0 means NSS pulse disabled.\n"
printf "--- end HW6 LPBAM DMA/SPI framing dump ---\n"
