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

#include "math_neon.h"

	
float fabsf_c(float x)
{
	union {
		int i;
		float f;
	} xx;

	xx.f = x;
	xx.i = xx.i & 0x7FFFFFFF;
	return xx.f;
}

float fabsf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asm volatile (
	"fabss	 		s0, s0					\n\t"	//s0 = fabs(s0)
	);
#endif
}

float fabsf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile (
	"bic	 		r0, r0, #0x80000000		\n\t"	//r0 = r0 & ~(1 << 31)
	);
#else
	return fabsf_c(x);
#endif
}
