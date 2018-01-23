/*++

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    xnamathvector.inl

Abstract:

	XNA math library for Windows and Xbox 360: Vector functions
--*/

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifndef __XNAMATHVECTOR_INL__
#define __XNAMATHVECTOR_INL__

#if defined(_XM_NO_INTRINSICS_)
#define XMISNAN(x)  ((*(UINT*)&(x) & 0x7F800000) == 0x7F800000 && (*(UINT*)&(x) & 0x7FFFFF) != 0)
#define XMISINF(x)  ((*(UINT*)&(x) & 0x7FFFFFFF) == 0x7F800000)
#endif

/****************************************************************************
 *
 * General Vector
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Assignment operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Return a vector with all elements equaling zero
XMFINLINE XMVECTOR XMVectorZero()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult = {0.0f,0.0f,0.0f,0.0f};
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_setzero_ps();
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with four floating point values
XMFINLINE XMVECTOR XMVectorSet
(
    FLOAT x, 
    FLOAT y, 
    FLOAT z, 
    FLOAT w
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTORF32 vResult = {x,y,z,w};
    return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_set_ps( w, z, y, x );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with four integer values
XMFINLINE XMVECTOR XMVectorSetInt
(
    UINT x, 
    UINT y, 
    UINT z, 
    UINT w
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTORU32 vResult = {x,y,z,w};
    return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i V = _mm_set_epi32( w, z, y, x );
    return reinterpret_cast<__m128 *>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with a replicated floating point value
XMFINLINE XMVECTOR XMVectorReplicate
(
    FLOAT Value
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
    XMVECTORF32 vResult = {Value,Value,Value,Value};
    return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_set_ps1( Value );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with a replicated floating point value passed by pointer
XMFINLINE XMVECTOR XMVectorReplicatePtr
(
    CONST FLOAT *pValue
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
    FLOAT Value = pValue[0];
    XMVECTORF32 vResult = {Value,Value,Value,Value};
    return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_load_ps1( pValue );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with a replicated integer value
XMFINLINE XMVECTOR XMVectorReplicateInt
(
    UINT Value
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
    XMVECTORU32 vResult = {Value,Value,Value,Value};
    return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_set1_epi32( Value );
    return reinterpret_cast<const __m128 *>(&vTemp)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with a replicated integer value passed by pointer
XMFINLINE XMVECTOR XMVectorReplicateIntPtr
(
    CONST UINT *pValue
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
    UINT Value = pValue[0];
    XMVECTORU32 vResult = {Value,Value,Value,Value};
    return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_load_ps1(reinterpret_cast<const float *>(pValue));
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with all bits set (true mask)
XMFINLINE XMVECTOR XMVectorTrueInt()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTORU32 vResult = {0xFFFFFFFFU,0xFFFFFFFFU,0xFFFFFFFFU,0xFFFFFFFFU};
    return vResult.v;
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i V = _mm_set1_epi32(-1);
    return reinterpret_cast<__m128 *>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Initialize a vector with all bits clear (false mask)
XMFINLINE XMVECTOR XMVectorFalseInt()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult = {0.0f,0.0f,0.0f,0.0f};
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_setzero_ps();
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Replicate the x component of the vector
XMFINLINE XMVECTOR XMVectorSplatX
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_f32[0] = 
    vResult.vector4_f32[1] = 
    vResult.vector4_f32[2] = 
    vResult.vector4_f32[3] = V.vector4_f32[0];
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_shuffle_ps( V, V, _MM_SHUFFLE(0, 0, 0, 0) );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Replicate the y component of the vector
XMFINLINE XMVECTOR XMVectorSplatY
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_f32[0] = 
    vResult.vector4_f32[1] = 
    vResult.vector4_f32[2] = 
    vResult.vector4_f32[3] = V.vector4_f32[1];
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_shuffle_ps( V, V, _MM_SHUFFLE(1, 1, 1, 1) );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Replicate the z component of the vector
XMFINLINE XMVECTOR XMVectorSplatZ
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_f32[0] = 
    vResult.vector4_f32[1] = 
    vResult.vector4_f32[2] = 
    vResult.vector4_f32[3] = V.vector4_f32[2];
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_shuffle_ps( V, V, _MM_SHUFFLE(2, 2, 2, 2) );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Replicate the w component of the vector
XMFINLINE XMVECTOR XMVectorSplatW
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_f32[0] = 
    vResult.vector4_f32[1] = 
    vResult.vector4_f32[2] = 
    vResult.vector4_f32[3] = V.vector4_f32[3];
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_shuffle_ps( V, V, _MM_SHUFFLE(3, 3, 3, 3) );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return a vector of 1.0f,1.0f,1.0f,1.0f
XMFINLINE XMVECTOR XMVectorSplatOne()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_f32[0] = 
    vResult.vector4_f32[1] = 
    vResult.vector4_f32[2] = 
    vResult.vector4_f32[3] = 1.0f;
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return g_XMOne;
#else //  _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return a vector of INF,INF,INF,INF
XMFINLINE XMVECTOR XMVectorSplatInfinity()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_u32[0] = 
    vResult.vector4_u32[1] = 
    vResult.vector4_u32[2] = 
    vResult.vector4_u32[3] = 0x7F800000;
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return g_XMInfinity;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return a vector of Q_NAN,Q_NAN,Q_NAN,Q_NAN
XMFINLINE XMVECTOR XMVectorSplatQNaN()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_u32[0] = 
    vResult.vector4_u32[1] = 
    vResult.vector4_u32[2] = 
    vResult.vector4_u32[3] = 0x7FC00000;
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return g_XMQNaN;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return a vector of 1.192092896e-7f,1.192092896e-7f,1.192092896e-7f,1.192092896e-7f
XMFINLINE XMVECTOR XMVectorSplatEpsilon()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_u32[0] = 
    vResult.vector4_u32[1] = 
    vResult.vector4_u32[2] = 
    vResult.vector4_u32[3] = 0x34000000;
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    return g_XMEpsilon;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return a vector of -0.0f (0x80000000),-0.0f,-0.0f,-0.0f
XMFINLINE XMVECTOR XMVectorSplatSignMask()
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult;
    vResult.vector4_u32[0] = 
    vResult.vector4_u32[1] = 
    vResult.vector4_u32[2] = 
    vResult.vector4_u32[3] = 0x80000000U;
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i V = _mm_set1_epi32( 0x80000000 );
    return reinterpret_cast<__m128*>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return a floating point value via an index. This is not a recommended
// function to use due to performance loss.
XMFINLINE FLOAT XMVectorGetByIndex(FXMVECTOR V,UINT i)
{
    XMASSERT( i <= 3 );
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_f32[i];
#elif defined(_XM_SSE_INTRINSICS_)
    return V.m128_f32[i];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return the X component in an FPU register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE FLOAT XMVectorGetX(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_f32[0];
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    return _mm_cvtss_f32(V);    
#else
    return V.m128_f32[0];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Return the Y component in an FPU register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE FLOAT XMVectorGetY(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_f32[1];
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    return _mm_cvtss_f32(vTemp);
#else
    return V.m128_f32[1];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Return the Z component in an FPU register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE FLOAT XMVectorGetZ(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_f32[2];
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(2,2,2,2));
    return _mm_cvtss_f32(vTemp);
#else
    return V.m128_f32[2];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Return the W component in an FPU register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE FLOAT XMVectorGetW(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_f32[3];
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_MSC_VER) && (_MSC_VER>=1500)
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,3,3,3));
    return _mm_cvtss_f32(vTemp);
#else
    return V.m128_f32[3];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Store a component indexed by i into a 32 bit float location in memory.
// This causes Load/Hit/Store on VMX targets
XMFINLINE VOID XMVectorGetByIndexPtr(FLOAT *f,FXMVECTOR V,UINT i)
{
    XMASSERT( f != 0 );
    XMASSERT( i <  4 );
#if defined(_XM_NO_INTRINSICS_)
    *f = V.vector4_f32[i];
#elif defined(_XM_SSE_INTRINSICS_)
    *f = V.m128_f32[i];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Store the X component into a 32 bit float location in memory.
XMFINLINE VOID XMVectorGetXPtr(FLOAT *x,FXMVECTOR V)
{
    XMASSERT( x != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *x = V.vector4_f32[0];
#elif defined(_XM_SSE_INTRINSICS_)
    _mm_store_ss(x,V);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Store the Y component into a 32 bit float location in memory.
XMFINLINE VOID XMVectorGetYPtr(FLOAT *y,FXMVECTOR V)
{
    XMASSERT( y != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *y = V.vector4_f32[1];
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    _mm_store_ss(y,vResult);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Store the Z component into a 32 bit float location in memory.
XMFINLINE VOID XMVectorGetZPtr(FLOAT *z,FXMVECTOR V)
{
    XMASSERT( z != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *z = V.vector4_f32[2];
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(2,2,2,2));
    _mm_store_ss(z,vResult);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Store the W component into a 32 bit float location in memory.
XMFINLINE VOID XMVectorGetWPtr(FLOAT *w,FXMVECTOR V)
{
    XMASSERT( w != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *w = V.vector4_f32[3];
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,3,3,3));
    _mm_store_ss(w,vResult);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Return an integer value via an index. This is not a recommended
// function to use due to performance loss.
XMFINLINE UINT XMVectorGetIntByIndex(FXMVECTOR V, UINT i)
{
    XMASSERT( i < 4 );
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_u32[i];
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_MSC_VER) && (_MSC_VER<1400)
    XMVECTORU32 tmp;
    tmp.v = V;
    return tmp.u[i];
#else
    return V.m128_u32[i];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Return the X component in an integer register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE UINT XMVectorGetIntX(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_u32[0];
#elif defined(_XM_SSE_INTRINSICS_)
    return static_cast<UINT>(_mm_cvtsi128_si32(reinterpret_cast<const __m128i *>(&V)[0]));
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Return the Y component in an integer register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE UINT XMVectorGetIntY(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_u32[1];
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vResulti = _mm_shuffle_epi32(reinterpret_cast<const __m128i *>(&V)[0],_MM_SHUFFLE(1,1,1,1));
    return static_cast<UINT>(_mm_cvtsi128_si32(vResulti));
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Return the Z component in an integer register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE UINT XMVectorGetIntZ(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_u32[2];
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vResulti = _mm_shuffle_epi32(reinterpret_cast<const __m128i *>(&V)[0],_MM_SHUFFLE(2,2,2,2));
    return static_cast<UINT>(_mm_cvtsi128_si32(vResulti));
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Return the W component in an integer register. 
// This causes Load/Hit/Store on VMX targets
XMFINLINE UINT XMVectorGetIntW(FXMVECTOR V)
{
#if defined(_XM_NO_INTRINSICS_)
    return V.vector4_u32[3];
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vResulti = _mm_shuffle_epi32(reinterpret_cast<const __m128i *>(&V)[0],_MM_SHUFFLE(3,3,3,3));
    return static_cast<UINT>(_mm_cvtsi128_si32(vResulti));
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Store a component indexed by i into a 32 bit integer location in memory.
// This causes Load/Hit/Store on VMX targets
XMFINLINE VOID XMVectorGetIntByIndexPtr(UINT *x,FXMVECTOR V,UINT i)
{
    XMASSERT( x != 0 );
    XMASSERT( i <  4 );
#if defined(_XM_NO_INTRINSICS_)
    *x = V.vector4_u32[i];
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_MSC_VER) && (_MSC_VER<1400)
    XMVECTORU32 tmp;
    tmp.v = V;
    *x = tmp.u[i];
#else
    *x = V.m128_u32[i];
#endif
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Store the X component into a 32 bit integer location in memory.
XMFINLINE VOID XMVectorGetIntXPtr(UINT *x,FXMVECTOR V)
{
    XMASSERT( x != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *x = V.vector4_u32[0];
#elif defined(_XM_SSE_INTRINSICS_)
    _mm_store_ss(reinterpret_cast<float *>(x),V);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Store the Y component into a 32 bit integer location in memory.
XMFINLINE VOID XMVectorGetIntYPtr(UINT *y,FXMVECTOR V)
{
    XMASSERT( y != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *y = V.vector4_u32[1];
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    _mm_store_ss(reinterpret_cast<float *>(y),vResult);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Store the Z component into a 32 bit integer locaCantion in memory.
XMFINLINE VOID XMVectorGetIntZPtr(UINT *z,FXMVECTOR V)
{
    XMASSERT( z != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *z = V.vector4_u32[2];
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(2,2,2,2));
    _mm_store_ss(reinterpret_cast<float *>(z),vResult);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Store the W component into a 32 bit integer location in memory.
XMFINLINE VOID XMVectorGetIntWPtr(UINT *w,FXMVECTOR V)
{
    XMASSERT( w != 0 );
#if defined(_XM_NO_INTRINSICS_)
    *w = V.vector4_u32[3];
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,3,3,3));
    _mm_store_ss(reinterpret_cast<float *>(w),vResult);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Set a single indexed floating point component
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetByIndex(FXMVECTOR V, FLOAT f,UINT i)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( i <= 3 );
    U = V;
    U.vector4_f32[i] = f;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( i <= 3 );
    XMVECTOR U = V;
    U.m128_f32[i] = f;
    return U;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Sets the X component of a vector to a passed floating point value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetX(FXMVECTOR V, FLOAT x)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_f32[0] = x;
    U.vector4_f32[1] = V.vector4_f32[1];
    U.vector4_f32[2] = V.vector4_f32[2];
    U.vector4_f32[3] = V.vector4_f32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_f32[0] = x;
    return vResult;
#else
    XMVECTOR vResult = _mm_set_ss(x);
    vResult = _mm_move_ss(V,vResult);
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the Y component of a vector to a passed floating point value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetY(FXMVECTOR V, FLOAT y)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_f32[0] = V.vector4_f32[0];
    U.vector4_f32[1] = y;
    U.vector4_f32[2] = V.vector4_f32[2];
    U.vector4_f32[3] = V.vector4_f32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_f32[1] = y;
    return vResult;
#else
    // Swap y and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,2,0,1));
    // Convert input to vector
    XMVECTOR vTemp = _mm_set_ss(y);
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap y and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,2,0,1));
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}
// Sets the Z component of a vector to a passed floating point value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetZ(FXMVECTOR V, FLOAT z)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_f32[0] = V.vector4_f32[0];
    U.vector4_f32[1] = V.vector4_f32[1];
    U.vector4_f32[2] = z;
    U.vector4_f32[3] = V.vector4_f32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_f32[2] = z;
    return vResult;
#else
    // Swap z and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,0,1,2));
    // Convert input to vector
    XMVECTOR vTemp = _mm_set_ss(z);
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap z and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,0,1,2));
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the W component of a vector to a passed floating point value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetW(FXMVECTOR V, FLOAT w)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_f32[0] = V.vector4_f32[0];
    U.vector4_f32[1] = V.vector4_f32[1];
    U.vector4_f32[2] = V.vector4_f32[2];
    U.vector4_f32[3] = w;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_f32[3] = w;
    return vResult;
#else
    // Swap w and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,2,1,3));
    // Convert input to vector
    XMVECTOR vTemp = _mm_set_ss(w);
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap w and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,2,1,3));
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Sets a component of a vector to a floating point value passed by pointer
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetByIndexPtr(FXMVECTOR V,CONST FLOAT *f,UINT i)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( f != 0 );
    XMASSERT( i <= 3 );
    U = V;
    U.vector4_f32[i] = *f;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( f != 0 );
    XMASSERT( i <= 3 );
    XMVECTOR U = V;
    U.m128_f32[i] = *f;
    return U;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Sets the X component of a vector to a floating point value passed by pointer
XMFINLINE XMVECTOR XMVectorSetXPtr(FXMVECTOR V,CONST FLOAT *x)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( x != 0 );
    U.vector4_f32[0] = *x;
    U.vector4_f32[1] = V.vector4_f32[1];
    U.vector4_f32[2] = V.vector4_f32[2];
    U.vector4_f32[3] = V.vector4_f32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( x != 0 );
    XMVECTOR vResult = _mm_load_ss(x);
    vResult = _mm_move_ss(V,vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the Y component of a vector to a floating point value passed by pointer
XMFINLINE XMVECTOR XMVectorSetYPtr(FXMVECTOR V,CONST FLOAT *y)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( y != 0 );
    U.vector4_f32[0] = V.vector4_f32[0];
    U.vector4_f32[1] = *y;
    U.vector4_f32[2] = V.vector4_f32[2];
    U.vector4_f32[3] = V.vector4_f32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( y != 0 );
    // Swap y and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,2,0,1));
    // Convert input to vector
    XMVECTOR vTemp = _mm_load_ss(y);
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap y and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,2,0,1));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the Z component of a vector to a floating point value passed by pointer
XMFINLINE XMVECTOR XMVectorSetZPtr(FXMVECTOR V,CONST FLOAT *z)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( z != 0 );
    U.vector4_f32[0] = V.vector4_f32[0];
    U.vector4_f32[1] = V.vector4_f32[1];
    U.vector4_f32[2] = *z;
    U.vector4_f32[3] = V.vector4_f32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( z != 0 );
    // Swap z and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,0,1,2));
    // Convert input to vector
    XMVECTOR vTemp = _mm_load_ss(z);
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap z and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,0,1,2));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the W component of a vector to a floating point value passed by pointer
XMFINLINE XMVECTOR XMVectorSetWPtr(FXMVECTOR V,CONST FLOAT *w)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( w != 0 );
    U.vector4_f32[0] = V.vector4_f32[0];
    U.vector4_f32[1] = V.vector4_f32[1];
    U.vector4_f32[2] = V.vector4_f32[2];
    U.vector4_f32[3] = *w;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( w != 0 );
    // Swap w and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,2,1,3));
    // Convert input to vector
    XMVECTOR vTemp = _mm_load_ss(w);
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap w and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,2,1,3));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Sets a component of a vector to an integer passed by value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetIntByIndex(FXMVECTOR V, UINT x, UINT i)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( i <= 3 );
    U = V;
    U.vector4_u32[i] = x;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( i <= 3 );
    XMVECTORU32 tmp;
    tmp.v = V;
    tmp.u[i] = x;
    return tmp;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Sets the X component of a vector to an integer passed by value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetIntX(FXMVECTOR V, UINT x)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_u32[0] = x;
    U.vector4_u32[1] = V.vector4_u32[1];
    U.vector4_u32[2] = V.vector4_u32[2];
    U.vector4_u32[3] = V.vector4_u32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_i32[0] = x;
    return vResult;
#else
    __m128i vTemp = _mm_cvtsi32_si128(x);
    XMVECTOR vResult = _mm_move_ss(V,reinterpret_cast<const __m128 *>(&vTemp)[0]);
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the Y component of a vector to an integer passed by value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetIntY(FXMVECTOR V, UINT y)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_u32[0] = V.vector4_u32[0];
    U.vector4_u32[1] = y;
    U.vector4_u32[2] = V.vector4_u32[2];
    U.vector4_u32[3] = V.vector4_u32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_i32[1] = y;
    return vResult;
#else    // Swap y and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,2,0,1));
    // Convert input to vector
    __m128i vTemp = _mm_cvtsi32_si128(y);
    // Replace the x component
    vResult = _mm_move_ss(vResult,reinterpret_cast<const __m128 *>(&vTemp)[0]);
    // Swap y and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,2,0,1));
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the Z component of a vector to an integer passed by value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetIntZ(FXMVECTOR V, UINT z)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_u32[0] = V.vector4_u32[0];
    U.vector4_u32[1] = V.vector4_u32[1];
    U.vector4_u32[2] = z;
    U.vector4_u32[3] = V.vector4_u32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_i32[2] = z;
    return vResult;
#else
    // Swap z and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,0,1,2));
    // Convert input to vector
    __m128i vTemp = _mm_cvtsi32_si128(z);
    // Replace the x component
    vResult = _mm_move_ss(vResult,reinterpret_cast<const __m128 *>(&vTemp)[0]);
    // Swap z and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,0,1,2));
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the W component of a vector to an integer passed by value
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetIntW(FXMVECTOR V, UINT w)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    U.vector4_u32[0] = V.vector4_u32[0];
    U.vector4_u32[1] = V.vector4_u32[1];
    U.vector4_u32[2] = V.vector4_u32[2];
    U.vector4_u32[3] = w;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_XM_ISVS2005_)
    XMVECTOR vResult = V;
    vResult.m128_i32[3] = w;
    return vResult;
#else
    // Swap w and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,2,1,3));
    // Convert input to vector
    __m128i vTemp = _mm_cvtsi32_si128(w);
    // Replace the x component
    vResult = _mm_move_ss(vResult,reinterpret_cast<const __m128 *>(&vTemp)[0]);
    // Swap w and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,2,1,3));
    return vResult;
#endif // _XM_ISVS2005_
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Sets a component of a vector to an integer value passed by pointer
// This causes Load/Hit/Store on VMX targets
XMFINLINE XMVECTOR XMVectorSetIntByIndexPtr(FXMVECTOR V, CONST UINT *x,UINT i)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( x != 0 );
    XMASSERT( i <= 3 );
    U = V;
    U.vector4_u32[i] = *x;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( x != 0 );
    XMASSERT( i <= 3 );
    XMVECTORU32 tmp;
    tmp.v = V;
    tmp.u[i] = *x;
    return tmp;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Sets the X component of a vector to an integer value passed by pointer
XMFINLINE XMVECTOR XMVectorSetIntXPtr(FXMVECTOR V,CONST UINT *x)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( x != 0 );
    U.vector4_u32[0] = *x;
    U.vector4_u32[1] = V.vector4_u32[1];
    U.vector4_u32[2] = V.vector4_u32[2];
    U.vector4_u32[3] = V.vector4_u32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( x != 0 );
    XMVECTOR vTemp = _mm_load_ss(reinterpret_cast<const float *>(x));
    XMVECTOR vResult = _mm_move_ss(V,vTemp);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the Y component of a vector to an integer value passed by pointer
XMFINLINE XMVECTOR XMVectorSetIntYPtr(FXMVECTOR V,CONST UINT *y)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( y != 0 );
    U.vector4_u32[0] = V.vector4_u32[0];
    U.vector4_u32[1] = *y;
    U.vector4_u32[2] = V.vector4_u32[2];
    U.vector4_u32[3] = V.vector4_u32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( y != 0 );
    // Swap y and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,2,0,1));
    // Convert input to vector
    XMVECTOR vTemp = _mm_load_ss(reinterpret_cast<const float *>(y));
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap y and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,2,0,1));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the Z component of a vector to an integer value passed by pointer
XMFINLINE XMVECTOR XMVectorSetIntZPtr(FXMVECTOR V,CONST UINT *z)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( z != 0 );
    U.vector4_u32[0] = V.vector4_u32[0];
    U.vector4_u32[1] = V.vector4_u32[1];
    U.vector4_u32[2] = *z;
    U.vector4_u32[3] = V.vector4_u32[3];
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( z != 0 );
    // Swap z and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,0,1,2));
    // Convert input to vector
    XMVECTOR vTemp = _mm_load_ss(reinterpret_cast<const float *>(z));
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap z and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,0,1,2));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

// Sets the W component of a vector to an integer value passed by pointer
XMFINLINE XMVECTOR XMVectorSetIntWPtr(FXMVECTOR V,CONST UINT *w)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR U;
    XMASSERT( w != 0 );
    U.vector4_u32[0] = V.vector4_u32[0];
    U.vector4_u32[1] = V.vector4_u32[1];
    U.vector4_u32[2] = V.vector4_u32[2];
    U.vector4_u32[3] = *w;
    return U;
#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( w != 0 );
    // Swap w and x
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,2,1,3));
    // Convert input to vector
    XMVECTOR vTemp = _mm_load_ss(reinterpret_cast<const float *>(w));
    // Replace the x component
    vResult = _mm_move_ss(vResult,vTemp);
    // Swap w and x again
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,2,1,3));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Define a control vector to be used in XMVectorPermute
// operations.  Visualize the two vectors V1 and V2 given
// in a permute as arranged back to back in a linear fashion,
// such that they form an array of 8 floating point values.
// The four integers specified in XMVectorPermuteControl
// will serve as indices into the array to select components
// from the two vectors.  ElementIndex0 is used to select
// an element from the vectors to be placed in the first
// component of the resulting vector, ElementIndex1 is used
// to select an element for the second component, etc.

XMFINLINE XMVECTOR XMVectorPermuteControl
(
    UINT     ElementIndex0, 
    UINT     ElementIndex1, 
    UINT     ElementIndex2, 
    UINT     ElementIndex3
)
{
#if defined(_XM_SSE_INTRINSICS_) || defined(_XM_NO_INTRINSICS_)
    XMVECTORU32 vControl;
    static CONST UINT ControlElement[] = {
                    XM_PERMUTE_0X,
                    XM_PERMUTE_0Y,
                    XM_PERMUTE_0Z,
                    XM_PERMUTE_0W,
                    XM_PERMUTE_1X,
                    XM_PERMUTE_1Y,
                    XM_PERMUTE_1Z,
                    XM_PERMUTE_1W
                };
    XMASSERT(ElementIndex0 < 8);
    XMASSERT(ElementIndex1 < 8);
    XMASSERT(ElementIndex2 < 8);
    XMASSERT(ElementIndex3 < 8);

    vControl.u[0] = ControlElement[ElementIndex0];
    vControl.u[1] = ControlElement[ElementIndex1];
    vControl.u[2] = ControlElement[ElementIndex2];
    vControl.u[3] = ControlElement[ElementIndex3];
    return vControl.v;
#else
#endif
}

//------------------------------------------------------------------------------

// Using a control vector made up of 16 bytes from 0-31, remap V1 and V2's byte
// entries into a single 16 byte vector and return it. Index 0-15 = V1,
// 16-31 = V2
XMFINLINE XMVECTOR XMVectorPermute
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR Control
)
{
#if defined(_XM_NO_INTRINSICS_)
    const BYTE *aByte[2];
    XMVECTOR Result;
    UINT i, uIndex, VectorIndex;
    const BYTE *pControl;
    BYTE *pWork;

    // Indices must be in range from 0 to 31
    XMASSERT((Control.vector4_u32[0] & 0xE0E0E0E0) == 0);
    XMASSERT((Control.vector4_u32[1] & 0xE0E0E0E0) == 0);
    XMASSERT((Control.vector4_u32[2] & 0xE0E0E0E0) == 0);
    XMASSERT((Control.vector4_u32[3] & 0xE0E0E0E0) == 0);

    // 0-15 = V1, 16-31 = V2
    aByte[0] = (const BYTE*)(&V1);
    aByte[1] = (const BYTE*)(&V2);
    i = 16;
    pControl = (const BYTE *)(&Control);
    pWork = (BYTE *)(&Result);
    do {
        // Get the byte to map from
        uIndex = pControl[0];
        ++pControl;
        VectorIndex = (uIndex>>4)&1;
        uIndex &= 0x0F;
#if defined(_XM_LITTLEENDIAN_)
        uIndex ^= 3; // Swap byte ordering on little endian machines
#endif
        pWork[0] = aByte[VectorIndex][uIndex];
        ++pWork;
    } while (--i);
    return Result;
#elif defined(_XM_SSE_INTRINSICS_)
#if defined(_PREFAST_) || defined(XMDEBUG)
    // Indices must be in range from 0 to 31
    static const XMVECTORI32 PremuteTest = {0xE0E0E0E0,0xE0E0E0E0,0xE0E0E0E0,0xE0E0E0E0};
    XMVECTOR vAssert = _mm_and_ps(Control,PremuteTest);
    __m128i vAsserti = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&vAssert)[0],g_XMZero);
    XMASSERT(_mm_movemask_ps(*reinterpret_cast<const __m128 *>(&vAsserti)) == 0xf);
#endif
    // Store the vectors onto local memory on the stack
    XMVECTOR Array[2];
    Array[0] = V1;
    Array[1] = V2;
    // Output vector, on the stack
    XMVECTORU8 vResult;
    // Get pointer to the two vectors on the stack
    const BYTE *pInput = reinterpret_cast<const BYTE *>(Array);
    // Store the Control vector on the stack to access the bytes
    // don't use Control, it can cause a register variable to spill on the stack.
    XMVECTORU8 vControl;
    vControl.v = Control;   // Write to memory
    UINT i = 0;
    do {
        UINT ComponentIndex = vControl.u[i] & 0x1FU;
        ComponentIndex ^= 3; // Swap byte ordering
        vResult.u[i] = pInput[ComponentIndex];
    } while (++i<16);
    return vResult;
#else // _XM_SSE_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Define a control vector to be used in XMVectorSelect 
// operations.  The four integers specified in XMVectorSelectControl
// serve as indices to select between components in two vectors.
// The first index controls selection for the first component of 
// the vectors involved in a select operation, the second index 
// controls selection for the second component etc.  A value of
// zero for an index causes the corresponding component from the first 
// vector to be selected whereas a one causes the component from the
// second vector to be selected instead.

XMFINLINE XMVECTOR XMVectorSelectControl
(
    UINT VectorIndex0, 
    UINT VectorIndex1, 
    UINT VectorIndex2, 
    UINT VectorIndex3
)
{
#if defined(_XM_SSE_INTRINSICS_) && !defined(_XM_NO_INTRINSICS_)
    // x=Index0,y=Index1,z=Index2,w=Index3
    __m128i vTemp = _mm_set_epi32(VectorIndex3,VectorIndex2,VectorIndex1,VectorIndex0);
    // Any non-zero entries become 0xFFFFFFFF else 0
    vTemp = _mm_cmpgt_epi32(vTemp,g_XMZero);
	return reinterpret_cast<__m128 *>(&vTemp)[0];
#else
    XMVECTOR    ControlVector;
    CONST UINT  ControlElement[] =
                {
                    XM_SELECT_0,
                    XM_SELECT_1
                };

    XMASSERT(VectorIndex0 < 2);
    XMASSERT(VectorIndex1 < 2);
    XMASSERT(VectorIndex2 < 2);
    XMASSERT(VectorIndex3 < 2);

    ControlVector.vector4_u32[0] = ControlElement[VectorIndex0];
    ControlVector.vector4_u32[1] = ControlElement[VectorIndex1];
    ControlVector.vector4_u32[2] = ControlElement[VectorIndex2];
    ControlVector.vector4_u32[3] = ControlElement[VectorIndex3];

    return ControlVector;

#endif
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorSelect
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR Control
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = (V1.vector4_u32[0] & ~Control.vector4_u32[0]) | (V2.vector4_u32[0] & Control.vector4_u32[0]);
    Result.vector4_u32[1] = (V1.vector4_u32[1] & ~Control.vector4_u32[1]) | (V2.vector4_u32[1] & Control.vector4_u32[1]);
    Result.vector4_u32[2] = (V1.vector4_u32[2] & ~Control.vector4_u32[2]) | (V2.vector4_u32[2] & Control.vector4_u32[2]);
    Result.vector4_u32[3] = (V1.vector4_u32[3] & ~Control.vector4_u32[3]) | (V2.vector4_u32[3] & Control.vector4_u32[3]);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vTemp1 = _mm_andnot_ps(Control,V1);
    XMVECTOR vTemp2 = _mm_and_ps(V2,Control);
    return _mm_or_ps(vTemp1,vTemp2);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorMergeXY
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = V1.vector4_u32[0];
    Result.vector4_u32[1] = V2.vector4_u32[0];
    Result.vector4_u32[2] = V1.vector4_u32[1];
    Result.vector4_u32[3] = V2.vector4_u32[1];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_unpacklo_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorMergeZW
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = V1.vector4_u32[2];
    Result.vector4_u32[1] = V2.vector4_u32[2];
    Result.vector4_u32[2] = V1.vector4_u32[3];
    Result.vector4_u32[3] = V2.vector4_u32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_unpackhi_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;

    Control.vector4_u32[0] = (V1.vector4_f32[0] == V2.vector4_f32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V1.vector4_f32[1] == V2.vector4_f32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V1.vector4_f32[2] == V2.vector4_f32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V1.vector4_f32[3] == V2.vector4_f32[3]) ? 0xFFFFFFFF : 0;

    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_cmpeq_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorEqualR
(
    UINT*    pCR,
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT ux, uy, uz, uw, CR;
    XMVECTOR Control;

    XMASSERT( pCR );

    ux = (V1.vector4_f32[0] == V2.vector4_f32[0]) ? 0xFFFFFFFFU : 0;
    uy = (V1.vector4_f32[1] == V2.vector4_f32[1]) ? 0xFFFFFFFFU : 0;
    uz = (V1.vector4_f32[2] == V2.vector4_f32[2]) ? 0xFFFFFFFFU : 0;
    uw = (V1.vector4_f32[3] == V2.vector4_f32[3]) ? 0xFFFFFFFFU : 0;
    CR = 0;
    if (ux&uy&uz&uw)
    {
        // All elements are greater
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!(ux|uy|uz|uw))
    {
        // All elements are not greater
        CR = XM_CRMASK_CR6FALSE;
    }
    *pCR = CR;
    Control.vector4_u32[0] = ux;
    Control.vector4_u32[1] = uy;
    Control.vector4_u32[2] = uz;
    Control.vector4_u32[3] = uw;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( pCR );
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
    UINT CR = 0;
    int iTest = _mm_movemask_ps(vTemp);
    if (iTest==0xf)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        // All elements are not greater
        CR = XM_CRMASK_CR6FALSE;
    }
    *pCR = CR;
    return vTemp;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Treat the components of the vectors as unsigned integers and
// compare individual bits between the two.  This is useful for
// comparing control vectors and result vectors returned from
// other comparison operations.

XMFINLINE XMVECTOR XMVectorEqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;

    Control.vector4_u32[0] = (V1.vector4_u32[0] == V2.vector4_u32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V1.vector4_u32[1] == V2.vector4_u32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V1.vector4_u32[2] == V2.vector4_u32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V1.vector4_u32[3] == V2.vector4_u32[3]) ? 0xFFFFFFFF : 0;

    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
	__m128i V = _mm_cmpeq_epi32( reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0] );
    return reinterpret_cast<__m128 *>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorEqualIntR
(
    UINT*    pCR,
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;

    XMASSERT(pCR);

    Control = XMVectorEqualInt(V1, V2);

    *pCR = 0;

    if (XMVector4EqualInt(Control, XMVectorTrueInt()))
    {
        // All elements are equal
        *pCR |= XM_CRMASK_CR6TRUE;
    }
    else if (XMVector4EqualInt(Control, XMVectorFalseInt()))
    {
        // All elements are not equal
        *pCR |= XM_CRMASK_CR6FALSE;
    }

    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pCR);
    __m128i V = _mm_cmpeq_epi32( reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0] );
    int iTemp = _mm_movemask_ps(reinterpret_cast<const __m128*>(&V)[0]);
    UINT CR = 0;
    if (iTemp==0x0F)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTemp)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    *pCR = CR;
    return reinterpret_cast<__m128 *>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorNearEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR Epsilon
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT fDeltax, fDeltay, fDeltaz, fDeltaw;
    XMVECTOR Control;

    fDeltax = V1.vector4_f32[0]-V2.vector4_f32[0];
    fDeltay = V1.vector4_f32[1]-V2.vector4_f32[1];
    fDeltaz = V1.vector4_f32[2]-V2.vector4_f32[2];
    fDeltaw = V1.vector4_f32[3]-V2.vector4_f32[3];

    fDeltax = fabsf(fDeltax);
    fDeltay = fabsf(fDeltay);
    fDeltaz = fabsf(fDeltaz);
    fDeltaw = fabsf(fDeltaw);

    Control.vector4_u32[0] = (fDeltax <= Epsilon.vector4_f32[0]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[1] = (fDeltay <= Epsilon.vector4_f32[1]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[2] = (fDeltaz <= Epsilon.vector4_f32[2]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[3] = (fDeltaw <= Epsilon.vector4_f32[3]) ? 0xFFFFFFFFU : 0;

    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    // Get the difference
    XMVECTOR vDelta = _mm_sub_ps(V1,V2);
    // Get the absolute value of the difference
    XMVECTOR vTemp = _mm_setzero_ps();
    vTemp = _mm_sub_ps(vTemp,vDelta);
    vTemp = _mm_max_ps(vTemp,vDelta);
    vTemp = _mm_cmple_ps(vTemp,Epsilon);
    return vTemp;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorNotEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = (V1.vector4_f32[0] != V2.vector4_f32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V1.vector4_f32[1] != V2.vector4_f32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V1.vector4_f32[2] != V2.vector4_f32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V1.vector4_f32[3] != V2.vector4_f32[3]) ? 0xFFFFFFFF : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_cmpneq_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorNotEqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = (V1.vector4_u32[0] != V2.vector4_u32[0]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[1] = (V1.vector4_u32[1] != V2.vector4_u32[1]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[2] = (V1.vector4_u32[2] != V2.vector4_u32[2]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[3] = (V1.vector4_u32[3] != V2.vector4_u32[3]) ? 0xFFFFFFFFU : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    __m128i V = _mm_cmpeq_epi32( reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0] );
    return _mm_xor_ps(reinterpret_cast<__m128 *>(&V)[0],g_XMNegOneMask);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorGreater
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = (V1.vector4_f32[0] > V2.vector4_f32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V1.vector4_f32[1] > V2.vector4_f32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V1.vector4_f32[2] > V2.vector4_f32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V1.vector4_f32[3] > V2.vector4_f32[3]) ? 0xFFFFFFFF : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_cmpgt_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorGreaterR
(
    UINT*    pCR,
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT ux, uy, uz, uw, CR;
    XMVECTOR Control;

    XMASSERT( pCR );

    ux = (V1.vector4_f32[0] > V2.vector4_f32[0]) ? 0xFFFFFFFFU : 0;
    uy = (V1.vector4_f32[1] > V2.vector4_f32[1]) ? 0xFFFFFFFFU : 0;
    uz = (V1.vector4_f32[2] > V2.vector4_f32[2]) ? 0xFFFFFFFFU : 0;
    uw = (V1.vector4_f32[3] > V2.vector4_f32[3]) ? 0xFFFFFFFFU : 0;
    CR = 0;
    if (ux&uy&uz&uw)
    {
        // All elements are greater
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!(ux|uy|uz|uw))
    {
        // All elements are not greater
        CR = XM_CRMASK_CR6FALSE;
    }
    *pCR = CR;
    Control.vector4_u32[0] = ux;
    Control.vector4_u32[1] = uy;
    Control.vector4_u32[2] = uz;
    Control.vector4_u32[3] = uw;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( pCR );
    XMVECTOR vTemp = _mm_cmpgt_ps(V1,V2);
    UINT CR = 0;
    int iTest = _mm_movemask_ps(vTemp);
    if (iTest==0xf)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        // All elements are not greater
        CR = XM_CRMASK_CR6FALSE;
    }
    *pCR = CR;
    return vTemp;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorGreaterOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = (V1.vector4_f32[0] >= V2.vector4_f32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V1.vector4_f32[1] >= V2.vector4_f32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V1.vector4_f32[2] >= V2.vector4_f32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V1.vector4_f32[3] >= V2.vector4_f32[3]) ? 0xFFFFFFFF : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_cmpge_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorGreaterOrEqualR
(
    UINT*    pCR,
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT ux, uy, uz, uw, CR;
    XMVECTOR Control;

    XMASSERT( pCR );

    ux = (V1.vector4_f32[0] >= V2.vector4_f32[0]) ? 0xFFFFFFFFU : 0;
    uy = (V1.vector4_f32[1] >= V2.vector4_f32[1]) ? 0xFFFFFFFFU : 0;
    uz = (V1.vector4_f32[2] >= V2.vector4_f32[2]) ? 0xFFFFFFFFU : 0;
    uw = (V1.vector4_f32[3] >= V2.vector4_f32[3]) ? 0xFFFFFFFFU : 0;
    CR = 0;
    if (ux&uy&uz&uw)
    {
        // All elements are greater
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!(ux|uy|uz|uw))
    {
        // All elements are not greater
        CR = XM_CRMASK_CR6FALSE;
    }
    *pCR = CR;
    Control.vector4_u32[0] = ux;
    Control.vector4_u32[1] = uy;
    Control.vector4_u32[2] = uz;
    Control.vector4_u32[3] = uw;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( pCR );
    XMVECTOR vTemp = _mm_cmpge_ps(V1,V2);
    UINT CR = 0;
    int iTest = _mm_movemask_ps(vTemp);
    if (iTest==0xf)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        // All elements are not greater
        CR = XM_CRMASK_CR6FALSE;
    }
    *pCR = CR;
    return vTemp;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorLess
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = (V1.vector4_f32[0] < V2.vector4_f32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V1.vector4_f32[1] < V2.vector4_f32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V1.vector4_f32[2] < V2.vector4_f32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V1.vector4_f32[3] < V2.vector4_f32[3]) ? 0xFFFFFFFF : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_cmplt_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorLessOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = (V1.vector4_f32[0] <= V2.vector4_f32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V1.vector4_f32[1] <= V2.vector4_f32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V1.vector4_f32[2] <= V2.vector4_f32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V1.vector4_f32[3] <= V2.vector4_f32[3]) ? 0xFFFFFFFF : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_cmple_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorInBounds
(
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = (V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[1] = (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[2] = (V.vector4_f32[2] <= Bounds.vector4_f32[2] && V.vector4_f32[2] >= -Bounds.vector4_f32[2]) ? 0xFFFFFFFF : 0;
    Control.vector4_u32[3] = (V.vector4_f32[3] <= Bounds.vector4_f32[3] && V.vector4_f32[3] >= -Bounds.vector4_f32[3]) ? 0xFFFFFFFF : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    return vTemp1;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorInBoundsR
(
    UINT*    pCR,
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT ux, uy, uz, uw, CR;
    XMVECTOR Control;

    XMASSERT( pCR != 0 );

    ux = (V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) ? 0xFFFFFFFFU : 0;
    uy = (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1]) ? 0xFFFFFFFFU : 0;
    uz = (V.vector4_f32[2] <= Bounds.vector4_f32[2] && V.vector4_f32[2] >= -Bounds.vector4_f32[2]) ? 0xFFFFFFFFU : 0;
    uw = (V.vector4_f32[3] <= Bounds.vector4_f32[3] && V.vector4_f32[3] >= -Bounds.vector4_f32[3]) ? 0xFFFFFFFFU : 0;

    CR = 0;

    if (ux&uy&uz&uw)
    {
        // All elements are in bounds
        CR = XM_CRMASK_CR6BOUNDS;
    }
    *pCR = CR;
    Control.vector4_u32[0] = ux;
    Control.vector4_u32[1] = uy;
    Control.vector4_u32[2] = uz;
    Control.vector4_u32[3] = uw;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT( pCR != 0 );
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);

    UINT CR = 0;
    if (_mm_movemask_ps(vTemp1)==0xf) {
        // All elements are in bounds
        CR = XM_CRMASK_CR6BOUNDS;
    }
    *pCR = CR;
    return vTemp1;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorIsNaN
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = XMISNAN(V.vector4_f32[0]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[1] = XMISNAN(V.vector4_f32[1]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[2] = XMISNAN(V.vector4_f32[2]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[3] = XMISNAN(V.vector4_f32[3]) ? 0xFFFFFFFFU : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the exponent
    __m128i vTempInf = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMInfinity);
    // Mask off the mantissa
    __m128i vTempNan = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMQNaNTest);
    // Are any of the exponents == 0x7F800000?
    vTempInf = _mm_cmpeq_epi32(vTempInf,g_XMInfinity);
    // Are any of the mantissa's zero? (SSE2 doesn't have a neq test)
    vTempNan = _mm_cmpeq_epi32(vTempNan,g_XMZero);
    // Perform a not on the NaN test to be true on NON-zero mantissas
    vTempNan = _mm_andnot_si128(vTempNan,vTempInf);
    // If any are NaN, the signs are true after the merge above
    return reinterpret_cast<const XMVECTOR *>(&vTempNan)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorIsInfinite
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Control;
    Control.vector4_u32[0] = XMISINF(V.vector4_f32[0]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[1] = XMISINF(V.vector4_f32[1]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[2] = XMISINF(V.vector4_f32[2]) ? 0xFFFFFFFFU : 0;
    Control.vector4_u32[3] = XMISINF(V.vector4_f32[3]) ? 0xFFFFFFFFU : 0;
    return Control;

#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the sign bit
    __m128 vTemp = _mm_and_ps(V,g_XMAbsMask);
    // Compare to infinity
    vTemp = _mm_cmpeq_ps(vTemp,g_XMInfinity);
    // If any are infinity, the signs are true.
    return vTemp;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Rounding and clamping operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorMin
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result.vector4_f32[0] = (V1.vector4_f32[0] < V2.vector4_f32[0]) ? V1.vector4_f32[0] : V2.vector4_f32[0];
    Result.vector4_f32[1] = (V1.vector4_f32[1] < V2.vector4_f32[1]) ? V1.vector4_f32[1] : V2.vector4_f32[1];
    Result.vector4_f32[2] = (V1.vector4_f32[2] < V2.vector4_f32[2]) ? V1.vector4_f32[2] : V2.vector4_f32[2];
    Result.vector4_f32[3] = (V1.vector4_f32[3] < V2.vector4_f32[3]) ? V1.vector4_f32[3] : V2.vector4_f32[3];
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_min_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorMax
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result.vector4_f32[0] = (V1.vector4_f32[0] > V2.vector4_f32[0]) ? V1.vector4_f32[0] : V2.vector4_f32[0];
    Result.vector4_f32[1] = (V1.vector4_f32[1] > V2.vector4_f32[1]) ? V1.vector4_f32[1] : V2.vector4_f32[1];
    Result.vector4_f32[2] = (V1.vector4_f32[2] > V2.vector4_f32[2]) ? V1.vector4_f32[2] : V2.vector4_f32[2];
    Result.vector4_f32[3] = (V1.vector4_f32[3] > V2.vector4_f32[3]) ? V1.vector4_f32[3] : V2.vector4_f32[3];
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_max_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorRound
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR       Result;
    XMVECTOR       Bias;
    CONST XMVECTOR Zero = XMVectorZero();
    CONST XMVECTOR BiasPos = XMVectorReplicate(0.5f);
    CONST XMVECTOR BiasNeg = XMVectorReplicate(-0.5f);

    Bias = XMVectorLess(V, Zero);
    Bias = XMVectorSelect(BiasPos, BiasNeg, Bias);
    Result = XMVectorAdd(V, Bias);
    Result = XMVectorTruncate(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // To handle NAN, INF and numbers greater than 8388608, use masking
    // Get the abs value
    __m128i vTest = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMAbsMask);
    // Test for greater than 8388608 (All floats with NO fractionals, NAN and INF
    vTest = _mm_cmplt_epi32(vTest,g_XMNoFraction);
    // Convert to int and back to float for rounding
    __m128i vInt = _mm_cvtps_epi32(V);
    // Convert back to floats
    XMVECTOR vResult = _mm_cvtepi32_ps(vInt);
    // All numbers less than 8388608 will use the round to int
    vResult = _mm_and_ps(vResult,reinterpret_cast<const XMVECTOR *>(&vTest)[0]);
    // All others, use the ORIGINAL value
    vTest = _mm_andnot_si128(vTest,reinterpret_cast<const __m128i *>(&V)[0]);
    vResult = _mm_or_ps(vResult,reinterpret_cast<const XMVECTOR *>(&vTest)[0]);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorTruncate
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Result;
    UINT     i;

    // Avoid C4701
    Result.vector4_f32[0] = 0.0f;

    for (i = 0; i < 4; i++)
    {
        if (XMISNAN(V.vector4_f32[i]))
        {
            Result.vector4_u32[i] = 0x7FC00000;
        }
        else if (fabsf(V.vector4_f32[i]) < 8388608.0f)
        {
            Result.vector4_f32[i] = (FLOAT)((INT)V.vector4_f32[i]);
        }
        else
        {
            Result.vector4_f32[i] = V.vector4_f32[i];
        }
    }
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // To handle NAN, INF and numbers greater than 8388608, use masking
    // Get the abs value
    __m128i vTest = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMAbsMask);
    // Test for greater than 8388608 (All floats with NO fractionals, NAN and INF
    vTest = _mm_cmplt_epi32(vTest,g_XMNoFraction);
    // Convert to int and back to float for rounding with truncation
    __m128i vInt = _mm_cvttps_epi32(V);
    // Convert back to floats
    XMVECTOR vResult = _mm_cvtepi32_ps(vInt);
    // All numbers less than 8388608 will use the round to int
    vResult = _mm_and_ps(vResult,reinterpret_cast<const XMVECTOR *>(&vTest)[0]);
    // All others, use the ORIGINAL value
    vTest = _mm_andnot_si128(vTest,reinterpret_cast<const __m128i *>(&V)[0]);
    vResult = _mm_or_ps(vResult,reinterpret_cast<const XMVECTOR *>(&vTest)[0]);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorFloor
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR vResult = {
        floorf(V.vector4_f32[0]),
        floorf(V.vector4_f32[1]),
        floorf(V.vector4_f32[2]),
        floorf(V.vector4_f32[3])
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_sub_ps(V,g_XMOneHalfMinusEpsilon);
    __m128i vInt = _mm_cvtps_epi32(vResult);
    vResult = _mm_cvtepi32_ps(vInt);
	return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorCeiling
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult = {
        ceilf(V.vector4_f32[0]),
        ceilf(V.vector4_f32[1]),
        ceilf(V.vector4_f32[2]),
        ceilf(V.vector4_f32[3])
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_add_ps(V,g_XMOneHalfMinusEpsilon);
    __m128i vInt = _mm_cvtps_epi32(vResult);
    vResult = _mm_cvtepi32_ps(vInt);
	return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorClamp
(
    FXMVECTOR V, 
    FXMVECTOR Min, 
    FXMVECTOR Max
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    XMASSERT(XMVector4LessOrEqual(Min, Max));

    Result = XMVectorMax(Min, V);
    Result = XMVectorMin(Max, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vResult;
	XMASSERT(XMVector4LessOrEqual(Min, Max));
	vResult = _mm_max_ps(Min,V);
	vResult = _mm_min_ps(vResult,Max);
	return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorSaturate
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    CONST XMVECTOR Zero = XMVectorZero();

    return XMVectorClamp(V, Zero, g_XMOne.v);

#elif defined(_XM_SSE_INTRINSICS_)
    // Set <0 to 0
    XMVECTOR vResult = _mm_max_ps(V,g_XMZero);
    // Set>1 to 1
    return _mm_min_ps(vResult,g_XMOne);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Bitwise logical operations
//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorAndInt
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = V1.vector4_u32[0] & V2.vector4_u32[0];
    Result.vector4_u32[1] = V1.vector4_u32[1] & V2.vector4_u32[1];
    Result.vector4_u32[2] = V1.vector4_u32[2] & V2.vector4_u32[2];
    Result.vector4_u32[3] = V1.vector4_u32[3] & V2.vector4_u32[3];
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_and_ps(V1,V2);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorAndCInt
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = V1.vector4_u32[0] & ~V2.vector4_u32[0];
    Result.vector4_u32[1] = V1.vector4_u32[1] & ~V2.vector4_u32[1];
    Result.vector4_u32[2] = V1.vector4_u32[2] & ~V2.vector4_u32[2];
    Result.vector4_u32[3] = V1.vector4_u32[3] & ~V2.vector4_u32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    __m128i V = _mm_andnot_si128( reinterpret_cast<const __m128i *>(&V2)[0], reinterpret_cast<const __m128i *>(&V1)[0] );
    return reinterpret_cast<__m128 *>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorOrInt
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = V1.vector4_u32[0] | V2.vector4_u32[0];
    Result.vector4_u32[1] = V1.vector4_u32[1] | V2.vector4_u32[1];
    Result.vector4_u32[2] = V1.vector4_u32[2] | V2.vector4_u32[2];
    Result.vector4_u32[3] = V1.vector4_u32[3] | V2.vector4_u32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    __m128i V = _mm_or_si128( reinterpret_cast<const __m128i *>(&V1)[0], reinterpret_cast<const __m128i *>(&V2)[0] );
    return reinterpret_cast<__m128 *>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorNorInt
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = ~(V1.vector4_u32[0] | V2.vector4_u32[0]);
    Result.vector4_u32[1] = ~(V1.vector4_u32[1] | V2.vector4_u32[1]);
    Result.vector4_u32[2] = ~(V1.vector4_u32[2] | V2.vector4_u32[2]);
    Result.vector4_u32[3] = ~(V1.vector4_u32[3] | V2.vector4_u32[3]);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    __m128i Result;
    Result = _mm_or_si128( reinterpret_cast<const __m128i *>(&V1)[0], reinterpret_cast<const __m128i *>(&V2)[0] );
    Result = _mm_andnot_si128( Result,g_XMNegOneMask);
    return reinterpret_cast<__m128 *>(&Result)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorXorInt
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_u32[0] = V1.vector4_u32[0] ^ V2.vector4_u32[0];
    Result.vector4_u32[1] = V1.vector4_u32[1] ^ V2.vector4_u32[1];
    Result.vector4_u32[2] = V1.vector4_u32[2] ^ V2.vector4_u32[2];
    Result.vector4_u32[3] = V1.vector4_u32[3] ^ V2.vector4_u32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	__m128i V = _mm_xor_si128( reinterpret_cast<const __m128i *>(&V1)[0], reinterpret_cast<const __m128i *>(&V2)[0] );
    return reinterpret_cast<__m128 *>(&V)[0];
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorNegate
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_f32[0] = -V.vector4_f32[0];
    Result.vector4_f32[1] = -V.vector4_f32[1];
    Result.vector4_f32[2] = -V.vector4_f32[2];
    Result.vector4_f32[3] = -V.vector4_f32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR Z;

	Z = _mm_setzero_ps();

	return _mm_sub_ps( Z, V );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorAdd
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_f32[0] = V1.vector4_f32[0] + V2.vector4_f32[0];
    Result.vector4_f32[1] = V1.vector4_f32[1] + V2.vector4_f32[1];
    Result.vector4_f32[2] = V1.vector4_f32[2] + V2.vector4_f32[2];
    Result.vector4_f32[3] = V1.vector4_f32[3] + V2.vector4_f32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_add_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorAddAngles
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR       Mask;
    XMVECTOR       Offset;
    XMVECTOR       Result;
    CONST XMVECTOR Zero = XMVectorZero();

    // Add the given angles together.  If the range of V1 is such
    // that -Pi <= V1 < Pi and the range of V2 is such that
    // -2Pi <= V2 <= 2Pi, then the range of the resulting angle
    // will be -Pi <= Result < Pi.
    Result = XMVectorAdd(V1, V2);

    Mask = XMVectorLess(Result, g_XMNegativePi.v);
    Offset = XMVectorSelect(Zero, g_XMTwoPi.v, Mask);

    Mask = XMVectorGreaterOrEqual(Result, g_XMPi.v);
    Offset = XMVectorSelect(Offset, g_XMNegativeTwoPi.v, Mask);

    Result = XMVectorAdd(Result, Offset);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Adjust the angles
    XMVECTOR vResult = _mm_add_ps(V1,V2);
    // Less than Pi?
    XMVECTOR vOffset = _mm_cmplt_ps(vResult,g_XMNegativePi);
    vOffset = _mm_and_ps(vOffset,g_XMTwoPi);
    // Add 2Pi to all entries less than -Pi
    vResult = _mm_add_ps(vResult,vOffset);
    // Greater than or equal to Pi?
    vOffset = _mm_cmpge_ps(vResult,g_XMPi);
    vOffset = _mm_and_ps(vOffset,g_XMTwoPi);
    // Sub 2Pi to all entries greater than Pi
    vResult = _mm_sub_ps(vResult,vOffset);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorSubtract
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_f32[0] = V1.vector4_f32[0] - V2.vector4_f32[0];
    Result.vector4_f32[1] = V1.vector4_f32[1] - V2.vector4_f32[1];
    Result.vector4_f32[2] = V1.vector4_f32[2] - V2.vector4_f32[2];
    Result.vector4_f32[3] = V1.vector4_f32[3] - V2.vector4_f32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_sub_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorSubtractAngles
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR       Mask;
    XMVECTOR       Offset;
    XMVECTOR       Result;
    CONST XMVECTOR Zero = XMVectorZero();

    // Subtract the given angles.  If the range of V1 is such
    // that -Pi <= V1 < Pi and the range of V2 is such that
    // -2Pi <= V2 <= 2Pi, then the range of the resulting angle
    // will be -Pi <= Result < Pi.
    Result = XMVectorSubtract(V1, V2);

    Mask = XMVectorLess(Result, g_XMNegativePi.v);
    Offset = XMVectorSelect(Zero, g_XMTwoPi.v, Mask);

    Mask = XMVectorGreaterOrEqual(Result, g_XMPi.v);
    Offset = XMVectorSelect(Offset, g_XMNegativeTwoPi.v, Mask);

    Result = XMVectorAdd(Result, Offset);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Adjust the angles
    XMVECTOR vResult = _mm_sub_ps(V1,V2);
    // Less than Pi?
    XMVECTOR vOffset = _mm_cmplt_ps(vResult,g_XMNegativePi);
    vOffset = _mm_and_ps(vOffset,g_XMTwoPi);
    // Add 2Pi to all entries less than -Pi
    vResult = _mm_add_ps(vResult,vOffset);
    // Greater than or equal to Pi?
    vOffset = _mm_cmpge_ps(vResult,g_XMPi);
    vOffset = _mm_and_ps(vOffset,g_XMTwoPi);
    // Sub 2Pi to all entries greater than Pi
    vResult = _mm_sub_ps(vResult,vOffset);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorMultiply
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Result = {
        V1.vector4_f32[0] * V2.vector4_f32[0],
        V1.vector4_f32[1] * V2.vector4_f32[1],
        V1.vector4_f32[2] * V2.vector4_f32[2],
        V1.vector4_f32[3] * V2.vector4_f32[3]
    };
    return Result;
#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_mul_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorMultiplyAdd
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR V3
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult = {
        (V1.vector4_f32[0] * V2.vector4_f32[0]) + V3.vector4_f32[0],
        (V1.vector4_f32[1] * V2.vector4_f32[1]) + V3.vector4_f32[1],
        (V1.vector4_f32[2] * V2.vector4_f32[2]) + V3.vector4_f32[2],
        (V1.vector4_f32[3] * V2.vector4_f32[3]) + V3.vector4_f32[3]
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vResult = _mm_mul_ps( V1, V2 );
	return _mm_add_ps(vResult, V3 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorDivide
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Result;
    Result.vector4_f32[0] = V1.vector4_f32[0] / V2.vector4_f32[0];
    Result.vector4_f32[1] = V1.vector4_f32[1] / V2.vector4_f32[1];
    Result.vector4_f32[2] = V1.vector4_f32[2] / V2.vector4_f32[2];
    Result.vector4_f32[3] = V1.vector4_f32[3] / V2.vector4_f32[3];
    return Result;
#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_div_ps( V1, V2 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorNegativeMultiplySubtract
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR V3
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR vResult = {
        V3.vector4_f32[0] - (V1.vector4_f32[0] * V2.vector4_f32[0]),
        V3.vector4_f32[1] - (V1.vector4_f32[1] * V2.vector4_f32[1]),
        V3.vector4_f32[2] - (V1.vector4_f32[2] * V2.vector4_f32[2]),
        V3.vector4_f32[3] - (V1.vector4_f32[3] * V2.vector4_f32[3])
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR R = _mm_mul_ps( V1, V2 );
	return _mm_sub_ps( V3, R );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorScale
(
    FXMVECTOR V, 
    FLOAT    ScaleFactor
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult = {
        V.vector4_f32[0] * ScaleFactor,
        V.vector4_f32[1] * ScaleFactor,
        V.vector4_f32[2] * ScaleFactor,
        V.vector4_f32[3] * ScaleFactor
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
   XMVECTOR vResult = _mm_set_ps1(ScaleFactor);
   return _mm_mul_ps(vResult,V);
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorReciprocalEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Result;
    UINT     i;

    // Avoid C4701
    Result.vector4_f32[0] = 0.0f;

    for (i = 0; i < 4; i++)
    {
        if (XMISNAN(V.vector4_f32[i]))
        {
            Result.vector4_u32[i] = 0x7FC00000;
        }
        else if (V.vector4_f32[i] == 0.0f || V.vector4_f32[i] == -0.0f)
        {
            Result.vector4_u32[i] = 0x7F800000 | (V.vector4_u32[i] & 0x80000000);
        }
        else
        {
            Result.vector4_f32[i] = 1.f / V.vector4_f32[i];
        }
    }
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_rcp_ps(V);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorReciprocal
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    return XMVectorReciprocalEst(V);

#elif defined(_XM_SSE_INTRINSICS_)
    return _mm_div_ps(g_XMOne,V);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Return an estimated square root
XMFINLINE XMVECTOR XMVectorSqrtEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Select;

    // if (x == +Infinity)  sqrt(x) = +Infinity
    // if (x == +0.0f)      sqrt(x) = +0.0f
    // if (x == -0.0f)      sqrt(x) = -0.0f
    // if (x < 0.0f)        sqrt(x) = QNaN

    XMVECTOR Result = XMVectorReciprocalSqrtEst(V);
    XMVECTOR Zero = XMVectorZero();
    XMVECTOR VEqualsInfinity = XMVectorEqualInt(V, g_XMInfinity.v);
    XMVECTOR VEqualsZero = XMVectorEqual(V, Zero);
    Result = XMVectorMultiply(V, Result);
    Select = XMVectorEqualInt(VEqualsInfinity, VEqualsZero);
    Result = XMVectorSelect(V, Result, Select);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_sqrt_ps(V);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorSqrt
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Zero;
    XMVECTOR VEqualsInfinity, VEqualsZero;
    XMVECTOR Select;
    XMVECTOR Result;

    // if (x == +Infinity)  sqrt(x) = +Infinity
    // if (x == +0.0f)      sqrt(x) = +0.0f
    // if (x == -0.0f)      sqrt(x) = -0.0f
    // if (x < 0.0f)        sqrt(x) = QNaN

    Result = XMVectorReciprocalSqrt(V);
    Zero = XMVectorZero();
    VEqualsInfinity = XMVectorEqualInt(V, g_XMInfinity.v);
    VEqualsZero = XMVectorEqual(V, Zero);
    Result = XMVectorMultiply(V, Result);
    Select = XMVectorEqualInt(VEqualsInfinity, VEqualsZero);
    Result = XMVectorSelect(V, Result, Select);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_sqrt_ps(V);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorReciprocalSqrtEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    // if (x == +Infinity)  rsqrt(x) = 0
    // if (x == +0.0f)      rsqrt(x) = +Infinity
    // if (x == -0.0f)      rsqrt(x) = -Infinity
    // if (x < 0.0f)        rsqrt(x) = QNaN

    XMVECTOR Result;
    UINT     i;

    // Avoid C4701
    Result.vector4_f32[0] = 0.0f;

    for (i = 0; i < 4; i++)
    {
        if (XMISNAN(V.vector4_f32[i]))
        {
            Result.vector4_u32[i] = 0x7FC00000;
        }
        else if (V.vector4_f32[i] == 0.0f || V.vector4_f32[i] == -0.0f)
        {
            Result.vector4_u32[i] = 0x7F800000 | (V.vector4_u32[i] & 0x80000000);
        }
        else if (V.vector4_f32[i] < 0.0f)
        {
            Result.vector4_u32[i] = 0x7FFFFFFF;
        }
        else if (XMISINF(V.vector4_f32[i]))
        {
            Result.vector4_f32[i] = 0.0f;
        }
        else
        {
            Result.vector4_f32[i] = 1.0f / sqrtf(V.vector4_f32[i]);
        }
    }

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	return _mm_rsqrt_ps(V);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorReciprocalSqrt
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    return XMVectorReciprocalSqrtEst(V);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_sqrt_ps(V);
    vResult = _mm_div_ps(g_XMOne,vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorExpEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result.vector4_f32[0] = powf(2.0f, V.vector4_f32[0]);
    Result.vector4_f32[1] = powf(2.0f, V.vector4_f32[1]);
    Result.vector4_f32[2] = powf(2.0f, V.vector4_f32[2]);
    Result.vector4_f32[3] = powf(2.0f, V.vector4_f32[3]);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_setr_ps(
        powf(2.0f,XMVectorGetX(V)),
        powf(2.0f,XMVectorGetY(V)),
        powf(2.0f,XMVectorGetZ(V)),
        powf(2.0f,XMVectorGetW(V)));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorExp
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR               E, S;
    XMVECTOR               R, R2, R3, R4;
    XMVECTOR               V0, V1;
    XMVECTOR               C0X, C0Y, C0Z, C0W;
    XMVECTOR               C1X, C1Y, C1Z, C1W;
    XMVECTOR               Result;
    static CONST XMVECTOR  C0 = {1.0f, -6.93147182e-1f, 2.40226462e-1f, -5.55036440e-2f};
    static CONST XMVECTOR  C1 = {9.61597636e-3f, -1.32823968e-3f, 1.47491097e-4f, -1.08635004e-5f};

    R = XMVectorFloor(V);
    E = XMVectorExpEst(R);
    R = XMVectorSubtract(V, R);
    R2 = XMVectorMultiply(R, R);
    R3 = XMVectorMultiply(R, R2);
    R4 = XMVectorMultiply(R2, R2);

    C0X = XMVectorSplatX(C0);
    C0Y = XMVectorSplatY(C0);
    C0Z = XMVectorSplatZ(C0);
    C0W = XMVectorSplatW(C0);

    C1X = XMVectorSplatX(C1);
    C1Y = XMVectorSplatY(C1);
    C1Z = XMVectorSplatZ(C1);
    C1W = XMVectorSplatW(C1);

    V0 = XMVectorMultiplyAdd(R, C0Y, C0X);
    V0 = XMVectorMultiplyAdd(R2, C0Z, V0);
    V0 = XMVectorMultiplyAdd(R3, C0W, V0);

    V1 = XMVectorMultiplyAdd(R, C1Y, C1X);
    V1 = XMVectorMultiplyAdd(R2, C1Z, V1);
    V1 = XMVectorMultiplyAdd(R3, C1W, V1);

    S = XMVectorMultiplyAdd(R4, V1, V0);

    S = XMVectorReciprocal(S);
    Result = XMVectorMultiply(E, S);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 C0 = {1.0f, -6.93147182e-1f, 2.40226462e-1f, -5.55036440e-2f};
    static CONST XMVECTORF32 C1 = {9.61597636e-3f, -1.32823968e-3f, 1.47491097e-4f, -1.08635004e-5f};

    // Get the integer of the input
    XMVECTOR R = XMVectorFloor(V);
    // Get the exponent estimate
    XMVECTOR E = XMVectorExpEst(R);
    // Get the fractional only
    R = _mm_sub_ps(V,R);
    // Get R^2
    XMVECTOR R2 = _mm_mul_ps(R,R);
    // And R^3
    XMVECTOR R3 = _mm_mul_ps(R,R2);

    XMVECTOR V0 = _mm_load_ps1(&C0.f[1]);
    V0 = _mm_mul_ps(V0,R);
    XMVECTOR vConstants = _mm_load_ps1(&C0.f[0]);
    V0 = _mm_add_ps(V0,vConstants);
    vConstants = _mm_load_ps1(&C0.f[2]);
    vConstants = _mm_mul_ps(vConstants,R2);
    V0 = _mm_add_ps(V0,vConstants);
    vConstants = _mm_load_ps1(&C0.f[3]);
    vConstants = _mm_mul_ps(vConstants,R3);
    V0 = _mm_add_ps(V0,vConstants);

    XMVECTOR V1 = _mm_load_ps1(&C1.f[1]);
    V1 = _mm_mul_ps(V1,R);
    vConstants = _mm_load_ps1(&C1.f[0]);
    V1 = _mm_add_ps(V1,vConstants);
    vConstants = _mm_load_ps1(&C1.f[2]);
    vConstants = _mm_mul_ps(vConstants,R2);
    V1 = _mm_add_ps(V1,vConstants);
    vConstants = _mm_load_ps1(&C1.f[3]);
    vConstants = _mm_mul_ps(vConstants,R3);
    V1 = _mm_add_ps(V1,vConstants);
    // R2 = R^4
    R2 = _mm_mul_ps(R2,R2);
    R2 = _mm_mul_ps(R2,V1);
    R2 = _mm_add_ps(R2,V0);
    E = _mm_div_ps(E,R2);
    return E;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorLogEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    FLOAT fScale = (1.0f / logf(2.0f));
    XMVECTOR Result;

    Result.vector4_f32[0] = logf(V.vector4_f32[0])*fScale;
    Result.vector4_f32[1] = logf(V.vector4_f32[1])*fScale;
    Result.vector4_f32[2] = logf(V.vector4_f32[2])*fScale;
    Result.vector4_f32[3] = logf(V.vector4_f32[3])*fScale;
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vScale = _mm_set_ps1(1.0f / logf(2.0f));
    XMVECTOR vResult = _mm_setr_ps(
        logf(XMVectorGetX(V)),
        logf(XMVectorGetY(V)),
        logf(XMVectorGetZ(V)),
        logf(XMVectorGetW(V)));
    vResult = _mm_mul_ps(vResult,vScale);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorLog
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fScale = (1.0f / logf(2.0f));
    XMVECTOR Result;

    Result.vector4_f32[0] = logf(V.vector4_f32[0])*fScale;
    Result.vector4_f32[1] = logf(V.vector4_f32[1])*fScale;
    Result.vector4_f32[2] = logf(V.vector4_f32[2])*fScale;
    Result.vector4_f32[3] = logf(V.vector4_f32[3])*fScale;
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vScale = _mm_set_ps1(1.0f / logf(2.0f));
    XMVECTOR vResult = _mm_setr_ps(
        logf(XMVectorGetX(V)),
        logf(XMVectorGetY(V)),
        logf(XMVectorGetZ(V)),
        logf(XMVectorGetW(V)));
    vResult = _mm_mul_ps(vResult,vScale);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorPowEst
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_f32[0] = powf(V1.vector4_f32[0], V2.vector4_f32[0]);
    Result.vector4_f32[1] = powf(V1.vector4_f32[1], V2.vector4_f32[1]);
    Result.vector4_f32[2] = powf(V1.vector4_f32[2], V2.vector4_f32[2]);
    Result.vector4_f32[3] = powf(V1.vector4_f32[3], V2.vector4_f32[3]);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_setr_ps(
        powf(XMVectorGetX(V1),XMVectorGetX(V2)),
        powf(XMVectorGetY(V1),XMVectorGetY(V2)),
        powf(XMVectorGetZ(V1),XMVectorGetZ(V2)),
        powf(XMVectorGetW(V1),XMVectorGetW(V2)));
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorPow
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(_XM_SSE_INTRINSICS_)

    return XMVectorPowEst(V1, V2);

#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorAbs
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult = {
        fabsf(V.vector4_f32[0]),
        fabsf(V.vector4_f32[1]),
        fabsf(V.vector4_f32[2]),
        fabsf(V.vector4_f32[3])
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR vResult = _mm_setzero_ps();
	vResult = _mm_sub_ps(vResult,V);
	vResult = _mm_max_ps(vResult,V);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorMod
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Reciprocal;
    XMVECTOR Quotient;
    XMVECTOR Result;

    // V1 % V2 = V1 - V2 * truncate(V1 / V2)
    Reciprocal = XMVectorReciprocal(V2);
    Quotient = XMVectorMultiply(V1, Reciprocal);
    Quotient = XMVectorTruncate(Quotient);
    Result = XMVectorNegativeMultiplySubtract(V2, Quotient, V1);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_div_ps(V1, V2);
    vResult = XMVectorTruncate(vResult);
    vResult = _mm_mul_ps(vResult,V2);
    vResult = _mm_sub_ps(V1,vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorModAngles
(
    FXMVECTOR Angles
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR Result;

    // Modulo the range of the given angles such that -XM_PI <= Angles < XM_PI
    V = XMVectorMultiply(Angles, g_XMReciprocalTwoPi.v);
    V = XMVectorRound(V);
    Result = XMVectorNegativeMultiplySubtract(g_XMTwoPi.v, V, Angles);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Modulo the range of the given angles such that -XM_PI <= Angles < XM_PI
    XMVECTOR vResult = _mm_mul_ps(Angles,g_XMReciprocalTwoPi);
    // Use the inline function due to complexity for rounding
    vResult = XMVectorRound(vResult);
    vResult = _mm_mul_ps(vResult,g_XMTwoPi);
    vResult = _mm_sub_ps(Angles,vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorSin
(
    FXMVECTOR V
)
{

#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2, V3, V5, V7, V9, V11, V13, V15, V17, V19, V21, V23;
    XMVECTOR S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11;
    XMVECTOR Result;

    V1 = XMVectorModAngles(V);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! - 
    //           V^15 / 15! + V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)
    V2  = XMVectorMultiply(V1, V1);
    V3  = XMVectorMultiply(V2, V1);
    V5  = XMVectorMultiply(V3, V2);
    V7  = XMVectorMultiply(V5, V2);
    V9  = XMVectorMultiply(V7, V2);
    V11 = XMVectorMultiply(V9, V2);
    V13 = XMVectorMultiply(V11, V2);
    V15 = XMVectorMultiply(V13, V2);
    V17 = XMVectorMultiply(V15, V2);
    V19 = XMVectorMultiply(V17, V2);
    V21 = XMVectorMultiply(V19, V2);
    V23 = XMVectorMultiply(V21, V2);

    S1  = XMVectorSplatY(g_XMSinCoefficients0.v);
    S2  = XMVectorSplatZ(g_XMSinCoefficients0.v);
    S3  = XMVectorSplatW(g_XMSinCoefficients0.v);
    S4  = XMVectorSplatX(g_XMSinCoefficients1.v);
    S5  = XMVectorSplatY(g_XMSinCoefficients1.v);
    S6  = XMVectorSplatZ(g_XMSinCoefficients1.v);
    S7  = XMVectorSplatW(g_XMSinCoefficients1.v);
    S8  = XMVectorSplatX(g_XMSinCoefficients2.v);
    S9  = XMVectorSplatY(g_XMSinCoefficients2.v);
    S10 = XMVectorSplatZ(g_XMSinCoefficients2.v);
    S11 = XMVectorSplatW(g_XMSinCoefficients2.v);

    Result = XMVectorMultiplyAdd(S1, V3, V1);
    Result = XMVectorMultiplyAdd(S2, V5, Result);
    Result = XMVectorMultiplyAdd(S3, V7, Result);
    Result = XMVectorMultiplyAdd(S4, V9, Result);
    Result = XMVectorMultiplyAdd(S5, V11, Result);
    Result = XMVectorMultiplyAdd(S6, V13, Result);
    Result = XMVectorMultiplyAdd(S7, V15, Result);
    Result = XMVectorMultiplyAdd(S8, V17, Result);
    Result = XMVectorMultiplyAdd(S9, V19, Result);
    Result = XMVectorMultiplyAdd(S10, V21, Result);
    Result = XMVectorMultiplyAdd(S11, V23, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Force the value within the bounds of pi
    XMVECTOR vResult = XMVectorModAngles(V);
    // Each on is V to the "num" power
    // V2 = V1^2
    XMVECTOR V2  = _mm_mul_ps(vResult,vResult);
    // V1^3
    XMVECTOR vPower = _mm_mul_ps(vResult,V2);    
    XMVECTOR vConstants = _mm_load_ps1(&g_XMSinCoefficients0.f[1]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^5
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients0.f[2]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^7
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients0.f[3]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^9
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients1.f[0]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^11
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients1.f[1]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^13
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients1.f[2]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^15
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients1.f[3]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^17
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients2.f[0]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^19
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients2.f[1]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^21
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients2.f[2]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^23
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMSinCoefficients2.f[3]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorCos
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2, V4, V6, V8, V10, V12, V14, V16, V18, V20, V22;
    XMVECTOR C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11;
    XMVECTOR Result;

    V1 = XMVectorModAngles(V);

    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! + V^12 / 12! - 
    //           V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)
    V2 = XMVectorMultiply(V1, V1);
    V4 = XMVectorMultiply(V2, V2);
    V6 = XMVectorMultiply(V4, V2);
    V8 = XMVectorMultiply(V4, V4);
    V10 = XMVectorMultiply(V6, V4);
    V12 = XMVectorMultiply(V6, V6);
    V14 = XMVectorMultiply(V8, V6);
    V16 = XMVectorMultiply(V8, V8);
    V18 = XMVectorMultiply(V10, V8);
    V20 = XMVectorMultiply(V10, V10);
    V22 = XMVectorMultiply(V12, V10);

    C1 = XMVectorSplatY(g_XMCosCoefficients0.v);
    C2 = XMVectorSplatZ(g_XMCosCoefficients0.v);
    C3 = XMVectorSplatW(g_XMCosCoefficients0.v);
    C4 = XMVectorSplatX(g_XMCosCoefficients1.v);
    C5 = XMVectorSplatY(g_XMCosCoefficients1.v);
    C6 = XMVectorSplatZ(g_XMCosCoefficients1.v);
    C7 = XMVectorSplatW(g_XMCosCoefficients1.v);
    C8 = XMVectorSplatX(g_XMCosCoefficients2.v);
    C9 = XMVectorSplatY(g_XMCosCoefficients2.v);
    C10 = XMVectorSplatZ(g_XMCosCoefficients2.v);
    C11 = XMVectorSplatW(g_XMCosCoefficients2.v);

    Result = XMVectorMultiplyAdd(C1, V2, g_XMOne.v);
    Result = XMVectorMultiplyAdd(C2, V4, Result);
    Result = XMVectorMultiplyAdd(C3, V6, Result);
    Result = XMVectorMultiplyAdd(C4, V8, Result);
    Result = XMVectorMultiplyAdd(C5, V10, Result);
    Result = XMVectorMultiplyAdd(C6, V12, Result);
    Result = XMVectorMultiplyAdd(C7, V14, Result);
    Result = XMVectorMultiplyAdd(C8, V16, Result);
    Result = XMVectorMultiplyAdd(C9, V18, Result);
    Result = XMVectorMultiplyAdd(C10, V20, Result);
    Result = XMVectorMultiplyAdd(C11, V22, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Force the value within the bounds of pi
    XMVECTOR V2 = XMVectorModAngles(V);
    // Each on is V to the "num" power
    // V2 = V1^2
    V2  = _mm_mul_ps(V2,V2);
    // V^2
    XMVECTOR vConstants = _mm_load_ps1(&g_XMCosCoefficients0.f[1]);
    vConstants = _mm_mul_ps(vConstants,V2);
    XMVECTOR vResult = _mm_add_ps(vConstants,g_XMOne);

    // V^4
    XMVECTOR vPower = _mm_mul_ps(V2,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients0.f[2]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^6
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients0.f[3]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^8
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients1.f[0]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^10
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients1.f[1]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^12
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients1.f[2]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^14
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients1.f[3]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^16
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients2.f[0]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^18
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients2.f[1]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^20
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients2.f[2]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);

    // V^22
    vPower = _mm_mul_ps(vPower,V2);
    vConstants = _mm_load_ps1(&g_XMCosCoefficients2.f[3]);
    vConstants = _mm_mul_ps(vConstants,vPower);
    vResult = _mm_add_ps(vResult,vConstants);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE VOID XMVectorSinCos
(
    XMVECTOR* pSin, 
    XMVECTOR* pCos, 
    FXMVECTOR  V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13;
    XMVECTOR V14, V15, V16, V17, V18, V19, V20, V21, V22, V23;
    XMVECTOR S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11;
    XMVECTOR C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11;
    XMVECTOR Sin, Cos;

    XMASSERT(pSin);
    XMASSERT(pCos);

    V1 = XMVectorModAngles(V);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! - 
    //           V^15 / 15! + V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)
    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! + V^12 / 12! - 
    //           V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)

    V2 = XMVectorMultiply(V1, V1);
    V3 = XMVectorMultiply(V2, V1);
    V4 = XMVectorMultiply(V2, V2);
    V5 = XMVectorMultiply(V3, V2);
    V6 = XMVectorMultiply(V3, V3);
    V7 = XMVectorMultiply(V4, V3);
    V8 = XMVectorMultiply(V4, V4);
    V9 = XMVectorMultiply(V5, V4);
    V10 = XMVectorMultiply(V5, V5);
    V11 = XMVectorMultiply(V6, V5);
    V12 = XMVectorMultiply(V6, V6);
    V13 = XMVectorMultiply(V7, V6);
    V14 = XMVectorMultiply(V7, V7);
    V15 = XMVectorMultiply(V8, V7);
    V16 = XMVectorMultiply(V8, V8);
    V17 = XMVectorMultiply(V9, V8);
    V18 = XMVectorMultiply(V9, V9);
    V19 = XMVectorMultiply(V10, V9);
    V20 = XMVectorMultiply(V10, V10);
    V21 = XMVectorMultiply(V11, V10);
    V22 = XMVectorMultiply(V11, V11);
    V23 = XMVectorMultiply(V12, V11);

    S1  = XMVectorSplatY(g_XMSinCoefficients0.v);
    S2  = XMVectorSplatZ(g_XMSinCoefficients0.v);
    S3  = XMVectorSplatW(g_XMSinCoefficients0.v);
    S4  = XMVectorSplatX(g_XMSinCoefficients1.v);
    S5  = XMVectorSplatY(g_XMSinCoefficients1.v);
    S6  = XMVectorSplatZ(g_XMSinCoefficients1.v);
    S7  = XMVectorSplatW(g_XMSinCoefficients1.v);
    S8  = XMVectorSplatX(g_XMSinCoefficients2.v);
    S9  = XMVectorSplatY(g_XMSinCoefficients2.v);
    S10  = XMVectorSplatZ(g_XMSinCoefficients2.v);
    S11  = XMVectorSplatW(g_XMSinCoefficients2.v);

    C1 = XMVectorSplatY(g_XMCosCoefficients0.v);
    C2 = XMVectorSplatZ(g_XMCosCoefficients0.v);
    C3 = XMVectorSplatW(g_XMCosCoefficients0.v);
    C4 = XMVectorSplatX(g_XMCosCoefficients1.v);
    C5 = XMVectorSplatY(g_XMCosCoefficients1.v);
    C6 = XMVectorSplatZ(g_XMCosCoefficients1.v);
    C7 = XMVectorSplatW(g_XMCosCoefficients1.v);
    C8 = XMVectorSplatX(g_XMCosCoefficients2.v);
    C9 = XMVectorSplatY(g_XMCosCoefficients2.v);
    C10 = XMVectorSplatZ(g_XMCosCoefficients2.v);
    C11 = XMVectorSplatW(g_XMCosCoefficients2.v);

    Sin = XMVectorMultiplyAdd(S1, V3, V1);
    Sin = XMVectorMultiplyAdd(S2, V5, Sin);
    Sin = XMVectorMultiplyAdd(S3, V7, Sin);
    Sin = XMVectorMultiplyAdd(S4, V9, Sin);
    Sin = XMVectorMultiplyAdd(S5, V11, Sin);
    Sin = XMVectorMultiplyAdd(S6, V13, Sin);
    Sin = XMVectorMultiplyAdd(S7, V15, Sin);
    Sin = XMVectorMultiplyAdd(S8, V17, Sin);
    Sin = XMVectorMultiplyAdd(S9, V19, Sin);
    Sin = XMVectorMultiplyAdd(S10, V21, Sin);
    Sin = XMVectorMultiplyAdd(S11, V23, Sin);

    Cos = XMVectorMultiplyAdd(C1, V2, g_XMOne.v);
    Cos = XMVectorMultiplyAdd(C2, V4, Cos);
    Cos = XMVectorMultiplyAdd(C3, V6, Cos);
    Cos = XMVectorMultiplyAdd(C4, V8, Cos);
    Cos = XMVectorMultiplyAdd(C5, V10, Cos);
    Cos = XMVectorMultiplyAdd(C6, V12, Cos);
    Cos = XMVectorMultiplyAdd(C7, V14, Cos);
    Cos = XMVectorMultiplyAdd(C8, V16, Cos);
    Cos = XMVectorMultiplyAdd(C9, V18, Cos);
    Cos = XMVectorMultiplyAdd(C10, V20, Cos);
    Cos = XMVectorMultiplyAdd(C11, V22, Cos);

    *pSin = Sin;
    *pCos = Cos;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pSin);
    XMASSERT(pCos);
    XMVECTOR V1, V2, V3, V4, V5, V6, V7, V8, V9, V10, V11, V12, V13;
    XMVECTOR V14, V15, V16, V17, V18, V19, V20, V21, V22, V23;
    XMVECTOR S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11;
    XMVECTOR C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11;
    XMVECTOR Sin, Cos;

    V1 = XMVectorModAngles(V);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! - 
    //           V^15 / 15! + V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)
    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! + V^12 / 12! - 
    //           V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)

    V2 = XMVectorMultiply(V1, V1);
    V3 = XMVectorMultiply(V2, V1);
    V4 = XMVectorMultiply(V2, V2);
    V5 = XMVectorMultiply(V3, V2);
    V6 = XMVectorMultiply(V3, V3);
    V7 = XMVectorMultiply(V4, V3);
    V8 = XMVectorMultiply(V4, V4);
    V9 = XMVectorMultiply(V5, V4);
    V10 = XMVectorMultiply(V5, V5);
    V11 = XMVectorMultiply(V6, V5);
    V12 = XMVectorMultiply(V6, V6);
    V13 = XMVectorMultiply(V7, V6);
    V14 = XMVectorMultiply(V7, V7);
    V15 = XMVectorMultiply(V8, V7);
    V16 = XMVectorMultiply(V8, V8);
    V17 = XMVectorMultiply(V9, V8);
    V18 = XMVectorMultiply(V9, V9);
    V19 = XMVectorMultiply(V10, V9);
    V20 = XMVectorMultiply(V10, V10);
    V21 = XMVectorMultiply(V11, V10);
    V22 = XMVectorMultiply(V11, V11);
    V23 = XMVectorMultiply(V12, V11);

    S1  = _mm_load_ps1(&g_XMSinCoefficients0.f[1]);
    S2  = _mm_load_ps1(&g_XMSinCoefficients0.f[2]);
    S3  = _mm_load_ps1(&g_XMSinCoefficients0.f[3]);
    S4  = _mm_load_ps1(&g_XMSinCoefficients1.f[0]);
    S5  = _mm_load_ps1(&g_XMSinCoefficients1.f[1]);
    S6  = _mm_load_ps1(&g_XMSinCoefficients1.f[2]);
    S7  = _mm_load_ps1(&g_XMSinCoefficients1.f[3]);
    S8  = _mm_load_ps1(&g_XMSinCoefficients2.f[0]);
    S9  = _mm_load_ps1(&g_XMSinCoefficients2.f[1]);
    S10  = _mm_load_ps1(&g_XMSinCoefficients2.f[2]);
    S11  = _mm_load_ps1(&g_XMSinCoefficients2.f[3]);

    C1 = _mm_load_ps1(&g_XMCosCoefficients0.f[1]);
    C2 = _mm_load_ps1(&g_XMCosCoefficients0.f[2]);
    C3 = _mm_load_ps1(&g_XMCosCoefficients0.f[3]);
    C4 = _mm_load_ps1(&g_XMCosCoefficients1.f[0]);
    C5 = _mm_load_ps1(&g_XMCosCoefficients1.f[1]);
    C6 = _mm_load_ps1(&g_XMCosCoefficients1.f[2]);
    C7 = _mm_load_ps1(&g_XMCosCoefficients1.f[3]);
    C8 = _mm_load_ps1(&g_XMCosCoefficients2.f[0]);
    C9 = _mm_load_ps1(&g_XMCosCoefficients2.f[1]);
    C10 = _mm_load_ps1(&g_XMCosCoefficients2.f[2]);
    C11 = _mm_load_ps1(&g_XMCosCoefficients2.f[3]);

    S1 = _mm_mul_ps(S1,V3);
    Sin = _mm_add_ps(S1,V1);
    Sin = XMVectorMultiplyAdd(S2, V5, Sin);
    Sin = XMVectorMultiplyAdd(S3, V7, Sin);
    Sin = XMVectorMultiplyAdd(S4, V9, Sin);
    Sin = XMVectorMultiplyAdd(S5, V11, Sin);
    Sin = XMVectorMultiplyAdd(S6, V13, Sin);
    Sin = XMVectorMultiplyAdd(S7, V15, Sin);
    Sin = XMVectorMultiplyAdd(S8, V17, Sin);
    Sin = XMVectorMultiplyAdd(S9, V19, Sin);
    Sin = XMVectorMultiplyAdd(S10, V21, Sin);
    Sin = XMVectorMultiplyAdd(S11, V23, Sin);

    Cos = _mm_mul_ps(C1,V2);
    Cos = _mm_add_ps(Cos,g_XMOne);
    Cos = XMVectorMultiplyAdd(C2, V4, Cos);
    Cos = XMVectorMultiplyAdd(C3, V6, Cos);
    Cos = XMVectorMultiplyAdd(C4, V8, Cos);
    Cos = XMVectorMultiplyAdd(C5, V10, Cos);
    Cos = XMVectorMultiplyAdd(C6, V12, Cos);
    Cos = XMVectorMultiplyAdd(C7, V14, Cos);
    Cos = XMVectorMultiplyAdd(C8, V16, Cos);
    Cos = XMVectorMultiplyAdd(C9, V18, Cos);
    Cos = XMVectorMultiplyAdd(C10, V20, Cos);
    Cos = XMVectorMultiplyAdd(C11, V22, Cos);

    *pSin = Sin;
    *pCos = Cos;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorTan
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    // Cody and Waite algorithm to compute tangent.

    XMVECTOR VA, VB, VC, VC2;
    XMVECTOR T0, T1, T2, T3, T4, T5, T6, T7;
    XMVECTOR C0, C1, TwoDivPi, Epsilon;
    XMVECTOR N, D;
    XMVECTOR R0, R1;
    XMVECTOR VIsZero, VCNearZero, VBIsEven;
    XMVECTOR Zero;
    XMVECTOR Result;
    UINT     i;
    static CONST XMVECTOR TanCoefficients0 = {1.0f, -4.667168334e-1f, 2.566383229e-2f, -3.118153191e-4f};
    static CONST XMVECTOR TanCoefficients1 = {4.981943399e-7f, -1.333835001e-1f, 3.424887824e-3f, -1.786170734e-5f};
    static CONST XMVECTOR TanConstants = {1.570796371f, 6.077100628e-11f, 0.000244140625f, 2.0f / XM_PI};
    static CONST XMVECTORU32 Mask = {0x1, 0x1, 0x1, 0x1};

    TwoDivPi = XMVectorSplatW(TanConstants);

    Zero = XMVectorZero();

    C0 = XMVectorSplatX(TanConstants);
    C1 = XMVectorSplatY(TanConstants);
    Epsilon = XMVectorSplatZ(TanConstants);

    VA = XMVectorMultiply(V, TwoDivPi);

    VA = XMVectorRound(VA);

    VC = XMVectorNegativeMultiplySubtract(VA, C0, V);

    VB = XMVectorAbs(VA);

    VC = XMVectorNegativeMultiplySubtract(VA, C1, VC);

    for (i = 0; i < 4; i++)
    {
        VB.vector4_u32[i] = (UINT)VB.vector4_f32[i];
    }

    VC2 = XMVectorMultiply(VC, VC);

    T7 = XMVectorSplatW(TanCoefficients1);
    T6 = XMVectorSplatZ(TanCoefficients1);
    T4 = XMVectorSplatX(TanCoefficients1);
    T3 = XMVectorSplatW(TanCoefficients0);
    T5 = XMVectorSplatY(TanCoefficients1);
    T2 = XMVectorSplatZ(TanCoefficients0);
    T1 = XMVectorSplatY(TanCoefficients0);
    T0 = XMVectorSplatX(TanCoefficients0);

    VBIsEven = XMVectorAndInt(VB, Mask.v);
    VBIsEven = XMVectorEqualInt(VBIsEven, Zero);

    N = XMVectorMultiplyAdd(VC2, T7, T6);
    D = XMVectorMultiplyAdd(VC2, T4, T3);
    N = XMVectorMultiplyAdd(VC2, N, T5);
    D = XMVectorMultiplyAdd(VC2, D, T2);
    N = XMVectorMultiply(VC2, N);
    D = XMVectorMultiplyAdd(VC2, D, T1);
    N = XMVectorMultiplyAdd(VC, N, VC);
    VCNearZero = XMVectorInBounds(VC, Epsilon);
    D = XMVectorMultiplyAdd(VC2, D, T0);

    N = XMVectorSelect(N, VC, VCNearZero);
    D = XMVectorSelect(D, g_XMOne.v, VCNearZero);

    R0 = XMVectorNegate(N);
    R1 = XMVectorReciprocal(D);
    R0 = XMVectorReciprocal(R0);
    R1 = XMVectorMultiply(N, R1);
    R0 = XMVectorMultiply(D, R0);

    VIsZero = XMVectorEqual(V, Zero);

    Result = XMVectorSelect(R0, R1, VBIsEven);

    Result = XMVectorSelect(Result, Zero, VIsZero);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Cody and Waite algorithm to compute tangent.

    XMVECTOR VA, VB, VC, VC2;
    XMVECTOR T0, T1, T2, T3, T4, T5, T6, T7;
    XMVECTOR C0, C1, TwoDivPi, Epsilon;
    XMVECTOR N, D;
    XMVECTOR R0, R1;
    XMVECTOR VIsZero, VCNearZero, VBIsEven;
    XMVECTOR Zero;
    XMVECTOR Result;
    static CONST XMVECTORF32 TanCoefficients0 = {1.0f, -4.667168334e-1f, 2.566383229e-2f, -3.118153191e-4f};
    static CONST XMVECTORF32 TanCoefficients1 = {4.981943399e-7f, -1.333835001e-1f, 3.424887824e-3f, -1.786170734e-5f};
    static CONST XMVECTORF32 TanConstants = {1.570796371f, 6.077100628e-11f, 0.000244140625f, 2.0f / XM_PI};
    static CONST XMVECTORI32 Mask = {0x1, 0x1, 0x1, 0x1};

    TwoDivPi = XMVectorSplatW(TanConstants);

    Zero = XMVectorZero();

    C0 = XMVectorSplatX(TanConstants);
    C1 = XMVectorSplatY(TanConstants);
    Epsilon = XMVectorSplatZ(TanConstants);

    VA = XMVectorMultiply(V, TwoDivPi);

    VA = XMVectorRound(VA);

    VC = XMVectorNegativeMultiplySubtract(VA, C0, V);

    VB = XMVectorAbs(VA);

    VC = XMVectorNegativeMultiplySubtract(VA, C1, VC);

    reinterpret_cast<__m128i *>(&VB)[0] = _mm_cvttps_epi32(VB);

    VC2 = XMVectorMultiply(VC, VC);

    T7 = XMVectorSplatW(TanCoefficients1);
    T6 = XMVectorSplatZ(TanCoefficients1);
    T4 = XMVectorSplatX(TanCoefficients1);
    T3 = XMVectorSplatW(TanCoefficients0);
    T5 = XMVectorSplatY(TanCoefficients1);
    T2 = XMVectorSplatZ(TanCoefficients0);
    T1 = XMVectorSplatY(TanCoefficients0);
    T0 = XMVectorSplatX(TanCoefficients0);

    VBIsEven = XMVectorAndInt(VB,Mask);
    VBIsEven = XMVectorEqualInt(VBIsEven, Zero);

    N = XMVectorMultiplyAdd(VC2, T7, T6);
    D = XMVectorMultiplyAdd(VC2, T4, T3);
    N = XMVectorMultiplyAdd(VC2, N, T5);
    D = XMVectorMultiplyAdd(VC2, D, T2);
    N = XMVectorMultiply(VC2, N);
    D = XMVectorMultiplyAdd(VC2, D, T1);
    N = XMVectorMultiplyAdd(VC, N, VC);
    VCNearZero = XMVectorInBounds(VC, Epsilon);
    D = XMVectorMultiplyAdd(VC2, D, T0);

    N = XMVectorSelect(N, VC, VCNearZero);
    D = XMVectorSelect(D, g_XMOne, VCNearZero);
    R0 = XMVectorNegate(N);
    R1 = _mm_div_ps(N,D);
    R0 = _mm_div_ps(D,R0);
    VIsZero = XMVectorEqual(V, Zero);
    Result = XMVectorSelect(R0, R1, VBIsEven);
    Result = XMVectorSelect(Result, Zero, VIsZero);

    return Result;

#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorSinH
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTORF32 Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = XMVectorMultiplyAdd(V, Scale.v, g_XMNegativeOne.v);
    V2 = XMVectorNegativeMultiplySubtract(V, Scale.v, g_XMNegativeOne.v);

    E1 = XMVectorExp(V1);
    E2 = XMVectorExp(V2);

    Result = XMVectorSubtract(E1, E2);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTORF32 Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = _mm_mul_ps(V, Scale);
    V1 = _mm_add_ps(V1,g_XMNegativeOne);
    V2 = _mm_mul_ps(V, Scale);
    V2 = _mm_sub_ps(g_XMNegativeOne,V2);
    E1 = XMVectorExp(V1);
    E2 = XMVectorExp(V2);

    Result = _mm_sub_ps(E1, E2);

    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorCosH
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTOR Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = XMVectorMultiplyAdd(V, Scale, g_XMNegativeOne.v);
    V2 = XMVectorNegativeMultiplySubtract(V, Scale, g_XMNegativeOne.v);

    E1 = XMVectorExp(V1);
    E2 = XMVectorExp(V2);

    Result = XMVectorAdd(E1, E2);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTORF32 Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = _mm_mul_ps(V,Scale);
    V1 = _mm_add_ps(V1,g_XMNegativeOne);
    V2 = _mm_mul_ps(V, Scale);
    V2 = _mm_sub_ps(g_XMNegativeOne,V2);
    E1 = XMVectorExp(V1);
    E2 = XMVectorExp(V2);
    Result = _mm_add_ps(E1, E2);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorTanH
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR E;
    XMVECTOR Result;
    static CONST XMVECTORF32 Scale = {2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f}; // 2.0f / ln(2.0f)

    E = XMVectorMultiply(V, Scale.v);
    E = XMVectorExp(E);
    E = XMVectorMultiplyAdd(E, g_XMOneHalf.v, g_XMOneHalf.v);
    E = XMVectorReciprocal(E);

    Result = XMVectorSubtract(g_XMOne.v, E);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 Scale = {2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f}; // 2.0f / ln(2.0f)

    XMVECTOR E = _mm_mul_ps(V, Scale);
    E = XMVectorExp(E);
    E = _mm_mul_ps(E,g_XMOneHalf);
    E = _mm_add_ps(E,g_XMOneHalf);
    E = XMVectorReciprocal(E);
    E = _mm_sub_ps(g_XMOne, E);
    return E;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorASin
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V2, V3, AbsV;
    XMVECTOR C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11;
    XMVECTOR R0, R1, R2, R3, R4;
    XMVECTOR OneMinusAbsV;
    XMVECTOR Rsq;
    XMVECTOR Result;
    static CONST XMVECTOR OnePlusEpsilon = {1.00000011921f, 1.00000011921f, 1.00000011921f, 1.00000011921f};

    // asin(V) = V * (C0 + C1 * V + C2 * V^2 + C3 * V^3 + C4 * V^4 + C5 * V^5) + (1 - V) * rsq(1 - V) * 
    //           V * (C6 + C7 * V + C8 * V^2 + C9 * V^3 + C10 * V^4 + C11 * V^5)

    AbsV = XMVectorAbs(V);

    V2 = XMVectorMultiply(V, V);
    V3 = XMVectorMultiply(V2, AbsV);

    R4 = XMVectorNegativeMultiplySubtract(AbsV, V, V);

    OneMinusAbsV = XMVectorSubtract(OnePlusEpsilon, AbsV);
    Rsq = XMVectorReciprocalSqrt(OneMinusAbsV);

    C0 = XMVectorSplatX(g_XMASinCoefficients0.v);
    C1 = XMVectorSplatY(g_XMASinCoefficients0.v);
    C2 = XMVectorSplatZ(g_XMASinCoefficients0.v);
    C3 = XMVectorSplatW(g_XMASinCoefficients0.v);

    C4 = XMVectorSplatX(g_XMASinCoefficients1.v);
    C5 = XMVectorSplatY(g_XMASinCoefficients1.v);
    C6 = XMVectorSplatZ(g_XMASinCoefficients1.v);
    C7 = XMVectorSplatW(g_XMASinCoefficients1.v);

    C8 = XMVectorSplatX(g_XMASinCoefficients2.v);
    C9 = XMVectorSplatY(g_XMASinCoefficients2.v);
    C10 = XMVectorSplatZ(g_XMASinCoefficients2.v);
    C11 = XMVectorSplatW(g_XMASinCoefficients2.v);

    R0 = XMVectorMultiplyAdd(C3, AbsV, C7);
    R1 = XMVectorMultiplyAdd(C1, AbsV, C5);
    R2 = XMVectorMultiplyAdd(C2, AbsV, C6);
    R3 = XMVectorMultiplyAdd(C0, AbsV, C4);

    R0 = XMVectorMultiplyAdd(R0, AbsV, C11);
    R1 = XMVectorMultiplyAdd(R1, AbsV, C9);
    R2 = XMVectorMultiplyAdd(R2, AbsV, C10);
    R3 = XMVectorMultiplyAdd(R3, AbsV, C8);

    R0 = XMVectorMultiplyAdd(R2, V3, R0);
    R1 = XMVectorMultiplyAdd(R3, V3, R1);

    R0 = XMVectorMultiply(V, R0);
    R1 = XMVectorMultiply(R4, R1);

    Result = XMVectorMultiplyAdd(R1, Rsq, R0);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 OnePlusEpsilon = {1.00000011921f, 1.00000011921f, 1.00000011921f, 1.00000011921f};

    // asin(V) = V * (C0 + C1 * V + C2 * V^2 + C3 * V^3 + C4 * V^4 + C5 * V^5) + (1 - V) * rsq(1 - V) * 
    //           V * (C6 + C7 * V + C8 * V^2 + C9 * V^3 + C10 * V^4 + C11 * V^5)
    // Get abs(V)
	XMVECTOR vAbsV = _mm_setzero_ps();
	vAbsV = _mm_sub_ps(vAbsV,V);
	vAbsV = _mm_max_ps(vAbsV,V);

    XMVECTOR R0 = vAbsV;
    XMVECTOR vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[3]);
    R0 = _mm_mul_ps(R0,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[3]);
    R0 = _mm_add_ps(R0,vConstants);

    XMVECTOR R1 = vAbsV;
    vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[1]);
    R1 = _mm_mul_ps(R1,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[1]);
    R1 = _mm_add_ps(R1, vConstants);

    XMVECTOR R2 = vAbsV;
    vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[2]);
    R2 = _mm_mul_ps(R2,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[2]);
    R2 = _mm_add_ps(R2, vConstants);

    XMVECTOR R3 = vAbsV;
    vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[0]);
    R3 = _mm_mul_ps(R3,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[0]);
    R3 = _mm_add_ps(R3, vConstants);

    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[3]);
    R0 = _mm_mul_ps(R0,vAbsV);
    R0 = _mm_add_ps(R0,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[1]);
    R1 = _mm_mul_ps(R1,vAbsV);
    R1 = _mm_add_ps(R1,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[2]);
    R2 = _mm_mul_ps(R2,vAbsV);
    R2 = _mm_add_ps(R2,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[0]);
    R3 = _mm_mul_ps(R3,vAbsV);
    R3 = _mm_add_ps(R3,vConstants);

    // V3 = V^3
    vConstants = _mm_mul_ps(V,V);
    vConstants = _mm_mul_ps(vConstants, vAbsV);
    // Mul by V^3
    R2 = _mm_mul_ps(R2,vConstants);
    R3 = _mm_mul_ps(R3,vConstants);
    // Merge the results
    R0 = _mm_add_ps(R0,R2);
    R1 = _mm_add_ps(R1,R3);

    R0 = _mm_mul_ps(R0,V);
    // vConstants = V-(V^2 retaining sign)
    vConstants = _mm_mul_ps(vAbsV, V);
    vConstants = _mm_sub_ps(V,vConstants);
    R1 = _mm_mul_ps(R1,vConstants);
    vConstants = _mm_sub_ps(OnePlusEpsilon,vAbsV);
    // Do NOT use rsqrt/mul. This needs the precision
    vConstants = _mm_sqrt_ps(vConstants);
    R1 = _mm_div_ps(R1,vConstants);
    R0 = _mm_add_ps(R0,R1);
    return R0;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorACos
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V2, V3, AbsV;
    XMVECTOR C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11;
    XMVECTOR R0, R1, R2, R3, R4;
    XMVECTOR OneMinusAbsV;
    XMVECTOR Rsq;
    XMVECTOR Result;
    static CONST XMVECTOR OnePlusEpsilon = {1.00000011921f, 1.00000011921f, 1.00000011921f, 1.00000011921f};

    // acos(V) = PI / 2 - asin(V)

    AbsV = XMVectorAbs(V);

    V2 = XMVectorMultiply(V, V);
    V3 = XMVectorMultiply(V2, AbsV);

    R4 = XMVectorNegativeMultiplySubtract(AbsV, V, V);

    OneMinusAbsV = XMVectorSubtract(OnePlusEpsilon, AbsV);
    Rsq = XMVectorReciprocalSqrt(OneMinusAbsV);

    C0 = XMVectorSplatX(g_XMASinCoefficients0.v);
    C1 = XMVectorSplatY(g_XMASinCoefficients0.v);
    C2 = XMVectorSplatZ(g_XMASinCoefficients0.v);
    C3 = XMVectorSplatW(g_XMASinCoefficients0.v);

    C4 = XMVectorSplatX(g_XMASinCoefficients1.v);
    C5 = XMVectorSplatY(g_XMASinCoefficients1.v);
    C6 = XMVectorSplatZ(g_XMASinCoefficients1.v);
    C7 = XMVectorSplatW(g_XMASinCoefficients1.v);

    C8 = XMVectorSplatX(g_XMASinCoefficients2.v);
    C9 = XMVectorSplatY(g_XMASinCoefficients2.v);
    C10 = XMVectorSplatZ(g_XMASinCoefficients2.v);
    C11 = XMVectorSplatW(g_XMASinCoefficients2.v);

    R0 = XMVectorMultiplyAdd(C3, AbsV, C7);
    R1 = XMVectorMultiplyAdd(C1, AbsV, C5);
    R2 = XMVectorMultiplyAdd(C2, AbsV, C6);
    R3 = XMVectorMultiplyAdd(C0, AbsV, C4);

    R0 = XMVectorMultiplyAdd(R0, AbsV, C11);
    R1 = XMVectorMultiplyAdd(R1, AbsV, C9);
    R2 = XMVectorMultiplyAdd(R2, AbsV, C10);
    R3 = XMVectorMultiplyAdd(R3, AbsV, C8);

    R0 = XMVectorMultiplyAdd(R2, V3, R0);
    R1 = XMVectorMultiplyAdd(R3, V3, R1);

    R0 = XMVectorMultiply(V, R0);
    R1 = XMVectorMultiply(R4, R1);

    Result = XMVectorMultiplyAdd(R1, Rsq, R0);

    Result = XMVectorSubtract(g_XMHalfPi.v, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 OnePlusEpsilon = {1.00000011921f, 1.00000011921f, 1.00000011921f, 1.00000011921f};
    // Uses only 6 registers for good code on x86 targets
    // acos(V) = PI / 2 - asin(V)
    // Get abs(V)
	XMVECTOR vAbsV = _mm_setzero_ps();
	vAbsV = _mm_sub_ps(vAbsV,V);
	vAbsV = _mm_max_ps(vAbsV,V);
    // Perform the series in precision groups to
    // retain precision across 20 bits. (3 bits of imprecision due to operations)
    XMVECTOR R0 = vAbsV;
    XMVECTOR vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[3]);
    R0 = _mm_mul_ps(R0,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[3]);
    R0 = _mm_add_ps(R0,vConstants);
    R0 = _mm_mul_ps(R0,vAbsV);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[3]);
    R0 = _mm_add_ps(R0,vConstants);

    XMVECTOR R1 = vAbsV;
    vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[1]);
    R1 = _mm_mul_ps(R1,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[1]);
    R1 = _mm_add_ps(R1,vConstants);
    R1 = _mm_mul_ps(R1, vAbsV);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[1]);
    R1 = _mm_add_ps(R1,vConstants);

    XMVECTOR R2 = vAbsV;
    vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[2]);
    R2 = _mm_mul_ps(R2,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[2]);
    R2 = _mm_add_ps(R2,vConstants);
    R2 = _mm_mul_ps(R2, vAbsV);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[2]);
    R2 = _mm_add_ps(R2,vConstants);

    XMVECTOR R3 = vAbsV;
    vConstants = _mm_load_ps1(&g_XMASinCoefficients0.f[0]);
    R3 = _mm_mul_ps(R3,vConstants);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients1.f[0]);
    R3 = _mm_add_ps(R3,vConstants);
    R3 = _mm_mul_ps(R3, vAbsV);
    vConstants = _mm_load_ps1(&g_XMASinCoefficients2.f[0]);
    R3 = _mm_add_ps(R3,vConstants);

    // vConstants = V^3
    vConstants = _mm_mul_ps(V,V);
    vConstants = _mm_mul_ps(vConstants,vAbsV);
    R2 = _mm_mul_ps(R2,vConstants);
    R3 = _mm_mul_ps(R3,vConstants);
    // Add the pair of values together here to retain
    // as much precision as possible
    R0 = _mm_add_ps(R0,R2);
    R1 = _mm_add_ps(R1,R3);

    R0 = _mm_mul_ps(R0,V);
    // vConstants = V-(V*abs(V))
    vConstants = _mm_mul_ps(V,vAbsV);
    vConstants = _mm_sub_ps(V,vConstants);
    R1 = _mm_mul_ps(R1,vConstants);
    // Episilon exists to allow 1.0 as an answer
    vConstants = _mm_sub_ps(OnePlusEpsilon, vAbsV);
    // Use sqrt instead of rsqrt for precision
    vConstants = _mm_sqrt_ps(vConstants);
    R1 = _mm_div_ps(R1,vConstants);
    R1 = _mm_add_ps(R1,R0);
    vConstants = _mm_sub_ps(g_XMHalfPi,R1);
    return vConstants;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorATan
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    // Cody and Waite algorithm to compute inverse tangent.

    XMVECTOR N, D;
    XMVECTOR VF, G, ReciprocalF, AbsF, FA, FB;
    XMVECTOR Sqrt3, Sqrt3MinusOne, TwoMinusSqrt3;
    XMVECTOR HalfPi, OneThirdPi, OneSixthPi, Epsilon, MinV, MaxV;
    XMVECTOR Zero;
    XMVECTOR NegativeHalfPi;
    XMVECTOR Angle1, Angle2;
    XMVECTOR F_GT_One, F_GT_TwoMinusSqrt3, AbsF_LT_Epsilon, V_LT_Zero, V_GT_MaxV, V_LT_MinV;
    XMVECTOR NegativeResult, Result;
    XMVECTOR P0, P1, P2, P3, Q0, Q1, Q2, Q3;
    static CONST XMVECTOR ATanConstants0 = {-1.3688768894e+1f, -2.0505855195e+1f, -8.4946240351f, -8.3758299368e-1f};
    static CONST XMVECTOR ATanConstants1 = {4.1066306682e+1f, 8.6157349597e+1f, 5.9578436142e+1f, 1.5024001160e+1f};
    static CONST XMVECTOR ATanConstants2 = {1.732050808f, 7.320508076e-1f, 2.679491924e-1f, 0.000244140625f}; // <sqrt(3), sqrt(3) - 1, 2 - sqrt(3), Epsilon>
    static CONST XMVECTOR ATanConstants3 = {XM_PIDIV2, XM_PI / 3.0f, XM_PI / 6.0f, 8.507059173e+37f}; // <Pi / 2, Pi / 3, Pi / 6, MaxV>

    Zero = XMVectorZero();

    P0 = XMVectorSplatX(ATanConstants0);
    P1 = XMVectorSplatY(ATanConstants0);
    P2 = XMVectorSplatZ(ATanConstants0);
    P3 = XMVectorSplatW(ATanConstants0);

    Q0 = XMVectorSplatX(ATanConstants1);
    Q1 = XMVectorSplatY(ATanConstants1);
    Q2 = XMVectorSplatZ(ATanConstants1);
    Q3 = XMVectorSplatW(ATanConstants1);

    Sqrt3 = XMVectorSplatX(ATanConstants2);
    Sqrt3MinusOne = XMVectorSplatY(ATanConstants2);
    TwoMinusSqrt3 = XMVectorSplatZ(ATanConstants2);
    Epsilon = XMVectorSplatW(ATanConstants2);

    HalfPi = XMVectorSplatX(ATanConstants3);
    OneThirdPi = XMVectorSplatY(ATanConstants3);
    OneSixthPi = XMVectorSplatZ(ATanConstants3);
    MaxV = XMVectorSplatW(ATanConstants3);

    VF = XMVectorAbs(V);
    ReciprocalF = XMVectorReciprocal(VF);

    F_GT_One = XMVectorGreater(VF, g_XMOne.v);

    VF = XMVectorSelect(VF, ReciprocalF, F_GT_One);
    Angle1 = XMVectorSelect(Zero, HalfPi, F_GT_One);
    Angle2 = XMVectorSelect(OneSixthPi, OneThirdPi, F_GT_One);

    F_GT_TwoMinusSqrt3 = XMVectorGreater(VF, TwoMinusSqrt3);

    FA = XMVectorMultiplyAdd(Sqrt3MinusOne, VF, VF);
    FA = XMVectorAdd(FA, g_XMNegativeOne.v);
    FB = XMVectorAdd(VF, Sqrt3);
    FB = XMVectorReciprocal(FB);
    FA = XMVectorMultiply(FA, FB);

    VF = XMVectorSelect(VF, FA, F_GT_TwoMinusSqrt3);
    Angle1 = XMVectorSelect(Angle1, Angle2, F_GT_TwoMinusSqrt3);

    AbsF = XMVectorAbs(VF);
    AbsF_LT_Epsilon = XMVectorLess(AbsF, Epsilon);

    G = XMVectorMultiply(VF, VF);

    D = XMVectorAdd(G, Q3);
    D = XMVectorMultiplyAdd(D, G, Q2);
    D = XMVectorMultiplyAdd(D, G, Q1);
    D = XMVectorMultiplyAdd(D, G, Q0);
    D = XMVectorReciprocal(D);

    N = XMVectorMultiplyAdd(P3, G, P2);
    N = XMVectorMultiplyAdd(N, G, P1);
    N = XMVectorMultiplyAdd(N, G, P0);
    N = XMVectorMultiply(N, G);
    Result = XMVectorMultiply(N, D);

    Result = XMVectorMultiplyAdd(Result, VF, VF);

    Result = XMVectorSelect(Result, VF, AbsF_LT_Epsilon);

    NegativeResult = XMVectorNegate(Result);
    Result = XMVectorSelect(Result, NegativeResult, F_GT_One);

    Result = XMVectorAdd(Result, Angle1);

    V_LT_Zero = XMVectorLess(V, Zero);
    NegativeResult = XMVectorNegate(Result);
    Result = XMVectorSelect(Result, NegativeResult, V_LT_Zero);

    MinV = XMVectorNegate(MaxV);
    NegativeHalfPi = XMVectorNegate(HalfPi);
    V_GT_MaxV = XMVectorGreater(V, MaxV);
    V_LT_MinV = XMVectorLess(V, MinV);
    Result = XMVectorSelect(Result, g_XMHalfPi.v, V_GT_MaxV);
    Result = XMVectorSelect(Result, NegativeHalfPi, V_LT_MinV);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 ATanConstants0 = {-1.3688768894e+1f, -2.0505855195e+1f, -8.4946240351f, -8.3758299368e-1f};
    static CONST XMVECTORF32 ATanConstants1 = {4.1066306682e+1f, 8.6157349597e+1f, 5.9578436142e+1f, 1.5024001160e+1f};
    static CONST XMVECTORF32 ATanConstants2 = {1.732050808f, 7.320508076e-1f, 2.679491924e-1f, 0.000244140625f}; // <sqrt(3), sqrt(3) - 1, 2 - sqrt(3), Epsilon>
    static CONST XMVECTORF32 ATanConstants3 = {XM_PIDIV2, XM_PI / 3.0f, XM_PI / 6.0f, 8.507059173e+37f}; // <Pi / 2, Pi / 3, Pi / 6, MaxV>

    XMVECTOR VF = XMVectorAbs(V);
    XMVECTOR F_GT_One = _mm_cmpgt_ps(VF,g_XMOne);
    XMVECTOR ReciprocalF = XMVectorReciprocal(VF);
    VF = XMVectorSelect(VF, ReciprocalF, F_GT_One);
    XMVECTOR Zero = XMVectorZero();
    XMVECTOR HalfPi = _mm_load_ps1(&ATanConstants3.f[0]);
    XMVECTOR Angle1 = XMVectorSelect(Zero, HalfPi, F_GT_One);
    // Pi/3
    XMVECTOR vConstants = _mm_load_ps1(&ATanConstants3.f[1]);
    // Pi/6
    XMVECTOR Angle2 = _mm_load_ps1(&ATanConstants3.f[2]);
    Angle2 = XMVectorSelect(Angle2, vConstants, F_GT_One);

    // 1-sqrt(3)
    XMVECTOR FA = _mm_load_ps1(&ATanConstants2.f[1]);
    FA = _mm_mul_ps(FA,VF);
    FA = _mm_add_ps(FA,VF);
    FA = _mm_add_ps(FA,g_XMNegativeOne);
    // sqrt(3)
    vConstants = _mm_load_ps1(&ATanConstants2.f[0]);
    vConstants = _mm_add_ps(vConstants,VF);
    FA = _mm_div_ps(FA,vConstants);

    // 2-sqrt(3)
    vConstants = _mm_load_ps1(&ATanConstants2.f[2]);
    // >2-sqrt(3)?
    vConstants = _mm_cmpgt_ps(VF,vConstants);
    VF = XMVectorSelect(VF, FA, vConstants);
    Angle1 = XMVectorSelect(Angle1, Angle2, vConstants);

    XMVECTOR AbsF = XMVectorAbs(VF);

    XMVECTOR G = _mm_mul_ps(VF,VF);
    XMVECTOR D = _mm_load_ps1(&ATanConstants1.f[3]);
    D = _mm_add_ps(D,G);
    D = _mm_mul_ps(D,G);
    vConstants = _mm_load_ps1(&ATanConstants1.f[2]);
    D = _mm_add_ps(D,vConstants);
    D = _mm_mul_ps(D,G);
    vConstants = _mm_load_ps1(&ATanConstants1.f[1]);
    D = _mm_add_ps(D,vConstants);
    D = _mm_mul_ps(D,G);
    vConstants = _mm_load_ps1(&ATanConstants1.f[0]);
    D = _mm_add_ps(D,vConstants);

    XMVECTOR N = _mm_load_ps1(&ATanConstants0.f[3]);
    N = _mm_mul_ps(N,G);
    vConstants = _mm_load_ps1(&ATanConstants0.f[2]);
    N = _mm_add_ps(N,vConstants);
    N = _mm_mul_ps(N,G);
    vConstants = _mm_load_ps1(&ATanConstants0.f[1]);
    N = _mm_add_ps(N,vConstants);
    N = _mm_mul_ps(N,G);
    vConstants = _mm_load_ps1(&ATanConstants0.f[0]);
    N = _mm_add_ps(N,vConstants);
    N = _mm_mul_ps(N,G);
    XMVECTOR Result = _mm_div_ps(N,D);

    Result = _mm_mul_ps(Result,VF);
    Result = _mm_add_ps(Result,VF);
    // Epsilon
    vConstants = _mm_load_ps1(&ATanConstants2.f[3]);
    vConstants = _mm_cmpge_ps(vConstants,AbsF);
    Result = XMVectorSelect(Result,VF,vConstants);

    XMVECTOR NegativeResult = _mm_mul_ps(Result,g_XMNegativeOne);
    Result = XMVectorSelect(Result,NegativeResult,F_GT_One);
    Result = _mm_add_ps(Result,Angle1);

    Zero = _mm_cmpge_ps(Zero,V);
    NegativeResult = _mm_mul_ps(Result,g_XMNegativeOne);
    Result = XMVectorSelect(Result,NegativeResult,Zero);

    XMVECTOR MaxV = _mm_load_ps1(&ATanConstants3.f[3]);
    XMVECTOR MinV = _mm_mul_ps(MaxV,g_XMNegativeOne);
    // Negate HalfPi
    HalfPi = _mm_mul_ps(HalfPi,g_XMNegativeOne);
    MaxV = _mm_cmple_ps(MaxV,V);
    MinV = _mm_cmpge_ps(MinV,V);
    Result = XMVectorSelect(Result,g_XMHalfPi,MaxV);
    // HalfPi = -HalfPi
    Result = XMVectorSelect(Result,HalfPi,MinV);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVectorATan2
(
    FXMVECTOR Y, 
    FXMVECTOR X
)
{
#if defined(_XM_NO_INTRINSICS_)

    // Return the inverse tangent of Y / X in the range of -Pi to Pi with the following exceptions:

    //     Y == 0 and X is Negative         -> Pi with the sign of Y
    //     y == 0 and x is positive         -> 0 with the sign of y
    //     Y != 0 and X == 0                -> Pi / 2 with the sign of Y
    //     Y != 0 and X is Negative         -> atan(y/x) + (PI with the sign of Y)
    //     X == -Infinity and Finite Y      -> Pi with the sign of Y
    //     X == +Infinity and Finite Y      -> 0 with the sign of Y
    //     Y == Infinity and X is Finite    -> Pi / 2 with the sign of Y
    //     Y == Infinity and X == -Infinity -> 3Pi / 4 with the sign of Y
    //     Y == Infinity and X == +Infinity -> Pi / 4 with the sign of Y

    XMVECTOR Reciprocal;
    XMVECTOR V;
    XMVECTOR YSign;
    XMVECTOR Pi, PiOverTwo, PiOverFour, ThreePiOverFour;
    XMVECTOR YEqualsZero, XEqualsZero, XIsPositive, YEqualsInfinity, XEqualsInfinity;
    XMVECTOR ATanResultValid;
    XMVECTOR R0, R1, R2, R3, R4, R5;
    XMVECTOR Zero;
    XMVECTOR Result;
    static CONST XMVECTOR ATan2Constants = {XM_PI, XM_PIDIV2, XM_PIDIV4, XM_PI * 3.0f / 4.0f};

    Zero = XMVectorZero();
    ATanResultValid = XMVectorTrueInt();

    Pi = XMVectorSplatX(ATan2Constants);
    PiOverTwo = XMVectorSplatY(ATan2Constants);
    PiOverFour = XMVectorSplatZ(ATan2Constants);
    ThreePiOverFour = XMVectorSplatW(ATan2Constants);

    YEqualsZero = XMVectorEqual(Y, Zero);
    XEqualsZero = XMVectorEqual(X, Zero);
    XIsPositive = XMVectorAndInt(X, g_XMNegativeZero.v);
    XIsPositive = XMVectorEqualInt(XIsPositive, Zero);
    YEqualsInfinity = XMVectorIsInfinite(Y);
    XEqualsInfinity = XMVectorIsInfinite(X);

    YSign = XMVectorAndInt(Y, g_XMNegativeZero.v);
    Pi = XMVectorOrInt(Pi, YSign);
    PiOverTwo = XMVectorOrInt(PiOverTwo, YSign);
    PiOverFour = XMVectorOrInt(PiOverFour, YSign);
    ThreePiOverFour = XMVectorOrInt(ThreePiOverFour, YSign);

    R1 = XMVectorSelect(Pi, YSign, XIsPositive);
    R2 = XMVectorSelect(ATanResultValid, PiOverTwo, XEqualsZero);
    R3 = XMVectorSelect(R2, R1, YEqualsZero);
    R4 = XMVectorSelect(ThreePiOverFour, PiOverFour, XIsPositive);
    R5 = XMVectorSelect(PiOverTwo, R4, XEqualsInfinity);
    Result = XMVectorSelect(R3, R5, YEqualsInfinity);
    ATanResultValid = XMVectorEqualInt(Result, ATanResultValid);

    Reciprocal = XMVectorReciprocal(X);
    V = XMVectorMultiply(Y, Reciprocal);
    R0 = XMVectorATan(V);

    R1 = XMVectorSelect( Pi, Zero, XIsPositive );
    R2 = XMVectorAdd(R0, R1);

    Result = XMVectorSelect(Result, R2, ATanResultValid);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 ATan2Constants = {XM_PI, XM_PIDIV2, XM_PIDIV4, XM_PI * 3.0f / 4.0f};

    // Mask if Y>0 && Y!=INF
    XMVECTOR YEqualsInfinity = XMVectorIsInfinite(Y);
    // Get the sign of (Y&0x80000000)
    XMVECTOR YSign = _mm_and_ps(Y, g_XMNegativeZero);
    // Get the sign bits of X
    XMVECTOR XIsPositive = _mm_and_ps(X,g_XMNegativeZero);
    // Change them to masks
    XIsPositive = XMVectorEqualInt(XIsPositive,g_XMZero);
    // Get Pi
    XMVECTOR Pi = _mm_load_ps1(&ATan2Constants.f[0]);
    // Copy the sign of Y
    Pi = _mm_or_ps(Pi,YSign);
    XMVECTOR R1 = XMVectorSelect(Pi,YSign,XIsPositive);
    // Mask for X==0
    XMVECTOR vConstants = _mm_cmpeq_ps(X,g_XMZero);
    // Get Pi/2 with with sign of Y
    XMVECTOR PiOverTwo = _mm_load_ps1(&ATan2Constants.f[1]);
    PiOverTwo = _mm_or_ps(PiOverTwo,YSign);
    XMVECTOR R2 = XMVectorSelect(g_XMNegOneMask,PiOverTwo,vConstants);
    // Mask for Y==0
    vConstants = _mm_cmpeq_ps(Y,g_XMZero);
    R2 = XMVectorSelect(R2,R1,vConstants);
    // Get Pi/4 with sign of Y
    XMVECTOR PiOverFour = _mm_load_ps1(&ATan2Constants.f[2]);
    PiOverFour = _mm_or_ps(PiOverFour,YSign);
    // Get (Pi*3)/4 with sign of Y
    XMVECTOR ThreePiOverFour = _mm_load_ps1(&ATan2Constants.f[3]);
    ThreePiOverFour = _mm_or_ps(ThreePiOverFour,YSign);
    vConstants = XMVectorSelect(ThreePiOverFour, PiOverFour, XIsPositive);
    XMVECTOR XEqualsInfinity = XMVectorIsInfinite(X);
    vConstants = XMVectorSelect(PiOverTwo,vConstants,XEqualsInfinity);

    XMVECTOR vResult = XMVectorSelect(R2,vConstants,YEqualsInfinity);
    vConstants = XMVectorSelect(R1,vResult,YEqualsInfinity);
    // At this point, any entry that's zero will get the result
    // from XMVectorATan(), otherwise, return the failsafe value
    vResult = XMVectorSelect(vResult,vConstants,XEqualsInfinity);
    // Any entries not 0xFFFFFFFF, are considered precalculated
    XMVECTOR ATanResultValid = XMVectorEqualInt(vResult,g_XMNegOneMask);
    // Let's do the ATan2 function
    vConstants = _mm_div_ps(Y,X);
    vConstants = XMVectorATan(vConstants);
    // Discard entries that have been declared void

    XMVECTOR R3 = XMVectorSelect( Pi, g_XMZero, XIsPositive );
    vConstants = _mm_add_ps( vConstants, R3 );

    vResult = XMVectorSelect(vResult,vConstants,ATanResultValid);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorSinEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V2, V3, V5, V7;
    XMVECTOR S1, S2, S3;
    XMVECTOR Result;

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! (for -PI <= V < PI)
    V2 = XMVectorMultiply(V, V);
    V3 = XMVectorMultiply(V2, V);
    V5 = XMVectorMultiply(V3, V2);
    V7 = XMVectorMultiply(V5, V2);

    S1 = XMVectorSplatY(g_XMSinEstCoefficients.v);
    S2 = XMVectorSplatZ(g_XMSinEstCoefficients.v);
    S3 = XMVectorSplatW(g_XMSinEstCoefficients.v);

    Result = XMVectorMultiplyAdd(S1, V3, V);
    Result = XMVectorMultiplyAdd(S2, V5, Result);
    Result = XMVectorMultiplyAdd(S3, V7, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! (for -PI <= V < PI)
    XMVECTOR V2 = _mm_mul_ps(V,V);
    XMVECTOR V3 = _mm_mul_ps(V2,V);
    XMVECTOR vResult = _mm_load_ps1(&g_XMSinEstCoefficients.f[1]);
    vResult = _mm_mul_ps(vResult,V3);
    vResult = _mm_add_ps(vResult,V);
    XMVECTOR vConstants = _mm_load_ps1(&g_XMSinEstCoefficients.f[2]);
    // V^5
    V3 = _mm_mul_ps(V3,V2);
    vConstants = _mm_mul_ps(vConstants,V3);
    vResult = _mm_add_ps(vResult,vConstants);
    vConstants = _mm_load_ps1(&g_XMSinEstCoefficients.f[3]);
    // V^7
    V3 = _mm_mul_ps(V3,V2);
    vConstants = _mm_mul_ps(vConstants,V3);
    vResult = _mm_add_ps(vResult,vConstants);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorCosEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V2, V4, V6;
    XMVECTOR C0, C1, C2, C3;
    XMVECTOR Result;

    V2 = XMVectorMultiply(V, V);
    V4 = XMVectorMultiply(V2, V2);
    V6 = XMVectorMultiply(V4, V2);

    C0 = XMVectorSplatX(g_XMCosEstCoefficients.v);
    C1 = XMVectorSplatY(g_XMCosEstCoefficients.v);
    C2 = XMVectorSplatZ(g_XMCosEstCoefficients.v);
    C3 = XMVectorSplatW(g_XMCosEstCoefficients.v);

    Result = XMVectorMultiplyAdd(C1, V2, C0);
    Result = XMVectorMultiplyAdd(C2, V4, Result);
    Result = XMVectorMultiplyAdd(C3, V6, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Get V^2
    XMVECTOR V2 = _mm_mul_ps(V,V);
    XMVECTOR vResult = _mm_load_ps1(&g_XMCosEstCoefficients.f[1]);
    vResult = _mm_mul_ps(vResult,V2);
    XMVECTOR vConstants = _mm_load_ps1(&g_XMCosEstCoefficients.f[0]);
    vResult = _mm_add_ps(vResult,vConstants);
    vConstants = _mm_load_ps1(&g_XMCosEstCoefficients.f[2]);
    // Get V^4
    XMVECTOR V4 = _mm_mul_ps(V2, V2);
    vConstants = _mm_mul_ps(vConstants,V4);
    vResult = _mm_add_ps(vResult,vConstants);
    vConstants = _mm_load_ps1(&g_XMCosEstCoefficients.f[3]);
    // It's really V^6
    V4 = _mm_mul_ps(V4,V2);
    vConstants = _mm_mul_ps(vConstants,V4);
    vResult = _mm_add_ps(vResult,vConstants);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE VOID XMVectorSinCosEst
(
    XMVECTOR* pSin, 
    XMVECTOR* pCos, 
    FXMVECTOR  V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V2, V3, V4, V5, V6, V7;
    XMVECTOR S1, S2, S3;
    XMVECTOR C0, C1, C2, C3;
    XMVECTOR Sin, Cos;

    XMASSERT(pSin);
    XMASSERT(pCos);

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! (for -PI <= V < PI)
    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! (for -PI <= V < PI)
    V2 = XMVectorMultiply(V, V);
    V3 = XMVectorMultiply(V2, V);
    V4 = XMVectorMultiply(V2, V2);
    V5 = XMVectorMultiply(V3, V2);
    V6 = XMVectorMultiply(V3, V3);
    V7 = XMVectorMultiply(V4, V3);

    S1 = XMVectorSplatY(g_XMSinEstCoefficients.v);
    S2 = XMVectorSplatZ(g_XMSinEstCoefficients.v);
    S3 = XMVectorSplatW(g_XMSinEstCoefficients.v);

    C0 = XMVectorSplatX(g_XMCosEstCoefficients.v);
    C1 = XMVectorSplatY(g_XMCosEstCoefficients.v);
    C2 = XMVectorSplatZ(g_XMCosEstCoefficients.v);
    C3 = XMVectorSplatW(g_XMCosEstCoefficients.v);

    Sin = XMVectorMultiplyAdd(S1, V3, V);
    Sin = XMVectorMultiplyAdd(S2, V5, Sin);
    Sin = XMVectorMultiplyAdd(S3, V7, Sin);

    Cos = XMVectorMultiplyAdd(C1, V2, C0);
    Cos = XMVectorMultiplyAdd(C2, V4, Cos);
    Cos = XMVectorMultiplyAdd(C3, V6, Cos);

    *pSin = Sin;
    *pCos = Cos;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pSin);
    XMASSERT(pCos);
    XMVECTOR V2, V3, V4, V5, V6, V7;
    XMVECTOR S1, S2, S3;
    XMVECTOR C0, C1, C2, C3;
    XMVECTOR Sin, Cos;

    // sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! (for -PI <= V < PI)
    // cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! (for -PI <= V < PI)
    V2 = XMVectorMultiply(V, V);
    V3 = XMVectorMultiply(V2, V);
    V4 = XMVectorMultiply(V2, V2);
    V5 = XMVectorMultiply(V3, V2);
    V6 = XMVectorMultiply(V3, V3);
    V7 = XMVectorMultiply(V4, V3);

    S1 = _mm_load_ps1(&g_XMSinEstCoefficients.f[1]);
    S2 = _mm_load_ps1(&g_XMSinEstCoefficients.f[2]);
    S3 = _mm_load_ps1(&g_XMSinEstCoefficients.f[3]);

    C0 = _mm_load_ps1(&g_XMCosEstCoefficients.f[0]);
    C1 = _mm_load_ps1(&g_XMCosEstCoefficients.f[1]);
    C2 = _mm_load_ps1(&g_XMCosEstCoefficients.f[2]);
    C3 = _mm_load_ps1(&g_XMCosEstCoefficients.f[3]);

    Sin = XMVectorMultiplyAdd(S1, V3, V);
    Sin = XMVectorMultiplyAdd(S2, V5, Sin);
    Sin = XMVectorMultiplyAdd(S3, V7, Sin);

    Cos = XMVectorMultiplyAdd(C1, V2, C0);
    Cos = XMVectorMultiplyAdd(C2, V4, Cos);
    Cos = XMVectorMultiplyAdd(C3, V6, Cos);

    *pSin = Sin;
    *pCos = Cos;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorTanEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2, V1T0, V1T1, V2T2;
    XMVECTOR T0, T1, T2;
    XMVECTOR N, D;
    XMVECTOR OneOverPi;
    XMVECTOR Result;

    OneOverPi = XMVectorSplatW(g_XMTanEstCoefficients.v);

    V1 = XMVectorMultiply(V, OneOverPi);
    V1 = XMVectorRound(V1);

    V1 = XMVectorNegativeMultiplySubtract(g_XMPi.v, V1, V);

    T0 = XMVectorSplatX(g_XMTanEstCoefficients.v);
    T1 = XMVectorSplatY(g_XMTanEstCoefficients.v);
    T2 = XMVectorSplatZ(g_XMTanEstCoefficients.v);

    V2T2 = XMVectorNegativeMultiplySubtract(V1, V1, T2);
    V2 = XMVectorMultiply(V1, V1);
    V1T0 = XMVectorMultiply(V1, T0);
    V1T1 = XMVectorMultiply(V1, T1);

    D = XMVectorReciprocalEst(V2T2);
    N = XMVectorMultiplyAdd(V2, V1T1, V1T0);

    Result = XMVectorMultiply(N, D);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR V1, V2, V1T0, V1T1, V2T2;
    XMVECTOR T0, T1, T2;
    XMVECTOR N, D;
    XMVECTOR OneOverPi;
    XMVECTOR Result;

    OneOverPi = XMVectorSplatW(g_XMTanEstCoefficients);

    V1 = XMVectorMultiply(V, OneOverPi);
    V1 = XMVectorRound(V1);

    V1 = XMVectorNegativeMultiplySubtract(g_XMPi, V1, V);

    T0 = XMVectorSplatX(g_XMTanEstCoefficients);
    T1 = XMVectorSplatY(g_XMTanEstCoefficients);
    T2 = XMVectorSplatZ(g_XMTanEstCoefficients);

    V2T2 = XMVectorNegativeMultiplySubtract(V1, V1, T2);
    V2 = XMVectorMultiply(V1, V1);
    V1T0 = XMVectorMultiply(V1, T0);
    V1T1 = XMVectorMultiply(V1, T1);

    D = XMVectorReciprocalEst(V2T2);
    N = XMVectorMultiplyAdd(V2, V1T1, V1T0);

    Result = XMVectorMultiply(N, D);

    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorSinHEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTORF32 Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = XMVectorMultiplyAdd(V, Scale.v, g_XMNegativeOne.v);
    V2 = XMVectorNegativeMultiplySubtract(V, Scale.v, g_XMNegativeOne.v);

    E1 = XMVectorExpEst(V1);
    E2 = XMVectorExpEst(V2);

    Result = XMVectorSubtract(E1, E2);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTORF32 Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = _mm_mul_ps(V,Scale);
    V1 = _mm_add_ps(V1,g_XMNegativeOne);
    V2 = _mm_mul_ps(V,Scale);
    V2 = _mm_sub_ps(g_XMNegativeOne,V2);
    E1 = XMVectorExpEst(V1);
    E2 = XMVectorExpEst(V2);
    Result = _mm_sub_ps(E1, E2);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorCosHEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTOR Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = XMVectorMultiplyAdd(V, Scale, g_XMNegativeOne.v);
    V2 = XMVectorNegativeMultiplySubtract(V, Scale, g_XMNegativeOne.v);

    E1 = XMVectorExpEst(V1);
    E2 = XMVectorExpEst(V2);

    Result = XMVectorAdd(E1, E2);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR V1, V2;
    XMVECTOR E1, E2;
    XMVECTOR Result;
    static CONST XMVECTORF32 Scale = {1.442695040888963f, 1.442695040888963f, 1.442695040888963f, 1.442695040888963f}; // 1.0f / ln(2.0f)

    V1 = _mm_mul_ps(V,Scale);
    V1 = _mm_add_ps(V1,g_XMNegativeOne);
    V2 = _mm_mul_ps(V, Scale);
    V2 = _mm_sub_ps(g_XMNegativeOne,V2);
    E1 = XMVectorExpEst(V1);
    E2 = XMVectorExpEst(V2);
    Result = _mm_add_ps(E1, E2);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorTanHEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR E;
    XMVECTOR Result;
    static CONST XMVECTOR Scale = {2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f}; // 2.0f / ln(2.0f)

    E = XMVectorMultiply(V, Scale);
    E = XMVectorExpEst(E);
    E = XMVectorMultiplyAdd(E, g_XMOneHalf.v, g_XMOneHalf.v);
    E = XMVectorReciprocalEst(E);

    Result = XMVectorSubtract(g_XMOne.v, E);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 Scale = {2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f, 2.8853900817779268f}; // 2.0f / ln(2.0f)

    XMVECTOR E = _mm_mul_ps(V, Scale);
    E = XMVectorExpEst(E);
    E = _mm_mul_ps(E,g_XMOneHalf);
    E = _mm_add_ps(E,g_XMOneHalf);
    E = XMVectorReciprocalEst(E);
    E = _mm_sub_ps(g_XMOne, E);
    return E;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorASinEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR AbsV, V2, VD, VC0, V2C3;
    XMVECTOR C0, C1, C2, C3;
    XMVECTOR D, Rsq, SqrtD;
    XMVECTOR OnePlusEps;
    XMVECTOR Result;

    AbsV = XMVectorAbs(V);

    OnePlusEps = XMVectorSplatX(g_XMASinEstConstants.v);

    C0 = XMVectorSplatX(g_XMASinEstCoefficients.v);
    C1 = XMVectorSplatY(g_XMASinEstCoefficients.v);
    C2 = XMVectorSplatZ(g_XMASinEstCoefficients.v);
    C3 = XMVectorSplatW(g_XMASinEstCoefficients.v);

    D = XMVectorSubtract(OnePlusEps, AbsV);

    Rsq = XMVectorReciprocalSqrtEst(D);
    SqrtD = XMVectorMultiply(D, Rsq);

    V2 = XMVectorMultiply(V, AbsV);
    V2C3 = XMVectorMultiply(V2, C3);
    VD = XMVectorMultiply(D, AbsV);
    VC0 = XMVectorMultiply(V, C0);

    Result = XMVectorMultiply(V, C1);
    Result = XMVectorMultiplyAdd(V2, C2, Result);
    Result = XMVectorMultiplyAdd(V2C3, VD, Result);
    Result = XMVectorMultiplyAdd(VC0, SqrtD, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Get abs(V)
	XMVECTOR vAbsV = _mm_setzero_ps();
	vAbsV = _mm_sub_ps(vAbsV,V);
	vAbsV = _mm_max_ps(vAbsV,V);

    XMVECTOR D = _mm_load_ps1(&g_XMASinEstConstants.f[0]);
    D = _mm_sub_ps(D,vAbsV);
    // Since this is an estimate, rqsrt is okay
    XMVECTOR vConstants = _mm_rsqrt_ps(D);
    XMVECTOR SqrtD = _mm_mul_ps(D,vConstants);
    // V2 = V^2 retaining sign
    XMVECTOR V2 = _mm_mul_ps(V,vAbsV);
    D = _mm_mul_ps(D,vAbsV);

    XMVECTOR vResult = _mm_load_ps1(&g_XMASinEstCoefficients.f[1]);
    vResult = _mm_mul_ps(vResult,V);
    vConstants = _mm_load_ps1(&g_XMASinEstCoefficients.f[2]);
    vConstants = _mm_mul_ps(vConstants,V2);
    vResult = _mm_add_ps(vResult,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinEstCoefficients.f[3]);
    vConstants = _mm_mul_ps(vConstants,V2);
    vConstants = _mm_mul_ps(vConstants,D);
    vResult = _mm_add_ps(vResult,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinEstCoefficients.f[0]);
    vConstants = _mm_mul_ps(vConstants,V);
    vConstants = _mm_mul_ps(vConstants,SqrtD);
    vResult = _mm_add_ps(vResult,vConstants);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorACosEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR AbsV, V2, VD, VC0, V2C3;
    XMVECTOR C0, C1, C2, C3;
    XMVECTOR D, Rsq, SqrtD;
    XMVECTOR OnePlusEps, HalfPi;
    XMVECTOR Result;

    // acos(V) = PI / 2 - asin(V)

    AbsV = XMVectorAbs(V);

    OnePlusEps = XMVectorSplatX(g_XMASinEstConstants.v);
    HalfPi = XMVectorSplatY(g_XMASinEstConstants.v);

    C0 = XMVectorSplatX(g_XMASinEstCoefficients.v);
    C1 = XMVectorSplatY(g_XMASinEstCoefficients.v);
    C2 = XMVectorSplatZ(g_XMASinEstCoefficients.v);
    C3 = XMVectorSplatW(g_XMASinEstCoefficients.v);

    D = XMVectorSubtract(OnePlusEps, AbsV);

    Rsq = XMVectorReciprocalSqrtEst(D);
    SqrtD = XMVectorMultiply(D, Rsq);

    V2 = XMVectorMultiply(V, AbsV);
    V2C3 = XMVectorMultiply(V2, C3);
    VD = XMVectorMultiply(D, AbsV);
    VC0 = XMVectorMultiply(V, C0);

    Result = XMVectorMultiply(V, C1);
    Result = XMVectorMultiplyAdd(V2, C2, Result);
    Result = XMVectorMultiplyAdd(V2C3, VD, Result);
    Result = XMVectorMultiplyAdd(VC0, SqrtD, Result);
    Result = XMVectorSubtract(HalfPi, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // acos(V) = PI / 2 - asin(V)
    // Get abs(V)
	XMVECTOR vAbsV = _mm_setzero_ps();
	vAbsV = _mm_sub_ps(vAbsV,V);
	vAbsV = _mm_max_ps(vAbsV,V);
    // Calc D
    XMVECTOR D = _mm_load_ps1(&g_XMASinEstConstants.f[0]);
    D = _mm_sub_ps(D,vAbsV);
    // SqrtD = sqrt(D-abs(V)) estimated
    XMVECTOR vConstants = _mm_rsqrt_ps(D);
    XMVECTOR SqrtD = _mm_mul_ps(D,vConstants);
    // V2 = V^2 while retaining sign
    XMVECTOR V2 = _mm_mul_ps(V, vAbsV);
    // Drop vAbsV here. D = (Const-abs(V))*abs(V)
    D = _mm_mul_ps(D, vAbsV);

    XMVECTOR vResult = _mm_load_ps1(&g_XMASinEstCoefficients.f[1]);
    vResult = _mm_mul_ps(vResult,V);
    vConstants = _mm_load_ps1(&g_XMASinEstCoefficients.f[2]);
    vConstants = _mm_mul_ps(vConstants,V2);
    vResult = _mm_add_ps(vResult,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinEstCoefficients.f[3]);
    vConstants = _mm_mul_ps(vConstants,V2);
    vConstants = _mm_mul_ps(vConstants,D);
    vResult = _mm_add_ps(vResult,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinEstCoefficients.f[0]);
    vConstants = _mm_mul_ps(vConstants,V);
    vConstants = _mm_mul_ps(vConstants,SqrtD);
    vResult = _mm_add_ps(vResult,vConstants);

    vConstants = _mm_load_ps1(&g_XMASinEstConstants.f[1]);
    vResult = _mm_sub_ps(vConstants,vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorATanEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR AbsV, V2S2, N, D;
    XMVECTOR S0, S1, S2;
    XMVECTOR HalfPi;
    XMVECTOR Result;

    S0 = XMVectorSplatX(g_XMATanEstCoefficients.v);
    S1 = XMVectorSplatY(g_XMATanEstCoefficients.v);
    S2 = XMVectorSplatZ(g_XMATanEstCoefficients.v);
    HalfPi = XMVectorSplatW(g_XMATanEstCoefficients.v);

    AbsV = XMVectorAbs(V);

    V2S2 = XMVectorMultiplyAdd(V, V, S2);
    N = XMVectorMultiplyAdd(AbsV, HalfPi, S0);
    D = XMVectorMultiplyAdd(AbsV, S1, V2S2);
    N = XMVectorMultiply(N, V);
    D = XMVectorReciprocalEst(D);

    Result = XMVectorMultiply(N, D);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Get abs(V)
	XMVECTOR vAbsV = _mm_setzero_ps();
	vAbsV = _mm_sub_ps(vAbsV,V);
	vAbsV = _mm_max_ps(vAbsV,V);

    XMVECTOR vResult = _mm_load_ps1(&g_XMATanEstCoefficients.f[3]);
    vResult = _mm_mul_ps(vResult,vAbsV);
    XMVECTOR vConstants = _mm_load_ps1(&g_XMATanEstCoefficients.f[0]);
    vResult = _mm_add_ps(vResult,vConstants);
    vResult = _mm_mul_ps(vResult,V);

    XMVECTOR D = _mm_mul_ps(V,V);
    vConstants = _mm_load_ps1(&g_XMATanEstCoefficients.f[2]);
    D = _mm_add_ps(D,vConstants);
    vConstants = _mm_load_ps1(&g_XMATanEstCoefficients.f[1]);
    vConstants = _mm_mul_ps(vConstants,vAbsV);
    D = _mm_add_ps(D,vConstants);
    vResult = _mm_div_ps(vResult,D);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorATan2Est
(
    FXMVECTOR Y, 
    FXMVECTOR X
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Reciprocal;
    XMVECTOR V;
    XMVECTOR YSign;
    XMVECTOR Pi, PiOverTwo, PiOverFour, ThreePiOverFour;
    XMVECTOR YEqualsZero, XEqualsZero, XIsPositive, YEqualsInfinity, XEqualsInfinity;
    XMVECTOR ATanResultValid;
    XMVECTOR R0, R1, R2, R3, R4, R5;
    XMVECTOR Zero;
    XMVECTOR Result;
    static CONST XMVECTOR ATan2Constants = {XM_PI, XM_PIDIV2, XM_PIDIV4, XM_PI * 3.0f / 4.0f};

    Zero = XMVectorZero();
    ATanResultValid = XMVectorTrueInt();

    Pi = XMVectorSplatX(ATan2Constants);
    PiOverTwo = XMVectorSplatY(ATan2Constants);
    PiOverFour = XMVectorSplatZ(ATan2Constants);
    ThreePiOverFour = XMVectorSplatW(ATan2Constants);

    YEqualsZero = XMVectorEqual(Y, Zero);
    XEqualsZero = XMVectorEqual(X, Zero);
    XIsPositive = XMVectorAndInt(X, g_XMNegativeZero.v);
    XIsPositive = XMVectorEqualInt(XIsPositive, Zero);
    YEqualsInfinity = XMVectorIsInfinite(Y);
    XEqualsInfinity = XMVectorIsInfinite(X);

    YSign = XMVectorAndInt(Y, g_XMNegativeZero.v);
    Pi = XMVectorOrInt(Pi, YSign);
    PiOverTwo = XMVectorOrInt(PiOverTwo, YSign);
    PiOverFour = XMVectorOrInt(PiOverFour, YSign);
    ThreePiOverFour = XMVectorOrInt(ThreePiOverFour, YSign);

    R1 = XMVectorSelect(Pi, YSign, XIsPositive);
    R2 = XMVectorSelect(ATanResultValid, PiOverTwo, XEqualsZero);
    R3 = XMVectorSelect(R2, R1, YEqualsZero);
    R4 = XMVectorSelect(ThreePiOverFour, PiOverFour, XIsPositive);
    R5 = XMVectorSelect(PiOverTwo, R4, XEqualsInfinity);
    Result = XMVectorSelect(R3, R5, YEqualsInfinity);
    ATanResultValid = XMVectorEqualInt(Result, ATanResultValid);

    Reciprocal = XMVectorReciprocalEst(X);
    V = XMVectorMultiply(Y, Reciprocal);
    R0 = XMVectorATanEst(V);

    R1 = XMVectorSelect( Pi, Zero, XIsPositive );
    R2 = XMVectorAdd(R0, R1);

    Result = XMVectorSelect(Result, R2, ATanResultValid);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static CONST XMVECTORF32 ATan2Constants = {XM_PI, XM_PIDIV2, XM_PIDIV4, XM_PI * 3.0f / 4.0f};

    // Mask if Y>0 && Y!=INF
    XMVECTOR YEqualsInfinity = XMVectorIsInfinite(Y);
    // Get the sign of (Y&0x80000000)
    XMVECTOR YSign = _mm_and_ps(Y, g_XMNegativeZero);
    // Get the sign bits of X
    XMVECTOR XIsPositive = _mm_and_ps(X,g_XMNegativeZero);
    // Change them to masks
    XIsPositive = XMVectorEqualInt(XIsPositive,g_XMZero);
    // Get Pi
    XMVECTOR Pi = _mm_load_ps1(&ATan2Constants.f[0]);
    // Copy the sign of Y
    Pi = _mm_or_ps(Pi,YSign);
    XMVECTOR R1 = XMVectorSelect(Pi,YSign,XIsPositive);
    // Mask for X==0
    XMVECTOR vConstants = _mm_cmpeq_ps(X,g_XMZero);
    // Get Pi/2 with with sign of Y
    XMVECTOR PiOverTwo = _mm_load_ps1(&ATan2Constants.f[1]);
    PiOverTwo = _mm_or_ps(PiOverTwo,YSign);
    XMVECTOR R2 = XMVectorSelect(g_XMNegOneMask,PiOverTwo,vConstants);
    // Mask for Y==0
    vConstants = _mm_cmpeq_ps(Y,g_XMZero);
    R2 = XMVectorSelect(R2,R1,vConstants);
    // Get Pi/4 with sign of Y
    XMVECTOR PiOverFour = _mm_load_ps1(&ATan2Constants.f[2]);
    PiOverFour = _mm_or_ps(PiOverFour,YSign);
    // Get (Pi*3)/4 with sign of Y
    XMVECTOR ThreePiOverFour = _mm_load_ps1(&ATan2Constants.f[3]);
    ThreePiOverFour = _mm_or_ps(ThreePiOverFour,YSign);
    vConstants = XMVectorSelect(ThreePiOverFour, PiOverFour, XIsPositive);
    XMVECTOR XEqualsInfinity = XMVectorIsInfinite(X);
    vConstants = XMVectorSelect(PiOverTwo,vConstants,XEqualsInfinity);

    XMVECTOR vResult = XMVectorSelect(R2,vConstants,YEqualsInfinity);
    vConstants = XMVectorSelect(R1,vResult,YEqualsInfinity);
    // At this point, any entry that's zero will get the result
    // from XMVectorATan(), otherwise, return the failsafe value
    vResult = XMVectorSelect(vResult,vConstants,XEqualsInfinity);
    // Any entries not 0xFFFFFFFF, are considered precalculated
    XMVECTOR ATanResultValid = XMVectorEqualInt(vResult,g_XMNegOneMask);
    // Let's do the ATan2 function
    XMVECTOR Reciprocal = _mm_rcp_ps(X);
    vConstants = _mm_mul_ps(Y, Reciprocal);
    vConstants = XMVectorATanEst(vConstants);
    // Discard entries that have been declared void

    XMVECTOR R3 = XMVectorSelect( Pi, g_XMZero, XIsPositive );
    vConstants = _mm_add_ps( vConstants, R3 );

    vResult = XMVectorSelect(vResult,vConstants,ATanResultValid);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorLerp
(
    FXMVECTOR V0, 
    FXMVECTOR V1, 
    FLOAT    t
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Scale;
    XMVECTOR Length;
    XMVECTOR Result;

    // V0 + t * (V1 - V0)
    Scale = XMVectorReplicate(t);
    Length = XMVectorSubtract(V1, V0);
    Result = XMVectorMultiplyAdd(Length, Scale, V0);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR L, S;
	XMVECTOR Result;

	L = _mm_sub_ps( V1, V0 );

	S = _mm_set_ps1( t );

	Result = _mm_mul_ps( L, S );

	return _mm_add_ps( Result, V0 );
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorLerpV
(
    FXMVECTOR V0, 
    FXMVECTOR V1, 
    FXMVECTOR T
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Length;
    XMVECTOR Result;

    // V0 + T * (V1 - V0)
    Length = XMVectorSubtract(V1, V0);
    Result = XMVectorMultiplyAdd(Length, T, V0);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR Length;
	XMVECTOR Result;

	Length = _mm_sub_ps( V1, V0 );

	Result = _mm_mul_ps( Length, T );

	return _mm_add_ps( Result, V0 );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorHermite
(
    FXMVECTOR Position0, 
    FXMVECTOR Tangent0, 
    FXMVECTOR Position1, 
    CXMVECTOR Tangent1, 
    FLOAT    t
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR P0;
    XMVECTOR T0;
    XMVECTOR P1;
    XMVECTOR T1;
    XMVECTOR Result;
    FLOAT    t2;
    FLOAT    t3;

    // Result = (2 * t^3 - 3 * t^2 + 1) * Position0 +
    //          (t^3 - 2 * t^2 + t) * Tangent0 +
    //          (-2 * t^3 + 3 * t^2) * Position1 +
    //          (t^3 - t^2) * Tangent1
    t2 = t * t;
    t3 = t * t2;

    P0 = XMVectorReplicate(2.0f * t3 - 3.0f * t2 + 1.0f);
    T0 = XMVectorReplicate(t3 - 2.0f * t2 + t);
    P1 = XMVectorReplicate(-2.0f * t3 + 3.0f * t2);
    T1 = XMVectorReplicate(t3 - t2);

    Result = XMVectorMultiply(P0, Position0);
    Result = XMVectorMultiplyAdd(T0, Tangent0, Result);
    Result = XMVectorMultiplyAdd(P1, Position1, Result);
    Result = XMVectorMultiplyAdd(T1, Tangent1, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    FLOAT t2 = t * t;
    FLOAT t3 = t * t2;

    XMVECTOR P0 = _mm_set_ps1(2.0f * t3 - 3.0f * t2 + 1.0f);
    XMVECTOR T0 = _mm_set_ps1(t3 - 2.0f * t2 + t);
    XMVECTOR P1 = _mm_set_ps1(-2.0f * t3 + 3.0f * t2);
    XMVECTOR T1 = _mm_set_ps1(t3 - t2);

    XMVECTOR vResult = _mm_mul_ps(P0, Position0);
    XMVECTOR vTemp = _mm_mul_ps(T0, Tangent0);
    vResult = _mm_add_ps(vResult,vTemp);
    vTemp = _mm_mul_ps(P1, Position1);
    vResult = _mm_add_ps(vResult,vTemp);
    vTemp = _mm_mul_ps(T1, Tangent1);
    vResult = _mm_add_ps(vResult,vTemp);
    return vResult;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorHermiteV
(
    FXMVECTOR Position0, 
    FXMVECTOR Tangent0, 
    FXMVECTOR Position1, 
    CXMVECTOR Tangent1, 
    CXMVECTOR T
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR P0;
    XMVECTOR T0;
    XMVECTOR P1;
    XMVECTOR T1;
    XMVECTOR Result;
    XMVECTOR T2;
    XMVECTOR T3;

    // Result = (2 * t^3 - 3 * t^2 + 1) * Position0 +
    //          (t^3 - 2 * t^2 + t) * Tangent0 +
    //          (-2 * t^3 + 3 * t^2) * Position1 +
    //          (t^3 - t^2) * Tangent1
    T2 = XMVectorMultiply(T, T);
    T3 = XMVectorMultiply(T , T2);

    P0 = XMVectorReplicate(2.0f * T3.vector4_f32[0] - 3.0f * T2.vector4_f32[0] + 1.0f);
    T0 = XMVectorReplicate(T3.vector4_f32[1] - 2.0f * T2.vector4_f32[1] + T.vector4_f32[1]);
    P1 = XMVectorReplicate(-2.0f * T3.vector4_f32[2] + 3.0f * T2.vector4_f32[2]);
    T1 = XMVectorReplicate(T3.vector4_f32[3] - T2.vector4_f32[3]);

    Result = XMVectorMultiply(P0, Position0);
    Result = XMVectorMultiplyAdd(T0, Tangent0, Result);
    Result = XMVectorMultiplyAdd(P1, Position1, Result);
    Result = XMVectorMultiplyAdd(T1, Tangent1, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static const XMVECTORF32 CatMulT2 = {-3.0f,-2.0f,3.0f,-1.0f};
    static const XMVECTORF32 CatMulT3 = {2.0f,1.0f,-2.0f,1.0f};

    // Result = (2 * t^3 - 3 * t^2 + 1) * Position0 +
    //          (t^3 - 2 * t^2 + t) * Tangent0 +
    //          (-2 * t^3 + 3 * t^2) * Position1 +
    //          (t^3 - t^2) * Tangent1
    XMVECTOR T2 = _mm_mul_ps(T,T);
    XMVECTOR T3 = _mm_mul_ps(T,T2);
    // Mul by the constants against t^2
    T2 = _mm_mul_ps(T2,CatMulT2);
    // Mul by the constants against t^3
    T3 = _mm_mul_ps(T3,CatMulT3);
    // T3 now has the pre-result.
    T3 = _mm_add_ps(T3,T2);
    // I need to add t.y only
    T2 = _mm_and_ps(T,g_XMMaskY);
    T3 = _mm_add_ps(T3,T2);
    // Add 1.0f to x
    T3 = _mm_add_ps(T3,g_XMIdentityR0);
    // Now, I have the constants created
    // Mul the x constant to Position0
    XMVECTOR vResult = _mm_shuffle_ps(T3,T3,_MM_SHUFFLE(0,0,0,0));
    vResult = _mm_mul_ps(vResult,Position0);
    // Mul the y constant to Tangent0
    T2 = _mm_shuffle_ps(T3,T3,_MM_SHUFFLE(1,1,1,1));
    T2 = _mm_mul_ps(T2,Tangent0);
    vResult = _mm_add_ps(vResult,T2);
    // Mul the z constant to Position1
    T2 = _mm_shuffle_ps(T3,T3,_MM_SHUFFLE(2,2,2,2));
    T2 = _mm_mul_ps(T2,Position1);
    vResult = _mm_add_ps(vResult,T2);
    // Mul the w constant to Tangent1
    T3 = _mm_shuffle_ps(T3,T3,_MM_SHUFFLE(3,3,3,3));
    T3 = _mm_mul_ps(T3,Tangent1);
    vResult = _mm_add_ps(vResult,T3);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorCatmullRom
(
    FXMVECTOR Position0, 
    FXMVECTOR Position1, 
    FXMVECTOR Position2, 
    CXMVECTOR Position3, 
    FLOAT    t
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR P0;
    XMVECTOR P1;
    XMVECTOR P2;
    XMVECTOR P3;
    XMVECTOR Result;
    FLOAT    t2;
    FLOAT    t3;

    // Result = ((-t^3 + 2 * t^2 - t) * Position0 +
    //           (3 * t^3 - 5 * t^2 + 2) * Position1 +
    //           (-3 * t^3 + 4 * t^2 + t) * Position2 +
    //           (t^3 - t^2) * Position3) * 0.5
    t2 = t * t;
    t3 = t * t2;

    P0 = XMVectorReplicate((-t3 + 2.0f * t2 - t) * 0.5f);
    P1 = XMVectorReplicate((3.0f * t3 - 5.0f * t2 + 2.0f) * 0.5f);
    P2 = XMVectorReplicate((-3.0f * t3 + 4.0f * t2 + t) * 0.5f);
    P3 = XMVectorReplicate((t3 - t2) * 0.5f);

    Result = XMVectorMultiply(P0, Position0);
    Result = XMVectorMultiplyAdd(P1, Position1, Result);
    Result = XMVectorMultiplyAdd(P2, Position2, Result);
    Result = XMVectorMultiplyAdd(P3, Position3, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    FLOAT t2 = t * t;
    FLOAT t3 = t * t2;

    XMVECTOR P0 = _mm_set_ps1((-t3 + 2.0f * t2 - t) * 0.5f);
    XMVECTOR P1 = _mm_set_ps1((3.0f * t3 - 5.0f * t2 + 2.0f) * 0.5f);
    XMVECTOR P2 = _mm_set_ps1((-3.0f * t3 + 4.0f * t2 + t) * 0.5f);
    XMVECTOR P3 = _mm_set_ps1((t3 - t2) * 0.5f);

    P0 = _mm_mul_ps(P0, Position0);
    P1 = _mm_mul_ps(P1, Position1);
    P2 = _mm_mul_ps(P2, Position2);
    P3 = _mm_mul_ps(P3, Position3);
    P0 = _mm_add_ps(P0,P1);
    P2 = _mm_add_ps(P2,P3);
    P0 = _mm_add_ps(P0,P2);
    return P0;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorCatmullRomV
(
    FXMVECTOR Position0, 
    FXMVECTOR Position1, 
    FXMVECTOR Position2, 
    CXMVECTOR Position3, 
    CXMVECTOR T
)
{
#if defined(_XM_NO_INTRINSICS_)
    float fx = T.vector4_f32[0];
    float fy = T.vector4_f32[1];
    float fz = T.vector4_f32[2];
    float fw = T.vector4_f32[3];
    XMVECTOR vResult = {
        0.5f*((-fx*fx*fx+2*fx*fx-fx)*Position0.vector4_f32[0]+
        (3*fx*fx*fx-5*fx*fx+2)*Position1.vector4_f32[0]+
        (-3*fx*fx*fx+4*fx*fx+fx)*Position2.vector4_f32[0]+
        (fx*fx*fx-fx*fx)*Position3.vector4_f32[0]),
        0.5f*((-fy*fy*fy+2*fy*fy-fy)*Position0.vector4_f32[1]+
        (3*fy*fy*fy-5*fy*fy+2)*Position1.vector4_f32[1]+
        (-3*fy*fy*fy+4*fy*fy+fy)*Position2.vector4_f32[1]+
        (fy*fy*fy-fy*fy)*Position3.vector4_f32[1]),
        0.5f*((-fz*fz*fz+2*fz*fz-fz)*Position0.vector4_f32[2]+
        (3*fz*fz*fz-5*fz*fz+2)*Position1.vector4_f32[2]+
        (-3*fz*fz*fz+4*fz*fz+fz)*Position2.vector4_f32[2]+
        (fz*fz*fz-fz*fz)*Position3.vector4_f32[2]),
        0.5f*((-fw*fw*fw+2*fw*fw-fw)*Position0.vector4_f32[3]+
        (3*fw*fw*fw-5*fw*fw+2)*Position1.vector4_f32[3]+
        (-3*fw*fw*fw+4*fw*fw+fw)*Position2.vector4_f32[3]+
        (fw*fw*fw-fw*fw)*Position3.vector4_f32[3])
    };
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    static const XMVECTORF32 Catmul2 = {2.0f,2.0f,2.0f,2.0f};
    static const XMVECTORF32 Catmul3 = {3.0f,3.0f,3.0f,3.0f};
    static const XMVECTORF32 Catmul4 = {4.0f,4.0f,4.0f,4.0f};
    static const XMVECTORF32 Catmul5 = {5.0f,5.0f,5.0f,5.0f};
    // Cache T^2 and T^3
    XMVECTOR T2 = _mm_mul_ps(T,T);
    XMVECTOR T3 = _mm_mul_ps(T,T2);
    // Perform the Position0 term
    XMVECTOR vResult = _mm_add_ps(T2,T2);
    vResult = _mm_sub_ps(vResult,T);
    vResult = _mm_sub_ps(vResult,T3);
    vResult = _mm_mul_ps(vResult,Position0);
    // Perform the Position1 term and add
    XMVECTOR vTemp = _mm_mul_ps(T3,Catmul3);
    XMVECTOR vTemp2 = _mm_mul_ps(T2,Catmul5);
    vTemp = _mm_sub_ps(vTemp,vTemp2);
    vTemp = _mm_add_ps(vTemp,Catmul2);
    vTemp = _mm_mul_ps(vTemp,Position1);
    vResult = _mm_add_ps(vResult,vTemp);
    // Perform the Position2 term and add
    vTemp = _mm_mul_ps(T2,Catmul4);
    vTemp2 = _mm_mul_ps(T3,Catmul3);
    vTemp = _mm_sub_ps(vTemp,vTemp2);
    vTemp = _mm_add_ps(vTemp,T);
    vTemp = _mm_mul_ps(vTemp,Position2);
    vResult = _mm_add_ps(vResult,vTemp);
    // Position3 is the last term
    T3 = _mm_sub_ps(T3,T2);
    T3 = _mm_mul_ps(T3,Position3);
    vResult = _mm_add_ps(vResult,T3);
    // Multiply by 0.5f and exit
    vResult = _mm_mul_ps(vResult,g_XMOneHalf);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorBaryCentric
(
    FXMVECTOR Position0, 
    FXMVECTOR Position1, 
    FXMVECTOR Position2, 
    FLOAT    f, 
    FLOAT    g
)
{
#if defined(_XM_NO_INTRINSICS_)

    // Result = Position0 + f * (Position1 - Position0) + g * (Position2 - Position0)
    XMVECTOR P10;
    XMVECTOR P20;
    XMVECTOR ScaleF;
    XMVECTOR ScaleG;
    XMVECTOR Result;

    P10 = XMVectorSubtract(Position1, Position0);
    ScaleF = XMVectorReplicate(f);

    P20 = XMVectorSubtract(Position2, Position0);
    ScaleG = XMVectorReplicate(g);

    Result = XMVectorMultiplyAdd(P10, ScaleF, Position0);
    Result = XMVectorMultiplyAdd(P20, ScaleG, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR R1 = _mm_sub_ps(Position1,Position0);
	XMVECTOR SF = _mm_set_ps1(f);
	XMVECTOR R2 = _mm_sub_ps(Position2,Position0);
	XMVECTOR SG = _mm_set_ps1(g);
	R1 = _mm_mul_ps(R1,SF);
	R2 = _mm_mul_ps(R2,SG);
	R1 = _mm_add_ps(R1,Position0);
	R1 = _mm_add_ps(R1,R2);
    return R1;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVectorBaryCentricV
(
    FXMVECTOR Position0, 
    FXMVECTOR Position1, 
    FXMVECTOR Position2, 
    CXMVECTOR F, 
    CXMVECTOR G
)
{
#if defined(_XM_NO_INTRINSICS_)

    // Result = Position0 + f * (Position1 - Position0) + g * (Position2 - Position0)
    XMVECTOR P10;
    XMVECTOR P20;
    XMVECTOR Result;

    P10 = XMVectorSubtract(Position1, Position0);
    P20 = XMVectorSubtract(Position2, Position0);

    Result = XMVectorMultiplyAdd(P10, F, Position0);
    Result = XMVectorMultiplyAdd(P20, G, Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
	XMVECTOR R1 = _mm_sub_ps(Position1,Position0);
	XMVECTOR R2 = _mm_sub_ps(Position2,Position0);
	R1 = _mm_mul_ps(R1,F);
	R2 = _mm_mul_ps(R2,G);
	R1 = _mm_add_ps(R1,Position0);
	R1 = _mm_add_ps(R1,R2);
    return R1;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

/****************************************************************************
 *
 * 2D Vector
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2Equal
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] == V2.vector4_f32[0]) && (V1.vector4_f32[1] == V2.vector4_f32[1])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
// z and w are don't care
    return (((_mm_movemask_ps(vTemp)&3)==3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector2EqualR(V1, V2));
#endif
}


//------------------------------------------------------------------------------

XMFINLINE UINT XMVector2EqualR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    UINT CR = 0;

    if ((V1.vector4_f32[0] == V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] == V2.vector4_f32[1]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] != V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] != V2.vector4_f32[1]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
// z and w are don't care
    int iTest = _mm_movemask_ps(vTemp)&3;
    UINT CR = 0;
    if (iTest==3)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2EqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_u32[0] == V2.vector4_u32[0]) && (V1.vector4_u32[1] == V2.vector4_u32[1])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    return (((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])&3)==3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector2EqualIntR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector2EqualIntR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    UINT CR = 0;
    if ((V1.vector4_u32[0] == V2.vector4_u32[0]) && 
        (V1.vector4_u32[1] == V2.vector4_u32[1]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_u32[0] != V2.vector4_u32[0]) && 
        (V1.vector4_u32[1] != V2.vector4_u32[1]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    int iTest = _mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])&3;
    UINT CR = 0;
    if (iTest==3)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
	return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2NearEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR Epsilon
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT dx, dy;
    dx = fabsf(V1.vector4_f32[0]-V2.vector4_f32[0]);
    dy = fabsf(V1.vector4_f32[1]-V2.vector4_f32[1]);
    return ((dx <= Epsilon.vector4_f32[0]) &&
            (dy <= Epsilon.vector4_f32[1]));
#elif defined(_XM_SSE_INTRINSICS_)
    // Get the difference
    XMVECTOR vDelta = _mm_sub_ps(V1,V2);
    // Get the absolute value of the difference
    XMVECTOR vTemp = _mm_setzero_ps();
    vTemp = _mm_sub_ps(vTemp,vDelta);
    vTemp = _mm_max_ps(vTemp,vDelta);
    vTemp = _mm_cmple_ps(vTemp,Epsilon);
    // z and w are don't care
    return (((_mm_movemask_ps(vTemp)&3)==0x3) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2NotEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] != V2.vector4_f32[0]) || (V1.vector4_f32[1] != V2.vector4_f32[1])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
// z and w are don't care
    return (((_mm_movemask_ps(vTemp)&3)!=3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAnyFalse(XMVector2EqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2NotEqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_u32[0] != V2.vector4_u32[0]) || (V1.vector4_u32[1] != V2.vector4_u32[1])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    return (((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])&3)!=3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAnyFalse(XMVector2EqualIntR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2Greater
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] > V2.vector4_f32[0]) && (V1.vector4_f32[1] > V2.vector4_f32[1])) != 0);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpgt_ps(V1,V2);
// z and w are don't care
    return (((_mm_movemask_ps(vTemp)&3)==3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector2GreaterR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector2GreaterR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    UINT CR = 0;
    if ((V1.vector4_f32[0] > V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] > V2.vector4_f32[1]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] <= V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] <= V2.vector4_f32[1]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpgt_ps(V1,V2);
    int iTest = _mm_movemask_ps(vTemp)&3;
    UINT CR = 0;
    if (iTest==3)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2GreaterOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] >= V2.vector4_f32[0]) && (V1.vector4_f32[1] >= V2.vector4_f32[1])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpge_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&3)==3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector2GreaterOrEqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector2GreaterOrEqualR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if ((V1.vector4_f32[0] >= V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] >= V2.vector4_f32[1]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] < V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] < V2.vector4_f32[1]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpge_ps(V1,V2);
    int iTest = _mm_movemask_ps(vTemp)&3;
    UINT CR = 0;
    if (iTest == 3)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2Less
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] < V2.vector4_f32[0]) && (V1.vector4_f32[1] < V2.vector4_f32[1])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmplt_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&3)==3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector2GreaterR(V2, V1));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2LessOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] <= V2.vector4_f32[0]) && (V1.vector4_f32[1] <= V2.vector4_f32[1])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmple_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&3)==3) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector2GreaterOrEqualR(V2, V1));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2InBounds
(
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
 #if defined(_XM_NO_INTRINSICS_)
    return (((V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) && 
        (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1])) != 0);
 #elif defined(_XM_SSE_INTRINSICS_)
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    // x and y in bounds? (z and w are don't care)
    return (((_mm_movemask_ps(vTemp1)&0x3)==0x3) != 0);
#else // _XM_VMX128_INTRINSICS_   
    return XMComparisonAllInBounds(XMVector2InBoundsR(V, Bounds));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector2InBoundsR
(
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if ((V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) && 
        (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1]))
    {
        CR = XM_CRMASK_CR6BOUNDS;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    // x and y in bounds? (z and w are don't care)
    return ((_mm_movemask_ps(vTemp1)&0x3)==0x3) ? XM_CRMASK_CR6BOUNDS : 0;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2IsNaN
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (XMISNAN(V.vector4_f32[0]) ||
            XMISNAN(V.vector4_f32[1]));
#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the exponent
    __m128i vTempInf = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMInfinity);
    // Mask off the mantissa
    __m128i vTempNan = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMQNaNTest);
    // Are any of the exponents == 0x7F800000?
    vTempInf = _mm_cmpeq_epi32(vTempInf,g_XMInfinity);
    // Are any of the mantissa's zero? (SSE2 doesn't have a neq test)
    vTempNan = _mm_cmpeq_epi32(vTempNan,g_XMZero);
    // Perform a not on the NaN test to be true on NON-zero mantissas
    vTempNan = _mm_andnot_si128(vTempNan,vTempInf);
    // If x or y are NaN, the signs are true after the merge above
    return ((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTempNan)[0])&3) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector2IsInfinite
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    return (XMISINF(V.vector4_f32[0]) ||
            XMISINF(V.vector4_f32[1]));
#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the sign bit
    __m128 vTemp = _mm_and_ps(V,g_XMAbsMask);
    // Compare to infinity
    vTemp = _mm_cmpeq_ps(vTemp,g_XMInfinity);
    // If x or z are infinity, the signs are true.
    return ((_mm_movemask_ps(vTemp)&3) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Dot
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_f32[0] =
    Result.vector4_f32[1] =
    Result.vector4_f32[2] =
    Result.vector4_f32[3] = V1.vector4_f32[0] * V2.vector4_f32[0] + V1.vector4_f32[1] * V2.vector4_f32[1];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y
    XMVECTOR vLengthSq = _mm_mul_ps(V1,V2);
    // vTemp has y splatted
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    // x+y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Cross
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fCross = (V1.vector4_f32[0] * V2.vector4_f32[1]) - (V1.vector4_f32[1] * V2.vector4_f32[0]);
    XMVECTOR vResult = { 
        fCross,
        fCross,
        fCross,
        fCross
    };
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    // Swap x and y
    XMVECTOR vResult = _mm_shuffle_ps(V2,V2,_MM_SHUFFLE(0,1,0,1));
    // Perform the muls
    vResult = _mm_mul_ps(vResult,V1);
    // Splat y
    XMVECTOR vTemp = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(1,1,1,1));
    // Sub the values
    vResult = _mm_sub_ss(vResult,vTemp);
    // Splat the cross product
    vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,0,0,0));
	return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2LengthSq
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    return XMVector2Dot(V, V);
#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has y splatted
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    // x+y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    return vLengthSq;
#else
    return XMVector2Dot(V, V);
#endif
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2ReciprocalLengthEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector2LengthSq(V);
    Result = XMVectorReciprocalSqrtEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has y splatted
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    // x+y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vLengthSq = _mm_rsqrt_ss(vLengthSq);
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2ReciprocalLength
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector2LengthSq(V);
    Result = XMVectorReciprocalSqrt(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has y splatted
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    // x+y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vLengthSq = _mm_sqrt_ss(vLengthSq);
    vLengthSq = _mm_div_ss(g_XMOne,vLengthSq);
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2LengthEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Result;
    Result = XMVector2LengthSq(V);
    Result = XMVectorSqrtEst(Result);
    return Result;
#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has y splatted
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    // x+y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vLengthSq = _mm_sqrt_ss(vLengthSq);
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Length
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result = XMVector2LengthSq(V);
    Result = XMVectorSqrt(Result);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has y splatted
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    // x+y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    vLengthSq = _mm_sqrt_ps(vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// XMVector2NormalizeEst uses a reciprocal estimate and
// returns QNaN on zero and infinite vectors.

XMFINLINE XMVECTOR XMVector2NormalizeEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result = XMVector2ReciprocalLength(V);
    Result = XMVectorMultiply(V, Result);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has y splatted
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    // x+y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vLengthSq = _mm_rsqrt_ss(vLengthSq);
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    vLengthSq = _mm_mul_ps(vLengthSq,V);
	return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Normalize
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fLength;
    XMVECTOR vResult;

    vResult = XMVector2Length( V );
    fLength = vResult.vector4_f32[0];

    // Prevent divide by zero
    if (fLength > 0) {
        fLength = 1.0f/fLength;
    }
    
    vResult.vector4_f32[0] = V.vector4_f32[0]*fLength;
    vResult.vector4_f32[1] = V.vector4_f32[1]*fLength;
    vResult.vector4_f32[2] = V.vector4_f32[2]*fLength;
    vResult.vector4_f32[3] = V.vector4_f32[3]*fLength;
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x and y only
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,1,1,1));
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    // Prepare for the division
    XMVECTOR vResult = _mm_sqrt_ps(vLengthSq);
    // Create zero with a single instruction
    XMVECTOR vZeroMask = _mm_setzero_ps();
    // Test for a divide by zero (Must be FP to detect -0.0)
    vZeroMask = _mm_cmpneq_ps(vZeroMask,vResult);
    // Failsafe on zero (Or epsilon) length planes
    // If the length is infinity, set the elements to zero
    vLengthSq = _mm_cmpneq_ps(vLengthSq,g_XMInfinity);
    // Reciprocal mul to perform the normalization
    vResult = _mm_div_ps(V,vResult);
    // Any that are infinity, set to zero
    vResult = _mm_and_ps(vResult,vZeroMask);
    // Select qnan or result based on infinite length
	XMVECTOR vTemp1 = _mm_andnot_ps(vLengthSq,g_XMQNaN);
    XMVECTOR vTemp2 = _mm_and_ps(vResult,vLengthSq);
    vResult = _mm_or_ps(vTemp1,vTemp2);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2ClampLength
(
    FXMVECTOR V, 
    FLOAT    LengthMin, 
    FLOAT    LengthMax
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR ClampMax;
    XMVECTOR ClampMin;

    ClampMax = XMVectorReplicate(LengthMax);
    ClampMin = XMVectorReplicate(LengthMin);

    return XMVector2ClampLengthV(V, ClampMin, ClampMax);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR ClampMax = _mm_set_ps1(LengthMax);
    XMVECTOR ClampMin = _mm_set_ps1(LengthMin);
    return XMVector2ClampLengthV(V, ClampMin, ClampMax);
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2ClampLengthV
(
    FXMVECTOR V, 
    FXMVECTOR LengthMin, 
    FXMVECTOR LengthMax
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR ClampLength;
    XMVECTOR LengthSq;
    XMVECTOR RcpLength;
    XMVECTOR Length;
    XMVECTOR Normal;
    XMVECTOR Zero;
    XMVECTOR InfiniteLength;
    XMVECTOR ZeroLength;
    XMVECTOR Select;
    XMVECTOR ControlMax;
    XMVECTOR ControlMin;
    XMVECTOR Control;
    XMVECTOR Result;

    XMASSERT((LengthMin.vector4_f32[1] == LengthMin.vector4_f32[0]));
    XMASSERT((LengthMax.vector4_f32[1] == LengthMax.vector4_f32[0]));
    XMASSERT(XMVector2GreaterOrEqual(LengthMin, XMVectorZero()));
    XMASSERT(XMVector2GreaterOrEqual(LengthMax, XMVectorZero()));
    XMASSERT(XMVector2GreaterOrEqual(LengthMax, LengthMin));

    LengthSq = XMVector2LengthSq(V);

    Zero = XMVectorZero();

    RcpLength = XMVectorReciprocalSqrt(LengthSq);

    InfiniteLength = XMVectorEqualInt(LengthSq, g_XMInfinity.v);
    ZeroLength = XMVectorEqual(LengthSq, Zero);

    Length = XMVectorMultiply(LengthSq, RcpLength);

    Normal = XMVectorMultiply(V, RcpLength);

    Select = XMVectorEqualInt(InfiniteLength, ZeroLength);
    Length = XMVectorSelect(LengthSq, Length, Select);
    Normal = XMVectorSelect(LengthSq, Normal, Select);

    ControlMax = XMVectorGreater(Length, LengthMax);
    ControlMin = XMVectorLess(Length, LengthMin);

    ClampLength = XMVectorSelect(Length, LengthMax, ControlMax);
    ClampLength = XMVectorSelect(ClampLength, LengthMin, ControlMin);

    Result = XMVectorMultiply(Normal, ClampLength);

    // Preserve the original vector (with no precision loss) if the length falls within the given range
    Control = XMVectorEqualInt(ControlMax, ControlMin);
    Result = XMVectorSelect(Result, V, Control);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR ClampLength;
    XMVECTOR LengthSq;
    XMVECTOR RcpLength;
    XMVECTOR Length;
    XMVECTOR Normal;
    XMVECTOR InfiniteLength;
    XMVECTOR ZeroLength;
    XMVECTOR Select;
    XMVECTOR ControlMax;
    XMVECTOR ControlMin;
    XMVECTOR Control;
    XMVECTOR Result;

    XMASSERT((XMVectorGetY(LengthMin) == XMVectorGetX(LengthMin)));
    XMASSERT((XMVectorGetY(LengthMax) == XMVectorGetX(LengthMax)));
    XMASSERT(XMVector2GreaterOrEqual(LengthMin, g_XMZero));
    XMASSERT(XMVector2GreaterOrEqual(LengthMax, g_XMZero));
    XMASSERT(XMVector2GreaterOrEqual(LengthMax, LengthMin));
    LengthSq = XMVector2LengthSq(V);
    RcpLength = XMVectorReciprocalSqrt(LengthSq);
    InfiniteLength = XMVectorEqualInt(LengthSq, g_XMInfinity);
    ZeroLength = XMVectorEqual(LengthSq, g_XMZero);
    Length = _mm_mul_ps(LengthSq, RcpLength);
    Normal = _mm_mul_ps(V, RcpLength);
    Select = XMVectorEqualInt(InfiniteLength, ZeroLength);
    Length = XMVectorSelect(LengthSq, Length, Select);
    Normal = XMVectorSelect(LengthSq, Normal, Select);
    ControlMax = XMVectorGreater(Length, LengthMax);
    ControlMin = XMVectorLess(Length, LengthMin);
    ClampLength = XMVectorSelect(Length, LengthMax, ControlMax);
    ClampLength = XMVectorSelect(ClampLength, LengthMin, ControlMin);
    Result = _mm_mul_ps(Normal, ClampLength);
    // Preserve the original vector (with no precision loss) if the length falls within the given range
    Control = XMVectorEqualInt(ControlMax, ControlMin);
    Result = XMVectorSelect(Result, V, Control);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Reflect
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    // Result = Incident - (2 * dot(Incident, Normal)) * Normal
    Result = XMVector2Dot(Incident, Normal);
    Result = XMVectorAdd(Result, Result);
    Result = XMVectorNegativeMultiplySubtract(Result, Normal, Incident);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Result = Incident - (2 * dot(Incident, Normal)) * Normal
    XMVECTOR Result = XMVector2Dot(Incident,Normal);
    Result = _mm_add_ps(Result, Result);
    Result = _mm_mul_ps(Result, Normal);
    Result = _mm_sub_ps(Incident,Result);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Refract
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal, 
    FLOAT    RefractionIndex
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Index;
    Index = XMVectorReplicate(RefractionIndex);
    return XMVector2RefractV(Incident, Normal, Index);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR Index = _mm_set_ps1(RefractionIndex);
    return XMVector2RefractV(Incident,Normal,Index);
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

// Return the refraction of a 2D vector
XMFINLINE XMVECTOR XMVector2RefractV
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal, 
    FXMVECTOR RefractionIndex
)
{
#if defined(_XM_NO_INTRINSICS_)
    float IDotN;
    float RX,RY;
    XMVECTOR vResult;
    // Result = RefractionIndex * Incident - Normal * (RefractionIndex * dot(Incident, Normal) + 
    // sqrt(1 - RefractionIndex * RefractionIndex * (1 - dot(Incident, Normal) * dot(Incident, Normal))))
    IDotN = (Incident.vector4_f32[0]*Normal.vector4_f32[0])+(Incident.vector4_f32[1]*Normal.vector4_f32[1]);
    // R = 1.0f - RefractionIndex * RefractionIndex * (1.0f - IDotN * IDotN)
    RY = 1.0f-(IDotN*IDotN);
    RX = 1.0f-(RY*RefractionIndex.vector4_f32[0]*RefractionIndex.vector4_f32[0]);
    RY = 1.0f-(RY*RefractionIndex.vector4_f32[1]*RefractionIndex.vector4_f32[1]);
    if (RX>=0.0f) {
        RX = (RefractionIndex.vector4_f32[0]*Incident.vector4_f32[0])-(Normal.vector4_f32[0]*((RefractionIndex.vector4_f32[0]*IDotN)+sqrtf(RX)));
    } else {
        RX = 0.0f;
    }
    if (RY>=0.0f) {
        RY = (RefractionIndex.vector4_f32[1]*Incident.vector4_f32[1])-(Normal.vector4_f32[1]*((RefractionIndex.vector4_f32[1]*IDotN)+sqrtf(RY)));
    } else {
        RY = 0.0f;
    }
    vResult.vector4_f32[0] = RX;
    vResult.vector4_f32[1] = RY;
    vResult.vector4_f32[2] = 0.0f;   
    vResult.vector4_f32[3] = 0.0f;
    return vResult;
#elif defined(_XM_SSE_INTRINSICS_)
    // Result = RefractionIndex * Incident - Normal * (RefractionIndex * dot(Incident, Normal) + 
    // sqrt(1 - RefractionIndex * RefractionIndex * (1 - dot(Incident, Normal) * dot(Incident, Normal))))
    // Get the 2D Dot product of Incident-Normal
    XMVECTOR IDotN = _mm_mul_ps(Incident,Normal);
    XMVECTOR vTemp = _mm_shuffle_ps(IDotN,IDotN,_MM_SHUFFLE(1,1,1,1));
    IDotN = _mm_add_ss(IDotN,vTemp);
    IDotN = _mm_shuffle_ps(IDotN,IDotN,_MM_SHUFFLE(0,0,0,0));
    // vTemp = 1.0f - RefractionIndex * RefractionIndex * (1.0f - IDotN * IDotN)
    vTemp = _mm_mul_ps(IDotN,IDotN);
    vTemp = _mm_sub_ps(g_XMOne,vTemp);
    vTemp = _mm_mul_ps(vTemp,RefractionIndex);
    vTemp = _mm_mul_ps(vTemp,RefractionIndex);
    vTemp = _mm_sub_ps(g_XMOne,vTemp);
    // If any terms are <=0, sqrt() will fail, punt to zero
    XMVECTOR vMask = _mm_cmpgt_ps(vTemp,g_XMZero);
    // R = RefractionIndex * IDotN + sqrt(R)
    vTemp = _mm_sqrt_ps(vTemp);
    XMVECTOR vResult = _mm_mul_ps(RefractionIndex,IDotN);
    vTemp = _mm_add_ps(vTemp,vResult);
    // Result = RefractionIndex * Incident - Normal * R
    vResult = _mm_mul_ps(RefractionIndex,Incident);
    vTemp = _mm_mul_ps(vTemp,Normal);
    vResult = _mm_sub_ps(vResult,vTemp);
    vResult = _mm_and_ps(vResult,vMask);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Orthogonal
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_f32[0] = -V.vector4_f32[1];
    Result.vector4_f32[1] = V.vector4_f32[0];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,2,0,1));
    vResult = _mm_mul_ps(vResult,g_XMNegateX);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2AngleBetweenNormalsEst
(
    FXMVECTOR N1, 
    FXMVECTOR N2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR NegativeOne;
    XMVECTOR One;
    XMVECTOR Result;

    Result = XMVector2Dot(N1, N2);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    Result = XMVectorClamp(Result, NegativeOne, One);
    Result = XMVectorACosEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = XMVector2Dot(N1,N2);
    // Clamp to -1.0f to 1.0f
	vResult = _mm_max_ps(vResult,g_XMNegativeOne);
	vResult = _mm_min_ps(vResult,g_XMOne);;
    vResult = XMVectorACosEst(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2AngleBetweenNormals
(
    FXMVECTOR N1, 
    FXMVECTOR N2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR NegativeOne;
    XMVECTOR One;
    XMVECTOR Result;

    Result = XMVector2Dot(N1, N2);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    Result = XMVectorClamp(Result, NegativeOne, One);
    Result = XMVectorACos(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = XMVector2Dot(N1,N2);
    // Clamp to -1.0f to 1.0f
	vResult = _mm_max_ps(vResult,g_XMNegativeOne);
	vResult = _mm_min_ps(vResult,g_XMOne);;
    vResult = XMVectorACos(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2AngleBetweenVectors
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR L1;
    XMVECTOR L2;
    XMVECTOR Dot;
    XMVECTOR CosAngle;
    XMVECTOR NegativeOne;
    XMVECTOR One;
    XMVECTOR Result;

    L1 = XMVector2ReciprocalLength(V1);
    L2 = XMVector2ReciprocalLength(V2);

    Dot = XMVector2Dot(V1, V2);

    L1 = XMVectorMultiply(L1, L2);

    CosAngle = XMVectorMultiply(Dot, L1);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    CosAngle = XMVectorClamp(CosAngle, NegativeOne, One);

    Result = XMVectorACos(CosAngle);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR L1;
    XMVECTOR L2;
    XMVECTOR Dot;
    XMVECTOR CosAngle;
    XMVECTOR Result;
    L1 = XMVector2ReciprocalLength(V1);
    L2 = XMVector2ReciprocalLength(V2);
    Dot = XMVector2Dot(V1, V2);
    L1 = _mm_mul_ps(L1, L2);
    CosAngle = _mm_mul_ps(Dot, L1);
    CosAngle = XMVectorClamp(CosAngle, g_XMNegativeOne,g_XMOne);
    Result = XMVectorACos(CosAngle);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2LinePointDistance
(
    FXMVECTOR LinePoint1, 
    FXMVECTOR LinePoint2, 
    FXMVECTOR Point
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR PointVector;
    XMVECTOR LineVector;
    XMVECTOR ReciprocalLengthSq;
    XMVECTOR PointProjectionScale;
    XMVECTOR DistanceVector;
    XMVECTOR Result;

    // Given a vector PointVector from LinePoint1 to Point and a vector
    // LineVector from LinePoint1 to LinePoint2, the scaled distance 
    // PointProjectionScale from LinePoint1 to the perpendicular projection
    // of PointVector onto the line is defined as:
    //
    //     PointProjectionScale = dot(PointVector, LineVector) / LengthSq(LineVector)

    PointVector = XMVectorSubtract(Point, LinePoint1);
    LineVector = XMVectorSubtract(LinePoint2, LinePoint1);

    ReciprocalLengthSq = XMVector2LengthSq(LineVector);
    ReciprocalLengthSq = XMVectorReciprocal(ReciprocalLengthSq);

    PointProjectionScale = XMVector2Dot(PointVector, LineVector);
    PointProjectionScale = XMVectorMultiply(PointProjectionScale, ReciprocalLengthSq);

    DistanceVector = XMVectorMultiply(LineVector, PointProjectionScale);
    DistanceVector = XMVectorSubtract(PointVector, DistanceVector);

    Result = XMVector2Length(DistanceVector);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR PointVector = _mm_sub_ps(Point,LinePoint1);
    XMVECTOR LineVector = _mm_sub_ps(LinePoint2,LinePoint1);
    XMVECTOR ReciprocalLengthSq = XMVector2LengthSq(LineVector);
    XMVECTOR vResult = XMVector2Dot(PointVector,LineVector);
    vResult = _mm_div_ps(vResult,ReciprocalLengthSq);
    vResult = _mm_mul_ps(vResult,LineVector);
    vResult = _mm_sub_ps(PointVector,vResult);
    vResult = XMVector2Length(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2IntersectLine
(
    FXMVECTOR Line1Point1, 
    FXMVECTOR Line1Point2, 
    FXMVECTOR Line2Point1, 
    CXMVECTOR Line2Point2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR        V1;
    XMVECTOR        V2;
    XMVECTOR        V3;
    XMVECTOR        C1;
    XMVECTOR        C2;
    XMVECTOR        Result;
    CONST XMVECTOR  Zero = XMVectorZero();

    V1 = XMVectorSubtract(Line1Point2, Line1Point1);
    V2 = XMVectorSubtract(Line2Point2, Line2Point1);
    V3 = XMVectorSubtract(Line1Point1, Line2Point1);

    C1 = XMVector2Cross(V1, V2);
    C2 = XMVector2Cross(V2, V3);

    if (XMVector2NearEqual(C1, Zero, g_XMEpsilon.v))
    {
        if (XMVector2NearEqual(C2, Zero, g_XMEpsilon.v))
        {
            // Coincident
            Result = g_XMInfinity.v;
        }
        else
        {
            // Parallel
            Result = g_XMQNaN.v;
        }
    }
    else
    {
        // Intersection point = Line1Point1 + V1 * (C2 / C1)
        XMVECTOR Scale;
        Scale = XMVectorReciprocal(C1);
        Scale = XMVectorMultiply(C2, Scale);
        Result = XMVectorMultiplyAdd(V1, Scale, Line1Point1);
    }

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR V1 = _mm_sub_ps(Line1Point2, Line1Point1);
    XMVECTOR V2 = _mm_sub_ps(Line2Point2, Line2Point1);
    XMVECTOR V3 = _mm_sub_ps(Line1Point1, Line2Point1);
    // Generate the cross products
    XMVECTOR C1 = XMVector2Cross(V1, V2);
    XMVECTOR C2 = XMVector2Cross(V2, V3);
    // If C1 is not close to epsilon, use the calculated value
    XMVECTOR vResultMask = _mm_setzero_ps();
    vResultMask = _mm_sub_ps(vResultMask,C1);
    vResultMask = _mm_max_ps(vResultMask,C1);
    // 0xFFFFFFFF if the calculated value is to be used
    vResultMask = _mm_cmpgt_ps(vResultMask,g_XMEpsilon);
    // If C1 is close to epsilon, which fail type is it? INFINITY or NAN?
    XMVECTOR vFailMask = _mm_setzero_ps();
    vFailMask = _mm_sub_ps(vFailMask,C2);
    vFailMask = _mm_max_ps(vFailMask,C2);
    vFailMask = _mm_cmple_ps(vFailMask,g_XMEpsilon);
    XMVECTOR vFail = _mm_and_ps(vFailMask,g_XMInfinity);
    vFailMask = _mm_andnot_ps(vFailMask,g_XMQNaN);
    // vFail is NAN or INF
    vFail = _mm_or_ps(vFail,vFailMask);
    // Intersection point = Line1Point1 + V1 * (C2 / C1)
    XMVECTOR vResult = _mm_div_ps(C2,C1);
    vResult = _mm_mul_ps(vResult,V1);
    vResult = _mm_add_ps(vResult,Line1Point1);
    // Use result, or failure value
    vResult = _mm_and_ps(vResult,vResultMask);
    vResultMask = _mm_andnot_ps(vResultMask,vFail);
    vResult = _mm_or_ps(vResult,vResultMask);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2Transform
(
    FXMVECTOR V, 
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Result;

    Y = XMVectorSplatY(V);
    X = XMVectorSplatX(V);

    Result = XMVectorMultiplyAdd(Y, M.r[1], M.r[3]);
    Result = XMVectorMultiplyAdd(X, M.r[0], Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,0,0,0));
    vResult = _mm_mul_ps(vResult,M.r[0]);
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    vTemp = _mm_mul_ps(vTemp,M.r[1]);
    vResult = _mm_add_ps(vResult,vTemp);
    vResult = _mm_add_ps(vResult,M.r[3]);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT4* XMVector2TransformStream
(
    XMFLOAT4*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT2* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX        M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Result;
    UINT     i;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat2((XMFLOAT2*)pInputVector);
        Y = XMVectorSplatY(V);
        X = XMVectorSplatX(V);
//        Y = XMVectorReplicate(((XMFLOAT2*)pInputVector)->y);
//        X = XMVectorReplicate(((XMFLOAT2*)pInputVector)->x);

        Result = XMVectorMultiplyAdd(Y, M.r[1], M.r[3]);
        Result = XMVectorMultiplyAdd(X, M.r[0], Result);

        XMStoreFloat4((XMFLOAT4*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
	XMASSERT(pOutputStream);
	XMASSERT(pInputStream);
    UINT i;
    const BYTE* pInputVector = (const BYTE*)pInputStream;
    BYTE* pOutputVector = (BYTE*)pOutputStream;

    for (i = 0; i < VectorCount; i++)
    {
        XMVECTOR X = _mm_load_ps1(&reinterpret_cast<const XMFLOAT2*>(pInputVector)->x);
        XMVECTOR vResult = _mm_load_ps1(&reinterpret_cast<const XMFLOAT2*>(pInputVector)->y);
        vResult = _mm_mul_ps(vResult,M.r[1]);
        vResult = _mm_add_ps(vResult,M.r[3]);
        X = _mm_mul_ps(X,M.r[0]);
        vResult = _mm_add_ps(vResult,X);
        _mm_storeu_ps(reinterpret_cast<float*>(pOutputVector),vResult);
        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }
    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT4* XMVector2TransformStreamNC
(
    XMFLOAT4*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT2* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX     M
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(XM_NO_MISALIGNED_VECTOR_ACCESS) || defined(_XM_SSE_INTRINSICS_)
	return XMVector2TransformStream( pOutputStream, OutputStride, pInputStream, InputStride, VectorCount, M );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2TransformCoord
(
    FXMVECTOR V, 
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR InverseW;
    XMVECTOR Result;

    Y = XMVectorSplatY(V);
    X = XMVectorSplatX(V);

    Result = XMVectorMultiplyAdd(Y, M.r[1], M.r[3]);
    Result = XMVectorMultiplyAdd(X, M.r[0], Result);

    InverseW = XMVectorSplatW(Result);
    InverseW = XMVectorReciprocal(InverseW);

    Result = XMVectorMultiply(Result, InverseW);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,0,0,0));
    vResult = _mm_mul_ps(vResult,M.r[0]);
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    vTemp = _mm_mul_ps(vTemp,M.r[1]);
    vResult = _mm_add_ps(vResult,vTemp);
    vResult = _mm_add_ps(vResult,M.r[3]);
    vTemp = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,3,3,3));
    vResult = _mm_div_ps(vResult,vTemp);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT2* XMVector2TransformCoordStream
(
    XMFLOAT2*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT2* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX     M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR InverseW;
    XMVECTOR Result;
    UINT     i;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat2((XMFLOAT2*)pInputVector);
        Y = XMVectorSplatY(V);
        X = XMVectorSplatX(V);
//        Y = XMVectorReplicate(((XMFLOAT2*)pInputVector)->y);
//        X = XMVectorReplicate(((XMFLOAT2*)pInputVector)->x);

        Result = XMVectorMultiplyAdd(Y, M.r[1], M.r[3]);
        Result = XMVectorMultiplyAdd(X, M.r[0], Result);

        InverseW = XMVectorSplatW(Result);
        InverseW = XMVectorReciprocal(InverseW);

        Result = XMVectorMultiply(Result, InverseW);

        XMStoreFloat2((XMFLOAT2*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);
    UINT i;
    const BYTE *pInputVector = (BYTE*)pInputStream;
    BYTE *pOutputVector = (BYTE*)pOutputStream;

    for (i = 0; i < VectorCount; i++)
    {
        XMVECTOR X = _mm_load_ps1(&reinterpret_cast<const XMFLOAT2*>(pInputVector)->x);
        XMVECTOR vResult = _mm_load_ps1(&reinterpret_cast<const XMFLOAT2*>(pInputVector)->y);
        vResult = _mm_mul_ps(vResult,M.r[1]);
        vResult = _mm_add_ps(vResult,M.r[3]);
        X = _mm_mul_ps(X,M.r[0]);
        vResult = _mm_add_ps(vResult,X);
        X = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,3,3,3));
        vResult = _mm_div_ps(vResult,X);
        _mm_store_sd(reinterpret_cast<double *>(pOutputVector),reinterpret_cast<__m128d *>(&vResult)[0]);
        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }
    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector2TransformNormal
(
    FXMVECTOR V, 
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Result;

    Y = XMVectorSplatY(V);
    X = XMVectorSplatX(V);

    Result = XMVectorMultiply(Y, M.r[1]);
    Result = XMVectorMultiplyAdd(X, M.r[0], Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,0,0,0));
    vResult = _mm_mul_ps(vResult,M.r[0]);
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    vTemp = _mm_mul_ps(vTemp,M.r[1]);
    vResult = _mm_add_ps(vResult,vTemp);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT2* XMVector2TransformNormalStream
(
    XMFLOAT2*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT2* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX        M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Result;
    UINT     i;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat2((XMFLOAT2*)pInputVector);
        Y = XMVectorSplatY(V);
        X = XMVectorSplatX(V);
//        Y = XMVectorReplicate(((XMFLOAT2*)pInputVector)->y);
//        X = XMVectorReplicate(((XMFLOAT2*)pInputVector)->x);

        Result = XMVectorMultiply(Y, M.r[1]);
        Result = XMVectorMultiplyAdd(X, M.r[0], Result);

        XMStoreFloat2((XMFLOAT2*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);
    UINT i;
    const BYTE*pInputVector = (const BYTE*)pInputStream;
    BYTE *pOutputVector = (BYTE*)pOutputStream;
    for (i = 0; i < VectorCount; i++)
    {
        XMVECTOR X = _mm_load_ps1(&reinterpret_cast<const XMFLOAT2 *>(pInputVector)->x);
        XMVECTOR vResult = _mm_load_ps1(&reinterpret_cast<const XMFLOAT2 *>(pInputVector)->y);
        vResult = _mm_mul_ps(vResult,M.r[1]);
        X = _mm_mul_ps(X,M.r[0]);
        vResult = _mm_add_ps(vResult,X);
        _mm_store_sd(reinterpret_cast<double*>(pOutputVector),reinterpret_cast<const __m128d *>(&vResult)[0]);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

/****************************************************************************
 *
 * 3D Vector
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3Equal
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] == V2.vector4_f32[0]) && (V1.vector4_f32[1] == V2.vector4_f32[1]) && (V1.vector4_f32[2] == V2.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&7)==7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector3EqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector3EqualR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if ((V1.vector4_f32[0] == V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] == V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] == V2.vector4_f32[2]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] != V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] != V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] != V2.vector4_f32[2]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
    int iTest = _mm_movemask_ps(vTemp)&7;
    UINT CR = 0;
    if (iTest==7)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3EqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_u32[0] == V2.vector4_u32[0]) && (V1.vector4_u32[1] == V2.vector4_u32[1]) && (V1.vector4_u32[2] == V2.vector4_u32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    return (((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])&7)==7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector3EqualIntR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector3EqualIntR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if ((V1.vector4_u32[0] == V2.vector4_u32[0]) && 
        (V1.vector4_u32[1] == V2.vector4_u32[1]) &&
        (V1.vector4_u32[2] == V2.vector4_u32[2]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_u32[0] != V2.vector4_u32[0]) && 
        (V1.vector4_u32[1] != V2.vector4_u32[1]) &&
        (V1.vector4_u32[2] != V2.vector4_u32[2]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    int iTemp = _mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])&7;
    UINT CR = 0;
    if (iTemp==7)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTemp)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3NearEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR Epsilon
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT dx, dy, dz;

    dx = fabsf(V1.vector4_f32[0]-V2.vector4_f32[0]);
    dy = fabsf(V1.vector4_f32[1]-V2.vector4_f32[1]);
    dz = fabsf(V1.vector4_f32[2]-V2.vector4_f32[2]);
    return (((dx <= Epsilon.vector4_f32[0]) &&
            (dy <= Epsilon.vector4_f32[1]) &&
            (dz <= Epsilon.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    // Get the difference
    XMVECTOR vDelta = _mm_sub_ps(V1,V2);
    // Get the absolute value of the difference
    XMVECTOR vTemp = _mm_setzero_ps();
    vTemp = _mm_sub_ps(vTemp,vDelta);
    vTemp = _mm_max_ps(vTemp,vDelta);
    vTemp = _mm_cmple_ps(vTemp,Epsilon);
    // w is don't care
    return (((_mm_movemask_ps(vTemp)&7)==0x7) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3NotEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] != V2.vector4_f32[0]) || (V1.vector4_f32[1] != V2.vector4_f32[1]) || (V1.vector4_f32[2] != V2.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&7)!=7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAnyFalse(XMVector3EqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3NotEqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_u32[0] != V2.vector4_u32[0]) || (V1.vector4_u32[1] != V2.vector4_u32[1]) || (V1.vector4_u32[2] != V2.vector4_u32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    return (((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])&7)!=7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAnyFalse(XMVector3EqualIntR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3Greater
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] > V2.vector4_f32[0]) && (V1.vector4_f32[1] > V2.vector4_f32[1]) && (V1.vector4_f32[2] > V2.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpgt_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&7)==7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector3GreaterR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector3GreaterR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if ((V1.vector4_f32[0] > V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] > V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] > V2.vector4_f32[2]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] <= V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] <= V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] <= V2.vector4_f32[2]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpgt_ps(V1,V2);
    UINT CR = 0;
    int iTest = _mm_movemask_ps(vTemp)&7;
    if (iTest==7) 
    {
        CR =  XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3GreaterOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] >= V2.vector4_f32[0]) && (V1.vector4_f32[1] >= V2.vector4_f32[1]) && (V1.vector4_f32[2] >= V2.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpge_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&7)==7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector3GreaterOrEqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector3GreaterOrEqualR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    UINT CR = 0;
    if ((V1.vector4_f32[0] >= V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] >= V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] >= V2.vector4_f32[2]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] < V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] < V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] < V2.vector4_f32[2]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpge_ps(V1,V2);
    UINT CR = 0;
    int iTest = _mm_movemask_ps(vTemp)&7;
    if (iTest==7) 
    {
        CR =  XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3Less
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] < V2.vector4_f32[0]) && (V1.vector4_f32[1] < V2.vector4_f32[1]) && (V1.vector4_f32[2] < V2.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmplt_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&7)==7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector3GreaterR(V2, V1));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3LessOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] <= V2.vector4_f32[0]) && (V1.vector4_f32[1] <= V2.vector4_f32[1]) && (V1.vector4_f32[2] <= V2.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmple_ps(V1,V2);
    return (((_mm_movemask_ps(vTemp)&7)==7) != 0);
#else // _XM_VMX128_INTRINSICS_
    return XMComparisonAllTrue(XMVector3GreaterOrEqualR(V2, V1));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3InBounds
(
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) && 
        (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1]) &&
        (V.vector4_f32[2] <= Bounds.vector4_f32[2] && V.vector4_f32[2] >= -Bounds.vector4_f32[2])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    // x,y and z in bounds? (w is don't care)
    return (((_mm_movemask_ps(vTemp1)&0x7)==0x7) != 0);
#else
    return XMComparisonAllInBounds(XMVector3InBoundsR(V, Bounds));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector3InBoundsR
(
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if ((V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) && 
        (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1]) &&
        (V.vector4_f32[2] <= Bounds.vector4_f32[2] && V.vector4_f32[2] >= -Bounds.vector4_f32[2]))
    {
        CR = XM_CRMASK_CR6BOUNDS;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    // x,y and z in bounds? (w is don't care)
    return ((_mm_movemask_ps(vTemp1)&0x7)==0x7) ? XM_CRMASK_CR6BOUNDS : 0;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3IsNaN
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    return (XMISNAN(V.vector4_f32[0]) ||
            XMISNAN(V.vector4_f32[1]) ||
            XMISNAN(V.vector4_f32[2]));

#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the exponent
    __m128i vTempInf = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMInfinity);
    // Mask off the mantissa
    __m128i vTempNan = _mm_and_si128(reinterpret_cast<const __m128i *>(&V)[0],g_XMQNaNTest);
    // Are any of the exponents == 0x7F800000?
    vTempInf = _mm_cmpeq_epi32(vTempInf,g_XMInfinity);
    // Are any of the mantissa's zero? (SSE2 doesn't have a neq test)
    vTempNan = _mm_cmpeq_epi32(vTempNan,g_XMZero);
    // Perform a not on the NaN test to be true on NON-zero mantissas
    vTempNan = _mm_andnot_si128(vTempNan,vTempInf);
    // If x, y or z are NaN, the signs are true after the merge above
    return ((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTempNan)[0])&7) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector3IsInfinite
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (XMISINF(V.vector4_f32[0]) ||
            XMISINF(V.vector4_f32[1]) ||
            XMISINF(V.vector4_f32[2]));
#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the sign bit
    __m128 vTemp = _mm_and_ps(V,g_XMAbsMask);
    // Compare to infinity
    vTemp = _mm_cmpeq_ps(vTemp,g_XMInfinity);
    // If x,y or z are infinity, the signs are true.
    return ((_mm_movemask_ps(vTemp)&7) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Dot
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fValue = V1.vector4_f32[0] * V2.vector4_f32[0] + V1.vector4_f32[1] * V2.vector4_f32[1] + V1.vector4_f32[2] * V2.vector4_f32[2];
    XMVECTOR vResult = {
        fValue,
        fValue,
        fValue,
        fValue
    };            
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product
    XMVECTOR vDot = _mm_mul_ps(V1,V2);
    // x=Dot.vector4_f32[1], y=Dot.vector4_f32[2]
    XMVECTOR vTemp = _mm_shuffle_ps(vDot,vDot,_MM_SHUFFLE(2,1,2,1));
    // Result.vector4_f32[0] = x+y
    vDot = _mm_add_ss(vDot,vTemp);
    // x=Dot.vector4_f32[2]
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(1,1,1,1));
    // Result.vector4_f32[0] = (x+y)+z
    vDot = _mm_add_ss(vDot,vTemp);
    // Splat x
	return _mm_shuffle_ps(vDot,vDot,_MM_SHUFFLE(0,0,0,0));
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Cross
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR vResult = {
        (V1.vector4_f32[1] * V2.vector4_f32[2]) - (V1.vector4_f32[2] * V2.vector4_f32[1]),
        (V1.vector4_f32[2] * V2.vector4_f32[0]) - (V1.vector4_f32[0] * V2.vector4_f32[2]),
        (V1.vector4_f32[0] * V2.vector4_f32[1]) - (V1.vector4_f32[1] * V2.vector4_f32[0]),
        0.0f
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    // y1,z1,x1,w1
    XMVECTOR vTemp1 = _mm_shuffle_ps(V1,V1,_MM_SHUFFLE(3,0,2,1));
    // z2,x2,y2,w2
    XMVECTOR vTemp2 = _mm_shuffle_ps(V2,V2,_MM_SHUFFLE(3,1,0,2));
    // Perform the left operation
    XMVECTOR vResult = _mm_mul_ps(vTemp1,vTemp2);
    // z1,x1,y1,w1
    vTemp1 = _mm_shuffle_ps(vTemp1,vTemp1,_MM_SHUFFLE(3,0,2,1));
    // y2,z2,x2,w2
    vTemp2 = _mm_shuffle_ps(vTemp2,vTemp2,_MM_SHUFFLE(3,1,0,2));
    // Perform the right operation
    vTemp1 = _mm_mul_ps(vTemp1,vTemp2);
    // Subract the right from left, and return answer
    vResult = _mm_sub_ps(vResult,vTemp1);
    // Set w to zero
    return _mm_and_ps(vResult,g_XMMask3);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3LengthSq
(
    FXMVECTOR V
)
{
    return XMVector3Dot(V, V);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3ReciprocalLengthEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector3LengthSq(V);
    Result = XMVectorReciprocalSqrtEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y and z
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and y
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,2,1,2));
    // x+z, y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    // y,y,y,y
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(1,1,1,1));
    // x+z+y,??,??,??
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    // Splat the length squared
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    // Get the reciprocal
    vLengthSq = _mm_rsqrt_ps(vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3ReciprocalLength
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector3LengthSq(V);
    Result = XMVectorReciprocalSqrt(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
     // Perform the dot product
    XMVECTOR vDot = _mm_mul_ps(V,V);
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
    vDot = _mm_sqrt_ps(vDot);
    // Get the reciprocal
    vDot = _mm_div_ps(g_XMOne,vDot);
    return vDot;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3LengthEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector3LengthSq(V);
    Result = XMVectorSqrtEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y and z
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and y
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,2,1,2));
    // x+z, y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    // y,y,y,y
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(1,1,1,1));
    // x+z+y,??,??,??
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    // Splat the length squared
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    // Get the length
    vLengthSq = _mm_sqrt_ps(vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Length
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector3LengthSq(V);
    Result = XMVectorSqrt(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y and z
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and y
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,2,1,2));
    // x+z, y
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    // y,y,y,y
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(1,1,1,1));
    // x+z+y,??,??,??
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    // Splat the length squared
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    // Get the length
    vLengthSq = _mm_sqrt_ps(vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// XMVector3NormalizeEst uses a reciprocal estimate and
// returns QNaN on zero and infinite vectors.

XMFINLINE XMVECTOR XMVector3NormalizeEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result = XMVector3ReciprocalLength(V);
    Result = XMVectorMultiply(V, Result);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
     // Perform the dot product
    XMVECTOR vDot = _mm_mul_ps(V,V);
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
    // Perform the normalization
    vDot = _mm_mul_ps(vDot,V);
    return vDot;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Normalize
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fLength;
    XMVECTOR vResult;

    vResult = XMVector3Length( V );
    fLength = vResult.vector4_f32[0];

    // Prevent divide by zero
    if (fLength > 0) {
        fLength = 1.0f/fLength;
    }
    
    vResult.vector4_f32[0] = V.vector4_f32[0]*fLength;
    vResult.vector4_f32[1] = V.vector4_f32[1]*fLength;
    vResult.vector4_f32[2] = V.vector4_f32[2]*fLength;
    vResult.vector4_f32[3] = V.vector4_f32[3]*fLength;
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y and z only
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,1,2,1));
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
    vTemp = _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(1,1,1,1));
    vLengthSq = _mm_add_ss(vLengthSq,vTemp);
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(0,0,0,0));
    // Prepare for the division
    XMVECTOR vResult = _mm_sqrt_ps(vLengthSq);
    // Create zero with a single instruction
    XMVECTOR vZeroMask = _mm_setzero_ps();
    // Test for a divide by zero (Must be FP to detect -0.0)
    vZeroMask = _mm_cmpneq_ps(vZeroMask,vResult);
    // Failsafe on zero (Or epsilon) length planes
    // If the length is infinity, set the elements to zero
    vLengthSq = _mm_cmpneq_ps(vLengthSq,g_XMInfinity);
    // Divide to perform the normalization
    vResult = _mm_div_ps(V,vResult);
    // Any that are infinity, set to zero
    vResult = _mm_and_ps(vResult,vZeroMask);
    // Select qnan or result based on infinite length
	XMVECTOR vTemp1 = _mm_andnot_ps(vLengthSq,g_XMQNaN);
    XMVECTOR vTemp2 = _mm_and_ps(vResult,vLengthSq);
    vResult = _mm_or_ps(vTemp1,vTemp2);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3ClampLength
(
    FXMVECTOR V, 
    FLOAT    LengthMin, 
    FLOAT    LengthMax
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR ClampMax;
    XMVECTOR ClampMin;

    ClampMax = XMVectorReplicate(LengthMax);
    ClampMin = XMVectorReplicate(LengthMin);

    return XMVector3ClampLengthV(V, ClampMin, ClampMax);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR ClampMax = _mm_set_ps1(LengthMax);
    XMVECTOR ClampMin = _mm_set_ps1(LengthMin);
    return XMVector3ClampLengthV(V,ClampMin,ClampMax);
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3ClampLengthV
(
    FXMVECTOR V, 
    FXMVECTOR LengthMin, 
    FXMVECTOR LengthMax
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR ClampLength;
    XMVECTOR LengthSq;
    XMVECTOR RcpLength;
    XMVECTOR Length;
    XMVECTOR Normal;
    XMVECTOR Zero;
    XMVECTOR InfiniteLength;
    XMVECTOR ZeroLength;
    XMVECTOR Select;
    XMVECTOR ControlMax;
    XMVECTOR ControlMin;
    XMVECTOR Control;
    XMVECTOR Result;

    XMASSERT((LengthMin.vector4_f32[1] == LengthMin.vector4_f32[0]) && (LengthMin.vector4_f32[2] == LengthMin.vector4_f32[0]));
    XMASSERT((LengthMax.vector4_f32[1] == LengthMax.vector4_f32[0]) && (LengthMax.vector4_f32[2] == LengthMax.vector4_f32[0]));
    XMASSERT(XMVector3GreaterOrEqual(LengthMin, XMVectorZero()));
    XMASSERT(XMVector3GreaterOrEqual(LengthMax, XMVectorZero()));
    XMASSERT(XMVector3GreaterOrEqual(LengthMax, LengthMin));

    LengthSq = XMVector3LengthSq(V);

    Zero = XMVectorZero();

    RcpLength = XMVectorReciprocalSqrt(LengthSq);

    InfiniteLength = XMVectorEqualInt(LengthSq, g_XMInfinity.v);
    ZeroLength = XMVectorEqual(LengthSq, Zero);

    Normal = XMVectorMultiply(V, RcpLength);

    Length = XMVectorMultiply(LengthSq, RcpLength);

    Select = XMVectorEqualInt(InfiniteLength, ZeroLength);
    Length = XMVectorSelect(LengthSq, Length, Select);
    Normal = XMVectorSelect(LengthSq, Normal, Select);

    ControlMax = XMVectorGreater(Length, LengthMax);
    ControlMin = XMVectorLess(Length, LengthMin);

    ClampLength = XMVectorSelect(Length, LengthMax, ControlMax);
    ClampLength = XMVectorSelect(ClampLength, LengthMin, ControlMin);

    Result = XMVectorMultiply(Normal, ClampLength);

    // Preserve the original vector (with no precision loss) if the length falls within the given range
    Control = XMVectorEqualInt(ControlMax, ControlMin);
    Result = XMVectorSelect(Result, V, Control);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR ClampLength;
    XMVECTOR LengthSq;
    XMVECTOR RcpLength;
    XMVECTOR Length;
    XMVECTOR Normal;
    XMVECTOR InfiniteLength;
    XMVECTOR ZeroLength;
    XMVECTOR Select;
    XMVECTOR ControlMax;
    XMVECTOR ControlMin;
    XMVECTOR Control;
    XMVECTOR Result;

    XMASSERT((XMVectorGetY(LengthMin) == XMVectorGetX(LengthMin)) && (XMVectorGetZ(LengthMin) == XMVectorGetX(LengthMin)));
    XMASSERT((XMVectorGetY(LengthMax) == XMVectorGetX(LengthMax)) && (XMVectorGetZ(LengthMax) == XMVectorGetX(LengthMax)));
    XMASSERT(XMVector3GreaterOrEqual(LengthMin, g_XMZero));
    XMASSERT(XMVector3GreaterOrEqual(LengthMax, g_XMZero));
    XMASSERT(XMVector3GreaterOrEqual(LengthMax, LengthMin));

    LengthSq = XMVector3LengthSq(V);
    RcpLength = XMVectorReciprocalSqrt(LengthSq);
    InfiniteLength = XMVectorEqualInt(LengthSq, g_XMInfinity);
    ZeroLength = XMVectorEqual(LengthSq,g_XMZero);
    Normal = _mm_mul_ps(V, RcpLength);
    Length = _mm_mul_ps(LengthSq, RcpLength);
    Select = XMVectorEqualInt(InfiniteLength, ZeroLength);
    Length = XMVectorSelect(LengthSq, Length, Select);
    Normal = XMVectorSelect(LengthSq, Normal, Select);
    ControlMax = XMVectorGreater(Length, LengthMax);
    ControlMin = XMVectorLess(Length, LengthMin);
    ClampLength = XMVectorSelect(Length, LengthMax, ControlMax);
    ClampLength = XMVectorSelect(ClampLength, LengthMin, ControlMin);
    Result = _mm_mul_ps(Normal, ClampLength);
    // Preserve the original vector (with no precision loss) if the length falls within the given range
    Control = XMVectorEqualInt(ControlMax, ControlMin);
    Result = XMVectorSelect(Result, V, Control);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Reflect
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    // Result = Incident - (2 * dot(Incident, Normal)) * Normal
    Result = XMVector3Dot(Incident, Normal);
    Result = XMVectorAdd(Result, Result);
    Result = XMVectorNegativeMultiplySubtract(Result, Normal, Incident);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Result = Incident - (2 * dot(Incident, Normal)) * Normal
    XMVECTOR Result = XMVector3Dot(Incident, Normal);
    Result = _mm_add_ps(Result, Result);
    Result = _mm_mul_ps(Result, Normal);
    Result = _mm_sub_ps(Incident,Result);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Refract
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal, 
    FLOAT    RefractionIndex
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Index;
    Index = XMVectorReplicate(RefractionIndex);
    return XMVector3RefractV(Incident, Normal, Index);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR Index = _mm_set_ps1(RefractionIndex);
    return XMVector3RefractV(Incident,Normal,Index);
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3RefractV
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal, 
    FXMVECTOR RefractionIndex
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR        IDotN;
    XMVECTOR        R;
    CONST XMVECTOR  Zero = XMVectorZero();

    // Result = RefractionIndex * Incident - Normal * (RefractionIndex * dot(Incident, Normal) + 
    // sqrt(1 - RefractionIndex * RefractionIndex * (1 - dot(Incident, Normal) * dot(Incident, Normal))))

    IDotN = XMVector3Dot(Incident, Normal);

    // R = 1.0f - RefractionIndex * RefractionIndex * (1.0f - IDotN * IDotN)
    R = XMVectorNegativeMultiplySubtract(IDotN, IDotN, g_XMOne.v);
    R = XMVectorMultiply(R, RefractionIndex);
    R = XMVectorNegativeMultiplySubtract(R, RefractionIndex, g_XMOne.v);

    if (XMVector4LessOrEqual(R, Zero))
    {
        // Total internal reflection
        return Zero;
    }
    else
    {
        XMVECTOR Result;

        // R = RefractionIndex * IDotN + sqrt(R)
        R = XMVectorSqrt(R);
        R = XMVectorMultiplyAdd(RefractionIndex, IDotN, R);

        // Result = RefractionIndex * Incident - Normal * R
        Result = XMVectorMultiply(RefractionIndex, Incident);
        Result = XMVectorNegativeMultiplySubtract(Normal, R, Result);

        return Result;
    }

#elif defined(_XM_SSE_INTRINSICS_)
    // Result = RefractionIndex * Incident - Normal * (RefractionIndex * dot(Incident, Normal) + 
    // sqrt(1 - RefractionIndex * RefractionIndex * (1 - dot(Incident, Normal) * dot(Incident, Normal))))
    XMVECTOR IDotN = XMVector3Dot(Incident, Normal);
    // R = 1.0f - RefractionIndex * RefractionIndex * (1.0f - IDotN * IDotN)
    XMVECTOR R = _mm_mul_ps(IDotN, IDotN);
    R = _mm_sub_ps(g_XMOne,R);
    R = _mm_mul_ps(R, RefractionIndex);
    R = _mm_mul_ps(R, RefractionIndex);
    R = _mm_sub_ps(g_XMOne,R);

    XMVECTOR vResult = _mm_cmple_ps(R,g_XMZero);
    if (_mm_movemask_ps(vResult)==0x0f)
    {
        // Total internal reflection
        vResult = g_XMZero;
    }
    else
    {
        // R = RefractionIndex * IDotN + sqrt(R)
        R = _mm_sqrt_ps(R);
        vResult = _mm_mul_ps(RefractionIndex,IDotN);
        R = _mm_add_ps(R,vResult);
        // Result = RefractionIndex * Incident - Normal * R
        vResult = _mm_mul_ps(RefractionIndex, Incident);
        R = _mm_mul_ps(R,Normal);
        vResult = _mm_sub_ps(vResult,R);
    }
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Orthogonal
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR NegativeV;
    XMVECTOR Z, YZYY;
    XMVECTOR ZIsNegative, YZYYIsNegative;
    XMVECTOR S, D;
    XMVECTOR R0, R1;
    XMVECTOR Select;
    XMVECTOR Zero;
    XMVECTOR Result;
    static CONST XMVECTORU32 Permute1X0X0X0X = {XM_PERMUTE_1X, XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0X};
    static CONST XMVECTORU32 Permute0Y0Z0Y0Y= {XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_0Y};

    Zero = XMVectorZero();
    Z = XMVectorSplatZ(V);
    YZYY = XMVectorPermute(V, V, Permute0Y0Z0Y0Y.v);

    NegativeV = XMVectorSubtract(Zero, V);

    ZIsNegative = XMVectorLess(Z, Zero);
    YZYYIsNegative = XMVectorLess(YZYY, Zero);

    S = XMVectorAdd(YZYY, Z);
    D = XMVectorSubtract(YZYY, Z);

    Select = XMVectorEqualInt(ZIsNegative, YZYYIsNegative);

    R0 = XMVectorPermute(NegativeV, S, Permute1X0X0X0X.v);
    R1 = XMVectorPermute(V, D, Permute1X0X0X0X.v);

    Result = XMVectorSelect(R1, R0, Select);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR NegativeV;
    XMVECTOR Z, YZYY;
    XMVECTOR ZIsNegative, YZYYIsNegative;
    XMVECTOR S, D;
    XMVECTOR R0, R1;
    XMVECTOR Select;
    XMVECTOR Zero;
    XMVECTOR Result;
    static CONST XMVECTORI32 Permute1X0X0X0X = {XM_PERMUTE_1X, XM_PERMUTE_0X, XM_PERMUTE_0X, XM_PERMUTE_0X};
    static CONST XMVECTORI32 Permute0Y0Z0Y0Y= {XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_0Y, XM_PERMUTE_0Y};

    Zero = XMVectorZero();
    Z = XMVectorSplatZ(V);
    YZYY = XMVectorPermute(V, V, Permute0Y0Z0Y0Y);

    NegativeV = _mm_sub_ps(Zero, V);

    ZIsNegative = XMVectorLess(Z, Zero);
    YZYYIsNegative = XMVectorLess(YZYY, Zero);

    S = _mm_add_ps(YZYY, Z);
    D = _mm_sub_ps(YZYY, Z);

    Select = XMVectorEqualInt(ZIsNegative, YZYYIsNegative);

    R0 = XMVectorPermute(NegativeV, S, Permute1X0X0X0X);
    R1 = XMVectorPermute(V, D,Permute1X0X0X0X);
    Result = XMVectorSelect(R1, R0, Select);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3AngleBetweenNormalsEst
(
    FXMVECTOR N1, 
    FXMVECTOR N2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    XMVECTOR NegativeOne;
    XMVECTOR One;

    Result = XMVector3Dot(N1, N2);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    Result = XMVectorClamp(Result, NegativeOne, One);
    Result = XMVectorACosEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = XMVector3Dot(N1,N2);
    // Clamp to -1.0f to 1.0f
    vResult = _mm_max_ps(vResult,g_XMNegativeOne);
    vResult = _mm_min_ps(vResult,g_XMOne);
    vResult = XMVectorACosEst(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3AngleBetweenNormals
(
    FXMVECTOR N1, 
    FXMVECTOR N2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    XMVECTOR NegativeOne;
    XMVECTOR One;

    Result = XMVector3Dot(N1, N2);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    Result = XMVectorClamp(Result, NegativeOne, One);
    Result = XMVectorACos(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = XMVector3Dot(N1,N2);
    // Clamp to -1.0f to 1.0f
    vResult = _mm_max_ps(vResult,g_XMNegativeOne);
    vResult = _mm_min_ps(vResult,g_XMOne);
    vResult = XMVectorACos(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3AngleBetweenVectors
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR L1;
    XMVECTOR L2;
    XMVECTOR Dot;
    XMVECTOR CosAngle;
    XMVECTOR NegativeOne;
    XMVECTOR One;
    XMVECTOR Result;

    L1 = XMVector3ReciprocalLength(V1);
    L2 = XMVector3ReciprocalLength(V2);

    Dot = XMVector3Dot(V1, V2);

    L1 = XMVectorMultiply(L1, L2);

    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();

    CosAngle = XMVectorMultiply(Dot, L1);

    CosAngle = XMVectorClamp(CosAngle, NegativeOne, One);

    Result = XMVectorACos(CosAngle);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR L1;
    XMVECTOR L2;
    XMVECTOR Dot;
    XMVECTOR CosAngle;
    XMVECTOR Result;

    L1 = XMVector3ReciprocalLength(V1);
    L2 = XMVector3ReciprocalLength(V2);
    Dot = XMVector3Dot(V1, V2);
    L1 = _mm_mul_ps(L1, L2);
    CosAngle = _mm_mul_ps(Dot, L1);
    CosAngle = XMVectorClamp(CosAngle,g_XMNegativeOne,g_XMOne);
    Result = XMVectorACos(CosAngle);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3LinePointDistance
(
    FXMVECTOR LinePoint1, 
    FXMVECTOR LinePoint2, 
    FXMVECTOR Point
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR PointVector;
    XMVECTOR LineVector;
    XMVECTOR ReciprocalLengthSq;
    XMVECTOR PointProjectionScale;
    XMVECTOR DistanceVector;
    XMVECTOR Result;

    // Given a vector PointVector from LinePoint1 to Point and a vector
    // LineVector from LinePoint1 to LinePoint2, the scaled distance 
    // PointProjectionScale from LinePoint1 to the perpendicular projection
    // of PointVector onto the line is defined as:
    //
    //     PointProjectionScale = dot(PointVector, LineVector) / LengthSq(LineVector)

    PointVector = XMVectorSubtract(Point, LinePoint1);
    LineVector = XMVectorSubtract(LinePoint2, LinePoint1);

    ReciprocalLengthSq = XMVector3LengthSq(LineVector);
    ReciprocalLengthSq = XMVectorReciprocal(ReciprocalLengthSq);

    PointProjectionScale = XMVector3Dot(PointVector, LineVector);
    PointProjectionScale = XMVectorMultiply(PointProjectionScale, ReciprocalLengthSq);

    DistanceVector = XMVectorMultiply(LineVector, PointProjectionScale);
    DistanceVector = XMVectorSubtract(PointVector, DistanceVector);

    Result = XMVector3Length(DistanceVector);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR PointVector = _mm_sub_ps(Point,LinePoint1);
    XMVECTOR LineVector = _mm_sub_ps(LinePoint2,LinePoint1);
    XMVECTOR ReciprocalLengthSq = XMVector3LengthSq(LineVector);
    XMVECTOR vResult = XMVector3Dot(PointVector,LineVector);
    vResult = _mm_div_ps(vResult,ReciprocalLengthSq);
    vResult = _mm_mul_ps(vResult,LineVector);
    vResult = _mm_sub_ps(PointVector,vResult);
    vResult = XMVector3Length(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE VOID XMVector3ComponentsFromNormal
(
    XMVECTOR* pParallel, 
    XMVECTOR* pPerpendicular, 
    FXMVECTOR  V, 
    FXMVECTOR  Normal
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Parallel;
    XMVECTOR Scale;

    XMASSERT(pParallel);
    XMASSERT(pPerpendicular);

    Scale = XMVector3Dot(V, Normal);

    Parallel = XMVectorMultiply(Normal, Scale);

    *pParallel = Parallel;
    *pPerpendicular = XMVectorSubtract(V, Parallel);

#elif defined(_XM_SSE_INTRINSICS_)
	XMASSERT(pParallel);
	XMASSERT(pPerpendicular);
    XMVECTOR Scale = XMVector3Dot(V, Normal);
    XMVECTOR Parallel = _mm_mul_ps(Normal,Scale);
    *pParallel = Parallel;
    *pPerpendicular = _mm_sub_ps(V,Parallel);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Transform a vector using a rotation expressed as a unit quaternion

XMFINLINE XMVECTOR XMVector3Rotate
(
    FXMVECTOR V, 
    FXMVECTOR RotationQuaternion
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR A;
    XMVECTOR Q;
    XMVECTOR Result;

    A = XMVectorSelect(g_XMSelect1110.v, V, g_XMSelect1110.v);
    Q = XMQuaternionConjugate(RotationQuaternion);
    Result = XMQuaternionMultiply(Q, A);
    Result = XMQuaternionMultiply(Result, RotationQuaternion);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR A;
    XMVECTOR Q;
    XMVECTOR Result;

    A = _mm_and_ps(V,g_XMMask3);
    Q = XMQuaternionConjugate(RotationQuaternion);
    Result = XMQuaternionMultiply(Q, A);
    Result = XMQuaternionMultiply(Result, RotationQuaternion);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Transform a vector using the inverse of a rotation expressed as a unit quaternion

XMFINLINE XMVECTOR XMVector3InverseRotate
(
    FXMVECTOR V, 
    FXMVECTOR RotationQuaternion
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR A;
    XMVECTOR Q;
    XMVECTOR Result;

    A = XMVectorSelect(g_XMSelect1110.v, V, g_XMSelect1110.v);
    Result = XMQuaternionMultiply(RotationQuaternion, A);
    Q = XMQuaternionConjugate(RotationQuaternion);
    Result = XMQuaternionMultiply(Result, Q);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR A;
    XMVECTOR Q;
    XMVECTOR Result;
    A = _mm_and_ps(V,g_XMMask3);
    Result = XMQuaternionMultiply(RotationQuaternion, A);
    Q = XMQuaternionConjugate(RotationQuaternion);
    Result = XMQuaternionMultiply(Result, Q);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Transform
(
    FXMVECTOR V, 
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR Result;

    Z = XMVectorSplatZ(V);
    Y = XMVectorSplatY(V);
    X = XMVectorSplatX(V);

    Result = XMVectorMultiplyAdd(Z, M.r[2], M.r[3]);
    Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
    Result = XMVectorMultiplyAdd(X, M.r[0], Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,0,0,0));
    vResult = _mm_mul_ps(vResult,M.r[0]);
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    vTemp = _mm_mul_ps(vTemp,M.r[1]);
    vResult = _mm_add_ps(vResult,vTemp);
    vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(2,2,2,2));
    vTemp = _mm_mul_ps(vTemp,M.r[2]);
    vResult = _mm_add_ps(vResult,vTemp);
    vResult = _mm_add_ps(vResult,M.r[3]);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT4* XMVector3TransformStream
(
    XMFLOAT4*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT3* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX     M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR Result;
    UINT     i;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat3((XMFLOAT3*)pInputVector);
        Z = XMVectorSplatZ(V);
        Y = XMVectorSplatY(V);
        X = XMVectorSplatX(V);

        Result = XMVectorMultiplyAdd(Z, M.r[2], M.r[3]);
        Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
        Result = XMVectorMultiplyAdd(X, M.r[0], Result);

        XMStoreFloat4((XMFLOAT4*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);
    UINT     i;
    const BYTE* pInputVector = (const BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    for (i = 0; i < VectorCount; i++)
    {
        XMVECTOR X = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->x);
        XMVECTOR Y = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->y);
        XMVECTOR vResult = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->z);
        vResult = _mm_mul_ps(vResult,M.r[2]);
        vResult = _mm_add_ps(vResult,M.r[3]);
        Y = _mm_mul_ps(Y,M.r[1]);
        vResult = _mm_add_ps(vResult,Y);
        X = _mm_mul_ps(X,M.r[0]);
        vResult = _mm_add_ps(vResult,X);
        _mm_storeu_ps(reinterpret_cast<float *>(pOutputVector),vResult);
        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT4* XMVector3TransformStreamNC
(
    XMFLOAT4*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT3* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX     M
)
{
#if defined(_XM_NO_INTRINSICS_) || defined(XM_NO_MISALIGNED_VECTOR_ACCESS) || defined(_XM_SSE_INTRINSICS_)
	return XMVector3TransformStream( pOutputStream, OutputStride, pInputStream, InputStride, VectorCount, M );
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3TransformCoord
(
    FXMVECTOR V, 
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR InverseW;
    XMVECTOR Result;

    Z = XMVectorSplatZ(V);
    Y = XMVectorSplatY(V);
    X = XMVectorSplatX(V);

    Result = XMVectorMultiplyAdd(Z, M.r[2], M.r[3]);
    Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
    Result = XMVectorMultiplyAdd(X, M.r[0], Result);

    InverseW = XMVectorSplatW(Result);
    InverseW = XMVectorReciprocal(InverseW);

    Result = XMVectorMultiply(Result, InverseW);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,0,0,0));
    vResult = _mm_mul_ps(vResult,M.r[0]);
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    vTemp = _mm_mul_ps(vTemp,M.r[1]);
    vResult = _mm_add_ps(vResult,vTemp);
    vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(2,2,2,2));
    vTemp = _mm_mul_ps(vTemp,M.r[2]);
    vResult = _mm_add_ps(vResult,vTemp);
    vResult = _mm_add_ps(vResult,M.r[3]);
    vTemp = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,3,3,3));
    vResult = _mm_div_ps(vResult,vTemp);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT3* XMVector3TransformCoordStream
(
    XMFLOAT3*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT3* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX     M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR InverseW;
    XMVECTOR Result;
    UINT     i;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat3((XMFLOAT3*)pInputVector);
        Z = XMVectorSplatZ(V);
        Y = XMVectorSplatY(V);
        X = XMVectorSplatX(V);
//        Z = XMVectorReplicate(((XMFLOAT3*)pInputVector)->z);
//        Y = XMVectorReplicate(((XMFLOAT3*)pInputVector)->y);
//        X = XMVectorReplicate(((XMFLOAT3*)pInputVector)->x);

        Result = XMVectorMultiplyAdd(Z, M.r[2], M.r[3]);
        Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
        Result = XMVectorMultiplyAdd(X, M.r[0], Result);

        InverseW = XMVectorSplatW(Result);
        InverseW = XMVectorReciprocal(InverseW);

        Result = XMVectorMultiply(Result, InverseW);

        XMStoreFloat3((XMFLOAT3*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    UINT i;
    const BYTE *pInputVector = (BYTE*)pInputStream;
    BYTE *pOutputVector = (BYTE*)pOutputStream;

    for (i = 0; i < VectorCount; i++)
    {
        XMVECTOR X = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->x);
        XMVECTOR Y = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->y);
        XMVECTOR vResult = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->z);
        vResult = _mm_mul_ps(vResult,M.r[2]);
        vResult = _mm_add_ps(vResult,M.r[3]);
        Y = _mm_mul_ps(Y,M.r[1]);
        vResult = _mm_add_ps(vResult,Y);
        X = _mm_mul_ps(X,M.r[0]);
        vResult = _mm_add_ps(vResult,X);

        X = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(3,3,3,3));
        vResult = _mm_div_ps(vResult,X);
    	_mm_store_ss(&reinterpret_cast<XMFLOAT3 *>(pOutputVector)->x,vResult);
        vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,3,2,1));
    	_mm_store_ss(&reinterpret_cast<XMFLOAT3 *>(pOutputVector)->y,vResult);
        vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,3,2,1));
	    _mm_store_ss(&reinterpret_cast<XMFLOAT3 *>(pOutputVector)->z,vResult);
        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3TransformNormal
(
    FXMVECTOR V, 
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR Result;

    Z = XMVectorSplatZ(V);
    Y = XMVectorSplatY(V);
    X = XMVectorSplatX(V);

    Result = XMVectorMultiply(Z, M.r[2]);
    Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
    Result = XMVectorMultiplyAdd(X, M.r[0], Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,0,0,0));
    vResult = _mm_mul_ps(vResult,M.r[0]);
    XMVECTOR vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    vTemp = _mm_mul_ps(vTemp,M.r[1]);
    vResult = _mm_add_ps(vResult,vTemp);
    vTemp = _mm_shuffle_ps(V,V,_MM_SHUFFLE(2,2,2,2));
    vTemp = _mm_mul_ps(vTemp,M.r[2]);
    vResult = _mm_add_ps(vResult,vTemp);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT3* XMVector3TransformNormalStream
(
    XMFLOAT3*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT3* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX     M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR Result;
    UINT     i;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat3((XMFLOAT3*)pInputVector);
        Z = XMVectorSplatZ(V);
        Y = XMVectorSplatY(V);
        X = XMVectorSplatX(V);
//        Z = XMVectorReplicate(((XMFLOAT3*)pInputVector)->z);
//        Y = XMVectorReplicate(((XMFLOAT3*)pInputVector)->y);
//        X = XMVectorReplicate(((XMFLOAT3*)pInputVector)->x);

        Result = XMVectorMultiply(Z, M.r[2]);
        Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
        Result = XMVectorMultiplyAdd(X, M.r[0], Result);

        XMStoreFloat3((XMFLOAT3*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    UINT i;
    const BYTE *pInputVector = (BYTE*)pInputStream;
    BYTE *pOutputVector = (BYTE*)pOutputStream;

    for (i = 0; i < VectorCount; i++)
    {
        XMVECTOR X = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->x);
        XMVECTOR Y = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->y);
        XMVECTOR vResult = _mm_load_ps1(&reinterpret_cast<const XMFLOAT3 *>(pInputVector)->z);
        vResult = _mm_mul_ps(vResult,M.r[2]);
        Y = _mm_mul_ps(Y,M.r[1]);
        vResult = _mm_add_ps(vResult,Y);
        X = _mm_mul_ps(X,M.r[0]);
        vResult = _mm_add_ps(vResult,X);
    	_mm_store_ss(&reinterpret_cast<XMFLOAT3 *>(pOutputVector)->x,vResult);
        vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,3,2,1));
    	_mm_store_ss(&reinterpret_cast<XMFLOAT3 *>(pOutputVector)->y,vResult);
        vResult = _mm_shuffle_ps(vResult,vResult,_MM_SHUFFLE(0,3,2,1));
	    _mm_store_ss(&reinterpret_cast<XMFLOAT3 *>(pOutputVector)->z,vResult);
        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMVECTOR XMVector3Project
(
    FXMVECTOR V, 
    FLOAT    ViewportX, 
    FLOAT    ViewportY, 
    FLOAT    ViewportWidth, 
    FLOAT    ViewportHeight, 
    FLOAT    ViewportMinZ, 
    FLOAT    ViewportMaxZ, 
    CXMMATRIX Projection, 
    CXMMATRIX View, 
    CXMMATRIX World
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX Transform;
    XMVECTOR Scale;
    XMVECTOR Offset;
    XMVECTOR Result;
    FLOAT    HalfViewportWidth = ViewportWidth * 0.5f;
    FLOAT    HalfViewportHeight = ViewportHeight * 0.5f;

    Scale = XMVectorSet(HalfViewportWidth, 
                        -HalfViewportHeight,
                        ViewportMaxZ - ViewportMinZ,
                        0.0f);

    Offset = XMVectorSet(ViewportX + HalfViewportWidth,
                        ViewportY + HalfViewportHeight,
                        ViewportMinZ,
                        0.0f);

    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);

    Result = XMVector3TransformCoord(V, Transform);

    Result = XMVectorMultiplyAdd(Result, Scale, Offset);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX Transform;
    XMVECTOR Scale;
    XMVECTOR Offset;
    XMVECTOR Result;
    FLOAT    HalfViewportWidth = ViewportWidth * 0.5f;
    FLOAT    HalfViewportHeight = ViewportHeight * 0.5f;

    Scale = XMVectorSet(HalfViewportWidth, 
                        -HalfViewportHeight,
                        ViewportMaxZ - ViewportMinZ,
                        0.0f);

    Offset = XMVectorSet(ViewportX + HalfViewportWidth,
                        ViewportY + HalfViewportHeight,
                        ViewportMinZ,
                        0.0f);
    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);
    Result = XMVector3TransformCoord(V, Transform);
    Result = _mm_mul_ps(Result,Scale);
    Result = _mm_add_ps(Result,Offset);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT3* XMVector3ProjectStream
(
    XMFLOAT3*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT3* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    FLOAT           ViewportX, 
    FLOAT           ViewportY, 
    FLOAT           ViewportWidth, 
    FLOAT           ViewportHeight, 
    FLOAT           ViewportMinZ, 
    FLOAT           ViewportMaxZ, 
    CXMMATRIX     Projection, 
    CXMMATRIX     View, 
    CXMMATRIX     World
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX Transform;
    XMVECTOR V;
    XMVECTOR Scale;
    XMVECTOR Offset;
    XMVECTOR Result;
    UINT     i;
    FLOAT    HalfViewportWidth = ViewportWidth * 0.5f;
    FLOAT    HalfViewportHeight = ViewportHeight * 0.5f;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    Scale = XMVectorSet(HalfViewportWidth, 
                        -HalfViewportHeight,
                        ViewportMaxZ - ViewportMinZ,
                        1.0f);

    Offset = XMVectorSet(ViewportX + HalfViewportWidth,
                        ViewportY + HalfViewportHeight,
                        ViewportMinZ,
                        0.0f);

    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat3((XMFLOAT3*)pInputVector);

        Result = XMVector3TransformCoord(V, Transform);

        Result = XMVectorMultiplyAdd(Result, Scale, Offset);

        XMStoreFloat3((XMFLOAT3*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
	XMASSERT(pOutputStream);
    XMASSERT(pInputStream);
    XMMATRIX Transform;
    XMVECTOR V;
    XMVECTOR Scale;
    XMVECTOR Offset;
    XMVECTOR Result;
    UINT     i;
    FLOAT    HalfViewportWidth = ViewportWidth * 0.5f;
    FLOAT    HalfViewportHeight = ViewportHeight * 0.5f;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    Scale = XMVectorSet(HalfViewportWidth, 
                        -HalfViewportHeight,
                        ViewportMaxZ - ViewportMinZ,
                        1.0f);

    Offset = XMVectorSet(ViewportX + HalfViewportWidth,
                        ViewportY + HalfViewportHeight,
                        ViewportMinZ,
                        0.0f);

    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat3((XMFLOAT3*)pInputVector);

        Result = XMVector3TransformCoord(V, Transform);

        Result = _mm_mul_ps(Result,Scale);
        Result = _mm_add_ps(Result,Offset);
        XMStoreFloat3((XMFLOAT3*)pOutputVector, Result);
        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }
    return pOutputStream;

#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector3Unproject
(
    FXMVECTOR V, 
    FLOAT    ViewportX, 
    FLOAT    ViewportY, 
    FLOAT    ViewportWidth, 
    FLOAT    ViewportHeight, 
    FLOAT    ViewportMinZ, 
    FLOAT    ViewportMaxZ, 
    CXMMATRIX Projection, 
    CXMMATRIX View, 
    CXMMATRIX World
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX        Transform;
    XMVECTOR        Scale;
    XMVECTOR        Offset;
    XMVECTOR        Determinant;
    XMVECTOR        Result;
    CONST XMVECTOR  D = XMVectorSet(-1.0f, 1.0f, 0.0f, 0.0f);

    Scale = XMVectorSet(ViewportWidth * 0.5f,
                        -ViewportHeight * 0.5f,
                        ViewportMaxZ - ViewportMinZ,
                        1.0f);
    Scale = XMVectorReciprocal(Scale);

    Offset = XMVectorSet(-ViewportX,
                        -ViewportY,
                        -ViewportMinZ,
                        0.0f);
    Offset = XMVectorMultiplyAdd(Scale, Offset, D);

    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);
    Transform = XMMatrixInverse(&Determinant, Transform);

    Result = XMVectorMultiplyAdd(V, Scale, Offset);

    Result = XMVector3TransformCoord(Result, Transform);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMMATRIX        Transform;
    XMVECTOR        Scale;
    XMVECTOR        Offset;
    XMVECTOR        Determinant;
    XMVECTOR        Result;
    CONST XMVECTORF32  D = {-1.0f, 1.0f, 0.0f, 0.0f};

    Scale = XMVectorSet(ViewportWidth * 0.5f,
                        -ViewportHeight * 0.5f,
                        ViewportMaxZ - ViewportMinZ,
                        1.0f);
    Scale = XMVectorReciprocal(Scale);

    Offset = XMVectorSet(-ViewportX,
                        -ViewportY,
                        -ViewportMinZ,
                        0.0f);
    Offset = _mm_mul_ps(Offset,Scale);
    Offset = _mm_add_ps(Offset,D);

    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);
    Transform = XMMatrixInverse(&Determinant, Transform);

    Result = _mm_mul_ps(V,Scale);
    Result = _mm_add_ps(Result,Offset);

    Result = XMVector3TransformCoord(Result, Transform);

    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT3* XMVector3UnprojectStream
(
    XMFLOAT3*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT3* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    FLOAT           ViewportX, 
    FLOAT           ViewportY, 
    FLOAT           ViewportWidth, 
    FLOAT           ViewportHeight, 
    FLOAT           ViewportMinZ, 
    FLOAT           ViewportMaxZ, 
    CXMMATRIX     Projection, 
    CXMMATRIX     View, 
    CXMMATRIX     World)
{
#if defined(_XM_NO_INTRINSICS_)

    XMMATRIX        Transform;
    XMVECTOR        Scale;
    XMVECTOR        Offset;
    XMVECTOR        V;
    XMVECTOR        Determinant;
    XMVECTOR        Result;
    UINT            i;
    BYTE*           pInputVector = (BYTE*)pInputStream;
    BYTE*           pOutputVector = (BYTE*)pOutputStream;
    CONST XMVECTOR  D = XMVectorSet(-1.0f, 1.0f, 0.0f, 0.0f);

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    Scale = XMVectorSet(ViewportWidth * 0.5f,
                        -ViewportHeight * 0.5f,
                        ViewportMaxZ - ViewportMinZ,
                        1.0f);
    Scale = XMVectorReciprocal(Scale);

    Offset = XMVectorSet(-ViewportX,
                        -ViewportY,
                        -ViewportMinZ,
                        0.0f);
    Offset = XMVectorMultiplyAdd(Scale, Offset, D);

    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);
    Transform = XMMatrixInverse(&Determinant, Transform);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat3((XMFLOAT3*)pInputVector);

        Result = XMVectorMultiplyAdd(V, Scale, Offset);

        Result = XMVector3TransformCoord(Result, Transform);

        XMStoreFloat3((XMFLOAT3*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);
    XMMATRIX        Transform;
    XMVECTOR        Scale;
    XMVECTOR        Offset;
    XMVECTOR        V;
    XMVECTOR        Determinant;
    XMVECTOR        Result;
    UINT            i;
    BYTE*           pInputVector = (BYTE*)pInputStream;
    BYTE*           pOutputVector = (BYTE*)pOutputStream;
    CONST XMVECTORF32  D = {-1.0f, 1.0f, 0.0f, 0.0f};

    Scale = XMVectorSet(ViewportWidth * 0.5f,
                        -ViewportHeight * 0.5f,
                        ViewportMaxZ - ViewportMinZ,
                        1.0f);
    Scale = XMVectorReciprocal(Scale);

    Offset = XMVectorSet(-ViewportX,
                        -ViewportY,
                        -ViewportMinZ,
                        0.0f);
    Offset = _mm_mul_ps(Offset,Scale);
    Offset = _mm_add_ps(Offset,D);

    Transform = XMMatrixMultiply(World, View);
    Transform = XMMatrixMultiply(Transform, Projection);
    Transform = XMMatrixInverse(&Determinant, Transform);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat3((XMFLOAT3*)pInputVector);

        Result = XMVectorMultiplyAdd(V, Scale, Offset);

        Result = XMVector3TransformCoord(Result, Transform);

        XMStoreFloat3((XMFLOAT3*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

/****************************************************************************
 *
 * 4D Vector
 *
 ****************************************************************************/

//------------------------------------------------------------------------------
// Comparison operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4Equal
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] == V2.vector4_f32[0]) && (V1.vector4_f32[1] == V2.vector4_f32[1]) && (V1.vector4_f32[2] == V2.vector4_f32[2]) && (V1.vector4_f32[3] == V2.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
    return ((_mm_movemask_ps(vTemp)==0x0f) != 0);
#else
    return XMComparisonAllTrue(XMVector4EqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector4EqualR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    UINT CR = 0;

    if ((V1.vector4_f32[0] == V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] == V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] == V2.vector4_f32[2]) &&
        (V1.vector4_f32[3] == V2.vector4_f32[3]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] != V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] != V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] != V2.vector4_f32[2]) &&
        (V1.vector4_f32[3] != V2.vector4_f32[3]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpeq_ps(V1,V2);
    int iTest = _mm_movemask_ps(vTemp);
    UINT CR = 0;
    if (iTest==0xf)     // All equal?
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (iTest==0)  // All not equal?
    {
        CR = XM_CRMASK_CR6FALSE;
    }
	return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4EqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_u32[0] == V2.vector4_u32[0]) && (V1.vector4_u32[1] == V2.vector4_u32[1]) && (V1.vector4_u32[2] == V2.vector4_u32[2]) && (V1.vector4_u32[3] == V2.vector4_u32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    return ((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])==0xf) != 0);
#else
    return XMComparisonAllTrue(XMVector4EqualIntR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector4EqualIntR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if (V1.vector4_u32[0] == V2.vector4_u32[0] && 
        V1.vector4_u32[1] == V2.vector4_u32[1] &&
        V1.vector4_u32[2] == V2.vector4_u32[2] &&
        V1.vector4_u32[3] == V2.vector4_u32[3])
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (V1.vector4_u32[0] != V2.vector4_u32[0] && 
        V1.vector4_u32[1] != V2.vector4_u32[1] &&
        V1.vector4_u32[2] != V2.vector4_u32[2] &&
        V1.vector4_u32[3] != V2.vector4_u32[3])
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    int iTest = _mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0]);
    UINT CR = 0;
    if (iTest==0xf)     // All equal?
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (iTest==0)  // All not equal?
    {
        CR = XM_CRMASK_CR6FALSE;
    }
	return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

XMFINLINE BOOL XMVector4NearEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR Epsilon
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT dx, dy, dz, dw;

    dx = fabsf(V1.vector4_f32[0]-V2.vector4_f32[0]);
    dy = fabsf(V1.vector4_f32[1]-V2.vector4_f32[1]);
    dz = fabsf(V1.vector4_f32[2]-V2.vector4_f32[2]);
    dw = fabsf(V1.vector4_f32[3]-V2.vector4_f32[3]);
    return (((dx <= Epsilon.vector4_f32[0]) &&
            (dy <= Epsilon.vector4_f32[1]) &&
            (dz <= Epsilon.vector4_f32[2]) &&
            (dw <= Epsilon.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    // Get the difference
    XMVECTOR vDelta = _mm_sub_ps(V1,V2);
    // Get the absolute value of the difference
    XMVECTOR vTemp = _mm_setzero_ps();
    vTemp = _mm_sub_ps(vTemp,vDelta);
    vTemp = _mm_max_ps(vTemp,vDelta);
    vTemp = _mm_cmple_ps(vTemp,Epsilon);
    return ((_mm_movemask_ps(vTemp)==0xf) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4NotEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] != V2.vector4_f32[0]) || (V1.vector4_f32[1] != V2.vector4_f32[1]) || (V1.vector4_f32[2] != V2.vector4_f32[2]) || (V1.vector4_f32[3] != V2.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpneq_ps(V1,V2);
    return ((_mm_movemask_ps(vTemp)) != 0);
#else
    return XMComparisonAnyFalse(XMVector4EqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4NotEqualInt
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_u32[0] != V2.vector4_u32[0]) || (V1.vector4_u32[1] != V2.vector4_u32[1]) || (V1.vector4_u32[2] != V2.vector4_u32[2]) || (V1.vector4_u32[3] != V2.vector4_u32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    __m128i vTemp = _mm_cmpeq_epi32(reinterpret_cast<const __m128i *>(&V1)[0],reinterpret_cast<const __m128i *>(&V2)[0]);
    return ((_mm_movemask_ps(reinterpret_cast<const __m128 *>(&vTemp)[0])!=0xF) != 0);
#else
    return XMComparisonAnyFalse(XMVector4EqualIntR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4Greater
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] > V2.vector4_f32[0]) && (V1.vector4_f32[1] > V2.vector4_f32[1]) && (V1.vector4_f32[2] > V2.vector4_f32[2]) && (V1.vector4_f32[3] > V2.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpgt_ps(V1,V2);
    return ((_mm_movemask_ps(vTemp)==0x0f) != 0);
#else
    return XMComparisonAllTrue(XMVector4GreaterR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector4GreaterR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if (V1.vector4_f32[0] > V2.vector4_f32[0] && 
        V1.vector4_f32[1] > V2.vector4_f32[1] &&
        V1.vector4_f32[2] > V2.vector4_f32[2] &&
        V1.vector4_f32[3] > V2.vector4_f32[3])
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (V1.vector4_f32[0] <= V2.vector4_f32[0] && 
        V1.vector4_f32[1] <= V2.vector4_f32[1] &&
        V1.vector4_f32[2] <= V2.vector4_f32[2] &&
        V1.vector4_f32[3] <= V2.vector4_f32[3])
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    UINT CR = 0;
	XMVECTOR vTemp = _mm_cmpgt_ps(V1,V2);
    int iTest = _mm_movemask_ps(vTemp);
    if (iTest==0xf) {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4GreaterOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] >= V2.vector4_f32[0]) && (V1.vector4_f32[1] >= V2.vector4_f32[1]) && (V1.vector4_f32[2] >= V2.vector4_f32[2]) && (V1.vector4_f32[3] >= V2.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmpge_ps(V1,V2);
    return ((_mm_movemask_ps(vTemp)==0x0f) != 0);
#else
    return XMComparisonAllTrue(XMVector4GreaterOrEqualR(V1, V2));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector4GreaterOrEqualR
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    UINT CR = 0;
    if ((V1.vector4_f32[0] >= V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] >= V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] >= V2.vector4_f32[2]) &&
        (V1.vector4_f32[3] >= V2.vector4_f32[3]))
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if ((V1.vector4_f32[0] < V2.vector4_f32[0]) && 
        (V1.vector4_f32[1] < V2.vector4_f32[1]) &&
        (V1.vector4_f32[2] < V2.vector4_f32[2]) &&
        (V1.vector4_f32[3] < V2.vector4_f32[3]))
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    UINT CR = 0;
	XMVECTOR vTemp = _mm_cmpge_ps(V1,V2);
	int iTest = _mm_movemask_ps(vTemp);
    if (iTest==0x0f)
    {
        CR = XM_CRMASK_CR6TRUE;
    }
    else if (!iTest)
    {
        CR = XM_CRMASK_CR6FALSE;
    }
    return CR;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4Less
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] < V2.vector4_f32[0]) && (V1.vector4_f32[1] < V2.vector4_f32[1]) && (V1.vector4_f32[2] < V2.vector4_f32[2]) && (V1.vector4_f32[3] < V2.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmplt_ps(V1,V2);
    return ((_mm_movemask_ps(vTemp)==0x0f) != 0);
#else
    return XMComparisonAllTrue(XMVector4GreaterR(V2, V1));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4LessOrEqual
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V1.vector4_f32[0] <= V2.vector4_f32[0]) && (V1.vector4_f32[1] <= V2.vector4_f32[1]) && (V1.vector4_f32[2] <= V2.vector4_f32[2]) && (V1.vector4_f32[3] <= V2.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp = _mm_cmple_ps(V1,V2);
    return ((_mm_movemask_ps(vTemp)==0x0f) != 0);
#else
    return XMComparisonAllTrue(XMVector4GreaterOrEqualR(V2, V1));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4InBounds
(
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (((V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) && 
        (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1]) &&
        (V.vector4_f32[2] <= Bounds.vector4_f32[2] && V.vector4_f32[2] >= -Bounds.vector4_f32[2]) &&
        (V.vector4_f32[3] <= Bounds.vector4_f32[3] && V.vector4_f32[3] >= -Bounds.vector4_f32[3])) != 0);
#elif defined(_XM_SSE_INTRINSICS_)
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    // All in bounds?
    return ((_mm_movemask_ps(vTemp1)==0x0f) != 0);
#else
    return XMComparisonAllInBounds(XMVector4InBoundsR(V, Bounds));
#endif
}

//------------------------------------------------------------------------------

XMFINLINE UINT XMVector4InBoundsR
(
    FXMVECTOR V, 
    FXMVECTOR Bounds
)
{
#if defined(_XM_NO_INTRINSICS_)

    UINT CR = 0;
    if ((V.vector4_f32[0] <= Bounds.vector4_f32[0] && V.vector4_f32[0] >= -Bounds.vector4_f32[0]) && 
        (V.vector4_f32[1] <= Bounds.vector4_f32[1] && V.vector4_f32[1] >= -Bounds.vector4_f32[1]) &&
        (V.vector4_f32[2] <= Bounds.vector4_f32[2] && V.vector4_f32[2] >= -Bounds.vector4_f32[2]) &&
        (V.vector4_f32[3] <= Bounds.vector4_f32[3] && V.vector4_f32[3] >= -Bounds.vector4_f32[3]))
    {
        CR = XM_CRMASK_CR6BOUNDS;
    }
    return CR;

#elif defined(_XM_SSE_INTRINSICS_)
    // Test if less than or equal
    XMVECTOR vTemp1 = _mm_cmple_ps(V,Bounds);
    // Negate the bounds
    XMVECTOR vTemp2 = _mm_mul_ps(Bounds,g_XMNegativeOne);
    // Test if greater or equal (Reversed)
    vTemp2 = _mm_cmple_ps(vTemp2,V);
    // Blend answers
    vTemp1 = _mm_and_ps(vTemp1,vTemp2);
    // All in bounds?
    return (_mm_movemask_ps(vTemp1)==0x0f) ? XM_CRMASK_CR6BOUNDS : 0;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4IsNaN
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    return (XMISNAN(V.vector4_f32[0]) ||
            XMISNAN(V.vector4_f32[1]) ||
            XMISNAN(V.vector4_f32[2]) ||
            XMISNAN(V.vector4_f32[3]));
#elif defined(_XM_SSE_INTRINSICS_)
    // Test against itself. NaN is always not equal
    XMVECTOR vTempNan = _mm_cmpneq_ps(V,V);
    // If any are NaN, the mask is non-zero
    return (_mm_movemask_ps(vTempNan)!=0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE BOOL XMVector4IsInfinite
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    return (XMISINF(V.vector4_f32[0]) ||
            XMISINF(V.vector4_f32[1]) ||
            XMISINF(V.vector4_f32[2]) ||
            XMISINF(V.vector4_f32[3]));

#elif defined(_XM_SSE_INTRINSICS_)
    // Mask off the sign bit
    XMVECTOR vTemp = _mm_and_ps(V,g_XMAbsMask);
    // Compare to infinity
    vTemp = _mm_cmpeq_ps(vTemp,g_XMInfinity);
    // If any are infinity, the signs are true.
    return (_mm_movemask_ps(vTemp) != 0);
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// Computation operations
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Dot
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result.vector4_f32[0] =
    Result.vector4_f32[1] =
    Result.vector4_f32[2] =
    Result.vector4_f32[3] = V1.vector4_f32[0] * V2.vector4_f32[0] + V1.vector4_f32[1] * V2.vector4_f32[1] + V1.vector4_f32[2] * V2.vector4_f32[2] + V1.vector4_f32[3] * V2.vector4_f32[3];

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vTemp2 = V2;
    XMVECTOR vTemp = _mm_mul_ps(V1,vTemp2);
    vTemp2 = _mm_shuffle_ps(vTemp2,vTemp,_MM_SHUFFLE(1,0,0,0)); // Copy X to the Z position and Y to the W position
    vTemp2 = _mm_add_ps(vTemp2,vTemp);          // Add Z = X+Z; W = Y+W;
    vTemp = _mm_shuffle_ps(vTemp,vTemp2,_MM_SHUFFLE(0,3,0,0));  // Copy W to the Z position
    vTemp = _mm_add_ps(vTemp,vTemp2);           // Add Z and W together
    return _mm_shuffle_ps(vTemp,vTemp,_MM_SHUFFLE(2,2,2,2));    // Splat Z and return
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Cross
(
    FXMVECTOR V1, 
    FXMVECTOR V2, 
    FXMVECTOR V3
)
{
#if defined(_XM_NO_INTRINSICS_)
    XMVECTOR Result;   

    Result.vector4_f32[0] = (((V2.vector4_f32[2]*V3.vector4_f32[3])-(V2.vector4_f32[3]*V3.vector4_f32[2]))*V1.vector4_f32[1])-(((V2.vector4_f32[1]*V3.vector4_f32[3])-(V2.vector4_f32[3]*V3.vector4_f32[1]))*V1.vector4_f32[2])+(((V2.vector4_f32[1]*V3.vector4_f32[2])-(V2.vector4_f32[2]*V3.vector4_f32[1]))*V1.vector4_f32[3]);
    Result.vector4_f32[1] = (((V2.vector4_f32[3]*V3.vector4_f32[2])-(V2.vector4_f32[2]*V3.vector4_f32[3]))*V1.vector4_f32[0])-(((V2.vector4_f32[3]*V3.vector4_f32[0])-(V2.vector4_f32[0]*V3.vector4_f32[3]))*V1.vector4_f32[2])+(((V2.vector4_f32[2]*V3.vector4_f32[0])-(V2.vector4_f32[0]*V3.vector4_f32[2]))*V1.vector4_f32[3]);
    Result.vector4_f32[2] = (((V2.vector4_f32[1]*V3.vector4_f32[3])-(V2.vector4_f32[3]*V3.vector4_f32[1]))*V1.vector4_f32[0])-(((V2.vector4_f32[0]*V3.vector4_f32[3])-(V2.vector4_f32[3]*V3.vector4_f32[0]))*V1.vector4_f32[1])+(((V2.vector4_f32[0]*V3.vector4_f32[1])-(V2.vector4_f32[1]*V3.vector4_f32[0]))*V1.vector4_f32[3]);
    Result.vector4_f32[3] = (((V2.vector4_f32[2]*V3.vector4_f32[1])-(V2.vector4_f32[1]*V3.vector4_f32[2]))*V1.vector4_f32[0])-(((V2.vector4_f32[2]*V3.vector4_f32[0])-(V2.vector4_f32[0]*V3.vector4_f32[2]))*V1.vector4_f32[1])+(((V2.vector4_f32[1]*V3.vector4_f32[0])-(V2.vector4_f32[0]*V3.vector4_f32[1]))*V1.vector4_f32[2]);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // V2zwyz * V3wzwy
    XMVECTOR vResult = _mm_shuffle_ps(V2,V2,_MM_SHUFFLE(2,1,3,2));
    XMVECTOR vTemp3 = _mm_shuffle_ps(V3,V3,_MM_SHUFFLE(1,3,2,3));
    vResult = _mm_mul_ps(vResult,vTemp3);
    // - V2wzwy * V3zwyz
    XMVECTOR vTemp2 = _mm_shuffle_ps(V2,V2,_MM_SHUFFLE(1,3,2,3));
    vTemp3 = _mm_shuffle_ps(vTemp3,vTemp3,_MM_SHUFFLE(1,3,0,1));
    vTemp2 = _mm_mul_ps(vTemp2,vTemp3);
    vResult = _mm_sub_ps(vResult,vTemp2);
    // term1 * V1yxxx
    XMVECTOR vTemp1 = _mm_shuffle_ps(V1,V1,_MM_SHUFFLE(0,0,0,1));
    vResult = _mm_mul_ps(vResult,vTemp1);

    // V2ywxz * V3wxwx
    vTemp2 = _mm_shuffle_ps(V2,V2,_MM_SHUFFLE(2,0,3,1));
    vTemp3 = _mm_shuffle_ps(V3,V3,_MM_SHUFFLE(0,3,0,3));
    vTemp3 = _mm_mul_ps(vTemp3,vTemp2);
    // - V2wxwx * V3ywxz
    vTemp2 = _mm_shuffle_ps(vTemp2,vTemp2,_MM_SHUFFLE(2,1,2,1));
    vTemp1 = _mm_shuffle_ps(V3,V3,_MM_SHUFFLE(2,0,3,1));
    vTemp2 = _mm_mul_ps(vTemp2,vTemp1);
    vTemp3 = _mm_sub_ps(vTemp3,vTemp2);
    // vResult - temp * V1zzyy
    vTemp1 = _mm_shuffle_ps(V1,V1,_MM_SHUFFLE(1,1,2,2));
    vTemp1 = _mm_mul_ps(vTemp1,vTemp3);
    vResult = _mm_sub_ps(vResult,vTemp1);

    // V2yzxy * V3zxyx
    vTemp2 = _mm_shuffle_ps(V2,V2,_MM_SHUFFLE(1,0,2,1));
    vTemp3 = _mm_shuffle_ps(V3,V3,_MM_SHUFFLE(0,1,0,2));
    vTemp3 = _mm_mul_ps(vTemp3,vTemp2);
    // - V2zxyx * V3yzxy
    vTemp2 = _mm_shuffle_ps(vTemp2,vTemp2,_MM_SHUFFLE(2,0,2,1));
    vTemp1 = _mm_shuffle_ps(V3,V3,_MM_SHUFFLE(1,0,2,1));
    vTemp1 = _mm_mul_ps(vTemp1,vTemp2);
    vTemp3 = _mm_sub_ps(vTemp3,vTemp1);
    // vResult + term * V1wwwz
    vTemp1 = _mm_shuffle_ps(V1,V1,_MM_SHUFFLE(2,3,3,3));
    vTemp3 = _mm_mul_ps(vTemp3,vTemp1);
    vResult = _mm_add_ps(vResult,vTemp3);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4LengthSq
(
    FXMVECTOR V
)
{
    return XMVector4Dot(V, V);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4ReciprocalLengthEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector4LengthSq(V);
    Result = XMVectorReciprocalSqrtEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y,z and w
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and w
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(3,2,3,2));
    // x+z, y+w
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // x+z,x+z,x+z,y+w
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,0,0,0));
    // ??,??,y+w,y+w
    vTemp = _mm_shuffle_ps(vTemp,vLengthSq,_MM_SHUFFLE(3,3,0,0));
    // ??,??,x+z+y+w,??
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // Splat the length
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,2,2,2));
    // Get the reciprocal
    vLengthSq = _mm_rsqrt_ps(vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4ReciprocalLength
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector4LengthSq(V);
    Result = XMVectorReciprocalSqrt(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y,z and w
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and w
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(3,2,3,2));
    // x+z, y+w
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // x+z,x+z,x+z,y+w
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,0,0,0));
    // ??,??,y+w,y+w
    vTemp = _mm_shuffle_ps(vTemp,vLengthSq,_MM_SHUFFLE(3,3,0,0));
    // ??,??,x+z+y+w,??
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // Splat the length
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,2,2,2));
    // Get the reciprocal
    vLengthSq = _mm_sqrt_ps(vLengthSq);
    // Accurate!
    vLengthSq = _mm_div_ps(g_XMOne,vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4LengthEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;

    Result = XMVector4LengthSq(V);
    Result = XMVectorSqrtEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y,z and w
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and w
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(3,2,3,2));
    // x+z, y+w
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // x+z,x+z,x+z,y+w
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,0,0,0));
    // ??,??,y+w,y+w
    vTemp = _mm_shuffle_ps(vTemp,vLengthSq,_MM_SHUFFLE(3,3,0,0));
    // ??,??,x+z+y+w,??
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // Splat the length
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,2,2,2));
    // Prepare for the division
    vLengthSq = _mm_sqrt_ps(vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Length
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_) 

    XMVECTOR Result;

    Result = XMVector4LengthSq(V);
    Result = XMVectorSqrt(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y,z and w
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and w
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(3,2,3,2));
    // x+z, y+w
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // x+z,x+z,x+z,y+w
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,0,0,0));
    // ??,??,y+w,y+w
    vTemp = _mm_shuffle_ps(vTemp,vLengthSq,_MM_SHUFFLE(3,3,0,0));
    // ??,??,x+z+y+w,??
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // Splat the length
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,2,2,2));
    // Prepare for the division
    vLengthSq = _mm_sqrt_ps(vLengthSq);
    return vLengthSq;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------
// XMVector4NormalizeEst uses a reciprocal estimate and
// returns QNaN on zero and infinite vectors.

XMFINLINE XMVECTOR XMVector4NormalizeEst
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result = XMVector4ReciprocalLength(V);
    Result = XMVectorMultiply(V, Result);
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y,z and w
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and w
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(3,2,3,2));
    // x+z, y+w
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // x+z,x+z,x+z,y+w
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,0,0,0));
    // ??,??,y+w,y+w
    vTemp = _mm_shuffle_ps(vTemp,vLengthSq,_MM_SHUFFLE(3,3,0,0));
    // ??,??,x+z+y+w,??
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // Splat the length
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,2,2,2));
    // Get the reciprocal
    XMVECTOR vResult = _mm_rsqrt_ps(vLengthSq);
    // Reciprocal mul to perform the normalization
    vResult = _mm_mul_ps(vResult,V);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Normalize
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fLength;
    XMVECTOR vResult;

    vResult = XMVector4Length( V );
    fLength = vResult.vector4_f32[0];

    // Prevent divide by zero
    if (fLength > 0) {
        fLength = 1.0f/fLength;
    }
    
    vResult.vector4_f32[0] = V.vector4_f32[0]*fLength;
    vResult.vector4_f32[1] = V.vector4_f32[1]*fLength;
    vResult.vector4_f32[2] = V.vector4_f32[2]*fLength;
    vResult.vector4_f32[3] = V.vector4_f32[3]*fLength;
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    // Perform the dot product on x,y,z and w
    XMVECTOR vLengthSq = _mm_mul_ps(V,V);
    // vTemp has z and w
    XMVECTOR vTemp = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(3,2,3,2));
    // x+z, y+w
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // x+z,x+z,x+z,y+w
    vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(1,0,0,0));
    // ??,??,y+w,y+w
    vTemp = _mm_shuffle_ps(vTemp,vLengthSq,_MM_SHUFFLE(3,3,0,0));
    // ??,??,x+z+y+w,??
    vLengthSq = _mm_add_ps(vLengthSq,vTemp);
    // Splat the length
	vLengthSq = _mm_shuffle_ps(vLengthSq,vLengthSq,_MM_SHUFFLE(2,2,2,2));
    // Prepare for the division
    XMVECTOR vResult = _mm_sqrt_ps(vLengthSq);
    // Create zero with a single instruction
    XMVECTOR vZeroMask = _mm_setzero_ps();
    // Test for a divide by zero (Must be FP to detect -0.0)
    vZeroMask = _mm_cmpneq_ps(vZeroMask,vResult);
    // Failsafe on zero (Or epsilon) length planes
    // If the length is infinity, set the elements to zero
    vLengthSq = _mm_cmpneq_ps(vLengthSq,g_XMInfinity);
    // Divide to perform the normalization
    vResult = _mm_div_ps(V,vResult);
    // Any that are infinity, set to zero
    vResult = _mm_and_ps(vResult,vZeroMask);
    // Select qnan or result based on infinite length
	XMVECTOR vTemp1 = _mm_andnot_ps(vLengthSq,g_XMQNaN);
    XMVECTOR vTemp2 = _mm_and_ps(vResult,vLengthSq);
    vResult = _mm_or_ps(vTemp1,vTemp2);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4ClampLength
(
    FXMVECTOR V, 
    FLOAT    LengthMin, 
    FLOAT    LengthMax
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR ClampMax;
    XMVECTOR ClampMin;

    ClampMax = XMVectorReplicate(LengthMax);
    ClampMin = XMVectorReplicate(LengthMin);

    return XMVector4ClampLengthV(V, ClampMin, ClampMax);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR ClampMax = _mm_set_ps1(LengthMax);
    XMVECTOR ClampMin = _mm_set_ps1(LengthMin);
    return XMVector4ClampLengthV(V, ClampMin, ClampMax);
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4ClampLengthV
(
    FXMVECTOR V, 
    FXMVECTOR LengthMin, 
    FXMVECTOR LengthMax
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR ClampLength;
    XMVECTOR LengthSq;
    XMVECTOR RcpLength;
    XMVECTOR Length;
    XMVECTOR Normal;
    XMVECTOR Zero;
    XMVECTOR InfiniteLength;
    XMVECTOR ZeroLength;
    XMVECTOR Select;
    XMVECTOR ControlMax;
    XMVECTOR ControlMin;
    XMVECTOR Control;
    XMVECTOR Result;

    XMASSERT((LengthMin.vector4_f32[1] == LengthMin.vector4_f32[0]) && (LengthMin.vector4_f32[2] == LengthMin.vector4_f32[0]) && (LengthMin.vector4_f32[3] == LengthMin.vector4_f32[0]));
    XMASSERT((LengthMax.vector4_f32[1] == LengthMax.vector4_f32[0]) && (LengthMax.vector4_f32[2] == LengthMax.vector4_f32[0]) && (LengthMax.vector4_f32[3] == LengthMax.vector4_f32[0]));
    XMASSERT(XMVector4GreaterOrEqual(LengthMin, XMVectorZero()));
    XMASSERT(XMVector4GreaterOrEqual(LengthMax, XMVectorZero()));
    XMASSERT(XMVector4GreaterOrEqual(LengthMax, LengthMin));

    LengthSq = XMVector4LengthSq(V);

    Zero = XMVectorZero();

    RcpLength = XMVectorReciprocalSqrt(LengthSq);

    InfiniteLength = XMVectorEqualInt(LengthSq, g_XMInfinity.v);
    ZeroLength = XMVectorEqual(LengthSq, Zero);

    Normal = XMVectorMultiply(V, RcpLength);

    Length = XMVectorMultiply(LengthSq, RcpLength);

    Select = XMVectorEqualInt(InfiniteLength, ZeroLength);
    Length = XMVectorSelect(LengthSq, Length, Select);
    Normal = XMVectorSelect(LengthSq, Normal, Select);

    ControlMax = XMVectorGreater(Length, LengthMax);
    ControlMin = XMVectorLess(Length, LengthMin);

    ClampLength = XMVectorSelect(Length, LengthMax, ControlMax);
    ClampLength = XMVectorSelect(ClampLength, LengthMin, ControlMin);

    Result = XMVectorMultiply(Normal, ClampLength);

    // Preserve the original vector (with no precision loss) if the length falls within the given range
    Control = XMVectorEqualInt(ControlMax, ControlMin);
    Result = XMVectorSelect(Result, V, Control);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR ClampLength;
    XMVECTOR LengthSq;
    XMVECTOR RcpLength;
    XMVECTOR Length;
    XMVECTOR Normal;
    XMVECTOR Zero;
    XMVECTOR InfiniteLength;
    XMVECTOR ZeroLength;
    XMVECTOR Select;
    XMVECTOR ControlMax;
    XMVECTOR ControlMin;
    XMVECTOR Control;
    XMVECTOR Result;

    XMASSERT((XMVectorGetY(LengthMin) == XMVectorGetX(LengthMin)) && (XMVectorGetZ(LengthMin) == XMVectorGetX(LengthMin)) && (XMVectorGetW(LengthMin) == XMVectorGetX(LengthMin)));
    XMASSERT((XMVectorGetY(LengthMax) == XMVectorGetX(LengthMax)) && (XMVectorGetZ(LengthMax) == XMVectorGetX(LengthMax)) && (XMVectorGetW(LengthMax) == XMVectorGetX(LengthMax)));
    XMASSERT(XMVector4GreaterOrEqual(LengthMin, g_XMZero));
    XMASSERT(XMVector4GreaterOrEqual(LengthMax, g_XMZero));
    XMASSERT(XMVector4GreaterOrEqual(LengthMax, LengthMin));

    LengthSq = XMVector4LengthSq(V);
    Zero = XMVectorZero();
    RcpLength = XMVectorReciprocalSqrt(LengthSq);
    InfiniteLength = XMVectorEqualInt(LengthSq, g_XMInfinity);
    ZeroLength = XMVectorEqual(LengthSq, Zero);
    Normal = _mm_mul_ps(V, RcpLength);
    Length = _mm_mul_ps(LengthSq, RcpLength);
    Select = XMVectorEqualInt(InfiniteLength, ZeroLength);
    Length = XMVectorSelect(LengthSq, Length, Select);
    Normal = XMVectorSelect(LengthSq, Normal, Select);
    ControlMax = XMVectorGreater(Length, LengthMax);
    ControlMin = XMVectorLess(Length, LengthMin);
    ClampLength = XMVectorSelect(Length, LengthMax, ControlMax);
    ClampLength = XMVectorSelect(ClampLength, LengthMin, ControlMin);
    Result = _mm_mul_ps(Normal, ClampLength);
    // Preserve the original vector (with no precision loss) if the length falls within the given range
    Control = XMVectorEqualInt(ControlMax,ControlMin);
    Result = XMVectorSelect(Result,V,Control);
    return Result;

#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Reflect
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal
)
{
#if defined(_XM_NO_INTRINSICS_) 

    XMVECTOR Result;

    // Result = Incident - (2 * dot(Incident, Normal)) * Normal
    Result = XMVector4Dot(Incident, Normal);
    Result = XMVectorAdd(Result, Result);
    Result = XMVectorNegativeMultiplySubtract(Result, Normal, Incident);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    // Result = Incident - (2 * dot(Incident, Normal)) * Normal
    XMVECTOR Result = XMVector4Dot(Incident,Normal);
    Result = _mm_add_ps(Result,Result);
    Result = _mm_mul_ps(Result,Normal);
    Result = _mm_sub_ps(Incident,Result);
    return Result;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Refract
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal, 
    FLOAT    RefractionIndex
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Index;
    Index = XMVectorReplicate(RefractionIndex);
    return XMVector4RefractV(Incident, Normal, Index);

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR Index = _mm_set_ps1(RefractionIndex);
    return XMVector4RefractV(Incident,Normal,Index);
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4RefractV
(
    FXMVECTOR Incident, 
    FXMVECTOR Normal, 
    FXMVECTOR RefractionIndex
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR        IDotN;
    XMVECTOR        R;
    CONST XMVECTOR  Zero = XMVectorZero();

    // Result = RefractionIndex * Incident - Normal * (RefractionIndex * dot(Incident, Normal) + 
    // sqrt(1 - RefractionIndex * RefractionIndex * (1 - dot(Incident, Normal) * dot(Incident, Normal))))

    IDotN = XMVector4Dot(Incident, Normal);

    // R = 1.0f - RefractionIndex * RefractionIndex * (1.0f - IDotN * IDotN)
    R = XMVectorNegativeMultiplySubtract(IDotN, IDotN, g_XMOne.v);
    R = XMVectorMultiply(R, RefractionIndex);
    R = XMVectorNegativeMultiplySubtract(R, RefractionIndex, g_XMOne.v);

    if (XMVector4LessOrEqual(R, Zero))
    {
        // Total internal reflection
        return Zero;
    }
    else
    {
        XMVECTOR Result;

        // R = RefractionIndex * IDotN + sqrt(R)
        R = XMVectorSqrt(R);
        R = XMVectorMultiplyAdd(RefractionIndex, IDotN, R);

        // Result = RefractionIndex * Incident - Normal * R
        Result = XMVectorMultiply(RefractionIndex, Incident);
        Result = XMVectorNegativeMultiplySubtract(Normal, R, Result);

        return Result;
    }

#elif defined(_XM_SSE_INTRINSICS_)
    // Result = RefractionIndex * Incident - Normal * (RefractionIndex * dot(Incident, Normal) + 
    // sqrt(1 - RefractionIndex * RefractionIndex * (1 - dot(Incident, Normal) * dot(Incident, Normal))))

    XMVECTOR IDotN = XMVector4Dot(Incident,Normal);

    // R = 1.0f - RefractionIndex * RefractionIndex * (1.0f - IDotN * IDotN)
    XMVECTOR R = _mm_mul_ps(IDotN,IDotN);
    R = _mm_sub_ps(g_XMOne,R);
    R = _mm_mul_ps(R, RefractionIndex);
    R = _mm_mul_ps(R, RefractionIndex);
    R = _mm_sub_ps(g_XMOne,R);

    XMVECTOR vResult = _mm_cmple_ps(R,g_XMZero);
    if (_mm_movemask_ps(vResult)==0x0f)
    {
        // Total internal reflection
        vResult = g_XMZero;
    }
    else
    {
        // R = RefractionIndex * IDotN + sqrt(R)
        R = _mm_sqrt_ps(R);
        vResult = _mm_mul_ps(RefractionIndex, IDotN);
        R = _mm_add_ps(R,vResult);
        // Result = RefractionIndex * Incident - Normal * R
        vResult = _mm_mul_ps(RefractionIndex, Incident);
        R = _mm_mul_ps(R,Normal);
        vResult = _mm_sub_ps(vResult,R);
    }
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Orthogonal
(
    FXMVECTOR V
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR Result;
    Result.vector4_f32[0] = V.vector4_f32[2];
    Result.vector4_f32[1] = V.vector4_f32[3];
    Result.vector4_f32[2] = -V.vector4_f32[0];
    Result.vector4_f32[3] = -V.vector4_f32[1];
    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    static const XMVECTORF32 FlipZW = {1.0f,1.0f,-1.0f,-1.0f};
    XMVECTOR vResult = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,0,3,2));
    vResult = _mm_mul_ps(vResult,FlipZW);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4AngleBetweenNormalsEst
(
    FXMVECTOR N1, 
    FXMVECTOR N2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR NegativeOne;
    XMVECTOR One;
    XMVECTOR Result;

    Result = XMVector4Dot(N1, N2);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    Result = XMVectorClamp(Result, NegativeOne, One);
    Result = XMVectorACosEst(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = XMVector4Dot(N1,N2);
    // Clamp to -1.0f to 1.0f
    vResult = _mm_max_ps(vResult,g_XMNegativeOne);
    vResult = _mm_min_ps(vResult,g_XMOne);;
    vResult = XMVectorACosEst(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4AngleBetweenNormals
(
    FXMVECTOR N1, 
    FXMVECTOR N2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR NegativeOne;
    XMVECTOR One;
    XMVECTOR Result;

    Result = XMVector4Dot(N1, N2);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    Result = XMVectorClamp(Result, NegativeOne, One);
    Result = XMVectorACos(Result);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR vResult = XMVector4Dot(N1,N2);
    // Clamp to -1.0f to 1.0f
    vResult = _mm_max_ps(vResult,g_XMNegativeOne);
    vResult = _mm_min_ps(vResult,g_XMOne);;
    vResult = XMVectorACos(vResult);
    return vResult;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4AngleBetweenVectors
(
    FXMVECTOR V1, 
    FXMVECTOR V2
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR L1;
    XMVECTOR L2;
    XMVECTOR Dot;
    XMVECTOR CosAngle;
    XMVECTOR NegativeOne;
    XMVECTOR One;
    XMVECTOR Result;

    L1 = XMVector4ReciprocalLength(V1);
    L2 = XMVector4ReciprocalLength(V2);

    Dot = XMVector4Dot(V1, V2);

    L1 = XMVectorMultiply(L1, L2);

    CosAngle = XMVectorMultiply(Dot, L1);
    NegativeOne = XMVectorSplatConstant(-1, 0);
    One = XMVectorSplatOne();
    CosAngle = XMVectorClamp(CosAngle, NegativeOne, One);

    Result = XMVectorACos(CosAngle);

    return Result;

#elif defined(_XM_SSE_INTRINSICS_)
    XMVECTOR L1;
    XMVECTOR L2;
    XMVECTOR Dot;
    XMVECTOR CosAngle;
    XMVECTOR Result;

    L1 = XMVector4ReciprocalLength(V1);
    L2 = XMVector4ReciprocalLength(V2);
    Dot = XMVector4Dot(V1, V2);
    L1 = _mm_mul_ps(L1,L2);
    CosAngle = _mm_mul_ps(Dot,L1);
    CosAngle = XMVectorClamp(CosAngle, g_XMNegativeOne, g_XMOne);
    Result = XMVectorACos(CosAngle);
    return Result;

#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR XMVector4Transform
(
    FXMVECTOR V, 
    CXMMATRIX M
)
{
#if defined(_XM_NO_INTRINSICS_)
    FLOAT fX = (M.m[0][0]*V.vector4_f32[0])+(M.m[1][0]*V.vector4_f32[1])+(M.m[2][0]*V.vector4_f32[2])+(M.m[3][0]*V.vector4_f32[3]);
    FLOAT fY = (M.m[0][1]*V.vector4_f32[0])+(M.m[1][1]*V.vector4_f32[1])+(M.m[2][1]*V.vector4_f32[2])+(M.m[3][1]*V.vector4_f32[3]);
    FLOAT fZ = (M.m[0][2]*V.vector4_f32[0])+(M.m[1][2]*V.vector4_f32[1])+(M.m[2][2]*V.vector4_f32[2])+(M.m[3][2]*V.vector4_f32[3]);
    FLOAT fW = (M.m[0][3]*V.vector4_f32[0])+(M.m[1][3]*V.vector4_f32[1])+(M.m[2][3]*V.vector4_f32[2])+(M.m[3][3]*V.vector4_f32[3]);
    XMVECTOR vResult = {
        fX,
        fY,
        fZ,
        fW
    };
    return vResult;

#elif defined(_XM_SSE_INTRINSICS_)
    // Splat x,y,z and w
    XMVECTOR vTempX = _mm_shuffle_ps(V,V,_MM_SHUFFLE(0,0,0,0));
    XMVECTOR vTempY = _mm_shuffle_ps(V,V,_MM_SHUFFLE(1,1,1,1));
    XMVECTOR vTempZ = _mm_shuffle_ps(V,V,_MM_SHUFFLE(2,2,2,2));
    XMVECTOR vTempW = _mm_shuffle_ps(V,V,_MM_SHUFFLE(3,3,3,3));
    // Mul by the matrix
    vTempX = _mm_mul_ps(vTempX,M.r[0]);
    vTempY = _mm_mul_ps(vTempY,M.r[1]);
    vTempZ = _mm_mul_ps(vTempZ,M.r[2]);
    vTempW = _mm_mul_ps(vTempW,M.r[3]);
    // Add them all together
    vTempX = _mm_add_ps(vTempX,vTempY);
    vTempZ = _mm_add_ps(vTempZ,vTempW);
    vTempX = _mm_add_ps(vTempX,vTempZ);
    return vTempX;
#else // _XM_VMX128_INTRINSICS_
#endif // _XM_VMX128_INTRINSICS_
}

//------------------------------------------------------------------------------

XMINLINE XMFLOAT4* XMVector4TransformStream
(
    XMFLOAT4*       pOutputStream, 
    UINT            OutputStride, 
    CONST XMFLOAT4* pInputStream, 
    UINT            InputStride, 
    UINT            VectorCount, 
    CXMMATRIX     M
)
{
#if defined(_XM_NO_INTRINSICS_)

    XMVECTOR V;
    XMVECTOR X;
    XMVECTOR Y;
    XMVECTOR Z;
    XMVECTOR W;
    XMVECTOR Result;
    UINT     i;
    BYTE*    pInputVector = (BYTE*)pInputStream;
    BYTE*    pOutputVector = (BYTE*)pOutputStream;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    for (i = 0; i < VectorCount; i++)
    {
        V = XMLoadFloat4((XMFLOAT4*)pInputVector);
        W = XMVectorSplatW(V);
        Z = XMVectorSplatZ(V);
        Y = XMVectorSplatY(V);
        X = XMVectorSplatX(V);
//        W = XMVectorReplicate(((XMFLOAT4*)pInputVector)->w);
//        Z = XMVectorReplicate(((XMFLOAT4*)pInputVector)->z);
//        Y = XMVectorReplicate(((XMFLOAT4*)pInputVector)->y);
//        X = XMVectorReplicate(((XMFLOAT4*)pInputVector)->x);

        Result = XMVectorMultiply(W, M.r[3]);
        Result = XMVectorMultiplyAdd(Z, M.r[2], Result);
        Result = XMVectorMultiplyAdd(Y, M.r[1], Result);
        Result = XMVectorMultiplyAdd(X, M.r[0], Result);

        XMStoreFloat4((XMFLOAT4*)pOutputVector, Result);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }

    return pOutputStream;

#elif defined(_XM_SSE_INTRINSICS_)
    UINT i;

    XMASSERT(pOutputStream);
    XMASSERT(pInputStream);

    const BYTE*pInputVector = reinterpret_cast<const BYTE *>(pInputStream);
    BYTE* pOutputVector = reinterpret_cast<BYTE *>(pOutputStream);
    for (i = 0; i < VectorCount; i++)
    {
        // Fetch the row and splat it
        XMVECTOR vTempx = _mm_loadu_ps(reinterpret_cast<const float *>(pInputVector));
        XMVECTOR vTempy = _mm_shuffle_ps(vTempx,vTempx,_MM_SHUFFLE(1,1,1,1));
        XMVECTOR vTempz = _mm_shuffle_ps(vTempx,vTempx,_MM_SHUFFLE(2,2,2,2));
        XMVECTOR vTempw = _mm_shuffle_ps(vTempx,vTempx,_MM_SHUFFLE(3,3,3,3));
        vTempx = _mm_shuffle_ps(vTempx,vTempx,_MM_SHUFFLE(0,0,0,0));
        vTempx = _mm_mul_ps(vTempx,M.r[0]);
        vTempy = _mm_mul_ps(vTempy,M.r[1]);
        vTempz = _mm_mul_ps(vTempz,M.r[2]);
        vTempw = _mm_mul_ps(vTempw,M.r[3]);
        vTempx = _mm_add_ps(vTempx,vTempy);
        vTempw = _mm_add_ps(vTempw,vTempz); 
        vTempw = _mm_add_ps(vTempw,vTempx);
        // Store the transformed vector
        _mm_storeu_ps(reinterpret_cast<float *>(pOutputVector),vTempw);

        pInputVector += InputStride; 
        pOutputVector += OutputStride;
    }
    return pOutputStream;
#elif defined(XM_NO_MISALIGNED_VECTOR_ACCESS)
#endif // _XM_VMX128_INTRINSICS_
}

#ifdef __cplusplus

/****************************************************************************
 *
 * XMVECTOR operators
 *
 ****************************************************************************/

#ifndef XM_NO_OPERATOR_OVERLOADS

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator+ (FXMVECTOR V)
{
    return V;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator- (FXMVECTOR V)
{
    return XMVectorNegate(V);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR& operator+=
(
    XMVECTOR&       V1,
    FXMVECTOR       V2
)
{
    V1 = XMVectorAdd(V1, V2);
    return V1;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR& operator-=
(
    XMVECTOR&       V1,
    FXMVECTOR       V2
)
{
    V1 = XMVectorSubtract(V1, V2);
    return V1;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR& operator*=
(
    XMVECTOR&       V1,
    FXMVECTOR       V2
)
{
    V1 = XMVectorMultiply(V1, V2);
    return V1;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR& operator/=
(
    XMVECTOR&       V1,
    FXMVECTOR       V2
)
{
    V1 = XMVectorDivide(V1,V2);
    return V1;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR& operator*=
(
    XMVECTOR&   V,
    CONST FLOAT S
)
{
    V = XMVectorScale(V, S);
    return V;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR& operator/=
(
    XMVECTOR&   V,
    CONST FLOAT S
)
{
    V = XMVectorScale(V, 1.0f / S);
    return V;
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator+
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
    return XMVectorAdd(V1, V2);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator-
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
    return XMVectorSubtract(V1, V2);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator*
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
    return XMVectorMultiply(V1, V2);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator/
(
    FXMVECTOR V1,
    FXMVECTOR V2
)
{
    return XMVectorDivide(V1,V2);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator*
(
    FXMVECTOR      V,
    CONST FLOAT    S
)
{
    return XMVectorScale(V, S);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator/
(
    FXMVECTOR      V,
    CONST FLOAT    S
)
{
    return XMVectorScale(V, 1.0f / S);
}

//------------------------------------------------------------------------------

XMFINLINE XMVECTOR operator*
(
    FLOAT           S,
    FXMVECTOR  	    V
)
{
    return XMVectorScale(V, S);
}

#endif // !XM_NO_OPERATOR_OVERLOADS

/****************************************************************************
 *
 * XMFLOAT2 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT2::_XMFLOAT2
(
    CONST FLOAT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT2& _XMFLOAT2::operator=
(
    CONST _XMFLOAT2& Float2
)
{
    x = Float2.x;
    y = Float2.y;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE XMFLOAT2A& XMFLOAT2A::operator=
(
    CONST XMFLOAT2A& Float2
)
{
    x = Float2.x;
    y = Float2.y;
    return *this;
}

/****************************************************************************
 *
 * XMHALF2 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMHALF2::_XMHALF2
(
    CONST HALF* pArray
)
{
    x = pArray[0];
    y = pArray[1];
}

//------------------------------------------------------------------------------

XMFINLINE _XMHALF2::_XMHALF2
(
    FLOAT _x,
    FLOAT _y
)
{
    x = XMConvertFloatToHalf(_x);
    y = XMConvertFloatToHalf(_y);
}

//------------------------------------------------------------------------------

XMFINLINE _XMHALF2::_XMHALF2
(
    CONST FLOAT* pArray
)
{
    x = XMConvertFloatToHalf(pArray[0]);
    y = XMConvertFloatToHalf(pArray[1]);
}

//------------------------------------------------------------------------------

XMFINLINE _XMHALF2& _XMHALF2::operator=
(
    CONST _XMHALF2& Half2
)
{
    x = Half2.x;
    y = Half2.y;
    return *this;
}

/****************************************************************************
 *
 * XMSHORTN2 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN2::_XMSHORTN2
(
    CONST SHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN2::_XMSHORTN2
(
    FLOAT _x,
    FLOAT _y
)
{
    XMStoreShortN2(this, XMVectorSet(_x, _y, 0.0f, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN2::_XMSHORTN2
(
    CONST FLOAT* pArray
)
{
    XMStoreShortN2(this, XMLoadFloat2((XMFLOAT2*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN2& _XMSHORTN2::operator=
(
    CONST _XMSHORTN2& ShortN2
)
{
    x = ShortN2.x;
    y = ShortN2.y;
    return *this;
}

/****************************************************************************
 *
 * XMSHORT2 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT2::_XMSHORT2
(
    CONST SHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT2::_XMSHORT2
(
    FLOAT _x,
    FLOAT _y
)
{
    XMStoreShort2(this, XMVectorSet(_x, _y, 0.0f, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT2::_XMSHORT2
(
    CONST FLOAT* pArray
)
{
    XMStoreShort2(this, XMLoadFloat2((XMFLOAT2*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT2& _XMSHORT2::operator=
(
    CONST _XMSHORT2& Short2
)
{
    x = Short2.x;
    y = Short2.y;
    return *this;
}

/****************************************************************************
 *
 * XMUSHORTN2 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN2::_XMUSHORTN2
(
    CONST USHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN2::_XMUSHORTN2
(
    FLOAT _x,
    FLOAT _y
)
{
    XMStoreUShortN2(this, XMVectorSet(_x, _y, 0.0f, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN2::_XMUSHORTN2
(
    CONST FLOAT* pArray
)
{
    XMStoreUShortN2(this, XMLoadFloat2((XMFLOAT2*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN2& _XMUSHORTN2::operator=
(
    CONST _XMUSHORTN2& UShortN2
)
{
    x = UShortN2.x;
    y = UShortN2.y;
    return *this;
}

/****************************************************************************
 *
 * XMUSHORT2 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT2::_XMUSHORT2
(
    CONST USHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT2::_XMUSHORT2
(
    FLOAT _x,
    FLOAT _y
)
{
    XMStoreUShort2(this, XMVectorSet(_x, _y, 0.0f, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT2::_XMUSHORT2
(
    CONST FLOAT* pArray
)
{
    XMStoreUShort2(this, XMLoadFloat2((XMFLOAT2*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT2& _XMUSHORT2::operator=
(
    CONST _XMUSHORT2& UShort2
)
{
    x = UShort2.x;
    y = UShort2.y;
    return *this;
}

/****************************************************************************
 *
 * XMFLOAT3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT3::_XMFLOAT3
(
    CONST FLOAT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT3& _XMFLOAT3::operator=
(
    CONST _XMFLOAT3& Float3
)
{
    x = Float3.x;
    y = Float3.y;
    z = Float3.z;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE XMFLOAT3A& XMFLOAT3A::operator=
(
    CONST XMFLOAT3A& Float3
)
{
    x = Float3.x;
    y = Float3.y;
    z = Float3.z;
    return *this;
}

/****************************************************************************
 *
 * XMHENDN3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMHENDN3::_XMHENDN3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreHenDN3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMHENDN3::_XMHENDN3
(
    CONST FLOAT* pArray
)
{
    XMStoreHenDN3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMHENDN3& _XMHENDN3::operator=
(
    CONST _XMHENDN3& HenDN3
)
{
    v = HenDN3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMHENDN3& _XMHENDN3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMHEND3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMHEND3::_XMHEND3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreHenD3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMHEND3::_XMHEND3
(
    CONST FLOAT* pArray
)
{
    XMStoreHenD3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMHEND3& _XMHEND3::operator=
(
    CONST _XMHEND3& HenD3
)
{
    v = HenD3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMHEND3& _XMHEND3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUHENDN3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUHENDN3::_XMUHENDN3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreUHenDN3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUHENDN3::_XMUHENDN3
(
    CONST FLOAT* pArray
)
{
    XMStoreUHenDN3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUHENDN3& _XMUHENDN3::operator=
(
    CONST _XMUHENDN3& UHenDN3
)
{
    v = UHenDN3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUHENDN3& _XMUHENDN3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUHEND3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUHEND3::_XMUHEND3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreUHenD3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUHEND3::_XMUHEND3
(
    CONST FLOAT* pArray
)
{
    XMStoreUHenD3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUHEND3& _XMUHEND3::operator=
(
    CONST _XMUHEND3& UHenD3
)
{
    v = UHenD3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUHEND3& _XMUHEND3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMDHENN3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMDHENN3::_XMDHENN3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreDHenN3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDHENN3::_XMDHENN3
(
    CONST FLOAT* pArray
)
{
    XMStoreDHenN3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDHENN3& _XMDHENN3::operator=
(
    CONST _XMDHENN3& DHenN3
)
{
    v = DHenN3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMDHENN3& _XMDHENN3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMDHEN3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMDHEN3::_XMDHEN3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreDHen3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDHEN3::_XMDHEN3
(
    CONST FLOAT* pArray
)
{
    XMStoreDHen3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDHEN3& _XMDHEN3::operator=
(
    CONST _XMDHEN3& DHen3
)
{
    v = DHen3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMDHEN3& _XMDHEN3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUDHENN3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUDHENN3::_XMUDHENN3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreUDHenN3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDHENN3::_XMUDHENN3
(
    CONST FLOAT* pArray
)
{
    XMStoreUDHenN3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDHENN3& _XMUDHENN3::operator=
(
    CONST _XMUDHENN3& UDHenN3
)
{
    v = UDHenN3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDHENN3& _XMUDHENN3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUDHEN3 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUDHEN3::_XMUDHEN3
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreUDHen3(this, XMVectorSet(_x, _y, _z, 0.0f));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDHEN3::_XMUDHEN3
(
    CONST FLOAT* pArray
)
{
    XMStoreUDHen3(this, XMLoadFloat3((XMFLOAT3*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDHEN3& _XMUDHEN3::operator=
(
    CONST _XMUDHEN3& UDHen3
)
{
    v = UDHen3.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDHEN3& _XMUDHEN3::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMU565 operators
 *
 ****************************************************************************/

XMFINLINE _XMU565::_XMU565
(
    CONST CHAR *pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
}

XMFINLINE _XMU565::_XMU565
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreU565(this, XMVectorSet( _x, _y, _z, 0.0f ));
}

XMFINLINE _XMU565::_XMU565
(
    CONST FLOAT *pArray
)
{
    XMStoreU565(this, XMLoadFloat3((XMFLOAT3*)pArray ));
}

XMFINLINE _XMU565& _XMU565::operator=
(
    CONST _XMU565& U565
)
{
    v = U565.v;
    return *this;
}

XMFINLINE _XMU565& _XMU565::operator=
(
    CONST USHORT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMFLOAT3PK operators
 *
 ****************************************************************************/

XMFINLINE _XMFLOAT3PK::_XMFLOAT3PK
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreFloat3PK(this, XMVectorSet( _x, _y, _z, 0.0f ));
}

XMFINLINE _XMFLOAT3PK::_XMFLOAT3PK
(
    CONST FLOAT *pArray
)
{
    XMStoreFloat3PK(this, XMLoadFloat3((XMFLOAT3*)pArray ));
}

XMFINLINE _XMFLOAT3PK& _XMFLOAT3PK::operator=
(
    CONST _XMFLOAT3PK& float3pk
)
{
    v = float3pk.v;
    return *this;
}

XMFINLINE _XMFLOAT3PK& _XMFLOAT3PK::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMFLOAT3SE operators
 *
 ****************************************************************************/

XMFINLINE _XMFLOAT3SE::_XMFLOAT3SE
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z
)
{
    XMStoreFloat3SE(this, XMVectorSet( _x, _y, _z, 0.0f ));
}

XMFINLINE _XMFLOAT3SE::_XMFLOAT3SE
(
    CONST FLOAT *pArray
)
{
    XMStoreFloat3SE(this, XMLoadFloat3((XMFLOAT3*)pArray ));
}

XMFINLINE _XMFLOAT3SE& _XMFLOAT3SE::operator=
(
    CONST _XMFLOAT3SE& float3se
)
{
    v = float3se.v;
    return *this;
}

XMFINLINE _XMFLOAT3SE& _XMFLOAT3SE::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMFLOAT4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4::_XMFLOAT4
(
    CONST FLOAT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMFLOAT4& _XMFLOAT4::operator=
(
    CONST _XMFLOAT4& Float4
)
{
    x = Float4.x;
    y = Float4.y;
    z = Float4.z;
    w = Float4.w;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE XMFLOAT4A& XMFLOAT4A::operator=
(
    CONST XMFLOAT4A& Float4
)
{
    x = Float4.x;
    y = Float4.y;
    z = Float4.z;
    w = Float4.w;
    return *this;
}

/****************************************************************************
 *
 * XMHALF4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMHALF4::_XMHALF4
(
    CONST HALF* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMHALF4::_XMHALF4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    x = XMConvertFloatToHalf(_x);
    y = XMConvertFloatToHalf(_y);
    z = XMConvertFloatToHalf(_z);
    w = XMConvertFloatToHalf(_w);
}

//------------------------------------------------------------------------------

XMFINLINE _XMHALF4::_XMHALF4
(
    CONST FLOAT* pArray
)
{
    XMConvertFloatToHalfStream(&x, sizeof(HALF), pArray, sizeof(FLOAT), 4);
}

//------------------------------------------------------------------------------

XMFINLINE _XMHALF4& _XMHALF4::operator=
(
    CONST _XMHALF4& Half4
)
{
    x = Half4.x;
    y = Half4.y;
    z = Half4.z;
    w = Half4.w;
    return *this;
}

/****************************************************************************
 *
 * XMSHORTN4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN4::_XMSHORTN4
(
    CONST SHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN4::_XMSHORTN4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreShortN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN4::_XMSHORTN4
(
    CONST FLOAT* pArray
)
{
    XMStoreShortN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORTN4& _XMSHORTN4::operator=
(
    CONST _XMSHORTN4& ShortN4
)
{
    x = ShortN4.x;
    y = ShortN4.y;
    z = ShortN4.z;
    w = ShortN4.w;
    return *this;
}

/****************************************************************************
 *
 * XMSHORT4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT4::_XMSHORT4
(
    CONST SHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT4::_XMSHORT4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreShort4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT4::_XMSHORT4
(
    CONST FLOAT* pArray
)
{
    XMStoreShort4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMSHORT4& _XMSHORT4::operator=
(
    CONST _XMSHORT4& Short4
)
{
    x = Short4.x;
    y = Short4.y;
    z = Short4.z;
    w = Short4.w;
    return *this;
}

/****************************************************************************
 *
 * XMUSHORTN4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN4::_XMUSHORTN4
(
    CONST USHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN4::_XMUSHORTN4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUShortN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN4::_XMUSHORTN4
(
    CONST FLOAT* pArray
)
{
    XMStoreUShortN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORTN4& _XMUSHORTN4::operator=
(
    CONST _XMUSHORTN4& UShortN4
)
{
    x = UShortN4.x;
    y = UShortN4.y;
    z = UShortN4.z;
    w = UShortN4.w;
    return *this;
}

/****************************************************************************
 *
 * XMUSHORT4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT4::_XMUSHORT4
(
    CONST USHORT* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT4::_XMUSHORT4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUShort4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT4::_XMUSHORT4
(
    CONST FLOAT* pArray
)
{
    XMStoreUShort4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUSHORT4& _XMUSHORT4::operator=
(
    CONST _XMUSHORT4& UShort4
)
{
    x = UShort4.x;
    y = UShort4.y;
    z = UShort4.z;
    w = UShort4.w;
    return *this;
}

/****************************************************************************
 *
 * XMXDECN4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMXDECN4::_XMXDECN4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreXDecN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXDECN4::_XMXDECN4
(
    CONST FLOAT* pArray
)
{
    XMStoreXDecN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXDECN4& _XMXDECN4::operator=
(
    CONST _XMXDECN4& XDecN4
)
{
    v = XDecN4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMXDECN4& _XMXDECN4::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMXDEC4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMXDEC4::_XMXDEC4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreXDec4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXDEC4::_XMXDEC4
(
    CONST FLOAT* pArray
)
{
    XMStoreXDec4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXDEC4& _XMXDEC4::operator=
(
    CONST _XMXDEC4& XDec4
)
{
    v = XDec4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMXDEC4& _XMXDEC4::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMDECN4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMDECN4::_XMDECN4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreDecN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDECN4::_XMDECN4
(
    CONST FLOAT* pArray
)
{
    XMStoreDecN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDECN4& _XMDECN4::operator=
(
    CONST _XMDECN4& DecN4
)
{
    v = DecN4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMDECN4& _XMDECN4::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMDEC4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMDEC4::_XMDEC4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreDec4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDEC4::_XMDEC4
(
    CONST FLOAT* pArray
)
{
    XMStoreDec4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMDEC4& _XMDEC4::operator=
(
    CONST _XMDEC4& Dec4
)
{
    v = Dec4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMDEC4& _XMDEC4::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUDECN4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUDECN4::_XMUDECN4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUDecN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDECN4::_XMUDECN4
(
    CONST FLOAT* pArray
)
{
    XMStoreUDecN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDECN4& _XMUDECN4::operator=
(
    CONST _XMUDECN4& UDecN4
)
{
    v = UDecN4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDECN4& _XMUDECN4::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUDEC4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUDEC4::_XMUDEC4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUDec4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDEC4::_XMUDEC4
(
    CONST FLOAT* pArray
)
{
    XMStoreUDec4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDEC4& _XMUDEC4::operator=
(
    CONST _XMUDEC4& UDec4
)
{
    v = UDec4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUDEC4& _XMUDEC4::operator=
(
    CONST UINT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMXICON4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMXICON4::_XMXICON4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreXIcoN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXICON4::_XMXICON4
(
    CONST FLOAT* pArray
)
{
    XMStoreXIcoN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXICON4& _XMXICON4::operator=
(
    CONST _XMXICON4& XIcoN4
)
{
    v = XIcoN4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMXICON4& _XMXICON4::operator=
(
    CONST UINT64 Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMXICO4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMXICO4::_XMXICO4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreXIco4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXICO4::_XMXICO4
(
    CONST FLOAT* pArray
)
{
    XMStoreXIco4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMXICO4& _XMXICO4::operator=
(
    CONST _XMXICO4& XIco4
)
{
    v = XIco4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMXICO4& _XMXICO4::operator=
(
    CONST UINT64 Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMICON4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMICON4::_XMICON4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreIcoN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMICON4::_XMICON4
(
    CONST FLOAT* pArray
)
{
    XMStoreIcoN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMICON4& _XMICON4::operator=
(
    CONST _XMICON4& IcoN4
)
{
    v = IcoN4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMICON4& _XMICON4::operator=
(
    CONST UINT64 Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMICO4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMICO4::_XMICO4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreIco4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMICO4::_XMICO4
(
    CONST FLOAT* pArray
)
{
    XMStoreIco4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMICO4& _XMICO4::operator=
(
    CONST _XMICO4& Ico4
)
{
    v = Ico4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMICO4& _XMICO4::operator=
(
    CONST UINT64 Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUICON4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUICON4::_XMUICON4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUIcoN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUICON4::_XMUICON4
(
    CONST FLOAT* pArray
)
{
    XMStoreUIcoN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUICON4& _XMUICON4::operator=
(
    CONST _XMUICON4& UIcoN4
)
{
    v = UIcoN4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUICON4& _XMUICON4::operator=
(
    CONST UINT64 Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMUICO4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUICO4::_XMUICO4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUIco4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUICO4::_XMUICO4
(
    CONST FLOAT* pArray
)
{
    XMStoreUIco4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUICO4& _XMUICO4::operator=
(
    CONST _XMUICO4& UIco4
)
{
    v = UIco4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUICO4& _XMUICO4::operator=
(
    CONST UINT64 Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMCOLOR4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMCOLOR::_XMCOLOR
(
    FLOAT _r,
    FLOAT _g,
    FLOAT _b,
    FLOAT _a
)
{
    XMStoreColor(this, XMVectorSet(_r, _g, _b, _a));
}

//------------------------------------------------------------------------------

XMFINLINE _XMCOLOR::_XMCOLOR
(
    CONST FLOAT* pArray
)
{
    XMStoreColor(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMCOLOR& _XMCOLOR::operator=
(
    CONST _XMCOLOR& Color
)
{
    c = Color.c;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMCOLOR& _XMCOLOR::operator=
(
    CONST UINT Color
)
{
    c = Color;
    return *this;
}

/****************************************************************************
 *
 * XMBYTEN4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMBYTEN4::_XMBYTEN4
(
    CONST CHAR* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMBYTEN4::_XMBYTEN4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreByteN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMBYTEN4::_XMBYTEN4
(
    CONST FLOAT* pArray
)
{
    XMStoreByteN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMBYTEN4& _XMBYTEN4::operator=
(
    CONST _XMBYTEN4& ByteN4
)
{
    x = ByteN4.x;
    y = ByteN4.y;
    z = ByteN4.z;
    w = ByteN4.w;
    return *this;
}

/****************************************************************************
 *
 * XMBYTE4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMBYTE4::_XMBYTE4
(
    CONST CHAR* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMBYTE4::_XMBYTE4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreByte4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMBYTE4::_XMBYTE4
(
    CONST FLOAT* pArray
)
{
    XMStoreByte4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMBYTE4& _XMBYTE4::operator=
(
    CONST _XMBYTE4& Byte4
)
{
    x = Byte4.x;
    y = Byte4.y;
    z = Byte4.z;
    w = Byte4.w;
    return *this;
}

/****************************************************************************
 *
 * XMUBYTEN4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTEN4::_XMUBYTEN4
(
    CONST BYTE* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTEN4::_XMUBYTEN4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUByteN4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTEN4::_XMUBYTEN4
(
    CONST FLOAT* pArray
)
{
    XMStoreUByteN4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTEN4& _XMUBYTEN4::operator=
(
    CONST _XMUBYTEN4& UByteN4
)
{
    x = UByteN4.x;
    y = UByteN4.y;
    z = UByteN4.z;
    w = UByteN4.w;
    return *this;
}

/****************************************************************************
 *
 * XMUBYTE4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTE4::_XMUBYTE4
(
    CONST BYTE* pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTE4::_XMUBYTE4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUByte4(this, XMVectorSet(_x, _y, _z, _w));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTE4::_XMUBYTE4
(
    CONST FLOAT* pArray
)
{
    XMStoreUByte4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUBYTE4& _XMUBYTE4::operator=
(
    CONST _XMUBYTE4& UByte4
)
{
    x = UByte4.x;
    y = UByte4.y;
    z = UByte4.z;
    w = UByte4.w;
    return *this;
}

/****************************************************************************
 *
 * XMUNIBBLE4 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMUNIBBLE4::_XMUNIBBLE4
(
    CONST CHAR *pArray
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = pArray[3];
}

//------------------------------------------------------------------------------

XMFINLINE _XMUNIBBLE4::_XMUNIBBLE4
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    FLOAT _w
)
{
    XMStoreUNibble4(this, XMVectorSet( _x, _y, _z, _w ));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUNIBBLE4::_XMUNIBBLE4
(
    CONST FLOAT *pArray
)
{
    XMStoreUNibble4(this, XMLoadFloat4((XMFLOAT4*)pArray));
}

//------------------------------------------------------------------------------

XMFINLINE _XMUNIBBLE4& _XMUNIBBLE4::operator=
(
    CONST _XMUNIBBLE4& UNibble4
)
{
    v = UNibble4.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMUNIBBLE4& _XMUNIBBLE4::operator=
(
    CONST USHORT Packed
)
{
    v = Packed;
    return *this;
}

/****************************************************************************
 *
 * XMU555 operators
 *
 ****************************************************************************/

//------------------------------------------------------------------------------

XMFINLINE _XMU555::_XMU555
(
    CONST CHAR *pArray,
    BOOL _w
)
{
    x = pArray[0];
    y = pArray[1];
    z = pArray[2];
    w = _w;
}

//------------------------------------------------------------------------------

XMFINLINE _XMU555::_XMU555
(
    FLOAT _x,
    FLOAT _y,
    FLOAT _z,
    BOOL _w
)
{
    XMStoreU555(this, XMVectorSet(_x, _y, _z, ((_w) ? 1.0f : 0.0f) ));
}

//------------------------------------------------------------------------------

XMFINLINE _XMU555::_XMU555
(
    CONST FLOAT *pArray,
    BOOL _w
)
{
    XMVECTOR V = XMLoadFloat3((XMFLOAT3*)pArray);
    XMStoreU555(this, XMVectorSetW(V, ((_w) ? 1.0f : 0.0f) ));
}

//------------------------------------------------------------------------------

XMFINLINE _XMU555& _XMU555::operator=
(
    CONST _XMU555& U555
)
{
    v = U555.v;
    return *this;
}

//------------------------------------------------------------------------------

XMFINLINE _XMU555& _XMU555::operator=
(
    CONST USHORT Packed
)
{
    v = Packed;
    return *this;
}

#endif // __cplusplus

#if defined(_XM_NO_INTRINSICS_)
#undef XMISNAN
#undef XMISINF
#endif

#endif // __XNAMATHVECTOR_INL__

