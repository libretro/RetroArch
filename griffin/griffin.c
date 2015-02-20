/* RetroArch - A frontend for libretro.
* Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef HAVE_LIBRETRODB
#define HAVE_LIBRETRODB
#endif

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
CONSOLE EXTENSIONS
============================================================ */
#ifdef RARCH_CONSOLE

#if defined(HAVE_LOGGER) && defined(__PSL1GHT__)
#include "../logger/netlogger/psl1ght_logger.c"
#elif defined(HAVE_LOGGER) && !defined(ANDROID)
#include "../logger/netlogger/logger.c"
#endif

#ifdef HW_DOL
#include "../ngc/ssaram.c"
#endif

#endif

#ifdef HAVE_ZLIB
#include "../file_extract.c"
#include "../decompress/zip_support.c"
#endif

/*============================================================
PERFORMANCE
============================================================ */

#ifdef ANDROID
#include "../performance/performance_android.c"
#endif

#include "../performance.c"

/*============================================================
COMPATIBILITY
============================================================ */
#include "../compat/compat.c"
#include "../libretro-common/compat/compat_fnmatch.c"

/*============================================================
CONFIG FILE
============================================================ */
#if defined(_MSC_VER)
#undef __LIBRETRO_SDK_COMPAT_POSIX_STRING_H
#undef __LIBRETRO_SDK_COMPAT_MSVC_H
#undef strcasecmp
#endif

#include "../libretro-common/file/config_file.c"
#include "../libretro-common/file/config_file_userdata.c"
#include "../core_options.c"

/*============================================================
CHEATS
============================================================ */
#include "../cheats.c"
#include "../hash.c"

/*============================================================
UI COMMON CONTEXT
============================================================ */
#if defined(_WIN32) && !defined(_XBOX)
#include "../gfx/drivers_context/win32_common.c"
#include "../gfx/drivers_context/win32_dwm_common.c"
#endif

/*============================================================
VIDEO CONTEXT
============================================================ */

#include "../gfx/video_context_driver.c"
#include "../gfx/drivers_context/gfx_null_ctx.c"

#if defined(__CELLOS_LV2__)
#include "../gfx/drivers_context/ps3_ctx.c"
#elif defined(_XBOX) || defined(HAVE_WIN32_D3D9)
#include "../gfx/drivers_context/d3d_ctx.cpp"
#elif defined(ANDROID)
#include "../gfx/drivers_context/androidegl_ctx.c"
#elif defined(__QNX__)
#include "../gfx/drivers_context/bbqnx_ctx.c"
#elif defined(EMSCRIPTEN)
#include "../gfx/drivers_context/emscriptenegl_ctx.c"
#endif


#if defined(HAVE_OPENGL)

#if defined(HAVE_KMS)
#include "../gfx/drivers_context/drm_egl_ctx.c"
#endif
#if defined(HAVE_VIDEOCORE)
#include "../gfx/drivers_context/vc_egl_ctx.c"
#endif
#if defined(HAVE_X11) && !defined(HAVE_OPENGLES)
#include "../gfx/drivers_context/glx_ctx.c"
#endif

#if defined(HAVE_EGL)
#include "../gfx/drivers_context/xegl_ctx.c"
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include "../gfx/drivers_context/wgl_ctx.c"
#endif

#endif

#ifdef HAVE_X11
#include "../gfx/drivers_context/x11_common.c"
#endif


/*============================================================
VIDEO SHADERS
============================================================ */

#ifdef HAVE_SHADERS
#include "../gfx/video_shader_driver.c"
#include "../gfx/video_shader_parse.c"

#include "../gfx/drivers_shader/shader_null.c"

#ifdef HAVE_CG
#ifdef HAVE_OPENGL
#include "../gfx/drivers_shader/shader_gl_cg.c"
#endif
#endif

#ifdef HAVE_HLSL
#include "../gfx/drivers_shader/shader_hlsl.c"
#endif

#ifdef HAVE_GLSL
#include "../gfx/drivers_shader/shader_glsl.c"
#endif

#endif

/*============================================================
VIDEO IMAGE
============================================================ */

#if defined(__CELLOS_LV2__)
#include "../gfx/image/image_ps3.c"
#elif defined(_XBOX1)
#include "../gfx/image/image_xdk1.c"
#else
#include "../gfx/image/image_rpng.c"
//#include "../gfx/image/image_mpng.c"
#endif

#include "../libretro-common/formats/png/rpng.c"
//#include "../libretro-common/formats/mpng/mpng_decode.c"

/*============================================================
VIDEO DRIVER
============================================================ */

#if defined(HAVE_OPENGL)
#include "../libretro-common/gfx/math/matrix_4x4.c"
#elif defined(GEKKO)
#ifdef HW_RVL
#include "../wii/vi_encoder.c"
#include "../wii/mem2_manager.c"
#endif
#endif

#ifdef HAVE_VG
#include "../gfx/drivers/vg.c"
#include "../libretro-common/gfx/math/matrix_3x3.c"
#endif

#ifdef HAVE_OMAP
#include "../gfx/drivers/omap_gfx.c"
#endif

#ifdef _XBOX
#include "../xdk/xdk_resources.cpp"
#endif

#ifdef HAVE_OPENGL
#include "../gfx/drivers/gl.c"
#include "../gfx/gl_common.c"

#ifndef HAVE_PSGL
#include "../libretro-common/glsym/rglgen.c"
#ifdef HAVE_OPENGLES2
#include "../libretro-common/glsym/glsym_es2.c"
#else
#include "../libretro-common/glsym/glsym_gl.c"
#endif
#endif

#endif

#ifdef HAVE_XVIDEO
#include "../gfx/drivers/xvideo.c"
#endif

#if defined(_XBOX) || defined(HAVE_WIN32_D3D9)
#include "../gfx/d3d/d3d_wrapper.cpp"
#include "../gfx/d3d/d3d.cpp"
#ifndef _XBOX
#include "../gfx/d3d/render_chain.cpp"
#endif
#endif

#if defined(GEKKO)
#include "../gfx/drivers/gx_gfx.c"
#elif defined(PSP)
#include "../gfx/drivers/psp1_gfx.c"
#elif defined(XENON)
#include "../gfx/drivers/xenon360_gfx.c"
#endif

#include "../gfx/drivers/nullgfx.c"

/*============================================================
FONTS
============================================================ */

#include "../gfx/font_renderer_driver.c"
#include "../gfx/drivers_font_renderer/bitmapfont.c"

#if defined(HAVE_FREETYPE)
#include "../gfx/drivers_font_renderer/freetype.c"
#endif

#if defined(__APPLE__)
#include "../gfx/drivers_font_renderer/coretext.c"
#endif

#ifdef HAVE_OPENGL
#include "../gfx/font_gl_driver.c"
#endif

#if defined(_XBOX) || defined(HAVE_WIN32_D3D9)
#include "../gfx/font_d3d_driver.c"
#endif

#if defined(HAVE_WIN32_D3D9)
#include "../gfx/drivers_font/d3d_w32_font.cpp"
#endif

#if defined(HAVE_LIBDBGFONT)
#include "../gfx/drivers_font/ps_libdbgfont.c"
#endif

#if defined(HAVE_OPENGL)
#include "../gfx/drivers_font/gl_raster_font.c"
#endif

#if defined(_XBOX1)
#include "../gfx/drivers_font/xdk1_xfonts.c"
#endif

#if defined(_XBOX360)
#include "../gfx/drivers_font/xdk360_fonts.cpp"
#endif

/*============================================================
INPUT
============================================================ */
#include "../input/input_autodetect.c"
#include "../input/input_joypad_driver.c"
#include "../input/input_joypad.c"
#include "../input/input_common.c"
#include "../input/input_keymaps.c"
#include "../input/input_remapping.c"
#include "../input/input_sensor.c"
#include "../input/keyboard_line.c"

#ifdef HAVE_OVERLAY
#include "../input/input_overlay.c"
#endif

#if defined(__CELLOS_LV2__)
#include "../input/drivers/ps3_input.c"
#include "../input/drivers_joypad/ps3_input_joypad.c"
#include "../input/autoconf/builtin_ps3.c"
#elif defined(SN_TARGET_PSP2) || defined(PSP)
#include "../input/drivers/psp_input.c"
#include "../input/drivers_joypad/psp_input_joypad.c"
#include "../input/autoconf/builtin_psp.c"
#elif defined(GEKKO)
#ifdef HAVE_LIBSICKSAXIS
#include "../input/drivers_joypad/gx_input_sicksaxis.c"
#endif
#include "../input/drivers/gx_input.c"
#include "../input/drivers_joypad/gx_input_joypad.c"
#include "../input/autoconf/builtin_gx.c"
#elif defined(_XBOX)
#include "../input/drivers/xdk_xinput_input.c"
#include "../input/drivers_joypad/xdk_xinput_input_joypad.c"
#include "../input/autoconf/builtin_xdk.c"
#elif defined(_WIN32)
#include "../input/autoconf/builtin_win.c"
#elif defined(XENON)
#include "../input/drivers/xenon360_input.c"
#elif defined(ANDROID)
#include "../input/drivers/android_input.c"
#include "../input/drivers_joypad/android_input_joypad.c"
#elif defined(__APPLE__)
#include "../input/drivers/apple_input.c"
#elif defined(__QNX__)
#include "../input/drivers/qnx_input.c"
#include "../input/drivers_joypad/qnx_input_joypad.c"
#elif defined(EMSCRIPTEN)
#include "../input/drivers/rwebinput_input.c"
#endif

#if defined(__APPLE__)
#include "../input/connect/joypad_connection.c"
#include "../input/connect/connect_ps3.c"
#include "../input/connect/connect_ps4.c"
#include "../input/connect/connect_wii.c"

#ifdef HAVE_HID
#include "../input/drivers_joypad/apple_joypad_hid.c"
#endif

#ifdef IOS
#include "../input/drivers_joypad/apple_joypad_ios.c"
#endif

#endif

#ifdef HAVE_DINPUT
#include "../input/drivers/dinput.c"
#endif

#ifdef HAVE_WINXINPUT
#include "../input/drivers_joypad/winxinput_joypad.c"
#endif

#if defined(__linux__) && !defined(ANDROID) 
#include "../input/drivers/linuxraw_input.c"
#include "../input/drivers_joypad/linuxraw_joypad.c"
#endif

#ifdef HAVE_X11
#include "../input/drivers/x11_input.c"
#endif

#ifdef HAVE_UDEV
#include "../input/drivers/udev_input.c"
#include "../input/drivers_joypad/udev_joypad.c"
#endif

#include "../input/drivers/nullinput.c"
#include "../input/drivers_joypad/nullinput_joypad.c"

/*============================================================
 KEYBOARD EVENT
 ============================================================ */

#if defined(_WIN32) && !defined(_XBOX)
#include "../input/keyboard_event_win32.c"
#endif

#ifdef HAVE_X11
#include "../input/keyboard_event_x11.c"
#endif

#ifdef __APPLE__
#include "../input/keyboard_event_apple.c"
#endif

#ifdef HAVE_XKBCOMMON
#include "../input/keyboard_event_xkb.c"
#endif

/*============================================================
STATE TRACKER
============================================================ */
#include "../gfx/video_state_tracker.c"

#ifdef HAVE_PYTHON
#include "../gfx/video_state_python.c"
#endif

/*============================================================
FIFO BUFFER
============================================================ */
#include "../libretro-common/queues/fifo_buffer.c"

/*============================================================
AUDIO RESAMPLER
============================================================ */
#include "../audio/audio_resampler_driver.c"
#include "../audio/drivers_resampler/sinc.c"
#include "../audio/drivers_resampler/nearest.c"
#include "../audio/drivers_resampler/cc_resampler.c"

/*============================================================
CAMERA
============================================================ */
#if defined(ANDROID)
#include "../camera/drivers/android.c"
#elif defined(EMSCRIPTEN)
#include "../camera/drivers/rwebcam.c"
#endif

#ifdef HAVE_V4L2
#include "../camera/drivers/video4linux2.c"
#endif

#include "../camera/drivers/nullcamera.c"

/*============================================================
LOCATION
============================================================ */
#if defined(ANDROID)
#include "../location/drivers/android.c"
#endif

#include "../location/drivers/nulllocation.c"

/*============================================================
RSOUND
============================================================ */
#ifdef HAVE_RSOUND
#include "../audio/librsound.c"
#include "../audio/drivers/rsound.c"
#endif

/*============================================================
AUDIO
============================================================ */
#if defined(__CELLOS_LV2__)
#include "../audio/drivers/ps3_audio.c"
#elif defined(XENON)
#include "../audio/drivers/xenon360_audio.c"
#elif defined(GEKKO)
#include "../audio/drivers/gx_audio.c"
#elif defined(EMSCRIPTEN)
#include "../audio/drivers/rwebaudio.c"
#elif defined(PSP)
#include "../audio/drivers/psp1_audio.c"
#endif

#ifdef HAVE_XAUDIO
#include "../audio/drivers/xaudio.c"
#include "../audio/drivers/xaudio-c.cpp"
#endif

#ifdef HAVE_DSOUND
#include "../audio/drivers/dsound.c"
#endif

#ifdef HAVE_SL
#include "../audio/drivers/opensl.c"
#endif

#ifdef HAVE_ALSA
#ifdef __QNX__
#include "../audio/drivers/alsa_qsa.c"
#else
#include "../audio/drivers/alsa.c"
#include "../audio/drivers/alsathread.c"
#endif
#endif

#ifdef HAVE_AL
#include "../audio/drivers/openal.c"
#endif

#ifdef HAVE_COREAUDIO
#include "../audio/drivers/coreaudio.c"
#endif

#include "../audio/drivers/nullaudio.c"

/*============================================================
DRIVERS
============================================================ */
#include "../gfx/video_driver.c"
#include "../gfx/video_monitor.c"
#include "../gfx/video_pixel_converter.c"
#include "../gfx/video_viewport.c"
#include "../input/input_driver.c"
#include "../audio/audio_driver.c"
#include "../audio/audio_monitor.c"
#include "../camera/camera_driver.c"
#include "../location/location_driver.c"
#include "../menu/menu_driver.c"
#include "../driver.c"

/*============================================================
SCALERS
============================================================ */
#include "../libretro-common/gfx/scaler/scaler_filter.c"
#include "../libretro-common/gfx/scaler/pixconv.c"
#include "../libretro-common/gfx/scaler/scaler.c"
#include "../libretro-common/gfx/scaler/scaler_int.c"

/*============================================================
FILTERS
============================================================ */

#ifdef HAVE_FILTERS_BUILTIN
#include "../gfx/video_filters/2xsai.c"
#include "../gfx/video_filters/super2xsai.c"
#include "../gfx/video_filters/supereagle.c"
#include "../gfx/video_filters/2xbr.c"
#include "../gfx/video_filters/darken.c"
#include "../gfx/video_filters/epx.c"
#include "../gfx/video_filters/scale2x.c"
#include "../gfx/video_filters/blargg_ntsc_snes.c"
#include "../gfx/video_filters/lq2x.c"
#include "../gfx/video_filters/phosphor2x.c"

#include "../audio/audio_filters/echo.c"
#include "../audio/audio_filters/eq.c"
#include "../audio/audio_filters/chorus.c"
#include "../audio/audio_filters/iir.c"
#include "../audio/audio_filters/panning.c"
#include "../audio/audio_filters/phaser.c"
#include "../audio/audio_filters/reverb.c"
#include "../audio/audio_filters/wahwah.c"
#endif
/*============================================================
DYNAMIC
============================================================ */
#include "../dynamic.c"
#include "../dynamic_dummy.c"
#include "../gfx/video_filter.c"
#include "../audio/audio_dsp_filter.c"


/*============================================================
FILE
============================================================ */
#include "../content.c"
#include "../libretro-common/file/file_path.c"
#include "../libretro-common/file/dir_list.c"
#include "../libretro-common/string/string_list.c"
#include "../file_ops.c"
#include "../libretro-common/file/nbio/nbio_stdio.c"
#include "../libretro-common/file/file_list.c"

/*============================================================
MESSAGE
============================================================ */
#include "../libretro-common/queues/message_queue.c"

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
FRONTEND
============================================================ */

#include "../frontend/frontend_driver.c"

#if defined(__CELLOS_LV2__)
#include "../frontend/drivers/platform_ps3.c"
#elif defined(GEKKO)
#include "../frontend/drivers/platform_gx.c"
#ifdef HW_RVL
#include "../frontend/drivers/platform_wii.c"
#endif
#elif defined(_XBOX)
#include "../frontend/drivers/platform_xdk.c"
#elif defined(PSP)
#include "../frontend/drivers/platform_psp.c"
#elif defined(__QNX__)
#include "../frontend/drivers/platform_qnx.c"
#elif defined(OSX) || defined(IOS)
#include "../frontend/drivers/platform_apple.c"
#elif defined(ANDROID)
#include "../frontend/drivers/platform_android.c"
#endif
#include "../frontend/drivers/platform_null.c"

#include "../core_info.c"

/*============================================================
MAIN
============================================================ */
#if defined(XENON)
#include "../frontend/frontend_xenon.c"
#else
#include "../frontend/frontend.c"
#endif

/*============================================================
RETROARCH
============================================================ */
#include "../libretro_version_1.c"
#include "../retroarch.c"
#include "../runloop.c"

/*============================================================
RECORDING
============================================================ */
#include "../movie.c"
#include "../record/record_driver.c"

/*============================================================
THREAD
============================================================ */
#if defined(HAVE_THREADS) && defined(XENON)
#include "../thread/xenon_sdl_threads.c"
#elif defined(HAVE_THREADS)
#include "../libretro-common/rthreads/rthreads.c"
#include "../gfx/video_thread_wrapper.c"
#include "../audio/audio_thread_wrapper.c"
#include "../autosave.c"
#endif


/*============================================================
NETPLAY
============================================================ */
#ifdef HAVE_NETPLAY
#include "../netplay.c"
#include "../net_compat.c"
#include "../net_http.c"
#endif

/*============================================================
SCREENSHOTS
============================================================ */
#if defined(_XBOX1)
#include "../xdk/screenshot_xdk1.c"
#else
#include "../screenshot.c"
#endif

/*============================================================
PLAYLISTS
============================================================ */
#include "../playlist.c"

/*============================================================
MENU
============================================================ */
#ifdef HAVE_MENU
#include "../menu/menu_input.c"
#include "../menu/menu.c"
#include "../menu/menu_common_list.c"
#include "../menu/menu_setting.c"
#include "../menu/menu_list.c"
#include "../menu/menu_entries.c"
#include "../menu/menu_entries_cbs.c"
#include "../menu/menu_shader.c"
#include "../menu/menu_texture.c"
#include "../menu/menu_navigation.c"
#include "../menu/menu_animation.c"
#include "../menu/menu_database.c"
#endif

#ifdef HAVE_RMENU
#include "../menu/drivers/rmenu.c"
#endif

#ifdef HAVE_RGUI
#include "../menu/drivers/rgui.c"
#endif

#ifdef HAVE_RMENU_XUI
#include "../menu/drivers/rmenu_xui.cpp"
#endif

#ifdef HAVE_OPENGL

#ifdef HAVE_XMB
#include "../menu/drivers/xmb.c"
#endif

#ifdef HAVE_GLUI
#include "../menu/drivers/glui.c"
#endif

#endif

#ifdef IOS
#include "../menu/drivers/ios.c"
#endif

#ifdef HAVE_COMMAND
#include "../command.c"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
DEPENDENCIES
============================================================ */
#ifdef WANT_MINIZ
#include "../deps/rzlib/adler32.c"
#include "../deps/rzlib/compress.c"
#include "../deps/rzlib/crc32.c"
#include "../deps/rzlib/deflate.c"
#include "../deps/rzlib/gzclose.c"
#include "../deps/rzlib/gzlib.c"
#include "../deps/rzlib/gzread.c"
#include "../deps/rzlib/gzwrite.c"
#include "../deps/rzlib/inffast.c"
#include "../deps/rzlib/inflate.c"
#include "../deps/rzlib/inftrees.c"
#include "../deps/rzlib/trees.c"
#include "../deps/rzlib/uncompr.c"
#include "../deps/rzlib/zutil.c"
#endif

/* Decompression support always requires the next two files */
#if defined(WANT_MINIZ) || defined(HAVE_ZLIB)
#include "../deps/rzlib/ioapi.c"
#include "../deps/rzlib/unzip.c"
#endif

#ifdef HAVE_7ZIP
#include "../deps/7zip/7zIn.c"
#include "../deps/7zip/7zAlloc.c"
#include "../deps/7zip/Bra86.c"
#include "../deps/7zip/CpuArch.c"
#include "../deps/7zip/7zFile.c"
#include "../deps/7zip/7zStream.c"
#include "../deps/7zip/7zBuf2.c"
#include "../deps/7zip/LzmaDec.c"
#include "../deps/7zip/7zCrcOpt.c"
#include "../deps/7zip/Bra.c"
#include "../deps/7zip/7zDec.c"
#include "../deps/7zip/Bcj2.c"
#include "../deps/7zip/7zCrc.c"
#include "../deps/7zip/Lzma2Dec.c"
#include "../deps/7zip/7zBuf.c"
#include "../decompress/7zip_support.c"
#endif

/*============================================================
XML
============================================================ */
#if 0
#ifndef HAVE_LIBXML2
#define RXML_LIBXML2_COMPAT
#include "../libretro-common/formats/xml/rxml.c"
#endif
#endif

/*============================================================
 SETTINGS
============================================================ */
#include "../settings_list.c"
#include "../settings_data.c"

/*============================================================
 AUDIO UTILS
============================================================ */
#include "../audio/audio_utils.c"

/*============================================================
 LIBRETRODB
============================================================ */
#ifdef HAVE_LIBRETRODB
#include "../libretro-db/bintree.c"
#include "../libretro-db/libretrodb.c"
#include "../libretro-db/rmsgpack.c"
#include "../libretro-db/rmsgpack_dom.c"
#include "../libretro-db/query.c"
#include "../database_info.c"
#endif

#ifdef __cplusplus
}
#endif
