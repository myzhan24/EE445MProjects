// filename ************** OS.C *******************
// Created by Rob and Matt 2/9/15


// oscilloscope or LED connected to PF2 for period measurement
 #include "OS.h"
 #include "tm4c123gh6pm.h"
 #include "SysTickInts.h"
 #include "PLL.h"
 #include "UART.h"
 #include "heap.h"
 #include "Timer3A.h"
 #include "Timer1A.h"
 #include "FIFO.h"
 #include "Switch.h"
 #include "ADC.h"
 #include "ST7735.h"


#define MAIL_FR_RXFF            0x00000040  // UART Receive FIFO Full
#define MAIL_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define MAIL_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define MAIL_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define MAIL_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define MAIL_CTL_UARTEN         0x00000001  // UART Enable
#define MAIL_IFLS_RX1_8         0x00000000  // RX FIFO >= 1/8 full
#define MAIL_IFLS_TX1_8         0x00000000  // TX FIFO <= 1/8 full
#define MAIL_IM_RTIM            0x00000040  // UART Receive Time-Out Interrupt
                                            // Mask
#define MAIL_IM_TXIM            0x00000020  // UART Transmit Interrupt Mask
#define MAIL_IM_RXIM            0x00000010  // UART Receive Interrupt Mask
#define MAIL_RIS_RTRIS          0x00000040  // UART Receive Time-Out Raw
                                            // Interrupt Status
#define MAIL_RIS_TXRIS          0x00000020  // UART Transmit Raw Interrupt
                                            // Status
#define MAIL_RIS_RXRIS          0x00000010  // UART Receive Raw Interrupt
                                            // Status
#define MAIL_ICR_RTIC           0x00000040  // Receive Time-Out Interrupt Clear
#define MAIL_ICR_TXIC           0x00000020  // Transmit Interrupt Clear
#define MAIL_ICR_RXIC           0x00000010  // Receive Interrupt Clear


#define FIFOSIZE   16         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure

#define SYSTICKPERIOD 					1000000
#define	OSTIMERPERIOD						0xffffffff
#define OSTIMERTICK							53687091.1875 //a whole period is this many microseconds

//mail fifos and semaphores
	AddIndexFifo(Rmail, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)
	AddIndexFifo(Tmail, FIFOSIZE, char, FIFOSUCCESS, FIFOFAIL)
			
	Sema4Type TmailFifoLock;
	Sema4Type RmailFifoLock;


	                              //    red, yellow,    green, light blue, blue, purple,   white,          dark
const long COLORWHEEL[WHEELSIZE] = {RED, RED+GREEN, GREEN, GREEN+BLUE, BLUE, BLUE+RED, RED+GREEN+BLUE, 0};

int ci = 0;
	
TCBType *RunPt;            		// thread currently running 
NodeType *NodePt;							//current thread node

NodeType *head;								//head of linked list, used for initialization of add threads
NodeType *tail;								//tail of linked list, used for initialization of add threads
volatile int numThreads=0;

	
//volatile int *killSwitch;
int SysTickPeriod = 0;
volatile int periodicInterruptCount;
volatile int rtCounter;				// real time clock counter to keep track of the number of timer interrupts 
volatile int RTC;							// real time clock value in MS
	
// Mailbox semaphores
Sema4Type DataValid;    //0 == mailbox empty
Sema4Type BoxFree;	    // 1 == MailBox free

//Mailbox data
volatile int mailbox;

// Fifo semaphores
Sema4Type DataAvailable;
Sema4Type DataRoomLeft;
Sema4Type FifoAvailable;



void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode


//void OS_Launch(unsigned long theTimeSlice);


void (*currentTask)(void); //make it a list for multithread
void (*swTask)(void);

void Color_Init(void){ unsigned long volatile delay;	//for pe2
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
  delay = SYSCTL_RCGC2_R;          // allow time to finish activating
  GPIO_PORTF_DIR_R |= 0x0E;        // make PF3-1 output (PF3-1 built-in LEDs)
  GPIO_PORTF_AFSEL_R &= ~0x0E;     // disable alt funct on PF3-1
  GPIO_PORTF_DEN_R |= 0x0E;        // enable digital I/O on PF3-1
                                   // configure PF3-1 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFF000F)+0x00000000;
  GPIO_PORTF_AMSEL_R = 0;          // disable analog functionality on PF
  LEDS = 0;                        // turn all LEDs off
//  Timer3_Init(&UserTask, 4000);    // initialize timer3 (20,000 Hz)
//  Timer3_Init(&UserTask, 80000000);// initialize timer3 (1 Hz)
//  Timer3_Init(&UserTask, 0xFFFFFFFF); // initialize timer23 (slowest rate)
}

void ColorSwap(){
	LEDS = COLORWHEEL[ci&(WHEELSIZE-1)];
	ci++;
}

void OS_Init(void){
		DisableInterrupts();
		periodicInterruptCount = 0;

    // code below taken from PeriodicSysTickInts.c
    PLL_Init();                 // bus clock at 80 MHz
		ST7735_InitR(INITR_REDTAB);
		Timer1A_Init(OSTIMERPERIOD);
		SysTick_Init(SYSTICKPERIOD,6);//enable for lab2.1 testmain2
		
		//intialize systick timer for the thread switching
		//Timer5A_Init(0xffffffff);
		
		//Timer0A_Init((50000000/16));
	//	Color_Init();
		//ADC_Init(2);
		
		//Board_Init();
		Heap_Init();
		//OS_Fifo_Init(FIFOSIZE);
		UART_Init();
		
    EnableInterrupts();
}




//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
//	new threads are always added in ascending order, based on thread id, head will always point to the lowest and tail will always point to the highest
int OS_AddThread(void (*task)(void), unsigned long stackSize, unsigned long priority){
	//int status = StartCritical();
	DisableInterrupts();
	TCBType	*newTCB;
	NodeType *newNode;
	
	
	if(!head && !tail){//no threads currently, update the head and tail and set next to itself
		newTCB = (TCBType*) Heap_Malloc(sizeof(TCBType));
		
		newTCB->StackPt = newTCB->InitialReg;
		newTCB->InitialPC = (uint32_t)task;
		newTCB->InitialPSR = 0x01000000;
		newTCB->kill = 0;
		
		newNode = (NodeType*) Heap_Malloc(sizeof(NodeType));
		newNode->Next = newNode;
		newNode->Prev = newNode;
		newNode->ThreadPt = newTCB;
		newNode->TimeSlice = 0;
		newNode->Sleep = 0;
		
		head = newNode;												//set the head to the front of the linked list
		//SNodePt = head;
	  NodePt = head;      // Specify first thread to run 
		RunPt = NodePt->ThreadPt;
	} else {	//there are existing threads, so update the tail's next and the tail to be the new node
		if(!tail) {//tail was cutoff at some point, find next tail
			tail=head;
			while(tail->Next != head)
				tail=tail->Next;
		}
			
		newTCB = (TCBType*) Heap_Malloc(sizeof(TCBType));
		newTCB->StackPt = newTCB->InitialReg;
		newTCB->InitialPC = (uint32_t)task;
		newTCB->InitialPSR = 0x01000000;
		newTCB->kill = 0;
		newNode = (NodeType*) Heap_Malloc(sizeof(NodeType));
		newNode->Next = head;
		newNode->ThreadPt = newTCB;
		newNode->TimeSlice = 0;
		newNode->Sleep = 0;
		newNode->Prev = tail;							//update the new node's prev to the outdated tail
		
		head->Prev = newNode;							//head points prev to the new last
		tail->Next = newNode;							//update old last in the list's next
	}
	tail = newNode;											//set the tail to the newly added node

	
	
	if(newTCB&&newNode){
		numThreads++;
		newTCB->id = numThreads;
		EnableInterrupts();
		//EndCritical(status);
		return 1;
	}
	EnableInterrupts();
	//EndCritical(status);
	return 0;
}

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
	//  NVIC_INT_CTRL_R |= 0x10000000;    // trigger PendSV 
	NVIC_ST_CURRENT_R = 0;
	NVIC_INT_CTRL_R |= 0x04000000;  //trigger SYSTICK Interrupt
}

// calls a thread with a periodic interrupt
// Inputs: task pointer to the function, period millisecond period to execute function, priority NVIC priority (0-7)
// Output: ??
int OS_AddPeriodicThread(void (*task) (void), unsigned long period, unsigned long priority){
  //int status = StartCritical();
	DisableInterrupts();
	//Timer3A_Init((50000000/16)); 
	//Timer3A_Init(period);
	Timer3A_Init(task,period);
	//EndCritical(status);
	EnableInterrupts();
	return 1;

}

void OS_ClearPeriodicTime(void){
    periodicInterruptCount = 0;
}

unsigned long OS_ReadPeriodicTime(void){
    return periodicInterruptCount;
}

//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), unsigned long priority){
	int status = StartCritical();
	Board_Init(task);
	EndCritical(status);
	return 1;
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
	RTC=0;
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void){
    char output;
    OS_Wait(&DataAvailable);
    OS_bWait(&FifoAvailable);
    TmailFifo_Get(&output);
    OS_bSignal(&FifoAvailable);
    OS_Signal(&DataRoomLeft);
		return output;
}

// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(unsigned long size){
    TmailFifo_Init();
    OS_InitSemaphore(&DataAvailable, 0);
    OS_InitSemaphore(&DataRoomLeft, size);
    OS_InitSemaphore(&FifoAvailable, 1);
}


// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data){
    int value;
//    OS_bWait(&FifoAvailable);
    value = TmailFifo_Put(data);
//    OS_bSignal(&FifoAvailable);
    return value;
}


//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
unsigned long OS_Id(void){
	return RunPt->id;
}

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value){
	semaPt->Value = value;
} 


void OS_RemoveCurrentThread(){
	int status = StartCritical();

	TCBType *currentTCB;
	NodeType *currentNode;
	

	//first release the TCB's stack, then the TCB, then the Node pointing to the TCB
	//since the stack is a defined size in the struct, there is no need to free it individually
	if(head==NodePt){//if head is being killed, then the head will go to the next element in the linked list
		if(numThreads>1)		//at least one thread after the kill
			head = NodePt->Next;
		else
			head = 0;
	}
	//if tail is being killed, then the tail will be updated on the next addthread call
	if(tail==NodePt){
		tail=0;
	}
	
	currentTCB = RunPt;
	currentNode = NodePt;
	currentNode->Prev->Next = NodePt->Next; // o -> x -> o 	x is being swapped out
	currentNode->Next->Prev = NodePt->Prev;				// o <- x <- o 	x is being swapped out
	


	Heap_Free(currentTCB);
	Heap_Free(currentNode);

	NodePt = currentNode->Prev;
	RunPt = NodePt->ThreadPt;
	
	numThreads--;
	EndCritical(status);
}

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
	RunPt->kill=1;			//Thread is Marked for Deletion.
	OS_Suspend();				//PendSV will check this 
}

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
    OS_InitSemaphore(&DataValid, 0);
    OS_InitSemaphore(&BoxFree, 1);
}
// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(unsigned long data){
    OS_bWait(&BoxFree);
    mailbox = data;
    OS_bSignal(&DataValid);
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
unsigned long OS_MailBox_Recv(void){
    unsigned long data;
    OS_bWait(&DataValid);
    data = mailbox;
    OS_bSignal(&BoxFree);
    return data;
}

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void){
	int ustime;
	int sr = StartCritical();
	ustime = OS_Time();
	EndCritical(sr);
	return ustime/1000;
}
 
 // ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
	int status;
	status= StartCritical();
	semaPt->Value++;
	EndCritical(status);
	//OS_Suspend();
}


// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
	DisableInterrupts();
	while(semaPt->Value <= 0){
		EnableInterrupts();
	//ColorSwap();
		OS_Suspend();
		DisableInterrupts();
	}
	semaPt->Value--;
	EnableInterrupts();
}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
	long status;
	status= StartCritical();
	semaPt->Value=1;
	EndCritical(status);
}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
	DisableInterrupts();
	while(semaPt->Value <= 0){
		EnableInterrupts();
		OS_Suspend();
		DisableInterrupts();
	}
	semaPt->Value=0;
	EnableInterrupts();
}

int OS_SleepCheck(NodeType* node) {
	int sr = StartCritical();
	int currentTime = OS_Time();
	int sleepTime = node->Sleep;
	EndCritical(sr);
	if(!sleepTime){
		return 0;			//not sleeping
		
	}
	else if(currentTime < sleepTime){		//still sleeping
		node->Sleep = 0;
		return -1;
	}
	
	return 1;
}

void OS_SleepClear(NodeType* node) {
	node->Sleep = 0;
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime){
	//1 msec = 1000000 nanosec
	//1 msec = 80000 bus cycles
	//
	//since the systick period is 12.5 * period ns, 
	//RunPt->Sleep = sleepTime * 80000;		//set the thread to sleep, will decrement with systick
	//NodePt->Sleep = sleepTime * 80000;
	//NodePt->Sleep = sleepTime;
	
	//1 milliseconds to 1000 micro seconds
	int status = StartCritical();
	int currentTime = OS_Time();
	EndCritical(status);
	NodePt->Sleep = (sleepTime*1000)+currentTime;
	OS_Suspend();
} 

unsigned int dumpt[100];
int dumpti = 0;

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: RTC in us  										time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void){
	
	
	//every tick of the timer is 12.5 ns worth of time, to make 1 ms = 1000000 ns, w
	
	int status = StartCritical();
	RTC += rtCounter*OSTIMERTICK + (0.0125 * (TIMER1_TAILR_R-TIMER1_TAV_R));
	rtCounter=0;
	EndCritical(status);
	return RTC;	//SysTick Current Time
}


// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop){
	signed long diff = start - stop;
	if(diff<0)
		return (-1*diff);		//if this happens, and entire reload occurred. stop should always be lower than start
	
	return diff;
}


