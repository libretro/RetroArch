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
Assumes the floating point value |x| < 2,147,483,648
*/

#include "math_neon.h"

float modf_c(float x, int *i)
{
	int n;
	n = (int)x;
	*i = n;
	x = x - (float)n;
	return x;
}


float modf_neon_hfp(float x, int *i)
{
#ifdef __MATH_NEON
	asm volatile (	
	"vcvt.s32.f32	d1, d0					\n\t"	//d1 = (int) d0; 
	"vcvt.f32.s32	d2, d1					\n\t"	//d2 = (float) d1;
	"vsub.f32		d0, d0, d2				\n\t"	//d0 = d0 - d2; 
	"vstr.i32		s2, [r0]				\n\t"	//[r0] = d1[0] 
	::: "d0", "d1", "d2"
	);		
#endif
}


float modf_neon_sfp(float x, int *i)
{
#ifdef __MATH_NEON
	asm volatile (
	"vdup.f32 		d0, r0					\n\t"	//d0 = {x, x}	
	"vcvt.s32.f32	d1, d0					\n\t"	//d1 = (int) d0; 
	"vcvt.f32.s32	d2, d1					\n\t"	//d2 = (float) d1;
	"vsub.f32		d0, d0, d2				\n\t"	//d0 = d0 - d2; 
	"vstr.i32		s2, [r1]				\n\t"	//[r0] = d1[0] 
	"vmov.f32 		r0, s0					\n\t"	//r0 = d0[0];
	::: "d0", "d1", "d2"
	);
		
#else
	return modf_c(x, i);
#endif
}
