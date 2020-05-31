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
Matrices are specified in row major format:

| x0 x2 |
| x1 x3 |

therefore m[2] = x2

*/

#include "math_neon.h"

//matrix matrix multipication. d = m0 * m1;
void
matmul3_c(float m0[9], float m1[9], float d[9])
{
	d[0] = m0[0]*m1[0] + m0[3]*m1[1] + m0[6]*m1[2];
	d[1] = m0[1]*m1[0] + m0[4]*m1[1] + m0[7]*m1[2];
	d[2] = m0[2]*m1[0] + m0[5]*m1[1] + m0[8]*m1[2];
	d[3] = m0[0]*m1[3] + m0[3]*m1[4] + m0[6]*m1[5];
	d[4] = m0[1]*m1[3] + m0[4]*m1[4] + m0[7]*m1[5];
	d[5] = m0[2]*m1[3] + m0[5]*m1[4] + m0[8]*m1[5];
	d[6] = m0[0]*m1[6] + m0[3]*m1[7] + m0[6]*m1[8];
	d[7] = m0[1]*m1[6] + m0[4]*m1[7] + m0[7]*m1[8];
	d[8] = m0[2]*m1[6] + m0[5]*m1[7] + m0[8]*m1[8];
}

void 
matmul3_neon(float m0[9], float m1[9], float d[9])
{
#ifdef __MATH_NEON
	asm volatile (
	"vld1.32 		{d0, d1}, [%1]!			\n\t"	//q0 = m1
	"vld1.32 		{d2, d3}, [%1]!			\n\t"	//q1 = m1+4
	"flds 			s8, [%1]				\n\t"	//q2 = m1+8
	
	"vld1.32 		{d6, d7}, [%0]			\n\t"	//q3[0] = m0
	"add 			%0, %0, #12				\n\t"	//q3[0] = m0
	"vld1.32 		{d8, d9}, [%0]			\n\t"	//q4[0] = m0+12
	"add 			%0, %0, #12				\n\t"	//q3[0] = m0
	"vld1.32 		{d10}, [%0]				\n\t"	//q5[0] = m0+24
	"add 			%0, %0, #8				\n\t"	//q3[0] = m0
	"flds 			s22, [%0]				\n\t"	//q2 = m1+8
	
	"vmul.f32 		q6, q3, d0[0] 			\n\t"	//q12 = q3 * d0[0]
	"vmul.f32 		q7, q3, d1[1] 			\n\t"	//q13 = q3 * d2[0]
	"vmul.f32 		q8, q3, d3[0] 			\n\t"	//q14 = q3 * d4[0]
	"vmla.f32 		q6, q4, d0[1] 			\n\t"	//q12 = q9 * d0[1]
	"vmla.f32 		q7, q4, d2[0] 			\n\t"	//q13 = q9 * d2[1]
	"vmla.f32 		q8, q4, d3[1] 			\n\t"	//q14 = q9 * d4[1]
	"vmla.f32 		q6, q5, d1[0] 			\n\t"	//q12 = q10 * d0[0]
	"vmla.f32 		q7, q5, d2[1] 			\n\t"	//q13 = q10 * d2[0]
	"vmla.f32 		q8, q5, d4[0] 			\n\t"	//q14 = q10 * d4[0]

	"vmov.f32 		q0, q8 					\n\t"	//q14 = q10 * d4[0]
	"vst1.32 		{d12, d13}, [%2] 		\n\t"	//d = q12
	"add 			%2, %2, #12				\n\t"	//q3[0] = m0
	"vst1.32 		{d14, d15}, [%2] 		\n\t"	//d+4 = q13	
	"add 			%2, %2, #12				\n\t"	//q3[0] = m0
	"vst1.32 		{d0}, [%2] 				\n\t"	//d+8 = q14	
	"add 			%2, %2, #8				\n\t"	//q3[0] = m0
	"fsts 			s2, [%2] 				\n\t"	//d = q12	
	
	: "+r"(m0), "+r"(m1), "+r"(d): 
    : "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15", "memory"
	);	
#else
	matmul3_c(m0, m1, d);
#endif
};

//matrix vector multiplication. d = m * v
void
matvec3_c(float m[9], float v[3], float d[3])
{
	d[0] = m[0]*v[0] + m[3]*v[1] + m[6]*v[2];
	d[1] = m[1]*v[0] + m[4]*v[1] + m[7]*v[2];
	d[2] = m[2]*v[0] + m[5]*v[1] + m[8]*v[2];
}

void
matvec3_neon(float m[9], float v[3], float d[3])
{
#ifdef __MATH_NEON
	int tmp;
	asm volatile (
	"mov 			%3, #12					\n\t"	//r3 = 12
	"vld1.32 		{d0, d1}, [%1]			\n\t"	//Q0 = v
	"vld1.32 		{d2, d3}, [%0], %3		\n\t"	//Q1 = m
	"vld1.32 		{d4, d5}, [%0], %3		\n\t"	//Q2 = m+12
	"vld1.32 		{d6, d7}, [%0], %3		\n\t"	//Q3 = m+24
	
	"vmul.f32 		q9, q1, d0[0]			\n\t"	//Q9 = Q1*Q0[0]
	"vmla.f32 		q9, q2, d0[1]			\n\t"	//Q9 += Q2*Q0[1] 
	"vmla.f32 		q9, q3, d1[0]			\n\t"	//Q9 += Q3*Q0[2] 
	"vmov.f32 		q0, q9					\n\t"	//Q0 = q9
	
	"vst1.32 		d0, [%2]! 				\n\t"	//r2 = D24	
	"fsts 			s2, [%2] 				\n\t"	//r2 = D25[0]	

	: "+r"(m), "+r"(v), "+r"(d), "+r"(tmp):
    : "q0", "q9", "q10","q11", "q12", "q13", "memory"
	);	
#else
	matvec3_c(m, v, d);
#endif
}
