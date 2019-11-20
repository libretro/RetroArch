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

#ifndef __MATH_NEON_H__ 
#define __MATH_NEON_H__ 

#if !defined(__i386__) && defined(__arm__)
//if defined neon ASM routines are used, otherwise all calls to *_neon 
//functions are rerouted to their equivalent *_c function.
#define __MATH_NEON			

//Default Floating Point value ABI: 0=softfp, 1=hardfp. Only effects *_neon routines.
//You can access the hardfp versions directly via the *_hard suffix. 
//You can access the softfp versions directly via the *_soft suffix. 
#define __MATH_FPABI 	0	

#endif

#ifdef GCC
#define ALIGN(A) __attribute__ ((aligned (A))
#else
#define ALIGN(A)
#endif

#ifndef _MATH_H
#define M_PI		3.14159265358979323846	/* pi */
#define M_PI_2		1.57079632679489661923	/* pi/2 */
#define M_PI_4		0.78539816339744830962	/* pi/4 */
#define M_E			2.7182818284590452354	/* e */
#define M_LOG2E		1.4426950408889634074	/* log_2 e */
#define M_LOG10E	0.43429448190325182765	/* log_10 e */
#define M_LN2		0.69314718055994530942	/* log_e 2 */
#define M_LN10		2.30258509299404568402	/* log_e 10 */
#define M_1_PI		0.31830988618379067154	/* 1/pi */
#define M_2_PI		0.63661977236758134308	/* 2/pi */
#define M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */
#endif 

#if __MATH_FPABI == 1
#define sinf_neon		sinf_neon_hfp
#define cosf_neon		cosf_neon_hfp
#define	sincosf_neon	sincosf_neon_hfp
#define tanf_neon		tanf_neon_hfp
#define atanf_neon		atanf_neon_hfp
#define atan2f_neon		atan2f_neon_hfp
#define asinf_neon		asinf_neon_hfp
#define acosf_neon		acosf_neon_hfp
#define sinhf_neon		sinhf_neon_hfp
#define coshf_neon		coshf_neon_hfp
#define tanhf_neon		tanhf_neon_hfp
#define expf_neon		expf_neon_hfp
#define logf_neon		logf_neon_hfp
#define log10f_neon		log10f_neon_hfp
#define powf_neon		powf_neon_hfp
#define floorf_neon		floorf_neon_hfp
#define ceilf_neon		ceilf_neon_hfp
#define fabsf_neon		fabsf_neon_hfp
#define ldexpf_neon		ldexpf_neon_hfp
#define frexpf_neon		frexpf_neon_hfp
#define fmodf_neon		fmodf_neon_hfp
#define modf_neon		modf_neon_hfp
#define sqrtf_neon		sqrtf_neon_hfp
#define invsqrtf_neon	invsqrtf_neon_hfp
#else
#define sinf_neon		sinf_neon_sfp
#define cosf_neon		cosf_neon_sfp
#define	sincosf_neon	sincosf_neon_sfp
#define tanf_neon		tanf_neon_sfp
#define atanf_neon		atanf_neon_sfp
#define atan2f_neon		atan2f_neon_sfp
#define asinf_neon		asinf_neon_sfp
#define acosf_neon		acosf_neon_sfp
#define sinhf_neon		sinhf_neon_sfp
#define coshf_neon		coshf_neon_sfp
#define tanhf_neon		tanhf_neon_sfp
#define expf_neon		expf_neon_sfp
#define logf_neon		logf_neon_sfp
#define log10f_neon		log10f_neon_sfp
#define powf_neon		powf_neon_sfp
#define floorf_neon		floorf_neon_sfp
#define ceilf_neon		ceilf_neon_sfp
#define fabsf_neon		fabsf_neon_sfp
#define ldexpf_neon		ldexpf_neon_sfp
#define frexpf_neon		frexpf_neon_sfp
#define fmodf_neon		fmodf_neon_sfp
#define modf_neon		modf_neon_sfp
#define sqrtf_neon		sqrtf_neon_sfp
#define invsqrtf_neon	invsqrtf_neon_sfp

#define dot2_neon		dot2_neon_sfp
#define dot3_neon		dot3_neon_sfp
#define dot4_neon		dot4_neon_sfp
#endif

/* 
function:	enable_runfast
			this function enables the floating point runfast mode on the 
			ARM Cortex A8.  	
*/
void		enable_runfast();


float dot2_c(float v0[2], float v1[2]);
float dot2_neon(float v0[2], float v1[2]);
float dot3_c(float v0[3], float v1[3]);
float dot3_neon(float v0[3], float v1[3]);
float dot4_c(float v0[4], float v1[4]);
float dot4_neon(float v0[4], float v1[4]);

void cross3_c(float v0[3], float v1[3], float d[3]);
void cross3_neon(float v0[3], float v1[3], float d[3]);

void normalize2_c(float v[2], float d[2]);
void normalize2_neon(float v[2], float d[2]);
void normalize3_c(float v[3], float d[3]);
void normalize3_neon(float v[3], float d[3]);
void normalize4_c(float v[4], float d[4]);
void normalize4_neon(float v[4], float d[4]);

/* 
function:	matmul2
arguments:  m0 2x2 matrix, m1 2x2 matrix
return: 	d 2x2 matrix
expression: d = m0 * m1
*/
void		matmul2_c(float m0[4], float m1[4], float d[4]);
void		matmul2_neon(float m0[4], float m1[4], float d[4]);

/* 
function:	matmul3
arguments:  m0 3x3 matrix, m1 3x3 matrix
return: 	d 3x3 matrix
expression: d = m0 * m1
*/
void		matmul3_c(float m0[9], float m1[9], float d[9]);
void		matmul3_neon(float m0[9], float m1[9], float d[9]);

/* 
function:	matmul4
arguments:  m0 4x4 matrix, m1 4x4 matrix
return: 	d 4x4 matrix
expression: d = m0 * m1
*/
void		matmul4_c(float m0[16], float m1[16], float d[16]);
void		matmul4_neon(float m0[16], float m1[16], float d[16]);

/* 
function:	matvec2
arguments:  m 2x2 matrix, v 2 element vector
return: 	d 2x2 matrix
expression: d = m * v
*/
void		matvec2_c(float m[4], float v[2], float d[2]);
void		matvec2_neon(float m[4], float v[2], float d[2]);

/* 
function:	matvec3
arguments:  m 3x3 matrix, v 3 element vector
return: 	d 3x3 matrix
expression: d = m * v
*/
void		matvec3_c(float m[9], float v[3], float d[3]);
void		matvec3_neon(float m[9], float v[3], float d[3]);

/* 
function:	matvec4
arguments:  m 4x4 matrix, v 4 element vector
return: 	d 4x4 matrix
expression: d = m * v
*/
void		matvec4_c(float m[16], float v[4], float d[4]);
void		matvec4_neon(float m[16], float v[4], float d[4]);

/* 
function:	sinf
arguments:  x radians
return: 	the sine function evaluated at x radians.	
expression: r = sin(x) 	
*/
float 		sinf_c(float x);
float 		sinf_neon_hfp(float x);
float 		sinf_neon_sfp(float x);

/* 
function:	cosf
arguments:  x radians
return: 	the cosine function evaluated at x radians.	
expression: r = cos(x) 	
notes:		computed using cos(x) = sin(x + pi/2)
*/
float 		cosf_c(float x);
float 		cosf_neon_hfp(float x);
float 		cosf_neon_sfp(float x);

/* 
function:	sincosf
arguments:  x radians, r[2] result array.
return: 	both the sine and the cosine evaluated at x radians.	
expression: r = {sin(x), cos(x)} 	
notes:		faster than evaluating seperately.
*/
void		sincosf_c(float x, float r[2]);
void		sincosf_neon_hfp(float x, float r[2]);
void		sincosf_neon_sfp(float x, float r[2]);

/* 
function:	sinfv
return: 	the sine function evaluated at x[i] radians 	
expression: r[i] = sin(x[i])	
notes:		faster than evaluating individually.
			r and x can be the same memory location.
*/
void		sinfv_c(float *x, int n, float *r);
void  		sinfv_neon(float *x, int n, float *r);

/* 
function:	tanf
return: 	the tangent evaluated at x radians.	
expression: r = tan(x) 	
notes:		computed using tan(x) = sin(x) / cos(x)
*/
float 		tanf_c(float x);
float 		tanf_neon_hfp(float x);
float 		tanf_neon_sfp(float x);

/* 
function:	atanf
return: 	the arctangent evaluated at x.	
expression: r = atan(x) 	
*/
float 		atanf_c(float x);
float 		atanf_neon_hfp(float x);
float 		atanf_neon_sfp(float x);

/* 
function:	atanf
return: 	the arctangent evaluated at x.	
expression: r = atan(x) 	
*/
float 		atan2f_c(float y, float x);
float 		atan2f_neon_hfp(float y, float x);
float 		atan2f_neon_sfp(float y, float x);

/* 
function:	asinf
return: 	the arcsine evaluated at x.	
expression: r = asin(x) 	
*/
float 		asinf_c(float x);
float 		asinf_neon_hfp(float x);
float 		asinf_neon_sfp(float x);

/* 
function:	acosf
return: 	the arcsine evaluated at x.	
expression: r = asin(x) 	
*/
float 		acosf_c(float x);
float 		acosf_neon_hfp(float x);
float 		acosf_neon_sfp(float x);

/* 
function:	sinhf
return: 	the arcsine evaluated at x.	
expression: r = asin(x) 	
*/
float 		sinhf_c(float x);
float 		sinhf_neon_hfp(float x);
float 		sinhf_neon_sfp(float x);

/* 
function:	coshf
return: 	the arcsine evaluated at x.	
expression: r = asin(x) 	
*/
float 		coshf_c(float x);
float 		coshf_neon_hfp(float x);
float 		coshf_neon_sfp(float x);

/* 
function:	tanhf
return: 	the arcsine evaluated at x.	
expression: r = asin(x) 	
*/
float 		tanhf_c(float x);
float 		tanhf_neon_hfp(float x);
float 		tanhf_neon_sfp(float x);

/* 
function:	expf
return: 	the natural exponential evaluated at x.	
expression: r = e ** x	
*/
float 		expf_c(float x);
float 		expf_neon_hfp(float x);
float 		expf_neon_sfp(float x);

/* 
function:	logf
return: 	the value of the natural logarithm of x.	
expression: r = ln(x)	
notes:		assumes x > 0
*/
float 		logf_c(float x);
float 		logf_neon_hfp(float x);
float 		logf_neon_sfp(float x);

/* 
function:	log10f
return: 	the value of the power 10 logarithm of x.	
expression: r = log10(x)	
notes:		assumes x > 0
*/
float 		log10f_c(float x);
float 		log10f_neon_hfp(float x);
float 		log10f_neon_sfp(float x);

/* 
function:	powf
return: 	x raised to the power of n, x ** n.
expression: r = x ** y	
notes:		computed using e ** (y * ln(x))
*/
float 		powf_c(float x, float n);
float 		powf_neon_sfp(float x, float n);
float 		powf_neon_hfp(float x, float n);

/* 
function:	floorf
return: 	x rounded down (towards negative infinity) to its nearest 
			integer value.	
notes:		assumes |x| < 2 ** 31
*/
float 		floorf_c(float x);
float 		floorf_neon_sfp(float x);
float 		floorf_neon_hfp(float x);

/* 
function:	ceilf
return: 	x rounded up (towards positive infinity) to its nearest 
			integer value.	
notes:		assumes |x| < 2 ** 31
*/
float 		ceilf_c(float x);
float 		ceilf_neon_hfp(float x);
float 		ceilf_neon_sfp(float x);

/* 
function:	fabsf
return: 	absolute vvalue of x	
notes:		assumes |x| < 2 ** 31
*/
float 		fabsf_c(float x);
float 		fabsf_neon_hfp(float x);
float 		fabsf_neon_sfp(float x);

/* 
function:	ldexpf
return: 	the value of m multiplied by 2 to the power of e. 
expression: r = m * (2 ** e)
*/
float 		ldexpf_c(float m, int e);
float 		ldexpf_neon_hfp(float m, int e);
float 		ldexpf_neon_sfp(float m, int e);

/* 
function:	frexpf
return: 	the exponent and mantissa of x 
*/
float 		frexpf_c(float x, int *e);
float 		frexpf_neon_hfp(float x, int *e);
float 		frexpf_neon_sfp(float x, int *e);

/* 
function:	fmodf
return: 	the remainder of x divided by y, x % y	
expression: r = x - floor(x / y) * y;
notes:		assumes that |x / y| < 2 ** 31 
*/
float 		fmodf_c(float x, float y);
float 		fmodf_neon_hfp(float x, float y);
float 		fmodf_neon_sfp(float x, float y);

/* 
function:	modf
return: 	breaks x into the integer (i) and fractional part (return)
notes:		assumes that |x| < 2 ** 31 
*/
float 		modf_c(float x, int *i);
float 		modf_neon_hfp(float x, int *i);
float 		modf_neon_sfp(float x, int *i);

/* 
function:	sqrtf
return: 	(x^0.5)
notes:		 
*/
float 		sqrtf_c(float x);
float 		sqrtf_neon_hfp(float x);
float 		sqrtf_neon_sfp(float x);


/* 
function:	invsqrtf
return: 	1.0f / (x^0.5)
notes:		 
*/
float 		invsqrtf_c(float x);
float 		invsqrtf_neon_hfp(float x);
float 		invsqrtf_neon_sfp(float x);

#endif
