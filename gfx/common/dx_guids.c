/* dx_guids.c
 *
 * Emits DirectX COM GUID storage in lieu of linking dxguid.lib.
 *
 * The Windows 7.x Platform SDK (bundled with MSVC 2010 and earlier)
 * does not ship dxguid.lib. Rather than require the legacy June 2010
 * DirectX SDK just to pick up a handful of IID_* / CLSID_* constants,
 * we define them ourselves in this single translation unit.
 *
 * Including <initguid.h> before any DX header causes DEFINE_GUID() to
 * expand to actual storage rather than extern references. Every other
 * TU in the program continues to see the default extern declarations
 * and resolves the GUIDs to the storage defined here at link time.
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
#ifdef HAVE_D3D9
#include <d3d9.h>
#endif
#ifdef HAVE_D3D8
#include <d3d8.h>
#endif
#if defined(HAVE_D3D9) || defined(HAVE_D3D10) \
 || defined(HAVE_D3D11) || defined(HAVE_D3D12)
#include <d3dcompiler.h>
#include <dxgi.h>
#endif
#ifdef HAVE_DINPUT
#include <dinput.h>
#endif
#ifdef HAVE_XAUDIO
#include <xaudio2.h>
#endif

#endif /* _WIN32 && !_XBOX && !HAVE_GRIFFIN && desktop */
