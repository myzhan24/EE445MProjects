// filename ************** OS.C *******************
// Created by Rob and Matt 2/9/15


#include "OS.h"
// oscilloscope or LED connected to PF2 for period measurement
 #include <stdint.h>
 #include <stdlib.h>
 #include "tm4c123gh6pm.h"

 #include "SysTickInts.h"
 #include "PLL.h"
 #include "UART.h"

 
 #define PF2     (*((volatile uint32_t *)0x40025010))


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

volatile unsigned long periodicInterruptCount; 
void (*currentTask)(void); //make it a list for multithread
volatile unsigned int delta=0;
 
volatile int Counts;
void OS_Init(void){
		volatile int delay; 
    periodicInterruptCount = 0;

    // code below taken from PeriodicSysTickInts.c
    PLL_Init();                 // bus clock at 80 MHz
    SYSCTL_RCGCGPIO_R |= 0x20;  // activate port F
    Counts = 0;
    GPIO_PORTF_DIR_R |= 0x04;   // make PF2 output (PF2 built-in LED)
    GPIO_PORTF_AFSEL_R &= ~0x04;// disable alt funct on PF2
    GPIO_PORTF_DEN_R |= 0x04;   // enable digital I/O on PF2
				// configure PF2 as GPIO
    GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFF0FF)+0x00000000;
    GPIO_PORTF_AMSEL_R = 0;     // disable analog functionality on PF
		//timer1
	
    EnableInterrupts();

/* //put this in the main program that calls this?
    while(1){                   // interrupts every 1ms, 500 Hz flash
      WaitForInterrupt();
    } 
*/

}
 

// calls a thread with a periodic interrupt
// Inputs: task pointer to the function, period millisecond period to execute function, priority NVIC priority (0-7)
// Output: ??
unsigned int OS_AddPeriodicThread(void (*task) (void), unsigned long period, unsigned long priority){
    SysTick_Init(period, priority); // Initizalize SysTick Timer 
    currentTask = task;
		return 1;
}

void OS_ClearPeriodicTime(void){
    periodicInterruptCount = 0;
}

unsigned long OS_ReadPeriodicTime(void){
    return periodicInterruptCount;
}


void SysTick_Handler(void){
	//delta = abs(NVIC_ST_CURRENT_R - delta);
	//delta = abs((NVIC_ST_RELOAD_R+1) - NVIC_ST_CURRENT_R);
	delta = NVIC_ST_CURRENT_R;
		currentTask();
	delta = (delta - NVIC_ST_CURRENT_R);
 /* PF2 ^= 0x04;                // toggle PF2
  PF2 ^= 0x04;                // toggle PF2
  periodicInterruptCount = periodicInterruptCount + 1;
  
  PF2 ^= 0x04;                // toggle PF2
	*/
	/*UART_OutUDec(delta);
	OutCRLF();
	UART_OutUDec(delta)*/
	// 80 Mhz = 1/12.5

}


 
