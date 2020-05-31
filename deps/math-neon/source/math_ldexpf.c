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

float ldexpf_c(float m, int e)
{
	union {
		float 	f;
		int 	i;
	} r;
	r.f = m;
	r.i += (e << 23);
	return r.f;
}

float ldexpf_neon_hfp(float m, int e)
{
#ifdef __MATH_NEON
	float r;
	asm volatile (
	"lsl 			r0, r0, #23				\n\t"	//r0 = r0 << 23	
	"vdup.i32 		d1, r0					\n\t"	//d1 = {r0, r0}
	"vadd.i32 		d0, d0, d1				\n\t"	//d0 = d0 + d1
	::: "d0", "d1"
	);
#endif
}

float ldexpf_neon_sfp(float m, int e)
{
#ifdef __MATH_NEON
	float r;
	asm volatile (
	"lsl 			r1, r1, #23				\n\t"	//r1 = r1 << 23	
	"vdup.f32 		d0, r0					\n\t"	//d0 = {r0, r0}	
	"vdup.i32 		d1, r1					\n\t"	//d1 = {r1, r1}
	"vadd.i32 		d0, d0, d1				\n\t"	//d0 = d0 + d1
	"vmov.f32 		r0, s0					\n\t"	//r0 = s0
	::: "d0", "d1"
	);
#else
	return ldexpf_c(m,e);
#endif
}
