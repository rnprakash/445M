
        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8
        
        EXTERN  _RunPt            ; currently running thread
        EXTERN OS_FindNextThread  ; function should be called by pendSV handler
        EXTERN OS_BlockThread
        EXTERN OS_WakeThread
		EXTERN OS_IncPriority
        
        EXPORT OS_bWait
        EXPORT OS_bSignal
        EXPORT OS_Signal
        EXPORT OS_Wait
        EXPORT StartOS
        EXPORT SysTick_Handler
        EXPORT PendSV_Handler
        
PENDSV_R   EQU 0xE000ED04
PENDSV_SET EQU 0x10000000

; void OS_bWait(OS_SemaphoreType* s)
; acquire binary semaphore s
OS_bWait
  ; decrement the number of permits
  LDREX R1, [R0]      ; R1 = value of *s (num permits)
  SUBS R1, #1         ; R1--, set condition codes     
  STREX R2, R1, [R0]  ; loaded a non-zero value, so attempt to decrement and store
  CMP R2, #0
  BNE OS_bWait        ; repeat until successful decrement
  CMP R1, #0          ; if R1 >= 0, then have successfully acquired semaphore so can return
  IT PL
  BXPL LR
  ; block thread
  PUSH {LR}
  BL.W OS_BlockThread
  POP {LR}
  BX LR
  
; void OS_bSignal(OS_SemaphoreType* s)
; release binary semaphore s
OS_bSignal
  ; increment number of permits
  LDREX R1, [R0]
  ADD R1, R1, #1
  STREX R2, R1, [R0]
  CMP R2, #0
  BNE OS_bSignal    ; repeat until successful increment
  PUSH {LR}
  CMP R1, #0
  IT LE            ; if R1 <= 0, need to wake up a blocked thread
  BLLE OS_WakeThread
  POP {LR}
  BX LR

; void OS_Signal(OS_SemaphoreType* s)
; release a permit from semaphore s
; by incrementing value
OS_Signal
  ; increment number of permits
  LDREX R1, [R0]
  ADD R1, R1, #1
  STREX R2, R1, [R0]
  CMP R2, #0
  BNE OS_Signal    ; repeat until successful increment
  PUSH {LR}
  CMP R1, #0
  IT LE            ; if R1 <= 0, need to wake up a blocked thread
  BLLE OS_WakeThread
  POP {LR}
  BX LR
  
; void OS_Wait(OS_SemaphoreType* s)
; acquire a permit from semaphore s
; by decrementing value if greater than zero
OS_Wait
  ; decrement the number of permits
  LDREX R1, [R0]      ; R1 = value of *s (num permits)
  SUBS R1, #1         ; R1--, set condition codes     
  STREX R2, R1, [R0]  ; loaded a non-zero value, so attempt to decrement and store
  CMP R2, #0
  BNE OS_Wait        ; repeat until successful decrement
  CMP R1, #0          ; if R1 >= 0, then have successfully acquired semaphore so can return
  IT PL
  BXPL LR
  ; block thread
  PUSH {LR}
  BL.W OS_BlockThread
  POP {LR}
  BX LR
  
; void SysTick_Handler(void)
; handles switching threads
; copied exactly from starter files for purposes of lab preparation
SysTick_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
; why was CPSID and CPSIE commented out??
;  CPSID   I                  ; 2) Prevent interrupt during switch
	; trigger PendSV
	LDR		R2, =PENDSV_R	; load PendSV register
	LDR		R3, =PENDSV_SET; load trigger value
	STR		R3, [R2]			; store
;	CPSIE	I
	BX LR
	
PendSV_Handler
  CPSID	I
  PUSH    {R4-R11}           ; 3) Save remaining regs r4-11
  LDR     R0, =_RunPt        ; 4) R0=pointer to RunPt, old thread
  LDR     R1, [R0]           ; R1 = RunPt
  CMP R1, #0                 ; if RunPt is not NULL (last thread killed itself, save SP
  IT NE
  STRNE     SP, [R1]           ; 5) Save SP into TCB
  PUSH {LR}                  ; temporarily save LR to call a function
  BL.W OS_FindNextThread     ; index through blocked/sleeping threads
;  BL.W OS_IncPriority   		 ; increment priority of threads that haven't run, if necessary
  POP {LR}                   ; restore LR
  LDR     R0, =_RunPt			   ; load RunPt (new)
  LDR     R1, [R0]			     ; load RunPt (new)
  LDR     SP, [R1]           ; 7) new thread SP; SP = RunPt->sp;
  POP     {R4-R11}           ; 8) restore regs r4-11
  CPSIE   I                  ; 9) tasks run with interrupts enabled
  BX      LR                 ; 10) restore R0-R3,R12,LR,PC,PSR


StartOS
    LDR     R0, =_RunPt         ; currently running thread
    LDR     R2, [R0]           ; R2 = value of RunPt
    LDR     SP, [R2]           ; new thread SP; SP = RunPt->stackPointer;
    POP     {R4-R11}           ; restore regs r4-11
    POP     {R0-R3}            ; restore regs r0-3
    POP     {R12}
    POP     {LR}               ; discard LR from initial stack
    POP     {LR}               ; start location
    POP     {R1}               ; discard PSR
    CPSIE   I                  ; Enable interrupts at processor level
    BX      LR                 ; start first thread
  
    ALIGN
    END
    