;/*****************************************************************************/
;/* OSasm.s: low-level OS commands, written in assembly                       */
;/* derived from uCOS-II                                                      */
;/*****************************************************************************/
;Jonathan Valvano, Fixed Scheduler, 8/10/14


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
        EXTERN  NodePt           ; linked list of treads to run
		EXTERN  OS_SleepCheck	;
		EXTERN	OS_SleepClear
		EXTERN TempTCBPt
		;EXTERN	killSwitch		; PendSV skip saving the conteext?
		;EXTERN  SNodePt
		;EXTERN	PeriodicNode
        ;EXTERN  sys
        EXPORT  OS_Launch
        EXPORT  SysTick_Handler
        EXPORT  PendSV_Handler
		;EXPORT  Timer0A_Handler

TIMER0_ICR_R       EQU 0x40030024
TIMER_ICR_TATOCINT EQU 0x00000001   ; GPTM TimerA Time-Out Raw
NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; PendSV priority register (position 14).
NVIC_SYSPRI15   EQU     0xE000ED23                              ; Systick priority register (position 15).
NVIC_LEVEL14    EQU           0xEF                              ; Systick priority value (second lowest).
NVIC_LEVEL15    EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.
NVIC_ST_RELOAD  EQU     0xE000E014
NVIC_ST_CURRENT EQU     0xE000E018


       BX      LR            ; yes – we successfully incremented it


OS_Launch
    LDR     R0, =RunPt         ; currently running thread
    LDR     R2, [R0]		   ; R2 = value of RunPt
    LDR     SP, [R2]           ; new thread SP; SP = RunPt->stackPointer;
    POP     {R4-R11}           ; restore regs r4-11 
    POP     {R0-R3}            ; restore regs r0-3 
    POP     {R12}
    POP     {LR}               ; discard LR from initial stack
    POP     {LR}               ; start location
    POP     {R1}               ; discard PSR
    CPSIE   I                  ; Enable interrupts at processor level
    BX      LR                 ; start first thread

OSStartHang
    B       OSStartHang        ; Should never get here



;********************************************************************************************************


SysTick_Handler
    CPSID   I                  ; Prevent interruption during context switch
	
	;check if this context needs to be saved at all
	LDR		R0, =RunPt
	LDR		R0, [R0]		; RunPt Val
	LDR		R1, [R0,#4]		; RunPt->tempSP
	ADDS	R1,#0				;set cc
	BEQ		SysTick_SaveContext		    ; if it's zero then continue normally
	LDR		R0, =TempTCBPt		;temptcbpt address
	LDR		R1,[R0]				;TempTCBPt val
	LDR		SP,[R1]				;SP = TempTCBPt->SP
	B		SleepCheck							;other wise 1 means to skip pushing onto the stack
							


SysTick_SaveContext
	
    PUSH    {R4-R11}           ; Save remaining regs r4-11
    LDR     R0, =RunPt         ; R0=pointer to RunPt, old thread
    LDR     R1, [R0]		   ; RunPt->stackPointer = SP;
    STR     SP, [R1]           ; save SP of process being switched out
	
	;context saved
	;check the sleep

SleepCheck
	LDR     R0, =RunPt         ; R0=pointer to RunPt, old thread
    LDR     R1, =NodePt
    LDR     R2, [R1]           ; NodePt
    LDR     R2, [R2]           ; next to run
	
    STR     R2, [R1]           ; NodePt= NodePt->Next;
    LDR     R3, [R2,#4]        ; R3= NodePt->Next->Thread;// which thread
    STR     R3, [R0]			; RunPt = NodePt->Next->ThreadPt
   
   PUSH{LR}
	BL		OS_SleepCheck
   POP{LR}
	ADDS	R0,#0					; -1 still sleeping
    
	BMI		SleepCheck		   ;  Negative still sleeping
	BEQ		SkipSleepClear	   ;  0: OS Sleep was 0, skip updating the sleep
	PUSH{LR}
	BL		OS_SleepClear
	POP{LR}

SkipSleepClear	
	; reload SYStiCK Reload time
	LDR 	R2, [R1]		   ;
    LDR     R2, [R2,#8]		   ; NodePt->TimeSlice
    SUB     R2, #50            ; subtract off time to run this ISR
    LDR     R1, =NVIC_ST_RELOAD
   ; STR     R2, [R1]           ; how long to run next thread
    LDR     R1, =NVIC_ST_CURRENT
    STR     R2, [R1]           ; write to current, clears it
	
	LDR		R1, =RunPt
	LDR		R3, [R1]		   ; R3 = RunPt Val
	;LDR     R3, [R2]        ; 	R3 = RunPt->StackPt
    LDR     SP, [R3]           ; new thread SP; SP = RunPt->stackPointer;
	
	
    POP     {R4-R11}           ; restore regs r4-11 

    CPSIE   I				   ; tasks run with I=0
    BX      LR                 ; Exception return will restore remaining context   
	
	
	

	
PendSV_Handler
	CPSID   I                  ; Prevent interruption during context switch
;check if this context needs to be saved at all
	LDR		R0, =RunPt
	LDR		R0, [R0]		; RunPt Val
	LDR		R1, [R0,#4]		; RunPt->tempSP
	ADDS	R1,#0				;set cc
	BEQ		PendSV_SaveContext		    ; if it's zero then continue normally
	LDR		R0, =TempTCBPt		;temptcbpt address
	LDR		R1,[R0]				;TempTCBPt val
	LDR		SP,[R1]				;SP = TempTCBPt->SP
	B		SleepCheckX							;other wise 1 means to skip pushing onto the stack
							


PendSV_SaveContext
    PUSH    {R4-R11}           ; Save remaining regs r4-11
    LDR     R0, =RunPt         ; R0=pointer to RunPt, old thread
    LDR     R1, [R0]		   ; RunPt->stackPointer = SP;
    STR     SP, [R1]           ; save SP of process being switched out
	
	;context saved
	;check the sleep

SleepCheckX
	LDR     R0, =RunPt         ; R0=pointer to RunPt, old thread
    LDR     R1, =NodePt
    LDR     R2, [R1]           ; NodePt
    LDR     R2, [R2]           ; next to run
	
    STR     R2, [R1]           ; NodePt= NodePt->Next;
    LDR     R3, [R2,#4]        ; RunPt = &sys[NodePt->Thread];// which thread
    STR     R3, [R0]
   
   PUSH{LR}
	BL		OS_SleepCheck
   POP{LR}
	ADDS	R0,#0					; -1 still sleeping
    
	BMI		SleepCheckX		   ;  Negative still sleeping
	BEQ		SkipSleepClearX	   ;  0: OS Sleep was 0, skip updating the sleep
	PUSH{LR}
	BL		OS_SleepClear
	POP{LR}

SkipSleepClearX	

	
	LDR		R1, =RunPt
	LDR		R3, [R1]		   ; R2 = RunPt
	;LDR     R3, [R2]        ; 	R3 = RunPt->StackPt
    LDR     SP, [R3]           ; new thread SP; SP = RunPt->stackPointer;
	
	
    POP     {R4-R11}           ; restore regs r4-11 

    CPSIE   I				   ; tasks run with I=0
    BX      LR                 ; Exception return will restore remaining context   

    ALIGN
    END