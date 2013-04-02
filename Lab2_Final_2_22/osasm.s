
        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8
        
        EXTERN  _RunPt            ; currently running thread
        EXTERN OS_FindNextThread  ; function should be called by pendSV handler
		EXTERN _threads
        
        EXPORT OS_bWait
        EXPORT OS_bSignal
        EXPORT OS_Signal
        EXPORT OS_Wait
        EXPORT StartOS
        EXPORT SysTick_Handler
        EXPORT PendSV_Handler
		EXPORT OS_InitMemory
        
PENDSV_R   EQU 0xE000ED04
NVIC_MPU_NUMBER_R EQU 0xE000ED98
PENDSV_SET EQU 0x10000000

; void OS_bWait(OS_SemaphoreType* s)
; acquire binary semaphore s
OS_bWait
  MOV R1, #0      ; R1 = value to be written to semaphore to indicate taken
  LDREX R2, [R0]  ; R2 = value of *s (exclusive)
  CMP R2, #1
  ITT EQ
  STREXEQ R2, R1, [R0]  ; attempts to store R1 (0) to s
  CMPEQ R2, #0          ; if no other process in the system has accessed the memory location
  BNE OS_bWait          ; since LDREX, R2 = 0 (success), else R2 = 1 (fail)
  BX LR
  
; void OS_bSignal(OS_SemaphoreType* s)
; release binary semaphore s
OS_bSignal
  MOV R1, #1    ; R1 = value indicating released semaphore
  STR R1, [R0]  ; TODO: wakeup blocked threads?
  BX LR

; void OS_Signal(OS_SemaphoreType* s)
; release a permit from semaphore s
; by incrementing value
OS_Signal
  LDREX R1, [R0]      ; R1 = value of *s (num permits)
  ADD R1, R1, #1      ; R1++
  STREX R2, R1, [R0]  ; attempt mutex store
  CMP R2, #0          ; R2 = 0 -> success
  BNE OS_Signal       ; R2 = 1 -> failure, retry
  BX LR               ; TODO: wakeup blocked threads?
  
; void OS_Wait(OS_SemaphoreType* s)
; acquire a permit from semaphore s
; by decrementing value if greater than zero
OS_Wait
  LDREX R1, [R0]      ; R1 = value of *s (num permits)
  SUBS R1, #1         ; R1--, set condition codes
  ITT PL              ; if R1 >= 0       
  STREXPL R2, R1, [R0]; loaded a non-zero value, so attempt to decrement and store
  CMPPL R2, #0        ; attempt to update the number of permits
  BNE OS_Wait         ; retry from the beginning if store failed
  BX LR
  
; void SysTick_Handler(void)
; handles switching threads
; copied exactly from starter files for purposes of lab preparation
SysTick_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
	; why was CPSID and CPSIE commented out??
	CPSID   I                  ; 2) Prevent interrupt during switch
	; trigger PendSV
	LDR		R2, =PENDSV_R	; load PendSV register
	LDR		R3, =PENDSV_SET; load trigger value
	STR		R3, [R2]			; store
	CPSIE	I
	BX LR
	
PendSV_Handler
	CPSID	I
	PUSH    {R4-R11}           ; 3) Save remaining regs r4-11
	LDR     R0, =_RunPt        ; 4) R0=pointer to RunPt, old thread
	LDR     R1, [R0]           ; R1 = RunPt
	STR     SP, [R1]           ; 5) Save SP into TCB
	PUSH {LR}                  ; temporarily save LR to call a function
	BL.W OS_FindNextThread     ; index through blocked/sleeping threads
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

OS_InitMemory
	DMB								; memory barrier
	LDR		R0, =NVIC_MPU_CTRL_R
	MOV		R1, #1
	STR		R1, [R0, #0x00]
	MOV		R1, #0
	STR		R1, [R0, #0x04]		 	; Region 0
	LDR		R2, =_threads
	STR		R2, [R0, #0x08] 		; Base address and enable
	MOV		R3, #13					; 4KB size
	MOV		R4, #1					; Privileged r/w only
	STRH	R3, [R0, #0x0C]			; Store half-word for size bits
	STRH	R4, [R0, #0x0E]			; Store half-word for attribute bits
	BX		LR

    ALIGN
    END
