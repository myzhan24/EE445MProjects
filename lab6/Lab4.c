#include "OS.h"
#include "tm4c123gh6pm.h"
#include "ST7735.h"
#include "ADC.h"
#include "UART.h"
#include "PLL.h"
#include "Timer3A.h"
#include "Interpreter.h"
#include "heap.h"
#include "Filter.h"
#include "PWM.h"
#include <string.h> 
#include <math.h>


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

//#define FFT
#define PE0  (*((volatile unsigned long *)0x40024004))
#define PE1  (*((volatile unsigned long *)0x40024008))
#define PE2  (*((volatile unsigned long *)0x40024010))
#define PE3  (*((volatile unsigned long *)0x40024020))
#define dataN 1
#define Samples 256

long xx[Samples+51];
double yy[Samples+51];
double imaginary[Samples];
double magnitude[Samples];
//double yy[128*4];

//long *x;
//double *y;
//double *im;
//double *mag;
long adc_count;

void PortE_Init(void){ unsigned long volatile delay;
  SYSCTL_RCGCGPIO_R |= 0x10;       // activate port E
  delay = SYSCTL_RCGCGPIO_R;        
  delay = SYSCTL_RCGCGPIO_R;         
  GPIO_PORTE_DIR_R |= 0x0F;    // make PE3-0 output heartbeats
  GPIO_PORTE_AFSEL_R &= ~0x0F;   // disable alt funct on PE3-0
  GPIO_PORTE_DEN_R |= 0x0F;     // enable digital I/O on PE3-0
  GPIO_PORTE_PCTL_R = ~0x0000FFFF;
  GPIO_PORTE_AMSEL_R &= ~0x0F;      // disable analog functionality on PF
}

void interp(void) {
	while (1) {
		Interpreter_CommandLine();
	}
}

int u;
int j;
void transfer(unsigned long data) {
	int i=0;
	
	i=OS_Fifo_Put(data);
	if(i==0)
	{
		u++;
	}
	j++;
}

void filterWork(void) {
	while(1) {
		xx[adc_count] = OS_Fifo_Get();
	//UART_OutString("output: ");
	//UART_OutUDec(xx[adc_count]);
	//UART_OutCRLF();
		//ST7735_Message(1,2,"Output: ",xx[adc_count]);
		adc_count++;

		if (adc_count == 128+51) {
			Filter_51Work(xx, yy, adc_count);
			#ifdef FFT
			//frequency(xx+51, 128);
			ST7735_PlotArrayVF(yy+51, 25,0,3000);
			#endif 
			adc_count = 0;
		}

		
	}
}



double findMaxOf(double* a, int len){
	int i;
	double ret = 0;
	for(i=0;i<len;i++){
		if(a[i]>ret)
			ret = a[i];
	}
	return ret;
}

double findMinOf(double* a, int len){
	int i;
	double ret = 99999999;
	for(i=0;i<len;i++){
		if(a[i]<ret)
			ret = a[i];
	}
	return ret;
}

int determineDirection(double* a, int len){
	int i = 0;
	int dir=0;
	double current;
	if(a[0] < 0)
		return 1;
	
	else if (a[0] > 0)
		return 0;
	
	else
	for(i=1;i<len;i++){
		if(a[0] < 0)
			return 1;
		
		else if (a[0] > 0)
			return 0;
	}
	
	return -1;
}

double findAmplitudeOf(double* a, int len){
	return findMaxOf(a,len) - findMinOf(a,len);
}

double findMaxOfLong(long* a, int len){
	int i;
	double ret = 0;
	for(i=0;i<len;i++){
		if(a[i]>ret)
			ret = a[i];
	}
	return ret;
}

double findMinOfLong(long* a, int len){
	int i;
	double ret = 99999999;
	for(i=0;i<len;i++){
		if(a[i]<ret)
			ret = a[i];
	}
	return ret;
}

double findAmplitudeOfLong(long* a, int len){
	double ret;
	ret = findMaxOfLong(a,len) - findMinOfLong(a,len);
	return ret;
}


/*
double findAverageGain(long *a, double* b, int len){
	double ret;
	int i;
	double bb;
	long aa;
	for(i=0; i < len; i++){
		aa = a[i];
		bb = b[i];
		ret += abs(bb/aa);
	}
	ret = ret/(len);
	return ret;
	
}*/




double findGain(long * a, double * b, int len){
	double ret;
	ret = findAmplitudeOf(b,len) / findAmplitudeOfLong(a,len);
	return ret;
}
	


void filterWorkVVF(void) {
	long gain;
	unsigned long gainer;
	while(1) {
		
		xx[adc_count] = OS_Fifo_Get();
		adc_count++;
		if(adc_count == Samples+51){
			Filter_51Work(xx, yy, adc_count);									//xx [_51_|__256___]  yy [_51_|__256___]  
			//gain = findGain(xx+51, yy+51, 256);
			FFT(1,8,yy+51,imaginary);													//FFT of 256 samples will give 128 plot points at 50 Hz resolution
			complexMagnitude(magnitude,yy+51,imaginary,Samples);	//yy [XXX|__256___] 		//magnitude[__+f>__N/2__<-f__]  
																																								//         f=0     f=N/2T   f=-1/T
			ST7735_PlotArrayVFDub(magnitude, 1,0,400);				//plots 128 data points of positive magnitude, min 0, max 6000
			//ST7735_PlotArrayVFDub(yy+51, 1,-3000,3000);
			//ST7735_PlotArrayVT(xx, 1,-3000,3000);
			adc_count = 0;
			
			//OS_Sleep(1000);
		}
	}
}


void filterWorkVVT(void) {
	double amp;
	while(1) {
	
		xx[adc_count] = OS_Fifo_Get();
		adc_count++;
		if(adc_count == Samples){
	
			ST7735_PlotArrayVT(xx, 2,0,3000);
			adc_count = 0;
			//amp = findAmplitudeOfLong(xx,256);
		}
		
		/*if(adc_count == Samples){
			adc_count = 0;
			UART_OutChar('X');
			UART_OutCRLF();
			UART_OutCRLF();
			UART_OutCRLF();
		}*/
	}
}



void softwareSampling(void) {
	while(1) {
		unsigned long data = ADC_In();
		OS_Fifo_Put(data);	
	}
}

void dummy(void) {
	static int i = 0;
	while (1) {
		i++;
		
	}
}


//hpf removes DC component, then a 1.36V bias is added

//lab 6 main for the motor, runs interpreter
int main(void){
	OS_Init();
	Motor_Init();
	OS_AddThread(&interp, 128, 3);
	OS_AddThread(&dummy, 128, 4);

  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
	return 0;
}

int lab4main(void) {
	OS_Init();           // initialize, disable interrupts
  PortE_Init();
	//PortF_Init();
	u=0;
	j=0;
	
  
//********initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(64);    // ***note*** 4 is not big enough*****
	//ADC_Init(5);
	//x = Heap_Calloc((Samples+51)*4);
	//y = Heap_Calloc((Samples+51)*8);
	//mag = Heap_Calloc(Samples*8);
	//im = Heap_Calloc(Samples*8);
	adc_count = 0;
	
//*******attach background tasks***********
	OS_AddThread(&interp, 128, 3);
	OS_AddThread(&dummy, 128, 4);
	//ADC_Init(5);
	//ADC_Collect(5, 12800, &transfer);
	//OS_AddThread(&filterWorkFFT, 128, 2);
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}


void Printer(unsigned long data){
	//ST7735_PlotPointVT(data,0,3000);
	ST7735_PlotPointVF(data,0,1500);
}

void ADCTASK(void){
	int i;
	while(1){
		ADC_Collect(5, 12800, &Printer);
		
	}
}


//adc collect tester adc test main
int adcmain(void){
	OS_Init();           // initialize, disable interrupts
  ADC_Init(5);  // sequencer 3, channel 4, PD3, sampling in DAS()
	OS_AddThread(&ADCTASK,128,1); 
	//ADC_Collect(5, 12800, &transfer);
	//OS_AddThread(&filterWork,128,1);
	//ADC_Collect(5, 12800, &Printer);
	//OS_AddThread(&dummy,128,1);
	
	OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}

