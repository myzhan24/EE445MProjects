// Lab1Tester.c
// Tests lab1
//

#include <stdint.h>
#include <string.h>
#include "tm4c123gh6pm.h"

#include "OS.h"
#include "ST7735.h"
#include "ADCT0ATrigger.h"
#include "UART.h"


//testing LCD and ADC
void ADCReadPrint(void) {
		volatile int adc = 0;
		
		char adcstring[] = {'A','D','C',' ','0',' ',0};
    // do ADC collection
		adc = ADC_In();
    // some UART stuff
		//UART_PromptCommand(string,19);
    // do lcd transmit
		adcstring[4]+=ADC_CurrentChannel();
		//ST7735_PlotClear(0,160);
		Output_Clear();
		ST7735_Message(1,0,adcstring,adc);
		UART_OutString(adcstring);
		UART_OutUDec(adc);
		OutCRLF();
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
		OutCRLF();
		if(n>-1 && n<12)
		{
			ADC_Open(n);
			UART_OutString("Switched to ADC Port: "); UART_OutUDec(n);
			OutCRLF();
			OutCRLF();
		}
		else
		{
			UART_OutString("Invalid ADC Port: "); UART_OutUDec(n);
			OutCRLF();
			OutCRLF();
		}
	}
	else if(strcmp(bufPt,"print")==0)
	{
		char string[20];
		UART_OutString("String: ");
		UART_InString(string,20);
		OutCRLF();
		
		UART_OutString("Value: ");
		n=UART_InUDec();
		OutCRLF();
		
		UART_OutString("Device: ");
		device=UART_InUDec();
		OutCRLF();
		if(device <0 || device > 1) {
			UART_OutString("Invalid Device [0,1]");
			OutCRLF();
			return;
		}
		
		UART_OutString("Line: ");
		line=UART_InUDec();
		OutCRLF();
		if(line <0 || line > 3) {
			UART_OutString("Invalid line [0,7]");
			OutCRLF();
			return;
		}
		
		
    UART_OutString("Printing to LCD: "); 
		UART_OutString(string);
		UART_OutChar(' ');
		UART_OutUDec(n);
		OutCRLF();
		OutCRLF();
		
		ST7735_Message(device, line, string, n);
	}
	else if (strcmp(bufPt, "printadc")==0)
	{
		
		UART_OutString("Device: ");
		device=UART_InUDec();
		OutCRLF();
		if(device <0 || device > 1) {
			
			UART_OutString("Invalid Device [0,1]");OutCRLF();return;
		}
		
		UART_OutString("Line: ");
		line=UART_InUDec();
		OutCRLF();
		if(line <0 || line > 3) {
			
			UART_OutString("Invalid line [0,7]");OutCRLF();return;
		}
		
		adcval = ADC_In();
		ST7735_Message(device,line,first,ADC_In());
    UART_OutString("Printing to LCD: "); 
		first[4]+=ADC_CurrentChannel();
		UART_OutString(first);
		UART_OutChar(' ');
		UART_OutUDec(adcval);
		OutCRLF();
		OutCRLF();
		
		ST7735_Message(device, line, first, adcval);
	}
}

void promptCommand(char *bufPt, uint16_t max) {
	
int length=0;
char character;
	UART_OutString("Enter a Command: ");
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
	OutCRLF();
	serviceCommand(bufPt);
}

void commandLine(void){
		char string[20];
		promptCommand(string,19);
}

void dummy(void) {}

int main(void){
		char string[20];
    OS_Init(); //system and systick init
    // UART Init goes here!!
		UART_Init();
    //ST7735_InitB(); //LCD Init 
		ST7735_InitR(INITR_REDTAB);
    // ADC Init goes here!!
		ADC_InitTimerTriggerSeq3(0,8000000); //highest frequency of 10000 Hz starting at channel 0
    //OS_AddPeriodicThread(ADCReadPrint, 800000000, 2);
		OS_AddPeriodicThread(commandLine,80000000,3);
		//OS_AddPeriodicThread(dummy,80000000,3);
    while(1) {
				//OS_AddPeriodicThread(UARTCommandLine,8000,3);
    }
}


