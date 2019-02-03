/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    xnamathmatrix.inl

Abstract:

	XNA math library for Windows and Xbox 360: Matrix functions
--*/

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifndef __XNAMATHMATRIX_INL__
#define __XNAMATHMATRIX_INL__

/****************************************************************************
 *
 * Matrix
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

// Return TRUE if any entry in the matrix is NaN
XMFINLINE BOOL XMMatrixIsNaN
(
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT i, uTest;
    const UINT *pWork;

    i = 16;
    pWork = (const UINT *)(&M.m[0][0]);
    do {
        // Fetch value into integer unit
        uTest = pWork[0];
        // Remove sign
        uTest &= 0x7FFFFFFFU;
        // NaN is 0x7F800001 through 0x7FFFFFFF inclusive
        uTest -= 0x7F800001U;
        if (uTest<0x007FFFFFU) {
            break;      // NaN found
        }
        ++pWork;        // Next entry
    } while (--i);
    return (i!=0);      // i == 0 if nothing matched
#elif defined(_XM_SSE_INTRINSICS_)
    // Load in registers
    XMVECTOR vX = M.r[0];
    XMVECTOR vY = M.r[1];
    XMVECTOR vZ = M.r[2];
    XMVECTOR vW = M.r[3];
    // Test themselves to check for NaN
    vX = _mm_cmpneq_ps(vX,vX);
    vY = _mm_cmpneq_ps(vY,vY);
    vZ = _mm_cmpneq_ps(vZ,vZ);
    vW = _mm_cmpneq_ps(vW,vW);
    // Or all the results
    vX = _mm_or_ps(vX,vZ);
    vY = _mm_or_ps(vY,vW);
    vX = _mm_or_ps(vX,vY);
    // If any tested true, return true
    return (_mm_movemask_ps(vX)!=0);
#else
#endif
}

//------------------------------------------------------------------------------

// Return TRUE if any entry in the matrix is +/-INF
XMFINLINE BOOL XMMatrixIsInfinite
(
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT i, uTest;
    const UINT *pWork;

    i = 16;
    pWork = (const UINT *)(&M.m[0][0]);
    do {
        // Fetch value into integer unit
        uTest = pWork[0];
        // Remove sign
        uTest &= 0x7FFFFFFFU;
        // INF is 0x7F800000
        if (uTest==0x7F800000U) {
            break;      // INF found
        }
        ++pWork;        // Next entry
    } while (--i);
    return (i!=0);      // i == 0 if nothing matched
#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the sign bits
    XMVECTOR vTemp1 = _mm_and_ps(M.r[0],g_XMAbsMask);
    XMVECTOR vTemp2 = _mm_and_ps(M.r[1],g_XMAbsMask);
    XMVECTOR vTemp3 = _mm_and_ps(M.r[2],g_XMAbsMask);
    XMVECTOR vTemp4 = _mm_and_ps(M.r[3],g_XMAbsMask);
    // Compare to infinity
    vTemp1 = _mm_cmpeq_ps(vTemp1,g_XMInfinity);
    vTemp2 = _mm_cmpeq_ps(vTemp2,g_XMInfinity);
    vTemp3 = _mm_cmpeq_ps(vTemp3,g_XMInfinity);
    vTemp4 = _mm_cmpeq_ps(vTemp4,g_XMInfinity);
    // Or the answers together
    vTemp1 = _mm_or_ps(vTemp1,vTemp2);
    vTemp3 = _mm_or_ps(vTemp3,vTemp4);
    vTemp1 = _mm_or_ps(vTemp1,vTemp3);
    // If any are infinity, the signs are true.
    return (_mm_movemask_ps(vTemp1)!=0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Return TRUE if the XMMatrix is equal to identity
XMFINLINE BOOL XMMatrixIsIdentity
(
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)
    unsigned int uOne, uZero;
    const unsigned int *pWork;

    // Use the integer pipeline to reduce branching to a minimum
    pWork = (const unsigned int*)(&M.m[0][0]);
    // Convert 1.0f to zero and or them together
    uOne = pWork[0]^0x3F800000U;
    // Or all the 0.0f entries together
    uZero = pWork[1];
    uZero |= pWork[2];
    uZero |= pWork[3];
    // 2nd row
    uZero |= pWork[4];
    uOne |= pWork[5]^0x3F800000U;
    uZero |= pWork[6];
    uZero |= pWork[7];
    // 3rd row
    uZero |= pWork[8];
    uZero |= pWork[9];
    uOne |= pWork[10]^0x3F800000U;
    uZero |= pWork[11];
    // 4th row
    uZero |= pWork[12];
    uZero |= pWork[13];
    uZero |= pWork[14];
    uOne |= pWork[15]^0x3F800000U;
    // If all zero entries are zero, the uZero==0
    uZero &= 0x7FFFFFFF;    // Allow -0.0f
    // If all 1.0f entries are 1.0f, then uOne==0
    uOne |= uZero;
    return (uOne==0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp1 = _mm_cmpeq_ps(M.r[0],g_XMIdentityR0);
    XMVECTOR vTemp2 = _mm_cmpeq_ps(M.r[1],g_XMIdentityR1);
    XMVECTOR vTemp3 = _mm_cmpeq_ps(M.r[2],g_XMIdentityR2);
    XMVECTOR vTemp4 = _mm_cmpeq_ps(M.r[3],g_XMIdentityR3);
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    vTemp3 = _mm_and_ps(vTemp3,vTemp4);
    vTemp1 = _mm_and_ps(vTemp1,vTemp3);
    return (_mm_movemask_ps(vTemp1)==0x0f);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Perform a 4x4 matrix multiply by a 4x4 matrix
XMFINLINE XMMATRIX XMMatrixMultiply
(
    CXMMATRIX M1,
    CXMMATRIX M2
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMMATRIX mResult;
    // Cache the invariants in registers
    float x = M1.m[0][0];
    float y = M1.m[0][1];
    float z = M1.m[0][2];
    float w = M1.m[0][3];
    // Perform the operation on the first row
    mResult.m[0][0] = (M2.m[0][0]*x)+(M2.m[1][0]*y)+(M2.m[2][0]*z)+(M2.m[3][0]*w);
    mResult.m[0][1] = (M2.m[0][1]*x)+(M2.m[1][1]*y)+(M2.m[2][1]*z)+(M2.m[3][1]*w);
    mResult.m[0][2] = (M2.m[0][2]*x)+(M2.m[1][2]*y)+(M2.m[2][2]*z)+(M2.m[3][2]*w);
    mResult.m[0][3] = (M2.m[0][3]*x)+(M2.m[1][3]*y)+(M2.m[2][3]*z)+(M2.m[3][3]*w);
    // Repeat for all the other rows
    x = M1.m[1][0];
    y = M1.m[1][1];
    z = M1.m[1][2];
    w = M1.m[1][3];
    mResult.m[1][0] = (M2.m[0][0]*x)+(M2.m[1][0]*y)+(M2.m[2][0]*z)+(M2.m[3][0]*w);
    mResult.m[1][1] = (M2.m[0][1]*x)+(M2.m[1][1]*y)+(M2.m[2][1]*z)+(M2.m[3][1]*w);
    mResult.m[1][2] = (M2.m[0][2]*x)+(M2.m[1][2]*y)+(M2.m[2][2]*z)+(M2.m[3][2]*w);
    mResult.m[1][3] = (M2.m[0][3]*x)+(M2.m[1][3]*y)+(M2.m[2][3]*z)+(M2.m[3][3]*w);
    x = M1.m[2][0];
    y = M1.m[2][1];
    z = M1.m[2][2];
    w = M1.m[2][3];
    mResult.m[2][0] = (M2.m[0][0]*x)+(M2.m[1][0]*y)+(M2.m[2][0]*z)+(M2.m[3][0]*w);
    mResult.m[2][1] = (M2.m[0][1]*x)+(M2.m[1][1]*y)+(M2.m[2][1]*z)+(M2.m[3][1]*w);
    mResult.m[2][2] = (M2.m[0][2]*x)+(M2.m[1][2]*y)+(M2.m[2][2]*z)+(M2.m[3][2]*w);
    mResult.m[2][3] = (M2.m[0][3]*x)+(M2.m[1][3]*y)+(M2.m[2][3]*z)+(M2.m[3][3]*w);
    x = M1.m[3][0];
    y = M1.m[3][1];
    z = M1.m[3][2];
    w = M1.m[3][3];
    mResult.m[3][0] = (M2.m[0][0]*x)+(M2.m[1][0]*y)+(M2.m[2][0]*z)+(M2.m[3][0]*w);
    mResult.m[3][1] = (M2.m[0][1]*x)+(M2.m[1][1]*y)+(M2.m[2][1]*z)+(M2.m[3][1]*w);
    mResult.m[3][2] = (M2.m[0][2]*x)+(M2.m[1][2]*y)+(M2.m[2][2]*z)+(M2.m[3][2]*w);
    mResult.m[3][3] = (M2.m[0][3]*x)+(M2.m[1][3]*y)+(M2.m[2][3]*z)+(M2.m[3][3]*w);
    return mResult;
#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX mResult;
    // Use vW to hold the original row
    XMVECTOR vW = M1.r[0];
    // Splat the component X,Y,Z then W
    XMVECTOR vX = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(0,0,0,0));
    XMVECTOR vY = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(1,1,1,1));
    XMVECTOR vZ = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(2,2,2,2));
    vW = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(3,3,3,3));
    // Perform the opertion on the first row
    vX = _mm_mul_ps(vX,M2.r[0]);
    vY = _mm_mul_ps(vY,M2.r[1]);
    vZ = _mm_mul_ps(vZ,M2.r[2]);
    vW = _mm_mul_ps(vW,M2.r[3]);
    // Perform a binary add to reduce cumulative errors
    vX = _mm_add_ps(vX,vZ);
    vY = _mm_add_ps(vY,vW);
    vX = _mm_add_ps(vX,vY);
    mResult.r[0] = vX;
    // Repeat for the other 3 rows
    vW = M1.r[1];
    vX = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(0,0,0,0));
    vY = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(1,1,1,1));
    vZ = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(2,2,2,2));
    vW = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(3,3,3,3));
    vX = _mm_mul_ps(vX,M2.r[0]);
    vY = _mm_mul_ps(vY,M2.r[1]);
    vZ = _mm_mul_ps(vZ,M2.r[2]);
    vW = _mm_mul_ps(vW,M2.r[3]);
    vX = _mm_add_ps(vX,vZ);
    vY = _mm_add_ps(vY,vW);
    vX = _mm_add_ps(vX,vY);
    mResult.r[1] = vX;
    vW = M1.r[2];
    vX = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(0,0,0,0));
    vY = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(1,1,1,1));
    vZ = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(2,2,2,2));
    vW = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(3,3,3,3));
    vX = _mm_mul_ps(vX,M2.r[0]);
    vY = _mm_mul_ps(vY,M2.r[1]);
    vZ = _mm_mul_ps(vZ,M2.r[2]);
    vW = _mm_mul_ps(vW,M2.r[3]);
    vX = _mm_add_ps(vX,vZ);
    vY = _mm_add_ps(vY,vW);
    vX = _mm_add_ps(vX,vY);
    mResult.r[2] = vX;
    vW = M1.r[3];
    vX = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(0,0,0,0));
    vY = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(1,1,1,1));
    vZ = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(2,2,2,2));
    vW = _mm_shuffle_ps(vW,vW,_MM_SHUFFLE(3,3,3,3));
    vX = _mm_mul_ps(vX,M2.r[0]);
    vY = _mm_mul_ps(vY,M2.r[1]);
    vZ = _mm_mul_ps(vZ,M2.r[2]);
    vW = _mm_mul_ps(vW,M2.r[3]);
    vX = _mm_add_ps(vX,vZ);
    vY = _mm_add_ps(vY,vW);
    vX = _mm_add_ps(vX,vY);
    mResult.r[3] = vX;
    return mResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixMultiplyTranspose
(
    CXMMATRIX M1,
    CXMMATRIX M2
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMMATRIX mResult;
    // Cache the invariants in registers
    float x = M2.m[0][0];
    float y = M2.m[1][0];
    float z = M2.m[2][0];
    float w = M2.m[3][0];
    // Perform the operation on the first row
    mResult.m[0][0] = (M1.m[0][0]*x)+(M1.m[0][1]*y)+(M1.m[0][2]*z)+(M1.m[0][3]*w);
    mResult.m[0][1] = (M1.m[1][0]*x)+(M1.m[1][1]*y)+(M1.m[1][2]*z)+(M1.m[1][3]*w);
    mResult.m[0][2] = (M1.m[2][0]*x)+(M1.m[2][1]*y)+(M1.m[2][2]*z)+(M1.m[2][3]*w);
    mResult.m[0][3] = (M1.m[3][0]*x)+(M1.m[3][1]*y)+(M1.m[3][2]*z)+(M1.m[3][3]*w);
    // Repeat for all the other rows
    x = M2.m[0][1];
    y = M2.m[1][1];
    z = M2.m[2][1];
    w = M2.m[3][1];
    mResult.m[1][0] = (M1.m[0][0]*x)+(M1.m[0][1]*y)+(M1.m[0][2]*z)+(M1.m[0][3]*w);
    mResult.m[1][1] = (M1.m[1][0]*x)+(M1.m[1][1]*y)+(M1.m[1][2]*z)+(M1.m[1][3]*w);
    mResult.m[1][2] = (M1.m[2][0]*x)+(M1.m[2][1]*y)+(M1.m[2][2]*z)+(M1.m[2][3]*w);
    mResult.m[1][3] = (M1.m[3][0]*x)+(M1.m[3][1]*y)+(M1.m[3][2]*z)+(M1.m[3][3]*w);
    x = M2.m[0][2];
    y = M2.m[1][2];
    z = M2.m[2][2];
    w = M2.m[3][2];
    mResult.m[2][0] = (M1.m[0][0]*x)+(M1.m[0][1]*y)+(M1.m[0][2]*z)+(M1.m[0][3]*w);
    mResult.m[2][1] = (M1.m[1][0]*x)+(M1.m[1][1]*y)+(M1.m[1][2]*z)+(M1.m[1][3]*w);
    mResult.m[2][2] = (M1.m[2][0]*x)+(M1.m[2][1]*y)+(M1.m[2][2]*z)+(M1.m[2][3]*w);
    mResult.m[2][3] = (M1.m[3][0]*x)+(M1.m[3][1]*y)+(M1.m[3][2]*z)+(M1.m[3][3]*w);
    x = M2.m[0][3];
    y = M2.m[1][3];
    z = M2.m[2][3];
    w = M2.m[3][3];
    mResult.m[3][0] = (M1.m[0][0]*x)+(M1.m[0][1]*y)+(M1.m[0][2]*z)+(M1.m[0][3]*w);
    mResult.m[3][1] = (M1.m[1][0]*x)+(M1.m[1][1]*y)+(M1.m[1][2]*z)+(M1.m[1][3]*w);
    mResult.m[3][2] = (M1.m[2][0]*x)+(M1.m[2][1]*y)+(M1.m[2][2]*z)+(M1.m[2][3]*w);
    mResult.m[3][3] = (M1.m[3][0]*x)+(M1.m[3][1]*y)+(M1.m[3][2]*z)+(M1.m[3][3]*w);
    return mResult;
#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX Product;
    XMMATRIX Result;
    Product = XMMatrixMultiply(M1, M2);
    Result = XMMatrixTranspose(Product);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixTranspose
(
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX P;
    XMMATRIX MT;

    // Original matrix:
    //
    //     m00m01m02m03
    //     m10m11m12m13
    //     m20m21m22m23
    //     m30m31m32m33

    P.r[0] = XMVectorMergeXY(M.r[0], M.r[2]); // m00m20m01m21
    P.r[1] = XMVectorMergeXY(M.r[1], M.r[3]); // m10m30m11m31
    P.r[2] = XMVectorMergeZW(M.r[0], M.r[2]); // m02m22m03m23
    P.r[3] = XMVectorMergeZW(M.r[1], M.r[3]); // m12m32m13m33

    MT.r[0] = XMVectorMergeXY(P.r[0], P.r[1]); // m00m10m20m30
    MT.r[1] = XMVectorMergeZW(P.r[0], P.r[1]); // m01m11m21m31
    MT.r[2] = XMVectorMergeXY(P.r[2], P.r[3]); // m02m12m22m32
    MT.r[3] = XMVectorMergeZW(P.r[2], P.r[3]); // m03m13m23m33

    return MT;

#elif defined(_XM_SSE_INTRINSICS_)
    // x.x,x.y,y.x,y.y
    XMVECTOR vTemp1 = _mm_shuffle_ps(M.r[0],M.r[1],_MM_SHUFFLE(1,0,1,0));
    // x.z,x.w,y.z,y.w
    XMVECTOR vTemp3 = _mm_shuffle_ps(M.r[0],M.r[1],_MM_SHUFFLE(3,2,3,2));
    // z.x,z.y,w.x,w.y
    XMVECTOR vTemp2 = _mm_shuffle_ps(M.r[2],M.r[3],_MM_SHUFFLE(1,0,1,0));
    // z.z,z.w,w.z,w.w
    XMVECTOR vTemp4 = _mm_shuffle_ps(M.r[2],M.r[3],_MM_SHUFFLE(3,2,3,2));
    XMMATRIX mResult;

    // x.x,y.x,z.x,w.x
    mResult.r[0] = _mm_shuffle_ps(vTemp1, vTemp2,_MM_SHUFFLE(2,0,2,0));
    // x.y,y.y,z.y,w.y
    mResult.r[1] = _mm_shuffle_ps(vTemp1, vTemp2,_MM_SHUFFLE(3,1,3,1));
    // x.z,y.z,z.z,w.z
    mResult.r[2] = _mm_shuffle_ps(vTemp3, vTemp4,_MM_SHUFFLE(2,0,2,0));
    // x.w,y.w,z.w,w.w
    mResult.r[3] = _mm_shuffle_ps(vTemp3, vTemp4,_MM_SHUFFLE(3,1,3,1));
	return mResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return the inverse and the determinant of a 4x4 matrix
XMINLINE XMMATRIX XMMatrixInverse
(
    XMVECTOR* pDeterminant,
    CXMMATRIX  M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX               R;
    XMMATRIX               MT;
    XMVECTOR               D0, D1, D2;
    XMVECTOR               C0, C1, C2, C3, C4, C5, C6, C7;
    XMVECTOR               V0[4], V1[4];
    XMVECTOR               Determinant;
    XMVECTOR               Reciprocal;
    XMMATRIX               Result;
    static CONST XMVECTORU32 SwizzleXXYY = {XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0Y};
    static CONST XMVECTORU32 SwizzleZWZW = {XM_PERMUTE_0Z, XM_PERMUTE_0W, XM_PERMUTE_0Z, XM_PERMUTE_0W};
    static CONST XMVECTORU32 SwizzleYZXY = {XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0X, XM_PERMUTE_0Y};
    static CONST XMVECTORU32 SwizzleZWYZ = {XM_PERMUTE_0Z, XM_PERMUTE_0W, XM_PERMUTE_0Y, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 SwizzleWXWX = {XM_PERMUTE_0W, XM_PERMUTE_0X, XM_PERMUTE_0W, XM_PERMUTE_0X};
    static CONST XMVECTORU32 SwizzleZXYX = {XM_PERMUTE_0Z, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0X};
    static CONST XMVECTORU32 SwizzleYWXZ = {XM_PERMUTE_0Y, XM_PERMUTE_0W, XM_PERMUTE_0X, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 SwizzleWZWY = {XM_PERMUTE_0W, XM_PERMUTE_0Z, XM_PERMUTE_0W, XM_PERMUTE_0Y};
    static CONST XMVECTORU32 Permute0X0Z1X1Z = {XM_PERMUTE_0X, XM_PERMUTE_0Z, XM_PERMUTE_1X, XM_PERMUTE_1Z};
    static CONST XMVECTORU32 Permute0Y0W1Y1W = {XM_PERMUTE_0Y, XM_PERMUTE_0W, XM_PERMUTE_1Y, XM_PERMUTE_1W};
    static CONST XMVECTORU32 Permute1Y0Y0W0X = {XM_PERMUTE_1Y, XM_PERMUTE_0Y, XM_PERMUTE_0W, XM_PERMUTE_0X};
    static CONST XMVECTORU32 Permute0W0X0Y1X = {XM_PERMUTE_0W, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_1X};
    static CONST XMVECTORU32 Permute0Z1Y1X0Z = {XM_PERMUTE_0Z, XM_PERMUTE_1Y, XM_PERMUTE_1X, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 Permute0W1Y0Y0Z = {XM_PERMUTE_0W, XM_PERMUTE_1Y, XM_PERMUTE_0Y, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 Permute0Z0Y1X0X = {XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_1X, XM_PERMUTE_0X};
    static CONST XMVECTORU32 Permute1Y0X0W1X = {XM_PERMUTE_1Y, XM_PERMUTE_0X, XM_PERMUTE_0W, XM_PERMUTE_1X};
    static CONST XMVECTORU32 Permute1W0Y0W0X = {XM_PERMUTE_1W, XM_PERMUTE_0Y, XM_PERMUTE_0W, XM_PERMUTE_0X};
    static CONST XMVECTORU32 Permute0W0X0Y1Z = {XM_PERMUTE_0W, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_1Z};
    static CONST XMVECTORU32 Permute0Z1W1Z0Z = {XM_PERMUTE_0Z, XM_PERMUTE_1W, XM_PERMUTE_1Z, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 Permute0W1W0Y0Z = {XM_PERMUTE_0W, XM_PERMUTE_1W, XM_PERMUTE_0Y, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 Permute0Z0Y1Z0X = {XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_1Z, XM_PERMUTE_0X};
    static CONST XMVECTORU32 Permute1W0X0W1Z = {XM_PERMUTE_1W, XM_PERMUTE_0X, XM_PERMUTE_0W, XM_PERMUTE_1Z};

    XMASSERT(pDeterminant);

    MT = XMMatrixTranspose(M);

    V0[0] = XMVectorPermute(MT.r[2], MT.r[2], SwizzleXXYY.v);
    V1[0] = XMVectorPermute(MT.r[3], MT.r[3], SwizzleZWZW.v);
    V0[1] = XMVectorPermute(MT.r[0], MT.r[0], SwizzleXXYY.v);
    V1[1] = XMVectorPermute(MT.r[1], MT.r[1], SwizzleZWZW.v);
    V0[2] = XMVectorPermute(MT.r[2], MT.r[0], Permute0X0Z1X1Z.v);
    V1[2] = XMVectorPermute(MT.r[3], MT.r[1], Permute0Y0W1Y1W.v);

    D0 = XMVectorMultiply(V0[0], V1[0]);
    D1 = XMVectorMultiply(V0[1], V1[1]);
    D2 = XMVectorMultiply(V0[2], V1[2]);

    V0[0] = XMVectorPermute(MT.r[2], MT.r[2], SwizzleZWZW.v);
    V1[0] = XMVectorPermute(MT.r[3], MT.r[3], SwizzleXXYY.v);
    V0[1] = XMVectorPermute(MT.r[0], MT.r[0], SwizzleZWZW.v);
    V1[1] = XMVectorPermute(MT.r[1], MT.r[1], SwizzleXXYY.v);
    V0[2] = XMVectorPermute(MT.r[2], MT.r[0], Permute0Y0W1Y1W.v);
    V1[2] = XMVectorPermute(MT.r[3], MT.r[1], Permute0X0Z1X1Z.v);

    D0 = XMVectorNegativeMultiplySubtract(V0[0], V1[0], D0);
    D1 = XMVectorNegativeMultiplySubtract(V0[1], V1[1], D1);
    D2 = XMVectorNegativeMultiplySubtract(V0[2], V1[2], D2);

    V0[0] = XMVectorPermute(MT.r[1], MT.r[1], SwizzleYZXY.v);
    V1[0] = XMVectorPermute(D0, D2, Permute1Y0Y0W0X.v);
    V0[1] = XMVectorPermute(MT.r[0], MT.r[0], SwizzleZXYX.v);
    V1[1] = XMVectorPermute(D0, D2, Permute0W1Y0Y0Z.v);
    V0[2] = XMVectorPermute(MT.r[3], MT.r[3], SwizzleYZXY.v);
    V1[2] = XMVectorPermute(D1, D2, Permute1W0Y0W0X.v);
    V0[3] = XMVectorPermute(MT.r[2], MT.r[2], SwizzleZXYX.v);
    V1[3] = XMVectorPermute(D1, D2, Permute0W1W0Y0Z.v);

    C0 = XMVectorMultiply(V0[0], V1[0]);
    C2 = XMVectorMultiply(V0[1], V1[1]);
    C4 = XMVectorMultiply(V0[2], V1[2]);
    C6 = XMVectorMultiply(V0[3], V1[3]);

    V0[0] = XMVectorPermute(MT.r[1], MT.r[1], SwizzleZWYZ.v);
    V1[0] = XMVectorPermute(D0, D2, Permute0W0X0Y1X.v);
    V0[1] = XMVectorPermute(MT.r[0], MT.r[0], SwizzleWZWY.v);
    V1[1] = XMVectorPermute(D0, D2, Permute0Z0Y1X0X.v);
    V0[2] = XMVectorPermute(MT.r[3], MT.r[3], SwizzleZWYZ.v);
    V1[2] = XMVectorPermute(D1, D2, Permute0W0X0Y1Z.v);
    V0[3] = XMVectorPermute(MT.r[2], MT.r[2], SwizzleWZWY.v);
    V1[3] = XMVectorPermute(D1, D2, Permute0Z0Y1Z0X.v);

    C0 = XMVectorNegativeMultiplySubtract(V0[0], V1[0], C0);
    C2 = XMVectorNegativeMultiplySubtract(V0[1], V1[1], C2);
    C4 = XMVectorNegativeMultiplySubtract(V0[2], V1[2], C4);
    C6 = XMVectorNegativeMultiplySubtract(V0[3], V1[3], C6);

    V0[0] = XMVectorPermute(MT.r[1], MT.r[1], SwizzleWXWX.v);
    V1[0] = XMVectorPermute(D0, D2, Permute0Z1Y1X0Z.v);
    V0[1] = XMVectorPermute(MT.r[0], MT.r[0], SwizzleYWXZ.v);
    V1[1] = XMVectorPermute(D0, D2, Permute1Y0X0W1X.v);
    V0[2] = XMVectorPermute(MT.r[3], MT.r[3], SwizzleWXWX.v);
    V1[2] = XMVectorPermute(D1, D2, Permute0Z1W1Z0Z.v);
    V0[3] = XMVectorPermute(MT.r[2], MT.r[2], SwizzleYWXZ.v);
    V1[3] = XMVectorPermute(D1, D2, Permute1W0X0W1Z.v);

    C1 = XMVectorNegativeMultiplySubtract(V0[0], V1[0], C0);
    C0 = XMVectorMultiplyAdd(V0[0], V1[0], C0);
    C3 = XMVectorMultiplyAdd(V0[1], V1[1], C2);
    C2 = XMVectorNegativeMultiplySubtract(V0[1], V1[1], C2);
    C5 = XMVectorNegativeMultiplySubtract(V0[2], V1[2], C4);
    C4 = XMVectorMultiplyAdd(V0[2], V1[2], C4);
    C7 = XMVectorMultiplyAdd(V0[3], V1[3], C6);
    C6 = XMVectorNegativeMultiplySubtract(V0[3], V1[3], C6);

    R.r[0] = XMVectorSelect(C0, C1, g_XMSelect0101.v);
    R.r[1] = XMVectorSelect(C2, C3, g_XMSelect0101.v);
    R.r[2] = XMVectorSelect(C4, C5, g_XMSelect0101.v);
    R.r[3] = XMVectorSelect(C6, C7, g_XMSelect0101.v);

    Determinant = XMVector4Dot(R.r[0], MT.r[0]);

    *pDeterminant = Determinant;

    Reciprocal = XMVectorReciprocal(Determinant);

    Result.r[0] = XMVectorMultiply(R.r[0], Reciprocal);
    Result.r[1] = XMVectorMultiply(R.r[1], Reciprocal);
    Result.r[2] = XMVectorMultiply(R.r[2], Reciprocal);
    Result.r[3] = XMVectorMultiply(R.r[3], Reciprocal);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pDeterminant);
    XMMATRIX MT = XMMatrixTranspose(M);
    XMVECTOR V00 = _mm_shuffle_ps(MT.r[2], MT.r[2],_MM_SHUFFLE(1,1,0,0));
    XMVECTOR V10 = _mm_shuffle_ps(MT.r[3], MT.r[3],_MM_SHUFFLE(3,2,3,2));
    XMVECTOR V01 = _mm_shuffle_ps(MT.r[0], MT.r[0],_MM_SHUFFLE(1,1,0,0));
    XMVECTOR V11 = _mm_shuffle_ps(MT.r[1], MT.r[1],_MM_SHUFFLE(3,2,3,2));
    XMVECTOR V02 = _mm_shuffle_ps(MT.r[2], MT.r[0],_MM_SHUFFLE(2,0,2,0));
    XMVECTOR V12 = _mm_shuffle_ps(MT.r[3], MT.r[1],_MM_SHUFFLE(3,1,3,1));

    XMVECTOR D0 = _mm_mul_ps(V00,V10);
    XMVECTOR D1 = _mm_mul_ps(V01,V11);
    XMVECTOR D2 = _mm_mul_ps(V02,V12);

    V00 = _mm_shuffle_ps(MT.r[2],MT.r[2],_MM_SHUFFLE(3,2,3,2));
    V10 = _mm_shuffle_ps(MT.r[3],MT.r[3],_MM_SHUFFLE(1,1,0,0));
    V01 = _mm_shuffle_ps(MT.r[0],MT.r[0],_MM_SHUFFLE(3,2,3,2));
    V11 = _mm_shuffle_ps(MT.r[1],MT.r[1],_MM_SHUFFLE(1,1,0,0));
    V02 = _mm_shuffle_ps(MT.r[2],MT.r[0],_MM_SHUFFLE(3,1,3,1));
    V12 = _mm_shuffle_ps(MT.r[3],MT.r[1],_MM_SHUFFLE(2,0,2,0));

    V00 = _mm_mul_ps(V00,V10);
    V01 = _mm_mul_ps(V01,V11);
    V02 = _mm_mul_ps(V02,V12);
    D0 = _mm_sub_ps(D0,V00);
    D1 = _mm_sub_ps(D1,V01);
    D2 = _mm_sub_ps(D2,V02);
    // V11 = D0Y,D0W,D2Y,D2Y
    V11 = _mm_shuffle_ps(D0,D2,_MM_SHUFFLE(1,1,3,1));
    V00 = _mm_shuffle_ps(MT.r[1], MT.r[1],_MM_SHUFFLE(1,0,2,1));
    V10 = _mm_shuffle_ps(V11,D0,_MM_SHUFFLE(0,3,0,2));
    V01 = _mm_shuffle_ps(MT.r[0], MT.r[0],_MM_SHUFFLE(0,1,0,2));
    V11 = _mm_shuffle_ps(V11,D0,_MM_SHUFFLE(2,1,2,1));
    // V13 = D1Y,D1W,D2W,D2W
    XMVECTOR V13 = _mm_shuffle_ps(D1,D2,_MM_SHUFFLE(3,3,3,1));
    V02 = _mm_shuffle_ps(MT.r[3], MT.r[3],_MM_SHUFFLE(1,0,2,1));
    V12 = _mm_shuffle_ps(V13,D1,_MM_SHUFFLE(0,3,0,2));
    XMVECTOR V03 = _mm_shuffle_ps(MT.r[2], MT.r[2],_MM_SHUFFLE(0,1,0,2));
    V13 = _mm_shuffle_ps(V13,D1,_MM_SHUFFLE(2,1,2,1));

    XMVECTOR C0 = _mm_mul_ps(V00,V10);
    XMVECTOR C2 = _mm_mul_ps(V01,V11);
    XMVECTOR C4 = _mm_mul_ps(V02,V12);
    XMVECTOR C6 = _mm_mul_ps(V03,V13);

    // V11 = D0X,D0Y,D2X,D2X
    V11 = _mm_shuffle_ps(D0,D2,_MM_SHUFFLE(0,0,1,0));
    V00 = _mm_shuffle_ps(MT.r[1], MT.r[1],_MM_SHUFFLE(2,1,3,2));
    V10 = _mm_shuffle_ps(D0,V11,_MM_SHUFFLE(2,1,0,3));
    V01 = _mm_shuffle_ps(MT.r[0], MT.r[0],_MM_SHUFFLE(1,3,2,3));
    V11 = _mm_shuffle_ps(D0,V11,_MM_SHUFFLE(0,2,1,2));
    // V13 = D1X,D1Y,D2Z,D2Z
    V13 = _mm_shuffle_ps(D1,D2,_MM_SHUFFLE(2,2,1,0));
    V02 = _mm_shuffle_ps(MT.r[3], MT.r[3],_MM_SHUFFLE(2,1,3,2));
    V12 = _mm_shuffle_ps(D1,V13,_MM_SHUFFLE(2,1,0,3));
    V03 = _mm_shuffle_ps(MT.r[2], MT.r[2],_MM_SHUFFLE(1,3,2,3));
    V13 = _mm_shuffle_ps(D1,V13,_MM_SHUFFLE(0,2,1,2));

    V00 = _mm_mul_ps(V00,V10);
    V01 = _mm_mul_ps(V01,V11);
    V02 = _mm_mul_ps(V02,V12);
    V03 = _mm_mul_ps(V03,V13);
    C0 = _mm_sub_ps(C0,V00);
    C2 = _mm_sub_ps(C2,V01);
    C4 = _mm_sub_ps(C4,V02);
    C6 = _mm_sub_ps(C6,V03);

    V00 = _mm_shuffle_ps(MT.r[1],MT.r[1],_MM_SHUFFLE(0,3,0,3));
    // V10 = D0Z,D0Z,D2X,D2Y
    V10 = _mm_shuffle_ps(D0,D2,_MM_SHUFFLE(1,0,2,2));
    V10 = _mm_shuffle_ps(V10,V10,_MM_SHUFFLE(0,2,3,0));
    V01 = _mm_shuffle_ps(MT.r[0],MT.r[0],_MM_SHUFFLE(2,0,3,1));
    // V11 = D0X,D0W,D2X,D2Y
    V11 = _mm_shuffle_ps(D0,D2,_MM_SHUFFLE(1,0,3,0));
    V11 = _mm_shuffle_ps(V11,V11,_MM_SHUFFLE(2,1,0,3));
    V02 = _mm_shuffle_ps(MT.r[3],MT.r[3],_MM_SHUFFLE(0,3,0,3));
    // V12 = D1Z,D1Z,D2Z,D2W
    V12 = _mm_shuffle_ps(D1,D2,_MM_SHUFFLE(3,2,2,2));
    V12 = _mm_shuffle_ps(V12,V12,_MM_SHUFFLE(0,2,3,0));
    V03 = _mm_shuffle_ps(MT.r[2],MT.r[2],_MM_SHUFFLE(2,0,3,1));
    // V13 = D1X,D1W,D2Z,D2W
    V13 = _mm_shuffle_ps(D1,D2,_MM_SHUFFLE(3,2,3,0));
    V13 = _mm_shuffle_ps(V13,V13,_MM_SHUFFLE(2,1,0,3));

    V00 = _mm_mul_ps(V00,V10);
    V01 = _mm_mul_ps(V01,V11);
    V02 = _mm_mul_ps(V02,V12);
    V03 = _mm_mul_ps(V03,V13);
    XMVECTOR C1 = _mm_sub_ps(C0,V00);
    C0 = _mm_add_ps(C0,V00);
    XMVECTOR C3 = _mm_add_ps(C2,V01);
    C2 = _mm_sub_ps(C2,V01);
    XMVECTOR C5 = _mm_sub_ps(C4,V02);
    C4 = _mm_add_ps(C4,V02);
    XMVECTOR C7 = _mm_add_ps(C6,V03);
    C6 = _mm_sub_ps(C6,V03);

    C0 = _mm_shuffle_ps(C0,C1,_MM_SHUFFLE(3,1,2,0));
    C2 = _mm_shuffle_ps(C2,C3,_MM_SHUFFLE(3,1,2,0));
    C4 = _mm_shuffle_ps(C4,C5,_MM_SHUFFLE(3,1,2,0));
    C6 = _mm_shuffle_ps(C6,C7,_MM_SHUFFLE(3,1,2,0));
    C0 = _mm_shuffle_ps(C0,C0,_MM_SHUFFLE(3,1,2,0));
    C2 = _mm_shuffle_ps(C2,C2,_MM_SHUFFLE(3,1,2,0));
    C4 = _mm_shuffle_ps(C4,C4,_MM_SHUFFLE(3,1,2,0));
    C6 = _mm_shuffle_ps(C6,C6,_MM_SHUFFLE(3,1,2,0));
    // Get the determinate
    XMVECTOR vTemp = XMVector4Dot(C0,MT.r[0]);
    *pDeterminant = vTemp;
    vTemp = _mm_div_ps(g_XMOne,vTemp);
    XMMATRIX mResult;
    mResult.r[0] = _mm_mul_ps(C0,vTemp);
    mResult.r[1] = _mm_mul_ps(C2,vTemp);
    mResult.r[2] = _mm_mul_ps(C4,vTemp);
    mResult.r[3] = _mm_mul_ps(C6,vTemp);
    return mResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMMatrixDeterminant
(
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR                V0, V1, V2, V3, V4, V5;
    XMVECTOR                P0, P1, P2, R, S;
    XMVECTOR                Result;
    static CONST XMVECTORU32 SwizzleYXXX = {XM_PERMUTE_0Y, XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0X};
    static CONST XMVECTORU32 SwizzleZZYY = {XM_PERMUTE_0Z, XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_0Y};
    static CONST XMVECTORU32 SwizzleWWWZ = {XM_PERMUTE_0W, XM_PERMUTE_0W, XM_PERMUTE_0W, XM_PERMUTE_0Z};
    static CONST XMVECTOR   Sign = {1.0f, -1.0f, 1.0f, -1.0f};

    V0 = XMVectorPermute(M.r[2], M.r[2], SwizzleYXXX.v);
    V1 = XMVectorPermute(M.r[3], M.r[3], SwizzleZZYY.v);
    V2 = XMVectorPermute(M.r[2], M.r[2], SwizzleYXXX.v);
    V3 = XMVectorPermute(M.r[3], M.r[3], SwizzleWWWZ.v);
    V4 = XMVectorPermute(M.r[2], M.r[2], SwizzleZZYY.v);
    V5 = XMVectorPermute(M.r[3], M.r[3], SwizzleWWWZ.v);

    P0 = XMVectorMultiply(V0, V1);
    P1 = XMVectorMultiply(V2, V3);
    P2 = XMVectorMultiply(V4, V5);

    V0 = XMVectorPermute(M.r[2], M.r[2], SwizzleZZYY.v);
    V1 = XMVectorPermute(M.r[3], M.r[3], SwizzleYXXX.v);
    V2 = XMVectorPermute(M.r[2], M.r[2], SwizzleWWWZ.v);
    V3 = XMVectorPermute(M.r[3], M.r[3], SwizzleYXXX.v);
    V4 = XMVectorPermute(M.r[2], M.r[2], SwizzleWWWZ.v);
    V5 = XMVectorPermute(M.r[3], M.r[3], SwizzleZZYY.v);

    P0 = XMVectorNegativeMultiplySubtract(V0, V1, P0);
    P1 = XMVectorNegativeMultiplySubtract(V2, V3, P1);
    P2 = XMVectorNegativeMultiplySubtract(V4, V5, P2);

    V0 = XMVectorPermute(M.r[1], M.r[1], SwizzleWWWZ.v);
    V1 = XMVectorPermute(M.r[1], M.r[1], SwizzleZZYY.v);
    V2 = XMVectorPermute(M.r[1], M.r[1], SwizzleYXXX.v);

    S = XMVectorMultiply(M.r[0], Sign);
    R = XMVectorMultiply(V0, P0);
    R = XMVectorNegativeMultiplySubtract(V1, P1, R);
    R = XMVectorMultiplyAdd(V2, P2, R);

    Result = XMVector4Dot(S, R);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR                V0, V1, V2, V3, V4, V5;
    XMVECTOR                P0, P1, P2, R, S;
    XMVECTOR                Result;
    static CONST XMVECTORU32 SwizzleYXXX = {XM_PERMUTE_0Y, XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0X};
    static CONST XMVECTORU32 SwizzleZZYY = {XM_PERMUTE_0Z, XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_0Y};
    static CONST XMVECTORU32 SwizzleWWWZ = {XM_PERMUTE_0W, XM_PERMUTE_0W, XM_PERMUTE_0W, XM_PERMUTE_0Z};
    static CONST XMVECTORF32 Sign = {1.0f, -1.0f, 1.0f, -1.0f};

    V0 = XMVectorPermute(M.r[2], M.r[2], SwizzleYXXX);
    V1 = XMVectorPermute(M.r[3], M.r[3], SwizzleZZYY);
    V2 = XMVectorPermute(M.r[2], M.r[2], SwizzleYXXX);
    V3 = XMVectorPermute(M.r[3], M.r[3], SwizzleWWWZ);
    V4 = XMVectorPermute(M.r[2], M.r[2], SwizzleZZYY);
    V5 = XMVectorPermute(M.r[3], M.r[3], SwizzleWWWZ);

    P0 = _mm_mul_ps(V0, V1);
    P1 = _mm_mul_ps(V2, V3);
    P2 = _mm_mul_ps(V4, V5);

    V0 = XMVectorPermute(M.r[2], M.r[2], SwizzleZZYY);
    V1 = XMVectorPermute(M.r[3], M.r[3], SwizzleYXXX);
    V2 = XMVectorPermute(M.r[2], M.r[2], SwizzleWWWZ);
    V3 = XMVectorPermute(M.r[3], M.r[3], SwizzleYXXX);
    V4 = XMVectorPermute(M.r[2], M.r[2], SwizzleWWWZ);
    V5 = XMVectorPermute(M.r[3], M.r[3], SwizzleZZYY);

    P0 = XMVectorNegativeMultiplySubtract(V0, V1, P0);
    P1 = XMVectorNegativeMultiplySubtract(V2, V3, P1);
    P2 = XMVectorNegativeMultiplySubtract(V4, V5, P2);

    V0 = XMVectorPermute(M.r[1], M.r[1], SwizzleWWWZ);
    V1 = XMVectorPermute(M.r[1], M.r[1], SwizzleZZYY);
    V2 = XMVectorPermute(M.r[1], M.r[1], SwizzleYXXX);

    S = _mm_mul_ps(M.r[0], Sign);
    R = _mm_mul_ps(V0, P0);
    R = XMVectorNegativeMultiplySubtract(V1, P1, R);
    R = XMVectorMultiplyAdd(V2, P2, R);

    Result = XMVector4Dot(S, R);

    return Result;

#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

#define XMRANKDECOMPOSE(a, b, c, x, y, z)      \
    if((x) < (y))                   \
    {                               \
        if((y) < (z))               \
        {                           \
            (a) = 2;                \
            (b) = 1;                \
            (c) = 0;                \
        }                           \
        else                        \
        {                           \
            (a) = 1;                \
                                    \
            if((x) < (z))           \
            {                       \
                (b) = 2;            \
                (c) = 0;            \
            }                       \
            else                    \
            {                       \
                (b) = 0;            \
                (c) = 2;            \
            }                       \
        }                           \
    }                               \
    else                            \
    {                               \
        if((x) < (z))               \
        {                           \
            (a) = 2;                \
            (b) = 0;                \
            (c) = 1;                \
        }                           \
        else                        \
        {                           \
            (a) = 0;                \
                                    \
            if((y) < (z))           \
            {                       \
                (b) = 2;            \
                (c) = 1;            \
            }                       \
            else                    \
            {                       \
                (b) = 1;            \
                (c) = 2;            \
            }                       \
        }                           \
    }

#define XM_DECOMP_EPSILON 0.0001f

XMINLINE BOOL XMMatrixDecompose( XMVECTOR *outScale, XMVECTOR *outRotQuat, XMVECTOR *outTrans, CXMMATRIX M )
{
	FLOAT fDet;
	FLOAT *pfScales;
	XMVECTOR *ppvBasis[3];
	XMMATRIX matTemp;
	UINT a, b, c;
	static const XMVECTOR *pvCanonicalBasis[3] = {
	    &g_XMIdentityR0.v,
	    &g_XMIdentityR1.v,
	    &g_XMIdentityR2.v
    };

    // Get the translation
    outTrans[0] = M.r[3];

	ppvBasis[0] = &matTemp.r[0];
	ppvBasis[1] = &matTemp.r[1];
	ppvBasis[2] = &matTemp.r[2];

	matTemp.r[0] = M.r[0];
	matTemp.r[1] = M.r[1];
	matTemp.r[2] = M.r[2];
    matTemp.r[3] = g_XMIdentityR3.v;

	pfScales = (FLOAT *)outScale;

	XMVectorGetXPtr(&pfScales[0],XMVector3Length(ppvBasis[0][0]));
	XMVectorGetXPtr(&pfScales[1],XMVector3Length(ppvBasis[1][0]));
	XMVectorGetXPtr(&pfScales[2],XMVector3Length(ppvBasis[2][0]));

	XMRANKDECOMPOSE(a, b, c, pfScales[0], pfScales[1], pfScales[2])

	if(pfScales[a] < XM_DECOMP_EPSILON)
	{
		ppvBasis[a][0] = pvCanonicalBasis[a][0];
	}
    ppvBasis[a][0] = XMVector3Normalize(ppvBasis[a][0]);

	if(pfScales[b] < XM_DECOMP_EPSILON)
	{
		UINT aa, bb, cc;
		FLOAT fAbsX, fAbsY, fAbsZ;

		fAbsX = fabsf(XMVectorGetX(ppvBasis[a][0]));
		fAbsY = fabsf(XMVectorGetY(ppvBasis[a][0]));
		fAbsZ = fabsf(XMVectorGetZ(ppvBasis[a][0]));

		XMRANKDECOMPOSE(aa, bb, cc, fAbsX, fAbsY, fAbsZ)

		ppvBasis[b][0] = XMVector3Cross(ppvBasis[a][0],pvCanonicalBasis[cc][0]);
	}

	ppvBasis[b][0] = XMVector3Normalize(ppvBasis[b][0]);

	if(pfScales[c] < XM_DECOMP_EPSILON)
	{
		ppvBasis[c][0] = XMVector3Cross(ppvBasis[a][0],ppvBasis[b][0]);
	}

	ppvBasis[c][0] = XMVector3Normalize(ppvBasis[c][0]);

	fDet = XMVectorGetX(XMMatrixDeterminant(matTemp));

	// use Kramer's rule to check for handedness of coordinate system
	if(fDet < 0.0f)
	{
		// switch coordinate system by negating the scale and inverting the basis vector on the x-axis
		pfScales[a] = -pfScales[a];
		ppvBasis[a][0] = XMVectorNegate(ppvBasis[a][0]);

		fDet = -fDet;
	}

	fDet -= 1.0f;
	fDet *= fDet;

	if(XM_DECOMP_EPSILON < fDet)
	{
//		Non-SRT matrix encountered
		return FALSE;
	}

	// generate the quaternion from the matrix
	outRotQuat[0] = XMQuaternionRotationMatrix(matTemp);
    return TRUE;
}

//------------------------------------------------------------------------------
// Transformation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixIdentity()
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;
    M.r[0] = g_XMIdentityR0.v;
    M.r[1] = g_XMIdentityR1.v;
    M.r[2] = g_XMIdentityR2.v;
    M.r[3] = g_XMIdentityR3.v;
    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    M.r[0] = g_XMIdentityR0;
    M.r[1] = g_XMIdentityR1;
    M.r[2] = g_XMIdentityR2;
    M.r[3] = g_XMIdentityR3;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixSet
(
    FLOAT m00, FLOAT m01, FLOAT m02, FLOAT m03,
    FLOAT m10, FLOAT m11, FLOAT m12, FLOAT m13,
    FLOAT m20, FLOAT m21, FLOAT m22, FLOAT m23,
    FLOAT m30, FLOAT m31, FLOAT m32, FLOAT m33
)
{
    XMMATRIX M;

    M.r[0] = XMVectorSet(m00, m01, m02, m03);
    M.r[1] = XMVectorSet(m10, m11, m12, m13);
    M.r[2] = XMVectorSet(m20, m21, m22, m23);
    M.r[3] = XMVectorSet(m30, m31, m32, m33);

    return M;
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixTranslation
(
    FLOAT OffsetX,
    FLOAT OffsetY,
    FLOAT OffsetZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;

    M.m[0][0] = 1.0f;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = 1.0f;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 1.0f;
    M.m[2][3] = 0.0f;

    M.m[3][0] = OffsetX;
    M.m[3][1] = OffsetY;
    M.m[3][2] = OffsetZ;
    M.m[3][3] = 1.0f;
    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    M.r[0] = g_XMIdentityR0;
    M.r[1] = g_XMIdentityR1;
    M.r[2] = g_XMIdentityR2;
    M.r[3] = _mm_set_ps(1.0f,OffsetZ,OffsetY,OffsetX);
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixTranslationFromVector
(
    FXMVECTOR Offset
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;
    M.m[0][0] = 1.0f;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = 1.0f;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 1.0f;
    M.m[2][3] = 0.0f;

    M.m[3][0] = Offset.vector4_f32[0];
    M.m[3][1] = Offset.vector4_f32[1];
    M.m[3][2] = Offset.vector4_f32[2];
    M.m[3][3] = 1.0f;
    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_and_ps(Offset,g_XMMask3);
    vTemp = _mm_or_ps(vTemp,g_XMIdentityR3);
    XMMATRIX M;
    M.r[0] = g_XMIdentityR0;
    M.r[1] = g_XMIdentityR1;
    M.r[2] = g_XMIdentityR2;
    M.r[3] = vTemp;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixScaling
(
    FLOAT ScaleX,
    FLOAT ScaleY,
    FLOAT ScaleZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;

    M.r[0] = XMVectorSet(ScaleX, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, ScaleY, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(0.0f, 0.0f, ScaleZ, 0.0f);

    M.r[3] = g_XMIdentityR3.v;

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    M.r[0] = _mm_set_ps( 0, 0, 0, ScaleX );
    M.r[1] = _mm_set_ps( 0, 0, ScaleY, 0 );
    M.r[2] = _mm_set_ps( 0, ScaleZ, 0, 0 );
    M.r[3] = g_XMIdentityR3;
    return M;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixScalingFromVector
(
    FXMVECTOR Scale
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMMATRIX M;
    M.m[0][0] = Scale.vector4_f32[0];
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = Scale.vector4_f32[1];
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = Scale.vector4_f32[2];
    M.m[2][3] = 0.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;
    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    M.r[0] = _mm_and_ps(Scale,g_XMMaskX);
    M.r[1] = _mm_and_ps(Scale,g_XMMaskY);
    M.r[2] = _mm_and_ps(Scale,g_XMMaskZ);
    M.r[3] = g_XMIdentityR3;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixRotationX
(
    FLOAT Angle
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMMATRIX M;

    FLOAT fSinAngle = sinf(Angle);
    FLOAT fCosAngle = cosf(Angle);

    M.m[0][0] = 1.0f;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = fCosAngle;
    M.m[1][2] = fSinAngle;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = -fSinAngle;
    M.m[2][2] = fCosAngle;
    M.m[2][3] = 0.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;
    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    FLOAT SinAngle = sinf(Angle);
    FLOAT CosAngle = cosf(Angle);

    XMVECTOR vSin = _mm_set_ss(SinAngle);
    XMVECTOR vCos = _mm_set_ss(CosAngle);
    // x = 0,y = cos,z = sin, w = 0
    vCos = _mm_shuffle_ps(vCos,vSin,_MM_SHUFFLE(3,0,0,3));
    XMMATRIX M;
    M.r[0] = g_XMIdentityR0;
    M.r[1] = vCos;
    // x = 0,y = sin,z = cos, w = 0
    vCos = _mm_shuffle_ps(vCos,vCos,_MM_SHUFFLE(3,1,2,0));
    // x = 0,y = -sin,z = cos, w = 0
    vCos = _mm_mul_ps(vCos,g_XMNegateY);
    M.r[2] = vCos;
    M.r[3] = g_XMIdentityR3;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixRotationY
(
    FLOAT Angle
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMMATRIX M;

    FLOAT fSinAngle = sinf(Angle);
    FLOAT fCosAngle = cosf(Angle);

    M.m[0][0] = fCosAngle;
    M.m[0][1] = 0.0f;
    M.m[0][2] = -fSinAngle;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = 1.0f;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = fSinAngle;
    M.m[2][1] = 0.0f;
    M.m[2][2] = fCosAngle;
    M.m[2][3] = 0.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;
    return M;
#elif defined(_XM_SSE_INTRINSICS_)
    FLOAT SinAngle = sinf(Angle);
    FLOAT CosAngle = cosf(Angle);

    XMVECTOR vSin = _mm_set_ss(SinAngle);
    XMVECTOR vCos = _mm_set_ss(CosAngle);
    // x = sin,y = 0,z = cos, w = 0
    vSin = _mm_shuffle_ps(vSin,vCos,_MM_SHUFFLE(3,0,3,0));
    XMMATRIX M;
    M.r[2] = vSin;
    M.r[1] = g_XMIdentityR1;
    // x = cos,y = 0,z = sin, w = 0
    vSin = _mm_shuffle_ps(vSin,vSin,_MM_SHUFFLE(3,0,1,2));
    // x = cos,y = 0,z = -sin, w = 0
    vSin = _mm_mul_ps(vSin,g_XMNegateZ);
    M.r[0] = vSin;
    M.r[3] = g_XMIdentityR3;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixRotationZ
(
    FLOAT Angle
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMMATRIX M;

    FLOAT fSinAngle = sinf(Angle);
    FLOAT fCosAngle = cosf(Angle);

    M.m[0][0] = fCosAngle;
    M.m[0][1] = fSinAngle;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = -fSinAngle;
    M.m[1][1] = fCosAngle;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 1.0f;
    M.m[2][3] = 0.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = 0.0f;
    M.m[3][3] = 1.0f;
    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    FLOAT SinAngle = sinf(Angle);
    FLOAT CosAngle = cosf(Angle);

    XMVECTOR vSin = _mm_set_ss(SinAngle);
    XMVECTOR vCos = _mm_set_ss(CosAngle);
    // x = cos,y = sin,z = 0, w = 0
    vCos = _mm_unpacklo_ps(vCos,vSin);
    XMMATRIX M;
    M.r[0] = vCos;
    // x = sin,y = cos,z = 0, w = 0
    vCos = _mm_shuffle_ps(vCos,vCos,_MM_SHUFFLE(3,2,0,1));
    // x = cos,y = -sin,z = 0, w = 0
    vCos = _mm_mul_ps(vCos,g_XMNegateX);
    M.r[1] = vCos;
    M.r[2] = g_XMIdentityR2;
    M.r[3] = g_XMIdentityR3;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixRotationRollPitchYaw
(
    FLOAT Pitch,
    FLOAT Yaw,
    FLOAT Roll
)
{
    XMVECTOR Angles;
    XMMATRIX M;

    Angles = XMVectorSet(Pitch, Yaw, Roll, 0.0f);
    M = XMMatrixRotationRollPitchYawFromVector(Angles);

    return M;
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixRotationRollPitchYawFromVector
(
    FXMVECTOR Angles // <Pitch, Yaw, Roll, undefined>
)
{
    XMVECTOR Q;
    XMMATRIX M;

    Q = XMQuaternionRotationRollPitchYawFromVector(Angles);
    M = XMMatrixRotationQuaternion(Q);

    return M;
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixRotationNormal
(
    FXMVECTOR NormalAxis,
    FLOAT    Angle
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR               A;
    XMVECTOR               N0, N1;
    XMVECTOR               V0, V1, V2;
    XMVECTOR               R0, R1, R2;
    XMVECTOR               C0, C1, C2;
    XMMATRIX               M;
    static CONST XMVECTORU32 SwizzleYZXW = {XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0X, XM_PERMUTE_0W};
    static CONST XMVECTORU32 SwizzleZXYW = {XM_PERMUTE_0Z, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute0Z1Y1Z0X = {XM_PERMUTE_0Z, XM_PERMUTE_1Y, XM_PERMUTE_1Z, XM_PERMUTE_0X};
    static CONST XMVECTORU32 Permute0Y1X0Y1X = {XM_PERMUTE_0Y, XM_PERMUTE_1X, XM_PERMUTE_0Y, XM_PERMUTE_1X};
    static CONST XMVECTORU32 Permute0X1X1Y0W = {XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute1Z0Y1W0W = {XM_PERMUTE_1Z, XM_PERMUTE_0Y, XM_PERMUTE_1W, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute1X1Y0Z0W = {XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0Z, XM_PERMUTE_0W};

    FLOAT fSinAngle = sinf(Angle);
    FLOAT fCosAngle = cosf(Angle);

    A = XMVectorSet(fSinAngle, fCosAngle, 1.0f - fCosAngle, 0.0f);

    C2 = XMVectorSplatZ(A);
    C1 = XMVectorSplatY(A);
    C0 = XMVectorSplatX(A);

    N0 = XMVectorPermute(NormalAxis, NormalAxis, SwizzleYZXW.v);
    N1 = XMVectorPermute(NormalAxis, NormalAxis, SwizzleZXYW.v);

    V0 = XMVectorMultiply(C2, N0);
    V0 = XMVectorMultiply(V0, N1);

    R0 = XMVectorMultiply(C2, NormalAxis);
    R0 = XMVectorMultiplyAdd(R0, NormalAxis, C1);

    R1 = XMVectorMultiplyAdd(C0, NormalAxis, V0);
    R2 = XMVectorNegativeMultiplySubtract(C0, NormalAxis, V0);

    V0 = XMVectorSelect(A, R0, g_XMSelect1110.v);
    V1 = XMVectorPermute(R1, R2, Permute0Z1Y1Z0X.v);
    V2 = XMVectorPermute(R1, R2, Permute0Y1X0Y1X.v);

    M.r[0] = XMVectorPermute(V0, V1, Permute0X1X1Y0W.v);
    M.r[1] = XMVectorPermute(V0, V1, Permute1Z0Y1W0W.v);
    M.r[2] = XMVectorPermute(V0, V2, Permute1X1Y0Z0W.v);
    M.r[3] = g_XMIdentityR3.v;

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR               N0, N1;
    XMVECTOR               V0, V1, V2;
    XMVECTOR               R0, R1, R2;
    XMVECTOR               C0, C1, C2;
    XMMATRIX               M;

    FLOAT fSinAngle = sinf(Angle);
    FLOAT fCosAngle = cosf(Angle);

    C2 = _mm_set_ps1(1.0f - fCosAngle);
    C1 = _mm_set_ps1(fCosAngle);
    C0 = _mm_set_ps1(fSinAngle);

    N0 = _mm_shuffle_ps(NormalAxis,NormalAxis,_MM_SHUFFLE(3,0,2,1));
//    N0 = XMVectorPermute(NormalAxis, NormalAxis, SwizzleYZXW);
    N1 = _mm_shuffle_ps(NormalAxis,NormalAxis,_MM_SHUFFLE(3,1,0,2));
//    N1 = XMVectorPermute(NormalAxis, NormalAxis, SwizzleZXYW);

    V0 = _mm_mul_ps(C2, N0);
    V0 = _mm_mul_ps(V0, N1);

    R0 = _mm_mul_ps(C2, NormalAxis);
    R0 = _mm_mul_ps(R0, NormalAxis);
    R0 = _mm_add_ps(R0, C1);

    R1 = _mm_mul_ps(C0, NormalAxis);
    R1 = _mm_add_ps(R1, V0);
    R2 = _mm_mul_ps(C0, NormalAxis);
    R2 = _mm_sub_ps(V0,R2);

    V0 = _mm_and_ps(R0,g_XMMask3);
//    V0 = XMVectorSelect(A, R0, g_XMSelect1110);
    V1 = _mm_shuffle_ps(R1,R2,_MM_SHUFFLE(2,1,2,0));
    V1 = _mm_shuffle_ps(V1,V1,_MM_SHUFFLE(0,3,2,1));
//    V1 = XMVectorPermute(R1, R2, Permute0Z1Y1Z0X);
    V2 = _mm_shuffle_ps(R1,R2,_MM_SHUFFLE(0,0,1,1));
    V2 = _mm_shuffle_ps(V2,V2,_MM_SHUFFLE(2,0,2,0));
//    V2 = XMVectorPermute(R1, R2, Permute0Y1X0Y1X);

    R2 = _mm_shuffle_ps(V0,V1,_MM_SHUFFLE(1,0,3,0));
    R2 = _mm_shuffle_ps(R2,R2,_MM_SHUFFLE(1,3,2,0));
    M.r[0] = R2;
//    M.r[0] = XMVectorPermute(V0, V1, Permute0X1X1Y0W);
    R2 = _mm_shuffle_ps(V0,V1,_MM_SHUFFLE(3,2,3,1));
    R2 = _mm_shuffle_ps(R2,R2,_MM_SHUFFLE(1,3,0,2));
    M.r[1] = R2;
//    M.r[1] = XMVectorPermute(V0, V1, Permute1Z0Y1W0W);
    V2 = _mm_shuffle_ps(V2,V0,_MM_SHUFFLE(3,2,1,0));
//    R2 = _mm_shuffle_ps(R2,R2,_MM_SHUFFLE(3,2,1,0));
    M.r[2] = V2;
//    M.r[2] = XMVectorPermute(V0, V2, Permute1X1Y0Z0W);
    M.r[3] = g_XMIdentityR3;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixRotationAxis
(
    FXMVECTOR Axis,
    FLOAT    Angle
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Normal;
    XMMATRIX M;

    XMASSERT(!XMVector3Equal(Axis, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(Axis));

    Normal = XMVector3Normalize(Axis);
    M = XMMatrixRotationNormal(Normal, Angle);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMVector3Equal(Axis, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(Axis));
    XMVECTOR Normal = XMVector3Normalize(Axis);
    XMMATRIX M = XMMatrixRotationNormal(Normal, Angle);
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixRotationQuaternion
(
    FXMVECTOR Quaternion
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX               M;
    XMVECTOR               Q0, Q1;
    XMVECTOR               V0, V1, V2;
    XMVECTOR               R0, R1, R2;
    static CONST XMVECTOR  Constant1110 = {1.0f, 1.0f, 1.0f, 0.0f};
    static CONST XMVECTORU32 SwizzleXXYW = {XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0W};
    static CONST XMVECTORU32 SwizzleZYZW = {XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0W};
    static CONST XMVECTORU32 SwizzleYZXW = {XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0X, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute0Y0X0X1W = {XM_PERMUTE_0Y, XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_1W};
    static CONST XMVECTORU32 Permute0Z0Z0Y1W = {XM_PERMUTE_0Z, XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_1W};
    static CONST XMVECTORU32 Permute0Y1X1Y0Z = {XM_PERMUTE_0Y, XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 Permute0X1Z0X1Z = {XM_PERMUTE_0X, XM_PERMUTE_1Z, XM_PERMUTE_0X, XM_PERMUTE_1Z};
    static CONST XMVECTORU32 Permute0X1X1Y0W = {XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute1Z0Y1W0W = {XM_PERMUTE_1Z, XM_PERMUTE_0Y, XM_PERMUTE_1W, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute1X1Y0Z0W = {XM_PERMUTE_1X, XM_PERMUTE_1Y, XM_PERMUTE_0Z, XM_PERMUTE_0W};

    Q0 = XMVectorAdd(Quaternion, Quaternion);
    Q1 = XMVectorMultiply(Quaternion, Q0);

    V0 = XMVectorPermute(Q1, Constant1110, Permute0Y0X0X1W.v);
    V1 = XMVectorPermute(Q1, Constant1110, Permute0Z0Z0Y1W.v);
    R0 = XMVectorSubtract(Constant1110, V0);
    R0 = XMVectorSubtract(R0, V1);

    V0 = XMVectorPermute(Quaternion, Quaternion, SwizzleXXYW.v);
    V1 = XMVectorPermute(Q0, Q0, SwizzleZYZW.v);
    V0 = XMVectorMultiply(V0, V1);

    V1 = XMVectorSplatW(Quaternion);
    V2 = XMVectorPermute(Q0, Q0, SwizzleYZXW.v);
    V1 = XMVectorMultiply(V1, V2);

    R1 = XMVectorAdd(V0, V1);
    R2 = XMVectorSubtract(V0, V1);

    V0 = XMVectorPermute(R1, R2, Permute0Y1X1Y0Z.v);
    V1 = XMVectorPermute(R1, R2, Permute0X1Z0X1Z.v);

    M.r[0] = XMVectorPermute(R0, V0, Permute0X1X1Y0W.v);
    M.r[1] = XMVectorPermute(R0, V0, Permute1Z0Y1W0W.v);
    M.r[2] = XMVectorPermute(R0, V1, Permute1X1Y0Z0W.v);
    M.r[3] = g_XMIdentityR3.v;

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
	XMMATRIX M;
    XMVECTOR               Q0, Q1;
    XMVECTOR               V0, V1, V2;
    XMVECTOR               R0, R1, R2;
    static CONST XMVECTORF32  Constant1110 = {1.0f, 1.0f, 1.0f, 0.0f};

    Q0 = _mm_add_ps(Quaternion,Quaternion);
    Q1 = _mm_mul_ps(Quaternion,Q0);

    V0 = _mm_shuffle_ps(Q1,Q1,_MM_SHUFFLE(3,0,0,1));
    V0 = _mm_and_ps(V0,g_XMMask3);
//    V0 = XMVectorPermute(Q1, Constant1110,Permute0Y0X0X1W);
    V1 = _mm_shuffle_ps(Q1,Q1,_MM_SHUFFLE(3,1,2,2));
    V1 = _mm_and_ps(V1,g_XMMask3);
//    V1 = XMVectorPermute(Q1, Constant1110,Permute0Z0Z0Y1W);
    R0 = _mm_sub_ps(Constant1110,V0);
    R0 = _mm_sub_ps(R0, V1);

    V0 = _mm_shuffle_ps(Quaternion,Quaternion,_MM_SHUFFLE(3,1,0,0));
//    V0 = XMVectorPermute(Quaternion, Quaternion,SwizzleXXYW);
    V1 = _mm_shuffle_ps(Q0,Q0,_MM_SHUFFLE(3,2,1,2));
//    V1 = XMVectorPermute(Q0, Q0,SwizzleZYZW);
    V0 = _mm_mul_ps(V0, V1);

    V1 = _mm_shuffle_ps(Quaternion,Quaternion,_MM_SHUFFLE(3,3,3,3));
//    V1 = XMVectorSplatW(Quaternion);
    V2 = _mm_shuffle_ps(Q0,Q0,_MM_SHUFFLE(3,0,2,1));
//    V2 = XMVectorPermute(Q0, Q0,SwizzleYZXW);
    V1 = _mm_mul_ps(V1, V2);

    R1 = _mm_add_ps(V0, V1);
    R2 = _mm_sub_ps(V0, V1);

    V0 = _mm_shuffle_ps(R1,R2,_MM_SHUFFLE(1,0,2,1));
    V0 = _mm_shuffle_ps(V0,V0,_MM_SHUFFLE(1,3,2,0));
//    V0 = XMVectorPermute(R1, R2,Permute0Y1X1Y0Z);
    V1 = _mm_shuffle_ps(R1,R2,_MM_SHUFFLE(2,2,0,0));
    V1 = _mm_shuffle_ps(V1,V1,_MM_SHUFFLE(2,0,2,0));
//    V1 = XMVectorPermute(R1, R2,Permute0X1Z0X1Z);

    Q1 = _mm_shuffle_ps(R0,V0,_MM_SHUFFLE(1,0,3,0));
    Q1 = _mm_shuffle_ps(Q1,Q1,_MM_SHUFFLE(1,3,2,0));
    M.r[0] = Q1;
//    M.r[0] = XMVectorPermute(R0, V0,Permute0X1X1Y0W);
    Q1 = _mm_shuffle_ps(R0,V0,_MM_SHUFFLE(3,2,3,1));
    Q1 = _mm_shuffle_ps(Q1,Q1,_MM_SHUFFLE(1,3,0,2));
    M.r[1] = Q1;
//    M.r[1] = XMVectorPermute(R0, V0,Permute1Z0Y1W0W);
    Q1 = _mm_shuffle_ps(V1,R0,_MM_SHUFFLE(3,2,1,0));
    M.r[2] = Q1;
//    M.r[2] = XMVectorPermute(R0, V1,Permute1X1Y0Z0W);
    M.r[3] = g_XMIdentityR3;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixTransformation2D
(
    FXMVECTOR ScalingOrigin,
    FLOAT    ScalingOrientation,
    FXMVECTOR Scaling,
    FXMVECTOR RotationOrigin,
    FLOAT    Rotation,
    CXMVECTOR Translation
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;
    XMVECTOR VScaling;
    XMVECTOR NegScalingOrigin;
    XMVECTOR VScalingOrigin;
    XMMATRIX MScalingOriginI;
    XMMATRIX MScalingOrientation;
    XMMATRIX MScalingOrientationT;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;

    // M = Inverse(MScalingOrigin) * Transpose(MScalingOrientation) * MScaling * MScalingOrientation *
    //         MScalingOrigin * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;

    VScalingOrigin       = XMVectorSelect(g_XMSelect1100.v, ScalingOrigin, g_XMSelect1100.v);
    NegScalingOrigin     = XMVectorNegate(VScalingOrigin);

    MScalingOriginI      = XMMatrixTranslationFromVector(NegScalingOrigin);
    MScalingOrientation  = XMMatrixRotationZ(ScalingOrientation);
    MScalingOrientationT = XMMatrixTranspose(MScalingOrientation);
    VScaling             = XMVectorSelect(g_XMOne.v, Scaling, g_XMSelect1100.v);
    MScaling             = XMMatrixScalingFromVector(VScaling);
    VRotationOrigin      = XMVectorSelect(g_XMSelect1100.v, RotationOrigin, g_XMSelect1100.v);
    MRotation            = XMMatrixRotationZ(Rotation);
    VTranslation         = XMVectorSelect(g_XMSelect1100.v, Translation,g_XMSelect1100.v);

    M      = XMMatrixMultiply(MScalingOriginI, MScalingOrientationT);
    M      = XMMatrixMultiply(M, MScaling);
    M      = XMMatrixMultiply(M, MScalingOrientation);
    M.r[3] = XMVectorAdd(M.r[3], VScalingOrigin);
    M.r[3] = XMVectorSubtract(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = XMVectorAdd(M.r[3], VRotationOrigin);
    M.r[3] = XMVectorAdd(M.r[3], VTranslation);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    XMVECTOR VScaling;
    XMVECTOR NegScalingOrigin;
    XMVECTOR VScalingOrigin;
    XMMATRIX MScalingOriginI;
    XMMATRIX MScalingOrientation;
    XMMATRIX MScalingOrientationT;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;

    // M = Inverse(MScalingOrigin) * Transpose(MScalingOrientation) * MScaling * MScalingOrientation *
    //         MScalingOrigin * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;
    static const XMVECTORU32 Mask2 = {0xFFFFFFFF,0xFFFFFFFF,0,0};
    static const XMVECTORF32 ZWOne = {0,0,1.0f,1.0f};

    VScalingOrigin       = _mm_and_ps(ScalingOrigin, Mask2);
    NegScalingOrigin     = XMVectorNegate(VScalingOrigin);

    MScalingOriginI      = XMMatrixTranslationFromVector(NegScalingOrigin);
    MScalingOrientation  = XMMatrixRotationZ(ScalingOrientation);
    MScalingOrientationT = XMMatrixTranspose(MScalingOrientation);
    VScaling             = _mm_and_ps(Scaling, Mask2);
    VScaling = _mm_or_ps(VScaling,ZWOne);
    MScaling             = XMMatrixScalingFromVector(VScaling);
    VRotationOrigin      = _mm_and_ps(RotationOrigin, Mask2);
    MRotation            = XMMatrixRotationZ(Rotation);
    VTranslation         = _mm_and_ps(Translation, Mask2);

    M      = XMMatrixMultiply(MScalingOriginI, MScalingOrientationT);
    M      = XMMatrixMultiply(M, MScaling);
    M      = XMMatrixMultiply(M, MScalingOrientation);
    M.r[3] = XMVectorAdd(M.r[3], VScalingOrigin);
    M.r[3] = XMVectorSubtract(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = XMVectorAdd(M.r[3], VRotationOrigin);
    M.r[3] = XMVectorAdd(M.r[3], VTranslation);

    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixTransformation
(
    FXMVECTOR ScalingOrigin,
    FXMVECTOR ScalingOrientationQuaternion,
    FXMVECTOR Scaling,
    CXMVECTOR RotationOrigin,
    CXMVECTOR RotationQuaternion,
    CXMVECTOR Translation
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;
    XMVECTOR NegScalingOrigin;
    XMVECTOR VScalingOrigin;
    XMMATRIX MScalingOriginI;
    XMMATRIX MScalingOrientation;
    XMMATRIX MScalingOrientationT;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;

    // M = Inverse(MScalingOrigin) * Transpose(MScalingOrientation) * MScaling * MScalingOrientation *
    //         MScalingOrigin * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;

    VScalingOrigin       = XMVectorSelect(g_XMSelect1110.v, ScalingOrigin, g_XMSelect1110.v);
    NegScalingOrigin     = XMVectorNegate(ScalingOrigin);

    MScalingOriginI      = XMMatrixTranslationFromVector(NegScalingOrigin);
    MScalingOrientation  = XMMatrixRotationQuaternion(ScalingOrientationQuaternion);
    MScalingOrientationT = XMMatrixTranspose(MScalingOrientation);
    MScaling             = XMMatrixScalingFromVector(Scaling);
    VRotationOrigin      = XMVectorSelect(g_XMSelect1110.v, RotationOrigin, g_XMSelect1110.v);
    MRotation            = XMMatrixRotationQuaternion(RotationQuaternion);
    VTranslation         = XMVectorSelect(g_XMSelect1110.v, Translation, g_XMSelect1110.v);

    M      = XMMatrixMultiply(MScalingOriginI, MScalingOrientationT);
    M      = XMMatrixMultiply(M, MScaling);
    M      = XMMatrixMultiply(M, MScalingOrientation);
    M.r[3] = XMVectorAdd(M.r[3], VScalingOrigin);
    M.r[3] = XMVectorSubtract(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = XMVectorAdd(M.r[3], VRotationOrigin);
    M.r[3] = XMVectorAdd(M.r[3], VTranslation);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    XMVECTOR NegScalingOrigin;
    XMVECTOR VScalingOrigin;
    XMMATRIX MScalingOriginI;
    XMMATRIX MScalingOrientation;
    XMMATRIX MScalingOrientationT;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;

    // M = Inverse(MScalingOrigin) * Transpose(MScalingOrientation) * MScaling * MScalingOrientation *
    //         MScalingOrigin * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;

    VScalingOrigin       = _mm_and_ps(ScalingOrigin,g_XMMask3);
    NegScalingOrigin     = XMVectorNegate(ScalingOrigin);

    MScalingOriginI      = XMMatrixTranslationFromVector(NegScalingOrigin);
    MScalingOrientation  = XMMatrixRotationQuaternion(ScalingOrientationQuaternion);
    MScalingOrientationT = XMMatrixTranspose(MScalingOrientation);
    MScaling             = XMMatrixScalingFromVector(Scaling);
    VRotationOrigin      = _mm_and_ps(RotationOrigin,g_XMMask3);
    MRotation            = XMMatrixRotationQuaternion(RotationQuaternion);
    VTranslation         = _mm_and_ps(Translation,g_XMMask3);

    M      = XMMatrixMultiply(MScalingOriginI, MScalingOrientationT);
    M      = XMMatrixMultiply(M, MScaling);
    M      = XMMatrixMultiply(M, MScalingOrientation);
    M.r[3] = XMVectorAdd(M.r[3], VScalingOrigin);
    M.r[3] = XMVectorSubtract(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = XMVectorAdd(M.r[3], VRotationOrigin);
    M.r[3] = XMVectorAdd(M.r[3], VTranslation);

    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixAffineTransformation2D
(
    FXMVECTOR Scaling,
    FXMVECTOR RotationOrigin,
    FLOAT    Rotation,
    FXMVECTOR Translation
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;
    XMVECTOR VScaling;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;

    // M = MScaling * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;

    VScaling             = XMVectorSelect(g_XMOne.v, Scaling, g_XMSelect1100.v);
    MScaling             = XMMatrixScalingFromVector(VScaling);
    VRotationOrigin      = XMVectorSelect(g_XMSelect1100.v, RotationOrigin, g_XMSelect1100.v);
    MRotation            = XMMatrixRotationZ(Rotation);
    VTranslation         = XMVectorSelect(g_XMSelect1100.v, Translation,g_XMSelect1100.v);

    M      = MScaling;
    M.r[3] = XMVectorSubtract(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = XMVectorAdd(M.r[3], VRotationOrigin);
    M.r[3] = XMVectorAdd(M.r[3], VTranslation);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    XMVECTOR VScaling;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;
    static const XMVECTORU32 Mask2 = {0xFFFFFFFFU,0xFFFFFFFFU,0,0};
    static const XMVECTORF32 ZW1 = {0,0,1.0f,1.0f};

    // M = MScaling * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;

    VScaling = _mm_and_ps(Scaling, Mask2);
    VScaling = _mm_or_ps(VScaling, ZW1);
    MScaling = XMMatrixScalingFromVector(VScaling);
    VRotationOrigin = _mm_and_ps(RotationOrigin, Mask2);
    MRotation = XMMatrixRotationZ(Rotation);
    VTranslation = _mm_and_ps(Translation, Mask2);

    M      = MScaling;
    M.r[3] = _mm_sub_ps(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = _mm_add_ps(M.r[3], VRotationOrigin);
    M.r[3] = _mm_add_ps(M.r[3], VTranslation);
	return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixAffineTransformation
(
    FXMVECTOR Scaling,
    FXMVECTOR RotationOrigin,
    FXMVECTOR RotationQuaternion,
    CXMVECTOR Translation
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;

    // M = MScaling * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;

    MScaling            = XMMatrixScalingFromVector(Scaling);
    VRotationOrigin     = XMVectorSelect(g_XMSelect1110.v, RotationOrigin,g_XMSelect1110.v);
    MRotation           = XMMatrixRotationQuaternion(RotationQuaternion);
    VTranslation        = XMVectorSelect(g_XMSelect1110.v, Translation,g_XMSelect1110.v);

    M      = MScaling;
    M.r[3] = XMVectorSubtract(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = XMVectorAdd(M.r[3], VRotationOrigin);
    M.r[3] = XMVectorAdd(M.r[3], VTranslation);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    XMMATRIX MScaling;
    XMVECTOR VRotationOrigin;
    XMMATRIX MRotation;
    XMVECTOR VTranslation;

    // M = MScaling * Inverse(MRotationOrigin) * MRotation * MRotationOrigin * MTranslation;

    MScaling            = XMMatrixScalingFromVector(Scaling);
    VRotationOrigin     = _mm_and_ps(RotationOrigin,g_XMMask3);
    MRotation           = XMMatrixRotationQuaternion(RotationQuaternion);
    VTranslation        = _mm_and_ps(Translation,g_XMMask3);

    M      = MScaling;
    M.r[3] = _mm_sub_ps(M.r[3], VRotationOrigin);
    M      = XMMatrixMultiply(M, MRotation);
    M.r[3] = _mm_add_ps(M.r[3], VRotationOrigin);
    M.r[3] = _mm_add_ps(M.r[3], VTranslation);

    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixReflect
(
    FXMVECTOR ReflectionPlane
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR               P;
    XMVECTOR               S;
    XMVECTOR               A, B, C, D;
    XMMATRIX               M;
    static CONST XMVECTOR  NegativeTwo = {-2.0f, -2.0f, -2.0f, 0.0f};

    XMASSERT(!XMVector3Equal(ReflectionPlane, XMVectorZero()));
    XMASSERT(!XMPlaneIsInfinite(ReflectionPlane));

    P = XMPlaneNormalize(ReflectionPlane);
    S = XMVectorMultiply(P, NegativeTwo);

    A = XMVectorSplatX(P);
    B = XMVectorSplatY(P);
    C = XMVectorSplatZ(P);
    D = XMVectorSplatW(P);

    M.r[0] = XMVectorMultiplyAdd(A, S, g_XMIdentityR0.v);
    M.r[1] = XMVectorMultiplyAdd(B, S, g_XMIdentityR1.v);
    M.r[2] = XMVectorMultiplyAdd(C, S, g_XMIdentityR2.v);
    M.r[3] = XMVectorMultiplyAdd(D, S, g_XMIdentityR3.v);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    static CONST XMVECTORF32 NegativeTwo = {-2.0f, -2.0f, -2.0f, 0.0f};

    XMASSERT(!XMVector3Equal(ReflectionPlane, XMVectorZero()));
    XMASSERT(!XMPlaneIsInfinite(ReflectionPlane));

    XMVECTOR P = XMPlaneNormalize(ReflectionPlane);
    XMVECTOR S = _mm_mul_ps(P,NegativeTwo);
    XMVECTOR X = _mm_shuffle_ps(P,P,_MM_SHUFFLE(0,0,0,0));
    XMVECTOR Y = _mm_shuffle_ps(P,P,_MM_SHUFFLE(1,1,1,1));
    XMVECTOR Z = _mm_shuffle_ps(P,P,_MM_SHUFFLE(2,2,2,2));
    P = _mm_shuffle_ps(P,P,_MM_SHUFFLE(3,3,3,3));
    X = _mm_mul_ps(X,S);
    Y = _mm_mul_ps(Y,S);
    Z = _mm_mul_ps(Z,S);
    P = _mm_mul_ps(P,S);
    X = _mm_add_ps(X,g_XMIdentityR0);
    Y = _mm_add_ps(Y,g_XMIdentityR1);
    Z = _mm_add_ps(Z,g_XMIdentityR2);
    P = _mm_add_ps(P,g_XMIdentityR3);
    M.r[0] = X;
    M.r[1] = Y;
    M.r[2] = Z;
    M.r[3] = P;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixShadow
(
    FXMVECTOR ShadowPlane,
    FXMVECTOR LightPosition
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR               P;
    XMVECTOR               Dot;
    XMVECTOR               A, B, C, D;
    XMMATRIX               M;
    static CONST XMVECTORU32 Select0001 = {XM_SELECT_0, XM_SELECT_0, XM_SELECT_0, XM_SELECT_1};

    XMASSERT(!XMVector3Equal(ShadowPlane, XMVectorZero()));
    XMASSERT(!XMPlaneIsInfinite(ShadowPlane));

    P = XMPlaneNormalize(ShadowPlane);
    Dot = XMPlaneDot(P, LightPosition);
    P = XMVectorNegate(P);
    D = XMVectorSplatW(P);
    C = XMVectorSplatZ(P);
    B = XMVectorSplatY(P);
    A = XMVectorSplatX(P);
    Dot = XMVectorSelect(Select0001.v, Dot, Select0001.v);
    M.r[3] = XMVectorMultiplyAdd(D, LightPosition, Dot);
    Dot = XMVectorRotateLeft(Dot, 1);
    M.r[2] = XMVectorMultiplyAdd(C, LightPosition, Dot);
    Dot = XMVectorRotateLeft(Dot, 1);
    M.r[1] = XMVectorMultiplyAdd(B, LightPosition, Dot);
    Dot = XMVectorRotateLeft(Dot, 1);
    M.r[0] = XMVectorMultiplyAdd(A, LightPosition, Dot);
    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;
    XMASSERT(!XMVector3Equal(ShadowPlane, XMVectorZero()));
    XMASSERT(!XMPlaneIsInfinite(ShadowPlane));
    XMVECTOR P = XMPlaneNormalize(ShadowPlane);
    XMVECTOR Dot = XMPlaneDot(P,LightPosition);
    // Negate
    P = _mm_mul_ps(P,g_XMNegativeOne);
    XMVECTOR X = _mm_shuffle_ps(P,P,_MM_SHUFFLE(0,0,0,0));
    XMVECTOR Y = _mm_shuffle_ps(P,P,_MM_SHUFFLE(1,1,1,1));
    XMVECTOR Z = _mm_shuffle_ps(P,P,_MM_SHUFFLE(2,2,2,2));
    P = _mm_shuffle_ps(P,P,_MM_SHUFFLE(3,3,3,3));
    Dot = _mm_and_ps(Dot,g_XMMaskW);
    X = _mm_mul_ps(X,LightPosition);
    Y = _mm_mul_ps(Y,LightPosition);
    Z = _mm_mul_ps(Z,LightPosition);
    P = _mm_mul_ps(P,LightPosition);
    P = _mm_add_ps(P,Dot);
    Dot = _mm_shuffle_ps(Dot,Dot,_MM_SHUFFLE(0,3,2,1));
    Z = _mm_add_ps(Z,Dot);
    Dot = _mm_shuffle_ps(Dot,Dot,_MM_SHUFFLE(0,3,2,1));
    Y = _mm_add_ps(Y,Dot);
    Dot = _mm_shuffle_ps(Dot,Dot,_MM_SHUFFLE(0,3,2,1));
    X = _mm_add_ps(X,Dot);
    // Store the resulting matrix
    M.r[0] = X;
    M.r[1] = Y;
    M.r[2] = Z;
    M.r[3] = P;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// View and projection initialization operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixLookAtLH
(
    FXMVECTOR EyePosition,
    FXMVECTOR FocusPosition,
    FXMVECTOR UpDirection
)
{
    XMVECTOR EyeDirection;
    XMMATRIX M;

    EyeDirection = XMVectorSubtract(FocusPosition, EyePosition);
    M = XMMatrixLookToLH(EyePosition, EyeDirection, UpDirection);

    return M;
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixLookAtRH
(
    FXMVECTOR EyePosition,
    FXMVECTOR FocusPosition,
    FXMVECTOR UpDirection
)
{
    XMVECTOR NegEyeDirection;
    XMMATRIX M;

    NegEyeDirection = XMVectorSubtract(EyePosition, FocusPosition);
    M = XMMatrixLookToLH(EyePosition, NegEyeDirection, UpDirection);

    return M;
}

//------------------------------------------------------------------------------

XMINLINE XMMATRIX XMMatrixLookToLH
(
    FXMVECTOR EyePosition,
    FXMVECTOR EyeDirection,
    FXMVECTOR UpDirection
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR NegEyePosition;
    XMVECTOR D0, D1, D2;
    XMVECTOR R0, R1, R2;
    XMMATRIX M;

    XMASSERT(!XMVector3Equal(EyeDirection, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(EyeDirection));
    XMASSERT(!XMVector3Equal(UpDirection, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(UpDirection));

    R2 = XMVector3Normalize(EyeDirection);

    R0 = XMVector3Cross(UpDirection, R2);
    R0 = XMVector3Normalize(R0);

    R1 = XMVector3Cross(R2, R0);

    NegEyePosition = XMVectorNegate(EyePosition);

    D0 = XMVector3Dot(R0, NegEyePosition);
    D1 = XMVector3Dot(R1, NegEyePosition);
    D2 = XMVector3Dot(R2, NegEyePosition);

    M.r[0] = XMVectorSelect(D0, R0, g_XMSelect1110.v);
    M.r[1] = XMVectorSelect(D1, R1, g_XMSelect1110.v);
    M.r[2] = XMVectorSelect(D2, R2, g_XMSelect1110.v);
    M.r[3] = g_XMIdentityR3.v;

    M = XMMatrixTranspose(M);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX M;

    XMASSERT(!XMVector3Equal(EyeDirection, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(EyeDirection));
    XMASSERT(!XMVector3Equal(UpDirection, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(UpDirection));

    XMVECTOR R2 = XMVector3Normalize(EyeDirection);
    XMVECTOR R0 = XMVector3Cross(UpDirection, R2);
    R0 = XMVector3Normalize(R0);
    XMVECTOR R1 = XMVector3Cross(R2,R0);
    XMVECTOR NegEyePosition = _mm_mul_ps(EyePosition,g_XMNegativeOne);
    XMVECTOR D0 = XMVector3Dot(R0,NegEyePosition);
    XMVECTOR D1 = XMVector3Dot(R1,NegEyePosition);
    XMVECTOR D2 = XMVector3Dot(R2,NegEyePosition);
    R0 = _mm_and_ps(R0,g_XMMask3);
    R1 = _mm_and_ps(R1,g_XMMask3);
    R2 = _mm_and_ps(R2,g_XMMask3);
    D0 = _mm_and_ps(D0,g_XMMaskW);
    D1 = _mm_and_ps(D1,g_XMMaskW);
    D2 = _mm_and_ps(D2,g_XMMaskW);
    D0 = _mm_or_ps(D0,R0);
    D1 = _mm_or_ps(D1,R1);
    D2 = _mm_or_ps(D2,R2);
    M.r[0] = D0;
    M.r[1] = D1;
    M.r[2] = D2;
    M.r[3] = g_XMIdentityR3;
    M = XMMatrixTranspose(M);
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixLookToRH
(
    FXMVECTOR EyePosition,
    FXMVECTOR EyeDirection,
    FXMVECTOR UpDirection
)
{
    XMVECTOR NegEyeDirection;
    XMMATRIX M;

    NegEyeDirection = XMVectorNegate(EyeDirection);
    M = XMMatrixLookToLH(EyePosition, NegEyeDirection, UpDirection);

    return M;
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixPerspectiveLH
(
    FLOAT ViewWidth,
    FLOAT ViewHeight,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT TwoNearZ, fRange;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    TwoNearZ = NearZ + NearZ;
    fRange = FarZ / (FarZ - NearZ);
    M.m[0][0] = TwoNearZ / ViewWidth;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = TwoNearZ / ViewHeight;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = fRange;
    M.m[2][3] = 1.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = -fRange * NearZ;
    M.m[3][3] = 0.0f;

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

	XMMATRIX M;
    FLOAT TwoNearZ = NearZ + NearZ;
    FLOAT fRange = FarZ / (FarZ - NearZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        TwoNearZ / ViewWidth,
        TwoNearZ / ViewHeight,
        fRange,
        -fRange * NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // TwoNearZ / ViewWidth,0,0,0
    M.r[0] = vTemp;
    // 0,TwoNearZ / ViewHeight,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // x=fRange,y=-fRange * NearZ,0,1.0f
    vValues = _mm_shuffle_ps(vValues,g_XMIdentityR3,_MM_SHUFFLE(3,2,3,2));
    // 0,0,fRange,1.0f
    vTemp = _mm_setzero_ps();
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(3,0,0,0));
    M.r[2] = vTemp;
    // 0,0,-fRange * NearZ,0
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(2,1,0,0));
    M.r[3] = vTemp;

    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixPerspectiveRH
(
    FLOAT ViewWidth,
    FLOAT ViewHeight,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT TwoNearZ, fRange;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    TwoNearZ = NearZ + NearZ;
    fRange = FarZ / (NearZ - FarZ);
    M.m[0][0] = TwoNearZ / ViewWidth;
    M.m[0][1] = 0.0f;
    M.m[0][2] = 0.0f;
    M.m[0][3] = 0.0f;

    M.m[1][0] = 0.0f;
    M.m[1][1] = TwoNearZ / ViewHeight;
    M.m[1][2] = 0.0f;
    M.m[1][3] = 0.0f;

    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = fRange;
    M.m[2][3] = -1.0f;

    M.m[3][0] = 0.0f;
    M.m[3][1] = 0.0f;
    M.m[3][2] = fRange * NearZ;
    M.m[3][3] = 0.0f;

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

	XMMATRIX M;
    FLOAT TwoNearZ = NearZ + NearZ;
    FLOAT fRange = FarZ / (NearZ-FarZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        TwoNearZ / ViewWidth,
        TwoNearZ / ViewHeight,
        fRange,
        fRange * NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // TwoNearZ / ViewWidth,0,0,0
    M.r[0] = vTemp;
    // 0,TwoNearZ / ViewHeight,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // x=fRange,y=-fRange * NearZ,0,-1.0f
    vValues = _mm_shuffle_ps(vValues,g_XMNegIdentityR3,_MM_SHUFFLE(3,2,3,2));
    // 0,0,fRange,-1.0f
    vTemp = _mm_setzero_ps();
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(3,0,0,0));
    M.r[2] = vTemp;
    // 0,0,-fRange * NearZ,0
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(2,1,0,0));
    M.r[3] = vTemp;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixPerspectiveFovLH
(
    FLOAT FovAngleY,
    FLOAT AspectRatio,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT    SinFov;
    FLOAT    CosFov;
    FLOAT    Height;
    FLOAT    Width;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(FovAngleY, 0.0f, 0.00001f * 2.0f));
    XMASSERT(!XMScalarNearEqual(AspectRatio, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    Height = CosFov / SinFov;
    Width = Height / AspectRatio;

    M.r[0] = XMVectorSet(Width, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, Height, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(0.0f, 0.0f, FarZ / (FarZ - NearZ), 1.0f);
    M.r[3] = XMVectorSet(0.0f, 0.0f, -M.r[2].vector4_f32[2] * NearZ, 0.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(FovAngleY, 0.0f, 0.00001f * 2.0f));
    XMASSERT(!XMScalarNearEqual(AspectRatio, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));
	XMMATRIX M;
    FLOAT    SinFov;
    FLOAT    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);
    FLOAT fRange = FarZ / (FarZ-NearZ);
    // Note: This is recorded on the stack
    FLOAT Height = CosFov / SinFov;
    XMVECTOR rMem = {
        Height / AspectRatio,
        Height,
        fRange,
        -fRange * NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // CosFov / SinFov,0,0,0
    M.r[0] = vTemp;
    // 0,Height / AspectRatio,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // x=fRange,y=-fRange * NearZ,0,1.0f
    vTemp = _mm_setzero_ps();
    vValues = _mm_shuffle_ps(vValues,g_XMIdentityR3,_MM_SHUFFLE(3,2,3,2));
    // 0,0,fRange,1.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(3,0,0,0));
    M.r[2] = vTemp;
    // 0,0,-fRange * NearZ,0.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(2,1,0,0));
    M.r[3] = vTemp;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixPerspectiveFovRH
(
    FLOAT FovAngleY,
    FLOAT AspectRatio,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT    SinFov;
    FLOAT    CosFov;
    FLOAT    Height;
    FLOAT    Width;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(FovAngleY, 0.0f, 0.00001f * 2.0f));
    XMASSERT(!XMScalarNearEqual(AspectRatio, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);

    Height = CosFov / SinFov;
    Width = Height / AspectRatio;

    M.r[0] = XMVectorSet(Width, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, Height, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(0.0f, 0.0f, FarZ / (NearZ - FarZ), -1.0f);
    M.r[3] = XMVectorSet(0.0f, 0.0f, M.r[2].vector4_f32[2] * NearZ, 0.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(FovAngleY, 0.0f, 0.00001f * 2.0f));
    XMASSERT(!XMScalarNearEqual(AspectRatio, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));
	XMMATRIX M;
    FLOAT    SinFov;
    FLOAT    CosFov;
    XMScalarSinCos(&SinFov, &CosFov, 0.5f * FovAngleY);
    FLOAT fRange = FarZ / (NearZ-FarZ);
    // Note: This is recorded on the stack
    FLOAT Height = CosFov / SinFov;
    XMVECTOR rMem = {
        Height / AspectRatio,
        Height,
        fRange,
        fRange * NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // CosFov / SinFov,0,0,0
    M.r[0] = vTemp;
    // 0,Height / AspectRatio,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // x=fRange,y=-fRange * NearZ,0,-1.0f
    vTemp = _mm_setzero_ps();
    vValues = _mm_shuffle_ps(vValues,g_XMNegIdentityR3,_MM_SHUFFLE(3,2,3,2));
    // 0,0,fRange,-1.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(3,0,0,0));
    M.r[2] = vTemp;
    // 0,0,fRange * NearZ,0.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(2,1,0,0));
    M.r[3] = vTemp;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixPerspectiveOffCenterLH
(
    FLOAT ViewLeft,
    FLOAT ViewRight,
    FLOAT ViewBottom,
    FLOAT ViewTop,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT    TwoNearZ;
    FLOAT    ReciprocalWidth;
    FLOAT    ReciprocalHeight;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewRight, ViewLeft, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewTop, ViewBottom, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    TwoNearZ = NearZ + NearZ;
    ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);

    M.r[0] = XMVectorSet(TwoNearZ * ReciprocalWidth, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, TwoNearZ * ReciprocalHeight, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(-(ViewLeft + ViewRight) * ReciprocalWidth,
                         -(ViewTop + ViewBottom) * ReciprocalHeight,
                         FarZ / (FarZ - NearZ),
                         1.0f);
    M.r[3] = XMVectorSet(0.0f, 0.0f, -M.r[2].vector4_f32[2] * NearZ, 0.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(ViewRight, ViewLeft, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewTop, ViewBottom, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));
	XMMATRIX M;
    FLOAT TwoNearZ = NearZ+NearZ;
    FLOAT ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    FLOAT ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);
    FLOAT fRange = FarZ / (FarZ-NearZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        TwoNearZ*ReciprocalWidth,
        TwoNearZ*ReciprocalHeight,
        -fRange * NearZ,
        0
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // TwoNearZ*ReciprocalWidth,0,0,0
    M.r[0] = vTemp;
    // 0,TwoNearZ*ReciprocalHeight,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // 0,0,fRange,1.0f
    M.m[2][0] = -(ViewLeft + ViewRight) * ReciprocalWidth;
    M.m[2][1] = -(ViewTop + ViewBottom) * ReciprocalHeight;
    M.m[2][2] = fRange;
    M.m[2][3] = 1.0f;
    // 0,0,-fRange * NearZ,0.0f
    vValues = _mm_and_ps(vValues,g_XMMaskZ);
    M.r[3] = vValues;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixPerspectiveOffCenterRH
(
    FLOAT ViewLeft,
    FLOAT ViewRight,
    FLOAT ViewBottom,
    FLOAT ViewTop,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT    TwoNearZ;
    FLOAT    ReciprocalWidth;
    FLOAT    ReciprocalHeight;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewRight, ViewLeft, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewTop, ViewBottom, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    TwoNearZ = NearZ + NearZ;
    ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);

    M.r[0] = XMVectorSet(TwoNearZ * ReciprocalWidth, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, TwoNearZ * ReciprocalHeight, 0.0f, 0.0f);
    M.r[2] = XMVectorSet((ViewLeft + ViewRight) * ReciprocalWidth,
                         (ViewTop + ViewBottom) * ReciprocalHeight,
                         FarZ / (NearZ - FarZ),
                         -1.0f);
    M.r[3] = XMVectorSet(0.0f, 0.0f, M.r[2].vector4_f32[2] * NearZ, 0.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(ViewRight, ViewLeft, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewTop, ViewBottom, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

	XMMATRIX M;
    FLOAT TwoNearZ = NearZ+NearZ;
    FLOAT ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    FLOAT ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);
    FLOAT fRange = FarZ / (NearZ-FarZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        TwoNearZ*ReciprocalWidth,
        TwoNearZ*ReciprocalHeight,
        fRange * NearZ,
        0
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // TwoNearZ*ReciprocalWidth,0,0,0
    M.r[0] = vTemp;
    // 0,TwoNearZ*ReciprocalHeight,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // 0,0,fRange,1.0f
    M.m[2][0] = (ViewLeft + ViewRight) * ReciprocalWidth;
    M.m[2][1] = (ViewTop + ViewBottom) * ReciprocalHeight;
    M.m[2][2] = fRange;
    M.m[2][3] = -1.0f;
    // 0,0,-fRange * NearZ,0.0f
    vValues = _mm_and_ps(vValues,g_XMMaskZ);
    M.r[3] = vValues;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixOrthographicLH
(
    FLOAT ViewWidth,
    FLOAT ViewHeight,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT fRange;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    fRange = 1.0f / (FarZ-NearZ);
    M.r[0] = XMVectorSet(2.0f / ViewWidth, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, 2.0f / ViewHeight, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(0.0f, 0.0f, fRange, 0.0f);
    M.r[3] = XMVectorSet(0.0f, 0.0f, -fRange * NearZ, 1.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));
	XMMATRIX M;
    FLOAT fRange = 1.0f / (FarZ-NearZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        2.0f / ViewWidth,
        2.0f / ViewHeight,
        fRange,
        -fRange * NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // 2.0f / ViewWidth,0,0,0
    M.r[0] = vTemp;
    // 0,2.0f / ViewHeight,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // x=fRange,y=-fRange * NearZ,0,1.0f
    vTemp = _mm_setzero_ps();
    vValues = _mm_shuffle_ps(vValues,g_XMIdentityR3,_MM_SHUFFLE(3,2,3,2));
    // 0,0,fRange,0.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(2,0,0,0));
    M.r[2] = vTemp;
    // 0,0,-fRange * NearZ,1.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(3,1,0,0));
    M.r[3] = vTemp;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixOrthographicRH
(
    FLOAT ViewWidth,
    FLOAT ViewHeight,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    M.r[0] = XMVectorSet(2.0f / ViewWidth, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, 2.0f / ViewHeight, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(0.0f, 0.0f, 1.0f / (NearZ - FarZ), 0.0f);
    M.r[3] = XMVectorSet(0.0f, 0.0f, M.r[2].vector4_f32[2] * NearZ, 1.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(!XMScalarNearEqual(ViewWidth, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewHeight, 0.0f, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));
	XMMATRIX M;
    FLOAT fRange = 1.0f / (NearZ-FarZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        2.0f / ViewWidth,
        2.0f / ViewHeight,
        fRange,
        fRange * NearZ
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // 2.0f / ViewWidth,0,0,0
    M.r[0] = vTemp;
    // 0,2.0f / ViewHeight,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    M.r[1] = vTemp;
    // x=fRange,y=fRange * NearZ,0,1.0f
    vTemp = _mm_setzero_ps();
    vValues = _mm_shuffle_ps(vValues,g_XMIdentityR3,_MM_SHUFFLE(3,2,3,2));
    // 0,0,fRange,0.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(2,0,0,0));
    M.r[2] = vTemp;
    // 0,0,fRange * NearZ,1.0f
    vTemp = _mm_shuffle_ps(vTemp,vValues,_MM_SHUFFLE(3,1,0,0));
    M.r[3] = vTemp;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixOrthographicOffCenterLH
(
    FLOAT ViewLeft,
    FLOAT ViewRight,
    FLOAT ViewBottom,
    FLOAT ViewTop,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT    ReciprocalWidth;
    FLOAT    ReciprocalHeight;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewRight, ViewLeft, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewTop, ViewBottom, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);

    M.r[0] = XMVectorSet(ReciprocalWidth + ReciprocalWidth, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, ReciprocalHeight + ReciprocalHeight, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(0.0f, 0.0f, 1.0f / (FarZ - NearZ), 0.0f);
    M.r[3] = XMVectorSet(-(ViewLeft + ViewRight) * ReciprocalWidth,
                         -(ViewTop + ViewBottom) * ReciprocalHeight,
                         -M.r[2].vector4_f32[2] * NearZ,
                         1.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
	XMMATRIX M;
    FLOAT fReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    FLOAT fReciprocalHeight = 1.0f / (ViewTop - ViewBottom);
    FLOAT fRange = 1.0f / (FarZ-NearZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        fReciprocalWidth,
        fReciprocalHeight,
        fRange,
        1.0f
    };
    XMVECTOR rMem2 = {
        -(ViewLeft + ViewRight),
        -(ViewTop + ViewBottom),
        -NearZ,
        1.0f
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // fReciprocalWidth*2,0,0,0
    vTemp = _mm_add_ss(vTemp,vTemp);
    M.r[0] = vTemp;
    // 0,fReciprocalHeight*2,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    vTemp = _mm_add_ps(vTemp,vTemp);
    M.r[1] = vTemp;
    // 0,0,fRange,0.0f
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskZ);
    M.r[2] = vTemp;
    // -(ViewLeft + ViewRight)*fReciprocalWidth,-(ViewTop + ViewBottom)*fReciprocalHeight,fRange*-NearZ,1.0f
    vValues = _mm_mul_ps(vValues,rMem2);
    M.r[3] = vValues;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMMATRIX XMMatrixOrthographicOffCenterRH
(
    FLOAT ViewLeft,
    FLOAT ViewRight,
    FLOAT ViewBottom,
    FLOAT ViewTop,
    FLOAT NearZ,
    FLOAT FarZ
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT    ReciprocalWidth;
    FLOAT    ReciprocalHeight;
    XMMATRIX M;

    XMASSERT(!XMScalarNearEqual(ViewRight, ViewLeft, 0.00001f));
    XMASSERT(!XMScalarNearEqual(ViewTop, ViewBottom, 0.00001f));
    XMASSERT(!XMScalarNearEqual(FarZ, NearZ, 0.00001f));

    ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);

    M.r[0] = XMVectorSet(ReciprocalWidth + ReciprocalWidth, 0.0f, 0.0f, 0.0f);
    M.r[1] = XMVectorSet(0.0f, ReciprocalHeight + ReciprocalHeight, 0.0f, 0.0f);
    M.r[2] = XMVectorSet(0.0f, 0.0f, 1.0f / (NearZ - FarZ), 0.0f);
    M.r[3] = XMVectorSet(-(ViewLeft + ViewRight) * ReciprocalWidth,
                         -(ViewTop + ViewBottom) * ReciprocalHeight,
                         M.r[2].vector4_f32[2] * NearZ,
                         1.0f);

    return M;

#elif defined(_XM_SSE_INTRINSICS_)
	XMMATRIX M;
    FLOAT fReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
    FLOAT fReciprocalHeight = 1.0f / (ViewTop - ViewBottom);
    FLOAT fRange = 1.0f / (NearZ-FarZ);
    // Note: This is recorded on the stack
    XMVECTOR rMem = {
        fReciprocalWidth,
        fReciprocalHeight,
        fRange,
        1.0f
    };
    XMVECTOR rMem2 = {
        -(ViewLeft + ViewRight),
        -(ViewTop + ViewBottom),
        NearZ,
        1.0f
    };
    // Copy from memory to SSE register
    XMVECTOR vValues = rMem;
    XMVECTOR vTemp = _mm_setzero_ps();
    // Copy x only
    vTemp = _mm_move_ss(vTemp,vValues);
    // fReciprocalWidth*2,0,0,0
    vTemp = _mm_add_ss(vTemp,vTemp);
    M.r[0] = vTemp;
    // 0,fReciprocalHeight*2,0,0
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskY);
    vTemp = _mm_add_ps(vTemp,vTemp);
    M.r[1] = vTemp;
    // 0,0,fRange,0.0f
    vTemp = vValues;
    vTemp = _mm_and_ps(vTemp,g_XMMaskZ);
    M.r[2] = vTemp;
    // -(ViewLeft + ViewRight)*fReciprocalWidth,-(ViewTop + ViewBottom)*fReciprocalHeight,fRange*-NearZ,1.0f
    vValues = _mm_mul_ps(vValues,rMem2);
    M.r[3] = vValues;
    return M;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

#ifdef __cplusplus

/****************************************************************************
 *
 * XMMATRIX operators and methods
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMMATRIX::_XMMATRIX
(
    FXMVECTOR R0,
    FXMVECTOR R1,
    FXMVECTOR R2,
    CXMVECTOR R3
)
{
    r[0] = R0;
    r[1] = R1;
    r[2] = R2;
    r[3] = R3;
}

//------------------------------------------------------------------------------

XMFINLINE _XMMATRIX::_XMMATRIX
(
    FLOAT m00, FLOAT m01, FLOAT m02, FLOAT m03,
    FLOAT m10, FLOAT m11, FLOAT m12, FLOAT m13,
    FLOAT m20, FLOAT m21, FLOAT m22, FLOAT m23,
    FLOAT m30, FLOAT m31, FLOAT m32, FLOAT m33
)
{
    r[0] = XMVectorSet(m00, m01, m02, m03);
    r[1] = XMVectorSet(m10, m11, m12, m13);
    r[2] = XMVectorSet(m20, m21, m22, m23);
    r[3] = XMVectorSet(m30, m31, m32, m33);
}

//------------------------------------------------------------------------------

XMFINLINE _XMMATRIX::_XMMATRIX
(
    CONST FLOAT* pArray
)
{
    r[0] = XMLoadFloat4((XMFLOAT4*)pArray);
    r[1] = XMLoadFloat4((XMFLOAT4*)(pArray + 4));
    r[2] = XMLoadFloat4((XMFLOAT4*)(pArray + 8));
    r[3] = XMLoadFloat4((XMFLOAT4*)(pArray + 12));
}

//------------------------------------------------------------------------------

XMFINLINE _XMMATRIX& _XMMATRIX::operator=
(
    CONST _XMMATRIX& M
)
{
    r[0] = M.r[0];
    r[1] = M.r[1];
    r[2] = M.r[2];
    r[3] = M.r[3];
    return *this;
}

//------------------------------------------------------------------------------

#ifndef XM_NO_OPERATOR_OVERLOADS

#if !defined(_XBOX_VER) && defined(_XM_ISVS2005_) && defined(_XM_X64_)
#pragma warning(push)
#pragma warning(disable : 4328)
#endif

XMFINLINE _XMMATRIX& _XMMATRIX::operator*=
(
    CONST _XMMATRIX& M
)
{
    *this = XMMatrixMultiply(*this, M);
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMMATRIX _XMMATRIX::operator*
(
    CONST _XMMATRIX& M
) CONST
{
    return XMMatrixMultiply(*this, M);
}

#if !defined(_XBOX_VER) && defined(_XM_ISVS2005_) && defined(_XM_X64_)
#pragma warning(pop)
#endif

#endif // !XM_NO_OPERATOR_OVERLOADS

/****************************************************************************
 *
 * XMFLOAT3X3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT3X3::_XMFLOAT3X3
(
    FLOAT m00, FLOAT m01, FLOAT m02,
    FLOAT m10, FLOAT m11, FLOAT m12,
    FLOAT m20, FLOAT m21, FLOAT m22
)
{
    m[0][0] = m00;
    m[0][1] = m01;
    m[0][2] = m02;

    m[1][0] = m10;
    m[1][1] = m11;
    m[1][2] = m12;

    m[2][0] = m20;
    m[2][1] = m21;
    m[2][2] = m22;
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT3X3::_XMFLOAT3X3
(
    CONST FLOAT* pArray
)
{
    UINT Row;
    UINT Column;

    for (Row = 0; Row < 3; Row++)
    {
        for (Column = 0; Column < 3; Column++)
        {
            m[Row][Column] = pArray[Row * 3 + Column];
        }
    }
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT3X3& _XMFLOAT3X3::operator=
(
    CONST _XMFLOAT3X3& Float3x3
)
{
    _11 = Float3x3._11;
    _12 = Float3x3._12;
    _13 = Float3x3._13;
    _21 = Float3x3._21;
    _22 = Float3x3._22;
    _23 = Float3x3._23;
    _31 = Float3x3._31;
    _32 = Float3x3._32;
    _33 = Float3x3._33;

    return *this;
}

/****************************************************************************
 *
 * XMFLOAT4X3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4X3::_XMFLOAT4X3
(
    FLOAT m00, FLOAT m01, FLOAT m02,
    FLOAT m10, FLOAT m11, FLOAT m12,
    FLOAT m20, FLOAT m21, FLOAT m22,
    FLOAT m30, FLOAT m31, FLOAT m32
)
{
    m[0][0] = m00;
    m[0][1] = m01;
    m[0][2] = m02;

    m[1][0] = m10;
    m[1][1] = m11;
    m[1][2] = m12;

    m[2][0] = m20;
    m[2][1] = m21;
    m[2][2] = m22;

    m[3][0] = m30;
    m[3][1] = m31;
    m[3][2] = m32;
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4X3::_XMFLOAT4X3
(
    CONST FLOAT* pArray
)
{
    UINT Row;
    UINT Column;

    for (Row = 0; Row < 4; Row++)
    {
        for (Column = 0; Column < 3; Column++)
        {
            m[Row][Column] = pArray[Row * 3 + Column];
        }
    }
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4X3& _XMFLOAT4X3::operator=
(
    CONST _XMFLOAT4X3& Float4x3
)
{
    XMVECTOR V1 = XMLoadFloat4((XMFLOAT4*)&Float4x3._11);
    XMVECTOR V2 = XMLoadFloat4((XMFLOAT4*)&Float4x3._22);
    XMVECTOR V3 = XMLoadFloat4((XMFLOAT4*)&Float4x3._33);

    XMStoreFloat4((XMFLOAT4*)&_11, V1);
    XMStoreFloat4((XMFLOAT4*)&_22, V2);
    XMStoreFloat4((XMFLOAT4*)&_33, V3);

    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE XMFLOAT4X3A& XMFLOAT4X3A::operator=
(
    CONST XMFLOAT4X3A& Float4x3
)
{
    XMVECTOR V1 = XMLoadFloat4A((XMFLOAT4A*)&Float4x3._11);
    XMVECTOR V2 = XMLoadFloat4A((XMFLOAT4A*)&Float4x3._22);
    XMVECTOR V3 = XMLoadFloat4A((XMFLOAT4A*)&Float4x3._33);

    XMStoreFloat4A((XMFLOAT4A*)&_11, V1);
    XMStoreFloat4A((XMFLOAT4A*)&_22, V2);
    XMStoreFloat4A((XMFLOAT4A*)&_33, V3);

    return *this;
}

/****************************************************************************
 *
 * XMFLOAT4X4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4X4::_XMFLOAT4X4
(
    FLOAT m00, FLOAT m01, FLOAT m02, FLOAT m03,
    FLOAT m10, FLOAT m11, FLOAT m12, FLOAT m13,
    FLOAT m20, FLOAT m21, FLOAT m22, FLOAT m23,
    FLOAT m30, FLOAT m31, FLOAT m32, FLOAT m33
)
{
    m[0][0] = m00;
    m[0][1] = m01;
    m[0][2] = m02;
    m[0][3] = m03;

    m[1][0] = m10;
    m[1][1] = m11;
    m[1][2] = m12;
    m[1][3] = m13;

    m[2][0] = m20;
    m[2][1] = m21;
    m[2][2] = m22;
    m[2][3] = m23;

    m[3][0] = m30;
    m[3][1] = m31;
    m[3][2] = m32;
    m[3][3] = m33;
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4X4::_XMFLOAT4X4
(
    CONST FLOAT* pArray
)
{
    UINT Row;
    UINT Column;

    for (Row = 0; Row < 4; Row++)
    {
        for (Column = 0; Column < 4; Column++)
        {
            m[Row][Column] = pArray[Row * 4 + Column];
        }
    }
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4X4& _XMFLOAT4X4::operator=
(
    CONST _XMFLOAT4X4& Float4x4
)
{
    XMVECTOR V1 = XMLoadFloat4((XMFLOAT4*)&Float4x4._11);
    XMVECTOR V2 = XMLoadFloat4((XMFLOAT4*)&Float4x4._21);
    XMVECTOR V3 = XMLoadFloat4((XMFLOAT4*)&Float4x4._31);
    XMVECTOR V4 = XMLoadFloat4((XMFLOAT4*)&Float4x4._41);

    XMStoreFloat4((XMFLOAT4*)&_11, V1);
    XMStoreFloat4((XMFLOAT4*)&_21, V2);
    XMStoreFloat4((XMFLOAT4*)&_31, V3);
    XMStoreFloat4((XMFLOAT4*)&_41, V4);

    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE XMFLOAT4X4A& XMFLOAT4X4A::operator=
(
    CONST XMFLOAT4X4A& Float4x4
)
{
    XMVECTOR V1 = XMLoadFloat4A((XMFLOAT4A*)&Float4x4._11);
    XMVECTOR V2 = XMLoadFloat4A((XMFLOAT4A*)&Float4x4._21);
    XMVECTOR V3 = XMLoadFloat4A((XMFLOAT4A*)&Float4x4._31);
    XMVECTOR V4 = XMLoadFloat4A((XMFLOAT4A*)&Float4x4._41);

    XMStoreFloat4A((XMFLOAT4A*)&_11, V1);
    XMStoreFloat4A((XMFLOAT4A*)&_21, V2);
    XMStoreFloat4A((XMFLOAT4A*)&_31, V3);
    XMStoreFloat4A((XMFLOAT4A*)&_41, V4);

    return *this;
}

#endif // __cplusplus

#endif // __XNAMATHMATRIX_INL__
