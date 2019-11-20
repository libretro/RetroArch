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
matmul4_c(float m0[16], float m1[16], float d[16])
{
	d[0] = m0[0]*m1[0] + m0[4]*m1[1] + m0[8]*m1[2] + m0[12]*m1[3];
	d[1] = m0[1]*m1[0] + m0[5]*m1[1] + m0[9]*m1[2] + m0[13]*m1[3];
	d[2] = m0[2]*m1[0] + m0[6]*m1[1] + m0[10]*m1[2] + m0[14]*m1[3];
	d[3] = m0[3]*m1[0] + m0[7]*m1[1] + m0[11]*m1[2] + m0[15]*m1[3];
	d[4] = m0[0]*m1[4] + m0[4]*m1[5] + m0[8]*m1[6] + m0[12]*m1[7];
	d[5] = m0[1]*m1[4] + m0[5]*m1[5] + m0[9]*m1[6] + m0[13]*m1[7];
	d[6] = m0[2]*m1[4] + m0[6]*m1[5] + m0[10]*m1[6] + m0[14]*m1[7];
	d[7] = m0[3]*m1[4] + m0[7]*m1[5] + m0[11]*m1[6] + m0[15]*m1[7];
	d[8] = m0[0]*m1[8] + m0[4]*m1[9] + m0[8]*m1[10] + m0[12]*m1[11];
	d[9] = m0[1]*m1[8] + m0[5]*m1[9] + m0[9]*m1[10] + m0[13]*m1[11];
	d[10] = m0[2]*m1[8] + m0[6]*m1[9] + m0[10]*m1[10] + m0[14]*m1[11];
	d[11] = m0[3]*m1[8] + m0[7]*m1[9] + m0[11]*m1[10] + m0[15]*m1[11];
	d[12] = m0[0]*m1[12] + m0[4]*m1[13] + m0[8]*m1[14] + m0[12]*m1[15];
	d[13] = m0[1]*m1[12] + m0[5]*m1[13] + m0[9]*m1[14] + m0[13]*m1[15];
	d[14] = m0[2]*m1[12] + m0[6]*m1[13] + m0[10]*m1[14] + m0[14]*m1[15];
	d[15] = m0[3]*m1[12] + m0[7]*m1[13] + m0[11]*m1[14] + m0[15]*m1[15];
}

void 
matmul4_neon(float m0[16], float m1[16], float d[16])
{
#ifdef __MATH_NEON
	asm volatile (
	"vld1.32 		{d0, d1}, [%1]!			\n\t"	//q0 = m1
	"vld1.32 		{d2, d3}, [%1]!			\n\t"	//q1 = m1+4
	"vld1.32 		{d4, d5}, [%1]!			\n\t"	//q2 = m1+8
	"vld1.32 		{d6, d7}, [%1]			\n\t"	//q3 = m1+12
	"vld1.32 		{d16, d17}, [%0]!		\n\t"	//q8 = m0
	"vld1.32 		{d18, d19}, [%0]!		\n\t"	//q9 = m0+4
	"vld1.32 		{d20, d21}, [%0]!		\n\t"	//q10 = m0+8
	"vld1.32 		{d22, d23}, [%0]		\n\t"	//q11 = m0+12

	"vmul.f32 		q12, q8, d0[0] 			\n\t"	//q12 = q8 * d0[0]
	"vmul.f32 		q13, q8, d2[0] 			\n\t"	//q13 = q8 * d2[0]
	"vmul.f32 		q14, q8, d4[0] 			\n\t"	//q14 = q8 * d4[0]
	"vmul.f32 		q15, q8, d6[0]	 		\n\t"	//q15 = q8 * d6[0]
	"vmla.f32 		q12, q9, d0[1] 			\n\t"	//q12 = q9 * d0[1]
	"vmla.f32 		q13, q9, d2[1] 			\n\t"	//q13 = q9 * d2[1]
	"vmla.f32 		q14, q9, d4[1] 			\n\t"	//q14 = q9 * d4[1]
	"vmla.f32 		q15, q9, d6[1] 			\n\t"	//q15 = q9 * d6[1]
	"vmla.f32 		q12, q10, d1[0] 		\n\t"	//q12 = q10 * d0[0]
	"vmla.f32 		q13, q10, d3[0] 		\n\t"	//q13 = q10 * d2[0]
	"vmla.f32 		q14, q10, d5[0] 		\n\t"	//q14 = q10 * d4[0]
	"vmla.f32 		q15, q10, d7[0] 		\n\t"	//q15 = q10 * d6[0]
	"vmla.f32 		q12, q11, d1[1] 		\n\t"	//q12 = q11 * d0[1]
	"vmla.f32 		q13, q11, d3[1] 		\n\t"	//q13 = q11 * d2[1]
	"vmla.f32 		q14, q11, d5[1] 		\n\t"	//q14 = q11 * d4[1]
	"vmla.f32 		q15, q11, d7[1]	 		\n\t"	//q15 = q11 * d6[1]

	"vst1.32 		{d24, d25}, [%2]! 		\n\t"	//d = q12	
	"vst1.32 		{d26, d27}, [%2]!		\n\t"	//d+4 = q13	
	"vst1.32 		{d28, d29}, [%2]! 		\n\t"	//d+8 = q14	
	"vst1.32 		{d30, d31}, [%2]	 	\n\t"	//d+12 = q15	

	: "+r"(m0), "+r"(m1), "+r"(d) : 
    : "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	"memory"
	);	
#else
	matmul4_c(m0, m1, d);
#endif
}


//matrix vector multiplication. d = m * v
void
matvec4_c(float m[16], float v[4], float d[4])
{
	d[0] = m[0]*v[0] + m[4]*v[1] + m[8]*v[2] + m[12]*v[3];
	d[1] = m[1]*v[0] + m[5]*v[1] + m[9]*v[2] + m[13]*v[3];
	d[2] = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14]*v[3];
	d[3] = m[3]*v[0] + m[7]*v[1] + m[11]*v[2] + m[15]*v[3];
}

void
matvec4_neon(float m[16], float v[4], float d[4])
{
#ifdef __MATH_NEON
	asm volatile (
	"vld1.32 		{d0, d1}, [%1]			\n\t"	//Q0 = v
	"vld1.32 		{d18, d19}, [%0]!		\n\t"	//Q1 = m
	"vld1.32 		{d20, d21}, [%0]!		\n\t"	//Q2 = m+4
	"vld1.32 		{d22, d23}, [%0]!		\n\t"	//Q3 = m+8
	"vld1.32 		{d24, d25}, [%0]!		\n\t"	//Q4 = m+12	
	
	"vmul.f32 		q13, q9, d0[0]			\n\t"	//Q5 = Q1*Q0[0]
	"vmla.f32 		q13, q10, d0[1]			\n\t"	//Q5 += Q1*Q0[1] 
	"vmla.f32 		q13, q11, d1[0]			\n\t"	//Q5 += Q2*Q0[2] 
	"vmla.f32 		q13, q12, d1[1]			\n\t"	//Q5 += Q3*Q0[3]
	
	"vst1.32 		{d26, d27}, [%2] 		\n\t"	//Q4 = m+12	
	: 
	: "r"(m), "r"(v), "r"(d) 
    : "q0", "q9", "q10","q11", "q12", "q13", "memory"
	);	
#else
	matvec4_c(m, v, d);
#endif
}





