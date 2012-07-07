/* RetroArch - A frontend for libretro.
* Copyright (C) 2010-2012 - Hans-Kristian Arntzen
* Copyright (C) 2011-2012 - Daniel De Matteis
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

#if defined(_XBOX)
#include "../../msvc/msvc_compat.h"
#endif

/*============================================================
CONSOLE EXTENSIONS
============================================================ */
#include "../retroarch_console.c"
#include "../console_settings.c"

#ifdef HAVE_LIBRETRO_MANAGEMENT
#include "../libretro_mgmt.c"
#endif

/*============================================================
COMPATIBILITY
============================================================ */
#include "../../compat/compat.c"

/*============================================================
CONFIG FILE
============================================================ */
#ifdef _XBOX
#undef __RARCH_POSIX_STRING_H
#undef __RARCH_MSVC_COMPAT_H
#undef strcasecmp
#endif

#ifdef HAVE_CONFIGFILE
#include "../../conf/config_file.c"
#endif

/*============================================================
VIDEO CONTEXT
============================================================ */

#ifdef HAVE_VID_CONTEXT

#if defined(__CELLOS_LV2__)
#include "../../gfx/context/ps3_ctx.c"
#elif defined(_XBOX360)
#include "../../gfx/context/xdk360_ctx.c"
#endif

#endif

/*============================================================
VIDEO SHADERS
============================================================ */

#ifdef HAVE_CG
#include "../../gfx/shader_cg.c"
#endif

#ifdef HAVE_HLSL
#include "../../gfx/shader_hlsl.c"
#endif

#ifdef HAVE_GLSL
#include "../../gfx/shader_glsl.c"
#endif

/*============================================================
VIDEO IMAGE
============================================================ */

#if defined(__CELLOS_LV2__)
#include "../../ps3/image.c"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */

#if defined(HAVE_OPENGLES11)
#include "../../gfx/gl.c"
#elif defined(HAVE_OPENGLES20)
#include "../../gfx/gles.c"
#elif defined(_XBOX360)
#include "../../360/xdk360_video_resources.cpp"
#elif defined(GEKKO)
#include "../../wii/video.c"
#endif

#ifdef _XBOX
#include "../../360/xdk360_video.cpp"
#endif

#include "../../gfx/null.c"

/*============================================================
FONTS
============================================================ */

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include "../../gfx/fonts/ps3_libdbgfont.c"
#elif defined(_XBOX360)
#include "../../gfx/fonts/xdk360_fonts.cpp"
#endif

/*============================================================
INPUT
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/ps3_input.c"
#elif defined(GEKKO)
#include "../../wii/input.c"
#endif

#ifdef HAVE_XINPUT2
#include "../../input/xinput2_input.c"
#endif

#include "../../input/null.c"

/*============================================================
STATE TRACKER
============================================================ */
#include "../../gfx/state_tracker.c"

/*============================================================
DRIVERS
============================================================ */
#include "../../driver.c"

/*============================================================
FIFO BUFFER
============================================================ */
#include "../../fifo_buffer.c"

/*============================================================
AUDIO HERMITE
============================================================ */
#include "../../audio/hermite.c"

/*============================================================
RSOUND
============================================================ */
#ifdef HAVE_RSOUND
#include "../../console/librsound/librsound.c"
#include "../../audio/rsound.c"
#endif

/*============================================================
AUDIO UTILS
============================================================ */
#include "../../audio/utils.c"

/*============================================================
AUDIO
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../../ps3/ps3_audio.c"
#elif defined(_XBOX360)
#include "../../360/xdk360_audio.cpp"
#elif defined(GEKKO)
#include "../../wii/audio.c"
#endif

#include "../../audio/null.c"

/*============================================================
DYNAMIC
============================================================ */
#include "../../dynamic.c"

/*============================================================
FILE
============================================================ */
#ifdef HAVE_FILEBROWSER
#include "../fileio/file_browser.c"
#endif
#include "../../file.c"
#include "../../file_path.c"

/*============================================================
MESSAGE
============================================================ */
#include "../../message.c"

/*============================================================
PATCH
============================================================ */
#include "../../patch.c"

/*============================================================
SETTINGS
============================================================ */
#include "../../settings.c"

/*============================================================
REWIND
============================================================ */
#include "../../rewind.c"

/*============================================================
MAIN
============================================================ */
#if defined(_XBOX360)
#include "../../360/frontend-xdk/main.c"
#elif defined(GEKKO)
#include "../../wii/frontend/main.c"
#endif

/*============================================================
RETROARCH
============================================================ */
#include "../../retroarch.c"

/*============================================================
THREAD
============================================================ */
#ifdef HAVE_THREAD
#include "../../thread.c"
#endif

/*============================================================
NETPLAY
============================================================ */
#ifdef HAVE_NETPLAY
#include "../../netplay.c"
#endif

/*============================================================
MENU
============================================================ */
#if defined(_XBOX360)
#include "../../360/frontend-xdk/menu.cpp"
#elif defined(GEKKO)
#include "../../wii/frontend/rgui.c"
#include "../../wii/frontend/list.c"
#endif
