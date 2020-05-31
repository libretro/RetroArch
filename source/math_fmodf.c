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

/*
Assumes the floating point value |x / y| < 2,147,483,648
*/

#include "math_neon.h"

float fmodf_c(float x, float y)
{
	int n;
	union {
		float f;
		int   i;
	} yinv;
	float a;
	
	//fast reciporical approximation (4x Newton)
	yinv.f = y;
	n = 0x3F800000 - (yinv.i & 0x7F800000);
	yinv.i = yinv.i + n;
	yinv.f = 1.41176471f - 0.47058824f * yinv.f;
	yinv.i = yinv.i + n;
	a = 2.0 - yinv.f * y;
	yinv.f = yinv.f * a;	
	a = 2.0 - yinv.f * y;
	yinv.f = yinv.f * a;
	a = 2.0 - yinv.f * y;
	yinv.f = yinv.f * a;
	a = 2.0 - yinv.f * y;
	yinv.f = yinv.f * a;
	
	n = (int)(x * yinv.f);
	x = x - ((float)n) * y;
	return x;
}


float fmodf_neon_hfp(float x, float y)
{
#ifdef __MATH_NEON
	asm volatile (
	"vdup.f32 		d1, d0[1]					\n\t"	//d1[0] = y
	"vdup.f32 		d0, d0[0]					\n\t"	//d1[0] = y
	
	//fast reciporical approximation
	"vrecpe.f32 	d2, d1					\n\t"	//d2 = ~1.0 / d1
	"vrecps.f32		d3, d2, d1				\n\t"	//d3 = 2.0 - d2 * d1; 
	"vmul.f32		d2, d2, d3				\n\t"	//d2 = d2 * d3; 
	"vrecps.f32		d3, d2, d1				\n\t"	//d3 = 2.0 - d2 * d1; 
	"vmul.f32		d2, d2, d3				\n\t"	//d2 = d2 * d3; 
	"vrecps.f32		d3, d2, d1				\n\t"	//d3 = 2.0 - d2 * d1; 
	"vmul.f32		d2, d2, d3				\n\t"	//d2 = d2 * d3; 
	"vrecps.f32		d3, d2, d1				\n\t"	//d3 = 2.0 - d2 * d1; 
	"vmul.f32		d2, d2, d3				\n\t"	//d2 = d2 * d3; 

	"vmul.f32		d2, d2, d0				\n\t"	//d2 = d2 * d0; 
	"vcvt.s32.f32	d2, d2					\n\t"	//d2 = (int) d2; 
	"vcvt.f32.s32	d2, d2					\n\t"	//d2 = (float) d2; 
	"vmls.f32		d0, d1, d2				\n\t"	//d0 = d0 - d1 * d2; 

	::: "d0", "d1", "d2", "d3"
	);
#endif
}


float fmodf_neon_sfp(float x, float y)
{
#ifdef __MATH_NEON
	asm volatile ("vmov.f32 s0, r0 		\n\t");
	asm volatile ("vmov.f32 s1, r1 		\n\t");
	fmodf_neon_hfp(x, y);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return fmodf_c(x,y);
#endif
};
