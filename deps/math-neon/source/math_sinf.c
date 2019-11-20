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

#include <math.h>
#include "math_neon.h"

static const float __sinf_rng[2] = {
	2.0 / M_PI,
	M_PI / 2.0
} ALIGN(16);

static const float __sinf_lut[4] = {
	-0.00018365f,	//p7
	-0.16664831f,	//p3
	+0.00830636f,	//p5
	+0.99999661f,	//p1
} ALIGN(16);

float sinf_c(float x)
{
	union {
		float 	f;
		int 	i;
	} ax;
	
	float r, a, b, xx;
	int m, n;
	
	ax.f = fabsf(x);

	//Range Reduction:
	m = (int) (ax.f * __sinf_rng[0]);	
	ax.f = ax.f - (((float)m) * __sinf_rng[1]);

	//Test Quadrant
	n = m & 1;
	ax.f = ax.f - n * __sinf_rng[1];	
	m = m >> 1;
	n = n ^ m;
	m = (x < 0.0);
	n = n ^ m;	
	n = n << 31;
	ax.i = ax.i ^ n; 

	//Taylor Polynomial (Estrins)
	xx = ax.f * ax.f;	
	a = (__sinf_lut[0] * ax.f) * xx + (__sinf_lut[2] * ax.f);
	b = (__sinf_lut[1] * ax.f) * xx + (__sinf_lut[3] * ax.f);
	xx = xx * xx;
	r = b + a * xx;

	return r;
}

float sinf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asm volatile (
	
	"vld1.32 		d3, [%0]				\n\t"	//d3 = {invrange, range}
	"vdup.f32 		d0, d0[0]				\n\t"	//d0 = {x, x}
	"vabs.f32 		d1, d0					\n\t"	//d1 = {ax, ax}
	
	"vmul.f32 		d2, d1, d3[0]			\n\t"	//d2 = d1 * d3[0] 
	"vcvt.u32.f32 	d2, d2					\n\t"	//d2 = (int) d2
	"vmov.i32	 	d5, #1					\n\t"	//d5 = 1	
	"vcvt.f32.u32 	d4, d2					\n\t"	//d4 = (float) d2	
	"vshr.u32 		d7, d2, #1				\n\t"	//d7 = d2 >> 1
	"vmls.f32 		d1, d4, d3[1]			\n\t"	//d1 = d1 - d4 * d3[1]
	
	"vand.i32 		d5, d2, d5				\n\t"	//d5 = d2 & d5
	"vclt.f32 		d18, d0, #0				\n\t"	//d18 = (d0 < 0.0)
	"vcvt.f32.u32 	d6, d5					\n\t"	//d6 = (float) d5
	"vmls.f32 		d1, d6, d3[1]			\n\t"	//d1 = d1 - d6 * d3[1]
	"veor.i32 		d5, d5, d7				\n\t"	//d5 = d5 ^ d7	
	"vmul.f32 		d2, d1, d1				\n\t"	//d2 = d1*d1 = {x^2, x^2}	
	
	"vld1.32 		{d16, d17}, [%1]		\n\t"	//q8 = {p7, p3, p5, p1}
	"veor.i32 		d5, d5, d18				\n\t"	//d5 = d5 ^ d18	
	"vshl.i32 		d5, d5, #31				\n\t"	//d5 = d5 << 31
	"veor.i32 		d1, d1, d5				\n\t"	//d1 = d1 ^ d5
	
	"vmul.f32 		d3, d2, d2				\n\t"	//d3 = d2*d2 = {x^4, x^4}		
	"vmul.f32 		q0, q8, d1[0]			\n\t"	//q0 = q8 * d1[0] = {p7x, p3x, p5x, p1x}
	"vmla.f32 		d1, d0, d2[0]			\n\t"	//d1 = d1 + d0*d2 = {p5x + p7x^3, p1x + p3x^3}		
	"vmla.f32 		d1, d3, d1[0]			\n\t"	//d1 = d1 + d3*d0 = {...., p1x + p3x^3 + p5x^5 + p7x^7}		

	"vmov.f32 		s0, s3					\n\t"	//s0 = s3
	: 
	: "r"(__sinf_rng), "r"(__sinf_lut) 
    : "q0", "q1", "q2", "q3", "q8", "q9"
	);
#endif
}

float sinf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vdup.f32 d0, r0 		\n\t");
	sinf_neon_hfp(x);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return sinf_c(x);
#endif

};

