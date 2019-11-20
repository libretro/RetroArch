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
Assumes the floating point value |x| < 2147483648
*/

#include "math.h"
#include "math_neon.h"

float ceilf_c(float x)
{
	int n;
	float r;	
	n = (int) x;
	r = (float) n;
	r = r + (x > r);
	return r;
}

float ceilf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asm volatile (

	"vcvt.s32.f32 	d1, d0					\n\t"	//d1 = (int) d0;
	"vcvt.f32.s32 	d1, d1					\n\t"	//d1 = (float) d1;
	"vcgt.f32 		d0, d0, d1				\n\t"	//d0 = (d0 > d1);
	"vshr.u32 		d0, #31					\n\t"	//d0 = d0 >> 31;
	"vcvt.f32.u32 	d0, d0					\n\t"	//d0 = (float) d0;
	"vadd.f32 		d0, d1, d0				\n\t"	//d0 = d1 + d0;

	::: "d0", "d1"
	);
		
#endif
}

float ceilf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vmov.f32 s0, r0 		\n\t");
	ceilf_neon_hfp(x);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return ceilf_c(x);
#endif
};


