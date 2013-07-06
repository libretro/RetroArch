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
#include "../msvc/msvc_compat.h"
#endif

#if defined(HAVE_RMENU) || defined(HAVE_RGUI) || defined(HAVE_RMENU_XUI)
#define HAVE_MENU
#endif

#ifdef __CELLOS_LV2__
#include "../ps3/altivec_mem.c"
#endif

/*============================================================
CONSOLE EXTENSIONS
============================================================ */
#ifdef RARCH_CONSOLE

#if defined(HAVE_LOGGER) && defined(__PSL1GHT__)
#include "../console/logger/psl1ght_logger.c"
#elif defined(HAVE_LOGGER) && !defined(ANDROID)
#include "../console/logger/logger.c"
#endif

#ifdef HW_DOL
#include "../ngc/ssaram.c"
#endif

#endif

#ifdef HAVE_ZLIB
#include "../file_extract.c"
#endif

/*============================================================
RLAUNCH
============================================================ */

#ifdef HAVE_RLAUNCH
#include "../tools/retrolaunch/rl_fnmatch.c"
#include "../tools/retrolaunch/sha1.c"
#include "../tools/retrolaunch/cd_detect.c"
#include "../tools/retrolaunch/parser.c"
#include "../tools/retrolaunch/main.c"
#endif

/*============================================================
PERFORMANCE
============================================================ */

#ifdef ANDROID
#include "../android/native/jni/cpufeatures.c"
#endif

#include "../performance.c"

/*============================================================
COMPATIBILITY
============================================================ */
#include "../compat/compat.c"

/*============================================================
CONFIG FILE
============================================================ */
#ifdef _XBOX
#undef __RARCH_POSIX_STRING_H
#undef __RARCH_MSVC_COMPAT_H
#undef strcasecmp
#endif

#include "../conf/config_file.c"
#include "../core_options.c"
#include "../core_info.c"

/*============================================================
CHEATS
============================================================ */
#include "../cheats.c"
#include "../hash.c"

/*============================================================
VIDEO CONTEXT
============================================================ */

#include "../gfx/gfx_context.c"

#if defined(__CELLOS_LV2__)
#include "../gfx/context/ps3_ctx.c"
#elif defined(_XBOX)
#include "../gfx/context/xdk_ctx.c"
#elif defined(ANDROID)
#include "../gfx/context/androidegl_ctx.c"
#elif defined(__BLACKBERRY_QNX__)
#include "../gfx/context/bbqnx_ctx.c"
#elif defined(IOS)
#include "../gfx/context/ioseagl_ctx.c"
#elif defined(GEKKO)
// none
#else
#include "../gfx/context/null_ctx.c"
#endif

/*============================================================
VIDEO SHADERS
============================================================ */

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#include "../gfx/shader_parse.c"
#endif

#ifdef HAVE_CG
#include "../gfx/shader_cg.c"
#endif

#ifdef HAVE_HLSL
#include "../gfx/shader_hlsl.c"
#endif

#ifdef HAVE_GLSL
#include "../gfx/shader_glsl.c"
#endif

/*============================================================
VIDEO IMAGE
============================================================ */

#if defined(__CELLOS_LV2__)
#include "../ps3/image.c"
#elif defined(_XBOX1)
#include "../xdk/image.c"
#elif defined(RARCH_MOBILE)
#include "../gfx/image.c"
#include "../gfx/rpng/rpng.c"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */

#if defined(HAVE_OPENGL)
#include "../gfx/math/matrix.c"
#elif defined(GEKKO)
#ifdef HW_RVL
#include "../wii/vi_encoder.c"
#include "../wii/mem2_manager.c"
#endif
#endif

#ifdef HAVE_DYLIB
#include "../gfx/ext_gfx.c"
#endif

#include "../gfx/gfx_common.c"

#ifdef _XBOX
#include "../xdk/xdk_resources.cpp"
#endif

#ifdef HAVE_OPENGL
#include "../gfx/gl.c"
#endif

#ifdef _XBOX
#include "../xdk/xdk_d3d.cpp"
#endif

#if defined(GEKKO)
#include "../gx/gx_video.c"
#elif defined(SN_TARGET_PSP2)
#include "../vita/vita_video.c"
//#elif defined(PSP)
//#include "../psp1/psp1_video.c"
#elif defined(XENON)
#include "../xenon/xenon360_video.c"
#endif

#if defined(HAVE_NULLVIDEO)
#include "../gfx/null.c"
#endif

/*============================================================
FONTS
============================================================ */

#ifdef _XBOX
#define DONT_HAVE_BITMAPFONTS
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_D3D8) || defined(HAVE_D3D9)

#if defined(HAVE_FREETYPE)
#include "../gfx/fonts/freetype.c"
#endif

#if !defined(DONT_HAVE_BITMAPFONTS)
#include "../gfx/fonts/fonts.c"
#include "../gfx/fonts/bitmapfont.c"
#endif

#ifdef HAVE_OPENGL
#include "../gfx/fonts/gl_font.c"
#endif

#ifdef _XBOX
#include "../gfx/fonts/d3d_font.c"
#endif

#if defined(HAVE_LIBDBGFONT)
#include "../gfx/fonts/ps_libdbgfont.c"
#elif defined(HAVE_OPENGL)
#include "../gfx/fonts/gl_raster_font.c"
#elif defined(_XBOX1)
#include "../gfx/fonts/xdk1_xfonts.c"
#elif defined(_XBOX360)
#include "../gfx/fonts/xdk360_fonts.cpp"
#endif

#endif

/*============================================================
INPUT
============================================================ */
#include "../input/input_common.c"

#ifdef HAVE_OVERLAY
#include "../input/overlay.c"
#endif

#if defined(__CELLOS_LV2__)
#include "../ps3/ps3_input.c"
#elif defined(SN_TARGET_PSP2) || defined(PSP)
#include "../psp/psp_input.c"
#elif defined(GEKKO)
#include "../gx/gx_input.c"
#elif defined(_XBOX)
#include "../xdk/xdk_xinput_input.c"
#elif defined(XENON)
#include "../xenon/xenon360_input.c"
#elif defined(ANDROID)
#include "../android/native/jni/input_autodetect.c"
#include "../android/native/jni/input_android.c"
#elif defined(IOS)
#include "../ios/RetroArch/input/ios_input.c"
#include "../ios/RetroArch/input/ios_joypad.c"
#include "../ios/RetroArch/input/BTStack/btdynamic.c"
#include "../ios/RetroArch/input/BTStack/wiimote.c"
#include "../ios/RetroArch/input/BTStack/btpad.c"
#include "../ios/RetroArch/input/BTStack/btpad_ps3.c"
#include "../ios/RetroArch/input/BTStack/btpad_wii.c"
#include "../ios/RetroArch/input/BTStack/btpad_queue.c"
#elif defined(__BLACKBERRY_QNX__)
#include "../blackberry-qnx/qnx_input.c"
#endif

#if defined(HAVE_NULLINPUT)
#include "../input/null.c"
#endif

/*============================================================
STATE TRACKER
============================================================ */
#ifdef _XBOX
#define DONT_HAVE_STATE_TRACKER
#endif

#ifndef DONT_HAVE_STATE_TRACKER
#include "../gfx/state_tracker.c"
#endif

/*============================================================
FIFO BUFFER
============================================================ */
#include "../fifo_buffer.c"

/*============================================================
AUDIO RESAMPLER
============================================================ */
#include "../audio/resampler.c"
#include "../audio/sinc.c"

/*============================================================
RSOUND
============================================================ */
#ifdef HAVE_RSOUND
#include "../deps/librsound/librsound.c"
#include "../audio/rsound.c"
#endif

/*============================================================
AUDIO UTILS
============================================================ */
#include "../audio/utils.c"

/*============================================================
AUDIO
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../ps3/ps3_audio.c"
#elif defined(XENON)
#include "../xenon/xenon360_audio.c"
#elif defined(GEKKO)
#include "../gx/gx_audio.c"
#endif

#ifdef HAVE_XAUDIO
#include "../audio/xaudio.c"
#include "../audio/xaudio-c/xaudio-c.cpp"
#endif

#ifdef HAVE_DSOUND
#include "../audio/dsound.c"
#endif

#ifdef HAVE_SL
#include "../audio/opensl.c"
#endif

#ifdef HAVE_ALSA
#ifdef __QNX__
#include "../blackberry-qnx/alsa_qsa.c"
#else
#include "../audio/alsa.c"
#include "../audio/alsathread.c"
#endif
#endif

#ifdef HAVE_AL
#include "../audio/openal.c"
#endif

#ifdef HAVE_COREAUDIO
#include "../audio/coreaudio.c"
#endif

#if defined(HAVE_NULLAUDIO)
#include "../audio/null.c"
#endif

#ifdef HAVE_DYLIB
#include "../audio/ext_audio.c"
#endif

/*============================================================
DRIVERS
============================================================ */
#include "../driver.c"

/*============================================================
SCALERS
============================================================ */
#include "../gfx/scaler/filter.c"
#include "../gfx/scaler/pixconv.c"
#include "../gfx/scaler/scaler.c"
#include "../gfx/scaler/scaler_int.c"

/*============================================================
DYNAMIC
============================================================ */
#include "../dynamic.c"
#include "../dynamic_dummy.c"

/*============================================================
FILE
============================================================ */
#include "../file.c"
#include "../file_path.c"

/*============================================================
MESSAGE
============================================================ */
#include "../message.c"

/*============================================================
PATCH
============================================================ */
#include "../patch.c"

/*============================================================
SETTINGS
============================================================ */
#include "../settings.c"

/*============================================================
REWIND
============================================================ */
#include "../rewind.c"

/*============================================================
MAIN
============================================================ */
#if defined(XENON)
#include "../frontend/frontend_xenon.c"
#elif defined(RARCH_CONSOLE) || defined(PSP)
#include "../frontend/frontend_console.c"
#elif defined(__BLACKBERRY_QNX__)
#include "../frontend/frontend_bbqnx.c"
#elif defined(ANDROID)
#include "../frontend/frontend_android.c"
#elif defined(__APPLE__)
#include "../frontend/frontend_objc.c"
#endif

#include "../frontend/frontend.c"

/*============================================================
RETROARCH
============================================================ */
#include "../retroarch.c"

/*============================================================
THREAD
============================================================ */
#if defined(HAVE_THREADS) && defined(XENON)
#include "../thread/xenon_sdl_threads.c"
#elif defined(HAVE_THREADS)
#include "../thread.c"
#include "../gfx/thread_wrapper.c"
#ifndef RARCH_CONSOLE
#include "../autosave.c"
#endif
#endif


/*============================================================
NETPLAY
============================================================ */
#ifdef HAVE_NETPLAY
#include "../netplay.c"
#endif

/*============================================================
SCREENSHOTS
============================================================ */
#if defined(_XBOX1)
#include "../xdk/screenshot_xdk1.c"
#elif defined(HAVE_SCREENSHOTS)
#include "../screenshot.c"
#endif

/*============================================================
MENU
============================================================ */
#ifdef HAVE_MENU
#include "../frontend/menu/menu_common.c"
#include "../frontend/menu/history.c"

#if defined(HAVE_RMENU_GUI)
#include "../frontend/menu/rmenu.c"
#elif defined(HAVE_RGUI)
#include "../frontend/menu/rgui.c"
#elif defined(HAVE_RMENU_XUI)
#include "../frontend/menu/rmenu_xui.cpp"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
RZLIB
============================================================ */
#ifdef WANT_MINIZ
#include "../deps/miniz/miniz.c"
#include "../deps/minizip/unzip.c"
#endif

/*============================================================
XML
============================================================ */
#define RXML_LIBXML2_COMPAT
#include "../compat/rxml/rxml.c"

#ifdef __cplusplus
}
#endif
