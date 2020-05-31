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

const float __atan2f_lut[4] = {
	-0.0443265554792128,	//p7
	-0.3258083974640975,	//p3
	+0.1555786518463281,	//p5
	+0.9997878412794807  	//p1
}; 
 
const float __atan2f_pi_2 = M_PI_2;

float atan2f_c(float y, float x)
{
	float a, b, c, r, xx;
	int m;
	union {
		float f;
		int i;
	} xinv;

	//fast inverse approximation (2x newton)
	xx = fabs(x);
	xinv.f = xx;
	m = 0x3F800000 - (xinv.i & 0x7F800000);
	xinv.i = xinv.i + m;
	xinv.f = 1.41176471f - 0.47058824f * xinv.f;
	xinv.i = xinv.i + m;
	b = 2.0 - xinv.f * xx;
	xinv.f = xinv.f * b;	
	b = 2.0 - xinv.f * xx;
	xinv.f = xinv.f * b;
	
	c = fabs(y * xinv.f);

	//fast inverse approximation (2x newton)
	xinv.f = c;
	m = 0x3F800000 - (xinv.i & 0x7F800000);
	xinv.i = xinv.i + m;
	xinv.f = 1.41176471f - 0.47058824f * xinv.f;
	xinv.i = xinv.i + m;
	b = 2.0 - xinv.f * c;
	xinv.f = xinv.f * b;	
	b = 2.0 - xinv.f * c;
	xinv.f = xinv.f * b;
	
	//if |x| > 1.0 -> ax = -1/ax, r = pi/2
	xinv.f = xinv.f + c;
	a = (c > 1.0f);
	c = c - a * xinv.f;
	r = a * __atan2f_pi_2;
	
	//polynomial evaluation
	xx = c * c;	
	a = (__atan2f_lut[0] * c) * xx + (__atan2f_lut[2] * c);
	b = (__atan2f_lut[1] * c) * xx + (__atan2f_lut[3] * c);
	xx = xx * xx;
	r = r + a * xx; 
	r = r + b;

	//determine quadrant and test for small x.
	b = M_PI;
	b = b - 2.0f * r;
	r = r + (x < 0.0f) * b;
	b = (fabs(x) < 0.000001f);
	c = !b;
	r = c * r;
	r = r + __atan2f_pi_2 * b;
	b = r + r;
	r = r - (y < 0.0f) * b;
	
	return r;
}

float atan2f_neon_hfp(float y, float x)
{
#ifdef __MATH_NEON
	asm volatile (

	"vdup.f32	 	d17, d0[1]				\n\t"	//d17 = {x, x};
	"vdup.f32	 	d16, d0[0]				\n\t"	//d16 = {y, y};
	
	//1.0 / x
	"vrecpe.f32		d18, d17				\n\t"	//d16 = ~ 1 / d1; 
	"vrecps.f32		d19, d18, d17			\n\t"	//d17 = 2.0 - d16 * d1; 
	"vmul.f32		d18, d18, d19			\n\t"	//d16 = d16 * d17; 
	"vrecps.f32		d19, d18, d17			\n\t"	//d17 = 2.0 - d16 * d1; 
	"vmul.f32		d18, d18, d19			\n\t"	//d16 = d16 * d17; 

	//y * (1.0 /x)
	"vmul.f32		d0, d16, d18			\n\t"	//d0 = d16 * d18; 


	"vdup.f32	 	d4, %1					\n\t"	//d4 = {pi/2, pi/2};
	"vmov.f32	 	d6, d0					\n\t"	//d6 = d0;
	"vabs.f32	 	d0, d0					\n\t"	//d0 = fabs(d0) ;

	//fast reciporical approximation
	"vrecpe.f32		d1, d0					\n\t"	//d1 = ~ 1 / d0; 
	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0; 
	"vmul.f32		d1, d1, d2				\n\t"	//d1 = d1 * d2; 
	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0; 
	"vmul.f32		d1, d1, d2				\n\t"	//d1 = d1 * d2; 

	//if |x| > 1.0 -> ax = 1/ax, r = pi/2
	"vadd.f32		d1, d1, d0				\n\t"	//d1 = d1 + d0; 
	"vmov.f32	 	d2, #1.0				\n\t"	//d2 = 1.0;
	"vcgt.f32	 	d3, d0, d2				\n\t"	//d3 = (d0 > d2);
	"vcvt.f32.u32	d3, d3					\n\t"	//d3 = (float) d3;
	"vmls.f32		d0, d1, d3				\n\t"	//d0 = d0 - d1 * d3; 	
	"vmul.f32		d7, d3, d4				\n\t"	//d7 = d3 * d4; 	
		
	//polynomial:
	"vmul.f32 		d2, d0, d0				\n\t"	//d2 = d0*d0 = {ax^2, ax^2}	
	"vld1.32 		{d4, d5}, [%0]			\n\t"	//d4 = {p7, p3}, d5 = {p5, p1}
	"vmul.f32 		d3, d2, d2				\n\t"	//d3 = d2*d2 = {x^4, x^4}		
	"vmul.f32 		q0, q2, d0[0]			\n\t"	//q0 = q2 * d0[0] = {p7x, p3x, p5x, p1x}
	"vmla.f32 		d1, d0, d2[0]			\n\t"	//d1 = d1 + d0*d2[0] = {p5x + p7x^3, p1x + p3x^3}		
	"vmla.f32 		d1, d3, d1[0]			\n\t"	//d1 = d1 + d3*d1[0] = {..., p1x + p3x^3 + p5x^5 + p7x^7}		
	"vadd.f32 		d1, d1, d7				\n\t"	//d1 = d1 + d7		
	
	"vadd.f32 		d2, d1, d1				\n\t"	//d2 = d1 + d1		
	"vclt.f32	 	d3, d6, #0				\n\t"	//d3 = (d6 < 0)	
	"vcvt.f32.u32	d3, d3					\n\t"	//d3 = (float) d3	
	"vmls.f32 		d1, d3, d2				\n\t"	//d1 = d1 - d2 * d3;

	"vmov.f32 		s0, s3					\n\t"	//s0 = s3

	:: "r"(__atan2f_lut),  "r"(__atan2f_pi_2) 
    : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7"
	);
#endif
}


float atan2f_neon_sfp(float x, float y)
{
#ifdef __MATH_NEON
	asm volatile ("vmov.f32 s0, r0 		\n\t");
	asm volatile ("vmov.f32 s1, r1 		\n\t");
	atan2f_neon_hfp(x, y);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return atan2f_c(y, x);
#endif
};
