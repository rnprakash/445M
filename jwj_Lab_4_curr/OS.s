SysTick_Handler ;Saves r0-r3, r12, LR, PC, PSR
  CPSID I          ; disable interrupts during switch
  PUSH  {R4-R11}   ; save remaining registers 
  LDR   R0, =RunPt ;r0 = pointer to runpt, old thread
  LDR   R1, [R0]   ;r1 = runpt->next
  STR   SP, [R1]   ;save sp to TCB
  LDR   R1, [R1, #4]; r1 = runpt->next
  STR   R1, [R0]    ;runpt = R1
  LDR   SP, [R1]   ;load new thread stack pointer = runpt->sp
  pop   {R4-R11}   ;load register values for new thread
  CPSIE I ;enable interrupts
  BX    LR ;return from ISR, restores rest of registers
