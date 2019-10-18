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

#if !defined(__WINRT__)
#define HAVE_IBXM 1
#endif

#if defined(HAVE_ZLIB) || defined(HAVE_7ZIP)
#define HAVE_COMPRESSION 1
#endif

#define JSON_STATIC 1 /* must come before runtime_file, netplay_room_parse and jsonsax_full */

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
#include "../performance_counters.c"

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

/*============================================================
RUNTIME FILE
============================================================ */
#include "../runtime_file.c"

/*============================================================
ACHIEVEMENTS
============================================================ */
#if defined(HAVE_CHEEVOS)
#if !defined(HAVE_NETWORKING)
#include "../libretro-common/net/net_http.c"
#endif

#include "../libretro-common/formats/json/jsonsax.c"
#include "../libretro-common/formats/cdfs/cdfs.c"
#include "../network/net_http_special.c"

#include "../cheevos-new/cheevos.c"
#include "../cheevos-new/badges.c"
#include "../cheevos-new/fixup.c"
#include "../cheevos-new/hash.c"
#include "../cheevos-new/parser.c"

#include "../deps/rcheevos/src/rcheevos/alloc.c"
#include "../deps/rcheevos/src/rcheevos/condition.c"
#include "../deps/rcheevos/src/rcheevos/condset.c"
#include "../deps/rcheevos/src/rcheevos/expression.c"
#include "../deps/rcheevos/src/rcheevos/format.c"
#include "../deps/rcheevos/src/rcheevos/lboard.c"
#include "../deps/rcheevos/src/rcheevos/operand.c"
#include "../deps/rcheevos/src/rcheevos/term.c"
#include "../deps/rcheevos/src/rcheevos/trigger.c"
#include "../deps/rcheevos/src/rcheevos/value.c"
#include "../deps/rcheevos/src/rcheevos/memref.c"
#include "../deps/rcheevos/src/rcheevos/richpresence.c"
#include "../deps/rcheevos/src/rurl/url.c"

#endif

/*============================================================
MD5
============================================================ */
#include "../libretro-common/utils/md5.c"

/*============================================================
CHEATS
============================================================ */
#include "../managers/cheat_manager.c"
#include "../libretro-common/hash/rhash.c"

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

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_VULKAN)
#include "../gfx/drivers_context/wgl_ctx.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
#include "../gfx/drivers_context/gdi_ctx.c"
#endif
#endif

#include "../gfx/display_servers/dispserv_win32.c"

#if defined(HAVE_FFMPEG)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES3)
#include "../cores/libretro-ffmpeg/ffmpeg_fft.c"
#endif
#endif

#endif

#if defined(__CELLOS_LV2__)
#include "../gfx/drivers_context/ps3_ctx.c"
#elif defined(ANDROID)
#include "../gfx/drivers_context/android_ctx.c"
#include "../gfx/display_servers/dispserv_android.c"
#elif defined(__QNX__)
#include "../gfx/drivers_context/qnx_ctx.c"
#elif defined(EMSCRIPTEN)
#include "../gfx/drivers_context/emscriptenegl_ctx.c"
#elif defined(__APPLE__) && !defined(TARGET_IPHONE_SIMULATOR) && !defined(TARGET_OS_IPHONE)
#include "../gfx/drivers_context/cgl_ctx.c"
#endif

#if defined(HAVE_VIVANTE_FBDEV)
#include "../gfx/drivers_context/vivante_fbdev_ctx.c"
#endif

#if defined(HAVE_OPENDINGUX_FBDEV)
#include "../gfx/drivers_context/opendingux_fbdev_ctx.c"
#endif

#ifdef HAVE_WAYLAND
#include "../gfx/drivers_context/wayland_ctx.c"
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
#endif

#if defined(HAVE_EGL)
#include "../gfx/common/egl_common.c"

#if defined(HAVE_VIDEOCORE)
#include "../gfx/drivers_context/vc_egl_ctx.c"
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
#include "../libretro-common/formats/wav/rwav.c"

/*============================================================
VIDEO DRIVER
============================================================ */
#if defined(HAVE_D3D)
#include "../gfx/common/d3d_common.c"

#if defined(HAVE_D3D8)
#include "../gfx/drivers/d3d8.c"
#include "../gfx/common/d3d8_common.c"
#endif

#if defined(HAVE_D3D9)
#include "../gfx/drivers/d3d9.c"
#include "../gfx/common/d3d9_common.c"

#ifdef HAVE_HLSL
#include "../gfx/drivers_renderchain/d3d9_hlsl_renderchain.c"
#endif

#ifdef HAVE_CG
#include "../gfx/drivers_renderchain/d3d9_cg_renderchain.c"
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

#include "../gfx/display_servers/dispserv_null.c"

#ifdef HAVE_OPENGL1
#include "../gfx/drivers/gl1.c"
#endif

#ifdef HAVE_OPENGL_CORE
#include "../gfx/drivers/gl_core.c"
#endif

#ifdef HAVE_OPENGL
#include "../gfx/drivers/gl.c"
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

#if defined(GEKKO)
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
#include "../gfx/drivers/nullgfx.c"

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
#include "../gfx/drivers/gdi_gfx.c"
#endif
#endif

#include "../deps/ibxm/ibxm.c"

#ifdef HAVE_VIDEO_LAYOUT
#include "../gfx/video_layout.c"
#include "../gfx/video_layout/view.c"
#include "../gfx/video_layout/element.c"
#include "../gfx/video_layout/component.c"
#include "../gfx/video_layout/internal.c"
#include "../gfx/video_layout/scope.c"
#include "../gfx/video_layout/load.c"
#endif

/*============================================================
FONTS
============================================================ */

#include "../gfx/drivers_font_renderer/bitmapfont.c"
#include "../gfx/font_driver.c"

#if defined(HAVE_D3D9) && defined(HAVE_D3DX)
#include "../gfx/drivers_font/d3d_w32_font.c"
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

#ifdef HAVE_OPENGL1
#include "../gfx/drivers_font/gl1_raster_font.c"
#endif

#if defined(HAVE_OPENGL)
#include "../gfx/drivers_font/gl_raster_font.c"
#endif

#ifdef HAVE_OPENGL_CORE
#include "../gfx/drivers_font/gl_core_raster_font.c"
#endif

#if defined(_XBOX1)
#include "../gfx/drivers_font/xdk1_xfonts.c"
#endif

#if defined(PS2)
#include "../gfx/drivers_font/ps2_font.c"
#endif

#if defined(VITA)
#include "../gfx/drivers_font/vita2d_font.c"
#endif

#if defined(_3DS)
#include "../gfx/drivers_font/ctr_font.c"
#endif

#if defined(WIIU)
#include "../gfx/drivers_font/wiiu_font.c"
#endif

#if defined(HAVE_CACA)
#include "../gfx/drivers_font/caca_font.c"
#endif

#if defined(DJGPP)
#include "../gfx/drivers_font/vga_font.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
#include "../gfx/drivers_font/gdi_font.c"
#endif
#endif

#if defined(HAVE_VULKAN)
#include "../gfx/drivers_font/vulkan_raster_font.c"
#endif

#if defined(HAVE_D3D10)
#include "../gfx/drivers_font/d3d10_font.c"
#endif

#if defined(HAVE_D3D11)
#include "../gfx/drivers_font/d3d11_font.c"
#endif

#if defined(HAVE_D3D12)
#include "../gfx/drivers_font/d3d12_font.c"
#endif

/*============================================================
INPUT
============================================================ */
#include "../tasks/task_autodetect.c"
#ifdef HAVE_AUDIOMIXER
#include "../tasks/task_audio_mixer.c"
#endif
#include "../input/input_keymaps.c"
#include "../input/input_remapping.c"

#ifdef HAVE_OVERLAY
#include "../led/drivers/led_overlay.c"
#include "../tasks/task_overlay.c"
#endif

#ifdef HAVE_X11
#include "../input/common/input_x11_common.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
/* winraw only available since XP */
#include "../input/drivers/winraw_input.c"
#endif

#include "../input/input_autodetect_builtin.c"

#if defined(__CELLOS_LV2__)
#include "../input/drivers/ps3_input.c"
#include "../input/drivers_joypad/ps3_joypad.c"
#elif defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
#include "../input/drivers/psp_input.c"
#include "../input/drivers_joypad/psp_joypad.c"
#elif defined(ORBIS)
#include "../input/drivers/ps4_input.c"
#include "../input/drivers_joypad/ps4_joypad.c"
#elif defined(PS2)
#include "../input/drivers/ps2_input.c"
#include "../input/drivers_joypad/ps2_joypad.c"
#elif defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
#include "../input/drivers/cocoa_input.c"
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
#include "../input/drivers_joypad/xdk_joypad.c"
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
#endif

#ifdef HAVE_WAYLAND
#include "../input/drivers/wayland_input.c"
#endif

#ifdef HAVE_DINPUT
#include "../input/drivers/dinput.c"
#include "../input/drivers_joypad/dinput_joypad.c"
#endif

#ifdef HAVE_XINPUT
#include "../input/drivers_joypad/xinput_joypad.c"
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

#include "../input/drivers/nullinput.c"
#include "../input/drivers_joypad/null_joypad.c"

/*============================================================
INPUT (HID)
============================================================ */
#ifdef HAVE_HID
#include "../input/common/input_hid_common.c"
#include "../input/drivers_joypad/hid_joypad.c"
#include "../input/drivers_hid/null_hid.c"

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
#endif

/*============================================================
 KEYBOARD EVENT
 ============================================================ */

#ifdef __APPLE__
#include "../input/drivers_keyboard/keyboard_event_apple.c"
#endif

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
#include "../libretro-common/audio/resampler/drivers/nearest_resampler.c"
#include "../libretro-common/audio/resampler/drivers/null_resampler.c"
#ifdef HAVE_CC_RESAMPLER
#include "../audio/drivers_resampler/cc_resampler.c"
#endif

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

#ifdef HAVE_VIDEOPROCESSOR
#include "../cores/libretro-video-processor/video_processor_v4l2.c"
#endif

#include "../camera/drivers/nullcamera.c"

/*============================================================
LEDS
============================================================ */

#include "../led/led_driver.c"

#include "../led/drivers/led_null.c"

#if defined(HAVE_RPILED)
#include "../led/drivers/led_rpi.c"
#endif

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
#endif

#ifdef HAVE_XAUDIO
#include "../audio/drivers/xaudio.c"
#endif

#if defined(HAVE_SDL2)
#include "../audio/drivers/sdl_audio.c"
#endif

#ifdef HAVE_DSOUND
#include "../audio/drivers/dsound.c"
#endif

#ifdef HAVE_WASAPI
#include "../audio/drivers/wasapi.c"
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

#include "../audio/drivers/nullaudio.c"

#if defined(HAVE_WASAPI) || ((_WIN32_WINNT >= 0x0602) && !defined(__WINRT__))
#include "../audio/common/mmdevice_common.c"
#endif

/*============================================================
MIDI
============================================================ */
#include "../midi/drivers/null_midi.c"

#ifdef HAVE_WINMM
#include "../midi/drivers/winmm_midi.c"
#endif

/*============================================================
DRIVERS
============================================================ */
#include "../gfx/video_crt_switch.c"
#include "../gfx/video_display_server.c"
#include "../gfx/video_coord_array.c"
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
#include "../gfx/video_filters/scanline2x.c"

#include "../libretro-common/audio/dsp_filters/echo.c"
#include "../libretro-common/audio/dsp_filters/eq.c"
#include "../libretro-common/audio/dsp_filters/chorus.c"
#include "../libretro-common/audio/dsp_filters/iir.c"
#include "../libretro-common/audio/dsp_filters/panning.c"
#include "../libretro-common/audio/dsp_filters/phaser.c"
#include "../libretro-common/audio/dsp_filters/reverb.c"
#include "../libretro-common/audio/dsp_filters/wahwah.c"
#endif

/*============================================================
DYNAMIC
============================================================ */
#include "../libretro-common/dynamic/dylib.c"
#include "../gfx/video_filter.c"
#include "../libretro-common/audio/dsp_filter.c"

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
#include "../file_path_special.c"
#include "../file_path_str.c"
#include "../libretro-common/lists/dir_list.c"
#include "../libretro-common/lists/string_list.c"
#include "../libretro-common/lists/file_list.c"
#include "../libretro-common/file/retro_dirent.c"
#include "../libretro-common/streams/file_stream.c"
#include "../libretro-common/streams/file_stream_transforms.c"
#include "../libretro-common/streams/interface_stream.c"
#include "../libretro-common/streams/memory_stream.c"
#ifndef __WINRT__
#include "../libretro-common/vfs/vfs_implementation.c"
#endif

#ifdef HAVE_CDROM
#include "../libretro-common/cdrom/cdrom.c"
#include "../libretro-common/vfs/vfs_implementation_cdrom.c"
#include "../libretro-common/media/media_detect_cd.c"
#endif

#include "../list_special.c"
#include "../libretro-common/string/stdstring.c"
#include "../libretro-common/file/nbio/nbio_stdio.c"
#if defined(__linux__)
#include "../libretro-common/file/nbio/nbio_linux.c"
#endif
#if defined(HAVE_MMAP) && defined(BSD)
#include "../libretro-common/file/nbio/nbio_unixmmap.c"
#endif
#if defined(_WIN32) && !defined(_XBOX)
#include "../libretro-common/file/nbio/nbio_windowsmmap.c"
#endif
#if defined(ORBIS)
#include "../libretro-common/file/nbio/nbio_orbis.c"
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
#include "../managers/state_manager.c"

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

#if defined(__CELLOS_LV2__)
#include "../frontend/drivers/platform_ps3.c"
#elif defined(GEKKO)
#include "../frontend/drivers/platform_gx.c"
#ifdef HW_RVL
#include "../frontend/drivers/platform_wii.c"
#endif
#elif defined(__wiiu__)
#include "../frontend/drivers/platform_wiiu.c"
#elif defined(PSP) || defined(VITA)
#include "../frontend/drivers/platform_psp.c"
#elif defined(ORBIS)
#include "../frontend/drivers/platform_orbis.c"
#elif defined(PS2)
#include "../frontend/drivers/platform_ps2.c"
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
#include "../frontend/drivers/platform_null.c"

#include "../core_info.c"

/*============================================================
UI
============================================================ */
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include "../ui/drivers/ui_win32.c"
#include "../ui/drivers/win32/ui_win32_window.c"
#include "../ui/drivers/win32/ui_win32_browser_window.c"
#include "../ui/drivers/win32/ui_win32_msg_window.c"
#include "../ui/drivers/win32/ui_win32_application.c"
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
#include "../paths.c"
#include "../libretro-common/queues/task_queue.c"

#include "../msg_hash.c"
#ifdef HAVE_LANGEXTRA
#include "../intl/msg_hash_de.c"
#include "../intl/msg_hash_es.c"
#include "../intl/msg_hash_eo.c"
#include "../intl/msg_hash_fr.c"
#include "../intl/msg_hash_it.c"
#include "../intl/msg_hash_ja.c"
#include "../intl/msg_hash_ko.c"
#include "../intl/msg_hash_nl.c"
#include "../intl/msg_hash_pt_br.c"
#include "../intl/msg_hash_pt_pt.c"
#include "../intl/msg_hash_pl.c"
#include "../intl/msg_hash_ru.c"
#include "../intl/msg_hash_vn.c"
#include "../intl/msg_hash_chs.c"
#include "../intl/msg_hash_cht.c"
#include "../intl/msg_hash_ar.c"
#include "../intl/msg_hash_el.c"
#include "../intl/msg_hash_tr.c"
#endif

#include "../intl/msg_hash_us.c"

/*============================================================
WIFI
============================================================ */
#include "../wifi/drivers/nullwifi.c"

#ifdef HAVE_LAKKA
#include "../wifi/drivers/connmanctl.c"
#endif

/*============================================================
RECORDING
============================================================ */
#include "../record/drivers/record_null.c"

#ifdef HAVE_FFMPEG
#include "../record/drivers/record_ffmpeg.c"
#endif

/*============================================================
THREAD
============================================================ */
#if defined(HAVE_THREADS)

#if defined(XENON)
#include "../thread/xenon_sdl_threads.c"
#elif defined(PSP)
#include "../deps/pthreads/platform/helper/tls-helper.c"
#include "../deps/pthreads/platform/psp/psp_osal.c"
#include "../deps/pthreads/pte_main.c"
#include "../deps/pthreads/pte.c"
#include "../deps/pthreads/pthread_attr.c"
#include "../deps/pthreads/pthread_barrier.c"
#include "../deps/pthreads/pthread_cond.c"
#include "../deps/pthreads/pthread_condattr.c"
#include "../deps/pthreads/pthread_get.c"
#include "../deps/pthreads/pthread_key.c"
#include "../deps/pthreads/pthread_mutex.c"
#include "../deps/pthreads/pthread_mutexattr.c"
#include "../deps/pthreads/pthread_rwlock.c"
#include "../deps/pthreads/pthread_rwlockattr.c"
#include "../deps/pthreads/pthread_set.c"
#include "../deps/pthreads/pthread_spin.c"
#include "../deps/pthreads/pthread.c"
#include "../deps/pthreads/sched.c"
#include "../deps/pthreads/sem.c"
#endif

#include "../libretro-common/rthreads/rthreads.c"
#include "../gfx/video_thread_wrapper.c"
#include "../audio/audio_thread_wrapper.c"
#endif

/* needed for both playlists and netplay lobbies */
#include "../libretro-common/formats/json/jsonsax_full.c"

/*============================================================
NETPLAY
============================================================ */
#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay_delta.c"
#include "../network/netplay/netplay_frontend.c"
#include "../network/netplay/netplay_handshake.c"
#include "../network/netplay/netplay_init.c"
#include "../network/netplay/netplay_io.c"
#include "../network/netplay/netplay_keyboard.c"
#include "../network/netplay/netplay_sync.c"
#include "../network/netplay/netplay_discovery.c"
#include "../network/netplay/netplay_buf.c"
#include "../network/netplay/netplay_room_parse.c"
#include "../libretro-common/net/net_compat.c"
#include "../libretro-common/net/net_socket.c"
#include "../libretro-common/net/net_http.c"
#include "../libretro-common/net/net_natt.c"
#if !defined(HAVE_SOCKET_LEGACY)
#include "../libretro-common/net/net_ifinfo.c"
#endif
#include "../tasks/task_http.c"
#include "../tasks/task_netplay_lan_scan.c"
#include "../tasks/task_netplay_nat_traversal.c"
#include "../tasks/task_wifi.c"
#include "../tasks/task_netplay_find_content.c"
#endif

/*============================================================
DATA RUNLOOP
============================================================ */
#include "../tasks/task_powerstate.c"
#include "../tasks/task_content.c"
#include "../tasks/task_save.c"
#include "../tasks/task_image.c"
#include "../tasks/task_file_transfer.c"
#include "../tasks/task_playlist_manager.c"
#ifdef HAVE_ZLIB
#include "../tasks/task_decompress.c"
#endif
#ifdef HAVE_LIBRETRODB
#include "../tasks/task_database.c"
#include "../tasks/task_database_cue.c"
#endif
#if defined(HAVE_NETWORKING) && defined(HAVE_MENU)
#include "../tasks/task_pl_thumbnail_download.c"
#endif

/*============================================================
SCREENSHOTS
============================================================ */
#include "../tasks/task_screenshot.c"

/*============================================================
PLAYLISTS
============================================================ */
#include "../playlist.c"

/*============================================================
MENU
============================================================ */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL) || defined(HAVE_SLANG)
#include "../menu/menu_shader.c"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.c"
#include "../menu/menu_setting.c"
#include "../menu/menu_cbs.c"

#if defined(HAVE_NETWORKING)
#include "../menu/menu_networking.c"
#endif

#include "../menu/widgets/menu_filebrowser.c"
#include "../menu/widgets/menu_dialog.c"
#include "../menu/widgets/menu_input_bind_dialog.c"
#ifdef HAVE_MENU_WIDGETS
#include "../menu/widgets/menu_widgets.c"
#endif
#include "../menu/widgets/menu_osk.c"
#include "../menu/cbs/menu_cbs_ok.c"
#include "../menu/cbs/menu_cbs_cancel.c"
#include "../menu/cbs/menu_cbs_select.c"
#include "../menu/cbs/menu_cbs_start.c"
#include "../menu/cbs/menu_cbs_info.c"
#include "../menu/cbs/menu_cbs_refresh.c"
#include "../menu/cbs/menu_cbs_left.c"
#include "../menu/cbs/menu_cbs_right.c"
#include "../menu/cbs/menu_cbs_title.c"
#include "../menu/cbs/menu_cbs_deferred_push.c"
#include "../menu/cbs/menu_cbs_scan.c"
#include "../menu/cbs/menu_cbs_get_value.c"
#include "../menu/cbs/menu_cbs_label.c"
#include "../menu/cbs/menu_cbs_sublabel.c"
#include "../menu/cbs/menu_cbs_up.c"
#include "../menu/cbs/menu_cbs_down.c"
#include "../menu/cbs/menu_cbs_contentlist_switch.c"
#include "../menu/menu_displaylist.c"
#include "../menu/menu_animation.c"
#include "../menu/menu_thumbnail_path.c"

#include "../menu/drivers/null.c"
#include "../menu/drivers/menu_generic.c"

#include "../menu/drivers_display/menu_display_null.c"

#if defined(HAVE_D3D8)
#include "../menu/drivers_display/menu_display_d3d8.c"
#endif

#if defined(HAVE_D3D9)
#include "../menu/drivers_display/menu_display_d3d9.c"
#endif

#if defined(HAVE_D3D10)
#include "../menu/drivers_display/menu_display_d3d10.c"
#endif

#if defined(HAVE_D3D11)
#include "../menu/drivers_display/menu_display_d3d11.c"
#endif

#if defined(HAVE_D3D12)
#include "../menu/drivers_display/menu_display_d3d12.c"
#endif

#ifdef HAVE_OPENGL1
#include "../menu/drivers_display/menu_display_gl1.c"
#endif

#ifdef HAVE_OPENGL
#include "../menu/drivers_display/menu_display_gl.c"
#endif

#ifdef HAVE_OPENGL_CORE
#include "../menu/drivers_display/menu_display_gl_core.c"
#endif

#ifdef HAVE_VULKAN
#include "../menu/drivers_display/menu_display_vulkan.c"
#endif

#ifdef HAVE_VITA2D
#include "../menu/drivers_display/menu_display_vita2d.c"
#endif

#ifdef _3DS
#include "../menu/drivers_display/menu_display_ctr.c"
#endif

#ifdef WIIU
#include "../menu/drivers_display/menu_display_wiiu.c"
#endif

#if defined(HAVE_LIBNX)
#include "../menu/drivers_display/menu_display_switch.c"
#endif

#ifdef HAVE_CACA
#include "../menu/drivers_display/menu_display_caca.c"
#endif

#ifdef DJGPP
#include "../menu/drivers_display/menu_display_vga.c"
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
#include "../menu/drivers_display/menu_display_gdi.c"
#endif
#endif

#endif

#ifdef HAVE_RGUI
#include "../menu/drivers/rgui.c"
#endif

#ifdef HAVE_XMB
#include "../menu/drivers/xmb.c"
#endif

#ifdef HAVE_OZONE
#include "../menu/drivers/ozone/ozone.c"
#include "../menu/drivers/ozone/ozone_display.c"
#include "../menu/drivers/ozone/ozone_entries.c"
#include "../menu/drivers/ozone/ozone_sidebar.c"
#include "../menu/drivers/ozone/ozone_texture.c"
#include "../menu/drivers/ozone/ozone_theme.c"
#endif

#ifdef HAVE_STRIPES
#include "../menu/drivers/stripes.c"
#endif

#ifdef HAVE_MATERIALUI
#include "../menu/drivers/materialui.c"
#endif

#ifdef HAVE_NETWORKGAMEPAD
#include "../cores/libretro-net-retropad/net_retropad_core.c"
#endif

#include "../input/input_mapper.c"

#if defined(HAVE_NETWORKING)
#include "../libretro-common/net/net_http_parse.c"
#endif

#ifdef HAVE_RUNAHEAD
#include "../runahead/mem_util.c"
#include "../runahead/mylist.c"
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
#include "../deps/7zip/7zIn.c"
#include "../deps/7zip/Bra86.c"
#include "../deps/7zip/7zFile.c"
#include "../deps/7zip/7zStream.c"
#include "../deps/7zip/LzmaDec.c"
#include "../deps/7zip/LzmaEnc.c"
#include "../deps/7zip/7zCrcOpt.c"
#include "../deps/7zip/Bra.c"
#include "../deps/7zip/7zDec.c"
#include "../deps/7zip/Bcj2.c"
#include "../deps/7zip/7zCrc.c"
#include "../deps/7zip/Lzma2Dec.c"
#include "../deps/7zip/LzFind.c"
#include "../deps/7zip/7zBuf.c"
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
#include "../deps/libiosuhax/iosuhax.c"
#include "../deps/libiosuhax/iosuhax_devoptab.c"
#include "../deps/libiosuhax/iosuhax_disc_interface.c"
#endif

/*============================================================
XML
============================================================ */
#include "../libretro-common/formats/xml/rxml.c"

/*============================================================
 AUDIO UTILS
============================================================ */
#include "../libretro-common/audio/conversion/s16_to_float.c"
#include "../libretro-common/audio/conversion/float_to_s16.c"
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

#if defined(HAVE_BUILTINMINIUPNPC)
#include "../deps/miniupnpc/igd_desc_parse.c"
#include "../deps/miniupnpc/upnpreplyparse.c"
#include "../deps/miniupnpc/upnpcommands.c"
#include "../deps/miniupnpc/upnperrors.c"
#include "../deps/miniupnpc/connecthostport.c"
#include "../deps/miniupnpc/portlistingparse.c"
#include "../deps/miniupnpc/receivedata.c"
#include "../deps/miniupnpc/upnpdev.c"
#include "../deps/miniupnpc/minissdpc.c"
#include "../deps/miniupnpc/miniwget.c"
#include "../deps/miniupnpc/miniupnpc.c"
#include "../deps/miniupnpc/minixml.c"
#include "../deps/miniupnpc/minisoap.c"
#endif

/*============================================================
HTTP SERVER
============================================================ */
#if defined(HAVE_DISCORD)
#include "../discord/discord.c"

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
#include "../deps/mbedtls/cmac.c"
#include "../deps/mbedtls/ctr_drbg.c"
#include "../deps/mbedtls/des.c"
#include "../deps/mbedtls/dhm.c"
#include "../deps/mbedtls/ecdh.c"
#include "../deps/mbedtls/ecdsa.c"
#include "../deps/mbedtls/ecjpake.c"
#include "../deps/mbedtls/ecp.c"
#include "../deps/mbedtls/ecp_curves.c"
#include "../deps/mbedtls/entropy.c"
#include "../deps/mbedtls/entropy_poll.c"
#include "../deps/mbedtls/error.c"
#include "../deps/mbedtls/gcm.c"
#include "../deps/mbedtls/havege.c"
#include "../deps/mbedtls/hmac_drbg.c"
#include "../deps/mbedtls/md.c"
#include "../deps/mbedtls/md2.c"
#include "../deps/mbedtls/md4.c"
#include "../deps/mbedtls/md5.c"
#include "../deps/mbedtls/md_wrap.c"
#include "../deps/mbedtls/memory_buffer_alloc.c"
#include "../deps/mbedtls/oid.c"
#include "../deps/mbedtls/padlock.c"
#include "../deps/mbedtls/pem.c"
#include "../deps/mbedtls/pk.c"
#include "../deps/mbedtls/pk_wrap.c"
#include "../deps/mbedtls/pkcs12.c"
#include "../deps/mbedtls/pkcs5.c"
#include "../deps/mbedtls/pkparse.c"
#include "../deps/mbedtls/pkwrite.c"
#include "../deps/mbedtls/platform.c"
#include "../deps/mbedtls/ripemd160.c"
#include "../deps/mbedtls/rsa.c"
#include "../deps/mbedtls/sha1.c"
#include "../deps/mbedtls/sha256.c"
#include "../deps/mbedtls/sha512.c"
#include "../deps/mbedtls/threading.c"
#include "../deps/mbedtls/timing.c"
#include "../deps/mbedtls/version.c"
#include "../deps/mbedtls/version_features.c"
#include "../deps/mbedtls/xtea.c"

#include "../deps/mbedtls/certs.c"
#include "../deps/mbedtls/pkcs11.c"
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

#include "../libretro-common/net/net_socket_ssl.c"
#endif
#endif
#endif

#ifdef HAVE_EASTEREGG
#include "../cores/libretro-gong/gong.c"
#endif

/*============================================================
PLAYLIST NAME SANITIZATION
============================================================ */
#include "../libretro-common/playlists/label_sanitization.c"
