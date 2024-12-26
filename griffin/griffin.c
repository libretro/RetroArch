/* RetroArch - A frontend for libretro.
* Copyright (C) 2010-2014 - Hans-Kristian Arntzen
* Copyright (C) 2011-2017 - Daniel De Matteis
* Copyright (C) 2016-2019 - Brad Parker
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
#define VFS_FRONTEND
#include <retro_environment.h>

#define CINTERFACE

#define HAVE_IBXM 1

#if defined(HAVE_ZLIB) || defined(HAVE_7ZIP)
#define HAVE_COMPRESSION 1
#endif

#if defined(HAVE_OPENGL) && defined(HAVE_ANGLE)
#ifndef HAVE_OPENGLES
#define HAVE_OPENGLES  1
#endif
#if !defined(HAVE_OPENGLES3) && !defined(HAVE_OPENGLES2)
#define HAVE_OPENGLES3 1
#endif
#ifndef HAVE_EGL
#define HAVE_EGL       1
#endif
#endif

#ifndef _XBOX
#if defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER >= 1500

#ifndef HAVE_MMAP_WIN32
#define HAVE_MMAP_WIN32
#endif

#elif !defined(_MSC_VER)

#ifndef HAVE_MMAP_WIN32
#define HAVE_MMAP_WIN32
#endif
#endif

#endif
#endif

#if _MSC_VER && !defined(__WINRT__)
#include "../libretro-common/compat/compat_snprintf.c"
#endif

#include "../verbosity.c"

#if defined(HAVE_LOGGER) && !defined(ANDROID)
#include "../network/net_logger.c"
#endif

/*============================================================
COMPATIBILITY
============================================================ */
#ifndef HAVE_GETOPT_LONG
#include "../compat/compat_getopt.c"
#endif

#ifndef HAVE_STRCASESTR
#include "../compat/compat_strcasestr.c"
#endif

#ifndef HAVE_STRL
#include "../compat/compat_strl.c"
#endif

#if defined(_WIN32)
#include "../compat/compat_posix_string.c"
#endif

#if defined(WANT_IFADDRS)
#include "../compat/compat_ifaddrs.c"
#endif

#include "../libretro-common/compat/compat_fnmatch.c"
#include "../libretro-common/compat/compat_strldup.c"
#include "../libretro-common/compat/fopen_utf8.c"
#include "../libretro-common/memmap/memalign.c"

/*============================================================
CONSOLE EXTENSIONS
============================================================ */
#ifdef RARCH_CONSOLE

#ifdef HW_DOL
#include "../memory/ngc/ssaram.c"
#endif

#ifdef INTERNAL_LIBOGC
#include "../wii/libogc/libfat/cache.c"
#include "../wii/libogc/libfat/directory.c"
#include "../wii/libogc/libfat/disc.c"
#include "../wii/libogc/libfat/fatdir.c"
#include "../wii/libogc/libfat/fatfile.c"
#include "../wii/libogc/libfat/file_allocation_table.c"
#include "../wii/libogc/libfat/filetime.c"
#include "../wii/libogc/libfat/libfat.c"
#include "../wii/libogc/libfat/lock.c"
#include "../wii/libogc/libfat/partition.c"
#endif

#endif

/*============================================================
ALGORITHMS
============================================================ */

/*============================================================
ARCHIVE FILE
============================================================ */
#include "../libretro-common/file/archive_file.c"

#ifdef HAVE_ZLIB
#include "../libretro-common/file/archive_file_zlib.c"
#endif

#ifdef HAVE_7ZIP
#include "../libretro-common/file/archive_file_7z.c"
#endif

/*============================================================
COMPRESSION
============================================================ */
#include "../libretro-common/streams/stdin_stream.c"
#include "../libretro-common/streams/trans_stream.c"
#include "../libretro-common/streams/trans_stream_pipe.c"

#ifdef HAVE_ZLIB
#include "../libretro-common/streams/trans_stream_zlib.c"
#include "../libretro-common/streams/rzip_stream.c"
#endif

/*============================================================
ENCODINGS
============================================================ */
#include "../libretro-common/encodings/encoding_utf.c"
#include "../libretro-common/encodings/encoding_crc32.c"
#include "../libretro-common/encodings/encoding_base64.c"

/*============================================================
PERFORMANCE
============================================================ */
#include "../libretro-common/features/features_cpu.c"

/*============================================================
CONFIG FILE
============================================================ */
#if defined(_MSC_VER)
#undef __LIBRETRO_SDK_COMPAT_POSIX_STRING_H
#undef __LIBRETRO_SDK_COMPAT_MSVC_H
#undef strcasecmp
#endif

#ifdef HAVE_CONFIGFILE
#include "../libretro-common/file/config_file.c"
#include "../libretro-common/file/config_file_userdata.c"
#endif

/*============================================================
CONTENT METADATA RECORDS
============================================================ */
#include "../runtime_file.c"
#include "../disk_index_file.c"

/*============================================================
ACHIEVEMENTS
============================================================ */
#if defined(HAVE_CHEEVOS)
#if !defined(HAVE_NETWORKING)
#include "../libretro-common/net/net_http.c"
#endif

/* rcheevos doesn't actually spawn and manage threads, RC_NO_THREADS
 * simply disables the mutexes that provide thread safety. */
#if !defined(HAVE_THREADS)
#define RC_NO_THREADS 1
#elif defined(GEKKO) || defined(_3DS)
 /* Gekko (Wii) and 3DS use custom pthread wrappers (see rthreads.c) */
#define RC_NO_THREADS 1
#endif

#include "../libretro-common/formats/cdfs/cdfs.c"
#include "../network/net_http_special.c"

#include "../cheevos/cheevos.c"
#include "../cheevos/cheevos_client.c"
#include "../cheevos/cheevos_menu.c"

#include "../deps/rcheevos/src/rc_client.c"
#include "../deps/rcheevos/src/rc_compat.c"
#include "../deps/rcheevos/src/rc_libretro.c"
#include "../deps/rcheevos/src/rc_util.c"
#include "../deps/rcheevos/src/rapi/rc_api_common.c"
#include "../deps/rcheevos/src/rapi/rc_api_info.c"
#include "../deps/rcheevos/src/rapi/rc_api_runtime.c"
#include "../deps/rcheevos/src/rapi/rc_api_user.c"
#include "../deps/rcheevos/src/rcheevos/alloc.c"
#include "../deps/rcheevos/src/rcheevos/condition.c"
#include "../deps/rcheevos/src/rcheevos/condset.c"
#include "../deps/rcheevos/src/rcheevos/consoleinfo.c"
#include "../deps/rcheevos/src/rcheevos/format.c"
#include "../deps/rcheevos/src/rcheevos/lboard.c"
#include "../deps/rcheevos/src/rcheevos/memref.c"
#include "../deps/rcheevos/src/rcheevos/operand.c"
#include "../deps/rcheevos/src/rcheevos/richpresence.c"
#include "../deps/rcheevos/src/rcheevos/runtime.c"
#include "../deps/rcheevos/src/rcheevos/runtime_progress.c"
#include "../deps/rcheevos/src/rcheevos/trigger.c"
#include "../deps/rcheevos/src/rcheevos/value.c"
#include "../deps/rcheevos/src/rhash/cdreader.c"
#include "../deps/rcheevos/src/rhash/hash.c"

#endif

/*============================================================
MD5
============================================================ */
#include "../libretro-common/utils/md5.c"

/*============================================================
CHEATS
============================================================ */
#ifdef HAVE_CHEATS
#include "../cheat_manager.c"
#endif
#include "../libretro-common/hash/lrc_hash.c"

#include "../gfx/video_driver.c"
/*============================================================
UI COMMON CONTEXT
============================================================ */
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include "../gfx/common/win32_common.c"
#endif

/*============================================================
VIDEO CONTEXT
============================================================ */
#include "../gfx/drivers_context/gfx_null_ctx.c"

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_VULKAN) || defined(HAVE_OPENGLES) || defined(HAVE_OPENGL_CORE)
#include "../gfx/common/gl_common.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_VULKAN) || defined(HAVE_OPENGLES)
#include "../gfx/drivers_context/wgl_ctx.c"
#endif
#if defined(HAVE_VULKAN)
#include "../gfx/drivers_context/w_vk_ctx.c"
#endif

#include "../gfx/display_servers/dispserv_win32.c"

#if defined(HAVE_FFMPEG)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
#include "../cores/libretro-ffmpeg/ffmpeg_fft.c"
#endif
#endif

#endif

#if defined(ANDROID)
#include "../gfx/drivers_context/android_ctx.c"
#if defined(HAVE_VULKAN)
#include "../gfx/drivers_context/android_vk_ctx.c"
#endif
#include "../gfx/display_servers/dispserv_android.c"
#elif defined(__QNX__)
#include "../gfx/drivers_context/qnx_ctx.c"
#elif defined(EMSCRIPTEN)
#include "../gfx/drivers_context/emscriptenegl_ctx.c"
#elif defined(__PS3__)
#include "../gfx/drivers_context/ps3_ctx.c"
#endif

#if defined(HAVE_VIVANTE_FBDEV)
#include "../gfx/drivers_context/vivante_fbdev_ctx.c"
#endif

#if defined(HAVE_OPENDINGUX_FBDEV)
#include "../gfx/drivers_context/opendingux_fbdev_ctx.c"
#endif

#ifdef HAVE_WAYLAND
#include "../gfx/drivers_context/wayland_ctx.c"
#ifdef HAVE_VULKAN
#include "../gfx/drivers_context/wayland_vk_ctx.c"
#endif
#endif

#ifdef HAVE_DRM
#include "../gfx/common/drm_common.c"
#endif

#ifdef HAVE_VULKAN
#include "../gfx/common/vulkan_common.c"
#include "../libretro-common/vulkan/vulkan_symbol_wrapper.c"
#ifdef HAVE_VULKAN_DISPLAY
#include "../gfx/drivers_context/khr_display_ctx.c"
#endif
#endif

#if defined(HAVE_KMS)
#include "../gfx/drivers_context/drm_ctx.c"
#include "../gfx/display_servers/dispserv_kms.c"
#endif

#if defined(HAVE_EGL)
#include "../gfx/common/egl_common.c"

#if defined(HAVE_VIDEOCORE)
#include "../gfx/drivers_context/vc_egl_ctx.c"
#endif

#if defined(_WIN32) && defined(HAVE_ANGLE)
#include "../gfx/common/angle_common.c"
#endif

#if defined(__WINRT__)
#include "../gfx/drivers_context/uwp_egl_ctx.c"
#endif

#endif

#if defined(HAVE_X11)
#include "../gfx/common/x11_common.c"
#include "../gfx/common/xinerama_common.c"
#include "../gfx/display_servers/dispserv_x11.c"

#ifdef HAVE_DBUS
#include "../gfx/common/dbus_common.c"
#endif

#ifndef HAVE_OPENGLES
#include "../gfx/drivers_context/x_ctx.c"
#endif

#ifdef HAVE_VULKAN
#include "../gfx/drivers_context/x_vk_ctx.c"
#endif

#ifdef HAVE_EGL
#include "../gfx/drivers_context/xegl_ctx.c"
#endif

#endif

/*============================================================
VIDEO SHADERS
============================================================ */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL) || defined(HAVE_SLANG)
#include "../gfx/video_shader_parse.c"
#endif

#ifdef HAVE_SLANG
#include "../gfx/drivers_shader/glslang_util.c"
#endif

#ifdef HAVE_CG
#ifdef HAVE_OPENGL
#include "../gfx/drivers_shader/shader_gl_cg.c"
#endif
#endif

#ifdef HAVE_GLSL
#include "../gfx/drivers_shader/shader_glsl.c"
#endif

/*============================================================
VIDEO IMAGE
============================================================ */

#include "../libretro-common/formats/image_texture.c"

#ifdef HAVE_RTGA
#include "../libretro-common/formats/tga/rtga.c"
#endif

#ifdef HAVE_IMAGEVIEWER
#include "../cores/libretro-imageviewer/image_core.c"
#endif

#include "../libretro-common/formats/image_transfer.c"
#ifdef HAVE_RPNG
#include "../libretro-common/formats/png/rpng.c"
#include "../libretro-common/formats/png/rpng_encode.c"
#endif
#ifdef HAVE_RJPEG
#include "../libretro-common/formats/jpeg/rjpeg.c"
#endif
#ifdef HAVE_RBMP
#include "../libretro-common/formats/bmp/rbmp.c"
#endif

#include "../libretro-common/formats/bmp/rbmp_encode.c"
#ifdef HAVE_RWAV
#include "../libretro-common/formats/wav/rwav.c"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */
#if defined(HAVE_D3D)
#include "../gfx/common/d3d_common.c"

#if defined(HAVE_D3D8)
#include "../gfx/drivers/d3d8.c"
#endif

#if defined(HAVE_D3D9)
#include "../gfx/common/d3d9_common.c"

#ifdef HAVE_HLSL
#include "../gfx/drivers/d3d9hlsl.c"
#endif

#ifdef HAVE_CG
#include "../gfx/drivers/d3d9cg.c"
#endif

#endif

#endif

#if defined(HAVE_D3D11)
#include "../gfx/drivers/d3d11.c"
#include "../gfx/common/d3d11_common.c"
#endif

#if defined(HAVE_D3D12)
#include "../gfx/drivers/d3d12.c"
#include "../gfx/common/d3d12_common.c"
#endif

#if defined(HAVE_D3D10)
#include "../gfx/drivers/d3d10.c"
#include "../gfx/common/d3d10_common.c"
#endif

#if defined(HAVE_D3D10) || defined(HAVE_D3D11) || defined(HAVE_D3D12)
#include "../gfx/common/d3dcompiler_common.c"
#include "../gfx/common/dxgi_common.c"
#endif

#if defined(GEKKO)
#ifdef HW_RVL
#include "../gfx/drivers/gx_gfx_vi_encoder.c"
#include "../memory/wii/mem2_manager.c"
#endif
#endif

#if defined(__wiiu__)
#include "../gfx/drivers/gx2_gfx.c"
#endif

#ifdef HAVE_SDL2
#include "../gfx/drivers/sdl2_gfx.c"
#include "../gfx/common/sdl2_common.c"
#endif

#if defined(DINGUX) && defined(HAVE_SDL_DINGUX)
#if defined(RS90) || defined(MIYOO)
#include "../gfx/drivers/sdl_rs90_gfx.c"
#else
#include "../gfx/drivers/sdl_dingux_gfx.c"
#endif
#endif

#ifdef HAVE_VG
#include "../gfx/drivers/vg.c"
#endif

#ifdef HAVE_OMAP
#include "../gfx/drivers/omap_gfx.c"
#endif

#ifdef HAVE_VULKAN
#include "../gfx/drivers/vulkan.c"
#endif

#if defined(HAVE_PLAIN_DRM)
#include "../gfx/drivers/drm_gfx.c"
#endif

#ifdef HAVE_OPENGL1
#include "../gfx/drivers/gl1.c"
#endif

#ifdef HAVE_OPENGL_CORE
#include "../gfx/drivers/gl3.c"
#endif

#ifdef HAVE_OPENGL
#include "../gfx/drivers/gl2.c"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
#include "../libretro-common/gfx/gl_capabilities.c"

#ifndef HAVE_PSGL
#include "../libretro-common/glsym/rglgen.c"
#if defined(HAVE_OPENGLES2)
#include "../libretro-common/glsym/glsym_es2.c"
#elif defined(HAVE_OPENGLES3)
#include "../libretro-common/glsym/glsym_es3.c"
#else
#include "../libretro-common/glsym/glsym_gl.c"
#endif
#endif

#endif

#ifdef HAVE_XVIDEO
#include "../gfx/drivers/xvideo.c"
#endif

#if defined(HAVE_GCM)
#include "../gfx/drivers/rsx_gfx.c"
#elif defined(GEKKO)
#include "../gfx/drivers/gx_gfx.c"
#elif defined(PSP)
#include "../gfx/drivers/psp1_gfx.c"
#elif defined(PS2)
#include "../gfx/drivers/ps2_gfx.c"
#elif defined(HAVE_VITA2D)
#include "../deps/libvita2d/source/vita2d.c"
#include "../deps/libvita2d/source/vita2d_texture.c"
#include "../deps/libvita2d/source/vita2d_draw.c"
#include "../deps/libvita2d/source/utils.c"

#include "../gfx/drivers/vita2d_gfx.c"
#elif defined(_3DS)
#include "../gfx/drivers/ctr_gfx.c"
#elif defined(XENON)
#include "../gfx/drivers/xenon360_gfx.c"
#elif defined(DJGPP)
#include "../gfx/drivers/vga_gfx.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
#include "../gfx/drivers/gdi_gfx.c"
#endif
#endif

#include "../deps/ibxm/ibxm.c"

/*============================================================
FONTS
============================================================ */

#include "../gfx/drivers_font_renderer/bitmapfont.c"

#ifdef HAVE_LANGEXTRA
#include "../gfx/drivers_font_renderer/bitmapfont_10x10.c"
#include "../gfx/drivers_font_renderer/bitmapfont_6x10.c"
#endif

#include "../gfx/font_driver.c"

#if defined(HAVE_D3D9) && defined(HAVE_D3DX)
#include "../gfx/drivers_font/d3d9x_w32_font.c"
#endif

#if defined(HAVE_STB_FONT)
#include "../gfx/drivers_font_renderer/stb_unicode.c"
#include "../gfx/drivers_font_renderer/stb.c"
#endif

#if defined(HAVE_FREETYPE)
#include "../gfx/drivers_font_renderer/freetype.c"
#endif

#if defined(__APPLE__) && defined(HAVE_CORETEXT)
#include "../gfx/drivers_font_renderer/coretext.c"
#endif

/*============================================================
INPUT
============================================================ */

#include "../input/input_driver.c"
#include "../input/input_keymaps.c"
#include "../tasks/task_autodetect.c"
#include "../input/input_autodetect_builtin.c"

#ifdef HAVE_BLISSBOX
#include "../tasks/task_autodetect_blissbox.c"
#endif

#ifdef HAVE_AUDIOMIXER
#include "../tasks/task_audio_mixer.c"
#endif

#ifdef HAVE_OVERLAY
#include "../led/drivers/led_overlay.c"
#include "../tasks/task_overlay.c"
#endif

#ifdef HAVE_X11
#include "../input/common/input_x11_common.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
#ifdef HAVE_WINRAWINPUT
/* winraw only available since XP */
#include "../input/drivers/winraw_input.c"
#endif
#endif

#if defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
#include "../input/drivers/psp_input.c"
#include "../input/drivers_joypad/psp_joypad.c"
#elif defined(PS2)
#include "../input/drivers/ps2_input.c"
#include "../input/drivers_joypad/ps2_joypad.c"
#elif defined(__PS3__)
#include "../input/drivers/ps3_input.c"
#include "../input/drivers_joypad/ps3_joypad.c"
#elif defined(ORBIS)
#include "../input/drivers/ps4_input.c"
#include "../input/drivers_joypad/ps4_joypad.c"
#elif defined(_3DS)
#include "../input/drivers/ctr_input.c"
#include "../input/drivers_joypad/ctr_joypad.c"
#elif defined(GEKKO)
#include "../input/drivers/gx_input.c"
#include "../input/drivers_joypad/gx_joypad.c"
#elif defined(__wiiu__)
#include "../input/common/hid/hid_device_driver.c"
#include "../input/common/hid/device_wiiu_gca.c"
#include "../input/common/hid/device_ds3.c"
#include "../input/common/hid/device_ds4.c"
#include "../input/common/hid/device_null.c"
#include "../input/drivers/wiiu_input.c"
#include "../input/drivers_joypad/wiiu_joypad.c"
#include "../input/drivers_joypad/wiiu/hidpad_driver.c"
#include "../input/drivers_joypad/wiiu/kpad_driver.c"
#include "../input/drivers_joypad/wiiu/wpad_driver.c"
#include "../input/drivers_joypad/wiiu/pad_functions.c"
#elif defined(_XBOX)
#include "../input/drivers/xdk_xinput_input.c"
#ifdef _XBOX1
#include "../input/drivers_joypad/xdk_joypad.c"
#endif
#elif defined(XENON)
#include "../input/drivers/xenon360_input.c"
#elif defined(ANDROID)
#include "../input/drivers/android_input.c"
#include "../input/drivers_joypad/android_joypad.c"
#elif defined(__QNX__)
#include "../input/drivers/qnx_input.c"
#include "../input/drivers_joypad/qnx_joypad.c"
#elif defined(EMSCRIPTEN)
#include "../input/drivers/rwebinput_input.c"
#include "../input/drivers_joypad/rwebpad_joypad.c"
#elif defined(DJGPP)
#include "../input/drivers/dos_input.c"
#include "../input/drivers_joypad/dos_joypad.c"
#elif defined(__WINRT__)
#include "../input/drivers/xdk_xinput_input.c"
#include "../input/drivers/uwp_input.c"
#elif defined(DINGUX) && defined(HAVE_SDL_DINGUX)
#include "../input/drivers/sdl_dingux_input.c"
#include "../input/drivers_joypad/sdl_dingux_joypad.c"
#endif

#ifdef HAVE_WAYLAND
#include "../input/common/wayland_common.c"
#include "../input/drivers/wayland_input.c"
#endif

#ifdef HAVE_DINPUT
#include "../input/drivers/dinput.c"
#include "../input/drivers_joypad/dinput_joypad.c"
#endif

#ifdef HAVE_XINPUT
#ifdef HAVE_DINPUT
#include "../input/drivers_joypad/xinput_hybrid_joypad.c"
#else
#include "../input/drivers_joypad/xinput_joypad.c"
#endif
#endif

#if defined(__linux__) && !defined(ANDROID)
#include "../input/common/linux_common.c"
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

#if defined(HAVE_LIBSHAKE)
#include "../deps/libShake/src/common/error.c"
#include "../deps/libShake/src/common/helpers.c"
#include "../deps/libShake/src/common/presets.c"
#if defined(OSX)
#include "../deps/libShake/src/osx/shake.c"
#elif defined(__linux__) || (defined(BSD) && !defined(__MACH__))
#include "../deps/libShake/src/linux/shake.c"
#endif
#endif

/*============================================================
INPUT (HID)
============================================================ */
#ifdef HAVE_HID
#include "../input/common/input_hid_common.c"
#include "../input/drivers_joypad/hid_joypad.c"

#if defined(HAVE_LIBUSB) && defined(HAVE_THREADS)
#include "../input/drivers_hid/libusb_hid.c"
#endif

#ifdef HAVE_BTSTACK
#include "../input/drivers_hid/btstack_hid.c"
#endif

#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
#include "../input/drivers_hid/iohidmanager_hid.c"
#endif

#ifdef HAVE_WIIUSB_HID
#include "../input/drivers_hid/wiiusb_hid.c"
#endif

#include "../input/connect/joypad_connection.c"
#include "../input/connect/connect_ps3.c"
#include "../input/connect/connect_ps4.c"
#include "../input/connect/connect_wii.c"
#include "../input/connect/connect_wiiupro.c"
#include "../input/connect/connect_snesusb.c"
#include "../input/connect/connect_nesusb.c"
#include "../input/connect/connect_wiiugca.c"
#include "../input/connect/connect_ps2adapter.c"
#include "../input/connect/connect_psxadapter.c"
#include "../input/connect/connect_retrode.c"
#include "../input/connect/connect_ps4_hori_mini.c"
#include "../input/connect/connect_kade.c"
#include "../input/connect/connect_zerodelay_dragonrise.c"
#endif

/*============================================================
 KEYBOARD EVENT
 ============================================================ */
#ifdef HAVE_XKBCOMMON
#include "../input/drivers_keyboard/keyboard_event_xkb.c"
#endif

/*============================================================
FIFO BUFFER
============================================================ */
#include "../libretro-common/queues/fifo_queue.c"

/*============================================================
AUDIO RESAMPLER
============================================================ */
#include "../libretro-common/audio/resampler/audio_resampler.c"
#include "../libretro-common/audio/resampler/drivers/sinc_resampler.c"
#ifdef HAVE_NEAREST_RESAMPLER
#include "../libretro-common/audio/resampler/drivers/nearest_resampler.c"
#endif
#ifdef HAVE_CC_RESAMPLER
#include "../audio/drivers_resampler/cc_resampler.c"
#endif

/*============================================================
CAMERA
============================================================ */
#include "../camera/camera_driver.c"
#if defined(ANDROID)
#include "../camera/drivers/android.c"
#elif defined(EMSCRIPTEN)
#include "../camera/drivers/rwebcam.c"
#endif

#ifdef HAVE_V4L2
#include "../camera/drivers/video4linux2.c"
#endif

#ifdef HAVE_VIDEOPROCESSOR
#include "../cores/libretro-video-processor/video_processor_v4l2.c"
#endif

/*============================================================
LEDS
============================================================ */

#include "../led/led_driver.c"

#if defined(HAVE_RPILED)
#include "../led/drivers/led_rpi.c"
#include "../led/drivers/led_sys_linux.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include "../led/drivers/led_win32_keyboard.c"
#endif

/*============================================================
LOCATION
============================================================ */
#if defined(ANDROID)
#include "../location/drivers/android.c"
#endif

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
#include "../audio/audio_driver.c"
#ifdef HAVE_MICROPHONE
#include "../audio/microphone_driver.c"
#endif
#if defined(__PS3__) || defined (__PSL1GHT__)
#include "../audio/drivers/ps3_audio.c"
#elif defined(XENON)
#include "../audio/drivers/xenon360_audio.c"
#elif defined(GEKKO)
#include "../audio/drivers/gx_audio.c"
#elif defined(__wiiu__)
#include "../audio/drivers/wiiu_audio.c"
#elif defined(EMSCRIPTEN)
#include "../audio/drivers/rwebaudio.c"
#elif defined(PSP) || defined(VITA) || defined(ORBIS)
#include "../audio/drivers/psp_audio.c"
#elif defined(PS2)
#include "../audio/drivers/ps2_audio.c"
#elif defined(_3DS)
#include "../audio/drivers/ctr_csnd_audio.c"
#include "../audio/drivers/ctr_dsp_audio.c"
#ifdef HAVE_THREADS
#include "../audio/drivers/ctr_dsp_thread_audio.c"
#endif
#endif

#ifdef HAVE_XAUDIO
#include "../audio/drivers/xaudio.c"
#endif

#if defined(HAVE_SDL2)
#include "../audio/drivers/sdl_audio.c"
#ifdef HAVE_MICROPHONE
#include "../audio/drivers_microphone/sdl_microphone.c"
#endif
#endif

#ifdef HAVE_DSOUND
#include "../audio/drivers/dsound.c"
#endif

#ifdef HAVE_WASAPI
#include "../audio/drivers/wasapi.c"
#include "../audio/common/wasapi.c"

#ifdef HAVE_MICROPHONE
#include "../audio/drivers_microphone/wasapi.c"
#endif
#endif

#ifdef HAVE_SL
#include "../audio/drivers/opensl.c"
#endif

#ifdef HAVE_ALSA
#ifdef __QNX__
#include "../audio/drivers/alsa_qsa.c"
#else
#include "../audio/drivers/alsa.c"
#include "../audio/common/alsa.c"
#include "../audio/drivers/alsathread.c"
#include "../audio/common/alsathread.c"

#ifdef HAVE_MICROPHONE
#include "../audio/drivers_microphone/alsa.c"
#include "../audio/drivers_microphone/alsathread.c"
#endif
#endif
#endif

#ifdef HAVE_TINYALSA
#include "../audio/drivers/tinyalsa.c"
#endif

#ifdef HAVE_PULSE
#include "../audio/drivers/pulse.c"
#endif

#ifdef HAVE_AL
#include "../audio/drivers/openal.c"
#endif

#ifdef HAVE_COREAUDIO
#include "../audio/drivers/coreaudio.c"
#endif

#if defined(HAVE_WASAPI) || ((_WIN32_WINNT >= 0x0602) && !defined(__WINRT__))
#include "../audio/common/mmdevice_common.c"
#endif

/*============================================================
MIDI
============================================================ */
#ifdef HAVE_WINMM
#include "../midi/drivers/winmm_midi.c"
#endif

/*============================================================
DRIVERS
============================================================ */
#ifdef HAVE_CRTSWITCHRES
#include "../gfx/video_crt_switch.c"
#endif
#include "../gfx/gfx_animation.c"
#include "../gfx/gfx_display.c"
#include "../gfx/gfx_thumbnail_path.c"
#include "../gfx/gfx_thumbnail.c"
#ifdef HAVE_AUDIOMIXER
#include "../libretro-common/audio/audio_mixer.c"
#endif

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
#ifdef HAVE_VIDEO_FILTER
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
#include "../gfx/video_filters/normal2x.c"
#include "../gfx/video_filters/normal2x_width.c"
#include "../gfx/video_filters/normal2x_height.c"
#include "../gfx/video_filters/normal4x.c"
#include "../gfx/video_filters/scanline2x.c"
#include "../gfx/video_filters/grid2x.c"
#include "../gfx/video_filters/grid3x.c"
#include "../gfx/video_filters/gameboy3x.c"
#include "../gfx/video_filters/gameboy4x.c"
#include "../gfx/video_filters/dot_matrix_3x.c"
#include "../gfx/video_filters/dot_matrix_4x.c"
#include "../gfx/video_filters/upscale_1_5x.c"
#include "../gfx/video_filters/upscale_256x_320x240.c"
#include "../gfx/video_filters/picoscale_256x_320x240.c"
#include "../gfx/video_filters/upscale_240x160_320x240.c"
#include "../gfx/video_filters/upscale_mix_240x160_320x240.c"
#endif

#ifdef HAVE_DSP_FILTER
#include "../libretro-common/audio/dsp_filters/echo.c"
#include "../libretro-common/audio/dsp_filters/eq.c"
#include "../libretro-common/audio/dsp_filters/chorus.c"
#include "../libretro-common/audio/dsp_filters/iir.c"
#include "../libretro-common/audio/dsp_filters/panning.c"
#include "../libretro-common/audio/dsp_filters/phaser.c"
#include "../libretro-common/audio/dsp_filters/reverb.c"
#include "../libretro-common/audio/dsp_filters/wahwah.c"
#endif
#endif

/*============================================================
DYNAMIC
============================================================ */
#include "../libretro-common/dynamic/dylib.c"
#ifdef HAVE_VIDEO_FILTER
#include "../gfx/video_filter.c"
#endif
#ifdef HAVE_DSP_FILTER
#include "../libretro-common/audio/dsp_filter.c"
#endif

/*============================================================
CORES
============================================================ */
#ifdef HAVE_FFMPEG
#include "../cores/libretro-ffmpeg/ffmpeg_core.c"
#endif

#if defined(HAVE_MPV)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../cores/libretro-mpv/mpv-libretro.c"
#endif
#endif

#include "../cores/dynamic_dummy.c"

/*============================================================
FILE
============================================================ */
#include "../libretro-common/file/file_path.c"
#include "../libretro-common/file/file_path_io.c"
#include "../file_path_special.c"
#include "../libretro-common/lists/dir_list.c"
#include "../libretro-common/lists/string_list.c"
#include "../libretro-common/lists/nested_list.c"
#include "../libretro-common/lists/file_list.c"
#include "../libretro-common/file/retro_dirent.c"
#include "../libretro-common/streams/file_stream.c"
#include "../libretro-common/streams/file_stream_transforms.c"
#include "../libretro-common/streams/interface_stream.c"
#include "../libretro-common/streams/memory_stream.c"
#include "../libretro-common/streams/network_stream.c"
#ifndef __WINRT__
#include "../libretro-common/vfs/vfs_implementation.c"
#endif

#ifdef HAVE_CDROM
#include "../libretro-common/cdrom/cdrom.c"
#include "../libretro-common/vfs/vfs_implementation_cdrom.c"
#include "../libretro-common/media/media_detect_cd.c"
#endif

#include "../libretro-common/string/stdstring.c"
#include "../libretro-common/file/nbio/nbio_stdio.c"
#if defined(__linux__)
#include "../libretro-common/file/nbio/nbio_linux.c"
#endif
#if defined(HAVE_MMAP) && defined(BSD)
#include "../libretro-common/file/nbio/nbio_unixmmap.c"
#endif
#if defined(HAVE_MMAP_WIN32)
#include "../libretro-common/file/nbio/nbio_windowsmmap.c"
#endif
#include "../libretro-common/file/nbio/nbio_intf.c"

/*============================================================
MESSAGE
============================================================ */
#include "../libretro-common/queues/message_queue.c"

/*============================================================
CONFIGURATION
============================================================ */
#include "../configuration.c"

/*============================================================
STATE MANAGER
============================================================ */
#ifdef HAVE_REWIND
#include "../state_manager.c"
#endif

/*============================================================
FRONTEND
============================================================ */

#include "../frontend/frontend_driver.c"

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include "../frontend/drivers/platform_win32.c"
#endif

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#include "../frontend/drivers/platform_uwp.c"
#endif

#ifdef _XBOX
#include "../frontend/drivers/platform_xdk.c"
#endif

#if defined(GEKKO)
#include "../frontend/drivers/platform_gx.c"
#ifdef HW_RVL
#include "../frontend/drivers/platform_wii.c"
#endif
#elif defined(__wiiu__)
#include "../frontend/drivers/platform_wiiu.c"
#elif defined(PS2)
#include "../frontend/drivers/platform_ps2.c"
#elif defined(__PS3__)
#include "../frontend/drivers/platform_ps3.c"
#elif defined(ORBIS)
#include "../frontend/drivers/platform_orbis.c"
#elif defined(PSP) || defined(VITA)
#include "../frontend/drivers/platform_psp.c"
#elif defined(_3DS)
#include "../frontend/drivers/platform_ctr.c"
#elif defined(SWITCH) && defined(HAVE_LIBNX)
#include "../frontend/drivers/platform_switch.c"
#elif defined(XENON)
#include "../frontend/drivers/platform_xenon.c"
#elif defined(__QNX__)
#include "../frontend/drivers/platform_qnx.c"
#elif defined(__linux__) || (defined(BSD) && !defined(__MACH__))
#include "../frontend/drivers/platform_unix.c"
#elif defined(DJGPP)
#include "../frontend/drivers/platform_dos.c"
#endif

#if defined(DINGUX)
#include "../dingux/dingux_utils.c"
#endif

#include "../core_info.c"
#include "../core_backup.c"
#include "../core_option_manager.c"

#if defined(HAVE_NETWORKING)
#include "../core_updater_list.c"
#endif

/*============================================================
UI
============================================================ */
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include "../ui/drivers/ui_win32.c"
#endif

/*============================================================
GIT
============================================================ */

#ifdef HAVE_GIT_VERSION
#include "../version_git.c"
#endif

/*============================================================
RETROARCH
============================================================ */
#include "../retroarch.c"
#include "../runloop.c"
#ifdef HAVE_RUNAHEAD
#include "../runahead.c"
#endif
#include "../command.c"
#include "../midi_driver.c"
#include "../location_driver.c"
#include "../ui/ui_companion_driver.c"
#include "../libretro-common/queues/task_queue.c"

#include "../msg_hash.c"
#include "../intl/msg_hash_us.c"

/*============================================================
BLUETOOTH
============================================================ */
#ifdef HAVE_BLUETOOTH
#include "../bluetooth/bluetooth_driver.c"
#include "../bluetooth/drivers/bluetoothctl.c"
#ifdef HAVE_DBUS
#include "../bluetooth/drivers/bluez.c"
#endif
#endif

/*============================================================
WIFI
============================================================ */
#ifdef HAVE_WIFI
#include "../network/wifi_driver.c"

#ifdef HAVE_LAKKA
#include "../network/drivers_wifi/connmanctl.c"
#endif

#include "../tasks/task_wifi.c"

#endif

/*============================================================
RECORDING
============================================================ */
#include "../record/record_driver.c"
#ifdef HAVE_FFMPEG
#include "../record/drivers/record_ffmpeg.c"
#endif

/*============================================================
THREAD
============================================================ */
#if defined(HAVE_THREADS)

#if defined(XENON)
#include "../thread/xenon_sdl_threads.c"
#endif

#include "../libretro-common/rthreads/rthreads.c"
#include "../gfx/video_thread_wrapper.c"
#include "../audio/audio_thread_wrapper.c"
#endif

/* needed for playlists, netplay lobbies and achievements */
#include "../libretro-common/formats/json/rjson.c"

/*============================================================
NETPLAY
============================================================ */
#ifdef HAVE_NETWORKING
#include "../network/natt.c"
#include "../network/netplay/netplay_frontend.c"
#include "../network/netplay/netplay_room_parse.c"
#include "../libretro-common/net/net_compat.c"
#include "../libretro-common/net/net_socket.c"
#include "../libretro-common/net/net_http.c"
#ifdef HAVE_IFINFO
#include "../libretro-common/net/net_ifinfo.c"
#endif
#include "../tasks/task_http.c"
#include "../tasks/task_netplay_lan_scan.c"
#include "../tasks/task_netplay_nat_traversal.c"
#ifdef HAVE_BLUETOOTH
#include "../tasks/task_bluetooth.c"
#endif
#include "../tasks/task_netplay_find_content.c"
#include "../tasks/task_pl_thumbnail_download.c"
#endif

/*============================================================
DATA RUNLOOP
============================================================ */
#include "../tasks/task_powerstate.c"
#include "../tasks/task_content.c"
#ifdef HAVE_CDROM
#include "../tasks/task_content_disc.c"
#endif
#ifdef HAVE_PATCH
#include "../tasks/task_patch.c"
#endif
#include "../tasks/task_save.c"
#include "../tasks/task_movie.c"
#include "../tasks/task_image.c"
#include "../tasks/task_file_transfer.c"
#include "../tasks/task_playlist_manager.c"
#include "../tasks/task_manual_content_scan.c"
#include "../tasks/task_core_backup.c"
#ifdef HAVE_TRANSLATE
#include "../tasks/task_translation.c"
#endif
#ifdef HAVE_ZLIB
#include "../tasks/task_decompress.c"
#endif
#ifdef HAVE_LIBRETRODB
#include "../tasks/task_database.c"
#include "../tasks/task_database_cue.c"
#endif
#if defined(HAVE_NETWORKING) && defined(HAVE_MENU)
#include "../tasks/task_core_updater.c"
#endif

/*============================================================
SCREENSHOTS
============================================================ */
#ifdef HAVE_SCREENSHOTS
#include "../tasks/task_screenshot.c"
#endif

/*============================================================
PLAYLISTS
============================================================ */
#include "../playlist.c"

/*============================================================
MENU
============================================================ */
#ifdef HAVE_GFX_WIDGETS
#include "../gfx/gfx_widgets.c"
#ifdef HAVE_SCREENSHOTS
#include "../gfx/widgets/gfx_widget_screenshot.c"
#endif
#include "../gfx/widgets/gfx_widget_volume.c"
#include "../gfx/widgets/gfx_widget_generic_message.c"
#include "../gfx/widgets/gfx_widget_libretro_message.c"
#include "../gfx/widgets/gfx_widget_progress_message.c"
#ifdef HAVE_CHEEVOS
#include "../gfx/widgets/gfx_widget_achievement_popup.c"
#include "../gfx/widgets/gfx_widget_leaderboard_display.c"
#endif
#include "../gfx/widgets/gfx_widget_load_content_animation.c"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.c"
#include "../menu/menu_setting.c"
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
#include "../menu/menu_screensaver.c"
#endif

#include "../menu/cbs/menu_cbs_ok.c"
#include "../menu/cbs/menu_cbs_cancel.c"
#include "../menu/cbs/menu_cbs_select.c"
#include "../menu/cbs/menu_cbs_start.c"
#include "../menu/cbs/menu_cbs_info.c"
#include "../menu/cbs/menu_cbs_left.c"
#include "../menu/cbs/menu_cbs_right.c"
#include "../menu/cbs/menu_cbs_title.c"
#include "../menu/cbs/menu_cbs_deferred_push.c"
#include "../menu/cbs/menu_cbs_scan.c"
#include "../menu/cbs/menu_cbs_get_value.c"
#include "../menu/cbs/menu_cbs_label.c"
#include "../menu/cbs/menu_cbs_sublabel.c"
#include "../menu/menu_displaylist.c"
#include "../menu/menu_contentless_cores.c"
#ifdef HAVE_LIBRETRODB
#include "../menu/menu_explore.c"
#include "../tasks/task_menu_explore.c"
#endif
#endif

#ifdef HAVE_RGUI
#include "../menu/drivers/rgui.c"
#endif

#ifdef HAVE_XMB
#include "../menu/drivers/xmb.c"
#endif

#ifdef HAVE_OZONE
#include "../menu/drivers/ozone.c"
#endif

#ifdef HAVE_MATERIALUI
#include "../menu/drivers/materialui.c"
#endif

#ifdef HAVE_NETWORKGAMEPAD
#include "../cores/libretro-net-retropad/net_retropad_core.c"
#endif

#if defined(HAVE_NETWORKING)
#include "../libretro-common/net/net_http_parse.c"
#endif

/*============================================================
DEPENDENCIES
============================================================ */
#ifdef HAVE_FLAC
#include "../deps/libFLAC/bitmath.c"
#include "../deps/libFLAC/bitreader.c"
#include "../deps/libFLAC/cpu.c"
#include "../deps/libFLAC/crc.c"
#include "../deps/libFLAC/fixed.c"
#include "../deps/libFLAC/float.c"
#include "../deps/libFLAC/format.c"
#include "../deps/libFLAC/lpc.c"
#include "../deps/libFLAC/lpc_intrin_avx2.c"
#include "../deps/libFLAC/lpc_intrin_sse2.c"
#include "../deps/libFLAC/lpc_intrin_sse41.c"
#include "../deps/libFLAC/lpc_intrin_sse.c"
#include "../deps/libFLAC/md5.c"
#include "../deps/libFLAC/memory.c"
#include "../deps/libFLAC/stream_decoder.c"
#endif

#ifdef HAVE_ZLIB
#ifndef HAVE_NO_BUILTINZLIB
#include "../deps/libz/adler32.c"
#include "../deps/libz/compress.c"
#include "../deps/libz/libz-crc32.c"
#include "../deps/libz/deflate.c"
#include "../deps/libz/gzclose.c"
#include "../deps/libz/gzlib.c"
#include "../deps/libz/gzread.c"
#include "../deps/libz/gzwrite.c"
#include "../deps/libz/inffast.c"
#include "../deps/libz/inflate.c"
#include "../deps/libz/inftrees.c"
#include "../deps/libz/trees.c"
#include "../deps/libz/uncompr.c"
#include "../deps/libz/zutil.c"
#endif

#ifdef HAVE_CHD
#include "../libretro-common/formats/libchdr/libchdr_zlib.c"
#include "../libretro-common/formats/libchdr/libchdr_bitstream.c"
#include "../libretro-common/formats/libchdr/libchdr_cdrom.c"
#include "../libretro-common/formats/libchdr/libchdr_chd.c"

#ifdef HAVE_FLAC
#include "../libretro-common/formats/libchdr/libchdr_flac.c"
#include "../libretro-common/formats/libchdr/libchdr_flac_codec.c"
#endif

#ifdef HAVE_7ZIP
#include "../libretro-common/formats/libchdr/libchdr_lzma.c"
#endif

#include "../libretro-common/formats/libchdr/libchdr_huffman.c"

#include "../libretro-common/streams/chd_stream.c"
#endif
#endif

#ifdef HAVE_7ZIP
#include "../deps/7zip/7zArcIn.c"
#include "../deps/7zip/7zBuf.c"
#include "../deps/7zip/7zCrc.c"
#include "../deps/7zip/7zCrcOpt.c"
#include "../deps/7zip/7zDec.c"
#include "../deps/7zip/CpuArch.c"
#include "../deps/7zip/Delta.c"
#include "../deps/7zip/LzFind.c"
#include "../deps/7zip/LzmaDec.c"
#include "../deps/7zip/Lzma2Dec.c"
#include "../deps/7zip/LzmaEnc.c"
#include "../deps/7zip/Bra.c"
#include "../deps/7zip/Bra86.c"
#include "../deps/7zip/BraIA64.c"
#include "../deps/7zip/Bcj2.c"
#include "../deps/7zip/7zFile.c"
#include "../deps/7zip/7zStream.c"
#endif

#ifdef WANT_LIBFAT
#include "../deps/libfat/cache.c"
#include "../deps/libfat/directory.c"
#include "../deps/libfat/disc.c"
#include "../deps/libfat/fatdir.c"
#include "../deps/libfat/fatfile.c"
#include "../deps/libfat/file_allocation_table.c"
#include "../deps/libfat/filetime.c"
#include "../deps/libfat/libfat.c"
#include "../deps/libfat/lock.c"
#include "../deps/libfat/partition.c"
#endif

#ifdef WANT_IOSUHAX
#include "../deps/libiosuhax/source/iosuhax.c"
#include "../deps/libiosuhax/source/iosuhax_devoptab.c"
#include "../deps/libiosuhax/source/iosuhax_disc_interface.c"
#endif

/*============================================================
XML
============================================================ */
#include "../libretro-common/formats/xml/rxml.c"
#include "../libretro-common/formats/logiqx_dat/logiqx_dat.c"
#include "../deps/yxml/yxml.c"

/*============================================================
 AUDIO UTILS
============================================================ */
#include "../libretro-common/audio/conversion/s16_to_float.c"
#include "../libretro-common/audio/conversion/float_to_s16.c"
#include "../libretro-common/audio/conversion/stereo_to_mono_float.c"
#include "../libretro-common/audio/conversion/mono_to_stereo_float.c"
#ifdef HAVE_AUDIOMIXER
#include "../libretro-common/audio/audio_mix.c"
#endif

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

/*============================================================
HTTP SERVER
============================================================ */
#if defined(HAVE_DISCORD)
#include "../network/discord.c"
#if defined(_WIN32)
#include "../deps/discord-rpc/src/discord_register_win.c"
#endif
#if defined(__linux__)
#include "../deps/discord-rpc/src/discord_register_linux.c"
#endif
#endif

/*============================================================
SSL
============================================================ */
#if defined(HAVE_SSL)
#if defined(HAVE_NETWORKING)
#if defined(HAVE_BUILTINMBEDTLS)
#include "../deps/mbedtls/aes.c"
#include "../deps/mbedtls/aesni.c"
#include "../deps/mbedtls/arc4.c"
#include "../deps/mbedtls/asn1parse.c"
#include "../deps/mbedtls/asn1write.c"
#include "../deps/mbedtls/base64.c"
#include "../deps/mbedtls/bignum.c"
#include "../deps/mbedtls/blowfish.c"
#include "../deps/mbedtls/camellia.c"
#include "../deps/mbedtls/ccm.c"
#include "../deps/mbedtls/cipher.c"
#include "../deps/mbedtls/cipher_wrap.c"
#include "../deps/mbedtls/ctr_drbg.c"
#include "../deps/mbedtls/des.c"
#include "../deps/mbedtls/dhm.c"
#include "../deps/mbedtls/ecdh.c"
#include "../deps/mbedtls/ecdsa.c"
#include "../deps/mbedtls/ecp.c"
#include "../deps/mbedtls/ecp_curves.c"
#include "../deps/mbedtls/entropy.c"
#include "../deps/mbedtls/entropy_poll.c"
#include "../deps/mbedtls/gcm.c"
#include "../deps/mbedtls/hmac_drbg.c"
#include "../deps/mbedtls/md.c"
#include "../deps/mbedtls/md5.c"
#include "../deps/mbedtls/md_wrap.c"
#include "../deps/mbedtls/oid.c"
#include "../deps/mbedtls/padlock.c"
#include "../deps/mbedtls/pem.c"
#include "../deps/mbedtls/pk.c"
#include "../deps/mbedtls/pk_wrap.c"
#include "../deps/mbedtls/pkcs12.c"
#include "../deps/mbedtls/pkcs5.c"
#include "../deps/mbedtls/pkparse.c"
#include "../deps/mbedtls/pkwrite.c"
#include "../deps/mbedtls/ripemd160.c"
#include "../deps/mbedtls/rsa.c"
#include "../deps/mbedtls/sha1.c"
#include "../deps/mbedtls/sha256.c"
#include "../deps/mbedtls/sha512.c"
#include "../deps/mbedtls/threading.c"
#include "../deps/mbedtls/timing.c"
#include "../deps/mbedtls/xtea.c"

#include "../deps/mbedtls/certs.c"
#include "../deps/mbedtls/x509.c"
#include "../deps/mbedtls/x509_create.c"
#include "../deps/mbedtls/x509_crl.c"
#include "../deps/mbedtls/x509_crt.c"
#include "../deps/mbedtls/x509_csr.c"
#include "../deps/mbedtls/x509write_crt.c"
#include "../deps/mbedtls/x509write_csr.c"

#include "../deps/mbedtls/debug.c"
#include "../deps/mbedtls/net_sockets.c"
#include "../deps/mbedtls/ssl_cache.c"
#include "../deps/mbedtls/ssl_ciphersuites.c"
#include "../deps/mbedtls/ssl_cli.c"
#include "../deps/mbedtls/ssl_cookie.c"
#include "../deps/mbedtls/ssl_srv.c"
#include "../deps/mbedtls/ssl_ticket.c"
#include "../deps/mbedtls/ssl_tls.c"

#include "../libretro-common/net/net_socket_ssl_mbed.c"
#endif
#endif
#endif

/*============================================================
PLAYLIST NAME SANITIZATION
============================================================ */
#include "../libretro-common/playlists/label_sanitization.c"

/*============================================================
MANUAL CONTENT SCAN
============================================================ */
#include "../manual_content_scan.c"

/*============================================================
DISK CONTROL INTERFACE
============================================================ */
#include "../disk_control_interface.c"

/*============================================================
MISC FILE FORMATS
============================================================ */
#include "../libretro-common/formats/m3u/m3u_file.c"

/*============================================================
TIME
============================================================ */
#include "../libretro-common/time/rtime.c"

/*============================================================
ANDROID PLAY FEATURE DELIVERY
============================================================ */
#if defined(ANDROID)
#include "../play_feature_delivery/play_feature_delivery.c"
#endif

/*============================================================
STEAM INTEGRATION USING MIST
============================================================ */
#ifdef HAVE_MIST
#include "../steam/steam.c"
#include "../tasks/task_steam.c"
#endif

#ifdef HAVE_PRESENCE
#include "../network/presence.c"
#endif

/*============================================================
CLOUD SYNC
============================================================ */
#ifdef HAVE_CLOUDSYNC
#include "../tasks/task_cloudsync.c"
#include "../network/cloud_sync_driver.c"
#include "../network/cloud_sync/webdav.c"
#endif
