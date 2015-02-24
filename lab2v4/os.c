// filename ************** OS.C *******************
// Created by Rob and Matt 2/9/15


#include "OS.h"

// oscilloscope or LED connected to PF2 for period measurement
 #include "tm4c123gh6pm.h"
 #include "SysTickInts.h"
 #include "PLL.h"
 #include "UART.h"
 #include "heap.h"

 
 volatile int periodicInterruptCount;

// each TCB is 100 words, 400 bytes
struct TCB{
  uint32_t *StackPt;       // Stack Pointer
  uint32_t MoreStack[11];  // 396 bytes of stack 
  uint32_t InitialReg[14]; // R4-R11,R0-R3,R12,R14
  uint32_t InitialPC;      // pointer to program to execute
  uint32_t InitialPSR;     // 0x01000000
};
typedef struct TCB TCBType;
TCBType *RunPt;            // thread currently running 

struct Node{
  struct Node *Next;        // circular linked list
  TCBType *ThreadPt;        // which thread to run
  uint32_t TimeSlice; // how long to run it
};

typedef struct Node NodeType;
NodeType *NodePt;
NodeType *head;
NodeType *tail;
volatile int numThreads=0;


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode


//void OS_Launch(unsigned long theTimeSlice);


void (*currentTask)(void); //make it a list for multithread


void OS_Init(void){
	periodicInterruptCount=0;
    // code below taken from PeriodicSysTickInts.c
    PLL_Init();                 // bus clock at 80 MHz
		//intialize systick timer for the thread switching
		
		SysTick_Init(80000000,3);//enable for lab2.1 testmain2
		Heap_Init();
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
int OS_AddThread(void (*task)(void), unsigned long stackSize, unsigned long priority){
	TCBType	*newTCB;
	NodeType *newNode;
	
	if(!head && !tail){//no threads currently, update the head and tail and set next to itself
		//newTCB = (TCBType){ &newTCB.InitialReg[0],{0},{0},(uint32_t) task,0x01000000};
		//newNode = (NodeType){&newNode,&newTCB,0};
		newTCB = (TCBType*) Heap_Malloc(sizeof(TCBType));
		
		newTCB->StackPt = newTCB->InitialReg;
		newTCB->InitialPC = (uint32_t)task;
		newTCB->InitialPSR = 0x01000000;
		
		newNode = (NodeType*) Heap_Malloc(sizeof(NodeType));
		newNode->Next = newNode;
		newNode->ThreadPt = newTCB;
		newNode->TimeSlice = 0;
		
		head = newNode;												//set the head to the front of the linked list
	  NodePt = head;      // Specify first thread to run 
		RunPt = NodePt->ThreadPt;
	} else {	//there are existing threads, so update the tail's next and the tail to be the new node
		//newTCB = (TCBType){ &newTCB.InitialReg[0],{0},{0},(uint32_t) task,0x01000000};	//new TCB 
		//newNode = (NodeType){head,&newTCB,0};	//last node in the linked list, next = head, tcb = new TCB
		newTCB = (TCBType*) Heap_Malloc(sizeof(TCBType));
		newTCB->StackPt = newTCB->InitialReg;
		newTCB->InitialPC = (uint32_t)task;
		newTCB->InitialPSR = 0x01000000;
		newNode = (NodeType*) Heap_Malloc(sizeof(NodeType));
		newNode->Next = head;
		newNode->ThreadPt = newTCB;
		newNode->TimeSlice = 0;
		
		tail->Next = newNode;									//update old last in the list's next
	}
	tail = newNode;											//set the tail to the newly added node
	
	if(newTCB&&newNode)
		return 1;
	
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
	  NVIC_INT_CTRL_R = 0x10000000;    // trigger PendSV 
}

// calls a thread with a periodic interrupt
// Inputs: task pointer to the function, period millisecond period to execute function, priority NVIC priority (0-7)
// Output: ??
int OS_AddPeriodicThread(void (*task) (void), unsigned long period, unsigned long priority){
   // SysTick_Init(period, priority); // Initizalize SysTick Timer 
    currentTask = task;
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
	return 0;
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void){
	return 0;
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
	return 0;
}

//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
unsigned long OS_Id(void){
	return 0;
}

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value){
	semaPt->Value = value;
} 

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
}

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(unsigned long data){
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
unsigned long OS_MailBox_Recv(void){
	return 0;
}

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void){
	return 0;
}
 
 // ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
	long status;
	status= StartCritical();
	semaPt->Value++;
	EndCritical(status);
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
		DisableInterrupts();
	}
	semaPt->Value=0;
	EnableInterrupts();
}




// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime){
} 

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void){
	return 0;
}


// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop){
	return 0;
}


