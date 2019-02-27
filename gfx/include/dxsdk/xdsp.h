/*-========================================================================-_
 |                                 - XDSP -                                 |
 |        Copyright (c) Microsoft Corporation.  All rights reserved.        |
 |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
 |PROJECT: XDSP                         MODEL:   Unmanaged User-mode        |
 |VERSION: 1.2                          EXCEPT:  No Exceptions              |
 |CLASS:   N / A                        MINREQ:  WinXP, Xbox360             |
 |BASE:    N / A                        DIALECT: MSC++ 14.00                |
 |>------------------------------------------------------------------------<|
 | DUTY: DSP functions with CPU extension specific optimizations            |
 ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^
  NOTES:
    1.  Definition of terms:
            DSP: Digital Signal Processing.
            FFT: Fast Fourier Transform.
            Frame: A block of samples, one per channel,
                   to be played simultaneously.

    2.  All buffer parameters must be 16-byte aligned.

    3.  All FFT functions support only FLOAT32 audio.                       */

#pragma once
//--------------<D-E-F-I-N-I-T-I-O-N-S>-------------------------------------//
#include <windef.h> // general windows types
#include <math.h>   // trigonometric functions
#if defined(_XBOX)  // SIMD intrinsics
    #include <ppcintrinsics.h>
#else
    #include <emmintrin.h>
#endif

//--------------<M-A-C-R-O-S>-----------------------------------------------//
// assertion
#if !defined(DSPASSERT)
    #if DBG
        #define DSPASSERT(exp) if (!(exp)) { OutputDebugStringA("XDSP ASSERT: " #exp ", {" __FUNCTION__ "}\n"); __debugbreak(); }
    #else
        #define DSPASSERT(exp) __assume(exp)
    #endif
#endif

// true if n is a power of 2
#if !defined(ISPOWEROF2)
    #define ISPOWEROF2(n) ( ((n)&((n)-1)) == 0 && (n) != 0 )
#endif

//--------------<H-E-L-P-E-R-S>---------------------------------------------//
namespace XDSP {
#pragma warning(push)
#pragma warning(disable: 4328 4640) // disable "indirection alignment of formal parameter", "construction of local static object is not thread-safe" compile warnings

// Helper functions, used by the FFT functions.
// The application need not call them directly.

    // primitive types
    typedef __m128 XVECTOR;
    typedef XVECTOR& XVECTORREF;
    typedef const XVECTOR& XVECTORREFC;

    // Parallel multiplication of four complex numbers, assuming
    // real and imaginary values are stored in separate vectors.
    __forceinline void vmulComplex (__out XVECTORREF rResult, __out XVECTORREF iResult, __in XVECTORREFC r1, __in XVECTORREFC i1, __in XVECTORREFC r2, __in XVECTORREFC i2)
    {
        // (r1, i1) * (r2, i2) = (r1r2 - i1i2, r1i2 + r2i1)
        XVECTOR vi1i2 = _mm_mul_ps(i1, i2);
        XVECTOR vr1r2 = _mm_mul_ps(r1, r2);
        XVECTOR vr1i2 = _mm_mul_ps(r1, i2);
        XVECTOR vr2i1 = _mm_mul_ps(r2, i1);
        rResult = _mm_sub_ps(vr1r2, vi1i2); // real:      (r1*r2 - i1*i2)
        iResult = _mm_add_ps(vr1i2, vr2i1); // imaginary: (r1*i2 + r2*i1)
    }
    __forceinline void vmulComplex (__inout XVECTORREF r1, __inout XVECTORREF i1, __in XVECTORREFC r2, __in XVECTORREFC i2)
    {
        // (r1, i1) * (r2, i2) = (r1r2 - i1i2, r1i2 + r2i1)
        XVECTOR vi1i2 = _mm_mul_ps(i1, i2);
        XVECTOR vr1r2 = _mm_mul_ps(r1, r2);
        XVECTOR vr1i2 = _mm_mul_ps(r1, i2);
        XVECTOR vr2i1 = _mm_mul_ps(r2, i1);
        r1 = _mm_sub_ps(vr1r2, vi1i2); // real:      (r1*r2 - i1*i2)
        i1 = _mm_add_ps(vr1i2, vr2i1); // imaginary: (r1*i2 + r2*i1)
    }

    // Radix-4 decimation-in-time FFT butterfly.
    // This version assumes that all four elements of the butterfly are
    // adjacent in a single vector.
    //
    // Compute the product of the complex input vector and the
    // 4-element DFT matrix:
    //     | 1  1  1  1 |    | (r1X,i1X) |
    //     | 1 -j -1  j |    | (r1Y,i1Y) |
    //     | 1 -1  1 -1 |    | (r1Z,i1Z) |
    //     | 1  j -1 -j |    | (r1W,i1W) |
    //
    // This matrix can be decomposed into two simpler ones to reduce the
    // number of additions needed. The decomposed matrices look like this:
    //     | 1  0  1  0 |    | 1  0  1  0 |
    //     | 0  1  0 -j |    | 1  0 -1  0 |
    //     | 1  0 -1  0 |    | 0  1  0  1 |
    //     | 0  1  0  j |    | 0  1  0 -1 |
    //
    // Combine as follows:
    //          | 1  0  1  0 |   | (r1X,i1X) |         | (r1X + r1Z, i1X + i1Z) |
    // Temp   = | 1  0 -1  0 | * | (r1Y,i1Y) |       = | (r1X - r1Z, i1X - i1Z) |
    //          | 0  1  0  1 |   | (r1Z,i1Z) |         | (r1Y + r1W, i1Y + i1W) |
    //          | 0  1  0 -1 |   | (r1W,i1W) |         | (r1Y - r1W, i1Y - i1W) |
    //
    //          | 1  0  1  0 |   | (rTempX,iTempX) |   | (rTempX + rTempZ, iTempX + iTempZ) |
    // Result = | 0  1  0 -j | * | (rTempY,iTempY) | = | (rTempY + iTempW, iTempY - rTempW) |
    //          | 1  0 -1  0 |   | (rTempZ,iTempZ) |   | (rTempX - rTempZ, iTempX - iTempZ) |
    //          | 0  1  0  j |   | (rTempW,iTempW) |   | (rTempY - iTempW, iTempY + rTempW) |
    __forceinline void ButterflyDIT4_1 (__inout XVECTORREF r1, __inout XVECTORREF i1)
    {
        // sign constants for radix-4 butterflies
        const static XVECTOR vDFT4SignBits1 = { 0.0f, -0.0f,  0.0f, -0.0f };
        const static XVECTOR vDFT4SignBits2 = { 0.0f,  0.0f, -0.0f, -0.0f };
        const static XVECTOR vDFT4SignBits3 = { 0.0f, -0.0f, -0.0f,  0.0f };

        // calculating Temp
        XVECTOR rTemp = _mm_add_ps( _mm_shuffle_ps(r1, r1, _MM_SHUFFLE(1, 1, 0, 0)),                               // [r1X| r1X|r1Y| r1Y] +
                                    _mm_xor_ps(_mm_shuffle_ps(r1, r1, _MM_SHUFFLE(3, 3, 2, 2)), vDFT4SignBits1) ); // [r1Z|-r1Z|r1W|-r1W]
        XVECTOR iTemp = _mm_add_ps( _mm_shuffle_ps(i1, i1, _MM_SHUFFLE(1, 1, 0, 0)),                               // [i1X| i1X|i1Y| i1Y] +
                                    _mm_xor_ps(_mm_shuffle_ps(i1, i1, _MM_SHUFFLE(3, 3, 2, 2)), vDFT4SignBits1) ); // [i1Z|-i1Z|i1W|-i1W]

        // calculating Result
        XVECTOR rZrWiZiW = _mm_shuffle_ps(rTemp, iTemp, _MM_SHUFFLE(3, 2, 3, 2));       // [rTempZ|rTempW|iTempZ|iTempW]
        XVECTOR rZiWrZiW = _mm_shuffle_ps(rZrWiZiW, rZrWiZiW, _MM_SHUFFLE(3, 0, 3, 0)); // [rTempZ|iTempW|rTempZ|iTempW]
        XVECTOR iZrWiZrW = _mm_shuffle_ps(rZrWiZiW, rZrWiZiW, _MM_SHUFFLE(1, 2, 1, 2)); // [rTempZ|iTempW|rTempZ|iTempW]
        r1 = _mm_add_ps( _mm_shuffle_ps(rTemp, rTemp, _MM_SHUFFLE(1, 0, 1, 0)), // [rTempX| rTempY| rTempX| rTempY] +
                         _mm_xor_ps(rZiWrZiW, vDFT4SignBits2) );                // [rTempZ| iTempW|-rTempZ|-iTempW]
        i1 = _mm_add_ps( _mm_shuffle_ps(iTemp, iTemp, _MM_SHUFFLE(1, 0, 1, 0)), // [iTempX| iTempY| iTempX| iTempY] +
                         _mm_xor_ps(iZrWiZrW, vDFT4SignBits3) );                // [iTempZ|-rTempW|-iTempZ| rTempW]
    }

    // Radix-4 decimation-in-time FFT butterfly.
    // This version assumes that elements of the butterfly are
    // in different vectors, so that each vector in the input
    // contains elements from four different butterflies.
    // The four separate butterflies are processed in parallel.
    //
    // The calculations here are the same as the ones in the single-vector
    // radix-4 DFT, but instead of being done on a single vector (X,Y,Z,W)
    // they are done in parallel on sixteen independent complex values.
    // There is no interdependence between the vector elements:
    // | 1  0  1  0 |    | (rIn0,iIn0) |               | (rIn0 + rIn2, iIn0 + iIn2) |
    // | 1  0 -1  0 | *  | (rIn1,iIn1) |  =   Temp   = | (rIn0 - rIn2, iIn0 - iIn2) |
    // | 0  1  0  1 |    | (rIn2,iIn2) |               | (rIn1 + rIn3, iIn1 + iIn3) |
    // | 0  1  0 -1 |    | (rIn3,iIn3) |               | (rIn1 - rIn3, iIn1 - iIn3) |
    //
    //          | 1  0  1  0 |   | (rTemp0,iTemp0) |   | (rTemp0 + rTemp2, iTemp0 + iTemp2) |
    // Result = | 0  1  0 -j | * | (rTemp1,iTemp1) | = | (rTemp1 + iTemp3, iTemp1 - rTemp3) |
    //          | 1  0 -1  0 |   | (rTemp2,iTemp2) |   | (rTemp0 - rTemp2, iTemp0 - iTemp2) |
    //          | 0  1  0  j |   | (rTemp3,iTemp3) |   | (rTemp1 - iTemp3, iTemp1 + rTemp3) |
    __forceinline void ButterflyDIT4_4 (__inout XVECTORREF r0,
                                        __inout XVECTORREF r1,
                                        __inout XVECTORREF r2,
                                        __inout XVECTORREF r3,
                                        __inout XVECTORREF i0,
                                        __inout XVECTORREF i1,
                                        __inout XVECTORREF i2,
                                        __inout XVECTORREF i3,
                                        __in_ecount(uStride*4) const XVECTOR* __restrict pUnityTableReal,
                                        __in_ecount(uStride*4) const XVECTOR* __restrict pUnityTableImaginary,
                                        const UINT32 uStride, const BOOL fLast)
    {
        DSPASSERT(pUnityTableReal != NULL);
        DSPASSERT(pUnityTableImaginary != NULL);
        DSPASSERT((UINT_PTR)pUnityTableReal % 16 == 0);
        DSPASSERT((UINT_PTR)pUnityTableImaginary % 16 == 0);
        DSPASSERT(ISPOWEROF2(uStride));

        XVECTOR rTemp0, rTemp1, rTemp2, rTemp3, rTemp4, rTemp5, rTemp6, rTemp7;
        XVECTOR iTemp0, iTemp1, iTemp2, iTemp3, iTemp4, iTemp5, iTemp6, iTemp7;

        // calculating Temp
        rTemp0 = _mm_add_ps(r0, r2);          iTemp0 = _mm_add_ps(i0, i2);
        rTemp2 = _mm_add_ps(r1, r3);          iTemp2 = _mm_add_ps(i1, i3);
        rTemp1 = _mm_sub_ps(r0, r2);          iTemp1 = _mm_sub_ps(i0, i2);
        rTemp3 = _mm_sub_ps(r1, r3);          iTemp3 = _mm_sub_ps(i1, i3);
        rTemp4 = _mm_add_ps(rTemp0, rTemp2);  iTemp4 = _mm_add_ps(iTemp0, iTemp2);
        rTemp5 = _mm_add_ps(rTemp1, iTemp3);  iTemp5 = _mm_sub_ps(iTemp1, rTemp3);
        rTemp6 = _mm_sub_ps(rTemp0, rTemp2);  iTemp6 = _mm_sub_ps(iTemp0, iTemp2);
        rTemp7 = _mm_sub_ps(rTemp1, iTemp3);  iTemp7 = _mm_add_ps(iTemp1, rTemp3);

        // calculating Result
        // vmulComplex(rTemp0, iTemp0, rTemp0, iTemp0, pUnityTableReal[0], pUnityTableImaginary[0]); // first one is always trivial
        vmulComplex(rTemp5, iTemp5, pUnityTableReal[uStride], pUnityTableImaginary[uStride]);
        vmulComplex(rTemp6, iTemp6, pUnityTableReal[uStride*2], pUnityTableImaginary[uStride*2]);
        vmulComplex(rTemp7, iTemp7, pUnityTableReal[uStride*3], pUnityTableImaginary[uStride*3]);
        if (fLast) {
            ButterflyDIT4_1(rTemp4, iTemp4);
            ButterflyDIT4_1(rTemp5, iTemp5);
            ButterflyDIT4_1(rTemp6, iTemp6);
            ButterflyDIT4_1(rTemp7, iTemp7);
        }

        r0 = rTemp4;    i0 = iTemp4;
        r1 = rTemp5;    i1 = iTemp5;
        r2 = rTemp6;    i2 = iTemp6;
        r3 = rTemp7;    i3 = iTemp7;
    }

//--------------<F-U-N-C-T-I-O-N-S>-----------------------------------------//

      ////
      // DESCRIPTION:
      //  4-sample FFT.
      //
      // PARAMETERS:
      //  pReal      - [inout] real components, must have at least uCount elements
      //  pImaginary - [inout] imaginary components, must have at least uCount elements
      //  uCount     - [in]    number of FFT iterations
      //
      // RETURN VALUE:
      //  void
      ////
    __forceinline void FFT4 (__inout_ecount(uCount) XVECTOR* __restrict pReal, __inout_ecount(uCount) XVECTOR* __restrict pImaginary, const UINT32 uCount=1)
    {
        DSPASSERT(pReal != NULL);
        DSPASSERT(pImaginary != NULL);
        DSPASSERT((UINT_PTR)pReal % 16 == 0);
        DSPASSERT((UINT_PTR)pImaginary % 16 == 0);
        DSPASSERT(ISPOWEROF2(uCount));

        for (UINT32 uIndex=0; uIndex<uCount; ++uIndex) {
            ButterflyDIT4_1(pReal[uIndex], pImaginary[uIndex]);
        }
    }

      ////
      // DESCRIPTION:
      //  8-sample FFT.
      //
      // PARAMETERS:
      //  pReal      - [inout] real components, must have at least uCount*2 elements
      //  pImaginary - [inout] imaginary components, must have at least uCount*2 elements
      //  uCount     - [in]    number of FFT iterations
      //
      // RETURN VALUE:
      //  void
      ////
    __forceinline void FFT8 (__inout_ecount(uCount*2) XVECTOR* __restrict pReal, __inout_ecount(uCount*2) XVECTOR* __restrict pImaginary, const UINT32 uCount=1)
    {
        DSPASSERT(pReal != NULL);
        DSPASSERT(pImaginary != NULL);
        DSPASSERT((UINT_PTR)pReal % 16 == 0);
        DSPASSERT((UINT_PTR)pImaginary % 16 == 0);
        DSPASSERT(ISPOWEROF2(uCount));

        static XVECTOR wr1 = {  1.0f,  0.70710677f,  0.0f, -0.70710677f };
        static XVECTOR wi1 = {  0.0f, -0.70710677f, -1.0f, -0.70710677f };
        static XVECTOR wr2 = { -1.0f, -0.70710677f,  0.0f,  0.70710677f };
        static XVECTOR wi2 = {  0.0f,  0.70710677f,  1.0f,  0.70710677f };

        for (UINT32 uIndex=0; uIndex<uCount; ++uIndex) {
            XVECTOR* __restrict pR = pReal      + uIndex*2;
            XVECTOR* __restrict pI = pImaginary + uIndex*2;

            XVECTOR oddsR  = _mm_shuffle_ps(pR[0], pR[1], _MM_SHUFFLE(3, 1, 3, 1));
            XVECTOR evensR = _mm_shuffle_ps(pR[0], pR[1], _MM_SHUFFLE(2, 0, 2, 0));
            XVECTOR oddsI  = _mm_shuffle_ps(pI[0], pI[1], _MM_SHUFFLE(3, 1, 3, 1));
            XVECTOR evensI = _mm_shuffle_ps(pI[0], pI[1], _MM_SHUFFLE(2, 0, 2, 0));
            ButterflyDIT4_1(oddsR, oddsI);
            ButterflyDIT4_1(evensR, evensI);

            XVECTOR r, i;
            vmulComplex(r, i, oddsR, oddsI, wr1, wi1);
            pR[0] = _mm_add_ps(evensR, r);
            pI[0] = _mm_add_ps(evensI, i);

            vmulComplex(r, i, oddsR, oddsI, wr2, wi2);
            pR[1] = _mm_add_ps(evensR, r);
            pI[1] = _mm_add_ps(evensI, i);
        }
    }

      ////
      // DESCRIPTION:
      //  16-sample FFT.
      //
      // PARAMETERS:
      //  pReal      - [inout] real components, must have at least uCount*4 elements
      //  pImaginary - [inout] imaginary components, must have at least uCount*4 elements
      //  uCount     - [in]    number of FFT iterations
      //
      // RETURN VALUE:
      //  void
      ////
    __forceinline void FFT16 (__inout_ecount(uCount*4) XVECTOR* __restrict pReal, __inout_ecount(uCount*4) XVECTOR* __restrict pImaginary, const UINT32 uCount=1)
    {
        DSPASSERT(pReal != NULL);
        DSPASSERT(pImaginary != NULL);
        DSPASSERT((UINT_PTR)pReal % 16 == 0);
        DSPASSERT((UINT_PTR)pImaginary % 16 == 0);
        DSPASSERT(ISPOWEROF2(uCount));

        XVECTOR aUnityTableReal[4]      = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.92387950f, 0.70710677f, 0.38268343f, 1.0f, 0.70710677f, -4.3711388e-008f, -0.70710677f, 1.0f, 0.38268343f, -0.70710677f, -0.92387950f };
        XVECTOR aUnityTableImaginary[4] = { -0.0f, -0.0f, -0.0f, -0.0f, -0.0f, -0.38268343f, -0.70710677f, -0.92387950f, -0.0f, -0.70710677f, -1.0f, -0.70710677f, -0.0f, -0.92387950f, -0.70710677f, 0.38268343f };

        for (UINT32 uIndex=0; uIndex<uCount; ++uIndex) {
            ButterflyDIT4_4(pReal[uIndex*4],
                            pReal[uIndex*4 + 1],
                            pReal[uIndex*4 + 2],
                            pReal[uIndex*4 + 3],
                            pImaginary[uIndex*4],
                            pImaginary[uIndex*4 + 1],
                            pImaginary[uIndex*4 + 2],
                            pImaginary[uIndex*4 + 3],
                            aUnityTableReal,
                            aUnityTableImaginary,
                            1, TRUE);
        }
    }

      ////
      // DESCRIPTION:
      //  2^N-sample FFT.
      //
      // REMARKS:
      //  For FFTs length 16 and below, call FFT16(), FFT8(), or FFT4().
      //
      // PARAMETERS:
      //  pReal       - [inout] real components, must have at least (uLength*uCount)/4 elements
      //  pImaginary  - [inout] imaginary components, must have at least (uLength*uCount)/4 elements
      //  pUnityTable - [in]    unity table, must have at least uLength*uCount elements, see FFTInitializeUnityTable()
      //  uLength     - [in]    FFT length in samples, must be a power of 2 > 16
      //  uCount      - [in]    number of FFT iterations
      //
      // RETURN VALUE:
      //  void
      ////
    inline void FFT (__inout_ecount((uLength*uCount)/4) XVECTOR* __restrict pReal, __inout_ecount((uLength*uCount)/4) XVECTOR* __restrict pImaginary, __in_ecount(uLength*uCount) const XVECTOR* __restrict pUnityTable, const UINT32 uLength, const UINT32 uCount=1)
    {
        DSPASSERT(pReal != NULL);
        DSPASSERT(pImaginary != NULL);
        DSPASSERT(pUnityTable != NULL);
        DSPASSERT((UINT_PTR)pReal % 16 == 0);
        DSPASSERT((UINT_PTR)pImaginary % 16 == 0);
        DSPASSERT((UINT_PTR)pUnityTable % 16 == 0);
        DSPASSERT(uLength > 16);
        DSPASSERT(ISPOWEROF2(uLength));
        DSPASSERT(ISPOWEROF2(uCount));

        const XVECTOR* __restrict pUnityTableReal      = pUnityTable;
        const XVECTOR* __restrict pUnityTableImaginary = pUnityTable + (uLength>>2);
        const UINT32 uTotal              = uCount * uLength;
        const UINT32 uTotal_vectors      = uTotal >> 2;
        const UINT32 uStage_vectors      = uLength >> 2;
        const UINT32 uStage_vectors_mask = uStage_vectors - 1;
        const UINT32 uStride        = uLength >> 4; // stride between butterfly elements
        const UINT32 uStrideMask    = uStride - 1;
        const UINT32 uStride2       = uStride * 2;
        const UINT32 uStride3       = uStride * 3;
        const UINT32 uStrideInvMask = ~uStrideMask;

        for (UINT32 uIndex=0; uIndex<(uTotal_vectors>>2); ++uIndex) {
            const UINT32 n = ((uIndex & uStrideInvMask) << 2) + (uIndex & uStrideMask);
            ButterflyDIT4_4(pReal[n],
                            pReal[n + uStride],
                            pReal[n + uStride2],
                            pReal[n + uStride3],
                            pImaginary[n ],
                            pImaginary[n + uStride],
                            pImaginary[n + uStride2],
                            pImaginary[n + uStride3],
                            pUnityTableReal      + (n & uStage_vectors_mask),
                            pUnityTableImaginary + (n & uStage_vectors_mask),
                            uStride, FALSE);
        }

        if (uLength > 16*4) {
            FFT(pReal, pImaginary, pUnityTable+(uLength>>1), uLength>>2, uCount*4);
        } else if (uLength == 16*4) {
            FFT16(pReal, pImaginary, uCount*4);
        } else if (uLength == 8*4) {
            FFT8(pReal, pImaginary, uCount*4);
        } else if (uLength == 4*4) {
            FFT4(pReal, pImaginary, uCount*4);
        }
    }

//--------------------------------------------------------------------------//
  ////
  // DESCRIPTION:
  //  Initializes unity roots lookup table used by FFT functions.
  //  Once initialized, the table need not be initialized again unless a
  //  different FFT length is desired.
  //
  // REMARKS:
  //  The unity tables of FFT length 16 and below are hard coded into the
  //  respective FFT functions and so need not be initialized.
  //
  // PARAMETERS:
  //  pUnityTable - [out] unity table, receives unity roots lookup table, must have at least uLength elements
  //  uLength     - [in]  FFT length in frames, must be a power of 2 > 16
  //
  // RETURN VALUE:
  //  void
  ////
inline void FFTInitializeUnityTable (__out_ecount(uLength) XVECTOR* __restrict pUnityTable, UINT32 uLength)
{
    DSPASSERT(pUnityTable != NULL);
    DSPASSERT(uLength > 16);
    DSPASSERT(ISPOWEROF2(uLength));

    FLOAT32* __restrict pfUnityTable = (FLOAT32* __restrict)pUnityTable;

    // initialize unity table for recursive FFT lengths: uLength, uLength/4, uLength/16... > 16
    do {
        FLOAT32 flStep = 6.283185307f / uLength; // 2PI / FFT length
        uLength >>= 2;

        // pUnityTable[0 to uLength*4-1] contains real components for current FFT length
        // pUnityTable[uLength*4 to uLength*8-1] contains imaginary components for current FFT length
        for (UINT32 i=0; i<4; ++i) {
            for (UINT32 j=0; j<uLength; ++j) {
                UINT32 uIndex = (i*uLength) + j;
                pfUnityTable[uIndex]             = cosf(FLOAT32(i)*FLOAT32(j)*flStep);  // real component
                pfUnityTable[uIndex + uLength*4] = -sinf(FLOAT32(i)*FLOAT32(j)*flStep); // imaginary component
            }
        }
        pfUnityTable += uLength*8;
    } while (uLength > 16);
}

  ////
  // DESCRIPTION:
  //  The FFT functions generate output in bit reversed order.
  //  Use this function to re-arrange them into order of increasing frequency.
  //
  // REMARKS:
  //
  // PARAMETERS:
  //  pOutput     - [out] output buffer, receives samples in order of increasing frequency, cannot overlap pInput, must have at least (1<<uLog2Length)/4 elements
  //  pInput      - [in]  input buffer, samples in bit reversed order as generated by FFT functions, cannot overlap pOutput, must have at least (1<<uLog2Length)/4 elements
  //  uLog2Length - [in]  LOG (base 2) of FFT length in samples, must be >= 2
  //
  // RETURN VALUE:
  //  void
  ////
inline void FFTUnswizzle (__out_ecount((1<<uLog2Length)/4) XVECTOR* __restrict pOutput, __in_ecount((1<<uLog2Length)/4) const XVECTOR* __restrict pInput, const UINT32 uLog2Length)
{
    DSPASSERT(pOutput != NULL);
    DSPASSERT(pInput != NULL);
    DSPASSERT(uLog2Length >= 2);

    FLOAT32*       __restrict pfOutput = (FLOAT32* __restrict)pOutput;
    const FLOAT32* __restrict pfInput  = (const FLOAT32* __restrict)pInput;
    const UINT32 uLength = UINT32(1 << uLog2Length);

    if ((uLog2Length & 0x1) == 0) {
        // even powers of two
        for (UINT32 uIndex=0; uIndex<uLength; ++uIndex) {
            UINT32 n = uIndex;
            n = ( (n & 0xcccccccc) >> 2 )  | ( (n & 0x33333333) << 2 );
            n = ( (n & 0xf0f0f0f0) >> 4 )  | ( (n & 0x0f0f0f0f) << 4 );
            n = ( (n & 0xff00ff00) >> 8 )  | ( (n & 0x00ff00ff) << 8 );
            n = ( (n & 0xffff0000) >> 16 ) | ( (n & 0x0000ffff) << 16 );
            n >>= (32 - uLog2Length);
            pfOutput[n] = pfInput[uIndex];
        }
    } else {
        // odd powers of two
        for (UINT32 uIndex=0; uIndex<uLength; ++uIndex) {
            UINT32 n = (uIndex>>3);
            n = ( (n & 0xcccccccc) >> 2 )  | ( (n & 0x33333333) << 2 );
            n = ( (n & 0xf0f0f0f0) >> 4 )  | ( (n & 0x0f0f0f0f) << 4 );
            n = ( (n & 0xff00ff00) >> 8 )  | ( (n & 0x00ff00ff) << 8 );
            n = ( (n & 0xffff0000) >> 16 ) | ( (n & 0x0000ffff) << 16 );
            n >>= (32 - (uLog2Length-3));
            n |= ((uIndex & 0x7) << (uLog2Length - 3));
            pfOutput[n] = pfInput[uIndex];
        }
    }
}

  ////
  // DESCRIPTION:
  //  Convert complex components to polar form.
  //
  // PARAMETERS:
  //  pOutput         - [out] output buffer, receives samples in polar form, must have at least uLength/4 elements
  //  pInputReal      - [in]  input buffer (real components), must have at least uLength/4 elements
  //  pInputImaginary - [in]  input buffer (imaginary components), must have at least uLength/4 elements
  //  uLength         - [in]  FFT length in samples, must be a power of 2 >= 4
  //
  // RETURN VALUE:
  //  void
  ////
inline void FFTPolar (__out_ecount(uLength/4) XVECTOR* __restrict pOutput, __in_ecount(uLength/4) const XVECTOR* __restrict pInputReal, __in_ecount(uLength/4) const XVECTOR* __restrict pInputImaginary, const UINT32 uLength)
{
    DSPASSERT(pOutput != NULL);
    DSPASSERT(pInputReal != NULL);
    DSPASSERT(pInputImaginary != NULL);
    DSPASSERT(uLength >= 4);
    DSPASSERT(ISPOWEROF2(uLength));

    FLOAT32 flOneOverLength = 1.0f / uLength;

    // result = sqrtf((real/uLength)^2 + (imaginary/uLength)^2) * 2
        XVECTOR vOneOverLength = _mm_set_ps1(flOneOverLength);

        for (UINT32 uIndex=0; uIndex<(uLength>>2); ++uIndex) {
            XVECTOR vReal      = _mm_mul_ps(pInputReal[uIndex], vOneOverLength);
            XVECTOR vImaginary = _mm_mul_ps(pInputImaginary[uIndex], vOneOverLength);
            XVECTOR vRR        = _mm_mul_ps(vReal, vReal);
            XVECTOR vII        = _mm_mul_ps(vImaginary, vImaginary);
            XVECTOR vRRplusII  = _mm_add_ps(vRR, vII);
            XVECTOR vTotal  = _mm_sqrt_ps(vRRplusII);
            pOutput[uIndex] = _mm_add_ps(vTotal, vTotal);
        }
}

//--------------------------------------------------------------------------//
  ////
  // DESCRIPTION:
  //  Deinterleaves audio samples such that all samples corresponding to

  //
  // REMARKS:
  //  For example, audio of the form [LRLRLR] becomes [LLLRRR].
  //
  // PARAMETERS:
  //  pOutput       - [out] output buffer, receives samples in deinterleaved form, cannot overlap pInput, must have at least (uChannelCount*uFrameCount)/4 elements
  //  pInput        - [in]  input buffer, cannot overlap pOutput, must have at least (uChannelCount*uFrameCount)/4 elements
  //  uChannelCount - [in]  number of channels, must be > 1
  //  uFrameCount   - [in]  number of frames of valid data, must be > 0
  //
  // RETURN VALUE:
  //  void
  ////
inline void Deinterleave (__out_ecount((uChannelCount*uFrameCount)/4) XVECTOR* __restrict pOutput, __in_ecount((uChannelCount*uFrameCount)/4) const XVECTOR* __restrict pInput, const UINT32 uChannelCount, const UINT32 uFrameCount)
{
    DSPASSERT(pOutput != NULL);
    DSPASSERT(pInput != NULL);
    DSPASSERT(uChannelCount > 1);
    DSPASSERT(uFrameCount > 0);

    FLOAT32*       __restrict pfOutput = (FLOAT32* __restrict)pOutput;
    const FLOAT32* __restrict pfInput  = (const FLOAT32* __restrict)pInput;

    for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
        for (UINT32 uFrame=0; uFrame<uFrameCount; ++uFrame) {
            pfOutput[uChannel * uFrameCount + uFrame] = pfInput[uFrame * uChannelCount + uChannel];
        }
    }
}

  ////
  // DESCRIPTION:
  //  Interleaves audio samples such that all samples corresponding to

  //
  // REMARKS:
  //  For example, audio of the form [LLLRRR] becomes [LRLRLR].
  //
  // PARAMETERS:
  //  pOutput       - [out] output buffer, receives samples in interleaved form, cannot overlap pInput, must have at least (uChannelCount*uFrameCount)/4 elements
  //  pInput        - [in]  input buffer, cannot overlap pOutput, must have at least (uChannelCount*uFrameCount)/4 elements
  //  uChannelCount - [in]  number of channels, must be > 1
  //  uFrameCount   - [in]  number of frames of valid data, must be > 0
  //
  // RETURN VALUE:
  //  void
  ////
inline void Interleave (__out_ecount((uChannelCount*uFrameCount)/4) XVECTOR* __restrict pOutput, __in_ecount((uChannelCount*uFrameCount)/4) const XVECTOR* __restrict pInput, const UINT32 uChannelCount, const UINT32 uFrameCount)
{
    DSPASSERT(pOutput != NULL);
    DSPASSERT(pInput != NULL);
    DSPASSERT(uChannelCount > 1);
    DSPASSERT(uFrameCount > 0);

    FLOAT32*       __restrict pfOutput = (FLOAT32* __restrict)pOutput;
    const FLOAT32* __restrict pfInput  = (const FLOAT32* __restrict)pInput;

    for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
        for (UINT32 uFrame=0; uFrame<uFrameCount; ++uFrame) {
            pfOutput[uFrame * uChannelCount + uChannel] = pfInput[uChannel * uFrameCount + uFrame];
        }
    }
}

//--------------------------------------------------------------------------//
  ////
  // DESCRIPTION:
  //  This function applies a 2^N-sample FFT and unswizzles the result such
  //  that the samples are in order of increasing frequency.
  //  Audio is first deinterleaved if multichannel.
  //
  // PARAMETERS:
  //  pReal         - [inout] real components, must have at least (1<<uLog2Length*uChannelCount)/4 elements
  //  pImaginary    - [out]   imaginary components, must have at least (1<<uLog2Length*uChannelCount)/4 elements
  //  pUnityTable   - [in]    unity table, must have at least (1<<uLog2Length) elements, see FFTInitializeUnityTable()
  //  uChannelCount - [in]    number of channels, must be within [1, 6]
  //  uLog2Length   - [in]    LOG (base 2) of FFT length in frames, must within [2, 9]
  //
  // RETURN VALUE:
  //  void
  ////
inline void FFTInterleaved (__inout_ecount((1<<uLog2Length*uChannelCount)/4) XVECTOR* __restrict pReal, __out_ecount((1<<uLog2Length*uChannelCount)/4) XVECTOR* __restrict pImaginary, __in_ecount(1<<uLog2Length) const XVECTOR* __restrict pUnityTable, const UINT32 uChannelCount, const UINT32 uLog2Length)
{
    DSPASSERT(pReal != NULL);
    DSPASSERT(pImaginary != NULL);
    DSPASSERT(pUnityTable != NULL);
    DSPASSERT((UINT_PTR)pReal % 16 == 0);
    DSPASSERT((UINT_PTR)pImaginary % 16 == 0);
    DSPASSERT((UINT_PTR)pUnityTable % 16 == 0);
    DSPASSERT(uChannelCount > 0 && uChannelCount <= 6);
    DSPASSERT(uLog2Length >= 2 && uLog2Length <= 9);

    XVECTOR vRealTemp[768];
    XVECTOR vImaginaryTemp[768];
    const UINT32 uLength = UINT32(1 << uLog2Length);

    if (uChannelCount > 1) {
        Deinterleave(vRealTemp, pReal, uChannelCount, uLength);
    } else {
        CopyMemory(vRealTemp, pReal, (uLength>>2)*sizeof(XVECTOR));
    }
        for (UINT32 u=0; u<uChannelCount*(uLength>>2); u++) {
            vImaginaryTemp[u] = _mm_setzero_ps();
        }

    if (uLength > 16) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)], pUnityTable, uLength);
        }
    } else if (uLength == 16) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT16(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)]);
        }
    } else if (uLength == 8) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT8(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)]);
        }
    } else if (uLength == 4) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT4(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)]);
        }
    }

    for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
        FFTUnswizzle(&pReal[uChannel*(uLength>>2)], &vRealTemp[uChannel*(uLength>>2)], uLog2Length);
        FFTUnswizzle(&pImaginary[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)], uLog2Length);
    }
}

  ////
  // DESCRIPTION:
  //  This function applies a 2^N-sample inverse FFT.
  //  Audio is interleaved if multichannel.
  //
  // PARAMETERS:
  //  pReal         - [inout] real components, must have at least (1<<uLog2Length*uChannelCount)/4 elements
  //  pImaginary    - [out]   imaginary components, must have at least (1<<uLog2Length*uChannelCount)/4 elements
  //  pUnityTable   - [in]    unity table, must have at least (1<<uLog2Length) elements, see FFTInitializeUnityTable()
  //  uChannelCount - [in]    number of channels, must be > 0
  //  uLog2Length   - [in]    LOG (base 2) of FFT length in frames, must within [2, 10]
  //
  // RETURN VALUE:
  //  void
  ////
inline void IFFTDeinterleaved (__inout_ecount((1<<uLog2Length*uChannelCount)/4) XVECTOR* __restrict pReal, __out_ecount((1<<uLog2Length*uChannelCount)/4) XVECTOR* __restrict pImaginary, __in_ecount(1<<uLog2Length) const XVECTOR* __restrict pUnityTable, const UINT32 uChannelCount, const UINT32 uLog2Length)
{
    DSPASSERT(pReal != NULL);
    DSPASSERT(pImaginary != NULL);
    DSPASSERT(pUnityTable != NULL);
    DSPASSERT((UINT_PTR)pReal % 16 == 0);
    DSPASSERT((UINT_PTR)pImaginary % 16 == 0);
    DSPASSERT((UINT_PTR)pUnityTable % 16 == 0);
    DSPASSERT(uChannelCount > 0 && uChannelCount <= 6);
    DSPASSERT(uLog2Length >= 2 && uLog2Length <= 9);

    XVECTOR vRealTemp[768];
    XVECTOR vImaginaryTemp[768];
    const UINT32 uLength = UINT32(1 << uLog2Length);

        const XVECTOR vRnp = _mm_set_ps1(1.0f/uLength);
        const XVECTOR vRnm = _mm_set_ps1(-1.0f/uLength);
        for (UINT32 u=0; u<uChannelCount*(uLength>>2); u++) {
            vRealTemp[u]      = _mm_mul_ps(pReal[u], vRnp);
            vImaginaryTemp[u] = _mm_mul_ps(pImaginary[u], vRnm);
        }

    if (uLength > 16) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)], pUnityTable, uLength);
        }
    } else if (uLength == 16) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT16(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)]);
        }
    } else if (uLength == 8) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT8(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)]);
        }
    } else if (uLength == 4) {
        for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
            FFT4(&vRealTemp[uChannel*(uLength>>2)], &vImaginaryTemp[uChannel*(uLength>>2)]);
        }
    }

    for (UINT32 uChannel=0; uChannel<uChannelCount; ++uChannel) {
        FFTUnswizzle(&vImaginaryTemp[uChannel*(uLength>>2)], &vRealTemp[uChannel*(uLength>>2)], uLog2Length);
    }
    if (uChannelCount > 1) {
        Interleave(pReal, vImaginaryTemp, uChannelCount, uLength);
    } else {
        CopyMemory(pReal, vImaginaryTemp, (uLength>>2)*sizeof(XVECTOR));
    }
}

#pragma warning(pop)
}; // namespace XDSP
//---------------------------------<-EOF->----------------------------------//
