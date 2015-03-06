What's Changed:
-ADC
-UART
-Priority


IMPORTANT:
UART has to have low contention because the FIFOs are limited to 16 bits.
If you print too much/too often to the UART, it will hard fault.
If you have this problem use OS_Sleep() in between printing a lot (helped when I tested it very frequently)


ADC works on 100-10000 Hz with ADC_Collect.
ADC can sample from any of the channels.

UART works and uses busy-wait synchronization for right now (the pdf says semaphores or busy wait is fine)

I have another testmain, adctestmain, which strictly tests UART and the ADC.





Current Priority List

adc	1
UART 	2
switch	3
timer3a	4
timer1a	5
systick	6


-UART must be lower than ADC but higher than SysTick
-ADC I have found only works at very high


