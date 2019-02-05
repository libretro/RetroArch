//=============================================================================
// D3D11 HLSL Routines for Manual Pack/Unpack of 32-bit DXGI_FORMAT_*
//=============================================================================
//
// This file contains format conversion routines for use in the
// Compute Shader or Pixel Shader on D3D11 Hardware.
//
// Skip to the end of this comment to see a summary of the routines
// provided.  The rest of the text below explains why they are needed
// and how to use them.
//
// The scenario where these can be useful is if your application
// needs to simultaneously both read and write texture - i.e. in-place
// image editing.
//
// D3D11's Unordered Access View (UAV) of a Texture1D/2D/3D resource
// allows random access reads and writes to memory from a Compute Shader
// or Pixel Shader.  However, the only texture format that supports this
// is DXGI_FORMAT_R32_UINT.  e.g. Other more interesting formats like
// DXGI_FORMAT_R8G8B8A8_UNORM do not support simultaneous read and
// write.  You can use such formats for random access writing only
// using a UAV, or reading only using a Shader Resource View (SRV).
// But for simultaneous read+write, the format conversion hardware is
// not available.
//
// There is a workaround to this limitation, involving casting the texture
// to R32_UINT when creating a UAV, as long as the original format of the
// resource supports it (most 32 bit per element formats). This allows
// simultaneous read+write as long as the shader does manual format
// unpacking on read and packing on write.
//
// The benefit is that later on, other views such as RenderTarget Views
// or ShaderResource Views on the same texture can be used with the
// proper format (e.g. DXGI_FORMAT_R16G16_FLOAT) so the hardware can
// do the usual automatic format unpack/pack and do texture filtering etc.
// where there are no hardware limitations.
//
// The sequence of actions for an application is the following:
//
// Suppose you want to make a texture than you can use a Pixel Shader
// or Compute Shader to perform in-place editing, and that the format
// you want the data to be stored in happens to be a descendent
// of of one of these formats:
//
//    DXGI_FORMAT_R10G10B10A2_TYPELESS
//    DXGI_FORMAT_R8G8B8A8_TYPELESS
//    DXGI_FORMAT_B8G8R8A8_TYPELESS
//    DXGI_FORMAT_B8G8R8X8_TYPELESS
//    DXGI_FORMAT_R16G16_TYPELESS
//
// e.g. DXGI_FORMAT_R10G10B10A2_UNORM is a descendent of
//      DXGI_FORMAT_R10G10B10A2_TYPELESS, so it supports the
//      usage pattern described here.
//
// (Formats descending from DXGI_FORMAT_R32_TYPELESS, such as
//  DXGI_FORMAT_R32_FLOAT, are trivially supported without
//  needing any of the format conversion help provided here.)
//
// Steps:
//
// (1) Create a texture with the appropriate _TYPELESS format above
//     along with the needed bind flags, such as
//     D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE.
//
// (2) For in-place image editing, create a UAV with the format
//     DXGI_FORMAT_R32_UINT.  D3D normally doesn't allow casting
//     between different format "families", but the API makes
//     an exception here.
//
// (3) In the Compute Shader or Pixel Shader, use the appropriate
//     format pack/unpack routines provided in this file.
//     For example if the DXGI_FORMAT_R32_UINT UAV really holds
//     DXGI_FORMAT_R10G10B10A2_UNORM data, then, after reading a
//     uint from the UAV into the shader, unpack by calling:
//
//          XMFLOAT4 D3DX_R10G10B10A2_UNORM_to_FLOAT4(UINT packedInput)
//
//     Then to write to the UAV in the same shader, call the following
//     to pack shader data into a uint that can be written out:
//
//          UINT D3DX_FLOAT4_to_R10G10B10A2_UNORM(hlsl_precise XMFLOAT4 unpackedInput)
//
// (4) Other views, such as SRVs, can be created with the desired format;
//     e.g. DXGI_FORMAT_R10G10B10A2_UNORM if the resource was created as
//     DXGI_FORMAT_R10G10B10A2_TYPELESS.  When that view is accessed by a
//     shader, the hardware can do automatic type conversion as usual.
//
// Note, again, that if the shader only needs to write to a UAV, or read
// as an SRV, then none of this is needed - fully typed UAV or SRVs can
// be used.  Only if simultaneous reading and writing to a UAV of a texture
// is needed are the format conversion routines provided here potentially
// useful.
//
// The following is the list of format conversion routines included in this
// file, categorized by the DXGI_FORMAT they unpack/pack.  Each of the
// formats supported descends from one of the TYPELESS formats listed
// above, and supports casting to DXGI_FORMAT_R32_UINT as a UAV.
//
// DXGI_FORMAT_R10G10B10A2_UNORM:
//
//      XMFLOAT4 D3DX_R10G10B10A2_UNORM_to_FLOAT4(UINT packedInput)
//      UINT     D3DX_FLOAT4_to_R10G10B10A2_UNORM(hlsl_precise XMFLOAT4 unpackedInput)
//
// DXGI_FORMAT_R10G10B10A2_UINT:
//
//      XMUINT4 D3DX_R10G10B10A2_UINT_to_UINT4(UINT packedInput)
//      UINT    D3DX_UINT4_to_R10G10B10A2_UINT(XMUINT4 unpackedInput)
//
// DXGI_FORMAT_R8G8B8A8_UNORM:
//
//      XMFLOAT4 D3DX_R8G8B8A8_UNORM_to_FLOAT4(UINT packedInput)
//      UINT     D3DX_FLOAT4_to_R8G8B8A8_UNORM(hlsl_precise XMFLOAT4 unpackedInput)
//
// DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
//
//      XMFLOAT4 D3DX_R8G8B8A8_UNORM_SRGB_to_FLOAT4_inexact(UINT packedInput) *
//      XMFLOAT4 D3DX_R8G8B8A8_UNORM_SRGB_to_FLOAT4(UINT packedInput)
//      UINT     D3DX_FLOAT4_to_R8G8B8A8_UNORM_SRGB(hlsl_precise XMFLOAT4 unpackedInput)
//
//      * The "_inexact" function above uses shader instructions that don't
//      have high enough precision to give the exact answer, albeit close.
//      The alternative function uses a lookup table stored in the shader
//      to give an exact SRGB->float conversion.
//
// DXGI_FORMAT_R8G8B8A8_UINT:
//
//      XMUINT4 D3DX_R8G8B8A8_UINT_to_UINT4(UINT packedInput)
//      XMUINT  D3DX_UINT4_to_R8G8B8A8_UINT(XMUINT4 unpackedInput)
//
// DXGI_FORMAT_R8G8B8A8_SNORM:
//
//      XMFLOAT4 D3DX_R8G8B8A8_SNORM_to_FLOAT4(UINT packedInput)
//      UINT     D3DX_FLOAT4_to_R8G8B8A8_SNORM(hlsl_precise XMFLOAT4 unpackedInput)
//
// DXGI_FORMAT_R8G8B8A8_SINT:
//
//      XMINT4 D3DX_R8G8B8A8_SINT_to_INT4(UINT packedInput)
//      UINT   D3DX_INT4_to_R8G8B8A8_SINT(XMINT4 unpackedInput)
//
// DXGI_FORMAT_B8G8R8A8_UNORM:
//
//      XMFLOAT4 D3DX_B8G8R8A8_UNORM_to_FLOAT4(UINT packedInput)
//      UINT     D3DX_FLOAT4_to_B8G8R8A8_UNORM(hlsl_precise XMFLOAT4 unpackedInput)
//
// DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
//
//      XMFLOAT4 D3DX_B8G8R8A8_UNORM_SRGB_to_FLOAT4_inexact(UINT packedInput) *
//      XMFLOAT4 D3DX_B8G8R8A8_UNORM_SRGB_to_FLOAT4(UINT packedInput)
//      UINT     D3DX_FLOAT4_to_R8G8B8A8_UNORM_SRGB(hlsl_precise XMFLOAT4 unpackedInput)
//
//      * The "_inexact" function above uses shader instructions that don't
//      have high enough precision to give the exact answer, albeit close.
//      The alternative function uses a lookup table stored in the shader
//      to give an exact SRGB->float conversion.
//
// DXGI_FORMAT_B8G8R8X8_UNORM:
//
//      XMFLOAT3 D3DX_B8G8R8X8_UNORM_to_FLOAT3(UINT packedInput)
//      UINT     D3DX_FLOAT3_to_B8G8R8X8_UNORM(hlsl_precise XMFLOAT3 unpackedInput)
//
// DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
//
//      XMFLOAT3 D3DX_B8G8R8X8_UNORM_SRGB_to_FLOAT3_inexact(UINT packedInput) *
//      XMFLOAT3 D3DX_B8G8R8X8_UNORM_SRGB_to_FLOAT3(UINT packedInput)
//      UINT     D3DX_FLOAT3_to_B8G8R8X8_UNORM_SRGB(hlsl_precise XMFLOAT3 unpackedInput)
//
//      * The "_inexact" function above uses shader instructions that don't
//      have high enough precision to give the exact answer, albeit close.
//      The alternative function uses a lookup table stored in the shader
//      to give an exact SRGB->float conversion.
//
// DXGI_FORMAT_R16G16_FLOAT:
//
//      XMFLOAT2 D3DX_R16G16_FLOAT_to_FLOAT2(UINT packedInput)
//      UINT     D3DX_FLOAT2_to_R16G16_FLOAT(hlsl_precise XMFLOAT2 unpackedInput)
//
// DXGI_FORMAT_R16G16_UNORM:
//
//      XMFLOAT2 D3DX_R16G16_UNORM_to_FLOAT2(UINT packedInput)
//      UINT     D3DX_FLOAT2_to_R16G16_UNORM(hlsl_precise FLOAT2 unpackedInput)
//
// DXGI_FORMAT_R16G16_UINT:
//
//      XMUINT2 D3DX_R16G16_UINT_to_UINT2(UINT packedInput)
//      UINT    D3DX_UINT2_to_R16G16_UINT(XMUINT2 unpackedInput)
//
// DXGI_FORMAT_R16G16_SNORM:
//
//      XMFLOAT2 D3DX_R16G16_SNORM_to_FLOAT2(UINT packedInput)
//      UINT     D3DX_FLOAT2_to_R16G16_SNORM(hlsl_precise XMFLOAT2 unpackedInput)
//
// DXGI_FORMAT_R16G16_SINT:
//
//      XMINT2 D3DX_R16G16_SINT_to_INT2(UINT packedInput)
//      UINT   D3DX_INT2_to_R16G16_SINT(XMINT2 unpackedInput)
//
//=============================================================================

#ifndef __D3DX_DXGI_FORMAT_CONVERT_INL___
#define __D3DX_DXGI_FORMAT_CONVERT_INL___

#if HLSL_VERSION > 0

#define D3DX11INLINE

typedef int INT;
typedef uint UINT;

typedef float2 XMFLOAT2;
typedef float3 XMFLOAT3;
typedef float4 XMFLOAT4;
typedef int2 XMINT2;
typedef int4 XMINT4;
typedef uint2 XMUINT2;
typedef uint4 XMUINT4;

#define hlsl_precise precise

#define D3DX_Saturate_FLOAT(_V) saturate(_V)
#define D3DX_IsNan(_V) isnan(_V)
#define D3DX_Truncate_FLOAT(_V) trunc(_V)

#else // HLSL_VERSION > 0

#ifndef __cplusplus
#error C++ compilation required
#endif

#include <float.h>
#include <xnamath.h>

#define hlsl_precise

D3DX11INLINE FLOAT D3DX_Saturate_FLOAT(FLOAT _V)
{
    return min(max(_V, 0), 1);
}
D3DX11INLINE bool D3DX_IsNan(FLOAT _V)
{
    return _V != _V;
}
D3DX11INLINE FLOAT D3DX_Truncate_FLOAT(FLOAT _V)
{
    return _V >= 0 ? floor(_V) : ceil(_V);
}

// 2D Vector; 32 bit signed integer components
typedef struct _XMINT2
{
    INT x;
    INT y;
} XMINT2;

// 2D Vector; 32 bit unsigned integer components
typedef struct _XMUINT2
{
    UINT x;
    UINT y;
} XMUINT2;

// 4D Vector; 32 bit signed integer components
typedef struct _XMINT4
{
    INT x;
    INT y;
    INT z;
    INT w;
} XMINT4;

// 4D Vector; 32 bit unsigned integer components
typedef struct _XMUINT4
{
    UINT x;
    UINT y;
    UINT z;
    UINT w;
} XMUINT4;

#endif // HLSL_VERSION > 0

//=============================================================================
// SRGB Helper Functions Called By Conversions Further Below.
//=============================================================================
// SRGB_to_FLOAT_inexact is imprecise due to precision of pow implementations.
// If exact SRGB->float conversion is needed, a table lookup is provided
// further below.
D3DX11INLINE FLOAT D3DX_SRGB_to_FLOAT_inexact(hlsl_precise FLOAT val)
{
    if( val < 0.04045f )
        val /= 12.92f;
    else
        val = pow((val + 0.055f)/1.055f,2.4f);
    return val;
}

static const UINT D3DX_SRGBTable[] =
{
    0x00000000,0x399f22b4,0x3a1f22b4,0x3a6eb40e,0x3a9f22b4,0x3ac6eb61,0x3aeeb40e,0x3b0b3e5d,
    0x3b1f22b4,0x3b33070b,0x3b46eb61,0x3b5b518d,0x3b70f18d,0x3b83e1c6,0x3b8fe616,0x3b9c87fd,
    0x3ba9c9b7,0x3bb7ad6f,0x3bc63549,0x3bd56361,0x3be539c1,0x3bf5ba70,0x3c0373b5,0x3c0c6152,
    0x3c15a703,0x3c1f45be,0x3c293e6b,0x3c3391f7,0x3c3e4149,0x3c494d43,0x3c54b6c7,0x3c607eb1,
    0x3c6ca5df,0x3c792d22,0x3c830aa8,0x3c89af9f,0x3c9085db,0x3c978dc5,0x3c9ec7c2,0x3ca63433,
    0x3cadd37d,0x3cb5a601,0x3cbdac20,0x3cc5e639,0x3cce54ab,0x3cd6f7d5,0x3cdfd010,0x3ce8ddb9,
    0x3cf2212c,0x3cfb9ac1,0x3d02a569,0x3d0798dc,0x3d0ca7e6,0x3d11d2af,0x3d171963,0x3d1c7c2e,
    0x3d21fb3c,0x3d2796b2,0x3d2d4ebb,0x3d332380,0x3d39152b,0x3d3f23e3,0x3d454fd1,0x3d4b991c,
    0x3d51ffef,0x3d58846a,0x3d5f26b7,0x3d65e6fe,0x3d6cc564,0x3d73c20f,0x3d7add29,0x3d810b67,
    0x3d84b795,0x3d887330,0x3d8c3e4a,0x3d9018f6,0x3d940345,0x3d97fd4a,0x3d9c0716,0x3da020bb,
    0x3da44a4b,0x3da883d7,0x3daccd70,0x3db12728,0x3db59112,0x3dba0b3b,0x3dbe95b5,0x3dc33092,
    0x3dc7dbe2,0x3dcc97b6,0x3dd1641f,0x3dd6412c,0x3ddb2eef,0x3de02d77,0x3de53cd5,0x3dea5d19,
    0x3def8e52,0x3df4d091,0x3dfa23e8,0x3dff8861,0x3e027f07,0x3e054280,0x3e080ea3,0x3e0ae378,
    0x3e0dc105,0x3e10a754,0x3e13966b,0x3e168e52,0x3e198f10,0x3e1c98ad,0x3e1fab30,0x3e22c6a3,
    0x3e25eb09,0x3e29186c,0x3e2c4ed0,0x3e2f8e41,0x3e32d6c4,0x3e362861,0x3e39831e,0x3e3ce703,
    0x3e405416,0x3e43ca5f,0x3e4749e4,0x3e4ad2ae,0x3e4e64c2,0x3e520027,0x3e55a4e6,0x3e595303,
    0x3e5d0a8b,0x3e60cb7c,0x3e6495e0,0x3e6869bf,0x3e6c4720,0x3e702e0c,0x3e741e84,0x3e781890,
    0x3e7c1c38,0x3e8014c2,0x3e82203c,0x3e84308d,0x3e8645ba,0x3e885fc5,0x3e8a7eb2,0x3e8ca283,
    0x3e8ecb3d,0x3e90f8e1,0x3e932b74,0x3e9562f8,0x3e979f71,0x3e99e0e2,0x3e9c274e,0x3e9e72b7,
    0x3ea0c322,0x3ea31892,0x3ea57308,0x3ea7d289,0x3eaa3718,0x3eaca0b7,0x3eaf0f69,0x3eb18333,
    0x3eb3fc18,0x3eb67a18,0x3eb8fd37,0x3ebb8579,0x3ebe12e1,0x3ec0a571,0x3ec33d2d,0x3ec5da17,
    0x3ec87c33,0x3ecb2383,0x3ecdd00b,0x3ed081cd,0x3ed338cc,0x3ed5f50b,0x3ed8b68d,0x3edb7d54,
    0x3ede4965,0x3ee11ac1,0x3ee3f16b,0x3ee6cd67,0x3ee9aeb6,0x3eec955d,0x3eef815d,0x3ef272ba,
    0x3ef56976,0x3ef86594,0x3efb6717,0x3efe6e02,0x3f00bd2d,0x3f02460e,0x3f03d1a7,0x3f055ff9,
    0x3f06f106,0x3f0884cf,0x3f0a1b56,0x3f0bb49b,0x3f0d50a0,0x3f0eef67,0x3f1090f1,0x3f12353e,
    0x3f13dc51,0x3f15862b,0x3f1732cd,0x3f18e239,0x3f1a946f,0x3f1c4971,0x3f1e0141,0x3f1fbbdf,
    0x3f21794e,0x3f23398e,0x3f24fca0,0x3f26c286,0x3f288b41,0x3f2a56d3,0x3f2c253d,0x3f2df680,
    0x3f2fca9e,0x3f31a197,0x3f337b6c,0x3f355820,0x3f3737b3,0x3f391a26,0x3f3aff7c,0x3f3ce7b5,
    0x3f3ed2d2,0x3f40c0d4,0x3f42b1be,0x3f44a590,0x3f469c4b,0x3f4895f1,0x3f4a9282,0x3f4c9201,
    0x3f4e946e,0x3f5099cb,0x3f52a218,0x3f54ad57,0x3f56bb8a,0x3f58ccb0,0x3f5ae0cd,0x3f5cf7e0,
    0x3f5f11ec,0x3f612eee,0x3f634eef,0x3f6571e9,0x3f6797e3,0x3f69c0d6,0x3f6beccd,0x3f6e1bbf,
    0x3f704db8,0x3f7282af,0x3f74baae,0x3f76f5ae,0x3f7933b9,0x3f7b74c6,0x3f7db8e0,0x3f800000
};

D3DX11INLINE FLOAT D3DX_SRGB_to_FLOAT(UINT val)
{
#if HLSL_VERSION > 0
    return asfloat(D3DX_SRGBTable[val]);
#else
    return *(FLOAT*)&D3DX_SRGBTable[val];
#endif
}

D3DX11INLINE FLOAT D3DX_FLOAT_to_SRGB(hlsl_precise FLOAT val)
{
    if( val < 0.0031308f )
        val *= 12.92f;
    else
        val = 1.055f * pow(val,1.0f/2.4f) - 0.055f;
    return val;
}

D3DX11INLINE FLOAT D3DX_SaturateSigned_FLOAT(FLOAT _V)
{
    if (D3DX_IsNan(_V))
    {
        return 0;
    }

    return min(max(_V, -1), 1);
}

D3DX11INLINE UINT D3DX_FLOAT_to_UINT(FLOAT _V,
                                     FLOAT _Scale)
{
    return (UINT)floor(_V * _Scale + 0.5f);
}

D3DX11INLINE FLOAT D3DX_INT_to_FLOAT(INT _V,
                                     FLOAT _Scale)
{
    FLOAT Scaled = (FLOAT)_V / _Scale;
    // The integer is a two's-complement signed
    // number so the negative range is slightly
    // larger than the positive range, meaning
    // the scaled value can be slight less than -1.
    // Clamp to keep the float range [-1, 1].
    return max(Scaled, -1.0f);
}

D3DX11INLINE INT D3DX_FLOAT_to_INT(FLOAT _V,
                                   FLOAT _Scale)
{
    return (INT)D3DX_Truncate_FLOAT(_V * _Scale + (_V >= 0 ? 0.5f : -0.5f));
}

//=============================================================================
// Conversion routines
//=============================================================================
//-----------------------------------------------------------------------------
// R10B10G10A2_UNORM <-> FLOAT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT4 D3DX_R10G10B10A2_UNORM_to_FLOAT4(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    unpackedOutput.x = (FLOAT)  (packedInput      & 0x000003ff)  / 1023;
    unpackedOutput.y = (FLOAT)(((packedInput>>10) & 0x000003ff)) / 1023;
    unpackedOutput.z = (FLOAT)(((packedInput>>20) & 0x000003ff)) / 1023;
    unpackedOutput.w = (FLOAT)(((packedInput>>30) & 0x00000003)) / 3;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT4_to_R10G10B10A2_UNORM(hlsl_precise XMFLOAT4 unpackedInput)
{
    UINT packedOutput;
    packedOutput = ( (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.x), 1023))     |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.y), 1023)<<10) |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.z), 1023)<<20) |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.w), 3)<<30) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R10B10G10A2_UINT <-> UINT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMUINT4 D3DX_R10G10B10A2_UINT_to_UINT4(UINT packedInput)
{
    XMUINT4 unpackedOutput;
    unpackedOutput.x = packedInput & 0x000003ff;
    unpackedOutput.y = (packedInput>>10) & 0x000003ff;
    unpackedOutput.z = (packedInput>>20) & 0x000003ff;
    unpackedOutput.w = (packedInput>>30) & 0x00000003;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_UINT4_to_R10G10B10A2_UINT(XMUINT4 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.x = min(unpackedInput.x, 0x000003ff);
    unpackedInput.y = min(unpackedInput.y, 0x000003ff);
    unpackedInput.z = min(unpackedInput.z, 0x000003ff);
    unpackedInput.w = min(unpackedInput.w, 0x00000003);
    packedOutput = ( (unpackedInput.x)      |
                    ((unpackedInput.y)<<10) |
                    ((unpackedInput.z)<<20) |
                    ((unpackedInput.w)<<30) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R8G8B8A8_UNORM <-> FLOAT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT4 D3DX_R8G8B8A8_UNORM_to_FLOAT4(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    unpackedOutput.x = (FLOAT)  (packedInput      & 0x000000ff)  / 255;
    unpackedOutput.y = (FLOAT)(((packedInput>> 8) & 0x000000ff)) / 255;
    unpackedOutput.z = (FLOAT)(((packedInput>>16) & 0x000000ff)) / 255;
    unpackedOutput.w = (FLOAT)  (packedInput>>24)                / 255;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT4_to_R8G8B8A8_UNORM(hlsl_precise XMFLOAT4 unpackedInput)
{
    UINT packedOutput;
    packedOutput = ( (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.x), 255))     |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.y), 255)<< 8) |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.z), 255)<<16) |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.w), 255)<<24) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R8G8B8A8_UNORM_SRGB <-> FLOAT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT4 D3DX_R8G8B8A8_UNORM_SRGB_to_FLOAT4_inexact(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    unpackedOutput.x = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)  (packedInput      & 0x000000ff) )/255);
    unpackedOutput.y = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)(((packedInput>> 8) & 0x000000ff)))/255);
    unpackedOutput.z = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)(((packedInput>>16) & 0x000000ff)))/255);
    unpackedOutput.w = (FLOAT)(packedInput>>24) / 255;
    return unpackedOutput;
}

D3DX11INLINE XMFLOAT4 D3DX_R8G8B8A8_UNORM_SRGB_to_FLOAT4(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    unpackedOutput.x = D3DX_SRGB_to_FLOAT(  (packedInput      & 0x000000ff) );
    unpackedOutput.y = D3DX_SRGB_to_FLOAT((((packedInput>> 8) & 0x000000ff)));
    unpackedOutput.z = D3DX_SRGB_to_FLOAT((((packedInput>>16) & 0x000000ff)));
    unpackedOutput.w = (FLOAT)(packedInput>>24) / 255;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT4_to_R8G8B8A8_UNORM_SRGB(hlsl_precise XMFLOAT4 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.x = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.x));
    unpackedInput.y = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.y));
    unpackedInput.z = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.z));
    unpackedInput.w = D3DX_Saturate_FLOAT(unpackedInput.w);
    packedOutput = ( (D3DX_FLOAT_to_UINT(unpackedInput.x, 255))     |
                     (D3DX_FLOAT_to_UINT(unpackedInput.y, 255)<< 8) |
                     (D3DX_FLOAT_to_UINT(unpackedInput.z, 255)<<16) |
                     (D3DX_FLOAT_to_UINT(unpackedInput.w, 255)<<24) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R8G8B8A8_UINT <-> UINT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMUINT4 D3DX_R8G8B8A8_UINT_to_UINT4(UINT packedInput)
{
    XMUINT4 unpackedOutput;
    unpackedOutput.x =  packedInput      & 0x000000ff;
    unpackedOutput.y = (packedInput>> 8) & 0x000000ff;
    unpackedOutput.z = (packedInput>>16) & 0x000000ff;
    unpackedOutput.w =  packedInput>>24;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_UINT4_to_R8G8B8A8_UINT(XMUINT4 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.x = min(unpackedInput.x, 0x000000ff);
    unpackedInput.y = min(unpackedInput.y, 0x000000ff);
    unpackedInput.z = min(unpackedInput.z, 0x000000ff);
    unpackedInput.w = min(unpackedInput.w, 0x000000ff);
    packedOutput = ( unpackedInput.x      |
                    (unpackedInput.y<< 8) |
                    (unpackedInput.z<<16) |
                    (unpackedInput.w<<24) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R8G8B8A8_SNORM <-> FLOAT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT4 D3DX_R8G8B8A8_SNORM_to_FLOAT4(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    XMINT4 signExtendedBits;
    signExtendedBits.x =  (INT)(packedInput << 24) >> 24;
    signExtendedBits.y = (INT)((packedInput << 16) & 0xff000000) >> 24;
    signExtendedBits.z = (INT)((packedInput <<  8) & 0xff000000) >> 24;
    signExtendedBits.w =  (INT)(packedInput & 0xff000000) >> 24;
    unpackedOutput.x = D3DX_INT_to_FLOAT(signExtendedBits.x, 127);
    unpackedOutput.y = D3DX_INT_to_FLOAT(signExtendedBits.y, 127);
    unpackedOutput.z = D3DX_INT_to_FLOAT(signExtendedBits.z, 127);
    unpackedOutput.w = D3DX_INT_to_FLOAT(signExtendedBits.w, 127);
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT4_to_R8G8B8A8_SNORM(hlsl_precise XMFLOAT4 unpackedInput)
{
    UINT packedOutput;
    packedOutput = ( (D3DX_FLOAT_to_INT(D3DX_SaturateSigned_FLOAT(unpackedInput.x), 127) & 0x000000ff)      |
                    ((D3DX_FLOAT_to_INT(D3DX_SaturateSigned_FLOAT(unpackedInput.y), 127) & 0x000000ff)<< 8) |
                    ((D3DX_FLOAT_to_INT(D3DX_SaturateSigned_FLOAT(unpackedInput.z), 127) & 0x000000ff)<<16) |
                    ((D3DX_FLOAT_to_INT(D3DX_SaturateSigned_FLOAT(unpackedInput.w), 127))             <<24) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R8G8B8A8_SINT <-> INT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMINT4 D3DX_R8G8B8A8_SINT_to_INT4(UINT packedInput)
{
    XMINT4 unpackedOutput;
    unpackedOutput.x =  (INT)(packedInput << 24) >> 24;
    unpackedOutput.y = (INT)((packedInput << 16) & 0xff000000) >> 24;
    unpackedOutput.z = (INT)((packedInput <<  8) & 0xff000000) >> 24;
    unpackedOutput.w =  (INT)(packedInput & 0xff000000) >> 24;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_INT4_to_R8G8B8A8_SINT(XMINT4 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.x = max(min(unpackedInput.x,127),-128);
    unpackedInput.y = max(min(unpackedInput.y,127),-128);
    unpackedInput.z = max(min(unpackedInput.z,127),-128);
    unpackedInput.w = max(min(unpackedInput.w,127),-128);
    packedOutput = ( (unpackedInput.x & 0x000000ff)      |
                    ((unpackedInput.y & 0x000000ff)<< 8) |
                    ((unpackedInput.z & 0x000000ff)<<16) |
                     (unpackedInput.w              <<24) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// B8G8R8A8_UNORM <-> FLOAT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT4 D3DX_B8G8R8A8_UNORM_to_FLOAT4(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    unpackedOutput.z = (FLOAT)  (packedInput      & 0x000000ff)  / 255;
    unpackedOutput.y = (FLOAT)(((packedInput>> 8) & 0x000000ff)) / 255;
    unpackedOutput.x = (FLOAT)(((packedInput>>16) & 0x000000ff)) / 255;
    unpackedOutput.w = (FLOAT)  (packedInput>>24)                / 255;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT4_to_B8G8R8A8_UNORM(hlsl_precise XMFLOAT4 unpackedInput)
{
    UINT packedOutput;
    packedOutput = ( (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.z), 255))     |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.y), 255)<< 8) |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.x), 255)<<16) |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.w), 255)<<24) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// B8G8R8A8_UNORM_SRGB <-> FLOAT4
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT4 D3DX_B8G8R8A8_UNORM_SRGB_to_FLOAT4_inexact(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    unpackedOutput.z = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)  (packedInput      & 0x000000ff) )/255);
    unpackedOutput.y = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)(((packedInput>> 8) & 0x000000ff)))/255);
    unpackedOutput.x = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)(((packedInput>>16) & 0x000000ff)))/255);
    unpackedOutput.w = (FLOAT)(packedInput>>24) / 255;
    return unpackedOutput;
}

D3DX11INLINE XMFLOAT4 D3DX_B8G8R8A8_UNORM_SRGB_to_FLOAT4(UINT packedInput)
{
    hlsl_precise XMFLOAT4 unpackedOutput;
    unpackedOutput.z = D3DX_SRGB_to_FLOAT(  (packedInput      & 0x000000ff) );
    unpackedOutput.y = D3DX_SRGB_to_FLOAT((((packedInput>> 8) & 0x000000ff)));
    unpackedOutput.x = D3DX_SRGB_to_FLOAT((((packedInput>>16) & 0x000000ff)));
    unpackedOutput.w = (FLOAT)(packedInput>>24) / 255;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT4_to_B8G8R8A8_UNORM_SRGB(hlsl_precise XMFLOAT4 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.z = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.z));
    unpackedInput.y = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.y));
    unpackedInput.x = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.x));
    unpackedInput.w = D3DX_Saturate_FLOAT(unpackedInput.w);
    packedOutput = ( (D3DX_FLOAT_to_UINT(unpackedInput.z, 255))     |
                     (D3DX_FLOAT_to_UINT(unpackedInput.y, 255)<< 8) |
                     (D3DX_FLOAT_to_UINT(unpackedInput.x, 255)<<16) |
                     (D3DX_FLOAT_to_UINT(unpackedInput.w, 255)<<24) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// B8G8R8X8_UNORM <-> FLOAT3
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT3 D3DX_B8G8R8X8_UNORM_to_FLOAT3(UINT packedInput)
{
    hlsl_precise XMFLOAT3 unpackedOutput;
    unpackedOutput.z = (FLOAT)  (packedInput      & 0x000000ff)  / 255;
    unpackedOutput.y = (FLOAT)(((packedInput>> 8) & 0x000000ff)) / 255;
    unpackedOutput.x = (FLOAT)(((packedInput>>16) & 0x000000ff)) / 255;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT3_to_B8G8R8X8_UNORM(hlsl_precise XMFLOAT3 unpackedInput)
{
    UINT packedOutput;
    packedOutput = ( (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.z), 255))     |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.y), 255)<< 8) |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.x), 255)<<16) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// B8G8R8X8_UNORM_SRGB <-> FLOAT3
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT3 D3DX_B8G8R8X8_UNORM_SRGB_to_FLOAT3_inexact(UINT packedInput)
{
    hlsl_precise XMFLOAT3 unpackedOutput;
    unpackedOutput.z = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)  (packedInput      & 0x000000ff) )/255);
    unpackedOutput.y = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)(((packedInput>> 8) & 0x000000ff)))/255);
    unpackedOutput.x = D3DX_SRGB_to_FLOAT_inexact(((FLOAT)(((packedInput>>16) & 0x000000ff)))/255);
    return unpackedOutput;
}

D3DX11INLINE XMFLOAT3 D3DX_B8G8R8X8_UNORM_SRGB_to_FLOAT3(UINT packedInput)
{
    hlsl_precise XMFLOAT3 unpackedOutput;
    unpackedOutput.z = D3DX_SRGB_to_FLOAT(  (packedInput      & 0x000000ff) );
    unpackedOutput.y = D3DX_SRGB_to_FLOAT((((packedInput>> 8) & 0x000000ff)));
    unpackedOutput.x = D3DX_SRGB_to_FLOAT((((packedInput>>16) & 0x000000ff)));
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT3_to_B8G8R8X8_UNORM_SRGB(hlsl_precise XMFLOAT3 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.z = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.z));
    unpackedInput.y = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.y));
    unpackedInput.x = D3DX_FLOAT_to_SRGB(D3DX_Saturate_FLOAT(unpackedInput.x));
    packedOutput = ( (D3DX_FLOAT_to_UINT(unpackedInput.z, 255))     |
                     (D3DX_FLOAT_to_UINT(unpackedInput.y, 255)<< 8) |
                     (D3DX_FLOAT_to_UINT(unpackedInput.x, 255)<<16) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R16G16_FLOAT <-> FLOAT2
//-----------------------------------------------------------------------------

#if HLSL_VERSION > 0

D3DX11INLINE XMFLOAT2 D3DX_R16G16_FLOAT_to_FLOAT2(UINT packedInput)
{
    hlsl_precise XMFLOAT2 unpackedOutput;
    unpackedOutput.x = f16tof32(packedInput&0x0000ffff);
    unpackedOutput.y = f16tof32(packedInput>>16);
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT2_to_R16G16_FLOAT(hlsl_precise XMFLOAT2 unpackedInput)
{
    UINT packedOutput;
    packedOutput = asuint(f32tof16(unpackedInput.x)) |
                   (asuint(f32tof16(unpackedInput.y)) << 16);
    return packedOutput;
}

#endif // HLSL_VERSION > 0

//-----------------------------------------------------------------------------
// R16G16_UNORM <-> FLOAT2
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT2 D3DX_R16G16_UNORM_to_FLOAT2(UINT packedInput)
{
    hlsl_precise XMFLOAT2 unpackedOutput;
    unpackedOutput.x = (FLOAT)  (packedInput      & 0x0000ffff)  / 65535;
    unpackedOutput.y = (FLOAT)  (packedInput>>16)                / 65535;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT2_to_R16G16_UNORM(hlsl_precise XMFLOAT2 unpackedInput)
{
    UINT packedOutput;
    packedOutput = ( (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.x), 65535))     |
                     (D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpackedInput.y), 65535)<< 16) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R16G16_UINT <-> UINT2
//-----------------------------------------------------------------------------
D3DX11INLINE XMUINT2 D3DX_R16G16_UINT_to_UINT2(UINT packedInput)
{
    XMUINT2 unpackedOutput;
    unpackedOutput.x =  packedInput      & 0x0000ffff;
    unpackedOutput.y =  packedInput>>16;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_UINT2_to_R16G16_UINT(XMUINT2 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.x = min(unpackedInput.x,0x0000ffff);
    unpackedInput.y = min(unpackedInput.y,0x0000ffff);
    packedOutput = ( unpackedInput.x      |
                    (unpackedInput.y<<16) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R16G16_SNORM <-> FLOAT2
//-----------------------------------------------------------------------------
D3DX11INLINE XMFLOAT2 D3DX_R16G16_SNORM_to_FLOAT2(UINT packedInput)
{
    hlsl_precise XMFLOAT2 unpackedOutput;
    XMINT2 signExtendedBits;
    signExtendedBits.x =  (INT)(packedInput << 16) >> 16;
    signExtendedBits.y =  (INT)(packedInput & 0xffff0000) >> 16;
    unpackedOutput.x = D3DX_INT_to_FLOAT(signExtendedBits.x, 32767);
    unpackedOutput.y = D3DX_INT_to_FLOAT(signExtendedBits.y, 32767);
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_FLOAT2_to_R16G16_SNORM(hlsl_precise XMFLOAT2 unpackedInput)
{
    UINT packedOutput;
    packedOutput = ( (D3DX_FLOAT_to_INT(D3DX_SaturateSigned_FLOAT(unpackedInput.x), 32767) & 0x0000ffff)      |
                     (D3DX_FLOAT_to_INT(D3DX_SaturateSigned_FLOAT(unpackedInput.y), 32767)              <<16) );
    return packedOutput;
}

//-----------------------------------------------------------------------------
// R16G16_SINT <-> INT2
//-----------------------------------------------------------------------------
D3DX11INLINE XMINT2 D3DX_R16G16_SINT_to_INT2(UINT packedInput)
{
    XMINT2 unpackedOutput;
    unpackedOutput.x = (INT)(packedInput << 16) >> 16;
    unpackedOutput.y = (INT)(packedInput & 0xffff0000) >> 16;
    return unpackedOutput;
}

D3DX11INLINE UINT D3DX_INT2_to_R16G16_SINT(XMINT2 unpackedInput)
{
    UINT packedOutput;
    unpackedInput.x = max(min(unpackedInput.x,32767),-32768);
    unpackedInput.y = max(min(unpackedInput.y,32767),-32768);
    packedOutput = ( (unpackedInput.x & 0x0000ffff)      |
                     (unpackedInput.y              <<16) );
    return packedOutput;
}

#endif // __D3DX_DXGI_FORMAT_CONVERT_INL___
