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

const float __sincosf_rng[2] = {
	2.0 / M_PI,
	M_PI / 2.0
};

const float __sincosf_lut[8] = {
	-0.00018365f,	//p7
	-0.00018365f,	//p7
	+0.00830636f,	//p5
	+0.00830636f,	//p5
	-0.16664831f,	//p3
	-0.16664831f,	//p3
	+0.99999661f,	//p1
	+0.99999661f,	//p1
};

void sincosf_c( float x, float r[2])
{
	union {
		float 	f;
		int 	i;
	} ax, bx;
	
	float y;
	float a, b, c, d, xx, yy;
	int m, n, o, p;
	
	y = x + __sincosf_rng[1];
	ax.f = fabsf(x);
	bx.f = fabsf(y);
	
	//Range Reduction:
	m = (int) (ax.f * __sincosf_rng[0]);	
	o = (int) (bx.f * __sincosf_rng[0]);	
	ax.f = ax.f - (((float)m) * __sincosf_rng[1]);
	bx.f = bx.f - (((float)o) * __sincosf_rng[1]);
	
	//Test Quadrant
	n = m & 1;
	p = o & 1;
	ax.f = ax.f - n * __sincosf_rng[1];	
	bx.f = bx.f - p * __sincosf_rng[1];	
	m = m >> 1;
	o = o >> 1;
	n = n ^ m;
	p = p ^ o;
	m = (x < 0.0);
	o = (y < 0.0);
	n = n ^ m;	
	p = p ^ o;	
	n = n << 31;
	p = p << 31;
	ax.i = ax.i ^ n; 
	bx.i = bx.i ^ p; 

	//Taylor Polynomial
	xx = ax.f * ax.f;	
	yy = bx.f * bx.f;
	r[0] = __sincosf_lut[0];
	r[1] = __sincosf_lut[1];
	r[0] = r[0] * xx + __sincosf_lut[2];
	r[1] = r[1] * yy + __sincosf_lut[3];
	r[0] = r[0] * xx + __sincosf_lut[4];
	r[1] = r[1] * yy + __sincosf_lut[5];
	r[0] = r[0] * xx + __sincosf_lut[6];
	r[1] = r[1] * yy + __sincosf_lut[7];
	r[0] = r[0] * ax.f;
	r[1] = r[1] * bx.f;

}

void sincosf_neon_hfp(float x, float r[2])
{
//HACK: Assumes for softfp that r1 = x, and for hardfp that s0 = x.
#ifdef __MATH_NEON
	asm volatile (
	//{x, y} = {x, x + pi/2}
	"vdup.f32 		d1, d0[0]				\n\t"	//d1 = {x, x}
	"vld1.32 		d3, [%1]				\n\t"	//d3 = {invrange, range}
	"vadd.f32 		d0, d1, d3				\n\t"	//d0 = d1 + d3
	"vmov.f32 		s0, s2					\n\t"	//d0[0] = d1[0]	
	"vabs.f32 		d1, d0					\n\t"	//d1 = {abs(x), abs(y)}
	
	//Range Reduction:
	"vmul.f32 		d2, d1, d3[0]			\n\t"	//d2 = d1 * d3[0] 
	"vcvt.u32.f32 	d2, d2					\n\t"	//d2 = (int) d2
	"vcvt.f32.u32 	d4, d2					\n\t"	//d4 = (float) d2
	"vmls.f32 		d1, d4, d3[1]			\n\t"	//d1 = d1 - d4 * d3[1]
	
	//Checking Quadrant:
	//ax = ax - (k&1) * M_PI_2
	"vmov.i32	 	d4, #1					\n\t"	//d4 = 1
	"vand.i32	 	d4, d4, d2				\n\t"	//d4 = d4 & d2
	"vcvt.f32.u32 	d5, d4					\n\t"	//d5 = (float) d4
	"vmls.f32 		d1, d5, d3[1]			\n\t"	//d1 = d1 - d5 * d3[1]

	//ax = ax ^ ((k & 1) ^ (k >> 1) ^ (x < 0) << 31)
	"vshr.u32 		d3, d2, #1				\n\t"	//d3 = d2 >> 1
	"veor.i32 		d4, d4, d3				\n\t"	//d4 = d4 ^ d3	
	"vclt.f32 		d3, d0, #0				\n\t"	//d3 = (d0 < 0.0)
	"veor.i32 		d4, d4, d3				\n\t"	//d4 = d4 ^ d3	
	"vshl.i32 		d4, d4, #31				\n\t"	//d4 = d4 << 31
	"veor.i32 		d0, d1, d4				\n\t"	//d0 = d1 ^ d4
	
	//polynomial:
	"vldm 			%2!, {d2, d3}	 		\n\t"	//d2 = {p7, p7}, d3 = {p5, p5}, r3 += 4;
	"vmul.f32 		d1, d0, d0				\n\t"	//d1 = d0 * d0 = {x^2, y^2}
	"vldm 			%2!, {d4}				\n\t"	//d4 = {p3, p3}, r3 += 2;
	"vmla.f32 		d3, d2, d1				\n\t"	//d3 = d3 + d2 * d1;	
	"vldm	 		%2!, {d5}				\n\t"	//d5 = {p1, p1}, r3 += 2;
	"vmla.f32 		d4, d3, d1				\n\t"	//d4 = d4 + d3 * d1;	
	"vmla.f32 		d5, d4, d1				\n\t"	//d5 = d5 + d4 * d1;	
	"vmul.f32 		d5, d5, d0				\n\t"	//d5 = d5 * d0;	
	
	"vstm.f32 		%0, {d5}				\n\t"	//r[0] = d5[0], r[1]=d5[1];	
	
	: "+r"(r)
	: "r"(__sincosf_rng), "r"(__sincosf_lut) 
    : "d0", "d1", "d2", "d3", "d4", "d5"
	);
#else
	sincosf_c(x, r);
#endif
}

void sincosf_neon_sfp(float x, float r[2])
{
#ifdef __MATH_NEON
	asm volatile ("vdup.f32 d0, r0 		\n\t");
	sincosf_neon_hfp(x, r);
	asm volatile ("vmov.f32 r0, s0 		\n\t");
#else 
    sincosf_c(x, r);
#endif
};

