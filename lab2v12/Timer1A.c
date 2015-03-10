// timer1.c
// Runs on LM4F120/TM4C123
// Use timer1 in 32-bit periodic mode to request interrupts at a periodic rate
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

#define TIMER1APRIORITY	0x00008000		//4

extern volatile int rtCounter;

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

// ***************** timer1_Init ****************
// Activate timer1 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer1A_Init(uint32_t period){ int delay;
  SYSCTL_RCGCTIMER_R |= 0x02;   // 0) activate timer1
	delay = 20;
  TIMER1_CTL_R = 0x00000000;    // 1) disable timer1A during setup
  TIMER1_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER1_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER1_TAILR_R = period-1;    // 4) reload value
  TIMER1_TAPR_R = 0;            // 5) bus clock resolution
  TIMER1_ICR_R = 0x00000001;    // 6) clear timer1A timeout flag
  TIMER1_IMR_R = 0x00000001;    // 7) arm timeout interrupt
//NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF0FFF)|0x0000A000; 		// PRI5   at priority 5 
	NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF0FFF)|TIMER1APRIORITY; 		// PRI5   at priority 5 
// interrupts enabled in the main program after all devices initialized
// vector number 51, interrupt number 35
  NVIC_EN0_R |= 1<<21;      // 9) IRQ 21 = TIMER1A Interrupt Enable
	TIMER1_CTL_R = 0x00000200;    // 10) enable timer1A
  TIMER1_CTL_R = 0x00000001;    // 10) enable timer1A
}

void Timer1A_Handler(void){
	int sr = StartCritical();
	//DisableInterrupts();
	TIMER1_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer1A timeout
	rtCounter++;
	//EnableInterrupts();
	EndCritical(sr);
}
