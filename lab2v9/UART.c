// UART.c
// Runs on LM4F120/TM4C123
// Use UART0 to implement bidirectional data transfer to and from a
// computer running HyperTerminal.  This time, interrupts and FIFOs
// are used.
// Daniel Valvano
// September 11, 2013
// Modified by EE345L students Charlie Gough && Matt Hawk
// Modified by EE345M students Agustinus Darmawan && Mingjie Qiu

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2014
   Program 5.11 Section 5.6, Program 3.10

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

// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1
#include <stdint.h>
#include <string.h>
#include "tm4c123gh6pm.h"

#include "FIFO.h"
#include "UART.h"
#include "ADC.h"
#include "ST7735.h"
#include "OS.h"

#define NVIC_EN0_INT5           0x00000020  // Interrupt 5 enable

#define UART_FR_RXFF            0x00000040  // UART Receive FIFO Full
#define UART_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART_CTL_UARTEN         0x00000001  // UART Enable
#define UART_IFLS_RX1_8         0x00000000  // RX FIFO >= 1/8 full
#define UART_IFLS_TX1_8         0x00000000  // TX FIFO <= 1/8 full
#define UART_IM_RTIM            0x00000040  // UART Receive Time-Out Interrupt
                                            // Mask
#define UART_IM_TXIM            0x00000020  // UART Transmit Interrupt Mask
#define UART_IM_RXIM            0x00000010  // UART Receive Interrupt Mask
#define UART_RIS_RTRIS          0x00000040  // UART Receive Time-Out Raw
                                            // Interrupt Status
#define UART_RIS_TXRIS          0x00000020  // UART Transmit Raw Interrupt
                                            // Status
#define UART_RIS_RXRIS          0x00000010  // UART Receive Raw Interrupt
                                            // Status
#define UART_ICR_RTIC           0x00000040  // Receive Time-Out Interrupt Clear
#define UART_ICR_TXIC           0x00000020  // Transmit Interrupt Clear
#define UART_ICR_RXIC           0x00000010  // Receive Interrupt Clear



void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
#define FIFOSIZE   16         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure
                              // create index implementation FIFO (see FIFO.h)
AddIndexFifo(Rx, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)
AddIndexFifo(Tx, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)
	
Sema4Type txFifoLock;
Sema4Type rxFifoLock;


//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(){
  UART_OutChar(CR);
  UART_OutChar(LF);
}
// Initialize UART0
// Baud rate is 115200 bits/sec
void UART_Init(void){
	int status = StartCritical();
  SYSCTL_RCGCUART_R |= 0x01;            // activate UART0
  SYSCTL_RCGCGPIO_R |= 0x01;            // activate port A
  RxFifo_Init();                        // initialize empty FIFOs
  TxFifo_Init();
  UART0_CTL_R &= ~UART_CTL_UARTEN;      // disable UART
  UART0_IBRD_R = 27;                    // IBRD = int(50,000,000 / (16 * 115,200)) = int(27.1267)
  UART0_FBRD_R = 8;                     // FBRD = int(0.1267 * 64 + 0.5) = 8
                                        // 8 bit word length (no parity bits, one stop bit, FIFOs)
  UART0_LCRH_R = (UART_LCRH_WLEN_8|UART_LCRH_FEN);
  UART0_IFLS_R &= ~0x3F;                // clear TX and RX interrupt FIFO level fields
                                        // configure interrupt for TX FIFO <= 1/8 full
                                        // configure interrupt for RX FIFO >= 1/8 full
  UART0_IFLS_R += (UART_IFLS_TX1_8|UART_IFLS_RX1_8);
                                        // enable TX and RX FIFO interrupts and RX time-out interrupt
  UART0_IM_R |= (UART_IM_RXIM|UART_IM_TXIM|UART_IM_RTIM);
  UART0_CTL_R |= UART_CTL_UARTEN;       // enable UART
  GPIO_PORTA_AFSEL_R |= 0x03;           // enable alt funct on PA1-0
  GPIO_PORTA_DEN_R |= 0x03;             // enable digital I/O on PA1-0
                                        // configure PA1-0 as UART
  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFFFFFF00)+0x00000011;
  GPIO_PORTA_AMSEL_R = 0;               // disable analog functionality on PA
                                        // UART0=priority 2
  //NVIC_PRI1_R = (NVIC_PRI1_R&0xFFFF0FFF)|0x00004000; // bits 13-15
	NVIC_PRI1_R = (NVIC_PRI1_R&0xFFFF0FFF)|0x0000E000; // bits 13-15 priority 7
  NVIC_EN0_R |= NVIC_EN0_INT5;           // enable interrupt 5 in NVIC

	EndCritical(status);
}

// copy from hardware RX FIFO to software RX FIFO
// stop when hardware RX FIFO is empty or software RX FIFO is full
void static copyHardwareToSoftware(void){
  char letter;
  while(((UART0_FR_R&UART_FR_RXFE) == 0) && (RxFifo_Size() < (FIFOSIZE - 1))){
	//	OS_Wait(&rxFifoLock);//semaphore acquire
    letter = UART0_DR_R;
    RxFifo_Put(letter);
//		OS_Signal(&rxFifoLock);//rx semaphore release
		//semaphore signal
  }
}
// copy from software TX FIFO to hardware TX FIFO
// stop when software TX FIFO is empty or hardware TX FIFO is full
void static copySoftwareToHardware(void){
  char letter;
  while(((UART0_FR_R&UART_FR_TXFF) == 0) && (TxFifo_Size() > 0)){
//		OS_Wait(&txFifoLock);	// tx semaphore wait acquire
    TxFifo_Get(&letter);
    UART0_DR_R = letter;
	//	OS_Signal(&txFifoLock);	//tx semaphore signal release
  }
}
// input ASCII character from UART
// spin if RxFifo is empty
char UART_InChar(void){
  char letter;
  while(RxFifo_Get(&letter) == FIFOFAIL){
		//WaitForInterrupt();
	}
  return(letter);
}
// output ASCII character to UART
// spin if TxFifo is full
void UART_OutChar(char data){
  while(TxFifo_Put(data) == FIFOFAIL){
		//WaitForInterrupt();
	}
  UART0_IM_R &= ~UART_IM_TXIM;          // disable TX FIFO interrupt
  copySoftwareToHardware();
  UART0_IM_R |= UART_IM_TXIM;           // enable TX FIFO interrupt
}


// at least one of three things has happened:
// hardware TX FIFO goes from 3 to 2 or less items
// hardware RX FIFO goes from 1 to 2 or more items
// UART receiver has timed out
void UART0_Handler(void){
  if(UART0_RIS_R&UART_RIS_TXRIS){       // hardware TX FIFO <= 2 items
    UART0_ICR_R = UART_ICR_TXIC;        // acknowledge TX FIFO
    // copy from software TX FIFO to hardware TX FIFO
    copySoftwareToHardware();
    if(TxFifo_Size() == 0){             // software TX FIFO is empty
      UART0_IM_R &= ~UART_IM_TXIM;      // disable TX FIFO interrupt
    }
  }
  if(UART0_RIS_R&UART_RIS_RXRIS){       // hardware RX FIFO >= 2 items
    UART0_ICR_R = UART_ICR_RXIC;        // acknowledge RX FIFO
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
  if(UART0_RIS_R&UART_RIS_RTRIS){       // receiver timed out
    UART0_ICR_R = UART_ICR_RTIC;        // acknowledge receiver time out
    // copy from hardware RX FIFO to software RX FIFO
    copyHardwareToSoftware();
  }
}

//------------UART_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void UART_OutString(char *pt){
  while(*pt){
    UART_OutChar(*pt);
    pt++;
  }
}

//------------UART_InUDec------------
// InUDec accepts ASCII input in unsigned decimal format
//     and converts to a 32-bit unsigned number
//     valid range is 0 to 4294967295 (2^32-1)
// Input: none
// Output: 32-bit unsigned number
// If you enter a number above 4294967295, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t UART_InUDec(void){
uint32_t number=0, length=0;
char character;
  character = UART_InChar();
  while(character != CR){ // accepts until <enter> is typed
// The next line checks that the input is a digit, 0-9.
// If the character is not 0-9, it is ignored and not echoed
    if((character>='0') && (character<='9')) {
      number = 10*number+(character-'0');   // this line overflows if above 4294967295
      length++;
      UART_OutChar(character);
    }
// If the input is a backspace, then the return number is
// changed and a backspace is outputted to the screen
    else if((character==BS) && length){
      number /= 10;
      length--;
      UART_OutChar(character);
    }
    character = UART_InChar();
  }
  return number;
}

//-----------------------UART_OutUDec-----------------------
// Output a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1-10 digits with no space before or after
void UART_OutUDec(uint32_t n){
// This function uses recursion to convert decimal number
//   of unspecified length as an ASCII string
  if(n >= 10){
    UART_OutUDec(n/10);
    n = n%10;
  }
  UART_OutChar(n+'0'); /* n is between 0 and 9 */
}

//---------------------UART_InUHex----------------------------------------
// Accepts ASCII input in unsigned hexadecimal (base 16) format
// Input: none
// Output: 32-bit unsigned number
// No '$' or '0x' need be entered, just the 1 to 8 hex digits
// It will convert lower case a-f to uppercase A-F
//     and converts to a 16 bit unsigned number
//     value range is 0 to FFFFFFFF
// If you enter a number above FFFFFFFF, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t UART_InUHex(void){
uint32_t number=0, digit, length=0;
char character;
  character = UART_InChar();
  while(character != CR){
    digit = 0x10; // assume bad
    if((character>='0') && (character<='9')){
      digit = character-'0';
    }
    else if((character>='A') && (character<='F')){
      digit = (character-'A')+0xA;
    }
    else if((character>='a') && (character<='f')){
      digit = (character-'a')+0xA;
    }
// If the character is not 0-9 or A-F, it is ignored and not echoed
    if(digit <= 0xF){
      number = number*0x10+digit;
      length++;
      UART_OutChar(character);
    }
// Backspace outputted and return value changed if a backspace is inputted
    else if((character==BS) && length){
      number /= 0x10;
      length--;
      UART_OutChar(character);
    }
    character = UART_InChar();
  }
  return number;
}

//--------------------------UART_OutUHex----------------------------
// Output a 32-bit number in unsigned hexadecimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1 to 8 digits with no space before or after
void UART_OutUHex(uint32_t number){
// This function uses recursion to convert the number of
//   unspecified length as an ASCII string
  if(number >= 0x10){
    UART_OutUHex(number/0x10);
    UART_OutUHex(number%0x10);
  }
  else{
    if(number < 0xA){
      UART_OutChar(number+'0');
     }
    else{
      UART_OutChar((number-0x0A)+'A');
    }
  }
}

//------------UART_InString------------
// Accepts ASCII characters from the serial port
//    and adds them to a string until <enter> is typed
//    or until max length of the string is reached.
// It echoes each character as it is inputted.
// If a backspace is inputted, the string is modified
//    and the backspace is echoed
// terminates the string with a null character
// uses busy-waiting synchronization on RDRF
// Input: pointer to empty buffer, size of buffer
// Output: Null terminated string
// -- Modified by Agustinus Darmawan + Mingjie Qiu --
void UART_InString(char *bufPt, uint16_t max) {
int length=0;
char character;
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
}

char* intToAscii(int in) {
	char* ret;
	int i=0;
	while(in!=0) {
		*ret = (in%10)+'0';
		ret++;
		i++;
		in=in/10;
	}
	ret-=i;
	return ret;
}


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
	else if(strcmp(bufPt,"derp")==0){
		UART_OutString("HEEEEEY");
		OutCRLF();
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
  //sr=UART_InUDec();
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


void promptNumber(char *bufPt, uint16_t max) {
int number=0;
int length=0;
char character;
	UART_OutString("Enter a Command: ");
  character = UART_InChar();
  //sr=UART_InUDec();
	while(character != CR){
		
		if((character>='0') && (character<='9')) {
      number = 10*number+(character-'0');   // this line overflows if above 4294967295
      length++;
      UART_OutChar(character);
    }
		
    else if(character == BS){
      if(length){
				bufPt--;
        length--;
				number /= 10;
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


void promptNumberV(char *bufPt, uint16_t max) {
int number=0;
int length=0;
char character;
	UART_OutString("Enter a number: ");
  character = UART_InChar();
  while(character != CR){ // accepts until <enter> is typed
// The next line checks that the input is a digit, 0-9.
// If the character is not 0-9, it is ignored and not echoed
    if((character>='0') && (character<='9')) {
      number = 10*number+(character-'0');   // this line overflows if above 4294967295
      length++;
      UART_OutChar(character);
    }
		
    else if(character == BS){
      if(length){
				bufPt--;
        length--;
				number /= 10;
        UART_OutChar(BS);
      }
    }
    else if(length < max){
      *bufPt = character;
      bufPt++;
      length++;
      UART_OutChar(character);
    }
    //character = UART_InChar();
    character = UART_InChar();
  }
	OutCRLF();

}

char string[20];

void Interpreter_CommandLine(void){
		
		promptCommand(string,19);
		//promptNumber(string,19);
		//promptNumber(string,19);
		
}

