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


#ifdef __MATH_NEON
#include "arm_neon.h" 
#endif

//vec4 scalar product
float dot4_c(float v0[4], float v1[4])
{
	float r;
	r = v0[0]*v1[0];
	r += v0[1]*v1[1];
	r += v0[2]*v1[2]; 
	r += v0[3]*v1[3];
	return r;
}

void normalize4_c(float v[4], float d[4])
{
	float b, c, x;
	union {
		float 	f;
		int 	i;
	} a;
	
	x = v[0]*v[0];
	x += v[1]*v[1];
	x += v[2]*v[2];
	x += v[3]*v[3];

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
	d[2] = v[2]*a.f;
	d[3] = v[3]*a.f;
}

void normalize4_neon(float v[4], float d[4])
{
#ifdef __MATH_NEON
	asm volatile (
	"vld1.32 		{d4, d5}, [%0]			\n\t"	//d2={x0,y0}, d3={z0, w0}
	"vmul.f32 		d0, d4, d4				\n\t"	//d0= d4*d4
	"vmla.f32 		d0, d5, d5				\n\t"	//d0 = d0 + d5*d5 
	"vpadd.f32 		d0, d0					\n\t"	//d0 = d[0] + d[1]
	
	"vmov.f32 		d1, d0					\n\t"	//d1 = d0
	"vrsqrte.f32 	d0, d0					\n\t"	//d0 = ~ 1.0 / sqrt(d0)
	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1
	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d3 = (3 - d0 * d2) / 2 	
	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d3
	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1	
	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d4 = (3 - d0 * d3) / 2	
	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d4	

	"vmul.f32 		q2, q2, d0[0]			\n\t"	//d0= d2*d4
	"vst1.32 		{d4, d5}, [%1]			\n\t"	//d2={x0,y0}, d3={z0, w0}
	
	:: "r"(v), "r"(d) 
    : "d0", "d1", "d2", "d3", "d4", "d5", "memory"
	);	
#else
	normalize4_c(v, d);
#endif

}


float dot4_neon_hfp(float v0[4], float v1[4])
{
#ifdef __MATH_NEON
	asm volatile (
	"vld1.32 		{d2, d3}, [%0]			\n\t"	//d2={x0,y0}, d3={z0, w0}
	"vld1.32 		{d4, d5}, [%1]			\n\t"	//d4={x1,y1}, d5={z1, w1}
	"vmul.f32 		d0, d2, d4				\n\t"	//d0= d2*d4
	"vmla.f32 		d0, d3, d5				\n\t"	//d0 = d0 + d3*d5 
	"vpadd.f32 		d0, d0					\n\t"	//d0 = d[0] + d[1]
	:: "r"(v0), "r"(v1) : 
	);	
#endif
}

float dot4_neon_sfp(float v0[4], float v1[4])
{
#ifdef __MATH_NEON
	dot4_neon_hfp(v0, v1);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return dot4_c(v0, v1);
#endif
};

