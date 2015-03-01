// Switch.c
// Runs on LM4F120/TM4C123
// Provide functions that initialize a GPIO as an input pin and
// allow reading of two negative logic switches on PF0 and PF4
// and an external switch on PA5.
// Use bit-banded I/O.
// Daniel and Jonathan Valvano
// September 12, 2013

/* This example accompanies the book
   "Embedded Systems: Introduction to ARM Cortex M Microcontrollers",
   ISBN: 978-1469998749, Jonathan Valvano, copyright (c) 2013
   Section 4.2    Program 4.2

  "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2014
   Example 2.3, Program 2.9, Figure 2.36

 Copyright 2014 by Jonathan W. Valvano, valvano@mail.utexas.edu
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

// negative logic switches connected to PF0 and PF4 on the Launchpad
// red LED connected to PF1 on the Launchpad
// blue LED connected to PF2 on the Launchpad
// green LED connected to PF3 on the Launchpad
// NOTE: The NMI (non-maskable interrupt) is on PF0.  That means that
// the Alternate Function Select, Pull-Up Resistor, Pull-Down Resistor,
// and Digital Enable are all locked for PF0 until a value of 0x4C4F434B
// is written to the Port F GPIO Lock Register.  After Port F is
// unlocked, bit 0 of the Port F GPIO Commit Register must be set to
// allow access to PF0's control registers.  On the LM4F120, the other
// bits of the Port F GPIO Commit Register are hard-wired to 1, meaning
// that the rest of Port F can always be freely re-configured at any
// time.  Requiring this procedure makes it unlikely to accidentally
// re-configure the JTAG and NMI pins as GPIO, which can lock the
// debugger out of the processor and make it permanently unable to be
// debugged or re-programmed.
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "Switch.h"

#define GPIO_LOCK_KEY           0x4C4F434B  // Unlocks the GPIO_CR register
#define PF0                     (*((volatile uint32_t *)0x40025004))
#define PF4                     (*((volatile uint32_t *)0x40025040))
#define PA5                     (*((volatile uint32_t *)0x40004080))
#define SWITCHES                (*((volatile uint32_t *)0x40025044))
#define SW1       0x10                      // on the left side of the Launchpad board
#define SW2       0x01                      // on the right side of the Launchpad board
#define SYSCTL_RCGC2_GPIOF      0x00000020  // port F Clock Gating Control

#define NVIC_EN0_INT30					0x40000000  // interrupt 30 enable (port f gpio)


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void (*SWTask)(void);   // user function


//------------Board_Init------------
// Initialize GPIO Port F for negative logic switches on PF0 and
// PF4 as the Launchpad is wired.  Weak internal pull-up
// resistors are enabled, and the NMI functionality on PF0 is
// disabled.
// Input: none
// Output: none
// modified by Rob to only use PF4 (the left switch)
void Board_Init(void(*task)(void)){          
	
	int status = StartCritical();
	int delay;
	SWTask = task;
	
  SYSCTL_RCGCGPIO_R |= 0x20;     // 1) activate Port F
  while((SYSCTL_PRGPIO_R&0x20) == 0){};// ready?
                                 // 2a) unlock GPIO Port F Commit Register
  GPIO_PORTF_LOCK_R = GPIO_LOCK_KEY;
  GPIO_PORTF_CR_R |= (SW1);  // 2b) enable commit for PF4 
                                 // 3) disable analog functionality on PF4 
  GPIO_PORTF_AMSEL_R &= ~(SW1);
		
		
                                 // 4) configure PF4 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFF0FFF0)+0x00000000;
  GPIO_PORTF_DIR_R &= ~(SW1);// 5) make PF4 in (built-in buttons)
                                 // 6) disable alt funct on PF0 and PF4
  GPIO_PORTF_AFSEL_R &= ~(SW1);
  delay = SYSCTL_RCGC2_R;        // put a delay here if you are seeing erroneous NMI
  GPIO_PORTF_PUR_R |= (SW1); // enable weak pull-up on PF4
  GPIO_PORTF_DEN_R |= (SW1); // 7) enable digital I/O on PF4
		
		
	GPIO_PORTF_IS_R &= ~0x10;   // PF4 is edge-sensitive (default setting)
  GPIO_PORTF_IBE_R &= ~0x10;  // PF4 is not both edges (default setting)
  GPIO_PORTF_IEV_R |= 0x10;   // PF4 rising edge event
  GPIO_PORTF_ICR_R = 0x10;    // clear flag4
  GPIO_PORTF_IM_R |= 0x10;    // enable interrupt on PF4
                              // GPIO PortF=priority 2
  //NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00400000; // bits 21-23	
	NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // (g) priority 5
	NVIC_EN0_R = (NVIC_EN0_INT30);	// enable interrupt 30
	
	EndCritical(status);
}


void GPIOPortF_Handler(void) {
	//int status = StartCritical();
	(*SWTask)();						//execute user task
	GPIO_PORTF_ICR_R = 0x10; //acknowledge flag4
	
//	EndCritical(status);
}


