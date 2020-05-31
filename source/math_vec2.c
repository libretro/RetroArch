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

//vec2 scalar product
float 
dot2_c(float v0[2], float v1[2])
{
	float r;
	r = v0[0]*v1[0];
	r += v0[1]*v1[1];
	return r;
}

void 
normalize2_c(float v[2], float d[2])
{
	float b, c, x;
	union {
		float 	f;
		int 	i;
	} a;
	
	x = v[0]*v[0];
	x += v[1]*v[1];

	//fast invsqrt approx
	a.f = x;
	a.i = 0x5F3759DF - (a.i >> 1);		//VRSQRTE
	c = x * a.f;
	b = (3.0f - c * a.f) * 0.5;		//VRSQRTS
	a.f = a.f * b;		
	c = x * a.f;
	b = (3.0f - c * a.f) * 0.5;
    a.f = a.f * b;	

	d[0] = v[0]*a.f;
	d[1] = v[1]*a.f;
}

float 
dot2_neon_hfp(float v0[2], float v1[2])
{
#ifdef __MATH_NEON
	asm volatile (
	"vld1.32 		{d2}, [%0]			\n\t"	//d2={x0,y0}
	"vld1.32 		{d4}, [%1]			\n\t"	//d4={x1,y1}
	"vmul.f32 		d0, d2, d4			\n\t"	//d0 = d2*d4
	"vpadd.f32 		d0, d0, d0			\n\t"	//d0 = d[0] + d[1]
	:: "r"(v0), "r"(v1) 
    : 
	);	
#endif
}

float 
dot2_neon_sfp(float v0[2], float v1[2])
{
#ifdef __MATH_NEON
	dot2_neon_hfp(v0, v1);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return dot2_c(v0, v1);
#endif
};

void 
normalize2_neon(float v[2], float d[2])
{
#ifdef __MATH_NEON
	asm volatile (
	"vld1.32 		d4, [%0]				\n\t"	//d4 = {x0,y0}
	"vmul.f32 		d0, d4, d4				\n\t"	//d0 = d2*d2
	"vpadd.f32 		d0, d0					\n\t"	//d0 = d[0] + d[1]
	
	"vmov.f32 		d1, d0					\n\t"	//d1 = d0
	"vrsqrte.f32 	d0, d0					\n\t"	//d0 = ~ 1.0 / sqrt(d0)
	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1
	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d3 = (3 - d0 * d2) / 2 	
	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d3
	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1	
	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d3 = (3 - d0 * d2) / 2	
	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d3	

	"vmul.f32 		d4, d4, d0[0]			\n\t"	//d4 = d4*d0[0]
	"vst1.32 		d4, [%1]				\n\t"	//
	
	:: "r"(v), "r"(d) 
    : "d0", "d1", "d2", "d3", "d4", "memory"
	);	
#else
	normalize2_c(v, d);
#endif
}

