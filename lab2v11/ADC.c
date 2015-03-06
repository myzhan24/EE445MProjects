// ADCT0ATrigger.c
// Runs on LM4F120/TM4C123
// Provide a function that initializes Timer0A to trigger ADC
// SS3 conversions and request an interrupt when the conversion
// is complete.
// Daniel Valvano
// September 11, 2013

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2014

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
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "ADC.h"
#define NVIC_EN0_INT17          0x00020000  // Interrupt 17 enable

#define TIMER_CFG_16_BIT        0x00000004  // 16-bit timer configuration,
                                            // function is controlled by bits
                                            // 1:0 of GPTMTAMR and GPTMTBMR
#define TIMER_TAMR_TACDIR       0x00000010  // GPTM Timer A Count Direction
#define TIMER_TAMR_TAMR_PERIOD  0x00000002  // Periodic Timer mode
#define TIMER_CTL_TAOTE         0x00000020  // GPTM TimerA Output Trigger
                                            // Enable
#define TIMER_CTL_TAEN          0x00000001  // GPTM TimerA Enable
#define TIMER_IMR_TATOIM        0x00000001  // GPTM TimerA Time-Out Interrupt
                                            // Mask
#define TIMER_TAILR_TAILRL_M    0x0000FFFF  // GPTM TimerA Interval Load
                                            // Register Low

#define ADC_ACTSS_ASEN3         0x00000008  // ADC SS3 Enable
#define ADC_RIS_INR3            0x00000008  // SS3 Raw Interrupt Status
#define ADC_IM_MASK3            0x00000008  // SS3 Interrupt Mask
#define ADC_ISC_IN3             0x00000008  // SS3 Interrupt Status and Clear
#define ADC_EMUX_EM3_M          0x0000F000  // SS3 Trigger Select mask
#define ADC_EMUX_EM3_TIMER      0x00005000  // Timer
#define ADC_SSPRI_SS3_4TH       0x00003000  // fourth priority
#define ADC_SSPRI_SS2_3RD       0x00000200  // third priority
#define ADC_SSPRI_SS1_2ND       0x00000010  // second priority
#define ADC_SSPRI_SS0_1ST       0x00000000  // first priority
#define ADC_PSSI_SS3            0x00000008  // SS3 Initiate
#define ADC_SSCTL3_TS0          0x00000008  // 1st Sample Temp Sensor Select
#define ADC_SSCTL3_IE0          0x00000004  // 1st Sample Interrupt Enable
#define ADC_SSCTL3_END0         0x00000002  // 1st Sample is End of Sequence
#define ADC_SSCTL3_D0           0x00000001  // 1st Sample Diff Input Select
#define ADC_SSFIFO3_DATA_M      0x00000FFF  // Conversion Result Data mask
#define ADC_PC_SR_M             0x0000000F  // ADC Sample Rate
#define ADC_PC_SR_125K          0x00000001  // 125 ksps
#define SYSCTL_RCGCGPIO_R4      0x00000010  // GPIO Port E Run Mode Clock
                                            // Gating Control
#define SYSCTL_RCGCGPIO_R3      0x00000008  // GPIO Port D Run Mode Clock
                                            // Gating Control
#define SYSCTL_RCGCGPIO_R1      0x00000002  // GPIO Port B Run Mode Clock
                                            // Gating Control

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void Producer(unsigned long data);

// There are many choices to make when using the ADC, and many
// different combinations of settings will all do basically the
// same thing.  For simplicity, this function makes some choices
// for you.  When calling this function, be sure that it does
// not conflict with any other software that may be running on
// the microcontroller.  Particularly, ADC0 sample sequencer 3
// is used here because it only takes one sample, and only one
// sample is absolutely needed.  Sample sequencer 3 generates a
// raw interrupt when the conversion is complete, and it is then
// promoted to an ADC0 controller interrupt.  Hardware Timer0A
// triggers the ADC0 conversion at the programmed interval, and
// software handles the interrupt to process the measurement
// when it is complete.
//
// A simpler approach would be to use software to trigger the
// ADC0 conversion, wait for it to complete, and then process the
// measurement.
//
// This initialization function sets up the ADC according to the
// following parameters.  Any parameters not explicitly listed
// below are not modified:
// Timer0A: ADC0
// Mode: 32-bit, down counting
// One-shot or periodic: periodic
// Interval value: programmable using 32-bit period
// Sample time is busPeriod*period
// Max sample rate: <=125,000 samples/second
// Sequencer 0 priority: 1st (highest)
// Sequencer 1 priority: 2nd
// Sequencer 2 priority: 3rd
// Sequencer 3 priority: 4th (lowest)
// SS3 triggering event: Timer0A
// SS3 1st sample source: programmable using variable 'channelNum' [0:11]
// SS3 interrupts: enabled and promoted to controller
// ADCNum ADC0 or ADC1

volatile int samples;
volatile int totalSamples;

volatile int collectDone;
volatile int samplingFrequency;
volatile int samplingPeriod;

void ADC_InitT0(uint8_t channelNum,uint32_t period) {
	  int status = StartCritical();
	volatile uint32_t delay;

	
	collectDone = 0;
	
  // **** GPIO pin initialization ****
             // 1) activate clock
                        //    these are on GPIO_PORTE
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R4; 
                       //    these are on GPIO_PORTD
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R3;
                       //    these are on GPIO_PORTB
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
    
  delay = SYSCTL_RCGCGPIO_R;      // 2) allow time for clock to stabilize
  delay = SYSCTL_RCGCGPIO_R;
	
			//PE0-PE5
	
	                  //      Ain8 is on PE5
										//      Ain9 is on PE4
										//      Ain0 is on PE3
										//      Ain1 is on PE2
										//      Ain2 is on PE1
										//      Ain3 is on PE0
					GPIO_PORTE_DIR_R &= ~0x3F;  // 3.8) make PE5 input
      GPIO_PORTE_AFSEL_R |= 0x3F; // 4.8) enable alternate function on PE5
      GPIO_PORTE_DEN_R &= ~0x3F;  // 5.8) disable digital I/O on PE5
      GPIO_PORTE_AMSEL_R |= 0x3F; // 6.8) enable analog functionality on PE5
                         
			
			//PD0-PD3
                     //      Ain4 is on PD3
										 //      Ain5 is on PD2
										 //      Ain6 is on PD1
										 //      Ain7 is on PD0
      GPIO_PORTD_DIR_R &= ~0x0F;  // 3.4) make PD3 input
      GPIO_PORTD_AFSEL_R |= 0x0F; // 4.4) enable alternate function on PD3
      GPIO_PORTD_DEN_R &= ~0x0F;  // 5.4) disable digital I/O on PD3
      GPIO_PORTD_AMSEL_R |= 0x0F; // 6.4) enable analog functionality on PD3
                       
      
			
			//PB4 PB5
                        //       Ain10 is on PB4
												//       Ain11 is on PB5
      GPIO_PORTB_DIR_R &= ~0x30;  // 3.10) make PB4 input
      GPIO_PORTB_AFSEL_R |= 0x30; // 4.10) enable alternate function on PB4-PB5
      GPIO_PORTB_DEN_R &= ~0x30;  // 5.10) disable digital I/O on PB4-PB5
      GPIO_PORTB_AMSEL_R |= 0x30; // 6.10) enable analog functionality on PB4-PB5

   
  
	

	SYSCTL_RCGCADC_R |= 0x01;     // activate ADC0 
	SYSCTL_RCGCTIMER_R |= 0x01;   // activate timer0 
	delay = SYSCTL_RCGCTIMER_R;   // allow time to finish activating
	TIMER0_CTL_R = 0x00000000;    // disable timer0A during setup
	TIMER0_CTL_R |= 0x00000020;   // enable timer0A trigger to ADC
	TIMER0_CFG_R = 0;             // configure for 32-bit timer mode
	TIMER0_TAMR_R = 0x00000002;   // configure for periodic mode, default down-count settings
	TIMER0_TAPR_R = 0;            // prescale value for trigger
	TIMER0_TAILR_R = period-1;    // start value for trigger
	TIMER0_IMR_R = 0x00000000;    // disable all interrupts
	TIMER0_CTL_R |= 0x0000c0001;   // enable timer0A 32-b, periodic, no interrupts
	ADC0_PC_R = 0x01;         // configure for MAX 125K samples/sec
	ADC0_SSPRI_R = 0x3210;    // sequencer 0 is highest, sequencer 3 is lowest
	ADC0_ACTSS_R &= ~0x08;    // disable sample sequencer 3
	ADC0_EMUX_R = (ADC0_EMUX_R&0xFFFF0FFF)+0x5000; // timer trigger event
	ADC0_SSMUX3_R = channelNum;
	ADC0_SSCTL3_R = 0x06;          // set flag and end                       
	ADC0_IM_R |= 0x08;             // enable SS3 interrupts
	ADC0_ACTSS_R |= 0x08;          // enable sample sequencer 3
	NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x00002000; //priority 1
//	NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x00004000; //priority 2
//	NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x0000E000; //priority 7
	NVIC_EN0_R |= 1<<17;              // enable interrupt 17 in NVIC
  EndCritical(status);
}


//uses a default period instead of one specified
void ADC_Init(uint8_t channelNum) {
	int stat = StartCritical();
	volatile uint32_t delay;
	collectDone = 0;
  // **** GPIO pin initialization ****
             // 1) activate clock
                        //    these are on GPIO_PORTE
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R4; 
                       //    these are on GPIO_PORTD
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R3;
                       //    these are on GPIO_PORTB
      SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
    
  delay = SYSCTL_RCGCGPIO_R;      // 2) allow time for clock to stabilize
  delay = SYSCTL_RCGCGPIO_R;
	
			//PE0-PE5
	
	                  //      Ain8 is on PE5
										//      Ain9 is on PE4
										//      Ain0 is on PE3
										//      Ain1 is on PE2
										//      Ain2 is on PE1
										//      Ain3 is on PE0
					GPIO_PORTE_DIR_R &= ~0x3F;  // 3.8) make PE5 input
      GPIO_PORTE_AFSEL_R |= 0x3F; // 4.8) enable alternate function on PE5
      GPIO_PORTE_DEN_R &= ~0x3F;  // 5.8) disable digital I/O on PE5
      GPIO_PORTE_AMSEL_R |= 0x3F; // 6.8) enable analog functionality on PE5
                         
			
			//PD0-PD3
                     //      Ain4 is on PD3
										 //      Ain5 is on PD2
										 //      Ain6 is on PD1
										 //      Ain7 is on PD0
      GPIO_PORTD_DIR_R &= ~0x0F;  // 3.4) make PD3 input
      GPIO_PORTD_AFSEL_R |= 0x0F; // 4.4) enable alternate function on PD3
      GPIO_PORTD_DEN_R &= ~0x0F;  // 5.4) disable digital I/O on PD3
      GPIO_PORTD_AMSEL_R |= 0x0F; // 6.4) enable analog functionality on PD3
                       
      
			
			//PB4 PB5
                        //       Ain10 is on PB4
												//       Ain11 is on PB5
      GPIO_PORTB_DIR_R &= ~0x30;  // 3.10) make PB4 input
      GPIO_PORTB_AFSEL_R |= 0x30; // 4.10) enable alternate function on PB4-PB5
      GPIO_PORTB_DEN_R &= ~0x30;  // 5.10) disable digital I/O on PB4-PB5
      GPIO_PORTB_AMSEL_R |= 0x30; // 6.10) enable analog functionality on PB4-PB5

   
  
	
 // int status = StartCritical();
	SYSCTL_RCGCADC_R |= 0x01;     // activate ADC0 
	SYSCTL_RCGCTIMER_R |= 0x01;   // activate timer0 
	delay = SYSCTL_RCGCTIMER_R;   // allow time to finish activating
	TIMER0_CTL_R = 0x00000000;    // disable timer0A during setup
	TIMER0_CTL_R |= 0x00000020;   // enable timer0A trigger to ADC
	TIMER0_CFG_R = 0;             // configure for 32-bit timer mode
	TIMER0_TAMR_R = 0x00000002;   // configure for periodic mode, default down-count settings
	TIMER0_TAPR_R = 0;            // prescale value for trigger
	TIMER0_TAILR_R = 8000000-1;    // start value for trigger
	TIMER0_IMR_R = 0x00000000;    // disable all interrupts
	TIMER0_CTL_R |= 0x0000c0001;   // enable timer0A 32-b, periodic, no interrupts
	ADC0_PC_R = 0x01;         // configure for MAX 125K samples/sec
	ADC0_SSPRI_R = 0x3210;    // sequencer 0 is highest, sequencer 3 is lowest
	ADC0_ACTSS_R &= ~0x08;    // disable sample sequencer 3
	ADC0_EMUX_R = (ADC0_EMUX_R&0xFFFF0FFF)+0x5000; // timer trigger event
	ADC0_SSMUX3_R = channelNum;
	ADC0_SSCTL3_R = 0x06;          // set flag and end                       
	ADC0_IM_R |= 0x08;             // enable SS3 interrupts
	ADC0_ACTSS_R |= 0x08;          // enable sample sequencer 3
		NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x00002000; //priority 1
//	NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x00004000; //priority 2
//		NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFF00FF)|0x0000E000; //priority 7
	NVIC_EN0_R |= 1<<17;              // enable interrupt 17 in NVIC
  EndCritical(stat);
}

void ADC_ChangeSampleRatePeriod(uint32_t period) {
	
	if(period > 8000000)		//no slower than 100Hz
		period = 8000000; 	//if the period is outside the bounds, the default is the lowest frequency
	
	else if(period < 8000)	//no faster than 10000Hz
		period = 8000;
	
		samplingPeriod = period;
	samplingFrequency = 80000000/period;
	
//	int status = StartCritical();
//	TIMER0_CTL_R = 0x00000000;    // disable timer0A during setup
	TIMER0_TAILR_R = period-1;    // start value for trigger
//	TIMER0_CTL_R |= 0x00000001;   // enable timer0A 32-b, periodic, no interrupts
//	EndCritical(status);
}

void ADC_ChangeSampleRateHz(uint32_t Hz) {
	
	/*
	int period = 80000000/Hz; // p = 1/f. p * 1000000000 / 12.5 = timer ticks on 80Mhz bus clock
	
	if(period > 8000000)		//no slower than 100Hz
		period = 8000000; 	//if the period is outside the bounds, the default is the lowest frequency
	
	else if(period < 8000)	//no faster than 10000Hz
		period = 8000;
	*/

		samplingFrequency = Hz;
	
	int period = 80000000/Hz; // p = 1/f. p * 1,000,000,000 / 12.5 = timer ticks on 80 Mhz bus clock
	
	if(period > 8000000)		//no slower than 100Hz
		period = 8000000; 	//if the period is outside the bounds, the default is the lowest frequency
	
	else if(period < 8000)	//no faster than 10000Hz
		period = 8000;
	
	
	samplingPeriod = period;
//	int status = StartCritical();
//	TIMER0_CTL_R = 0x00000000;    // disable timer0A during setup
	TIMER0_TAILR_R = period-1;    // start value for trigger
//	TIMER0_CTL_R |= 0x00000001;   // enable timer0A 32-b, periodic, no interrupts
//	EndCritical(status);
}

int8_t ADC_Open(uint8_t channelNum){
	int sr = StartCritical();
	if(ADC0_SSMUX3_R != channelNum && channelNum <12) { //the channel needs to be updated
		ADC0_SSMUX3_R = channelNum;
		EndCritical(sr);
		return channelNum;
	}
	EndCritical(sr);
	return -1;
}

volatile uint32_t ADCvalue;
void ADC0Seq3_Handler(void){	// timer generates conversion
	ADC0_ISC_R = 0x08;          // acknowledge ADC sequence 3 completion
  ADCvalue = ADC0_SSFIFO3_R;  // 12-bit result
	//collectDone = 1;
}


//for lab 2
/*
void ADC0Seq3_Handler(void){	// timer generates conversion
  ADC0_ISC_R = 0x08;          // acknowledge ADC sequence 3 completion
  Producer(ADC0_SSFIFO3_R);  // 12-bit result
}
*/

uint32_t ADC_In(){
	while((ADC0_RIS_R & 0x08)==8) {};	// wait for conversion done
	//while(!collectDone){};
	//collectDone = 0;
	return ADCvalue;
}

uint8_t ADC_CurrentChannel() {
	return ADC0_SSMUX3_R;
}

uint32_t ADC_CurrentPeriod() {
	return samplingPeriod;
}

uint32_t ADC_CurrentFrequency(){
	return samplingFrequency;
}

//
//
/*
int ADC_Collect(unsigned int channelNum, unsigned int fs, unsigned short buffer[], unsigned int numberOfSamples){
	volatile int i;
	int sr = StartCritical();
	if(ADC_CurrentChannel()!=channelNum)
			ADC_Open(channelNum);
	
	ADC_ChangeSampleRateHz(fs);
	
	EndCritical(sr);
	
	i = 0;
	while(i<numberOfSamples){
		buffer[i] = ADC_In();
		i++;
	}
	
	collectDone = 1;
	return 0;
}

int ADC_Status(){
	int sr = StartCritical();
	int temp = collectDone;
	collectDone = 0;
	EndCritical(sr);
	return temp;
}
*/


void ADC_Collect(uint8_t channelNum, uint32_t FS, void (*task)(unsigned long)){
	uint32_t temp=0;
	
	int sr = StartCritical();
	if(ADC0_SSMUX3_R!=channelNum)
			ADC_Open(channelNum);
	
	if(samplingFrequency!=FS)
			ADC_ChangeSampleRateHz(FS);
	
	EndCritical(sr);
	
	while((ADC0_RIS_R & 0x08)==8) {};	// wait for conversion done
		temp = ADCvalue;
		
	task(temp);
	
	//collectDone = 1;
}

int ADC_Status(){
	int sr = StartCritical();
	int temp = collectDone;
	collectDone = 0;
	EndCritical(sr);
	return temp;
}

