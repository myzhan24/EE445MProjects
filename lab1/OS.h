// this file is created by Rob and Matt with some sections from the starter code by Valvano
// This is the header file for the Operating System. 
// This is the first rivision which allows you to specify tasks by timer interrupt


// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers 
// input:  none
// output: none
void OS_Init(void);

// calls a thread with a periodic interrupt
// Inputs: task pointer to the function, period millisecond period to execute function, priority NVIC priority (0-7)
// Output: ??
unsigned int OS_AddPeriodicThread(void (*task) (void), unsigned long period, unsigned long priority);

// resets the 32bit periodic counter to 0 (I think this counts number of thread switches)
void OS_ClearPeriodicTime(void);

// returns the 32bit periodic counter
unsigned long OS_ReadPeriodicTime(void);

