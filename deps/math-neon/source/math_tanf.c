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

const float __tanf_rng[2] = {
	2.0 / M_PI,
	M_PI / 2.0
};

const float __tanf_lut[4] = {
	-0.00018365f,	//p7
	-0.16664831f,	//p3
	+0.00830636f,	//p5
	+0.99999661f,	//p1
};
 
float tanf_c(float x){

	union {
		float f;
		int i;
	} ax, c;

	float r, a, b, xx, cc, cx;
	int m;
	
	ax.f = fabsf(x);

	//Range Reduction:
	m = (int) (ax.f * __tanf_rng[0]);	
	ax.f = ax.f - (((float)m) * __tanf_rng[1]);

	//Test Quadrant
	ax.f = ax.f - (m & 1) * __tanf_rng[1];
	ax.i = ax.i ^ ((*(int*)&x) & 0x80000000);
		
	//Taylor Polynomial (Estrins)
	xx = ax.f * ax.f;	
	a = (__tanf_lut[0] * ax.f) * xx + (__tanf_lut[2] * ax.f);
	b = (__tanf_lut[1] * ax.f) * xx + (__tanf_lut[3] * ax.f);
	xx = xx * xx;
	r = b + a * xx;

	//cosine
	c.f = 1.0 - r * r;
	
	//fast invsqrt approximation (2x newton iterations)
    cc = c.f;
	c.i = 0x5F3759DF - (c.i >> 1);		//VRSQRTE
	cx = cc * c.f;
	a = (3.0f - cx * c.f) / 2;			//VRSQRTS
	c.f = c.f * a;		
	cx = cc * c.f;
	a = (3.0f - cx * c.f) / 2;
    c.f = c.f * a;	

	r = r * c.f;
	
	return r;
}


float tanf_neon_hfp(float x)
{
#ifdef __MATH_NEON
	asm volatile (

	"vdup.f32 		d0, d0[0]				\n\t"	//d0 = {x, x}
	"vabs.f32 		d1, d0					\n\t"	//d1 = {ax, ax}
	
	//Range Reduction:
	"vld1.32 		d3, [%0]				\n\t"	//d3 = {invrange, range}
	"vmul.f32 		d2, d1, d3[0]			\n\t"	//d2 = d1 * d3[0] 
	"vcvt.u32.f32 	d2, d2					\n\t"	//d2 = (int) d2
	"vcvt.f32.u32 	d4, d2					\n\t"	//d4 = (float) d2
	"vmls.f32 		d1, d4, d3[1]			\n\t"	//d1 = d1 - d4 * d3[1]
	
	//Checking Quadrant:
	//ax = ax - (k&1) * M_PI_2
	"vmov.i32 		d4, #1					\n\t"	//d4 = 1
	"vand.i32 		d2, d2, d4				\n\t"	//d2 = d2 & d4
	"vcvt.f32.u32 	d2, d2					\n\t"	//d2 = (float) d2
	"vmls.f32 		d1, d2, d3[1]			\n\t"	//d1 = d1 - d2 * d3[1]
	
	//ax = ax ^ ( x.i & 0x800000000)
	"vmov.i32 		d4, #0x80000000			\n\t"	//d4 = 0x80000000
	"vand.i32 		d0, d0, d4				\n\t"	//d0 = d0 & d4
	"veor.i32 		d1, d1, d0				\n\t"	//d1 = d1 ^ d0
	
	//polynomial:
	"vmul.f32 		d2, d1, d1				\n\t"	//d2 = d1*d1 = {x^2, x^2}	
	"vld1.32 		{d4, d5}, [%1]			\n\t"	//d4 = {p7, p3}, d5 = {p5, p1}
	"vmul.f32 		d3, d2, d2				\n\t"	//d3 = d2*d2 = {x^4, x^4}		
	"vmul.f32 		q0, q2, d1[0]			\n\t"	//q0 = q2 * d1[0] = {p7x, p3x, p5x, p1x}
	"vmla.f32 		d1, d0, d2[0]			\n\t"	//d1 = d1 + d0*d2 = {p5x + p7x^3, p1x + p3x^3}		
	"vmla.f32 		d1, d3, d1[0]			\n\t"	//d1 = d1 + d3*d0 = {..., p1x + p3x^3 + p5x^5 + p7x^7}		
	
	//cosine
	"vmov.f32 		s1, #1.0				\n\t"	//d0[1] = 1.0
	"vmls.f32 		d0, d1, d1				\n\t"	//d0 = {..., 1.0 - sx*sx}
	
	//invsqrt approx
	"vmov.f32 		d2, d0					\n\t"	//d2 = d0
	"vrsqrte.f32 	d0, d0					\n\t"	//d0 = ~ 1.0 / sqrt(d0)
	"vmul.f32 		d3, d0, d2				\n\t"	//d3 = d0 * d2
	"vrsqrts.f32 	d4, d3, d0				\n\t"	//d4 = (3 - d0 * d3) / 2 	
	"vmul.f32 		d0, d0, d4				\n\t"	//d0 = d0 * d4	
	"vmul.f32 		d3, d0, d2				\n\t"	//d3 = d0 * d2	
	"vrsqrts.f32 	d4, d3, d0				\n\t"	//d4 = (3 - d0 * d3) / 2	
	"vmul.f32 		d0, d0, d4				\n\t"	//d0 = d0 * d4	
	
	"vmul.f32 		d0, d0, d1				\n\t"	//d0 = d0 * d1
	
	"vmov.f32 		s0, s1					\n\t"	//s0 = s1
	
	:: "r"(__tanf_rng), "r"(__tanf_lut) 
    : "d0", "d1", "d2", "d3", "d4", "d5"
	);
#endif
}


float tanf_neon_sfp(float x)
{
#ifdef __MATH_NEON
	asm volatile ("vdup.f32 d0, r0 		\n\t");
	tanf_neon_hfp(x);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else
	return tanf_c(x);
#endif
};

