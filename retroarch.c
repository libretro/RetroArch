/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined(DEBUG) && defined(HAVE_DRMINGW)
#include "exchndl.h"
#endif
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif


#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <math.h>
#include <locale.h>

#include <boolean.h>
#include <clamping.h>
#include <string/stdstring.h>
#include <dynamic/dylib.h>
#include <file/config_file.h>
#include <lists/string_list.h>
#include <retro_math.h>
#include <retro_timers.h>
#include <encodings/utf.h>

#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <libretro.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#include <features/features_cpu.h>

#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <compat/getopt.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>
#include <audio/audio_mixer.h>
#include <audio/dsp_filter.h>
#include <compat/posix_string.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <lists/dir_list.h>
#include <net/net_http.h>

#include "config.def.h"
#include "config.def.keybinds.h"

#include "runtime_file.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

#include <audio/audio_resampler.h>

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/menu_animation.h"
#include "menu/menu_input.h"
#include "menu/widgets/menu_dialog.h"
#include "menu/widgets/menu_input_dialog.h"
#include "menu/widgets/menu_input_bind_dialog.h"
#ifdef HAVE_MENU_WIDGETS
#include "menu/widgets/menu_widgets.h"
#endif
#include "menu/widgets/menu_osk.h"
#endif

#include "input/input_driver.h"
#include "input/input_mapper.h"
#include "input/input_keymaps.h"
#include "input/input_remapping.h"

#ifdef HAVE_CHEEVOS
#include "cheevos-new/cheevos.h"
#endif

#ifdef HAVE_DISCORD
#include "discord/discord.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#if defined(HAVE_OPENGL)
#include "gfx/common/gl_common.h"
#elif defined(HAVE_OPENGL_CORE)
#include "gfx/common/gl_core_common.h"
#endif

#include "autosave.h"
#include "command.h"
#include "config.features.h"
#include "cores/internal_cores.h"
#include "content.h"
#include "core_type.h"
#include "core_info.h"
#include "dynamic.h"
#include "driver.h"
#include "input/input_driver.h"
#include "msg_hash.h"
#include "paths.h"
#include "file_path_special.h"
#include "ui/ui_companion_driver.h"
#include "verbosity.h"

#include "frontend/frontend_driver.h"
#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif
#include "../gfx/video_display_server.h"
#include "../gfx/video_crt_switch.h"
#include "wifi/wifi_driver.h"
#include "led/led_driver.h"
#include "midi/midi_driver.h"
#include "core.h"
#include "configuration.h"
#include "list_special.h"
#include "managers/core_option_manager.h"
#include "managers/cheat_manager.h"
#include "managers/state_manager.h"
#ifdef HAVE_AUDIOMIXER
#include "tasks/task_audio_mixer.h"
#endif
#include "tasks/task_content.h"
#include "tasks/tasks_internal.h"
#include "performance_counters.h"

#include "version.h"
#include "version_git.h"

#include "retroarch.h"

#ifdef HAVE_RUNAHEAD
#include "runahead/copy_load_info.h"
#include "runahead/mylist.h"
#include "runahead/mem_util.h"
#endif

#include "audio/audio_thread_wrapper.h"

/* DRIVERS */

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#if !defined(__QNX__) && defined(HAVE_THREADS)
   &audio_alsathread,
#endif
#endif
#ifdef HAVE_TINYALSA
	&audio_tinyalsa,
#endif
#if defined(HAVE_AUDIOIO)
   &audio_audioio,
#endif
#if defined(HAVE_OSS) || defined(HAVE_OSS_BSD)
   &audio_oss,
#endif
#ifdef HAVE_RSOUND
   &audio_rsound,
#endif
#ifdef HAVE_COREAUDIO
   &audio_coreaudio,
#endif
#ifdef HAVE_COREAUDIO3
   &audio_coreaudio3,
#endif
#ifdef HAVE_AL
   &audio_openal,
#endif
#ifdef HAVE_SL
   &audio_opensl,
#endif
#ifdef HAVE_ROAR
   &audio_roar,
#endif
#ifdef HAVE_JACK
   &audio_jack,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &audio_sdl,
#endif
#ifdef HAVE_XAUDIO
   &audio_xa,
#endif
#ifdef HAVE_DSOUND
   &audio_dsound,
#endif
#ifdef HAVE_WASAPI
   &audio_wasapi,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#ifdef __CELLOS_LV2__
   &audio_ps3,
#endif
#ifdef XENON
   &audio_xenon360,
#endif
#ifdef GEKKO
   &audio_gx,
#endif
#ifdef WIIU
   &audio_ax,
#endif
#ifdef EMSCRIPTEN
   &audio_rwebaudio,
#endif
#if defined(PSP) || defined(VITA) || defined(ORBIS)
  &audio_psp,
#endif
#if defined(PS2)
  &audio_ps2,
#endif
#ifdef _3DS
   &audio_ctr_csnd,
   &audio_ctr_dsp,
#endif
#ifdef SWITCH
   &audio_switch_thread,
   &audio_switch,
#endif
   &audio_null,
   NULL,
};


static const video_driver_t *video_drivers[] = {
#ifdef HAVE_OPENGL
   &video_gl2,
#endif
#if defined(HAVE_OPENGL_CORE)
   &video_gl_core,
#endif
#ifdef HAVE_OPENGL1
   &video_gl1,
#endif
#ifdef HAVE_VULKAN
   &video_vulkan,
#endif
#ifdef HAVE_METAL
   &video_metal,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(HAVE_D3D12)
   &video_d3d12,
#endif
#if defined(HAVE_D3D11)
   &video_d3d11,
#endif
#if defined(HAVE_D3D10)
   &video_d3d10,
#endif
#if defined(HAVE_D3D9)
   &video_d3d9,
#endif
#if defined(HAVE_D3D8)
   &video_d3d8,
#endif
#ifdef HAVE_VITA2D
   &video_vita2d,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef PS2
   &video_ps2,
#endif
#ifdef _3DS
   &video_ctr,
#endif
#ifdef SWITCH
   &video_switch,
#endif
#ifdef HAVE_SDL
   &video_sdl,
#endif
#ifdef HAVE_SDL2
   &video_sdl2,
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef WIIU
   &video_wiiu,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
#ifdef HAVE_EXYNOS
   &video_exynos,
#endif
#ifdef HAVE_DISPMANX
   &video_dispmanx,
#endif
#ifdef HAVE_SUNXI
   &video_sunxi,
#endif
#ifdef HAVE_PLAIN_DRM
   &video_drm,
#endif
#ifdef HAVE_XSHM
   &video_xshm,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &video_gdi,
#endif
#endif
#ifdef DJGPP
   &video_vga,
#endif
#ifdef HAVE_SIXEL
   &video_sixel,
#endif
#ifdef HAVE_CACA
   &video_caca,
#endif
   &video_null,
   NULL,
};

static const gfx_ctx_driver_t *gfx_ctx_drivers[] = {
#if defined(ORBIS)
   &orbis_ctx,
#endif
#if defined(HAVE_LIBNX) && defined(HAVE_OPENGL)
   &switch_ctx,
#endif
#if defined(__CELLOS_LV2__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_VIDEOCORE)
   &gfx_ctx_videocore,
#endif
#if defined(HAVE_MALI_FBDEV)
   &gfx_ctx_mali_fbdev,
#endif
#if defined(HAVE_VIVANTE_FBDEV)
   &gfx_ctx_vivante_fbdev,
#endif
#if defined(HAVE_OPENDINGUX_FBDEV)
   &gfx_ctx_opendingux_fbdev,
#endif
#if defined(_WIN32) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE) || defined(HAVE_VULKAN))
   &gfx_ctx_wgl,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && !defined(HAVE_OPENGLES)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE) || defined(HAVE_VULKAN)
   &gfx_ctx_x,
#endif
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
   &gfx_ctx_drm,
#endif
#if defined(ANDROID)
   &gfx_ctx_android,
#endif
#if defined(__QNX__)
   &gfx_ctx_qnx,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
   &gfx_ctx_cocoagl,
#endif
#if defined(__APPLE__) && !defined(TARGET_IPHONE_SIMULATOR) && !defined(TARGET_OS_IPHONE)
   &gfx_ctx_cgl,
#endif
#if (defined(HAVE_SDL) || defined(HAVE_SDL2)) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE))
   &gfx_ctx_sdl_gl,
#endif
#ifdef HAVE_OSMESA
   &gfx_ctx_osmesa,
#endif
#ifdef EMSCRIPTEN
   &gfx_ctx_emscripten,
#endif
#if defined(HAVE_VULKAN) && defined(HAVE_VULKAN_DISPLAY)
   &gfx_ctx_khr_display,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &gfx_ctx_gdi,
#endif
#endif
#ifdef HAVE_SIXEL
   &gfx_ctx_sixel,
#endif
   &gfx_ctx_null,
   NULL
};


static const input_driver_t *input_drivers[] = {
#ifdef ORBIS
   &input_ps4,
#endif
#ifdef __CELLOS_LV2__
   &input_ps3,
#endif
#if defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
   &input_psp,
#endif
#if defined(PS2)
   &input_ps2,
#endif
#if defined(_3DS)
   &input_ctr,
#endif
#if defined(SWITCH)
   &input_switch,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &input_sdl,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef __WINRT__
   &input_uwp,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1) || defined(__WINRT__)
   &input_xinput,
#endif
#ifdef GEKKO
   &input_gx,
#endif
#ifdef WIIU
   &input_wiiu,
#endif
#ifdef ANDROID
   &input_android,
#endif
#ifdef HAVE_UDEV
   &input_udev,
#endif
#if defined(__linux__) && !defined(ANDROID)
   &input_linuxraw,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
   &input_cocoa,
#endif
#ifdef __QNX__
   &input_qnx,
#endif
#ifdef EMSCRIPTEN
   &input_rwebinput,
#endif
#ifdef DJGPP
   &input_dos,
#endif
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
   /* winraw only available since XP */
   &input_winraw,
#endif
   &input_null,
   NULL,
};

static input_device_driver_t *joypad_drivers[] = {
#ifdef __CELLOS_LV2__
   &ps3_joypad,
#endif
#ifdef HAVE_XINPUT
   &xinput_joypad,
#endif
#ifdef GEKKO
   &gx_joypad,
#endif
#ifdef WIIU
   &wiiu_joypad,
#endif
#ifdef _XBOX
   &xdk_joypad,
#endif
#if defined(ORBIS)
   &ps4_joypad,
#endif
#if defined(PSP) || defined(VITA)
   &psp_joypad,
#endif
#if defined(PS2)
   &ps2_joypad,
#endif
#ifdef _3DS
   &ctr_joypad,
#endif
#ifdef SWITCH
   &switch_joypad,
#endif
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#ifdef HAVE_UDEV
   &udev_joypad,
#endif
#if defined(__linux) && !defined(ANDROID)
   &linuxraw_joypad,
#endif
#ifdef HAVE_PARPORT
   &parport_joypad,
#endif
#ifdef ANDROID
   &android_joypad,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &sdl_joypad,
#endif
#ifdef __QNX__
   &qnx_joypad,
#endif
#ifdef HAVE_MFI
   &mfi_joypad,
#endif
#ifdef DJGPP
   &dos_joypad,
#endif
/* Selecting the HID gamepad driver disables the Wii U gamepad. So while
 * we want the HID code to be compiled & linked, we don't want the driver
 * to be selectable in the UI. */
#if defined(HAVE_HID) && !defined(WIIU)
   &hid_joypad,
#endif
#ifdef EMSCRIPTEN
   &rwebpad_joypad,
#endif
   &null_joypad,
   NULL,
};

#ifdef HAVE_HID
static hid_driver_t *hid_drivers[] = {
#if defined(HAVE_BTSTACK)
   &btstack_hid,
#endif
#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
   &iohidmanager_hid,
#endif
#if defined(HAVE_LIBUSB) && defined(HAVE_THREADS)
   &libusb_hid,
#endif
#ifdef HW_RVL
   &wiiusb_hid,
#endif
   &null_hid,
   NULL,
};
#endif

static const wifi_driver_t *wifi_drivers[] = {
#ifdef HAVE_LAKKA
   &wifi_connmanctl,
#endif
   &wifi_null,
   NULL,
};

static const location_driver_t *location_drivers[] = {
#ifdef ANDROID
   &location_android,
#endif
   &location_null,
   NULL,
};

static const ui_companion_driver_t *ui_companion_drivers[] = {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &ui_companion_win32,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
   &ui_companion_cocoa,
#endif
#ifdef HAVE_COCOATOUCH
   &ui_companion_cocoatouch,
#endif
   &ui_companion_null,
   NULL
};

static const record_driver_t *record_drivers[] = {
#ifdef HAVE_FFMPEG
   &record_ffmpeg,
#endif
   &record_null,
   NULL,
};

extern midi_driver_t midi_null;
extern midi_driver_t midi_winmm;
extern midi_driver_t midi_alsa;

static midi_driver_t *midi_drivers[] = {
#if defined(HAVE_ALSA) && !defined(HAVE_HAKCHI)
   &midi_alsa,
#endif
#ifdef HAVE_WINMM
   &midi_winmm,
#endif
   &midi_null
};

static const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#ifdef EMSCRIPTEN
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
   &camera_null,
   NULL,
};

/* MAIN GLOBAL VARIABLES */

#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, var ? "yes" : "no")

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(1, "validate_cpu_features()"); \
} while(0)

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#define SHADER_FILE_WATCH_DELAY_MSEC 500
#define HOLD_START_DELAY_SEC 2

#define QUIT_DELAY_USEC 3 * 1000000 /* 3 seconds */

#define DEBUG_INFO_FILENAME "debug_info.txt"

/* Descriptive names for options without short variant.
 *
 * Please keep the name in sync with the option name.
 * Order does not matter. */
enum
{
   RA_OPT_MENU = 256, /* must be outside the range of a char */
   RA_OPT_STATELESS,
   RA_OPT_CHECK_FRAMES,
   RA_OPT_PORT,
   RA_OPT_SPECTATE,
   RA_OPT_NICK,
   RA_OPT_COMMAND,
   RA_OPT_APPENDCONFIG,
   RA_OPT_BPS,
   RA_OPT_IPS,
   RA_OPT_NO_PATCH,
   RA_OPT_RECORDCONFIG,
   RA_OPT_SUBSYSTEM,
   RA_OPT_SIZE,
   RA_OPT_FEATURES,
   RA_OPT_VERSION,
   RA_OPT_EOF_EXIT,
   RA_OPT_LOG_FILE,
   RA_OPT_MAX_FRAMES,
   RA_OPT_MAX_FRAMES_SCREENSHOT,
   RA_OPT_MAX_FRAMES_SCREENSHOT_PATH
};

enum  runloop_state
{
   RUNLOOP_STATE_ITERATE = 0,
   RUNLOOP_STATE_POLLED_AND_SLEEP,
   RUNLOOP_STATE_MENU_ITERATE,
   RUNLOOP_STATE_END,
   RUNLOOP_STATE_QUIT
};

typedef struct runloop_ctx_msg_info
{
   const char *msg;
   unsigned prio;
   unsigned duration;
   bool flush;
} runloop_ctx_msg_info_t;

static struct global g_extern;


static struct                     retro_callbacks retro_ctx;
static struct                     retro_core_t current_core;

static jmp_buf error_sjlj_context;

static settings_t *configuration_settings                       = NULL;

static enum rarch_core_type current_core_type                   = CORE_TYPE_PLAIN;
static enum rarch_core_type explicit_current_core_type          = CORE_TYPE_PLAIN;
static char error_string[255]                                   = {0};
static char runtime_shader_preset[255]                          = {0};
static bool shader_presets_need_reload                          = true;

#ifdef HAVE_THREAD_STORAGE
static sthread_tls_t rarch_tls;
const void *MAGIC_POINTER                                       = (void*)(uintptr_t)0x0DEFACED;
#endif

static retro_bits_t has_set_libretro_device;

static bool has_set_core                                        = false;
#ifdef HAVE_DISCORD
bool discord_is_inited                                          = false;
#endif
static bool rarch_block_config_read                             = false;
static bool rarch_is_inited                                     = false;
static bool rarch_error_on_init                                 = false;
static bool rarch_force_fullscreen                              = false;
static bool rarch_is_switching_display_mode                     = false;
static bool has_set_verbosity                                   = false;
static bool has_set_libretro                                    = false;
static bool has_set_libretro_directory                          = false;
static bool has_set_save_path                                   = false;
static bool has_set_state_path                                  = false;
static bool has_set_netplay_mode                                = false;
static bool has_set_netplay_ip_address                          = false;
static bool has_set_netplay_ip_port                             = false;
static bool has_set_netplay_stateless_mode                      = false;
static bool has_set_netplay_check_frames                        = false;
static bool has_set_ups_pref                                    = false;
static bool has_set_bps_pref                                    = false;
static bool has_set_ips_pref                                    = false;
static bool has_set_log_to_file                                 = false;

static bool rarch_is_sram_load_disabled                         = false;
static bool rarch_is_sram_save_disabled                         = false;
static bool rarch_use_sram                                      = false;
static bool rarch_ups_pref                                      = false;
static bool rarch_bps_pref                                      = false;
static bool rarch_ips_pref                                      = false;

static bool runloop_force_nonblock                              = false;
static bool runloop_paused                                      = false;
static bool runloop_idle                                        = false;
static bool runloop_slowmotion                                  = false;
static bool runloop_fastmotion                                  = false;
static bool runloop_shutdown_initiated                          = false;
static bool runloop_core_shutdown_initiated                     = false;
static bool runloop_perfcnt_enable                              = false;
static bool runloop_overrides_active                            = false;
static bool runloop_remaps_core_active                          = false;
static bool runloop_remaps_game_active                          = false;
static bool runloop_remaps_content_dir_active                   = false;
static bool runloop_game_options_active                         = false;
static bool runloop_autosave                                    = false;
static rarch_system_info_t runloop_system;
static struct retro_frame_time_callback runloop_frame_time;
static retro_keyboard_event_t runloop_key_event                 = NULL;
static retro_keyboard_event_t runloop_frontend_key_event        = NULL;
static core_option_manager_t *runloop_core_options              = NULL;
static msg_queue_t *runloop_msg_queue                           = NULL;

static unsigned runloop_pending_windowed_scale                  = 0;
static unsigned runloop_max_frames                              = 0;
static bool runloop_max_frames_screenshot                       = false;
static char runloop_max_frames_screenshot_path[PATH_MAX_LENGTH] = {0};
static unsigned fastforward_after_frames                        = 0;

static retro_usec_t runloop_frame_time_last                     = 0;
static retro_time_t frame_limit_minimum_time                    = 0.0;
static retro_time_t frame_limit_last_time                       = 0.0;
static retro_time_t libretro_core_runtime_last                  = 0;
static retro_time_t libretro_core_runtime_usec                  = 0;

static char runtime_content_path[PATH_MAX_LENGTH]               = {0};
static char runtime_core_path[PATH_MAX_LENGTH]                  = {0};

static bool log_file_created                                    = false;
static char timestamped_log_file_name[64]                       = {0};

static bool log_file_override_active                            = false;
static char log_file_override_path[PATH_MAX_LENGTH]             = {0};

static char launch_arguments[4096];

static bool has_variable_update                                 = false;

#ifdef HAVE_MENU
/* MENU INPUT GLOBAL VARIABLES */
enum menu_mouse_action
{
   MENU_MOUSE_ACTION_NONE = 0,
   MENU_MOUSE_ACTION_BUTTON_L,
   MENU_MOUSE_ACTION_BUTTON_L_TOGGLE,
   MENU_MOUSE_ACTION_BUTTON_L_SET_NAVIGATION,
   MENU_MOUSE_ACTION_BUTTON_R,
   MENU_MOUSE_ACTION_WHEEL_UP,
   MENU_MOUSE_ACTION_WHEEL_DOWN,
   MENU_MOUSE_ACTION_HORIZ_WHEEL_UP,
   MENU_MOUSE_ACTION_HORIZ_WHEEL_DOWN
};

static unsigned char menu_keyboard_key_state[RETROK_LAST]       = {0};

static menu_input_t menu_input_state;
#endif

/* RECORDING GLOBAL VARIABLES */
static unsigned recording_width                                 = 0;
static unsigned recording_height                                = 0;
static size_t recording_gpu_width                               = 0;
static size_t recording_gpu_height                              = 0;
static bool recording_enable                                    = false;
static bool streaming_enable                                    = false;

static const record_driver_t *recording_driver                  = NULL;
static void *recording_data                                     = NULL;

static uint8_t *video_driver_record_gpu_buffer                  = NULL;

/* MESSAGE QUEUE GLOBAL VARIABLES */

#ifdef HAVE_THREADS
static slock_t *_runloop_msg_queue_lock                         = NULL;

#define runloop_msg_queue_lock() slock_lock(_runloop_msg_queue_lock)
#define runloop_msg_queue_unlock() slock_unlock(_runloop_msg_queue_lock)
#else
#define runloop_msg_queue_lock()
#define runloop_msg_queue_unlock()
#endif

/* BSV MOVIE GLOBAL VARIABLES */

enum rarch_movie_type
{
   RARCH_MOVIE_PLAYBACK = 0,
   RARCH_MOVIE_RECORD
};

struct bsv_state
{
   bool movie_start_recording;
   bool movie_start_playback;
   bool movie_playback;
   bool eof_exit;
   bool movie_end;

   /* Movie playback/recording support. */
   char movie_path[PATH_MAX_LENGTH];
   /* Immediate playback/recording. */
   char movie_start_path[PATH_MAX_LENGTH];
};

struct bsv_movie
{
   intfstream_t *file;

   /* A ring buffer keeping track of positions
    * in the file for each frame. */
   size_t *frame_pos;
   size_t frame_mask;
   size_t frame_ptr;

   size_t min_file_pos;

   size_t state_size;
   uint8_t *state;

   bool playback;
   bool first_rewind;
   bool did_rewind;
};

#define BSV_MAGIC          0x42535631

#define MAGIC_INDEX        0
#define SERIALIZER_INDEX   1
#define CRC_INDEX          2
#define STATE_SIZE_INDEX   3

#define BSV_MOVIE_IS_PLAYBACK_ON() (bsv_movie_state_handle && bsv_movie_state.movie_playback)
#define BSV_MOVIE_IS_PLAYBACK_OFF() (bsv_movie_state_handle && !bsv_movie_state.movie_playback)

typedef struct bsv_movie bsv_movie_t;

static bsv_movie_t     *bsv_movie_state_handle = NULL;
static struct bsv_state bsv_movie_state;


/* CAMERA GLOBAL VARIABLES */

static struct retro_camera_callback camera_cb;
static const camera_driver_t *camera_driver                     = NULL;
static void *camera_data                                        = NULL;
static bool camera_driver_active                                = false;

/* MIDI GLOBAL VARIABLES */

#define MIDI_DRIVER_BUF_SIZE 4096

static midi_driver_t *midi_drv              = &midi_null;
static void *midi_drv_data                  = NULL;
static struct string_list *midi_drv_inputs  = NULL;
static struct string_list *midi_drv_outputs = NULL;
static bool midi_drv_input_enabled          = false;
static bool midi_drv_output_enabled         = false;
static uint8_t *midi_drv_input_buffer       = NULL;
static uint8_t *midi_drv_output_buffer      = NULL;
static midi_event_t midi_drv_input_event;
static midi_event_t midi_drv_output_event;
static bool midi_drv_output_pending         = false;

static const uint8_t midi_drv_ev_sizes[128] =
{
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   0, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};


/* UI COMPANION GLOBAL VARIABLES */

static bool main_ui_companion_is_on_foreground   = false;
static const ui_companion_driver_t *ui_companion = NULL;
static void *ui_companion_data                   = NULL;

#ifdef HAVE_QT
static void *ui_companion_qt_data                = NULL;
static bool qt_is_inited                         = false;
#endif

/* LOCATION GLOBAL VARIABLES */
static const location_driver_t *location_driver  = NULL;
static void *location_data                       = NULL;

static bool location_driver_active               = false;

/* WIFI GLOBAL VARIABLES */
static const wifi_driver_t *wifi_driver          = NULL;
static void *wifi_data                           = NULL;
static bool wifi_driver_active                   = false;

/* VIDEO GLOBAL VARIABLES */

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))

#define FPS_UPDATE_INTERVAL 256

#ifdef HAVE_THREADS
#define video_driver_is_threaded_internal() ((!video_driver_is_hw_context() && video_driver_threaded) ? true : false)
#else
#define video_driver_is_threaded_internal() (false)
#endif

#ifdef HAVE_THREADS
#define video_driver_lock() \
   if (display_lock) \
      slock_lock(display_lock)

#define video_driver_unlock() \
   if (display_lock) \
      slock_unlock(display_lock)

#define video_driver_context_lock() \
   if (context_lock) \
      slock_lock(context_lock)

#define video_driver_context_unlock() \
   if (context_lock) \
      slock_unlock(context_lock)

#define video_driver_lock_free() \
   slock_free(display_lock); \
   slock_free(context_lock); \
   display_lock = NULL; \
   context_lock = NULL

#define video_driver_threaded_lock(is_threaded) \
   if (is_threaded) \
      video_driver_lock()

#define video_driver_threaded_unlock(is_threaded) \
   if (is_threaded) \
      video_driver_unlock()
#else
#define video_driver_lock()            ((void)0)
#define video_driver_unlock()          ((void)0)
#define video_driver_lock_free()       ((void)0)
#define video_driver_threaded_lock(is_threaded)   ((void)0)
#define video_driver_threaded_unlock(is_threaded) ((void)0)
#define video_driver_context_lock()    ((void)0)
#define video_driver_context_unlock()  ((void)0)
#endif

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

typedef struct
{
   enum gfx_ctx_api api;
   struct string_list *list;
} gfx_api_gpu_map;

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "4:3",           1.3333f },
   { "16:9",          1.7778f },
   { "16:10",         1.6f },
   { "16:15",         16.0f / 15.0f },
   { "21:9",          21.0f / 9.0f },
   { "1:1",           1.0f },
   { "2:1",           2.0f },
   { "3:2",           1.5f },
   { "3:4",           0.75f },
   { "4:1",           4.0f },
   { "9:16",          0.5625f },
   { "5:4",           1.25f },
   { "6:5",           1.2f },
   { "7:9",           0.7777f },
   { "8:3",           2.6666f },
   { "8:7",           1.1428f },
   { "19:12",         1.5833f },
   { "19:14",         1.3571f },
   { "30:17",         1.7647f },
   { "32:9",          3.5555f },
   { "Config",        0.0f },
   { "Square pixel",  1.0f },
   { "Core provided", 1.0f },
   { "Custom",        0.0f }
};

static gfx_api_gpu_map gpu_map[] = {
   { GFX_CTX_VULKAN_API, NULL },
   { GFX_CTX_DIRECT3D10_API, NULL },
   { GFX_CTX_DIRECT3D11_API, NULL },
   { GFX_CTX_DIRECT3D12_API, NULL }
};


/* Opaque handles to currently running window.
 * Used by e.g. input drivers which bind to a window.
 * Drivers are responsible for setting these if an input driver
 * could potentially make use of this. */
static uintptr_t video_driver_display                           = 0;
static uintptr_t video_driver_window                            = 0;

static rarch_softfilter_t *video_driver_state_filter            = NULL;
static void               *video_driver_state_buffer            = NULL;
static unsigned            video_driver_state_scale             = 0;
static unsigned            video_driver_state_out_bpp           = 0;
static bool                video_driver_state_out_rgb32         = false;
static bool                video_driver_crt_switching_active    = false;
static bool                video_driver_crt_dynamic_super_width = false;

static enum retro_pixel_format video_driver_pix_fmt      = RETRO_PIXEL_FORMAT_0RGB1555;

static const void *frame_cache_data                      = NULL;
static unsigned frame_cache_width                        = 0;
static unsigned frame_cache_height                       = 0;
static size_t frame_cache_pitch                          = 0;
static bool   video_driver_threaded                      = false;

static float video_driver_core_hz                        = 0.0f;
static float video_driver_aspect_ratio                   = 0.0f;
static unsigned video_driver_width                       = 0;
static unsigned video_driver_height                      = 0;

static enum rarch_display_type video_driver_display_type = RARCH_DISPLAY_NONE;
static char video_driver_title_buf[64]                   = {0};
static char video_driver_window_title[512]               = {0};
static bool video_driver_window_title_update             = true;

static retro_time_t video_driver_frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
static uint64_t video_driver_frame_time_count            = 0;
static uint64_t video_driver_frame_count                 = 0;

static void *video_driver_data                           = NULL;
static video_driver_t *current_video                     = NULL;

/* Interface for "poking". */
static const video_poke_interface_t *video_driver_poke   = NULL;

/* Used for 15-bit -> 16-bit conversions that take place before
 * being passed to video driver. */
static video_pixel_scaler_t *video_driver_scaler_ptr     = NULL;

static struct retro_hw_render_callback hw_render;

static const struct
retro_hw_render_context_negotiation_interface *
hw_render_context_negotiation                            = NULL;

/* Graphics driver requires RGBA byte order data (ABGR on little-endian)
 * for 32-bit.
 * This takes effect for overlay and shader cores that wants to load
 * data into graphics driver. Kinda hackish to place it here, it is only
 * used for GLES.
 * TODO: Refactor this better. */
static bool video_driver_use_rgba                        = false;
static bool video_driver_active                          = false;

static video_driver_frame_t frame_bak                    = NULL;

/* If set during context deinit, the driver should keep
 * graphics context alive to avoid having to reset all
 * context state. */
static bool video_driver_cache_context                   = false;

/* Set to true by driver if context caching succeeded. */
static bool video_driver_cache_context_ack               = false;

#ifdef HAVE_THREADS
static slock_t *display_lock                             = NULL;
static slock_t *context_lock                             = NULL;
#endif

static gfx_ctx_driver_t current_video_context;

static void *video_context_data                          = NULL;

/**
 * dynamic.c:dynamic_request_hw_context will try to set flag data when the context
 * is in the middle of being rebuilt; in these cases we will save flag
 * data and set this to true.
 * When the context is reinit, it checks this, reads from
 * deferred_flag_data and cleans it.
 *
 * TODO - Dirty hack, fix it better
 */
static bool deferred_video_context_driver_set_flags      = false;
static gfx_ctx_flags_t deferred_flag_data                = {0};

static bool video_started_fullscreen                     = false;

static char video_driver_gpu_device_string[128]          = {0};
static char video_driver_gpu_api_version_string[128]     = {0};

static struct retro_system_av_info video_driver_av_info;

/* AUDIO GLOBAL VARIABLES */
#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)

#define MENU_SOUND_FORMATS "ogg|mod|xm|s3m|mp3|flac"

/**
 * db_to_gain:
 * @db          : Decibels.
 *
 * Converts decibels to voltage gain.
 *
 * Returns: voltage gain value.
 **/
#define db_to_gain(db) (powf(10.0f, (db) / 20.0f))


#ifdef HAVE_AUDIOMIXER
static struct audio_mixer_stream 
audio_mixer_streams[AUDIO_MIXER_MAX_SYSTEM_STREAMS]      = {{0}};

static bool audio_driver_mixer_mute_enable               = false;
static bool audio_mixer_active                           = false;
static float audio_driver_mixer_volume_gain              = 0.0f;
#endif

static size_t audio_driver_chunk_size                    = 0;
static size_t audio_driver_chunk_nonblock_size           = 0;
static size_t audio_driver_chunk_block_size              = 0;

static size_t audio_driver_rewind_ptr                    = 0;
static size_t audio_driver_rewind_size                   = 0;

static int16_t *audio_driver_rewind_buf                  = NULL;
static int16_t *audio_driver_output_samples_conv_buf     = NULL;

static unsigned audio_driver_free_samples_buf[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
static uint64_t audio_driver_free_samples_count          = 0;

static size_t audio_driver_buffer_size                   = 0;
static size_t audio_driver_data_ptr                      = 0;

static bool audio_driver_control                         = false;
static bool audio_driver_mute_enable                     = false;
static bool audio_driver_use_float                       = false;
static bool audio_driver_active                          = false;

static float audio_driver_rate_control_delta             = 0.0f;
static float audio_driver_input                          = 0.0f;
static float audio_driver_volume_gain                    = 0.0f;

static float *audio_driver_input_data                    = NULL;
static float *audio_driver_output_samples_buf            = NULL;

static double audio_source_ratio_original                = 0.0f;
static double audio_source_ratio_current                 = 0.0f;

static struct retro_audio_callback audio_callback        = {0};

static retro_dsp_filter_t *audio_driver_dsp              = NULL;
static struct string_list *audio_driver_devices_list     = NULL;
static const retro_resampler_t *audio_driver_resampler   = NULL;

static void *audio_driver_resampler_data                 = NULL;
static const audio_driver_t *current_audio               = NULL;
static void *audio_driver_context_audio_data             = NULL;

static bool audio_suspended                              = false;
static bool audio_is_threaded                            = false;

/* RUNAHEAD GLOBAL VARIABLES */

typedef struct input_list_element_t
{
   unsigned port;
   unsigned device;
   unsigned index;
   int16_t *state;
   unsigned int state_size;
} input_list_element;

static size_t runahead_save_state_size          = 0;

static bool runahead_save_state_size_known      = false;
static bool request_fast_savestate              = false;
static bool hard_disable_audio                  = false;

#ifdef HAVE_RUNAHEAD
/* Save State List for Run Ahead */
static MyList *runahead_save_state_list         = NULL;
static MyList *input_state_list                 = NULL;

static bool input_is_dirty                      = false;

typedef bool(*runahead_load_state_function)(const void*, size_t);

static retro_input_state_t input_state_callback_original;
static function_t retro_reset_callback_original = NULL;
static runahead_load_state_function
retro_unserialize_callback_original             = NULL;

static function_t original_retro_deinit         = NULL;
static function_t original_retro_unload         = NULL;

static bool runahead_video_driver_is_active     = true;
static bool runahead_available                  = true;
static bool runahead_secondary_core_available   = true;
static bool runahead_force_input_dirty          = true;
static uint64_t runahead_last_frame_count       = 0;
#endif

/* INPUT REMOTE GLOBAL VARIABLES */

#define DEFAULT_NETWORK_GAMEPAD_PORT 55400
#define UDP_FRAME_PACKETS 16

struct remote_message
{
   uint16_t state;
   int port;
   int device;
   int index;
   int id;
};

struct input_remote
{
   bool state[RARCH_BIND_LIST_END];
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   int net_fd[MAX_USERS];
#endif
};

typedef struct input_remote input_remote_t;

typedef struct input_remote_state
{
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4][MAX_USERS];
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons[MAX_USERS];
} input_remote_state_t;

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
static input_remote_state_t remote_st_ptr;
#endif

/* INPUT OVERLAY GLOBAL VARIABLES */
#ifdef HAVE_OVERLAY

#define OVERLAY_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define OVERLAY_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)

#define MAX_VISIBILITY 32

static enum overlay_visibility* visibility = NULL;

typedef struct input_overlay_state
{
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4];
   uint32_t keys[RETROK_LAST / 32 + 1];
   /* This is a bitmask of (1 << key_bind_id). */
   input_bits_t buttons;
} input_overlay_state_t;

struct input_overlay
{
   enum overlay_status state;

   bool enable;
   bool blocked;
   bool alive;

   unsigned next_index;

   size_t index;
   size_t size;

   struct overlay *overlays;
   const struct overlay *active;
   void *iface_data;
   const video_overlay_interface_t *iface;

   input_overlay_state_t overlay_state;
};

static input_overlay_t *overlay_ptr = NULL;
#endif

/* INPUT GLOBAL VARIABLES */

/* Input config. */
struct input_bind_map
{
   bool valid;

   /* Meta binds get input as prefix, not input_playerN".
    * 0 = libretro related.
    * 1 = Common hotkey.
    * 2 = Uncommon/obscure hotkey.
    */
   uint8_t meta;

   const char *base;
   enum msg_hash_enums desc;
   uint8_t retro_key;
};

static const uint8_t buttons[] = {
   RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_B,
};

static pad_connection_listener_t *pad_connection_listener = NULL;

static uint16_t input_config_vid[MAX_USERS];
static uint16_t input_config_pid[MAX_USERS];

static char input_device_display_names[MAX_INPUT_DEVICES][64];
static char input_device_config_names [MAX_INPUT_DEVICES][64];
static char input_device_config_paths [MAX_INPUT_DEVICES][64];
char        input_device_names        [MAX_INPUT_DEVICES][64];

uint64_t lifecycle_state;
struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];
static const struct retro_keybind *libretro_input_binds[MAX_USERS];

#define DECLARE_BIND(x, bind, desc) { true, 0, #x, desc, bind }
#define DECLARE_META_BIND(level, x, bind, desc) { true, level, #x, desc, bind }

const struct input_bind_map input_config_bind_map[RARCH_BIND_LIST_END_NULL] = {
      DECLARE_BIND(b,         RETRO_DEVICE_ID_JOYPAD_B,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B),
      DECLARE_BIND(y,         RETRO_DEVICE_ID_JOYPAD_Y,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y),
      DECLARE_BIND(select,    RETRO_DEVICE_ID_JOYPAD_SELECT, MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT),
      DECLARE_BIND(start,     RETRO_DEVICE_ID_JOYPAD_START,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START),
      DECLARE_BIND(up,        RETRO_DEVICE_ID_JOYPAD_UP,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP),
      DECLARE_BIND(down,      RETRO_DEVICE_ID_JOYPAD_DOWN,   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN),
      DECLARE_BIND(left,      RETRO_DEVICE_ID_JOYPAD_LEFT,   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT),
      DECLARE_BIND(right,     RETRO_DEVICE_ID_JOYPAD_RIGHT,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT),
      DECLARE_BIND(a,         RETRO_DEVICE_ID_JOYPAD_A,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A),
      DECLARE_BIND(x,         RETRO_DEVICE_ID_JOYPAD_X,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X),
      DECLARE_BIND(l,         RETRO_DEVICE_ID_JOYPAD_L,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L),
      DECLARE_BIND(r,         RETRO_DEVICE_ID_JOYPAD_R,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R),
      DECLARE_BIND(l2,        RETRO_DEVICE_ID_JOYPAD_L2,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2),
      DECLARE_BIND(r2,        RETRO_DEVICE_ID_JOYPAD_R2,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2),
      DECLARE_BIND(l3,        RETRO_DEVICE_ID_JOYPAD_L3,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3),
      DECLARE_BIND(r3,        RETRO_DEVICE_ID_JOYPAD_R3,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3),
      DECLARE_BIND(l_x_plus,  RARCH_ANALOG_LEFT_X_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS),
      DECLARE_BIND(l_x_minus, RARCH_ANALOG_LEFT_X_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS),
      DECLARE_BIND(l_y_plus,  RARCH_ANALOG_LEFT_Y_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS),
      DECLARE_BIND(l_y_minus, RARCH_ANALOG_LEFT_Y_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS),
      DECLARE_BIND(r_x_plus,  RARCH_ANALOG_RIGHT_X_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS),
      DECLARE_BIND(r_x_minus, RARCH_ANALOG_RIGHT_X_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS),
      DECLARE_BIND(r_y_plus,  RARCH_ANALOG_RIGHT_Y_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS),
      DECLARE_BIND(r_y_minus, RARCH_ANALOG_RIGHT_Y_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS),

      DECLARE_BIND( gun_trigger,			RARCH_LIGHTGUN_TRIGGER,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER ),
      DECLARE_BIND( gun_offscreen_shot,	RARCH_LIGHTGUN_RELOAD,	        MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD ),
      DECLARE_BIND( gun_aux_a,			RARCH_LIGHTGUN_AUX_A,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A ),
      DECLARE_BIND( gun_aux_b,			RARCH_LIGHTGUN_AUX_B,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B ),
      DECLARE_BIND( gun_aux_c,			RARCH_LIGHTGUN_AUX_C,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C ),
      DECLARE_BIND( gun_start,			RARCH_LIGHTGUN_START,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START ),
      DECLARE_BIND( gun_select,			RARCH_LIGHTGUN_SELECT,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT ),
      DECLARE_BIND( gun_dpad_up,			RARCH_LIGHTGUN_DPAD_UP,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP ),
      DECLARE_BIND( gun_dpad_down,		RARCH_LIGHTGUN_DPAD_DOWN,		MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN ),
      DECLARE_BIND( gun_dpad_left,		RARCH_LIGHTGUN_DPAD_LEFT,		MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT ),
      DECLARE_BIND( gun_dpad_right,		RARCH_LIGHTGUN_DPAD_RIGHT,		MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT ),

      DECLARE_BIND(turbo,     RARCH_TURBO_ENABLE,            MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE),

      DECLARE_META_BIND(1, toggle_fast_forward,   RARCH_FAST_FORWARD_KEY,      MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY),
      DECLARE_META_BIND(2, hold_fast_forward,     RARCH_FAST_FORWARD_HOLD_KEY, MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY),
      DECLARE_META_BIND(1, toggle_slowmotion,     RARCH_SLOWMOTION_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY),
      DECLARE_META_BIND(2, hold_slowmotion,       RARCH_SLOWMOTION_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY),
      DECLARE_META_BIND(1, load_state,            RARCH_LOAD_STATE_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY),
      DECLARE_META_BIND(1, save_state,            RARCH_SAVE_STATE_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY),
      DECLARE_META_BIND(2, toggle_fullscreen,     RARCH_FULLSCREEN_TOGGLE_KEY, MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY),
      DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY,              MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY),
      DECLARE_META_BIND(2, state_slot_increase,   RARCH_STATE_SLOT_PLUS,       MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS),
      DECLARE_META_BIND(2, state_slot_decrease,   RARCH_STATE_SLOT_MINUS,      MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS),
      DECLARE_META_BIND(1, rewind,                RARCH_REWIND,                MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND),
      DECLARE_META_BIND(2, movie_record_toggle,   RARCH_BSV_RECORD_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE),
      DECLARE_META_BIND(2, pause_toggle,          RARCH_PAUSE_TOGGLE,          MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE),
      DECLARE_META_BIND(2, frame_advance,         RARCH_FRAMEADVANCE,          MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE),
      DECLARE_META_BIND(2, reset,                 RARCH_RESET,                 MENU_ENUM_LABEL_VALUE_INPUT_META_RESET),
      DECLARE_META_BIND(2, shader_next,           RARCH_SHADER_NEXT,           MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT),
      DECLARE_META_BIND(2, shader_prev,           RARCH_SHADER_PREV,           MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV),
      DECLARE_META_BIND(2, cheat_index_plus,      RARCH_CHEAT_INDEX_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS),
      DECLARE_META_BIND(2, cheat_index_minus,     RARCH_CHEAT_INDEX_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS),
      DECLARE_META_BIND(2, cheat_toggle,          RARCH_CHEAT_TOGGLE,          MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE),
      DECLARE_META_BIND(2, screenshot,            RARCH_SCREENSHOT,            MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT),
      DECLARE_META_BIND(2, audio_mute,            RARCH_MUTE,                  MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE),
      DECLARE_META_BIND(2, osk_toggle,            RARCH_OSK,                   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK),
      DECLARE_META_BIND(2, fps_toggle,            RARCH_FPS_TOGGLE,            MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE),
      DECLARE_META_BIND(2, send_debug_info,       RARCH_SEND_DEBUG_INFO,       MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO),
      DECLARE_META_BIND(2, netplay_host_toggle,   RARCH_NETPLAY_HOST_TOGGLE,   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE),
      DECLARE_META_BIND(2, netplay_game_watch,    RARCH_NETPLAY_GAME_WATCH,    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH),
      DECLARE_META_BIND(2, enable_hotkey,         RARCH_ENABLE_HOTKEY,         MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY),
      DECLARE_META_BIND(2, volume_up,             RARCH_VOLUME_UP,             MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP),
      DECLARE_META_BIND(2, volume_down,           RARCH_VOLUME_DOWN,           MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN),
      DECLARE_META_BIND(2, overlay_next,          RARCH_OVERLAY_NEXT,          MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT),
      DECLARE_META_BIND(2, disk_eject_toggle,     RARCH_DISK_EJECT_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE),
      DECLARE_META_BIND(2, disk_next,             RARCH_DISK_NEXT,             MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT),
      DECLARE_META_BIND(2, disk_prev,             RARCH_DISK_PREV,             MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV),
      DECLARE_META_BIND(2, grab_mouse_toggle,     RARCH_GRAB_MOUSE_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE),
      DECLARE_META_BIND(2, game_focus_toggle,     RARCH_GAME_FOCUS_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE),
      DECLARE_META_BIND(2, desktop_menu_toggle,   RARCH_UI_COMPANION_TOGGLE,   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE),
#ifdef HAVE_MENU
      DECLARE_META_BIND(1, menu_toggle,           RARCH_MENU_TOGGLE,           MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE),
#endif
      DECLARE_META_BIND(2, recording_toggle,      RARCH_RECORDING_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE),
      DECLARE_META_BIND(2, streaming_toggle,      RARCH_STREAMING_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE),
      DECLARE_META_BIND(2, ai_service,            RARCH_AI_SERVICE,            MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE),
};

typedef struct turbo_buttons turbo_buttons_t;

/* Turbo support. */
struct turbo_buttons
{
   bool frame_enable[MAX_USERS];
   uint16_t enable[MAX_USERS];
   unsigned count;
};

struct input_keyboard_line
{
   char *buffer;
   size_t ptr;
   size_t size;

   /** Line complete callback.
    * Calls back after return is
    * pressed with the completed line.
    * Line can be NULL.
    **/
   input_keyboard_line_complete_t cb;
   void *userdata;
};

static bool input_driver_keyboard_linefeed_enable = false;
static input_keyboard_line_t *g_keyboard_line     = NULL;

static void *g_keyboard_press_data                = NULL;

static unsigned osk_last_codepoint                = 0;
static unsigned osk_last_codepoint_len            = 0;

static input_keyboard_press_t g_keyboard_press_cb;

static turbo_buttons_t input_driver_turbo_btns;
#ifdef HAVE_COMMAND
static command_t *input_driver_command            = NULL;
#endif
#ifdef HAVE_NETWORKGAMEPAD
static input_remote_t *input_driver_remote        = NULL;
#endif
static input_mapper_t *input_driver_mapper        = NULL;
static const input_driver_t *current_input        = NULL;
static void *current_input_data                   = NULL;
static bool input_driver_block_hotkey             = false;
static bool input_driver_block_libretro_input     = false;
static bool input_driver_nonblock_state           = false;
static bool input_driver_flushing_input           = false;
static float input_driver_axis_threshold          = 0.0f;
static unsigned input_driver_max_users            = 0;

#ifdef HAVE_HID
static const void *hid_data                       = NULL;
#endif

#ifdef HAVE_THREADS
#define video_driver_get_ptr_internal(force) ((video_driver_is_threaded_internal() && !force) ? video_thread_get_ptr(NULL) : video_driver_data)
#else
#define video_driver_get_ptr_internal(force) (video_driver_data)
#endif

#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)

/* Forward declarations */
static bool secondary_core_create(void);
static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id);

extern retro_ctx_load_content_info_t *load_content_info;
extern enum rarch_core_type last_core_type;

/* RUNAHEAD - SECONDARY CORE GLOBAL VARIABLES */
static int port_map[16];

static dylib_t secondary_module;
static struct retro_core_t secondary_core;
static struct retro_callbacks secondary_callbacks;
static char *secondary_library_path                = NULL;
#endif
#endif

/* Forward declarations */
static bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy);
static void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance);
static void driver_location_stop(void);
static bool driver_location_start(void);
static void driver_camera_stop(void);
static bool driver_camera_start(void);
static retro_proc_address_t video_driver_get_proc_address(const char *sym);
static uintptr_t video_driver_get_current_framebuffer(void);
static bool video_driver_find_driver(void);
static bool core_uninit_libretro_callbacks(void);
static int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id);
static bool driver_update_system_av_info(
      const struct retro_system_av_info *info);

/* GLOBAL POINTER GETTERS */
#define video_driver_get_hw_context_internal() (&hw_render)

struct retro_hw_render_callback *video_driver_get_hw_context(void)
{
   return video_driver_get_hw_context_internal();
}

struct retro_system_av_info *video_viewport_get_system_av_info(void)
{
   return &video_driver_av_info;
}

settings_t *config_get_ptr(void)
{
   return configuration_settings;
}

global_t *global_get_ptr(void)
{
   return &g_extern;
}

const ui_companion_driver_t *ui_companion_get_ptr(void)
{
   return ui_companion;
}

const input_driver_t *input_get_ptr(void)
{
   return current_input;
}

/**
 * video_driver_get_ptr:
 *
 * Use this if you need the real video driver
 * and driver data pointers.
 *
 * Returns: video driver's userdata.
 **/
void *video_driver_get_ptr(bool force_nonthreaded_data)
{
   return video_driver_get_ptr_internal(force_nonthreaded_data);
}

/* CORE OPTIONS */
static bool core_option_manager_parse_variable(
      core_option_manager_t *opt, size_t idx,
      const struct retro_variable *var)
{
   const char *val_start      = NULL;
   char *value                = NULL;
   char *desc_end             = NULL;
   char *config_val           = NULL;
   struct core_option *option = (struct core_option*)&opt->opts[idx];

   /* All options are visible by default */
   option->visible = true;

   if (!string_is_empty(var->key))
      option->key             = strdup(var->key);
   if (!string_is_empty(var->value))
      value                   = strdup(var->value);

   if (!string_is_empty(value))
      desc_end                = strstr(value, "; ");

   if (!desc_end)
      goto error;

   *desc_end    = '\0';

   if (!string_is_empty(value))
      option->desc    = strdup(value);

   val_start          = desc_end + 2;
   option->vals       = string_split(val_start, "|");

   if (!option->vals)
      goto error;

   /* Legacy core option interface has no concept of
    * value labels - use actual values for display purposes */
   option->val_labels = string_list_clone(option->vals);

   if (!option->val_labels)
      goto error;

   /* Legacy core option interface always uses first
    * defined value as the default */
   option->default_index = 0;
   option->index         = 0;

   if (config_get_string(opt->conf, option->key, &config_val))
   {
      size_t i;

      for (i = 0; i < option->vals->size; i++)
      {
         if (string_is_equal(option->vals->elems[i].data, config_val))
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   free(value);

   return true;

error:
   free(value);
   return false;
}

static bool core_option_manager_parse_option(
      core_option_manager_t *opt, size_t idx,
      const struct retro_core_option_definition *option_def)
{
   size_t i;
   union string_list_elem_attr attr;
   size_t num_vals                              = 0;
   char *config_val                             = NULL;
   struct core_option *option                   = (struct core_option*)&opt->opts[idx];
   const struct retro_core_option_value *values = option_def->values;

   /* All options are visible by default */
   option->visible = true;

   if (!string_is_empty(option_def->key))
      option->key             = strdup(option_def->key);

   if (!string_is_empty(option_def->desc))
      option->desc            = strdup(option_def->desc);

   if (!string_is_empty(option_def->info))
      option->info            = strdup(option_def->info);

   /* Get number of values */
   while (true)
   {
      if (!string_is_empty(values[num_vals].value))
         num_vals++;
      else
         break;
   }

   if (num_vals < 1)
      return false;

   /* Initialise string lists */
   attr.i             = 0;
   option->vals       = string_list_new();
   option->val_labels = string_list_new();

   if (!option->vals || !option->val_labels)
      return false;

   /* Initialse default value */
   option->default_index = 0;
   option->index         = 0;

   /* Extract value/label pairs */
   for (i = 0; i < num_vals; i++)
   {
      /* We know that 'value' is valid */
      string_list_append(option->vals, values[i].value, attr);

      /* Value 'label' may be NULL */
      if (!string_is_empty(values[i].label))
         string_list_append(option->val_labels, values[i].label, attr);
      else
         string_list_append(option->val_labels, values[i].value, attr);

      /* Check whether this value is the default setting */
      if (!string_is_empty(option_def->default_value))
      {
         if (string_is_equal(option_def->default_value, values[i].value))
         {
            option->default_index = i;
            option->index         = i;
         }
      }
   }

   /* Set current config value */
   if (config_get_string(opt->conf, option->key, &config_val))
   {
      for (i = 0; i < option->vals->size; i++)
      {
         if (string_is_equal(option->vals->elems[i].data, config_val))
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   return true;
}

/**
 * core_option_manager_free:
 * @opt              : options manager handle
 *
 * Frees core option manager handle.
 **/
static void core_option_manager_free(core_option_manager_t *opt)
{
   size_t i;

   if (!opt)
      return;

   for (i = 0; i < opt->size; i++)
   {
      if (opt->opts[i].desc)
         free(opt->opts[i].desc);
      if (opt->opts[i].info)
         free(opt->opts[i].info);
      if (opt->opts[i].key)
         free(opt->opts[i].key);

      if (opt->opts[i].vals)
         string_list_free(opt->opts[i].vals);
      if (opt->opts[i].val_labels)
         string_list_free(opt->opts[i].val_labels);

      opt->opts[i].desc = NULL;
      opt->opts[i].info = NULL;
      opt->opts[i].key  = NULL;
      opt->opts[i].vals = NULL;
   }

   if (opt->conf)
      config_file_free(opt->conf);
   free(opt->opts);
   free(opt);
}

static void core_option_manager_get(core_option_manager_t *opt,
      struct retro_variable *var)
{
   size_t i;

#ifdef HAVE_RUNAHEAD
   if (opt->updated)
      has_variable_update = true;
#endif

   opt->updated = false;

   for (i = 0; i < opt->size; i++)
   {
      if (string_is_empty(opt->opts[i].key))
         continue;

      if (string_is_equal(opt->opts[i].key, var->key))
      {
         var->value = opt->opts[i].vals->elems[opt->opts[i].index].data;
         return;
      }
   }

   var->value = NULL;
}

/**
 * core_option_manager_new_vars:
 * @conf_path        : Filesystem path to write core option config file to.
 * @vars             : Pointer to variable array handle.
 *
 * Legacy version of core_option_manager_new().
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_manager_new_vars(const char *conf_path,
      const struct retro_variable *vars)
{
   const struct retro_variable *var;
   size_t size                       = 0;
   core_option_manager_t *opt        = (core_option_manager_t*)
      calloc(1, sizeof(*opt));

   if (!opt)
      return NULL;

   if (!string_is_empty(conf_path))
      opt->conf                     = config_file_new(conf_path);
   if (!opt->conf)
      opt->conf                     = config_file_new(NULL);

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   if (!opt->conf)
      goto error;

   for (var = vars; var->key && var->value; var++)
      size++;

   if (size == 0)
      goto error;

   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   for (var = vars; var->key && var->value; size++, var++)
   {
      if (!core_option_manager_parse_variable(opt, size, var))
         goto error;
   }

   return opt;

error:
   core_option_manager_free(opt);
   return NULL;
}

/**
 * core_option_manager_new:
 * @conf_path        : Filesystem path to write core option config file to.
 * @option_defs      : Pointer to variable array handle.
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_manager_new(const char *conf_path,
      const struct retro_core_option_definition *option_defs)
{
   const struct retro_core_option_definition *option_def;
   size_t size                       = 0;
   core_option_manager_t *opt        = (core_option_manager_t*)
      calloc(1, sizeof(*opt));

   if (!opt)
      return NULL;

   if (!string_is_empty(conf_path))
      opt->conf                     = config_file_new(conf_path);
   if (!opt->conf)
      opt->conf                     = config_file_new(NULL);

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   if (!opt->conf)
      goto error;

   /* Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        option_def++)
      size++;

   if (size == 0)
      goto error;

   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   /* Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        size++, option_def++)
   {
      if (!core_option_manager_parse_option(opt, size, option_def))
         goto error;
   }

   return opt;

error:
   core_option_manager_free(opt);
   return NULL;
}

/**
 * core_option_manager_flush:
 * @opt              : options manager handle
 *
 * Writes core option key-pair values to file.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
static bool core_option_manager_flush(core_option_manager_t *opt)
{
   size_t i;

   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = (struct core_option*)&opt->opts[i];

      if (option)
         config_set_string(opt->conf, option->key,
               opt->opts[i].vals->elems[opt->opts[i].index].data);
   }

   RARCH_LOG("Saved core options file to \"%s\"\n", opt->conf_path);
   return config_file_write(opt->conf, opt->conf_path, true);
}

/**
 * core_option_manager_flush_game_specific:
 * @opt              : options manager handle
 * @path             : path for the core options file
 *
 * Writes core option key-pair values to a custom file.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
static bool core_option_manager_flush_game_specific(
      core_option_manager_t *opt, const char* path)
{
   size_t i;
   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = (struct core_option*)&opt->opts[i];

      if (option)
         config_set_string(opt->conf, option->key,
               opt->opts[i].vals->elems[opt->opts[i].index].data);
   }

   return config_file_write(opt->conf, path, true);
}

/**
 * core_option_manager_get_desc:
 * @opt              : options manager handle
 * @index            : index identifier of the option
 *
 * Gets description for an option.
 *
 * Returns: Description for an option.
 **/
const char *core_option_manager_get_desc(
      core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   return opt->opts[idx].desc;
}

/**
 * core_option_manager_get_info:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets information text for an option.
 *
 * Returns: Information text for an option.
 **/
const char *core_option_manager_get_info(
      core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   return opt->opts[idx].info;
}

/**
 * core_option_manager_get_val:
 * @opt              : options manager handle
 * @index            : index identifier of the option
 *
 * Gets value for an option.
 *
 * Returns: Value for an option.
 **/
const char *core_option_manager_get_val(core_option_manager_t *opt, size_t idx)
{
   struct core_option *option = NULL;
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   option = (struct core_option*)&opt->opts[idx];
   return option->vals->elems[option->index].data;
}

/**
 * core_option_manager_get_val_label:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets value label for an option.
 *
 * Returns: Value label for an option.
 **/
const char *core_option_manager_get_val_label(core_option_manager_t *opt, size_t idx)
{
   struct core_option *option = NULL;
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   option = (struct core_option*)&opt->opts[idx];
   return option->val_labels->elems[option->index].data;
}

/**
 * core_option_manager_get_visible:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets whether option should be visible when displaying
 * core options in the frontend
 *
 * Returns: 'true' if option should be displayed by the frontend.
 **/
bool core_option_manager_get_visible(core_option_manager_t *opt,
      size_t idx)
{
   if (!opt)
      return false;
   if (idx >= opt->size)
      return false;
   return opt->opts[idx].visible;
}

void core_option_manager_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx)
{
   struct core_option *option= NULL;

   if (!opt)
      return;
   if (idx >= opt->size)
      return;

   option        = (struct core_option*)&opt->opts[idx];
   option->index = val_idx % option->vals->size;

   opt->updated  = true;
}

/**
 * core_option_manager_set_default:
 * @opt                   : pointer to core option manager object.
 * @idx                   : index of core option to be reset to defaults.
 *
 * Reset core option specified by @idx and sets default value for option.
 **/
void core_option_manager_set_default(core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return;
   if (idx >= opt->size)
      return;

   opt->opts[idx].index = opt->opts[idx].default_index;
   opt->updated         = true;
}

static struct retro_core_option_definition *core_option_manager_get_definitions(
      const struct retro_core_options_intl *core_options_intl)
{
   size_t i;
   size_t num_options                                     = 0;
   struct retro_core_option_definition *option_defs_us    = NULL;
   struct retro_core_option_definition *option_defs_local = NULL;
   struct retro_core_option_definition *option_defs       = NULL;

   if (!core_options_intl)
      return NULL;

   option_defs_us    = core_options_intl->us;
   option_defs_local = core_options_intl->local;

   if (!option_defs_us)
      return NULL;

   /* Determine number of options */
   while (true)
   {
      if (!string_is_empty(option_defs_us[num_options].key))
         num_options++;
      else
         break;
   }

   if (num_options < 1)
      return NULL;

   /* Allocate output option_defs array
    * > One extra entry required for terminating NULL entry
    * > Note that calloc() sets terminating NULL entry and
    *   correctly 'nullifies' each values array */
   option_defs = (struct retro_core_option_definition *)calloc(
         num_options + 1, sizeof(struct retro_core_option_definition));

   if (!option_defs)
      return NULL;

   /* Loop through options... */
   for (i = 0; i < num_options; i++)
   {
      size_t j;
      size_t num_values                            = 0;
      const char *key                              = option_defs_us[i].key;
      const char *local_desc                       = NULL;
      const char *local_info                       = NULL;
      struct retro_core_option_value *local_values = NULL;

      /* Key is always taken from us english defs */
      option_defs[i].key = key;

      /* Default value is always taken from us english defs */
      option_defs[i].default_value = option_defs_us[i].default_value;

      /* Try to find corresponding entry in local defs array */
      if (option_defs_local)
      {
         size_t index = 0;

         while (true)
         {
            const char *local_key = option_defs_local[index].key;

            if (!string_is_empty(local_key))
            {
               if (string_is_equal(key, local_key))
               {
                  local_desc   = option_defs_local[index].desc;
                  local_info   = option_defs_local[index].info;
                  local_values = option_defs_local[index].values;
                  break;
               }
               else
                  index++;
            }
            else
               break;
         }
      }

      /* Set desc and info strings */
      option_defs[i].desc = string_is_empty(local_desc) ? option_defs_us[i].desc : local_desc;
      option_defs[i].info = string_is_empty(local_info) ? option_defs_us[i].info : local_info;

      /* Determine number of values
       * (always taken from us english defs) */
      while (true)
      {
         if (!string_is_empty(option_defs_us[i].values[num_values].value))
            num_values++;
         else
            break;
      }

      /* Copy values */
      for (j = 0; j < num_values; j++)
      {
         const char *value       = option_defs_us[i].values[j].value;
         const char *local_label = NULL;

         /* Value string is always taken from us english defs */
         option_defs[i].values[j].value = value;

         /* Try to find corresponding entry in local defs values array */
         if (local_values)
         {
            size_t value_index = 0;

            while (true)
            {
               const char *local_value = local_values[value_index].value;

               if (!string_is_empty(local_value))
               {
                  if (string_is_equal(value, local_value))
                  {
                     local_label = local_values[value_index].label;
                     break;
                  }
                  else
                     value_index++;
               }
               else
                  break;
            }
         }

         /* Set value label string */
         option_defs[i].values[j].label = string_is_empty(local_label) ?
               option_defs_us[i].values[j].label : local_label;
      }
   }

   return option_defs;
}

static void core_option_manager_set_display(core_option_manager_t *opt,
      const char *key, bool visible)
{
   size_t i;

   if (!opt || string_is_empty(key))
      return;

   for (i = 0; i < opt->size; i++)
   {
      if (string_is_empty(opt->opts[i].key))
         continue;

      if (string_is_equal(opt->opts[i].key, key))
      {
         opt->opts[i].visible = visible;
         return;
      }
   }
}

/* DYNAMIC LIBRETRO CORE  */

#ifdef HAVE_DYNAMIC
#define SYMBOL(x) do { \
   function_t func = dylib_proc(lib_handle_local, #x); \
   memcpy(&current_core->x, &func, sizeof(func)); \
   if (current_core->x == NULL) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); retroarch_fail(1, "init_libretro_symbols()"); } \
} while (0)

static dylib_t lib_handle;
#else
#define SYMBOL(x) current_core->x = x
#endif

#define SYMBOL_DUMMY(x) current_core->x = libretro_dummy_##x

#ifdef HAVE_FFMPEG
#define SYMBOL_FFMPEG(x) current_core->x = libretro_ffmpeg_##x
#endif

#ifdef HAVE_MPV
#define SYMBOL_MPV(x) current_core->x = libretro_mpv_##x
#endif

#ifdef HAVE_IMAGEVIEWER
#define SYMBOL_IMAGEVIEWER(x) current_core->x = libretro_imageviewer_##x
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define SYMBOL_NETRETROPAD(x) current_core->x = libretro_netretropad_##x
#endif

#if defined(HAVE_VIDEOPROCESSOR)
#define SYMBOL_VIDEOPROCESSOR(x) current_core->x = libretro_videoprocessor_##x
#endif

#ifdef HAVE_EASTEREGG
#define SYMBOL_GONG(x) current_core->x = libretro_gong_##x
#endif

#define CORE_SYMBOLS(x) \
            x(retro_init); \
            x(retro_deinit); \
            x(retro_api_version); \
            x(retro_get_system_info); \
            x(retro_get_system_av_info); \
            x(retro_set_environment); \
            x(retro_set_video_refresh); \
            x(retro_set_audio_sample); \
            x(retro_set_audio_sample_batch); \
            x(retro_set_input_poll); \
            x(retro_set_input_state); \
            x(retro_set_controller_port_device); \
            x(retro_reset); \
            x(retro_run); \
            x(retro_serialize_size); \
            x(retro_serialize); \
            x(retro_unserialize); \
            x(retro_cheat_reset); \
            x(retro_cheat_set); \
            x(retro_load_game); \
            x(retro_load_game_special); \
            x(retro_unload_game); \
            x(retro_get_region); \
            x(retro_get_memory_data); \
            x(retro_get_memory_size);

static bool ignore_environment_cb   = false;
static bool core_set_shared_context = false;
static bool *load_no_content_hook   = NULL;

struct retro_subsystem_info subsystem_data[SUBSYSTEM_MAX_SUBSYSTEMS];
struct retro_subsystem_rom_info subsystem_data_roms[SUBSYSTEM_MAX_SUBSYSTEMS][SUBSYSTEM_MAX_SUBSYSTEM_ROMS];
unsigned subsystem_current_count;

const struct retro_subsystem_info *libretro_find_subsystem_info(
      const struct retro_subsystem_info *info, unsigned num_info,
      const char *ident)
{
   unsigned i;
   for (i = 0; i < num_info; i++)
   {
      if (string_is_equal(info[i].ident, ident))
         return &info[i];
      else if (string_is_equal(info[i].desc, ident))
         return &info[i];
   }

   return NULL;
}

/**
 * libretro_find_controller_description:
 * @info                         : Pointer to controller info handle.
 * @id                           : Identifier of controller to search
 *                                 for.
 *
 * Search for a controller of type @id in @info.
 *
 * Returns: controller description of found controller on success,
 * otherwise NULL.
 **/
const struct retro_controller_description *
libretro_find_controller_description(
      const struct retro_controller_info *info, unsigned id)
{
   unsigned i;

   for (i = 0; i < info->num_types; i++)
   {
      if (info->types[i].id != id)
         continue;

      return &info->types[i];
   }

   return NULL;
}

/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info)
{
   if (!info)
      return;

   free((void*)info->library_name);
   free((void*)info->library_version);
   free((void*)info->valid_extensions);
   memset(info, 0, sizeof(*info));
}

static bool environ_cb_get_system_info(unsigned cmd, void *data)
{
   rarch_system_info_t *system  = &runloop_system;
   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         *load_no_content_hook = *(const bool*)data;
         break;
      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         unsigned i, j, size;
         const struct retro_subsystem_info *info =
            (const struct retro_subsystem_info*)data;
         settings_t *settings    = configuration_settings;
         unsigned log_level      = settings->uints.libretro_log_level;

         subsystem_current_count = 0;

         RARCH_LOG("Environ SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Subsystem ID: %d\n", i);
            RARCH_LOG("Special game type: %s\n", info[i].desc);
            RARCH_LOG("  Ident: %s\n", info[i].ident);
            RARCH_LOG("  ID: %u\n", info[i].id);
            RARCH_LOG("  Content:\n");
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         if (log_level == RETRO_LOG_DEBUG)
            RARCH_LOG("Subsystems: %d\n", i);
         size = i;

         if (log_level == RETRO_LOG_DEBUG)
            if (size > SUBSYSTEM_MAX_SUBSYSTEMS)
               RARCH_WARN("Subsystems exceed subsystem max, clamping to %d\n", SUBSYSTEM_MAX_SUBSYSTEMS);

         if (system)
         {
            for (i = 0; i < size && i < SUBSYSTEM_MAX_SUBSYSTEMS; i++)
            {
               /* Nasty, but have to do it like this since
                * the pointers are const char *
                * (if we don't free them, we get a memory leak) */
               if (!string_is_empty(subsystem_data[i].desc))
                  free((char *)subsystem_data[i].desc);
               if (!string_is_empty(subsystem_data[i].ident))
                  free((char *)subsystem_data[i].ident);
               subsystem_data[i].desc = strdup(info[i].desc);
               subsystem_data[i].ident = strdup(info[i].ident);
               subsystem_data[i].id = info[i].id;
               subsystem_data[i].num_roms = info[i].num_roms;

               if (log_level == RETRO_LOG_DEBUG)
                  if (subsystem_data[i].num_roms > SUBSYSTEM_MAX_SUBSYSTEM_ROMS)
                     RARCH_WARN("Subsystems exceed subsystem max roms, clamping to %d\n", SUBSYSTEM_MAX_SUBSYSTEM_ROMS);

               for (j = 0; j < subsystem_data[i].num_roms && j < SUBSYSTEM_MAX_SUBSYSTEM_ROMS; j++)
               {
                  /* Nasty, but have to do it like this since
                   * the pointers are const char *
                   * (if we don't free them, we get a memory leak) */
                  if (!string_is_empty(subsystem_data_roms[i][j].desc))
                     free((char *)subsystem_data_roms[i][j].desc);
                  if (!string_is_empty(subsystem_data_roms[i][j].valid_extensions))
                     free((char *)subsystem_data_roms[i][j].valid_extensions);
                  subsystem_data_roms[i][j].desc = strdup(info[i].roms[j].desc);
                  subsystem_data_roms[i][j].valid_extensions = strdup(info[i].roms[j].valid_extensions);
                  subsystem_data_roms[i][j].required = info[i].roms[j].required;
                  subsystem_data_roms[i][j].block_extract = info[i].roms[j].block_extract;
                  subsystem_data_roms[i][j].need_fullpath = info[i].roms[j].need_fullpath;
               }
               subsystem_data[i].roms = subsystem_data_roms[i];
            }

            subsystem_current_count = 
               size <= SUBSYSTEM_MAX_SUBSYSTEMS 
               ? size 
               : SUBSYSTEM_MAX_SUBSYSTEMS;
         }
         break;
      }
      default:
         return false;
   }

   return true;
}

static bool dynamic_request_hw_context(enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_NONE:
         RARCH_LOG("Requesting no HW context.\n");
         break;

      case RETRO_HW_CONTEXT_VULKAN:
#ifdef HAVE_VULKAN
         RARCH_LOG("Requesting Vulkan context.\n");
         break;
#else
         RARCH_ERR("Requesting Vulkan context, but RetroArch is not compiled against Vulkan. Cannot use HW context.\n");
         return false;
#endif

#if defined(HAVE_OPENGLES)

#if (defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3))
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_LOG("Requesting OpenGLES%u context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         break;

#if defined(HAVE_OPENGLES3)
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         RARCH_LOG("Requesting OpenGLES%u.%u context.\n",
               major, minor);
         break;
#endif

#endif
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         RARCH_ERR("Requesting OpenGL context, but RetroArch "
               "is compiled against OpenGLES. Cannot use HW context.\n");
         return false;

#elif defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_ERR("Requesting OpenGLES%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         return false;

      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         RARCH_ERR("Requesting OpenGLES%u.%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               major, minor);
         return false;

      case RETRO_HW_CONTEXT_OPENGL:
         RARCH_LOG("Requesting OpenGL context.\n");
         break;

      case RETRO_HW_CONTEXT_OPENGL_CORE:
         /* TODO/FIXME - we should do a check here to see if
          * the requested core GL version is supported */
         RARCH_LOG("Requesting core OpenGL context (%u.%u).\n",
               major, minor);
         break;
#endif

#if defined(HAVE_D3D9) || defined(HAVE_D3D11)
      case RETRO_HW_CONTEXT_DIRECT3D:
         switch (major)
         {
#ifdef HAVE_D3D9
            case 9:
               RARCH_LOG("Requesting D3D9 context.\n");
               break;
#endif
#ifdef HAVE_D3D11
            case 11:
               RARCH_LOG("Requesting D3D11 context.\n");
               break;
#endif
            default:
               RARCH_LOG("Requesting unknown context.\n");
               return false;
         }
         break;
#endif

      default:
         RARCH_LOG("Requesting unknown context.\n");
         return false;
   }

   return true;
}

static bool dynamic_verify_hw_context(enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   const char *video_ident = (current_video) ? current_video->ident : NULL;

   switch (type)
   {
      case RETRO_HW_CONTEXT_VULKAN:
         if (!string_is_equal(video_ident, "vulkan"))
            return false;
         break;
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         if (!string_is_equal(video_ident, "gl") &&
             !string_is_equal(video_ident, "glcore"))
         {
            return false;
         }
         break;
		case RETRO_HW_CONTEXT_DIRECT3D:
			if (!(string_is_equal(video_ident, "d3d11") && major == 11))
				return false;
		break;
      default:
         break;
   }

   return true;
}

static void rarch_log_libretro(enum retro_log_level level,
      const char *fmt, ...)
{
   va_list vp;
   settings_t *settings = configuration_settings;

   if ((unsigned)level < settings->uints.libretro_log_level)
      return;

   if (!verbosity_is_enabled())
      return;

   va_start(vp, fmt);

   switch (level)
   {
      case RETRO_LOG_DEBUG:
         RARCH_LOG_V("[libretro DEBUG]", fmt, vp);
         break;

      case RETRO_LOG_INFO:
         RARCH_LOG_OUTPUT_V("[libretro INFO]", fmt, vp);
         break;

      case RETRO_LOG_WARN:
         RARCH_WARN_V("[libretro WARN]", fmt, vp);
         break;

      case RETRO_LOG_ERROR:
         RARCH_ERR_V("[libretro ERROR]", fmt, vp);
         break;

      default:
         break;
   }

   va_end(vp);
}

static void core_performance_counter_start(struct retro_perf_counter *perf)
{
   if (runloop_perfcnt_enable)
   {
      perf->call_cnt++;
      perf->start      = cpu_features_get_perf_counter();
   }
}

static void core_performance_counter_stop(struct retro_perf_counter *perf)
{
   if (runloop_perfcnt_enable)
      perf->total += cpu_features_get_perf_counter() - perf->start;
}

static size_t mmap_add_bits_down(size_t n)
{
   n |= n >>  1;
   n |= n >>  2;
   n |= n >>  4;
   n |= n >>  8;
   n |= n >> 16;

   /* double shift to avoid warnings on 32bit (it's dead code, but compilers suck) */
   if (sizeof(size_t) > 4)
      n |= n >> 16 >> 16;

   return n;
}

static size_t mmap_inflate(size_t addr, size_t mask)
{
    while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;

      /* to put in an 1 bit instead, OR in tmp+1 */
      addr       = ((addr & ~tmp) << 1) | (addr & tmp);
      mask       = mask & (mask - 1);
   }

   return addr;
}

static size_t mmap_reduce(size_t addr, size_t mask)
{
   while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;
      addr       = (addr & tmp) | ((addr >> 1) & ~tmp);
      mask       = (mask & (mask - 1)) >> 1;
   }

   return addr;
}

static size_t mmap_highest_bit(size_t n)
{
   n = mmap_add_bits_down(n);
   return n ^ (n >> 1);
}


static bool mmap_preprocess_descriptors(rarch_memory_descriptor_t *first, unsigned count)
{
   size_t                      top_addr = 1;
   rarch_memory_descriptor_t *desc      = NULL;
   const rarch_memory_descriptor_t *end = first + count;

   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select != 0)
         top_addr |= desc->core.select;
      else
         top_addr |= desc->core.start + desc->core.len - 1;
   }

   top_addr = mmap_add_bits_down(top_addr);

   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select == 0)
      {
         if (desc->core.len == 0)
            return false;

         if ((desc->core.len & (desc->core.len - 1)) != 0)
            return false;

         desc->core.select = top_addr & ~mmap_inflate(mmap_add_bits_down(desc->core.len - 1),
               desc->core.disconnect);
      }

      if (desc->core.len == 0)
         desc->core.len = mmap_add_bits_down(mmap_reduce(top_addr & ~desc->core.select,
                  desc->core.disconnect)) + 1;

      if (desc->core.start & ~desc->core.select)
         return false;

      while (mmap_reduce(top_addr & ~desc->core.select, desc->core.disconnect) >> 1 > desc->core.len - 1)
         desc->core.disconnect |= mmap_highest_bit(top_addr & ~desc->core.select & ~desc->core.disconnect);

      desc->disconnect_mask = mmap_add_bits_down(desc->core.len - 1);
      desc->core.disconnect &= desc->disconnect_mask;

      while ((~desc->disconnect_mask) >> 1 & desc->core.disconnect)
      {
         desc->disconnect_mask >>= 1;
         desc->core.disconnect &= desc->disconnect_mask;
      }
   }

   return true;
}

static bool rarch_clear_all_thread_waits(unsigned clear_threads, void *data)
{
   if ( clear_threads > 0)
      audio_driver_start(false) ;
   else
      audio_driver_stop() ;

   return true;
}



/**
 * rarch_environment_cb:
 * @cmd                          : Identifier of command.
 * @data                         : Pointer to data.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if environment callback command could
 * be performed, otherwise false (0).
 **/
bool rarch_environment_cb(unsigned cmd, void *data)
{
   unsigned p;
   settings_t         *settings = configuration_settings;
   rarch_system_info_t *system  = &runloop_system;

   if (ignore_environment_cb)
      return false;

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_OVERSCAN:
         *(bool*)data = !settings->bools.video_crop_overscan;
         RARCH_LOG("Environ GET_OVERSCAN: %u\n",
               (unsigned)!settings->bools.video_crop_overscan);
         break;

      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool*)data = true;
         RARCH_LOG("Environ GET_CAN_DUPE: true\n");
         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE:
         {
            unsigned log_level         = settings->uints.libretro_log_level;
            struct retro_variable *var = (struct retro_variable*)data;

            if (!runloop_core_options || !var)
            {
               if (var)
               {
                  RARCH_LOG("Environ GET_VARIABLE %s: not implemented.\n",
                        var->key);
                  var->value = NULL;
               }
               return true;
            }
            core_option_manager_get(runloop_core_options, var);

            if (log_level == RETRO_LOG_DEBUG)
            {
               RARCH_LOG("Environ GET_VARIABLE %s:\n", var->key);
               RARCH_LOG("\t%s\n", var->value ? var->value :
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
            }
         }

         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
         if (runloop_core_options)
            *(bool*)data = runloop_core_options->updated;
         else
            *(bool*)data = false;
         break;

      /* SET_VARIABLES: Legacy path */
      case RETRO_ENVIRONMENT_SET_VARIABLES:
         RARCH_LOG("Environ SET_VARIABLES.\n");

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_DEINIT, NULL);
         rarch_ctl(RARCH_CTL_CORE_VARIABLES_INIT, data);

         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
         RARCH_LOG("Environ SET_CORE_OPTIONS.\n");

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_DEINIT, NULL);
         rarch_ctl(RARCH_CTL_CORE_OPTIONS_INIT,   data);

         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
         RARCH_LOG("Environ RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL.\n");

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_DEINIT,    NULL);
         rarch_ctl(RARCH_CTL_CORE_OPTIONS_INTL_INIT, data);

         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
         RARCH_LOG("Environ RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY.\n");

         rarch_ctl(RARCH_CTL_CORE_OPTIONS_DISPLAY, data);

         break;

      case RETRO_ENVIRONMENT_SET_MESSAGE:
      {
         const struct retro_message *msg = (const struct retro_message*)data;
         RARCH_LOG("Environ SET_MESSAGE: %s\n", msg->msg);
#ifdef HAVE_MENU_WIDGETS
         if (!menu_widgets_set_libretro_message(msg->msg, roundf((float)msg->frames / 60.0f * 1000.0f)))
#endif
            runloop_msg_queue_push(msg->msg, 3, msg->frames, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         break;
      }

      case RETRO_ENVIRONMENT_SET_ROTATION:
      {
         unsigned rotation = *(const unsigned*)data;
         RARCH_LOG("Environ SET_ROTATION: %u\n", rotation);
         if (!settings->bools.video_allow_rotate)
            break;

         if (system)
            system->rotation = rotation;

         if (!video_driver_set_rotation(rotation))
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_SHUTDOWN:
         RARCH_LOG("Environ SHUTDOWN.\n");

         /* This case occurs when a core (internally) requests
          * a shutdown event. Must save runtime log file here,
          * since normal command.c CMD_EVENT_CORE_DEINIT event
          * will not occur until after the current content has
          * been cleared (causing log to be skipped) */
         rarch_ctl(RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT, NULL);

         rarch_ctl(RARCH_CTL_SET_SHUTDOWN,      NULL);
         rarch_ctl(RARCH_CTL_SET_CORE_SHUTDOWN, NULL);
         break;

      case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
         if (system)
         {
            system->performance_level = *(const unsigned*)data;
            RARCH_LOG("Environ PERFORMANCE_LEVEL: %u.\n",
                  system->performance_level);
         }
         break;

      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         if (string_is_empty(settings->paths.directory_system) || settings->bools.systemfiles_in_content_dir)
         {
            const char *fullpath = path_get(RARCH_PATH_CONTENT);
            if (!string_is_empty(fullpath))
            {
               size_t path_size = PATH_MAX_LENGTH * sizeof(char);
               char *temp_path  = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

               temp_path[0] = '\0';

               if (string_is_empty(settings->paths.directory_system))
                  RARCH_WARN("SYSTEM DIR is empty, assume CONTENT DIR %s\n",
                        fullpath);
               fill_pathname_basedir(temp_path, fullpath, path_size);
               dir_set(RARCH_DIR_SYSTEM, temp_path);
               free(temp_path);
            }

            *(const char**)data = dir_get_ptr(RARCH_DIR_SYSTEM);
            RARCH_LOG("Environ SYSTEM_DIRECTORY: \"%s\".\n",
                  dir_get(RARCH_DIR_SYSTEM));
         }
         else
         {
            *(const char**)data = settings->paths.directory_system;
            RARCH_LOG("Environ SYSTEM_DIRECTORY: \"%s\".\n",
               settings->paths.directory_system);
         }

         break;

      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char**)data = dir_get(RARCH_DIR_CURRENT_SAVEFILE);
         break;

      case RETRO_ENVIRONMENT_GET_USERNAME:
         *(const char**)data = *settings->paths.username ?
            settings->paths.username : NULL;
         RARCH_LOG("Environ GET_USERNAME: \"%s\".\n",
               settings->paths.username);
         break;

      case RETRO_ENVIRONMENT_GET_LANGUAGE:
#ifdef HAVE_LANGEXTRA
         {
            unsigned user_lang = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
            *(unsigned *)data  = user_lang;
            RARCH_LOG("Environ GET_LANGUAGE: \"%u\".\n", user_lang);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      {
         enum retro_pixel_format pix_fmt =
            *(const enum retro_pixel_format*)data;

         switch (pix_fmt)
         {
            case RETRO_PIXEL_FORMAT_0RGB1555:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: 0RGB1555.\n");
               break;

            case RETRO_PIXEL_FORMAT_RGB565:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: RGB565.\n");
               break;
            case RETRO_PIXEL_FORMAT_XRGB8888:
               RARCH_LOG("Environ SET_PIXEL_FORMAT: XRGB8888.\n");
               break;
            default:
               return false;
         }

         video_driver_set_pixel_format(pix_fmt);
         break;
      }

      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      {
         static const char *libretro_btn_desc[]    = {
            "B (bottom)", "Y (left)", "Select", "Start",
            "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
            "A (right)", "X (up)",
            "L", "R", "L2", "R2", "L3", "R3",
         };

         if (system)
         {
            unsigned retro_id;
            const struct retro_input_descriptor *desc = NULL;
            memset((void*)&system->input_desc_btn, 0,
                  sizeof(system->input_desc_btn));

            desc = (const struct retro_input_descriptor*)data;

            for (; desc->description; desc++)
            {
               unsigned retro_port = desc->port;

               retro_id            = desc->id;

               if (desc->port >= MAX_USERS)
                  continue;

               /* Ignore all others for now. */
               if (
                     desc->device != RETRO_DEVICE_JOYPAD  &&
                     desc->device != RETRO_DEVICE_ANALOG)
                  continue;

               if (desc->id >= RARCH_FIRST_CUSTOM_BIND)
                  continue;

               if (desc->device == RETRO_DEVICE_ANALOG)
               {
                  switch (retro_id)
                  {
                     case RETRO_DEVICE_ID_ANALOG_X:
                        switch (desc->index)
                        {
                           case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_X_PLUS]  = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_X_MINUS] = desc->description;
                              break;
                           case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_X_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_X_MINUS] = desc->description;
                              break;
                        }
                        break;
                     case RETRO_DEVICE_ID_ANALOG_Y:
                        switch (desc->index)
                        {
                           case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_Y_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_Y_MINUS] = desc->description;
                              break;
                           case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_Y_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_Y_MINUS] = desc->description;
                              break;
                        }
                        break;
                  }
               }
               else
                  system->input_desc_btn[retro_port]
                     [retro_id] = desc->description;
            }

            RARCH_LOG("Environ SET_INPUT_DESCRIPTORS:\n");

            {
               unsigned log_level      = settings->uints.libretro_log_level;
               
               if (log_level == RETRO_LOG_DEBUG)
               {
                  for (p = 0; p < input_driver_max_users; p++)
                  {
                     for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
                     {
                        const char *description = system->input_desc_btn[p][retro_id];

                        if (!description)
                           continue;

                        RARCH_LOG("\tRetroPad, User %u, Button \"%s\" => \"%s\"\n",
                              p + 1, libretro_btn_desc[retro_id], description);
                     }
                  }
               }
            }

            current_core.has_set_input_descriptors = true;
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
      {
         retro_keyboard_event_t *frontend_key_event = NULL;
         retro_keyboard_event_t *key_event          = NULL;
         const struct retro_keyboard_callback *info =
            (const struct retro_keyboard_callback*)data;

         rarch_ctl(RARCH_CTL_FRONTEND_KEY_EVENT_GET, &frontend_key_event);
         rarch_ctl(RARCH_CTL_KEY_EVENT_GET, &key_event);

         RARCH_LOG("Environ SET_KEYBOARD_CALLBACK.\n");
         if (key_event)
            *key_event                  = info->callback;

         if (frontend_key_event && key_event)
            *frontend_key_event         = *key_event;
         break;
      }

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
         RARCH_LOG("Environ SET_DISK_CONTROL_INTERFACE.\n");
         if (system)
            system->disk_control_cb =
               *(const struct retro_disk_control_callback*)data;
         break;

      case RETRO_ENVIRONMENT_SET_HW_RENDER:
      case RETRO_ENVIRONMENT_SET_HW_RENDER | RETRO_ENVIRONMENT_EXPERIMENTAL:
      {
         struct retro_hw_render_callback *cb =
            (struct retro_hw_render_callback*)data;
         struct retro_hw_render_callback *hwr =
            video_driver_get_hw_context_internal();

         RARCH_LOG("Environ SET_HW_RENDER.\n");

         if (!dynamic_request_hw_context(
                  cb->context_type, cb->version_minor, cb->version_major))
            return false;

         if (!dynamic_verify_hw_context(
                  cb->context_type, cb->version_minor, cb->version_major))
            return false;

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
         if (!gl_set_core_context(cb->context_type)) { }
#endif

         cb->get_current_framebuffer = video_driver_get_current_framebuffer;
         cb->get_proc_address        = video_driver_get_proc_address;

         /* Old ABI. Don't copy garbage. */
         if (cmd & RETRO_ENVIRONMENT_EXPERIMENTAL)
         {
            memcpy(hwr,
                  cb, offsetof(struct retro_hw_render_callback, stencil));
            memset((uint8_t*)hwr + offsetof(struct retro_hw_render_callback, stencil),
               0, sizeof(*cb) - offsetof(struct retro_hw_render_callback, stencil));
         }
         else
            memcpy(hwr, cb, sizeof(*cb));
         break;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      {
         bool state = *(const bool*)data;
         RARCH_LOG("Environ SET_SUPPORT_NO_GAME: %s.\n", state ? "yes" : "no");

         if (state)
            content_set_does_not_need_content();
         else
            content_unset_does_not_need_content();
         break;
      }

      case RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND:
      {
         bool state = *(const bool*)data;
         RARCH_LOG("Environ SET_SAVE_STATE_IN_BACKGROUND: %s.\n", state ? "yes" : "no");

         set_save_state_in_background(state) ;

         break;
      }

      case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
      {
         const char **path = (const char**)data;
#ifdef HAVE_DYNAMIC
         *path = path_get(RARCH_PATH_CORE);
#else
         *path = NULL;
#endif
         break;
      }

      case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
#ifdef HAVE_THREADS
      {
         const struct retro_audio_callback *cb = (const struct retro_audio_callback*)data;
         RARCH_LOG("Environ SET_AUDIO_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         if (recording_data) /* A/V sync is a must. */
            return false;
         if (cb)
            audio_callback = *cb;
      }
#endif
      break;

      case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
      {
         const struct retro_frame_time_callback *info =
            (const struct retro_frame_time_callback*)data;

         RARCH_LOG("Environ SET_FRAME_TIME_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         /* retro_run() will be called in very strange and
          * mysterious ways, have to disable it. */
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         runloop_frame_time = *info;
         break;
      }

      case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
      {
         struct retro_rumble_interface *iface =
            (struct retro_rumble_interface*)data;

         RARCH_LOG("Environ GET_RUMBLE_INTERFACE.\n");
         iface->set_rumble_state = input_driver_set_rumble_state;
         break;
      }

      case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
      {
         uint64_t *mask = (uint64_t*)data;

         RARCH_LOG("Environ GET_INPUT_DEVICE_CAPABILITIES.\n");
         if (!current_input->get_capabilities || !current_input_data)
            return false;
         *mask = input_driver_get_capabilities();
         break;
      }

      case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
      {
         struct retro_sensor_interface *iface =
            (struct retro_sensor_interface*)data;

         RARCH_LOG("Environ GET_SENSOR_INTERFACE.\n");
         iface->set_sensor_state = input_sensor_set_state;
         iface->get_sensor_input = input_sensor_get_input;
         break;
      }
      case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
      {
         struct retro_camera_callback *cb =
            (struct retro_camera_callback*)data;

         RARCH_LOG("Environ GET_CAMERA_INTERFACE.\n");
         cb->start                        = driver_camera_start;
         cb->stop                         = driver_camera_stop;

         camera_cb                        = *cb;

         if (cb->caps != 0)
            camera_driver_active = true;
         else
            camera_driver_active = false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
      {
         struct retro_location_callback *cb =
            (struct retro_location_callback*)data;

         RARCH_LOG("Environ GET_LOCATION_INTERFACE.\n");
         cb->start                 = driver_location_start;
         cb->stop                  = driver_location_stop;
         cb->get_position          = driver_location_get_position;
         cb->set_interval          = driver_location_set_interval;

         if (system)
            system->location_cb    = *cb;

         location_driver_active    = false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      {
         struct retro_log_callback *cb = (struct retro_log_callback*)data;

         RARCH_LOG("Environ GET_LOG_INTERFACE.\n");
         cb->log = rarch_log_libretro;
         break;
      }

      case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
      {
         struct retro_perf_callback *cb = (struct retro_perf_callback*)data;

         RARCH_LOG("Environ GET_PERF_INTERFACE.\n");
         cb->get_time_usec    = cpu_features_get_time_usec;
         cb->get_cpu_features = cpu_features_get;
         cb->get_perf_counter = cpu_features_get_perf_counter;

         cb->perf_register    = performance_counter_register;
         cb->perf_start       = core_performance_counter_start;
         cb->perf_stop        = core_performance_counter_stop;
         cb->perf_log         = retro_perf_log;
         break;
      }

      case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
      {
         const char **dir = (const char**)data;

         *dir = *settings->paths.directory_core_assets ?
            settings->paths.directory_core_assets : NULL;
         RARCH_LOG("Environ CORE_ASSETS_DIRECTORY: \"%s\".\n",
               settings->paths.directory_core_assets);
         break;
      }

      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      /**
       * Update the system Audio/Video information.
       * Will reinitialize audio/video drivers.
       * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO.
       **/
      {
         const struct retro_system_av_info **info = (const struct retro_system_av_info**)&data;
         if (info)
         {
            RARCH_LOG("Environ SET_SYSTEM_AV_INFO.\n");
            return driver_update_system_av_info(*info);
         }
         return false;
      }

      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         unsigned i;
         const struct retro_subsystem_info *info =
            (const struct retro_subsystem_info*)data;
         unsigned log_level   = settings->uints.libretro_log_level;

         if (log_level == RETRO_LOG_DEBUG)
            RARCH_LOG("Environ SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            unsigned j;
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Special game type: %s\n", info[i].desc);
            RARCH_LOG("  Ident: %s\n", info[i].ident);
            RARCH_LOG("  ID: %u\n", info[i].id);
            RARCH_LOG("  Content:\n");
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         if (system)
         {
            struct retro_subsystem_info *info_ptr = NULL;
            free(system->subsystem.data);
            system->subsystem.data = NULL;
            system->subsystem.size = 0;

            info_ptr = (struct retro_subsystem_info*)
               malloc(i * sizeof(*info_ptr));

            if (!info_ptr)
               return false;

            system->subsystem.data = info_ptr;

            memcpy(system->subsystem.data, info,
                  i * sizeof(*system->subsystem.data));
            system->subsystem.size = i;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      {
         unsigned i, j;
         const struct retro_controller_info *info =
            (const struct retro_controller_info*)data;
         unsigned log_level      = settings->uints.libretro_log_level;

         RARCH_LOG("Environ SET_CONTROLLER_INFO.\n");

         for (i = 0; info[i].types; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Controller port: %u\n", i + 1);
            for (j = 0; j < info[i].num_types; j++)
               RARCH_LOG("   %s (ID: %u)\n", info[i].types[j].desc,
                     info[i].types[j].id);
         }

         if (system)
         {
            struct retro_controller_info *info_ptr = NULL;

            free(system->ports.data);
            system->ports.data = NULL;
            system->ports.size = 0;

            info_ptr = (struct retro_controller_info*)calloc(i, sizeof(*info_ptr));
            if (!info_ptr)
               return false;

            system->ports.data = info_ptr;
            memcpy(system->ports.data, info,
                  i * sizeof(*system->ports.data));
            system->ports.size = i;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
      {
         if (system)
         {
            unsigned i;
            const struct retro_memory_map *mmaps        =
               (const struct retro_memory_map*)data;
            rarch_memory_descriptor_t *descriptors = NULL;

            RARCH_LOG("Environ SET_MEMORY_MAPS.\n");
            free((void*)system->mmaps.descriptors);
            system->mmaps.descriptors     = 0;
            system->mmaps.num_descriptors = 0;
            descriptors = (rarch_memory_descriptor_t*)
               calloc(mmaps->num_descriptors,
                     sizeof(*descriptors));

            if (!descriptors)
               return false;

            system->mmaps.descriptors     = descriptors;
            system->mmaps.num_descriptors = mmaps->num_descriptors;

            for (i = 0; i < mmaps->num_descriptors; i++)
               system->mmaps.descriptors[i].core = mmaps->descriptors[i];

            mmap_preprocess_descriptors(descriptors, mmaps->num_descriptors);

            if (sizeof(void *) == 8)
               RARCH_LOG("   ndx flags  ptr              offset   start    select   disconn  len      addrspace\n");
            else
               RARCH_LOG("   ndx flags  ptr          offset   start    select   disconn  len      addrspace\n");

            for (i = 0; i < system->mmaps.num_descriptors; i++)
            {
               const rarch_memory_descriptor_t *desc =
                  &system->mmaps.descriptors[i];
               char flags[7];

               flags[0] = 'M';
               if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
                  flags[1] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
                  flags[1] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
                  flags[1] = '2';
               else
                  flags[1] = '1';

               flags[2] = 'A';
               if ((desc->core.flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
                  flags[3] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
                  flags[3] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
                  flags[3] = '2';
               else
                  flags[3] = '1';

               flags[4] = (desc->core.flags & RETRO_MEMDESC_BIGENDIAN) ? 'B' : 'b';
               flags[5] = (desc->core.flags & RETRO_MEMDESC_CONST) ? 'C' : 'c';
               flags[6] = 0;

               RARCH_LOG("   %03u %s %p %08X %08X %08X %08X %08X %s\n",
                     i + 1, flags, desc->core.ptr, desc->core.offset, desc->core.start,
                     desc->core.select, desc->core.disconnect, desc->core.len,
                     desc->core.addrspace ? desc->core.addrspace : "");
            }
         }
         else
         {
            RARCH_WARN("Environ SET_MEMORY_MAPS, but system pointer not initialized..\n");
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_GEOMETRY:
      {
         const struct retro_game_geometry *in_geom = NULL;
         struct retro_system_av_info *av_info      = &video_driver_av_info;
         struct retro_game_geometry  *geom         = (struct retro_game_geometry*)&av_info->geometry;

         if (!geom)
            return false;

         in_geom = (const struct retro_game_geometry*)data;

         RARCH_LOG("Environ SET_GEOMETRY.\n");

         /* Can potentially be called every frame,
          * don't do anything unless required. */
         if (  (geom->base_width   != in_geom->base_width)  ||
               (geom->base_height  != in_geom->base_height) ||
               (geom->aspect_ratio != in_geom->aspect_ratio))
         {
            geom->base_width   = in_geom->base_width;
            geom->base_height  = in_geom->base_height;
            geom->aspect_ratio = in_geom->aspect_ratio;

            RARCH_LOG("SET_GEOMETRY: %ux%u, aspect: %.3f.\n",
                  geom->base_width, geom->base_height, geom->aspect_ratio);

            /* Forces recomputation of aspect ratios if
             * using core-dependent aspect ratios. */
            video_driver_set_aspect_ratio();

            /* TODO: Figure out what to do, if anything, with recording. */
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
      {
         struct retro_framebuffer *fb = (struct retro_framebuffer*)data;
         if (
               video_driver_poke
               && video_driver_poke->get_current_software_framebuffer
               && video_driver_poke->get_current_software_framebuffer(
                  video_driver_data, fb))
            return true;

         return false;
      }

      case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
      {
         const struct retro_hw_render_interface **iface = (const struct retro_hw_render_interface **)data;
         if (
               video_driver_poke
               && video_driver_poke->get_hw_render_interface
               && video_driver_poke->get_hw_render_interface(
                  video_driver_data, iface))
            return true;

         return false;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
#ifdef HAVE_CHEEVOS
         {
            bool state = *(const bool*)data;
            RARCH_LOG("Environ SET_SUPPORT_ACHIEVEMENTS: %s.\n", state ? "yes" : "no");
            rcheevos_set_support_cheevos(state);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
      {
         const struct retro_hw_render_context_negotiation_interface *iface =
            (const struct retro_hw_render_context_negotiation_interface*)data;
         RARCH_LOG("Environ SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE.\n");
         video_driver_set_context_negotiation_interface(iface);
         break;
      }

      case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
      {
         uint64_t *quirks = (uint64_t *) data;
         core_set_serialization_quirks(*quirks);
         break;
      }

      case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
         core_set_shared_context = true;
         break;

      case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
      {
         const uint32_t supported_vfs_version = 3;
         static struct retro_vfs_interface vfs_iface =
         {
            /* VFS API v1 */
            retro_vfs_file_get_path_impl,
            retro_vfs_file_open_impl,
            retro_vfs_file_close_impl,
            retro_vfs_file_size_impl,
            retro_vfs_file_tell_impl,
            retro_vfs_file_seek_impl,
            retro_vfs_file_read_impl,
            retro_vfs_file_write_impl,
            retro_vfs_file_flush_impl,
            retro_vfs_file_remove_impl,
            retro_vfs_file_rename_impl,
            /* VFS API v2 */
            retro_vfs_file_truncate_impl,
            /* VFS API v3 */
            retro_vfs_stat_impl,
            retro_vfs_mkdir_impl,
            retro_vfs_opendir_impl,
            retro_vfs_readdir_impl,
            retro_vfs_dirent_get_name_impl,
            retro_vfs_dirent_is_dir_impl,
            retro_vfs_closedir_impl
         };

         struct retro_vfs_interface_info *vfs_iface_info = (struct retro_vfs_interface_info *) data;
         if (vfs_iface_info->required_interface_version <= supported_vfs_version)
         {
            RARCH_LOG("Core requested VFS version >= v%d, providing v%d\n", vfs_iface_info->required_interface_version, supported_vfs_version);
            vfs_iface_info->required_interface_version = supported_vfs_version;
            vfs_iface_info->iface                      = &vfs_iface;
            system->supports_vfs = true;
         }
         else
         {
            RARCH_WARN("Core requested VFS version v%d which is higher than what we support (v%d)\n", vfs_iface_info->required_interface_version, supported_vfs_version);
            return false;
         }

         break;
      }

      case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
      {
         struct retro_led_interface *ledintf =
            (struct retro_led_interface *)data;
         if (ledintf)
            ledintf->set_led_state = led_driver_set_led;
      }
      break;

      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
      {
         int result = 0;
         if (!audio_suspended && audio_driver_active)
            result |= 2;
         if (video_driver_active && !(current_video->frame == video_null.frame))
            result |= 1;
#ifdef HAVE_RUNAHEAD
         if (request_fast_savestate)
            result |= 4;
         if (hard_disable_audio)
            result |= 8;
#endif
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_REPLAYING, NULL))
            result &= ~(1|2);
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            result |= 4;
#endif
         if (data != NULL)
         {
            int* result_p = (int*)data;
            *result_p = result;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_MIDI_INTERFACE:
      {
         struct retro_midi_interface *midi_interface =
               (struct retro_midi_interface *)data;

         if (midi_interface)
         {
            midi_interface->input_enabled = midi_driver_input_enabled;
            midi_interface->output_enabled = midi_driver_output_enabled;
            midi_interface->read = midi_driver_read;
            midi_interface->write = midi_driver_write;
            midi_interface->flush = midi_driver_flush;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
         *(bool *)data = runloop_fastmotion;
         break;

      case RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB:
         *(retro_environment_t *)data = rarch_clear_all_thread_waits;
         break;

      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
         /* Just falldown, the function will return true */
         break;

      case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
         /* Current API version is 1 */
         *(unsigned *)data = 1;
         break;

      case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
      {
         /* Try to use the polled refresh rate first.  */
         float target_refresh_rate = video_driver_get_refresh_rate();

         /* If the above function failed [possibly because it is not
          * implemented], use the refresh rate set in the config instead. */
         if (target_refresh_rate == 0.0 && settings)
            target_refresh_rate = settings->floats.video_refresh_rate;

         *(float *)data = target_refresh_rate;
         break;
      }

      default:
         RARCH_LOG("Environ UNSUPPORTED (#%u).\n", cmd);
         return false;
   }

   return true;
}

#ifdef HAVE_DYNAMIC
/**
 * libretro_get_environment_info:
 * @func                         : Function pointer for get_environment_info.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Sets environment callback in order to get statically known
 * information from it.
 *
 * Fetched via environment callbacks instead of
 * retro_get_system_info(), as this info is part of extensions.
 *
 * Should only be called once right after core load to
 * avoid overwriting the "real" environ callback.
 *
 * For statically linked cores, pass retro_set_environment as argument.
 */
static void libretro_get_environment_info(void (*func)(retro_environment_t),
      bool *load_no_content)
{
   load_no_content_hook = load_no_content;

   /* load_no_content gets set in this callback. */
   func(environ_cb_get_system_info);

   /* It's possible that we just set get_system_info callback
    * to the currently running core.
    *
    * Make sure we reset it to the actual environment callback.
    * Ignore any environment callbacks here in case we're running
    * on the non-current core. */
   ignore_environment_cb = true;
   func(rarch_environment_cb);
   ignore_environment_cb = false;
}

static bool load_dynamic_core(const char *path, char *buf, size_t size)
{
   /* Can't lookup symbols in itself on UWP */
#if !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
   if (dylib_proc(NULL, "retro_init"))
   {
      /* Try to verify that -lretro was not linked in from other modules
       * since loading it dynamically and with -l will fail hard. */
      RARCH_ERR("Serious problem. RetroArch wants to load libretro cores"
            " dynamically, but it is already linked.\n");
      RARCH_ERR("This could happen if other modules RetroArch depends on "
            "link against libretro directly.\n");
      RARCH_ERR("Proceeding could cause a crash. Aborting ...\n");
      retroarch_fail(1, "init_libretro_symbols()");
   }
#endif

   /* Need to use absolute path for this setting. It can be
    * saved to content history, and a relative path would
    * break in that scenario. */
   path_resolve_realpath(buf, size);
   if ((lib_handle = dylib_load(path)))
      return true;
   return false;
}

static dylib_t libretro_get_system_info_lib(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   dylib_t lib = dylib_load(path);
   void (*proc)(struct retro_system_info*);

   if (!lib)
      return NULL;

   proc = (void (*)(struct retro_system_info*))
      dylib_proc(lib, "retro_get_system_info");

   if (!proc)
   {
      dylib_close(lib);
      return NULL;
   }

   proc(info);

   if (load_no_content)
   {
      void (*set_environ)(retro_environment_t);
      *load_no_content = false;
      set_environ = (void (*)(retro_environment_t))
         dylib_proc(lib, "retro_set_environment");

      if (set_environ)
         libretro_get_environment_info(set_environ, load_no_content);
   }

   return lib;
}
#endif

static char current_library_name[1024];
static char current_library_version[1024];
static char current_valid_extensions[1024];

/**
 * libretro_get_system_info:
 * @path                         : Path to libretro library.
 * @info                         : Pointer to system info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from an arbitrary lib.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool libretro_get_system_info(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   struct retro_system_info dummy_info;
#ifdef HAVE_DYNAMIC
   dylib_t lib;
#endif

   dummy_info.library_name     = NULL;
   dummy_info.library_version  = NULL;
   dummy_info.valid_extensions = NULL;
   dummy_info.need_fullpath    = false;
   dummy_info.block_extract    = false;

#ifdef HAVE_DYNAMIC
   lib                         = libretro_get_system_info_lib(
         path, &dummy_info, load_no_content);

   if (!lib)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
            path);
      RARCH_ERR("Error(s): %s\n", dylib_error());
      return false;
   }
#else
   if (load_no_content)
   {
      load_no_content_hook = load_no_content;

      /* load_no_content gets set in this callback. */
      retro_set_environment(environ_cb_get_system_info);

      /* It's possible that we just set get_system_info callback
       * to the currently running core.
       *
       * Make sure we reset it to the actual environment callback.
       * Ignore any environment callbacks here in case we're running
       * on the non-current core. */
      ignore_environment_cb = true;
      retro_set_environment(rarch_environment_cb);
      ignore_environment_cb = false;
   }

   retro_get_system_info(&dummy_info);
#endif

   memcpy(info, &dummy_info, sizeof(*info));

   current_library_name[0] = '\0';
   current_library_version[0] = '\0';
   current_valid_extensions[0] = '\0';

   if (!string_is_empty(dummy_info.library_name))
      strlcpy(current_library_name,
            dummy_info.library_name, sizeof(current_library_name));
   if (!string_is_empty(dummy_info.library_version))
      strlcpy(current_library_version,
            dummy_info.library_version, sizeof(current_library_version));
   if (dummy_info.valid_extensions)
      strlcpy(current_valid_extensions,
            dummy_info.valid_extensions, sizeof(current_valid_extensions));

   info->library_name     = current_library_name;
   info->library_version  = current_library_version;
   info->valid_extensions = current_valid_extensions;

#ifdef HAVE_DYNAMIC
   dylib_close(lib);
#endif
   return true;
}

/**
 * load_symbols:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Setup libretro callback symbols. Returns true on success,
 * or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols_custom(enum rarch_core_type type,
      struct retro_core_t *current_core, const char *lib_path, void *_lib_handle_p)
{
#ifdef HAVE_DYNAMIC
   /* the library handle for use with the SYMBOL macro */
   dylib_t lib_handle_local;
#endif

   switch (type)
   {
      case CORE_TYPE_PLAIN:
         {
#ifdef HAVE_DYNAMIC
#ifdef HAVE_RUNAHEAD
            dylib_t *lib_handle_p = (dylib_t*)_lib_handle_p;
            if (!lib_path || !lib_handle_p)
#endif
            {
               const char *path = path_get(RARCH_PATH_CORE);

               if (string_is_empty(path))
               {
                  RARCH_ERR("Frontend is built for dynamic libretro cores, but "
                        "path is not set. Cannot continue.\n");
                  retroarch_fail(1, "init_libretro_symbols()");
               }

               RARCH_LOG("Loading dynamic libretro core from: \"%s\"\n",
                     path);

               if (!load_dynamic_core(
                        path,
                        path_get_ptr(RARCH_PATH_CORE),
                        path_get_realsize(RARCH_PATH_CORE)
                        ))
               {
                  RARCH_ERR("Failed to open libretro core: \"%s\"\n", path);
                  RARCH_ERR("Error(s): %s\n", dylib_error());
                  runloop_msg_queue_push(msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
                        1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  return false;
               }
               lib_handle_local = lib_handle;
            }
#ifdef HAVE_RUNAHEAD
            else
            {
               /* for a secondary core, we already have a
                * primary library loaded, so we can skip
                * some checks and just load the library */
               retro_assert(lib_path != NULL && lib_handle_p != NULL);
               lib_handle_local = dylib_load(lib_path);

               if (!lib_handle_local)
                  return false;
               *lib_handle_p = lib_handle_local;
            }
#endif
#endif

            CORE_SYMBOLS(SYMBOL);
         }
         break;
      case CORE_TYPE_DUMMY:
         CORE_SYMBOLS(SYMBOL_DUMMY);
         break;
      case CORE_TYPE_FFMPEG:
#ifdef HAVE_FFMPEG
         CORE_SYMBOLS(SYMBOL_FFMPEG);
#endif
         break;
      case CORE_TYPE_MPV:
#ifdef HAVE_MPV
         CORE_SYMBOLS(SYMBOL_MPV);
#endif
         break;
      case CORE_TYPE_IMAGEVIEWER:
#ifdef HAVE_IMAGEVIEWER
         CORE_SYMBOLS(SYMBOL_IMAGEVIEWER);
#endif
         break;
      case CORE_TYPE_NETRETROPAD:
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         CORE_SYMBOLS(SYMBOL_NETRETROPAD);
#endif
         break;
      case CORE_TYPE_VIDEO_PROCESSOR:
#if defined(HAVE_VIDEOPROCESSOR)
         CORE_SYMBOLS(SYMBOL_VIDEOPROCESSOR);
#endif
         break;
      case CORE_TYPE_GONG:
#ifdef HAVE_EASTEREGG
         CORE_SYMBOLS(SYMBOL_GONG);
#endif
         break;
   }

   return true;
}

/**
 * init_libretro_symbols:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Initializes libretro symbols and
 * setups environment callback functions. Returns true on success,
 * or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols(enum rarch_core_type type, struct retro_core_t *current_core)
{
   /* Load symbols */
   if (!init_libretro_symbols_custom(type, current_core, NULL, NULL))
      return false;

#ifdef HAVE_RUNAHEAD
   /* remember last core type created, so creating a
    * secondary core will know what core type to use. */
   set_last_core_type(type);
#endif
   return true;
}

bool libretro_get_shared_context(void)
{
   return core_set_shared_context;
}

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
static void uninit_libretro_symbols(struct retro_core_t *current_core)
{
#ifdef HAVE_DYNAMIC
   if (lib_handle)
      dylib_close(lib_handle);
   lib_handle = NULL;
#endif

   memset(current_core, 0, sizeof(struct retro_core_t));

   core_set_shared_context = false;

   rarch_ctl(RARCH_CTL_CORE_OPTIONS_DEINIT, NULL);
   rarch_ctl(RARCH_CTL_SYSTEM_INFO_FREE, NULL);
   rarch_ctl(RARCH_CTL_FRAME_TIME_FREE, NULL);
   camera_driver_active      = false;
   location_driver_active    = false;

   /* Performance counters no longer valid. */
   performance_counters_clear();
}

#if defined(HAVE_RUNAHEAD)
/* RUNAHEAD - SECONDARY CORE  */
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static void secondary_core_destroy(void)
{
   if (!secondary_module)
      return;

   /* unload game from core */
   if (secondary_core.retro_unload_game)
      secondary_core.retro_unload_game();
   /* deinit */
   if (secondary_core.retro_deinit)
      secondary_core.retro_deinit();
   memset(&secondary_core, 0, sizeof(struct retro_core_t));

   dylib_close(secondary_module);
   secondary_module = NULL;
   filestream_delete(secondary_library_path);
   if (secondary_library_path)
      free(secondary_library_path);
   secondary_library_path = NULL;
}

static bool secondary_core_ensure_exists(void)
{
   if (!secondary_module)
   {
      if (!secondary_core_create())
      {
         secondary_core_destroy();
         return false;
      }
   }
   return true;
}

static bool secondary_core_deserialize(const void *buffer, int size)
{
   if (secondary_core_ensure_exists())
      return secondary_core.retro_unserialize(buffer, size);
   return false;
}

static void remember_controller_port_device(long port, long device)
{
   if (port >= 0 && port < 16)
      port_map[port] = (int)device;
   if (secondary_module && secondary_core.retro_set_controller_port_device)
      secondary_core.retro_set_controller_port_device((unsigned)port, (unsigned)device);
}

static void clear_controller_port_map(void)
{
   unsigned port;
   for (port = 0; port < 16; port++)
      port_map[port] = -1;
}

static char *get_temp_directory_alloc(void)
{
   char *path             = NULL;
#ifdef _WIN32
#ifdef LEGACY_WIN32
   DWORD path_length      = GetTempPath(0, NULL) + 1;
   path                   = (char*)malloc(path_length * sizeof(char));

   if (!path)
      return NULL;

   path[path_length - 1]   = 0;
   GetTempPath(path_length, path);
#else
   DWORD path_length = GetTempPathW(0, NULL) + 1;
   wchar_t *wide_str = (wchar_t*)malloc(path_length * sizeof(wchar_t));

   if (!wide_str)
      return NULL;

   wide_str[path_length - 1] = 0;
   GetTempPathW(path_length, wide_str);

   path = utf16_to_utf8_string_alloc(wide_str);
   free(wide_str);
#endif
#elif defined ANDROID
   {
      settings_t *settings = configuration_settings;
      path = strcpy_alloc_force(settings->paths.directory_libretro);
   }
#else
   if (getenv("TMPDIR"))
      path = strcpy_alloc_force(getenv("TMPDIR"));
   else
      path = strcpy_alloc_force("/tmp");

#endif
   return path;
}

static bool write_file_with_random_name(char **temp_dll_path,
      const char *retroarch_temp_path, const void* data, ssize_t dataSize)
{
   unsigned i;
   char number_buf[32];
   bool okay                = false;
   const char *prefix       = "tmp";
   time_t time_value        = time(NULL);
   unsigned number_value    = (unsigned)time_value;
   char *ext                = strcpy_alloc_force(
         path_get_extension(*temp_dll_path));
   int ext_len              = (int)strlen(ext);

   if (ext_len > 0)
   {
      strcat_alloc(&ext, ".");
      memmove(ext + 1, ext, ext_len);
      ext[0] = '.';
      ext_len++;
   }

   /* Try up to 30 'random' filenames before giving up */
   for (i = 0; i < 30; i++)
   {
      int number;
      number_value = number_value * 214013 + 2531011;
      number       = (number_value >> 14) % 100000;

      snprintf(number_buf, sizeof(number_buf), "%05d", number);

      if (*temp_dll_path)
         free(*temp_dll_path);
      *temp_dll_path = NULL;

      strcat_alloc(temp_dll_path, retroarch_temp_path);
      strcat_alloc(temp_dll_path, prefix);
      strcat_alloc(temp_dll_path, number_buf);
      strcat_alloc(temp_dll_path, ext);

      if (filestream_write_file(*temp_dll_path, data, dataSize))
      {
         okay = true;
         break;
      }
   }

   if (ext)
      free(ext);
   ext = NULL;
   return okay;
}

static char *copy_core_to_temp_file(void)
{
   bool failed                = false;
   char *temp_directory       = NULL;
   char *retroarch_temp_path  = NULL;
   char *temp_dll_path        = NULL;
   void *dll_file_data        = NULL;
   int64_t dll_file_size      = 0;
   const char *core_path      = path_get(RARCH_PATH_CORE);
   const char *core_base_name = path_basename(core_path);

   if (strlen(core_base_name) == 0)
   {
      failed = true;
      goto end;
   }

   temp_directory = get_temp_directory_alloc();
   if (!temp_directory)
   {
      failed = true;
      goto end;
   }

   strcat_alloc(&retroarch_temp_path, temp_directory);
   strcat_alloc(&retroarch_temp_path, path_default_slash());
   strcat_alloc(&retroarch_temp_path, "retroarch_temp");
   strcat_alloc(&retroarch_temp_path, path_default_slash());

   if (!path_mkdir(retroarch_temp_path))
   {
      failed = true;
      goto end;
   }

   if (!filestream_read_file(core_path, &dll_file_data, &dll_file_size))
   {
      failed = true;
      goto end;
   }

   strcat_alloc(&temp_dll_path, retroarch_temp_path);
   strcat_alloc(&temp_dll_path, core_base_name);

   if (!filestream_write_file(temp_dll_path, dll_file_data, dll_file_size))
   {
      /* try other file names */
      if (!write_file_with_random_name(&temp_dll_path,
               retroarch_temp_path, dll_file_data, dll_file_size))
         failed = true;
   }

end:
   if (temp_directory)
      free(temp_directory);
   if (retroarch_temp_path)
      free(retroarch_temp_path);
   if (dll_file_data)
      free(dll_file_data);

   temp_directory      = NULL;
   retroarch_temp_path = NULL;
   dll_file_data       = NULL;

   if (!failed)
      return temp_dll_path;

   if (temp_dll_path)
      free(temp_dll_path);

   temp_dll_path     = NULL;

   return NULL;
}

static bool rarch_environment_secondary_core_hook(unsigned cmd, void *data)
{
   bool result = rarch_environment_cb(cmd, data);
   if (has_variable_update)
   {
      if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
      {
         bool *bool_p        = (bool*)data;
         *bool_p             = true;
         has_variable_update = false;
         return true;
      }
      else if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE)
         has_variable_update = false;
   }
   return result;
}

static bool secondary_core_create(void)
{
   long port, device;
   bool contentless       = false;
   bool is_inited         = false;

   if (  last_core_type != CORE_TYPE_PLAIN ||
         !load_content_info                ||
         load_content_info->special)
      return false;

   if (secondary_library_path)
      free(secondary_library_path);
   secondary_library_path = NULL;
   secondary_library_path = copy_core_to_temp_file();

   if (!secondary_library_path)
      return false;

   /* Load Core */
   if (init_libretro_symbols_custom(
            CORE_TYPE_PLAIN, &secondary_core,
            secondary_library_path, &secondary_module))
   {
      secondary_core.symbols_inited = true;
      secondary_core.retro_set_environment(
            rarch_environment_secondary_core_hook);
#ifdef HAVE_RUNAHEAD
      has_variable_update = true;
#endif

      secondary_core.retro_init();

      content_get_status(&contentless, &is_inited);
      secondary_core.inited = is_inited;

      /* Load Content */
      if (!load_content_info || load_content_info->special)
      {
         /* disabled due to crashes */
         return false;
#if 0
         secondary_core.game_loaded = secondary_core.retro_load_game_special(
               loadContentInfo.special->id, loadContentInfo.info, loadContentInfo.content->size);
         if (!secondary_core.game_loaded)
         {
            secondary_core_destroy();
            return false;
         }
#endif
      }
      else if (load_content_info->content->size > 0 && load_content_info->content->elems[0].data)
      {
         secondary_core.game_loaded = secondary_core.retro_load_game(load_content_info->info);
         if (!secondary_core.game_loaded)
         {
            secondary_core_destroy();
            return false;
         }
      }
      else if (contentless)
      {
         secondary_core.game_loaded = secondary_core.retro_load_game(NULL);
         if (!secondary_core.game_loaded)
         {
            secondary_core_destroy();
            return false;
         }
      }
      else
         secondary_core.game_loaded = false;

      if (!secondary_core.inited)
      {
         secondary_core_destroy();
         return false;
      }

      core_set_default_callbacks(&secondary_callbacks);
      secondary_core.retro_set_video_refresh(secondary_callbacks.frame_cb);
      secondary_core.retro_set_audio_sample(secondary_callbacks.sample_cb);
      secondary_core.retro_set_audio_sample_batch(secondary_callbacks.sample_batch_cb);
      secondary_core.retro_set_input_state(secondary_callbacks.state_cb);
      secondary_core.retro_set_input_poll(secondary_callbacks.poll_cb);

      for (port = 0; port < 16; port++)
      {
         device = port_map[port];
         if (device >= 0)
            secondary_core.retro_set_controller_port_device(
                  (unsigned)port, (unsigned)device);
      }
      clear_controller_port_map();
   }
   else
      return false;

   return true;
}

static void secondary_core_input_poll_null(void) { }

bool secondary_core_run_use_last_input(void)
{
   retro_input_poll_t old_poll_function;
   retro_input_state_t old_input_function;

   if (!secondary_core_ensure_exists())
      return false;

   old_poll_function            = secondary_callbacks.poll_cb;
   old_input_function           = secondary_callbacks.state_cb;

   secondary_callbacks.poll_cb  = secondary_core_input_poll_null;
   secondary_callbacks.state_cb = input_state_get_last;

   secondary_core.retro_set_input_poll(secondary_callbacks.poll_cb);
   secondary_core.retro_set_input_state(secondary_callbacks.state_cb);

   secondary_core.retro_run();

   secondary_callbacks.poll_cb  = old_poll_function;
   secondary_callbacks.state_cb = old_input_function;

   secondary_core.retro_set_input_poll(secondary_callbacks.poll_cb);
   secondary_core.retro_set_input_state(secondary_callbacks.state_cb);

   return true;
}

#else
bool secondary_core_deserialize(const void *buffer, int size)
{
   return false;
}

static void secondary_core_destroy(void)
{
}

static void remember_controller_port_device(long port, long device)
{
}

static void clear_controller_port_map(void)
{
}
#endif

#endif

/* MESSAGE QUEUE */
static void retroarch_msg_queue_deinit(void)
{
   runloop_msg_queue_lock();

   if (!runloop_msg_queue)
      return;

   msg_queue_free(runloop_msg_queue);

   runloop_msg_queue_unlock();
#ifdef HAVE_THREADS
   slock_free(_runloop_msg_queue_lock);
   _runloop_msg_queue_lock = NULL;
#endif

   runloop_msg_queue = NULL;
}

static void retroarch_msg_queue_init(void)
{
   retroarch_msg_queue_deinit();
   runloop_msg_queue       = msg_queue_new(8);

#ifdef HAVE_THREADS
   _runloop_msg_queue_lock = slock_new();
#endif
}

/* WIFI DRIVER  */

/**
 * wifi_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to wifi driver at index. Can be NULL
 * if nothing found.
 **/
const void *wifi_driver_find_handle(int idx)
{
   const void *drv = wifi_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * wifi_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of wifi driver at index. Can be NULL
 * if nothing found.
 **/
const char *wifi_driver_find_ident(int idx)
{
   const wifi_driver_t *drv = wifi_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_wifi_driver_options:
 *
 * Get an enumerated list of all wifi driver names,
 * separated by '|'.
 *
 * Returns: string listing of all wifi driver names,
 * separated by '|'.
 **/
const char* config_get_wifi_driver_options(void)
{
   return char_list_new_special(STRING_LIST_WIFI_DRIVERS, NULL);
}

void driver_wifi_scan(void)
{
   wifi_driver->scan();
}

void driver_wifi_get_ssids(struct string_list* ssids)
{
   wifi_driver->get_ssids(ssids);
}

bool driver_wifi_ssid_is_online(unsigned i)
{
   return wifi_driver->ssid_is_online(i);
}

bool driver_wifi_connect_ssid(unsigned i, const char* passphrase)
{
   return wifi_driver->connect_ssid(i, passphrase);
}

bool wifi_driver_ctl(enum rarch_wifi_ctl_state state, void *data)
{

   switch (state)
   {
      case RARCH_WIFI_CTL_DESTROY:
         wifi_driver_active   = false;
         wifi_driver          = NULL;
         wifi_data            = NULL;
         break;
      case RARCH_WIFI_CTL_SET_ACTIVE:
         wifi_driver_active = true;
         break;
      case RARCH_WIFI_CTL_FIND_DRIVER:
         {
            int i;
            driver_ctx_info_t drv;
            settings_t *settings = configuration_settings;

            drv.label = "wifi_driver";
            drv.s     = settings->arrays.wifi_driver;

            driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

            i = (int)drv.len;

            if (i >= 0)
               wifi_driver = (const wifi_driver_t*)wifi_driver_find_handle(i);
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_ERR("Couldn't find any wifi driver named \"%s\"\n",
                        settings->arrays.wifi_driver);
                  RARCH_LOG_OUTPUT("Available wifi drivers are:\n");
                  for (d = 0; wifi_driver_find_handle(d); d++)
                     RARCH_LOG_OUTPUT("\t%s\n", wifi_driver_find_ident(d));

                  RARCH_WARN("Going to default to first wifi driver...\n");
               }

               wifi_driver = (const wifi_driver_t*)wifi_driver_find_handle(0);

               if (!wifi_driver)
                  retroarch_fail(1, "find_wifi_driver()");
            }
         }
         break;
      case RARCH_WIFI_CTL_UNSET_ACTIVE:
         wifi_driver_active = false;
         break;
      case RARCH_WIFI_CTL_IS_ACTIVE:
        return wifi_driver_active;
      case RARCH_WIFI_CTL_DEINIT:
        if (wifi_data && wifi_driver)
        {
           if (wifi_driver->free)
              wifi_driver->free(wifi_data);
        }

        wifi_data = NULL;
        break;
      case RARCH_WIFI_CTL_STOP:
        if (     wifi_driver
              && wifi_driver->stop
              && wifi_data)
           wifi_driver->stop(wifi_data);
        break;
      case RARCH_WIFI_CTL_START:
        if (wifi_driver && wifi_data && wifi_driver->start)
        {
           settings_t *settings = configuration_settings;
           if (settings->bools.wifi_allow)
              return wifi_driver->start(wifi_data);
        }
        return false;
      case RARCH_WIFI_CTL_SET_CB:
        {
           /*struct retro_wifi_callback *cb =
              (struct retro_wifi_callback*)data;
           wifi_cb          = *cb;*/
        }
        break;
      case RARCH_WIFI_CTL_INIT:
        /* Resource leaks will follow if wifi is initialized twice. */
        if (wifi_data)
           return false;

        wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);

        wifi_data = wifi_driver->init();

        if (!wifi_data)
        {
           RARCH_ERR("Failed to initialize wifi driver. Will continue without wifi.\n");
           wifi_driver_ctl(RARCH_WIFI_CTL_UNSET_ACTIVE, NULL);
        }

        /*if (wifi_cb.initialized)
           wifi_cb.initialized();*/
        break;
      default:
         break;
   }

   return false;
}

/* UI COMPANION */

void ui_companion_set_foreground(unsigned enable)
{
   main_ui_companion_is_on_foreground = enable;
}

bool ui_companion_is_on_foreground(void)
{
   return main_ui_companion_is_on_foreground;
}

void ui_companion_event_command(enum event_command action)
{
   const ui_companion_driver_t *ui = ui_companion;

   if (ui && ui->event_command)
      ui->event_command(ui_companion_data, action);
#ifdef HAVE_QT
   if (ui_companion_qt.toggle && qt_is_inited)
      ui_companion_qt.event_command(ui_companion_qt_data, action);
#endif
}

void ui_companion_driver_deinit(void)
{
   const ui_companion_driver_t *ui = ui_companion;

   if (!ui)
      return;
   if (ui->deinit)
      ui->deinit(ui_companion_data);

#ifdef HAVE_QT
   if (qt_is_inited)
   {
      ui_companion_qt.deinit(ui_companion_qt_data);
      ui_companion_qt_data = NULL;
   }
#endif
   ui_companion_data = NULL;
}

void ui_companion_driver_init_first(void)
{
   settings_t *settings       = configuration_settings;

   ui_companion               = (ui_companion_driver_t*)ui_companion_drivers[0];

#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable && settings->bools.ui_companion_toggle)
   {
      ui_companion_qt_data = ui_companion_qt.init();
      qt_is_inited = true;
   }
#endif

   if (ui_companion)
   {
      if (settings->bools.ui_companion_start_on_boot)
      {
         if (ui_companion->init)
            ui_companion_data = ui_companion->init();

         ui_companion_driver_toggle(false);
      }
   }
}

void ui_companion_driver_toggle(bool force)
{
#ifdef HAVE_QT
   settings_t *settings = configuration_settings;
#endif

   if (ui_companion && ui_companion->toggle)
      ui_companion->toggle(ui_companion_data, false);

#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable)
   {
      if ((settings->bools.ui_companion_toggle || force) && !qt_is_inited)
      {
         ui_companion_qt_data = ui_companion_qt.init();
         qt_is_inited = true;
      }

      if (ui_companion_qt.toggle && qt_is_inited)
         ui_companion_qt.toggle(ui_companion_qt_data, force);
   }
#endif
}

void ui_companion_driver_notify_refresh(void)
{
   const ui_companion_driver_t *ui = ui_companion;
#ifdef HAVE_QT
   settings_t      *settings       = configuration_settings;
#endif

   if (!ui)
      return;
   if (ui->notify_refresh)
      ui->notify_refresh(ui_companion_data);
#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable)
      if (ui_companion_qt.notify_refresh && qt_is_inited)
         ui_companion_qt.notify_refresh(ui_companion_qt_data);
#endif
}

void ui_companion_driver_notify_list_loaded(file_list_t *list, file_list_t *menu_list)
{
   const ui_companion_driver_t *ui = ui_companion;
   if (!ui)
      return;
   if (ui->notify_list_loaded)
      ui->notify_list_loaded(ui_companion_data, list, menu_list);
}

void ui_companion_driver_notify_content_loaded(void)
{
   const ui_companion_driver_t *ui = ui_companion;
   if (!ui)
      return;
   if (ui->notify_content_loaded)
      ui->notify_content_loaded(ui_companion_data);
}

void ui_companion_driver_free(void)
{
   ui_companion = NULL;
}

const ui_msg_window_t *ui_companion_driver_get_msg_window_ptr(void)
{
   const ui_companion_driver_t *ui = ui_companion;
   if (!ui)
      return NULL;
   return ui->msg_window;
}

const ui_window_t *ui_companion_driver_get_window_ptr(void)
{
   const ui_companion_driver_t *ui = ui_companion;
   if (!ui)
      return NULL;
   return ui->window;
}

const ui_browser_window_t *ui_companion_driver_get_browser_window_ptr(void)
{
   const ui_companion_driver_t *ui = ui_companion;
   if (!ui)
      return NULL;
   return ui->browser_window;
}

static void ui_companion_driver_msg_queue_push(
      const char *msg, unsigned priority, unsigned duration, bool flush)
{
   const ui_companion_driver_t *ui = ui_companion;
#ifdef HAVE_QT
   settings_t *settings            = configuration_settings;
#endif

   if (ui && ui->msg_queue_push)
      ui->msg_queue_push(ui_companion_data, msg, priority, duration, flush);
#ifdef HAVE_QT
   if (settings->bools.desktop_menu_enable)
      if (ui_companion_qt.msg_queue_push && qt_is_inited)
         ui_companion_qt.msg_queue_push(ui_companion_qt_data, msg, priority, duration, flush);
#endif
}

void *ui_companion_driver_get_main_window(void)
{
   const ui_companion_driver_t *ui = ui_companion;
   if (!ui || !ui->get_main_window)
      return NULL;
   return ui->get_main_window(ui_companion_data);
}

const char *ui_companion_driver_get_ident(void)
{
   const ui_companion_driver_t *ui = ui_companion;
   if (!ui)
      return "null";
   return ui->ident;
}

void ui_companion_driver_log_msg(const char *msg)
{
#ifdef HAVE_QT
   settings_t *settings = configuration_settings;

   if (settings->bools.desktop_menu_enable)
      if (ui_companion_qt_data && qt_is_inited)
         ui_companion_qt.log_msg(ui_companion_qt_data, msg);
#endif
}

/* RECORDING */

/**
 * record_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of record driver at index. Can be NULL
 * if nothing found.
 **/
const char *record_driver_find_ident(int idx)
{
   const record_driver_t *drv = record_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * record_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to record driver at index. Can be NULL
 * if nothing found.
 **/
const void *record_driver_find_handle(int idx)
{
   const void *drv = record_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * Returns: string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void)
{
   return char_list_new_special(STRING_LIST_RECORD_DRIVERS, NULL);
}

#if 0
/* TODO/FIXME - not used apparently */
static void find_record_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = configuration_settings;

   drv.label = "record_driver";
   drv.s     = settings->arrays.record_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = (int)drv.len;

   if (i >= 0)
      recording_driver = (const record_driver_t*)record_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;

         RARCH_ERR("[recording] Couldn't find any record driver named \"%s\"\n",
               settings->arrays.record_driver);
         RARCH_LOG_OUTPUT("Available record drivers are:\n");
         for (d = 0; record_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", record_driver_find_ident(d));
         RARCH_WARN("[recording] Going to default to first record driver...\n");
      }

      recording_driver = (const record_driver_t*)record_driver_find_handle(0);

      if (!recording_driver)
         retroarch_fail(1, "find_record_driver()");
   }
}

/**
 * ffemu_find_backend:
 * @ident                   : Identifier of driver to find.
 *
 * Finds a recording driver with the name @ident.
 *
 * Returns: recording driver handle if successful, otherwise
 * NULL.
 **/
static const record_driver_t *ffemu_find_backend(const char *ident)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      if (string_is_equal(record_drivers[i]->ident, ident))
         return record_drivers[i];
   }

   return NULL;
}

static void recording_driver_free_state(void)
{
   /* TODO/FIXME - this is not being called anywhere */
   recording_gpu_width      = 0;
   recording_gpu_height     = 0;
   recording_width          = 0;
   recording_height         = 0;
}
#endif

/**
 * gfx_ctx_init_first:
 * @backend                 : Recording backend handle.
 * @data                    : Recording data handle.
 * @params                  : Recording info parameters.
 *
 * Finds first suitable recording context driver and initializes.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool record_driver_init_first(
      const record_driver_t **backend, void **data,
      const struct record_params *params)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      void *handle = record_drivers[i]->init(params);

      if (!handle)
         continue;

      *backend = record_drivers[i];
      *data = handle;
      return true;
   }

   return false;
}

static void recording_dump_frame(const void *data, unsigned width,
      unsigned height, size_t pitch, bool is_idle)
{
   struct record_video_data ffemu_data;

   ffemu_data.data     = data;
   ffemu_data.width    = width;
   ffemu_data.height   = height;
   ffemu_data.pitch    = (int)pitch;
   ffemu_data.is_dupe  = false;

   if (video_driver_record_gpu_buffer != NULL)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_WARN("[recording] %s \n",
               msg_hash_to_str(MSG_VIEWPORT_SIZE_CALCULATION_FAILED));
         command_event(CMD_EVENT_GPU_RECORD_DEINIT, NULL);

         recording_dump_frame(data, width, height, pitch, is_idle);
         return;
      }

      /* User has resized. We kinda have a problem now. */
      if (  vp.width  != recording_gpu_width ||
            vp.height != recording_gpu_height)
      {
         RARCH_WARN("[recording] %s\n", msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE));

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         return;
      }

      /* Big bottleneck.
       * Since we might need to do read-backs asynchronously,
       * it might take 3-4 times before this returns true. */
      if (!video_driver_read_viewport(video_driver_record_gpu_buffer, is_idle))
         return;

      ffemu_data.pitch  = (int)(recording_gpu_width * 3);
      ffemu_data.width  = (unsigned)recording_gpu_width;
      ffemu_data.height = (unsigned)recording_gpu_height;
      ffemu_data.data   = video_driver_record_gpu_buffer + (ffemu_data.height - 1) * ffemu_data.pitch;

      ffemu_data.pitch  = -ffemu_data.pitch;
   }
   else
      ffemu_data.is_dupe = !data;

   recording_driver->push_video(recording_data, &ffemu_data);
}

bool recording_deinit(void)
{
   if (!recording_data || !recording_driver)
      return false;

   if (recording_driver->finalize)
      recording_driver->finalize(recording_data);

   if (recording_driver->free)
      recording_driver->free(recording_data);

   recording_data            = NULL;
   recording_driver          = NULL;

   command_event(CMD_EVENT_GPU_RECORD_DEINIT, NULL);

   return true;
}

bool recording_is_enabled(void)
{
   return recording_enable;
}

void recording_set_state(bool state)
{
   recording_enable = state;
}

bool streaming_is_enabled(void)
{
   return streaming_enable;
}

void streaming_set_state(bool state)
{
   streaming_enable = state;
}

static bool video_driver_gpu_record_init(unsigned size)
{
   video_driver_record_gpu_buffer = (uint8_t*)malloc(size);
   if (!video_driver_record_gpu_buffer)
      return false;
   return true;
}

void video_driver_gpu_record_deinit(void)
{
   free(video_driver_record_gpu_buffer);
   video_driver_record_gpu_buffer = NULL;
}

/**
 * recording_init:
 *
 * Initializes recording.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool recording_init(void)
{
   char output[PATH_MAX_LENGTH];
   char buf[PATH_MAX_LENGTH];
   struct record_params params          = {0};
   struct retro_system_av_info *av_info = &video_driver_av_info;
   settings_t *settings                 = configuration_settings;
   global_t *global                     = &g_extern;

   if (!recording_enable)
      return false;

   output[0] = '\0';

   if (rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
   {
      RARCH_WARN("[recording] %s\n",
            msg_hash_to_str(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED));
      return false;
   }

   if (!settings->bools.video_gpu_record
         && video_driver_is_hw_context())
   {
      RARCH_WARN("[recording] %s.\n",
            msg_hash_to_str(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING));
      return false;
   }

   RARCH_LOG("[recording] %s: FPS: %.4f, Sample rate: %.4f\n",
         msg_hash_to_str(MSG_CUSTOM_TIMING_GIVEN),
         (float)av_info->timing.fps,
         (float)av_info->timing.sample_rate);

   if (!string_is_empty(global->record.path))
      strlcpy(output, global->record.path, sizeof(output));
   else
   {
      if (streaming_is_enabled())
         if (!string_is_empty(settings->paths.path_stream_url))
            strlcpy(output, settings->paths.path_stream_url, sizeof(output));
         else
            /* Fallback, stream locally to 127.0.0.1 */
            snprintf(output, sizeof(output), "udp://127.0.0.1:%u", settings->uints.video_stream_port);
      else
      {
         const char *game_name = path_basename(path_get(RARCH_PATH_BASENAME));
         if (settings->uints.video_record_quality < RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST)
         {
            fill_str_dated_filename(buf, game_name,
                     "mkv", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else if (settings->uints.video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST 
               && settings->uints.video_record_quality < RECORD_CONFIG_TYPE_RECORDING_GIF)
         {
            fill_str_dated_filename(buf, game_name,
                     "webm", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else if (settings->uints.video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_GIF 
               && settings->uints.video_record_quality < RECORD_CONFIG_TYPE_RECORDING_APNG)
         {
            fill_str_dated_filename(buf, game_name,
                     "gif", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else
         {
            fill_str_dated_filename(buf, game_name,
                     "png", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
      }
   }

   params.out_width  = av_info->geometry.base_width;
   params.out_height = av_info->geometry.base_height;
   params.fb_width   = av_info->geometry.max_width;
   params.fb_height  = av_info->geometry.max_height;
   params.channels   = 2;
   params.filename   = output;
   params.fps        = av_info->timing.fps;
   params.samplerate = av_info->timing.sample_rate;
   params.pix_fmt    = (video_driver_get_pixel_format() == RETRO_PIXEL_FORMAT_XRGB8888) ?
      FFEMU_PIX_ARGB8888 : FFEMU_PIX_RGB565;
   params.config     = NULL;

   if (!string_is_empty(global->record.config))
      params.config = global->record.config;
   else
   {
      if (streaming_is_enabled())
      {
         params.config = settings->paths.path_stream_config;
         params.preset = (enum record_config_type)
            settings->uints.video_stream_quality;
      }
      else
      {
         params.config = settings->paths.path_record_config;
         params.preset = (enum record_config_type)
            settings->uints.video_record_quality;
      }
   }

   if (settings->bools.video_gpu_record
      && current_video->read_viewport)
   {
      unsigned gpu_size;
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_ERR("[recording] Failed to get viewport information from video driver. "
               "Cannot start recording ...\n");
         return false;
      }

      params.out_width  = vp.width;
      params.out_height = vp.height;
      params.fb_width   = next_pow2(vp.width);
      params.fb_height  = next_pow2(vp.height);

      if (settings->bools.video_force_aspect &&
            (video_driver_get_aspect_ratio() > 0.0f))
         params.aspect_ratio  = video_driver_get_aspect_ratio();
      else
         params.aspect_ratio  = (float)vp.width / vp.height;

      params.pix_fmt             = FFEMU_PIX_BGR24;
      recording_gpu_width        = vp.width;
      recording_gpu_height       = vp.height;

      RARCH_LOG("[recording] %s %u x %u\n", msg_hash_to_str(MSG_DETECTED_VIEWPORT_OF),
            vp.width, vp.height);

      gpu_size = vp.width * vp.height * 3;
      if (!video_driver_gpu_record_init(gpu_size))
         return false;
   }
   else
   {
      if (recording_width || recording_height)
      {
         params.out_width  = recording_width;
         params.out_height = recording_height;
      }

      if (settings->bools.video_force_aspect &&
            (video_driver_get_aspect_ratio() > 0.0f))
         params.aspect_ratio = video_driver_get_aspect_ratio();
      else
         params.aspect_ratio = (float)params.out_width / params.out_height;

      if (settings->bools.video_post_filter_record
            && !!video_driver_state_filter)
      {
         unsigned max_width  = 0;
         unsigned max_height = 0;

         params.pix_fmt    = FFEMU_PIX_RGB565;

         if (video_driver_state_out_rgb32)
            params.pix_fmt = FFEMU_PIX_ARGB8888;

         rarch_softfilter_get_max_output_size(
               video_driver_state_filter,
               &max_width, &max_height);
         params.fb_width  = next_pow2(max_width);
         params.fb_height = next_pow2(max_height);
      }
   }

   RARCH_LOG("[recording] %s %s @ %ux%u. (FB size: %ux%u pix_fmt: %u)\n",
         msg_hash_to_str(MSG_RECORDING_TO),
         output,
         params.out_width, params.out_height,
         params.fb_width, params.fb_height,
         (unsigned)params.pix_fmt);

   if (!record_driver_init_first(&recording_driver, &recording_data, &params))
   {
      RARCH_ERR("[recording] %s\n", msg_hash_to_str(MSG_FAILED_TO_START_RECORDING));
      command_event(CMD_EVENT_GPU_RECORD_DEINIT, NULL);

      return false;
   }

   return true;
}

void recording_driver_update_streaming_url(void)
{
   settings_t *settings    = configuration_settings;
   const char* youtube_url = "rtmp://a.rtmp.youtube.com/live2/";
   const char* twitch_url  = "rtmp://live.twitch.tv/app/";

   if (!settings)
      return;

   switch (settings->uints.streaming_mode)
   {
      case STREAMING_MODE_TWITCH:
         if (!string_is_empty(settings->arrays.twitch_stream_key))
            snprintf(settings->paths.path_stream_url, sizeof(settings->paths.path_stream_url),
               "%s%s", twitch_url, settings->arrays.twitch_stream_key);
         else
         {
            /* TODO: Show input box for twitch_stream_key*/
            RARCH_LOG("[recording] twitch streaming key empty\n");
         }
         break;
      case STREAMING_MODE_YOUTUBE:
         if (!string_is_empty(settings->arrays.youtube_stream_key))
         {
            snprintf(settings->paths.path_stream_url, sizeof(settings->paths.path_stream_url),
               "%s%s", youtube_url, settings->arrays.youtube_stream_key);
         }
         else
         {
            /* TODO: Show input box for youtube_stream_key*/
            RARCH_LOG("[recording] youtube streaming key empty\n");
         }
         break;
      case STREAMING_MODE_LOCAL:
         /* TODO: figure out default interface and bind to that instead */
         snprintf(settings->paths.path_stream_url, sizeof(settings->paths.path_stream_url),
            "udp://%s:%u", "127.0.0.1", settings->uints.video_stream_port);
         break;
      case STREAMING_MODE_CUSTOM:
      default:
         /* Do nothing, let the user input the URL */
         break;
   }
}

/* BSV MOVIE */
static bool bsv_movie_init_playback(
      bsv_movie_t *handle, const char *path)
{
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for playback, path : \"%s\".\n", path);
      return false;
   }

   handle->file              = file;
   handle->playback          = true;

   intfstream_read(handle->file, header, sizeof(uint32_t) * 4);
   /* Compatibility with old implementation that
    * used incorrect documentation. */
   if (swap_if_little32(header[MAGIC_INDEX]) != BSV_MAGIC
         && swap_if_big32(header[MAGIC_INDEX]) != BSV_MAGIC)
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE));
      return false;
   }

   content_crc               = content_get_crc();

   if (content_crc != 0)
      if (swap_if_big32(header[CRC_INDEX]) != content_crc)
         RARCH_WARN("%s.\n", msg_hash_to_str(MSG_CRC32_CHECKSUM_MISMATCH));

   state_size = swap_if_big32(header[STATE_SIZE_INDEX]);

#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   if (state_size)
   {
      retro_ctx_size_info_t info;
      retro_ctx_serialize_info_t serial_info;
      uint8_t *buf       = (uint8_t*)malloc(state_size);

      if (!buf)
         return false;

      handle->state      = buf;
      handle->state_size = state_size;
      if (intfstream_read(handle->file,
               handle->state, state_size) != state_size)
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_COULD_NOT_READ_STATE_FROM_MOVIE));
         return false;
      }

      core_serialize_size( &info);

      if (info.size == state_size)
      {
         serial_info.data_const = handle->state;
         serial_info.size       = state_size;
         core_unserialize(&serial_info);
      }
      else
         RARCH_WARN("%s\n",
               msg_hash_to_str(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION));
   }

   handle->min_file_pos = sizeof(header) + state_size;

   return true;
}

static bool bsv_movie_init_record(
      bsv_movie_t *handle, const char *path)
{
   retro_ctx_size_info_t info;
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for recording, path : \"%s\".\n", path);
      return false;
   }

   handle->file             = file;

   content_crc              = content_get_crc();

   /* This value is supposed to show up as
    * BSV1 in a HEX editor, big-endian. */
   header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   header[CRC_INDEX]        = swap_if_big32(content_crc);

   core_serialize_size(&info);

   state_size               = (unsigned)info.size;

   header[STATE_SIZE_INDEX] = swap_if_big32(state_size);
#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   intfstream_write(handle->file, header, 4 * sizeof(uint32_t));

   handle->min_file_pos     = sizeof(header) + state_size;
   handle->state_size       = state_size;

   if (state_size)
   {
      retro_ctx_serialize_info_t serial_info;

      handle->state = (uint8_t*)malloc(state_size);
      if (!handle->state)
         return false;

      serial_info.data = handle->state;
      serial_info.size = state_size;

      core_serialize(&serial_info);

      intfstream_write(handle->file,
            handle->state, state_size);
   }

   return true;
}

static void bsv_movie_free(bsv_movie_t *handle)
{
   if (!handle)
      return;

   intfstream_close(handle->file);
   free(handle->file);

   free(handle->state);
   free(handle->frame_pos);
   free(handle);
}

static bsv_movie_t *bsv_movie_init_internal(const char *path,
      enum rarch_movie_type type)
{
   size_t *frame_pos   = NULL;
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!bsv_movie_init_playback(handle, path))
         goto error;
   }
   else if (!bsv_movie_init_record(handle, path))
      goto error;

   /* Just pick something really large
    * ~1 million frames rewind should do the trick. */
   if (!(frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error;

   handle->frame_pos       = frame_pos;

   handle->frame_pos[0]    = handle->min_file_pos;
   handle->frame_mask      = (1 << 20) - 1;

   return handle;

error:
   bsv_movie_free(handle);
   return NULL;
}

void bsv_movie_frame_rewind(void)
{
   bsv_movie_t *handle = bsv_movie_state_handle;

   if (!handle)
      return;

   handle->did_rewind = true;

   if (     (handle->frame_ptr <= 1)
         && (handle->frame_pos[0] == handle->min_file_pos))
   {
      /* If we're at the beginning... */
      handle->frame_ptr = 0;
      intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
   else
   {
      /* First time rewind is performed, the old frame is simply replayed.
       * However, playing back that frame caused us to read data, and push
       * data to the ring buffer.
       *
       * Sucessively rewinding frames, we need to rewind past the read data,
       * plus another. */
      handle->frame_ptr = (handle->frame_ptr -
            (handle->first_rewind ? 1 : 2)) & handle->frame_mask;
      intfstream_seek(handle->file,
            (int)handle->frame_pos[handle->frame_ptr], SEEK_SET);
   }

   if (intfstream_tell(handle->file) <= (long)handle->min_file_pos)
   {
      /* We rewound past the beginning. */

      if (!handle->playback)
      {
         retro_ctx_serialize_info_t serial_info;

         /* If recording, we simply reset
          * the starting point. Nice and easy. */

         intfstream_seek(handle->file, 4 * sizeof(uint32_t), SEEK_SET);

         serial_info.data = handle->state;
         serial_info.size = handle->state_size;

         core_serialize(&serial_info);

         intfstream_write(handle->file, handle->state, handle->state_size);
      }
      else
         intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
}

static bool bsv_movie_init_handle(const char *path,
      enum rarch_movie_type type)
{
   bsv_movie_t *state     = bsv_movie_init_internal(path, type);
   if (!state)
      return false;

   bsv_movie_state_handle = state;
   return true;
}

bool bsv_movie_init(void)
{
   bool set_granularity = false;

   if (bsv_movie_state.movie_start_playback)
   {
      if (!bsv_movie_init_handle(bsv_movie_state.movie_start_path,
                  RARCH_MOVIE_PLAYBACK))
      {
         RARCH_ERR("%s: \"%s\".\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
               bsv_movie_state.movie_start_path);
         return false;
      }

      bsv_movie_state.movie_playback = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK),
            2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s.\n", msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));

      set_granularity = true;
   }
   else if (bsv_movie_state.movie_start_recording)
   {
      if (!bsv_movie_init_handle(bsv_movie_state.movie_start_path,
                  RARCH_MOVIE_RECORD))
      {
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("%s.\n",
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
         return false;
      }

      {
         char msg[8192];
         snprintf(msg, sizeof(msg),
               "%s \"%s\".",
               msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
               bsv_movie_state.movie_start_path);

         runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
               bsv_movie_state.movie_start_path);
      }

      set_granularity = true;
   }

   if (set_granularity)
   {
      settings_t *settings    = configuration_settings;
      configuration_set_uint(settings,
            settings->uints.rewind_granularity, 1);
   }

   return true;
}

void bsv_movie_set_path(const char *path)
{
   strlcpy(bsv_movie_state.movie_path,
         path, sizeof(bsv_movie_state.movie_path));
}

void bsv_movie_deinit(void)
{
   if (!bsv_movie_state_handle)
      return;

   bsv_movie_free(bsv_movie_state_handle);
   bsv_movie_state_handle = NULL;
}

static bool runloop_check_movie_init(void)
{
   char msg[16384], path[8192];
   settings_t *settings       = configuration_settings;

   msg[0] = path[0]           = '\0';

   configuration_set_uint(settings, settings->uints.rewind_granularity, 1);

   if (settings->ints.state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            bsv_movie_state.movie_path,
            settings->ints.state_slot);
   else
      strlcpy(path, bsv_movie_state.movie_path, sizeof(path));

   strlcat(path,
         file_path_str(FILE_PATH_BSV_EXTENSION),
         sizeof(path));

   snprintf(msg, sizeof(msg), "%s \"%s\".",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   bsv_movie_init_handle(path, RARCH_MOVIE_RECORD);

   if (!bsv_movie_state_handle)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
      return false;
   }

   runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   return true;
}

bool bsv_movie_check(void)
{
   if (!bsv_movie_state_handle)
      return runloop_check_movie_init();

   if (bsv_movie_state.movie_playback)
   {
      /* Checks if movie is being played back. */
      if (!bsv_movie_state.movie_end)
         return false;
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED), 2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED));

      command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

      bsv_movie_state.movie_end      = false;
      bsv_movie_state.movie_playback = false;

      return true;
   }

   /* Checks if movie is being recorded. */
   if (!bsv_movie_state_handle)
      return false;

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED), 2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED));

   command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

   return true;
}

/* INPUT OVERLAY */

#ifdef HAVE_OVERLAY
static bool video_driver_overlay_interface(
      const video_overlay_interface_t **iface);

/**
 * input_overlay_add_inputs:
 * @ol : pointer to overlay
 * @port : the user to show the inputs of
 *
 * Adds inputs from current_input to the overlay, so it's displayed
 * returns true if an input that is pressed will change the overlay
 */
static bool input_overlay_add_inputs_inner(overlay_desc_t *desc,
      unsigned port, unsigned analog_dpad_mode)
{
   switch(desc->type)
   {
      case OVERLAY_TYPE_BUTTONS:
         {
            unsigned i;
            bool all_buttons_pressed        = false;

            /*Check each bank of the mask*/
            for (i = 0; i < ARRAY_SIZE(desc->button_mask.data); ++i)
            {
               /*Get bank*/
               uint32_t bank_mask = BITS_GET_ELEM(desc->button_mask,i);
               unsigned        id = i * 32;

               /*Worth pursuing? Have we got any bits left in here?*/
               while (bank_mask)
               {
                  /*If this bit is set then we need to query the pad
                   *The button must be pressed.*/
                  if (bank_mask & 1)
                  {
                     /* Light up the button if pressed */
                     if (!input_state(port, RETRO_DEVICE_JOYPAD, 0, id))
                     {
                        /* We need ALL of the inputs to be active,
                         * abort. */
                        desc->updated    = false;
                        return false;
                     }

                     all_buttons_pressed = true;
                     desc->updated       = true;
                  }

                  bank_mask >>= 1;
                  ++id;
               }
            }

            return all_buttons_pressed;
         }

      case OVERLAY_TYPE_ANALOG_LEFT:
      case OVERLAY_TYPE_ANALOG_RIGHT:
         {
            unsigned int index = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ?
               RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;

            float analog_x     = input_state(port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_X);
            float analog_y     = input_state(port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_Y);
            float dx           = (analog_x/0x8000)*(desc->range_x/2);
            float dy           = (analog_y/0x8000)*(desc->range_y/2);

            desc->delta_x      = dx;
            desc->delta_y      = dy;

            /*Maybe use some option here instead of 0, only display
              changes greater than some magnitude.
              */
            if ((dx * dx) > 0 || (dy*dy) > 0)
               return true;
         }
         break;

      case OVERLAY_TYPE_KEYBOARD:
         if (input_state(port, RETRO_DEVICE_KEYBOARD, 0, desc->retro_key_idx))
         {
            desc->updated  = true;
            return true;
         }
         break;

      default:
         break;
   }

   return false;
}

static bool input_overlay_add_inputs(input_overlay_t *ol,
      unsigned port, unsigned analog_dpad_mode)
{
   unsigned i;
   bool button_pressed             = false;
   input_overlay_state_t *ol_state = &ol->overlay_state;

   if (!ol_state)
      return false;

   for (i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc  = &(ol->active->descs[i]);
      button_pressed       |= input_overlay_add_inputs_inner(desc,
            port, analog_dpad_mode);
   }

   return button_pressed;
}
/**
 * input_overlay_scale:
 * @ol                    : Overlay handle.
 * @scale                 : Scaling factor.
 *
 * Scales overlay and all its associated descriptors
 * by a given scaling factor (@scale).
 **/
static void input_overlay_scale(struct overlay *ol, float scale)
{
   size_t i;

   if (ol->block_scale)
      scale = 1.0f;

   ol->scale = scale;
   ol->mod_w = ol->w * scale;
   ol->mod_h = ol->h * scale;
   ol->mod_x = ol->center_x +
      (ol->x - ol->center_x) * scale;
   ol->mod_y = ol->center_y +
      (ol->y - ol->center_y) * scale;

   for (i = 0; i < ol->size; i++)
   {
      struct overlay_desc *desc = &ol->descs[i];
      float scale_w             = ol->mod_w * desc->range_x;
      float scale_h             = ol->mod_h * desc->range_y;
      float adj_center_x        = ol->mod_x + desc->x * ol->mod_w;
      float adj_center_y        = ol->mod_y + desc->y * ol->mod_h;

      desc->mod_w               = 2.0f * scale_w;
      desc->mod_h               = 2.0f * scale_h;
      desc->mod_x               = adj_center_x - scale_w;
      desc->mod_y               = adj_center_y - scale_h;
   }
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   if (ol->iface->vertex_geom)
      for (i = 0; i < ol->active->size; i++)
      {
         struct overlay_desc *desc = &ol->active->descs[i];

         if (!desc->image.pixels)
            continue;

         ol->iface->vertex_geom(ol->iface_data, desc->image_index,
               desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
      }
}

/**
 * input_overlay_set_scale_factor:
 * @ol                    : Overlay handle.
 * @scale                 : Factor of scale to apply.
 *
 * Scales the overlay by a factor of scale.
 **/
static void input_overlay_set_scale_factor(input_overlay_t *ol, float scale)
{
   size_t i;

   if (!ol)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_scale(&ol->overlays[i], scale);

   input_overlay_set_vertex_geom(ol);
}

void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
      image_texture_free(&overlay->descs[i].image);

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
   image_texture_free(&overlay->image);

   if (overlay_ptr)
      free(overlay_ptr);
   overlay_ptr = NULL;
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   size_t i;

   for (i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);

   if (ol->overlays)
      free(ol->overlays);
   ol->overlays = NULL;
}

static enum overlay_visibility input_overlay_get_visibility(int overlay_idx)
{
    if (!visibility)
       return OVERLAY_VISIBILITY_DEFAULT;
    if ((overlay_idx < 0) || (overlay_idx >= MAX_VISIBILITY))
       return OVERLAY_VISIBILITY_DEFAULT;
    return visibility[overlay_idx];
}


static bool input_overlay_is_hidden(int overlay_idx)
{
    return (input_overlay_get_visibility(overlay_idx)
          == OVERLAY_VISIBILITY_HIDDEN);
}

/**
 * input_overlay_set_alpha_mod:
 * @ol                    : Overlay handle.
 * @mod                   : New modulating factor to apply.
 *
 * Sets a modulating factor for alpha channel. Default is 1.0.
 * The alpha factor is applied for all overlays.
 **/
static void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod)
{
   unsigned i;

   if (!ol)
      return;

   for (i = 0; i < ol->active->load_images_size; i++)
   {
      if (input_overlay_is_hidden(i))
          ol->iface->set_alpha(ol->iface_data, i, 0.0);
      else
          ol->iface->set_alpha(ol->iface_data, i, mod);
   }
}


static void input_overlay_load_active(input_overlay_t *ol, float opacity)
{
   if (ol->iface->load)
      ol->iface->load(ol->iface_data, ol->active->load_images,
            ol->active->load_images_size);

   input_overlay_set_alpha_mod(ol, opacity);
   input_overlay_set_vertex_geom(ol);

   if (ol->iface->full_screen)
      ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
}

/**
 * inside_hitbox:
 * @desc                  : Overlay descriptor handle.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Check whether the given @x and @y coordinates of the overlay
 * descriptor @desc is inside the overlay descriptor's hitbox.
 *
 * Returns: true (1) if X, Y coordinates are inside a hitbox,
 * otherwise false (0).
 **/
static bool inside_hitbox(const struct overlay_desc *desc, float x, float y)
{
   if (!desc)
      return false;

   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         /* Ellipsis. */
         float x_dist  = (x - desc->x) / desc->range_x_mod;
         float y_dist  = (y - desc->y) / desc->range_y_mod;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return (sq_dist <= 1.0f);
      }

      case OVERLAY_HITBOX_RECT:
         return
            (fabs(x - desc->x) <= desc->range_x_mod) &&
            (fabs(y - desc->y) <= desc->range_y_mod);
   }

   return false;
}

/**
 * input_overlay_poll:
 * @out                   : Polled output data.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 *
 * Polls input overlay.
 *
 * @norm_x and @norm_y are the result of
 * input_translate_coord_viewport().
 **/
static void input_overlay_poll(
      input_overlay_t *ol,
      input_overlay_state_t *out,
      int16_t norm_x, int16_t norm_y)
{
   size_t i;

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   for (i = 0; i < ol->active->size; i++)
   {
      float x_dist, y_dist;
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!inside_hitbox(desc, x, y))
         continue;

      desc->updated = true;
      x_dist        = x - desc->x;
      y_dist        = y - desc->y;

      switch (desc->type)
      {
         case OVERLAY_TYPE_BUTTONS:
            {
               bits_or_bits(out->buttons.data,
                     desc->button_mask.data,
                     ARRAY_SIZE(desc->button_mask.data));

               if (BIT256_GET(desc->button_mask, RARCH_OVERLAY_NEXT))
                  ol->next_index = desc->next_index;
            }
            break;
         case OVERLAY_TYPE_KEYBOARD:
            if (desc->retro_key_idx < RETROK_LAST)
               OVERLAY_SET_KEY(out, desc->retro_key_idx);
            break;
         default:
            {
               float x_val           = x_dist / desc->range_x;
               float y_val           = y_dist / desc->range_y;
               float x_val_sat       = x_val / desc->analog_saturate_pct;
               float y_val_sat       = y_val / desc->analog_saturate_pct;

               unsigned int base     =
                  (desc->type == OVERLAY_TYPE_ANALOG_RIGHT)
                  ? 2 : 0;

               out->analog[base + 0] = clamp_float(x_val_sat, -1.0f, 1.0f)
                  * 32767.0f;
               out->analog[base + 1] = clamp_float(y_val_sat, -1.0f, 1.0f)
                  * 32767.0f;
            }
            break;
      }

      if (desc->movable)
      {
         desc->delta_x = clamp_float(x_dist, -desc->range_x, desc->range_x)
            * ol->active->mod_w;
         desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
            * ol->active->mod_h;
      }
   }

   if (!bits_any_set(out->buttons.data, ARRAY_SIZE(out->buttons.data)))
      ol->blocked = false;
   else if (ol->blocked)
      memset(out, 0, sizeof(*out));
}

/**
 * input_overlay_update_desc_geom:
 * @ol                    : overlay handle.
 * @desc                  : overlay descriptors handle.
 *
 * Update input overlay descriptors' vertex geometry.
 **/
static void input_overlay_update_desc_geom(input_overlay_t *ol,
      struct overlay_desc *desc)
{
   if (!desc->image.pixels || !desc->movable)
      return;

   if (ol->iface->vertex_geom)
      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
            desc->mod_w, desc->mod_h);

   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
}

/**
 * input_overlay_post_poll:
 *
 * Called after all the input_overlay_poll() calls to
 * update the range modifiers for pressed/unpressed regions
 * and alpha mods.
 **/
static void input_overlay_post_poll(input_overlay_t *ol, float opacity)
{
   size_t i;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;

      if (desc->updated)
      {
         /* If pressed this frame, change the hitbox. */
         desc->range_x_mod *= desc->range_mod;
         desc->range_y_mod *= desc->range_mod;

         if (desc->image.pixels)
         {
            if (ol->iface->set_alpha)
               ol->iface->set_alpha(ol->iface_data, desc->image_index,
                     desc->alpha_mod * opacity);
         }
      }

      input_overlay_update_desc_geom(ol, desc);
      desc->updated = false;
   }
}

/**
 * input_overlay_poll_clear:
 * @ol                    : overlay handle
 *
 * Call when there is nothing to poll. Allows overlay to
 * clear certain state.
 **/
static void input_overlay_poll_clear(input_overlay_t *ol, float opacity)
{
   size_t i;

   ol->blocked = false;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;
      desc->updated     = false;

      desc->delta_x     = 0.0f;
      desc->delta_y     = 0.0f;
      input_overlay_update_desc_geom(ol, desc);
   }
}


/**
 * input_overlay_free:
 * @ol                    : Overlay handle.
 *
 * Frees overlay handle.
 **/
static void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;
   overlay_ptr = NULL;

   input_overlay_free_overlays(ol);

   if (ol->iface->enable)
      ol->iface->enable(ol->iface_data, false);

   free(ol);
}

/* task_data = overlay_task_data_t* */
void input_overlay_loaded(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   size_t i;
   overlay_task_data_t              *data = (overlay_task_data_t*)task_data;
   input_overlay_t                    *ol = NULL;
   const video_overlay_interface_t *iface = NULL;
   settings_t *settings                   = configuration_settings;

   if (err)
      return;

#ifdef HAVE_MENU
   /* We can't display when the menu is up */
   if (data->hide_in_menu && menu_driver_is_alive())
   {
      if (data->overlay_enable)
         goto abort_load;
   }
#endif

   if (  !data->overlay_enable                   ||
         !video_driver_overlay_interface(&iface) ||
         !iface)
   {
      RARCH_ERR("Overlay interface is not present in video driver,"
            " or not enabled.\n");
      goto abort_load;
   }

   ol             = (input_overlay_t*)calloc(1, sizeof(*ol));
   ol->overlays   = data->overlays;
   ol->size       = data->size;
   ol->active     = data->active;
   ol->iface      = iface;
   ol->iface_data = video_driver_get_ptr_internal(true);

   input_overlay_load_active(ol, data->overlay_opacity);

   /* Enable or disable the overlay. */
   ol->enable = data->overlay_enable;

   if (ol->iface->enable)
      ol->iface->enable(ol->iface_data, data->overlay_enable);

   input_overlay_set_scale_factor(ol, data->overlay_scale);

   ol->next_index = (unsigned)((ol->index + 1) % ol->size);
   ol->state      = OVERLAY_STATUS_NONE;
   ol->alive      = true;
   overlay_ptr    = ol;

   free(data);

   if (!settings->bools.input_overlay_show_mouse_cursor)
      video_driver_hide_mouse();

   return;

abort_load:
   for (i = 0; i < data->size; i++)
      input_overlay_free_overlay(&data->overlays[i]);

   free(data->overlays);
   free(data);
}

void input_overlay_set_visibility(int overlay_idx,
      enum overlay_visibility vis)
{
    input_overlay_t *ol = overlay_ptr;

    if (!visibility)
    {
       unsigned i;
       visibility = (enum overlay_visibility *)calloc(
             MAX_VISIBILITY, sizeof(enum overlay_visibility));

       for (i = 0; i < MAX_VISIBILITY; i++)
          visibility[i] = OVERLAY_VISIBILITY_DEFAULT;
    }

    visibility[overlay_idx] = vis;

    if (!ol)
       return;
    if (vis == OVERLAY_VISIBILITY_HIDDEN)
      ol->iface->set_alpha(ol->iface_data, overlay_idx, 0.0);
}

bool input_overlay_key_pressed(input_overlay_t *ol, unsigned key)
{
   input_overlay_state_t *ol_state  = ol ? &ol->overlay_state : NULL;
   if (!ol)
      return false;
   return (BIT256_GET(ol_state->buttons, key));
}

/*
 * input_poll_overlay:
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
static void input_poll_overlay(input_overlay_t *ol, float opacity,
      unsigned analog_dpad_mode,
      float axis_threshold)
{
   rarch_joypad_info_t joypad_info;
   input_overlay_state_t old_key_state;
   unsigned i, j, device;
   settings_t *settings            = configuration_settings;
   uint16_t key_mod                = 0;
   bool polled                     = false;
   bool button_pressed             = false;
   void *input_data                = current_input_data;
   input_overlay_state_t *ol_state = &ol->overlay_state;
   const input_driver_t *input_ptr = current_input;

   if (!ol_state)
      return;

   joypad_info.joy_idx             = 0;
   joypad_info.auto_binds          = NULL;
   joypad_info.axis_threshold      = 0.0f;

   memcpy(old_key_state.keys, ol_state->keys,
         sizeof(ol_state->keys));
   memset(ol_state, 0, sizeof(*ol_state));

   device = ol->active->full_screen ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   for (i = 0;
         input_ptr->input_state(input_data, joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      input_overlay_state_t polled_data;
      int16_t x = input_ptr->input_state(input_data, joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = input_ptr->input_state(input_data, joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_Y);

      memset(&polled_data, 0, sizeof(struct input_overlay_state));

      if (ol->enable)
         input_overlay_poll(ol, &polled_data, x, y);
      else
         ol->blocked = false;

      bits_or_bits(ol_state->buttons.data,
            polled_data.buttons.data,
            ARRAY_SIZE(polled_data.buttons.data));

      for (j = 0; j < ARRAY_SIZE(ol_state->keys); j++)
         ol_state->keys[j] |= polled_data.keys[j];

      /* Fingers pressed later take priority and matched up
       * with overlay poll priorities. */
      for (j = 0; j < 4; j++)
         if (polled_data.analog[j])
            ol_state->analog[j] = polled_data.analog[j];

      polled = true;
   }

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LCTRL) ||
       OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LMETA) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = 0; i < ARRAY_SIZE(ol_state->keys); i++)
   {
      if (ol_state->keys[i] != old_key_state.keys[i])
      {
         uint32_t orig_bits = old_key_state.keys[i];
         uint32_t new_bits  = ol_state->keys[i];

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
               input_keyboard_event(new_bits & (1 << j),
                     i * 32 + j, 0, key_mod, RETRO_DEVICE_POINTER);
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (ol_state->analog[j])
         continue;

      if (input_overlay_key_pressed(ol, bind_plus))
         ol_state->analog[j] += 0x7fff;
      if (input_overlay_key_pressed(ol, bind_minus))
         ol_state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (analog_dpad_mode == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)ol_state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)ol_state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      default:
         break;
   }

   if (settings->bools.input_overlay_show_physical_inputs)
      button_pressed = input_overlay_add_inputs(ol,
            settings->uints.input_overlay_show_physical_inputs_port,
            analog_dpad_mode);

   if (button_pressed || polled)
      input_overlay_post_poll(ol, opacity);
   else
      input_overlay_poll_clear(ol, opacity);
}

/**
 * retroarch_overlay_next:
 * @ol                    : Overlay handle.
 *
 * Switch to the next available overlay
 * screen.
 **/
void retroarch_overlay_next(void)
{
   settings_t *settings      = configuration_settings;
   float opacity             = settings->floats.input_overlay_opacity;

   if (!overlay_ptr)
      return;

   overlay_ptr->index      = overlay_ptr->next_index;
   overlay_ptr->active     = &overlay_ptr->overlays[overlay_ptr->index];

   input_overlay_load_active(overlay_ptr, opacity);

   overlay_ptr->blocked    = true;
   overlay_ptr->next_index = (unsigned)((overlay_ptr->index + 1) % overlay_ptr->size);
}

void retroarch_overlay_set_scale_factor(void)
{
   settings_t *settings      = configuration_settings;
   input_overlay_set_scale_factor(overlay_ptr, settings->floats.input_overlay_scale);
}

void retroarch_overlay_set_alpha_mod(void)
{
#ifdef HAVE_OVERLAY
   settings_t *settings      = configuration_settings;
   input_overlay_set_alpha_mod(overlay_ptr, settings->floats.input_overlay_opacity);
#endif
}

void retroarch_overlay_deinit(void)
{
   input_overlay_free(overlay_ptr);
   overlay_ptr = NULL;
}

void retroarch_overlay_init(void)
{
   settings_t *settings      = configuration_settings;
#if defined(GEKKO)
   /* Avoid a crash at startup or even when toggling overlay in rgui */
   uint64_t memory_used       = frontend_driver_get_used_memory();
   if(memory_used > (72 * 1024 * 1024))
      return;
#endif

   retroarch_overlay_deinit();

   if (settings->bools.input_overlay_enable)
      task_push_overlay_load_default(input_overlay_loaded,
            settings->paths.path_overlay,
            settings->bools.input_overlay_hide_in_menu,
            settings->bools.input_overlay_enable,
            settings->floats.input_overlay_opacity,
            settings->floats.input_overlay_scale,
            NULL);
}
#endif

/* INPUT REMOTE */

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define input_remote_key_pressed(key, port) (remote_st_ptr.buttons[(port)] & (UINT64_C(1) << (key)))

static bool input_remote_init_network(input_remote_t *handle,
      uint16_t port, unsigned user)
{
   int fd;
   struct addrinfo *res  = NULL;

   port = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

   fd = socket_init((void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   handle->net_fd[user] = fd;

   if (!socket_nonblock(handle->net_fd[user]))
      goto error;

   if (!socket_bind(handle->net_fd[user], res))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
      goto error;
   }

   freeaddrinfo_retro(res);
   return true;

error:
   if (res)
      freeaddrinfo_retro(res);
   return false;
}

static void input_remote_free(input_remote_t *handle, unsigned max_users)
{
   unsigned user;
   for(user = 0; user < max_users; user ++)
      socket_close(handle->net_fd[user]);

   free(handle);
}

static input_remote_t *input_remote_new(uint16_t port, unsigned max_users)
{
   unsigned user;
   settings_t   *settings = configuration_settings;
   input_remote_t *handle = (input_remote_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   for(user = 0; user < max_users; user ++)
   {
      handle->net_fd[user] = -1;
      if (settings->bools.network_remote_enable_user[user])
         if (!input_remote_init_network(handle, port, user))
         {
            input_remote_free(handle, max_users);
            return NULL;
         }
   }

   return handle;
}

static void input_remote_parse_packet(struct remote_message *msg, unsigned user)
{
   input_remote_state_t *input_state  = &remote_st_ptr;

   /* Parse message */
   switch (msg->device)
   {
      case RETRO_DEVICE_JOYPAD:
         input_state->buttons[user] &= ~(1 << msg->id);
         if (msg->state)
            input_state->buttons[user] |= 1 << msg->id;
         break;
      case RETRO_DEVICE_ANALOG:
         input_state->analog[msg->index * 2 + msg->id][user] = msg->state;
         break;
   }
}
#endif

/* INPUT */

void set_connection_listener(pad_connection_listener_t *listener)
{
   pad_connection_listener = listener;
}

void fire_connection_listener(unsigned port, input_device_driver_t *driver)
{
   if (!pad_connection_listener)
      return;

   pad_connection_listener->connected(port, driver);
}


/**
 * check_input_driver_block_hotkey:
 *
 * Checks if 'hotkey enable' key is pressed.
 *
 * If we haven't bound anything to this,
 * always allow hotkeys.

 * If we hold ENABLE_HOTKEY button, block all libretro input to allow
 * hotkeys to be bound to same keys as RetroPad.
 **/
#define check_input_driver_block_hotkey(normal_bind, autoconf_bind) \
( \
         (((normal_bind)->key      != RETROK_UNKNOWN) \
      || ((normal_bind)->mbutton   != NO_BTN) \
      || ((normal_bind)->joykey    != NO_BTN) \
      || ((normal_bind)->joyaxis   != AXIS_NONE) \
      || ((autoconf_bind)->key     != RETROK_UNKNOWN ) \
      || ((autoconf_bind)->joykey  != NO_BTN) \
      || ((autoconf_bind)->joyaxis != AXIS_NONE)) \
)

/**
 * input_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to input driver at index. Can be NULL
 * if nothing found.
 **/
const void *input_driver_find_handle(int idx)
{
   const void *drv = input_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * input_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of input driver at index. Can be NULL
 * if nothing found.
 **/
const char *input_driver_find_ident(int idx)
{
   const input_driver_t *drv = input_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_input_driver_options:
 *
 * Get an enumerated list of all input driver names, separated by '|'.
 *
 * Returns: string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_DRIVERS, NULL);
}

void *input_get_data(void)
{
   return current_input_data;
}

/**
 * input_driver_set_rumble_state:
 * @port               : User number.
 * @effect             : Rumble effect.
 * @strength           : Strength of rumble effect.
 *
 * Sets the rumble state.
 * Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE.
 **/
bool input_driver_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   if (!current_input || !current_input->set_rumble)
      return false;
   return current_input->set_rumble(current_input_data,
         port, effect, strength);
}

const input_device_driver_t *input_driver_get_joypad_driver(void)
{
   if (!current_input || !current_input->get_joypad_driver)
      return NULL;
   return current_input->get_joypad_driver(current_input_data);
}

const input_device_driver_t *input_driver_get_sec_joypad_driver(void)
{
   if (!current_input || !current_input->get_sec_joypad_driver)
      return NULL;
   return current_input->get_sec_joypad_driver(current_input_data);
}

uint64_t input_driver_get_capabilities(void)
{
   if (!current_input || !current_input->get_capabilities)
      return 0;
   return current_input->get_capabilities(current_input_data);
}

void input_driver_keyboard_mapping_set_block(bool value)
{
   if (current_input->keyboard_mapping_set_block)
      current_input->keyboard_mapping_set_block(current_input_data, value);
}

/**
 * input_sensor_set_state:
 * @port               : User number.
 * @effect             : Sensor action.
 * @rate               : Sensor rate update.
 *
 * Sets the sensor state.
 * Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE.
 **/
bool input_sensor_set_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   if (current_input_data &&
         current_input->set_sensor_state)
      return current_input->set_sensor_state(current_input_data,
            port, action, rate);
   return false;
}

float input_sensor_get_input(unsigned port, unsigned id)
{
   if (current_input_data &&
         current_input->get_sensor_input)
      return current_input->get_sensor_input(current_input_data,
            port, id);
   return 0.0f;
}

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
static void input_driver_poll(void)
{
   size_t i;
   settings_t *settings           = configuration_settings;
   uint8_t max_users              = (uint8_t)input_driver_max_users;

   current_input->poll(current_input_data);

   input_driver_turbo_btns.count++;

   for (i = 0; i < max_users; i++)
      input_driver_turbo_btns.frame_enable[i] = 0;

   if (input_driver_block_libretro_input)
      return;

   for (i = 0; i < max_users; i++)
   {
      rarch_joypad_info_t joypad_info;

      if (!libretro_input_binds[i][RARCH_TURBO_ENABLE].valid)
         continue;

      joypad_info.axis_threshold              = input_driver_axis_threshold;
      joypad_info.joy_idx                     = settings->uints.input_joypad_map[i];
      joypad_info.auto_binds                  = input_autoconf_binds[joypad_info.joy_idx];

      input_driver_turbo_btns.frame_enable[i] = current_input->input_state(
            current_input_data, joypad_info, libretro_input_binds,
            (unsigned)i, RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
   }

#ifdef HAVE_OVERLAY
   if (overlay_ptr && overlay_ptr->alive)
      input_poll_overlay(
            overlay_ptr,
            settings->floats.input_overlay_opacity,
            settings->uints.input_analog_dpad_mode[0],
            input_driver_axis_threshold);
#endif

   if (settings->bools.input_remap_binds_enable && input_driver_mapper)
   {
#ifdef HAVE_OVERLAY
      bool overlay_is_alive = (overlay_ptr && overlay_ptr->alive);
#else
      bool overlay_is_alive = false;
#endif
#ifdef HAVE_MENU
      bool do_poll          = menu_driver_is_alive() ? false : true;
#else
      bool do_poll          = true;
#endif

      if (do_poll)
         input_mapper_poll(input_driver_mapper, 
#ifdef HAVE_OVERLAY
               overlay_ptr,
#else
               NULL,
#endif
               settings,
               max_users,
               overlay_is_alive
               );
   }

#ifdef HAVE_COMMAND
   if (input_driver_command)
      command_poll(input_driver_command);
#endif

#ifdef HAVE_NETWORKGAMEPAD
   /* Poll remote */
   if (input_driver_remote)
   {
      unsigned user;

      for(user = 0; user < max_users; user++)
      {
         if (settings->bools.network_remote_enable_user[user])
         {
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
            struct remote_message msg;
            ssize_t ret;
            fd_set fds;

            if (input_driver_remote->net_fd[user] < 0)
               return;

            FD_ZERO(&fds);
            FD_SET(input_driver_remote->net_fd[user], &fds);

            ret = recvfrom(input_driver_remote->net_fd[user], (char*)&msg,
                  sizeof(msg), 0, NULL, NULL);

            if (ret == sizeof(msg))
               input_remote_parse_packet(&msg, user);
            else if ((ret != -1) || ((errno != EAGAIN) && (errno != ENOENT)))
#endif
            {
               input_remote_state_t *input_state  = &remote_st_ptr;
               input_state->buttons[user]   = 0;
               input_state->analog[0][user] = 0;
               input_state->analog[1][user] = 0;
               input_state->analog[2][user] = 0;
               input_state->analog[3][user] = 0;
            }
         }
      }
   }
#endif
}

static int16_t input_state_device(
      int16_t ret,
      unsigned port, unsigned device,
      unsigned idx, unsigned id,
      bool button_mask)
{
   int16_t res          = 0;
   settings_t *settings = configuration_settings;
#ifdef HAVE_OVERLAY
   int16_t res_overlay  = 0;

   if (overlay_ptr && port == 0)
   {
      input_overlay_state_t *ol_state = &overlay_ptr->overlay_state;

      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            if (input_overlay_key_pressed(overlay_ptr, id))
               res_overlay |= 1;
            break;
         case RETRO_DEVICE_KEYBOARD:
            if (id < RETROK_LAST)
            {
#if 0
               RARCH_LOG("UDLR %u %u %u %u\n",
                     OVERLAY_GET_KEY(ol_state, RETROK_UP),
                     OVERLAY_GET_KEY(ol_state, RETROK_DOWN),
                     OVERLAY_GET_KEY(ol_state, RETROK_LEFT),
                     OVERLAY_GET_KEY(ol_state, RETROK_RIGHT)
                     );
#endif
               if (OVERLAY_GET_KEY(ol_state, id))
                  res_overlay |= 1;
            }
            break;
         case RETRO_DEVICE_ANALOG:
            {
               unsigned base = 0;

               if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                  base = 2;
               if (id == RETRO_DEVICE_ID_ANALOG_Y)
                  base += 1;
               if (ol_state && ol_state->analog[base])
                  res_overlay = ol_state->analog[base];
            }
            break;
      }
   }
#endif

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         {
#ifdef HAVE_NETWORKGAMEPAD
            if (input_driver_remote)
               if (input_remote_key_pressed(id, port))
                  res |= 1;
#endif

            if (id < RARCH_FIRST_META_KEY)
            {
               bool bind_valid = libretro_input_binds[port] 
                  && libretro_input_binds[port][id].valid;

               if (settings->bools.input_remap_binds_enable &&
                     id != settings->uints.input_remap_ids[port][id])
                  res = 0;
               else if (bind_valid)
               {
                  if (button_mask)
                  {
                     res = 0;
                     if (ret & (1 << id))
                        res |= (1 << id);
                  }
                  else
                     res = ret;

#ifdef HAVE_OVERLAY
                  if (overlay_ptr && overlay_ptr->alive && port == 0)
                     res |= res_overlay;
#endif
               }
            }

            if (settings->bools.input_remap_binds_enable && input_driver_mapper)
               input_mapper_state(input_driver_mapper,
                     &res, port, device, idx, id);

            /* Don't allow turbo for D-pad. */
            if ((id < RETRO_DEVICE_ID_JOYPAD_UP || id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
            {
               /*
                * Apply turbo button if activated.
                *
                * If turbo button is held, all buttons pressed except
                * for D-pad will go into a turbo mode. Until the button is
                * released again, the input state will be modulated by a
                * periodic pulse defined by the configured duty cycle.
                */
               if (res && input_driver_turbo_btns.frame_enable[port])
                  input_driver_turbo_btns.enable[port] |= (1 << id);
               else if (!res)
                  input_driver_turbo_btns.enable[port] &= ~(1 << id);

               if (input_driver_turbo_btns.enable[port] & (1 << id))
               {
                  /* if turbo button is enabled for this key ID */
                  res = res && ((input_driver_turbo_btns.count
                           % settings->uints.input_turbo_period)
                        < settings->uints.input_turbo_duty_cycle);
               }
            }
         }

         break;

      case RETRO_DEVICE_MOUSE:

         {
            if (id < RARCH_FIRST_META_KEY)
            {
               bool bind_valid = libretro_input_binds[port] 
                  && libretro_input_binds[port][id].valid;

               if (bind_valid)
               {
                  if (button_mask)
                  {
                     res = 0;
                     if (ret & (1 << id))
                        res |= (1 << id);
                  }
                  else
                     res = ret;

#ifdef HAVE_OVERLAY
                  if (overlay_ptr && overlay_ptr->alive && port == 0)
                     res |= res_overlay;
#endif
               }
            }

            if (settings->bools.input_remap_binds_enable && input_driver_mapper)
               input_mapper_state(input_driver_mapper,
                     &res, port, device, idx, id);
         }
         break;

      case RETRO_DEVICE_KEYBOARD:

         {
            res = ret;

#ifdef HAVE_OVERLAY
            if (overlay_ptr && overlay_ptr->alive && port == 0)
               res |= res_overlay;
#endif

            if (settings->bools.input_remap_binds_enable && input_driver_mapper)
               input_mapper_state(input_driver_mapper,
                     &res, port, device, idx, id);
         }
         break;

      case RETRO_DEVICE_LIGHTGUN:

         {
            if (id < RARCH_FIRST_META_KEY)
            {
               bool bind_valid = libretro_input_binds[port] 
                  && libretro_input_binds[port][id].valid;

               if (bind_valid)
               {
                  if (button_mask)
                  {
                     res = 0;
                     if (ret & (1 << id))
                        res |= (1 << id);
                  }
                  else
                     res = ret;

#ifdef HAVE_OVERLAY
                  if (overlay_ptr && overlay_ptr->alive && port == 0)
                     res |= res_overlay;
#endif
               }
            }

            if (settings->bools.input_remap_binds_enable && input_driver_mapper)
               input_mapper_state(input_driver_mapper,
                     &res, port, device, idx, id);
         }
         break;

      case RETRO_DEVICE_ANALOG:

         {
#ifdef HAVE_NETWORKGAMEPAD
            if (input_driver_remote)
            {
               input_remote_state_t *input_state  = &remote_st_ptr;

               if (input_state)
               {
                  unsigned base = 0;
                  if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                     base = 2;
                  if (id == RETRO_DEVICE_ID_ANALOG_Y)
                     base += 1;
                  if (input_state->analog[base][port])
                     res = input_state->analog[base][port];
               }
            }
#endif

            if (id < RARCH_FIRST_META_KEY)
            {
               bool bind_valid = libretro_input_binds[port] 
                  && libretro_input_binds[port][id].valid;

               if (bind_valid)
               {
                  /* reset_state - used to reset input state of a button 
                   * when the gamepad mapper is in action for that button*/
                  bool reset_state     = false;
                  if (settings->bools.input_remap_binds_enable)
                  {
                     if (idx < 2 && id < 2)
                     {
                        unsigned offset = RARCH_FIRST_CUSTOM_BIND + 
                           (idx * 4) + (id * 2);

                        if (settings->uints.input_remap_ids
                              [port][offset]   != offset)
                           reset_state = true;
                        else if (settings->uints.input_remap_ids
                              [port][offset+1] != (offset+1))
                           reset_state = true;
                     }
                  }

                  if (!reset_state)
                  {
                     res = ret;

#ifdef HAVE_OVERLAY
                     if (overlay_ptr && overlay_ptr->alive && port == 0)
                        res |= res_overlay;
#endif
                  }
                  else
                     res = 0;
               }
            }

            if (settings->bools.input_remap_binds_enable && input_driver_mapper)
               input_mapper_state(input_driver_mapper,
                     &res, port, device, idx, id);
         }
         break;

      case RETRO_DEVICE_POINTER:

         {
            if (id < RARCH_FIRST_META_KEY)
            {
               bool bind_valid = libretro_input_binds[port] 
                  && libretro_input_binds[port][id].valid;

               if (bind_valid)
               {
                  if (button_mask)
                  {
                     res = 0;
                     if (ret & (1 << id))
                        res |= (1 << id);
                  }
                  else
                     res = ret;

#ifdef HAVE_OVERLAY
                  if (overlay_ptr && overlay_ptr->alive && port == 0)
                     res |= res_overlay;
#endif
               }
            }

            if (settings->bools.input_remap_binds_enable && input_driver_mapper)
               input_mapper_state(input_driver_mapper,
                     &res, port, device, idx, id);
         }
         break;
   }

   return res;
}

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id)
 * was pressed by the user (assigned to @port).
 **/
static int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   int16_t result             = 0;
   int16_t ret                = 0;
   joypad_info.axis_threshold = input_driver_axis_threshold;
   joypad_info.joy_idx        = configuration_settings->uints.input_joypad_map[port];
   joypad_info.auto_binds     = input_autoconf_binds[joypad_info.joy_idx];

   if (BSV_MOVIE_IS_PLAYBACK_ON())
   {
      int16_t bsv_result;
      if (intfstream_read(bsv_movie_state_handle->file, &bsv_result, 1) == 1)
         return swap_if_big16(bsv_result);
      bsv_movie_state.movie_end = true;
   }

   device &= RETRO_DEVICE_MASK;
   ret     = current_input->input_state(
         current_input_data, joypad_info, libretro_input_binds, port, device, idx, id);

   if (  (device == RETRO_DEVICE_JOYPAD) &&
         (id == RETRO_DEVICE_ID_JOYPAD_MASK))
   {
      unsigned i;
      int16_t res = 0;

      if (     !input_driver_flushing_input
            && !input_driver_block_libretro_input)
      {
         for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            if (input_state_device(ret, port, device, idx, i, true))
               res |= (1 << i);
      }
      if (BSV_MOVIE_IS_PLAYBACK_OFF())
      {
         res = swap_if_big16(res);
         intfstream_write(bsv_movie_state_handle->file, &res, 1);
      }
      return res;
   }

   if (     !input_driver_flushing_input
         && !input_driver_block_libretro_input)
      result = input_state_device(ret, port, device, idx, id, false);

   if (BSV_MOVIE_IS_PLAYBACK_OFF())
   {
      result = swap_if_big16(result);
      intfstream_write(bsv_movie_state_handle->file, &result, 1);
   }

   return result;
}

static INLINE bool input_keys_pressed_other_sources(unsigned i,
      input_bits_t* p_new_state)
{
#ifdef HAVE_OVERLAY
   if (overlay_ptr &&
         input_overlay_key_pressed(overlay_ptr, i))
      return true;
#endif

#ifdef HAVE_COMMAND
   if (input_driver_command)
   {
      command_handle_t handle;

      handle.handle = input_driver_command;
      handle.id     = i;

      if (command_get(&handle))
         return true;
   }
#endif

#ifdef HAVE_NETWORKGAMEPAD
   if (input_driver_remote &&
         input_remote_key_pressed(i, 0))
      return true;
#endif

   return false;
}

static int16_t input_joypad_axis(const input_device_driver_t *drv,
      unsigned port, uint32_t joyaxis)
{
   settings_t *settings           = configuration_settings;
   float input_analog_deadzone    = settings->floats.input_analog_deadzone;
   float input_analog_sensitivity = settings->floats.input_analog_sensitivity;
   int16_t val                    = drv->axis(port, joyaxis);

   if (input_analog_deadzone)
   {
      int16_t x, y;
      float normalized;
      float normal_mag;
      /* 2/3 are the right analog X/Y axes */
      unsigned x_axis      = 2;
      unsigned y_axis      = 3;

      /* 0/1 are the left analog X/Y axes */
      if (AXIS_POS_GET(joyaxis) == AXIS_DIR_NONE)
      {
         /* current axis is negative         */
         /* current stick is the left        */
         if (AXIS_NEG_GET(joyaxis) < 2)
         {
            x_axis = 0;
            y_axis = 1;
         }
      }
      else
      {
         /* current axis is positive */
         /* current stick is the left */
         if (AXIS_POS_GET(joyaxis) < 2)
         {
            x_axis = 0;
            y_axis = 1;
         }
      }

      x                = drv->axis(port, AXIS_POS(x_axis))
         + drv->axis(port, AXIS_NEG(x_axis));
      y                = drv->axis(port, AXIS_POS(y_axis))
         + drv->axis(port, AXIS_NEG(y_axis));
      normal_mag       = (1.0f / 0x7fff) * sqrt(x * x + y * y);

      /* if analog value is below the deadzone, ignore it */
      if (normal_mag <= input_analog_deadzone)
         return 0;

      normalized = (1.0f / 0x7fff) * val;

      /* now scale the "good" analog range appropriately, 
       * so we don't start out way above 0 */
      val = 0x7fff * normalized * MIN(1.0f, 
         ((normal_mag - input_analog_deadzone) 
          / (1.0f - input_analog_deadzone)));
   }

   if (input_analog_sensitivity != 1.0f)
   {
      float normalized = (1.0f / 0x7fff) * val;
      int      new_val = 0x7fff * normalized  * 
         input_analog_sensitivity;

      if (new_val > 0x7fff)
         return 0x7fff;
      else if (new_val < -0x7fff)
         return -0x7fff;

      return new_val;
   }

   return val;
}

#define inherit_joyaxis(binds) (((binds)[x_plus].joyaxis == (binds)[x_minus].joyaxis) || (  (binds)[y_plus].joyaxis == (binds)[y_minus].joyaxis))

/**
 * input_pop_analog_dpad:
 * @binds                          : Binds to modify.
 *
 * Restores binds temporarily overridden by input_push_analog_dpad().
 **/
#define input_pop_analog_dpad(binds) \
{ \
   unsigned j; \
   for (j = RETRO_DEVICE_ID_JOYPAD_UP; j <= RETRO_DEVICE_ID_JOYPAD_RIGHT; j++) \
      (binds)[j].joyaxis = (binds)[j].orig_joyaxis; \
}

/**
 * input_push_analog_dpad:
 * @binds                          : Binds to modify.
 * @mode                           : Which analog stick to bind D-Pad to.
 *                                   E.g:
 *                                   ANALOG_DPAD_LSTICK
 *                                   ANALOG_DPAD_RSTICK
 *
 * Push analog to D-Pad mappings to binds.
 **/
#define input_push_analog_dpad(binds, mode) \
{ \
   unsigned k; \
   unsigned x_plus      =  RARCH_ANALOG_RIGHT_X_PLUS; \
   unsigned y_plus      =  RARCH_ANALOG_RIGHT_Y_PLUS; \
   unsigned x_minus     =  RARCH_ANALOG_RIGHT_X_MINUS; \
   unsigned y_minus     =  RARCH_ANALOG_RIGHT_Y_MINUS; \
   if ((mode) == ANALOG_DPAD_LSTICK) \
   { \
      x_plus            =  RARCH_ANALOG_LEFT_X_PLUS; \
      y_plus            =  RARCH_ANALOG_LEFT_Y_PLUS; \
      x_minus           =  RARCH_ANALOG_LEFT_X_MINUS; \
      y_minus           =  RARCH_ANALOG_LEFT_Y_MINUS; \
   } \
   for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++) \
      (binds)[k].orig_joyaxis = (binds)[k].joyaxis; \
   if (!inherit_joyaxis(binds)) \
   { \
      unsigned j = x_plus + 3; \
      /* Inherit joyaxis from analogs. */ \
      for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++) \
         (binds)[k].joyaxis = (binds)[j--].joyaxis; \
   } \
}

/* MENU INPUT */
#ifdef HAVE_MENU
/* Set a specific keyboard key.
 *
 * 'down' sets the latch (true would
 * mean the key is being pressed down, while 'false' would mean that
 * the key has been released).
 **/
void menu_event_kb_set(bool down, enum retro_key key)
{
   if (key == RETROK_UNKNOWN)
   {
      unsigned i;

      for (i = 0; i < RETROK_LAST; i++)
         menu_keyboard_key_state[i] = (menu_keyboard_key_state[(enum retro_key)i] & 1) << 1;
   }
   else
      menu_keyboard_key_state[key] = ((menu_keyboard_key_state[key] & 1) << 1) | down;
}

/*
 * This function gets called in order to process all input events
 * for the current frame.
 *
 * Sends input code to menu for one frame.
 *
 * It uses as input the local variables' input' and 'trigger_input'.
 *
 * Mouse and touch input events get processed inside this function.
 *
 * NOTE: 'input' and 'trigger_input' is sourced from the keyboard and/or
 * the gamepad. It does not contain input state derived from the mouse
 * and/or touch - this gets dealt with separately within this function.
 *
 * TODO/FIXME - maybe needs to be overhauled so we can send multiple
 * events per frame if we want to, and we shouldn't send the
 * entire button state either but do a separate event per button
 * state.
 */
static unsigned menu_event(input_bits_t *p_input, input_bits_t *p_trigger_input)
{
   /* Used for key repeat */
   static float delay_timer                = 0.0f;
   static float delay_count                = 0.0f;
   static bool initial_held                = true;
   static bool first_held                  = false;
   static unsigned ok_old                  = 0;
   unsigned ret                            = MENU_ACTION_NOOP;
   bool set_scroll                         = false;
   bool mouse_enabled                      = false;
   size_t new_scroll_accel                 = 0;
   menu_input_t *menu_input                = &menu_input_state;
   settings_t *settings                    = configuration_settings;
   bool swap_ok_cancel_btns                = settings->bools.input_menu_swap_ok_cancel_buttons;
   bool input_swap_override                = input_autoconfigure_get_swap_override();
   unsigned menu_ok_btn                    = (!input_swap_override &&
      swap_ok_cancel_btns) ?
      RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A;
   unsigned menu_cancel_btn                = (!input_swap_override &&
      swap_ok_cancel_btns) ?
      RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B;
   unsigned ok_current                     = BIT256_GET_PTR(p_input, menu_ok_btn);
   unsigned ok_trigger                     = ok_current & ~ok_old;
   bool is_rgui                            = string_is_equal(settings->arrays.menu_driver, "rgui");

   ok_old                                  = ok_current;

   if (bits_any_set(p_input->data, ARRAY_SIZE(p_input->data)))
   {
      if (!first_held)
      {
         /* don't run anything first frame, only capture held inputs
          * for old_input_state. */

         first_held  = true;
         delay_timer = initial_held ? 200 : 100;
         delay_count = 0;
      }

      if (delay_count >= delay_timer)
      {
         uint32_t input_repeat = 0;
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_UP);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_DOWN);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_LEFT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_L);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_R);

         set_scroll           = true;
         first_held           = false;
         p_trigger_input->data[0] |= p_input->data[0] & input_repeat;

         menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL,
               &new_scroll_accel);

         new_scroll_accel = MIN(new_scroll_accel + 1, 64);
      }

      initial_held  = false;
   }
   else
   {
      set_scroll   = true;
      first_held   = false;
      initial_held = true;
   }

   if (set_scroll)
      menu_driver_ctl(MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL,
            &new_scroll_accel);

   delay_count += menu_animation_get_delta_time();

   if (menu_input_dialog_get_display_kb())
   {
      menu_event_osk_iterate();

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         if (menu_event_get_osk_ptr() < 33)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr()
                  + OSK_CHARS_PER_LINE);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_UP))
      {
         if (menu_event_get_osk_ptr() >= OSK_CHARS_PER_LINE)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr()
                  - OSK_CHARS_PER_LINE);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      {
         if (menu_event_get_osk_ptr() < 43)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr() + 1);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_LEFT))
      {
         if (menu_event_get_osk_ptr() >= 1)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr() - 1);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
      {
         if (menu_event_get_osk_idx() > OSK_TYPE_UNKNOWN + 1)
            menu_event_set_osk_idx((enum osk_type)(
                     menu_event_get_osk_idx() - 1));
         else
            menu_event_set_osk_idx((enum osk_type)(is_rgui ? OSK_SYMBOLS_PAGE1 : OSK_TYPE_LAST - 1));
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
      {
         if (menu_event_get_osk_idx() < (is_rgui ? OSK_SYMBOLS_PAGE1 : OSK_TYPE_LAST - 1))
            menu_event_set_osk_idx((enum osk_type)(
                     menu_event_get_osk_idx() + 1));
         else
            menu_event_set_osk_idx((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
      }

      if (BIT256_GET_PTR(p_trigger_input, menu_ok_btn))
      {
         if (menu_event_get_osk_ptr() >= 0)
            menu_event_osk_append(menu_event_get_osk_ptr());
      }

      if (BIT256_GET_PTR(p_trigger_input, menu_cancel_btn))
         input_keyboard_event(true, '\x7f', '\x7f',
               0, RETRO_DEVICE_KEYBOARD);

      /* send return key to close keyboard input window */
      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_START))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      BIT256_CLEAR_ALL_PTR(p_trigger_input);
   }
   else
   {
      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_UP))
         ret = MENU_ACTION_UP;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
         ret = MENU_ACTION_DOWN;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_LEFT))
         ret = MENU_ACTION_LEFT;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_RIGHT))
         ret = MENU_ACTION_RIGHT;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
         ret = MENU_ACTION_SCROLL_UP;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
         ret = MENU_ACTION_SCROLL_DOWN;
      else if (ok_trigger)
         ret = MENU_ACTION_OK;
      else if (BIT256_GET_PTR(p_trigger_input, menu_cancel_btn))
         ret = MENU_ACTION_CANCEL;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_X))
         ret = MENU_ACTION_SEARCH;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_Y))
         ret = MENU_ACTION_SCAN;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_START))
         ret = MENU_ACTION_START;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ret = MENU_ACTION_INFO;
      else if (BIT256_GET_PTR(p_trigger_input, RARCH_MENU_TOGGLE))
         ret = MENU_ACTION_TOGGLE;
   }

   if (menu_keyboard_key_state[RETROK_F11])
   {
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
      menu_keyboard_key_state[RETROK_F11] = 0;
   }

   mouse_enabled                      = settings->bools.menu_mouse_enable;
#ifdef HAVE_OVERLAY
   if (!mouse_enabled)
      mouse_enabled = !(settings->bools.input_overlay_enable
            && overlay_ptr && overlay_ptr->alive);
#endif

   if (!mouse_enabled)
      menu_input->mouse.ptr = 0;

   if (settings->bools.menu_pointer_enable)
   {
      /* This function gets called for handling pointer events.
       *
       * Pointer events are touchscreen events that are spawned
       * by touchpad/touchscreen. */
      rarch_joypad_info_t joypad_info;
      int pointer_x, pointer_y;
      size_t fb_pitch;
      unsigned fb_width, fb_height;
      const struct retro_keybind *binds[MAX_USERS] = {NULL};
      int pointer_device                           = menu_driver_is_texture_set()
         ?
         RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;

      menu_display_get_fb_size(&fb_width, &fb_height,
            &fb_pitch);

      joypad_info.joy_idx                          = 0;
      joypad_info.auto_binds                       = NULL;
      joypad_info.axis_threshold                   = 0.0f;

      pointer_x                                    =
         current_input->input_state(
               current_input_data, joypad_info, binds,
               0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_X);
      pointer_y                                    =
         current_input->input_state(
               current_input_data, joypad_info, binds,
               0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_Y);

      menu_input->pointer.pressed[0]  = current_input->input_state(
            current_input_data, joypad_info, binds,
            0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
      menu_input->pointer.pressed[1]  = current_input->input_state(
            current_input_data, joypad_info, binds,
            0, pointer_device, 1, RETRO_DEVICE_ID_POINTER_PRESSED);
      menu_input->pointer.back        = current_input->input_state(
            current_input_data, joypad_info, binds,
            0, pointer_device, 0, RARCH_DEVICE_ID_POINTER_BACK);

      menu_input->pointer.x = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
      menu_input->pointer.y = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;
   }
   else
   {
      menu_input->pointer.x          = 0;
      menu_input->pointer.y          = 0;
      menu_input->pointer.dx         = 0;
      menu_input->pointer.dy         = 0;
      menu_input->pointer.accel      = 0;
      menu_input->pointer.pressed[0] = false;
      menu_input->pointer.pressed[1] = false;
      menu_input->pointer.back       = false;
      menu_input->pointer.ptr        = 0;
   }

   return ret;
}

bool menu_input_mouse_check_vector_inside_hitbox(menu_input_ctx_hitbox_t *hitbox)
{
   int16_t  mouse_x       = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t  mouse_y       = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
   bool     inside_hitbox =
      (mouse_x    >= hitbox->x1)
      && (mouse_x <= hitbox->x2)
      && (mouse_y >= hitbox->y1)
      && (mouse_y <= hitbox->y2)
      ;

   return inside_hitbox;
}

bool menu_input_ctl(enum menu_input_ctl_state state, void *data)
{
   static bool pointer_dragging                 = false;
   menu_input_t *menu_input                     = &menu_input_state;

   if (!menu_input)
      return false;

   switch (state)
   {
      case MENU_INPUT_CTL_DEINIT:
         memset(menu_input, 0, sizeof(menu_input_t));
         pointer_dragging      = false;
         break;
      case MENU_INPUT_CTL_MOUSE_PTR:
         menu_input->mouse.ptr = (*(unsigned*)data);
         break;
      case MENU_INPUT_CTL_POINTER_PTR:
         menu_input->pointer.ptr = (*(unsigned*)data);
         break;
      case MENU_INPUT_CTL_POINTER_ACCEL_READ:
         {
            float *ptr = (float*)data;
            *ptr = menu_input->pointer.accel;
         }
         break;
      case MENU_INPUT_CTL_POINTER_ACCEL_WRITE:
         menu_input->pointer.accel = (*(float*)data);
         break;
      case MENU_INPUT_CTL_IS_POINTER_DRAGGED:
         return pointer_dragging;
      case MENU_INPUT_CTL_SET_POINTER_DRAGGED:
         pointer_dragging = true;
         break;
      case MENU_INPUT_CTL_UNSET_POINTER_DRAGGED:
         pointer_dragging = false;
         break;
      case MENU_INPUT_CTL_NONE:
         break;
   }

   return true;
}

static int menu_input_mouse_post_iterate(uint64_t *input_mouse,
      menu_file_list_cbs_t *cbs, unsigned action, bool *mouse_activity)
{
   settings_t *settings       = configuration_settings;
   static bool mouse_oldleft  = false;
   static bool mouse_oldright = false;

   if (
         !settings->bools.menu_mouse_enable
#ifdef HAVE_OVERLAY
         || (settings->bools.input_overlay_enable && overlay_ptr && overlay_ptr->alive)
#endif
         )
   {
      /* HACK: Need to lie to avoid false hits if mouse is held
       * when entering the RetroArch window. */

      /* This happens if, for example, someone double clicks the
       * window border to maximize it.
       *
       * The proper fix is, of course, triggering on WM_LBUTTONDOWN
       * rather than this state change. */
      mouse_oldleft   = true;
      mouse_oldright  = true;
      return 0;
   }

   if (menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON))
   {
      if (!mouse_oldleft)
      {
         menu_input_t *menu_input = &menu_input_state;
         size_t selection         = menu_navigation_get_selection();

         BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_L);

         mouse_oldleft = true;

         if ((menu_input->mouse.ptr == selection) && cbs && cbs->action_select)
         {
            BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_L_TOGGLE);
         }
         else if (menu_input->mouse.ptr <= (menu_entries_get_size() - 1))
         {
            BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_L_SET_NAVIGATION);
         }

         *mouse_activity = true;
      }
   }
   else
      mouse_oldleft = false;

   if (menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON))
   {
      if (!mouse_oldright)
      {
         mouse_oldright = true;
         BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_R);
         *mouse_activity = true;
      }
   }
   else
      mouse_oldright = false;

   if (menu_input_mouse_state(MENU_MOUSE_WHEEL_DOWN))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_WHEEL_DOWN);
      *mouse_activity = true;
   }

   if (menu_input_mouse_state(MENU_MOUSE_WHEEL_UP))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_WHEEL_UP);
      *mouse_activity = true;
   }

   if (menu_input_mouse_state(MENU_MOUSE_HORIZ_WHEEL_DOWN))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_HORIZ_WHEEL_DOWN);
      *mouse_activity = true;
   }

   if (menu_input_mouse_state(MENU_MOUSE_HORIZ_WHEEL_UP))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_HORIZ_WHEEL_UP);
      *mouse_activity = true;
   }

   return 0;
}

static int menu_input_mouse_frame(
      menu_file_list_cbs_t *cbs, menu_entry_t *entry,
      unsigned action)
{
   static rarch_timer_t mouse_activity_timer = {0};
   bool mouse_activity      = false;
   bool no_mouse_activity   = false;
   uint64_t mouse_state     = MENU_MOUSE_ACTION_NONE;
   int ret                  = 0;
   settings_t *settings     = configuration_settings;
   menu_input_t *menu_input = &menu_input_state;
   bool mouse_enable        = settings->bools.menu_mouse_enable;

   if (mouse_enable)
      ret  = menu_input_mouse_post_iterate(&mouse_state, cbs, action, &mouse_activity);

   if ((settings->bools.menu_pointer_enable || mouse_enable))
   {
      static unsigned mouse_old_x               = 0;
      static unsigned mouse_old_y               = 0;
      menu_ctx_pointer_t point;

      point.x       = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      point.y       = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
      point.ptr     = 0;
      point.cbs     = NULL;
      point.entry   = NULL;
      point.action  = 0;
      point.retcode = 0;

      if (menu_input_dialog_get_display_kb())
         menu_driver_ctl(RARCH_MENU_CTL_OSK_PTR_AT_POS, &point);

      if (rarch_timer_is_running(&mouse_activity_timer))
         rarch_timer_tick(&mouse_activity_timer);

      if (mouse_old_x != point.x || mouse_old_y != point.y)
      {
         if (!rarch_timer_is_running(&mouse_activity_timer))
            mouse_activity = true;
         menu_event_set_osk_ptr(point.retcode);
      }
      else
      {
         if (rarch_timer_has_expired(&mouse_activity_timer))
            no_mouse_activity = true;
      }
      mouse_old_x = point.x;
      mouse_old_y = point.y;
   }

   if (BIT64_GET(mouse_state, MENU_MOUSE_ACTION_BUTTON_L))
   {
      menu_ctx_pointer_t point;

      point.x      = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      point.y      = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
      point.ptr    = menu_input->mouse.ptr;
      point.cbs    = cbs;
      point.entry  = entry;
      point.action = action;

      if (menu_input_dialog_get_display_kb())
      {
         menu_driver_ctl(RARCH_MENU_CTL_OSK_PTR_AT_POS, &point);
         if (point.retcode > -1)
         {
            menu_event_set_osk_ptr(point.retcode);
            menu_event_osk_append(point.retcode);
         }
      }
      else
      {
         menu_driver_ctl(RARCH_MENU_CTL_POINTER_UP, &point);
         menu_driver_ctl(RARCH_MENU_CTL_POINTER_TAP, &point);
         ret = point.retcode;
      }
   }

   if (BIT64_GET(mouse_state, MENU_MOUSE_ACTION_BUTTON_R))
   {
      size_t selection = menu_navigation_get_selection();
      menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
   }

   if (BIT64_GET(mouse_state, MENU_MOUSE_ACTION_WHEEL_DOWN))
   {
      unsigned increment_by = 1;
      menu_driver_ctl(MENU_NAVIGATION_CTL_INCREMENT, &increment_by);
   }

   if (BIT64_GET(mouse_state, MENU_MOUSE_ACTION_WHEEL_UP))
   {
      unsigned decrement_by = 1;
      menu_driver_ctl(MENU_NAVIGATION_CTL_DECREMENT, &decrement_by);
   }

   if (BIT64_GET(mouse_state, MENU_MOUSE_ACTION_HORIZ_WHEEL_UP))
   {
      /* stub */
   }

   if (BIT64_GET(mouse_state, MENU_MOUSE_ACTION_HORIZ_WHEEL_DOWN))
   {
      /* stub */
   }

   if (mouse_activity)
   {
      menu_ctx_environment_t menu_environ;

      rarch_timer_begin(&mouse_activity_timer, 4);
      menu_environ.type = MENU_ENVIRON_ENABLE_MOUSE_CURSOR;
      menu_environ.data = NULL;

      menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
   }

   if (no_mouse_activity)
   {
      menu_ctx_environment_t menu_environ;

      rarch_timer_end(&mouse_activity_timer);
      menu_environ.type = MENU_ENVIRON_DISABLE_MOUSE_CURSOR;
      menu_environ.data = NULL;

      menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
   }

   return ret;
}

int16_t menu_input_pointer_state(enum menu_input_pointer_state state)
{
   menu_input_t *menu_input = &menu_input_state;

   if (!menu_input)
      return 0;

   switch (state)
   {
      case MENU_POINTER_X_AXIS:
         return menu_input->pointer.x;
      case MENU_POINTER_Y_AXIS:
         return menu_input->pointer.y;
      case MENU_POINTER_DELTA_X_AXIS:
         return menu_input->pointer.dx;
      case MENU_POINTER_DELTA_Y_AXIS:
         return menu_input->pointer.dy;
      case MENU_POINTER_PRESSED:
         return menu_input->pointer.pressed[0];
   }

   return 0;
}

int16_t menu_input_mouse_state(enum menu_input_mouse_state state)
{
   rarch_joypad_info_t joypad_info;
   unsigned type                   = 0;
   unsigned device                 = RETRO_DEVICE_MOUSE;

   joypad_info.joy_idx             = 0;
   joypad_info.auto_binds          = NULL;
   joypad_info.axis_threshold      = 0.0f;

   switch (state)
   {
      case MENU_MOUSE_X_AXIS:
         device = RARCH_DEVICE_MOUSE_SCREEN;
         type = RETRO_DEVICE_ID_MOUSE_X;
         break;
      case MENU_MOUSE_Y_AXIS:
         device = RARCH_DEVICE_MOUSE_SCREEN;
         type = RETRO_DEVICE_ID_MOUSE_Y;
         break;
      case MENU_MOUSE_LEFT_BUTTON:
         type = RETRO_DEVICE_ID_MOUSE_LEFT;
         break;
      case MENU_MOUSE_RIGHT_BUTTON:
         type = RETRO_DEVICE_ID_MOUSE_RIGHT;
         break;
      case MENU_MOUSE_WHEEL_UP:
         type = RETRO_DEVICE_ID_MOUSE_WHEELUP;
         break;
      case MENU_MOUSE_WHEEL_DOWN:
         type = RETRO_DEVICE_ID_MOUSE_WHEELDOWN;
         break;
      case MENU_MOUSE_HORIZ_WHEEL_UP:
         type = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP;
         break;
      case MENU_MOUSE_HORIZ_WHEEL_DOWN:
         type = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN;
         break;
   }

   return current_input->input_state(current_input_data, joypad_info,
         NULL, 0, device, 0, type);
}

static int menu_input_pointer_post_iterate(
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   static bool pointer_oldpressed[2];
   static bool pointer_oldback  = false;
   static int16_t start_x       = 0;
   static int16_t start_y       = 0;
   static int16_t pointer_old_x = 0;
   static int16_t pointer_old_y = 0;
   int ret                      = 0;
   menu_input_t *menu_input     = &menu_input_state;
   settings_t *settings         = configuration_settings;

#ifdef HAVE_OVERLAY
   /* If we have overlays enabled, overlay controls take
    * precedence and we don't want regular menu
    * pointer controls to be handled */
   if ((       settings->bools.input_overlay_enable
            && overlay_ptr && overlay_ptr->alive))
      return 0;
#endif

   if (menu_input->pointer.pressed[0])
   {
      gfx_ctx_metrics_t metrics;
      float dpi;
      static float accel0       = 0.0f;
      static float accel1       = 0.0f;
      int16_t pointer_x         = menu_input_pointer_state(MENU_POINTER_X_AXIS);
      int16_t pointer_y         = menu_input_pointer_state(MENU_POINTER_Y_AXIS);

      metrics.type  = DISPLAY_METRIC_DPI;
      metrics.value = &dpi;

      menu_input->pointer.counter++;

      if (menu_input->pointer.counter == 1 &&
            !menu_input_ctl(MENU_INPUT_CTL_IS_POINTER_DRAGGED, NULL))
      {
         menu_ctx_pointer_t point;

         point.x                           = pointer_x;
         point.y                           = pointer_y;
         point.ptr                         = menu_input->pointer.ptr;
         point.cbs                         = cbs;
         point.entry                       = entry;
         point.action                      = action;

         menu_driver_ctl(RARCH_MENU_CTL_POINTER_DOWN, &point);
      }

      if (!pointer_oldpressed[0])
      {
         menu_input->pointer.accel         = 0;
         accel0                            = 0;
         accel1                            = 0;
         start_x                           = pointer_x;
         start_y                           = pointer_y;
         pointer_old_x                     = pointer_x;
         pointer_old_y                     = pointer_y;
         pointer_oldpressed[0]             = true;
      }
      else if (video_context_driver_get_metrics(&metrics))
      {
         if (abs(pointer_x - start_x) > (dpi / 10)
               || abs(pointer_y - start_y) > (dpi / 10))
         {
            float s;

            menu_input_ctl(MENU_INPUT_CTL_SET_POINTER_DRAGGED, NULL);
            menu_input->pointer.dx            = pointer_x - pointer_old_x;
            menu_input->pointer.dy            = pointer_y - pointer_old_y;
            pointer_old_x                     = pointer_x;
            pointer_old_y                     = pointer_y;

            s = menu_input->pointer.dy;
            menu_input->pointer.accel = (accel0 + accel1 + s) / 3;
            accel0                    = accel1;
            accel1                    = menu_input->pointer.accel;
         }
      }
   }
   else
   {
      if (pointer_oldpressed[0])
      {
         if (!menu_input_ctl(MENU_INPUT_CTL_IS_POINTER_DRAGGED, NULL))
         {
            menu_ctx_pointer_t point;

            point.x      = start_x;
            point.y      = start_y;
            point.ptr    = menu_input->pointer.ptr;
            point.cbs    = cbs;
            point.entry  = entry;
            point.action = action;

            if (menu_input_dialog_get_display_kb())
            {
               menu_driver_ctl(RARCH_MENU_CTL_OSK_PTR_AT_POS, &point);
               if (point.retcode > -1)
               {
                  menu_event_set_osk_ptr(point.retcode);
                  menu_event_osk_append(point.retcode);
               }
            }
            else
            {
               if (menu_input->pointer.counter > 32)
               {
                  size_t selection = menu_navigation_get_selection();
                  if (cbs && cbs->action_start)
                     return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_START);

               }
               else
               {
                  menu_driver_ctl(RARCH_MENU_CTL_POINTER_UP, &point);
                  menu_driver_ctl(RARCH_MENU_CTL_POINTER_TAP, &point);
                  ret = point.retcode;
               }
            }
         }

         pointer_oldpressed[0]             = false;
         start_x                           = 0;
         start_y                           = 0;
         pointer_old_x                     = 0;
         pointer_old_y                     = 0;
         menu_input->pointer.dx            = 0;
         menu_input->pointer.dy            = 0;
         menu_input->pointer.counter       = 0;

         menu_input_ctl(MENU_INPUT_CTL_UNSET_POINTER_DRAGGED, NULL);
      }
   }

   if (menu_input->pointer.back)
   {
      if (!pointer_oldback)
      {
         pointer_oldback = true;
         menu_entry_action(entry, (unsigned)menu_navigation_get_selection(), MENU_ACTION_CANCEL);
      }
   }

   pointer_oldback = menu_input->pointer.back;

   return ret;
}

void menu_input_post_iterate(int *ret, unsigned action)
{
   menu_entry_t entry;
   settings_t *settings       = configuration_settings;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   menu_file_list_cbs_t *cbs  = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[selection].actiondata 
      : NULL;

   menu_entry_init(&entry);
   /* Note: If menu_input_mouse_frame() or
    * menu_input_pointer_post_iterate() are
    * modified, will have to verify that these
    * parameters remain unused... */
   entry.rich_label_enabled = false;
   entry.value_enabled      = false;
   entry.sublabel_enabled   = false;
   menu_entry_get(&entry, 0, selection, NULL, false);

   *ret = menu_input_mouse_frame(cbs, &entry, action);

   if (settings->bools.menu_pointer_enable)
      *ret |= menu_input_pointer_post_iterate(cbs, &entry, action);
}

#define MENU_INPUT_KEYS_CHECK(cond1, cond2, cond3) \
   for (i = cond1; i < cond2; i++) \
   { \
         bool bit_pressed    = false; \
         if (!cond3) \
         { \
            for (port = 0; port < port_max; port++) \
            { \
               uint16_t              joykey      = 0; \
               uint32_t              joyaxis     = 0; \
               const struct retro_keybind *mtkey = &input_config_binds[port][i]; \
               if (!mtkey->valid) \
                  continue; \
               joypad_info.joy_idx               = settings->uints.input_joypad_map[port]; \
               joypad_info.auto_binds            = input_autoconf_binds[joypad_info.joy_idx]; \
               joypad_info.axis_threshold        = input_driver_axis_threshold; \
               joykey                            = (input_config_binds[port][i].joykey != NO_BTN) \
                  ? input_config_binds[port][i].joykey : joypad_info.auto_binds[i].joykey; \
               joyaxis                           = (input_config_binds[port][i].joyaxis != AXIS_NONE) \
                  ? input_config_binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis; \
               if (sec) \
               { \
                  if (joykey == NO_BTN || !sec->button(joypad_info.joy_idx, joykey)) \
                  { \
                     float    scaled_axis = sec->axis ? ((float)abs(input_joypad_axis(sec, joypad_info.joy_idx, joyaxis)) / 0x8000) : 0.0; \
                     if ((bit_pressed          = scaled_axis > joypad_info.axis_threshold)) \
                        break; \
                  } \
                  else \
                  { \
                     bit_pressed          = true; \
                     break; \
                  } \
               } \
               if (first) \
               { \
                  if (joykey == NO_BTN || !first->button(joypad_info.joy_idx, joykey)) \
                  { \
                     float    scaled_axis = first->axis ? ((float)abs(input_joypad_axis(first, joypad_info.joy_idx, joyaxis)) / 0x8000) : 0.0; \
                     if ((bit_pressed          = scaled_axis > joypad_info.axis_threshold)) \
                        break; \
                  } \
                  else \
                  { \
                     bit_pressed          = true; \
                     break; \
                  } \
               } \
            } \
         } \
         if (bit_pressed || ((i >= RARCH_FIRST_META_KEY) && BIT64_GET(lifecycle_state, i)) || input_keys_pressed_other_sources(i, p_new_state)) \
         { \
            BIT256_SET_PTR(p_new_state, i); \
         } \
      }

/**
 * input_menu_keys_pressed:
 *
 * Grab an input sample for this frame. We exclude
 * keyboard input here.
 *
 * Returns: Input sample containing a mask of all pressed keys.
 */
static void input_menu_keys_pressed(input_bits_t *p_new_state)
{
   unsigned i, port;
   rarch_joypad_info_t joypad_info;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   settings_t     *settings                     = configuration_settings;
   uint8_t max_users                            = (uint8_t)input_driver_max_users;
   uint8_t port_max                             =
      settings->bools.input_all_users_control_menu
      ? max_users : 1;

   joypad_info.joy_idx                          = 0;
   joypad_info.auto_binds                       = NULL;

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *auto_binds          = input_autoconf_binds[i];
      binds[i]                                  = input_config_binds[i];

      input_push_analog_dpad(auto_binds, ANALOG_DPAD_LSTICK);
   }

   for (port = 0; port < port_max; port++)
   {
      const struct retro_keybind *binds_norm = &input_config_binds[port][RARCH_ENABLE_HOTKEY];
      const struct retro_keybind *binds_auto = &input_autoconf_binds[port][RARCH_ENABLE_HOTKEY];

      if (check_input_driver_block_hotkey(binds_norm, binds_auto))
      {
         const struct retro_keybind *htkey = &input_config_binds[port][RARCH_ENABLE_HOTKEY];

         joypad_info.joy_idx                          = settings->uints.input_joypad_map[port];
         joypad_info.auto_binds                       = input_autoconf_binds[joypad_info.joy_idx];
         joypad_info.axis_threshold                   = input_driver_axis_threshold;

         if (htkey->valid
               && current_input->input_state(current_input_data, joypad_info,
                  &binds[0], port, RETRO_DEVICE_JOYPAD, 0, RARCH_ENABLE_HOTKEY))
         {
            input_driver_block_libretro_input = true;
            break;
         }
         else
         {
            input_driver_block_hotkey         = true;
            break;
         }
      }
   }

   {
      const input_device_driver_t *first = current_input->get_joypad_driver
         ? current_input->get_joypad_driver(current_input_data) : NULL;
      const input_device_driver_t *sec   = current_input->get_sec_joypad_driver
         ? current_input->get_sec_joypad_driver(current_input_data) : NULL;

      /* Check the libretro input first */
      MENU_INPUT_KEYS_CHECK(0, RARCH_FIRST_META_KEY, input_driver_block_libretro_input);

      /* Check the hotkeys */
      MENU_INPUT_KEYS_CHECK(RARCH_FIRST_META_KEY, RARCH_BIND_LIST_END, input_driver_block_hotkey);
   }

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      input_pop_analog_dpad(auto_binds);
   }

   if (!menu_input_dialog_get_display_kb())
   {
      unsigned ids[][2] =
      {
         {RETROK_SPACE,     RETRO_DEVICE_ID_JOYPAD_START   },
         {RETROK_SLASH,     RETRO_DEVICE_ID_JOYPAD_X       },
         {RETROK_RSHIFT,    RETRO_DEVICE_ID_JOYPAD_SELECT  },
         {RETROK_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT   },
         {RETROK_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT    },
         {RETROK_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN    },
         {RETROK_UP,        RETRO_DEVICE_ID_JOYPAD_UP      },
         {RETROK_PAGEUP,    RETRO_DEVICE_ID_JOYPAD_L       },
         {RETROK_PAGEDOWN,  RETRO_DEVICE_ID_JOYPAD_R       },
         {0,                RARCH_QUIT_KEY                 },
         {0,                RARCH_FULLSCREEN_TOGGLE_KEY    },
         {RETROK_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_B      },
         {RETROK_RETURN,    RETRO_DEVICE_ID_JOYPAD_A      },
         {RETROK_DELETE,    RETRO_DEVICE_ID_JOYPAD_Y      },
         {0,                RARCH_UI_COMPANION_TOGGLE     },
         {0,                RARCH_FPS_TOGGLE              },
         {0,                RARCH_SEND_DEBUG_INFO         },
         {0,                RARCH_NETPLAY_HOST_TOGGLE     },
         {0,                RARCH_MENU_TOGGLE             },
      };

      ids[9][0]  = input_config_binds[0][RARCH_QUIT_KEY].key;
      ids[10][0] = input_config_binds[0][RARCH_FULLSCREEN_TOGGLE_KEY].key;
      ids[14][0] = input_config_binds[0][RARCH_UI_COMPANION_TOGGLE].key;
      ids[15][0] = input_config_binds[0][RARCH_FPS_TOGGLE].key;
      ids[16][0] = input_config_binds[0][RARCH_SEND_DEBUG_INFO].key;
      ids[17][0] = input_config_binds[0][RARCH_NETPLAY_HOST_TOGGLE].key;
      ids[18][0] = input_config_binds[0][RARCH_MENU_TOGGLE].key;

      if (settings->bools.input_menu_swap_ok_cancel_buttons)
      {
         ids[11][1] = RETRO_DEVICE_ID_JOYPAD_A;
         ids[12][1] = RETRO_DEVICE_ID_JOYPAD_B;
      }

      for (i = 0; i < ARRAY_SIZE(ids); i++)
      {
         if (current_input->input_state(current_input_data,
                  joypad_info, binds, 0,
                  RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
            BIT256_SET_PTR(p_new_state, ids[i][1]);
      }
   }
}
#endif

#define INPUT_KEYS_CHECK(cond1, cond2, cond3) \
   for (i = cond1; i < cond2; i++) \
   { \
      bool bit_pressed = !cond3 && binds[i].valid && current_input->input_state(current_input_data, joypad_info, &binds, 0, RETRO_DEVICE_JOYPAD, 0, i); \
      if (bit_pressed || ((i >= RARCH_FIRST_META_KEY) && BIT64_GET(lifecycle_state, i)) || input_keys_pressed_other_sources(i, p_new_state)) \
      { \
         BIT256_SET_PTR(p_new_state, i); \
      } \
   }

/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 *
 * Returns: Input sample containing a mask of all pressed keys.
 */
static void input_keys_pressed(input_bits_t *p_new_state)
{
   unsigned i;
   rarch_joypad_info_t joypad_info;
   settings_t              *settings            = configuration_settings;
   const struct retro_keybind *binds            = input_config_binds[0];
   const struct retro_keybind *binds_auto       = &input_autoconf_binds[0][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *binds_norm       = &binds[RARCH_ENABLE_HOTKEY];

   joypad_info.joy_idx                          = settings->uints.input_joypad_map[0];
   joypad_info.auto_binds                       = input_autoconf_binds[joypad_info.joy_idx];
   joypad_info.axis_threshold                   = input_driver_axis_threshold;

   if (check_input_driver_block_hotkey(binds_norm, binds_auto))
   {
      const struct retro_keybind *enable_hotkey    =
         &input_config_binds[0][RARCH_ENABLE_HOTKEY];

      if (     enable_hotkey && enable_hotkey->valid
            && current_input->input_state(
               current_input_data, joypad_info, &binds, 0,
               RETRO_DEVICE_JOYPAD, 0, RARCH_ENABLE_HOTKEY))
         input_driver_block_libretro_input = true;
      else
         input_driver_block_hotkey         = true;
   }

   if (binds[RARCH_GAME_FOCUS_TOGGLE].valid)
   {
      const struct retro_keybind *focus_binds_auto =
         &input_autoconf_binds[0][RARCH_GAME_FOCUS_TOGGLE];
      const struct retro_keybind *focus_normal     =
         &binds[RARCH_GAME_FOCUS_TOGGLE];

      /* Allows rarch_focus_toggle hotkey to still work
       * even though every hotkey is blocked */
      if (check_input_driver_block_hotkey(
               focus_normal, focus_binds_auto))
      {
         if (current_input->input_state(current_input_data, joypad_info, &binds, 0,
                  RETRO_DEVICE_JOYPAD, 0, RARCH_GAME_FOCUS_TOGGLE))
            input_driver_block_hotkey = false;
      }
   }

   /* Check the libretro input first */
   INPUT_KEYS_CHECK(0, RARCH_FIRST_META_KEY,
         input_driver_block_libretro_input);

   /* Check the hotkeys */
   INPUT_KEYS_CHECK(RARCH_FIRST_META_KEY, RARCH_BIND_LIST_END,
         input_driver_block_hotkey);
}

int16_t input_driver_input_state(
         rarch_joypad_info_t joypad_info,
         const struct retro_keybind **retro_keybinds,
         unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (current_input && current_input->input_state)
      return current_input->input_state(current_input_data, joypad_info,
            retro_keybinds,
            port, device, index, id);
   return 0;
}

void input_get_state_for_port(void *data, unsigned port,
      input_bits_t *p_new_state)
{
   unsigned i, j;
   rarch_joypad_info_t joypad_info;
   settings_t              *settings            = (settings_t*)data;
   const input_device_driver_t *joypad_driver   = input_driver_get_joypad_driver();

   joypad_info.joy_idx                          = settings->uints.input_joypad_map[port];
   joypad_info.auto_binds                       = input_autoconf_binds[joypad_info.joy_idx];
   joypad_info.axis_threshold                   = input_driver_axis_threshold;

   if (!joypad_driver)
      return;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      if (input_driver_input_state(joypad_info, libretro_input_binds,
               port, RETRO_DEVICE_JOYPAD, 0, i) != 0)
      {
         int16_t      val = input_joypad_analog(
               joypad_driver, joypad_info, port,
               RETRO_DEVICE_INDEX_ANALOG_BUTTON, i, libretro_input_binds[port]);

         BIT256_SET_PTR(p_new_state, i);

         if (val)
            p_new_state->analog_buttons[i] = val;
      }
   }

   for (i = 0; i < 2; i++)
   {
      for (j = 0; j < 2; j++)
      {
         unsigned offset = 0 + (i * 4) + (j * 2);
         int16_t     val = input_joypad_analog(joypad_driver,
               joypad_info, port, i, j, libretro_input_binds[port]);

         if (val >= 0)
            p_new_state->analogs[offset]   = val;
         else
            p_new_state->analogs[offset+1] = val;
      }
   }
}

void *input_driver_get_data(void)
{
   return current_input_data;
}

static bool input_driver_init(void)
{
   if (current_input)
   {
      settings_t *settings    = configuration_settings;
      current_input_data      = current_input->init(settings->arrays.input_joypad_driver);
   }

   if (!current_input_data)
      return false;
   return true;
}

static bool input_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = configuration_settings;

   drv.label            = "input_driver";
   drv.s                = settings->arrays.input_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i                    = (int)drv.len;

   if (i >= 0)
      current_input = (const input_driver_t*)
         input_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any input driver named \"%s\"\n",
            settings->arrays.input_driver);
      RARCH_LOG_OUTPUT("Available input drivers are:\n");
      for (d = 0; input_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", input_driver_find_ident(d));
      RARCH_WARN("Going to default to first input driver...\n");

      current_input = (const input_driver_t*)
         input_driver_find_handle(0);

      if (!current_input)
      {
         retroarch_fail(1, "find_input_driver()");
         return false;
      }
   }

   return true;
}

void input_driver_set_flushing_input(void)
{
   input_driver_flushing_input = true;
}

void input_driver_unset_hotkey_block(void)
{
   input_driver_block_hotkey = true;
}

void input_driver_set_hotkey_block(void)
{
   input_driver_block_hotkey = true;
}

void input_driver_set_libretro_input_blocked(void)
{
   input_driver_block_libretro_input = true;
}

void input_driver_unset_libretro_input_blocked(void)
{
   input_driver_block_libretro_input = false;
}

bool input_driver_is_libretro_input_blocked(void)
{
   return input_driver_block_libretro_input;
}

void input_driver_set_nonblock_state(void)
{
   input_driver_nonblock_state = true;
}

void input_driver_unset_nonblock_state(void)
{
   input_driver_nonblock_state = false;
}

bool input_driver_init_command(void)
{
#ifdef HAVE_COMMAND
   settings_t *settings          = configuration_settings;
   bool input_stdin_cmd_enable   = settings->bools.stdin_cmd_enable;
   bool input_network_cmd_enable = settings->bools.network_cmd_enable;
   bool grab_stdin               = current_input->grab_stdin && current_input->grab_stdin(current_input_data);

   if (!input_stdin_cmd_enable && !input_network_cmd_enable)
      return false;

   if (input_stdin_cmd_enable && grab_stdin)
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   input_driver_command = command_new();

   if (command_network_new(
            input_driver_command,
            input_stdin_cmd_enable && !grab_stdin,
            input_network_cmd_enable,
            settings->uints.network_cmd_port))
      return true;

   RARCH_ERR("Failed to initialize command interface.\n");
#endif
   return false;
}

void input_driver_deinit_command(void)
{
#ifdef HAVE_COMMAND
   if (input_driver_command)
      command_free(input_driver_command);
   input_driver_command = NULL;
#endif
}

void input_driver_deinit_remote(void)
{
#ifdef HAVE_NETWORKGAMEPAD
   if (input_driver_remote)
      input_remote_free(input_driver_remote,
            input_driver_max_users);
   input_driver_remote = NULL;
#endif
}

void input_driver_deinit_mapper(void)
{
   if (input_driver_mapper)
      input_mapper_free(input_driver_mapper);
   input_driver_mapper = NULL;
}

bool input_driver_init_remote(void)
{
#ifdef HAVE_NETWORKGAMEPAD
   settings_t *settings = configuration_settings;

   if (!settings->bools.network_remote_enable)
      return false;

   input_driver_remote = input_remote_new(
         settings->uints.network_remote_base_port,
         input_driver_max_users);

   if (input_driver_remote)
      return true;

   RARCH_ERR("Failed to initialize remote gamepad interface.\n");
#endif
   return false;
}

bool input_driver_init_mapper(void)
{
   settings_t *settings = configuration_settings;

   if (!settings->bools.input_remap_binds_enable)
      return false;

   input_driver_mapper = input_mapper_new();

   if (input_driver_mapper)
      return true;

   RARCH_ERR("Failed to initialize input mapper.\n");
   return false;
}

bool input_driver_grab_mouse(void)
{
   if (!current_input || !current_input->grab_mouse)
      return false;

   current_input->grab_mouse(current_input_data, true);
   return true;
}

float *input_driver_get_float(enum input_action action)
{
   switch (action)
   {
      case INPUT_ACTION_AXIS_THRESHOLD:
         return &input_driver_axis_threshold;
      default:
      case INPUT_ACTION_NONE:
         break;
   }

   return NULL;
}

unsigned *input_driver_get_uint(enum input_action action)
{
   switch (action)
   {
      case INPUT_ACTION_MAX_USERS:
         return &input_driver_max_users;
      default:
      case INPUT_ACTION_NONE:
         break;
   }

   return NULL;
}

bool input_driver_ungrab_mouse(void)
{
   if (!current_input || !current_input->grab_mouse)
      return false;

   current_input->grab_mouse(current_input_data, false);
   return true;
}

/**
 * joypad_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to joypad driver at index. Can be NULL
 * if nothing found.
 **/
const void *joypad_driver_find_handle(int idx)
{
   const void *drv = joypad_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * joypad_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of joypad driver at index. Can be NULL
 * if nothing found.
 **/
const char *joypad_driver_find_ident(int idx)
{
   const input_device_driver_t *drv = joypad_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_joypad_driver_options:
 *
 * Get an enumerated list of all joypad driver names, separated by '|'.
 *
 * Returns: string listing of all joypad driver names, separated by '|'.
 **/
const char* config_get_joypad_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_JOYPAD_DRIVERS, NULL);
}

/**
 * input_joypad_init_first:
 *
 * Finds first suitable joypad driver and initializes.
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
static const input_device_driver_t *input_joypad_init_first(void *data)
{
   unsigned i;

   for (i = 0; joypad_drivers[i]; i++)
   {
      if (joypad_drivers[i]->init(data))
      {
         RARCH_LOG("[Joypad]: Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

/**
 * input_joypad_init_driver:
 * @ident                           : identifier of driver to initialize.
 *
 * Initialize a joypad driver of name @ident.
 *
 * If ident points to NULL or a zero-length string,
 * equivalent to calling input_joypad_init_first().
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_driver(
      const char *ident, void *data)
{
   unsigned i;
   if (!ident || !*ident)
      return input_joypad_init_first(data);

   for (i = 0; joypad_drivers[i]; i++)
   {
      if (string_is_equal(ident, joypad_drivers[i]->ident)
            && joypad_drivers[i]->init(data))
      {
         RARCH_LOG("[Joypad]: Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return input_joypad_init_first(data);
}

/**
 * input_joypad_set_rumble:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @effect                  : Rumble effect to set.
 * @strength                : Strength of rumble effect.
 *
 * Sets rumble effect @effect with strength @strength.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_joypad_set_rumble(const input_device_driver_t *drv,
      unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   settings_t *settings       = configuration_settings;
   unsigned joy_idx           = settings->uints.input_joypad_map[port];

   if (!drv || !drv->set_rumble || joy_idx >= MAX_USERS)
      return false;

   return drv->set_rumble(joy_idx, effect, strength);
}

/**
 * input_joypad_analog:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @idx                     : Analog key index.
 *                            E.g.:
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @binds                   : Binds of user.
 *
 * Gets analog value of analog key identifiers @idx and @ident
 * from user with number @port with provided keybinds (@binds).
 *
 * Returns: analog value on success, otherwise 0.
 **/
int16_t input_joypad_analog(const input_device_driver_t *drv,
      rarch_joypad_info_t joypad_info,
      unsigned port, unsigned idx, unsigned ident,
      const struct retro_keybind *binds)
{
   int16_t res = 0;

   if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
   {
      /* A RETRO_DEVICE_JOYPAD button?
       * Otherwise, not a suitable button */
      if (ident < RARCH_FIRST_CUSTOM_BIND)
      {
         uint32_t axis = 0;
         const struct retro_keybind *bind = &binds[ ident ];

         if (!bind->valid)
            return 0;

         axis = (bind->joyaxis == AXIS_NONE) 
            ? joypad_info.auto_binds[ident].joyaxis
            : bind->joyaxis;

         /* Analog button. */
         /* no deadzone/sensitivity correction for analog buttons currently */
         if (drv->axis)
            res = abs(drv->axis(joypad_info.joy_idx, axis));

         /* If the result is zero, it's got a digital button 
          * attached to it instead */
         if (res == 0)
         {
            uint16_t key = (bind->joykey == NO_BTN) 
               ? joypad_info.auto_binds[ident].joykey
               : bind->joykey;

            if (drv->button(joypad_info.joy_idx, key))
               res = 0x7fff;
         }
      }
   }
   else
   {
      /* Analog sticks. Either RETRO_DEVICE_INDEX_ANALOG_LEFT
       * or RETRO_DEVICE_INDEX_ANALOG_RIGHT */

      unsigned ident_minus                   = 0;
      unsigned ident_plus                    = 0;
      const struct retro_keybind *bind_minus = NULL;
      const struct retro_keybind *bind_plus  = NULL;

      input_conv_analog_id_to_bind_id(idx, ident, ident_minus, ident_plus);

      bind_minus                             = &binds[ident_minus];
      bind_plus                              = &binds[ident_plus];

      if (!bind_minus->valid || !bind_plus->valid)
         return 0;

      if (drv->axis)
      {
         uint32_t axis_minus    = (bind_minus->joyaxis == AXIS_NONE) 
            ? joypad_info.auto_binds[ident_minus].joyaxis 
            : bind_minus->joyaxis;
         uint32_t axis_plus     = (bind_plus->joyaxis  == AXIS_NONE) 
            ? joypad_info.auto_binds[ident_plus].joyaxis  
            : bind_plus->joyaxis;
         int16_t  pressed_minus = abs(
               input_joypad_axis(drv, joypad_info.joy_idx,
                  axis_minus));
         int16_t pressed_plus  = abs(
               input_joypad_axis(drv, joypad_info.joy_idx,
                  axis_plus));
         res                   = pressed_plus - pressed_minus;
      }

      if (res == 0)
      {
         uint16_t key_minus    = (bind_minus->joykey == NO_BTN) 
            ? joypad_info.auto_binds[ident_minus].joykey 
            : bind_minus->joykey;
         uint16_t key_plus     = (bind_plus->joykey  == NO_BTN) 
            ? joypad_info.auto_binds[ident_plus].joykey  
            : bind_plus->joykey;
         int16_t digital_left  = drv->button(joypad_info.joy_idx, key_minus) 
            ? -0x7fff : 0;
         int16_t digital_right = drv->button(joypad_info.joy_idx, key_plus)  
            ? 0x7fff  : 0;

         return digital_right + digital_left;
      }
   }

   return res;
}

/**
 * input_mouse_button_raw:
 * @port                    : Mouse number.
 * @button                  : Identifier of key (libretro mouse constant).
 *
 * Checks if key (@button) was being pressed by user
 * with mouse number @port.
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
bool input_mouse_button_raw(unsigned port, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   settings_t *settings = configuration_settings;

   /*ignore axes*/
   if (id == RETRO_DEVICE_ID_MOUSE_X || id == RETRO_DEVICE_ID_MOUSE_Y)
      return false;

   joypad_info.axis_threshold = input_driver_axis_threshold;
   joypad_info.joy_idx        = settings->uints.input_joypad_map[port];
   joypad_info.auto_binds     = input_autoconf_binds[joypad_info.joy_idx];

   if (current_input->input_state(current_input_data,
         joypad_info, libretro_input_binds, port, RETRO_DEVICE_MOUSE, 0, id))
      return true;
   return false;
}

void input_pad_connect(unsigned port, input_device_driver_t *driver)
{
   if (port >= MAX_USERS || !driver)
   {
      RARCH_ERR("[input]: input_pad_connect: bad parameters\n");
      return;
   }

   fire_connection_listener(port, driver);

   if (!input_autoconfigure_connect(driver->name(port), NULL, driver->ident,
          port, 0, 0))
      input_config_set_device_name(port, driver->name(port));
}

#ifdef HAVE_HID
/**
 * hid_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to HID driver at index. Can be NULL
 * if nothing found.
 **/
const void *hid_driver_find_handle(int idx)
{
   const void *drv = hid_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

const void *hid_driver_get_data(void)
{
   return hid_data;
}

/* This is only to be called after we've invoked free() on the
 * HID driver; the memory will have already been freed, so we need to
 * reset the pointer.
 */
void hid_driver_reset_data(void)
{
   hid_data = NULL;
}

/**
 * hid_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of HID driver at index. Can be NULL
 * if nothing found.
 **/
const char *hid_driver_find_ident(int idx)
{
   const hid_driver_t *drv = hid_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_hid_driver_options:
 *
 * Get an enumerated list of all HID driver names, separated by '|'.
 *
 * Returns: string listing of all HID driver names, separated by '|'.
 **/
const char* config_get_hid_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_HID_DRIVERS, NULL);
}

/**
 * input_hid_init_first:
 *
 * Finds first suitable HID driver and initializes.
 *
 * Returns: HID driver if found, otherwise NULL.
 **/
const hid_driver_t *input_hid_init_first(void)
{
   unsigned i;

   for (i = 0; hid_drivers[i]; i++)
   {
      hid_data = hid_drivers[i]->init();

      if (hid_data)
      {
         RARCH_LOG("Found HID driver: \"%s\".\n",
               hid_drivers[i]->ident);
         return hid_drivers[i];
      }
   }

   return NULL;
}
#endif

static void osk_update_last_codepoint(const char *word)
{
   const char *letter = word;
   const char    *pos = letter;

   for (;;)
   {
      unsigned codepoint = utf8_walk(&letter);
      unsigned       len = (unsigned)(letter - pos);

      if (letter[0] == 0)
      {
         osk_last_codepoint     = codepoint;
         osk_last_codepoint_len = len;
         break;
      }

      pos = letter;
   }
}

/* Depends on ASCII character values */
#define ISPRINT(c) (((int)(c) >= ' ' && (int)(c) <= '~') ? 1 : 0)

/**
 * input_keyboard_line_event:
 * @state                    : Input keyboard line handle.
 * @character                : Inputted character.
 *
 * Called on every keyboard character event.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool input_keyboard_line_event(
      input_keyboard_line_t *state, uint32_t character)
{
   char array[2];
   bool ret         = false;
   const char *word = NULL;
   char c           = character >= 128 ? '?' : character;

   /* Treat extended chars as ? as we cannot support
    * printable characters for unicode stuff. */

   if (c == '\r' || c == '\n')
   {
      state->cb(state->userdata, state->buffer);

      array[0] = c;
      array[1] = 0;

      word     = array;
      ret      = true;
   }
   else if (c == '\b' || c == '\x7f') /* 0x7f is ASCII for del */
   {
      if (state->ptr)
      {
         unsigned i;

         for (i = 0; i < osk_last_codepoint_len; i++)
         {
            memmove(state->buffer + state->ptr - 1,
                  state->buffer + state->ptr,
                  state->size - state->ptr + 1);
            state->ptr--;
            state->size--;
         }

         word     = state->buffer;
      }
   }
   else if (ISPRINT(c))
   {
      /* Handle left/right here when suitable */
      char *newbuf = (char*)
         realloc(state->buffer, state->size + 2);
      if (!newbuf)
         return false;

      memmove(newbuf + state->ptr + 1,
            newbuf + state->ptr,
            state->size - state->ptr + 1);
      newbuf[state->ptr] = c;
      state->ptr++;
      state->size++;
      newbuf[state->size] = '\0';

      state->buffer = newbuf;

      array[0] = c;
      array[1] = 0;

      word     = array;
   }

   if (word != NULL)
   {
      /* OSK - update last character */
      if (word[0] == 0)
      {
         osk_last_codepoint     = 0;
         osk_last_codepoint_len = 0;
      }
      else
         osk_update_last_codepoint(word);
   }

   return ret;
}

bool input_keyboard_line_append(const char *word)
{
   unsigned i   = 0;
   unsigned len = (unsigned)strlen(word);
   char *newbuf = (char*)
      realloc(g_keyboard_line->buffer,
            g_keyboard_line->size + len*2);

   if (!newbuf)
      return false;

   memmove(newbuf + g_keyboard_line->ptr + len,
         newbuf + g_keyboard_line->ptr,
         g_keyboard_line->size - g_keyboard_line->ptr + len);

   for (i = 0; i < len; i++)
   {
      newbuf[g_keyboard_line->ptr] = word[i];
      g_keyboard_line->ptr++;
      g_keyboard_line->size++;
   }

   newbuf[g_keyboard_line->size] = '\0';

   g_keyboard_line->buffer = newbuf;

   if (word[0] == 0)
   {
      osk_last_codepoint     = 0;
      osk_last_codepoint_len = 0;
   }
   else
      osk_update_last_codepoint(word);

   return false;
}

/**
 * input_keyboard_start_line:
 * @userdata                 : Userdata.
 * @cb                       : Line complete callback function.
 *
 * Sets function pointer for keyboard line handle.
 *
 * The underlying buffer can be reallocated at any time
 * (or be NULL), but the pointer to it remains constant
 * throughout the objects lifetime.
 *
 * Returns: underlying buffer of the keyboard line.
 **/
const char **input_keyboard_start_line(void *userdata,
      input_keyboard_line_complete_t cb)
{
   input_keyboard_line_t *state = (input_keyboard_line_t*)
      calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   g_keyboard_line           = state;
   g_keyboard_line->cb       = cb;
   g_keyboard_line->userdata = userdata;

   /* While reading keyboard line input, we have to block all hotkeys. */
   if (current_input->keyboard_mapping_set_block)
      current_input->keyboard_mapping_set_block(current_input_data, true);

   return (const char**)&g_keyboard_line->buffer;
}

/**
 * input_keyboard_event:
 * @down                     : Keycode was pressed down?
 * @code                     : Keycode.
 * @character                : Character inputted.
 * @mod                      : TODO/FIXME: ???
 *
 * Keyboard event utils. Called by drivers when keyboard events are fired.
 * This interfaces with the global system driver struct and libretro callbacks.
 **/
void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   static bool deferred_wait_keys;

   if (deferred_wait_keys)
   {
      if (down)
         return;

      g_keyboard_press_cb   = NULL;
      g_keyboard_press_data = NULL;
      if (current_input->keyboard_mapping_set_block)
         current_input->keyboard_mapping_set_block(current_input_data, false);
      deferred_wait_keys    = false;
   }
   else if (g_keyboard_press_cb)
   {
      if (!down || code == RETROK_UNKNOWN)
         return;
      if (g_keyboard_press_cb(g_keyboard_press_data, code))
         return;
      deferred_wait_keys = true;
   }
   else if (g_keyboard_line)
   {
      if (!down)
         return;

      switch (device)
      {
         case RETRO_DEVICE_POINTER:
            if (code != 0x12d)
               character = (char)code;
            /* fall-through */
         default:
            if (!input_keyboard_line_event(g_keyboard_line, character))
               return;
            break;
      }

      /* Line is complete, can free it now. */
      input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_LINE_FREE, NULL);

      /* Unblock all hotkeys. */
      if (current_input->keyboard_mapping_set_block)
         current_input->keyboard_mapping_set_block(current_input_data, false);
   }
   else
   {
      retro_keyboard_event_t *key_event = &runloop_key_event;

      if (*key_event)
         (*key_event)(down, code, character, mod);
   }
}

bool input_keyboard_ctl(enum rarch_input_keyboard_ctl_state state, void *data)
{
   switch (state)
   {
      case RARCH_INPUT_KEYBOARD_CTL_LINE_FREE:
         if (g_keyboard_line)
         {
            free(g_keyboard_line->buffer);
            free(g_keyboard_line);
         }
         g_keyboard_line = NULL;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_START_WAIT_KEYS:
         {
            input_keyboard_ctx_wait_t *keys = (input_keyboard_ctx_wait_t*)data;

            if (!keys)
               return false;

            g_keyboard_press_cb   = keys->cb;
            g_keyboard_press_data = keys->userdata;
         }

         /* While waiting for input, we have to block all hotkeys. */
         if (current_input->keyboard_mapping_set_block)
            current_input->keyboard_mapping_set_block(current_input_data, true);
         break;
      case RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS:
         g_keyboard_press_cb   = NULL;
         g_keyboard_press_data = NULL;
         if (current_input->keyboard_mapping_set_block)
            current_input->keyboard_mapping_set_block(current_input_data, false);
         break;
      case RARCH_INPUT_KEYBOARD_CTL_SET_LINEFEED_ENABLED:
         input_driver_keyboard_linefeed_enable = true;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_UNSET_LINEFEED_ENABLED:
         input_driver_keyboard_linefeed_enable = false;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED:
         return input_driver_keyboard_linefeed_enable;
      case RARCH_INPUT_KEYBOARD_CTL_NONE:
      default:
         break;
   }

   return true;
}

#define input_config_bind_map_get(i) ((const struct input_bind_map*)&input_config_bind_map[(i)])

static bool input_config_bind_map_get_valid(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return false;
   return keybind->valid;
}

unsigned input_config_bind_map_get_meta(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return 0;
   return keybind->meta;
}

const char *input_config_bind_map_get_base(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return NULL;
   return keybind->base;
}

const char *input_config_bind_map_get_desc(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return NULL;
   return msg_hash_to_str(keybind->desc);
}

static void input_config_parse_key(
      config_file_t *conf,
      const char *prefix, const char *btn,
      struct retro_keybind *bind)
{
   char tmp[64];
   char key[64];

   tmp[0] = key[0] = '\0';

   fill_pathname_join_delim(key, prefix, btn, '_', sizeof(key));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
      bind->key = input_config_translate_str_to_rk(tmp);
}

static const char *input_config_get_prefix(unsigned user, bool meta)
{
   static const char *bind_user_prefix[MAX_USERS] = {
      "input_player1",
      "input_player2",
      "input_player3",
      "input_player4",
      "input_player5",
      "input_player6",
      "input_player7",
      "input_player8",
      "input_player9",
      "input_player10",
      "input_player11",
      "input_player12",
      "input_player13",
      "input_player14",
      "input_player15",
      "input_player16",
   };
   const char *prefix = bind_user_prefix[user];

   if (user == 0)
      return meta ? "input" : prefix;

   if (!meta)
      return prefix;

   /* Don't bother with meta bind for anyone else than first user. */
   return NULL;
}

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates tring representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str)
{
   size_t i;
   if (strlen(str) == 1 && isalpha((int)*str))
      return (enum retro_key)(RETROK_a + (tolower((int)*str) - (int)'a'));
   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (string_is_equal_noncase(input_config_key_map[i].str, str))
         return input_config_key_map[i].key;
   }

   RARCH_WARN("Key name %s not found.\n", str);
   return RETROK_UNKNOWN;
}

/**
 * input_config_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise
 * RARCH_BIND_LIST_END on not found.
 **/
unsigned input_config_translate_str_to_bind_id(const char *str)
{
   unsigned i;

   for (i = 0; input_config_bind_map[i].valid; i++)
      if (string_is_equal(str, input_config_bind_map[i].base))
         return i;

   return RARCH_BIND_LIST_END;
}

static void parse_hat(struct retro_keybind *bind, const char *str)
{
   uint16_t hat_dir = 0;
   char        *dir = NULL;
   uint16_t     hat = strtoul(str, &dir, 0);

   if (!dir)
   {
      RARCH_WARN("Found invalid hat in config!\n");
      return;
   }

   if      (string_is_equal(dir, "up"))
      hat_dir = HAT_UP_MASK;
   else if (string_is_equal(dir, "down"))
      hat_dir = HAT_DOWN_MASK;
   else if (string_is_equal(dir, "left"))
      hat_dir = HAT_LEFT_MASK;
   else if (string_is_equal(dir, "right"))
      hat_dir = HAT_RIGHT_MASK;

   if (hat_dir)
      bind->joykey = HAT_MAP(hat, hat_dir);
}

static void input_config_parse_joy_button(
      config_file_t *conf, const char *prefix,
      const char *btn, struct retro_keybind *bind)
{
   char str[256];
   char tmp[64];
   char key[64];
   char key_label[64];
   char *tmp_a              = NULL;

   str[0] = tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(str, prefix, btn,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "btn", '_', sizeof(key));
   fill_pathname_join_delim(key_label, str,
         "btn_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      btn = tmp;
      if (string_is_equal(btn, file_path_str(FILE_PATH_NUL)))
         bind->joykey = NO_BTN;
      else
      {
         if (*btn == 'h')
         {
            const char *str = btn + 1;
            if (bind && str && isdigit((int)*str))
               parse_hat(bind, str);
         }
         else
            bind->joykey = strtoull(tmp, NULL, 0);
      }
   }

   if (bind && config_get_string(conf, key_label, &tmp_a))
   {
      if (!string_is_empty(bind->joykey_label))
         free(bind->joykey_label);

      bind->joykey_label = strdup(tmp_a);
      free(tmp_a);
   }
}

static void input_config_parse_joy_axis(
      config_file_t *conf, const char *prefix,
      const char *axis, struct retro_keybind *bind)
{
   char str[256];
   char       tmp[64];
   char       key[64];
   char key_label[64];
   char        *tmp_a       = NULL;

   str[0] = tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(str, prefix, axis,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "axis", '_', sizeof(key));
   fill_pathname_join_delim(key_label, str,
         "axis_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      if (string_is_equal(tmp, file_path_str(FILE_PATH_NUL)))
         bind->joyaxis = AXIS_NONE;
      else if (strlen(tmp) >= 2 && (*tmp == '+' || *tmp == '-'))
      {
         int i_axis = (int)strtol(tmp + 1, NULL, 0);
         if (*tmp == '+')
            bind->joyaxis = AXIS_POS(i_axis);
         else
            bind->joyaxis = AXIS_NEG(i_axis);
      }

      /* Ensure that D-pad emulation doesn't screw this over. */
      bind->orig_joyaxis = bind->joyaxis;
   }

   if (config_get_string(conf, key_label, &tmp_a))
   {
      if (bind->joyaxis_label &&
            !string_is_empty(bind->joyaxis_label))
         free(bind->joyaxis_label);
      bind->joyaxis_label = strdup(tmp_a);
      free(tmp_a);
   }
}

static void input_config_parse_mouse_button(
      config_file_t *conf, const char *prefix,
      const char *btn, struct retro_keybind *bind)
{
   int val;
   char str[256];
   char tmp[64];
   char key[64];

   str[0] = tmp[0] = key[0] = '\0';

   fill_pathname_join_delim(str, prefix, btn,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "mbtn", '_', sizeof(key));

   if (bind && config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      bind->mbutton = NO_BTN;

      if (tmp[0]=='w')
      {
         switch (tmp[1])
         {
            case 'u':
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_WHEELUP;
               break;
            case 'd':
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_WHEELDOWN;
               break;
            case 'h':
               switch (tmp[2])
               {
                  case 'u':
                     bind->mbutton = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP;
                     break;
                  case 'd':
                     bind->mbutton = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN;
                     break;
               }
               break;
         }
      }
      else
      {
         val = atoi(tmp);
         switch (val)
         {
            case 1:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_LEFT;
               break;
            case 2:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_RIGHT;
               break;
            case 3:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_MIDDLE;
               break;
            case 4:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_BUTTON_4;
               break;
            case 5:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_BUTTON_5;
               break;
         }
      }
   }
}

static void input_config_get_bind_string_joykey(
      char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   settings_t *settings       = configuration_settings;
   bool label_show            = settings->bools.input_descriptor_label_show;

   if (GET_HAT_DIR(bind->joykey))
   {
      if (bind->joykey_label &&
            !string_is_empty(bind->joykey_label) && label_show)
         snprintf(buf, size, "%s %s (hat)", prefix, bind->joykey_label);
      else
      {
         const char *dir = "?";

         switch (GET_HAT_DIR(bind->joykey))
         {
            case HAT_UP_MASK:
               dir = "up";
               break;
            case HAT_DOWN_MASK:
               dir = "down";
               break;
            case HAT_LEFT_MASK:
               dir = "left";
               break;
            case HAT_RIGHT_MASK:
               dir = "right";
               break;
            default:
               break;
         }
         snprintf(buf, size, "%sHat #%u %s (%s)", prefix,
               (unsigned)GET_HAT(bind->joykey), dir,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
      }
   }
   else
   {
      if (bind->joykey_label &&
            !string_is_empty(bind->joykey_label) && label_show)
         snprintf(buf, size, "%s%s (btn)", prefix, bind->joykey_label);
      else
         snprintf(buf, size, "%s%u (%s)", prefix, (unsigned)bind->joykey,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
   }
}

static void input_config_get_bind_string_joyaxis(char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   settings_t *settings = configuration_settings;

   if (bind->joyaxis_label &&
         !string_is_empty(bind->joyaxis_label)
         && settings->bools.input_descriptor_label_show)
      snprintf(buf, size, "%s%s (axis)", prefix, bind->joyaxis_label);
   else
   {
      unsigned axis        = 0;
      char dir             = '\0';
      if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
      {
         dir = '-';
         axis = AXIS_NEG_GET(bind->joyaxis);
      }
      else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
      {
         dir = '+';
         axis = AXIS_POS_GET(bind->joyaxis);
      }
      snprintf(buf, size, "%s%c%u (%s)", prefix, dir, axis,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
   }
}

void input_config_get_bind_string(char *buf, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size)
{
   int delim = 0;
#ifndef RARCH_CONSOLE
   char key[64];
   char keybuf[64];

   key[0] = keybuf[0] = '\0';
#endif

   *buf = '\0';
   if (bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(buf, "", bind, size);
   else if (bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(buf, "", bind, size);
   else if (auto_bind && auto_bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(buf, "Auto: ", auto_bind, size);
   else if (auto_bind && auto_bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(buf, "Auto: ", auto_bind, size);

   if (*buf)
      delim = 1;

#ifndef RARCH_CONSOLE
   input_keymaps_translate_rk_to_str(bind->key, key, sizeof(key));
   if (string_is_equal(key, file_path_str(FILE_PATH_NUL)))
      *key = '\0';
   /*empty?*/
   if (*key != '\0')
   {
      if (delim )
         strlcat(buf, ", ", size);
      snprintf(keybuf, sizeof(keybuf), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_KEY), key);
      strlcat(buf, keybuf, size);
      delim = 1;
   }
#endif

   if (bind->mbutton != NO_BTN)
   {
      int tag = 0;
      switch (bind->mbutton)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT;
            break;
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT;
            break;
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN;
            break;
      }

      if (tag != 0)
      {
         if (delim)
            strlcat(buf, ", ", size);
         strlcat(buf, msg_hash_to_str((enum msg_hash_enums)tag), size );
      }
   }

   /*completely empty?*/
   if (*buf == '\0')
      strlcat(buf, "---", size);
}

unsigned input_config_get_device_count(void)
{
   unsigned num_devices;
   for (num_devices = 0; num_devices < MAX_INPUT_DEVICES; ++num_devices)
   {
      const char *device_name = input_config_get_device_name(num_devices);
      if (string_is_empty(device_name))
         break;
   }
   return num_devices;
}

const char *input_config_get_device_name(unsigned port)
{
   if (string_is_empty(input_device_names[port]))
      return NULL;
   return input_device_names[port];
}

const char *input_config_get_device_display_name(unsigned port)
{
   if (string_is_empty(input_device_display_names[port]))
      return NULL;
   return input_device_display_names[port];
}

const char *input_config_get_device_config_path(unsigned port)
{
   if (string_is_empty(input_device_config_paths[port]))
      return NULL;
   return input_device_config_paths[port];
}

const char *input_config_get_device_config_name(unsigned port)
{
   if (string_is_empty(input_device_config_names[port]))
      return NULL;
   return input_device_config_names[port];
}

void input_config_set_device_name(unsigned port, const char *name)
{
   if (string_is_empty(name))
      return;

   strlcpy(input_device_names[port],
         name,
         sizeof(input_device_names[port]));

   input_autoconfigure_joypad_reindex_devices();
}

void input_config_set_device_config_path(unsigned port, const char *path)
{
   if (string_is_empty(path))
      return;

   fill_pathname_parent_dir_name(input_device_config_paths[port],
         path, sizeof(input_device_config_paths[port]));
   strlcat(input_device_config_paths[port],
         "/",
         sizeof(input_device_config_paths[port]));
   strlcat(input_device_config_paths[port],
         path_basename(path),
         sizeof(input_device_config_paths[port]));
}

void input_config_set_device_config_name(unsigned port, const char *name)
{
   if (!string_is_empty(name))
      strlcpy(input_device_config_names[port],
            name,
            sizeof(input_device_config_names[port]));
}

void input_config_set_device_display_name(unsigned port, const char *name)
{
   if (!string_is_empty(name))
      strlcpy(input_device_display_names[port],
            name,
            sizeof(input_device_display_names[port]));
}

void input_config_clear_device_name(unsigned port)
{
   input_device_names[port][0] = '\0';
   input_autoconfigure_joypad_reindex_devices();
}

void input_config_clear_device_display_name(unsigned port)
{
   input_device_display_names[port][0] = '\0';
}

void input_config_clear_device_config_path(unsigned port)
{
   input_device_config_paths[port][0] = '\0';
}

void input_config_clear_device_config_name(unsigned port)
{
   input_device_config_names[port][0] = '\0';
}

unsigned *input_config_get_device_ptr(unsigned port)
{
   settings_t *settings = configuration_settings;
   return &settings->uints.input_libretro_device[port];
}

unsigned input_config_get_device(unsigned port)
{
   settings_t *settings = configuration_settings;
   return settings->uints.input_libretro_device[port];
}

void input_config_set_device(unsigned port, unsigned id)
{
   settings_t *settings = configuration_settings;

   if (settings)
      settings->uints.input_libretro_device[port] = id;
}


const struct retro_keybind *input_config_get_bind_auto(
      unsigned port, unsigned id)
{
   settings_t *settings = configuration_settings;
   unsigned joy_idx     = settings->uints.input_joypad_map[port];

   if (joy_idx < MAX_USERS)
      return &input_autoconf_binds[joy_idx][id];
   return NULL;
}

void input_config_set_pid(unsigned port, uint16_t pid)
{
   input_config_pid[port] = pid;
}

uint16_t input_config_get_pid(unsigned port)
{
   return input_config_pid[port];
}

void input_config_set_vid(unsigned port, uint16_t vid)
{
   input_config_vid[port] = vid;
}

uint16_t input_config_get_vid(unsigned port)
{
   return input_config_vid[port];
}

void input_config_reset(void)
{
   unsigned i, j;

   retro_assert(sizeof(input_config_binds[0]) >= sizeof(retro_keybinds_1));
   retro_assert(sizeof(input_config_binds[1]) >= sizeof(retro_keybinds_rest));

   memcpy(input_config_binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));

   for (i = 1; i < MAX_USERS; i++)
      memcpy(input_config_binds[i], retro_keybinds_rest,
            sizeof(retro_keybinds_rest));

   for (i = 0; i < MAX_USERS; i++)
   {
      input_config_vid[i]     = 0;
      input_config_pid[i]     = 0;
      libretro_input_binds[i] = input_config_binds[i];

      for (j = 0; j < 64; j++)
         input_device_names[i][j] = 0;
   }
}

void config_read_keybinds_conf(void *data)
{
   unsigned i;
   config_file_t *conf = (config_file_t*)data;

   if (!conf)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;

      for (j = 0; input_config_bind_map_get_valid(j); j++)
      {
         struct retro_keybind *bind = &input_config_binds[i][j];
         const char *prefix         = input_config_get_prefix(i, input_config_bind_map_get_meta(j));
         const char *btn            = input_config_bind_map_get_base(j);

         if (!bind->valid)
            continue;
         if (!input_config_bind_map_get_valid(j))
            continue;
         if (!btn || !prefix)
            continue;

         input_config_parse_key(conf, prefix, btn, bind);
         input_config_parse_joy_button(conf, prefix, btn, bind);
         input_config_parse_joy_axis(conf, prefix, btn, bind);
         input_config_parse_mouse_button(conf, prefix, btn, bind);
      }
   }
}

void input_autoconfigure_joypad_conf(void *data,
      struct retro_keybind *binds)
{
   unsigned i;
   config_file_t *conf = (config_file_t*)data;

   if (!conf)
      return;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_config_parse_joy_button(conf, "input",
            input_config_bind_map_get_base(i), &binds[i]);
      input_config_parse_joy_axis(conf, "input",
            input_config_bind_map_get_base(i), &binds[i]);
   }
}

/**
 * input_config_save_keybinds_user:
 * @conf               : pointer to config file object
 * @user               : user number
 *
 * Save the current keybinds of a user (@user) to the config file (@conf).
 */
void input_config_save_keybinds_user(void *data, unsigned user)
{
   unsigned i = 0;
   config_file_t *conf = (config_file_t*)data;

   for (i = 0; input_config_bind_map_get_valid(i); i++)
   {
      char key[64];
      char btn[64];
      const char *prefix               = input_config_get_prefix(user,
            input_config_bind_map_get_meta(i));
      const struct retro_keybind *bind = &input_config_binds[user][i];
      const char                 *base = input_config_bind_map_get_base(i);

      if (!prefix || !bind->valid)
         continue;

      key[0] = btn[0]  = '\0';

      fill_pathname_join_delim(key, prefix, base, '_', sizeof(key));

      input_keymaps_translate_rk_to_str(bind->key, btn, sizeof(btn));
      config_set_string(conf, key, btn);

      input_config_save_keybind(conf, prefix, base, bind, true);
   }
}

static void save_keybind_hat(config_file_t *conf, const char *key,
      const struct retro_keybind *bind)
{
   char config[16];
   unsigned hat     = (unsigned)GET_HAT(bind->joykey);
   const char *dir  = NULL;

   config[0]        = '\0';

   switch (GET_HAT_DIR(bind->joykey))
   {
      case HAT_UP_MASK:
         dir = "up";
         break;

      case HAT_DOWN_MASK:
         dir = "down";
         break;

      case HAT_LEFT_MASK:
         dir = "left";
         break;

      case HAT_RIGHT_MASK:
         dir = "right";
         break;

      default:
         break;
   }

   snprintf(config, sizeof(config), "h%u%s", hat, dir);
   config_set_string(conf, key, config);
}

static void save_keybind_joykey(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];

   key[0] = '\0';

   fill_pathname_join_delim_concat(key, prefix,
         base, '_', "_btn", sizeof(key));

   if (bind->joykey == NO_BTN)
   {
       if (save_empty)
         config_set_string(conf, key, file_path_str(FILE_PATH_NUL));
   }
   else if (GET_HAT_DIR(bind->joykey))
      save_keybind_hat(conf, key, bind);
   else
      config_set_uint64(conf, key, bind->joykey);
}

static void save_keybind_axis(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];
   unsigned axis   = 0;
   char dir        = '\0';

   key[0] = '\0';

   fill_pathname_join_delim_concat(key,
         prefix, base, '_',
         "_axis",
         sizeof(key));

   if (bind->joyaxis == AXIS_NONE)
   {
      if (save_empty)
         config_set_string(conf, key, file_path_str(FILE_PATH_NUL));
   }
   else if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }

   if (dir)
   {
      char config[16];

      config[0] = '\0';

      snprintf(config, sizeof(config), "%c%u", dir, axis);
      config_set_string(conf, key, config);
   }
}

static void save_keybind_mbutton(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];

   key[0] = '\0';

   fill_pathname_join_delim_concat(key, prefix,
      base, '_', "_mbtn", sizeof(key));

   switch (bind->mbutton)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         config_set_uint64(conf, key, 1);
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         config_set_uint64(conf, key, 2);
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         config_set_uint64(conf, key, 3);
         break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         config_set_uint64(conf, key, 4);
         break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         config_set_uint64(conf, key, 5);
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         config_set_string(conf, key, "wu");
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         config_set_string(conf, key, "wd");
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         config_set_string(conf, key, "whu");
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         config_set_string(conf, key, "whd");
         break;
      default:
         if (save_empty)
            config_set_string(conf, key, file_path_str(FILE_PATH_NUL));
         break;
   }
}

/**
 * input_config_save_keybind:
 * @conf               : pointer to config file object
 * @prefix             : prefix name of keybind
 * @base               : base name   of keybind
 * @bind               : pointer to key binding object
 * @kb                 : save keyboard binds
 *
 * Save a key binding to the config file.
 */
void input_config_save_keybind(void *data, const char *prefix,
      const char *base, const struct retro_keybind *bind,
      bool save_empty)
{
   config_file_t *conf = (config_file_t*)data;

   save_keybind_joykey (conf, prefix, base, bind, save_empty);
   save_keybind_axis   (conf, prefix, base, bind, save_empty);
   save_keybind_mbutton(conf, prefix, base, bind, save_empty);
}

/* MIDI */

static midi_driver_t *midi_driver_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(midi_drivers); ++i)
   {
      if (string_is_equal(midi_drivers[i]->ident, ident))
         return midi_drivers[i];
   }

   RARCH_ERR("[MIDI]: Unknown driver \"%s\", falling back to \"null\" driver.\n", ident);

   return &midi_null;
}

const void *midi_driver_find_handle(int index)
{
   if (index < 0 || index >= ARRAY_SIZE(midi_drivers))
      return NULL;

   return midi_drivers[index];
}

const char *midi_driver_find_ident(int index)
{
   if (index < 0 || index >= ARRAY_SIZE(midi_drivers))
      return NULL;

   return midi_drivers[index]->ident;
}

struct string_list *midi_driver_get_avail_inputs(void)
{
   return midi_drv_inputs;
}

struct string_list *midi_driver_get_avail_outputs(void)
{
   return midi_drv_outputs;
}
bool midi_driver_set_all_sounds_off(void)
{
   midi_event_t event;
   uint8_t i;
   uint8_t data[3]     = { 0xB0, 120, 0 };
   bool result         = true;

   if (!midi_drv_data || !midi_drv_output_enabled)
      return false;

   event.data       = data;
   event.data_size  = sizeof(data);
   event.delta_time = 0;

   for (i = 0; i < 16; ++i)
   {
      data[0] = 0xB0 | i;

      if (!midi_drv->write(midi_drv_data, &event))
         result = false;
   }

   if (!midi_drv->flush(midi_drv_data))
      result = false;

   if (!result)
      RARCH_ERR("[MIDI]: All sounds off failed.\n");

   return result;
}

bool midi_driver_set_volume(unsigned volume)
{
   midi_event_t event;
   uint8_t data[8]     = { 0xF0, 0x7F, 0x7F, 0x04, 0x01, 0, 0, 0xF7 };

   if (!midi_drv_data || !midi_drv_output_enabled)
      return false;

   volume = (unsigned)(163.83 * volume + 0.5);
   if (volume > 16383)
      volume = 16383;

   data[5] = (uint8_t)(volume & 0x7F);
   data[6] = (uint8_t)(volume >> 7);

   event.data = data;
   event.data_size = sizeof(data);
   event.delta_time = 0;

   if (!midi_drv->write(midi_drv_data, &event))
   {
      RARCH_ERR("[MIDI]: Volume change failed.\n");
      return false;
   }

   return true;
}

bool midi_driver_init_io_buffers(void)
{
   midi_drv_input_buffer  = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);
   midi_drv_output_buffer = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);

   if (!midi_drv_input_buffer || !midi_drv_output_buffer)
      return false;

   midi_drv_input_event.data = midi_drv_input_buffer;
   midi_drv_input_event.data_size = 0;

   midi_drv_output_event.data = midi_drv_output_buffer;
   midi_drv_output_event.data_size = 0;

   return true;
}

bool midi_driver_init(void)
{
   settings_t *settings             = configuration_settings;
   union string_list_elem_attr attr = {0};
   const char *err_str              = NULL;

   midi_drv_inputs                  = string_list_new();
   midi_drv_outputs                 = string_list_new();

   RARCH_LOG("[MIDI]: Initializing ...\n");

   if (!settings)
      err_str = "settings unavailable";
   else if (!midi_drv_inputs || !midi_drv_outputs)
      err_str = "string_list_new failed";
   else if (!string_list_append(midi_drv_inputs, "Off", attr) ||
         !string_list_append(midi_drv_outputs, "Off", attr))
      err_str = "string_list_append failed";
   else
   {
      char * input  = NULL;
      char * output = NULL;

      midi_drv = midi_driver_find_driver(settings->arrays.midi_driver);
      if (strcmp(midi_drv->ident, settings->arrays.midi_driver))
         strlcpy(settings->arrays.midi_driver, midi_drv->ident,
               sizeof(settings->arrays.midi_driver));

      if (!midi_drv->get_avail_inputs(midi_drv_inputs))
         err_str = "list of input devices unavailable";
      else if (!midi_drv->get_avail_outputs(midi_drv_outputs))
         err_str = "list of output devices unavailable";
      else
      {
         if (string_is_not_equal(settings->arrays.midi_input, "Off"))
         {
            if (string_list_find_elem(midi_drv_inputs, settings->arrays.midi_input))
               input = settings->arrays.midi_input;
            else
            {
               RARCH_WARN("[MIDI]: Input device \"%s\" unavailable.\n",
                     settings->arrays.midi_input);
               strlcpy(settings->arrays.midi_input, "Off",
                     sizeof(settings->arrays.midi_input));
            }
         }

         if (string_is_not_equal(settings->arrays.midi_output, "Off"))
         {
            if (string_list_find_elem(midi_drv_outputs, settings->arrays.midi_output))
               output = settings->arrays.midi_output;
            else
            {
               RARCH_WARN("[MIDI]: Output device \"%s\" unavailable.\n",
                     settings->arrays.midi_output);
               strlcpy(settings->arrays.midi_output, "Off",
                     sizeof(settings->arrays.midi_output));
            }
         }

         midi_drv_data = midi_drv->init(input, output);
         if (!midi_drv_data)
            err_str = "driver init failed";
         else
         {
            midi_drv_input_enabled = input != NULL;
            midi_drv_output_enabled = output != NULL;

            if (!midi_driver_init_io_buffers())
               err_str = "out of memory";
            else
            {
               if (input)
                  RARCH_LOG("[MIDI]: Input device \"%s\".\n", input);
               else
                  RARCH_LOG("[MIDI]: Input disabled.\n");
               if (output)
               {
                  RARCH_LOG("[MIDI]: Output device \"%s\".\n", output);
                  midi_driver_set_volume(settings->uints.midi_volume);
               }
               else
                  RARCH_LOG("[MIDI]: Output disabled.\n");
            }
         }
      }
   }

   if (err_str)
   {
      midi_driver_free();
      RARCH_ERR("[MIDI]: Initialization failed (%s).\n", err_str);
   }
   else
      RARCH_LOG("[MIDI]: Initialized \"%s\" driver.\n", midi_drv->ident);

   return err_str == NULL;
}

void midi_driver_free(void)
{
   if (midi_drv_data)
   {
      midi_drv->free(midi_drv_data);
      midi_drv_data = NULL;
   }

   if (midi_drv_inputs)
   {
      string_list_free(midi_drv_inputs);
      midi_drv_inputs = NULL;
   }
   if (midi_drv_outputs)
   {
      string_list_free(midi_drv_outputs);
      midi_drv_outputs = NULL;
   }

   if (midi_drv_input_buffer)
   {
      free(midi_drv_input_buffer);
      midi_drv_input_buffer = NULL;
   }
   if (midi_drv_output_buffer)
   {
      free(midi_drv_output_buffer);
      midi_drv_output_buffer = NULL;
   }

   midi_drv_input_enabled  = false;
   midi_drv_output_enabled = false;
}

bool midi_driver_set_input(const char *input)
{
   if (!midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_input called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(input, "Off"))
      input = NULL;

   if (!midi_drv->set_input(midi_drv_data, input))
   {
      if (input)
         RARCH_ERR("[MIDI]: Failed to change input device to \"%s\".\n", input);
      else
         RARCH_ERR("[MIDI]: Failed to disable input.\n");
      return false;
   }

   if (input)
      RARCH_LOG("[MIDI]: Input device changed to \"%s\".\n", input);
   else
      RARCH_LOG("[MIDI]: Input disabled.\n");

   midi_drv_input_enabled = input != NULL;

   return true;
}

bool midi_driver_set_output(const char *output)
{
   if (!midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_output called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(output, "Off"))
      output = NULL;

   if (!midi_drv->set_output(midi_drv_data, output))
   {
      if (output)
         RARCH_ERR("[MIDI]: Failed to change output device to \"%s\".\n", output);
      else
         RARCH_ERR("[MIDI]: Failed to disable output.\n");
      return false;
   }

   if (output)
   {
      settings_t *settings    = configuration_settings;

      midi_drv_output_enabled = true;
      RARCH_LOG("[MIDI]: Output device changed to \"%s\".\n", output);

      if (settings)
         midi_driver_set_volume(settings->uints.midi_volume);
      else
         RARCH_ERR("[MIDI]: Volume change failed (settings unavailable).\n");
   }
   else
   {
      midi_drv_output_enabled = false;
      RARCH_LOG("[MIDI]: Output disabled.\n");
   }

   return true;
}

bool midi_driver_input_enabled(void)
{
   return midi_drv_input_enabled;
}

bool midi_driver_output_enabled(void)
{
   return midi_drv_output_enabled;
}

bool midi_driver_read(uint8_t *byte)
{
   static int i;

   if (!midi_drv_data || !midi_drv_input_enabled || !byte)
   {
#ifdef DEBUG
      if (!midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_read called on uninitialized driver.\n");
      else if (!midi_drv_input_enabled)
         RARCH_ERR("[MIDI]: midi_driver_read called when input is disabled.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_read called with null pointer.\n");
#endif
      return false;
   }

   if (i == midi_drv_input_event.data_size)
   {
      midi_drv_input_event.data_size = MIDI_DRIVER_BUF_SIZE;
      if (!midi_drv->read(midi_drv_data, &midi_drv_input_event))
      {
         midi_drv_input_event.data_size = i;
         return false;
      }

      i = 0;

#ifdef DEBUG
      if (midi_drv_input_event.data_size == 1)
         RARCH_LOG("[MIDI]: In [0x%02X].\n",
               midi_drv_input_event.data[0]);
      else if (midi_drv_input_event.data_size == 2)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X].\n",
               midi_drv_input_event.data[0],
               midi_drv_input_event.data[1]);
      else if (midi_drv_input_event.data_size == 3)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X, 0x%02X].\n",
               midi_drv_input_event.data[0],
               midi_drv_input_event.data[1],
               midi_drv_input_event.data[2]);
      else
         RARCH_LOG("[MIDI]: In [0x%02X, ...], size %u.\n",
               midi_drv_input_event.data[0],
               midi_drv_input_event.data_size);
#endif
   }

   *byte = midi_drv_input_event.data[i++];

   return true;
}

bool midi_driver_write(uint8_t byte, uint32_t delta_time)
{
   static int event_size;

   if (!midi_drv_data || !midi_drv_output_enabled)
   {
#ifdef DEBUG
      if (!midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_write called on uninitialized driver.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_write called when output is disabled.\n");
#endif
      return false;
   }

   if (byte >= 0x80)
   {
      if (midi_drv_output_event.data_size &&
            midi_drv_output_event.data[0] == 0xF0)
      {
         if (byte == 0xF7)
            event_size = (int)midi_drv_output_event.data_size + 1;
         else
         {
            if (!midi_drv->write(midi_drv_data, &midi_drv_output_event))
               return false;

#ifdef DEBUG
            if (midi_drv_output_event.data_size == 1)
               RARCH_LOG("[MIDI]: Out [0x%02X].\n",
                     midi_drv_output_event.data[0]);
            else if (midi_drv_output_event.data_size == 2)
               RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
                     midi_drv_output_event.data[0],
                     midi_drv_output_event.data[1]);
            else if (midi_drv_output_event.data_size == 3)
               RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
                     midi_drv_output_event.data[0],
                     midi_drv_output_event.data[1],
                     midi_drv_output_event.data[2]);
            else
               RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
                     midi_drv_output_event.data[0],
                     midi_drv_output_event.data_size);
#endif

            midi_drv_output_pending          = true;
            event_size                       = (int)midi_driver_get_event_size(byte);
            midi_drv_output_event.data_size  = 0;
            midi_drv_output_event.delta_time = 0;
         }
      }
      else
      {
         event_size                          = (int)midi_driver_get_event_size(byte);
         midi_drv_output_event.data_size     = 0;
         midi_drv_output_event.delta_time    = 0;
      }
   }

   if (midi_drv_output_event.data_size < MIDI_DRIVER_BUF_SIZE)
   {
      midi_drv_output_event.data[midi_drv_output_event.data_size] = byte;
      ++midi_drv_output_event.data_size;
      midi_drv_output_event.delta_time += delta_time;
   }
   else
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: Output event dropped.\n");
#endif
      return false;
   }

   if (midi_drv_output_event.data_size == event_size)
   {
      if (!midi_drv->write(midi_drv_data, &midi_drv_output_event))
         return false;

#ifdef DEBUG
      if (midi_drv_output_event.data_size == 1)
         RARCH_LOG("[MIDI]: Out [0x%02X].\n",
               midi_drv_output_event.data[0]);
      else if (midi_drv_output_event.data_size == 2)
         RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
               midi_drv_output_event.data[0],
               midi_drv_output_event.data[1]);
      else if (midi_drv_output_event.data_size == 3)
         RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
               midi_drv_output_event.data[0],
               midi_drv_output_event.data[1],
               midi_drv_output_event.data[2]);
      else
         RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
               midi_drv_output_event.data[0],
               midi_drv_output_event.data_size);
#endif

      midi_drv_output_pending = true;
   }

   return true;
}

bool midi_driver_flush(void)
{
   if (!midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_flush called on uninitialized driver.\n");
#endif
      return false;
   }

   if (midi_drv_output_pending)
      midi_drv_output_pending = !midi_drv->flush(midi_drv_data);

   return !midi_drv_output_pending;
}

size_t midi_driver_get_event_size(uint8_t status)
{
   if (status < 0x80)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_get_event_size called with invalid status.\n");
#endif
      return 0;
   }

   return midi_drv_ev_sizes[status - 0x80];
}

/* AUDIO */

#ifdef HAVE_AUDIOMIXER
static void audio_mixer_play_stop_sequential_cb(
      audio_mixer_sound_t *sound, unsigned reason);
static void audio_mixer_play_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason);
static void audio_mixer_menu_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason);
#endif

static enum resampler_quality audio_driver_get_resampler_quality(void)
{
   settings_t *settings    = configuration_settings;

   if (!settings)
      return RESAMPLER_QUALITY_DONTCARE;

   return (enum resampler_quality)settings->uints.audio_resampler_quality;
}

#ifdef HAVE_AUDIOMIXER
audio_mixer_stream_t *audio_driver_mixer_get_stream(unsigned i)
{
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return NULL;
   return &audio_mixer_streams[i];
}

const char *audio_driver_mixer_get_stream_name(unsigned i)
{
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return "N/A";
   if (!string_is_empty(audio_mixer_streams[i].name))
      return audio_mixer_streams[i].name;
   return "N/A";
}

static void audio_driver_mixer_deinit(void)
{
   unsigned i;

   audio_mixer_active = false;

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_driver_mixer_stop_stream(i);
      audio_driver_mixer_remove_stream(i);
   }

   audio_mixer_done();
}

#endif

/**
 * audio_compute_buffer_statistics:
 *
 * Computes audio buffer statistics.
 *
 **/
static bool audio_compute_buffer_statistics(audio_statistics_t *stats)
{
   unsigned i, low_water_size, high_water_size, avg, stddev;
   uint64_t accum                = 0;
   uint64_t accum_var            = 0;
   unsigned low_water_count      = 0;
   unsigned high_water_count     = 0;
   unsigned samples              = MIN(
         (unsigned)audio_driver_free_samples_count,
         AUDIO_BUFFER_FREE_SAMPLES_COUNT);

   if (!stats || samples < 3)
      return false;

   stats->samples                = (unsigned)audio_driver_free_samples_count;

#ifdef WARPUP
   /* uint64 to double not implemented, fair chance
    * signed int64 to double doesn't exist either */
   /* https://forums.libretro.com/t/unsupported-platform-help/13903/ */
   (void)stddev;
#elif defined(_MSC_VER) && _MSC_VER <= 1200
   /* FIXME: error C2520: conversion from unsigned __int64
    * to double not implemented, use signed __int64 */
   (void)stddev;
#else
   for (i = 1; i < samples; i++)
      accum += audio_driver_free_samples_buf[i];

   avg = (unsigned)accum / (samples - 1);

   for (i = 1; i < samples; i++)
   {
      int diff     = avg - audio_driver_free_samples_buf[i];
      accum_var   += diff * diff;
   }

   stddev                                = (unsigned)
      sqrt((double)accum_var / (samples - 2));

   stats->average_buffer_saturation      = (1.0f - (float)avg
         / audio_driver_buffer_size) * 100.0;
   stats->std_deviation_percentage       = ((float)stddev
         / audio_driver_buffer_size)  * 100.0;
#endif

   low_water_size  = (unsigned)(audio_driver_buffer_size * 3 / 4);
   high_water_size = (unsigned)(audio_driver_buffer_size     / 4);

   for (i = 1; i < samples; i++)
   {
      if (audio_driver_free_samples_buf[i] >= low_water_size)
         low_water_count++;
      else if (audio_driver_free_samples_buf[i] <= high_water_size)
         high_water_count++;
   }

   stats->close_to_underrun      = (100.0 * low_water_count)  / (samples - 1);
   stats->close_to_blocking      = (100.0 * high_water_count) / (samples - 1);

   return true;
}

static void report_audio_buffer_statistics(void)
{
   audio_statistics_t audio_stats = {0.0f};
   if (!audio_compute_buffer_statistics(&audio_stats))
      return;

#ifdef DEBUG
   RARCH_LOG("[Audio]: Average audio buffer saturation: %.2f %%,"
         " standard deviation (percentage points): %.2f %%.\n"
         "[Audio]: Amount of time spent close to underrun: %.2f %%."
         " Close to blocking: %.2f %%.\n",
         audio_stats.average_buffer_saturation,
         audio_stats.std_deviation_percentage,
         audio_stats.close_to_underrun,
         audio_stats.close_to_blocking);
#endif
}

/**
 * audio_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to audio driver at index. Can be NULL
 * if nothing found.
 **/
const void *audio_driver_find_handle(int idx)
{
   const void *drv = audio_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * audio_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of audio driver at index. Can be NULL
 * if nothing found.
 **/
const char *audio_driver_find_ident(int idx)
{
   const audio_driver_t *drv = audio_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char *config_get_audio_driver_options(void)
{
   return char_list_new_special(STRING_LIST_AUDIO_DRIVERS, NULL);
}

static void audio_driver_deinit_resampler(void)
{
   if (audio_driver_resampler && audio_driver_resampler_data)
      audio_driver_resampler->free(audio_driver_resampler_data);
   audio_driver_resampler      = NULL;
   audio_driver_resampler_data = NULL;
}


static bool audio_driver_deinit_internal(void)
{
   settings_t *settings    = configuration_settings;

   if (current_audio && current_audio->free)
   {
      if (audio_driver_context_audio_data)
         current_audio->free(audio_driver_context_audio_data);
      audio_driver_context_audio_data = NULL;
   }

   if (audio_driver_output_samples_conv_buf)
      free(audio_driver_output_samples_conv_buf);
   audio_driver_output_samples_conv_buf = NULL;

   audio_driver_data_ptr                = 0;

   if (audio_driver_rewind_buf)
      free(audio_driver_rewind_buf);
   audio_driver_rewind_buf   = NULL;

   audio_driver_rewind_size  = 0;

   if (!settings->bools.audio_enable)
   {
      audio_driver_active = false;
      return false;
   }

   audio_driver_deinit_resampler();

   if (audio_driver_input_data)
      free(audio_driver_input_data);
   audio_driver_input_data = NULL;

   if (audio_driver_output_samples_buf)
      free(audio_driver_output_samples_buf);
   audio_driver_output_samples_buf = NULL;

   command_event(CMD_EVENT_DSP_FILTER_DEINIT, NULL);

   report_audio_buffer_statistics();

   return true;
}

static bool audio_driver_free_devices_list(void)
{
   if (!current_audio || !current_audio->device_list_free
         || !audio_driver_context_audio_data)
      return false;
   current_audio->device_list_free(audio_driver_context_audio_data,
         audio_driver_devices_list);
   audio_driver_devices_list = NULL;
   return true;
}

static bool audio_driver_deinit(void)
{
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_deinit();
#endif
   audio_driver_free_devices_list();

   if (!audio_driver_deinit_internal())
      return false;
   return true;
}

static bool audio_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings    = configuration_settings;

   drv.label = "audio_driver";
   drv.s     = settings->arrays.audio_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = (int)drv.len;

   if (i >= 0)
      current_audio = (const audio_driver_t*)audio_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any audio driver named \"%s\"\n",
               settings->arrays.audio_driver);
         RARCH_LOG_OUTPUT("Available audio drivers are:\n");
         for (d = 0; audio_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", audio_driver_find_ident(d));
         RARCH_WARN("Going to default to first audio driver...\n");
      }

      current_audio = (const audio_driver_t*)audio_driver_find_handle(0);

      if (!current_audio)
         retroarch_fail(1, "audio_driver_find()");
   }

   return true;
}


static bool audio_driver_init_internal(bool audio_cb_inited)
{
   unsigned new_rate     = 0;
   float   *aud_inp_data = NULL;
   float *samples_buf    = NULL;
   int16_t *conv_buf     = NULL;
   int16_t *rewind_buf   = NULL;
   size_t max_bufsamples = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   settings_t *settings  = configuration_settings;
   /* Accomodate rewind since at some point we might have two full buffers. */
   size_t outsamples_max = AUDIO_CHUNK_SIZE_NONBLOCKING * 2 * AUDIO_MAX_RATIO *
      settings->floats.slowmotion_ratio;

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();

   conv_buf = (int16_t*)malloc(outsamples_max
         * sizeof(int16_t));
   /* Used for recording even if audio isn't enabled. */
   retro_assert(conv_buf != NULL);

   if (!conv_buf)
      goto error;

   audio_driver_output_samples_conv_buf = conv_buf;
   audio_driver_chunk_block_size        = AUDIO_CHUNK_SIZE_BLOCKING;
   audio_driver_chunk_nonblock_size     = AUDIO_CHUNK_SIZE_NONBLOCKING;
   audio_driver_chunk_size              = audio_driver_chunk_block_size;

   /* Needs to be able to hold full content of a full max_bufsamples
    * in addition to its own. */
   rewind_buf = (int16_t*)malloc(max_bufsamples * sizeof(int16_t));
   retro_assert(rewind_buf != NULL);

   if (!rewind_buf)
      goto error;

   audio_driver_rewind_buf              = rewind_buf;
   audio_driver_rewind_size             = max_bufsamples;

   if (!settings->bools.audio_enable)
   {
      audio_driver_active = false;
      return false;
   }

   audio_driver_find_driver();
#ifdef HAVE_THREADS
   if (audio_cb_inited)
   {
      audio_is_threaded = true;
      RARCH_LOG("[Audio]: Starting threaded audio driver ...\n");
      if (!audio_init_thread(
               &current_audio,
               &audio_driver_context_audio_data,
               *settings->arrays.audio_device
               ? settings->arrays.audio_device : NULL,
               settings->uints.audio_out_rate, &new_rate,
               settings->uints.audio_latency,
               settings->uints.audio_block_frames,
               current_audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
         retroarch_fail(1, "audio_driver_init_internal()");
      }
   }
   else
#endif
   {
      audio_is_threaded = false;
      audio_driver_context_audio_data =
         current_audio->init(*settings->arrays.audio_device ?
               settings->arrays.audio_device : NULL,
               settings->uints.audio_out_rate,
               settings->uints.audio_latency,
               settings->uints.audio_block_frames,
               &new_rate);
   }

   if (new_rate != 0)
   {
      configuration_set_int(settings, settings->uints.audio_out_rate, new_rate);
   }

   if (!audio_driver_context_audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      audio_driver_active = false;
   }

   audio_driver_use_float = false;
   if (     audio_driver_active
         && current_audio->use_float(audio_driver_context_audio_data))
      audio_driver_use_float = true;

   if (!settings->bools.audio_sync && audio_driver_active)
   {
      command_event(CMD_EVENT_AUDIO_SET_NONBLOCKING_STATE, NULL);
      audio_driver_chunk_size = audio_driver_chunk_nonblock_size;
   }

   if (audio_driver_input <= 0.0f)
   {
      /* Should never happen. */
      RARCH_WARN("Input rate is invalid (%.3f Hz). Using output rate (%u Hz).\n",
            audio_driver_input, settings->uints.audio_out_rate);
      audio_driver_input = settings->uints.audio_out_rate;
   }

   audio_source_ratio_original   = audio_source_ratio_current =
      (double)settings->uints.audio_out_rate / audio_driver_input;

   if (!retro_resampler_realloc(
            &audio_driver_resampler_data,
            &audio_driver_resampler,
            settings->arrays.audio_resampler,
            audio_driver_get_resampler_quality(),
            audio_source_ratio_original))
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n",
            settings->arrays.audio_resampler);
      audio_driver_active = false;
   }

   aud_inp_data = (float*)malloc(max_bufsamples * sizeof(float));
   retro_assert(aud_inp_data != NULL);

   if (!aud_inp_data)
      goto error;

   audio_driver_input_data = aud_inp_data;
   audio_driver_data_ptr   = 0;

   retro_assert(settings->uints.audio_out_rate <
         audio_driver_input * AUDIO_MAX_RATIO);

   samples_buf = (float*)malloc(outsamples_max * sizeof(float));

   retro_assert(samples_buf != NULL);

   if (!samples_buf)
      goto error;

   audio_driver_output_samples_buf = samples_buf;
   audio_driver_control            = false;

   if (
         !audio_cb_inited
         && audio_driver_active
         && settings->bools.audio_rate_control
         )
   {
      /* Audio rate control requires write_avail
       * and buffer_size to be implemented. */
      if (current_audio->buffer_size)
      {
         audio_driver_buffer_size =
            current_audio->buffer_size(audio_driver_context_audio_data);
         audio_driver_control     = true;
      }
      else
         RARCH_WARN("Audio rate control was desired, but driver does not support needed features.\n");
   }

   command_event(CMD_EVENT_DSP_FILTER_INIT, NULL);

   audio_driver_free_samples_count = 0;

#ifdef HAVE_AUDIOMIXER
   audio_mixer_init(settings->uints.audio_out_rate);
#endif

   /* Threaded driver is initially stopped. */
   if (
         audio_driver_active
         && audio_cb_inited
         )
      audio_driver_start(false);

   return true;

error:
   return audio_driver_deinit();
}

#define audio_driver_set_nonblocking_state_internal(val, chunk_size) \
   if (audio_driver_active && audio_driver_context_audio_data) \
      current_audio->set_nonblock_state(audio_driver_context_audio_data, val); \
   audio_driver_chunk_size = chunk_size;


void audio_driver_set_nonblocking_state(bool enable)
{
   settings_t *settings  = configuration_settings;
   audio_driver_set_nonblocking_state_internal(
         settings->bools.audio_sync ? enable : true,
         enable ?
         audio_driver_chunk_nonblock_size :
         audio_driver_chunk_block_size);
}

/**
 * audio_driver_flush:
 * @data                 : pointer to audio buffer.
 * @right                : amount of samples to write.
 *
 * Writes audio samples to audio driver. Will first
 * perform DSP processing (if enabled) and resampling.
 **/
static void audio_driver_flush(const int16_t *data, size_t samples,
      bool is_slowmotion)
{
   struct resampler_data src_data;
   float audio_volume_gain           = !audio_driver_mute_enable ?
      audio_driver_volume_gain : 0.0f;

   src_data.data_out                 = NULL;
   src_data.output_frames            = 0;

   convert_s16_to_float(audio_driver_input_data, data, samples,
         audio_volume_gain);

   src_data.data_in                  = audio_driver_input_data;
   src_data.input_frames             = samples >> 1;

   if (audio_driver_dsp)
   {
      struct retro_dsp_data dsp_data;

      dsp_data.input                 = NULL;
      dsp_data.input_frames          = 0;
      dsp_data.output                = NULL;
      dsp_data.output_frames         = 0;

      dsp_data.input                 = audio_driver_input_data;
      dsp_data.input_frames          = (unsigned)(samples >> 1);

      retro_dsp_filter_process(audio_driver_dsp, &dsp_data);

      if (dsp_data.output)
      {
         src_data.data_in            = dsp_data.output;
         src_data.input_frames       = dsp_data.output_frames;
      }
   }

   src_data.data_out = audio_driver_output_samples_buf;

   if (audio_driver_control)
   {
      /* Readjust the audio input rate. */
      int      half_size   = (int)(audio_driver_buffer_size / 2);
      int      avail       =
         (int)current_audio->write_avail(audio_driver_context_audio_data);
      int      delta_mid   = avail - half_size;
      double   direction   = (double)delta_mid / half_size;
      double   adjust      = 1.0 + audio_driver_rate_control_delta * direction;
      unsigned write_idx   = audio_driver_free_samples_count++ &
         (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);

      audio_driver_free_samples_buf
         [write_idx]               = avail;
      audio_source_ratio_current   =
         audio_source_ratio_original * adjust;

#if 0
      if (verbosity_is_enabled())
      {
         RARCH_LOG_OUTPUT("[Audio]: Audio buffer is %u%% full\n",
               (unsigned)(100 - (avail * 100) / audio_driver_buffer_size));
         RARCH_LOG_OUTPUT("[Audio]: New rate: %lf, Orig rate: %lf\n",
               audio_source_ratio_current,
               audio_source_ratio_original);
      }
#endif
   }

   src_data.ratio           = audio_source_ratio_current;

   if (is_slowmotion)
      src_data.ratio       *= configuration_settings->floats.slowmotion_ratio;

   audio_driver_resampler->process(audio_driver_resampler_data, &src_data);

#ifdef HAVE_AUDIOMIXER
   if (audio_mixer_active)
   {
      bool override     = audio_driver_mixer_mute_enable ? true :
         (audio_driver_mixer_volume_gain != 1.0f) ? true : false;
      float mixer_gain  = !audio_driver_mixer_mute_enable ?
         audio_driver_mixer_volume_gain : 0.0f;
      audio_mixer_mix(audio_driver_output_samples_buf,
            src_data.output_frames, mixer_gain, override);
   }
#endif

   {
      const void *output_data = audio_driver_output_samples_buf;
      unsigned output_frames  = (unsigned)src_data.output_frames;

      if (audio_driver_use_float)
         output_frames  *= sizeof(float);
      else
      {
         convert_float_to_s16(audio_driver_output_samples_conv_buf,
               (const float*)output_data, output_frames * 2);

         output_data     = audio_driver_output_samples_conv_buf;
         output_frames  *= sizeof(int16_t);
      }

      if (current_audio->write(audio_driver_context_audio_data,
               output_data, output_frames * 2) < 0)
         audio_driver_active = false;
   }
}

/**
 * audio_driver_sample:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function.
 **/
static void audio_driver_sample(int16_t left, int16_t right)
{
   if (audio_suspended)
      return;

   audio_driver_output_samples_conv_buf[audio_driver_data_ptr++] = left;
   audio_driver_output_samples_conv_buf[audio_driver_data_ptr++] = right;

   if (audio_driver_data_ptr < audio_driver_chunk_size)
      return;

   if (recording_data && recording_driver && recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = audio_driver_output_samples_conv_buf;
      ffemu_data.frames                  = audio_driver_data_ptr / 2;

      recording_driver->push_audio(recording_data, &ffemu_data);
   }

   if (!(runloop_paused                ||
		   !audio_driver_active     ||
		   !audio_driver_input_data ||
		   !audio_driver_output_samples_buf))
      audio_driver_flush(audio_driver_output_samples_conv_buf,
            audio_driver_data_ptr, runloop_slowmotion);

   audio_driver_data_ptr = 0;
}

#ifdef HAVE_MENU
static void audio_driver_menu_sample(void)
{
   static int16_t samples_buf[1024]       = {0};
   struct retro_system_av_info *av_info   = &video_driver_av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;
   unsigned sample_count                  = (info->sample_rate / info->fps) * 2;
   bool check_flush                       = !(
         runloop_paused           ||
         !audio_driver_active     ||
         !audio_driver_input_data ||
         !audio_driver_output_samples_buf);

   while (sample_count > 1024)
   {
      if (recording_data && recording_driver && recording_driver->push_audio)
      {
         struct record_audio_data ffemu_data;

         ffemu_data.data                    = samples_buf;
         ffemu_data.frames                  = 1024 / 2;

         recording_driver->push_audio(recording_data, &ffemu_data);
      }
      if (check_flush)
         audio_driver_flush(samples_buf, 1024, runloop_slowmotion);
      sample_count -= 1024;
   }
   if (recording_data && recording_driver && recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = samples_buf;
      ffemu_data.frames                  = sample_count / 2;

      recording_driver->push_audio(recording_data, &ffemu_data);
   }
   if (check_flush)
      audio_driver_flush(samples_buf, sample_count, runloop_slowmotion);
}
#endif

/**
 * audio_driver_sample_batch:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
static size_t audio_driver_sample_batch(const int16_t *data, size_t frames)
{
   if (frames > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
      frames = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;

   if (audio_suspended)
      return frames;

   if (recording_data && recording_driver && recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = data;
      ffemu_data.frames                  = (frames << 1) / 2;

      recording_driver->push_audio(recording_data, &ffemu_data);
   }

   if (!(
         runloop_paused           ||
         !audio_driver_active     ||
         !audio_driver_input_data ||
         !audio_driver_output_samples_buf))
      audio_driver_flush(data, frames << 1, runloop_slowmotion);

   return frames;
}

/**
 * audio_driver_sample_rewind:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function (rewind version).
 * This callback function will be used instead of
 * audio_driver_sample when rewinding is activated.
 **/
static void audio_driver_sample_rewind(int16_t left, int16_t right)
{
   if (audio_driver_rewind_ptr == 0)
      return;

   audio_driver_rewind_buf[--audio_driver_rewind_ptr] = right;
   audio_driver_rewind_buf[--audio_driver_rewind_ptr] = left;
}

/**
 * audio_driver_sample_batch_rewind:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function (rewind version).
 *
 * This callback function will be used instead of
 * audio_driver_sample_batch when rewinding is activated.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
static size_t audio_driver_sample_batch_rewind(
      const int16_t *data, size_t frames)
{
   size_t i;
   size_t samples   = frames << 1;

   for (i = 0; i < samples; i++)
   {
      if (audio_driver_rewind_ptr > 0)
         audio_driver_rewind_buf[--audio_driver_rewind_ptr] = data[i];
   }

   return frames;
}

void audio_driver_dsp_filter_free(void)
{
   if (audio_driver_dsp)
      retro_dsp_filter_free(audio_driver_dsp);
   audio_driver_dsp = NULL;
}

bool audio_driver_dsp_filter_init(const char *device)
{
   struct string_list *plugs     = NULL;
#if defined(HAVE_DYLIB) && !defined(HAVE_FILTERS_BUILTIN)
   char *basedir   = (char*)calloc(PATH_MAX_LENGTH, sizeof(*basedir));
   char *ext_name  = (char*)calloc(PATH_MAX_LENGTH, sizeof(*ext_name));
   size_t str_size = PATH_MAX_LENGTH * sizeof(char);
   fill_pathname_basedir(basedir, device, str_size);

   if (!frontend_driver_get_core_extension(ext_name, str_size))
   {
      free(ext_name);
      free(basedir);
      return false;
   }

   plugs = dir_list_new(basedir, ext_name, false, true, false, false);
   free(ext_name);
   free(basedir);
   if (!plugs)
      return false;
#endif
   audio_driver_dsp = retro_dsp_filter_new(
         device, plugs, audio_driver_input);
   if (!audio_driver_dsp)
      return false;

   return true;
}

void audio_driver_set_buffer_size(size_t bufsize)
{
   audio_driver_buffer_size = bufsize;
}

static void audio_driver_monitor_adjust_system_rates(void)
{
   float timing_skew;
   settings_t *settings                   = configuration_settings;
   float video_refresh_rate               = settings->floats.video_refresh_rate;
   float max_timing_skew                  = settings->floats.audio_max_timing_skew;
   struct retro_system_av_info *av_info   = &video_driver_av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;

   if (info->sample_rate <= 0.0)
      return;

   timing_skew             = fabs(1.0f - info->fps / video_refresh_rate);
   audio_driver_input      = info->sample_rate;

   if (timing_skew <= max_timing_skew && !settings->bools.vrr_runloop_enable)
      audio_driver_input *= (video_refresh_rate / info->fps);

   RARCH_LOG("[Audio]: Set audio input rate to: %.2f Hz.\n",
         audio_driver_input);
}

void audio_driver_setup_rewind(void)
{
   unsigned i;

   /* Push audio ready to be played. */
   audio_driver_rewind_ptr = audio_driver_rewind_size;

   for (i = 0; i < audio_driver_data_ptr; i += 2)
   {
      if (audio_driver_rewind_ptr > 0)
         audio_driver_rewind_buf[--audio_driver_rewind_ptr] =
            audio_driver_output_samples_conv_buf[i + 1];

      if (audio_driver_rewind_ptr > 0)
         audio_driver_rewind_buf[--audio_driver_rewind_ptr] =
            audio_driver_output_samples_conv_buf[i + 0];
   }

   audio_driver_data_ptr = 0;
}


bool audio_driver_get_devices_list(void **data)
{
   struct string_list**ptr = (struct string_list**)data;
   if (!ptr)
      return false;
   *ptr = audio_driver_devices_list;
   return true;
}

#ifdef HAVE_AUDIOMIXER
bool audio_driver_mixer_extension_supported(const char *ext)
{
   union string_list_elem_attr attr;
   unsigned i;
   bool ret                      = false;
   struct string_list *str_list  = string_list_new();

   attr.i = 0;

#ifdef HAVE_STB_VORBIS
   string_list_append(str_list, "ogg", attr);
#endif
#ifdef HAVE_IBXM
   string_list_append(str_list, "mod", attr);
   string_list_append(str_list, "s3m", attr);
   string_list_append(str_list, "xm", attr);
#endif
#ifdef HAVE_DR_FLAC
   string_list_append(str_list, "flac", attr);
#endif
#ifdef HAVE_DR_MP3
   string_list_append(str_list, "mp3", attr);
#endif
   string_list_append(str_list, "wav", attr);

   for (i = 0; i < str_list->size; i++)
   {
      const char *str_ext = str_list->elems[i].data;
      if (string_is_equal_noncase(str_ext, ext))
      {
         ret = true;
         break;
      }
   }

   string_list_free(str_list);

   return ret;
}

static int audio_mixer_find_index(audio_mixer_sound_t *sound)
{
   unsigned i;

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_mixer_sound_t *handle = audio_mixer_streams[i].handle;
      if (handle == sound)
         return i;
   }
   return -1;
}

static void audio_mixer_play_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (!string_is_empty(audio_mixer_streams[i].name))
               free(audio_mixer_streams[i].name);

            audio_mixer_streams[i].name    = NULL;
            audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
            audio_mixer_streams[i].volume  = 0.0f;
            audio_mixer_streams[i].buf     = NULL;
            audio_mixer_streams[i].stop_cb = NULL;
            audio_mixer_streams[i].handle  = NULL;
            audio_mixer_streams[i].voice   = NULL;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_menu_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;
            audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
            audio_mixer_streams[i].volume  = 0.0f;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_play_stop_sequential_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (!string_is_empty(audio_mixer_streams[i].name))
               free(audio_mixer_streams[i].name);

            if (i < AUDIO_MIXER_MAX_STREAMS)
               audio_mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_USER;
            else
               audio_mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_SYSTEM;

            audio_mixer_streams[i].name    = NULL;
            audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
            audio_mixer_streams[i].volume  = 0.0f;
            audio_mixer_streams[i].buf     = NULL;
            audio_mixer_streams[i].stop_cb = NULL;
            audio_mixer_streams[i].handle  = NULL;
            audio_mixer_streams[i].voice   = NULL;

            i++;

            for (; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
            {
               if (audio_mixer_streams[i].state == AUDIO_STREAM_STATE_STOPPED)
               {
                  audio_driver_mixer_play_stream_sequential(i);
                  break;
               }
            }
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static bool audio_driver_mixer_get_free_stream_slot(unsigned *id, enum audio_mixer_stream_type type)
{
   unsigned i     = (type == AUDIO_STREAM_TYPE_USER) ? 0                       : AUDIO_MIXER_MAX_STREAMS;
   unsigned count = (type == AUDIO_STREAM_TYPE_USER) ? AUDIO_MIXER_MAX_STREAMS : AUDIO_MIXER_MAX_SYSTEM_STREAMS;
   for (; i < count; i++)
   {
      if (audio_mixer_streams[i].state == AUDIO_STREAM_STATE_NONE)
      {
         *id = i;
         return true;
      }
   }

   return false;
}

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params)
{
   unsigned free_slot            = 0;
   audio_mixer_voice_t *voice    = NULL;
   audio_mixer_sound_t *handle   = NULL;
   audio_mixer_stop_cb_t stop_cb = audio_mixer_play_stop_cb;
   bool looped                   = false;
   void *buf                     = NULL;

   if (params->stream_type == AUDIO_STREAM_TYPE_NONE)
      return false;

   switch (params->slot_selection_type)
   {
      case AUDIO_MIXER_SLOT_SELECTION_MANUAL:
         free_slot = params->slot_selection_idx;
         break;
      case AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC:
      default:
         if (!audio_driver_mixer_get_free_stream_slot(
                  &free_slot, params->stream_type))
            return false;
         break;
   }

   if (params->state == AUDIO_STREAM_STATE_NONE)
      return false;

   buf = malloc(params->bufsize);

   if (!buf)
      return false;

   memcpy(buf, params->buf, params->bufsize);

   switch (params->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle = audio_mixer_load_wav(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_OGG:
         handle = audio_mixer_load_ogg(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_MOD:
         handle = audio_mixer_load_mod(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_DR_FLAC
         handle = audio_mixer_load_flac(buf, (int32_t)params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
         handle = audio_mixer_load_mp3(buf, (int32_t)params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   if (!handle)
   {
      free(buf);
      return false;
   }

   switch (params->state)
   {
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
         looped = true;
         voice = audio_mixer_play(handle, looped, params->volume, stop_cb);
         break;
      case AUDIO_STREAM_STATE_PLAYING:
         voice = audio_mixer_play(handle, looped, params->volume, stop_cb);
         break;
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         stop_cb = audio_mixer_play_stop_sequential_cb;
         voice = audio_mixer_play(handle, looped, params->volume, stop_cb);
         break;
      default:
         break;
   }

   audio_mixer_active                     = true;

   audio_mixer_streams[free_slot].name    = !string_is_empty(params->basename) ? strdup(params->basename) : NULL;
   audio_mixer_streams[free_slot].buf     = buf;
   audio_mixer_streams[free_slot].handle  = handle;
   audio_mixer_streams[free_slot].voice   = voice;
   audio_mixer_streams[free_slot].stream_type = params->stream_type;
   audio_mixer_streams[free_slot].type    = params->type;
   audio_mixer_streams[free_slot].state   = params->state;
   audio_mixer_streams[free_slot].volume  = params->volume;
   audio_mixer_streams[free_slot].stop_cb = stop_cb;

   return true;
}

enum audio_mixer_state audio_driver_mixer_get_stream_state(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return AUDIO_STREAM_STATE_NONE;

   return audio_mixer_streams[i].state;
}

static void audio_driver_mixer_play_stream_internal(unsigned i, unsigned type)
{
   bool set_state              = false;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (audio_mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_STOPPED:
         audio_mixer_streams[i].voice = audio_mixer_play(audio_mixer_streams[i].handle,
               (type == AUDIO_STREAM_STATE_PLAYING_LOOPED) ? true : false,
               1.0f, audio_mixer_streams[i].stop_cb);
         set_state = true;
         break;
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (set_state)
      audio_mixer_streams[i].state   = (enum audio_mixer_state)type;
}

static void audio_driver_load_menu_bgm_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   bool contentless = false;
   bool is_inited = false;

   content_get_status(&contentless, &is_inited);

   if (!is_inited)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
}

void audio_driver_load_menu_sounds(void)
{
   settings_t *settings              = configuration_settings;
   const char *path_ok               = NULL;
   const char *path_cancel           = NULL;
   const char *path_notice           = NULL;
   const char *path_bgm              = NULL;
   struct string_list *list          = NULL;
   struct string_list *list_fallback = NULL;
   unsigned i                        = 0;
   char *sounds_path                 = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   char *sounds_fallback_path        = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));

   sounds_path[0] = sounds_fallback_path[0] = '\0';

   fill_pathname_join(
         sounds_fallback_path,
         settings->paths.directory_assets,
         "sounds",
         PATH_MAX_LENGTH * sizeof(char)
   );

   fill_pathname_application_special(
         sounds_path,
         PATH_MAX_LENGTH * sizeof(char),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS);

   list = dir_list_new(sounds_path, MENU_SOUND_FORMATS, false, false, false, false);
   list_fallback = dir_list_new(sounds_fallback_path, MENU_SOUND_FORMATS, false, false, false, false);

   if (!list)
   {
      list = list_fallback;
      list_fallback = NULL;
   }

   if (!list || list->size == 0)
      goto end;

   if (list_fallback && list_fallback->size > 0)
   {
      for (i = 0; i < list_fallback->size; i++)
      {
         if (list->size == 0 || !string_list_find_elem(list, list_fallback->elems[i].data))
         {
            union string_list_elem_attr attr = {0};
            string_list_append(list, list_fallback->elems[i].data, attr);
         }
      }
   }

   for (i = 0; i < list->size; i++)
   {
      const char *path = list->elems[i].data;
      const char *ext = path_get_extension(path);
      char basename_noext[PATH_MAX_LENGTH];

      basename_noext[0] = '\0';

      fill_pathname_base_noext(basename_noext, path, sizeof(basename_noext));

      if (audio_driver_mixer_extension_supported(ext))
      {
         if (string_is_equal_noncase(basename_noext, "ok"))
            path_ok = path;
         if (string_is_equal_noncase(basename_noext, "cancel"))
            path_cancel = path;
         if (string_is_equal_noncase(basename_noext, "notice"))
            path_notice = path;
         if (string_is_equal_noncase(basename_noext, "bgm"))
            path_bgm = path;
      }
   }

   if (path_ok && settings->bools.audio_enable_menu_ok)
      task_push_audio_mixer_load(path_ok, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_OK);
   if (path_cancel && settings->bools.audio_enable_menu_cancel)
      task_push_audio_mixer_load(path_cancel, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
   if (path_notice && settings->bools.audio_enable_menu_notice)
      task_push_audio_mixer_load(path_notice, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_NOTICE);
   if (path_bgm && settings->bools.audio_enable_menu_bgm)
      task_push_audio_mixer_load(path_bgm, audio_driver_load_menu_bgm_callback, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_BGM);

end:
   if (list)
      string_list_free(list);
   if (list_fallback)
      string_list_free(list_fallback);
   if (sounds_path)
      free(sounds_path);
   if (sounds_fallback_path)
      free(sounds_fallback_path);
}

void audio_driver_mixer_play_stream(unsigned i)
{
   audio_mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_menu_sound_looped(unsigned i)
{
   audio_mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_menu_sound(unsigned i)
{
   audio_mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_stream_looped(unsigned i)
{
   audio_mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_stream_sequential(unsigned i)
{
   audio_mixer_streams[i].stop_cb = audio_mixer_play_stop_sequential_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL);
}

float audio_driver_mixer_get_stream_volume(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return 0.0f;

   return audio_mixer_streams[i].volume;
}

void audio_driver_mixer_set_stream_volume(unsigned i, float vol)
{
   audio_mixer_voice_t *voice     = NULL;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   audio_mixer_streams[i].volume  = vol;

   voice                          = audio_mixer_streams[i].voice;

   if (voice)
      audio_mixer_voice_set_volume(voice, db_to_gain(vol));
}

void audio_driver_mixer_stop_stream(unsigned i)
{
   bool set_state              = false;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (audio_mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         set_state = true;
         break;
      case AUDIO_STREAM_STATE_STOPPED:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (set_state)
   {
      audio_mixer_voice_t *voice     = audio_mixer_streams[i].voice;

      if (voice)
         audio_mixer_stop(voice);
      audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
      audio_mixer_streams[i].volume  = 1.0f;
   }
}

void audio_driver_mixer_remove_stream(unsigned i)
{
   bool destroy                = false;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (audio_mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         audio_driver_mixer_stop_stream(i);
         destroy = true;
         break;
      case AUDIO_STREAM_STATE_STOPPED:
         destroy = true;
         break;
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (destroy)
   {
      audio_mixer_sound_t *handle = audio_mixer_streams[i].handle;
      if (handle)
         audio_mixer_destroy(handle);

      if (!string_is_empty(audio_mixer_streams[i].name))
         free(audio_mixer_streams[i].name);

      audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
      audio_mixer_streams[i].stop_cb = NULL;
      audio_mixer_streams[i].volume  = 0.0f;
      audio_mixer_streams[i].handle  = NULL;
      audio_mixer_streams[i].voice   = NULL;
      audio_mixer_streams[i].name    = NULL;
   }
}
#endif

bool audio_driver_enable_callback(void)
{
   if (!audio_callback.callback)
      return false;
   if (audio_callback.set_state)
      audio_callback.set_state(true);
   return true;
}

bool audio_driver_disable_callback(void)
{
   if (!audio_callback.callback)
      return false;

   if (audio_callback.set_state)
      audio_callback.set_state(false);
   return true;
}

/* Sets audio monitor rate to new value. */
static void audio_driver_monitor_set_rate(void)
{
   settings_t *settings       = configuration_settings;
   double new_src_ratio       = (double)settings->uints.audio_out_rate /
      audio_driver_input;

   audio_source_ratio_original = new_src_ratio;
   audio_source_ratio_current  = new_src_ratio;
}

bool audio_driver_callback(void)
{
   if (!audio_callback.callback)
      return false;

   if (audio_callback.callback)
      audio_callback.callback();

   return true;
}

bool audio_driver_has_callback(void)
{
   if (audio_callback.callback)
	   return true;
   return false;
}

bool audio_driver_toggle_mute(void)
{
   audio_driver_mute_enable  = !audio_driver_mute_enable;
   return true;
}

#ifdef HAVE_AUDIOMIXER
bool audio_driver_mixer_toggle_mute(void)
{
   audio_driver_mixer_mute_enable  = !audio_driver_mixer_mute_enable;
   return true;
}
#endif

static INLINE bool audio_driver_alive(void)
{
   if (     current_audio
         && current_audio->alive
         && audio_driver_context_audio_data)
      return current_audio->alive(audio_driver_context_audio_data);
   return false;
}

bool audio_driver_start(bool is_shutdown)
{
   if (!current_audio || !current_audio->start
         || !audio_driver_context_audio_data)
      goto error;
   if (!current_audio->start(audio_driver_context_audio_data, is_shutdown))
      goto error;

   return true;

error:
   RARCH_ERR("%s\n",
         msg_hash_to_str(MSG_FAILED_TO_START_AUDIO_DRIVER));
   audio_driver_active = false;
   return false;
}

bool audio_driver_stop(void)
{
   if (!current_audio || !current_audio->stop
         || !audio_driver_context_audio_data)
      return false;
   if (!audio_driver_alive())
      return false;
   return current_audio->stop(audio_driver_context_audio_data);
}

static void audio_driver_unset_callback(void)
{
   audio_callback.callback  = NULL;
   audio_callback.set_state = NULL;
}

void audio_driver_frame_is_reverse(void)
{
   /* We just rewound. Flush rewind audio buffer. */
   if (recording_data && recording_driver && recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = audio_driver_rewind_buf + audio_driver_rewind_ptr;
      ffemu_data.frames                  = (audio_driver_rewind_size - audio_driver_rewind_ptr) / 2;

      recording_driver->push_audio(recording_data, &ffemu_data);
   }

   if (!(
         runloop_paused           ||
         !audio_driver_active     ||
         !audio_driver_input_data ||
         !audio_driver_output_samples_buf))
      audio_driver_flush(
            audio_driver_rewind_buf + audio_driver_rewind_ptr,
            audio_driver_rewind_size - audio_driver_rewind_ptr,
            runloop_slowmotion);
}

static void audio_driver_destroy(void)
{
   audio_driver_active   = false;
   current_audio         = NULL;
}

void audio_set_bool(enum audio_action action, bool val)
{
   switch (action)
   {
      case AUDIO_ACTION_MIXER:
#ifdef HAVE_AUDIOMIXER
         audio_mixer_active = val;
#endif
         break;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }
}

void audio_set_float(enum audio_action action, float val)
{
   switch (action)
   {
      case AUDIO_ACTION_VOLUME_GAIN:
         audio_driver_volume_gain        = db_to_gain(val);
         break;
      case AUDIO_ACTION_MIXER_VOLUME_GAIN:
#ifdef HAVE_AUDIOMIXER
         audio_driver_mixer_volume_gain  = db_to_gain(val);
#endif
         break;
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         audio_driver_rate_control_delta = val;
         break;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }
}

float *audio_get_float_ptr(enum audio_action action)
{
   switch (action)
   {
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         return &audio_driver_rate_control_delta;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

bool *audio_get_bool_ptr(enum audio_action action)
{
   switch (action)
   {
      case AUDIO_ACTION_MIXER_MUTE_ENABLE:
#ifdef HAVE_AUDIOMIXER
         return &audio_driver_mixer_mute_enable;
#else
         break;
#endif
      case AUDIO_ACTION_MUTE_ENABLE:
         return &audio_driver_mute_enable;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

static const char* audio_driver_get_ident(void)
{
   if (!current_audio)
      return NULL;

   return current_audio->ident;
}

/* VIDEO */

bool video_driver_started_fullscreen(void)
{
   return video_started_fullscreen;
}

/* Stub functions */

static void update_window_title_null(void *data, void *data2)
{
}

static void swap_buffers_null(void *data, void *data2)
{
}

static bool get_metrics_null(void *data, enum display_metric_types type,
      float *value)
{
   return false;
}

static bool set_resize_null(void *a, unsigned b, unsigned c)
{
   return false;
}

/**
 * video_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to video driver at index. Can be NULL
 * if nothing found.
 **/
const void *video_driver_find_handle(int idx)
{
   const void *drv = video_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * video_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of video driver at index. Can be NULL
 * if nothing found.
 **/
const char *video_driver_find_ident(int idx)
{
   const video_driver_t *drv = video_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_video_driver_options:
 *
 * Get an enumerated list of all video driver names, separated by '|'.
 *
 * Returns: string listing of all video driver names, separated by '|'.
 **/
const char* config_get_video_driver_options(void)
{
   return char_list_new_special(STRING_LIST_VIDEO_DRIVERS, NULL);
}

bool video_driver_is_threaded(void)
{
   return video_driver_is_threaded_internal();
}

#ifdef HAVE_VULKAN
static bool hw_render_context_is_vulkan(enum retro_hw_context_type type)
{
   return type == RETRO_HW_CONTEXT_VULKAN;
}
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
static bool hw_render_context_is_gl(enum retro_hw_context_type type)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         return true;
      default:
         break;
   }

   return false;
}
#endif

bool *video_driver_get_threaded(void)
{
   return &video_driver_threaded;
}

void video_driver_set_threaded(bool val)
{
   video_driver_threaded = val;
}

const char *video_driver_get_ident(void)
{
   return (current_video) ? current_video->ident : NULL;
}

static void video_context_driver_reset(void)
{
   if (!current_video_context.get_metrics)
      current_video_context.get_metrics         = get_metrics_null;

   if (!current_video_context.update_window_title)
      current_video_context.update_window_title = update_window_title_null;

   if (!current_video_context.set_resize)
      current_video_context.set_resize          = set_resize_null;

   if (!current_video_context.swap_buffers)
      current_video_context.swap_buffers        = swap_buffers_null;
}

bool video_context_driver_set(const gfx_ctx_driver_t *data)
{
   if (!data)
      return false;
   current_video_context = *data;
   video_context_driver_reset();
   return true;
}

void video_context_driver_destroy(void)
{
   current_video_context.init                       = NULL;
   current_video_context.bind_api                   = NULL;
   current_video_context.swap_interval              = NULL;
   current_video_context.set_video_mode             = NULL;
   current_video_context.get_video_size             = NULL;
   current_video_context.get_video_output_size      = NULL;
   current_video_context.get_video_output_prev      = NULL;
   current_video_context.get_video_output_next      = NULL;
   current_video_context.get_metrics                = get_metrics_null;
   current_video_context.translate_aspect           = NULL;
   current_video_context.update_window_title        = update_window_title_null;
   current_video_context.check_window               = NULL;
   current_video_context.set_resize                 = set_resize_null;
   current_video_context.suppress_screensaver       = NULL;
   current_video_context.has_windowed               = NULL;
   current_video_context.swap_buffers               = swap_buffers_null;
   current_video_context.input_driver               = NULL;
   current_video_context.get_proc_address           = NULL;
   current_video_context.image_buffer_init          = NULL;
   current_video_context.image_buffer_write         = NULL;
   current_video_context.show_mouse                 = NULL;
   current_video_context.ident                      = NULL;
   current_video_context.get_flags                  = NULL;
   current_video_context.set_flags                  = NULL;
   current_video_context.bind_hw_render             = NULL;
   current_video_context.get_context_data           = NULL;
   current_video_context.make_current               = NULL;
}

/**
 * video_driver_get_current_framebuffer:
 *
 * Gets pointer to current hardware renderer framebuffer object.
 * Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
 *
 * Returns: pointer to hardware framebuffer object, otherwise 0.
 **/
static uintptr_t video_driver_get_current_framebuffer(void)
{
   if (video_driver_poke && video_driver_poke->get_current_framebuffer)
      return video_driver_poke->get_current_framebuffer(video_driver_data);
   return 0;
}

static retro_proc_address_t video_driver_get_proc_address(const char *sym)
{
   if (video_driver_poke && video_driver_poke->get_proc_address)
      return video_driver_poke->get_proc_address(video_driver_data, sym);
   return NULL;
}

bool video_driver_set_shader(enum rarch_shader_type type,
      const char *path)
{
   if (current_video->set_shader)
      return current_video->set_shader(video_driver_data, type, path);
   return false;
}

static void video_driver_filter_free(void)
{
   if (video_driver_state_filter)
      rarch_softfilter_free(video_driver_state_filter);
   video_driver_state_filter    = NULL;

   if (video_driver_state_buffer)
   {
#ifdef _3DS
      linearFree(video_driver_state_buffer);
#else
      free(video_driver_state_buffer);
#endif
   }
   video_driver_state_buffer    = NULL;

   video_driver_state_scale     = 0;
   video_driver_state_out_bpp   = 0;
   video_driver_state_out_rgb32 = false;
}

static void video_driver_init_filter(enum retro_pixel_format colfmt_int)
{
   unsigned pow2_x, pow2_y, maxsize;
   void *buf                            = NULL;
   settings_t *settings                 = configuration_settings;
   struct retro_game_geometry *geom     = &video_driver_av_info.geometry;
   unsigned width                       = geom->max_width;
   unsigned height                      = geom->max_height;
   /* Deprecated format. Gets pre-converted. */
   enum retro_pixel_format colfmt       =
      (colfmt_int == RETRO_PIXEL_FORMAT_0RGB1555) ?
      RETRO_PIXEL_FORMAT_RGB565 : colfmt_int;

   if (video_driver_is_hw_context())
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   video_driver_state_filter            = rarch_softfilter_new(
         settings->paths.path_softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!video_driver_state_filter)
   {
      RARCH_ERR("[Video]: Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(video_driver_state_filter,
         &width, &height);

   pow2_x                              = next_pow2(width);
   pow2_y                              = next_pow2(height);
   maxsize                             = MAX(pow2_x, pow2_y);
   video_driver_state_scale            = maxsize / RARCH_SCALE_BASE;
   video_driver_state_out_rgb32        = rarch_softfilter_get_output_format(
                                         video_driver_state_filter) ==
                                         RETRO_PIXEL_FORMAT_XRGB8888;

   video_driver_state_out_bpp          = video_driver_state_out_rgb32 ?
                                         sizeof(uint32_t)             :
                                         sizeof(uint16_t);

   /* TODO: Aligned output. */
#ifdef _3DS
   buf = linearMemAlign(
         width * height * video_driver_state_out_bpp, 0x80);
#else
   buf = malloc(
         width * height * video_driver_state_out_bpp);
#endif
   if (!buf)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed.\n");
      video_driver_filter_free();
      return;
   }

   video_driver_state_buffer    = buf;
}

static void video_driver_init_input(const input_driver_t *tmp)
{
   const input_driver_t **input = &current_input;
   if (*input)
      return;

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("[Video]: Graphics driver did not initialize an input driver."
         " Attempting to pick a suitable driver.\n");

   if (tmp)
      *input = tmp;
   else
      input_driver_find_driver();

   /* This should never really happen as tmp (driver.input) is always
    * found before this in find_driver_input(), or we have aborted
    * in a similar fashion anyways. */
   if (!current_input)
      goto error;

   if (input_driver_init())
      return;

error:
   RARCH_ERR("[Video]: Cannot initialize input driver. Exiting ...\n");
   retroarch_fail(1, "video_driver_init_input()");
}

/**
 * video_driver_monitor_compute_fps_statistics:
 *
 * Computes monitor FPS statistics.
 **/
static void video_driver_monitor_compute_fps_statistics(void)
{
   double avg_fps       = 0.0;
   double stddev        = 0.0;
   unsigned samples     = 0;

   if (video_driver_frame_time_count <
         (2 * MEASURE_FRAME_TIME_SAMPLES_COUNT))
   {
      RARCH_LOG(
            "[Video]: Does not have enough samples for monitor refresh rate"
            " estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   if (video_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("[Video]: Average monitor Hz: %.6f Hz. (%.3f %% frame time"
            " deviation, based on %u last samples).\n",
            avg_fps, 100.0 * stddev, samples);
   }
}

static void video_driver_pixel_converter_free(void)
{
   if (!video_driver_scaler_ptr)
      return;

   scaler_ctx_gen_reset(video_driver_scaler_ptr->scaler);

   if (video_driver_scaler_ptr->scaler)
      free(video_driver_scaler_ptr->scaler);
   video_driver_scaler_ptr->scaler     = NULL;

   if (video_driver_scaler_ptr->scaler_out)
      free(video_driver_scaler_ptr->scaler_out);
   video_driver_scaler_ptr->scaler_out = NULL;

   if (video_driver_scaler_ptr)
      free(video_driver_scaler_ptr);
   video_driver_scaler_ptr             = NULL;
}

static void video_driver_free_hw_context(void)
{
   video_driver_context_lock();

   if (hw_render.context_destroy)
      hw_render.context_destroy();

   memset(&hw_render, 0, sizeof(hw_render));

   video_driver_context_unlock();

   hw_render_context_negotiation = NULL;
}

static void video_driver_free_internal(void)
{
#ifdef HAVE_THREADS
   bool is_threaded     = video_driver_is_threaded_internal();
#endif

#ifdef HAVE_VIDEO_LAYOUT
   video_layout_deinit();
#endif

   command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);

   if (!video_driver_is_video_cache_context())
      video_driver_free_hw_context();

   if (!(current_input_data == video_driver_data))
   {
      if (current_input && current_input->free)
         current_input->free(current_input_data);
      current_input_data = NULL;
   }

   if (video_driver_data
         && current_video && current_video->free
      )
         current_video->free(video_driver_data);

   video_driver_pixel_converter_free();
   video_driver_filter_free();

   command_event(CMD_EVENT_SHADER_DIR_DEINIT, NULL);

#ifdef HAVE_THREADS
   if (is_threaded)
      return;
#endif

   video_driver_monitor_compute_fps_statistics();
}

static bool video_driver_pixel_converter_init(unsigned size)
{
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context_internal();
   void *scalr_out                      = NULL;
   video_pixel_scaler_t          *scalr = NULL;
   struct scaler_ctx        *scalr_ctx  = NULL;

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_pix_fmt != RETRO_PIXEL_FORMAT_0RGB1555)
      return true;

   /* No need to perform pixel conversion for HW rendering contexts. */
   if (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      return true;

   RARCH_WARN("0RGB1555 pixel format is deprecated,"
         " and will be slower. For 15/16-bit, RGB565"
         " format is preferred.\n");

   scalr = (video_pixel_scaler_t*)calloc(1, sizeof(*scalr));

   if (!scalr)
      goto error;

   video_driver_scaler_ptr         = scalr;

   scalr_ctx = (struct scaler_ctx*)calloc(1, sizeof(*scalr_ctx));

   if (!scalr_ctx)
      goto error;

   video_driver_scaler_ptr->scaler              = scalr_ctx;
   video_driver_scaler_ptr->scaler->scaler_type = SCALER_TYPE_POINT;
   video_driver_scaler_ptr->scaler->in_fmt      = SCALER_FMT_0RGB1555;

   /* TODO: Pick either ARGB8888 or RGB565 depending on driver. */
   video_driver_scaler_ptr->scaler->out_fmt     = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(scalr_ctx))
      goto error;

   scalr_out = calloc(sizeof(uint16_t), size * size);

   if (!scalr_out)
      goto error;

   video_driver_scaler_ptr->scaler_out          = scalr_out;

   return true;

error:
   video_driver_pixel_converter_free();
   video_driver_filter_free();

   return false;
}

static bool video_driver_init_internal(bool *video_is_threaded)
{
   video_info_t video;
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp            = NULL;
   const input_driver_t *tmp              = NULL;
   rarch_system_info_t *system            = NULL;
   static uint16_t dummy_pixels[32]       = {0};
   settings_t *settings                   = configuration_settings;
   struct retro_game_geometry *geom       = &video_driver_av_info.geometry;

   if (!string_is_empty(settings->paths.path_softfilter_plugin))
      video_driver_init_filter(video_driver_pix_fmt);

   max_dim   = MAX(geom->max_width, geom->max_height);
   scale     = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale     = MAX(scale, 1);

   if (video_driver_state_filter)
      scale = video_driver_state_scale;

   /* Update core-dependent aspect ratio values. */
   video_driver_set_viewport_square_pixel();
   video_driver_set_viewport_core();
   video_driver_set_viewport_config();

   /* Update CUSTOM viewport. */
   custom_vp = video_viewport_get_custom();

   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   video_driver_set_aspect_ratio_value(
      aspectratio_lut[settings->uints.video_aspect_ratio_idx].value);

   if (settings->bools.video_fullscreen|| retroarch_is_forced_fullscreen())
   {
      width  = settings->uints.video_fullscreen_x;
      height = settings->uints.video_fullscreen_y;
   }
   else
   {
      /* TODO: remove when the new window resizing core is hooked */
      if (settings->bools.video_window_save_positions &&
         (settings->uints.window_position_width || 
          settings->uints.window_position_height))
      {
         width  = settings->uints.window_position_width;
         height = settings->uints.window_position_height;
      }
      else
      {
         if (settings->bools.video_force_aspect)
         {
            /* Do rounding here to simplify integer scale correctness. */
            unsigned base_width =
               roundf(geom->base_height * video_driver_get_aspect_ratio());
            width  = roundf(base_width * settings->floats.video_scale);
         }
         else
            width  = roundf(geom->base_width   * settings->floats.video_scale);
         height    = roundf(geom->base_height  * settings->floats.video_scale);
      }
   }

   if (width && height)
      RARCH_LOG("[Video]: Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("[Video]: Video @ fullscreen\n");

   video_driver_display_type_set(RARCH_DISPLAY_NONE);
   video_driver_display_set(0);
   video_driver_window_set(0);

   if (!video_driver_pixel_converter_init(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("[Video]: Failed to initialize pixel converter.\n");
      goto error;
   }

   video.width         = width;
   video.height        = height;
   video.fullscreen    = settings->bools.video_fullscreen || retroarch_is_forced_fullscreen();
   video.vsync         = settings->bools.video_vsync && !runloop_force_nonblock;
   video.force_aspect  = settings->bools.video_force_aspect;
   video.font_enable   = settings->bools.video_font_enable;
   video.swap_interval = settings->uints.video_swap_interval;
#ifdef GEKKO
   video.viwidth       = settings->uints.video_viwidth;
   video.vfilter       = settings->bools.video_vfilter;
#endif
   video.smooth        = settings->bools.video_smooth;
   video.input_scale   = scale;
   video.rgb32         = video_driver_state_filter ?
      video_driver_state_out_rgb32 :
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
   video.parent        = 0;

   video_started_fullscreen = video.fullscreen;

   /* Reset video frame count */
   video_driver_frame_count = 0;

   tmp                      = current_input;
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_find_driver();

#ifdef HAVE_THREADS
   video.is_threaded   = video_driver_is_threaded_internal();
   *video_is_threaded  = video.is_threaded;

   if (video.is_threaded)
   {
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("[Video]: Starting threaded video driver ...\n");

      if (!video_init_thread((const video_driver_t**)&current_video,
               &video_driver_data,
               &current_input, (void**)&current_input_data,
               current_video, video))
      {
         RARCH_ERR("[Video]: Cannot open threaded video driver ... Exiting ...\n");
         goto error;
      }
   }
   else
#endif
      video_driver_data = current_video->init(
            &video, &current_input,
            (void**)&current_input_data);

   if (!video_driver_data)
   {
      RARCH_ERR("[Video]: Cannot open video driver ... Exiting ...\n");
      goto error;
   }

   video_driver_poke = NULL;
   if (current_video->poke_interface)
      current_video->poke_interface(video_driver_data, &video_driver_poke);

   if (current_video->viewport_info &&
         (!custom_vp->width  ||
          !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;

      video_driver_get_viewport_info(custom_vp);
   }

   system              = &runloop_system;

   video_driver_set_rotation(
            (settings->uints.video_rotation + system->rotation) % 4);

   current_video->suppress_screensaver(video_driver_data,
         settings->bools.ui_suspend_screensaver_enable);

   video_driver_init_input(tmp);

#ifdef HAVE_OVERLAY
   retroarch_overlay_deinit();
   retroarch_overlay_init();
#endif

#ifdef HAVE_VIDEO_LAYOUT
   if (settings->bools.video_layout_enable)
   {
      video_layout_init(video_driver_data, video_driver_layout_render_interface());
      video_layout_load(settings->paths.path_video_layout);
      video_layout_view_select(settings->uints.video_layout_selected_view);
   }
#endif

   if (!current_core.game_loaded)
      video_driver_cached_frame_set(&dummy_pixels, 4, 4, 8);

#if defined(PSP)
   video_driver_set_texture_frame(&dummy_pixels, false, 1, 1, 1.0f);
#endif

   video_context_driver_reset();

   video_display_server_init();

   if ((enum rotation)settings->uints.screen_orientation != ORIENTATION_NORMAL)
      video_display_server_set_screen_orientation((enum rotation)settings->uints.screen_orientation);

   command_event(CMD_EVENT_SHADER_DIR_INIT, NULL);

   return true;

error:
   retroarch_fail(1, "init_video()");
   return false;
}

bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate)
{
   if (!current_video || !current_video->set_viewport)
      return false;
   current_video->set_viewport(video_driver_data, width, height,
         force_fullscreen, allow_rotate);
   return true;
}

bool video_driver_set_rotation(unsigned rotation)
{
   if (!current_video || !current_video->set_rotation)
      return false;
   current_video->set_rotation(video_driver_data, rotation);
   return true;
}

bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen)
{
   gfx_ctx_mode_t mode;

   if (video_driver_poke && video_driver_poke->set_video_mode)
   {
      video_driver_poke->set_video_mode(video_driver_data,
            width, height, fullscreen);
      return true;
   }

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   return video_context_driver_set_video_mode(&mode);
}

bool video_driver_get_video_output_size(unsigned *width, unsigned *height)
{
   if (!video_driver_poke || !video_driver_poke->get_video_output_size)
      return false;
   video_driver_poke->get_video_output_size(video_driver_data,
         width, height);
   return true;
}

void video_driver_set_osd_msg(const char *msg, const void *data, void *font)
{
   video_frame_info_t video_info;
   video_driver_build_info(&video_info);
   if (video_driver_poke && video_driver_poke->set_osd_msg)
      video_driver_poke->set_osd_msg(video_driver_data, &video_info, msg, data, font);
}

void video_driver_set_texture_enable(bool enable, bool fullscreen)
{
   if (video_driver_poke && video_driver_poke->set_texture_enable)
      video_driver_poke->set_texture_enable(video_driver_data,
            enable, fullscreen);
}

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   if (video_driver_poke && video_driver_poke->set_texture_frame)
      video_driver_poke->set_texture_frame(video_driver_data,
            frame, rgb32, width, height, alpha);
}

#ifdef HAVE_OVERLAY
static bool video_driver_overlay_interface(
      const video_overlay_interface_t **iface)
{
   if (!current_video || !current_video->overlay_interface)
      return false;
   current_video->overlay_interface(video_driver_data, iface);
   return true;
}
#endif

#ifdef HAVE_VIDEO_LAYOUT
const video_layout_render_interface_t *video_driver_layout_render_interface(void)
{
   if (!current_video || !current_video->video_layout_render_interface)
      return NULL;

   return current_video->video_layout_render_interface(video_driver_data);
}
#endif

void *video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch)
{
   if (!current_video || !current_video->read_frame_raw)
      return NULL;
   return current_video->read_frame_raw(video_driver_data, width,
         height, pitch);
}

void video_driver_set_filtering(unsigned index, bool smooth)
{
   if (video_driver_poke && video_driver_poke->set_filtering)
      video_driver_poke->set_filtering(video_driver_data, index, smooth);
}

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   if (data)
      frame_cache_data = data;
   frame_cache_width   = width;
   frame_cache_height  = height;
   frame_cache_pitch   = pitch;
}

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch)
{
   if (data)
      *data    = frame_cache_data;
   if (width)
      *width   = frame_cache_width;
   if (height)
      *height  = frame_cache_height;
   if (pitch)
      *pitch   = frame_cache_pitch;
}

void video_driver_get_size(unsigned *width, unsigned *height)
{
#ifdef HAVE_THREADS
   bool is_threaded = video_driver_is_threaded_internal();
   video_driver_threaded_lock(is_threaded);
#endif
   if (width)
      *width  = video_driver_width;
   if (height)
      *height = video_driver_height;
#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
}

void video_driver_set_size(unsigned *width, unsigned *height)
{
#ifdef HAVE_THREADS
   bool is_threaded = video_driver_is_threaded_internal();
   video_driver_threaded_lock(is_threaded);
#endif
   if (width)
      video_driver_width  = *width;
   if (height)
      video_driver_height = *height;
#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
}

/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz)
{
   char msg[128];
   settings_t *settings = configuration_settings;

   snprintf(msg, sizeof(msg),
         "Setting refresh rate to: %.3f Hz.", hz);
   runloop_msg_queue_push(msg, 1, 180, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg);

   configuration_set_float(settings,
         settings->floats.video_refresh_rate,
         hz);
}

/**
 * video_monitor_fps_statistics
 * @refresh_rate       : Monitor refresh rate.
 * @deviation          : Deviation from measured refresh rate.
 * @sample_points      : Amount of sampled points.
 *
 * Gets the monitor FPS statistics based on the current
 * runtime.
 *
 * Returns: true (1) on success.
 * false (0) if:
 * a) threaded video mode is enabled
 * b) less than 2 frame time samples.
 * c) FPS monitor enable is off.
 **/
bool video_monitor_fps_statistics(double *refresh_rate,
      double *deviation, unsigned *sample_points)
{
   unsigned i;
   retro_time_t accum     = 0;
   retro_time_t avg       = 0;
   retro_time_t accum_var = 0;
   unsigned samples       = 0;

#ifdef HAVE_THREADS
   if (video_driver_is_threaded_internal())
      return false;
#endif

   samples = MIN(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         (unsigned)video_driver_frame_time_count);

   if (samples < 2)
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
   {
      accum += video_driver_frame_time_samples[i];
#if 0
      RARCH_LOG("[Video]: Interval #%u: %d usec / frame.\n",
            i, (int)frame_time_samples[i]);
#endif
   }

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = video_driver_frame_time_samples[i] - avg;
      accum_var         += diff * diff;
   }

   *deviation        = sqrt((double)accum_var / (samples - 1)) / avg;

   if (refresh_rate)
      *refresh_rate  = 1000000.0 / avg;

   if (sample_points)
      *sample_points = samples;

   return true;
}

float video_driver_get_aspect_ratio(void)
{
   return video_driver_aspect_ratio;
}

void video_driver_set_aspect_ratio_value(float value)
{
   video_driver_aspect_ratio = value;
}

enum retro_pixel_format video_driver_get_pixel_format(void)
{
   return video_driver_pix_fmt;
}

void video_driver_set_pixel_format(enum retro_pixel_format fmt)
{
   video_driver_pix_fmt = fmt;
}

/**
 * video_driver_cached_frame:
 *
 * Renders the current video frame.
 **/
bool video_driver_cached_frame(void)
{
   void *recording  = recording_data;

   /* Cannot allow recording when pushing duped frames. */
   recording_data   = NULL;

   if (current_core.inited)
      retro_ctx.frame_cb(
            (frame_cache_data != RETRO_HW_FRAME_BUFFER_VALID)
            ? frame_cache_data : NULL,
            frame_cache_width,
            frame_cache_height, frame_cache_pitch);

   recording_data   = recording;

   return true;
}

static void video_driver_monitor_adjust_system_rates(void)
{
   float timing_skew                      = 0.0f;
   settings_t *settings                   = configuration_settings;
   float video_refresh_rate               = settings->floats.video_refresh_rate;
   float timing_skew_hz                   = video_refresh_rate;
   const struct retro_system_timing *info = (const struct retro_system_timing*)&video_driver_av_info.timing;

   runloop_force_nonblock                 = false;

   if (!info || info->fps <= 0.0)
      return;

   video_driver_core_hz                   = info->fps;

   if (video_driver_crt_switching_active)
      timing_skew_hz                       = video_driver_core_hz;
   timing_skew                             = fabs(
         1.0f - info->fps / timing_skew_hz);

   if (!settings->bools.vrr_runloop_enable)
   {
      /* We don't want to adjust pitch too much. If we have extreme cases,
       * just don't readjust at all. */
      if (timing_skew <= settings->floats.audio_max_timing_skew)
         return;

      RARCH_LOG("[Video]: Timings deviate too much. Will not adjust."
            " (Display = %.2f Hz, Game = %.2f Hz)\n",
            video_refresh_rate,
            (float)info->fps);
   }

   if (info->fps <= timing_skew_hz)
      return;

   /* We won't be able to do VSync reliably when game FPS > monitor FPS. */
   runloop_force_nonblock = true;
   RARCH_LOG("[Video]: Game FPS > Monitor FPS. Cannot rely on VSync.\n");
}

static void video_driver_lock_new(void)
{
   video_driver_lock_free();
#ifdef HAVE_THREADS
   if (!display_lock)
      display_lock = slock_new();
   retro_assert(display_lock);

   if (!context_lock)
      context_lock = slock_new();
   retro_assert(context_lock);
#endif
}

static void video_driver_destroy(void)
{
   video_display_server_destroy();
   crt_video_restore();

   video_driver_use_rgba          = false;
   video_driver_active            = false;
   video_driver_cache_context     = false;
   video_driver_cache_context_ack = false;
   video_driver_record_gpu_buffer = NULL;
   current_video                  = NULL;
   video_driver_set_cached_frame_ptr(NULL);
}

void video_driver_set_cached_frame_ptr(const void *data)
{
   if (data)
      frame_cache_data = data;
}

void video_driver_set_stub_frame(void)
{
   frame_bak            = current_video->frame;
   current_video->frame = video_null.frame;
}

void video_driver_unset_stub_frame(void)
{
   if (frame_bak != NULL)
      current_video->frame = frame_bak;

   frame_bak = NULL;
}

bool video_driver_supports_viewport_read(void)
{
   return current_video->read_viewport && current_video->viewport_info;
}

bool video_driver_prefer_viewport_read(void)
{
   settings_t *settings = configuration_settings;
   return settings->bools.video_gpu_screenshot ||
      (video_driver_is_hw_context() && !current_video->read_frame_raw);
}

bool video_driver_supports_read_frame_raw(void)
{
   if (current_video->read_frame_raw)
      return true;
   return false;
}

void video_driver_set_viewport_config(void)
{
   settings_t *settings = configuration_settings;

   if (settings->floats.video_aspect_ratio < 0.0f)
   {
      struct retro_game_geometry *geom = &video_driver_av_info.geometry;

      if (geom->aspect_ratio > 0.0f &&
            settings->bools.video_aspect_ratio_auto)
         aspectratio_lut[ASPECT_RATIO_CONFIG].value = geom->aspect_ratio;
      else
      {
         unsigned base_width  = geom->base_width;
         unsigned base_height = geom->base_height;

         /* Get around division by zero errors */
         if (base_width == 0)
            base_width = 1;
         if (base_height == 0)
            base_height = 1;
         aspectratio_lut[ASPECT_RATIO_CONFIG].value =
            (float)base_width / base_height; /* 1:1 PAR. */
      }
   }
   else
   {
      aspectratio_lut[ASPECT_RATIO_CONFIG].value =
         settings->floats.video_aspect_ratio;
   }
}

void video_driver_set_viewport_square_pixel(void)
{
   unsigned len, highest, i, aspect_x, aspect_y;
   struct retro_game_geometry *geom  = &video_driver_av_info.geometry;
   unsigned width                    = geom->base_width;
   unsigned height                   = geom->base_height;

   if (width == 0 || height == 0)
      return;

   len      = MIN(width, height);
   highest  = 1;

   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   aspect_x = width / highest;
   aspect_y = height / highest;

   snprintf(aspectratio_lut[ASPECT_RATIO_SQUARE].name,
         sizeof(aspectratio_lut[ASPECT_RATIO_SQUARE].name),
         "1:1 PAR (%u:%u DAR)", aspect_x, aspect_y);

   aspectratio_lut[ASPECT_RATIO_SQUARE].value = (float)aspect_x / aspect_y;
}

void video_driver_set_viewport_core(void)
{
   struct retro_game_geometry *geom     = &video_driver_av_info.geometry;

   if (!geom || geom->base_width <= 0.0f || geom->base_height <= 0.0f)
      return;

   /* Fallback to 1:1 pixel ratio if none provided */
   if (geom->aspect_ratio > 0.0f)
      aspectratio_lut[ASPECT_RATIO_CORE].value = geom->aspect_ratio;
   else
      aspectratio_lut[ASPECT_RATIO_CORE].value =
         (float)geom->base_width / geom->base_height;
}

void video_driver_reset_custom_viewport(void)
{
   struct video_viewport *custom_vp = video_viewport_get_custom();

   custom_vp->width  = 0;
   custom_vp->height = 0;
   custom_vp->x      = 0;
   custom_vp->y      = 0;
}

void video_driver_set_rgba(void)
{
   video_driver_lock();
   video_driver_use_rgba = true;
   video_driver_unlock();
}

void video_driver_unset_rgba(void)
{
   video_driver_lock();
   video_driver_use_rgba = false;
   video_driver_unlock();
}

bool video_driver_supports_rgba(void)
{
   bool tmp;
   video_driver_lock();
   tmp = video_driver_use_rgba;
   video_driver_unlock();
   return tmp;
}

bool video_driver_get_next_video_out(void)
{
   if (!video_driver_poke)
      return false;

   if (!video_driver_poke->get_video_output_next)
      return video_context_driver_get_video_output_next();
   video_driver_poke->get_video_output_next(video_driver_data);
   return true;
}

bool video_driver_get_prev_video_out(void)
{
   if (!video_driver_poke)
      return false;

   if (!video_driver_poke->get_video_output_prev)
      return video_context_driver_get_video_output_prev();
   video_driver_poke->get_video_output_prev(video_driver_data);
   return true;
}

void video_driver_monitor_reset(void)
{
   video_driver_frame_time_count = 0;
}

void video_driver_set_aspect_ratio(void)
{
   settings_t *settings       = configuration_settings;
   unsigned aspect_ratio_idx  = settings->uints.video_aspect_ratio_idx;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(
            aspectratio_lut[aspect_ratio_idx].value);

   if (!video_driver_poke || !video_driver_poke->set_aspect_ratio)
      return;
   video_driver_poke->set_aspect_ratio(
         video_driver_data, aspect_ratio_idx);
}

void video_driver_update_viewport(struct video_viewport* vp, bool force_full, bool keep_aspect)
{
   gfx_ctx_aspect_t aspect_data;
   float            device_aspect = (float)vp->full_width / vp->full_height;
   settings_t *settings           = configuration_settings;

   aspect_data.aspect = &device_aspect;
   aspect_data.width  = vp->full_width;
   aspect_data.height = vp->full_height;

   video_context_driver_translate_aspect(&aspect_data);

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = vp->full_width;
   vp->height = vp->full_height;

   if (settings->bools.video_scale_integer && !force_full)
      video_viewport_get_scaled_integer(
            vp, vp->full_width, vp->full_height, video_driver_get_aspect_ratio(), keep_aspect);
   else if (keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport* custom = video_viewport_get_custom();

         vp->x      = custom->x;
         vp->y      = custom->y;
         vp->width  = custom->width;
         vp->height = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta      = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            vp->x      = (int)roundf(vp->full_width * (0.5f - delta));
            vp->width  = (unsigned)roundf(2.0f * vp->full_width * delta);
            vp->y      = 0;
            vp->height = vp->full_height;
         }
         else
         {
            vp->x      = 0;
            vp->width  = vp->full_width;
            delta      = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            vp->y      = (int)roundf(vp->full_height * (0.5f - delta));
            vp->height = (unsigned)roundf(2.0f * vp->full_height * delta);
         }
      }
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      vp->y = 0;
#endif
}

void video_driver_show_mouse(void)
{
   if (video_driver_poke && video_driver_poke->show_mouse)
      video_driver_poke->show_mouse(video_driver_data, true);
}

void video_driver_hide_mouse(void)
{
   if (video_driver_poke && video_driver_poke->show_mouse)
      video_driver_poke->show_mouse(video_driver_data, false);
}

void video_driver_set_nonblock_state(bool toggle)
{
   if (current_video->set_nonblock_state)
      current_video->set_nonblock_state(video_driver_data, toggle);
}

static bool video_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = configuration_settings;

   if (video_driver_is_hw_context())
   {
      struct retro_hw_render_callback *hwr = video_driver_get_hw_context_internal();

      current_video                        = NULL;

      (void)hwr;

#if defined(HAVE_VULKAN)
      if (hwr && hw_render_context_is_vulkan(hwr->context_type))
      {
         RARCH_LOG("[Video]: Using HW render, Vulkan driver forced.\n");
         current_video = &video_vulkan;
      }
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
      if (hwr && hw_render_context_is_gl(hwr->context_type))
      {
         RARCH_LOG("[Video]: Using HW render, OpenGL driver forced.\n");

         /* If we have configured one of the HW render capable GL drivers, go with that. */
         if (!string_is_equal(settings->arrays.video_driver, "gl") &&
               !string_is_equal(settings->arrays.video_driver, "glcore"))
         {
#if defined(HAVE_OPENGL_CORE)
            current_video = &video_gl_core;
            RARCH_LOG("[Video]: Forcing \"glcore\" driver.\n");
#else
            current_video = &video_gl2;
            RARCH_LOG("[Video]: Forcing \"gl\" driver.\n");
#endif
         }
         else
         {
            RARCH_LOG("[Video]: Using configured \"%s\" driver for GL HW render.\n",
                  settings->arrays.video_driver);
         }
      }
#endif

      if (current_video)
         return true;
   }

   if (frontend_driver_has_get_video_driver_func())
   {
      current_video = (video_driver_t*)frontend_driver_get_video_driver();

      if (current_video)
         return true;
      RARCH_WARN("Frontend supports get_video_driver() but did not specify one.\n");
   }

   drv.label = "video_driver";
   drv.s     = settings->arrays.video_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = (int)drv.len;

   if (i >= 0)
      current_video = (video_driver_t*)video_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any video driver named \"%s\"\n",
               settings->arrays.video_driver);
         RARCH_LOG_OUTPUT("Available video drivers are:\n");
         for (d = 0; video_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", video_driver_find_ident(d));
         RARCH_WARN("Going to default to first video driver...\n");
      }

      current_video = (video_driver_t*)video_driver_find_handle(0);

      if (!current_video)
         retroarch_fail(1, "find_video_driver()");
   }
   return true;
}

void video_driver_apply_state_changes(void)
{
   if (video_driver_poke &&
         video_driver_poke->apply_state_changes)
      video_driver_poke->apply_state_changes(video_driver_data);
}

bool video_driver_read_viewport(uint8_t *buffer, bool is_idle)
{
   if (     current_video->read_viewport
         && current_video->read_viewport(
            video_driver_data, buffer, is_idle))
      return true;
   return false;
}

void video_driver_default_settings(void)
{
   global_t *global    = &g_extern;

   if (!global)
      return;

   global->console.screen.gamma_correction       = DEFAULT_GAMMA;
   global->console.flickerfilter_enable          = false;
   global->console.softfilter_enable             = false;

   global->console.screen.resolutions.current.id = 0;
}

void video_driver_load_settings(config_file_t *conf)
{
   bool tmp_bool    = false;
   global_t *global = &g_extern;

   if (!conf)
      return;

#ifdef _XBOX
   CONFIG_GET_BOOL_BASE(conf, global,
         console.screen.gamma_correction, "gamma_correction");
#else
   CONFIG_GET_INT_BASE(conf, global,
         console.screen.gamma_correction, "gamma_correction");
#endif

   if (config_get_bool(conf, "flicker_filter_enable",
         &tmp_bool))
      global->console.flickerfilter_enable = tmp_bool;

   if (config_get_bool(conf, "soft_filter_enable",
         &tmp_bool))
      global->console.softfilter_enable = tmp_bool;

   CONFIG_GET_INT_BASE(conf, global,
         console.screen.soft_filter_index,
         "soft_filter_index");
   CONFIG_GET_INT_BASE(conf, global,
         console.screen.resolutions.current.id,
         "current_resolution_id");
   CONFIG_GET_INT_BASE(conf, global,
         console.screen.flicker_filter_index,
         "flicker_filter_index");
}

void video_driver_save_settings(config_file_t *conf)
{
   global_t *global = &g_extern;
   if (!conf)
      return;

#ifdef _XBOX
   config_set_bool(conf, "gamma_correction",
         global->console.screen.gamma_correction);
#else
   config_set_int(conf, "gamma_correction",
         global->console.screen.gamma_correction);
#endif
   config_set_bool(conf, "flicker_filter_enable",
         global->console.flickerfilter_enable);
   config_set_bool(conf, "soft_filter_enable",
         global->console.softfilter_enable);

   config_set_int(conf, "soft_filter_index",
         global->console.screen.soft_filter_index);
   config_set_int(conf, "current_resolution_id",
         global->console.screen.resolutions.current.id);
   config_set_int(conf, "flicker_filter_index",
         global->console.screen.flicker_filter_index);
}

void video_driver_reinit(void)
{
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context_internal();

   if (hwr->cache_context)
      video_driver_cache_context    = true;
   else
      video_driver_cache_context = false;

   video_driver_cache_context_ack = false;
   command_event(CMD_EVENT_RESET_CONTEXT, NULL);
   video_driver_cache_context = false;
}

bool video_driver_is_hw_context(void)
{
   bool is_hw_context = false;

   video_driver_context_lock();
   is_hw_context = (hw_render.context_type != RETRO_HW_CONTEXT_NONE);
   video_driver_context_unlock();

   return is_hw_context;
}

const struct retro_hw_render_context_negotiation_interface *
   video_driver_get_context_negotiation_interface(void)
{
   return hw_render_context_negotiation;
}

void video_driver_set_context_negotiation_interface(
      const struct retro_hw_render_context_negotiation_interface *iface)
{
   hw_render_context_negotiation = iface;
}

bool video_driver_is_video_cache_context(void)
{
   return video_driver_cache_context;
}

void video_driver_set_video_cache_context_ack(void)
{
   video_driver_cache_context_ack = true;
}

void video_driver_unset_video_cache_context_ack(void)
{
   video_driver_cache_context_ack = false;
}

bool video_driver_get_viewport_info(struct video_viewport *viewport)
{
   if (!current_video || !current_video->viewport_info)
      return false;
   current_video->viewport_info(video_driver_data, viewport);
   return true;
}

static void video_driver_set_title_buf(void)
{
   struct retro_system_info info;
   current_core.retro_get_system_info(&info);

   fill_pathname_join_concat_noext(
         video_driver_title_buf,
         msg_hash_to_str(MSG_PROGRAM),
         " ",
         info.library_name,
         sizeof(video_driver_title_buf));
   strlcat(video_driver_title_buf, " ",
         sizeof(video_driver_title_buf));
   strlcat(video_driver_title_buf, info.library_version,
         sizeof(video_driver_title_buf));
}

/**
 * video_viewport_get_scaled_integer:
 * @vp            : Viewport handle
 * @width         : Width.
 * @height        : Height.
 * @aspect_ratio  : Aspect ratio (in float).
 * @keep_aspect   : Preserve aspect ratio?
 *
 * Gets viewport scaling dimensions based on
 * scaled integer aspect ratio.
 **/
void video_viewport_get_scaled_integer(struct video_viewport *vp,
      unsigned width, unsigned height,
      float aspect_ratio, bool keep_aspect)
{
   int padding_x        = 0;
   int padding_y        = 0;
   settings_t *settings = configuration_settings;

   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      struct video_viewport *custom = video_viewport_get_custom();

      if (custom)
      {
         padding_x = width - custom->width;
         padding_y = height - custom->height;
         width     = custom->width;
         height    = custom->height;
      }
   }
   else
   {
      unsigned base_width;
      /* Use system reported sizes as these define the
       * geometry for the "normal" case. */
      unsigned base_height                 =
         video_driver_av_info.geometry.base_height;

      if (base_height == 0)
         base_height = 1;

      /* Account for non-square pixels.
       * This is sort of contradictory with the goal of integer scale,
       * but it is desirable in some cases.
       *
       * If square pixels are used, base_height will be equal to
       * system->av_info.base_height. */
      base_width = (unsigned)roundf(base_height * aspect_ratio);

      /* Make sure that we don't get 0x scale ... */
      if (width >= base_width && height >= base_height)
      {
         if (keep_aspect)
         {
            /* X/Y scale must be same. */
            unsigned max_scale = MIN(width / base_width,
                  height / base_height);
            padding_x          = width - base_width * max_scale;
            padding_y          = height - base_height * max_scale;
         }
         else
         {
            /* X/Y can be independent, each scaled as much as possible. */
            padding_x = width % base_width;
            padding_y = height % base_height;
         }
      }

      width     -= padding_x;
      height    -= padding_y;
   }

   vp->width  = width;
   vp->height = height;
   vp->x      = padding_x / 2;
   vp->y      = padding_y / 2;
}

struct video_viewport *video_viewport_get_custom(void)
{
   settings_t *settings = configuration_settings;
   return &settings->video_viewport_custom;
}

unsigned video_pixel_get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

/**
 * video_driver_frame:
 * @data                 : pointer to data of the video frame.
 * @width                : width of the video frame.
 * @height               : height of the video frame.
 * @pitch                : pitch of the video frame.
 *
 * Video frame render callback function.
 **/
void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   static char video_driver_msg[256];
   video_frame_info_t video_info;
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   static float last_fps, frame_time;
   retro_time_t        new_time                      =
      cpu_features_get_time_usec();

   if (!video_driver_active)
      return;

   if (video_driver_scaler_ptr && data &&
         (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555) &&
         (data != RETRO_HW_FRAME_BUFFER_VALID))
   {
      if (video_pixel_frame_scale(
               video_driver_scaler_ptr->scaler,
               video_driver_scaler_ptr->scaler_out,
               data, width, height, pitch))
      {
         data                = video_driver_scaler_ptr->scaler_out;
         pitch               = video_driver_scaler_ptr->scaler->out_stride;
      }
   }

   if (data)
      frame_cache_data = data;
   frame_cache_width   = width;
   frame_cache_height  = height;
   frame_cache_pitch   = pitch;

   video_driver_build_info(&video_info);

   /* Get the amount of frames per seconds. */
   if (video_driver_frame_count)
   {
      static char title[256];
      unsigned write_index                         =
         video_driver_frame_time_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
      frame_time                                   = new_time - fps_time;
      video_driver_frame_time_samples[write_index] = frame_time;
      fps_time                                     = new_time;

      if (video_driver_frame_count == 1)
         strlcpy(title, video_driver_window_title, sizeof(title));

      if (video_info.fps_show)
      {
         snprintf(video_info.fps_text, sizeof(video_info.fps_text),
               "FPS: %6.1f", last_fps);
         if (video_info.framecount_show)
            strlcat(video_info.fps_text, " || ", sizeof(video_info.fps_text));
      }

      if (video_info.framecount_show)
      {
         char frames_text[64];
         snprintf(frames_text,
               sizeof(frames_text),
               "%s: %" PRIu64, msg_hash_to_str(MSG_FRAMES),
               (uint64_t)video_driver_frame_count);
         strlcat(video_info.fps_text, frames_text, sizeof(video_info.fps_text));
      }

      if ((video_driver_frame_count % FPS_UPDATE_INTERVAL) == 0)
      {
         last_fps = TIME_TO_FPS(curr_time, new_time, FPS_UPDATE_INTERVAL);

         strlcpy(video_driver_window_title, title, sizeof(video_driver_window_title));

         if (!string_is_empty(video_info.fps_text))
         {
            strlcat(video_driver_window_title, "|| ", sizeof(video_driver_window_title));
            strlcat(video_driver_window_title,
                  video_info.fps_text, sizeof(video_driver_window_title));
         }

         curr_time                        = new_time;
         video_driver_window_title_update = true;
      }
   }
   else
   {
      curr_time = fps_time = new_time;

      strlcpy(video_driver_window_title,
            video_driver_title_buf,
            sizeof(video_driver_window_title));

      if (video_info.fps_show)
         strlcpy(video_info.fps_text,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(video_info.fps_text));

      video_driver_window_title_update = true;
   }

   video_info.frame_rate  = last_fps;
   video_info.frame_time  = frame_time / 1000.0f;
   video_info.frame_count = (uint64_t) video_driver_frame_count;

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if (
         (
             !video_driver_state_filter
          || !video_info.post_filter_record
          || !data
          || video_driver_record_gpu_buffer
         ) && recording_data
           && recording_driver && recording_driver->push_video
      )
      recording_dump_frame(data, width, height,
            pitch, video_info.runloop_is_idle);

   if (data && video_driver_state_filter)
   {
      unsigned output_width                             = 0;
      unsigned output_height                            = 0;
      unsigned output_pitch                             = 0;

      rarch_softfilter_get_output_size(video_driver_state_filter,
            &output_width, &output_height, width, height);

      output_pitch = (output_width) * video_driver_state_out_bpp;

      rarch_softfilter_process(video_driver_state_filter,
            video_driver_state_buffer, output_pitch,
            data, width, height, pitch);

      if (video_info.post_filter_record && recording_data
           && recording_driver && recording_driver->push_video)
         recording_dump_frame(video_driver_state_buffer,
               output_width, output_height, output_pitch,
               video_info.runloop_is_idle);

      data   = video_driver_state_buffer;
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }

   video_driver_msg[0] = '\0';

   if (video_info.font_enable)
   {
      const char *msg = NULL;
      runloop_msg_queue_lock();
      msg = msg_queue_pull(runloop_msg_queue);
      if (msg)
         strlcpy(video_driver_msg, msg, sizeof(video_driver_msg));
      runloop_msg_queue_unlock();
   }

   if (video_info.statistics_show)
   {
      audio_statistics_t audio_stats         = {0.0f};
      double stddev                          = 0.0;
      struct retro_system_av_info *av_info   = &video_driver_av_info;
      unsigned red                           = 255;
      unsigned green                         = 255;
      unsigned blue                          = 255;
      unsigned alpha                         = 255;

      video_monitor_fps_statistics(NULL, &stddev, NULL);

      video_info.osd_stat_params.x           = 0.010f;
      video_info.osd_stat_params.y           = 0.950f;
      video_info.osd_stat_params.scale       = 1.0f;
      video_info.osd_stat_params.full_screen = true;
      video_info.osd_stat_params.drop_x      = -2;
      video_info.osd_stat_params.drop_y      = -2;
      video_info.osd_stat_params.drop_mod    = 0.3f;
      video_info.osd_stat_params.drop_alpha  = 1.0f;
      video_info.osd_stat_params.color       = COLOR_ABGR(
            red, green, blue, alpha);

      audio_compute_buffer_statistics(&audio_stats);

      snprintf(video_info.stat_text,
            sizeof(video_info.stat_text),
            "Video Statistics:\n -Frame rate: %6.2f fps\n -Frame time: %6.2f ms\n -Frame time deviation: %.3f %%\n"
            " -Frame count: %" PRIu64"\n -Viewport: %d x %d x %3.2f\n"
            "Audio Statistics:\n -Average buffer saturation: %.2f %%\n -Standard deviation: %.2f %%\n -Time spent close to underrun: %.2f %%\n -Time spent close to blocking: %.2f %%\n -Sample count: %d\n"
            "Core Geometry:\n -Size: %u x %u\n -Max Size: %u x %u\n -Aspect: %3.2f\nCore Timing:\n -FPS: %3.2f\n -Sample Rate: %6.2f\n",
            video_info.frame_rate,
            video_info.frame_time,
            100.0 * stddev,
            video_info.frame_count,
            video_info.width,
            video_info.height,
            video_info.refresh_rate,
            audio_stats.average_buffer_saturation,
            audio_stats.std_deviation_percentage,
            audio_stats.close_to_underrun,
            audio_stats.close_to_blocking,
            audio_stats.samples,
            av_info->geometry.base_width,
            av_info->geometry.base_height,
            av_info->geometry.max_width,
            av_info->geometry.max_height,
            av_info->geometry.aspect_ratio,
            av_info->timing.fps,
            av_info->timing.sample_rate);

      /* TODO/FIXME - add OSD chat text here */
#if 0
      snprintf(video_info.chat_text, sizeof(video_info.chat_text),
            "anon: does retroarch netplay have in-game chat?\nradius: I don't know \u2605");
#endif
   }

   video_driver_active = current_video->frame(
         video_driver_data, data, width, height,
         video_driver_frame_count,
         (unsigned)pitch, video_driver_msg, &video_info);

   video_driver_frame_count++;

   /* Display the FPS, with a higher priority. */
   if (video_info.fps_show || video_info.framecount_show)
   {
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      if (!menu_widgets_set_fps_text(video_info.fps_text))
#endif
         runloop_msg_queue_push(video_info.fps_text, 2, 1, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* trigger set resolution*/
   if (video_info.crt_switch_resolution)
   {
      video_driver_crt_switching_active = true;

      if (video_info.crt_switch_resolution_super == 2560)
         width = 2560;
      if (video_info.crt_switch_resolution_super == 3840)
         width = 3840;
      if (video_info.crt_switch_resolution_super == 1920)
         width = 1920;
      
      if (video_info.crt_switch_resolution_super == 1)
         video_driver_crt_dynamic_super_width = true;
      else 
         video_driver_crt_dynamic_super_width = false;
      
      crt_switch_res_core(width, height, video_driver_core_hz, video_info.crt_switch_resolution, video_info.crt_switch_center_adjust, video_info.monitor_index, video_driver_crt_dynamic_super_width);
   }
   else if (!video_info.crt_switch_resolution)
      video_driver_crt_switching_active = false;

   /* trigger set resolution*/
}

void crt_switch_driver_reinit(void)
{
   video_driver_reinit();
}

void video_driver_display_type_set(enum rarch_display_type type)
{
   video_driver_display_type = type;
}

uintptr_t video_driver_display_get(void)
{
   return video_driver_display;
}

void video_driver_display_set(uintptr_t idx)
{
   video_driver_display = idx;
}

enum rarch_display_type video_driver_display_type_get(void)
{
   return video_driver_display_type;
}

void video_driver_window_set(uintptr_t idx)
{
   video_driver_window = idx;
}

uintptr_t video_driver_window_get(void)
{
   return video_driver_window;
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type  filter_type,
      uintptr_t *id)
{
#ifdef HAVE_THREADS
   bool is_threaded = video_driver_is_threaded_internal();
#endif
   if (!id || !video_driver_poke || !video_driver_poke->load_texture)
      return false;

#ifdef HAVE_THREADS
   if (is_threaded)
      video_context_driver_make_current(false);
#endif

   *id = video_driver_poke->load_texture(video_driver_data, data,
         video_driver_is_threaded_internal(),
         filter_type);

   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
#ifdef HAVE_THREADS
   bool is_threaded = video_driver_is_threaded_internal();
#endif
   if (!video_driver_poke || !video_driver_poke->unload_texture)
      return false;

#ifdef HAVE_THREADS
   if (is_threaded)
      video_context_driver_make_current(false);
#endif

   video_driver_poke->unload_texture(video_driver_data, *id);
   *id = 0;
   return true;
}

void video_driver_build_info(video_frame_info_t *video_info)
{
   video_viewport_t *custom_vp       = NULL;
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context_internal();
   settings_t *settings              = configuration_settings;
#ifdef HAVE_THREADS
   bool is_threaded                  = video_driver_is_threaded_internal();
   video_driver_threaded_lock(is_threaded);
#endif
   custom_vp                         = &settings->video_viewport_custom;
   video_info->refresh_rate          = settings->floats.video_refresh_rate;
   video_info->crt_switch_resolution = settings->uints.crt_switch_resolution;
   video_info->crt_switch_resolution_super = settings->uints.crt_switch_resolution_super;
   video_info->crt_switch_center_adjust    = settings->ints.crt_switch_center_adjust;
   video_info->black_frame_insertion = settings->bools.video_black_frame_insertion;
   video_info->hard_sync             = settings->bools.video_hard_sync;
   video_info->hard_sync_frames      = settings->uints.video_hard_sync_frames;
   video_info->fps_show              = settings->bools.video_fps_show;
   video_info->statistics_show       = settings->bools.video_statistics_show;
   video_info->framecount_show       = settings->bools.video_framecount_show;
   video_info->scale_integer         = settings->bools.video_scale_integer;
   video_info->aspect_ratio_idx      = settings->uints.video_aspect_ratio_idx;
   video_info->post_filter_record    = settings->bools.video_post_filter_record;
   video_info->input_menu_swap_ok_cancel_buttons    = settings->bools.input_menu_swap_ok_cancel_buttons;
   video_info->max_swapchain_images  = settings->uints.video_max_swapchain_images;
   video_info->windowed_fullscreen   = settings->bools.video_windowed_fullscreen;
   video_info->fullscreen            = settings->bools.video_fullscreen || retroarch_is_forced_fullscreen();
   video_info->monitor_index         = settings->uints.video_monitor_index;
   video_info->shared_context        = settings->bools.video_shared_context;

   if (core_set_shared_context && hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      video_info->shared_context     = true;

   video_info->font_enable           = settings->bools.video_font_enable;
   video_info->font_msg_pos_x        = settings->floats.video_msg_pos_x;
   video_info->font_msg_pos_y        = settings->floats.video_msg_pos_y;
   video_info->font_msg_color_r      = settings->floats.video_msg_color_r;
   video_info->font_msg_color_g      = settings->floats.video_msg_color_g;
   video_info->font_msg_color_b      = settings->floats.video_msg_color_b;
   video_info->custom_vp_x           = custom_vp->x;
   video_info->custom_vp_y           = custom_vp->y;
   video_info->custom_vp_width       = custom_vp->width;
   video_info->custom_vp_height      = custom_vp->height;
   video_info->custom_vp_full_width  = custom_vp->full_width;
   video_info->custom_vp_full_height = custom_vp->full_height;

   video_info->fps_text[0]           = '\0';

   video_info->width                 = video_driver_width;
   video_info->height                = video_driver_height;

   video_info->use_rgba              = video_driver_use_rgba;

   video_info->libretro_running       = false;
   video_info->msg_bgcolor_enable     = settings->bools.video_msg_bgcolor_enable;

#ifdef HAVE_MENU
   video_info->menu_is_alive          = menu_driver_is_alive();
   video_info->menu_footer_opacity    = settings->floats.menu_footer_opacity;
   video_info->menu_header_opacity    = settings->floats.menu_header_opacity;
   video_info->materialui_color_theme = settings->uints.menu_materialui_color_theme;
   video_info->ozone_color_theme      = settings->uints.menu_ozone_color_theme;
   video_info->menu_shader_pipeline   = settings->uints.menu_xmb_shader_pipeline;
   video_info->xmb_theme              = settings->uints.menu_xmb_theme;
   video_info->xmb_color_theme        = settings->uints.menu_xmb_color_theme;
   video_info->timedate_enable        = settings->bools.menu_timedate_enable;
   video_info->battery_level_enable   = settings->bools.menu_battery_level_enable;
   video_info->xmb_shadows_enable          = 
      settings->bools.menu_xmb_shadows_enable;
   video_info->xmb_alpha_factor            = 
      settings->uints.menu_xmb_alpha_factor;
   video_info->menu_wallpaper_opacity      = 
      settings->floats.menu_wallpaper_opacity;
   video_info->menu_framebuffer_opacity    = 
      settings->floats.menu_framebuffer_opacity;

   video_info->libretro_running            = current_core.game_loaded;
#else
   video_info->menu_is_alive               = false;
   video_info->menu_footer_opacity         = 0.0f;
   video_info->menu_header_opacity         = 0.0f;
   video_info->materialui_color_theme      = 0;
   video_info->menu_shader_pipeline        = 0;
   video_info->xmb_color_theme             = 0;
   video_info->xmb_theme                   = 0;
   video_info->timedate_enable             = false;
   video_info->battery_level_enable        = false;
   video_info->xmb_shadows_enable          = false;
   video_info->xmb_alpha_factor            = 0.0f;
   video_info->menu_framebuffer_opacity    = 0.0f;
   video_info->menu_wallpaper_opacity      = 0.0f;
#endif

   video_info->is_perfcnt_enable           = runloop_perfcnt_enable;
   video_info->runloop_is_paused           = runloop_paused;
   video_info->runloop_is_idle             = runloop_idle;
   video_info->runloop_is_slowmotion       = runloop_slowmotion;

   video_info->input_driver_nonblock_state = input_driver_nonblock_state;
   video_info->context_data                = video_context_data;
   video_info->cb_update_window_title      = current_video_context.update_window_title;
   video_info->cb_swap_buffers             = current_video_context.swap_buffers;
   video_info->cb_get_metrics              = current_video_context.get_metrics;
   video_info->cb_set_resize               = current_video_context.set_resize;

   video_info->userdata                    = video_driver_get_ptr_internal(false);

#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
}

/**
 * video_driver_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool video_driver_translate_coord_viewport(
      struct video_viewport *vp,
      int mouse_x,           int mouse_y,
      int16_t *res_x,        int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y)
{
   int scaled_screen_x, scaled_screen_y, scaled_x, scaled_y;
   int norm_vp_width         = (int)vp->width;
   int norm_vp_height        = (int)vp->height;
   int norm_full_vp_width    = (int)vp->full_width;
   int norm_full_vp_height   = (int)vp->full_height;

   if (norm_full_vp_width <= 0 || norm_full_vp_height <= 0)
      return false;

   if (mouse_x >= 0 && mouse_x <= norm_full_vp_width)
      scaled_screen_x = ((2 * mouse_x * 0x7fff)
            / norm_full_vp_width)  - 0x7fff;
   else
      scaled_screen_x = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_full_vp_height)
      scaled_screen_y = ((2 * mouse_y * 0x7fff)
            / norm_full_vp_height) - 0x7fff;
   else
      scaled_screen_y = -0x8000; /* OOB */

   mouse_x           -= vp->x;
   mouse_y           -= vp->y;

   if (mouse_x >= 0 && mouse_x <= norm_vp_width)
      scaled_x        = ((2 * mouse_x * 0x7fff)
            / norm_vp_width) - 0x7fff;
   else
      scaled_x        = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_vp_height)
      scaled_y        = ((2 * mouse_y * 0x7fff)
            / norm_vp_height) - 0x7fff;
   else
      scaled_y        = -0x8000; /* OOB */

   *res_x             = scaled_x;
   *res_y             = scaled_y;
   *res_screen_x      = scaled_screen_x;
   *res_screen_y      = scaled_screen_y;

   return true;
}

#define video_has_focus() ((current_video_context.has_focus) ? (current_video_context.has_focus && current_video_context.has_focus(video_context_data)) : (current_video->focus) ? (current_video && current_video->focus && current_video->focus(video_driver_data)) : true)

bool video_driver_has_focus(void)
{
   return video_has_focus();
}

void video_driver_get_window_title(char *buf, unsigned len)
{
   if (buf && video_driver_window_title_update)
   {
      strlcpy(buf, video_driver_window_title, len);
      video_driver_window_title_update = false;
   }
}

/**
 * find_video_context_driver_driver_index:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds graphics context driver index by @ident name.
 *
 * Returns: graphics context driver index if driver was found, otherwise
 * -1.
 **/
static int find_video_context_driver_index(const char *ident)
{
   unsigned i;
   for (i = 0; gfx_ctx_drivers[i]; i++)
      if (string_is_equal_noncase(ident, gfx_ctx_drivers[i]->ident))
         return i;
   return -1;
}

/**
 * find_prev_context_driver:
 *
 * Finds previous driver in graphics context driver array.
 **/
bool video_context_driver_find_prev_driver(void)
{
   settings_t *settings = configuration_settings;
   int                i = find_video_context_driver_index(
         settings->arrays.video_context_driver);

   if (i > 0)
   {
      strlcpy(settings->arrays.video_context_driver,
            gfx_ctx_drivers[i - 1]->ident,
            sizeof(settings->arrays.video_context_driver));
      return true;
   }

   RARCH_WARN("Couldn't find any previous video context driver.\n");
   return false;
}

/**
 * find_next_context_driver:
 *
 * Finds next driver in graphics context driver array.
 **/
bool video_context_driver_find_next_driver(void)
{
   settings_t *settings = configuration_settings;
   int                i = find_video_context_driver_index(
         settings->arrays.video_context_driver);

   if (i >= 0 && gfx_ctx_drivers[i + 1])
   {
      strlcpy(settings->arrays.video_context_driver,
            gfx_ctx_drivers[i + 1]->ident,
            sizeof(settings->arrays.video_context_driver));
      return true;
   }

   RARCH_WARN("Couldn't find any next video context driver.\n");
   return false;
}

/**
 * video_context_driver_init:
 * @data                    : Input data.
 * @ctx                     : Graphics context driver to initialize.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Initialize graphics context driver.
 *
 * Returns: graphics context driver if successfully initialized,
 * otherwise NULL.
 **/
static const gfx_ctx_driver_t *video_context_driver_init(
      void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx,
      void **ctx_data)
{
   video_frame_info_t video_info;

   if (!ctx->bind_api(data, api, major, minor))
   {
      RARCH_WARN("Failed to bind API (#%u, version %u.%u)"
            " on context driver \"%s\".\n",
            (unsigned)api, major, minor, ctx->ident);

      return NULL;
   }

   video_driver_build_info(&video_info);

   if (!(*ctx_data = ctx->init(&video_info, data)))
      return NULL;

   if (ctx->bind_hw_render)
      ctx->bind_hw_render(*ctx_data,
            video_info.shared_context && hw_render_ctx);

   return ctx;
}

/**
 * video_context_driver_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   int                i = find_video_context_driver_index(ident);

   if (i >= 0)
   {
      const gfx_ctx_driver_t *ctx = video_context_driver_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);
      if (ctx)
      {
         video_context_data = *ctx_data;
         return ctx;
      }
   }

   for (i = 0; gfx_ctx_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx =
         video_context_driver_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);

      if (ctx)
      {
         video_context_data = *ctx_data;
         return ctx;
      }
   }

   return NULL;
}

bool video_context_driver_init_image_buffer(const video_info_t *data)
{
   if (
            current_video_context.image_buffer_init
         && current_video_context.image_buffer_init(
            video_context_data, data))
      return true;
   return false;
}

bool video_context_driver_write_to_image_buffer(gfx_ctx_image_t *img)
{
   if (
            current_video_context.image_buffer_write
         && current_video_context.image_buffer_write(video_context_data,
            img->frame, img->width, img->height, img->pitch,
            img->rgb32, img->index, img->handle))
      return true;
   return false;
}

bool video_context_driver_get_video_output_prev(void)
{
   if (!current_video_context.get_video_output_prev)
      return false;
   current_video_context.get_video_output_prev(video_context_data);
   return true;
}

bool video_context_driver_get_video_output_next(void)
{
   if (!current_video_context.get_video_output_next)
      return false;
   current_video_context.get_video_output_next(video_context_data);
   return true;
}

void video_context_driver_make_current(bool release)
{
   if (current_video_context.make_current)
      current_video_context.make_current(release);
}

bool video_context_driver_translate_aspect(gfx_ctx_aspect_t *aspect)
{
   if (!video_context_data || !aspect)
      return false;
   if (!current_video_context.translate_aspect)
      return false;
   *aspect->aspect = current_video_context.translate_aspect(
         video_context_data, aspect->width, aspect->height);
   return true;
}

void video_context_driver_free(void)
{
   if (current_video_context.destroy)
      current_video_context.destroy(video_context_data);
   video_context_driver_destroy();
   video_context_data    = NULL;
}

bool video_context_driver_get_video_output_size(gfx_ctx_size_t *size_data)
{
   if (!size_data)
      return false;
   if (!current_video_context.get_video_output_size)
      return false;
   current_video_context.get_video_output_size(video_context_data,
         size_data->width, size_data->height);
   return true;
}

bool video_context_driver_swap_interval(int *interval)
{
   int current_interval                   = *interval;
   settings_t *settings                   = configuration_settings;
   bool adaptive_vsync_enabled            = video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && settings->bools.video_adaptive_vsync;

   if (!current_video_context.swap_interval)
      return false;
   if (adaptive_vsync_enabled && current_interval == 1)
      current_interval = -1;
   current_video_context.swap_interval(video_context_data, current_interval);
   return true;
}

bool video_context_driver_get_proc_address(gfx_ctx_proc_address_t *proc)
{
   if (!current_video_context.get_proc_address)
      return false;

   proc->addr = current_video_context.get_proc_address(proc->sym);

   return true;
}

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics)
{
   if (
         current_video_context.get_metrics(video_context_data,
            metrics->type,
            metrics->value))
      return true;
   return false;
}

bool video_context_driver_get_refresh_rate(float *refresh_rate)
{
   float refresh_holder      = 0;

   if (!current_video_context.get_refresh_rate || !refresh_rate)
      return false;
   if (!video_context_data)
      return false;

   if (!video_driver_crt_switching_active)
      if (refresh_rate)
         *refresh_rate =
             current_video_context.get_refresh_rate(video_context_data);

   if (video_driver_crt_switching_active)
   {
      if (refresh_rate)
         refresh_holder  =
             current_video_context.get_refresh_rate(video_context_data);
      if (refresh_holder != video_driver_core_hz) /* Fix for incorrect interlacing detection -- HARD SET VSNC TO REQUIRED REFRESH FOR CRT*/
         *refresh_rate = video_driver_core_hz;
   }

   return true;
}

bool video_context_driver_input_driver(gfx_ctx_input_t *inp)
{
   settings_t *settings          = configuration_settings;
   const char *joypad_name       = settings->arrays.input_joypad_driver;

   if (!current_video_context.input_driver)
      return false;
   current_video_context.input_driver(
         video_context_data, joypad_name,
         inp->input, inp->input_data);
   return true;
}

bool video_context_driver_suppress_screensaver(bool *bool_data)
{
   if (     video_context_data
         && current_video_context.suppress_screensaver(
            video_context_data, *bool_data))
      return true;
   return false;
}

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident)
{
   if (!ident)
      return false;
   ident->ident = current_video_context.ident;
   return true;
}

bool video_context_driver_set_video_mode(gfx_ctx_mode_t *mode_info)
{
   video_frame_info_t video_info;

   if (!current_video_context.set_video_mode)
      return false;

   video_driver_build_info(&video_info);

   if (!current_video_context.set_video_mode(
            video_context_data, &video_info, mode_info->width,
            mode_info->height, mode_info->fullscreen))
      return false;
   return true;
}

bool video_context_driver_get_video_size(gfx_ctx_mode_t *mode_info)
{
   if (!current_video_context.get_video_size)
      return false;
   current_video_context.get_video_size(video_context_data,
         &mode_info->width, &mode_info->height);
   return true;
}

bool video_context_driver_show_mouse(bool *bool_data)
{
   if (!current_video_context.show_mouse)
      return false;
   current_video_context.show_mouse(video_context_data, *bool_data);
   return true;
}

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags)
{
   if (!current_video_context.get_flags)
      return false;

   if (deferred_video_context_driver_set_flags)
   {
      flags->flags                            = deferred_flag_data.flags;
      deferred_video_context_driver_set_flags = false;
      return true;
   }

   flags->flags = current_video_context.get_flags(video_context_data);
   return true;
}

bool video_driver_get_flags(gfx_ctx_flags_t *flags)
{
   if (!video_driver_poke || !video_driver_poke->get_flags)
      return false;
   flags->flags = video_driver_poke->get_flags(video_driver_data);
   return true;
}

/**
 * video_driver_test_all_flags:
 * @testflag          : flag to test
 *
 * Poll both the video and context driver's flags and test
 * whether @testflag is set or not.
 **/
bool video_driver_test_all_flags(enum display_flags testflag)
{
   gfx_ctx_flags_t flags;

   if (video_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   if (video_context_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   return false;
}

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags)
{
   if (!flags)
      return false;
   if (!current_video_context.set_flags)
   {
      deferred_flag_data.flags                = flags->flags;
      deferred_video_context_driver_set_flags = true;
      return false;
   }

   current_video_context.set_flags(video_context_data, flags->flags);
   return true;
}

enum gfx_ctx_api video_context_driver_get_api(void)
{
   enum gfx_ctx_api ctx_api = video_context_data ?
      current_video_context.get_api(video_context_data) : GFX_CTX_NONE;

   if (ctx_api == GFX_CTX_NONE)
   {
      const char *video_ident  = (current_video) ? current_video->ident : NULL;
      if (string_is_equal(video_ident, "d3d9"))
         return GFX_CTX_DIRECT3D9_API;
      else if (string_is_equal(video_ident, "d3d10"))
         return GFX_CTX_DIRECT3D10_API;
      else if (string_is_equal(video_ident, "d3d11"))
         return GFX_CTX_DIRECT3D11_API;
      else if (string_is_equal(video_ident, "d3d12"))
         return GFX_CTX_DIRECT3D12_API;
      else if (string_is_equal(video_ident, "gx2"))
         return GFX_CTX_GX2_API;
      else if (string_is_equal(video_ident, "gx"))
         return GFX_CTX_GX_API;
      else if (string_is_equal(video_ident, "gl"))
         return GFX_CTX_OPENGL_API;
      else if (string_is_equal(video_ident, "gl1"))
         return GFX_CTX_OPENGL_API;
      else if (string_is_equal(video_ident, "glcore"))
         return GFX_CTX_OPENGL_API;
      else if (string_is_equal(video_ident, "vulkan"))
         return GFX_CTX_VULKAN_API;
      else if (string_is_equal(video_ident, "metal"))
         return GFX_CTX_METAL_API;

      return GFX_CTX_NONE;
   }

   return ctx_api;
}

bool video_driver_has_windowed(void)
{
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   if (video_driver_data && current_video->has_windowed)
      return current_video->has_windowed(video_driver_data);
   else if (video_context_data && current_video_context.has_windowed)
      return current_video_context.has_windowed(video_context_data);
#endif
   return false;
}

bool video_driver_cached_frame_has_valid_framebuffer(void)
{
   if (frame_cache_data)
      return (frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID);
   return false;
}


bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader)
{
   void *video_driver                       = video_driver_get_ptr_internal(true);
   const video_poke_interface_t *video_poke = video_driver_poke;

   shader->data = NULL;
   if (!video_poke || !video_driver || !video_poke->get_current_shader)
      return false;
   shader->data = video_poke->get_current_shader(video_driver);
   return true;
}

float video_driver_get_refresh_rate(void)
{
   if (video_driver_poke && video_driver_poke->get_refresh_rate)
      return video_driver_poke->get_refresh_rate(video_driver_data);

   return 0.0f;
}

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
static bool video_driver_has_widgets(void)
{
   return current_video && current_video->menu_widgets_enabled 
      && current_video->menu_widgets_enabled(video_driver_data);
}
#endif

void video_driver_set_gpu_device_string(const char *str)
{
   strlcpy(video_driver_gpu_device_string, str, sizeof(video_driver_gpu_device_string));
}

const char* video_driver_get_gpu_device_string(void)
{
   return video_driver_gpu_device_string;
}

void video_driver_set_gpu_api_version_string(const char *str)
{
   strlcpy(video_driver_gpu_api_version_string, str, sizeof(video_driver_gpu_api_version_string));
}

const char* video_driver_get_gpu_api_version_string(void)
{
   return video_driver_gpu_api_version_string;
}

/* string list stays owned by the caller and must be available at all times after the video driver is inited */
void video_driver_set_gpu_api_devices(enum gfx_ctx_api api, struct string_list *list)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
      {
         gpu_map[i].list = list;
         break;
      }
   }
}

struct string_list* video_driver_get_gpu_api_devices(enum gfx_ctx_api api)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
         return gpu_map[i].list;
   }

   return NULL;
}


/* LOCATION */

/**
 * location_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to location driver at index. Can be NULL
 * if nothing found.
 **/
const void *location_driver_find_handle(int idx)
{
   const void *drv = location_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * location_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of location driver at index. Can be NULL
 * if nothing found.
 **/
const char *location_driver_find_ident(int idx)
{
   const location_driver_t *drv = location_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_location_driver_options:
 *
 * Get an enumerated list of all location driver names,
 * separated by '|'.
 *
 * Returns: string listing of all location driver names,
 * separated by '|'.
 **/
const char* config_get_location_driver_options(void)
{
   return char_list_new_special(STRING_LIST_LOCATION_DRIVERS, NULL);
}

static void find_location_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = configuration_settings;

   drv.label = "location_driver";
   drv.s     = settings->arrays.location_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i         = (int)drv.len;

   if (i >= 0)
      location_driver = (const location_driver_t*)location_driver_find_handle(i);
   else
   {

      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any location driver named \"%s\"\n",
               settings->arrays.location_driver);
         RARCH_LOG_OUTPUT("Available location drivers are:\n");
         for (d = 0; location_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", location_driver_find_ident(d));

         RARCH_WARN("Going to default to first location driver...\n");
      }

      location_driver = (const location_driver_t*)location_driver_find_handle(0);

      if (!location_driver)
         retroarch_fail(1, "find_location_driver()");
   }
}

/**
 * driver_location_start:
 *
 * Starts location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool driver_location_start(void)
{
   if (location_driver && location_data && location_driver->start)
   {
      settings_t *settings = configuration_settings;
      if (settings->bools.location_allow)
         return location_driver->start(location_data);

      runloop_msg_queue_push("Location is explicitly disabled.\n", 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return false;
}

/**
 * driver_location_stop:
 *
 * Stops location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static void driver_location_stop(void)
{
   if (location_driver && location_driver->stop && location_data)
      location_driver->stop(location_data);
}

/**
 * driver_location_set_interval:
 * @interval_msecs     : Interval time in milliseconds.
 * @interval_distance  : Distance at which to update.
 *
 * Sets interval update time for location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 **/
static void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance)
{
   if (location_driver && location_driver->set_interval
         && location_data)
      location_driver->set_interval(location_data,
            interval_msecs, interval_distance);
}

/**
 * driver_location_get_position:
 * @lat                : Latitude of current position.
 * @lon                : Longitude of current position.
 * @horiz_accuracy     : Horizontal accuracy.
 * @vert_accuracy      : Vertical accuracy.
 *
 * Gets current positioning information from
 * location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: bool (1) if successful, otherwise false (0).
 **/
static bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy)
{
   if (location_driver && location_driver->get_position
         && location_data)
      return location_driver->get_position(location_data,
            lat, lon, horiz_accuracy, vert_accuracy);

   *lat = 0.0;
   *lon = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy = 0.0;
   return false;
}

static void init_location(void)
{
   rarch_system_info_t *system = &runloop_system;

   /* Resource leaks will follow if location interface is initialized twice. */
   if (location_data)
      return;

   find_location_driver();

   location_data = location_driver->init();

   if (!location_data)
   {
      RARCH_ERR("Failed to initialize location driver. Will continue without location.\n");
      location_driver_active = false;
   }

   if (system->location_cb.initialized)
      system->location_cb.initialized();
}

static void uninit_location(void)
{
   rarch_system_info_t *system = &runloop_system;

   if (location_data && location_driver)
   {
      if (system->location_cb.deinitialized)
         system->location_cb.deinitialized();

      if (location_driver->free)
         location_driver->free(location_data);
   }

   location_data = NULL;
}

/* CAMERA */

/**
 * camera_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to camera driver at index. Can be NULL
 * if nothing found.
 **/
const void *camera_driver_find_handle(int idx)
{
   const void *drv = camera_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * camera_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of camera driver at index. Can be NULL
 * if nothing found.
 **/
const char *camera_driver_find_ident(int idx)
{
   const camera_driver_t *drv = camera_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char* config_get_camera_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CAMERA_DRIVERS, NULL);
}

static bool driver_camera_start(void)
{
   if (camera_driver && camera_data && camera_driver->start)
   {
      settings_t *settings = configuration_settings;
      if (settings->bools.camera_allow)
         return camera_driver->start(camera_data);

      runloop_msg_queue_push(
            "Camera is explicitly disabled.\n", 1, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return true;
}

static void driver_camera_stop(void)
{
   if (     camera_driver
         && camera_driver->stop
         && camera_data)
      camera_driver->stop(camera_data);
}

static void camera_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = configuration_settings;

   drv.label = "camera_driver";
   drv.s     = settings->arrays.camera_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i         = (int)drv.len;

   if (i >= 0)
      camera_driver = (const camera_driver_t*)camera_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any camera driver named \"%s\"\n",
               settings->arrays.camera_driver);
         RARCH_LOG_OUTPUT("Available camera drivers are:\n");
         for (d = 0; camera_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", camera_driver_find_ident(d));

         RARCH_WARN("Going to default to first camera driver...\n");
      }

      camera_driver = (const camera_driver_t*)camera_driver_find_handle(0);

      if (!camera_driver)
         retroarch_fail(1, "find_camera_driver()");
   }
}

/* DRIVERS */

/**
 * find_driver_nonempty:
 * @label              : string of driver type to be found.
 * @i                  : index of driver.
 * @str                : identifier name of the found driver
 *                       gets written to this string.
 * @len                : size of @str.
 *
 * Find driver based on @label.
 *
 * Returns: NULL if no driver based on @label found, otherwise
 * pointer to driver.
 **/
static const void *find_driver_nonempty(const char *label, int i,
      char *s, size_t len)
{
   const void *drv = NULL;

   if (string_is_equal(label, "camera_driver"))
   {
      drv = camera_driver_find_handle(i);
      if (drv)
         strlcpy(s, camera_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "location_driver"))
   {
      drv = location_driver_find_handle(i);
      if (drv)
         strlcpy(s, location_driver_find_ident(i), len);
   }
#ifdef HAVE_MENU
   else if (string_is_equal(label, "menu_driver"))
   {
      drv = menu_driver_find_handle(i);
      if (drv)
         strlcpy(s, menu_driver_find_ident(i), len);
   }
#endif
   else if (string_is_equal(label, "input_driver"))
   {
      drv = input_driver_find_handle(i);
      if (drv)
         strlcpy(s, input_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "input_joypad_driver"))
   {
      drv = joypad_driver_find_handle(i);
      if (drv)
         strlcpy(s, joypad_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "video_driver"))
   {
      drv = video_driver_find_handle(i);
      if (drv)
         strlcpy(s, video_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "audio_driver"))
   {
      drv = audio_driver_find_handle(i);
      if (drv)
         strlcpy(s, audio_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "record_driver"))
   {
      drv = record_driver_find_handle(i);
      if (drv)
         strlcpy(s, record_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "midi_driver"))
   {
      drv = midi_driver_find_handle(i);
      if (drv)
         strlcpy(s, midi_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "audio_resampler_driver"))
   {
      drv = audio_resampler_driver_find_handle(i);
      if (drv)
         strlcpy(s, audio_resampler_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "wifi_driver"))
   {
      drv = wifi_driver_find_handle(i);
      if (drv)
         strlcpy(s, wifi_driver_find_ident(i), len);
   }

   return drv;
}

/**
 * driver_find_index:
 * @label              : string of driver type to be found.
 * @drv                : identifier of driver to be found.
 *
 * Find index of the driver, based on @label.
 *
 * Returns: -1 if no driver based on @label and @drv found, otherwise
 * index number of the driver found in the array.
 **/
static int driver_find_index(const char * label, const char *drv)
{
   unsigned i;
   char str[256];

   str[0] = '\0';

   for (i = 0;
         find_driver_nonempty(label, i, str, sizeof(str)) != NULL; i++)
   {
      if (string_is_empty(str))
         break;
      if (string_is_equal_noncase(drv, str))
         return i;
   }

   return -1;
}

/**
 * driver_find_last:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find last driver in driver array.
 **/
static bool driver_find_last(const char *label, char *s, size_t len)
{
   unsigned i;

   for (i = 0;
         find_driver_nonempty(label, i, s, len) != NULL; i++)
   {}

   if (i)
      i = i - 1;
   else
      i = 0;

   find_driver_nonempty(label, i, s, len);
   return true;
}

/**
 * driver_find_prev:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find previous driver in driver array.
 **/
static bool driver_find_prev(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i > 0)
   {
      find_driver_nonempty(label, i - 1, s, len);
      return true;
   }

   RARCH_WARN(
         "Couldn't find any previous driver (current one: \"%s\").\n", s);
   return false;
}

/**
 * driver_find_next:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find next driver in driver array.
 **/
static bool driver_find_next(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i >= 0 && string_is_not_equal(s, "null"))
   {
      find_driver_nonempty(label, i + 1, s, len);
      return true;
   }

   RARCH_WARN("%s (current one: \"%s\").\n",
         msg_hash_to_str(MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER),
         s);
   return false;
}

static void driver_adjust_system_rates(void)
{
   audio_driver_monitor_adjust_system_rates();
   video_driver_monitor_adjust_system_rates();

   if (!video_driver_get_ptr_internal(false))
      return;

   if (runloop_force_nonblock)
      command_event(CMD_EVENT_VIDEO_SET_NONBLOCKING_STATE, NULL);
   else
      driver_set_nonblock_state();
}

/**
 * driver_set_nonblock_state:
 *
 * Sets audio and video drivers to nonblock state (if enabled).
 *
 * If nonblock state is false, sets
 * blocking state for both audio and video drivers instead.
 **/
void driver_set_nonblock_state(void)
{
   bool                 enable = input_driver_nonblock_state;

   /* Only apply non-block-state for video if we're using vsync. */
   if (video_driver_active && video_driver_get_ptr_internal(false))
   {
      settings_t *settings       = configuration_settings;
      bool video_nonblock        = enable;

      if (!settings->bools.video_vsync || runloop_force_nonblock)
         video_nonblock = true;
      video_driver_set_nonblock_state(video_nonblock);
   }

   audio_driver_set_nonblocking_state(enable);
}

/**
 * driver_update_system_av_info:
 * @data               : pointer to new A/V info
 *
 * Update the system Audio/Video information.
 * Will reinitialize audio/video drivers.
 * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool driver_update_system_av_info(
      const struct retro_system_av_info *info)
{
   struct retro_system_av_info *av_info  = &video_driver_av_info;
   settings_t *settings                  = configuration_settings;

   memcpy(av_info, info, sizeof(*av_info));
   command_event(CMD_EVENT_REINIT, NULL);

   /* Cannot continue recording with different parameters.
    * Take the easiest route out and just restart the recording. */
   if (recording_data)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT),
            2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);
      command_event(CMD_EVENT_RECORD_INIT, NULL);
   }

   /* Hide mouse cursor in fullscreen after
    * a RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO call. */
   if (settings->bools.video_fullscreen)
      video_driver_hide_mouse();

   return true;
}

bool audio_driver_new_devices_list(void)
{
   if (!current_audio || !current_audio->device_list_new
         || !audio_driver_context_audio_data)
      return false;
   audio_driver_devices_list = (struct string_list*)
      current_audio->device_list_new(audio_driver_context_audio_data);
   if (!audio_driver_devices_list)
      return false;
   return true;
}


/**
 * drivers_init:
 * @flags              : Bitmask of drivers to initialize.
 *
 * Initializes drivers.
 * @flags determines which drivers get initialized.
 **/
void drivers_init(int flags)
{
   bool video_is_threaded = false;
   settings_t *settings   = configuration_settings;

#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   menu_driver_ctl(RARCH_MENU_CTL_SET_OWN_DRIVER, NULL);
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      driver_adjust_system_rates();

   /* Initialize video driver */
   if (flags & DRIVER_VIDEO_MASK)
   {
      struct retro_hw_render_callback *hwr =
         video_driver_get_hw_context_internal();

      video_driver_monitor_reset();

      video_driver_lock_new();
      video_driver_filter_free();
      video_driver_set_cached_frame_ptr(NULL);
      video_driver_init_internal(&video_is_threaded);

      if (!video_driver_cache_context_ack
            && hwr->context_reset)
         hwr->context_reset();
      video_driver_unset_video_cache_context_ack();

      runloop_frame_time_last        = 0;
   }

   /* Initialize audio driver */
   if (flags & DRIVER_AUDIO_MASK)
   {
      audio_driver_init_internal(audio_callback.callback != NULL);
      audio_driver_new_devices_list();
   }

   if (flags & DRIVER_CAMERA_MASK)
   {
      /* Only initialize camera driver if we're ever going to use it. */
      if (camera_driver_active)
      {
         /* Resource leaks will follow if camera is initialized twice. */
         if (!camera_data)
         {
            camera_driver_find_driver();

            if (camera_driver)
            {
               camera_data = camera_driver->init(
                     *settings->arrays.camera_device ? 
                     settings->arrays.camera_device : NULL,
                     camera_cb.caps,
                     settings->uints.camera_width ?
                     settings->uints.camera_width : camera_cb.width,
                     settings->uints.camera_height ?
                     settings->uints.camera_height : camera_cb.height);

               if (!camera_data)
               {
                  RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
                  camera_driver_active = false;
               }

               if (camera_cb.initialized)
                  camera_cb.initialized();
            }
         }
      }
   }

   if (flags & DRIVER_LOCATION_MASK)
   {
      /* Only initialize location driver if we're ever going to use it. */
      if (location_driver_active)
         init_location();
   }

   core_info_init_current_core();

#ifdef HAVE_MENU
#ifdef HAVE_MENU_WIDGETS
   if (settings->bools.menu_enable_widgets
      && video_driver_has_widgets())
   {
      menu_widgets_init(video_is_threaded);
      menu_widgets_context_reset(video_is_threaded);
   }
#endif

   if (flags & DRIVER_VIDEO_MASK)
   {
      /* Initialize menu driver */
      if (flags & DRIVER_MENU_MASK)
         menu_driver_init(video_is_threaded);
   }
#else
   /* Qt uses core info, even if the menu is disabled */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
   {
      /* Keep non-throttled state as good as possible. */
      if (input_driver_nonblock_state)
         driver_set_nonblock_state();
   }

   /* Initialize LED driver */
   if (flags & DRIVER_LED_MASK)
      led_driver_init();

   /* Initialize MIDI  driver */
   if (flags & DRIVER_MIDI_MASK)
      midi_driver_init();
}

/**
 * uninit_drivers:
 * @flags              : Bitmask of drivers to deinitialize.
 *
 * Deinitializes drivers.
 *
 *
 * @flags determines which drivers get deinitialized.
 **/

/**
 * Driver ownership - set this to true if the platform in question needs to 'own'
 * the respective handle and therefore skip regular RetroArch
 * driver teardown/reiniting procedure.
 *
 * If  to true, the 'free' function will get skipped. It is
 * then up to the driver implementation to properly handle
 * 'reiniting' inside the 'init' function and make sure it
 * returns the existing handle instead of allocating and
 * returning a pointer to a new handle.
 *
 * Typically, if a driver intends to make use of this, it should
 * set this to true at the end of its 'init' function.
 **/
void driver_uninit(int flags)
{
   core_info_deinit_list();
   core_info_free_current_core();

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU_MASK)
   {
#if defined(HAVE_MENU_WIDGETS)
      /* This absolutely has to be done before video_driver_free()
       * is called/completes, otherwise certain menu drivers
       * (e.g. Vulkan) will segfault */
      menu_widgets_context_destroy();
      menu_widgets_free();
#endif
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
   }
#endif

   if ((flags & DRIVER_LOCATION_MASK))
      uninit_location();

   if ((flags & DRIVER_CAMERA_MASK))
   {
      if (camera_data && camera_driver)
      {
         if (camera_cb.deinitialized)
            camera_cb.deinitialized();

         if (camera_driver->free)
            camera_driver->free(camera_data);
      }

      camera_data = NULL;
   }

   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_DEINIT, NULL);

   if (flags & DRIVER_LED)
      led_driver_free();

   if (flags & DRIVERS_VIDEO_INPUT)
   {
      video_driver_free_internal();
      video_driver_lock_free();
      video_driver_data = NULL;
      video_driver_set_cached_frame_ptr(NULL);
   }

   if (flags & DRIVER_AUDIO_MASK)
      audio_driver_deinit();

   if ((flags & DRIVER_VIDEO_MASK))
   {
      video_driver_data = NULL;
   }

   if ((flags & DRIVER_INPUT_MASK))
   {
      current_input_data = NULL;
   }

   if ((flags & DRIVER_AUDIO_MASK))
      audio_driver_context_audio_data = NULL;

   if (flags & DRIVER_MIDI_MASK)
      midi_driver_free();
}

bool driver_ctl(enum driver_ctl_state state, void *data)
{
   switch (state)
   {
      case RARCH_DRIVER_CTL_DEINIT:
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
         /* Tear down menu widgets no matter what
          * in case the handle is lost in the threaded
          * video driver in the meantime
          * (breaking video_driver_has_widgets) */
         menu_widgets_context_destroy();
         menu_widgets_free();
#endif
         video_driver_destroy();
         audio_driver_destroy();

         /* Input */
         input_driver_keyboard_linefeed_enable = false;
         input_driver_block_hotkey             = false;
         input_driver_block_libretro_input     = false;
         input_driver_nonblock_state           = false;
         input_driver_flushing_input           = false;
         memset(&input_driver_turbo_btns, 0, sizeof(turbo_buttons_t));
         current_input                         = NULL;

#ifdef HAVE_MENU
         menu_driver_destroy();
#endif
         location_driver_active    = false;
         location_driver           = NULL;

         /* Camera */
         camera_driver_active   = false;
         camera_driver          = NULL;
         camera_data            = NULL;

         wifi_driver_ctl(RARCH_WIFI_CTL_DESTROY, NULL);
         core_uninit_libretro_callbacks();
         break;
      case RARCH_DRIVER_CTL_SET_REFRESH_RATE:
         {
            float *hz = (float*)data;
            video_monitor_set_refresh_rate(*hz);
            audio_driver_monitor_set_rate();
            driver_adjust_system_rates();
         }
         break;
      case RARCH_DRIVER_CTL_FIND_FIRST:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            find_driver_nonempty(drv->label, 0, drv->s, drv->len);
         }
         break;
      case RARCH_DRIVER_CTL_FIND_LAST:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_last(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_PREV:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_prev(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_NEXT:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_next(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_INDEX:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            drv->len = driver_find_index(drv->label, drv->s);
         }
         break;
      case RARCH_DRIVER_CTL_NONE:
      default:
         break;
   }

   return true;
}

/* RUNAHEAD */

#ifdef HAVE_RUNAHEAD

static void *input_list_element_constructor(void)
{
   void *ptr                          = calloc(1, sizeof(input_list_element));
   input_list_element *element        = (input_list_element*)ptr;

   element->state_size                = 256;
   element->state                     = (int16_t*)calloc(
         element->state_size, sizeof(int16_t));

   return ptr;
}

static void input_list_element_realloc(
      input_list_element *element,
      unsigned int new_size)
{
   if (new_size > element->state_size)
   {
      element->state = (int16_t*)realloc(element->state,
            new_size * sizeof(int16_t));
      memset(&element->state[element->state_size], 0,
            (new_size - element->state_size) * sizeof(int16_t));
      element->state_size = new_size;
   }
}

static void input_list_element_expand(
      input_list_element *element, unsigned int newIndex)
{
   unsigned int new_size = element->state_size;
   if (new_size == 0)
      new_size = 32;
   while (newIndex >= new_size)
      new_size *= 2;
   input_list_element_realloc(element, new_size);
}

static void input_list_element_destructor(void* element_ptr)
{
   input_list_element *element = (input_list_element*)element_ptr;
   if (!element)
      return;

   free(element->state);
   free(element_ptr);
}

static void input_state_set_last(unsigned port, unsigned device,
      unsigned index, unsigned id, int16_t value)
{
   unsigned i;
   input_list_element *element = NULL;

   if (!input_state_list)
      mylist_create(&input_state_list, 16,
            input_list_element_constructor,
            input_list_element_destructor);

   /* Find list item */
   for (i = 0; i < (unsigned)input_state_list->size; i++)
   {
      element = (input_list_element*)input_state_list->data[i];
      if (  (element->port   == port)   &&
            (element->device == device) &&
            (element->index  == index)
         )
      {
         if (id >= element->state_size)
            input_list_element_expand(element, id);
         element->state[id] = value;
         return;
      }
   }

   element            = (input_list_element*)
      mylist_add_element(input_state_list);
   element->port      = port;
   element->device    = device;
   element->index     = index;
   if (id >= element->state_size)
      input_list_element_expand(element, id);
   element->state[id] = value;
}

static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   unsigned i;

   if (!input_state_list)
      return 0;

   /* find list item */
   for (i = 0; i < (unsigned)input_state_list->size; i++)
   {
      input_list_element *element =
         (input_list_element*)input_state_list->data[i];

      if (  (element->port   == port)   &&
            (element->device == device) &&
            (element->index  == index))
      {
         if (id < element->state_size)
            return element->state[id];
         return 0;
      }
   }
   return 0;
}

static int16_t input_state_with_logging(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   if (input_state_callback_original)
   {
      int16_t result     = input_state_callback_original(
            port, device, index, id);
      int16_t last_input = input_state_get_last(port, device, index, id);
      if (result != last_input)
         input_is_dirty  = true;

      /*arbitrary limit of up to 65536 elements in state array*/
      if (id < 65536)
         input_state_set_last(port, device, index, id, result);

      return result;
   }
   return 0;
}

static void reset_hook(void)
{
   input_is_dirty = true;
   if (retro_reset_callback_original)
      retro_reset_callback_original();
}

static bool unserialize_hook(const void *buf, size_t size)
{
   input_is_dirty = true;
   if (retro_unserialize_callback_original)
      return retro_unserialize_callback_original(buf, size);
   return false;
}

static void add_input_state_hook(void)
{
   if (!input_state_callback_original)
   {
      input_state_callback_original = retro_ctx.state_cb;
      retro_ctx.state_cb            = input_state_with_logging;
      current_core.retro_set_input_state(retro_ctx.state_cb);
   }

   if (!retro_reset_callback_original)
   {
      retro_reset_callback_original = current_core.retro_reset;
      current_core.retro_reset      = reset_hook;
   }

   if (!retro_unserialize_callback_original)
   {
      retro_unserialize_callback_original = current_core.retro_unserialize;
      current_core.retro_unserialize      = unserialize_hook;
   }
}

static void remove_input_state_hook(void)
{
   if (input_state_callback_original)
   {
      retro_ctx.state_cb            = input_state_callback_original;
      current_core.retro_set_input_state(retro_ctx.state_cb);
      input_state_callback_original = NULL;
      mylist_destroy(&input_state_list);
   }

   if (retro_reset_callback_original)
   {
      current_core.retro_reset      = retro_reset_callback_original;
      retro_reset_callback_original = NULL;
   }

   if (retro_unserialize_callback_original)
   {
      current_core.retro_unserialize      = retro_unserialize_callback_original;
      retro_unserialize_callback_original = NULL;
   }
}

static void *runahead_save_state_alloc(void)
{
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)
      malloc(sizeof(retro_ctx_serialize_info_t));

   if (!savestate)
      return NULL;

   savestate->data          = NULL;
   savestate->data_const    = NULL;
   savestate->size          = 0;

   if (runahead_save_state_size > 0 && runahead_save_state_size_known)
   {
      savestate->data       = malloc(runahead_save_state_size);
      savestate->data_const = savestate->data;
      savestate->size       = runahead_save_state_size;
   }

   return savestate;
}

static void runahead_save_state_free(void *data)
{
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)data;
   if (!savestate)
      return;
   free(savestate->data);
   free(savestate);
}

static void runahead_save_state_list_init(size_t saveStateSize)
{
   runahead_save_state_size       = saveStateSize;
   runahead_save_state_size_known = true;

   mylist_create(&runahead_save_state_list, 16,
         runahead_save_state_alloc, runahead_save_state_free);
}

#if 0
static void runahead_save_state_list_rotate(void)
{
   unsigned i;
   void *element      = NULL;
   void *firstElement = runahead_save_state_list->data[0];

   for (i = 1; i < runahead_save_state_list->size; i++)
      runahead_save_state_list->data[i - 1] =
         runahead_save_state_list->data[i];
   runahead_save_state_list->data[runahead_save_state_list->size - 1] =
      firstElement;
}
#endif

/* Hooks - Hooks to cleanup, and add dirty input hooks */
static void runahead_remove_hooks(void)
{
   if (original_retro_deinit)
   {
      current_core.retro_deinit      = original_retro_deinit;
      original_retro_deinit          = NULL;
   }

   if (original_retro_unload)
   {
      current_core.retro_unload_game = original_retro_unload;
      original_retro_unload          = NULL;
   }
   remove_input_state_hook();
}

static void runahead_clear_variables(void)
{
   runahead_save_state_size          = 0;
   runahead_save_state_size_known    = false;
   runahead_video_driver_is_active   = true;
   runahead_available                = true;
   runahead_secondary_core_available = true;
   runahead_force_input_dirty        = true;
   runahead_last_frame_count         = 0;
}

static void runahead_destroy(void)
{
   mylist_destroy(&runahead_save_state_list);
   runahead_remove_hooks();
   runahead_clear_variables();
}

static void unload_hook(void)
{
   runahead_remove_hooks();
   runahead_destroy();
   secondary_core_destroy();
   if (current_core.retro_unload_game)
      current_core.retro_unload_game();
}

static void runahead_deinit_hook(void)
{
   runahead_remove_hooks();
   runahead_destroy();
   secondary_core_destroy();
   if (current_core.retro_deinit)
      current_core.retro_deinit();
}

static void runahead_add_hooks(void)
{
   if (!original_retro_deinit)
   {
      original_retro_deinit     = current_core.retro_deinit;
      current_core.retro_deinit = runahead_deinit_hook;
   }

   if (!original_retro_unload)
   {
      original_retro_unload          = current_core.retro_unload_game;
      current_core.retro_unload_game = unload_hook;
   }
   add_input_state_hook();
}

/* Runahead Code */

static void runahead_error(void)
{
   runahead_available             = false;
   mylist_destroy(&runahead_save_state_list);
   runahead_remove_hooks();
   runahead_save_state_size       = 0;
   runahead_save_state_size_known = true;
}

static bool runahead_create(void)
{
   /* get savestate size and allocate buffer */
   retro_ctx_size_info_t info;
   request_fast_savestate = true;
   core_serialize_size(&info);
   request_fast_savestate = false;

   runahead_save_state_list_init(info.size);
   runahead_video_driver_is_active = video_driver_active;

   if (runahead_save_state_size == 0 || !runahead_save_state_size_known)
   {
      runahead_error();
      return false;
   }

   runahead_add_hooks();
   runahead_force_input_dirty = true;
   mylist_resize(runahead_save_state_list, 1, true);
   return true;
}

static bool runahead_save_state(void)
{
   retro_ctx_serialize_info_t *serialize_info;
   bool okay                                  = false;

   if (!runahead_save_state_list)
      return false;

   serialize_info         =
      (retro_ctx_serialize_info_t*)runahead_save_state_list->data[0];

   request_fast_savestate = true;
   okay                   = core_serialize(serialize_info);
   request_fast_savestate = false;

   if (okay)
      return true;

   runahead_error();
   return false;
}

static bool runahead_load_state(void)
{
   bool okay                                  = false;
   retro_ctx_serialize_info_t *serialize_info = (retro_ctx_serialize_info_t*)
      runahead_save_state_list->data[0];
   bool last_dirty                            = input_is_dirty;

   request_fast_savestate                     = true;
   /* calling core_unserialize has side effects with
    * netplay (it triggers transmitting your save state)
      call retro_unserialize directly from the core instead */
   okay = current_core.retro_unserialize(
         serialize_info->data_const, serialize_info->size);

   request_fast_savestate = false;
   input_is_dirty         = last_dirty;

   if (!okay)
      runahead_error();

   return okay;
}

#if HAVE_DYNAMIC
static bool runahead_load_state_secondary(void)
{
   bool okay                                  = false;
   retro_ctx_serialize_info_t *serialize_info =
      (retro_ctx_serialize_info_t*)runahead_save_state_list->data[0];

   request_fast_savestate                     = true;
   okay                                       = secondary_core_deserialize(
         serialize_info->data_const, (int)serialize_info->size);
   request_fast_savestate = false;

   if (!okay)
   {
      runahead_secondary_core_available = false;
      runahead_error();
      return false;
   }

   return true;
}

#define runahead_run_secondary() \
   if (!secondary_core_run_use_last_input()) \
      runahead_secondary_core_available = false

#endif

#define runahead_resume_video() \
   if (runahead_video_driver_is_active) \
      video_driver_active = true; \
   else \
      video_driver_active = false

static void retro_input_poll_null(void);

static bool runahead_core_run_use_last_input(void)
{
   retro_input_poll_t old_poll_function   = retro_ctx.poll_cb;
   retro_input_state_t old_input_function = retro_ctx.state_cb;

   retro_ctx.poll_cb                      = retro_input_poll_null;
   retro_ctx.state_cb                     = input_state_get_last;

   current_core.retro_set_input_poll(retro_ctx.poll_cb);
   current_core.retro_set_input_state(retro_ctx.state_cb);

   current_core.retro_run();

   retro_ctx.poll_cb                      = old_poll_function;
   retro_ctx.state_cb                     = old_input_function;

   current_core.retro_set_input_poll(retro_ctx.poll_cb);
   current_core.retro_set_input_state(retro_ctx.state_cb);

   return true;
}

static void do_runahead(int runahead_count, bool use_secondary)
{
   int frame_number        = 0;
   bool last_frame         = false;
   bool suspended_frame    = false;
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   const bool have_dynamic = true;
#else
   const bool have_dynamic = false;
#endif
   uint64_t frame_count    = video_driver_frame_count;

   if (runahead_count <= 0 || !runahead_available)
      goto force_input_dirty;

   if (!runahead_save_state_size_known)
   {
      if (!runahead_create())
      {
         settings_t *settings = configuration_settings;
         if (!settings->bools.run_ahead_hide_warnings)
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES), 0, 2 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         goto force_input_dirty;
      }
   }

   /* Check for GUI */
   /* Hack: If we were in the GUI, force a resync. */
   if (frame_count != runahead_last_frame_count + 1)
      runahead_force_input_dirty = true;

   runahead_last_frame_count = frame_count;

   if (!use_secondary || !have_dynamic || !runahead_secondary_core_available)
   {
      /* TODO: multiple savestates for higher performance
       * when not using secondary core */
      for (frame_number = 0; frame_number <= runahead_count; frame_number++)
      {
         last_frame      = frame_number == runahead_count;
         suspended_frame = !last_frame;

         if (suspended_frame)
         {
            audio_suspended     = true;
            video_driver_active = false;
         }

         if (frame_number == 0)
            core_run();
         else
            runahead_core_run_use_last_input();

         if (suspended_frame)
         {
            runahead_resume_video();
            audio_suspended = false;
         }

         if (frame_number == 0)
         {
            if (!runahead_save_state())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return;
            }
         }

         if (last_frame)
         {
            if (!runahead_load_state())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return;
            }
         }
      }
   }
   else
   {
#if HAVE_DYNAMIC
      if (!secondary_core_ensure_exists())
      {
         runahead_secondary_core_available = false;
         runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         goto force_input_dirty;
      }

      /* run main core with video suspended */
      video_driver_active = false;
      core_run();
      runahead_resume_video();

      if (input_is_dirty || runahead_force_input_dirty)
      {
         input_is_dirty       = false;

         if (!runahead_save_state())
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            return;
         }

         if (!runahead_load_state_secondary())
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            return;
         }

         for (frame_number = 0; frame_number < runahead_count - 1; frame_number++)
         {
            video_driver_active = false;
            audio_suspended     = true;
            hard_disable_audio  = true;
            runahead_run_secondary();
            hard_disable_audio  = false;
            audio_suspended     = false;
            runahead_resume_video();
         }
      }
      audio_suspended    = true;
      hard_disable_audio = true;
      runahead_run_secondary();
      hard_disable_audio = false;
      audio_suspended    = false;
#endif
   }
   runahead_force_input_dirty = false;
   return;

force_input_dirty:
   core_run();
   runahead_force_input_dirty = true;
}
#endif

void rarch_core_runtime_tick(void)
{
   struct retro_system_av_info *av_info = &video_driver_av_info;

   if (av_info && av_info->timing.fps)
   {
      retro_time_t frame_time = (1.0 / av_info->timing.fps) * 1000000;

      /* Account for slow motion */
      if (runloop_slowmotion)
      {
         settings_t *settings       = configuration_settings;
         frame_time                 = (retro_time_t)((double)
               frame_time * settings->floats.slowmotion_ratio);
      }
      /* Account for fast forward */
      else if (runloop_fastmotion)
      {
         /* Doing it this way means we miss the first frame after
          * turning fast forward on, but it saves the overhead of
          * having to do:
          *    retro_time_t current_usec = cpu_features_get_time_usec();
          *    libretro_core_runtime_last = current_usec;
          * every frame when fast forward is off. */
         retro_time_t current_usec = cpu_features_get_time_usec();

         if (current_usec - libretro_core_runtime_last < frame_time)
            frame_time = current_usec - libretro_core_runtime_last;

         libretro_core_runtime_last = current_usec;
      }

      libretro_core_runtime_usec += frame_time;
   }
}

static void update_runtime_log(bool log_per_core)
{
   /* Initialise runtime log file */
   runtime_log_t *runtime_log = runtime_log_init(
         runtime_content_path, runtime_core_path, log_per_core);

   if (!runtime_log)
      return;

   /* Add additional runtime */
   runtime_log_add_runtime_usec(runtime_log, libretro_core_runtime_usec);

   /* Update 'last played' entry */
   runtime_log_set_last_played_now(runtime_log);

   /* Save runtime log file */
   runtime_log_save(runtime_log);

   /* Clean up */
   free(runtime_log);
}


static void retroarch_override_setting_free_state(void)
{
   unsigned i;
   for (i = 0; i < RARCH_OVERRIDE_SETTING_LAST; i++)
   {
      if (i == RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE)
      {
         unsigned j;
         for (j = 0; j < MAX_USERS; j++)
            retroarch_override_setting_unset(
                  (enum rarch_override_setting)(i), &j);
      }
      else
         retroarch_override_setting_unset(
               (enum rarch_override_setting)(i), NULL);
   }
}

static void global_free(void)
{
   global_t *global = NULL;

   content_deinit();

   path_deinit_subsystem();
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);
   command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

   rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);
   rarch_is_sram_load_disabled           = false;
   rarch_is_sram_save_disabled           = false;
   rarch_use_sram                        = false;
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_PATCH_BLOCKED, NULL);
   runloop_overrides_active              = false;
   runloop_remaps_core_active            = false;
   runloop_remaps_game_active            = false;
   runloop_remaps_content_dir_active     = false;

   current_core.has_set_input_descriptors = false;

   global = &g_extern;
   path_clear_all();
   dir_clear_all();
   if (global)
   {
      if (!string_is_empty(global->name.remapfile))
         free(global->name.remapfile);
      memset(global, 0, sizeof(struct global));
   }
   retroarch_override_setting_free_state();
}

static void retroarch_print_features(void)
{
   frontend_driver_attach_console();
   puts("");
   puts("Features:");

   _PSUPP(SUPPORTS_LIBRETRODB,      "LibretroDB",      "LibretroDB support");
   _PSUPP(SUPPORTS_COMMAND,         "Command",         "Command interface support");
   _PSUPP(SUPPORTS_NETWORK_COMMAND, "Network Command", "Network Command interface "
         "support");

   _PSUPP(SUPPORTS_SDL,             "SDL",             "SDL input/audio/video drivers");
   _PSUPP(SUPPORTS_SDL2,            "SDL2",            "SDL2 input/audio/video drivers");
   _PSUPP(SUPPORTS_X11,             "X11",             "X11 input/video drivers");
   _PSUPP(SUPPORTS_WAYLAND,         "wayland",         "Wayland input/video drivers");
   _PSUPP(SUPPORTS_THREAD,          "Threads",         "Threading support");

   _PSUPP(SUPPORTS_VULKAN,          "Vulkan",          "Vulkan video driver");
   _PSUPP(SUPPORTS_METAL,           "Metal",           "Metal video driver");
   _PSUPP(SUPPORTS_OPENGL,          "OpenGL",          "OpenGL   video driver support");
   _PSUPP(SUPPORTS_OPENGLES,        "OpenGL ES",       "OpenGLES video driver support");
   _PSUPP(SUPPORTS_XVIDEO,          "XVideo",          "Video driver");
   _PSUPP(SUPPORTS_UDEV,            "UDEV",            "UDEV/EVDEV input driver support");
   _PSUPP(SUPPORTS_EGL,             "EGL",             "Video context driver");
   _PSUPP(SUPPORTS_KMS,             "KMS",             "Video context driver");
   _PSUPP(SUPPORTS_VG,              "OpenVG",          "Video context driver");

   _PSUPP(SUPPORTS_COREAUDIO,       "CoreAudio",       "Audio driver");
   _PSUPP(SUPPORTS_COREAUDIO3,      "CoreAudioV3",     "Audio driver");
   _PSUPP(SUPPORTS_ALSA,            "ALSA",            "Audio driver");
   _PSUPP(SUPPORTS_OSS,             "OSS",             "Audio driver");
   _PSUPP(SUPPORTS_JACK,            "Jack",            "Audio driver");
   _PSUPP(SUPPORTS_RSOUND,          "RSound",          "Audio driver");
   _PSUPP(SUPPORTS_ROAR,            "RoarAudio",       "Audio driver");
   _PSUPP(SUPPORTS_PULSE,           "PulseAudio",      "Audio driver");
   _PSUPP(SUPPORTS_DSOUND,          "DirectSound",     "Audio driver");
   _PSUPP(SUPPORTS_WASAPI,          "WASAPI",     "Audio driver");
   _PSUPP(SUPPORTS_XAUDIO,          "XAudio2",         "Audio driver");
   _PSUPP(SUPPORTS_AL,              "OpenAL",          "Audio driver");
   _PSUPP(SUPPORTS_SL,              "OpenSL",          "Audio driver");

   _PSUPP(SUPPORTS_7ZIP,            "7zip",            "7zip extraction support");
   _PSUPP(SUPPORTS_ZLIB,            "zlib",            ".zip extraction support");

   _PSUPP(SUPPORTS_DYLIB,           "External",        "External filter and plugin support");

   _PSUPP(SUPPORTS_CG,              "Cg",              "Fragment/vertex shader driver");
   _PSUPP(SUPPORTS_GLSL,            "GLSL",            "Fragment/vertex shader driver");
   _PSUPP(SUPPORTS_HLSL,            "HLSL",            "Fragment/vertex shader driver");

   _PSUPP(SUPPORTS_SDL_IMAGE,       "SDL_image",       "SDL_image image loading");
   _PSUPP(SUPPORTS_RPNG,            "rpng",            "PNG image loading/encoding");
   _PSUPP(SUPPORTS_RJPEG,            "rjpeg",           "JPEG image loading");
   _PSUPP(SUPPORTS_DYNAMIC,         "Dynamic",         "Dynamic run-time loading of "
                                              "libretro library");
   _PSUPP(SUPPORTS_FFMPEG,          "FFmpeg",          "On-the-fly recording of gameplay "
                                              "with libavcodec");

   _PSUPP(SUPPORTS_FREETYPE,        "FreeType",        "TTF font rendering driver");
   _PSUPP(SUPPORTS_CORETEXT,        "CoreText",        "TTF font rendering driver "
                                              "(for OSX and/or iOS)");
   _PSUPP(SUPPORTS_NETPLAY,         "Netplay",         "Peer-to-peer netplay");
   _PSUPP(SUPPORTS_PYTHON,          "Python",          "Script support in shaders");

   _PSUPP(SUPPORTS_LIBUSB,          "Libusb",          "Libusb support");

   _PSUPP(SUPPORTS_COCOA,           "Cocoa",           "Cocoa UI companion support "
                                              "(for OSX and/or iOS)");

   _PSUPP(SUPPORTS_QT,              "Qt",              "Qt UI companion support");
   _PSUPP(SUPPORTS_V4L2,            "Video4Linux2",    "Camera driver");
}

static void retroarch_print_version(void)
{
   char str[255];
   frontend_driver_attach_console();
   str[0] = '\0';

   fprintf(stderr, "%s: %s -- v%s",
         msg_hash_to_str(MSG_PROGRAM),
         msg_hash_to_str(MSG_LIBRETRO_FRONTEND),
         PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
   printf(" -- %s --\n", retroarch_git_version);
#endif
   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   fprintf(stdout, "%s", str);
   fprintf(stdout, "Built: %s\n", __DATE__);
}

/**
 * retroarch_print_help:
 *
 * Prints help message explaining the program's commandline switches.
 **/
static void retroarch_print_help(const char *arg0)
{
   frontend_driver_attach_console();
   puts("===================================================================");
   retroarch_print_version();
   puts("===================================================================");

   printf("Usage: %s [OPTIONS]... [FILE]\n", arg0);

   puts("  -h, --help            Show this help message.");
   puts("  -v, --verbose         Verbose logging.");
   puts("      --log-file FILE   Log messages to FILE.");
   puts("      --version         Show version.");
   puts("      --features        Prints available features compiled into "
         "program.");
#ifdef HAVE_MENU
   puts("      --menu            Do not require content or libretro core to "
         "be loaded,\n"
        "                        starts directly in menu. If no arguments "
        "are passed to\n"
        "                        the program, it is equivalent to using "
        "--menu as only argument.");
#endif
   puts("  -s, --save=PATH       Path for save files (*.srm).");
   puts("  -S, --savestate=PATH  Path for the save state files (*.state).");
   puts("  -f, --fullscreen      Start the program in fullscreen regardless "
         "of config settings.");
   puts("  -c, --config=FILE     Path for config file."
#ifdef _WIN32
         "\n\t\tDefaults to retroarch.cfg in same directory as retroarch.exe."
         "\n\t\tIf a default config is not found, the program will attempt to"
         "create one."
#else
         "\n\t\tBy default looks for config in $XDG_CONFIG_HOME/retroarch/"
         "retroarch.cfg,\n\t\t$HOME/.config/retroarch/retroarch.cfg,\n\t\t"
         "and $HOME/.retroarch.cfg.\n\t\tIf a default config is not found, "
         "the program will attempt to create one based on the \n\t\t"
         "skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg). \n"
#endif
         );
   puts("      --appendconfig=FILE\n"
        "                        Extra config files are loaded in, "
        "and take priority over\n"
        "                        config selected in -c (or default). "
        "Multiple configs are\n"
        "                        delimited by '|'.");
#ifdef HAVE_DYNAMIC
   puts("  -L, --libretro=FILE   Path to libretro implementation. "
         "Overrides any config setting.");
#endif
   puts("      --subsystem=NAME  Use a subsystem of the libretro core. "
         "Multiple content\n"
        "                        files are loaded as multiple arguments. "
        "If a content\n"
        "                        file is skipped, use a blank (\"\") "
        "command line argument.\n"
        "                        Content must be loaded in an order "
        "which depends on the\n"
        "                        particular subsystem used. See verbose "
        "log output to learn\n"
        "                        how a particular subsystem wants content "
        "to be loaded.\n");

   printf("  -N, --nodevice=PORT\n"
          "                        Disconnects controller device connected "
          "to PORT (1 to %d).\n", MAX_USERS);
   printf("  -A, --dualanalog=PORT\n"
          "                        Connect a DualAnalog controller to PORT "
          "(1 to %d).\n", MAX_USERS);
   printf("  -d, --device=PORT:ID\n"
          "                        Connect a generic device into PORT of "
          "the device (1 to %d).\n", MAX_USERS);
   puts("                        Format is PORT:ID, where ID is a number "
         "corresponding to the particular device.");

   puts("  -P, --bsvplay=FILE    Playback a BSV movie file.");
   puts("  -R, --bsvrecord=FILE  Start recording a BSV movie file from "
         "the beginning.");
   puts("      --eof-exit        Exit upon reaching the end of the "
         "BSV movie file.");
   puts("  -M, --sram-mode=MODE  SRAM handling mode. MODE can be "
         "'noload-nosave',\n"
        "                        'noload-save', 'load-nosave' or "
        "'load-save'.\n"
        "                        Note: 'noload-save' implies that "
        "save files *WILL BE OVERWRITTEN*.");

#ifdef HAVE_NETWORKING
   puts("  -H, --host            Host netplay as user 1.");
   puts("  -C, --connect=HOST    Connect to netplay server as user 2.");
   puts("      --port=PORT       Port used to netplay. Default is 55435.");
   puts("      --stateless       Use \"stateless\" mode for netplay");
   puts("                        (requires a very fast network).");
   puts("      --check-frames=NUMBER\n"
        "                        Check frames when using netplay.");
#if defined(HAVE_NETWORK_CMD)
   puts("      --command         Sends a command over UDP to an already "
         "running program process.");
   puts("      Available commands are listed if command is invalid.");
#endif

#endif
   puts("      --nick=NICK       Picks a username (for use with netplay). "
         "Not mandatory.");

   puts("  -r, --record=FILE     Path to record video file.\n        "
         "Using .mkv extension is recommended.");
   puts("      --recordconfig    Path to settings used during recording.");
   puts("      --size=WIDTHxHEIGHT\n"
        "                        Overrides output video size when recording.");
   puts("  -U, --ups=FILE        Specifies path for UPS patch that will be "
         "applied to content.");
   puts("      --bps=FILE        Specifies path for BPS patch that will be "
         "applied to content.");
   puts("      --ips=FILE        Specifies path for IPS patch that will be "
         "applied to content.");
   puts("      --no-patch        Disables all forms of content patching.");
   puts("  -D, --detach          Detach program from the running console. "
         "Not relevant for all platforms.");
   puts("      --max-frames=NUMBER\n"
        "                        Runs for the specified number of frames, "
        "then exits.");
   puts("      --max-frames-ss\n"
        "                        Takes a screenshot at the end of max-frames.");
   puts("      --max-frames-ss-path=FILE\n"
        "                        Path to save the screenshot to at the end of max-frames.\n");
}

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETWORKING
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif

#define BSV_MOVIE_ARG "P:R:M:"

/**
 * retroarch_parse_input_and_config:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Parses (commandline) arguments passed to program and loads the config file,
 * with command line options overriding the config file.
 *
 **/
static void retroarch_parse_input_and_config(int argc, char *argv[])
{
   unsigned i;
   static bool first_run = true;
   const char *optstring = NULL;
   bool explicit_menu    = false;
   global_t  *global     = &g_extern;

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro",           1, NULL, 'L' },
#endif
      { "menu",               0, NULL, RA_OPT_MENU },
      { "help",               0, NULL, 'h' },
      { "save",               1, NULL, 's' },
      { "fullscreen",         0, NULL, 'f' },
      { "record",             1, NULL, 'r' },
      { "recordconfig",       1, NULL, RA_OPT_RECORDCONFIG },
      { "size",               1, NULL, RA_OPT_SIZE },
      { "verbose",            0, NULL, 'v' },
      { "config",             1, NULL, 'c' },
      { "appendconfig",       1, NULL, RA_OPT_APPENDCONFIG },
      { "nodevice",           1, NULL, 'N' },
      { "dualanalog",         1, NULL, 'A' },
      { "device",             1, NULL, 'd' },
      { "savestate",          1, NULL, 'S' },
      { "bsvplay",            1, NULL, 'P' },
      { "bsvrecord",          1, NULL, 'R' },
      { "sram-mode",          1, NULL, 'M' },
#ifdef HAVE_NETWORKING
      { "host",               0, NULL, 'H' },
      { "connect",            1, NULL, 'C' },
      { "stateless",          0, NULL, RA_OPT_STATELESS },
      { "check-frames",       1, NULL, RA_OPT_CHECK_FRAMES },
      { "port",               1, NULL, RA_OPT_PORT },
#if defined(HAVE_NETWORK_CMD)
      { "command",            1, NULL, RA_OPT_COMMAND },
#endif
#endif
      { "nick",               1, NULL, RA_OPT_NICK },
      { "ups",                1, NULL, 'U' },
      { "bps",                1, NULL, RA_OPT_BPS },
      { "ips",                1, NULL, RA_OPT_IPS },
      { "no-patch",           0, NULL, RA_OPT_NO_PATCH },
      { "detach",             0, NULL, 'D' },
      { "features",           0, NULL, RA_OPT_FEATURES },
      { "subsystem",          1, NULL, RA_OPT_SUBSYSTEM },
      { "max-frames",         1, NULL, RA_OPT_MAX_FRAMES },
      { "max-frames-ss",      0, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT },
      { "max-frames-ss-path", 1, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT_PATH },
      { "eof-exit",           0, NULL, RA_OPT_EOF_EXIT },
      { "version",            0, NULL, RA_OPT_VERSION },
      { "log-file",           1, NULL, RA_OPT_LOG_FILE },
      { NULL, 0, NULL, 0 }
   };

   if (first_run)
   {
      /* Copy the args into a buffer so launch arguments can be reused */
      for (i = 0; i < (unsigned)argc; i++)
      {
         strlcat(launch_arguments, argv[i], sizeof(launch_arguments));
         strlcat(launch_arguments, " ", sizeof(launch_arguments));
      }
      string_trim_whitespace_left(launch_arguments);
      string_trim_whitespace_right(launch_arguments);

      first_run = false;
   }

   /* Handling the core type is finicky. Based on the arguments we pass in,
    * we handle it differently.
    * Some current cases which track desired behavior and how it is supposed to work:
    *
    * Dynamically linked RA:
    * ./retroarch                            -> CORE_TYPE_DUMMY
    * ./retroarch -v                         -> CORE_TYPE_DUMMY + verbose
    * ./retroarch --menu                     -> CORE_TYPE_DUMMY
    * ./retroarch --menu -v                  -> CORE_TYPE_DUMMY + verbose
    * ./retroarch -L contentless-core        -> CORE_TYPE_PLAIN
    * ./retroarch -L content-core            -> CORE_TYPE_PLAIN + FAIL (This currently crashes)
    * ./retroarch [-L content-core] ROM      -> CORE_TYPE_PLAIN
    * ./retroarch <-L or ROM> --menu         -> FAIL
    *
    * The heuristic here seems to be that if we use the -L CLI option or
    * optind < argc at the end we should set CORE_TYPE_PLAIN.
    * To handle --menu, we should ensure that CORE_TYPE_DUMMY is still set
    * otherwise, fail early, since the CLI options are non-sensical.
    * We could also simply ignore --menu in this case to be more friendly with
    * bogus arguments.
    */

   if (!has_set_core)
      retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);

   path_clear(RARCH_PATH_SUBSYSTEM);

   retroarch_override_setting_free_state();

   rarch_ctl(RARCH_CTL_USERNAME_UNSET, NULL);
   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   *global->name.ups                     = '\0';
   *global->name.bps                     = '\0';
   *global->name.ips                     = '\0';

   rarch_ctl(RARCH_CTL_UNSET_OVERRIDES_ACTIVE, NULL);

   /* Make sure we can call retroarch_parse_input several times ... */
   optind    = 0;
   optstring = "hs:fvS:A:c:U:DN:d:"
      BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;

#ifdef ORBIS
   argv = &(argv[2]);
   argc = argc - 2;
#endif

#ifndef HAVE_MENU
   if (argc == 1)
   {
      printf("%s\n", msg_hash_to_str(MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN));
      retroarch_print_help(argv[0]);
      exit(0);
   }
#endif

   /* First pass: Read the config file path and any directory overrides, so
    * they're in place when we load the config */
   if (argc)
   {
      for (;;)
      {
         int c = getopt_long(argc, argv, optstring, opts, NULL);

#if 0
         fprintf(stderr, "c is: %c (%d), optarg is: [%s]\n", c, c, string_is_empty(optarg) ? "" : optarg);
#endif

         if (c == -1)
            break;

         switch (c)
         {
            case 'h':
               retroarch_print_help(argv[0]);
               exit(0);

            case 'c':
               RARCH_LOG("Set config file to : %s\n", optarg);
               path_set(RARCH_PATH_CONFIG, optarg);
               break;

            case RA_OPT_APPENDCONFIG:
               path_set(RARCH_PATH_CONFIG_APPEND, optarg);
               break;

            case 's':
               strlcpy(global->name.savefile, optarg,
                     sizeof(global->name.savefile));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);
               break;

            case 'S':
               strlcpy(global->name.savestate, optarg,
                     sizeof(global->name.savestate));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
               break;

            /* Must handle '?' otherwise you get an infinite loop */
            case '?':
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
               break;
            /* All other arguments are handled in the second pass */
         }
      }
   }

   /* Flush out some states that could have been set
    * by core environment variables. */
   current_core.has_set_input_descriptors = false;

   /* Load the config file now that we know what it is */
   if (!rarch_block_config_read)
   {
      config_set_defaults();
      config_parse_file();
   }

   /* Second pass: All other arguments override the config file */
   optind = 1;

   if (argc)
   {
      for (;;)
      {
         int c = getopt_long(argc, argv, optstring, opts, NULL);

         if (c == -1)
            break;

         switch (c)
         {
            case 'd':
               {
                  unsigned new_port;
                  unsigned id              = 0;
                  struct string_list *list = string_split(optarg, ":");
                  int    port              = 0;

                  if (list && list->size == 2)
                  {
                     port = (int)strtol(list->elems[0].data, NULL, 0);
                     id   = (unsigned)strtoul(list->elems[1].data, NULL, 0);
                  }
                  string_list_free(list);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("%s\n", msg_hash_to_str(MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT));
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port -1;

                  input_config_set_device(new_port, id);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'A':
               {
                  unsigned new_port;
                  int port = (int)strtol(optarg, NULL, 0);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("Connect dualanalog to a valid port.\n");
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port - 1;

                  input_config_set_device(new_port, RETRO_DEVICE_ANALOG);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'f':
               rarch_force_fullscreen = true;
               break;

            case 'v':
               verbosity_enable();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
               break;

            case 'N':
               {
                  unsigned new_port;
                  int port = (int)strtol(optarg, NULL, 0);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("%s\n",
                           msg_hash_to_str(MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT));
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port - 1;
                  input_config_set_device(port - 1, RETRO_DEVICE_NONE);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'r':
               strlcpy(global->record.path, optarg,
                     sizeof(global->record.path));
               if (recording_enable)
                  recording_set_state(true);
               break;

   #ifdef HAVE_DYNAMIC
            case 'L':
               {
                  int path_stats = path_stat(optarg);

                  if ((path_stats & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
                  {
                     settings_t *settings = configuration_settings;

                     path_clear(RARCH_PATH_CORE);
                     strlcpy(settings->paths.directory_libretro, optarg,
                           sizeof(settings->paths.directory_libretro));

                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY, NULL);
                     RARCH_WARN("Using old --libretro behavior. "
                           "Setting libretro_directory to \"%s\" instead.\n",
                           optarg);
                  }
                  else if ((path_stats & RETRO_VFS_STAT_IS_VALID) != 0)
                  {
                     path_set(RARCH_PATH_CORE, optarg);
                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);

                     /* We requested explicit core, so use PLAIN core type. */
                     retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);
                  }
                  else
                  {
                     RARCH_WARN("--libretro argument \"%s\" is neither a file nor directory. Ignoring.\n",
                           optarg);
                  }
               }
               break;
   #endif
            case 'P':
            case 'R':
               strlcpy(bsv_movie_state.movie_start_path, optarg,
                     sizeof(bsv_movie_state.movie_start_path));

               if (c == 'P')
                  bsv_movie_state.movie_start_playback = true;
               else
                  bsv_movie_state.movie_start_playback = false;

               if (c == 'R')
                  bsv_movie_state.movie_start_recording = true;
               else
                  bsv_movie_state.movie_start_recording = false;
               break;

            case 'M':
               if (string_is_equal(optarg, "noload-nosave"))
               {
                  rarch_is_sram_load_disabled = true;
                  rarch_is_sram_save_disabled = true;
               }
               else if (string_is_equal(optarg, "noload-save"))
                  rarch_is_sram_load_disabled = true;
               else if (string_is_equal(optarg, "load-nosave"))
                  rarch_is_sram_save_disabled = true;
               else if (string_is_not_equal(optarg, "load-save"))
               {
                  RARCH_ERR("Invalid argument in --sram-mode.\n");
                  retroarch_print_help(argv[0]);
                  retroarch_fail(1, "retroarch_parse_input()");
               }
               break;

   #ifdef HAVE_NETWORKING
            case 'H':
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
               break;

            case 'C':
               {
                  settings_t *settings = configuration_settings;
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
                  strlcpy(settings->paths.netplay_server, optarg,
                        sizeof(settings->paths.netplay_server));
               }
               break;

            case RA_OPT_STATELESS:
               {
                  settings_t *settings = configuration_settings;

                  configuration_set_bool(settings,
                        settings->bools.netplay_stateless_mode, true);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE, NULL);
               }
               break;

            case RA_OPT_CHECK_FRAMES:
               {
                  settings_t *settings = configuration_settings;
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);

                  configuration_set_int(settings,
                        settings->ints.netplay_check_frames,
                        (int)strtoul(optarg, NULL, 0));
               }
               break;

            case RA_OPT_PORT:
               {
                  settings_t *settings = configuration_settings;
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT, NULL);
                  configuration_set_uint(settings,
                        settings->uints.netplay_port,
                        (int)strtoul(optarg, NULL, 0));
               }
               break;

   #if defined(HAVE_NETWORK_CMD)
            case RA_OPT_COMMAND:
   #ifdef HAVE_COMMAND
               if (command_network_send((const char*)optarg))
                  exit(0);
               else
                  retroarch_fail(1, "network_cmd_send()");
   #endif
               break;
   #endif

   #endif

            case RA_OPT_BPS:
               strlcpy(global->name.bps, optarg,
                     sizeof(global->name.bps));
               rarch_bps_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
               break;

            case 'U':
               strlcpy(global->name.ups, optarg,
                     sizeof(global->name.ups));
               rarch_ups_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
               break;

            case RA_OPT_IPS:
               strlcpy(global->name.ips, optarg,
                     sizeof(global->name.ips));
               rarch_ips_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_IPS_PREF, NULL);
               break;

            case RA_OPT_NO_PATCH:
               rarch_ctl(RARCH_CTL_SET_PATCH_BLOCKED, NULL);
               break;

            case 'D':
               frontend_driver_detach_console();
               break;

            case RA_OPT_MENU:
               explicit_menu = true;
               break;

            case RA_OPT_NICK:
               {
                  settings_t *settings = configuration_settings;

                  rarch_ctl(RARCH_CTL_USERNAME_SET, NULL);

                  strlcpy(settings->paths.username, optarg,
                        sizeof(settings->paths.username));
               }
               break;

            case RA_OPT_SIZE:
               if (sscanf(optarg, "%ux%u",
                        &recording_width,
                        &recording_height) != 2)
               {
                  RARCH_ERR("Wrong format for --size.\n");
                  retroarch_print_help(argv[0]);
                  retroarch_fail(1, "retroarch_parse_input()");
               }
               break;

            case RA_OPT_RECORDCONFIG:
               strlcpy(global->record.config, optarg,
                     sizeof(global->record.config));
               break;

            case RA_OPT_MAX_FRAMES:
               runloop_max_frames  = (unsigned)strtoul(optarg, NULL, 10);
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT:
               runloop_max_frames_screenshot = true;
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT_PATH:
               strlcpy(runloop_max_frames_screenshot_path, optarg, sizeof(runloop_max_frames_screenshot_path));
               break;

            case RA_OPT_SUBSYSTEM:
               path_set(RARCH_PATH_SUBSYSTEM, optarg);
               break;

            case RA_OPT_FEATURES:
               retroarch_print_features();
               exit(0);

            case RA_OPT_EOF_EXIT:
               bsv_movie_state.eof_exit = true;
               break;

            case RA_OPT_VERSION:
               retroarch_print_version();
               exit(0);

            case RA_OPT_LOG_FILE:
               {
                  settings_t *settings = configuration_settings;

                  /* Enable 'log to file' */
                  configuration_set_bool(settings,
                        settings->bools.log_to_file, true);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL);

                  /* Cache log file path override */
                  log_file_override_active = true;
                  strlcpy(log_file_override_path, optarg, sizeof(log_file_override_path));
               }
               break;

            case 'c':
            case 'h':
            case RA_OPT_APPENDCONFIG:
            case 's':
            case 'S':
               break; /* Handled in the first pass */

            case '?':
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");

            default:
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_ERROR_PARSING_ARGUMENTS));
               retroarch_fail(1, "retroarch_parse_input()");
         }
      }
   }

   if (verbosity_is_enabled())
      rarch_log_file_init();

#ifdef HAVE_GIT_VERSION
   RARCH_LOG("RetroArch %s (Git %s)\n",
         PACKAGE_VERSION, retroarch_git_version);
#endif

   if (explicit_menu)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         retroarch_fail(1, "retroarch_parse_input()");
      }
#ifdef HAVE_DYNAMIC
      else
      {
         /* Allow stray -L arguments to go through to workaround cases
          * where it's used as "config file".
          *
          * This seems to still be the case for Android, which
          * should be properly fixed. */
         retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);
      }
#endif
   }

   if (optind < argc)
   {
      bool subsystem_path_is_empty = path_is_empty(RARCH_PATH_SUBSYSTEM);

      /* We requested explicit ROM, so use PLAIN core type. */
      retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);

      if (subsystem_path_is_empty)
         path_set(RARCH_PATH_NAMES, (const char*)argv[optind]);
      else
         path_set_special(argv + optind, argc - optind);
   }

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         path_is_directory(global->name.savefile))
      dir_set(RARCH_DIR_SAVEFILE, global->name.savefile);

   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         path_is_directory(global->name.savestate))
      dir_set(RARCH_DIR_SAVESTATE, global->name.savestate);
}

bool retroarch_validate_game_options(char *s, size_t len, bool mkdir)
{
   char *config_directory                 = NULL;
   size_t str_size                        = PATH_MAX_LENGTH * sizeof(char);
   const char *core_name                  = runloop_system.info.library_name;
   const char *game_name                  = path_basename(path_get(RARCH_PATH_BASENAME));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   config_directory                       = (char*)malloc(str_size);
   config_directory[0]                    = '\0';

   fill_pathname_application_special(config_directory,
         str_size, APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* Concatenate strings into full paths for game_path */
   fill_pathname_join_special_ext(s,
         config_directory, core_name, game_name,
         file_path_str(FILE_PATH_OPT_EXTENSION),
         len);

   if (mkdir)
   {
      char *core_path  = (char*)malloc(str_size);
      core_path[0]     = '\0';

      fill_pathname_join(core_path,
            config_directory, core_name, str_size);

      if (!path_is_directory(core_path))
         path_mkdir(core_path);

      free(core_path);
   }

   free(config_directory);
   return true;
}

/* Validates CPU features for given processor architecture.
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support,
 * but this will do for now. */
static void retroarch_validate_cpu_features(void)
{
   uint64_t cpu = cpu_features_get();
   (void)cpu;

#ifdef __MMX__
   if (!(cpu & RETRO_SIMD_MMX))
      FAIL_CPU("MMX");
#endif
#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU("SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU("SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU("AVX");
#endif
}

/**
 * retroarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes the program.
 *
 * Returns: true on success, otherwise false if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[])
{
   bool init_failed  = false;
   global_t  *global = &g_extern;
#if defined(DEBUG) && defined(HAVE_DRMINGW)
   char log_file_name[128];
#endif

   video_driver_active = true;
   audio_driver_active = true;

   if (setjmp(error_sjlj_context) > 0)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FATAL_ERROR_RECEIVED_IN), error_string);
      return false;
   }

   rarch_error_on_init = true;

   /* Have to initialise non-file logging once at the start... */
   retro_main_log_file_init(NULL, false);

   retroarch_parse_input_and_config(argc, argv);

   if (verbosity_is_enabled())
   {
      char str[128];
      const char *cpu_model = NULL;
      str[0] = '\0';

      cpu_model = frontend_driver_get_cpu_model_name();

      RARCH_LOG_OUTPUT("=== Build =======================================\n");

      if (!string_is_empty(cpu_model))
         RARCH_LOG_OUTPUT("CPU Model Name: %s\n", cpu_model);

      retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));
      RARCH_LOG_OUTPUT("%s: %s\n", msg_hash_to_str(MSG_CAPABILITIES), str);
      RARCH_LOG_OUTPUT("Built: %s\n", __DATE__);
      RARCH_LOG_OUTPUT("Version: %s\n", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
      RARCH_LOG_OUTPUT("Git: %s\n", retroarch_git_version);
#endif
      RARCH_LOG_OUTPUT("=================================================\n");
   }

#if defined(DEBUG) && defined(HAVE_DRMINGW)
   RARCH_LOG("Initializing Dr.MingW Exception handler\n");
   fill_str_dated_filename(log_file_name, "crash",
         "log", sizeof(log_file_name));
   ExcHndlInit();
   ExcHndlSetLogFileNameA(log_file_name);
#endif

   retroarch_validate_cpu_features();

   rarch_ctl(RARCH_CTL_TASK_INIT, NULL);

   {
      const char    *fullpath  = path_get(RARCH_PATH_CONTENT);

      if (!string_is_empty(fullpath))
      {
         settings_t *settings              = configuration_settings;
         enum rarch_content_type cont_type = path_is_media_type(fullpath);
#ifdef HAVE_IMAGEVIEWER
         bool builtin_imageviewer          = settings ? settings->bools.multimedia_builtin_imageviewer_enable : false;
#endif
         bool builtin_mediaplayer          = settings ? settings->bools.multimedia_builtin_mediaplayer_enable : false;

         switch (cont_type)
         {
            case RARCH_CONTENT_MOVIE:
            case RARCH_CONTENT_MUSIC:
               if (builtin_mediaplayer)
               {
                  /* TODO/FIXME - it needs to become possible to 
                   * switch between FFmpeg and MPV at runtime */
#if defined(HAVE_MPV)
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  retroarch_set_current_core_type(CORE_TYPE_MPV, false);
#elif defined(HAVE_FFMPEG)
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  retroarch_set_current_core_type(CORE_TYPE_FFMPEG, false);
#endif
               }
               break;
#ifdef HAVE_IMAGEVIEWER
            case RARCH_CONTENT_IMAGE:
               if (builtin_imageviewer)
               {
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  retroarch_set_current_core_type(CORE_TYPE_IMAGEVIEWER, false);
               }
               break;
#endif
#ifdef HAVE_EASTEREGG
            case RARCH_CONTENT_GONG:
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
               retroarch_set_current_core_type(CORE_TYPE_GONG, false);
               break;
#endif
            default:
               break;
         }
      }
   }

   /* Pre-initialize all drivers 
    * Attempts to find a default driver for
    * all driver types.
    */
   audio_driver_find_driver();
   video_driver_find_driver();
   input_driver_find_driver();
   camera_driver_find_driver();
   wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);
   find_location_driver();
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_FIND_DRIVER, NULL);
#endif

   /* Attempt to initialize core */
   if (has_set_core)
   {
      has_set_core = false;
      if (!command_event(CMD_EVENT_CORE_INIT, &explicit_current_core_type))
         init_failed = true;
   }
   else if (!command_event(CMD_EVENT_CORE_INIT, &current_core_type))
      init_failed = true;

   /* Handle core initialization failure */
   if (init_failed)
   {
      /* Check if menu was active prior to core initialization */
      if (!content_launched_from_cli()
#ifdef HAVE_MENU
          || menu_driver_is_alive()
#endif
         )
      {
         /* Attempt initializing dummy core */
         current_core_type = CORE_TYPE_DUMMY;
         if (!command_event(CMD_EVENT_CORE_INIT, &current_core_type))
            goto error;
      }
      else
      {
         /* Fall back to regular error handling */
         goto error;
      }
   }

   command_event(CMD_EVENT_CHEATS_INIT, NULL);
   drivers_init(DRIVERS_CMD_ALL);
   command_event(CMD_EVENT_COMMAND_INIT, NULL);
   command_event(CMD_EVENT_REMOTE_INIT, NULL);
   command_event(CMD_EVENT_MAPPER_INIT, NULL);
   command_event(CMD_EVENT_REWIND_INIT, NULL);
   command_event(CMD_EVENT_CONTROLLERS_INIT, NULL);
   if (!string_is_empty(global->record.path))
      command_event(CMD_EVENT_RECORD_INIT, NULL);

   path_init_savefile();

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   rarch_error_on_init     = false;
   rarch_is_inited         = true;

#ifdef HAVE_DISCORD
   if (command_event(CMD_EVENT_DISCORD_INIT, NULL))
      discord_is_inited = true;

   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_MENU;

      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
#endif

#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
   if (configuration_settings->bools.audio_enable_menu)
      audio_driver_load_menu_sounds();
#endif

   return true;

error:
   command_event(CMD_EVENT_CORE_DEINIT, NULL);
   rarch_is_inited         = false;

   return false;
}

bool retroarch_is_on_main_thread(void)
{
#ifdef HAVE_THREAD_STORAGE
   if (sthread_tls_get(&rarch_tls) != MAGIC_POINTER)
      return false;
#endif
   return true;
}

void rarch_menu_running(void)
{
#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
   settings_t *settings = configuration_settings;
#endif
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SET_TOGGLE, NULL);

   /* Prevent stray input */
   input_driver_flushing_input = true;

#ifdef HAVE_AUDIOMIXER
   if (settings->bools.audio_enable_menu 
         && settings->bools.audio_enable_menu_bgm)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif
#endif
#ifdef HAVE_OVERLAY
   if (settings->bools.input_overlay_hide_in_menu)
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
#endif
}

void rarch_menu_running_finished(bool quit)
{
#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
   settings_t *settings = configuration_settings;
#endif
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_UNSET_TOGGLE, NULL);

   /* Prevent stray input */
   input_driver_flushing_input = true;

#ifdef HAVE_AUDIOMIXER
   if (!quit)
      /* Stop menu background music before we exit the menu */
      if (settings && settings->bools.audio_enable_menu && settings->bools.audio_enable_menu_bgm)
         audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif

#endif
   video_driver_set_texture_enable(false, false);
#ifdef HAVE_OVERLAY
   if (!quit)
      if (settings && settings->bools.input_overlay_hide_in_menu)
         retroarch_overlay_init();
#endif
}

/**
 * rarch_game_specific_options:
 *
 * Returns: true (1) if a game specific core
 * options path has been found,
 * otherwise false (0).
 **/
static bool rarch_game_specific_options(char **output)
{
   size_t game_path_size = 8192 * sizeof(char);
   char *game_path       = (char*)malloc(game_path_size);

   game_path[0] ='\0';

   if (!retroarch_validate_game_options(game_path,
            game_path_size, false))
      goto error;
   if (!config_file_exists(game_path))
      goto error;

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_path);
   *output = strdup(game_path);
   free(game_path);
   return true;

error:
   free(game_path);
   return false;
}

static void runloop_task_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   bool ready = menu_widgets_ready();
   if (ready && task->title && !task->mute)
   {
      runloop_msg_queue_lock();
      ui_companion_driver_msg_queue_push(msg,
            prio, task ? duration : duration * 60 / 1000, flush);
      menu_widgets_msg_queue_push(task, msg, duration, NULL, (enum message_queue_icon)MESSAGE_QUEUE_CATEGORY_INFO, (enum message_queue_category)MESSAGE_QUEUE_ICON_DEFAULT, prio, flush);
      runloop_msg_queue_unlock();
   }
   else
#endif
      runloop_msg_queue_push(msg, prio, duration, flush, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

static void rarch_init_core_options(
      const struct retro_core_option_definition *option_defs)
{
   settings_t *settings              = configuration_settings;
   char *game_options_path           = NULL;

   if (settings->bools.game_specific_options &&
         rarch_game_specific_options(&game_options_path))
   {
      runloop_game_options_active = true;
      runloop_core_options        =
         core_option_manager_new(game_options_path, option_defs);
      free(game_options_path);
   }
   else
   {
      char buf[PATH_MAX_LENGTH];
      const char *options_path       = settings ? settings->paths.path_core_options : NULL;

      buf[0] = '\0';

      if (string_is_empty(options_path) && !path_is_empty(RARCH_PATH_CONFIG))
      {
         fill_pathname_resolve_relative(buf, path_get(RARCH_PATH_CONFIG),
               file_path_str(FILE_PATH_CORE_OPTIONS_CONFIG), sizeof(buf));
         options_path = buf;
      }

      runloop_game_options_active = false;

      if (!string_is_empty(options_path))
         runloop_core_options =
            core_option_manager_new(options_path, option_defs);
   }
}

bool rarch_ctl(enum rarch_ctl_state state, void *data)
{
   static bool has_set_username        = false;
   static bool rarch_patch_blocked     = false;
   static bool runloop_missing_bios    = false;
   /* TODO/FIXME - not used right now? */

   switch(state)
   {
      case RARCH_CTL_BSV_MOVIE_IS_INITED:
         return (bsv_movie_state_handle != NULL);
      case RARCH_CTL_IS_PATCH_BLOCKED:
         return rarch_patch_blocked;
      case RARCH_CTL_SET_PATCH_BLOCKED:
         rarch_patch_blocked = true;
         break;
      case RARCH_CTL_UNSET_PATCH_BLOCKED:
         rarch_patch_blocked = false;
         break;
      case RARCH_CTL_IS_BPS_PREF:
         return rarch_bps_pref;
      case RARCH_CTL_UNSET_BPS_PREF:
         rarch_bps_pref = false;
         break;
      case RARCH_CTL_IS_UPS_PREF:
         return rarch_ups_pref;
      case RARCH_CTL_UNSET_UPS_PREF:
         rarch_ups_pref = false;
         break;
      case RARCH_CTL_IS_IPS_PREF:
         return rarch_ips_pref;
      case RARCH_CTL_UNSET_IPS_PREF:
         rarch_ips_pref = false;
         break;
      case RARCH_CTL_IS_DUMMY_CORE:
         return (current_core_type == CORE_TYPE_DUMMY);
      case RARCH_CTL_USERNAME_SET:
         has_set_username = true;
         break;
      case RARCH_CTL_USERNAME_UNSET:
         has_set_username = false;
         break;
      case RARCH_CTL_HAS_SET_USERNAME:
         return has_set_username;
      case RARCH_CTL_IS_INITED:
         return rarch_is_inited;
      case RARCH_CTL_DESTROY:
         has_set_username        = false;
         rarch_is_inited         = false;
         rarch_error_on_init     = false;
         rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);

         retroarch_msg_queue_deinit();
         driver_uninit(DRIVERS_CMD_ALL);
         command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

         rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
         global_free();
         rarch_ctl(RARCH_CTL_DATA_DEINIT, NULL);
         free(configuration_settings);
         configuration_settings = NULL;
         break;
      case RARCH_CTL_PREINIT:
         libretro_free_system_info(&runloop_system.info);
         command_event(CMD_EVENT_HISTORY_DEINIT, NULL);

         configuration_settings = (settings_t*)calloc(1, sizeof(settings_t));

         driver_ctl(RARCH_DRIVER_CTL_DEINIT,  NULL);
         rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
         global_free();
         break;
      case RARCH_CTL_MAIN_DEINIT:
         if (!rarch_is_inited)
            return false;
         command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
         command_event(CMD_EVENT_COMMAND_DEINIT, NULL);
         command_event(CMD_EVENT_REMOTE_DEINIT, NULL);
         command_event(CMD_EVENT_MAPPER_DEINIT, NULL);

         command_event(CMD_EVENT_AUTOSAVE_DEINIT, NULL);

         command_event(CMD_EVENT_RECORD_DEINIT, NULL);

         event_save_files();

         command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         command_event(CMD_EVENT_CHEATS_DEINIT, NULL);
         command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

         command_event(CMD_EVENT_CORE_DEINIT, NULL);

         content_deinit();

         path_deinit_subsystem();
         path_deinit_savefile();

         rarch_is_inited         = false;

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_delete(&rarch_tls);
#endif
         break;
      case RARCH_CTL_INIT:
         if (rarch_is_inited)
            driver_uninit(DRIVERS_CMD_ALL);

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_create(&rarch_tls);
         sthread_tls_set(&rarch_tls, MAGIC_POINTER);
#endif
         video_driver_active = true;
         audio_driver_active = true;
         {
            uint8_t i;
            for (i = 0; i < MAX_USERS; i++)
               input_config_set_device(i, RETRO_DEVICE_JOYPAD);
         }
         retroarch_msg_queue_init();
         break;
      case RARCH_CTL_IS_SRAM_LOAD_DISABLED:
         return rarch_is_sram_load_disabled;
      case RARCH_CTL_IS_SRAM_SAVE_DISABLED:
         return rarch_is_sram_save_disabled;
      case RARCH_CTL_IS_SRAM_USED:
         return rarch_use_sram;
      case RARCH_CTL_SET_SRAM_ENABLE:
         {
            bool contentless = false;
            bool is_inited   = false;
            content_get_status(&contentless, &is_inited);
            rarch_use_sram = (current_core_type == CORE_TYPE_PLAIN)
               && !contentless;
         }
         break;
      case RARCH_CTL_SET_SRAM_ENABLE_FORCE:
         rarch_use_sram = true;
         break;
      case RARCH_CTL_UNSET_SRAM_ENABLE:
         rarch_use_sram = false;
         break;
      case RARCH_CTL_SET_BLOCK_CONFIG_READ:
         rarch_block_config_read = true;
         break;
      case RARCH_CTL_UNSET_BLOCK_CONFIG_READ:
         rarch_block_config_read = false;
         break;
      case RARCH_CTL_IS_BLOCK_CONFIG_READ:
         return rarch_block_config_read;
      case RARCH_CTL_SYSTEM_INFO_INIT:
         current_core.retro_get_system_info(&runloop_system.info);

         if (!runloop_system.info.library_name)
            runloop_system.info.library_name = msg_hash_to_str(MSG_UNKNOWN);
         if (!runloop_system.info.library_version)
            runloop_system.info.library_version = "v0";

         video_driver_set_title_buf();

         strlcpy(runloop_system.valid_extensions,
               runloop_system.info.valid_extensions ?
               runloop_system.info.valid_extensions : DEFAULT_EXT,
               sizeof(runloop_system.valid_extensions));
         break;
      case RARCH_CTL_GET_CORE_OPTION_SIZE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            if (runloop_core_options)
               *idx = (unsigned)runloop_core_options->size;
            else
               *idx = 0;
         }
         break;
      case RARCH_CTL_HAS_CORE_OPTIONS:
         return (runloop_core_options != NULL);
      case RARCH_CTL_CORE_OPTIONS_LIST_GET:
         {
            core_option_manager_t **coreopts = (core_option_manager_t**)data;
            if (!coreopts)
               return false;
            *coreopts = runloop_core_options;
         }
         break;
      case RARCH_CTL_SYSTEM_INFO_FREE:
         if (runloop_system.subsystem.data)
            free(runloop_system.subsystem.data);
         runloop_system.subsystem.data = NULL;
         runloop_system.subsystem.size = 0;

         if (runloop_system.ports.data)
            free(runloop_system.ports.data);
         runloop_system.ports.data = NULL;
         runloop_system.ports.size = 0;

         if (runloop_system.mmaps.descriptors)
            free((void *)runloop_system.mmaps.descriptors);
         runloop_system.mmaps.descriptors          = NULL;
         runloop_system.mmaps.num_descriptors      = 0;

         runloop_key_event                         = NULL;
         runloop_frontend_key_event                = NULL;

         audio_driver_unset_callback();

         runloop_system.info.library_name          = NULL;
         runloop_system.info.library_version       = NULL;
         runloop_system.info.valid_extensions      = NULL;
         runloop_system.info.need_fullpath         = false;
         runloop_system.info.block_extract         = false;

         memset(&runloop_system, 0, sizeof(rarch_system_info_t));
         break;
      case RARCH_CTL_SET_FRAME_TIME_LAST:
         runloop_frame_time_last        = 0;
         break;
      case RARCH_CTL_SET_OVERRIDES_ACTIVE:
         runloop_overrides_active = true;
         break;
      case RARCH_CTL_UNSET_OVERRIDES_ACTIVE:
         runloop_overrides_active = false;
         break;
      case RARCH_CTL_IS_OVERRIDES_ACTIVE:
         return runloop_overrides_active;
      case RARCH_CTL_SET_REMAPS_CORE_ACTIVE:
         runloop_remaps_core_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_CORE_ACTIVE:
         runloop_remaps_core_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_CORE_ACTIVE:
         return runloop_remaps_core_active;
      case RARCH_CTL_SET_REMAPS_GAME_ACTIVE:
         runloop_remaps_game_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_GAME_ACTIVE:
         runloop_remaps_game_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_GAME_ACTIVE:
         return runloop_remaps_game_active;
      case RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE:
         runloop_remaps_content_dir_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_CONTENT_DIR_ACTIVE:
         runloop_remaps_content_dir_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE:
         return runloop_remaps_content_dir_active;
      case RARCH_CTL_SET_MISSING_BIOS:
         runloop_missing_bios = true;
         break;
      case RARCH_CTL_UNSET_MISSING_BIOS:
         runloop_missing_bios = false;
         break;
      case RARCH_CTL_IS_MISSING_BIOS:
         return runloop_missing_bios;
      case RARCH_CTL_IS_GAME_OPTIONS_ACTIVE:
         return runloop_game_options_active;
      case RARCH_CTL_SET_FRAME_LIMIT:
         {
            settings_t                 *settings = configuration_settings;
            struct retro_system_av_info *av_info = &video_driver_av_info;
            float fastforward_ratio_orig         = settings->floats.fastforward_ratio;
            float fastforward_ratio              = (fastforward_ratio_orig == 0.0f) ? 1.0f : fastforward_ratio_orig;

            frame_limit_last_time                = cpu_features_get_time_usec();
            frame_limit_minimum_time             = (retro_time_t)roundf(1000000.0f
                  / (av_info->timing.fps * fastforward_ratio));
         }
         break;
      case RARCH_CTL_CONTENT_RUNTIME_LOG_INIT:
      {
         const char *content_path = path_get(RARCH_PATH_CONTENT);
         const char *core_path = path_get(RARCH_PATH_CORE);

         libretro_core_runtime_last = cpu_features_get_time_usec();
         libretro_core_runtime_usec = 0;

         /* Have to cache content and core path here, otherwise
          * logging fails if new content is loaded without
          * closing existing content
          * i.e. RARCH_PATH_CONTENT and RARCH_PATH_CORE get
          * updated when the new content is loaded, which
          * happens *before* RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT
          * -> using RARCH_PATH_CONTENT and RARCH_PATH_CORE
          *    directly in RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT
          *    can therefore lead to the runtime of the currently
          *    loaded content getting written to the *new*
          *    content's log file... */
         memset(runtime_content_path, 0, sizeof(runtime_content_path));
         memset(runtime_core_path, 0, sizeof(runtime_core_path));

         if (!string_is_empty(content_path))
            strlcpy(runtime_content_path, content_path, sizeof(runtime_content_path));

         if (!string_is_empty(core_path))
            strlcpy(runtime_core_path, core_path, sizeof(runtime_core_path));

         break;
      }
      case RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT:
      {
         unsigned hours            = 0;
         unsigned minutes          = 0;
         unsigned seconds          = 0;
         char log[PATH_MAX_LENGTH] = {0};
         int n                     = 0;

         runtime_log_convert_usec2hms(
               libretro_core_runtime_usec, &hours, &minutes, &seconds);

         n = snprintf(log, sizeof(log),
               "Content ran for a total of: %02u hours, %02u minutes, %02u seconds.",
               hours, minutes, seconds);
         if ((n < 0) || (n >= PATH_MAX_LENGTH))
            n = 0; /* Just silence any potential gcc warnings... */
         RARCH_LOG("%s\n",log);

         /* Only write to file if content has run for a non-zero length of time */
         if (libretro_core_runtime_usec > 0)
         {
            settings_t *settings = configuration_settings;

            /* Per core logging */
            if (settings->bools.content_runtime_log)
               update_runtime_log(true);

            /* Aggregate logging */
            if (settings->bools.content_runtime_log_aggregate)
               update_runtime_log(false);
         }

         /* Reset runtime + content/core paths, to prevent any
          * possibility of duplicate logging */
         libretro_core_runtime_usec = 0;
         memset(runtime_content_path, 0, sizeof(runtime_content_path));
         memset(runtime_core_path, 0, sizeof(runtime_core_path));

         break;
      }
      case RARCH_CTL_GET_PERFCNT:
         {
            bool **perfcnt = (bool**)data;
            if (!perfcnt)
               return false;
            *perfcnt = &runloop_perfcnt_enable;
         }
         break;
      case RARCH_CTL_SET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = true;
         break;
      case RARCH_CTL_UNSET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = false;
         break;
      case RARCH_CTL_IS_PERFCNT_ENABLE:
         return runloop_perfcnt_enable;
      case RARCH_CTL_GET_WINDOWED_SCALE:
         {
            unsigned **scale = (unsigned**)data;
            if (!scale)
               return false;
            *scale       = (unsigned*)&runloop_pending_windowed_scale;
         }
         break;
      case RARCH_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            runloop_pending_windowed_scale = *idx;
         }
         break;
      case RARCH_CTL_FRAME_TIME_FREE:
         memset(&runloop_frame_time, 0,
               sizeof(struct retro_frame_time_callback));
         runloop_frame_time_last           = 0;
         runloop_max_frames                = 0;
         break;
      case RARCH_CTL_STATE_FREE:
         runloop_perfcnt_enable            = false;
         runloop_idle                      = false;
         runloop_paused                    = false;
         runloop_slowmotion                = false;
         runloop_overrides_active          = false;
         runloop_autosave                  = false;
         rarch_ctl(RARCH_CTL_FRAME_TIME_FREE, NULL);
         break;
      case RARCH_CTL_IS_IDLE:
         return runloop_idle;
      case RARCH_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_idle = *ptr;
         }
         break;
      case RARCH_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_paused = *ptr;
         }
         break;
      case RARCH_CTL_IS_PAUSED:
         return runloop_paused;
      case RARCH_CTL_TASK_INIT:
         {
#ifdef HAVE_THREADS
            settings_t *settings       = configuration_settings;
            bool threaded_enable       = settings->bools.threaded_data_runloop_enable;
#else
            bool threaded_enable = false;
#endif
            task_queue_deinit();
            task_queue_init(threaded_enable, runloop_task_msg_queue_push);
         }
         break;
      case RARCH_CTL_SET_CORE_SHUTDOWN:
         runloop_core_shutdown_initiated = true;
         break;
      case RARCH_CTL_SET_SHUTDOWN:
         runloop_shutdown_initiated = true;
         break;
      case RARCH_CTL_UNSET_SHUTDOWN:
         runloop_shutdown_initiated = false;
         break;
      case RARCH_CTL_IS_SHUTDOWN:
         return runloop_shutdown_initiated;
      case RARCH_CTL_DATA_DEINIT:
         task_queue_deinit();
         break;
      case RARCH_CTL_CORE_OPTION_PREV:
         /*
          * Get previous value for core option specified by @idx.
          * Options wrap around.
          */
         {
            struct core_option *option        = NULL;
            unsigned              *idx        = (unsigned*)data;
            if (!idx || !runloop_core_options)
               return false;
            option                            = (struct core_option*)
               &runloop_core_options->opts[*idx];

            if (option)
            {
               option->index                  = 
                  (option->index + option->vals->size - 1) %
                  option->vals->size;
               runloop_core_options->updated  = true;
            }
         }
         break;
      case RARCH_CTL_CORE_OPTION_NEXT:
         /*
          * Get next value for core option specified by @idx.
          * Options wrap around.
          */
         {
            struct core_option *option       = NULL;
            unsigned *idx                    = (unsigned*)data;

            if (!idx || !runloop_core_options)
               return false;

            option                           = 
               (struct core_option*)&runloop_core_options->opts[*idx];

            if (option)
            {
               option->index                 = 
                  (option->index + 1) % option->vals->size;
               runloop_core_options->updated = true;
            }
         }
         break;
      case RARCH_CTL_CORE_VARIABLES_INIT:
         {
            settings_t *settings              = configuration_settings;
            char *game_options_path           = NULL;
            const struct retro_variable *vars =
               (const struct retro_variable*)data;

            if (settings->bools.game_specific_options && 
                  rarch_game_specific_options(&game_options_path))
            {
               runloop_game_options_active = true;
               runloop_core_options        =
                  core_option_manager_new_vars(game_options_path, vars);
               free(game_options_path);
            }
            else
            {
               char buf[PATH_MAX_LENGTH];
               const char *options_path       = settings ? settings->paths.path_core_options : NULL;

               buf[0] = '\0';

               if (string_is_empty(options_path) && !path_is_empty(RARCH_PATH_CONFIG))
               {
                  fill_pathname_resolve_relative(buf, path_get(RARCH_PATH_CONFIG),
                        file_path_str(FILE_PATH_CORE_OPTIONS_CONFIG), sizeof(buf));
                  options_path = buf;
               }

               runloop_game_options_active = false;

               if (!string_is_empty(options_path))
                  runloop_core_options =
                     core_option_manager_new_vars(options_path, vars);
            }
         }
         break;
      case RARCH_CTL_CORE_OPTIONS_INIT:
         {
            const struct retro_core_option_definition *option_defs =
               (const struct retro_core_option_definition*)data;

            rarch_init_core_options(option_defs);
         }
         break;

      case RARCH_CTL_CORE_OPTIONS_INTL_INIT:
         {
            const struct retro_core_options_intl *core_options_intl =
               (const struct retro_core_options_intl*)data;

            /* Parse core_options_intl to create option definitions array */
            struct retro_core_option_definition *option_defs =
               core_option_manager_get_definitions(core_options_intl);

            if (option_defs)
            {
               /* Initialise core options */
               rarch_init_core_options(option_defs);

               /* Clean up */
               free(option_defs);
            }
         }
         break;

      case RARCH_CTL_CORE_OPTIONS_DEINIT:
         {
            if (!runloop_core_options)
               return false;

            /* check if game options file was just created and flush
               to that file instead */
            if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            {
               core_option_manager_flush_game_specific(runloop_core_options,
                     path_get(RARCH_PATH_CORE_OPTIONS));
               path_clear(RARCH_PATH_CORE_OPTIONS);
            }
            else
               core_option_manager_flush(runloop_core_options);

            if (runloop_game_options_active)
               runloop_game_options_active = false;

            if (runloop_core_options)
               core_option_manager_free(runloop_core_options);
            runloop_core_options          = NULL;
         }
         break;

      case RARCH_CTL_CORE_OPTIONS_DISPLAY:
         {
            const struct retro_core_option_display *core_options_display =
               (const struct retro_core_option_display*)data;

            if (!runloop_core_options || !core_options_display)
               return false;

            core_option_manager_set_display(
                  runloop_core_options,
                  core_options_display->key,
                  core_options_display->visible);
         }
         break;

      case RARCH_CTL_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_key_event;
         }
         break;
      case RARCH_CTL_FRONTEND_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_frontend_key_event;
         }
         break;
      case RARCH_CTL_NONE:
      default:
         return false;
   }

   return true;
}

bool retroarch_is_forced_fullscreen(void)
{
   return rarch_force_fullscreen;
}

void retroarch_unset_forced_fullscreen(void)
{
   rarch_force_fullscreen = false;
}

bool retroarch_is_switching_display_mode(void)
{
   return rarch_is_switching_display_mode;
}

void retroarch_set_switching_display_mode(void)
{
   rarch_is_switching_display_mode = true;
}

void retroarch_unset_switching_display_mode(void)
{
   rarch_is_switching_display_mode = false;
}

/* set a runtime shader preset without overwriting the settings value */
void retroarch_set_shader_preset(const char* preset)
{
   if (!string_is_empty(preset))
      strlcpy(runtime_shader_preset, preset, sizeof(runtime_shader_preset));
   else
      runtime_shader_preset[0] = '\0';
}

/* unset a runtime shader preset */
void retroarch_unset_shader_preset(void)
{
   runtime_shader_preset[0] = '\0';
}

static bool retroarch_load_shader_preset_internal(
      const char *shader_directory,
      const char *core_name,
      const char *special_name)
{
   unsigned i;
   char *shader_path = (char*)malloc(PATH_MAX_LENGTH);

   static enum rarch_shader_type types[] =
   {
      /* Shader preset priority, highest to lowest
       * only important for video drivers with multiple shader backends */
      RARCH_SHADER_GLSL, RARCH_SHADER_SLANG, RARCH_SHADER_CG, RARCH_SHADER_HLSL
   };

   for (i = 0; i < ARRAY_SIZE(types); i++)
   {
      if (!video_shader_is_supported(types[i]))
         continue;

      /* Concatenate strings into full paths */
      fill_pathname_join_special_ext(shader_path,
            shader_directory, core_name,
            special_name,
            video_shader_get_preset_extension(types[i]),
            PATH_MAX_LENGTH);

      if (!config_file_exists(shader_path))
         continue;

      /* Shader preset exists, load it. */
      RARCH_LOG("[Shaders]: Specific shader preset found at %s.\n",
            shader_path);
      retroarch_set_shader_preset(shader_path);
      free(shader_path);
      return true;
   }

   free(shader_path);
   return false;
}

/**
 * retroarch_load_shader_preset:
 *
 * Tries to load a supported core-, game- or folder-specific shader preset
 * from its respective location:
 *
 * core-specific:   $SHADER_DIR/presets/$CORE_NAME/$CORE_NAME.$PRESET_EXT
 * folder-specific: $SHADER_DIR/presets/$CORE_NAME/$FOLDER_NAME.$PRESET_EXT
 * game-specific:   $SHADER_DIR/presets/$CORE_NAME/$GAME_NAME.$PRESET_EXT
 *
 * Note: Uses video_shader_is_supported() which only works after
 *       context driver initialization.
 *
 * Returns: false if there was an error or no action was performed.
 */
static bool retroarch_load_shader_preset(void)
{
   settings_t *settings               = configuration_settings;
   const rarch_system_info_t *system  = &runloop_system;
   const char *video_shader_directory = settings->paths.directory_video_shader;
   const char *core_name              = system->info.library_name;
   const char *rarch_path_basename    = path_get(RARCH_PATH_BASENAME);

   const char *game_name              = path_basename(rarch_path_basename);
   char *shader_directory             = NULL;

   if (!settings->bools.auto_shaders_enable)
      return false;

   if (string_is_empty(video_shader_directory) ||
         string_is_empty(core_name) ||
         string_is_empty(game_name))
      return false;

   shader_directory = (char*)malloc(PATH_MAX_LENGTH);

   fill_pathname_join(shader_directory,
         video_shader_directory,
         "presets", PATH_MAX_LENGTH);

   RARCH_LOG("[Shaders]: preset directory: %s\n", shader_directory);

   if (retroarch_load_shader_preset_internal(shader_directory, core_name,
            game_name))
   {
      RARCH_LOG("[Shaders]: game-specific shader preset found.\n");
      goto success;
   }

   {
      char content_dir_name[PATH_MAX_LENGTH];
      if (!string_is_empty(rarch_path_basename))
         fill_pathname_parent_dir_name(content_dir_name,
               rarch_path_basename, sizeof(content_dir_name));

      if (retroarch_load_shader_preset_internal(shader_directory, core_name,
               content_dir_name))
      {
         RARCH_LOG("[Shaders]: folder-specific shader preset found.\n");
         goto success;
      }
   }

   if (retroarch_load_shader_preset_internal(shader_directory, core_name,
            core_name))
   {
      RARCH_LOG("[Shaders]: core-specific shader preset found.\n");
      goto success;
   }

   free(shader_directory);
   return false;

success:
   free(shader_directory);
   return true;
}

void retroarch_shader_presets_set_need_reload(void)
{
   shader_presets_need_reload = true;
}

/* get the name of the current shader preset */
char* retroarch_get_shader_preset(void)
{
   settings_t *settings = configuration_settings;
   if (!settings->bools.video_shader_enable)
      return NULL;

   if (shader_presets_need_reload)
   {
      retroarch_load_shader_preset();
      shader_presets_need_reload = false;
   }

   if (!string_is_empty(runtime_shader_preset))
      return runtime_shader_preset;

   if (!string_is_empty(settings->paths.path_shader))
      return settings->paths.path_shader;

   return NULL;
}

bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               return BIT256_GET(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         return has_set_verbosity;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         return has_set_libretro;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         return has_set_libretro_directory;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         return has_set_save_path;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         return has_set_state_path;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         return has_set_netplay_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         return has_set_netplay_ip_address;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         return has_set_netplay_ip_port;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         return has_set_netplay_stateless_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         return has_set_netplay_check_frames;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         return has_set_ups_pref;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         return has_set_bps_pref;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         return has_set_ips_pref;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         return has_set_log_to_file;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }

   return false;
}

void retroarch_override_setting_set(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT256_SET(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         has_set_verbosity = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         has_set_libretro = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         has_set_libretro_directory = true;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         has_set_save_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         has_set_state_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         has_set_netplay_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         has_set_netplay_ip_address = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         has_set_netplay_ip_port = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         has_set_netplay_stateless_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         has_set_netplay_check_frames = true;
         break;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         has_set_ups_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         has_set_bps_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         has_set_ips_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         has_set_log_to_file = true;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT256_CLEAR(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         has_set_verbosity = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         has_set_libretro = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         has_set_libretro_directory = false;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         has_set_save_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         has_set_state_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         has_set_netplay_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         has_set_netplay_ip_address = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         has_set_netplay_ip_port = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         has_set_netplay_stateless_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         has_set_netplay_check_frames = false;
         break;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         has_set_ups_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         has_set_bps_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         has_set_ips_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         has_set_log_to_file = false;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len)
{
   switch (type)
   {
      case RARCH_CAPABILITIES_CPU:
         {
            uint64_t cpu = cpu_features_get();

            if (cpu & RETRO_SIMD_MMX)
               strlcat(s, "MMX ", len);
            if (cpu & RETRO_SIMD_MMXEXT)
               strlcat(s, "MMXEXT ", len);
            if (cpu & RETRO_SIMD_SSE)
               strlcat(s, "SSE1 ", len);
            if (cpu & RETRO_SIMD_SSE2)
               strlcat(s, "SSE2 ", len);
            if (cpu & RETRO_SIMD_SSE3)
               strlcat(s, "SSE3 ", len);
            if (cpu & RETRO_SIMD_SSSE3)
               strlcat(s, "SSSE3 ", len);
            if (cpu & RETRO_SIMD_SSE4)
               strlcat(s, "SSE4 ", len);
            if (cpu & RETRO_SIMD_SSE42)
               strlcat(s, "SSE4.2 ", len);
            if (cpu & RETRO_SIMD_AVX)
               strlcat(s, "AVX ", len);
            if (cpu & RETRO_SIMD_AVX2)
               strlcat(s, "AVX2 ", len);
            if (cpu & RETRO_SIMD_VFPU)
               strlcat(s, "VFPU ", len);
            if (cpu & RETRO_SIMD_NEON)
               strlcat(s, "NEON ", len);
            if (cpu & RETRO_SIMD_VFPV3)
               strlcat(s, "VFPv3 ", len);
            if (cpu & RETRO_SIMD_VFPV4)
               strlcat(s, "VFPv4 ", len);
            if (cpu & RETRO_SIMD_PS)
               strlcat(s, "PS ", len);
            if (cpu & RETRO_SIMD_AES)
               strlcat(s, "AES ", len);
            if (cpu & RETRO_SIMD_VMX)
               strlcat(s, "VMX ", len);
            if (cpu & RETRO_SIMD_VMX128)
               strlcat(s, "VMX128 ", len);
            if (cpu & RETRO_SIMD_ASIMD)
               strlcat(s, "ASIMD ", len);
         }
         break;
      case RARCH_CAPABILITIES_COMPILER:
#if defined(_MSC_VER)
         snprintf(s, len, "%s: MSVC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               _MSC_VER, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
         snprintf(s, len, "%s: SNC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
         snprintf(s, len, "%s: MinGW (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
         snprintf(s, len, "%s: Clang/LLVM (%s) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __clang_version__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
         snprintf(s, len, "%s: GCC (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#else
         snprintf(s, len, "%s %u-bit",
               msg_hash_to_str(MSG_UNKNOWN_COMPILER),
               (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
         break;
      default:
      case RARCH_CAPABILITIES_NONE:
         break;
   }

   return 0;
}

void retroarch_set_current_core_type(enum rarch_core_type type, bool explicitly_set)
{
   if (explicitly_set && !has_set_core)
   {
      has_set_core                = true;
      explicit_current_core_type  = type;
      current_core_type           = type;
   }
   else if (!has_set_core)
      current_core_type           = type;
}

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
void retroarch_fail(int error_code, const char *error)
{
   /* We cannot longjmp unless we're in retroarch_main_init().
    * If not, something went very wrong, and we should
    * just exit right away. */
   retro_assert(rarch_error_on_init);

   strlcpy(error_string, error, sizeof(error_string));
   longjmp(error_sjlj_context, error_code);
}

bool retroarch_main_quit(void)
{
#ifdef HAVE_DISCORD
   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_SHUTDOWN;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
   command_event(CMD_EVENT_DISCORD_DEINIT, NULL);
   discord_is_inited          = false;
#endif

   if (!rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL))
   {
      command_event(CMD_EVENT_AUTOSAVE_STATE, NULL);
      command_event(CMD_EVENT_DISABLE_OVERRIDES, NULL);
      command_event(CMD_EVENT_RESTORE_DEFAULT_SHADER_PRESET, NULL);
      command_event(CMD_EVENT_RESTORE_REMAPS, NULL);
   }

   rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
   rarch_menu_running_finished(true);

   return true;
}

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   runloop_ctx_msg_info_t msg_info;

   runloop_msg_queue_lock();

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   if (menu_widgets_ready())
   {
      ui_companion_driver_msg_queue_push(msg,
            prio, duration * 60 / 1000, flush);
      menu_widgets_msg_queue_push(NULL, msg,
            roundf((float)duration / 60.0f * 1000.0f), title, icon, category, prio, flush);
      runloop_msg_queue_unlock();
      return;
   }
#endif

   if (flush)
      msg_queue_clear(runloop_msg_queue);

   msg_info.msg      = msg;
   msg_info.prio     = prio;
   msg_info.duration = duration;
   msg_info.flush    = flush;

   if (runloop_msg_queue)
   {
      msg_queue_push(runloop_msg_queue, msg_info.msg,
            msg_info.prio, msg_info.duration,
            title, icon, category);
      ui_companion_driver_msg_queue_push(msg_info.msg,
            msg_info.prio, msg_info.duration, msg_info.flush);
   }

   runloop_msg_queue_unlock();
}

void runloop_get_status(bool *is_paused, bool *is_idle,
      bool *is_slowmotion, bool *is_perfcnt_enable)
{
   *is_paused         = runloop_paused;
   *is_idle           = runloop_idle;
   *is_slowmotion     = runloop_slowmotion;
   *is_perfcnt_enable = runloop_perfcnt_enable;
}

#define BSV_MOVIE_IS_EOF() (bsv_movie_state.movie_end && bsv_movie_state.eof_exit)

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
#define TIME_TO_EXIT(quit_key_pressed) (runloop_shutdown_initiated || quit_key_pressed || !is_alive || BSV_MOVIE_IS_EOF() || ((runloop_max_frames != 0) && (frame_count >= runloop_max_frames)) || runloop_exec)

#define RUNLOOP_CHECK_CHEEVOS() (settings->bools.cheevos_enable && rcheevos_loaded && (!rcheevos_cheats_are_enabled && !rcheevos_cheats_were_enabled))

#ifdef HAVE_NETWORKING
/* FIXME: This is an ugly way to tell Netplay this... */
#define RUNLOOP_NETPLAY_PAUSE() netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL)
#else
#define RUNLOOP_NETPLAY_PAUSE() ((void)0)
#endif

#ifdef HAVE_MENU
static bool input_driver_toggle_button_combo(
      unsigned mode, input_bits_t* p_input)
{
   switch (mode)
   {
      case INPUT_TOGGLE_DOWN_Y_L_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L3_R3:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return false;
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      case INPUT_TOGGLE_L3_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_DOWN_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      case INPUT_TOGGLE_HOLD_START:
      {
         static rarch_timer_t timer = {0};

         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
         {
            /* timer only runs while start is held down */
            rarch_timer_end(&timer);
            return false;
         }

         /* user started holding down the start button, start the timer */
         if (!rarch_timer_is_running(&timer))
            rarch_timer_begin(&timer, HOLD_START_DELAY_SEC);

         rarch_timer_tick(&timer);

         if (!timer.timer_end && rarch_timer_has_expired(&timer))
         {
            /* start has been held down long enough, stop timer and enter menu */
            rarch_timer_end(&timer);
            return true;
         }

         return false;
      }
      default:
      case INPUT_TOGGLE_NONE:
         return false;
   }

   return true;
}
#endif

#define HOTKEY_CHECK(cmd1, cmd2, cond, cond2) \
   { \
      static bool old_pressed                   = false; \
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
      if (pressed && !old_pressed) \
         if (cond) \
            command_event(cmd2, cond2); \
      old_pressed                               = pressed; \
   }

#define HOTKEY_CHECK3(cmd1, cmd2, cmd3, cmd4, cmd5, cmd6) \
   { \
      static bool old_pressed                   = false; \
      static bool old_pressed2                  = false; \
      static bool old_pressed3                  = false; \
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
      bool pressed2                             = BIT256_GET(current_bits, cmd3); \
      bool pressed3                             = BIT256_GET(current_bits, cmd5); \
      if (pressed && !old_pressed) \
         command_event(cmd2, (void*)(intptr_t)0); \
      else if (pressed2 && !old_pressed2) \
         command_event(cmd4, (void*)(intptr_t)0); \
      else if (pressed3 && !old_pressed3) \
         command_event(cmd6, (void*)(intptr_t)0); \
      old_pressed                               = pressed; \
      old_pressed2                              = pressed2; \
      old_pressed3                              = pressed3; \
   }

static enum runloop_state runloop_check_state(
      settings_t *settings,
      bool input_nonblock_state,
      bool runloop_is_paused,
      float fastforward_ratio,
      unsigned *sleep_ms)
{
   input_bits_t current_bits;
#ifdef HAVE_MENU
   static input_bits_t last_input      = {{0}};
#endif
   static bool old_focus               = true;
   bool is_focused                     = video_has_focus();
   bool is_alive                       = current_video ?
      current_video->alive(video_driver_data) : true;
   uint64_t frame_count                = video_driver_frame_count;
   bool focused                        = true;
   bool pause_nonactive                = settings->bools.pause_nonactive;
   bool rarch_is_initialized           = rarch_is_inited;
#ifdef HAVE_MENU
   bool menu_driver_binding_state      = menu_driver_is_binding_state();
   bool menu_is_alive                  = menu_driver_is_alive();
   unsigned menu_toggle_gamepad_combo  = settings->uints.input_menu_toggle_gamepad_combo;
#ifdef HAVE_EASTEREGG
   static uint64_t seq                 = 0;
#endif
#endif

#ifdef HAVE_LIBNX
   /* Should be called once per frame */
   if (!appletMainLoop())
      return RUNLOOP_STATE_QUIT;
#endif

   BIT256_CLEAR_ALL_PTR(&current_bits);

   input_driver_block_libretro_input            = false;
   input_driver_block_hotkey                    = false;

   if (     current_input->keyboard_mapping_is_blocked
         && current_input->keyboard_mapping_is_blocked(current_input_data))
      input_driver_block_hotkey = true;


#ifdef HAVE_MENU
   if (menu_is_alive && !(settings->bools.menu_unified_controls && !menu_input_dialog_get_display_kb()))
      input_menu_keys_pressed(&current_bits);
   else
#endif
      input_keys_pressed(&current_bits);

#ifdef HAVE_MENU
   last_input                       = current_bits;
   if (
         ((menu_toggle_gamepad_combo != INPUT_TOGGLE_NONE) &&
          input_driver_toggle_button_combo(
             menu_toggle_gamepad_combo, &last_input)))
      BIT256_SET(current_bits, RARCH_MENU_TOGGLE);
#endif

   if (input_driver_flushing_input)
   {
      input_driver_flushing_input = false;
      if (bits_any_set(current_bits.data, ARRAY_SIZE(current_bits.data)))
      {
         BIT256_CLEAR_ALL(current_bits);
         if (runloop_is_paused)
            BIT256_SET(current_bits, RARCH_PAUSE_TOGGLE);
         input_driver_flushing_input = true;
      }
   }

   if (!video_driver_is_threaded_internal())
   {
      const ui_application_t *application = ui_companion 
         ? ui_companion->application : NULL;
      if (application)
         application->process_events();
   }

#ifdef HAVE_MENU
   if (menu_driver_binding_state)
      BIT256_CLEAR_ALL(current_bits);
#endif

#ifdef HAVE_OVERLAY
   /* Check next overlay */
   HOTKEY_CHECK(RARCH_OVERLAY_NEXT, CMD_EVENT_OVERLAY_NEXT, true, NULL);
#endif

   /* Check fullscreen toggle */
   {
      bool fullscreen_toggled = !runloop_is_paused
#ifdef HAVE_MENU
         || menu_is_alive;
#else
      ;
#endif
      HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY, CMD_EVENT_FULLSCREEN_TOGGLE,
            fullscreen_toggled, NULL);
   }

   /* Check mouse grab toggle */
   HOTKEY_CHECK(RARCH_GRAB_MOUSE_TOGGLE, CMD_EVENT_GRAB_MOUSE_TOGGLE, true, NULL); 

#ifdef HAVE_OVERLAY
   {
      static char prev_overlay_restore = false;
      if (input_driver_keyboard_linefeed_enable)
      {
         prev_overlay_restore  = false;
         retroarch_overlay_init();
      }
      else if (prev_overlay_restore)
      {
         if (!settings->bools.input_overlay_hide_in_menu)
            retroarch_overlay_init();
         prev_overlay_restore = false;
      }
   }
#endif

   /* Check quit key */
   {
      bool trig_quit_key;
      static bool quit_key     = false;
      static bool old_quit_key = false;
      static bool runloop_exec = false;
      quit_key                 = BIT256_GET(
            current_bits, RARCH_QUIT_KEY);
      trig_quit_key            = quit_key && !old_quit_key;
      old_quit_key             = quit_key;

      /* Check double press if enabled */
      if (trig_quit_key && settings->bools.quit_press_twice)
      {
         static retro_time_t quit_key_time   = 0;
         retro_time_t cur_time = cpu_features_get_time_usec();
         trig_quit_key         = (cur_time - quit_key_time < QUIT_DELAY_USEC);
         quit_key_time         = cur_time;

         if (!trig_quit_key)
         {
            float target_hz = 0.0;

            rarch_environment_cb(
                  RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &target_hz);

            runloop_msg_queue_push(msg_hash_to_str(MSG_PRESS_AGAIN_TO_QUIT), 1,
                  QUIT_DELAY_USEC * target_hz / 1000000,
                  true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }

      if (TIME_TO_EXIT(trig_quit_key))
      {
         bool quit_runloop = false;

         if ((runloop_max_frames != 0) && (frame_count >= runloop_max_frames)
               && runloop_max_frames_screenshot)
         {
            const char *screenshot_path = NULL;
            bool fullpath               = false;

            if (string_is_empty(runloop_max_frames_screenshot_path))
               screenshot_path          = path_get(RARCH_PATH_BASENAME);
            else
            {
               fullpath                 = true;
               screenshot_path          = runloop_max_frames_screenshot_path;
            }

            RARCH_LOG("Taking a screenshot before exiting...\n");

            /* Take a screenshot before we exit. */
            if (!take_screenshot(settings->paths.directory_screenshot,
                     screenshot_path, false,
                     video_driver_cached_frame_has_valid_framebuffer(), fullpath, false))
            {
               RARCH_ERR("Could not take a screenshot before exiting.\n");
            }
         }

         if (runloop_exec)
            runloop_exec = false;

         if (runloop_core_shutdown_initiated && settings->bools.load_dummy_on_core_shutdown)
         {
            content_ctx_info_t content_info;

            content_info.argc               = 0;
            content_info.argv               = NULL;
            content_info.args               = NULL;
            content_info.environ_get        = NULL;

            if (task_push_start_dummy_core(&content_info))
            {
               /* Loads dummy core instead of exiting RetroArch completely.
                * Aborts core shutdown if invoked. */
               rarch_ctl(RARCH_CTL_UNSET_SHUTDOWN, NULL);
               runloop_core_shutdown_initiated = false;
            }
            else
               quit_runloop = true;
         }
         else
            quit_runloop = true;

         if (quit_runloop)
         {
            old_quit_key                 = quit_key;
            retroarch_main_quit();
            return RUNLOOP_STATE_QUIT;
         }
      }
   }

#if defined(HAVE_MENU)
   menu_animation_update();

#ifdef HAVE_MENU_WIDGETS
   if (menu_widgets_ready())
   {
      runloop_msg_queue_lock();
      menu_widgets_iterate();
      runloop_msg_queue_unlock();
   }
#endif

   if (menu_is_alive)
   {
      enum menu_action action;
      static input_bits_t old_input = {{0}};
      static enum menu_action
         old_action              = MENU_ACTION_CANCEL;
      bool focused               = false;
      input_bits_t trigger_input = current_bits;
      global_t *global           = &g_extern;

      menu_ctx_iterate_t iter;

      retro_ctx.poll_cb();

      bits_clear_bits(trigger_input.data, old_input.data,
            ARRAY_SIZE(trigger_input.data));

      action                    = (enum menu_action)menu_event(&current_bits, &trigger_input);
      focused                   = pause_nonactive ? is_focused : true;
      focused                   = focused && !ui_companion_is_on_foreground();

      iter.action               = action;

      if (global)
      {
         if (action == old_action)
         {
            retro_time_t press_time           = cpu_features_get_time_usec();

            if (action == MENU_ACTION_NOOP)
               global->menu.noop_press_time   = press_time - global->menu.noop_start_time;
            else
               global->menu.action_press_time = press_time - global->menu.action_start_time;
         }
         else
         {
            if (action == MENU_ACTION_NOOP)
            {
               global->menu.noop_start_time      = cpu_features_get_time_usec();
               global->menu.noop_press_time      = 0;

               if (global->menu.prev_action == old_action)
                  global->menu.action_start_time = global->menu.prev_start_time;
               else
                  global->menu.action_start_time = cpu_features_get_time_usec();
            }
            else
            {
               if (  global->menu.prev_action == action &&
                     global->menu.noop_press_time < 200000) /* 250ms */
               {
                  global->menu.action_start_time = global->menu.prev_start_time;
                  global->menu.action_press_time = cpu_features_get_time_usec() - global->menu.action_start_time;
               }
               else
               {
                  global->menu.prev_start_time   = cpu_features_get_time_usec();
                  global->menu.prev_action       = action;
                  global->menu.action_press_time = 0;
               }
            }
         }
      }

      if (!menu_driver_iterate(&iter))
         rarch_menu_running_finished(false);

      if (focused || !runloop_idle)
      {
         bool core_type_is_dummy = current_core_type == CORE_TYPE_DUMMY;
         bool libretro_running   = menu_display_libretro_running(
               rarch_is_initialized,
               core_type_is_dummy);

         menu_driver_render(runloop_idle, rarch_is_initialized,
              core_type_is_dummy);
         if (settings->bools.audio_enable_menu &&
               !libretro_running)
            audio_driver_menu_sample();

#ifdef HAVE_EASTEREGG
         {
            bool library_name_is_empty = string_is_empty(runloop_system.info.library_name);

            if (library_name_is_empty && trigger_input.data[0])
            {
               seq |= trigger_input.data[0] & 0xF0;

               if (seq == 1157460427127406720ULL)
               {
                  content_ctx_info_t content_info;
                  content_info.argc                   = 0;
                  content_info.argv                   = NULL;
                  content_info.args                   = NULL;
                  content_info.environ_get            = NULL;

                  task_push_start_builtin_core(
                        &content_info,
                        CORE_TYPE_GONG, NULL, NULL);
               }

               seq <<= 8;
            }
            else if (!library_name_is_empty)
               seq = 0;
         }
#endif
      }

      old_input                 = current_bits;
      old_action                = action;

      if (!focused || runloop_idle)
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }
   else
#endif
   {
#if defined(HAVE_MENU) && defined(HAVE_EASTEREGG)
      seq = 0;
#endif
      if (runloop_idle)
      {
         retro_ctx.poll_cb();
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
      }
   }

   /* Check game focus toggle */
   HOTKEY_CHECK(RARCH_GAME_FOCUS_TOGGLE, CMD_EVENT_GAME_FOCUS_TOGGLE, true, NULL);
   /* Check if we have pressed the UI companion toggle button */
   HOTKEY_CHECK(RARCH_UI_COMPANION_TOGGLE, CMD_EVENT_UI_COMPANION_TOGGLE, true, NULL);

#ifdef HAVE_MENU
   /* Check if we have pressed the menu toggle button */
   {
      static bool old_pressed = false;
      char *menu_driver       = settings->arrays.menu_driver;
      bool pressed            = BIT256_GET(
            current_bits, RARCH_MENU_TOGGLE) &&
         !string_is_equal(menu_driver, "null");
      bool core_type_is_dummy = current_core_type == CORE_TYPE_DUMMY;

      if (menu_keyboard_key_state[RETROK_F1] == 1)
      {
         if (menu_driver_is_alive())
         {
            if (rarch_is_initialized && !core_type_is_dummy)
            {
               rarch_menu_running_finished(false);
               menu_event_kb_set(false, RETROK_F1);
            }
         }
      }
      else if ((!menu_keyboard_key_state[RETROK_F1] &&
               (pressed && !old_pressed)) ||
            core_type_is_dummy)
      {
         if (menu_driver_is_alive())
         {
            if (rarch_is_initialized && !core_type_is_dummy)
               rarch_menu_running_finished(false);
         }
         else
         {
            menu_display_toggle_set_reason(MENU_TOGGLE_REASON_USER);
            rarch_menu_running();
         }
      }
      else
         menu_event_kb_set(false, RETROK_F1);

      old_pressed             = pressed;
   }

   /* Check if we have pressed the "send debug info" button.
    * Must press 3 times in a row to activate, but it will
    * alert the user of this with each press of the hotkey. */
   {
      int any_i;
      static uint32_t debug_seq   = 0;
      static bool old_pressed     = false;
      static bool old_any_pressed = false;
      bool any_pressed            = false;
      bool pressed                = BIT256_GET(current_bits, RARCH_SEND_DEBUG_INFO);

      for (any_i = 0; any_i < ARRAY_SIZE(current_bits.data); any_i++)
      {
         if (current_bits.data[any_i])
         {
            any_pressed = true;
            break;
         }
      }

      if (pressed && !old_pressed)
         debug_seq |= pressed ? 1 : 0;

      switch (debug_seq)
      {
         case 1: /* pressed hotkey one time */
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO),
                  2, 180, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;
         case 3: /* pressed hotkey two times */
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO),
                  2, 180, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;
         case 7: /* pressed hotkey third and final time */
            debug_seq = 0;
            command_event(CMD_EVENT_SEND_DEBUG_INFO, NULL);
            break;
      }

      if (any_pressed && !old_any_pressed)
      {
         debug_seq <<= 1;

         if (debug_seq > 7)
            debug_seq = 0;
      }

      old_pressed     = pressed;
      old_any_pressed = any_pressed;
   }

   /* Check if we have pressed the FPS toggle button */
   HOTKEY_CHECK(RARCH_FPS_TOGGLE, CMD_EVENT_FPS_TOGGLE, true, NULL);

   /* Check if we have pressed the netplay host toggle button */
   HOTKEY_CHECK(RARCH_NETPLAY_HOST_TOGGLE, CMD_EVENT_NETPLAY_HOST_TOGGLE, true, NULL);

   if (menu_driver_is_alive())
   {
      if (!settings->bools.menu_throttle_framerate && !fastforward_ratio)
         return RUNLOOP_STATE_MENU_ITERATE;

      return RUNLOOP_STATE_END;
   }
#endif

   if (pause_nonactive)
      focused                = is_focused;

   /* Check if we have pressed the screenshot toggle button */
   HOTKEY_CHECK(RARCH_SCREENSHOT, CMD_EVENT_TAKE_SCREENSHOT, true, NULL);

   /* Check if we have pressed the audio mute toggle button */
   HOTKEY_CHECK(RARCH_MUTE, CMD_EVENT_AUDIO_MUTE_TOGGLE, true, NULL); 

   /* Check if we have pressed the OSK toggle button */
   HOTKEY_CHECK(RARCH_OSK, CMD_EVENT_OSK_TOGGLE, true, NULL); 

   /* Check if we have pressed the recording toggle button */
   HOTKEY_CHECK(RARCH_RECORDING_TOGGLE, CMD_EVENT_RECORDING_TOGGLE, true, NULL); 

   /* Check if we have pressed the AI Service toggle button */
   HOTKEY_CHECK(RARCH_AI_SERVICE, CMD_EVENT_AI_SERVICE_TOGGLE, true, NULL); 

   /* Check if we have pressed the streaming toggle button */
   HOTKEY_CHECK(RARCH_STREAMING_TOGGLE, CMD_EVENT_STREAMING_TOGGLE, true, NULL); 

   if (BIT256_GET(current_bits, RARCH_VOLUME_UP))
      command_event(CMD_EVENT_VOLUME_UP, NULL);
   else if (BIT256_GET(current_bits, RARCH_VOLUME_DOWN))
      command_event(CMD_EVENT_VOLUME_DOWN, NULL);

#ifdef HAVE_NETWORKING
   /* Check Netplay */
   HOTKEY_CHECK(RARCH_NETPLAY_GAME_WATCH, CMD_EVENT_NETPLAY_GAME_WATCH, true, NULL); 
#endif

   /* Check if we have pressed the pause button */
   {
      static bool old_frameadvance  = false;
      static bool old_pause_pressed = false;
      bool frameadvance_pressed     = BIT256_GET(
            current_bits, RARCH_FRAMEADVANCE);
      bool pause_pressed            = BIT256_GET(
            current_bits, RARCH_PAUSE_TOGGLE);
      bool trig_frameadvance        = frameadvance_pressed && !old_frameadvance;

      /* Check if libretro pause key was pressed. If so, pause or
       * unpause the libretro core. */

      /* FRAMEADVANCE will set us into pause mode. */
      pause_pressed                |= !runloop_is_paused && trig_frameadvance;

      if (focused && pause_pressed && !old_pause_pressed)
         command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
      else if (focused && !old_focus)
         command_event(CMD_EVENT_UNPAUSE, NULL);
      else if (!focused && old_focus)
         command_event(CMD_EVENT_PAUSE, NULL);

      old_focus           = focused;
      old_pause_pressed   = pause_pressed;
      old_frameadvance    = frameadvance_pressed;

      if (runloop_is_paused)
      {
         bool toggle = !runloop_idle ? true : false;

         HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY,
               CMD_EVENT_FULLSCREEN_TOGGLE, true, &toggle);

         /* Check if it's not oneshot */
         if (!(trig_frameadvance || BIT256_GET(current_bits, RARCH_REWIND)))
            focused = false;
      }
   }

   if (!focused)
   {
      retro_ctx.poll_cb();
      return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }

   /* Check if we have pressed the fast forward button */
   /* To avoid continous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between then.
    */
   {
      static bool old_button_state      = false;
      static bool old_hold_button_state = false;
      bool new_button_state             = BIT256_GET(
            current_bits, RARCH_FAST_FORWARD_KEY);
      bool new_hold_button_state        = BIT256_GET(
            current_bits, RARCH_FAST_FORWARD_HOLD_KEY);

      if (new_button_state && !old_button_state)
      {
         if (input_nonblock_state)
         {
            input_driver_nonblock_state = false;
            runloop_fastmotion          = false;
            fastforward_after_frames    = 1;
         }
         else
         {
            input_driver_nonblock_state = true;
            runloop_fastmotion          = true;
         }
         driver_set_nonblock_state();
      }
      else if (old_hold_button_state != new_hold_button_state)
      {
         if (new_hold_button_state)
         {
            input_driver_nonblock_state = true;
            runloop_fastmotion          = true;
         }
         else
         {
            input_driver_nonblock_state = false;
            runloop_fastmotion          = false;
            fastforward_after_frames    = 1;
         }
         driver_set_nonblock_state();
      }

      /* Display the fast forward state to the user, if needed. */
      if (runloop_fastmotion)
      {
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
         if (!menu_widgets_set_fast_forward(true))
#endif
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_FAST_FORWARD), 1, 1, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      else
         menu_widgets_set_fast_forward(false);
#endif

      old_button_state                  = new_button_state;
      old_hold_button_state             = new_hold_button_state;
   }

   /* Check if we have pressed any of the state slot buttons */
   {
      static bool old_should_slot_increase = false;
      static bool old_should_slot_decrease = false;
      bool should_slot_increase            = BIT256_GET(
            current_bits, RARCH_STATE_SLOT_PLUS);
      bool should_slot_decrease            = BIT256_GET(
            current_bits, RARCH_STATE_SLOT_MINUS);
      bool should_set                      = false;
      int cur_state_slot                   = settings->ints.state_slot;

      /* Checks if the state increase/decrease keys have been pressed
       * for this frame. */
      if (should_slot_increase && !old_should_slot_increase)
      {
         configuration_set_int(settings, settings->ints.state_slot, cur_state_slot + 1);

         should_set = true;
      }
      else if (should_slot_decrease && !old_should_slot_decrease)
      {
         if (cur_state_slot > 0)
            configuration_set_int(settings, settings->ints.state_slot, cur_state_slot - 1);

         should_set = true;
      }

      if (should_set)
      {
         char msg[128];
         msg[0] = '\0';

         snprintf(msg, sizeof(msg), "%s: %d",
               msg_hash_to_str(MSG_STATE_SLOT),
               settings->ints.state_slot);

         runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         RARCH_LOG("%s\n", msg);
      }

      old_should_slot_increase = should_slot_increase;
      old_should_slot_decrease = should_slot_decrease;
   }

   /* Check if we have pressed any of the savestate buttons */
   HOTKEY_CHECK(RARCH_SAVE_STATE_KEY, CMD_EVENT_SAVE_STATE, true, NULL);
   HOTKEY_CHECK(RARCH_LOAD_STATE_KEY, CMD_EVENT_LOAD_STATE, true, NULL);

#ifdef HAVE_CHEEVOS
   rcheevos_hardcore_active = settings->bools.cheevos_enable
      && settings->bools.cheevos_hardcore_mode_enable
      && rcheevos_loaded && !rcheevos_hardcore_paused;

   if (rcheevos_hardcore_active && rcheevos_state_loaded_flag)
   {
      rcheevos_hardcore_paused = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED), 0, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   if (!rcheevos_hardcore_active)
#endif
   {
      char s[128];
      bool rewinding = false;
      unsigned t     = 0;

      s[0]           = '\0';

      rewinding      = state_manager_check_rewind(BIT256_GET(current_bits, RARCH_REWIND),
            settings->uints.rewind_granularity, runloop_paused, s, sizeof(s), &t);

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      if (!menu_widgets_ready())
#endif
         if (rewinding)
            runloop_msg_queue_push(s, 0, t, true, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      menu_widgets_set_rewind(rewinding);
#endif
   }

   /* Checks if slowmotion toggle/hold was being pressed and/or held. */
#ifdef HAVE_CHEEVOS
   if (!rcheevos_hardcore_active)
#endif
   {
      static bool old_slowmotion_button_state      = false;
      static bool old_slowmotion_hold_button_state = false;
      bool new_slowmotion_button_state             = BIT256_GET(
            current_bits, RARCH_SLOWMOTION_KEY);
      bool new_slowmotion_hold_button_state        = BIT256_GET(
            current_bits, RARCH_SLOWMOTION_HOLD_KEY);

      if (new_slowmotion_button_state && !old_slowmotion_button_state)
         runloop_slowmotion = !runloop_slowmotion;
      else if (old_slowmotion_hold_button_state != new_slowmotion_hold_button_state)
         runloop_slowmotion = new_slowmotion_hold_button_state;

      if (runloop_slowmotion)
      {
         if (settings->bools.video_black_frame_insertion)
            if (!runloop_idle)
               video_driver_cached_frame();

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
         if (!menu_widgets_ready())
         {
#endif
            if (state_manager_frame_is_reversed())
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_SLOW_MOTION_REWIND), 1, 1, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_SLOW_MOTION), 1, 1, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      }
#endif

      old_slowmotion_button_state                  = new_slowmotion_button_state;
      old_slowmotion_hold_button_state             = new_slowmotion_hold_button_state;
   }

   /* Check movie record toggle */
   HOTKEY_CHECK(RARCH_BSV_RECORD_TOGGLE, CMD_EVENT_BSV_RECORDING_TOGGLE, true, NULL);

   /* Check shader prev/next */
   HOTKEY_CHECK(RARCH_SHADER_NEXT, CMD_EVENT_SHADER_NEXT, true, NULL);
   HOTKEY_CHECK(RARCH_SHADER_PREV, CMD_EVENT_SHADER_PREV, true, NULL);

   /* Check if we have pressed any of the disk buttons */
   HOTKEY_CHECK3(
         RARCH_DISK_EJECT_TOGGLE, CMD_EVENT_DISK_EJECT_TOGGLE,
         RARCH_DISK_NEXT,         CMD_EVENT_DISK_NEXT,
         RARCH_DISK_PREV,         CMD_EVENT_DISK_PREV);

   /* Check if we have pressed the reset button */
   HOTKEY_CHECK(RARCH_RESET, CMD_EVENT_RESET, true, NULL);

   /* Check cheats */
   HOTKEY_CHECK3(
         RARCH_CHEAT_INDEX_PLUS,  CMD_EVENT_CHEAT_INDEX_PLUS,
         RARCH_CHEAT_INDEX_MINUS, CMD_EVENT_CHEAT_INDEX_MINUS,
         RARCH_CHEAT_TOGGLE,      CMD_EVENT_CHEAT_TOGGLE);

   if (settings->bools.video_shader_watch_files)
   {
      static rarch_timer_t timer = {0};
      static bool need_to_apply = false;

      if (video_shader_check_for_changes())
      {
         need_to_apply = true;

         if (!rarch_timer_is_running(&timer))
         {
            /* rarch_timer_t convenience functions only support whole seconds. */

            /* rarch_timer_begin */
            timer.timeout_end = cpu_features_get_time_usec() + SHADER_FILE_WATCH_DELAY_MSEC * 1000;
            timer.timer_begin = true;
            timer.timer_end   = false;
         }
      }

      /* If a file is modified atomically (moved/renamed from a different file), we have no idea how long that might take.
       * If we're trying to re-apply shaders immediately after changes are made to the original file(s), the filesystem might be in an in-between
       * state where the new file hasn't been moved over yet and the original file was already deleted. This leaves us no choice
       * but to wait an arbitrary amount of time and hope for the best.
       */
      if (need_to_apply)
      {
         /* rarch_timer_tick */
         timer.current    = cpu_features_get_time_usec();
         timer.timeout_us = (timer.timeout_end - timer.current);

         if (!timer.timer_end && rarch_timer_has_expired(&timer))
         {
            rarch_timer_end(&timer);
            need_to_apply = false;
            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }

   return RUNLOOP_STATE_ITERATE;
}

void runloop_set(enum runloop_action action)
{
   switch (action)
   {
      case RUNLOOP_ACTION_AUTOSAVE:
         runloop_autosave = true;
         break;
      case RUNLOOP_ACTION_NONE:
         break;
   }
}

void runloop_unset(enum runloop_action action)
{
   switch (action)
   {
      case RUNLOOP_ACTION_AUTOSAVE:
         runloop_autosave = false;
         break;
      case RUNLOOP_ACTION_NONE:
         break;
   }
}

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until
 * button input in order to wake up the loop,
 * -1 if we forcibly quit out of the RetroArch iteration loop.
 **/
int runloop_iterate(unsigned *sleep_ms)
{
   unsigned i;
   bool runloop_is_paused                       = runloop_paused;
   bool input_nonblock_state                    = input_driver_nonblock_state;
   settings_t *settings                         = configuration_settings;
   float fastforward_ratio                      = settings->floats.fastforward_ratio;
   unsigned video_frame_delay                   = settings->uints.video_frame_delay;
   bool vrr_runloop_enable                      = settings->bools.vrr_runloop_enable;
   unsigned max_users                           = input_driver_max_users;

#ifdef HAVE_DISCORD
   if (discord_is_inited)
      discord_run_callbacks();
#endif

   if (runloop_frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */
      retro_usec_t runloop_last_frame_time = runloop_frame_time_last;
      retro_time_t current                 = cpu_features_get_time_usec();
      bool is_locked_fps                   = (runloop_is_paused || input_nonblock_state)
         | !!recording_data;
      retro_time_t delta                   = (!runloop_last_frame_time || is_locked_fps) ?
         runloop_frame_time.reference
         : (current - runloop_last_frame_time);

      if (is_locked_fps)
         runloop_frame_time_last = 0;
      else
      {
         float slowmotion_ratio  = settings->floats.slowmotion_ratio;

         runloop_frame_time_last = current;

         if (runloop_slowmotion)
            delta /= slowmotion_ratio;
      }

      runloop_frame_time.callback(delta);
   }

   switch ((enum runloop_state)
         runloop_check_state(
            settings,
            input_nonblock_state,
            runloop_is_paused,
            fastforward_ratio,
            sleep_ms))
   {
      case RUNLOOP_STATE_QUIT:
         frame_limit_last_time = 0.0;
         command_event(CMD_EVENT_QUIT, NULL);
         return -1;
      case RUNLOOP_STATE_POLLED_AND_SLEEP:
         RUNLOOP_NETPLAY_PAUSE();
         *sleep_ms = 10;
         return 1;
      case RUNLOOP_STATE_END:
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) 
               && settings->bools.menu_pause_libretro)
            RUNLOOP_NETPLAY_PAUSE();
#endif
         goto end;
      case RUNLOOP_STATE_MENU_ITERATE:
#ifdef HAVE_NETWORKING
         RUNLOOP_NETPLAY_PAUSE();
         return 0;
#endif
      case RUNLOOP_STATE_ITERATE:
         break;
   }

   if (runloop_autosave)
      autosave_lock();

   /* Used for rewinding while playback/record. */
   if (bsv_movie_state_handle)
      bsv_movie_state_handle->frame_pos[bsv_movie_state_handle->frame_ptr]
         = intfstream_tell(bsv_movie_state_handle->file);

   if (camera_cb.caps && camera_driver && camera_driver->poll && camera_data)
      camera_driver->poll(camera_data,
            camera_cb.frame_raw_framebuffer,
            camera_cb.frame_opengl_texture);

   /* Update binds for analog dpad modes. */
   for (i = 0; i < max_users; i++)
   {
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode != ANALOG_DPAD_NONE)
      {
         struct retro_keybind *general_binds = input_config_binds[i];
         struct retro_keybind *auto_binds    = input_autoconf_binds[i];
         input_push_analog_dpad(general_binds, dpad_mode);
         input_push_analog_dpad(auto_binds,    dpad_mode);
      }
   }

   if ((video_frame_delay > 0) && !input_nonblock_state)
      retro_sleep(video_frame_delay);

   {
      bool want_runahead            = false;
      unsigned run_ahead_num_frames = 0;
#ifdef HAVE_RUNAHEAD
      run_ahead_num_frames          = settings->uints.run_ahead_frames;
      /* Run Ahead Feature replaces the call to core_run in this loop */
      want_runahead                 = settings->bools.run_ahead_enabled && run_ahead_num_frames > 0;
#ifdef HAVE_NETWORKING
      want_runahead                 = want_runahead && !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif
#endif

#ifdef HAVE_RUNAHEAD
      if (want_runahead)
         do_runahead(run_ahead_num_frames, settings->bools.run_ahead_secondary_instance);
      else
#endif
         core_run();
   }

   /* Increment runtime tick counter after each call to
    * core_run() or run_ahead() */
   rarch_core_runtime_tick();

#ifdef HAVE_CHEEVOS
   if (RUNLOOP_CHECK_CHEEVOS())
      rcheevos_test();
#endif
   cheat_manager_apply_retro_cheats();

#ifdef HAVE_DISCORD
   if (discord_is_inited && discord_is_ready())
      discord_update(DISCORD_PRESENCE_GAME);
#endif

   for (i = 0; i < max_users; i++)
   {
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode != ANALOG_DPAD_NONE)
      {
         struct retro_keybind *general_binds = input_config_binds[i];
         struct retro_keybind *auto_binds    = input_autoconf_binds[i];

         input_pop_analog_dpad(general_binds);
         input_pop_analog_dpad(auto_binds);
      }
   }

   if (bsv_movie_state_handle)
   {
      bsv_movie_state_handle->frame_ptr    =
         (bsv_movie_state_handle->frame_ptr + 1)
         & bsv_movie_state_handle->frame_mask;

      bsv_movie_state_handle->first_rewind =
         !bsv_movie_state_handle->did_rewind;
      bsv_movie_state_handle->did_rewind   = false;
   }

   if (runloop_autosave)
      autosave_unlock();

   /* Condition for max speed x0.0 when vrr_runloop is off to skip that part */
   if (!(fastforward_ratio || vrr_runloop_enable))
      return 0;

end:
   if (vrr_runloop_enable)
   {
      struct retro_system_av_info *av_info = &video_driver_av_info;

      /* Sync on video only, block audio later. */
      if (fastforward_after_frames && settings->bools.audio_sync)
      {
         if (fastforward_after_frames == 1)
         {
            /* Nonblocking audio */
            audio_driver_set_nonblocking_state_internal(
                  true,
                  audio_driver_chunk_nonblock_size);
         }

         fastforward_after_frames++;

         if (fastforward_after_frames == 6)
         {
            /* Blocking audio */
            audio_driver_set_nonblocking_state_internal(
                  settings->bools.audio_sync ? false : true,
                  audio_driver_chunk_block_size);
            fastforward_after_frames = 0;
         }
      }

      /* Fast Forward for max speed x0.0 */
      if (!fastforward_ratio && runloop_fastmotion)
         return 0;

      frame_limit_minimum_time =
         (retro_time_t)roundf(1000000.0f / (av_info->timing.fps *
                  (runloop_fastmotion ? fastforward_ratio : 1.0f)));
   }

   {
      retro_time_t to_sleep_ms  = (
            (frame_limit_last_time + frame_limit_minimum_time)
            - cpu_features_get_time_usec()) / 1000;

      if (to_sleep_ms > 0)
      {
         *sleep_ms              = (unsigned)to_sleep_ms;
         /* Combat jitter a bit. */
         frame_limit_last_time += frame_limit_minimum_time;
         return 1;
      }
   }

   frame_limit_last_time  = cpu_features_get_time_usec();

   return 0;
}

rarch_system_info_t *runloop_get_system_info(void)
{
   return &runloop_system;
}

struct retro_system_info *runloop_get_libretro_system_info(void)
{
   return &runloop_system.info;
}

char *get_retroarch_launch_arguments(void)
{
   return launch_arguments;
}

void rarch_force_video_driver_fallback(const char *driver)
{
   settings_t *settings        = configuration_settings;
   ui_msg_window_t *msg_window = NULL;

   strlcpy(settings->arrays.video_driver,
         driver, sizeof(settings->arrays.video_driver));

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
   /* UI companion driver is not inited yet, just call into it directly */
   msg_window = &ui_msg_window_win32;
#endif

   if (msg_window)
   {
      char text[PATH_MAX_LENGTH];
      ui_msg_window_state window_state;
      char *title          = strdup(msg_hash_to_str(MSG_ERROR));

      text[0]              = '\0';

      snprintf(text, sizeof(text),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK),
            driver);

      window_state.buttons = UI_MSG_WINDOW_OK;
      window_state.text    = strdup(text);
      window_state.title   = title;
      window_state.window  = NULL;

      msg_window->error(&window_state);

      free(title);
   }
   exit(1);
}

void rarch_get_cpu_architecture_string(char *cpu_arch_str, size_t len)
{
   enum frontend_architecture arch = frontend_driver_get_cpu_architecture();

   if (!cpu_arch_str || !len)
      return;

   switch (arch)
   {
      case FRONTEND_ARCH_X86:
         strlcpy(cpu_arch_str, "x86", len);
         break;
      case FRONTEND_ARCH_X86_64:
         strlcpy(cpu_arch_str, "x64", len);
         break;
      case FRONTEND_ARCH_PPC:
         strlcpy(cpu_arch_str, "PPC", len);
         break;
      case FRONTEND_ARCH_ARM:
         strlcpy(cpu_arch_str, "ARM", len);
         break;
      case FRONTEND_ARCH_ARMV7:
         strlcpy(cpu_arch_str, "ARMv7", len);
         break;
      case FRONTEND_ARCH_ARMV8:
         strlcpy(cpu_arch_str, "ARMv8", len);
         break;
      case FRONTEND_ARCH_MIPS:
         strlcpy(cpu_arch_str, "MIPS", len);
         break;
      case FRONTEND_ARCH_TILE:
         strlcpy(cpu_arch_str, "Tilera", len);
         break;
      case FRONTEND_ARCH_NONE:
      default:
         strlcpy(cpu_arch_str,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               len);
         break;
   }
}

bool rarch_write_debug_info(void)
{
   int i;
   char str[PATH_MAX_LENGTH];
   char debug_filepath[PATH_MAX_LENGTH];
   gfx_ctx_mode_t mode_info              = {0};
   settings_t *settings                  = configuration_settings;
   RFILE *file                           = NULL;
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();
   const char *cpu_model                 = NULL;
   const char *path_config               = path_get(RARCH_PATH_CONFIG);
   unsigned lang                         = 
      *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);

   str[0]                                = 
      debug_filepath[0]                  = '\0';

   /* Only print our debug info in English */
   if (lang != RETRO_LANGUAGE_ENGLISH)
      msg_hash_set_uint(MSG_HASH_USER_LANGUAGE, RETRO_LANGUAGE_ENGLISH);

   fill_pathname_resolve_relative(
         debug_filepath,
         path_config,
         DEBUG_INFO_FILENAME,
         sizeof(debug_filepath));

   file = filestream_open(debug_filepath,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open debug info file for writing: %s\n", debug_filepath);
      goto error;
   }

#ifdef HAVE_MENU
   {
      time_t time_;
      char timedate[255];

      timedate[0] = '\0';

      time(&time_);

      setlocale(LC_TIME, "");

      strftime(timedate, sizeof(timedate),
            "%Y-%m-%d %H:%M:%S", localtime(&time_));

      filestream_printf(file, "Log Date/Time: %s\n", timedate);
   }
#endif
   filestream_printf(file, "RetroArch Version: %s\n", PACKAGE_VERSION);

#ifdef HAVE_LAKKA
   if (frontend->get_lakka_version)
   {
      frontend->get_lakka_version(str, sizeof(str));
      filestream_printf(file, "Lakka Version: %s\n", str);
      str[0] = '\0';
   }
#endif

   filestream_printf(file, "RetroArch Build Date: %s\n", __DATE__);
#ifdef HAVE_GIT_VERSION
   filestream_printf(file, "RetroArch Git Commit: %s\n", retroarch_git_version);
#endif

   filestream_printf(file, "\n");

   cpu_model = frontend_driver_get_cpu_model_name();

   if (!string_is_empty(cpu_model))
      filestream_printf(file, "CPU Model Name: %s\n", cpu_model);

   retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));
   filestream_printf(file, "CPU Capabilities: %s\n", str);

   str[0] = '\0';

   rarch_get_cpu_architecture_string(str, sizeof(str));

   filestream_printf(file, "CPU Architecture: %s\n", str);
   filestream_printf(file, "CPU Cores: %u\n", cpu_features_get_core_amount());

   {
      uint64_t memory_used = frontend_driver_get_used_memory();
      uint64_t memory_total = frontend_driver_get_total_memory();

      filestream_printf(file, "Memory: %" PRIu64 "/%" PRIu64 " MB\n", memory_used / 1024 / 1024, memory_total / 1024 / 1024);
   }

   filestream_printf(file, "GPU Device: %s\n", !string_is_empty(video_driver_get_gpu_device_string()) ?
         video_driver_get_gpu_device_string() : "n/a");
   filestream_printf(file, "GPU API/Driver Version: %s\n", !string_is_empty(video_driver_get_gpu_api_version_string()) ?
         video_driver_get_gpu_api_version_string() : "n/a");

   filestream_printf(file, "\n");

   video_context_driver_get_video_size(&mode_info);

   filestream_printf(file, "Window Resolution: %u x %u\n", mode_info.width, mode_info.height);

   {
      float width = 0, height = 0, refresh = 0.0f;
      gfx_ctx_metrics_t metrics;
      
      metrics.type  = DISPLAY_METRIC_PIXEL_WIDTH;
      metrics.value = &width;

      video_context_driver_get_metrics(&metrics);

      metrics.type = DISPLAY_METRIC_PIXEL_HEIGHT;
      metrics.value = &height;
      video_context_driver_get_metrics(&metrics);

      video_context_driver_get_refresh_rate(&refresh);

      filestream_printf(file, "Monitor Resolution: %d x %d @ %.2f Hz (configured for %.2f Hz)\n", (int)width, (int)height, refresh, settings->floats.video_refresh_rate);
   }

   filestream_printf(file, "\n");

   str[0] = '\0';

   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   filestream_printf(file, "%s\n", str);

   str[0] = '\0';

   filestream_printf(file, "Frontend Identifier: %s\n", frontend->ident);

   if (frontend->get_name)
   {
      frontend->get_name(str, sizeof(str));
      filestream_printf(file, "Frontend Name: %s\n", str);
      str[0] = '\0';
   }

   if (frontend->get_os)
   {
      int major = 0, minor = 0;
      const char *warning = "";

      frontend->get_os(str, sizeof(str), &major, &minor);

      if (strstr(str, "Build 16299"))
         warning = " (WARNING: Fall Creator's Update detected... OpenGL performance may be low)";

      filestream_printf(file, "Frontend OS: %s (v%d.%d)%s\n", str, major, minor, warning);

      str[0] = '\0';
   }

   filestream_printf(file, "\n");
   filestream_printf(file, "Input Devices (autoconfig is %s):\n", settings->bools.input_autodetect_enable ? "enabled" : "disabled");

   for (i = 0; i < 4; i++)
   {
      if (input_is_autoconfigured(i))
      {
         unsigned retro_id;
         unsigned rebind  = 0;
         unsigned device  = settings->uints.input_libretro_device[i];

         device          &= RETRO_DEVICE_MASK;

         if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_ANALOG)
         {
            for (retro_id = 0; retro_id < RARCH_ANALOG_BIND_LIST_END; retro_id++)
            {
               char descriptor[300];
               const struct retro_keybind *keybind   = &input_config_binds[i][retro_id];
               const struct retro_keybind *auto_bind = (const struct retro_keybind*)
                  input_config_get_bind_auto(i, retro_id);

               input_config_get_bind_string(descriptor,
                     keybind, auto_bind, sizeof(descriptor));

               if (!strstr(descriptor, "Auto") 
                     && auto_bind 
                     && !auto_bind->valid 
                     && (auto_bind->joykey != 0xFFFF)
                     && !string_is_empty(auto_bind->joykey_label))
                  rebind++;
            }
         }

         if (rebind)
            filestream_printf(file, "  - Port #%d autoconfigured (WARNING: %u keys rebinded):\n", i, rebind);
         else
            filestream_printf(file, "  - Port #%d autoconfigured:\n", i);

         filestream_printf(file, "      - Device name: %s (#%d)\n",
            input_config_get_device_name(i),
            input_autoconfigure_get_device_name_index(i));
         filestream_printf(file, "      - Display name: %s\n",
            input_config_get_device_display_name(i) ?
            input_config_get_device_display_name(i) : "N/A");
         filestream_printf(file, "      - Config path: %s\n",
            input_config_get_device_display_name(i) ?
            input_config_get_device_config_path(i) : "N/A");
         filestream_printf(file, "      - VID/PID: %d/%d (0x%04X/0x%04X)\n",
            input_config_get_vid(i), input_config_get_pid(i),
            input_config_get_vid(i), input_config_get_pid(i));
      }
      else
         filestream_printf(file, "  - Port #%d not autoconfigured\n", i);
   }

   filestream_printf(file, "\n");

   filestream_printf(file, "Drivers:\n");

   {
      gfx_ctx_ident_t ident_info = {0};
      const input_driver_t *input_driver;
      const input_device_driver_t *joypad_driver;
      const char *driver;
#ifdef HAVE_MENU
      driver = menu_driver_ident();

      if (string_is_equal(driver, settings->arrays.menu_driver))
         filestream_printf(file, "  - Menu: %s\n",
               !string_is_empty(driver) ? driver : "n/a");
      else
         filestream_printf(file, "  - Menu: %s (configured for %s)\n",
               !string_is_empty(driver) 
               ? driver 
               : "n/a",
               !string_is_empty(settings->arrays.menu_driver) 
               ? settings->arrays.menu_driver 
               : "n/a");
#endif
      driver =
#ifdef HAVE_THREADS
      (video_driver_is_threaded_internal()) ?
      video_thread_get_ident() :
#endif
      video_driver_get_ident();

      if (string_is_equal(driver, settings->arrays.video_driver))
         filestream_printf(file, "  - Video: %s\n",
               !string_is_empty(driver) 
               ? driver 
               : "n/a");
      else
         filestream_printf(file, "  - Video: %s (configured for %s)\n",
               !string_is_empty(driver) 
               ? driver 
               : "n/a",
               !string_is_empty(settings->arrays.video_driver) 
               ? settings->arrays.video_driver 
               : "n/a");

      video_context_driver_get_ident(&ident_info);
      filestream_printf(file, "  - Video Context: %s\n",
            ident_info.ident ? ident_info.ident : "n/a");

      driver = audio_driver_get_ident();

      if (string_is_equal(driver, settings->arrays.audio_driver))
         filestream_printf(file, "  - Audio: %s\n",
               !string_is_empty(driver) ? driver : "n/a");
      else
         filestream_printf(file, "  - Audio: %s (configured for %s)\n",
               !string_is_empty(driver) ? driver : "n/a",
               !string_is_empty(settings->arrays.audio_driver) ? settings->arrays.audio_driver : "n/a");

      input_driver = current_input;

      if (input_driver && string_is_equal(input_driver->ident, settings->arrays.input_driver))
         filestream_printf(file, "  - Input: %s\n", !string_is_empty(input_driver->ident) ? input_driver->ident : "n/a");
      else
         filestream_printf(file, "  - Input: %s (configured for %s)\n", !string_is_empty(input_driver->ident) ? input_driver->ident : "n/a", !string_is_empty(settings->arrays.input_driver) ? settings->arrays.input_driver : "n/a");

      joypad_driver = (input_driver->get_joypad_driver ? input_driver->get_joypad_driver(input_driver_get_data()) : NULL);

      if (joypad_driver && string_is_equal(joypad_driver->ident, settings->arrays.input_joypad_driver))
         filestream_printf(file, "  - Joypad: %s\n", !string_is_empty(joypad_driver->ident) ? joypad_driver->ident : "n/a");
      else
         filestream_printf(file, "  - Joypad: %s (configured for %s)\n", !string_is_empty(joypad_driver->ident) ? joypad_driver->ident : "n/a", !string_is_empty(settings->arrays.input_joypad_driver) ? settings->arrays.input_joypad_driver : "n/a");
   }

   filestream_printf(file, "\n");

   filestream_printf(file, "Configuration related settings:\n");
   filestream_printf(file, "  - Save on exit: %s\n", settings->bools.config_save_on_exit ? "yes" : "no");
   filestream_printf(file, "  - Load content-specific core options automatically: %s\n", settings->bools.game_specific_options ? "yes" : "no");
   filestream_printf(file, "  - Load override files automatically: %s\n", settings->bools.auto_overrides_enable ? "yes" : "no");
   filestream_printf(file, "  - Load remap files automatically: %s\n", settings->bools.auto_remaps_enable ? "yes" : "no");
   filestream_printf(file, "  - Sort saves in folders: %s\n", settings->bools.sort_savefiles_enable ? "yes" : "no");
   filestream_printf(file, "  - Sort states in folders: %s\n", settings->bools.sort_savestates_enable ? "yes" : "no");
   filestream_printf(file, "  - Write saves in content dir: %s\n", settings->bools.savefiles_in_content_dir ? "yes" : "no");
   filestream_printf(file, "  - Write savestates in content dir: %s\n", settings->bools.savestates_in_content_dir ? "yes" : "no");

   filestream_printf(file, "\n");

   filestream_printf(file, "Auto load state: %s\n", settings->bools.savestate_auto_load ? "yes (WARNING: not compatible with all cores)" : "no");
   filestream_printf(file, "Auto save state: %s\n", settings->bools.savestate_auto_save ? "yes" : "no");

   filestream_printf(file, "\n");

   filestream_printf(file, "Buildbot cores URL: %s\n", settings->paths.network_buildbot_url);
   filestream_printf(file, "Auto-extract downloaded archives: %s\n", settings->bools.network_buildbot_auto_extract_archive ? "yes" : "no");

   {
      size_t count = 0;
      core_info_list_t *core_info_list = NULL;
      struct string_list *list = NULL;
      const char *ext = file_path_str(FILE_PATH_RDB_EXTENSION);

      /* remove dot */
      if (!string_is_empty(ext) && ext[0] == '.' && strlen(ext) > 1)
         ext++;

      core_info_get_list(&core_info_list);

      if (core_info_list)
         count = core_info_list->count;

      filestream_printf(file, "Core info: %u entries\n", count);

      count = 0;

      list = dir_list_new(settings->paths.path_content_database, ext, false, true, false, true);

      if (list)
      {
         count = list->size;
         string_list_free(list);
      }

      filestream_printf(file, "Databases: %u entries\n", count);
   }

   filestream_printf(file, "\n");

   filestream_printf(file, "Performance and latency-sensitive features (may have a large impact depending on the core):\n");
   filestream_printf(file, "  - Video:\n");
   filestream_printf(file, "      - Runahead: %s\n", settings->bools.run_ahead_enabled ? "yes (WARNING: not compatible with all cores)" : "no");
   filestream_printf(file, "      - Rewind: %s\n", settings->bools.rewind_enable ? "yes (WARNING: not compatible with all cores)" : "no");
   filestream_printf(file, "      - Hard GPU Sync: %s\n", settings->bools.video_hard_sync ? "yes" : "no");
   filestream_printf(file, "      - Frame Delay: %u frames\n", settings->uints.video_frame_delay);
   filestream_printf(file, "      - Max Swapchain Images: %u\n", settings->uints.video_max_swapchain_images);
   filestream_printf(file, "      - Max Run Speed: %.1f x\n", settings->floats.fastforward_ratio);
   filestream_printf(file, "      - Sync to exact content framerate: %s\n", settings->bools.vrr_runloop_enable ? "yes (note: designed for G-Sync/FreeSync displays only)" : "no");
   filestream_printf(file, "      - Fullscreen: %s\n", settings->bools.video_fullscreen ? "yes" : "no");
   filestream_printf(file, "      - Windowed Fullscreen: %s\n", settings->bools.video_windowed_fullscreen ? "yes" : "no");
   filestream_printf(file, "      - Threaded Video: %s\n", settings->bools.video_threaded ? "yes" : "no");
   filestream_printf(file, "      - Vsync: %s\n", settings->bools.video_vsync ? "yes" : "no");
   filestream_printf(file, "      - Vsync Swap Interval: %u frames\n", settings->uints.video_swap_interval);
   filestream_printf(file, "      - Black Frame Insertion: %s\n", settings->bools.video_black_frame_insertion ? "yes" : "no");
   filestream_printf(file, "      - Bilinear Filtering: %s\n", settings->bools.video_smooth ? "yes" : "no");
   filestream_printf(file, "      - Video CPU Filter: %s\n", !string_is_empty(settings->paths.path_softfilter_plugin) ? settings->paths.path_softfilter_plugin : "n/a");
   filestream_printf(file, "      - CRT SwitchRes: %s\n", (settings->uints.crt_switch_resolution > CRT_SWITCH_NONE) ? "yes" : "no");
   filestream_printf(file, "      - Video Shared Context: %s\n", settings->bools.video_shared_context ? "yes" : "no");

   {
      video_shader_ctx_t shader_info = {0};

      video_shader_driver_get_current_shader(&shader_info);

      if (shader_info.data)
      {
         if (string_is_equal(shader_info.data->path, settings->paths.path_shader))
            filestream_printf(file, "      - Video Shader: %s\n", !string_is_empty(settings->paths.path_shader) ? settings->paths.path_shader : "n/a");
         else
            filestream_printf(file, "      - Video Shader: %s (configured for %s)\n", !string_is_empty(shader_info.data->path) ? shader_info.data->path : "n/a", !string_is_empty(settings->paths.path_shader) ? settings->paths.path_shader : "n/a");
      }
      else
         filestream_printf(file, "      - Video Shader: n/a\n");
   }

   filestream_printf(file, "  - Audio:\n");
   filestream_printf(file, "      - Audio Enabled: %s\n", settings->bools.audio_enable ? "yes" : "no (WARNING: content framerate will be incorrect)");
   filestream_printf(file, "      - Audio Sync: %s\n", settings->bools.audio_sync ? "yes" : "no (WARNING: content framerate will be incorrect)");

   {
      const char *s = NULL;

      switch (settings->uints.audio_resampler_quality)
      {
         case RESAMPLER_QUALITY_DONTCARE:
            s = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);
            break;
         case RESAMPLER_QUALITY_LOWEST:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWEST);
            break;
         case RESAMPLER_QUALITY_LOWER:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWER);
            break;
         case RESAMPLER_QUALITY_HIGHER:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHER);
            break;
         case RESAMPLER_QUALITY_HIGHEST:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHEST);
            break;
         case RESAMPLER_QUALITY_NORMAL:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_NORMAL);
            break;
      }

      filestream_printf(file, "      - Resampler Quality: %s\n", !string_is_empty(s) ? s : "n/a");
   }

   filestream_printf(file, "      - Audio Latency: %u ms\n", settings->uints.audio_latency);
   filestream_printf(file, "      - Dynamic Rate Control (DRC): %.3f\n", *audio_get_float_ptr(AUDIO_ACTION_RATE_CONTROL_DELTA));
   filestream_printf(file, "      - Max Timing Skew: %.2f\n", settings->floats.audio_max_timing_skew);
   filestream_printf(file, "      - Output Rate: %u Hz\n", settings->uints.audio_out_rate);
   filestream_printf(file, "      - DSP Plugin: %s\n", !string_is_empty(settings->paths.path_audio_dsp_plugin) ? settings->paths.path_audio_dsp_plugin : "n/a");

   {
      core_info_list_t *core_info_list = NULL;
      bool                       found = false;

      filestream_printf(file, "\n");
      filestream_printf(file, "Firmware files found:\n");

      core_info_get_list(&core_info_list);

      if (core_info_list)
      {
         unsigned i;

         for (i = 0; i < core_info_list->count; i++)
         {
            core_info_t *info = &core_info_list->list[i];

            if (!info)
               continue;

            if (info->firmware_count)
            {
               unsigned j;
               bool core_found = false;

               for (j = 0; j < info->firmware_count; j++)
               {
                  core_info_firmware_t *firmware = &info->firmware[j];
                  char path[PATH_MAX_LENGTH];

                  if (!firmware)
                     continue;

                  path[0] = '\0';

                  fill_pathname_join(
                        path,
                        settings->paths.directory_system,
                        firmware->path,
                        sizeof(path));

                  if (filestream_exists(path))
                  {
                     found = true;

                     if (!core_found)
                     {
                        core_found = true;
                        filestream_printf(file, "  - %s:\n", !string_is_empty(info->core_name) ? info->core_name : path_basename(info->path));
                     }

                     filestream_printf(file, "      - %s (%s)\n", firmware->path, firmware->optional ? "optional" : "required");
                  }
               }
            }
         }
      }

      if (!found)
         filestream_printf(file, "  - n/a\n");
   }

   filestream_close(file);

   RARCH_LOG("Wrote debug info to %s\n", debug_filepath);

   msg_hash_set_uint(MSG_HASH_USER_LANGUAGE, lang);

   return true;

error:
   return false;
}

#ifdef HAVE_NETWORKING
static void send_debug_info_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   if (task_data)
   {
      http_transfer_data_t *data = (http_transfer_data_t*)task_data;

      if (!data || data->len == 0)
      {
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO));

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO),
               2, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
         free(task_data);
         return;
      }

      /* don't use string_is_equal() here instead of the memcmp() because the data isn't NULL-terminated */
      if (!string_is_empty(data->data) && data->len >= 2 && !memcmp(data->data, "OK", 2))
      {
         char buf[32] = {0};
         struct string_list *list;

         memcpy(buf, data->data, data->len);

         list = string_split(buf, " ");

         if (list && list->size > 1)
         {
            unsigned id = 0;
            char msg[PATH_MAX_LENGTH];

            msg[0] = '\0';

            sscanf(list->elems[1].data, "%u", &id);

            snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_SENT_DEBUG_INFO), id);

            RARCH_LOG("%s\n", msg);

            runloop_msg_queue_push(
                  msg,
                  2, 600, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
        }

        if (list)
           string_list_free(list);
      }
      else
      {
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO));

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO),
               2, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      }

      free(task_data);
   }
   else
   {
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO));

      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   }
}
#endif

void rarch_send_debug_info(void)
{
#ifdef HAVE_NETWORKING
   char debug_filepath[PATH_MAX_LENGTH];
   const char *url             = "http://lobby.libretro.com/debuginfo/add/";
   char *info_buf              = NULL;
   const size_t param_buf_size = 65535;
   char *param_buf             = (char*)malloc(param_buf_size);
   char *param_buf_tmp         = NULL;
   int param_buf_pos           = 0;
   int64_t len                 = 0;
   const char *path_config     = path_get(RARCH_PATH_CONFIG);
   bool       info_written     = rarch_write_debug_info();

   debug_filepath[0]           = 
      param_buf[0]             = '\0';

   fill_pathname_resolve_relative(
         debug_filepath,
         path_config,
         DEBUG_INFO_FILENAME,
         sizeof(debug_filepath));

   if (info_written)
      filestream_read_file(debug_filepath, (void**)&info_buf, &len);

   if (string_is_empty(info_buf) || len == 0 || !info_written)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_SAVE_DEBUG_INFO),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      goto finish;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_SENDING_DEBUG_INFO));

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_SENDING_DEBUG_INFO),
         2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   param_buf_pos = (int)strlcpy(param_buf, "info=", param_buf_size);
   param_buf_tmp = param_buf + param_buf_pos;

   net_http_urlencode(&param_buf_tmp, info_buf);

   strlcat(param_buf, param_buf_tmp, param_buf_size - param_buf_pos);

   task_push_http_post_transfer(url, param_buf, true, NULL, send_debug_info_cb, NULL);

finish:
   if (param_buf)
      free(param_buf);
   if (info_buf)
      free(info_buf);
#endif
}

void rarch_log_file_init(void)
{
   char log_directory[PATH_MAX_LENGTH];
   char log_file_path[PATH_MAX_LENGTH];
   FILE             *fp       = NULL;
   settings_t *settings       = configuration_settings;
   bool log_to_file           = settings->bools.log_to_file;
   bool log_to_file_timestamp = settings->bools.log_to_file_timestamp;
   bool logging_to_file       = is_logging_to_file();
   
   log_directory[0]           = '\0';
   log_file_path[0]           = '\0';
   
   /* If this is the first run, generate a timestamped log
    * file name (do this even when not outputting timestamped
    * log files, since user may decide to switch at any moment...) */
   if (string_is_empty(timestamped_log_file_name))
   {
      char format[256];
      time_t cur_time      = time(NULL);
      const struct tm *tm_ = localtime(&cur_time);
      
      format[0] = '\0';
      strftime(format, sizeof(format), "retroarch__%Y_%m_%d__%H_%M_%S", tm_);
      fill_pathname_noext(timestamped_log_file_name, format,
            file_path_str(FILE_PATH_EVENT_LOG_EXTENSION),
            sizeof(timestamped_log_file_name));
   }
   
   /* If nothing has changed, do nothing */
   if ((!log_to_file && !logging_to_file) ||
       (log_to_file && logging_to_file))
      return;
   
   /* If we are currently logging to file and wish to stop,
    * de-initialise existing logger... */
   if (!log_to_file && logging_to_file)
   {
      retro_main_log_file_deinit();
      /* ...and revert to console */
      retro_main_log_file_init(NULL, false);
      return;
   }
   
   /* If we reach this point, then we are not currently
    * logging to file, and wish to do so */
   
   /* > Check whether we are already logging to console */
   fp = (FILE*)retro_main_log_file();
   /* De-initialise existing logger */
   if (fp)
      retro_main_log_file_deinit();
   
   /* > Get directory/file paths */
   if (log_file_override_active)
   {
      /* Get log directory */
      const char *last_slash        = find_last_slash(log_file_override_path);

      if (last_slash)
      {
         char tmp_buf[PATH_MAX_LENGTH] = {0};
         size_t path_length            = last_slash + 1 - log_file_override_path;

         if ((path_length > 1) && (path_length < PATH_MAX_LENGTH))
            strlcpy(tmp_buf, log_file_override_path, path_length * sizeof(char));
         strlcpy(log_directory, tmp_buf, sizeof(log_directory));
      }
      
      /* Get log file path */
      strlcpy(log_file_path, log_file_override_path, sizeof(log_file_path));
   }
   else if (!string_is_empty(settings->paths.log_dir))
   {
      /* Get log directory */
      strlcpy(log_directory, settings->paths.log_dir, sizeof(log_directory));
      
      /* Get log file path */
      fill_pathname_join(log_file_path, settings->paths.log_dir,
            log_to_file_timestamp 
            ? timestamped_log_file_name 
            : file_path_str(FILE_PATH_DEFAULT_EVENT_LOG),
            sizeof(log_file_path));
   }
   
   /* > Attempt to initialise log file */
   if (!string_is_empty(log_file_path))
   {
      /* Create log directory, if required */
      if (!string_is_empty(log_directory))
      {
         if (!path_is_directory(log_directory))
         {
            if (!path_mkdir(log_directory))
            {
               /* Re-enable console logging and output error message */
               retro_main_log_file_init(NULL, false);
               RARCH_ERR("Failed to create system event log directory: %s\n", log_directory);
               return;
            }
         }
      }
      
      /* When RetroArch is launched, log file is overwritten.
       * On subsequent calls within the same session, it is appended to. */
      retro_main_log_file_init(log_file_path, log_file_created);
      if (is_logging_to_file())
         log_file_created = true;
      return;
   }
   
   /* If we reach this point, then something went wrong...
    * Just fall back to console logging */
   retro_main_log_file_init(NULL, false);
   RARCH_ERR("Failed to initialise system event file logging...\n");
}

void rarch_log_file_deinit(void)
{
   FILE *fp = NULL;
   
   /* De-initialise existing logger, if currently logging to file */
   if (is_logging_to_file())
      retro_main_log_file_deinit();
   
   /* If logging is currently disabled... */
   fp = (FILE*)retro_main_log_file();
   if (!fp) /* ...initialise logging to console */
      retro_main_log_file_init(NULL, false);
}

enum retro_language rarch_get_language_from_iso(const char *iso639)
{
   unsigned i;
   enum retro_language lang = RETRO_LANGUAGE_ENGLISH;

   struct lang_pair
   {
      const char *iso639;
      enum retro_language lang;
   };

   const struct lang_pair pairs[] =
   {
      {"en", RETRO_LANGUAGE_ENGLISH},
      {"ja", RETRO_LANGUAGE_JAPANESE},
      {"fr", RETRO_LANGUAGE_FRENCH},
      {"es", RETRO_LANGUAGE_SPANISH},
      {"de", RETRO_LANGUAGE_GERMAN},
      {"it", RETRO_LANGUAGE_ITALIAN},
      {"nl", RETRO_LANGUAGE_DUTCH},
      {"pt_BR", RETRO_LANGUAGE_PORTUGUESE_BRAZIL},
      {"pt_PT", RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
      {"pt", RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
      {"ru", RETRO_LANGUAGE_RUSSIAN},
      {"ko", RETRO_LANGUAGE_KOREAN},
      {"zh_CN", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"zh_SG", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"zh_HK", RETRO_LANGUAGE_CHINESE_TRADITIONAL},
      {"zh_TW", RETRO_LANGUAGE_CHINESE_TRADITIONAL},
      {"zh", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"eo", RETRO_LANGUAGE_ESPERANTO},
      {"pl", RETRO_LANGUAGE_POLISH},
      {"vi", RETRO_LANGUAGE_VIETNAMESE},
      {"ar", RETRO_LANGUAGE_ARABIC},
      {"el", RETRO_LANGUAGE_GREEK},
   };
   
   if (string_is_empty(iso639))
      return lang;

   for (i = 0; i < ARRAY_SIZE(pairs); i++)
   {
      if (strcasestr(iso639, pairs[i].iso639))
      {
         lang = pairs[i].lang;
         break;
      }
   }

   return lang;
}

/* Libretro core loader */

static void retro_run_null(void)
{
}

static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
}

static void retro_input_poll_null(void)
{
}

static void core_input_state_poll_maybe(void)
{
   if (current_core.poll_type == POLL_TYPE_NORMAL)
      input_driver_poll();
}

static int16_t core_input_state_poll(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   if (current_core.poll_type == POLL_TYPE_LATE)
   {
      if (!current_core.input_polled)
         input_driver_poll();

      current_core.input_polled = true;
   }
   return input_state(port, device, idx, id);
}

/**
 * core_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks
 * to default callback functions.
 **/
static bool core_init_libretro_cbs(struct retro_callbacks *cbs)
{
   current_core.retro_set_video_refresh(video_driver_frame);
   current_core.retro_set_audio_sample(audio_driver_sample);
   current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   current_core.retro_set_input_state(core_input_state_poll);
   current_core.retro_set_input_poll(core_input_state_poll_maybe);

   core_set_default_callbacks(cbs);

#ifdef HAVE_NETWORKING
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      return true;

   core_set_netplay_callbacks();
#endif

   return true;
}

/**
 * core_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
bool core_set_default_callbacks(struct retro_callbacks *cbs)
{
   cbs->frame_cb        = video_driver_frame;
   cbs->sample_cb       = audio_driver_sample;
   cbs->sample_batch_cb = audio_driver_sample_batch;
   cbs->state_cb        = core_input_state_poll;
   cbs->poll_cb         = input_driver_poll;

   return true;
}

static bool core_uninit_libretro_callbacks(void)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)&retro_ctx;
   if (!cbs)
      return false;

   cbs->frame_cb        = retro_frame_null;
   cbs->sample_cb       = NULL;
   cbs->sample_batch_cb = NULL;
   cbs->state_cb        = NULL;
   cbs->poll_cb         = retro_input_poll_null;

   current_core.inited  = false;

   return true;
}

/**
 * core_set_rewind_callbacks:
 *
 * Sets the audio sampling callbacks based on whether or not
 * rewinding is currently activated.
 **/
bool core_set_rewind_callbacks(void)
{
   if (state_manager_frame_is_reversed())
   {
      current_core.retro_set_audio_sample(audio_driver_sample_rewind);
      current_core.retro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      current_core.retro_set_audio_sample(audio_driver_sample);
      current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   }
   return true;
}

#ifdef HAVE_NETWORKING
/**
 * core_set_netplay_callbacks:
 *
 * Set the I/O callbacks to use netplay's interceding callback system. Should
 * only be called while initializing netplay.
 **/
bool core_set_netplay_callbacks(void)
{
   /* Force normal poll type for netplay. */
   current_core.poll_type = POLL_TYPE_NORMAL;

   /* And use netplay's interceding callbacks */
   current_core.retro_set_video_refresh(video_frame_net);
   current_core.retro_set_audio_sample(audio_sample_net);
   current_core.retro_set_audio_sample_batch(audio_sample_batch_net);
   current_core.retro_set_input_state(input_state_net);

   return true;
}

/**
 * core_unset_netplay_callbacks
 *
 * Unset the I/O callbacks from having used netplay's interceding callback
 * system. Should only be called while uninitializing netplay.
 */
bool core_unset_netplay_callbacks(void)
{
   struct retro_callbacks cbs;
   if (!core_set_default_callbacks(&cbs))
      return false;

   current_core.retro_set_video_refresh(cbs.frame_cb);
   current_core.retro_set_audio_sample(cbs.sample_cb);
   current_core.retro_set_audio_sample_batch(cbs.sample_batch_cb);
   current_core.retro_set_input_state(cbs.state_cb);

   return true;
}
#endif

bool core_set_cheat(retro_ctx_cheat_info_t *info)
{
   current_core.retro_cheat_set(info->index, info->enabled, info->code);
   return true;
}

bool core_reset_cheat(void)
{
   current_core.retro_cheat_reset();
   return true;
}

bool core_api_version(retro_ctx_api_info_t *api)
{
   if (!api)
      return false;
   api->version = current_core.retro_api_version();
   return true;
}

bool core_set_poll_type(unsigned *type)
{
   current_core.poll_type = *type;
   return true;
}

void core_uninit_symbols(void)
{
   uninit_libretro_symbols(&current_core);
   current_core.symbols_inited = false;
}

bool core_init_symbols(enum rarch_core_type *type)
{
   if (!type || !init_libretro_symbols(*type, &current_core))
      return false;

   if (!current_core.retro_run)
      current_core.retro_run   = retro_run_null;
   current_core.symbols_inited = true;
   return true;
}

bool core_set_controller_port_device(retro_ctx_controller_info_t *pad)
{
   if (!pad)
      return false;

#ifdef HAVE_RUNAHEAD
   remember_controller_port_device(pad->port, pad->device);
#endif

   current_core.retro_set_controller_port_device(pad->port, pad->device);
   return true;
}

bool core_get_memory(retro_ctx_memory_info_t *info)
{
   if (!info)
      return false;
   info->size  = current_core.retro_get_memory_size(info->id);
   info->data  = current_core.retro_get_memory_data(info->id);
   return true;
}

bool core_load_game(retro_ctx_load_content_info_t *load_info)
{
   bool contentless = false;
   bool is_inited   = false;

   video_driver_set_cached_frame_ptr(NULL);

#ifdef HAVE_RUNAHEAD
   set_load_content_info(load_info);
   clear_controller_port_map();
#endif

   content_get_status(&contentless, &is_inited);
   set_save_state_in_background(false);

   if (load_info && load_info->special)
      current_core.game_loaded = current_core.retro_load_game_special(
            load_info->special->id, load_info->info, load_info->content->size);
   else if (load_info && !string_is_empty(load_info->content->elems[0].data))
      current_core.game_loaded = current_core.retro_load_game(load_info->info);
   else if (contentless)
      current_core.game_loaded = current_core.retro_load_game(NULL);
   else
      current_core.game_loaded = false;

   return current_core.game_loaded;
}

bool core_get_system_info(struct retro_system_info *system)
{
   if (!system)
      return false;
   current_core.retro_get_system_info(system);
   return true;
}

bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   if (!info || !current_core.retro_unserialize(info->data_const, info->size))
      return false;

#if HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, info);
#endif

   return true;
}

bool core_serialize(retro_ctx_serialize_info_t *info)
{
   if (!info || !current_core.retro_serialize(info->data, info->size))
      return false;
   return true;
}

bool core_serialize_size(retro_ctx_size_info_t *info)
{
   if (!info)
      return false;
   info->size = current_core.retro_serialize_size();
   return true;
}

uint64_t core_serialization_quirks(void)
{
   return current_core.serialization_quirks_v;
}

void core_set_serialization_quirks(uint64_t quirks)
{
   current_core.serialization_quirks_v = quirks;
}

bool core_set_environment(retro_ctx_environ_info_t *info)
{
   if (!info)
      return false;
   current_core.retro_set_environment(info->env);
   return true;
}

bool core_get_system_av_info(struct retro_system_av_info *av_info)
{
   if (!av_info)
      return false;
   current_core.retro_get_system_av_info(av_info);
   return true;
}

bool core_reset(void)
{
   video_driver_set_cached_frame_ptr(NULL);

   current_core.retro_reset();
   return true;
}

bool core_init(void)
{
   video_driver_set_cached_frame_ptr(NULL);

   current_core.retro_init();
   current_core.inited          = true;
   return true;
}

bool core_unload(void)
{
   video_driver_set_cached_frame_ptr(NULL);

   if (current_core.inited)
      current_core.retro_deinit();

   return true;
}

bool core_unload_game(void)
{
   video_driver_free_hw_context();

   video_driver_set_cached_frame_ptr(NULL);

   if (current_core.game_loaded)
   {
      current_core.retro_unload_game();
      current_core.game_loaded = false;
   }

   audio_driver_stop();

   return true;
}

bool core_run(void)
{
   bool early_polling    = current_core.poll_type == POLL_TYPE_EARLY;
   bool late_polling     = current_core.poll_type == POLL_TYPE_LATE;
#ifdef HAVE_NETWORKING
   bool netplay_preframe = netplay_driver_ctl(
         RARCH_NETPLAY_CTL_PRE_FRAME, NULL);

   if (!netplay_preframe)
   {
      /* Paused due to netplay. We must poll and display something so that a
       * netplay peer pausing doesn't just hang. */
      input_driver_poll();
      video_driver_cached_frame();
      return true;
   }
#endif

   if (early_polling)
      input_driver_poll();
   else if (late_polling)
      current_core.input_polled = false;

   current_core.retro_run();

   if (late_polling && !current_core.input_polled)
      input_driver_poll();

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_POST_FRAME, NULL);
#endif

   return true;
}

static bool core_verify_api_version(void)
{
   unsigned api_version = current_core.retro_api_version();
   RARCH_LOG("%s: %u\n",
         msg_hash_to_str(MSG_VERSION_OF_LIBRETRO_API),
         api_version);
   RARCH_LOG("%s: %u\n",
         msg_hash_to_str(MSG_COMPILED_AGAINST_API),
         RETRO_API_VERSION);

   if (api_version != RETRO_API_VERSION)
   {
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_LIBRETRO_ABI_BREAK));
      return false;
   }
   return true;
}

bool core_load(unsigned poll_type_behavior)
{
   current_core.poll_type = poll_type_behavior;

   if (!core_verify_api_version())
      return false;
   if (!core_init_libretro_cbs(&retro_ctx))
      return false;

   core_get_system_av_info(&video_driver_av_info);

   return true;
}

bool core_get_region(retro_ctx_region_info_t *info)
{
  if (!info)
    return false;
  info->region = current_core.retro_get_region();
  return true;
}

bool core_has_set_input_descriptor(void)
{
   return current_core.has_set_input_descriptors;
}

bool core_is_inited(void)
{
  return current_core.inited;
}

bool core_is_symbols_inited(void)
{
  return current_core.symbols_inited;
}

void core_free_retro_game_info(struct retro_game_info *dest)
{
   if (!dest)
      return;
   if (dest->path)
      free((void*)dest->path);
   if (dest->data)
      free((void*)dest->data);
   if (dest->meta)
      free((void*)dest->meta);
   dest->path = NULL;
   dest->data = NULL;
   dest->meta = NULL;
}
