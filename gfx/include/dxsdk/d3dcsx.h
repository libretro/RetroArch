
//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  File:       D3DX11GPGPU.h
//  Content:    D3DX11 General Purpose GPU computing algorithms
//
//////////////////////////////////////////////////////////////////////////////

/*#include <winapifamily.h>*/

/*#pragma region Desktop Family*/
/*#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)*/

#include "d3d11.h"

#ifndef __D3DX11GPGPU_H__
#define __D3DX11GPGPU_H__

// Current name of the DLL shipped in the same SDK as this header.

#define D3DCSX_DLL_W L"d3dcsx_47.dll"
#define D3DCSX_DLL_A "d3dcsx_47.dll"

#ifdef UNICODE
    #define D3DCSX_DLL D3DCSX_DLL_W
#else
    #define D3DCSX_DLL D3DCSX_DLL_A
#endif

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

//////////////////////////////////////////////////////////////////////////////

typedef enum D3DX11_SCAN_DATA_TYPE
{
    D3DX11_SCAN_DATA_TYPE_FLOAT = 1,
    D3DX11_SCAN_DATA_TYPE_INT,
    D3DX11_SCAN_DATA_TYPE_UINT,
} D3DX11_SCAN_DATA_TYPE;

typedef enum D3DX11_SCAN_OPCODE
{
    D3DX11_SCAN_OPCODE_ADD = 1,
    D3DX11_SCAN_OPCODE_MIN,
    D3DX11_SCAN_OPCODE_MAX,
    D3DX11_SCAN_OPCODE_MUL,
    D3DX11_SCAN_OPCODE_AND,
    D3DX11_SCAN_OPCODE_OR,
    D3DX11_SCAN_OPCODE_XOR,
} D3DX11_SCAN_OPCODE;

typedef enum D3DX11_SCAN_DIRECTION
{
    D3DX11_SCAN_DIRECTION_FORWARD = 1,
    D3DX11_SCAN_DIRECTION_BACKWARD,
} D3DX11_SCAN_DIRECTION;

//////////////////////////////////////////////////////////////////////////////
// ID3DX11Scan:
//////////////////////////////////////////////////////////////////////////////

// {5089b68f-e71d-4d38-be8e-f363b95a9405}
DEFINE_GUID(IID_ID3DX11Scan, 0x5089b68f, 0xe71d, 0x4d38, 0xbe, 0x8e, 0xf3, 0x63, 0xb9, 0x5a, 0x94, 0x05);

#undef INTERFACE
#define INTERFACE ID3DX11Scan

DECLARE_INTERFACE_(ID3DX11Scan, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX11Scan

    STDMETHOD(SetScanDirection)(THIS_ D3DX11_SCAN_DIRECTION Direction) PURE;

    //=============================================================================
    // Performs an unsegmented scan of a sequence in-place or out-of-place
    //  ElementType         element type
    //  OpCode              binary operation
    //  Direction           scan direction
    //  ElementScanSize     size of scan, in elements
    //  pSrc                input sequence on the device. pSrc==pDst for in-place scans
    //  pDst                output sequence on the device
    //=============================================================================
    STDMETHOD(Scan)( THIS_
        D3DX11_SCAN_DATA_TYPE               ElementType,
        D3DX11_SCAN_OPCODE                  OpCode,
        UINT                                ElementScanSize,
        _In_ ID3D11UnorderedAccessView*     pSrc,
        _In_ ID3D11UnorderedAccessView*     pDst
    ) PURE;

    //=============================================================================
    // Performs a multiscan of a sequence in-place or out-of-place
    //  ElementType         element type
    //  OpCode              binary operation
    //  Direction           scan direction
    //  ElementScanSize     size of scan, in elements
    //  ElementScanPitch    pitch of the next scan, in elements
    //  ScanCount           number of scans in a multiscan
    //  pSrc                input sequence on the device. pSrc==pDst for in-place scans
    //  pDst                output sequence on the device
    //=============================================================================
    STDMETHOD(Multiscan)( THIS_
        D3DX11_SCAN_DATA_TYPE               ElementType,
        D3DX11_SCAN_OPCODE                  OpCode,
        UINT                                ElementScanSize,
        UINT                                ElementScanPitch,
        UINT                                ScanCount,
        _In_ ID3D11UnorderedAccessView*     pSrc,
        _In_ ID3D11UnorderedAccessView*     pDst
    ) PURE;
};

//=============================================================================
// Creates a scan context
//  pDevice             the device context
//  MaxElementScanSize  maximum single scan size, in elements (FLOAT, UINT, or INT)
//  MaxScanCount        maximum number of scans in multiscan
//  ppScanContext       new scan context
//=============================================================================
HRESULT WINAPI D3DX11CreateScan(
    _In_ ID3D11DeviceContext* pDeviceContext,
    UINT MaxElementScanSize,
    UINT MaxScanCount,
    _Out_ ID3DX11Scan** ppScan );

//////////////////////////////////////////////////////////////////////////////
// ID3DX11SegmentedScan:
//////////////////////////////////////////////////////////////////////////////

// {a915128c-d954-4c79-bfe1-64db923194d6}
DEFINE_GUID(IID_ID3DX11SegmentedScan, 0xa915128c, 0xd954, 0x4c79, 0xbf, 0xe1, 0x64, 0xdb, 0x92, 0x31, 0x94, 0xd6);

#undef INTERFACE
#define INTERFACE ID3DX11SegmentedScan

DECLARE_INTERFACE_(ID3DX11SegmentedScan, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX11SegmentedScan

    STDMETHOD(SetScanDirection)(THIS_ D3DX11_SCAN_DIRECTION Direction) PURE;

    //=============================================================================
    // Performs a segscan of a sequence in-place or out-of-place
    //  ElementType         element type
    //  OpCode              binary operation
    //  Direction           scan direction
    //  pSrcElementFlags    compact array of bits, one per element of pSrc.  A set value
    //                      indicates the start of a new segment.
    //  ElementScanSize     size of scan, in elements
    //  pSrc                input sequence on the device. pSrc==pDst for in-place scans
    //  pDst                output sequence on the device
    //=============================================================================
    STDMETHOD(SegScan)( THIS_
        D3DX11_SCAN_DATA_TYPE               ElementType,
        D3DX11_SCAN_OPCODE                  OpCode,
        UINT                                ElementScanSize,
        _In_opt_ ID3D11UnorderedAccessView* pSrc,
        _In_ ID3D11UnorderedAccessView*     pSrcElementFlags,
        _In_ ID3D11UnorderedAccessView*     pDst
    ) PURE;
};

//=============================================================================
// Creates a segmented scan context
//  pDevice             the device context
//  MaxElementScanSize  maximum single scan size, in elements (FLOAT, UINT, or INT)
//  ppScanContext       new scan context
//=============================================================================
HRESULT WINAPI D3DX11CreateSegmentedScan(
    _In_ ID3D11DeviceContext* pDeviceContext,
    UINT MaxElementScanSize,
    _Out_ ID3DX11SegmentedScan** ppScan );

//////////////////////////////////////////////////////////////////////////////

#define D3DX11_FFT_MAX_PRECOMPUTE_BUFFERS 4
#define D3DX11_FFT_MAX_TEMP_BUFFERS 4
#define D3DX11_FFT_MAX_DIMENSIONS 32

//////////////////////////////////////////////////////////////////////////////
// ID3DX11FFT:
//////////////////////////////////////////////////////////////////////////////

// {b3f7a938-4c93-4310-a675-b30d6de50553}
DEFINE_GUID(IID_ID3DX11FFT, 0xb3f7a938, 0x4c93, 0x4310, 0xa6, 0x75, 0xb3, 0x0d, 0x6d, 0xe5, 0x05, 0x53);

#undef INTERFACE
#define INTERFACE ID3DX11FFT

DECLARE_INTERFACE_(ID3DX11FFT, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX11FFT

    // scale for forward transform (defaults to 1 if set to 0)
    STDMETHOD(SetForwardScale)(THIS_ FLOAT ForwardScale) PURE;
    STDMETHOD_(FLOAT, GetForwardScale)(THIS) PURE;

    // scale for inverse transform (defaults to 1/N if set to 0, where N is
    // the product of the transformed dimension lengths
    STDMETHOD(SetInverseScale)(THIS_ FLOAT InverseScale) PURE;
    STDMETHOD_(FLOAT, GetInverseScale)(THIS) PURE;

    //------------------------------------------------------------------------------
    // Attaches buffers to the context and performs any required precomputation.
    // The buffers must be no smaller than the corresponding buffer sizes returned
    // by D3DX11CreateFFT*(). Temp buffers may beshared between multiple contexts,
    // though care should be taken to concurrently execute multiple FFTs which share
    // temp buffers.
    //
    // NumTempBuffers               number of buffers in ppTempBuffers
    // ppTempBuffers                temp buffers to attach
    // NumPrecomputeBuffers         number of buffers in ppPrecomputeBufferSizes
    // ppPrecomputeBufferSizes      buffers to hold precomputed data
    STDMETHOD(AttachBuffersAndPrecompute)(  THIS_
        _In_range_(0,D3DX11_FFT_MAX_TEMP_BUFFERS) UINT NumTempBuffers,
        _In_reads_(NumTempBuffers) ID3D11UnorderedAccessView* const* ppTempBuffers,
        _In_range_(0,D3DX11_FFT_MAX_PRECOMPUTE_BUFFERS) UINT NumPrecomputeBuffers,
        _In_reads_(NumPrecomputeBuffers) ID3D11UnorderedAccessView* const* ppPrecomputeBufferSizes ) PURE;

    //------------------------------------------------------------------------------
    // Call after buffers have been attached to the context, pInput and *ppOuput can
    // be one of the temp buffers.  If *ppOutput == NULL, then the computation will ping-pong
    // between temp buffers and the last buffer written to is stored at *ppOutput.
    // Otherwise, *ppOutput is used as the output buffer (which may incur an extra copy).
    //
    // The format of complex data is interleaved components, e.g. (Real0, Imag0),
    // (Real1, Imag1) ... etc. Data is stored in row major order
    //
    // pInputBuffer         view onto input buffer
    // ppOutpuBuffert       pointer to view of output buffer
    STDMETHOD(ForwardTransform)( THIS_
        _In_ const ID3D11UnorderedAccessView* pInputBuffer,
        _Inout_ ID3D11UnorderedAccessView** ppOutputBuffer ) PURE;

    STDMETHOD(InverseTransform)( THIS_
        _In_ const ID3D11UnorderedAccessView* pInputBuffer,
        _Inout_ ID3D11UnorderedAccessView** ppOutputBuffer ) PURE;
};

//////////////////////////////////////////////////////////////////////////////
// ID3DX11FFT Creation Routines
//////////////////////////////////////////////////////////////////////////////

typedef enum D3DX11_FFT_DATA_TYPE
{
    D3DX11_FFT_DATA_TYPE_REAL,
    D3DX11_FFT_DATA_TYPE_COMPLEX,
} D3DX11_FFT_DATA_TYPE;

typedef enum D3DX11_FFT_DIM_MASK
{
    D3DX11_FFT_DIM_MASK_1D   = 0x1,
    D3DX11_FFT_DIM_MASK_2D   = 0x3,
    D3DX11_FFT_DIM_MASK_3D   = 0x7,
} D3DX11_FFT_DIM_MASK;

typedef struct D3DX11_FFT_DESC
{
    UINT NumDimensions;                             // number of dimensions
    UINT ElementLengths[D3DX11_FFT_MAX_DIMENSIONS]; // length of each dimension
    UINT DimensionMask;                             // a bit set for each dimensions to transform
                                                    //     (see D3DX11_FFT_DIM_MASK for common masks)
    D3DX11_FFT_DATA_TYPE Type;                      // type of the elements in spatial domain
} D3DX11_FFT_DESC;

//------------------------------------------------------------------------------
// NumTempBufferSizes           Number of temporary buffers needed
// pTempBufferSizes             Minimum sizes (in FLOATs) of temporary buffers
// NumPrecomputeBufferSizes     Number of precompute buffers needed
// pPrecomputeBufferSizes       minimum sizes (in FLOATs) for precompute buffers
//------------------------------------------------------------------------------

typedef struct D3DX11_FFT_BUFFER_INFO
{
    _Field_range_(0,D3DX11_FFT_MAX_TEMP_BUFFERS) UINT NumTempBufferSizes;
    UINT TempBufferFloatSizes[D3DX11_FFT_MAX_TEMP_BUFFERS];
    _Field_range_(0,D3DX11_FFT_MAX_PRECOMPUTE_BUFFERS) UINT NumPrecomputeBufferSizes;
    UINT PrecomputeBufferFloatSizes[D3DX11_FFT_MAX_PRECOMPUTE_BUFFERS];
} D3DX11_FFT_BUFFER_INFO;

typedef enum D3DX11_FFT_CREATE_FLAG
{
    D3DX11_FFT_CREATE_FLAG_NO_PRECOMPUTE_BUFFERS = 0x01L,   // do not precompute values and store into buffers
} D3DX11_FFT_CREATE_FLAG;

//------------------------------------------------------------------------------
// Creates an ID3DX11FFT COM interface object and returns a pointer to it at *ppFFT.
// The descriptor describes the shape of the data as well as the scaling factors
// that should be used for forward and inverse transforms.
// The FFT computation may require temporaries that act as ping-pong buffers
// and for other purposes. aTempSizes is a list of the sizes required for
// temporaries. Likewise, some data may need to be precomputed and the sizes
// of those sizes are returned in aPrecomputedBufferSizes.
//
// To perform a computation, follow these steps:
// 1) Create the FFT context object
// 2) Precompute (and Attach temp working buffers of at least the required size)
// 3) Call Compute() on some input data
//
// Compute() may be called repeatedly with different inputs and transform
// directions. When finished with the FFT work, release the FFT interface()
//
// Device                     Direct3DDeviceContext to use                      in
// pDesc                      Descriptor for FFT transform                      in
// Count                      the number of 1D FFTs to perform                  in
// Flags                      See D3DX11_FFT_CREATE_FLAG                        in
// pBufferInfo                Pointer to BUFFER_INFO struct, filled by funciton out
// ppFFT                      Pointer to returned context pointer               out
//------------------------------------------------------------------------------

HRESULT WINAPI D3DX11CreateFFT(
   ID3D11DeviceContext* pDeviceContext,
   _In_ const D3DX11_FFT_DESC* pDesc,
   UINT Flags,
   _Out_ D3DX11_FFT_BUFFER_INFO* pBufferInfo,
   _Out_ ID3DX11FFT** ppFFT
 );

HRESULT WINAPI D3DX11CreateFFT1DReal(
   ID3D11DeviceContext* pDeviceContext,
   UINT X,
   UINT Flags,
   _Out_ D3DX11_FFT_BUFFER_INFO* pBufferInfo,
   _Out_ ID3DX11FFT** ppFFT
 );
HRESULT WINAPI D3DX11CreateFFT1DComplex(
   ID3D11DeviceContext* pDeviceContext,
   UINT X,
   UINT Flags,
   _Out_ D3DX11_FFT_BUFFER_INFO* pBufferInfo,
   _Out_ ID3DX11FFT** ppFFT
 );
HRESULT WINAPI D3DX11CreateFFT2DReal(
   ID3D11DeviceContext* pDeviceContext,
   UINT X,
   UINT Y,
   UINT Flags,
   _Out_ D3DX11_FFT_BUFFER_INFO* pBufferInfo,
   _Out_ ID3DX11FFT** ppFFT
 );
HRESULT WINAPI D3DX11CreateFFT2DComplex(
   ID3D11DeviceContext* pDeviceContext,
   UINT X,
   UINT Y,
   UINT Flags,
   _Out_ D3DX11_FFT_BUFFER_INFO* pBufferInfo,
   _Out_ ID3DX11FFT** ppFFT
 );
HRESULT WINAPI D3DX11CreateFFT3DReal(
   ID3D11DeviceContext* pDeviceContext,
   UINT X,
   UINT Y,
   UINT Z,
   UINT Flags,
   _Out_ D3DX11_FFT_BUFFER_INFO* pBufferInfo,
   _Out_ ID3DX11FFT** ppFFT
 );
HRESULT WINAPI D3DX11CreateFFT3DComplex(
   ID3D11DeviceContext* pDeviceContext,
   UINT X,
   UINT Y,
   UINT Z,
   UINT Flags,
   _Out_ D3DX11_FFT_BUFFER_INFO* pBufferInfo,
   _Out_ ID3DX11FFT** ppFFT
 );

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__D3DX11GPGPU_H__

/*#endif*/ /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */
/*#pragma endregion*/
