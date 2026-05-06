/* dx_guids.c
 *
 * Emits DirectX COM GUID storage in lieu of linking dxguid.lib.
 *
 * The Windows 7.x Platform SDK (bundled with MSVC 2010 and earlier)
 * does not ship dxguid.lib. Rather than require the legacy June 2010
 * DirectX SDK just to pick up a handful of IID_* / CLSID_* constants,
 * we define them ourselves.
 *
 * Including <initguid.h> before any DX header causes DEFINE_GUID() to
 * expand to actual storage rather than extern references. Every other
 * TU in the program continues to see the default extern declarations
 * and resolves the GUIDs to the storage defined here at link time.
 *
 * GUID storage for the modern (DXGI-based) DX stack lives in this file
 * (D3D10/11/12, DXGI, D3DCompiler, DInput, XAudio). The legacy pre-DXGI
 * stack lives in two separate TUs:
 *   * gfx/common/dx_guids_d3d9.c
 *   * gfx/common/dx_guids_d3d8.c
 * because <d3d8.h> / <d3d9.h> and <d3d11.h> / <dxgi.h> cannot share a
 * single TU portably -- system <d3d8types.h> / <d3d9types.h> headers
 * (notably mingw-w64) do not honor the D3DCOLORVALUE_DEFINED guard
 * used by the bundled gfx/include/dxsdk headers, and redefine
 * D3DCOLORVALUE / D3DVECTOR / D3DMATRIX. Splitting per header family
 * sidesteps that altogether.
 *
 * IMPORTANT: no other TU in the program may include <initguid.h>
 * before DirectX headers, or the linker will report duplicate symbols
 * for IID_* / CLSID_* constants.
 *
 * This file is for the non-griffin build. The griffin build gets the
 * same behavior by including <initguid.h> at the top of griffin.c.
 *
 * The guard below disables this TU on:
 *   * non-Windows platforms
 *   * Xbox (its own SDKs provide GUID storage)
 *   * UWP / WinRT (the modern Windows SDK ships dxguid.lib)
 *   * griffin builds (griffin.c does the job there)
 */

#if defined(_WIN32) && !defined(_XBOX) && !defined(HAVE_GRIFFIN) \
 && (!defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP))

#include <initguid.h>

#ifdef HAVE_D3D12
#include <d3d12.h>
#include <d3d12shader.h>
#endif
#ifdef HAVE_D3D11
#include <d3d11.h>
#include <d3d11shader.h>
#endif
#ifdef HAVE_D3D10
#include <d3d10.h>
#endif
/* HAVE_D3D9: emitted in gfx/common/dx_guids_d3d9.c */
/* HAVE_D3D8: emitted in gfx/common/dx_guids_d3d8.c */

#if defined(HAVE_D3D10) || defined(HAVE_D3D11) || defined(HAVE_D3D12) \
 || (defined(HAVE_D3D9) && defined(HAVE_HLSL))
#include <d3dcompiler.h>
#endif

#if defined(HAVE_D3D10) || defined(HAVE_D3D11) || defined(HAVE_D3D12)
#include <dxgi.h>
#endif

#ifdef HAVE_DINPUT
#include <dinput.h>
#endif
#ifdef HAVE_XAUDIO
#include <xaudio2.h>
#endif

#endif /* _WIN32 && !_XBOX && !HAVE_GRIFFIN && desktop */
