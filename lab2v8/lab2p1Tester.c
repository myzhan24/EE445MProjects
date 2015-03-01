#include "OS.h"
#include "tm4c123gh6pm.h"

#define GPIO_PORTE_IN           (*((volatile uint32_t *)0x4002400C)) // bits 1-0
#define PE0			(*(volatile uint32_t *)0x40024004)
#define PE1			(*(volatile uint32_t *)0x40024008)	
#define PE2			(*(volatile uint32_t *)0x40024010)

unsigned long Count1; // number of times thread1 loops
unsigned long Count2; // number of times thread2 loops
unsigned long Count3; // number of times thread3 loops

unsigned int NumCreated;

void PortE_Init(void){  
	unsigned long volatile delay;
  SYSCTL_RCGCGPIO_R |= 0x10; // activate port E
  delay = SYSCTL_RCGCGPIO_R;
  delay = SYSCTL_RCGCGPIO_R;
  GPIO_PORTE_LOCK_R = 0x4C4F434B;   // 2) unlock PortF PF0  
  GPIO_PORTE_CR_R |= 0x07;           // allow changes to PF4-0       
  GPIO_PORTE_DIR_R = 0x07;     // make PF3-1 output (PF3-1 built-in LEDs),PF4,0 input
  GPIO_PORTE_PUR_R = 0x07;     // PF4,0 have pullup
  GPIO_PORTE_AFSEL_R = 0x00;   // disable alt funct on PF4-0
  GPIO_PORTE_DEN_R = 0x07;     // enable digital I/O on PF4-0
  GPIO_PORTE_PCTL_R = 0x00000000;
  GPIO_PORTE_AMSEL_R = 0;      // disable analog functionality on PF
}

void Thread1(void){
 Count1 = 0;
 for(;;){
 PE0 ^= 0x01; // heartbeat
 Count1++;
 OS_Suspend(); // cooperative multitasking
 }
}
void Thread2(void){
 Count2 = 0;
 for(;;){
 PE1 ^= 0x02; // heartbeat
 Count2++;
 OS_Suspend(); // cooperative multitasking
 }
}
void Thread3(void){
 Count3 = 0;
 for(;;){
 PE2 ^= 0x04; // heartbeat
 Count3++;
 OS_Suspend(); // cooperative multitasking
 }
}
int main(void){ // Testmain1
 OS_Init(); // initialize, disable interrupts
 PortE_Init(); // profile user threads
 
 NumCreated = 0 ;
 NumCreated += OS_AddThread(&Thread1,128,1);
 NumCreated += OS_AddThread(&Thread2,128,2);
 NumCreated += OS_AddThread(&Thread3,128,3);
 // Count1 Count2 Count3 should be equal or off by one at all times
 OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
 return 0; // this never executes
}
