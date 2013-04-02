;/*****************************************************************************/
;/* OSasm.s: low-level OS commands, written in assembly                       */
;// Real Time Operating System 

; This example accompanies the book
;  "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
;  ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011
;
;  Programs 6.4 through 6.12, section 6.2
;
;Copyright 2011 by Jonathan W. Valvano, valvano@mail.utexas.edu
;    You may use, edit, run or distribute this file
;    as long as the above copyright notice remains
; THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
; OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
; MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
; VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
; OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
; For more information about my classes, my research, and my books, see
; http://users.ece.utexas.edu/~valvano/
; */

        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
		EXTERN  NextRunPt 
		EXTERN  NVIC_INT_CTRL_R  ; interrupt control register to acknlowledge PendSV interrupt
        EXPORT  GetInterruptStatus
        EXPORT  StartOS
;        EXPORT  SysTick_Handler
		EXPORT  PendSV_Handler





; SysTick_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
    ; CPSID   I                  ; 2) Prevent interrupt during switch
    ; PUSH    {R4-R11}           ; 3) Save remaining regs r4-11
    ; LDR     R0, =RunPt         ; 4) R0=pointer to RunPt, old thread
    ; LDR     R1, [R0]           ;    R1 = RunPt
    ; STR     SP, [R1]           ; 5) Save SP into TCB
; Sleeping
    ; LDR     R1, [R1,#4]        ; 6) R1 = RunPt->next
	; LDR     R2, [R1, #12]      ; R2 = sleepcounter
	; CMP     R2, #0
	; BNE     Sleeping           ;if thread is asleep, try next thread
    ; STR     R1, [R0]           ;    RunPt = R1
    ; LDR     SP, [R1]           ; 7) new thread SP; SP = RunPt->sp;
    ; POP     {R4-R11}           ; 8) restore regs r4-11
    ; CPSIE   I                  ; 9) tasks run with interrupts enabled
    ; BX      LR                 ; 10) restore R0-R3,R12,LR,PC,PSR

GetInterruptStatus
    MRS R0, PRIMASK
	BX  LR

PendSV_Handler
    CPSID   I                  ; 2) Prevent interrupt during switch
    PUSH    {R4-R11}           ; 3) Save remaining regs r4-11
	LDR     R0, =0xE000ED04    ;R0 = pointer to NVIC_INT_CTRL_R to acknowledge pendSV
	LDR     R1, [R0]
	ORR     R1, R1, #0x08000000 ;unpend PendSV by writing to bit 27 of VNIC_INT_CTRL_R
	STR     R1, [R0]    ;store back new value of NVIC_INT_CTRL_R
    LDR     R0, =RunPt         ; 4) R0=pointer to RunPt, old thread
    LDR     R1, [R0]           ;    R1 = RunPt
    STR     SP, [R1]           ; 5) Save SP into TCB
;Sleeping2
;    LDR     R1, [R1,#4]        ; 6) R1 = RunPt->next
;	LDR     R2, [R1, #12]      ; R2 = sleepcounter
;	CMP     R2, #0
;	BNE     Sleeping2         ;if thread is asleep, try next thread
    LDR     R2, =NextRunPt     ;R2 = pointer to NextRunPt
	LDR     R1, [R2]           ; R1 = NextRunPt
    STR     R1, [R0]           ; RunPt = NextRunPt
    LDR     SP, [R1]           ; 7) new thread SP; SP = RunPt->sp;
    POP     {R4-R11}           ; 8) restore regs r4-11
    CPSIE   I                  ; 9) tasks run with interrupts enabled
    BX      LR                 ; 10) restore R0-R3,R12,LR,PC,PSR

StartOS
    LDR     R0, =RunPt         ; currently running thread
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