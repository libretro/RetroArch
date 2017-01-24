/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CONFIG_DEF_H
#define __CONFIG_DEF_H

#include <boolean.h>
#include "gfx/video_defines.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "input/input_driver.h"

#include "config.def.keybinds.h"

enum video_driver_enum
{
   VIDEO_GL                 = 0,
   VIDEO_VULKAN,
   VIDEO_DRM,
   VIDEO_XVIDEO,
   VIDEO_SDL,
   VIDEO_SDL2,
   VIDEO_EXT,
   VIDEO_WII,
   VIDEO_WIIU,
   VIDEO_XENON360,
   VIDEO_XDK_D3D,
   VIDEO_PSP1,
   VIDEO_VITA2D,
   VIDEO_CTR,
   VIDEO_D3D9,
   VIDEO_VG,
   VIDEO_OMAP,
   VIDEO_EXYNOS,
   VIDEO_SUNXI,
   VIDEO_DISPMANX,
   VIDEO_CACA,
   VIDEO_GDI,
   VIDEO_VGA,
   VIDEO_NULL
};

enum audio_driver_enum
{
   AUDIO_RSOUND             = VIDEO_NULL + 1,
   AUDIO_OSS,
   AUDIO_ALSA,
   AUDIO_ALSATHREAD,
   AUDIO_ROAR,
   AUDIO_AL,
   AUDIO_SL,
   AUDIO_JACK,
   AUDIO_SDL,
   AUDIO_SDL2,
   AUDIO_XAUDIO,
   AUDIO_PULSE,
   AUDIO_EXT,
   AUDIO_DSOUND,
   AUDIO_COREAUDIO,
   AUDIO_PS3,
   AUDIO_XENON360,
   AUDIO_WII,
   AUDIO_WIIU,
   AUDIO_RWEBAUDIO,
   AUDIO_PSP,
   AUDIO_CTR,
   AUDIO_NULL
};

enum audio_resampler_driver_enum
{
   AUDIO_RESAMPLER_CC       = AUDIO_NULL + 1,
   AUDIO_RESAMPLER_SINC,
   AUDIO_RESAMPLER_NEAREST,
   AUDIO_RESAMPLER_NULL
};

enum input_driver_enum
{
   INPUT_ANDROID            = AUDIO_RESAMPLER_NULL + 1,
   INPUT_SDL,
   INPUT_SDL2,
   INPUT_X,
   INPUT_WAYLAND,
   INPUT_DINPUT,
   INPUT_PS3,
   INPUT_PSP,
   INPUT_CTR,
   INPUT_XENON360,
   INPUT_WII,
   INPUT_WIIU,
   INPUT_XINPUT,
   INPUT_UDEV,
   INPUT_LINUXRAW,
   INPUT_COCOA,
   INPUT_QNX,
   INPUT_RWEBINPUT,
   INPUT_DOS,
   INPUT_NULL
};

enum joypad_driver_enum
{
   JOYPAD_PS3               = INPUT_NULL + 1,
   JOYPAD_XINPUT,
   JOYPAD_GX,
   JOYPAD_WIIU,
   JOYPAD_XDK,
   JOYPAD_PSP,
   JOYPAD_CTR,
   JOYPAD_DINPUT,
   JOYPAD_UDEV,
   JOYPAD_LINUXRAW,
   JOYPAD_ANDROID,
   JOYPAD_SDL,
   JOYPAD_DOS,
   JOYPAD_HID,
   JOYPAD_QNX,
   JOYPAD_NULL
};

enum camera_driver_enum
{
   CAMERA_V4L2              = JOYPAD_NULL + 1,
   CAMERA_RWEBCAM,
   CAMERA_ANDROID,
   CAMERA_AVFOUNDATION,
   CAMERA_NULL
};

enum wifi_driver_enum
{
   WIFI_CONNMANCTL          = CAMERA_NULL + 1,
   WIFI_NULL
};

enum location_driver_enum
{
   LOCATION_ANDROID         = WIFI_NULL + 1,
   LOCATION_CORELOCATION,
   LOCATION_NULL
};

enum osk_driver_enum
{
   OSK_PS3                  = LOCATION_NULL + 1,
   OSK_NULL
};

enum menu_driver_enum
{
   MENU_RGUI                = OSK_NULL + 1,
   MENU_XUI,
   MENU_MATERIALUI,
   MENU_XMB,
   MENU_NUKLEAR,
   MENU_NULL
};

enum record_driver_enum
{
   RECORD_FFMPEG            = MENU_NULL + 1,
   RECORD_NULL
};

#if defined(HW_RVL)
#define MAX_GAMMA_SETTING 30
#elif defined(GEKKO)
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) || defined(__CELLOS_LV2__)
#define VIDEO_DEFAULT_DRIVER VIDEO_GL
#elif defined(GEKKO)
#define VIDEO_DEFAULT_DRIVER VIDEO_WII
#elif defined(WIIU)
#define VIDEO_DEFAULT_DRIVER VIDEO_WIIU
#elif defined(XENON)
#define VIDEO_DEFAULT_DRIVER VIDEO_XENON360
#elif (defined(_XBOX1) || defined(_XBOX360)) && (defined(HAVE_D3D8) || defined(HAVE_D3D9))
#define VIDEO_DEFAULT_DRIVER VIDEO_XDK_D3D
#elif defined(HAVE_D3D9)
#define VIDEO_DEFAULT_DRIVER VIDEO_D3D9
#elif defined(HAVE_VG)
#define VIDEO_DEFAULT_DRIVER VIDEO_VG
#elif defined(HAVE_VITA2D)
#define VIDEO_DEFAULT_DRIVER VIDEO_VITA2D
#elif defined(PSP)
#define VIDEO_DEFAULT_DRIVER VIDEO_PSP1
#elif defined(_3DS)
#define VIDEO_DEFAULT_DRIVER VIDEO_CTR
#elif defined(HAVE_XVIDEO)
#define VIDEO_DEFAULT_DRIVER VIDEO_XVIDEO
#elif defined(HAVE_SDL)
#define VIDEO_DEFAULT_DRIVER VIDEO_SDL
#elif defined(HAVE_SDL2)
#define VIDEO_DEFAULT_DRIVER VIDEO_SDL2
#elif defined(_WIN32) && !defined(_XBOX)
#define VIDEO_DEFAULT_DRIVER VIDEO_GDI
#elif defined(DJGPP)
#define VIDEO_DEFAULT_DRIVER VIDEO_VGA
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
#define VIDEO_DEFAULT_DRIVER VIDEO_EXT
#else
#define VIDEO_DEFAULT_DRIVER VIDEO_NULL
#endif

#if defined(__CELLOS_LV2__)
#define AUDIO_DEFAULT_DRIVER AUDIO_PS3
#elif defined(XENON)
#define AUDIO_DEFAULT_DRIVER AUDIO_XENON360
#elif defined(GEKKO)
#define AUDIO_DEFAULT_DRIVER AUDIO_WII
#elif defined(WIIU)
#define AUDIO_DEFAULT_DRIVER AUDIO_WIIU
#elif defined(PSP) || defined(VITA)
#define AUDIO_DEFAULT_DRIVER AUDIO_PSP
#elif defined(_3DS)
#define AUDIO_DEFAULT_DRIVER AUDIO_CTR
#elif defined(HAVE_PULSE)
#define AUDIO_DEFAULT_DRIVER AUDIO_PULSE
#elif defined(HAVE_ALSA) && defined(HAVE_VIDEOCORE)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSATHREAD
#elif defined(HAVE_ALSA)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSA
#elif defined(HAVE_OSS)
#define AUDIO_DEFAULT_DRIVER AUDIO_OSS
#elif defined(HAVE_JACK)
#define AUDIO_DEFAULT_DRIVER AUDIO_JACK
#elif defined(HAVE_COREAUDIO)
#define AUDIO_DEFAULT_DRIVER AUDIO_COREAUDIO
#elif defined(HAVE_XAUDIO)
#define AUDIO_DEFAULT_DRIVER AUDIO_XAUDIO
#elif defined(HAVE_DSOUND)
#define AUDIO_DEFAULT_DRIVER AUDIO_DSOUND
#elif defined(HAVE_AL)
#define AUDIO_DEFAULT_DRIVER AUDIO_AL
#elif defined(HAVE_SL)
#define AUDIO_DEFAULT_DRIVER AUDIO_SL
#elif defined(EMSCRIPTEN)
#define AUDIO_DEFAULT_DRIVER AUDIO_RWEBAUDIO
#elif defined(HAVE_SDL)
#define AUDIO_DEFAULT_DRIVER AUDIO_SDL
#elif defined(HAVE_SDL2)
#define AUDIO_DEFAULT_DRIVER AUDIO_SDL2
#elif defined(HAVE_RSOUND)
#define AUDIO_DEFAULT_DRIVER AUDIO_RSOUND
#elif defined(HAVE_ROAR)
#define AUDIO_DEFAULT_DRIVER AUDIO_ROAR
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
#define AUDIO_DEFAULT_DRIVER AUDIO_EXT
#else
#define AUDIO_DEFAULT_DRIVER AUDIO_NULL
#endif

#if defined(PSP) || defined(EMSCRIPTEN)
#define AUDIO_DEFAULT_RESAMPLER_DRIVER  AUDIO_RESAMPLER_CC
#else
#define AUDIO_DEFAULT_RESAMPLER_DRIVER  AUDIO_RESAMPLER_SINC
#endif

#if defined(HAVE_FFMPEG)
#define RECORD_DEFAULT_DRIVER RECORD_FFMPEG
#else
#define RECORD_DEFAULT_DRIVER RECORD_NULL
#endif

#if defined(XENON)
#define INPUT_DEFAULT_DRIVER INPUT_XENON360
#elif defined(_XBOX360) || defined(_XBOX) || defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
#define INPUT_DEFAULT_DRIVER INPUT_XINPUT
#elif defined(ANDROID)
#define INPUT_DEFAULT_DRIVER INPUT_ANDROID
#elif defined(EMSCRIPTEN) && defined(HAVE_SDL2)
#define INPUT_DEFAULT_DRIVER INPUT_SDL2
#elif defined(EMSCRIPTEN)
#define INPUT_DEFAULT_DRIVER INPUT_RWEBINPUT
#elif defined(_WIN32)
#define INPUT_DEFAULT_DRIVER INPUT_DINPUT
#elif defined(__CELLOS_LV2__)
#define INPUT_DEFAULT_DRIVER INPUT_PS3
#elif defined(PSP) || defined(VITA)
#define INPUT_DEFAULT_DRIVER INPUT_PSP
#elif defined(_3DS)
#define INPUT_DEFAULT_DRIVER INPUT_CTR
#elif defined(GEKKO)
#define INPUT_DEFAULT_DRIVER INPUT_WII
#elif defined(WIIU)
#define INPUT_DEFAULT_DRIVER INPUT_WIIU
#elif defined(HAVE_UDEV)
#define INPUT_DEFAULT_DRIVER INPUT_UDEV
#elif defined(__linux__) && !defined(ANDROID)
#define INPUT_DEFAULT_DRIVER INPUT_LINUXRAW
#elif defined(HAVE_X11)
#define INPUT_DEFAULT_DRIVER INPUT_X
#elif defined(HAVE_WAYLAND)
#define INPUT_DEFAULT_DRIVER INPUT_WAYLAND
#elif defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
#define INPUT_DEFAULT_DRIVER INPUT_COCOA
#elif defined(__QNX__)
#define INPUT_DEFAULT_DRIVER INPUT_QNX
#elif defined(HAVE_SDL)
#define INPUT_DEFAULT_DRIVER INPUT_SDL
#elif defined(HAVE_SDL2)
#define INPUT_DEFAULT_DRIVER INPUT_SDL2
#elif defined(DJGPP)
#define INPUT_DEFAULT_DRIVER INPUT_DOS
#else
#define INPUT_DEFAULT_DRIVER INPUT_NULL
#endif

#if defined(__CELLOS_LV2__)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_PS3
#elif defined(HAVE_XINPUT)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_XINPUT
#elif defined(GEKKO)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_GX
#elif defined(WIIU)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_WIIU
#elif defined(_XBOX)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_XDK
#elif defined(PSP) || defined(VITA)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_PSP
#elif defined(_3DS)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_CTR
#elif defined(HAVE_DINPUT)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_DINPUT
#elif defined(HAVE_UDEV)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_UDEV
#elif defined(__linux) && !defined(ANDROID)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_LINUXRAW
#elif defined(ANDROID)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_ANDROID
#elif defined(HAVE_SDL) || defined(HAVE_SDL2)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_SDL
#elif defined(DJGPP)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_DOS
#elif defined(HAVE_HID)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_HID
#elif defined(__QNX__)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_QNX
#else
#define JOYPAD_DEFAULT_DRIVER JOYPAD_NULL
#endif

#if defined(HAVE_V4L2)
#define CAMERA_DEFAULT_DRIVER CAMERA_V4L2
#elif defined(EMSCRIPTEN)
#define CAMERA_DEFAULT_DRIVER CAMERA_RWEBCAM
#elif defined(ANDROID)
#define CAMERA_DEFAULT_DRIVER CAMERA_ANDROID
#elif defined(HAVE_AVFOUNDATION) && (defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH))
#define CAMERA_DEFAULT_DRIVER CAMERA_AVFOUNDATION
#else
#define CAMERA_DEFAULT_DRIVER CAMERA_NULL
#endif

#if defined(HAVE_LAKKA)
#define WIFI_DEFAULT_DRIVER WIFI_CONNMANCTL
#else
#define WIFI_DEFAULT_DRIVER WIFI_NULL
#endif

#if defined(ANDROID)
#define LOCATION_DEFAULT_DRIVER LOCATION_ANDROID
#elif defined(HAVE_CORELOCATION) && (defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH))
#define LOCATION_DEFAULT_DRIVER LOCATION_CORELOCATION
#else
#define LOCATION_DEFAULT_DRIVER LOCATION_NULL
#endif

#if defined(__CELLOS_LV2__)
#define OSK_DEFAULT_DRIVER OSK_PS3
#else
#define OSK_DEFAULT_DRIVER OSK_NULL
#endif

#if defined(HAVE_XUI)
#define MENU_DEFAULT_DRIVER MENU_XUI
#elif defined(HAVE_MATERIALUI) && defined(RARCH_MOBILE)
#define MENU_DEFAULT_DRIVER MENU_MATERIALUI
#elif defined(HAVE_XMB)
#define MENU_DEFAULT_DRIVER MENU_XMB
#elif defined(HAVE_RGUI)
#define MENU_DEFAULT_DRIVER MENU_RGUI
#else
#define MENU_DEFAULT_DRIVER MENU_NULL
#endif

#if defined(XENON) || defined(_XBOX360) || defined(__CELLOS_LV2__)
#define DEFAULT_ASPECT_RATIO 1.7778f
#elif defined(_XBOX1) || defined(GEKKO) || defined(ANDROID)
#define DEFAULT_ASPECT_RATIO 1.3333f
#else
#define DEFAULT_ASPECT_RATIO -1.0f
#endif

#ifdef RARCH_MOBILE
static const bool pointer_enable = true;
#else
static const bool pointer_enable = false;
#endif



/* Certain platforms might have assets stored in the bundle that
 * we need to extract to a user-writable directory on first boot.
 *
 * Examples include: Android, iOS/OSX) */
#if defined(ANDROID) || defined(IOS)
static bool bundle_assets_extract_enable = true;
#else
static bool bundle_assets_extract_enable = false;
#endif

static const bool def_history_list_enable = true;
static const bool def_playlist_entry_remove = true;

static const unsigned int def_user_language = 0;

#if (defined(_WIN32) && !defined(_XBOX)) || (defined(__linux) && !defined(ANDROID) && !defined(HAVE_LAKKA)) || (defined(__MACH__) && !defined(IOS))
static const bool def_mouse_enable = true;
#else
static const bool def_mouse_enable = false;
#endif

#ifdef HAVE_CHEEVOS
static const bool cheevos_enable = false;
#endif

/* VIDEO */

#if defined(_XBOX360)
#define DEFAULT_GAMMA 1
#else
#define DEFAULT_GAMMA 0
#endif

/* Windowed
 * Real x resolution = aspect * base_size * x scale
 * Real y resolution = base_size * y scale
 */
static const float scale = 3.0;

/* Fullscreen */

/* To start in Fullscreen, or not. */
static const bool fullscreen = false;

/* To use windowed mode or not when going fullscreen. */
static const bool windowed_fullscreen = true;

/* Which monitor to prefer. 0 is any monitor, 1 and up selects
 * specific monitors, 1 being the first monitor. */
static const unsigned monitor_index = 0;

/* Window */
/* Window size. A value of 0 uses window scale
 * multiplied by the core framebuffer size. */
static const unsigned window_x = 0;
static const unsigned window_y = 0;

/* Fullscreen resolution. A value of 0 uses the desktop
 * resolution. */
static const unsigned fullscreen_x = 0;
static const unsigned fullscreen_y = 0;

#if defined(RARCH_CONSOLE) || defined(__APPLE__)
static const bool load_dummy_on_core_shutdown = false;
#else
static const bool load_dummy_on_core_shutdown = true;
#endif
static const bool check_firmware_before_loading = false;
/* Forcibly disable composition.
 * Only valid on Windows Vista/7/8 for now. */
static const bool disable_composition = false;

/* Video VSYNC (recommended) */
static const bool vsync = true;

static const unsigned max_swapchain_images = 3;

/* Attempts to hard-synchronize CPU and GPU.
 * Can reduce latency at cost of performance. */
static const bool hard_sync = false;

/* Configures how many frames the GPU can run ahead of CPU.
 * 0: Syncs to GPU immediately.
 * 1: Syncs to previous frame.
 * 2: Etc ...
 */
static const unsigned hard_sync_frames = 0;

/* Sets how many milliseconds to delay after VSync before running the core.
 * Can reduce latency at cost of higher risk of stuttering.
 */
static const unsigned frame_delay = 0;

/* Inserts a black frame inbetween frames.
 * Useful for 120 Hz monitors who want to play 60 Hz material with eliminated
 * ghosting. video_refresh_rate should still be configured as if it
 * is a 60 Hz monitor (divide refresh rate by 2).
 */
static bool black_frame_insertion = false;

/* Uses a custom swap interval for VSync.
 * Set this to effectively halve monitor refresh rate.
 */
static unsigned swap_interval = 1;

/* Threaded video. Will possibly increase performance significantly
 * at the cost of worse synchronization and latency.
 */
static const bool video_threaded = false;

#if defined(HAVE_THREADS)
#if defined(GEKKO) || defined(PSP) || defined(_3DS) || defined(_XBOX1)
/* For single-core consoles right now it's better to have this be disabled. */
static const bool threaded_data_runloop_enable = false;
#else
static const bool threaded_data_runloop_enable = true;
#endif
#else
static const bool threaded_data_runloop_enable = false;
#endif

/* Set to true if HW render cores should get their private context. */
static const bool video_shared_context = false;

/* Sets GC/Wii screen width. */
static const unsigned video_viwidth = 640;

#ifdef GEKKO
/* Removes 480i flicker, smooths picture a little. */
static const bool video_vfilter = true;
#endif

/* Smooths picture. */
static const bool video_smooth = true;

/* On resize and fullscreen, rendering area will stay 4:3 */
static const bool force_aspect = true;

/* Enable use of shaders. */
#ifdef RARCH_CONSOLE
static const bool shader_enable = true;
#else
static const bool shader_enable = false;
#endif

/* Only scale in integer steps.
 * The base size depends on system-reported geometry and aspect ratio.
 * If video_force_aspect is not set, X/Y will be integer scaled independently.
 */
static const bool scale_integer = false;

/* Controls aspect ratio handling. */

/* Automatic */
static const float aspect_ratio = DEFAULT_ASPECT_RATIO;

/* 1:1 PAR */
static const bool aspect_ratio_auto = false;

#if defined(__CELLOS_LV2) || defined(_XBOX360)
static unsigned aspect_ratio_idx = ASPECT_RATIO_16_9;
#elif defined(PSP)
static unsigned aspect_ratio_idx = ASPECT_RATIO_CORE;
#elif defined(RARCH_CONSOLE)
static unsigned aspect_ratio_idx = ASPECT_RATIO_4_3;
#else
static unsigned aspect_ratio_idx = ASPECT_RATIO_CORE;
#endif

/* Save configuration file on exit. */
static bool config_save_on_exit = true;

static bool show_hidden_files = true;

static const bool overlay_hide_in_menu = true;

static const bool display_keyboard_overlay = false;

#ifdef HAVE_MENU
#include "menu/menu_display.h"

static bool default_block_config_read = true;

#ifdef HAVE_XMB
static unsigned xmb_scale_factor = 100;
static unsigned xmb_alpha_factor = 75;
static unsigned xmb_icon_theme   = XMB_ICON_THEME_MONOCHROME;
static unsigned xmb_theme        = XMB_THEME_ELECTRIC_BLUE;
#ifdef HAVE_LAKKA
static bool xmb_shadows_enable   = false;
#else
static bool xmb_shadows_enable   = true;
#endif
static bool xmb_show_settings    = true;
#ifdef HAVE_IMAGEVIEWER
static bool xmb_show_images      = true;
#endif
#ifdef HAVE_FFMPEG
static bool xmb_show_music       = true;
static bool xmb_show_video       = true;
#endif
static bool xmb_show_history     = true;
#ifdef HAVE_LIBRETRODB
static bool xmb_show_add     	 = true;
#endif
#endif

static float menu_wallpaper_opacity = 0.300;

static float menu_footer_opacity = 1.000;

static float menu_header_opacity = 1.000;

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL) || defined(HAVE_VULKAN)
#if defined(HAVE_OPENGLES2) || (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
static unsigned menu_shader_pipeline = 1;
#else
static unsigned menu_shader_pipeline = 2;
#endif
#endif

static bool show_advanced_settings    = true;
static const uint32_t menu_entry_normal_color = 0xffffffff;
static const uint32_t menu_entry_hover_color  = 0xff64ff64;
static const uint32_t menu_title_color        = 0xff64ff64;

#else
static bool default_block_config_read = false;
#endif

static bool default_game_specific_options = false;
static bool default_auto_overrides_enable = true;
static bool default_auto_remaps_enable = true;
static bool default_auto_shaders_enable = true;

static bool default_sort_savefiles_enable = false;
static bool default_sort_savestates_enable = false;

#if defined(__CELLOS_LV2__) || defined(_XBOX1) || defined(_XBOX360)
static unsigned menu_toggle_gamepad_combo    = INPUT_TOGGLE_L3_R3;
#elif defined(VITA)
static unsigned menu_toggle_gamepad_combo    = INPUT_TOGGLE_L1_R1_START_SELECT;
#else
static unsigned menu_toggle_gamepad_combo    = INPUT_TOGGLE_NONE;
#endif

#if defined(VITA)
static unsigned input_backtouch_enable       = false;
static unsigned input_backtouch_toggle       = false;
#endif

static bool all_users_control_menu = false;

#if defined(ANDROID) || defined(_WIN32)
static bool menu_swap_ok_cancel_buttons = true;
#else
static bool menu_swap_ok_cancel_buttons = false;
#endif

/* Crop overscanned frames. */
static const bool crop_overscan = true;

/* Font size for on-screen messages. */
#if defined(HAVE_LIBDBGFONT)
static const float font_size = 1.0f;
#else
static const float font_size = 32;
#endif

/* Offset for where messages will be placed on-screen.
 * Values are in range [0.0, 1.0]. */
static const float message_pos_offset_x = 0.05;
#if defined(_XBOX1)
static const float message_pos_offset_y = 0.90;
#else
static const float message_pos_offset_y = 0.05;
#endif

/* Color of the message.
 * RGB hex value. */
static const uint32_t message_color = 0xffff00;

/* Record post-filtered (CPU filter) video,
 * rather than raw game output. */
static const bool post_filter_record = false;

/* Screenshots post-shaded GPU output if available. */
static const bool gpu_screenshot = true;

/* Screenshots named automatically. */
static const bool auto_screenshot_filename = true;

/* Record post-shaded GPU output instead of raw game footage if available. */
static const bool gpu_record = false;

/* OSD-messages. */
static const bool font_enable = true;

/* The accurate refresh rate of your monitor (Hz).
 * This is used to calculate audio input rate with the formula:
 * audio_input_rate = game_input_rate * display_refresh_rate /
 * game_refresh_rate.
 *
 * If the implementation does not report any values,
 * NTSC defaults will be assumed for compatibility.
 * This value should stay close to 60Hz to avoid large pitch changes.
 * If your monitor does not run at 60Hz, or something close to it,
 * disable VSync, and leave this at its default. */
#ifdef _3DS
static const float refresh_rate = (32730.0 * 8192.0) / 4481134.0 ;
#else
static const float refresh_rate = 60/1.001;
#endif

/* Allow games to set rotation. If false, rotation requests are
 * honored, but ignored.
 * Used for setups where one manually rotates the monitor. */
static const bool allow_rotate = true;

/* AUDIO */

/* Will enable audio or not. */
static const bool audio_enable = true;

/* Output samplerate. */
#ifdef GEKKO
static const unsigned out_rate = 32000;
#elif defined(_3DS)
static const unsigned out_rate = 32730;
#else
static const unsigned out_rate = 48000;
#endif

/* Audio device (e.g. hw:0,0 or /dev/audio). If NULL, will use defaults. */
static const char *audio_device = NULL;

/* Desired audio latency in milliseconds. Might not be honored
 * if driver can't provide given latency. */
#if defined(ANDROID) || defined(EMSCRIPTEN)
/* For most Android devices, 64ms is way too low. */
static const int out_latency = 128;
#else
static const int out_latency = 64;
#endif

/* Will sync audio. (recommended) */
static const bool audio_sync = true;

/* Audio rate control. */
#if !defined(RARCH_CONSOLE)
static const bool rate_control = true;
#else
static const bool rate_control = false;
#endif

/* Rate control delta. Defines how much rate_control
 * is allowed to adjust input rate. */
static const float rate_control_delta = 0.005;

/* Maximum timing skew. Defines how much adjust_system_rates
 * is allowed to adjust input rate. */
static const float max_timing_skew = 0.05;

/* Default audio volume in dB. (0.0 dB == unity gain). */
static const float audio_volume = 0.0;

/* MISC */

/* Enables displaying the current frames per second. */
static const bool fps_show = false;

/* Enables use of rewind. This will incur some memory footprint
 * depending on the save state buffer. */
static const bool rewind_enable = false;

/* The buffer size for the rewind buffer. This needs to be about
 * 15-20MB per minute. Very game dependant. */
static const unsigned rewind_buffer_size = 20 << 20; /* 20MiB */

/* How many frames to rewind at a time. */
static const unsigned rewind_granularity = 1;

/* Pause gameplay when gameplay loses focus. */
#ifdef EMSCRIPTEN
static const bool pause_nonactive = false;
#else
static const bool pause_nonactive = true;
#endif

/* Saves non-volatile SRAM at a regular interval.
 * It is measured in seconds. A value of 0 disables autosave. */
static const unsigned autosave_interval = 0;

/* Netplay without savestates/rewind */
static const bool netplay_stateless_mode = false;

/* When being client over netplay, use keybinds for
 * user 1 rather than user 2. */
static const bool netplay_client_swap_input = true;

static const bool netplay_nat_traversal = false;

static const unsigned netplay_delay_frames = 16;

static const int netplay_check_frames = 30;

/* On save state load, block SRAM from being overwritten.
 * This could potentially lead to buggy games. */
static const bool block_sram_overwrite = false;

/* When saving savestates, state index is automatically
 * incremented before saving.
 * When the content is loaded, state index will be set
 * to the highest existing value. */
static const bool savestate_auto_index = false;

/* Automatically saves a savestate at the end of RetroArch's lifetime.
 * The path is $SRAM_PATH.auto.
 * RetroArch will automatically load any savestate with this path on
 * startup if savestate_auto_load is set. */
static const bool savestate_auto_save = false;
static const bool savestate_auto_load = false;

static const bool savestate_thumbnail_enable = false;

/* Slowmotion ratio. */
static const float slowmotion_ratio = 3.0;

/* Maximum fast forward ratio. */
static const float fastforward_ratio = 0.0;

/* Enable stdin/network command interface. */
static const bool network_cmd_enable = false;
static const uint16_t network_cmd_port = 55355;
static const bool stdin_cmd_enable = false;

static const uint16_t network_remote_base_port = 55400;
/* Number of entries that will be kept in content history playlist file. */
static const unsigned default_content_history_size = 100;

/* Show Menu start-up screen on boot. */
static const bool default_menu_show_start_screen = true;

#ifdef RARCH_MOBILE
static const bool menu_dpi_override_enable = false;
#else
static const bool menu_dpi_override_enable = true;
#endif

#ifdef RARCH_MOBILE
static const unsigned menu_dpi_override_value = 72;
#elif defined(__CELLOS_LV2__)
static const unsigned menu_dpi_override_value = 360;
#else
static const unsigned menu_dpi_override_value = 200;
#endif

/* Log level for libretro cores (GET_LOG_INTERFACE). */
static const unsigned libretro_log_level = 0;

#ifndef RARCH_DEFAULT_PORT
#define RARCH_DEFAULT_PORT 55435
#endif

/* KEYBINDS, JOYPAD */

/* Axis threshold (between 0.0 and 1.0)
 * How far an axis must be tilted to result in a button press. */
static const float axis_threshold = 0.5;

/* Describes speed of which turbo-enabled buttons toggle. */
static const unsigned turbo_period = 6;
static const unsigned turbo_duty_cycle = 3;

/* Enable input auto-detection. Will attempt to autoconfigure
 * gamepads, plug-and-play style. */
static const bool input_autodetect_enable = true;

/* Show the input descriptors set by the core instead
 * of the default ones. */
static const bool input_descriptor_label_show = true;

static const bool input_descriptor_hide_unbound = false;

static const unsigned input_max_users = 5;

static const unsigned input_poll_type_behavior = 2;

static const unsigned input_bind_timeout = 5;

static const unsigned menu_thumbnails_default = 3;

#ifdef IOS
static const bool ui_companion_start_on_boot = false;
#else
static const bool ui_companion_start_on_boot = true;
#endif

static const bool ui_companion_enable = false;

#if defined(ANDROID)
#if defined(ANDROID_ARM)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/armeabi-v7a/";
#elif defined(ANDROID_X86)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/x86/";
#else
static char buildbot_server_url[] = "";
#endif
#elif defined(__QNX__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/blackberry/latest/";
#elif defined(IOS)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/apple/ios/latest/";
#elif defined(OSX)
#if defined(__x86_64__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__)
static char buildbot_server_url[] = "http://bot.libretro.com/nightly/apple/osx/x86/latest/";
#else
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/apple/osx/ppc/latest/";
#endif
#elif defined(_WIN32) && !defined(_XBOX)
#if defined(__x86_64__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/win-x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__) || defined(_M_IX86) || defined(_M_IA64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/win-x86/latest/";
#endif
#elif defined(__linux__)
#if defined(__x86_64__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/linux/x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/linux/x86/latest/";
#else
static char buildbot_server_url[] = "";
#endif
#else
static char buildbot_server_url[] = "";
#endif

static char buildbot_assets_server_url[] = "http://buildbot.libretro.com/assets/";

#endif
