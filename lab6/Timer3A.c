// Timer3.c
// Runs on LM4F120/TM4C123
// Use Timer3 in 32-bit periodic mode to request interrupts at a periodic rate
// Daniel Valvano
// March 20, 2014

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013
  Program 7.5, example 7.6
 Copyright 2013 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

#include "tm4c123gh6pm.h"
#include "OS.h"
#include "UART.h"

extern NodeType *PeriodicPt; 

long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value

#define RELOAD (TIME_1MS/2)
// ***************** Timer3_Init ****************
// Activate Timer3 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer3A_Init(void){
	long status = StartCritical();
  SYSCTL_RCGCTIMER_R |= 0x08;   // 0) activate TIMER3
  TIMER3_CTL_R = 0x00000000;    // 1) disable TIMER3A during setup
  TIMER3_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER3_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER3_TAILR_R = RELOAD-1;    // 4) reload value
  TIMER3_TAPR_R = 0;            // 5) bus clock resolution
  TIMER3_ICR_R = 0x00000001;    // 6) clear TIMER3A timeout flag
  TIMER3_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI8_R = (NVIC_PRI8_R&0x00FFFFFF)|0xE0000000; // 8) priority 7
// interrupts enabled in the main program after all devices initialized
// vector number 51, interrupt number 35
  NVIC_EN1_R = 1<<(35-32);      // 9) enable IRQ 35 in NVIC
  TIMER3_CTL_R = 0x00000001;    // 10) enable TIMER3A
	EndCritical(status);
}

void Timer3A_Handler(void){
  TIMER3_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER3A timeout
	
	
	void (*task)(void);
	NodeType *temp = PeriodicPt;
	while(temp) {
		if (temp->TimeLeft <= RELOAD) {
			temp->TimeLeft = temp->TimeSlice;
			task = (void (*)(void)) temp->ThreadPt->InitialPC;
				
			
			#ifdef DEBUG
			unsigned long jit = 0;	
			temp->thisTime = OS_Time(); 
			if (temp->lastTime != 0) {
				unsigned long diff = OS_TimeDifference(temp->lastTime, temp->thisTime);
				temp->lastTime = temp->thisTime;
				unsigned long period = temp->TimeSlice;
				if(diff> period){
					jit = (diff-period+4)/8;  // in 0.1 usec
						}
				else{
					jit = (period-diff+4)/8;  // in 0.1 usec
				}
				if (jit > temp->maxJitter) {
					temp->maxJitter = jit;
				}
				if(jit >= JITTERSIZE){
					jit = JITTERSIZE-1;
				}
				temp->JitterHistogram[jit]++;
				}
			else {
				temp->lastTime = temp->thisTime;
			}
    
			#endif
			
			
			(task)();
			
		
		}
		else {
			temp->TimeLeft -= RELOAD;
		}
		temp = temp->Next;
	}
	
}
