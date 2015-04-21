
#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "heap.h"
#include "Filter.h"
#include <math.h>


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

const long h[51] = {4, -1, -8, -14, -16, -10, -1, 6, 5, -3, -13, -15, -8, 3, 5, -5, -20, -25, -8, 25, 46, 26, -49, -159, -257, 984, -257, -159, -49, 26, 46, 25, -8, -25, -20, -5, 5, 3, -8, -15, -13, -3, 5, 6, -1, -10, -16, -14, -8, -1, 4};
const long fixed51 = 256;
	
	
	
	//X(z) = FFT(x(t))
	//frequency domain = FFT of ADC input
	
	//H(z) is desired filter gain transer function
	
	//Y(z) = H(z)X(z)
	//FIR filter output in the frequency domain = desired gain function * ADC input in the frequency domain
	
	//y(n) = IFFT[ H(z) FFT(x(t)) ]
	//FIR filter output in the time domain

void Filter_51Work(long *x, double *y, unsigned long size) {

	if (size < 51) {return;}
	
	//k goes from 51 to 51+size
	for (int k = 51; k < size; k++) {
		y[k] = 0;	//reset the value to zero
		for (int i = 0; i < 51; i++) {
				y[k] += h[i]*x[k-i];
		}
		y[k] = y[k]/fixed51;
	}
}

const double freq = 250.9804;	//df
void frequency(long *x, int len) {
	for (int i = 0; i < len; i++) {
		if (i%52 < 25) {
			x[i] = freq*(i%26);
		}
		else {
			x[i] = (-1)*freq*(25 - (i%26));
		}
	}
}


/*
taken from http://paulbourke.net/miscellaneous/dft/
   This computes an in-place complex-to-complex FFT 
   x and y are the real and imaginary arrays of 2^m points.
   dir =  1 gives forward transform
   dir = -1 gives reverse transform 
*/

/*	FFT variables
Fs = 12.8 KHz
N = 

In graphing the x axis with the frequency domain, we want the frequency range to represent 6400 Hz, so our resolution would be 6400/128 = 50 Hz each pixel
We have 128 bins
With N samples...

N/2 = 128 FFT bins
N = 256 samples

6400 Hz / 128 bins = 50 Hz / bin


*/

short FFT(short int dir,long m,double *x,double *y)
{
   long n,i,i1,j,k,i2,l,l1,l2;
   double c1,c2,tx,ty,t1,t2,u1,u2,z;

   /* Calculate the number of points */
   n = 1;
   for (i=0;i<m;i++) 
      n *= 2;

   /* Do the bit reversal */
   i2 = n >> 1;
   j = 0;
   for (i=0;i<n-1;i++) {
      if (i < j) {
         tx = x[i];
         ty = y[i];
         x[i] = x[j];
         y[i] = y[j];
         x[j] = tx;
         y[j] = ty;
      }
      k = i2;
      while (k <= j) {
         j -= k;
         k >>= 1;
      }
      j += k;
   }

   /* Compute the FFT */
   c1 = -1.0; 
   c2 = 0.0;
   l2 = 1;
   for (l=0;l<m;l++) {
      l1 = l2;
      l2 <<= 1;
      u1 = 1.0; 
      u2 = 0.0;
      for (j=0;j<l1;j++) {
         for (i=j;i<n;i+=l2) {
            i1 = i + l1;
            t1 = u1 * x[i1] - u2 * y[i1];
            t2 = u1 * y[i1] + u2 * x[i1];
            x[i1] = x[i] - t1; 
            y[i1] = y[i] - t2;
            x[i] += t1;
            y[i] += t2;
         }
         z =  u1 * c1 - u2 * c2;
         u2 = u1 * c2 + u2 * c1;
         u1 = z;
      }
      c2 = sqrt((1.0 - c1) / 2.0);
      if (dir == 1) 
         c2 = -c2;
      c1 = sqrt((1.0 + c1) / 2.0);
   }

   /* Scaling for forward transform */
   if (dir == 1) {
      for (i=0;i<n;i++) {
         x[i] /= n;
         y[i] /= n;
      }
   }
   
   return(1);
}



void complexMagnitude(double* dest, double* real, double* imag, int len){
	int i;
	int rl;
	int im;
	for(i = 0; i < len;i++){
		rl = real[i];
		im = imag[i];
		dest[i] = sqrt((rl*rl)+(im*im));
	}
}
