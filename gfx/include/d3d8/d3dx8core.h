/*
 *
 *  Copyright (C) Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       d3dx8core.h
 *  Content:    D3DX core types and functions
 */

#include "d3dx8.h"

#ifndef __D3DX8CORE_H__
#define __D3DX8CORE_H__

/*
 * ID3DXBuffer:
 * ------------
 * The buffer object is used by D3DX to return arbitrary size data.
 *
 * GetBufferPointer -
 *    Returns a pointer to the beginning of the buffer.
 *
 * GetBufferSize -
 *    Returns the size of the buffer, in bytes.
 */

typedef interface ID3DXBuffer ID3DXBuffer;
typedef interface ID3DXBuffer *LPD3DXBUFFER;

/* {932E6A7E-C68E-45dd-A7BF-53D19C86DB1F} */
DEFINE_GUID(IID_ID3DXBuffer,
0x932e6a7e, 0xc68e, 0x45dd, 0xa7, 0xbf, 0x53, 0xd1, 0x9c, 0x86, 0xdb, 0x1f);

#undef INTERFACE
#define INTERFACE ID3DXBuffer

DECLARE_INTERFACE_(ID3DXBuffer, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* ID3DXBuffer */
    STDMETHOD_(LPVOID, GetBufferPointer)(THIS) PURE;
    STDMETHOD_(DWORD, GetBufferSize)(THIS) PURE;
};

/*
 * ID3DXFont:
 * ----------
 * Font objects contain the textures and resources needed to render
 * a specific font on a specific device.
 *
 * Begin -
 *    Prepartes device for drawing text.  This is optional.. if DrawText
 *    is called outside of Begin/End, it will call Begin and End for you.
 *
 * DrawText -
 *    Draws formatted text on a D3D device.  Some parameters are
 *    surprisingly similar to those of GDI's DrawText function.  See GDI
 *    documentation for a detailed description of these parameters.
 *
 * End -
 *    Restores device state to how it was when Begin was called.
 *
 * OnLostDevice, OnResetDevice -
 *    Call OnLostDevice() on this object before calling Reset() on the
 *    device, so that this object can release any stateblocks and video
 *    memory resources.  After Reset(), the call OnResetDevice().
 *
 */

typedef interface ID3DXFont ID3DXFont;
typedef interface ID3DXFont *LPD3DXFONT;

/* {89FAD6A5-024D-49af-8FE7-F51123B85E25} */
DEFINE_GUID( IID_ID3DXFont,
0x89fad6a5, 0x24d, 0x49af, 0x8f, 0xe7, 0xf5, 0x11, 0x23, 0xb8, 0x5e, 0x25);

#undef INTERFACE
#define INTERFACE ID3DXFont

DECLARE_INTERFACE_(ID3DXFont, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* ID3DXFont */
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE8* ppDevice) PURE;
    STDMETHOD(GetLogFont)(THIS_ LOGFONT* pLogFont) PURE;

    STDMETHOD(Begin)(THIS) PURE;
    STDMETHOD_(INT, DrawTextA)(THIS_ LPCSTR  pString, INT Count, LPRECT pRect, DWORD Format, D3DCOLOR Color) PURE;
    STDMETHOD_(INT, DrawTextW)(THIS_ LPCWSTR pString, INT Count, LPRECT pRect, DWORD Format, D3DCOLOR Color) PURE;
    STDMETHOD(End)(THIS) PURE;

    STDMETHOD(OnLostDevice)(THIS) PURE;
    STDMETHOD(OnResetDevice)(THIS) PURE;
};

#ifndef DrawText
#ifdef UNICODE
#define DrawText DrawTextW
#else
#define DrawText DrawTextA
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HRESULT WINAPI
    D3DXCreateFont(
        LPDIRECT3DDEVICE8   pDevice,
        HFONT               hFont,
        LPD3DXFONT*         ppFont);

HRESULT WINAPI
    D3DXCreateFontIndirect(
        LPDIRECT3DDEVICE8   pDevice,
        CONST LOGFONT*      pLogFont,
        LPD3DXFONT*         ppFont);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/*
 * ID3DXSprite:
 * ------------
 * This object intends to provide an easy way to drawing sprites using D3D.
 *
 * Begin -
 *    Prepares device for drawing sprites
 *
 * Draw, DrawAffine, DrawTransform -
 *    Draws a sprite in screen-space.  Before transformation, the sprite is
 *    the size of SrcRect, with its top-left corner at the origin (0,0).
 *    The color and alpha channels are modulated by Color.
 *
 * End -
 *     Restores device state to how it was when Begin was called.
 *
 * OnLostDevice, OnResetDevice -
 *    Call OnLostDevice() on this object before calling Reset() on the
 *    device, so that this object can release any stateblocks and video
 *    memory resources.  After Reset(), the call OnResetDevice().
 */

typedef interface ID3DXSprite ID3DXSprite;
typedef interface ID3DXSprite *LPD3DXSPRITE;

/* {13D69D15-F9B0-4e0f-B39E-C91EB33F6CE7} */
DEFINE_GUID( IID_ID3DXSprite,
0x13d69d15, 0xf9b0, 0x4e0f, 0xb3, 0x9e, 0xc9, 0x1e, 0xb3, 0x3f, 0x6c, 0xe7);

#undef INTERFACE
#define INTERFACE ID3DXSprite

DECLARE_INTERFACE_(ID3DXSprite, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* ID3DXSprite */
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE8* ppDevice) PURE;

    STDMETHOD(Begin)(THIS) PURE;

    STDMETHOD(Draw)(THIS_ LPDIRECT3DTEXTURE8  pSrcTexture,
        CONST RECT* pSrcRect, CONST D3DXVECTOR2* pScaling,
        CONST D3DXVECTOR2* pRotationCenter, FLOAT Rotation,
        CONST D3DXVECTOR2* pTranslation, D3DCOLOR Color) PURE;

    STDMETHOD(DrawTransform)(THIS_ LPDIRECT3DTEXTURE8 pSrcTexture,
        CONST RECT* pSrcRect, CONST D3DXMATRIX* pTransform,
        D3DCOLOR Color) PURE;

    STDMETHOD(End)(THIS) PURE;

    STDMETHOD(OnLostDevice)(THIS) PURE;
    STDMETHOD(OnResetDevice)(THIS) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HRESULT WINAPI
    D3DXCreateSprite(
        LPDIRECT3DDEVICE8   pDevice,
        LPD3DXSPRITE*       ppSprite);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/*
 * ID3DXRenderToSurface:
 * ---------------------
 * This object abstracts rendering to surfaces.  These surfaces do not
 * necessarily need to be render targets.  If they are not, a compatible
 * render target is used, and the result copied into surface at end scene.
 *
 * BeginScene, EndScene -
 *    Call BeginScene() and EndScene() at the beginning and ending of your
 *    scene.  These calls will setup and restore render targets, viewports,
 *    etc..
 *
 * OnLostDevice, OnResetDevice -
 *    Call OnLostDevice() on this object before calling Reset() on the
 *    device, so that this object can release any stateblocks and video
 *    memory resources.  After Reset(), the call OnResetDevice().
 */

typedef struct _D3DXRTS_DESC
{
    UINT                Width;
    UINT                Height;
    D3DFORMAT           Format;
    BOOL                DepthStencil;
    D3DFORMAT           DepthStencilFormat;

} D3DXRTS_DESC;

typedef interface ID3DXRenderToSurface ID3DXRenderToSurface;
typedef interface ID3DXRenderToSurface *LPD3DXRENDERTOSURFACE;

/* {82DF5B90-E34E-496e-AC1C-62117A6A5913} */
DEFINE_GUID( IID_ID3DXRenderToSurface,
0x82df5b90, 0xe34e, 0x496e, 0xac, 0x1c, 0x62, 0x11, 0x7a, 0x6a, 0x59, 0x13);

#undef INTERFACE
#define INTERFACE ID3DXRenderToSurface

DECLARE_INTERFACE_(ID3DXRenderToSurface, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* ID3DXRenderToSurface */
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE8* ppDevice) PURE;
    STDMETHOD(GetDesc)(THIS_ D3DXRTS_DESC* pDesc) PURE;

    STDMETHOD(BeginScene)(THIS_ LPDIRECT3DSURFACE8 pSurface, CONST D3DVIEWPORT8* pViewport) PURE;
    STDMETHOD(EndScene)(THIS) PURE;

    STDMETHOD(OnLostDevice)(THIS) PURE;
    STDMETHOD(OnResetDevice)(THIS) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HRESULT WINAPI
    D3DXCreateRenderToSurface(
        LPDIRECT3DDEVICE8       pDevice,
        UINT                    Width,
        UINT                    Height,
        D3DFORMAT               Format,
        BOOL                    DepthStencil,
        D3DFORMAT               DepthStencilFormat,
        LPD3DXRENDERTOSURFACE*  ppRenderToSurface);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/*
 * ID3DXRenderToEnvMap:
 * --------------------
 * This object abstracts rendering to environment maps.  These surfaces
 * do not necessarily need to be render targets.  If they are not, a
 * compatible render target is used, and the result copied into the
 * environment map at end scene.
 *
 * BeginCube, BeginSphere, BeginHemisphere, BeginParabolic -
 *    This function initiates the rendering of the environment map.  As
 *    parameters, you pass the textures in which will get filled in with
 *    the resulting environment map.
 *
 * Face -
 *    Call this function to initiate the drawing of each face.  For each
 *    environment map, you will call this six times.. once for each face
 *    in D3DCUBEMAP_FACES.
 *
 * End -
 *    This will restore all render targets, and if needed compose all the
 *    rendered faces into the environment map surfaces.
 *
 * OnLostDevice, OnResetDevice -
 *    Call OnLostDevice() on this object before calling Reset() on the
 *    device, so that this object can release any stateblocks and video
 *    memory resources.  After Reset(), the call OnResetDevice().
 */

typedef struct _D3DXRTE_DESC
{
    UINT        Size;
    D3DFORMAT   Format;
    BOOL        DepthStencil;
    D3DFORMAT   DepthStencilFormat;
} D3DXRTE_DESC;

typedef interface ID3DXRenderToEnvMap ID3DXRenderToEnvMap;
typedef interface ID3DXRenderToEnvMap *LPD3DXRenderToEnvMap;

/* {4E42C623-9451-44b7-8C86-ABCCDE5D52C8} */
DEFINE_GUID( IID_ID3DXRenderToEnvMap,
0x4e42c623, 0x9451, 0x44b7, 0x8c, 0x86, 0xab, 0xcc, 0xde, 0x5d, 0x52, 0xc8);

#undef INTERFACE
#define INTERFACE ID3DXRenderToEnvMap

DECLARE_INTERFACE_(ID3DXRenderToEnvMap, IUnknown)
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* ID3DXRenderToEnvMap */
    STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE8* ppDevice) PURE;
    STDMETHOD(GetDesc)(THIS_ D3DXRTE_DESC* pDesc) PURE;

    STDMETHOD(BeginCube)(THIS_
        LPDIRECT3DCUBETEXTURE8 pCubeTex) PURE;

    STDMETHOD(BeginSphere)(THIS_
        LPDIRECT3DTEXTURE8 pTex) PURE;

    STDMETHOD(BeginHemisphere)(THIS_
        LPDIRECT3DTEXTURE8 pTexZPos,
        LPDIRECT3DTEXTURE8 pTexZNeg) PURE;

    STDMETHOD(BeginParabolic)(THIS_
        LPDIRECT3DTEXTURE8 pTexZPos,
        LPDIRECT3DTEXTURE8 pTexZNeg) PURE;

    STDMETHOD(Face)(THIS_ D3DCUBEMAP_FACES Face) PURE;
    STDMETHOD(End)(THIS) PURE;

    STDMETHOD(OnLostDevice)(THIS) PURE;
    STDMETHOD(OnResetDevice)(THIS) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HRESULT WINAPI
    D3DXCreateRenderToEnvMap(
        LPDIRECT3DDEVICE8       pDevice,
        UINT                    Size,
        D3DFORMAT               Format,
        BOOL                    DepthStencil,
        D3DFORMAT               DepthStencilFormat,
        LPD3DXRenderToEnvMap*   ppRenderToEnvMap);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/*
 * Shader assemblers:
 */

/*
 * D3DXASM flags:
 * --------------
 *
 * D3DXASM_DEBUG
 *   Generate debug info.
 *
 * D3DXASM_SKIPVALIDATION
 *   Do not validate the generated code against known capabilities and
 *   constraints.  This option is only recommended when assembling shaders
 *   you KNOW will work.  (ie. have assembled before without this option.)
 */

#define D3DXASM_DEBUG           (1 << 0)
#define D3DXASM_SKIPVALIDATION  (1 << 1)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * D3DXAssembleShader:
 * -------------------
 * Assembles an ascii description of a vertex or pixel shader into
 * binary form.
 *
 * Parameters:
 *  pSrcFile
 *      Source file name
 *  hSrcModule
 *      Module handle. if NULL, current module will be used.
 *  pSrcResource
 *      Resource name in module
 *  pSrcData
 *      Pointer to source code
 *  SrcDataLen
 *      Size of source code, in bytes
 *  Flags
 *      D3DXASM_xxx flags
 *  ppConstants
 *      Returns an ID3DXBuffer object containing constant declarations.
 *  ppCompiledShader
 *      Returns an ID3DXBuffer object containing the object code.
 *  ppCompilationErrors
 *      Returns an ID3DXBuffer object containing ascii error messages
 */

HRESULT WINAPI
    D3DXAssembleShaderFromFileA(
        LPCSTR                pSrcFile,
        DWORD                 Flags,
        LPD3DXBUFFER*         ppConstants,
        LPD3DXBUFFER*         ppCompiledShader,
        LPD3DXBUFFER*         ppCompilationErrors);

HRESULT WINAPI
    D3DXAssembleShaderFromFileW(
        LPCWSTR               pSrcFile,
        DWORD                 Flags,
        LPD3DXBUFFER*         ppConstants,
        LPD3DXBUFFER*         ppCompiledShader,
        LPD3DXBUFFER*         ppCompilationErrors);

#ifdef UNICODE
#define D3DXAssembleShaderFromFile D3DXAssembleShaderFromFileW
#else
#define D3DXAssembleShaderFromFile D3DXAssembleShaderFromFileA
#endif

HRESULT WINAPI
    D3DXAssembleShaderFromResourceA(
        HMODULE               hSrcModule,
        LPCSTR                pSrcResource,
        DWORD                 Flags,
        LPD3DXBUFFER*         ppConstants,
        LPD3DXBUFFER*         ppCompiledShader,
        LPD3DXBUFFER*         ppCompilationErrors);

HRESULT WINAPI
    D3DXAssembleShaderFromResourceW(
        HMODULE               hSrcModule,
        LPCWSTR               pSrcResource,
        DWORD                 Flags,
        LPD3DXBUFFER*         ppConstants,
        LPD3DXBUFFER*         ppCompiledShader,
        LPD3DXBUFFER*         ppCompilationErrors);

#ifdef UNICODE
#define D3DXAssembleShaderFromResource D3DXAssembleShaderFromResourceW
#else
#define D3DXAssembleShaderFromResource D3DXAssembleShaderFromResourceA
#endif

HRESULT WINAPI
    D3DXAssembleShader(
        LPCVOID               pSrcData,
        UINT                  SrcDataLen,
        DWORD                 Flags,
        LPD3DXBUFFER*         ppConstants,
        LPD3DXBUFFER*         ppCompiledShader,
        LPD3DXBUFFER*         ppCompilationErrors);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/*
 * Misc APIs:
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * D3DXGetErrorString:
 * ------------------
 * Returns the error string for given an hresult.  Interprets all D3DX and
 * D3D hresults.
 *
 * Parameters:
 *  hr
 *      The error code to be deciphered.
 *  pBuffer
 *      Pointer to the buffer to be filled in.
 *  BufferLen
 *      Count of characters in buffer.  Any error message longer than this
 *      length will be truncated to fit.
 */
HRESULT WINAPI
    D3DXGetErrorStringA(
        HRESULT             hr,
        LPSTR               pBuffer,
        UINT                BufferLen);

HRESULT WINAPI
    D3DXGetErrorStringW(
        HRESULT             hr,
        LPWSTR              pBuffer,
        UINT                BufferLen);

#ifdef UNICODE
#define D3DXGetErrorString D3DXGetErrorStringW
#else
#define D3DXGetErrorString D3DXGetErrorStringA
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __D3DX8CORE_H__ */
