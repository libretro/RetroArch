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
Test func : sqrtf(x)
Test Range: 0 < x < 1,000,000,000
Peak Error:	~0.0010%
RMS  Error: ~0.0005%
*/

#include "math.h"
#include "math_neon.h"

float sqrtf_c(float x)
{

	float b, c;
	int m;
	union {
		float 	f;
		int 	i;
	} a;
	
	//fast invsqrt approx
	a.f = x;
	a.i = 0x5F3759DF - (a.i >> 1);		//VRSQRTE
	c = x * a.f;
	b = (3.0f - c * a.f) * 0.5;		//VRSQRTS
	a.f = a.f * b;		
	c = x * a.f;
	b = (3.0f - c * a.f) * 0.5;
    a.f = a.f * b;	

	//fast inverse approx
	x = a.f;
	m = 0x3F800000 - (a.i & 0x7F800000);
	a.i = a.i + m;
	a.f = 1.41176471f - 0.47058824f * a.f;
	a.i = a.i + m;
	b = 2.0 - a.f * x;
	a.f = a.f * b;	
	b = 2.0 - a.f * x;
	a.f = a.f * b;

	return a.f;
}

float sqrtf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asm volatile (
		
	//fast invsqrt approx
	"vmov.f32 		d1, d0					\n\t"	//d1 = d0
	"vrsqrte.f32 	d0, d0					\n\t"	//d0 = ~ 1.0 / sqrt(d0)
	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1
	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d3 = (3 - d0 * d2) / 2 	
	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d3
	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1	
	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d4 = (3 - d0 * d3) / 2	
	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d3	
		
	//fast reciporical approximation
	"vrecpe.f32		d1, d0					\n\t"	//d1 = ~ 1 / d0; 
	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0; 
	"vmul.f32		d1, d1, d2				\n\t"	//d1 = d1 * d2; 
	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0; 
	"vmul.f32		d0, d1, d2				\n\t"	//d0 = d1 * d2; 

	::: "d0", "d1", "d2", "d3"
	);
#endif
}

float sqrtf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vmov.f32 s0, r0 		\n\t");
	sqrtf_neon_hfp(x);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return sqrtf_c(x);
#endif
};
