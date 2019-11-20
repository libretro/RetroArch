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
TanH = (e^x - e^-x) / (e^x + e^-x)
TanH = (e^x - e^-x)(e^x) / (e^x + e^-x)(e^x)
TanH = (e^2x - 1) / (e^2x + 1)

*/
 
float tanhf_c(float x)
{
	float a, b, c;
	int m;
	union{
		float 	f;
		int 	i;
	} xx;
	
	x = 2.0f * x;
	a = expf_c(x);
	c = a + 1.0f;
		
	//reciporical approx.
	xx.f = c;
	m = 0x3F800000 - (xx.i & 0x7F800000);
	xx.i = xx.i + m;
	xx.f = 1.41176471f - 0.47058824f * xx.f;
	xx.i = xx.i + m;
	b = 2.0 - xx.f * c;
	xx.f = xx.f * b;	
	b = 2.0 - xx.f * c;
	xx.f = xx.f * b;
	c = a - 1.0;
	xx.f *= c;
	return xx.f;
}


float tanhf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vadd.f32 d0, d0, d0 		\n\t");
	expf_neon_hfp(x);
	asm volatile (
	"vmov.f32 		d2, #1.0 				\n\t"
	"vsub.f32 		d3, d0, d2 				\n\t"
	"vadd.f32 		d0, d0, d2 				\n\t"

	"vrecpe.f32		d1, d0					\n\t"	//d1 = ~ 1 / d0; 
	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0; 
	"vmul.f32		d1, d1, d2				\n\t"	//d1 = d1 * d2; 
	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0; 
	"vmul.f32		d0, d1, d2				\n\t"	//d0 = d1 * d2; 
	"vmul.f32		d0, d0, d3				\n\t"	//d0 = d0 * d3; 	
	::: "d0", "d1", "d2", "d3"
	);	
#endif
}

float tanhf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vmov.f32 s0, r0 		\n\t");
	tanhf_neon_hfp(x);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return tanhf_c(x);
#endif
};

