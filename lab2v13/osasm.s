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
		EXTERN  OS_RemoveCurrentThread	;removes the thread from the tcb linked list
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

PF1				EQU		0x40025008
       BX      LR            ; yes – we successfully incremented it


OS_Launch
	LDR 	R1, =NVIC_ST_RELOAD
	SUB		R0, R0, #1
	STR		R0, [R1]
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
	
	

	;testmain7 PF1 toggler for systick time analysis
	;LDR		R0, =PF1
	;MOV		R1, #2
	;STR		R1,[R0]
	
;1) check marked for deletion

	LDR		R0, =RunPt
	LDR		R1, [R0]		;Run Pt val
	LDR		R2, [R1,#4]		;RunPt->kill
	ADDS	R2, #0			;set cc
	BEQ		sh_SaveContext	;0: alive, 1: delete
	
;1) delete thread, do not save context
sh_DeleteLoop
	PUSH	{LR}
	BL		OS_RemoveCurrentThread		;NodePt = NodePt->Prev
	POP		{LR}
	LDR		R0, =RunPt						;RuntPt = RunPt->Prev
	LDR		R0, [R0]						;Old thread was freed
	LDR		SP, [R0]						;Update Stack Pointer to RunPt->Prev->SP
	
	;1) check marked for deletion again
	LDR		R0, =RunPt
	LDR		R1, [R0]		;Run Pt val
	LDR		R2, [R1,#4]		;RunPt->kill
	ADDS	R2, #0			;set cc
	
	BEQ		sh_SleepCheckLoop	;0: alive, 1: delete.
	B		sh_DeleteLoop		;kill = 1,  Loop again, needs deletion
	
	
;1) alive thread, save context
sh_SaveContext
	PUSH    {R4-R11}           ; Save remaining regs r4-11
    LDR     R0, =RunPt         ; R0=pointer to RunPt, old thread
    LDR     R1, [R0]		   ; RunPt->stackPointer = SP;
    STR     SP, [R1]           ; save SP of process being switched out

;sh_NoSaveContext
;2) Find the Next Thread that is not sleeping
sh_SleepCheckLoop	
	LDR     R0, =RunPt         ; R0=pointer to RunPt, current thread
    LDR     R1, =NodePt
    LDR     R2, [R1]           ; R2=NodePt
    LDR     R2, [R2]           ; R2=NodePt->Next
	
    STR     R2, [R1]           ; NodePt = NodePt->Next;
    LDR     R3, [R2,#4]        ; R3= NodePt->Next->Thread;// which thread
    STR     R3, [R0]		   ; RunPt = NodePt->Next->ThreadPt
		
	PUSH{LR}					;save LR
	LDR		R0, =NodePt			
	LDR		R0,[R0]
	BL		OS_SleepCheck		;Check if sleep > OS_Time
	POP{LR}
	ADDS	R0,#0				; -1 still sleeping
    
	BMI		sh_SleepCheckLoop	;  Negative = still sleeping so load next TCB
	BEQ		sh_SkipSleepClear	;  0: OS Sleep was 0, skip updating the sleep
	PUSH{LR}
	LDR		R0, =NodePt			
	LDR		R0,[R0]
	;BL		OS_SleepClear		; Set NodePt->Sleep to 0, it is awake
	POP{LR}

sh_SkipSleepClear
;3) clear SYSTICK CURRENT
;	LDR 	R2, [R1]		   ;
;    LDR     R2, [R2,#8]		   ; NodePt->TimeSlice
;    SUB     R2, #50            ; subtract off time to run this ISR
;    LDR     R1, =NVIC_ST_RELOAD
   ; STR     R2, [R1]           ; how long to run next thread
    LDR     R1, =NVIC_ST_CURRENT
	MOV		R2, #20
    STR     R2, [R1]           ; write to current, clears it
	
;4) load context of next thread
	LDR		R1, =RunPt
	LDR		R3, [R1]		   ; R3 = RunPt Val
    LDR     SP, [R3]           ; new thread SP; SP = RunPt->stackPointer;
	
	
	;testmain7 PF1
	;LDR		R0, =PF1
	;MOV		R1, #0
	;STR		R1,[R0]
	
    POP     {R4-R11}           ; restore regs r4-11 

    CPSIE   I				   ; tasks run with I=0
    BX      LR                 ; Exception return will restore remaining context   
	
	
	
	
	

	
PendSV_Handler
    CPSID   I                  ; Prevent interruption during context switch
	
	
;1) check marked for deletion
	LDR		R0, =RunPt
	LDR		R1, [R0]		;Run Pt val
	LDR		R2, [R1,#4]		;RunPt->kill
	ADDS	R2, #0			;set cc
	BEQ		psv_SaveContext	;0: alive, 1: delete
	
;1) delete thread, do not save context
psv_DeleteLoop
	PUSH	{LR}
	BL		OS_RemoveCurrentThread		;NodePt = NodePt->Prev
	POP		{LR}
	LDR		R0, =RunPt						;RuntPt = RunPt->Prev
	LDR		R0, [R0]						;Old thread was freed
	LDR		SP, [R0]						;Update Stack Pointer to RunPt->Prev->SP
	
	;1) check marked for deletion again
	LDR		R0, =RunPt
	LDR		R1, [R0]		;Run Pt val
	LDR		R2, [R1,#4]		;RunPt->kill
	ADDS	R2, #0			;set cc
	
	BEQ		psv_SleepCheckLoop	;0: alive, 1: delete.
	B		psv_DeleteLoop		;kill = 1,  Loop again, needs deletion
	
	
;1) alive thread, save context
psv_SaveContext
	PUSH    {R4-R11}           ; Save remaining regs r4-11
    LDR     R0, =RunPt         ; R0=pointer to RunPt, old thread
    LDR     R1, [R0]		   ; RunPt->stackPointer = SP;
    STR     SP, [R1]           ; save SP of process being switched out

;psv_NoSaveContext
;2) Find the Next Thread that is not sleeping
psv_SleepCheckLoop	
	LDR     R0, =RunPt         ; R0=pointer to RunPt, current thread
    LDR     R1, =NodePt
    LDR     R2, [R1]           ; R2=NodePt
    LDR     R2, [R2]           ; R2=NodePt->Next
	
    STR     R2, [R1]           ; NodePt = NodePt->Next;
    LDR     R3, [R2,#4]        ; R3= NodePt->Next->Thread;// which thread
    STR     R3, [R0]		   ; RunPt = NodePt->Next->ThreadPt
		
	PUSH{LR}					;save LR
	LDR		R0, =NodePt			
	LDR		R0,[R0]
	BL		OS_SleepCheck		;Check if sleep > OS_Time
	POP{LR}
	ADDS	R0,#0				; -1 still sleeping
    
	BMI		psv_SleepCheckLoop	;  Negative = still sleeping so load next TCB
	BEQ		psv_SkipSleepClear	;  0: OS Sleep was 0, skip updating the sleep
	PUSH{LR}
	LDR		R0, =NodePt			
	LDR		R0,[R0]
;	BL		OS_SleepClear		; Set NodePt->Sleep to 0, it is awake
	POP{LR}

psv_SkipSleepClear
;3) clear SYSTICK CURRENT
;	LDR 	R2, [R1]		   ;
;    LDR     R2, [R2,#8]		   ; NodePt->TimeSlice
;    SUB     R2, #50            ; subtract off time to run this ISR
;    LDR     R1, =NVIC_ST_RELOAD
   ; STR     R2, [R1]           ; how long to run next thread
    LDR     R1, =NVIC_ST_CURRENT
	MOV		R2, #20
    STR     R2, [R1]           ; write to current, clears it
	
;4) load context of next thread
	LDR		R1, =RunPt
	LDR		R3, [R1]		   ; R3 = RunPt Val
    LDR     SP, [R3]           ; new thread SP; SP = RunPt->stackPointer;
	
    POP     {R4-R11}           ; restore regs r4-11 

    CPSIE   I				   ; tasks run with I=0
    BX      LR                 ; Exception return will restore remaining context   
	



    ALIGN
    END