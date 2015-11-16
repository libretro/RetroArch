/* RetroArch - A frontend for libretro.
* Copyright (C) 2011-2015 - Daniel De Matteis
*
* RetroArch is free software: you can redistribute it and/or modify it under the terms
* of the GNU General Public License as published by the Free Software Found-
* ation, either version 3 of the License, or (at your option) any later version.
*
* RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with RetroArch.
* If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#define HAVE_SHADERS
#endif

#if defined(HAVE_ZLIB) || defined(HAVE_7ZIP)
#define HAVE_COMPRESSION
#endif

#if defined(_MSC_VER)
#include <compat/posix_string.h>
#endif

/*============================================================
AUDIO
============================================================ */
#ifdef HAVE_XAUDIO
#include "../audio/drivers/xaudio.cpp"
#endif

/*============================================================
 INPUT
 ============================================================ */
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
#include "../input/drivers/cocoa_input.m"
#endif

/*============================================================
 KEYBOARD EVENT
 ============================================================ */
#if defined(_WIN32) && !defined(_XBOX)
#include "../input/drivers_keyboard/keyboard_event_win32.cpp"
#endif

/*============================================================
UI COMMON CONTEXT
============================================================ */
#if defined(_WIN32) && !defined(_XBOX)
#include "../gfx/common/win32_common.cpp"

#ifdef HAVE_OPENGL
#include "../gfx/drivers_context/wgl_ctx.cpp"
#endif
#endif


/*============================================================
MENU
============================================================ */
#ifdef HAVE_RMENU_XUI
#include "../menu/drivers/rmenu_xui.cpp"
#endif

#if defined(HAVE_D3D)
#include "../menu/drivers_display/menu_display_d3d.cpp"
#endif

/*============================================================
VIDEO CONTEXT
============================================================ */

#if defined(HAVE_D3D)
#include "../gfx/drivers_context/d3d_ctx.cpp"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */
#ifdef _XBOX
#include "../frontend/drivers/platform_xdk.cpp"
#endif
#ifdef _WIN32
#include "../gfx/video_texture.cpp"
#endif

#if defined(HAVE_D3D)
#include "../gfx/d3d/d3d_wrapper.cpp"
#include "../gfx/d3d/d3d.cpp"
#ifdef _XBOX
#include "../gfx/d3d/render_chain_xdk.cpp"
#endif
#ifdef HAVE_CG
#include "../gfx/d3d/render_chain_cg.cpp"
#endif
#endif

/*============================================================
FONTS
============================================================ */

#if defined(HAVE_D3D9) && !defined(_XBOX)
#include "../gfx/drivers_font/d3d_w32_font.cpp"
#endif
