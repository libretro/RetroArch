/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include "driver.h"
#include "gfx/video_driver.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

enum
{
   VIDEO_GL = 0,
   VIDEO_VULKAN,
   VIDEO_XVIDEO,
   VIDEO_SDL,
   VIDEO_SDL2,
   VIDEO_EXT,
   VIDEO_WII,
   VIDEO_XENON360,
   VIDEO_XDK_D3D,
   VIDEO_PSP1,
   VIDEO_VITA2D,
   VIDEO_CTR,
   VIDEO_D3D9,
   VIDEO_VG,
   VIDEO_NULL,
   VIDEO_OMAP,
   VIDEO_EXYNOS,
   VIDEO_SUNXI,
   VIDEO_DISPMANX,

   AUDIO_RSOUND,
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
   AUDIO_RWEBAUDIO,
   AUDIO_PSP,
   AUDIO_CTR,
   AUDIO_NULL,

   AUDIO_RESAMPLER_CC,
   AUDIO_RESAMPLER_SINC,
   AUDIO_RESAMPLER_NEAREST,

   INPUT_ANDROID,
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
   INPUT_XINPUT,
   INPUT_UDEV,
   INPUT_LINUXRAW,
   INPUT_COCOA,
   INPUT_QNX,
   INPUT_RWEBINPUT,
   INPUT_NULL,

   JOYPAD_PS3,
   JOYPAD_XINPUT,
   JOYPAD_GX,
   JOYPAD_XDK,
   JOYPAD_PSP,
   JOYPAD_CTR,
   JOYPAD_DINPUT,
   JOYPAD_UDEV,
   JOYPAD_LINUXRAW,
   JOYPAD_ANDROID,
   JOYPAD_SDL,
   JOYPAD_HID,
   JOYPAD_QNX,
   JOYPAD_NULL,

   CAMERA_V4L2,
   CAMERA_RWEBCAM,
   CAMERA_ANDROID,
   CAMERA_AVFOUNDATION,
   CAMERA_NULL,

   LOCATION_ANDROID,
   LOCATION_CORELOCATION,
   LOCATION_NULL,

   OSK_PS3,
   OSK_NULL,

   MENU_RGUI,
   MENU_XUI,
   MENU_MATERIALUI,
   MENU_XMB,
   MENU_NUKLEAR,

   RECORD_FFMPEG,
   RECORD_NULL
};

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) || defined(__CELLOS_LV2__)
#define VIDEO_DEFAULT_DRIVER VIDEO_GL
#elif defined(GEKKO)
#define VIDEO_DEFAULT_DRIVER VIDEO_WII
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
#elif defined(PSP) || defined(VITA)
#define AUDIO_DEFAULT_DRIVER AUDIO_PSP
#elif defined(_3DS)
#define AUDIO_DEFAULT_DRIVER AUDIO_CTR
#elif defined(HAVE_ALSA) && defined(HAVE_VIDEOCORE)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSATHREAD
#elif defined(HAVE_ALSA)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSA
#elif defined(HAVE_PULSE)
#define AUDIO_DEFAULT_DRIVER AUDIO_PULSE
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

#ifdef PSP
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
#else
#define INPUT_DEFAULT_DRIVER INPUT_NULL
#endif

#if defined(__CELLOS_LV2__)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_PS3
#elif defined(HAVE_XINPUT)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_XINPUT
#elif defined(GEKKO)
#define JOYPAD_DEFAULT_DRIVER JOYPAD_GX
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
#elif defined(IOS) || defined(ANDROID)
#define MENU_DEFAULT_DRIVER MENU_MATERIALUI
#elif defined(__MACH__) && defined(HAVE_XMB) || defined(__CELLOS_LV2__)
#define MENU_DEFAULT_DRIVER MENU_XMB
#else
#define MENU_DEFAULT_DRIVER MENU_RGUI
#endif

#if defined(XENON) || defined(_XBOX360) || defined(__CELLOS_LV2__)
#define DEFAULT_ASPECT_RATIO 1.7778f
#elif defined(_XBOX1) || defined(GEKKO) || defined(ANDROID) || defined(__QNX__)
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

static const unsigned int def_user_language = 0;

#if (defined(__APPLE__) && defined(__MACH__) && defined(OSX)) || (defined(_WIN32) && !defined(_XBOX))
static const bool def_mouse_enable = true;
#else
static const bool def_mouse_enable = false;
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

/* Fullscreen resolution. A value of 0 uses the desktop
 * resolution. */
static const unsigned fullscreen_x = 0;
static const unsigned fullscreen_y = 0;

#if defined(RARCH_CONSOLE) || defined(__APPLE__)
static const bool load_dummy_on_core_shutdown = false;
#else
static const bool load_dummy_on_core_shutdown = true;
#endif

/* Forcibly disable composition.
 * Only valid on Windows Vista/7/8 for now. */
static const bool disable_composition = false;

/* Video VSYNC (recommended) */
static const bool vsync = true;

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

/* Removes 480i flicker, smooths picture a little. */
static const bool video_vfilter = true;

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

static const bool default_overlay_enable = false;

static const bool overlay_hide_in_menu = true;

#ifdef HAVE_MENU
static bool default_block_config_read = true;

static unsigned xmb_scale_factor = 100;
static unsigned xmb_alpha_factor = 75;
static unsigned xmb_theme        = 0;

#ifdef HAVE_LAKKA
static bool xmb_shadows_enable   = false;
#else
static bool xmb_shadows_enable   = true;
#endif

static unsigned menu_background_gradient = 4;

#if defined(HAVE_OPENGLES2)
static unsigned menu_shader_pipeline = 1;
#else
static unsigned menu_shader_pipeline = 2;
#endif

static bool show_advanced_settings    = true;
static const uint32_t menu_entry_normal_color = 0xffffffff;
static const uint32_t menu_entry_hover_color  = 0xff64ff64;
static const uint32_t menu_title_color        = 0xff64ff64;
#else
static bool default_block_config_read = false;
#endif

#ifdef RARCH_CONSOLE
static bool default_core_specific_config = true;
#else
static bool default_core_specific_config = false;
#endif

static bool default_game_specific_options = false;
static bool default_auto_overrides_enable = true;
static bool default_auto_remaps_enable = true;

static bool default_sort_savefiles_enable = false;
static bool default_sort_savestates_enable = false;

static unsigned default_menu_btn_ok          = RETRO_DEVICE_ID_JOYPAD_A;
static unsigned default_menu_btn_cancel      = RETRO_DEVICE_ID_JOYPAD_B;
static unsigned default_menu_btn_search      = RETRO_DEVICE_ID_JOYPAD_X;
static unsigned default_menu_btn_default     = RETRO_DEVICE_ID_JOYPAD_START;
static unsigned default_menu_btn_info        = RETRO_DEVICE_ID_JOYPAD_SELECT;
static unsigned default_menu_btn_scroll_down = RETRO_DEVICE_ID_JOYPAD_R;
static unsigned default_menu_btn_scroll_up   = RETRO_DEVICE_ID_JOYPAD_L;

#if defined(__CELLOS_LV2__) || defined(_XBOX1) || defined(_XBOX360)
static unsigned menu_toggle_gamepad_combo    = 2;
#else
static unsigned menu_toggle_gamepad_combo    = 0;
#endif

#ifdef ANDROID
static bool back_as_menu_toggle_enable = true;
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
#elif defined(RARCH_CONSOLE)
static const float refresh_rate = 60/1.001;
#else
static const float refresh_rate = 59.95;
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
#ifdef ANDROID
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
static const bool pause_nonactive = true;

/* Saves non-volatile SRAM at a regular interval.
 * It is measured in seconds. A value of 0 disables autosave. */
static const unsigned autosave_interval = 0;

/* When being client over netplay, use keybinds for
 * user 1 rather than user 2. */
static const bool netplay_client_swap_input = true;

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
#elif defined(__i386__) || defined(__i486__) || defined(__i686__)
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

#ifndef IS_SALAMANDER
#include "intl/intl.h"

/* User 1 */
static const struct retro_keybind retro_keybinds_1[] = {
    /*     | RetroPad button            | desc                           | keyboard key  | js btn |     js axis   | */
   { true, RETRO_DEVICE_ID_JOYPAD_B,      RETRO_LBL_JOYPAD_B,              RETROK_z,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_Y,      RETRO_LBL_JOYPAD_Y,              RETROK_a,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_LBL_JOYPAD_SELECT,         RETROK_RSHIFT,  NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_START,  RETRO_LBL_JOYPAD_START,          RETROK_RETURN,  NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_UP,     RETRO_LBL_JOYPAD_UP,             RETROK_UP,      NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_DOWN,   RETRO_LBL_JOYPAD_DOWN,           RETROK_DOWN,    NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_LEFT,   RETRO_LBL_JOYPAD_LEFT,           RETROK_LEFT,    NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_RIGHT,  RETRO_LBL_JOYPAD_RIGHT,          RETROK_RIGHT,   NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_A,      RETRO_LBL_JOYPAD_A,              RETROK_x,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_X,      RETRO_LBL_JOYPAD_X,              RETROK_s,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L,      RETRO_LBL_JOYPAD_L,              RETROK_q,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R,      RETRO_LBL_JOYPAD_R,              RETROK_w,       NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L2,     RETRO_LBL_JOYPAD_L2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R2,     RETRO_LBL_JOYPAD_R2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L3,     RETRO_LBL_JOYPAD_L3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R3,     RETRO_LBL_JOYPAD_R3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },

   { true, RARCH_ANALOG_LEFT_X_PLUS,      RETRO_LBL_ANALOG_LEFT_X_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_MINUS,     RETRO_LBL_ANALOG_LEFT_X_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_PLUS,      RETRO_LBL_ANALOG_LEFT_Y_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_MINUS,     RETRO_LBL_ANALOG_LEFT_Y_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_PLUS,     RETRO_LBL_ANALOG_RIGHT_X_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_MINUS,    RETRO_LBL_ANALOG_RIGHT_X_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_PLUS,     RETRO_LBL_ANALOG_RIGHT_Y_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_MINUS,    RETRO_LBL_ANALOG_RIGHT_Y_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },

   { true, RARCH_TURBO_ENABLE,             RETRO_LBL_TURBO_ENABLE,         RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FAST_FORWARD_KEY,         RETRO_LBL_FAST_FORWARD_KEY,     RETROK_SPACE,   NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FAST_FORWARD_HOLD_KEY,    RETRO_LBL_FAST_FORWARD_HOLD_KEY,RETROK_l,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_LOAD_STATE_KEY,           RETRO_LBL_LOAD_STATE_KEY,       RETROK_F4,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SAVE_STATE_KEY,           RETRO_LBL_SAVE_STATE_KEY,       RETROK_F2,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FULLSCREEN_TOGGLE_KEY,    RETRO_LBL_FULLSCREEN_TOGGLE_KEY,RETROK_f,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_QUIT_KEY,                 RETRO_LBL_QUIT_KEY,             RETROK_ESCAPE,  NO_BTN, 0, AXIS_NONE },
   { true, RARCH_STATE_SLOT_PLUS,          RETRO_LBL_STATE_SLOT_PLUS,      RETROK_F7,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_STATE_SLOT_MINUS,         RETRO_LBL_STATE_SLOT_MINUS,     RETROK_F6,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_REWIND,                   RETRO_LBL_REWIND,               RETROK_r,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_MOVIE_RECORD_TOGGLE,      RETRO_LBL_MOVIE_RECORD_TOGGLE,  RETROK_o,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_PAUSE_TOGGLE,             RETRO_LBL_PAUSE_TOGGLE,         RETROK_p,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_FRAMEADVANCE,             RETRO_LBL_FRAMEADVANCE,         RETROK_k,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_RESET,                    RETRO_LBL_RESET,                RETROK_h,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SHADER_NEXT,              RETRO_LBL_SHADER_NEXT,          RETROK_m,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SHADER_PREV,              RETRO_LBL_SHADER_PREV,          RETROK_n,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_CHEAT_INDEX_PLUS,         RETRO_LBL_CHEAT_INDEX_PLUS,     RETROK_y,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_CHEAT_INDEX_MINUS,        RETRO_LBL_CHEAT_INDEX_MINUS,    RETROK_t,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_CHEAT_TOGGLE,             RETRO_LBL_CHEAT_TOGGLE,         RETROK_u,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SCREENSHOT,               RETRO_LBL_SCREENSHOT,           RETROK_F8,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_MUTE,                     RETRO_LBL_MUTE,                 RETROK_F9,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_OSK,                      RETRO_LBL_OSK,                  RETROK_F12,      NO_BTN, 0, AXIS_NONE },
   { true, RARCH_NETPLAY_FLIP,             RETRO_LBL_NETPLAY_FLIP,         RETROK_i,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_SLOWMOTION,               RETRO_LBL_SLOWMOTION,           RETROK_e,       NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ENABLE_HOTKEY,            RETRO_LBL_ENABLE_HOTKEY,        RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_VOLUME_UP,                RETRO_LBL_VOLUME_UP,            RETROK_KP_PLUS, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_VOLUME_DOWN,              RETRO_LBL_VOLUME_DOWN,          RETROK_KP_MINUS,NO_BTN, 0, AXIS_NONE },
   { true, RARCH_OVERLAY_NEXT,             RETRO_LBL_OVERLAY_NEXT,         RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_DISK_EJECT_TOGGLE,        RETRO_LBL_DISK_EJECT_TOGGLE,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_DISK_NEXT,                RETRO_LBL_DISK_NEXT,            RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_DISK_PREV,                RETRO_LBL_DISK_PREV,            RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_GRAB_MOUSE_TOGGLE,        RETRO_LBL_GRAB_MOUSE_TOGGLE,    RETROK_F11,     NO_BTN, 0, AXIS_NONE },
   { true, RARCH_MENU_TOGGLE,              RETRO_LBL_MENU_TOGGLE,          RETROK_F1,      NO_BTN, 0, AXIS_NONE },
};

/* Users 2 to MAX_USERS */
static const struct retro_keybind retro_keybinds_rest[] = {
    /*     | RetroPad button            | desc                           | keyboard key  | js btn |     js axis   | */
   { true, RETRO_DEVICE_ID_JOYPAD_B,      RETRO_LBL_JOYPAD_B,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_Y,      RETRO_LBL_JOYPAD_Y,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_LBL_JOYPAD_SELECT,         RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_START,  RETRO_LBL_JOYPAD_START,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_UP,     RETRO_LBL_JOYPAD_UP,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_DOWN,   RETRO_LBL_JOYPAD_DOWN,           RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_LEFT,   RETRO_LBL_JOYPAD_LEFT,           RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_RIGHT,  RETRO_LBL_JOYPAD_RIGHT,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_A,      RETRO_LBL_JOYPAD_A,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_X,      RETRO_LBL_JOYPAD_X,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L,      RETRO_LBL_JOYPAD_L,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R,      RETRO_LBL_JOYPAD_R,              RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L2,     RETRO_LBL_JOYPAD_L2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R2,     RETRO_LBL_JOYPAD_R2,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L3,     RETRO_LBL_JOYPAD_L3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R3,     RETRO_LBL_JOYPAD_R3,             RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },

   { true, RARCH_ANALOG_LEFT_X_PLUS,      RETRO_LBL_ANALOG_LEFT_X_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_MINUS,     RETRO_LBL_ANALOG_LEFT_X_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_PLUS,      RETRO_LBL_ANALOG_LEFT_Y_PLUS,    RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_MINUS,     RETRO_LBL_ANALOG_LEFT_Y_MINUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_PLUS,     RETRO_LBL_ANALOG_RIGHT_X_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_MINUS,    RETRO_LBL_ANALOG_RIGHT_X_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_PLUS,     RETRO_LBL_ANALOG_RIGHT_Y_PLUS,   RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_MINUS,    RETRO_LBL_ANALOG_RIGHT_Y_MINUS,  RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
   { true, RARCH_TURBO_ENABLE,            RETRO_LBL_TURBO_ENABLE,          RETROK_UNKNOWN, NO_BTN, 0, AXIS_NONE },
};

#endif

#endif
