/*
The MIT License (MIT)

Copyright (c) 2015 Lachlan Tychsen-Smith (lachlan.ts@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "math.h"
#include "math_neon.h"

/*
Test func : asinf(x)
Test Range: -1.0 < x < 1.0
Peak Error:	~0.005%
RMS  Error: ~0.001%
*/


const float __asinf_lut[4] = {
	0.105312459675071, 	//p7
	0.169303418571894,	//p3
	0.051599985887214, 	//p5
	0.999954835104825	//p1
}; 

const float __asinf_pi_2 = M_PI_2;

float asinf_c(float x)
{

	float a, b, c, d, r, ax;
	int m;
	
	union {
		float f;
		int i;
	} xx;

	ax = fabs(x);
	d = 0.5;
	d = d - ax*0.5;
		
	//fast invsqrt approx
	xx.f = d;
	xx.i = 0x5F3759DF - (xx.i >> 1);		//VRSQRTE
	c = d * xx.f;
	b = (3.0f - c * xx.f) * 0.5;		//VRSQRTS
	xx.f = xx.f * b;		
	c = d * xx.f;
	b = (3.0f - c * xx.f) * 0.5;
    xx.f = xx.f * b;	

	//fast inverse approx
	d = xx.f;
	m = 0x3F800000 - (xx.i & 0x7F800000);
	xx.i = xx.i + m;
	xx.f = 1.41176471f - 0.47058824f * xx.f;
	xx.i = xx.i + m;
	b = 2.0 - xx.f * d;
	xx.f = xx.f * b;	
	b = 2.0 - xx.f * d;
	xx.f = xx.f * b;
	
	//if |x|>0.5 -> x = sqrt((1-x)/2)
	xx.f = xx.f - ax;	
	a = (ax > 0.5f);
	d = __asinf_pi_2 * a;
	c = 1.0f - 3.0f * a;
	ax = ax + xx.f * a;
		
	//polynomial evaluation
	xx.f = ax * ax;	
	a = (__asinf_lut[0] * ax) * xx.f + (__asinf_lut[2] * ax);
	b = (__asinf_lut[1] * ax) * xx.f + (__asinf_lut[3] * ax);
	xx.f = xx.f * xx.f;
	r = b + a * xx.f; 
	r = d + c * r;

	a = r + r;
	b = (x < 0.0f);
	r = r - a * b;
	return r;
}


float asinf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asm volatile (

	"vdup.f32	 	d0, d0[0]				\n\t"	//d0 = {x, x};
	"vdup.f32	 	d4, %1					\n\t"	//d4 = {pi/2, pi/2};
	"vmov.f32	 	d6, d0					\n\t"	//d6 = d0;
	"vabs.f32	 	d0, d0					\n\t"	//d0 = fabs(d0) ;

	"vmov.f32	 	d5, #0.5				\n\t"	//d5 = 0.5;
	"vmls.f32	 	d5, d0, d5				\n\t"	//d5 = d5 - d0*d5;

	//fast invsqrt approx
	"vmov.f32 		d1, d5					\n\t"	//d1 = d5
	"vrsqrte.f32 	d5, d5					\n\t"	//d5 = ~ 1.0 / sqrt(d5)
	"vmul.f32 		d2, d5, d1				\n\t"	//d2 = d5 * d1
	"vrsqrts.f32 	d3, d2, d5				\n\t"	//d3 = (3 - d5 * d2) / 2 	
	"vmul.f32 		d5, d5, d3				\n\t"	//d5 = d5 * d3
	"vmul.f32 		d2, d5, d1				\n\t"	//d2 = d5 * d1	
	"vrsqrts.f32 	d3, d2, d5				\n\t"	//d3 = (3 - d5 * d3) / 2	
	"vmul.f32 		d5, d5, d3				\n\t"	//d5 = d5 * d3	
		
	//fast reciporical approximation
	"vrecpe.f32		d1, d5					\n\t"	//d1 = ~ 1 / d5; 
	"vrecps.f32		d2, d1, d5				\n\t"	//d2 = 2.0 - d1 * d5; 
	"vmul.f32		d1, d1, d2				\n\t"	//d1 = d1 * d2; 
	"vrecps.f32		d2, d1, d5				\n\t"	//d2 = 2.0 - d1 * d5; 
	"vmul.f32		d5, d1, d2				\n\t"	//d5 = d1 * d2; 
	
	//if |x| > 0.5 -> ax = sqrt((1-ax)/2), r = pi/2
	"vsub.f32		d5, d0, d5				\n\t"	//d5 = d0 - d5; 
	"vmov.f32	 	d2, #0.5				\n\t"	//d2 = 0.5;
	"vcgt.f32	 	d3, d0, d2				\n\t"	//d3 = (d0 > d2);
	"vmov.f32		d1, #3.0 				\n\t"	//d5 = 3.0; 	
	"vshr.u32	 	d3, #31					\n\t"	//d3 = d3 >> 31;
	"vmov.f32		d16, #1.0 				\n\t"	//d16 = 1.0; 	
	"vcvt.f32.u32	d3, d3					\n\t"	//d3 = (float) d3;	
	"vmls.f32		d0, d5, d3[0]			\n\t"	//d0 = d0 - d5 * d3[0]; 	
	"vmul.f32		d7, d4, d3[0] 			\n\t"	//d7 = d5 * d4; 		
	"vmls.f32		d16, d1, d3[0] 			\n\t"	//d16 = d16 - d1 * d3; 	
		
	//polynomial:
	"vmul.f32 		d2, d0, d0				\n\t"	//d2 = d0*d0 = {ax^2, ax^2}	
	"vld1.32 		{d4, d5}, [%0]			\n\t"	//d4 = {p7, p3}, d5 = {p5, p1}
	"vmul.f32 		d3, d2, d2				\n\t"	//d3 = d2*d2 = {x^4, x^4}		
	"vmul.f32 		q0, q2, d0[0]			\n\t"	//q0 = q2 * d0[0] = {p7x, p3x, p5x, p1x}
	"vmla.f32 		d1, d0, d2[0]			\n\t"	//d1 = d1 + d0*d2[0] = {p5x + p7x^3, p1x + p3x^3}		
	"vmla.f32 		d1, d3, d1[0]			\n\t"	//d1 = d1 + d3*d1[0] = {..., p1x + p3x^3 + p5x^5 + p7x^7}		

	"vmla.f32 		d7, d1, d16				\n\t"	//d7 = d7 + d1*d16		

	"vadd.f32 		d2, d7, d7				\n\t"	//d2 = d7 + d7		
	"vclt.f32	 	d3, d6, #0				\n\t"	//d3 = (d6 < 0)	
	"vshr.u32	 	d3, #31					\n\t"	//d3 = d3 >> 31;
	"vcvt.f32.u32	d3, d3					\n\t"	//d3 = (float) d3	
	"vmls.f32 		d7, d2, d3[0]			\n\t"	//d7 = d7 - d2 * d3[0];

	"vmov.f32 		s0, s15					\n\t"	//s0 = s3

	:: "r"(__asinf_lut),  "r"(__asinf_pi_2) 
    : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7"
	);
#endif
}


float asinf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vmov.f32 s0, r0 		\n\t");
	asinf_neon_hfp(x);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return asinf_c(x);
#endif
}




