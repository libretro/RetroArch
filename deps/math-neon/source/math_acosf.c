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
Test func : acosf(x)
Test Range: -1.0 < x < 1.0
Peak Error:	~0.005%
RMS  Error: ~0.001%
*/

const float __acosf_pi_2 = M_PI_2;

float acosf_c(float x)
{
	return __acosf_pi_2 - asinf_c(x);
}


float acosf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asinf_neon_hfp(x);
	asm volatile (
	"vdup.f32	 	d1, %0					\n\t"	//d1 = {pi/2, pi/2};
	"vsub.f32	 	d0, d1, d0				\n\t"	//d0 = d1 - d0;
	::"r"(__acosf_pi_2):
	);
#endif
}

float acosf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vmov.f32 s0, r0 		\n\t");
	acosf_neon_hfp(x);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return acosf_c(x);
#endif
}



