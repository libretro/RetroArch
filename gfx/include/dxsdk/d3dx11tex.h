//////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) Microsoft Corporation.  All Rights Reserved.
//
//  File:       d3dx11tex.h
//  Content:    D3DX11 texturing APIs
//
//////////////////////////////////////////////////////////////////////////////

#include "d3dx11.h"

#ifndef __D3DX11TEX_H__
#define __D3DX11TEX_H__


//----------------------------------------------------------------------------
// D3DX11_FILTER flags:
// ------------------
//
// A valid filter must contain one of these values:
//
//  D3DX11_FILTER_NONE
//      No scaling or filtering will take place.  Pixels outside the bounds
//      of the source image are assumed to be transparent black.
//  D3DX11_FILTER_POINT
//      Each destination pixel is computed by sampling the nearest pixel
//      from the source image.
//  D3DX11_FILTER_LINEAR
//      Each destination pixel is computed by linearly interpolating between
//      the nearest pixels in the source image.  This filter works best 
//      when the scale on each axis is less than 2.
//  D3DX11_FILTER_TRIANGLE
//      Every pixel in the source image contributes equally to the
//      destination image.  This is the slowest of all the filters.
//  D3DX11_FILTER_BOX
//      Each pixel is computed by averaging a 2x2(x2) box pixels from 
//      the source image. Only works when the dimensions of the 
//      destination are half those of the source. (as with mip maps)
//
// And can be OR'd with any of these optional flags:
//
//  D3DX11_FILTER_MIRROR_U
//      Indicates that pixels off the edge of the texture on the U-axis
//      should be mirrored, not wraped.
//  D3DX11_FILTER_MIRROR_V
//      Indicates that pixels off the edge of the texture on the V-axis
//      should be mirrored, not wraped.
//  D3DX11_FILTER_MIRROR_W
//      Indicates that pixels off the edge of the texture on the W-axis
//      should be mirrored, not wraped.
//  D3DX11_FILTER_MIRROR
//      Same as specifying D3DX11_FILTER_MIRROR_U | D3DX11_FILTER_MIRROR_V |
//      D3DX11_FILTER_MIRROR_V
//  D3DX11_FILTER_DITHER
//      Dithers the resulting image using a 4x4 order dither pattern.
//  D3DX11_FILTER_SRGB_IN
//      Denotes that the input data is in sRGB (gamma 2.2) colorspace.
//  D3DX11_FILTER_SRGB_OUT
//      Denotes that the output data is in sRGB (gamma 2.2) colorspace.
//  D3DX11_FILTER_SRGB
//      Same as specifying D3DX11_FILTER_SRGB_IN | D3DX11_FILTER_SRGB_OUT
//
//----------------------------------------------------------------------------

typedef enum D3DX11_FILTER_FLAG
{
    D3DX11_FILTER_NONE            =   (1 << 0),
    D3DX11_FILTER_POINT           =   (2 << 0),
    D3DX11_FILTER_LINEAR          =   (3 << 0),
    D3DX11_FILTER_TRIANGLE        =   (4 << 0),
    D3DX11_FILTER_BOX             =   (5 << 0),

    D3DX11_FILTER_MIRROR_U        =   (1 << 16),
    D3DX11_FILTER_MIRROR_V        =   (2 << 16),
    D3DX11_FILTER_MIRROR_W        =   (4 << 16),
    D3DX11_FILTER_MIRROR          =   (7 << 16),

    D3DX11_FILTER_DITHER          =   (1 << 19),
    D3DX11_FILTER_DITHER_DIFFUSION=   (2 << 19),

    D3DX11_FILTER_SRGB_IN         =   (1 << 21),
    D3DX11_FILTER_SRGB_OUT        =   (2 << 21),
    D3DX11_FILTER_SRGB            =   (3 << 21),
} D3DX11_FILTER_FLAG;

//----------------------------------------------------------------------------
// D3DX11_NORMALMAP flags:
// ---------------------
// These flags are used to control how D3DX11ComputeNormalMap generates normal
// maps.  Any number of these flags may be OR'd together in any combination.
//
//  D3DX11_NORMALMAP_MIRROR_U
//      Indicates that pixels off the edge of the texture on the U-axis
//      should be mirrored, not wraped.
//  D3DX11_NORMALMAP_MIRROR_V
//      Indicates that pixels off the edge of the texture on the V-axis
//      should be mirrored, not wraped.
//  D3DX11_NORMALMAP_MIRROR
//      Same as specifying D3DX11_NORMALMAP_MIRROR_U | D3DX11_NORMALMAP_MIRROR_V
//  D3DX11_NORMALMAP_INVERTSIGN
//      Inverts the direction of each normal 
//  D3DX11_NORMALMAP_COMPUTE_OCCLUSION
//      Compute the per pixel Occlusion term and encodes it into the alpha.
//      An Alpha of 1 means that the pixel is not obscured in anyway, and
//      an alpha of 0 would mean that the pixel is completly obscured.
//
//----------------------------------------------------------------------------

typedef enum D3DX11_NORMALMAP_FLAG
{
    D3DX11_NORMALMAP_MIRROR_U          =   (1 << 16),
    D3DX11_NORMALMAP_MIRROR_V          =   (2 << 16),
    D3DX11_NORMALMAP_MIRROR            =   (3 << 16),
    D3DX11_NORMALMAP_INVERTSIGN        =   (8 << 16),
    D3DX11_NORMALMAP_COMPUTE_OCCLUSION =   (16 << 16),
} D3DX11_NORMALMAP_FLAG;

//----------------------------------------------------------------------------
// D3DX11_CHANNEL flags:
// -------------------
// These flags are used by functions which operate on or more channels
// in a texture.
//
// D3DX11_CHANNEL_RED
//     Indicates the red channel should be used
// D3DX11_CHANNEL_BLUE
//     Indicates the blue channel should be used
// D3DX11_CHANNEL_GREEN
//     Indicates the green channel should be used
// D3DX11_CHANNEL_ALPHA
//     Indicates the alpha channel should be used
// D3DX11_CHANNEL_LUMINANCE
//     Indicates the luminaces of the red green and blue channels should be 
//     used.
//
//----------------------------------------------------------------------------

typedef enum D3DX11_CHANNEL_FLAG
{
    D3DX11_CHANNEL_RED           =    (1 << 0),
    D3DX11_CHANNEL_BLUE          =    (1 << 1),
    D3DX11_CHANNEL_GREEN         =    (1 << 2),
    D3DX11_CHANNEL_ALPHA         =    (1 << 3),
    D3DX11_CHANNEL_LUMINANCE     =    (1 << 4),
} D3DX11_CHANNEL_FLAG;



//----------------------------------------------------------------------------
// D3DX11_IMAGE_FILE_FORMAT:
// ---------------------
// This enum is used to describe supported image file formats.
//
//----------------------------------------------------------------------------

typedef enum D3DX11_IMAGE_FILE_FORMAT
{
    D3DX11_IFF_BMP         = 0,
    D3DX11_IFF_JPG         = 1,
    D3DX11_IFF_PNG         = 3,
    D3DX11_IFF_DDS         = 4,
    D3DX11_IFF_TIFF		  = 10,
    D3DX11_IFF_GIF		  = 11,
    D3DX11_IFF_WMP		  = 12,
    D3DX11_IFF_FORCE_DWORD = 0x7fffffff

} D3DX11_IMAGE_FILE_FORMAT;


//----------------------------------------------------------------------------
// D3DX11_SAVE_TEXTURE_FLAG:
// ---------------------
// This enum is used to support texture saving options.
//
//----------------------------------------------------------------------------

typedef enum D3DX11_SAVE_TEXTURE_FLAG
{
    D3DX11_STF_USEINPUTBLOB      = 0x0001,
} D3DX11_SAVE_TEXTURE_FLAG;


//----------------------------------------------------------------------------
// D3DX11_IMAGE_INFO:
// ---------------
// This structure is used to return a rough description of what the
// the original contents of an image file looked like.
// 
//  Width
//      Width of original image in pixels
//  Height
//      Height of original image in pixels
//  Depth
//      Depth of original image in pixels
//  ArraySize
//      Array size in textures
//  MipLevels
//      Number of mip levels in original image
//  MiscFlags
//      Miscellaneous flags
//  Format
//      D3D format which most closely describes the data in original image
//  ResourceDimension
//      D3D11_RESOURCE_DIMENSION representing the dimension of texture stored in the file.
//      D3D11_RESOURCE_DIMENSION_TEXTURE1D, 2D, 3D
//  ImageFileFormat
//      D3DX11_IMAGE_FILE_FORMAT representing the format of the image file.
//----------------------------------------------------------------------------

typedef struct D3DX11_IMAGE_INFO
{
    UINT                        Width;
    UINT                        Height;
    UINT                        Depth;
    UINT                        ArraySize;
    UINT                        MipLevels;
    UINT                        MiscFlags;
    DXGI_FORMAT                 Format;
    D3D11_RESOURCE_DIMENSION    ResourceDimension;
    D3DX11_IMAGE_FILE_FORMAT    ImageFileFormat;
} D3DX11_IMAGE_INFO;





#ifdef __cplusplus
extern "C" {
#endif //__cplusplus



//////////////////////////////////////////////////////////////////////////////
// Image File APIs ///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
// D3DX11_IMAGE_LOAD_INFO:
// ---------------
// This structure can be optionally passed in to texture loader APIs to 
// control how textures get loaded. Pass in D3DX11_DEFAULT for any of these
// to have D3DX automatically pick defaults based on the source file.
// 
//  Width
//      Rescale texture to Width texels wide
//  Height
//      Rescale texture to Height texels high
//  Depth
//      Rescale texture to Depth texels deep
//  FirstMipLevel
//      First mip level to load
//  MipLevels
//      Number of mip levels to load after the first level
//  Usage
//      D3D11_USAGE flag for the new texture
//  BindFlags
//      D3D11 Bind flags for the new texture
//  CpuAccessFlags
//      D3D11 CPU Access flags for the new texture
//  MiscFlags
//      Reserved. Must be 0
//  Format
//      Resample texture to the specified format
//  Filter
//      Filter the texture using the specified filter (only when resampling)
//  MipFilter
//      Filter the texture mip levels using the specified filter (only if 
//      generating mips)
//  pSrcInfo
//      (optional) pointer to a D3DX11_IMAGE_INFO structure that will get 
//      populated with source image information
//----------------------------------------------------------------------------


typedef struct D3DX11_IMAGE_LOAD_INFO
{
    UINT                       Width;
    UINT                       Height;
    UINT                       Depth;
    UINT                       FirstMipLevel;
    UINT                       MipLevels;
    D3D11_USAGE                Usage;
    UINT                       BindFlags;
    UINT                       CpuAccessFlags;
    UINT                       MiscFlags;
    DXGI_FORMAT                Format;
    UINT                       Filter;
    UINT                       MipFilter;
    D3DX11_IMAGE_INFO*         pSrcInfo;
    
#ifdef __cplusplus
    D3DX11_IMAGE_LOAD_INFO()
    {
        Width = D3DX11_DEFAULT;
        Height = D3DX11_DEFAULT;
        Depth = D3DX11_DEFAULT;
        FirstMipLevel = D3DX11_DEFAULT;
        MipLevels = D3DX11_DEFAULT;
        Usage = (D3D11_USAGE) D3DX11_DEFAULT;
        BindFlags = D3DX11_DEFAULT;
        CpuAccessFlags = D3DX11_DEFAULT;
        MiscFlags = D3DX11_DEFAULT;
        Format = DXGI_FORMAT_FROM_FILE;
        Filter = D3DX11_DEFAULT;
        MipFilter = D3DX11_DEFAULT;
        pSrcInfo = NULL;
    }  
#endif

} D3DX11_IMAGE_LOAD_INFO;

//-------------------------------------------------------------------------------
// GetImageInfoFromFile/Resource/Memory:
// ------------------------------
// Fills in a D3DX11_IMAGE_INFO struct with information about an image file.
//
// Parameters:
//  pSrcFile
//      File name of the source image.
//  pSrcModule
//      Module where resource is located, or NULL for module associated
//      with image the os used to create the current process.
//  pSrcResource
//      Resource name.
//  pSrcData
//      Pointer to file in memory.
//  SrcDataSize
//      Size in bytes of file in memory.
//  pPump
//      Optional pointer to a thread pump object to use.
//  pSrcInfo
//      Pointer to a D3DX11_IMAGE_INFO structure to be filled in with the 
//      description of the data in the source image file.
//  pHResult
//      Pointer to a memory location to receive the return value upon completion.
//      Maybe NULL if not needed.
//      If pPump != NULL, pHResult must be a valid memory location until the
//      the asynchronous execution completes.
//-------------------------------------------------------------------------------

HRESULT WINAPI
    D3DX11GetImageInfoFromFileA(
        LPCSTR                    pSrcFile,
        ID3DX11ThreadPump*        pPump,
        D3DX11_IMAGE_INFO*        pSrcInfo,
        HRESULT*                  pHResult);

HRESULT WINAPI
    D3DX11GetImageInfoFromFileW(
        LPCWSTR                   pSrcFile,
        ID3DX11ThreadPump*        pPump,
        D3DX11_IMAGE_INFO*        pSrcInfo,
        HRESULT*                  pHResult);

#ifdef UNICODE
#define D3DX11GetImageInfoFromFile D3DX11GetImageInfoFromFileW
#else
#define D3DX11GetImageInfoFromFile D3DX11GetImageInfoFromFileA
#endif


HRESULT WINAPI
    D3DX11GetImageInfoFromResourceA(
        HMODULE                   hSrcModule,
        LPCSTR                    pSrcResource,
        ID3DX11ThreadPump*        pPump,
        D3DX11_IMAGE_INFO*        pSrcInfo,
        HRESULT*                  pHResult);

HRESULT WINAPI
    D3DX11GetImageInfoFromResourceW(
        HMODULE                   hSrcModule,
        LPCWSTR                   pSrcResource,
        ID3DX11ThreadPump*        pPump,
        D3DX11_IMAGE_INFO*        pSrcInfo,
        HRESULT*                  pHResult);

#ifdef UNICODE
#define D3DX11GetImageInfoFromResource D3DX11GetImageInfoFromResourceW
#else
#define D3DX11GetImageInfoFromResource D3DX11GetImageInfoFromResourceA
#endif


HRESULT WINAPI
    D3DX11GetImageInfoFromMemory(
        LPCVOID                   pSrcData,
        SIZE_T                    SrcDataSize,
        ID3DX11ThreadPump*        pPump,
        D3DX11_IMAGE_INFO*        pSrcInfo,
        HRESULT*                  pHResult);


//////////////////////////////////////////////////////////////////////////////
// Create/Save Texture APIs //////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
// D3DX11CreateTextureFromFile/Resource/Memory:
// D3DX11CreateShaderResourceViewFromFile/Resource/Memory:
// -----------------------------------
// Create a texture object from a file or resource.
//
// Parameters:
//
//  pDevice
//      The D3D device with which the texture is going to be used.
//  pSrcFile
//      File name.
//  hSrcModule
//      Module handle. if NULL, current module will be used.
//  pSrcResource
//      Resource name in module
//  pvSrcData
//      Pointer to file in memory.
//  SrcDataSize
//      Size in bytes of file in memory.
//  pLoadInfo
//      Optional pointer to a D3DX11_IMAGE_LOAD_INFO structure that
//      contains additional loader parameters.
//  pPump
//      Optional pointer to a thread pump object to use.
//  ppTexture
//      [out] Created texture object.
//  ppShaderResourceView
//      [out] Shader resource view object created.
//  pHResult
//      Pointer to a memory location to receive the return value upon completion.
//      Maybe NULL if not needed.
//      If pPump != NULL, pHResult must be a valid memory location until the
//      the asynchronous execution completes.
//
//----------------------------------------------------------------------------


// FromFile

HRESULT WINAPI
    D3DX11CreateShaderResourceViewFromFileA(
        ID3D11Device*               pDevice,
        LPCSTR                      pSrcFile,
        D3DX11_IMAGE_LOAD_INFO      *pLoadInfo,
        ID3DX11ThreadPump*          pPump,
        ID3D11ShaderResourceView**  ppShaderResourceView,
        HRESULT*                    pHResult);

HRESULT WINAPI
    D3DX11CreateShaderResourceViewFromFileW(
        ID3D11Device*               pDevice,
        LPCWSTR                     pSrcFile,
        D3DX11_IMAGE_LOAD_INFO      *pLoadInfo,
        ID3DX11ThreadPump*          pPump,
        ID3D11ShaderResourceView**  ppShaderResourceView,
        HRESULT*                    pHResult);

#ifdef UNICODE
#define D3DX11CreateShaderResourceViewFromFile D3DX11CreateShaderResourceViewFromFileW
#else
#define D3DX11CreateShaderResourceViewFromFile D3DX11CreateShaderResourceViewFromFileA
#endif

HRESULT WINAPI
    D3DX11CreateTextureFromFileA(
        ID3D11Device*               pDevice,
        LPCSTR                      pSrcFile,
        D3DX11_IMAGE_LOAD_INFO      *pLoadInfo,
        ID3DX11ThreadPump*          pPump,
        ID3D11Resource**            ppTexture,
        HRESULT*                    pHResult);

HRESULT WINAPI
    D3DX11CreateTextureFromFileW(
        ID3D11Device*               pDevice,
        LPCWSTR                     pSrcFile,
        D3DX11_IMAGE_LOAD_INFO      *pLoadInfo,
        ID3DX11ThreadPump*          pPump,
        ID3D11Resource**            ppTexture,
        HRESULT*                    pHResult);

#ifdef UNICODE
#define D3DX11CreateTextureFromFile D3DX11CreateTextureFromFileW
#else
#define D3DX11CreateTextureFromFile D3DX11CreateTextureFromFileA
#endif


// FromResource (resources in dll/exes)

HRESULT WINAPI
    D3DX11CreateShaderResourceViewFromResourceA(
        ID3D11Device*              pDevice,
        HMODULE                    hSrcModule,
        LPCSTR                     pSrcResource,
        D3DX11_IMAGE_LOAD_INFO*    pLoadInfo,
        ID3DX11ThreadPump*         pPump,
        ID3D11ShaderResourceView** ppShaderResourceView,
        HRESULT*                   pHResult);

HRESULT WINAPI
    D3DX11CreateShaderResourceViewFromResourceW(
        ID3D11Device*              pDevice,
        HMODULE                    hSrcModule,
        LPCWSTR                    pSrcResource,
        D3DX11_IMAGE_LOAD_INFO*    pLoadInfo,
        ID3DX11ThreadPump*         pPump,
        ID3D11ShaderResourceView** ppShaderResourceView,
        HRESULT*                   pHResult);

#ifdef UNICODE
#define D3DX11CreateShaderResourceViewFromResource D3DX11CreateShaderResourceViewFromResourceW
#else
#define D3DX11CreateShaderResourceViewFromResource D3DX11CreateShaderResourceViewFromResourceA
#endif

HRESULT WINAPI
    D3DX11CreateTextureFromResourceA(
        ID3D11Device*            pDevice,
        HMODULE                  hSrcModule,
        LPCSTR                   pSrcResource,
        D3DX11_IMAGE_LOAD_INFO   *pLoadInfo,  
        ID3DX11ThreadPump*       pPump,   
        ID3D11Resource**         ppTexture,
        HRESULT*                 pHResult);

HRESULT WINAPI
    D3DX11CreateTextureFromResourceW(
        ID3D11Device*           pDevice,
        HMODULE                 hSrcModule,
        LPCWSTR                 pSrcResource,
        D3DX11_IMAGE_LOAD_INFO* pLoadInfo,
        ID3DX11ThreadPump*      pPump,
        ID3D11Resource**        ppTexture,
        HRESULT*                pHResult);

#ifdef UNICODE
#define D3DX11CreateTextureFromResource D3DX11CreateTextureFromResourceW
#else
#define D3DX11CreateTextureFromResource D3DX11CreateTextureFromResourceA
#endif


// FromFileInMemory

HRESULT WINAPI
    D3DX11CreateShaderResourceViewFromMemory(
        ID3D11Device*              pDevice,
        LPCVOID                    pSrcData,
        SIZE_T                     SrcDataSize,
        D3DX11_IMAGE_LOAD_INFO*    pLoadInfo,
        ID3DX11ThreadPump*         pPump,        
        ID3D11ShaderResourceView** ppShaderResourceView,
        HRESULT*                   pHResult);

HRESULT WINAPI
    D3DX11CreateTextureFromMemory(
        ID3D11Device*             pDevice,
        LPCVOID                   pSrcData,
        SIZE_T                    SrcDataSize,
        D3DX11_IMAGE_LOAD_INFO*   pLoadInfo,    
        ID3DX11ThreadPump*        pPump,    
        ID3D11Resource**          ppTexture,
        HRESULT*                  pHResult);


//////////////////////////////////////////////////////////////////////////////
// Misc Texture APIs /////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
// D3DX11_TEXTURE_LOAD_INFO:
// ------------------------
//
//----------------------------------------------------------------------------

typedef struct _D3DX11_TEXTURE_LOAD_INFO
{
    D3D11_BOX       *pSrcBox;
    D3D11_BOX       *pDstBox;
    UINT            SrcFirstMip;
    UINT            DstFirstMip;
    UINT            NumMips;
    UINT            SrcFirstElement;
    UINT            DstFirstElement;
    UINT            NumElements;
    UINT            Filter;
    UINT            MipFilter;
    
#ifdef __cplusplus
    _D3DX11_TEXTURE_LOAD_INFO()
    {
        pSrcBox = NULL;
        pDstBox = NULL;
        SrcFirstMip = 0;
        DstFirstMip = 0;
        NumMips = D3DX11_DEFAULT;
        SrcFirstElement = 0;
        DstFirstElement = 0;
        NumElements = D3DX11_DEFAULT;
        Filter = D3DX11_DEFAULT;
        MipFilter = D3DX11_DEFAULT;
    }  
#endif

} D3DX11_TEXTURE_LOAD_INFO;


//----------------------------------------------------------------------------
// D3DX11LoadTextureFromTexture:
// ----------------------------
// Load a texture from a texture.
//
// Parameters:
//
//----------------------------------------------------------------------------


HRESULT WINAPI
    D3DX11LoadTextureFromTexture(
		ID3D11DeviceContext       *pContext,
        ID3D11Resource            *pSrcTexture,
        D3DX11_TEXTURE_LOAD_INFO  *pLoadInfo,
        ID3D11Resource            *pDstTexture);


//----------------------------------------------------------------------------
// D3DX11FilterTexture:
// ------------------
// Filters mipmaps levels of a texture.
//
// Parameters:
//  pBaseTexture
//      The texture object to be filtered
//  SrcLevel
//      The level whose image is used to generate the subsequent levels. 
//  MipFilter
//      D3DX11_FILTER flags controlling how each miplevel is filtered.
//      Or D3DX11_DEFAULT for D3DX11_FILTER_BOX,
//
//----------------------------------------------------------------------------

HRESULT WINAPI
    D3DX11FilterTexture(
		ID3D11DeviceContext       *pContext,
        ID3D11Resource            *pTexture,
        UINT                      SrcLevel,
        UINT                      MipFilter);


//----------------------------------------------------------------------------
// D3DX11SaveTextureToFile:
// ----------------------
// Save a texture to a file.
//
// Parameters:
//  pDestFile
//      File name of the destination file
//  DestFormat
//      D3DX11_IMAGE_FILE_FORMAT specifying file format to use when saving.
//  pSrcTexture
//      Source texture, containing the image to be saved
//
//----------------------------------------------------------------------------

HRESULT WINAPI
    D3DX11SaveTextureToFileA(
		ID3D11DeviceContext       *pContext,
        ID3D11Resource            *pSrcTexture,
        D3DX11_IMAGE_FILE_FORMAT    DestFormat,
        LPCSTR                    pDestFile);

HRESULT WINAPI
    D3DX11SaveTextureToFileW(
		ID3D11DeviceContext       *pContext,
        ID3D11Resource            *pSrcTexture,
        D3DX11_IMAGE_FILE_FORMAT    DestFormat,
        LPCWSTR                   pDestFile);

#ifdef UNICODE
#define D3DX11SaveTextureToFile D3DX11SaveTextureToFileW
#else
#define D3DX11SaveTextureToFile D3DX11SaveTextureToFileA
#endif


//----------------------------------------------------------------------------
// D3DX11SaveTextureToMemory:
// ----------------------
// Save a texture to a blob.
//
// Parameters:
//  pSrcTexture
//      Source texture, containing the image to be saved
//  DestFormat
//      D3DX11_IMAGE_FILE_FORMAT specifying file format to use when saving.
//  ppDestBuf
//      address of a d3dxbuffer pointer to return the image data
//  Flags
//      optional flags
//----------------------------------------------------------------------------

HRESULT WINAPI
    D3DX11SaveTextureToMemory(
		ID3D11DeviceContext       *pContext,
        ID3D11Resource*            pSrcTexture,
        D3DX11_IMAGE_FILE_FORMAT   DestFormat,
        ID3D10Blob**               ppDestBuf,
        UINT                       Flags);


//----------------------------------------------------------------------------
// D3DX11ComputeNormalMap:
// ---------------------
// Converts a height map into a normal map.  The (x,y,z) components of each
// normal are mapped to the (r,g,b) channels of the output texture.
//
// Parameters
//  pSrcTexture
//      Pointer to the source heightmap texture 
//  Flags
//      D3DX11_NORMALMAP flags
//  Channel
//      D3DX11_CHANNEL specifying source of height information
//  Amplitude
//      The constant value which the height information is multiplied by.
//  pDestTexture
//      Pointer to the destination texture
//---------------------------------------------------------------------------

HRESULT WINAPI
    D3DX11ComputeNormalMap(
        ID3D11DeviceContext      *pContext,
        ID3D11Texture2D		     *pSrcTexture,
        UINT                      Flags,
        UINT                      Channel,
        FLOAT                     Amplitude,
        ID3D11Texture2D		     *pDestTexture);


//----------------------------------------------------------------------------
// D3DX11SHProjectCubeMap:
// ----------------------
//  Projects a function represented in a cube map into spherical harmonics.
//
//  Parameters:
//   Order
//      Order of the SH evaluation, generates Order^2 coefs, degree is Order-1
//   pCubeMap
//      CubeMap that is going to be projected into spherical harmonics
//   pROut
//      Output SH vector for Red.
//   pGOut
//      Output SH vector for Green
//   pBOut
//      Output SH vector for Blue        
//
//---------------------------------------------------------------------------

HRESULT WINAPI
    D3DX11SHProjectCubeMap(
        ID3D11DeviceContext                                *pContext,
        __in_range(2,6) UINT                                Order,
        ID3D11Texture2D                                    *pCubeMap,
        __out_ecount(Order*Order) FLOAT                    *pROut,
        __out_ecount_opt(Order*Order) FLOAT                *pGOut,
        __out_ecount_opt(Order*Order) FLOAT                *pBOut);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__D3DX11TEX_H__

