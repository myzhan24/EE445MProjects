#include <stdint.h>
#include <string.h>
#include "tm4c123gh6pm.h"

#include "OS.h"
#include "ST7735.h"
#include "ADC.h"
#include "UART.h"


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

