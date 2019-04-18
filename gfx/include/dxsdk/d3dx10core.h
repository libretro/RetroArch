///////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) Microsoft Corporation.  All Rights Reserved.
//
//  File:       d3dx10core.h
//  Content:    D3DX10 core types and functions
//
///////////////////////////////////////////////////////////////////////////

#include "d3dx10.h"

#ifndef __D3DX10CORE_H__
#define __D3DX10CORE_H__

// Current name of the DLL shipped in the same SDK as this header.

#define D3DX10_DLL_W L"d3dx10_43.dll"
#define D3DX10_DLL_A "d3dx10_43.dll"

#ifdef UNICODE
    #define D3DX10_DLL D3DX10_DLL_W
#else
    #define D3DX10_DLL D3DX10_DLL_A
#endif

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

///////////////////////////////////////////////////////////////////////////
// D3DX10_SDK_VERSION:
// -----------------
// This identifier is passed to D3DX10CheckVersion in order to ensure that an
// application was built against the correct header files and lib files.
// This number is incremented whenever a header (or other) change would
// require applications to be rebuilt. If the version doesn't match,
// D3DX10CreateVersion will return FALSE. (The number itself has no meaning.)
///////////////////////////////////////////////////////////////////////////

#define D3DX10_SDK_VERSION 43

///////////////////////////////////////////////////////////////////////////
// D3DX10CreateDevice
// D3DX10CreateDeviceAndSwapChain
// D3DX10GetFeatureLevel1
///////////////////////////////////////////////////////////////////////////
HRESULT WINAPI D3DX10CreateDevice(IDXGIAdapter *pAdapter,
                                  D3D10_DRIVER_TYPE DriverType,
                                  HMODULE Software,
                                  UINT Flags,
                                  ID3D10Device **ppDevice);

HRESULT WINAPI D3DX10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter,
                                              D3D10_DRIVER_TYPE DriverType,
                                              HMODULE Software,
                                              UINT Flags,
                                              DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                                              IDXGISwapChain **ppSwapChain,
                                              ID3D10Device **ppDevice);

typedef interface ID3D10Device1 ID3D10Device1;
HRESULT WINAPI D3DX10GetFeatureLevel1(ID3D10Device *pDevice, ID3D10Device1 **ppDevice1);

#ifdef D3D_DIAG_DLL
BOOL WINAPI D3DX10DebugMute(BOOL Mute);
#endif
HRESULT WINAPI D3DX10CheckVersion(UINT D3DSdkVersion, UINT D3DX10SdkVersion);

#ifdef __cplusplus
}
#endif //__cplusplus

//////////////////////////////////////////////////////////////////////////////
// D3DX10_SPRITE flags:
// -----------------
// D3DX10_SPRITE_SAVE_STATE
//   Specifies device state should be saved and restored in Begin/End.
// D3DX10SPRITE_SORT_TEXTURE
//   Sprites are sorted by texture prior to drawing.  This is recommended when
//   drawing non-overlapping sprites of uniform depth.  For example, drawing
//   screen-aligned text with ID3DX10Font.
// D3DX10SPRITE_SORT_DEPTH_FRONT_TO_BACK
//   Sprites are sorted by depth front-to-back prior to drawing.  This is
//   recommended when drawing opaque sprites of varying depths.
// D3DX10SPRITE_SORT_DEPTH_BACK_TO_FRONT
//   Sprites are sorted by depth back-to-front prior to drawing.  This is
//   recommended when drawing transparent sprites of varying depths.
// D3DX10SPRITE_ADDREF_TEXTURES
//   AddRef/Release all textures passed in to DrawSpritesBuffered
//////////////////////////////////////////////////////////////////////////////

typedef enum _D3DX10_SPRITE_FLAG
{
    D3DX10_SPRITE_SORT_TEXTURE              = 0x01,
    D3DX10_SPRITE_SORT_DEPTH_BACK_TO_FRONT  = 0x02,
    D3DX10_SPRITE_SORT_DEPTH_FRONT_TO_BACK  = 0x04,
    D3DX10_SPRITE_SAVE_STATE                = 0x08,
    D3DX10_SPRITE_ADDREF_TEXTURES           = 0x10,
} D3DX10_SPRITE_FLAG;

typedef struct _D3DX10_SPRITE
{
    D3DXMATRIX  matWorld;

    D3DXVECTOR2 TexCoord;
    D3DXVECTOR2 TexSize;

    D3DXCOLOR   ColorModulate;

    ID3D10ShaderResourceView *pTexture;
    UINT        TextureIndex;
} D3DX10_SPRITE;

//////////////////////////////////////////////////////////////////////////////
// ID3DX10Sprite:
// ------------
// This object intends to provide an easy way to drawing sprites using D3D.
//
// Begin -
//    Prepares device for drawing sprites.
//
// Draw -
//    Draws a sprite
//
// Flush -
//    Forces all batched sprites to submitted to the device.
//
// End -
//    Restores device state to how it was when Begin was called.
//
//////////////////////////////////////////////////////////////////////////////

typedef interface ID3DX10Sprite ID3DX10Sprite;
typedef interface ID3DX10Sprite *LPD3DX10SPRITE;

// {BA0B762D-8D28-43ec-B9DC-2F84443B0614}
DEFINE_GUID(IID_ID3DX10Sprite,
0xba0b762d, 0x8d28, 0x43ec, 0xb9, 0xdc, 0x2f, 0x84, 0x44, 0x3b, 0x6, 0x14);

#undef INTERFACE
#define INTERFACE ID3DX10Sprite

DECLARE_INTERFACE_(ID3DX10Sprite, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX10Sprite
    STDMETHOD(Begin)(THIS_ UINT flags) PURE;

    STDMETHOD(DrawSpritesBuffered)(THIS_ D3DX10_SPRITE *pSprites, UINT cSprites) PURE;
    STDMETHOD(Flush)(THIS) PURE;

    STDMETHOD(DrawSpritesImmediate)(THIS_ D3DX10_SPRITE *pSprites, UINT cSprites, UINT cbSprite, UINT flags) PURE;
    STDMETHOD(End)(THIS) PURE;

    STDMETHOD(GetViewTransform)(THIS_ D3DXMATRIX *pViewTransform) PURE;
    STDMETHOD(SetViewTransform)(THIS_ D3DXMATRIX *pViewTransform) PURE;
    STDMETHOD(GetProjectionTransform)(THIS_ D3DXMATRIX *pProjectionTransform) PURE;
    STDMETHOD(SetProjectionTransform)(THIS_ D3DXMATRIX *pProjectionTransform) PURE;

    STDMETHOD(GetDevice)(THIS_ ID3D10Device** ppDevice) PURE;
};

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

HRESULT WINAPI
    D3DX10CreateSprite(
        ID3D10Device*         pDevice,
        UINT                  cDeviceBufferSize,
        LPD3DX10SPRITE*       ppSprite);

#ifdef __cplusplus
}
#endif //__cplusplus

//////////////////////////////////////////////////////////////////////////////
// ID3DX10ThreadPump:
//////////////////////////////////////////////////////////////////////////////

#undef INTERFACE
#define INTERFACE ID3DX10DataLoader

DECLARE_INTERFACE(ID3DX10DataLoader)
{
	STDMETHOD(Load)(THIS) PURE;
	STDMETHOD(Decompress)(THIS_ void **ppData, SIZE_T *pcBytes) PURE;
	STDMETHOD(Destroy)(THIS) PURE;
};

#undef INTERFACE
#define INTERFACE ID3DX10DataProcessor

DECLARE_INTERFACE(ID3DX10DataProcessor)
{
	STDMETHOD(Process)(THIS_ void *pData, SIZE_T cBytes) PURE;
	STDMETHOD(CreateDeviceObject)(THIS_ void **ppDataObject) PURE;
	STDMETHOD(Destroy)(THIS) PURE;
};

// {C93FECFA-6967-478a-ABBC-402D90621FCB}
DEFINE_GUID(IID_ID3DX10ThreadPump,
0xc93fecfa, 0x6967, 0x478a, 0xab, 0xbc, 0x40, 0x2d, 0x90, 0x62, 0x1f, 0xcb);

#undef INTERFACE
#define INTERFACE ID3DX10ThreadPump

DECLARE_INTERFACE_(ID3DX10ThreadPump, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX10ThreadPump
    STDMETHOD(AddWorkItem)(THIS_ ID3DX10DataLoader *pDataLoader, ID3DX10DataProcessor *pDataProcessor, HRESULT *pHResult, void **ppDeviceObject) PURE;
    STDMETHOD_(UINT, GetWorkItemCount)(THIS) PURE;

    STDMETHOD(WaitForAllItems)(THIS) PURE;
    STDMETHOD(ProcessDeviceWorkItems)(THIS_ UINT iWorkItemCount);

    STDMETHOD(PurgeAllItems)(THIS) PURE;
    STDMETHOD(GetQueueStatus)(THIS_ UINT *pIoQueue, UINT *pProcessQueue, UINT *pDeviceQueue) PURE;

};

HRESULT WINAPI D3DX10CreateThreadPump(UINT cIoThreads, UINT cProcThreads, ID3DX10ThreadPump **ppThreadPump);

//////////////////////////////////////////////////////////////////////////////
// ID3DX10Font:
// ----------
// Font objects contain the textures and resources needed to render a specific
// font on a specific device.
//
// GetGlyphData -
//    Returns glyph cache data, for a given glyph.
//
// PreloadCharacters/PreloadGlyphs/PreloadText -
//    Preloads glyphs into the glyph cache textures.
//
// DrawText -
//    Draws formatted text on a D3D device.  Some parameters are
//    surprisingly similar to those of GDI's DrawText function.  See GDI
//    documentation for a detailed description of these parameters.
//    If pSprite is NULL, an internal sprite object will be used.
//
//////////////////////////////////////////////////////////////////////////////

typedef struct _D3DX10_FONT_DESCA
{
    INT Height;
    UINT Width;
    UINT Weight;
    UINT MipLevels;
    BOOL Italic;
    BYTE CharSet;
    BYTE OutputPrecision;
    BYTE Quality;
    BYTE PitchAndFamily;
    CHAR FaceName[LF_FACESIZE];

} D3DX10_FONT_DESCA, *LPD3DX10_FONT_DESCA;

typedef struct _D3DX10_FONT_DESCW
{
    INT Height;
    UINT Width;
    UINT Weight;
    UINT MipLevels;
    BOOL Italic;
    BYTE CharSet;
    BYTE OutputPrecision;
    BYTE Quality;
    BYTE PitchAndFamily;
    WCHAR FaceName[LF_FACESIZE];

} D3DX10_FONT_DESCW, *LPD3DX10_FONT_DESCW;

#ifdef UNICODE
typedef D3DX10_FONT_DESCW D3DX10_FONT_DESC;
typedef LPD3DX10_FONT_DESCW LPD3DX10_FONT_DESC;
#else
typedef D3DX10_FONT_DESCA D3DX10_FONT_DESC;
typedef LPD3DX10_FONT_DESCA LPD3DX10_FONT_DESC;
#endif

typedef interface ID3DX10Font ID3DX10Font;
typedef interface ID3DX10Font *LPD3DX10FONT;

// {D79DBB70-5F21-4d36-BBC2-FF525C213CDC}
DEFINE_GUID(IID_ID3DX10Font,
0xd79dbb70, 0x5f21, 0x4d36, 0xbb, 0xc2, 0xff, 0x52, 0x5c, 0x21, 0x3c, 0xdc);

#undef INTERFACE
#define INTERFACE ID3DX10Font

DECLARE_INTERFACE_(ID3DX10Font, IUnknown)
{
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // ID3DX10Font
    STDMETHOD(GetDevice)(THIS_ ID3D10Device** ppDevice) PURE;
    STDMETHOD(GetDescA)(THIS_ D3DX10_FONT_DESCA *pDesc) PURE;
    STDMETHOD(GetDescW)(THIS_ D3DX10_FONT_DESCW *pDesc) PURE;
    STDMETHOD_(BOOL, GetTextMetricsA)(THIS_ TEXTMETRICA *pTextMetrics) PURE;
    STDMETHOD_(BOOL, GetTextMetricsW)(THIS_ TEXTMETRICW *pTextMetrics) PURE;

    STDMETHOD_(HDC, GetDC)(THIS) PURE;
    STDMETHOD(GetGlyphData)(THIS_ UINT Glyph, ID3D10ShaderResourceView** ppTexture, RECT *pBlackBox, POINT *pCellInc) PURE;

    STDMETHOD(PreloadCharacters)(THIS_ UINT First, UINT Last) PURE;
    STDMETHOD(PreloadGlyphs)(THIS_ UINT First, UINT Last) PURE;
    STDMETHOD(PreloadTextA)(THIS_ LPCSTR pString, INT Count) PURE;
    STDMETHOD(PreloadTextW)(THIS_ LPCWSTR pString, INT Count) PURE;

    STDMETHOD_(INT, DrawTextA)(THIS_ LPD3DX10SPRITE pSprite, LPCSTR pString, INT Count, LPRECT pRect, UINT Format, D3DXCOLOR Color) PURE;
    STDMETHOD_(INT, DrawTextW)(THIS_ LPD3DX10SPRITE pSprite, LPCWSTR pString, INT Count, LPRECT pRect, UINT Format, D3DXCOLOR Color) PURE;

#ifdef __cplusplus
#ifdef UNICODE
    HRESULT WINAPI_INLINE GetDesc(D3DX10_FONT_DESCW *pDesc) { return GetDescW(pDesc); }
    HRESULT WINAPI_INLINE PreloadText(LPCWSTR pString, INT Count) { return PreloadTextW(pString, Count); }
#else
    HRESULT WINAPI_INLINE GetDesc(D3DX10_FONT_DESCA *pDesc) { return GetDescA(pDesc); }
    HRESULT WINAPI_INLINE PreloadText(LPCSTR pString, INT Count) { return PreloadTextA(pString, Count); }
#endif
#endif //__cplusplus
};

#ifndef GetTextMetrics
#ifdef UNICODE
#define GetTextMetrics GetTextMetricsW
#else
#define GetTextMetrics GetTextMetricsA
#endif
#endif

#ifndef DrawText
#ifdef UNICODE
#define DrawText DrawTextW
#else
#define DrawText DrawTextA
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

HRESULT WINAPI
    D3DX10CreateFontA(
        ID3D10Device*           pDevice,
        INT                     Height,
        UINT                    Width,
        UINT                    Weight,
        UINT                    MipLevels,
        BOOL                    Italic,
        UINT                    CharSet,
        UINT                    OutputPrecision,
        UINT                    Quality,
        UINT                    PitchAndFamily,
        LPCSTR                  pFaceName,
        LPD3DX10FONT*           ppFont);

HRESULT WINAPI
    D3DX10CreateFontW(
        ID3D10Device*           pDevice,
        INT                     Height,
        UINT                    Width,
        UINT                    Weight,
        UINT                    MipLevels,
        BOOL                    Italic,
        UINT                    CharSet,
        UINT                    OutputPrecision,
        UINT                    Quality,
        UINT                    PitchAndFamily,
        LPCWSTR                 pFaceName,
        LPD3DX10FONT*           ppFont);

#ifdef UNICODE
#define D3DX10CreateFont D3DX10CreateFontW
#else
#define D3DX10CreateFont D3DX10CreateFontA
#endif

HRESULT WINAPI
    D3DX10CreateFontIndirectA(
        ID3D10Device*             pDevice,
        CONST D3DX10_FONT_DESCA*   pDesc,
        LPD3DX10FONT*             ppFont);

HRESULT WINAPI
    D3DX10CreateFontIndirectW(
        ID3D10Device*             pDevice,
        CONST D3DX10_FONT_DESCW*   pDesc,
        LPD3DX10FONT*             ppFont);

#ifdef UNICODE
#define D3DX10CreateFontIndirect D3DX10CreateFontIndirectW
#else
#define D3DX10CreateFontIndirect D3DX10CreateFontIndirectA
#endif

HRESULT WINAPI D3DX10UnsetAllDeviceObjects(ID3D10Device *pDevice);

#ifdef __cplusplus
}
#endif //__cplusplus

///////////////////////////////////////////////////////////////////////////

#define _FACD3D  0x876
#define MAKE_D3DHRESULT( code )  MAKE_HRESULT( 1, _FACD3D, code )
#define MAKE_D3DSTATUS( code )  MAKE_HRESULT( 0, _FACD3D, code )

#define D3DERR_INVALIDCALL                      MAKE_D3DHRESULT(2156)
#define D3DERR_WASSTILLDRAWING                  MAKE_D3DHRESULT(540)

#endif //__D3DX10CORE_H__
