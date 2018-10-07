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
#include <audio/audio_resampler.h>
#include "configuration.h"
#include "gfx/video_defines.h"
#include "input/input_driver.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#if defined(HW_RVL)
#define MAX_GAMMA_SETTING 30
#elif defined(GEKKO)
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

#if defined(XENON) || defined(_XBOX360) || defined(__CELLOS_LV2__)
#define DEFAULT_ASPECT_RATIO 1.7778f
#elif defined(_XBOX1) || defined(GEKKO) || defined(ANDROID)
#define DEFAULT_ASPECT_RATIO 1.3333f
#else
#define DEFAULT_ASPECT_RATIO -1.0f
#endif

#if defined(RARCH_MOBILE) || defined(HAVE_LIBNX)
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

#ifdef HAVE_MATERIALUI
static bool materialui_icons_enable      = true;
#endif

static const unsigned crt_switch_resolution  = CRT_SWITCH_NONE; 	
static const int crt_switch_resolution_super = 2560; 
static const int crt_switch_center_adjust    = 0;

static const bool def_history_list_enable    = true;
static const bool def_playlist_entry_remove  = true;
static const bool def_playlist_entry_rename  = true;

static const unsigned int def_user_language  = 0;

#if (defined(_WIN32) && !defined(_XBOX)) || (defined(__linux) && !defined(ANDROID) && !defined(HAVE_LAKKA)) || (defined(__MACH__) && !defined(IOS)) || defined(EMSCRIPTEN)
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

/* Amount of transparency to use for the main window.
 * 1 is the most transparent while 100 is opaque.
 */
static const unsigned window_opacity = 100;

/* Whether to show the usual window decorations like border, titlebar etc. */
static const bool window_decorations = true;

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

static const bool adaptive_vsync = false;

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
#if defined(HAVE_LIBNX)
static const bool video_threaded = true;
#else
static const bool video_threaded = false;
#endif

#if defined(HAVE_THREADS)
#if defined(GEKKO) || defined(PSP)
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

#if defined(__CELLOS_LV2) || defined(_XBOX360) || defined(ANDROID_AARCH64)
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

static bool show_hidden_files = false;

static const bool overlay_hide_in_menu = true;

static const bool display_keyboard_overlay = false;

#ifdef HAKCHI
static const float default_input_overlay_opacity = 0.5f;
#else
static const float default_input_overlay_opacity = 0.7f;
#endif

#ifdef HAVE_MENU
#include "menu/menu_driver.h"

static bool default_block_config_read    = true;

static bool quick_menu_show_take_screenshot             = true;
static bool quick_menu_show_save_load_state             = true;
static bool quick_menu_show_undo_save_load_state        = true;
static bool quick_menu_show_add_to_favorites            = true;
static bool quick_menu_show_options                     = true;
static bool quick_menu_show_controls                    = true;
static bool quick_menu_show_cheats                      = true;
static bool quick_menu_show_shaders                     = true;
static bool quick_menu_show_information                 = true;
static bool quick_menu_show_recording                   = true;
static bool quick_menu_show_streaming                   = true;

static bool quick_menu_show_save_core_overrides         = true;
static bool quick_menu_show_save_game_overrides         = true;
static bool quick_menu_show_save_content_dir_overrides  = true;

static bool kiosk_mode_enable            = false;

static bool menu_show_online_updater     = true;
static bool menu_show_load_core          = true;
static bool menu_show_load_content       = true;
static bool menu_show_information        = true;
static bool menu_show_configurations     = true;
static bool menu_show_help               = true;
static bool menu_show_quit_retroarch     = true;
static bool menu_show_reboot             = true;
#ifdef HAVE_LAKKA_SWITCH
static bool menu_show_shutdown           = false;
#else
static bool menu_show_shutdown           = true;
#endif
#if defined(HAVE_LAKKA) || defined(VITA) || defined(_3DS)
static bool menu_show_core_updater       = false;
#else
static bool menu_show_core_updater       = true;
#endif

static bool content_show_settings    = true;
static bool content_show_favorites   = true;
#ifdef HAVE_IMAGEVIEWER
static bool content_show_images      = true;
#endif
static bool content_show_music       = true;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
static bool content_show_video       = true;
#endif
#ifdef HAVE_NETWORKING
static bool content_show_netplay     = true;
#endif
static bool content_show_history     = true;
#ifdef HAVE_LIBRETRODB
static bool content_show_add     	 = true;
#endif
static bool content_show_playlists   = true;

#ifdef HAVE_XMB
static unsigned xmb_scale_factor = 100;
static unsigned xmb_alpha_factor = 75;
static unsigned menu_font_color_red = 255;
static unsigned menu_font_color_green = 255;
static unsigned menu_font_color_blue = 255;
static unsigned xmb_menu_layout  = 0;
static unsigned xmb_icon_theme   = XMB_ICON_THEME_MONOCHROME;
static unsigned xmb_theme        = XMB_THEME_ELECTRIC_BLUE;
#if defined(HAVE_LAKKA) || defined(__arm__) || defined(__PPC64__) || defined(__ppc64__) || defined(__powerpc64__) || defined(__powerpc__) || defined(__ppc__) || defined(__POWERPC__)
static bool xmb_shadows_enable   = false;
#else
static bool xmb_shadows_enable   = true;
#endif
#endif

static bool automatically_add_content_to_playlist = false;

static float menu_framebuffer_opacity = 0.900;

static float menu_wallpaper_opacity = 0.300;

static float menu_footer_opacity = 1.000;

static float menu_header_opacity = 1.000;

#if defined(HAVE_OPENGLES2) || (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
static unsigned menu_shader_pipeline = 1;
#else
static unsigned menu_shader_pipeline = 2;
#endif

static bool show_advanced_settings            = false;
static const uint32_t menu_entry_normal_color = 0xffffffff;
static const uint32_t menu_entry_hover_color  = 0xff64ff64;
static const uint32_t menu_title_color        = 0xff64ff64;

#else
static bool default_block_config_read = false;
static bool automatically_add_content_to_playlist = false;
#endif

static bool default_game_specific_options = true;
static bool default_auto_overrides_enable = true;
static bool default_auto_remaps_enable = true;
static bool default_auto_shaders_enable = true;

static bool default_sort_savefiles_enable = false;
static bool default_sort_savestates_enable = false;

static bool default_savestates_in_content_dir = false;
static bool default_savefiles_in_content_dir = false;
static bool default_systemfiles_in_content_dir = false;
static bool default_screenshots_in_content_dir = false;

#if defined(__CELLOS_LV2__) || defined(_XBOX1) || defined(_XBOX360)
static unsigned menu_toggle_gamepad_combo    = INPUT_TOGGLE_L3_R3;
#elif defined(VITA)
static unsigned menu_toggle_gamepad_combo    = INPUT_TOGGLE_L1_R1_START_SELECT;
#elif defined(SWITCH)
static unsigned menu_toggle_gamepad_combo    = INPUT_TOGGLE_START_SELECT;
#else
static unsigned menu_toggle_gamepad_combo    = INPUT_TOGGLE_NONE;
#endif

#if defined(VITA)
static unsigned input_backtouch_enable       = false;
static unsigned input_backtouch_toggle       = false;
#endif

static bool show_physical_inputs             = true;

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

static const bool message_bgcolor_enable = false;
static const uint32_t message_bgcolor_red = 0;
static const uint32_t message_bgcolor_green = 0;
static const uint32_t message_bgcolor_blue = 0;
static const float message_bgcolor_opacity = 1.0f;

/* Record post-filtered (CPU filter) video,
 * rather than raw game output. */
static const bool post_filter_record = false;

/* Screenshots post-shaded GPU output if available. */
static const bool gpu_screenshot = true;

/* Watch shader files for changes and auto-apply as necessary. */
static const bool video_shader_watch_files = false;

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
static const float refresh_rate     = (32730.0 * 8192.0) / 4481134.0 ;
static const float crt_refresh_rate = (32730.0 * 8192.0) / 4481134.0 ;
#else
static const float refresh_rate     = 60/1.001;
static const float crt_refresh_rate = 60/1.001;
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

/* Default audio volume of the audio mixer in dB. (0.0 dB == unity gain). */
static const float audio_mixer_volume = 0.0;

#ifdef HAVE_WASAPI
/* WASAPI defaults */
static const bool wasapi_exclusive_mode  = true;
static const bool wasapi_float_format    = false;
static const int wasapi_sh_buffer_length = -16; /* auto */
#endif

/* MISC */

/* Enables displaying the current frames per second. */
static const bool fps_show = false;

/* Show frame count on FPS display */
static const bool framecount_show = true;

/* Enables use of rewind. This will incur some memory footprint
 * depending on the save state buffer. */
static const bool rewind_enable = false;

/* When set, any time a cheat is toggled it is immediately applied. */
static const bool apply_cheats_after_toggle = false;

/* When set, all enabled cheats are auto-applied when a game is loaded. */
static const bool apply_cheats_after_load = false;

/* The buffer size for the rewind buffer. This needs to be about
 * 15-20MB per minute. Very game dependant. */
static const unsigned rewind_buffer_size = 20 << 20; /* 20MiB */

/* The amount of MB to increase/decrease the rewind_buffer_size when it is changed via the UI. */
static const unsigned rewind_buffer_size_step = 10; /* 10MB */

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

/* Publicly announce netplay */
static const bool netplay_public_announce = true;

/* Start netplay in spectator mode */
static const bool netplay_start_as_spectator = false;

/* Allow connections in slave mode */
static const bool netplay_allow_slaves = true;

/* Require connections only in slave mode */
static const bool netplay_require_slaves = false;

/* Netplay without savestates/rewind */
static const bool netplay_stateless_mode = false;

/* When being client over netplay, use keybinds for
 * user 1 rather than user 2. */
static const bool netplay_client_swap_input = true;

static const bool netplay_nat_traversal = false;

static const unsigned netplay_delay_frames = 16;

static const int netplay_check_frames = 600;

static const bool netplay_use_mitm_server = false;

static const char *netplay_mitm_server = "nyc";

#ifdef HAVE_NETWORKING
static const unsigned netplay_share_digital = RARCH_NETPLAY_SHARE_DIGITAL_NO_PREFERENCE;

static const unsigned netplay_share_analog = RARCH_NETPLAY_SHARE_ANALOG_NO_PREFERENCE;
#endif

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

/* Enable runloop for variable refresh rate screens. Force x1 speed while handling fast forward too. */
static const bool vrr_runloop_enable = false;

/* Run core logic one or more frames ahead then load the state back to reduce perceived input lag. */
static const unsigned run_ahead_frames = 1;

/* When using the Run Ahead feature, use a secondary instance of the core. */
static const bool run_ahead_secondary_instance = true;

/* Hide warning messages when using the Run Ahead feature. */
static const bool run_ahead_hide_warnings = false;

/* Enable stdin/network command interface. */
static const bool network_cmd_enable = false;
static const uint16_t network_cmd_port = 55355;
static const bool stdin_cmd_enable = false;

static const uint16_t network_remote_base_port = 55400;
/* Number of entries that will be kept in content history playlist file. */
static const unsigned default_content_history_size = 100;

/* Show Menu start-up screen on boot. */
static const bool default_menu_show_start_screen = true;

static const bool menu_dpi_override_enable = false;

#ifdef RARCH_MOBILE
static const unsigned menu_dpi_override_value = 72;
#elif defined(__CELLOS_LV2__)
static const unsigned menu_dpi_override_value = 360;
#else
static const unsigned menu_dpi_override_value = 200;
#endif

/* Log level for libretro cores (GET_LOG_INTERFACE). */
static const unsigned libretro_log_level = 1;

#ifndef RARCH_DEFAULT_PORT
#define RARCH_DEFAULT_PORT 55435
#endif

#ifndef RARCH_STREAM_DEFAULT_PORT
#define RARCH_STREAM_DEFAULT_PORT 56400
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

static const unsigned input_bind_hold = 2;

static const unsigned menu_thumbnails_default = 3;

static const unsigned menu_left_thumbnails_default = 0;

static const unsigned menu_timedate_style = 5;

static const bool xmb_vertical_thumbnails = false;

#ifdef IOS
static const bool ui_companion_start_on_boot = false;
#else
static const bool ui_companion_start_on_boot = true;
#endif

static const bool ui_companion_enable = false;

/* Currently only used to show the WIMP UI on startup */
static const bool ui_companion_toggle = false;

/* Only init the WIMP UI for this session if this is enabled */
static const bool desktop_menu_enable = true;

#if defined(__QNX__) || defined(_XBOX1) || defined(_XBOX360) || defined(__CELLOS_LV2__) || (defined(__MACH__) && defined(IOS)) || defined(ANDROID) || defined(WIIU) || defined(HAVE_NEON) || defined(GEKKO) || defined(__ARM_NEON__)
static enum resampler_quality audio_resampler_quality_level = RESAMPLER_QUALITY_LOWER;
#elif defined(PSP) || defined(_3DS) || defined(VITA)
static enum resampler_quality audio_resampler_quality_level = RESAMPLER_QUALITY_LOWEST;
#else
static enum resampler_quality audio_resampler_quality_level = RESAMPLER_QUALITY_NORMAL;
#endif

/* MIDI */
static const char *midi_input     = "Off";
static const char *midi_output    = "Off";
static const unsigned midi_volume = 100;

/* Only applies to Android 7.0 (API 24) and up */
static const bool sustained_performance_mode = false;

#if defined(HAKCHI)
static char buildbot_server_url[] = "http://hakchicloud.com/Libretro_Cores/";
#elif defined(ANDROID)
#if defined(ANDROID_ARM_V7)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/armeabi-v7a/";
#elif defined(ANDROID_ARM)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/armeabi/";
#elif defined(ANDROID_AARCH64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/arm64-v8a/";
#elif defined(ANDROID_X86)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/x86/";
#elif defined(ANDROID_X64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/android/latest/x86_64/";
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
#if _MSC_VER == 1600
#if defined(__x86_64__) || defined(_M_X64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows-msvc2010/x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__) || defined(_M_IX86) || defined(_M_IA64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows-msvc2010/x86/latest/";
#endif
#elif _MSC_VER == 1400
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows-msvc2005/x86/latest/";
#elif _MSC_VER == 1310
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows-msvc2003/x86/latest/";
#else
#if defined(__x86_64__) || defined(_M_X64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows/x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__) || defined(_M_IX86) || defined(_M_IA64)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/windows/x86/latest/";
#endif
#endif
#elif defined(__linux__)
#if defined(__x86_64__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/linux/x86_64/latest/";
#elif defined(__i386__) || defined(__i486__) || defined(__i686__)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/linux/x86/latest/";
#elif defined(__arm__) && __ARM_ARCH == 7 && defined(__ARM_PCS_VFP)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/linux/armhf/latest/";
#else
static char buildbot_server_url[] = "";
#endif
#elif defined(WIIU)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/nintendo/wiiu/latest/";
#elif defined(HAVE_LIBNX)
static char buildbot_server_url[] = "http://buildbot.libretro.com/nightly/nintendo/switch/libnx/latest/";
#elif defined(__CELLOS_LV2__) && defined(DEX_BUILD)
static char buildbot_server_url[] = "http://libretro.xbins.org/libretro/nightly/playstation/ps3/latest/dex-ps3/";
#elif defined(__CELLOS_LV2__) && defined(CEX_BUILD)
static char buildbot_server_url[] = "http://libretro.xbins.org/libretro/nightly/playstation/ps3/latest/cex-ps3/";
#elif defined(__CELLOS_LV2__) && defined(ODE_BUILD)
static char buildbot_server_url[] = "http://libretro.xbins.org/libretro/nightly/playstation/ps3/latest/ode-ps3/";
#else
static char buildbot_server_url[] = "";
#endif

static char buildbot_assets_server_url[] = "http://buildbot.libretro.com/assets/";

#endif
