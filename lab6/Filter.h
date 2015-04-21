
	
#ifndef __Filter_H
#define __Filter_H  1

void Filter_51Work(long *x, double *y, unsigned long size);
void frequency(long *x, int len);
unsigned long sqrtN(unsigned long s);
short FFT(short int dir, long m, double *x, double *y);
void complexMagnitude(double* dest, double* real, double* imag, int len);
	
#endif
