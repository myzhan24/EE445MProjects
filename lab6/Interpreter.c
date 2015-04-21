#include <stdint.h>
#include <string.h>
#include "tm4c123gh6pm.h"

#include "PWM.h"
#include "OS.h"
#include "ST7735.h"
#include "ADC.h"
#include "UART.h"
#include "Lab4.h"

extern long adc_count;
extern long* x;
extern long* y;
//testing LCD and ADC

#define FS 12800

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void ADCReadPrint(void) {
		volatile int adc = 0;
		
		char adcstring[] = {'A','D','C',' ','0',' ',0};
    // do ADC collection
		adc = ADC_In();
    // some UART stuffF
		//UART_PromptCommand(string,19);
    // do lcd transmit
		adcstring[4]+=ADC_CurrentChannel();
		//ST7735_PlotClear(0,160);
		//Output_Clear();
		ST7735_Message(1,0,adcstring,adc);
		UART_OutString(adcstring);
		UART_OutUDec(adc);
		UART_OutCRLF();
}


void serviceCommand(char *bufPt) {
	volatile int n;
	volatile int device;
	volatile int line;
	char first[]={'A','D','C',' ','0',':',' ',0};
	volatile int adcval;
	if(strcmp(bufPt,"adcopen")==0)
	{
		UART_OutString("Enter ADC port: ");
		n=UART_InUDec();
		UART_OutCRLF();
		if(n>-1 && n<12)
		{
			ADC_Open(n);
			UART_OutString("Switched to ADC Port: "); UART_OutUDec(n);
			UART_OutCRLF();
			UART_OutCRLF();
		}
		else
		{
			UART_OutString("Invalid ADC Port: "); UART_OutUDec(n);
			UART_OutCRLF();
			UART_OutCRLF();
		}
	}
	else if(strcmp(bufPt,"motorstop")==0){
		PWM0A_Period(50000);
		PWM0A_Duty(0);
		PE0 = 0;
		UART_OutCRLF();
	}
	else if(strcmp(bufPt,"motor")==0){
		UART_OutString("PWM Period: ");
		int period = UART_InUDec();
		UART_OutCRLF();
		UART_OutString("PWM Duty: ");
		
		int duty = UART_InUDec();
		UART_OutCRLF();
		UART_OutString("PE0: ");
		int p = UART_InUDec();
		UART_OutCRLF();
		
		PWM0A_Period(period);
		PWM0A_Duty(duty);
		PE0 = p;
		
	}
	else if(strcmp(bufPt,"print")==0)
	{
		char string[20];
		UART_OutString("String: ");
		UART_InString(string,20);
		UART_OutCRLF();
		
		UART_OutString("Value: ");
		n=UART_InUDec();
		UART_OutCRLF();
		
		UART_OutString("Device: ");
		device=UART_InUDec();
		UART_OutCRLF();
		if(device <0 || device > 1) {
			UART_OutString("Invalid Device [0,1]");
			UART_OutCRLF();
			return;
		}
		
		UART_OutString("Line: ");
		line=UART_InUDec();
		UART_OutCRLF();
		if(line <0 || line > 3) {
			UART_OutString("Invalid line [0,7]");
			UART_OutCRLF();
			return;
		}
		
		
    UART_OutString("Printing to LCD: "); 
		UART_OutString(string);
		UART_OutChar(' ');
		UART_OutUDec(n);
		UART_OutCRLF();
		UART_OutCRLF();
		
		ST7735_Message(device, line, string, n);
	}
	else if (strcmp(bufPt, "printadc")==0)
	{
		
		UART_OutString("Device: ");
		device=UART_InUDec();
		UART_OutCRLF();
		if(device <0 || device > 1) {
			
			UART_OutString("Invalid Device [0,1]");UART_OutCRLF();return;
		}
		
		UART_OutString("Line: ");
		line=UART_InUDec();
		UART_OutCRLF();
		if(line <0 || line > 3) {
			
			UART_OutString("Invalid line [0,7]");UART_OutCRLF();return;
		}
		
		adcval = ADC_In();
		ST7735_Message(device,line,first,ADC_In());
    UART_OutString("Printing to LCD: "); 
		first[4]+=ADC_CurrentChannel();
		UART_OutString(first);
		UART_OutChar(' ');
		UART_OutUDec(adcval);
		UART_OutCRLF();
		UART_OutCRLF();
		
		ST7735_Message(device, line, first, adcval);
	}
	else if(strcmp(bufPt,"vvt")==0){
		ADC_Init(5);
		ADC_Collect(5, FS, &transfer);
		OS_AddThread(&filterWorkVVT, 128, 2);
		//UART_OutString("adc sampling on PD2");
		UART_OutCRLF();
	}
	else if(strcmp(bufPt,"vvf")==0){
		ADC_Init(5);
		ADC_Collect(5, FS, &transfer);
		OS_AddThread(&filterWorkVVF, 128, 2);
		//UART_OutString("FFT on PD2");
		UART_OutCRLF();
	}
	else if (strcmp(bufPt, "adc")==0)
	{
		char string[20];
		volatile int chan;
		volatile char in;
		UART_OutString("Channel Number: ");
		chan = UART_InUDec();
		//sr = StartCritical();
		UART_OutCRLF();
		UART_OutString("Trigger Mode: ");
		//UART_OutString(" ");
		//EndCritical(sr);
		
		UART_InString(string,19);
		UART_OutCRLF();
		ADC_Init(chan);
		if (strcmp(string, "timer")==0) {
			ADC_Collect(chan, 12800, &transfer);
		}			
		else if (strcmp(string, "software") == 0) {
			OS_AddPeriodicThread(&softwareSampling , 12800, 2); //NEED TO FIX PERIOD
		}
				
		UART_OutString("filter work: ");
		in = UART_InChar();
		UART_OutCRLF();
		UART_OutChar(in);
		UART_OutCRLF();
		if (in == 'y') {
			//OS_AddThread(&filterWork, 128, 2);
		}
		
		
		
	}
	else if (strcmp(bufPt, "graphvvt")==0)
	{
		//ST7735_PlotArrayVT(x, adc_count,0,3000);
		/*UART_OutString("ADC values: ");
		for (int i = 0; i < adc_count; i++) {
			UART_OutUDec(x[i]);
			UART_OutChar(' ');
		}
		UART_OutCRLF();
		UART_OutCRLF();
		UART_OutString("FFT values: ");
		if (y != 0) {
			
			for (int i = 0; i < 256; i++) {
					UART_OutUDec(y[i]);
					UART_OutChar(' ');
			}
		}
		UART_OutCRLF();
		UART_OutCRLF();
		*/
	}
	else if (strcmp(bufPt, "graphfilter")==0)
	{
		//ST7735_PlotArrayVT(y, 256,0,3000);
	}
}
char string[40];
void promptCommand( uint16_t max) {
char* bufPt = string;
int length=0;
char character;
	UART_OutChar('>');
  character = UART_InChar();
  while(character != CR){
    if(character == BS){
      if(length){
        bufPt--;
        length--;
        UART_OutChar(BS);
      }
    }
    else if(length < max){
      *bufPt = character;
      bufPt++;
      length++;
      UART_OutChar(character);
    }
    character = UART_InChar();
  }
  *bufPt = 0;
	bufPt -= length;
	UART_OutCRLF();
	serviceCommand(bufPt);
}

void Interpreter_CommandLine(void){
		//char string[20];
		promptCommand(39);
}
