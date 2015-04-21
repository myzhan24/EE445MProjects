// filename ************** OS.C *******************
// Created by Rob and Matt 2/9/15


#include "OS.h"

// oscilloscope or LED connected to PF2 for period measurement
 #include "tm4c123gh6pm.h"
 #include "SysTickInts.h"
 #include "PLL.h"
 #include "UART.h"
 #include "heap.h"
 #include "Timer3A.h"
 #include "Timer1A.h"
 #include "Switch.h"
 #include "FIFO.h"
 #include "ST7735.h"
 
 
#define FIFOSIZE   512         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure

#define SYSTICKPERIOD 					1000000
#define	OSTIMERPERIOD						0xffffffff
#define OSTIMERTICK							53687091.1875 //a whole period is this many microseconds
#define SYSTICKPRIORITY					7
 
 volatile int periodicInterruptCount;


TCBType *RunPt;            		// thread currently running 
NodeType *NodePt;							//current thread node
//NodeType *SNodePt;						//current SystickThreadNode
//NodeType TempSNode;						//copy of the current foreground node
NodeType *head;								//head of linked list, used for initialization of add threads
NodeType *tail;								//tail of linked list, used for initialization of add threads
volatile int numThreads=0;
NodeType *PeriodicPt;
TCBType *PeriodicTCB;
NodeType *PeriodicHead;
NodeType *PeriodicTail;
NodeType TempNode;						//copy of the current foreground node
NodeType *TempNodePt;

// Mailbox semaphores
Sema4Type DataValid;    //0 == mailbox empty
Sema4Type BoxFree;	    // 1 == MailBox free

//Mailbox data
volatile int mailbox;

// Fifo semaphores
Sema4Type DataAvailable;
Sema4Type DataRoomLeft;
Sema4Type FifoAvailable;

volatile unsigned int RTC;
volatile unsigned int MSRTC;
volatile unsigned int rtCounter;

AddIndexFifo(Rmail, FIFOSIZE, int, FIFOSUCCESS, FIFOFAIL)
AddIndexFifo(Tmail, FIFOSIZE, int, FIFOSUCCESS, FIFOFAIL)
	
Sema4Type TmailFifoLock;
Sema4Type RmailFifoLock;


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode


//void OS_Launch(unsigned long theTimeSlice);


void (*currentTask)(void); //make it a list for multithread
void (*swTask)(void);


void OS_Init(void){
	
	
	DisableInterrupts();
		periodicInterruptCount = 0;
		RTC = 0;
		rtCounter=0;
    // code below taken from PeriodicSysTickInts.c
    PLL_Init();                 // bus clock at 80 MHz
		Output_Init();
		//ST7735_InitR(INITR_REDTAB);
		
		
		Heap_Init();
		Timer1A_Init(OSTIMERPERIOD);
		SysTick_Init(SYSTICKPERIOD,SYSTICKPRIORITY);//enable for lab2.1 testmain2
		Timer3A_Init();
		
		//intialize systick timer for the thread switching
		//Timer5A_Init(0xffffffff);
		
		//Timer0A_Init((50000000/16));
	//	Color_Init();
		//ADC_Init(2);
		
		//Board_Init();
		
		//OS_Fifo_Init(FIFOSIZE);
		
		UART_Init();
    EnableInterrupts();
}

void OS_Init2(void){
	DisableInterrupts();
		periodicInterruptCount = 0;
		RTC = 0;
		rtCounter=0;
    // code below taken from PeriodicSysTickInts.c
    PLL_Init();                 // bus clock at 80 MHz
		
		SysTick_Init(SYSTICKPERIOD,SYSTICKPRIORITY);//enable for lab2.1 testmain2
		ST7735_InitR(INITR_REDTAB);
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
	int status = StartCritical();
	//Timer3A_Init(task,period); 
	unsigned long static count = 0;
		
	TCBType	*newTCB = (TCBType*) Heap_Calloc(sizeof(*newTCB));
	NodeType *newNode  = (NodeType*) Heap_Calloc(sizeof(*newNode));
	if (newNode) {
		newNode->id = count++;
	}
	newTCB->StackPt = newTCB->InitialReg;
	newTCB->InitialPC = (uint32_t)task;
	newTCB->InitialPSR = 0x01000000;
	newNode->ThreadPt = newTCB;
	newNode->TimeSlice = 0;
	newNode->Priority = priority;
	
	if(!head){//no threads currently, update the PeriodicHead and PeriodicTail and set next to itself
		

	 // NodePt = newNode;      // Specify first thread to run 
	//	RunPt = NodePt->ThreadPt;
	//	NodePt->Next = NodePt;
	//	NodePt->Prev = NodePt;
		newNode->Next = newNode;
		newNode->Prev = newNode;
		head = newNode;
		tail= newNode;
	} else {	//there are existing threads, so update the PeriodicTail's next and the PeriodicTail to be the new node
		
			
			NodeType *tempNode = head;
			if (newNode->Priority < tempNode->Priority) {
				newNode->Next = tempNode;
				newNode->Prev = tempNode->Prev;
				tempNode->Prev->Next = newNode;
				tempNode->Prev = newNode;
				head = newNode;
				tail->Next = newNode;
				//RunPt = NodePt->ThreadPt;
			}
			else {
				int done = 0;
				while (!done) {
					if (newNode->Priority < tempNode->Next->Priority) {
						newNode->Next = tempNode->Next;
						tempNode->Next->Prev = newNode;
						tempNode->Next = newNode;
						newNode->Prev = tempNode;
						done = 1;
					}
					else {
						if (tempNode->Next == head) {
							tempNode->Next = newNode;
							newNode->Prev = tempNode;
							newNode->Next = head;
							head->Prev = newNode;
							tail = newNode;
							done = 1;
						}
						if (tempNode->Next == tail) {
							tempNode = tempNode->Next;
							tempNode->Next = newNode;
							newNode->Prev = tempNode;
							newNode->Next = head;
							head->Prev = newNode;
							tail=newNode;
							done = 1;
						}
						tempNode = tempNode->Next;
					}
				}
				
			}

			
	}

	
	if(newTCB&&newNode){
		EndCritical(status);
		numThreads++;
		return 1;
	}
	EndCritical(status);
	return 0;
	
}


/*int OS_AddThread(void (*task)(void), unsigned long stackSize, unsigned long priority){

	int status = StartCritical();
	//DisableInterrupts();
	TCBType	*newTCB;
	NodeType *newNode;
	
	
	if(!head && !tail){//no threads currently, update the head and tail and set next to itself
		newTCB = (TCBType*) Heap_Malloc(sizeof(TCBType));
		newNode = (NodeType*) Heap_Malloc(sizeof(NodeType));
		
		if(newTCB && newNode){
			
		newTCB->StackPt = newTCB->InitialReg;
		newTCB->InitialPC = (uint32_t)task;
		newTCB->InitialPSR = 0x01000000;
		newTCB->kill = 0;
		
		
		newNode->Next = newNode;
		newNode->Prev = newNode;
		newNode->ThreadPt = newTCB;
		newNode->TimeSlice = 0;
		newNode->Sleep = 0;
		
		head = newNode;												//set the head to the front of the linked list
		//SNodePt = head;
	  NodePt = head;      // Specify first thread to run 
		RunPt = NodePt->ThreadPt;
		tail = newNode;											//set the tail to the newly added node
			numThreads++;
		newTCB->id = numThreads;
			
		} else {
			Heap_Free(newTCB);
			Heap_Free(newNode);
		}
		
	} else {	//there are existing threads, so update the tail's next and the tail to be the new node
		if(!tail) {//tail was cutoff at some point, find next tail
			tail=head;
			while(tail->Next != head)
				tail=tail->Next;
		}
			
		newTCB = (TCBType*) Heap_Malloc(sizeof(TCBType));
		newNode = (NodeType*) Heap_Malloc(sizeof(NodeType));
		
		
		if(newTCB && newNode){
		
		
			
			
			
			
			
		newTCB->StackPt = newTCB->InitialReg;
		newTCB->InitialPC = (uint32_t)task;
		newTCB->InitialPSR = 0x01000000;
		newTCB->kill = 0;
			
		newNode->Next = head;
		newNode->ThreadPt = newTCB;
		newNode->TimeSlice = 0;
		newNode->Sleep = 0;
		newNode->Prev = tail;							//update the new node's prev to the outdated tail
		
		head->Prev = newNode;							//head points prev to the new last
		tail->Next = newNode;							//update old last in the list's next
		tail = newNode;											//set the tail to the newly added node
			numThreads++;
		newTCB->id = numThreads;
			
		} else {
			Heap_Free(newTCB);
			Heap_Free(newNode);
		}
	}
	

	
	
	if(newTCB&&newNode){
		
		//EnableInterrupts();
		EndCritical(status);
		return 1;
	}
	//EnableInterrupts();
	EndCritical(status);
	return 0;
}
*/
// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
	  NVIC_INT_CTRL_R |= 0x10000000;    // trigger PendSV 
	WaitForInterrupt();
}

// calls a thread with a periodic interrupt
// Inputs: task pointer to the function, period millisecond period to execute function, priority NVIC priority (0-7)
// Output: ??f
int OS_AddPeriodicThread(void (*task) (void), unsigned long period, unsigned long priority){
  int status = StartCritical();
	//Timer3A_Init(task,period); 
	unsigned long static count = 0;
		
	TCBType	*newTCB = (TCBType*) Heap_Calloc(sizeof(*newTCB));
	NodeType *newNode  = (NodeType*) Heap_Calloc(sizeof(*newNode));
	if (newNode) {
		newNode->id = count++;
	}
	
	newTCB->StackPt = newTCB->InitialReg;
	newTCB->InitialPC = (uint32_t)task;
	newTCB->InitialPSR = 0x01000000;
	newNode->ThreadPt = newTCB;
	newNode->TimeSlice = period;
	newNode->TimeLeft = period;
	newNode->Priority = priority;
	
	if(!PeriodicPt){//no threads currently, update the PeriodicHead and PeriodicTail and set next to itself
	
	  PeriodicPt = newNode;      // Specify first thread to run 
		PeriodicTCB = PeriodicPt->ThreadPt;
	} else {	//there are existing threads, so update the PeriodicTail's next and the PeriodicTail to be the new node
		
			
		
		

				
			NodeType *tempNode = PeriodicPt;
			if (newNode->Priority < tempNode->Priority) {
				newNode->Next = tempNode;
				PeriodicPt = newNode;
			}
			else {
				int done = 0;
			
				
				while (!done) {
					if (newNode->Priority < tempNode->Next->Priority) {
						newNode->Next = tempNode->Next;
						tempNode->Next = newNode;
						done = 1;
					}
					else {
						if (tempNode->Next == 0) {
							tempNode->Next = newNode;
							done = 1;
						}
						tempNode = tempNode->Next;
					}
				}
				
			}
			
			
	}
	//PeriodicTail = newNode;											//set the PeriodicTail to the newly added node
	#ifdef DEBUG
		
	#endif
	
	if(newTCB&&newNode){
		EndCritical(status);
		return 1;
	}
	EndCritical(status);
	return 0;
	

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


//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
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
int OS_AddSW2Task(void(*task)(void), unsigned long priority){
	int status = StartCritical();
	Board_Init2(task);
	EndCritical(status);
	return 1;
}


// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void){
    int output;
		int sr;
    OS_Wait(&DataAvailable);
    OS_bWait(&FifoAvailable);
    TmailFifo_Get(&output);
    OS_bSignal(&FifoAvailable);
    //OS_Signal(&DataRoomLeft);
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
	int ret; int sr;
	//OS_Wait(&DataRoomLeft);
  //OS_bWait(&FifoAvailable);
	//sr = StartCritical();
	ret = 0;
	if(FifoAvailable.Value){
	
	
	ret = TmailFifo_Put(data);
	
	if(ret)
		OS_Signal(&DataAvailable);
	
	//OS_bSignal(&FifoAvailable);
	}
	//EndCritical(sr);
	return ret;
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

uint32_t* findSaluditatoryTail(){
	uint32_t* saludID;
	
	
	return saludID;
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
		tail= NodePt->Prev;
	}
	
	currentTCB = RunPt;
	currentNode = NodePt;
	currentNode->Prev->Next = NodePt->Next; // o -> x -> o 	x is being swapped out
	currentNode->Next->Prev = NodePt->Prev;				// o <- x <- o 	x is being swapped out
	

	NodePt = currentNode->Prev;
	RunPt = NodePt->ThreadPt;

	Heap_Free(currentTCB);
	Heap_Free(currentNode);
	
	numThreads--;
	EndCritical(status);
}


// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
	RunPt->kill=1;			//Thread is Marked for Deletion.
	while(1) {
	OS_Suspend();				//PendSV will check this 
	}
	
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
 
 // ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
	long status;
	//status= StartCritical();
	semaPt->Value++;
	if (semaPt->Value > 0 && semaPt->BlockedNodePt) {
		semaPt->BlockedNodePt->NodePt->Blocked = 0;
		(semaPt->Value)--;
		BlockedNodeType *temp = semaPt->BlockedNodePt->Next;
		Heap_Free(semaPt->BlockedNodePt);
		semaPt->BlockedNodePt = temp;
	}
	
	//EndCritical(status);
}


// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
	long status;
	
	
		if (semaPt->Value < 1) {
			status = StartCritical();
	    NodePt->Blocked = 1;
	    BlockedNodeType *newBlocked = Heap_Calloc(sizeof(*newBlocked)); //check if valid pointer
	    newBlocked->NodePt = NodePt;
	    if (!semaPt->BlockedNodePt) {
				semaPt->BlockedNodePt = newBlocked;
	    }
	    else if (semaPt->BlockedNodePt->NodePt->Priority > NodePt->Priority) {
				newBlocked->Next = semaPt->BlockedNodePt;
				semaPt->BlockedNodePt = newBlocked;
	    }
	    else {
				BlockedNodeType *tempBlocked = semaPt->BlockedNodePt;
				int done = 0;
				while (!done) {
					if (!tempBlocked->Next) {
							tempBlocked->Next = newBlocked;
							done = 1;
						}
					else if (newBlocked->NodePt->Priority < tempBlocked->Next->NodePt->Priority) {
						newBlocked->Next = tempBlocked->Next;
						tempBlocked->Next = newBlocked;
						done = 1;
					}
					else {
						tempBlocked = tempBlocked->Next;
					}
				}
				
			}
			//status = StartCritical();
			/*if(semaPt->Value > 0 && semaPt->BlockedNodePt == newBlocked){
				semaPt->Value--;		
				OS_Signal(semaPt);
			}*/
			EndCritical(status);
			
			//EndCritical(status);
	}
		else {
			(semaPt->Value)--;
		}
	
	
	
	//EndCritical(status);
	while(NodePt->Blocked) {
		OS_Suspend();
	}
}

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
	long status;
	//status= StartCritical();
	semaPt->Value = 1;
	if (semaPt->BlockedNodePt) { //if thread waiting
		semaPt->BlockedNodePt->NodePt->Blocked = 0;
		semaPt->Value = 0;
		BlockedNodeType *temp = semaPt->BlockedNodePt->Next;
		Heap_Free(semaPt->BlockedNodePt);
		semaPt->BlockedNodePt = temp;
	}
	//EndCritical(status);
}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
	long status;
	
		if (semaPt->Value < 1) {
			status = StartCritical();
	    NodePt->Blocked = 1;
	    BlockedNodeType *newBlocked = Heap_Calloc(sizeof(*newBlocked)); //check if valid pointer
	    newBlocked->NodePt = NodePt;
	    if (!semaPt->BlockedNodePt) {
				semaPt->BlockedNodePt = newBlocked;
	    }
	    else if (semaPt->BlockedNodePt->NodePt->Priority > NodePt->Priority) {
				newBlocked->Next = semaPt->BlockedNodePt;
				semaPt->BlockedNodePt = newBlocked;
	    }
	    else {
				BlockedNodeType *tempBlocked = semaPt->BlockedNodePt;
				int done = 0;
				while (!done) {
					if (!tempBlocked->Next) {
							tempBlocked->Next = newBlocked;
							done = 1;
						}
					else if (newBlocked->NodePt->Priority < tempBlocked->Next->NodePt->Priority) {
						newBlocked->Next = tempBlocked->Next;
						tempBlocked->Next = newBlocked;
						done = 1;
					}
					else {
						tempBlocked = tempBlocked->Next;
					}
				}
				
			}
			EndCritical(status);
	}
	
	else {
		(semaPt->Value)= 0;
	}
	//EndCritical(status);
	while(NodePt->Blocked) {
		OS_Suspend();
	}
}


// will check the sleep status of a node and update the sleep value if its sleep time is passed
// -1
int OS_SleepCheck(NodeType* node) { //modified to check for blocked nodes
	int sr = StartCritical();
	int currentTime = OS_Time();
	int sleepTime = node->Sleep;
	int blocked = node->Blocked;
	EndCritical(sr);
	
	if(numThreads==1)
		return 0;
	
	if (blocked) {
		return -1;
	}
	if(!sleepTime){
		return 0;			//not sleeping
		
	}
	else if(currentTime < sleepTime){		//still sleeping
		node->Sleep = 0;
		return -1;
	}
	
	return 1;
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
	/*
	int status = StartCritical();
	int currentTime = OS_Time();
	EndCritical(status);
	NodePt->Sleep = (sleepTime*1000)+currentTime;
	OS_Suspend();*/
	
	
	//will be done in units of 12.5 ns. the sleep time 1ms * 80000 = x timer ticks or x ns
//	int status = StartCritical();
	int currentTime = OS_Time();
	NodePt->Sleep = (sleepTime*80000)+currentTime;
//	EndCritical(status);
	OS_Suspend();
} 

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295, meaning each point value is worth 12.5 ns, a timer tick
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void){
	
	
	//every tick of the timer is 12.5 ns worth of time, to make 1 ms = 1000000 ns, w
/*	
	int status = StartCritical();
	RTC += rtCounter*OSTIMERTICK + (0.0125 * (TIMER1_TAILR_R-TIMER1_TAV_R));
	rtCounter=0;
	EndCritical(status);
	return RTC;	//SysTick Current Time*/
	
	//int status = StartCritical();
	//RTC += rtCounter*OSTIMERPERIOD + (TIMER1_TAILR_R-TIMER1_TAR_R);
	RTC = TIMER1_TAILR_R - TIMER1_TAR_R;
	//EndCritical(status);
	return RTC;
}


unsigned long OS_TimeNano(void){
	int time = OS_Time();
	return time*1000;			//there are 1000 nanoseconds in 1 micro second
}

// ******** OS_ClearMsTime ************
// sets the system time to zero (from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void) {
	//TIMER1_TAILR_R = OSTIMERPERIOD;    // 4) reload value. set to max value
	//TIMER1_TAV_R = TIMER1_TAILR_R;
	MSRTC = 0;
}

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread
unsigned long OS_MsTime(void) {
	//OS_Time() is in units of 12.5ns
	//unsigned long raw_time = OS_Time()
	MSRTC = OS_Time()*(0.0000125);
	return MSRTC;
}


// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop){
	unsigned long diff = stop - start;
	if(stop < start)
		diff = TIMER1_TAILR_R - start + stop;
	
	return diff;
}


void OS_GetJitter(void) {
	UART_OutString("Jitter in 0.1 usec\n\r\n\r");
	NodeType *temp = PeriodicPt;
	while (temp) {
		
		UART_OutString("Thread ");
		UART_OutUDec(temp->id);
		UART_OutString("\n\r");
		UART_OutString("Max Jitter: ");
		UART_OutUDec(temp->maxJitter);
		UART_OutString("\n\r");
		UART_OutString("Histogram:");
		UART_OutString("\n\r");
		for (int i = 0; i < 64; i++){
			UART_OutUDec(i);
			UART_OutString(" : ");
			UART_OutUDec(temp->JitterHistogram[i]);
			UART_OutString("\n\r");
		}
		UART_OutString("\n\r");
		temp = temp->Next;
	}
}
