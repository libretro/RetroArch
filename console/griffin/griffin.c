/* RetroArch - A frontend for libretro.
* Copyright (C) 2010-2013 - Hans-Kristian Arntzen
* Copyright (C) 2011-2013 - Daniel De Matteis
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
LOGGERS
============================================================ */

#if defined(HAVE_LOGGER) && defined(__PSL1GHT__)
#include "../logger/psl1ght_logger.c"
#elif defined(HAVE_LOGGER) && !defined(ANDROID)
#include "../logger/logger.c"
#endif

/*============================================================
CONSOLE EXTENSIONS
============================================================ */
#ifdef RARCH_CONSOLE

#include "../rarch_console_rom_ext.c"
#include "../rarch_console_video.c"

#ifdef HW_DOL
#include "../../ngc/ssaram.c"
#include "../../ngc/sidestep.c"
#endif

#ifdef HAVE_RSOUND
#include "../rarch_console_rsound.c"
#endif

#ifdef HAVE_DEFAULT_RETROPAD_INPUT
#include "../rarch_console_input.c"
#endif

#include "../rarch_console_settings.c"

#endif


/*============================================================
PERFORMANCE
============================================================ */

#ifdef ANDROID
#include "../../android/native/jni/cpufeatures.c"
#endif

#include "../../performance.c"

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

#include "../../conf/config_file.c"

/*============================================================
CHEATS
============================================================ */
#include "../../cheats.c"
#include "../../hash.c"

/*============================================================
VIDEO CONTEXT
============================================================ */

#ifdef HAVE_VID_CONTEXT
#include "../../gfx/gfx_context.c"

#if defined(__CELLOS_LV2__)
#include "../../gfx/context/ps3_ctx.c"
#elif defined(_XBOX)
#include "../../gfx/context/xdk_ctx.c"
#elif defined(ANDROID)
#include "../../gfx/context/androidegl_ctx.c"
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
#elif defined(_XBOX1)
#include "../../xdk/image.c"
#elif defined(ANDROID)
#include "../../gfx/image.c"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */

#if defined(HAVE_OPENGL)
#include "../../gfx/math/matrix.c"
#elif defined(GEKKO)
#ifdef HW_RVL
#include "../../wii/vi_encoder.c"
#include "../../wii/mem2_manager.c"
#endif
#endif

#ifdef HAVE_DYLIB
#include "../../gfx/ext_gfx.c"
#endif

#include "../../gfx/gfx_common.c"

#ifdef _XBOX
#include "../../xdk/xdk_resources.cpp"
#endif

#ifdef HAVE_OPENGL
#include "../../gfx/gl.c"
#endif

#ifdef _XBOX
#include "../../xdk/xdk_d3d.cpp"
#endif

#if defined(GEKKO)
#include "../../gx/gx_video.c"
#elif defined(SN_TARGET_PSP2)
#include "../../vita/vita_video.c"
//#elif defined(PSP)
//#include "../../psp1/psp1_video.c"
#elif defined(XENON)
#include "../../xenon/xenon360_video.c"
#endif

#if defined(HAVE_NULLVIDEO)
#include "../../gfx/null.c"
#endif

/*============================================================
FONTS
============================================================ */

#if defined(HAVE_OPENGL) || defined(HAVE_D3D8) || defined(HAVE_D3D9)

#ifdef HAVE_FREETYPE
#include "../../gfx/fonts/freetype.c"
#endif

#include "../../gfx/fonts/fonts.c"
#include "../../gfx/fonts/bitmapfont.c"

#ifdef HAVE_OPENGL
#include "../../gfx/fonts/gl_font.c"
#endif

#ifdef _XBOX
#include "../../gfx/fonts/d3d_font.c"
#endif

#if defined(HAVE_LIBDBGFONT)
#include "../../gfx/fonts/ps_libdbgfont.c"
#elif defined(HAVE_OPENGL)
#include "../../gfx/fonts/gl_raster_font.c"
#elif defined(_XBOX1)
#include "../../gfx/fonts/xdk1_xfonts.c"
#elif defined(_XBOX360)
#include "../../gfx/fonts/xdk360_fonts.cpp"
#endif

#endif

/*============================================================
INPUT
============================================================ */
#ifndef RARCH_CONSOLE
#include "../../input/input_common.c"
#endif

#ifdef HAVE_OVERLAY
#include "../../input/overlay.c"
#endif

#ifdef HAVE_WIIUSE
#include "../../wii/wiiuse/classic.c"
#include "../../wii/wiiuse/dynamics.c"
#include "../../wii/wiiuse/events.c"
#include "../../wii/wiiuse/io.c"
#include "../../wii/wiiuse/io_wii.c"
#include "../../wii/wiiuse/ir.c"
#include "../../wii/wiiuse/motion_plus.c"
#include "../../wii/wiiuse/nunchuk.c"
#ifdef HAVE_WIIUSE_SPEAKER
#include "../../wii/wiiuse/speaker.c"
#endif
#include "../../wii/wiiuse/wiiuse.c"
#include "../../wii/wiiuse/wpad.c"
#endif

#if defined(__CELLOS_LV2__)
#include "../../ps3/ps3_input.c"
#elif defined(SN_TARGET_PSP2) || defined(PSP)
#include "../../psp/psp_input.c"
#elif defined(GEKKO)
#include "../../gx/gx_input.c"
#elif defined(_XBOX)
#include "../../xdk/xdk_xinput_input.c"
#elif defined(XENON)
#include "../../xenon/xenon360_input.c"
#elif defined(ANDROID)
#include "../../android/native/jni/input_autodetect.c"
#include "../../android/native/jni/input_android.c"
#endif

#if defined(HAVE_NULLINPUT)
#include "../../input/null.c"
#endif

/*============================================================
STATE TRACKER
============================================================ */
#include "../../gfx/state_tracker.c"

/*============================================================
FIFO BUFFER
============================================================ */
#include "../../fifo_buffer.c"

/*============================================================
AUDIO HERMITE
============================================================ */
#ifdef HAVE_SINC
#include "../../audio/sinc.c"
#else
#include "../../audio/hermite.c"
#endif

/*============================================================
RSOUND
============================================================ */
#ifdef HAVE_RSOUND
#include "../../deps/librsound/librsound.c"
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
#elif defined(XENON)
#include "../../xenon/xenon360_audio.c"
#elif defined(GEKKO)
#include "../../gx/gx_audio.c"
#endif

#ifdef HAVE_XAUDIO
#include "../../audio/xaudio.c"
#include "../../audio/xaudio-c/xaudio-c.cpp"
#endif

#ifdef HAVE_DSOUND
#include "../../audio/dsound.c"
#endif

#ifdef HAVE_SL
#include "../../audio/opensl.c"
#endif

#if defined(HAVE_NULLAUDIO)
#include "../../audio/null.c"
#endif

#ifdef HAVE_DYLIB
#include "../../audio/ext_audio.c"
#endif

/*============================================================
DRIVERS
============================================================ */
#include "../../driver.c"

/*============================================================
SCALERS
============================================================ */
#include "../../gfx/scaler/filter.c"
#include "../../gfx/scaler/pixconv.c"
#include "../../gfx/scaler/scaler.c"
#include "../../gfx/scaler/scaler_int.c"

/*============================================================
DYNAMIC
============================================================ */
#include "../../dynamic.c"

/*============================================================
FILE
============================================================ */
#ifdef HAVE_FILEBROWSER
#include "../../frontend/menu/utils/file_browser.c"
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
#if defined(XENON)
#include "../../frontend/frontend_xenon.c"
#elif defined(RARCH_CONSOLE) || defined(PSP)
#include "../../frontend/frontend_console.c"
#elif defined(ANDROID)
#include "../../frontend/frontend_android.c"
#endif

/*============================================================
RETROARCH
============================================================ */
#include "../../retroarch.c"

/*============================================================
THREAD
============================================================ */
#if defined(HAVE_THREAD) && defined(XENON)
#include "../../thread/xenon_sdl_threads.c"
#elif defined(HAVE_THREAD)
#include "../../thread.c"
#ifdef ANDROID
#include "../../autosave.c"
#endif
#endif

/*============================================================
NETPLAY
============================================================ */
#ifdef HAVE_NETPLAY
#include "../../netplay.c"
#endif

/*============================================================
SCREENSHOTS
============================================================ */
#ifdef HAVE_SCREENSHOTS
#include "../../screenshot.c"
#endif

/*============================================================
MENU
============================================================ */
#if defined(HAVE_RMENU_GUI)
#include "../../frontend/menu/utils/menu_stack.c"
#include "../../frontend/menu/rmenu.c"
#endif

#ifdef HAVE_RGUI
#include "../../frontend/menu/utils/file_list.c"
#include "../../frontend/menu/rgui.c"
#endif

#ifdef HAVE_RMENU

#if defined(_XBOX360)
#include "../../frontend/menu/rmenu_xui.cpp"
#elif defined(GEKKO)
#include "../../frontend/menu/rmenu_gx.c"
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
RZLIB
============================================================ */
#ifdef WANT_RZLIB
#include "../../deps/rzlib/rzlib.c"
#endif

/*============================================================
XML
============================================================ */
#define RXML_LIBXML2_COMPAT
#include "../../compat/rxml/rxml.c"

#ifdef __cplusplus
}
#endif
