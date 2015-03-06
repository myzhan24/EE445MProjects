// ADCT0ATrigger.h
// Runs on LM4F120/TM4C123
// Provide a function that initializes Timer0A to trigger ADC
// SS3 conversions and request an interrupt when the conversion
// is complete.
// Daniel Valvano
// September 11, 2013

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013

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

// This initialization function sets up the ADC according to the
// following parameters.  Any parameters not explicitly listed
// below are not modified:
// Timer0A: enabled
// Mode: 16-bit, down counting
// One-shot or periodic: periodic
// Prescale value: programmable using variable 'prescale' [0:255]
// Interval value: programmable using variable 'period' [0:65535]
// Sample time is busPeriod*(prescale+1)*(period+1)
// Max sample rate: <=125,000 samples/second
// Sequencer 0 priority: 1st (highest)
// Sequencer 1 priority: 2nd
// Sequencer 2 priority: 3rd
// Sequencer 3 priority: 4th (lowest)
// SS3 triggering event: Timer0A
// SS3 1st sample source: programmable using variable 'channelNum' [0:11]
// SS3 interrupts: enabled and promoted to controller
// channelNum must be 0-11 (inclusive) corresponding to Ain0 through Ain11

#ifndef _ADCH_
#define _ADCH_

void ADC_InitWith(uint8_t channelNum, uint32_t period);

void ADC_Init(uint8_t channelNum);

void ADC_InitT0(uint8_t channelNum,uint32_t period);

void ADC_SetSamplingPeriod(uint32_t period);

void ADC_SetSamplingFrequency(uint32_t frequency);

uint8_t ADC_CurrentChannel(void);

void ADC_Collect(uint8_t channelNum, uint32_t FS, void (*task)(unsigned long));


int8_t ADC_Open(uint8_t channelNum);


uint32_t ADC_In(void);

int ADC_Status(void);

void ADC_ChangeSampleRateHz(uint32_t FS);

uint32_t ADC_CurrentFrequency(void);
uint32_t ADC_CurrentPeriod(void);

#endif

