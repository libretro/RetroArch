/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    xnamathmisc.inl

Abstract:

	XNA math library for Windows and Xbox 360: Quaternion, plane, and color functions.
--*/

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifndef __XNAMATHMISC_INL__
#define __XNAMATHMISC_INL__

/****************************************************************************
 *
 * Quaternion
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE BOOL XMQuaternionEqual
(
    FXMVECTOR Q1,
    FXMVECTOR Q2
)
{
    return XMVector4Equal(Q1, Q2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMQuaternionNotEqual
(
    FXMVECTOR Q1,
    FXMVECTOR Q2
)
{
    return XMVector4NotEqual(Q1, Q2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMQuaternionIsNaN
(
    FXMVECTOR Q
)
{
    return XMVector4IsNaN(Q);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMQuaternionIsInfinite
(
    FXMVECTOR Q
)
{
    return XMVector4IsInfinite(Q);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMQuaternionIsIdentity
(
    FXMVECTOR Q
)
{
#if defined(_XM_NO_INTRINSICS_)

    return XMVector4Equal(Q, g_XMIdentityR3.v);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(Q,g_XMIdentityR3);
    return (_mm_movemask_ps(vTemp)==0x0f) ? true : false;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionDot
(
    FXMVECTOR Q1,
    FXMVECTOR Q2
)
{
    return XMVector4Dot(Q1, Q2);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionMultiply
(
    FXMVECTOR Q1,
    FXMVECTOR Q2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR         NegativeQ1;
    XMVECTOR         Q2X;
    XMVECTOR         Q2Y;
    XMVECTOR         Q2Z;
    XMVECTOR         Q2W;
    XMVECTOR         Q1WZYX;
    XMVECTOR         Q1ZWXY;
    XMVECTOR         Q1YXWZ;
    XMVECTOR         Result;
    CONST XMVECTORU32 ControlWZYX = {XM_PERMUTE_0W, XM_PERMUTE_1Z, XM_PERMUTE_0Y, XM_PERMUTE_1X};
    CONST XMVECTORU32 ControlZWXY = {XM_PERMUTE_0Z, XM_PERMUTE_0W, XM_PERMUTE_1X, XM_PERMUTE_1Y};
    CONST XMVECTORU32 ControlYXWZ = {XM_PERMUTE_1Y, XM_PERMUTE_0X, XM_PERMUTE_0W, XM_PERMUTE_1Z};

    NegativeQ1 = XMVectorNegate(Q1);

    Q2W = XMVectorSplatW(Q2);
    Q2X = XMVectorSplatX(Q2);
    Q2Y = XMVectorSplatY(Q2);
    Q2Z = XMVectorSplatZ(Q2);

    Q1WZYX = XMVectorPermute(Q1, NegativeQ1, ControlWZYX.v);
    Q1ZWXY = XMVectorPermute(Q1, NegativeQ1, ControlZWXY.v);
    Q1YXWZ = XMVectorPermute(Q1, NegativeQ1, ControlYXWZ.v);

    Result = XMVectorMultiply(Q1, Q2W);
    Result = XMVectorMultiplyAdd(Q1WZYX, Q2X, Result);
    Result = XMVectorMultiplyAdd(Q1ZWXY, Q2Y, Result);
    Result = XMVectorMultiplyAdd(Q1YXWZ, Q2Z, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 ControlWZYX = { 1.0f,-1.0f, 1.0f,-1.0f};
    static CONST XMVECTORF32 ControlZWXY = { 1.0f, 1.0f,-1.0f,-1.0f};
    static CONST XMVECTORF32 ControlYXWZ = {-1.0f, 1.0f, 1.0f,-1.0f};
    // Copy to SSE registers and use as few as possible for x86
    XMVECTOR Q2X = Q2;
    XMVECTOR Q2Y = Q2;
    XMVECTOR Q2Z = Q2;
    XMVECTOR vResult = Q2;
    // Splat with one instruction
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,3,3,3));
    Q2X = _mm_shuffle_ps(Q2X,Q2X,_MM_SHUFFLE(0,0,0,0));
    Q2Y = _mm_shuffle_ps(Q2Y,Q2Y,_MM_SHUFFLE(1,1,1,1));
    Q2Z = _mm_shuffle_ps(Q2Z,Q2Z,_MM_SHUFFLE(2,2,2,2));
    // Retire Q1 and perform Q1*Q2W
    vResult = _mm_mul_ps(vResult,Q1);
    XMVECTOR Q1Shuffle = Q1;
    // Shuffle the copies of Q1
    Q1Shuffle = _mm_shuffle_ps(Q1Shuffle,Q1Shuffle,_MM_SHUFFLE(0,1,2,3));
    // Mul by Q1WZYX
    Q2X = _mm_mul_ps(Q2X,Q1Shuffle);
    Q1Shuffle = _mm_shuffle_ps(Q1Shuffle,Q1Shuffle,_MM_SHUFFLE(2,3,0,1));
    // Flip the signs on y and z
    Q2X = _mm_mul_ps(Q2X,ControlWZYX);
    // Mul by Q1ZWXY
    Q2Y = _mm_mul_ps(Q2Y,Q1Shuffle);
    Q1Shuffle = _mm_shuffle_ps(Q1Shuffle,Q1Shuffle,_MM_SHUFFLE(0,1,2,3));
    // Flip the signs on z and w
    Q2Y = _mm_mul_ps(Q2Y,ControlZWXY);
    // Mul by Q1YXWZ
    Q2Z = _mm_mul_ps(Q2Z,Q1Shuffle);
    vResult = _mm_add_ps(vResult,Q2X);
    // Flip the signs on x and w
    Q2Z = _mm_mul_ps(Q2Z,ControlYXWZ);
    Q2Y = _mm_add_ps(Q2Y,Q2Z);
    vResult = _mm_add_ps(vResult,Q2Y);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionLengthSq
(
    FXMVECTOR Q
)
{
    return XMVector4LengthSq(Q);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionReciprocalLength
(
    FXMVECTOR Q
)
{
    return XMVector4ReciprocalLength(Q);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionLength
(
    FXMVECTOR Q
)
{
    return XMVector4Length(Q);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionNormalizeEst
(
    FXMVECTOR Q
)
{
    return XMVector4NormalizeEst(Q);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionNormalize
(
    FXMVECTOR Q
)
{
    return XMVector4Normalize(Q);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionConjugate
(
    FXMVECTOR Q
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result = {
        -Q.vector4_f32[0],
        -Q.vector4_f32[1],
        -Q.vector4_f32[2],
        Q.vector4_f32[3]
    };
    return Result;
#elif defined(_XM_SSE_INTRINSICS_)
    static const XMVECTORF32 NegativeOne3 = {-1.0f,-1.0f,-1.0f,1.0f};
    XMVECTOR Result = _mm_mul_ps(Q,NegativeOne3);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionInverse
(
    FXMVECTOR Q
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR        Conjugate;
    XMVECTOR        L;
    XMVECTOR        Control;
    XMVECTOR        Result;
    CONST XMVECTOR  Zero = XMVectorZero();

    L = XMVector4LengthSq(Q);
    Conjugate = XMQuaternionConjugate(Q);

    Control = XMVectorLessOrEqual(L, g_XMEpsilon.v);

    L = XMVectorReciprocal(L);
    Result = XMVectorMultiply(Conjugate, L);

    Result = XMVectorSelect(Result, Zero, Control);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR        Conjugate;
    XMVECTOR        L;
    XMVECTOR        Control;
    XMVECTOR        Result;
    XMVECTOR  Zero = XMVectorZero();

    L = XMVector4LengthSq(Q);
    Conjugate = XMQuaternionConjugate(Q);
    Control = XMVectorLessOrEqual(L, g_XMEpsilon);
    Result = _mm_div_ps(Conjugate,L);
    Result = XMVectorSelect(Result, Zero, Control);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionLn
(
    FXMVECTOR Q
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Q0;
    XMVECTOR QW;
    XMVECTOR Theta;
    XMVECTOR SinTheta;
    XMVECTOR S;
    XMVECTOR ControlW;
    XMVECTOR Result;
    static CONST XMVECTOR OneMinusEpsilon = {1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f};

    QW = XMVectorSplatW(Q);
    Q0 = XMVectorSelect(g_XMSelect1110.v, Q, g_XMSelect1110.v);

    ControlW = XMVectorInBounds(QW, OneMinusEpsilon);

    Theta = XMVectorACos(QW);
    SinTheta = XMVectorSin(Theta);

    S = XMVectorReciprocal(SinTheta);
    S = XMVectorMultiply(Theta, S);

    Result = XMVectorMultiply(Q0, S);

    Result = XMVectorSelect(Q0, Result, ControlW);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 OneMinusEpsilon = {1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f};
    static CONST XMVECTORF32 NegOneMinusEpsilon = {-(1.0f - 0.00001f), -(1.0f - 0.00001f),-(1.0f - 0.00001f),-(1.0f - 0.00001f)};
    // Get W only
    XMVECTOR QW = _mm_shuffle_ps(Q,Q,_MM_SHUFFLE(3,3,3,3));
    // W = 0
    XMVECTOR Q0 = _mm_and_ps(Q,g_XMMask3);
    // Use W if within bounds
    XMVECTOR ControlW = _mm_cmple_ps(QW,OneMinusEpsilon);
    XMVECTOR vTemp2 = _mm_cmpge_ps(QW,NegOneMinusEpsilon);
    ControlW = _mm_and_ps(ControlW,vTemp2);
    // Get theta
    XMVECTOR vTheta = XMVectorACos(QW);
    // Get Sine of theta
    vTemp2 = XMVectorSin(vTheta);
    // theta/sine of theta
    vTheta = _mm_div_ps(vTheta,vTemp2);
    // Here's the answer
    vTheta = _mm_mul_ps(vTheta,Q0);
    // Was W in bounds? If not, return input as is
    vTheta = XMVectorSelect(Q0,vTheta,ControlW);
    return vTheta;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionExp
(
    FXMVECTOR Q
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Theta;
    XMVECTOR SinTheta;
    XMVECTOR CosTheta;
    XMVECTOR S;
    XMVECTOR Control;
    XMVECTOR Zero;
    XMVECTOR Result;

    Theta = XMVector3Length(Q);
    XMVectorSinCos(&SinTheta, &CosTheta, Theta);

    S = XMVectorReciprocal(Theta);
    S = XMVectorMultiply(SinTheta, S);

    Result = XMVectorMultiply(Q, S);

    Zero = XMVectorZero();
    Control = XMVectorNearEqual(Theta, Zero, g_XMEpsilon.v);
    Result = XMVectorSelect(Result, Q, Control);

    Result = XMVectorSelect(CosTheta, Result, g_XMSelect1110.v);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR Theta;
    XMVECTOR SinTheta;
    XMVECTOR CosTheta;
    XMVECTOR S;
    XMVECTOR Control;
    XMVECTOR Zero;
    XMVECTOR Result;
    Theta = XMVector3Length(Q);
    XMVectorSinCos(&SinTheta, &CosTheta, Theta);
    S = _mm_div_ps(SinTheta,Theta);
    Result = _mm_mul_ps(Q, S);
    Zero = XMVectorZero();
    Control = XMVectorNearEqual(Theta, Zero, g_XMEpsilon);
    Result = XMVectorSelect(Result,Q,Control);
    Result = _mm_and_ps(Result,g_XMMask3);
    CosTheta = _mm_and_ps(CosTheta,g_XMMaskW);
    Result = _mm_or_ps(Result,CosTheta);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMQuaternionSlerp
(
    FXMVECTOR Q0,
    FXMVECTOR Q1,
    FLOAT    t
)
{
    XMVECTOR T = XMVectorReplicate(t);
    return XMQuaternionSlerpV(Q0, Q1, T);
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMQuaternionSlerpV
(
    FXMVECTOR Q0,
    FXMVECTOR Q1,
    FXMVECTOR T
)
{
#if defined(_XM_NO_INTRINSICS_)

    // Result = Q0 * sin((1.0 - t) * Omega) / sin(Omega) + Q1 * sin(t * Omega) / sin(Omega)
    XMVECTOR Omega;
    XMVECTOR CosOmega;
    XMVECTOR SinOmega;
    XMVECTOR InvSinOmega;
    XMVECTOR V01;
    XMVECTOR C1000;
    XMVECTOR SignMask;
    XMVECTOR S0;
    XMVECTOR S1;
    XMVECTOR Sign;
    XMVECTOR Control;
    XMVECTOR Result;
    XMVECTOR Zero;
    CONST XMVECTOR OneMinusEpsilon = {1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f};

    XMASSERT((T.vector4_f32[1] == T.vector4_f32[0]) && (T.vector4_f32[2] == T.vector4_f32[0]) && (T.vector4_f32[3] == T.vector4_f32[0]));

    CosOmega = XMQuaternionDot(Q0, Q1);

    Zero = XMVectorZero();
    Control = XMVectorLess(CosOmega, Zero);
    Sign = XMVectorSelect(g_XMOne.v, g_XMNegativeOne.v, Control);

    CosOmega = XMVectorMultiply(CosOmega, Sign);

    Control = XMVectorLess(CosOmega, OneMinusEpsilon);

    SinOmega = XMVectorNegativeMultiplySubtract(CosOmega, CosOmega, g_XMOne.v);
    SinOmega = XMVectorSqrt(SinOmega);

    Omega = XMVectorATan2(SinOmega, CosOmega);

    SignMask = XMVectorSplatSignMask();
    C1000 = XMVectorSetBinaryConstant(1, 0, 0, 0);
    V01 = XMVectorShiftLeft(T, Zero, 2);
    SignMask = XMVectorShiftLeft(SignMask, Zero, 3);
    V01 = XMVectorXorInt(V01, SignMask);
    V01 = XMVectorAdd(C1000, V01);

    InvSinOmega = XMVectorReciprocal(SinOmega);

    S0 = XMVectorMultiply(V01, Omega);
    S0 = XMVectorSin(S0);
    S0 = XMVectorMultiply(S0, InvSinOmega);

    S0 = XMVectorSelect(V01, S0, Control);

    S1 = XMVectorSplatY(S0);
    S0 = XMVectorSplatX(S0);

    S1 = XMVectorMultiply(S1, Sign);

    Result = XMVectorMultiply(Q0, S0);
    Result = XMVectorMultiplyAdd(Q1, S1, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Result = Q0 * sin((1.0 - t) * Omega) / sin(Omega) + Q1 * sin(t * Omega) / sin(Omega)
    XMVECTOR Omega;
    XMVECTOR CosOmega;
    XMVECTOR SinOmega;
    XMVECTOR V01;
    XMVECTOR S0;
    XMVECTOR S1;
    XMVECTOR Sign;
    XMVECTOR Control;
    XMVECTOR Result;
    XMVECTOR Zero;
    static const XMVECTORF32 OneMinusEpsilon = {1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f, 1.0f - 0.00001f};
    static const XMVECTORI32 SignMask2 = {0x80000000,0x00000000,0x00000000,0x00000000};
    static const XMVECTORI32 MaskXY = {0xFFFFFFFF,0xFFFFFFFF,0x00000000,0x00000000};

    XMASSERT((XMVectorGetY(T) == XMVectorGetX(T)) && (XMVectorGetZ(T) == XMVectorGetX(T)) && (XMVectorGetW(T) == XMVectorGetX(T)));

    CosOmega = XMQuaternionDot(Q0, Q1);

    Zero = XMVectorZero();
    Control = XMVectorLess(CosOmega, Zero);
    Sign = XMVectorSelect(g_XMOne, g_XMNegativeOne, Control);

    CosOmega = _mm_mul_ps(CosOmega, Sign);

    Control = XMVectorLess(CosOmega, OneMinusEpsilon);

    SinOmega = _mm_mul_ps(CosOmega,CosOmega);
    SinOmega = _mm_sub_ps(g_XMOne,SinOmega);
    SinOmega = _mm_sqrt_ps(SinOmega);

    Omega = XMVectorATan2(SinOmega, CosOmega);

    V01 = _mm_shuffle_ps(T,T,_MM_SHUFFLE(2,3,0,1));
    V01 = _mm_and_ps(V01,MaskXY);
    V01 = _mm_xor_ps(V01,SignMask2);
    V01 = _mm_add_ps(g_XMIdentityR0, V01);

    S0 = _mm_mul_ps(V01, Omega);
    S0 = XMVectorSin(S0);
    S0 = _mm_div_ps(S0, SinOmega);

    S0 = XMVectorSelect(V01, S0, Control);

    S1 = XMVectorSplatY(S0);
    S0 = XMVectorSplatX(S0);

    S1 = _mm_mul_ps(S1, Sign);
    Result = _mm_mul_ps(Q0, S0);
    S1 = _mm_mul_ps(S1, Q1);
    Result = _mm_add_ps(Result,S1);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionSquad
(
    FXMVECTOR Q0,
    FXMVECTOR Q1,
    FXMVECTOR Q2,
    CXMVECTOR Q3,
    FLOAT    t
)
{
    XMVECTOR T = XMVectorReplicate(t);
    return XMQuaternionSquadV(Q0, Q1, Q2, Q3, T);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionSquadV
(
    FXMVECTOR Q0,
    FXMVECTOR Q1,
    FXMVECTOR Q2,
    CXMVECTOR Q3,
    CXMVECTOR T
)
{
    XMVECTOR Q03;
    XMVECTOR Q12;
    XMVECTOR TP;
    XMVECTOR Two;
    XMVECTOR Result;

    XMASSERT( (XMVectorGetY(T) == XMVectorGetX(T)) && (XMVectorGetZ(T) == XMVectorGetX(T)) && (XMVectorGetW(T) == XMVectorGetX(T)) );

    TP = T;
    Two = XMVectorSplatConstant(2, 0);

    Q03 = XMQuaternionSlerpV(Q0, Q3, T);
    Q12 = XMQuaternionSlerpV(Q1, Q2, T);

    TP = XMVectorNegativeMultiplySubtract(TP, TP, TP);
    TP = XMVectorMultiply(TP, Two);

    Result = XMQuaternionSlerpV(Q03, Q12, TP);

    return Result;

}

//------------------------------------------------------------------------------

XMINLINE VOID XMQuaternionSquadSetup
(
    XMVECTOR* pA,
    XMVECTOR* pB,
    XMVECTOR* pC,
    FXMVECTOR  Q0,
    FXMVECTOR  Q1,
    FXMVECTOR  Q2,
    CXMVECTOR  Q3
)
{
    XMVECTOR SQ0, SQ2, SQ3;
    XMVECTOR InvQ1, InvQ2;
    XMVECTOR LnQ0, LnQ1, LnQ2, LnQ3;
    XMVECTOR ExpQ02, ExpQ13;
    XMVECTOR LS01, LS12, LS23;
    XMVECTOR LD01, LD12, LD23;
    XMVECTOR Control0, Control1, Control2;
    XMVECTOR NegativeOneQuarter;

    XMASSERT(pA);
    XMASSERT(pB);
    XMASSERT(pC);

    LS12 = XMQuaternionLengthSq(XMVectorAdd(Q1, Q2));
    LD12 = XMQuaternionLengthSq(XMVectorSubtract(Q1, Q2));
    SQ2 = XMVectorNegate(Q2);

    Control1 = XMVectorLess(LS12, LD12);
    SQ2 = XMVectorSelect(Q2, SQ2, Control1);

    LS01 = XMQuaternionLengthSq(XMVectorAdd(Q0, Q1));
    LD01 = XMQuaternionLengthSq(XMVectorSubtract(Q0, Q1));
    SQ0 = XMVectorNegate(Q0);

    LS23 = XMQuaternionLengthSq(XMVectorAdd(SQ2, Q3));
    LD23 = XMQuaternionLengthSq(XMVectorSubtract(SQ2, Q3));
    SQ3 = XMVectorNegate(Q3);

    Control0 = XMVectorLess(LS01, LD01);
    Control2 = XMVectorLess(LS23, LD23);

    SQ0 = XMVectorSelect(Q0, SQ0, Control0);
    SQ3 = XMVectorSelect(Q3, SQ3, Control2);

    InvQ1 = XMQuaternionInverse(Q1);
    InvQ2 = XMQuaternionInverse(SQ2);

    LnQ0 = XMQuaternionLn(XMQuaternionMultiply(InvQ1, SQ0));
    LnQ2 = XMQuaternionLn(XMQuaternionMultiply(InvQ1, SQ2));
    LnQ1 = XMQuaternionLn(XMQuaternionMultiply(InvQ2, Q1));
    LnQ3 = XMQuaternionLn(XMQuaternionMultiply(InvQ2, SQ3));

    NegativeOneQuarter = XMVectorSplatConstant(-1, 2);

    ExpQ02 = XMVectorMultiply(XMVectorAdd(LnQ0, LnQ2), NegativeOneQuarter);
    ExpQ13 = XMVectorMultiply(XMVectorAdd(LnQ1, LnQ3), NegativeOneQuarter);
    ExpQ02 = XMQuaternionExp(ExpQ02);
    ExpQ13 = XMQuaternionExp(ExpQ13);

    *pA = XMQuaternionMultiply(Q1, ExpQ02);
    *pB = XMQuaternionMultiply(SQ2, ExpQ13);
    *pC = SQ2;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionBaryCentric
(
    FXMVECTOR Q0,
    FXMVECTOR Q1,
    FXMVECTOR Q2,
    FLOAT    f,
    FLOAT    g
)
{
    XMVECTOR Q01;
    XMVECTOR Q02;
    FLOAT    s;
    XMVECTOR Result;

    s = f + g;

    if ((s < 0.00001f) && (s > -0.00001f))
    {
        Result = Q0;
    }
    else
    {
        Q01 = XMQuaternionSlerp(Q0, Q1, s);
        Q02 = XMQuaternionSlerp(Q0, Q2, s);

        Result = XMQuaternionSlerp(Q01, Q02, g / s);
    }

    return Result;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionBaryCentricV
(
    FXMVECTOR Q0,
    FXMVECTOR Q1,
    FXMVECTOR Q2,
    CXMVECTOR F,
    CXMVECTOR G
)
{
    XMVECTOR Q01;
    XMVECTOR Q02;
    XMVECTOR S, GS;
    XMVECTOR Epsilon;
    XMVECTOR Result;

    XMASSERT( (XMVectorGetY(F) == XMVectorGetX(F)) && (XMVectorGetZ(F) == XMVectorGetX(F)) && (XMVectorGetW(F) == XMVectorGetX(F)) );
    XMASSERT( (XMVectorGetY(G) == XMVectorGetX(G)) && (XMVectorGetZ(G) == XMVectorGetX(G)) && (XMVectorGetW(G) == XMVectorGetX(G)) );

    Epsilon = XMVectorSplatConstant(1, 16);

    S = XMVectorAdd(F, G);

    if (XMVector4InBounds(S, Epsilon))
    {
        Result = Q0;
    }
    else
    {
        Q01 = XMQuaternionSlerpV(Q0, Q1, S);
        Q02 = XMQuaternionSlerpV(Q0, Q2, S);
        GS = XMVectorReciprocal(S);
        GS = XMVectorMultiply(G, GS);

        Result = XMQuaternionSlerpV(Q01, Q02, GS);
    }

    return Result;
}

//------------------------------------------------------------------------------
// Transformation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionIdentity()
{
#if defined(_XM_NO_INTRINSICS_)
    return g_XMIdentityR3.v;
#elif defined(_XM_SSE_INTRINSICS_)
    return g_XMIdentityR3;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionRotationRollPitchYaw
(
    FLOAT Pitch,
    FLOAT Yaw,
    FLOAT Roll
)
{
    XMVECTOR Angles;
    XMVECTOR Q;

    Angles = XMVectorSet(Pitch, Yaw, Roll, 0.0f);
    Q = XMQuaternionRotationRollPitchYawFromVector(Angles);

    return Q;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionRotationRollPitchYawFromVector
(
    FXMVECTOR Angles // <Pitch, Yaw, Roll, 0>
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR                Q, Q0, Q1;
    XMVECTOR                P0, P1, Y0, Y1, R0, R1;
    XMVECTOR                HalfAngles;
    XMVECTOR                SinAngles, CosAngles;
    static CONST XMVECTORU32 ControlPitch = {XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1X, XM_PERMUTE_1X};
    static CONST XMVECTORU32 ControlYaw = {XM_PERMUTE_1Y, XM_PERMUTE_0Y, XM_PERMUTE_1Y, XM_PERMUTE_1Y};
    static CONST XMVECTORU32 ControlRoll = {XM_PERMUTE_1Z, XM_PERMUTE_1Z, XM_PERMUTE_0Z, XM_PERMUTE_1Z};
    static CONST XMVECTOR   Sign = {1.0f, -1.0f, -1.0f, 1.0f};

    HalfAngles = XMVectorMultiply(Angles, g_XMOneHalf.v);
    XMVectorSinCos(&SinAngles, &CosAngles, HalfAngles);

    P0 = XMVectorPermute(SinAngles, CosAngles, ControlPitch.v);
    Y0 = XMVectorPermute(SinAngles, CosAngles, ControlYaw.v);
    R0 = XMVectorPermute(SinAngles, CosAngles, ControlRoll.v);
    P1 = XMVectorPermute(CosAngles, SinAngles, ControlPitch.v);
    Y1 = XMVectorPermute(CosAngles, SinAngles, ControlYaw.v);
    R1 = XMVectorPermute(CosAngles, SinAngles, ControlRoll.v);

    Q1 = XMVectorMultiply(P1, Sign);
    Q0 = XMVectorMultiply(P0, Y0);
    Q1 = XMVectorMultiply(Q1, Y1);
    Q0 = XMVectorMultiply(Q0, R0);
    Q = XMVectorMultiplyAdd(Q1, R1, Q0);

    return Q;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR                Q, Q0, Q1;
    XMVECTOR                P0, P1, Y0, Y1, R0, R1;
    XMVECTOR                HalfAngles;
    XMVECTOR                SinAngles, CosAngles;
    static CONST XMVECTORI32 ControlPitch = {XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1X, XM_PERMUTE_1X};
    static CONST XMVECTORI32 ControlYaw = {XM_PERMUTE_1Y, XM_PERMUTE_0Y, XM_PERMUTE_1Y, XM_PERMUTE_1Y};
    static CONST XMVECTORI32 ControlRoll = {XM_PERMUTE_1Z, XM_PERMUTE_1Z, XM_PERMUTE_0Z, XM_PERMUTE_1Z};
    static CONST XMVECTORF32 Sign = {1.0f, -1.0f, -1.0f, 1.0f};

    HalfAngles = _mm_mul_ps(Angles, g_XMOneHalf);
    XMVectorSinCos(&SinAngles, &CosAngles, HalfAngles);

    P0 = XMVectorPermute(SinAngles, CosAngles, ControlPitch);
    Y0 = XMVectorPermute(SinAngles, CosAngles, ControlYaw);
    R0 = XMVectorPermute(SinAngles, CosAngles, ControlRoll);
    P1 = XMVectorPermute(CosAngles, SinAngles, ControlPitch);
    Y1 = XMVectorPermute(CosAngles, SinAngles, ControlYaw);
    R1 = XMVectorPermute(CosAngles, SinAngles, ControlRoll);

    Q1 = _mm_mul_ps(P1, Sign);
    Q0 = _mm_mul_ps(P0, Y0);
    Q1 = _mm_mul_ps(Q1, Y1);
    Q0 = _mm_mul_ps(Q0, R0);
    Q = _mm_mul_ps(Q1, R1);
    Q = _mm_add_ps(Q,Q0);
    return Q;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionRotationNormal
(
    FXMVECTOR NormalAxis,
    FLOAT    Angle
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Q;
    XMVECTOR N;
    XMVECTOR Scale;

    N = XMVectorSelect(g_XMOne.v, NormalAxis, g_XMSelect1110.v);

    XMScalarSinCos(&Scale.vector4_f32[2], &Scale.vector4_f32[3], 0.5f * Angle);

    Scale.vector4_f32[0] = Scale.vector4_f32[1] = Scale.vector4_f32[2];

    Q = XMVectorMultiply(N, Scale);

    return Q;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR N = _mm_and_ps(NormalAxis,g_XMMask3);
    N = _mm_or_ps(N,g_XMIdentityR3);
    XMVECTOR Scale = _mm_set_ps1(0.5f * Angle);
    XMVECTOR vSine;
    XMVECTOR vCosine;
    XMVectorSinCos(&vSine,&vCosine,Scale);
    Scale = _mm_and_ps(vSine,g_XMMask3);
    vCosine = _mm_and_ps(vCosine,g_XMMaskW);
    Scale = _mm_or_ps(Scale,vCosine);
    N = _mm_mul_ps(N,Scale);
    return N;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMQuaternionRotationAxis
(
    FXMVECTOR Axis,
    FLOAT    Angle
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Normal;
    XMVECTOR Q;

    XMASSERT(!XMVector3Equal(Axis, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(Axis));

    Normal = XMVector3Normalize(Axis);
    Q = XMQuaternionRotationNormal(Normal, Angle);

    return Q;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR Normal;
    XMVECTOR Q;

    XMASSERT(!XMVector3Equal(Axis, XMVectorZero()));
    XMASSERT(!XMVector3IsInfinite(Axis));

    Normal = XMVector3Normalize(Axis);
    Q = XMQuaternionRotationNormal(Normal, Angle);
    return Q;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMQuaternionRotationMatrix
(
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(_XM_SSE_INTRINSICS_)

    XMVECTOR Q0, Q1, Q2;
    XMVECTOR M00, M11, M22;
    XMVECTOR CQ0, CQ1, C;
    XMVECTOR CX, CY, CZ, CW;
    XMVECTOR SQ1, Scale;
    XMVECTOR Rsq, Sqrt, VEqualsNaN;
    XMVECTOR A, B, P;
    XMVECTOR PermuteSplat, PermuteSplatT;
    XMVECTOR SignB, SignBT;
    XMVECTOR PermuteControl, PermuteControlT;
    XMVECTOR Result;
    static CONST XMVECTORF32 OneQuarter = {0.25f, 0.25f, 0.25f, 0.25f};
    static CONST XMVECTORF32 SignPNNP = {1.0f, -1.0f, -1.0f, 1.0f};
    static CONST XMVECTORF32 SignNPNP = {-1.0f, 1.0f, -1.0f, 1.0f};
    static CONST XMVECTORF32 SignNNPP = {-1.0f, -1.0f, 1.0f, 1.0f};
    static CONST XMVECTORF32 SignPNPP = {1.0f, -1.0f, 1.0f, 1.0f};
    static CONST XMVECTORF32 SignPPNP = {1.0f, 1.0f, -1.0f, 1.0f};
    static CONST XMVECTORF32 SignNPPP = {-1.0f, 1.0f, 1.0f, 1.0f};
    static CONST XMVECTORU32 Permute0X0X0Y0W = {XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute0Y0Z0Z1W = {XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0Z, XM_PERMUTE_1W};
    static CONST XMVECTORU32 SplatX = {XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0X};
    static CONST XMVECTORU32 SplatY = {XM_PERMUTE_0Y, XM_PERMUTE_0Y, XM_PERMUTE_0Y, XM_PERMUTE_0Y};
    static CONST XMVECTORU32 SplatZ = {XM_PERMUTE_0Z, XM_PERMUTE_0Z, XM_PERMUTE_0Z, XM_PERMUTE_0Z};
    static CONST XMVECTORU32 SplatW = {XM_PERMUTE_0W, XM_PERMUTE_0W, XM_PERMUTE_0W, XM_PERMUTE_0W};
    static CONST XMVECTORU32 PermuteC = {XM_PERMUTE_0X, XM_PERMUTE_0Z, XM_PERMUTE_1X, XM_PERMUTE_1Y};
    static CONST XMVECTORU32 PermuteA = {XM_PERMUTE_0Y, XM_PERMUTE_1Y, XM_PERMUTE_1Z, XM_PERMUTE_0W};
    static CONST XMVECTORU32 PermuteB = {XM_PERMUTE_1X, XM_PERMUTE_1W, XM_PERMUTE_0Z, XM_PERMUTE_0W};
    static CONST XMVECTORU32 Permute0 = {XM_PERMUTE_0X, XM_PERMUTE_1X, XM_PERMUTE_1Z, XM_PERMUTE_1Y};
    static CONST XMVECTORU32 Permute1 = {XM_PERMUTE_1X, XM_PERMUTE_0Y, XM_PERMUTE_1Y, XM_PERMUTE_1Z};
    static CONST XMVECTORU32 Permute2 = {XM_PERMUTE_1Z, XM_PERMUTE_1Y, XM_PERMUTE_0Z, XM_PERMUTE_1X};
    static CONST XMVECTORU32 Permute3 = {XM_PERMUTE_1Y, XM_PERMUTE_1Z, XM_PERMUTE_1X, XM_PERMUTE_0W};

    M00 = XMVectorSplatX(M.r[0]);
    M11 = XMVectorSplatY(M.r[1]);
    M22 = XMVectorSplatZ(M.r[2]);

    Q0 = XMVectorMultiply(SignPNNP.v, M00);
    Q0 = XMVectorMultiplyAdd(SignNPNP.v, M11, Q0);
    Q0 = XMVectorMultiplyAdd(SignNNPP.v, M22, Q0);

    Q1 = XMVectorAdd(Q0, g_XMOne.v);

    Rsq = XMVectorReciprocalSqrt(Q1);
    VEqualsNaN = XMVectorIsNaN(Rsq);
    Sqrt = XMVectorMultiply(Q1, Rsq);
    Q1 = XMVectorSelect(Sqrt, Q1, VEqualsNaN);

    Q1 = XMVectorMultiply(Q1, g_XMOneHalf.v);

    SQ1 = XMVectorMultiply(Rsq, g_XMOneHalf.v);

    CQ0 = XMVectorPermute(Q0, Q0, Permute0X0X0Y0W.v);
    CQ1 = XMVectorPermute(Q0, g_XMEpsilon.v, Permute0Y0Z0Z1W.v);
    C = XMVectorGreaterOrEqual(CQ0, CQ1);

    CX = XMVectorSplatX(C);
    CY = XMVectorSplatY(C);
    CZ = XMVectorSplatZ(C);
    CW = XMVectorSplatW(C);

    PermuteSplat = XMVectorSelect(SplatZ.v, SplatY.v, CZ);
    SignB = XMVectorSelect(SignNPPP.v, SignPPNP.v, CZ);
    PermuteControl = XMVectorSelect(Permute2.v, Permute1.v, CZ);

    PermuteSplat = XMVectorSelect(PermuteSplat, SplatZ.v, CX);
    SignB = XMVectorSelect(SignB, SignNPPP.v, CX);
    PermuteControl = XMVectorSelect(PermuteControl, Permute2.v, CX);

    PermuteSplatT = XMVectorSelect(PermuteSplat,SplatX.v, CY);
    SignBT = XMVectorSelect(SignB, SignPNPP.v, CY);
    PermuteControlT = XMVectorSelect(PermuteControl,Permute0.v, CY);

    PermuteSplat = XMVectorSelect(PermuteSplat, PermuteSplatT, CX);
    SignB = XMVectorSelect(SignB, SignBT, CX);
    PermuteControl = XMVectorSelect(PermuteControl, PermuteControlT, CX);

    PermuteSplat = XMVectorSelect(PermuteSplat,SplatW.v, CW);
    SignB = XMVectorSelect(SignB, g_XMNegativeOne.v, CW);
    PermuteControl = XMVectorSelect(PermuteControl,Permute3.v, CW);

    Scale = XMVectorPermute(SQ1, SQ1, PermuteSplat);

    P = XMVectorPermute(M.r[1], M.r[2],PermuteC.v);  // {M10, M12, M20, M21}
    A = XMVectorPermute(M.r[0], P, PermuteA.v);       // {M01, M12, M20, M03}
    B = XMVectorPermute(M.r[0], P, PermuteB.v);       // {M10, M21, M02, M03}

    Q2 = XMVectorMultiplyAdd(SignB, B, A);
    Q2 = XMVectorMultiply(Q2, Scale);

    Result = XMVectorPermute(Q1, Q2, PermuteControl);

    return Result;

#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Conversion operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE VOID XMQuaternionToAxisAngle
(
    XMVECTOR* pAxis,
    FLOAT*    pAngle,
    FXMVECTOR  Q
)
{
    XMASSERT(pAxis);
    XMASSERT(pAngle);

    *pAxis = Q;

#if defined(_XM_SSE_INTRINSICS_) && !defined(_XM_NO_INTRINSICS_)
    *pAngle = 2.0f * acosf(XMVectorGetW(Q));
#else
    *pAngle = 2.0f * XMScalarACos(XMVectorGetW(Q));
#endif
}

/****************************************************************************
 *
 * Plane
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE BOOL XMPlaneEqual
(
    FXMVECTOR P1,
    FXMVECTOR P2
)
{
    return XMVector4Equal(P1, P2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMPlaneNearEqual
(
    FXMVECTOR P1,
    FXMVECTOR P2,
    FXMVECTOR Epsilon
)
{
    XMVECTOR NP1 = XMPlaneNormalize(P1);
    XMVECTOR NP2 = XMPlaneNormalize(P2);
    return XMVector4NearEqual(NP1, NP2, Epsilon);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMPlaneNotEqual
(
    FXMVECTOR P1,
    FXMVECTOR P2
)
{
    return XMVector4NotEqual(P1, P2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMPlaneIsNaN
(
    FXMVECTOR P
)
{
    return XMVector4IsNaN(P);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMPlaneIsInfinite
(
    FXMVECTOR P
)
{
    return XMVector4IsInfinite(P);
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneDot
(
    FXMVECTOR P,
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    return XMVector4Dot(P, V);

#elif defined(_XM_SSE_INTRINSICS_)
    __m128 vTemp2 = V;
    __m128 vTemp = _mm_mul_ps(P,vTemp2);
    vTemp2 = _mm_shuffle_ps(vTemp2,vTemp,_MM_SHUFFLE(1,0,0,0)); // Copy X to the Z position and Y to the W position
    vTemp2 = _mm_add_ps(vTemp2,vTemp);          // Add Z = X+Z; W = Y+W;
    vTemp = _mm_shuffle_ps(vTemp,vTemp2,_MM_SHUFFLE(0,3,0,0));  // Copy W to the Z position
    vTemp = _mm_add_ps(vTemp,vTemp2);           // Add Z and W together
    return _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(2,2,2,2));    // Splat Z and return
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneDotCoord
(
    FXMVECTOR P,
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V3;
    XMVECTOR Result;

    // Result = P[0] * V[0] + P[1] * V[1] + P[2] * V[2] + P[3]
    V3 = XMVectorSelect(g_XMOne.v, V, g_XMSelect1110.v);
    Result = XMVector4Dot(P, V3);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp2 = _mm_and_ps(V,g_XMMask3);
    vTemp2 = _mm_or_ps(vTemp2,g_XMIdentityR3);
    XMVECTOR vTemp = _mm_mul_ps(P,vTemp2);
    vTemp2 = _mm_shuffle_ps(vTemp2,vTemp,_MM_SHUFFLE(1,0,0,0)); // Copy X to the Z position and Y to the W position
    vTemp2 = _mm_add_ps(vTemp2,vTemp);          // Add Z = X+Z; W = Y+W;
    vTemp = _mm_shuffle_ps(vTemp,vTemp2,_MM_SHUFFLE(0,3,0,0));  // Copy W to the Z position
    vTemp = _mm_add_ps(vTemp,vTemp2);           // Add Z and W together
    return _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(2,2,2,2));    // Splat Z and return
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneDotNormal
(
    FXMVECTOR P,
    FXMVECTOR V
)
{
    return XMVector3Dot(P, V);
}

//------------------------------------------------------------------------------
// XMPlaneNormalizeEst uses a reciprocal estimate and
// returns QNaN on zero and infinite vectors.

XMFINLINE XMVECTOR XMPlaneNormalizeEst
(
    FXMVECTOR P
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result = XMVector3ReciprocalLength(P);
    Result = XMVectorMultiply(P, Result);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product
    XMVECTOR vDot = _mm_mul_ps(P,P);
    // x=Dot.y, y=Dot.z
    XMVECTOR vTemp = _mm_shuffle_ps(vDot,vDot,_MM_SHUFFLE(2,1,2,1));
    // Result.x = x+y
    vDot = _mm_add_ss(vDot,vTemp);
    // x=Dot.z
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(1,1,1,1));
    // Result.x = (x+y)+z
    vDot = _mm_add_ss(vDot,vTemp);
    // Splat x
	vDot = _mm_shuffle_ps(vDot,vDot,_MM_SHUFFLE(0,0,0,0));
    // Get the reciprocal
    vDot = _mm_rsqrt_ps(vDot);
    // Get the reciprocal
    vDot = _mm_mul_ps(vDot,P);
    return vDot;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneNormalize
(
    FXMVECTOR P
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fLengthSq = sqrtf((P.vector4_f32[0]*P.vector4_f32[0])+(P.vector4_f32[1]*P.vector4_f32[1])+(P.vector4_f32[2]*P.vector4_f32[2]));
    // Prevent divide by zero
    if (fLengthSq) {
        fLengthSq = 1.0f/fLengthSq;
    }
    {
    XMVECTOR vResult = {
        P.vector4_f32[0]*fLengthSq,
        P.vector4_f32[1]*fLengthSq,
        P.vector4_f32[2]*fLengthSq,
        P.vector4_f32[3]*fLengthSq
    };
    return vResult;
    }
#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y and z only
    XMVECTOR vLengthSq = _mm_mul_ps(P,P);
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,1,2,1));
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(1,1,1,1));
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    // Prepare for the division
    XMVECTOR vResult = _mm_sqrt_ps(vLengthSq);
    // Failsafe on zero (Or epsilon) length planes
    // If the length is infinity, set the elements to zero
    vLengthSq = _mm_cmpneq_ps(vLengthSq,g_XMInfinity);
    // Reciprocal mul to perform the normalization
    vResult = _mm_div_ps(P,vResult);
    // Any that are infinity, set to zero
    vResult = _mm_and_ps(vResult,vLengthSq);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneIntersectLine
(
    FXMVECTOR P,
    FXMVECTOR LinePoint1,
    FXMVECTOR LinePoint2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1;
    XMVECTOR V2;
    XMVECTOR D;
    XMVECTOR ReciprocalD;
    XMVECTOR VT;
    XMVECTOR Point;
    XMVECTOR Zero;
    XMVECTOR Control;
    XMVECTOR Result;

    V1 = XMVector3Dot(P, LinePoint1);
    V2 = XMVector3Dot(P, LinePoint2);
    D = XMVectorSubtract(V1, V2);

    ReciprocalD = XMVectorReciprocal(D);
    VT = XMPlaneDotCoord(P, LinePoint1);
    VT = XMVectorMultiply(VT, ReciprocalD);

    Point = XMVectorSubtract(LinePoint2, LinePoint1);
    Point = XMVectorMultiplyAdd(Point, VT, LinePoint1);

    Zero = XMVectorZero();
    Control = XMVectorNearEqual(D, Zero, g_XMEpsilon.v);

    Result = XMVectorSelect(Point, g_XMQNaN.v, Control);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR V1;
    XMVECTOR V2;
    XMVECTOR D;
    XMVECTOR VT;
    XMVECTOR Point;
    XMVECTOR Zero;
    XMVECTOR Control;
    XMVECTOR Result;

    V1 = XMVector3Dot(P, LinePoint1);
    V2 = XMVector3Dot(P, LinePoint2);
    D = _mm_sub_ps(V1, V2);

    VT = XMPlaneDotCoord(P, LinePoint1);
    VT = _mm_div_ps(VT, D);

    Point = _mm_sub_ps(LinePoint2, LinePoint1);
    Point = _mm_mul_ps(Point,VT);
    Point = _mm_add_ps(Point,LinePoint1);
    Zero = XMVectorZero();
    Control = XMVectorNearEqual(D, Zero, g_XMEpsilon);
    Result = XMVectorSelect(Point, g_XMQNaN, Control);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE VOID XMPlaneIntersectPlane
(
    XMVECTOR* pLinePoint1,
    XMVECTOR* pLinePoint2,
    FXMVECTOR  P1,
    FXMVECTOR  P2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1;
    XMVECTOR V2;
    XMVECTOR V3;
    XMVECTOR LengthSq;
    XMVECTOR RcpLengthSq;
    XMVECTOR Point;
    XMVECTOR P1W;
    XMVECTOR P2W;
    XMVECTOR Control;
    XMVECTOR LinePoint1;
    XMVECTOR LinePoint2;

    XMASSERT(pLinePoint1);
    XMASSERT(pLinePoint2);

    V1 = XMVector3Cross(P2, P1);

    LengthSq = XMVector3LengthSq(V1);

    V2 = XMVector3Cross(P2, V1);

    P1W = XMVectorSplatW(P1);
    Point = XMVectorMultiply(V2, P1W);

    V3 = XMVector3Cross(V1, P1);

    P2W = XMVectorSplatW(P2);
    Point = XMVectorMultiplyAdd(V3, P2W, Point);

    RcpLengthSq = XMVectorReciprocal(LengthSq);
    LinePoint1 = XMVectorMultiply(Point, RcpLengthSq);

    LinePoint2 = XMVectorAdd(LinePoint1, V1);

    Control = XMVectorLessOrEqual(LengthSq, g_XMEpsilon.v);
    *pLinePoint1 = XMVectorSelect(LinePoint1,g_XMQNaN.v, Control);
    *pLinePoint2 = XMVectorSelect(LinePoint2,g_XMQNaN.v, Control);

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pLinePoint1);
    XMASSERT(pLinePoint2);
    XMVECTOR V1;
    XMVECTOR V2;
    XMVECTOR V3;
    XMVECTOR LengthSq;
    XMVECTOR Point;
    XMVECTOR P1W;
    XMVECTOR P2W;
    XMVECTOR Control;
    XMVECTOR LinePoint1;
    XMVECTOR LinePoint2;

    V1 = XMVector3Cross(P2, P1);

    LengthSq = XMVector3LengthSq(V1);

    V2 = XMVector3Cross(P2, V1);

    P1W = _mm_shuffle_ps(P1,P1,_MM_SHUFFLE(3,3,3,3));
    Point = _mm_mul_ps(V2, P1W);

    V3 = XMVector3Cross(V1, P1);

    P2W = _mm_shuffle_ps(P2,P2,_MM_SHUFFLE(3,3,3,3));
    V3 = _mm_mul_ps(V3,P2W);
    Point = _mm_add_ps(Point,V3);
    LinePoint1 = _mm_div_ps(Point,LengthSq);

    LinePoint2 = _mm_add_ps(LinePoint1, V1);

    Control = XMVectorLessOrEqual(LengthSq, g_XMEpsilon);
    *pLinePoint1 = XMVectorSelect(LinePoint1,g_XMQNaN, Control);
    *pLinePoint2 = XMVectorSelect(LinePoint2,g_XMQNaN, Control);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneTransform
(
    FXMVECTOR P,
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR W;
    XMVECTOR Result;

    W = XMVectorSplatW(P);
    Z = XMVectorSplatZ(P);
    Y = XMVectorSplatY(P);
    X = XMVectorSplatX(P);

    Result = XMVectorMultiply(W, M.r[3]);
    Result = XMVectorMultiplyAdd(Z, M.r[2], Result);
    Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
    Result = XMVectorMultiplyAdd(X, M.r[0], Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR X = _mm_shuffle_ps(P,P,_MM_SHUFFLE(0,0,0,0));
    XMVECTOR Y = _mm_shuffle_ps(P,P,_MM_SHUFFLE(1,1,1,1));
    XMVECTOR Z = _mm_shuffle_ps(P,P,_MM_SHUFFLE(2,2,2,2));
    XMVECTOR W = _mm_shuffle_ps(P,P,_MM_SHUFFLE(3,3,3,3));
    X = _mm_mul_ps(X, M.r[0]);
    Y = _mm_mul_ps(Y, M.r[1]);
    Z = _mm_mul_ps(Z, M.r[2]);
    W = _mm_mul_ps(W, M.r[3]);
    X = _mm_add_ps(X,Z);
    Y = _mm_add_ps(Y,W);
    X = _mm_add_ps(X,Y);
    return X;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMFLOAT4* XMPlaneTransformStream
(
    XMFLOAT4*       pOutputStream,
    UINT            OutputStride,
    CONST XMFLOAT4* pInputStream,
    UINT            InputStride,
    UINT            PlaneCount,
    CXMMATRIX     M
)
{
    return XMVector4TransformStream(pOutputStream,
                                    OutputStride,
                                    pInputStream,
                                    InputStride,
                                    PlaneCount,
                                    M);
}

//------------------------------------------------------------------------------
// Conversion operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneFromPointNormal
(
    FXMVECTOR Point,
    FXMVECTOR Normal
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR W;
    XMVECTOR Result;

    W = XMVector3Dot(Point, Normal);
    W = XMVectorNegate(W);
    Result = XMVectorSelect(W, Normal, g_XMSelect1110.v);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR W;
    XMVECTOR Result;
    W = XMVector3Dot(Point,Normal);
    W = _mm_mul_ps(W,g_XMNegativeOne);
    Result = _mm_and_ps(Normal,g_XMMask3);
    W = _mm_and_ps(W,g_XMMaskW);
    Result = _mm_or_ps(Result,W);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMPlaneFromPoints
(
    FXMVECTOR Point1,
    FXMVECTOR Point2,
    FXMVECTOR Point3
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR N;
    XMVECTOR D;
    XMVECTOR V21;
    XMVECTOR V31;
    XMVECTOR Result;

    V21 = XMVectorSubtract(Point1, Point2);
    V31 = XMVectorSubtract(Point1, Point3);

    N = XMVector3Cross(V21, V31);
    N = XMVector3Normalize(N);

    D = XMPlaneDotNormal(N, Point1);
    D = XMVectorNegate(D);

    Result = XMVectorSelect(D, N, g_XMSelect1110.v);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR N;
    XMVECTOR D;
    XMVECTOR V21;
    XMVECTOR V31;
    XMVECTOR Result;

    V21 = _mm_sub_ps(Point1, Point2);
    V31 = _mm_sub_ps(Point1, Point3);

    N = XMVector3Cross(V21, V31);
    N = XMVector3Normalize(N);

    D = XMPlaneDotNormal(N, Point1);
    D = _mm_mul_ps(D,g_XMNegativeOne);
    N = _mm_and_ps(N,g_XMMask3);
    D = _mm_and_ps(D,g_XMMaskW);
    Result = _mm_or_ps(D,N);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

/****************************************************************************
 *
 * Color
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorEqual
(
    FXMVECTOR C1,
    FXMVECTOR C2
)
{
    return XMVector4Equal(C1, C2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorNotEqual
(
    FXMVECTOR C1,
    FXMVECTOR C2
)
{
    return XMVector4NotEqual(C1, C2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorGreater
(
    FXMVECTOR C1,
    FXMVECTOR C2
)
{
    return XMVector4Greater(C1, C2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorGreaterOrEqual
(
    FXMVECTOR C1,
    FXMVECTOR C2
)
{
    return XMVector4GreaterOrEqual(C1, C2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorLess
(
    FXMVECTOR C1,
    FXMVECTOR C2
)
{
    return XMVector4Less(C1, C2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorLessOrEqual
(
    FXMVECTOR C1,
    FXMVECTOR C2
)
{
    return XMVector4LessOrEqual(C1, C2);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorIsNaN
(
    FXMVECTOR C
)
{
    return XMVector4IsNaN(C);
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMColorIsInfinite
(
    FXMVECTOR C
)
{
    return XMVector4IsInfinite(C);
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMColorNegative
(
    FXMVECTOR vColor
)
{
#if defined(_XM_NO_INTRINSICS_)
//    XMASSERT(XMVector4GreaterOrEqual(C, XMVectorReplicate(0.0f)));
//    XMASSERT(XMVector4LessOrEqual(C, XMVectorReplicate(1.0f)));
    XMVECTOR vResult = {
        1.0f - vColor.vector4_f32[0],
        1.0f - vColor.vector4_f32[1],
        1.0f - vColor.vector4_f32[2],
        vColor.vector4_f32[3]
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    // Negate only x,y and z.
    XMVECTOR vTemp = _mm_xor_ps(vColor,g_XMNegate3);
    // Add 1,1,1,0 to -x,-y,-z,w
	return _mm_add_ps(vTemp,g_XMOne3);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMColorModulate
(
    FXMVECTOR C1,
    FXMVECTOR C2
)
{
    return XMVectorMultiply(C1, C2);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMColorAdjustSaturation
(
    FXMVECTOR vColor,
    FLOAT    fSaturation
)
{
#if defined(_XM_NO_INTRINSICS_)
    CONST XMVECTOR gvLuminance = {0.2125f, 0.7154f, 0.0721f, 0.0f};

    // Luminance = 0.2125f * C[0] + 0.7154f * C[1] + 0.0721f * C[2];
    // Result = (C - Luminance) * Saturation + Luminance;

    FLOAT fLuminance = (vColor.vector4_f32[0]*gvLuminance.vector4_f32[0])+(vColor.vector4_f32[1]*gvLuminance.vector4_f32[1])+(vColor.vector4_f32[2]*gvLuminance.vector4_f32[2]);
    XMVECTOR vResult = {
        ((vColor.vector4_f32[0] - fLuminance)*fSaturation)+fLuminance,
        ((vColor.vector4_f32[1] - fLuminance)*fSaturation)+fLuminance,
        ((vColor.vector4_f32[2] - fLuminance)*fSaturation)+fLuminance,
        vColor.vector4_f32[3]};
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    static const XMVECTORF32 gvLuminance = {0.2125f, 0.7154f, 0.0721f, 0.0f};
// Mul RGB by intensity constants
    XMVECTOR vLuminance = _mm_mul_ps(vColor,gvLuminance);
// vResult.x = vLuminance.y, vResult.y = vLuminance.y,
// vResult.z = vLuminance.z, vResult.w = vLuminance.z
    XMVECTOR vResult = vLuminance;
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(2,2,1,1));
// vLuminance.x += vLuminance.y
    vLuminance = _mm_add_ss(vLuminance,vResult);
// Splat vLuminance.z
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(2,2,2,2));
// vLuminance.x += vLuminance.z (Dot product)
    vLuminance = _mm_add_ss(vLuminance,vResult);
// Splat vLuminance
    vLuminance = _mm_shuffle_ps(vLuminance,vLuminance,_MM_SHUFFLE(0,0,0,0));
// Splat fSaturation
    XMVECTOR vSaturation = _mm_set_ps1(fSaturation);
// vResult = ((vColor-vLuminance)*vSaturation)+vLuminance;
    vResult = _mm_sub_ps(vColor,vLuminance);
    vResult = _mm_mul_ps(vResult,vSaturation);
    vResult = _mm_add_ps(vResult,vLuminance);
// Retain w from the source color
    vLuminance = _mm_shuffle_ps(vResult,vColor,_MM_SHUFFLE(3,2,2,2));   // x = vResult.z,y = vResult.z,z = vColor.z,w=vColor.w
    vResult = _mm_shuffle_ps(vResult,vLuminance,_MM_SHUFFLE(3,0,1,0));  // x = vResult.x,y = vResult.y,z = vResult.z,w=vColor.w
    return vResult;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMColorAdjustContrast
(
    FXMVECTOR vColor,
    FLOAT    fContrast
)
{
#if defined(_XM_NO_INTRINSICS_)
    // Result = (vColor - 0.5f) * fContrast + 0.5f;
    XMVECTOR vResult = {
        ((vColor.vector4_f32[0]-0.5f) * fContrast) + 0.5f,
        ((vColor.vector4_f32[1]-0.5f) * fContrast) + 0.5f,
        ((vColor.vector4_f32[2]-0.5f) * fContrast) + 0.5f,
        vColor.vector4_f32[3]        // Leave W untouched
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vScale = _mm_set_ps1(fContrast);           // Splat the scale
    XMVECTOR vResult = _mm_sub_ps(vColor,g_XMOneHalf);  // Subtract 0.5f from the source (Saving source)
    vResult = _mm_mul_ps(vResult,vScale);               // Mul by scale
    vResult = _mm_add_ps(vResult,g_XMOneHalf);          // Add 0.5f
// Retain w from the source color
    vScale = _mm_shuffle_ps(vResult,vColor,_MM_SHUFFLE(3,2,2,2));   // x = vResult.z,y = vResult.z,z = vColor.z,w=vColor.w
    vResult = _mm_shuffle_ps(vResult,vScale,_MM_SHUFFLE(3,0,1,0));  // x = vResult.x,y = vResult.y,z = vResult.z,w=vColor.w
    return vResult;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

/****************************************************************************
 *
 * Miscellaneous
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMINLINE BOOL XMVerifyCPUSupport()
{
#if defined(_XM_NO_INTRINSICS_) || !defined(_XM_SSE_INTRINSICS_)
	return TRUE;
#else // _XM_SSE_INTRINSICS_
	// Note that on Windows 2000 or older, SSE2 detection is not supported so this will always fail
	// Detecting SSE2 on older versions of Windows would require using cpuid directly
	return ( IsProcessorFeaturePresent( PF_XMMI_INSTRUCTIONS_AVAILABLE ) && IsProcessorFeaturePresent( PF_XMMI64_INSTRUCTIONS_AVAILABLE ) );
#endif
}

//------------------------------------------------------------------------------

#define XMASSERT_LINE_STRING_SIZE 16

XMINLINE VOID XMAssert
(
    CONST CHAR* pExpression,
    CONST CHAR* pFileName,
    UINT        LineNumber
)
{
    CHAR        aLineString[XMASSERT_LINE_STRING_SIZE];
    CHAR*       pLineString;
    UINT        Line;

    aLineString[XMASSERT_LINE_STRING_SIZE - 2] = '0';
    aLineString[XMASSERT_LINE_STRING_SIZE - 1] = '\0';
    for (Line = LineNumber, pLineString = aLineString + XMASSERT_LINE_STRING_SIZE - 2;
         Line != 0 && pLineString >= aLineString;
         Line /= 10, pLineString--)
    {
        *pLineString = (CHAR)('0' + (Line % 10));
    }

#ifndef NO_OUTPUT_DEBUG_STRING
    OutputDebugStringA("Assertion failed: ");
    OutputDebugStringA(pExpression);
    OutputDebugStringA(", file ");
    OutputDebugStringA(pFileName);
    OutputDebugStringA(", line ");
    OutputDebugStringA(pLineString + 1);
    OutputDebugStringA("\r\n");
#else
    DbgPrint("Assertion failed: %s, file %s, line %d\r\n", pExpression, pFileName, LineNumber);
#endif

    __debugbreak();
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMFresnelTerm
(
    FXMVECTOR CosIncidentAngle,
    FXMVECTOR RefractionIndex
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR G;
    XMVECTOR D, S;
    XMVECTOR V0, V1, V2, V3;
    XMVECTOR Result;

    // Result = 0.5f * (g - c)^2 / (g + c)^2 * ((c * (g + c) - 1)^2 / (c * (g - c) + 1)^2 + 1) where
    // c = CosIncidentAngle
    // g = sqrt(c^2 + RefractionIndex^2 - 1)

    XMASSERT(!XMVector4IsInfinite(CosIncidentAngle));

    G = XMVectorMultiplyAdd(RefractionIndex, RefractionIndex, g_XMNegativeOne.v);
    G = XMVectorMultiplyAdd(CosIncidentAngle, CosIncidentAngle, G);
    G = XMVectorAbs(G);
    G = XMVectorSqrt(G);

    S = XMVectorAdd(G, CosIncidentAngle);
    D = XMVectorSubtract(G, CosIncidentAngle);

    V0 = XMVectorMultiply(D, D);
    V1 = XMVectorMultiply(S, S);
    V1 = XMVectorReciprocal(V1);
    V0 = XMVectorMultiply(g_XMOneHalf.v, V0);
    V0 = XMVectorMultiply(V0, V1);

    V2 = XMVectorMultiplyAdd(CosIncidentAngle, S, g_XMNegativeOne.v);
    V3 = XMVectorMultiplyAdd(CosIncidentAngle, D, g_XMOne.v);
    V2 = XMVectorMultiply(V2, V2);
    V3 = XMVectorMultiply(V3, V3);
    V3 = XMVectorReciprocal(V3);
    V2 = XMVectorMultiplyAdd(V2, V3, g_XMOne.v);

    Result = XMVectorMultiply(V0, V2);

    Result = XMVectorSaturate(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Result = 0.5f * (g - c)^2 / (g + c)^2 * ((c * (g + c) - 1)^2 / (c * (g - c) + 1)^2 + 1) where
    // c = CosIncidentAngle
    // g = sqrt(c^2 + RefractionIndex^2 - 1)

    XMASSERT(!XMVector4IsInfinite(CosIncidentAngle));

    // G = sqrt(abs((RefractionIndex^2-1) + CosIncidentAngle^2))
    XMVECTOR G = _mm_mul_ps(RefractionIndex,RefractionIndex);
    XMVECTOR vTemp = _mm_mul_ps(CosIncidentAngle,CosIncidentAngle);
    G = _mm_sub_ps(G,g_XMOne);
    vTemp = _mm_add_ps(vTemp,G);
    // max((0-vTemp),vTemp) == abs(vTemp)
    // The abs is needed to deal with refraction and cosine being zero
	G = _mm_setzero_ps();
	G = _mm_sub_ps(G,vTemp);
	G = _mm_max_ps(G,vTemp);
    // Last operation, the sqrt()
    G = _mm_sqrt_ps(G);

    // Calc G-C and G+C
    XMVECTOR GAddC = _mm_add_ps(G,CosIncidentAngle);
    XMVECTOR GSubC = _mm_sub_ps(G,CosIncidentAngle);
    // Perform the term (0.5f *(g - c)^2) / (g + c)^2
    XMVECTOR vResult = _mm_mul_ps(GSubC,GSubC);
    vTemp = _mm_mul_ps(GAddC,GAddC);
    vResult = _mm_mul_ps(vResult,g_XMOneHalf);
    vResult = _mm_div_ps(vResult,vTemp);
    // Perform the term ((c * (g + c) - 1)^2 / (c * (g - c) + 1)^2 + 1)
    GAddC = _mm_mul_ps(GAddC,CosIncidentAngle);
    GSubC = _mm_mul_ps(GSubC,CosIncidentAngle);
    GAddC = _mm_sub_ps(GAddC,g_XMOne);
    GSubC = _mm_add_ps(GSubC,g_XMOne);
    GAddC = _mm_mul_ps(GAddC,GAddC);
    GSubC = _mm_mul_ps(GSubC,GSubC);
    GAddC = _mm_div_ps(GAddC,GSubC);
    GAddC = _mm_add_ps(GAddC,g_XMOne);
    // Multiply the two term parts
    vResult = _mm_mul_ps(vResult,GAddC);
    // Clamp to 0.0 - 1.0f
    vResult = _mm_max_ps(vResult,g_XMZero);
    vResult = _mm_min_ps(vResult,g_XMOne);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMScalarNearEqual
(
    FLOAT S1,
    FLOAT S2,
    FLOAT Epsilon
)
{
    FLOAT Delta = S1 - S2;
#if defined(_XM_NO_INTRINSICS_)
    UINT  AbsDelta = *(UINT*)&Delta & 0x7FFFFFFF;
    return (*(FLOAT*)&AbsDelta <= Epsilon);
#elif defined(_XM_SSE_INTRINSICS_)
    return (fabsf(Delta) <= Epsilon);
#else
    return (__fabs(Delta) <= Epsilon);
#endif
}

//------------------------------------------------------------------------------
// Modulo the range of the given angle such that -XM_PI <= Angle < XM_PI
XMFINLINE FLOAT XMScalarModAngle
(
    FLOAT Angle
)
{
    // Note: The modulo is performed with unsigned math only to work
    // around a precision error on numbers that are close to PI
    float fTemp;
#if defined(_XM_NO_INTRINSICS_) || !defined(_XM_VMX128_INTRINSICS_)
    // Normalize the range from 0.0f to XM_2PI
    Angle = Angle + XM_PI;
    // Perform the modulo, unsigned
    fTemp = fabsf(Angle);
    fTemp = fTemp - (XM_2PI * (FLOAT)((INT)(fTemp/XM_2PI)));
    // Restore the number to the range of -XM_PI to XM_PI-epsilon
    fTemp = fTemp - XM_PI;
    // If the modulo'd value was negative, restore negation
    if (Angle<0.0f) {
        fTemp = -fTemp;
    }
    return fTemp;
#else
#endif
}

//------------------------------------------------------------------------------

XMINLINE FLOAT XMScalarSin
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT                  ValueMod;
    FLOAT                  ValueSq;
    XMVECTOR               V0123, V0246, V1357, V9111315, V17192123;
    XMVECTOR               V1, V7, V8;
    XMVECTOR               R0, R1, R2;

    ValueMod = XMScalarModAngle(Value);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! - V^15 / 15! +
    //           V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)

    ValueSq = ValueMod * ValueMod;

    V0123     = XMVectorSet(1.0f, ValueMod, ValueSq, ValueSq * ValueMod);
    V1        = XMVectorSplatY(V0123);
    V0246     = XMVectorMultiply(V0123, V0123);
    V1357     = XMVectorMultiply(V0246, V1);
    V7        = XMVectorSplatW(V1357);
    V8        = XMVectorMultiply(V7, V1);
    V9111315  = XMVectorMultiply(V1357, V8);
    V17192123 = XMVectorMultiply(V9111315, V8);

    R0        = XMVector4Dot(V1357, g_XMSinCoefficients0.v);
    R1        = XMVector4Dot(V9111315, g_XMSinCoefficients1.v);
    R2        = XMVector4Dot(V17192123, g_XMSinCoefficients2.v);

    return R0.vector4_f32[0] + R1.vector4_f32[0] + R2.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    return sinf( Value );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE FLOAT XMScalarCos
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT                  ValueMod;
    FLOAT                  ValueSq;
    XMVECTOR               V0123, V0246, V8101214, V16182022;
    XMVECTOR               V2, V6, V8;
    XMVECTOR               R0, R1, R2;

    ValueMod = XMScalarModAngle(Value);

    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! +
    //           V^12 / 12! - V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)

    ValueSq = ValueMod * ValueMod;

    V0123 = XMVectorSet(1.0f, ValueMod, ValueSq, ValueSq * ValueMod);
    V0246 = XMVectorMultiply(V0123, V0123);

    V2 = XMVectorSplatZ(V0123);
    V6 = XMVectorSplatW(V0246);
    V8 = XMVectorMultiply(V6, V2);

    V8101214 = XMVectorMultiply(V0246, V8);
    V16182022 = XMVectorMultiply(V8101214, V8);

    R0 = XMVector4Dot(V0246, g_XMCosCoefficients0.v);
    R1 = XMVector4Dot(V8101214, g_XMCosCoefficients1.v);
    R2 = XMVector4Dot(V16182022, g_XMCosCoefficients2.v);

    return R0.vector4_f32[0] + R1.vector4_f32[0] + R2.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    return cosf(Value);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE VOID XMScalarSinCos
(
    FLOAT* pSin,
    FLOAT* pCos,
    FLOAT  Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT                  ValueMod;
    FLOAT                  ValueSq;
    XMVECTOR               V0123, V0246, V1357, V8101214, V9111315, V16182022, V17192123;
    XMVECTOR               V1, V2, V6, V8;
    XMVECTOR               S0, S1, S2, C0, C1, C2;

    XMASSERT(pSin);
    XMASSERT(pCos);

    ValueMod = XMScalarModAngle(Value);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! - V^15 / 15! +
    //           V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)
    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! +
    //           V^12 / 12! - V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)

    ValueSq = ValueMod * ValueMod;

    V0123 = XMVectorSet(1.0f, ValueMod, ValueSq, ValueSq * ValueMod);

    V1 = XMVectorSplatY(V0123);
    V2 = XMVectorSplatZ(V0123);

    V0246 = XMVectorMultiply(V0123, V0123);
    V1357 = XMVectorMultiply(V0246, V1);

    V6 = XMVectorSplatW(V0246);
    V8 = XMVectorMultiply(V6, V2);

    V8101214 = XMVectorMultiply(V0246, V8);
    V9111315 = XMVectorMultiply(V1357, V8);
    V16182022 = XMVectorMultiply(V8101214, V8);
    V17192123 = XMVectorMultiply(V9111315, V8);

    C0 = XMVector4Dot(V0246, g_XMCosCoefficients0.v);
    S0 = XMVector4Dot(V1357, g_XMSinCoefficients0.v);
    C1 = XMVector4Dot(V8101214, g_XMCosCoefficients1.v);
    S1 = XMVector4Dot(V9111315, g_XMSinCoefficients1.v);
    C2 = XMVector4Dot(V16182022, g_XMCosCoefficients2.v);
    S2 = XMVector4Dot(V17192123, g_XMSinCoefficients2.v);

    *pCos = C0.vector4_f32[0] + C1.vector4_f32[0] + C2.vector4_f32[0];
    *pSin = S0.vector4_f32[0] + S1.vector4_f32[0] + S2.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pSin);
    XMASSERT(pCos);

    *pSin = sinf(Value);
    *pCos = cosf(Value);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE FLOAT XMScalarASin
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT AbsValue, Value2, Value3, D;
    XMVECTOR AbsV, R0, R1, Result;
    XMVECTOR V3;

    *(UINT*)&AbsValue = *(UINT*)&Value & 0x7FFFFFFF;

    Value2 = Value * AbsValue;
    Value3 = Value * Value2;
    D = (Value - Value2) / sqrtf(1.00000011921f - AbsValue);

    AbsV = XMVectorReplicate(AbsValue);

    V3.vector4_f32[0] = Value3;
    V3.vector4_f32[1] = 1.0f;
    V3.vector4_f32[2] = Value3;
    V3.vector4_f32[3] = 1.0f;

    R1 = XMVectorSet(D, D, Value, Value);
    R1 = XMVectorMultiply(R1, V3);

    R0 = XMVectorMultiplyAdd(AbsV, g_XMASinCoefficients0.v, g_XMASinCoefficients1.v);
    R0 = XMVectorMultiplyAdd(AbsV, R0, g_XMASinCoefficients2.v);

    Result = XMVector4Dot(R0, R1);

    return Result.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    return asinf(Value);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE FLOAT XMScalarACos
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    return XM_PIDIV2 - XMScalarASin(Value);

#elif defined(_XM_SSE_INTRINSICS_)
    return acosf(Value);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE FLOAT XMScalarSinEst
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT                  ValueSq;
    XMVECTOR               V;
    XMVECTOR               Y;
    XMVECTOR               Result;

    XMASSERT(Value >= -XM_PI);
    XMASSERT(Value < XM_PI);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! (for -PI <= V < PI)

    ValueSq = Value * Value;

    V = XMVectorSet(1.0f, Value, ValueSq, ValueSq * Value);
    Y = XMVectorSplatY(V);
    V = XMVectorMultiply(V, V);
    V = XMVectorMultiply(V, Y);

    Result = XMVector4Dot(V, g_XMSinEstCoefficients.v);

    return Result.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(Value >= -XM_PI);
    XMASSERT(Value < XM_PI);
    float ValueSq = Value*Value;
    XMVECTOR vValue = _mm_set_ps1(Value);
    XMVECTOR vTemp = _mm_set_ps(ValueSq * Value,ValueSq,Value,1.0f);
    vTemp = _mm_mul_ps(vTemp,vTemp);
    vTemp = _mm_mul_ps(vTemp,vValue);
    // vTemp = Value,Value^3,Value^5,Value^7
    vTemp = _mm_mul_ps(vTemp,g_XMSinEstCoefficients);
    vValue = _mm_shuffle_ps(vValue,vTemp,_MM_SHUFFLE(1,0,0,0)); // Copy X to the Z position and Y to the W position
    vValue = _mm_add_ps(vValue,vTemp);          // Add Z = X+Z; W = Y+W;
    vTemp = _mm_shuffle_ps(vTemp,vValue,_MM_SHUFFLE(0,3,0,0));  // Copy W to the Z position
    vTemp = _mm_add_ps(vTemp,vValue);           // Add Z and W together
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(2,2,2,2));    // Splat Z and return
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    return _mm_cvtss_f32(vTemp);
#else
    return vTemp.m128_f32[0];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE FLOAT XMScalarCosEst
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT    ValueSq;
    XMVECTOR V;
    XMVECTOR Result;
    XMASSERT(Value >= -XM_PI);
    XMASSERT(Value < XM_PI);
    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! (for -PI <= V < PI)
    ValueSq = Value * Value;
    V = XMVectorSet(1.0f, Value, ValueSq, ValueSq * Value);
    V = XMVectorMultiply(V, V);
    Result = XMVector4Dot(V, g_XMCosEstCoefficients.v);
    return Result.vector4_f32[0];
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(Value >= -XM_PI);
    XMASSERT(Value < XM_PI);
    float ValueSq = Value*Value;
    XMVECTOR vValue = _mm_setzero_ps();
    XMVECTOR vTemp = _mm_set_ps(ValueSq * Value,ValueSq,Value,1.0f);
    vTemp = _mm_mul_ps(vTemp,vTemp);
    // vTemp = 1.0f,Value^2,Value^4,Value^6
    vTemp = _mm_mul_ps(vTemp,g_XMCosEstCoefficients);
    vValue = _mm_shuffle_ps(vValue,vTemp,_MM_SHUFFLE(1,0,0,0)); // Copy X to the Z position and Y to the W position
    vValue = _mm_add_ps(vValue,vTemp);          // Add Z = X+Z; W = Y+W;
    vTemp = _mm_shuffle_ps(vTemp,vValue,_MM_SHUFFLE(0,3,0,0));  // Copy W to the Z position
    vTemp = _mm_add_ps(vTemp,vValue);           // Add Z and W together
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(2,2,2,2));    // Splat Z and return
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    return _mm_cvtss_f32(vTemp);
#else
    return vTemp.m128_f32[0];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE VOID XMScalarSinCosEst
(
    FLOAT* pSin,
    FLOAT* pCos,
    FLOAT  Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT    ValueSq;
    XMVECTOR V, Sin, Cos;
    XMVECTOR Y;

    XMASSERT(pSin);
    XMASSERT(pCos);
    XMASSERT(Value >= -XM_PI);
    XMASSERT(Value < XM_PI);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! (for -PI <= V < PI)
    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! (for -PI <= V < PI)

    ValueSq = Value * Value;
    V = XMVectorSet(1.0f, Value, ValueSq, Value * ValueSq);
    Y = XMVectorSplatY(V);
    Cos = XMVectorMultiply(V, V);
    Sin = XMVectorMultiply(Cos, Y);

    Cos = XMVector4Dot(Cos, g_XMCosEstCoefficients.v);
    Sin = XMVector4Dot(Sin, g_XMSinEstCoefficients.v);

    *pCos = Cos.vector4_f32[0];
    *pSin = Sin.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pSin);
    XMASSERT(pCos);
    XMASSERT(Value >= -XM_PI);
    XMASSERT(Value < XM_PI);
    float ValueSq = Value * Value;
    XMVECTOR Cos = _mm_set_ps(Value * ValueSq,ValueSq,Value,1.0f);
    XMVECTOR Sin = _mm_set_ps1(Value);
    Cos = _mm_mul_ps(Cos,Cos);
    Sin = _mm_mul_ps(Sin,Cos);
    // Cos = 1.0f,Value^2,Value^4,Value^6
    Cos = XMVector4Dot(Cos,g_XMCosEstCoefficients);
    _mm_store_ss(pCos,Cos);
    // Sin = Value,Value^3,Value^5,Value^7
    Sin = XMVector4Dot(Sin, g_XMSinEstCoefficients);
    _mm_store_ss(pSin,Sin);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE FLOAT XMScalarASinEst
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR VR, CR, CS;
    XMVECTOR Result;
    FLOAT AbsV, V2, D;
    CONST FLOAT OnePlusEps = 1.00000011921f;

    *(UINT*)&AbsV = *(UINT*)&Value & 0x7FFFFFFF;
    V2 = Value * AbsV;
    D = OnePlusEps - AbsV;

    CS = XMVectorSet(Value, 1.0f, 1.0f, V2);
    VR = XMVectorSet(sqrtf(D), Value, V2, D * AbsV);
    CR = XMVectorMultiply(CS, g_XMASinEstCoefficients.v);

    Result = XMVector4Dot(VR, CR);

    return Result.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    CONST FLOAT OnePlusEps = 1.00000011921f;
    FLOAT AbsV = fabsf(Value);
    FLOAT V2 = Value * AbsV;    // Square with sign retained
    FLOAT D = OnePlusEps - AbsV;

    XMVECTOR Result = _mm_set_ps(V2,1.0f,1.0f,Value);
    XMVECTOR VR = _mm_set_ps(D * AbsV,V2,Value,sqrtf(D));
    Result = _mm_mul_ps(Result, g_XMASinEstCoefficients);
    Result = XMVector4Dot(VR,Result);
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    return _mm_cvtss_f32(Result);
#else
    return Result.m128_f32[0];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE FLOAT XMScalarACosEst
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR VR, CR, CS;
    XMVECTOR Result;
    FLOAT AbsV, V2, D;
    CONST FLOAT OnePlusEps = 1.00000011921f;

    // return XM_PIDIV2 - XMScalarASin(Value);

    *(UINT*)&AbsV = *(UINT*)&Value & 0x7FFFFFFF;
    V2 = Value * AbsV;
    D = OnePlusEps - AbsV;

    CS = XMVectorSet(Value, 1.0f, 1.0f, V2);
    VR = XMVectorSet(sqrtf(D), Value, V2, D * AbsV);
    CR = XMVectorMultiply(CS, g_XMASinEstCoefficients.v);

    Result = XMVector4Dot(VR, CR);

    return XM_PIDIV2 - Result.vector4_f32[0];

#elif defined(_XM_SSE_INTRINSICS_)
    CONST FLOAT OnePlusEps = 1.00000011921f;
    FLOAT AbsV = fabsf(Value);
    FLOAT V2 = Value * AbsV;    // Value^2 retaining sign
    FLOAT D = OnePlusEps - AbsV;
    XMVECTOR Result = _mm_set_ps(V2,1.0f,1.0f,Value);
    XMVECTOR VR = _mm_set_ps(D * AbsV,V2,Value,sqrtf(D));
    Result = _mm_mul_ps(Result,g_XMASinEstCoefficients);
    Result = XMVector4Dot(VR,Result);
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    return XM_PIDIV2 - _mm_cvtss_f32(Result);
#else
    return XM_PIDIV2 - Result.m128_f32[0];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

#endif // __XNAMATHMISC_INL__
