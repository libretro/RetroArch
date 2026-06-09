/* dx_guids_d3d9.c
 *
 * Emits Direct3D 9 COM GUID storage in lieu of linking dxguid.lib.
 *
 * Split out from dx_guids.c because the legacy <d3d9.h> header family
 * (pre-DXGI) and the modern <dxgi.h>/<d3d11.h> header family cannot
 * coexist in a single translation unit on every toolchain. The bundled
 * gfx/include/dxsdk headers and the bundled gfx/include/d3d9 headers
 * cooperate via shared D3DCOLORVALUE_DEFINED / D3DVECTOR_DEFINED /
 * D3DMATRIX_DEFINED guards, but a system <d3d9types.h> (e.g. mingw-w64)
 * will not honor those guards, redefining D3DCOLORVALUE et al. and
 * breaking the build.
 *
 * Including <initguid.h> before <d3d9.h> causes DEFINE_GUID() in
 * gfx/include/d3d9/d3d9.h (or in the system d3d9.h) to expand to actual
 * storage. See dx_guids.c for the rest of the design rationale.
 */

#if defined(_WIN32) && !defined(_XBOX) && !defined(HAVE_GRIFFIN) \
 && defined(HAVE_D3D9) \
 && (!defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP))

#include <initguid.h>
#include <d3d9.h>

#endif /* _WIN32 && !_XBOX && !HAVE_GRIFFIN && HAVE_D3D9 && desktop */
