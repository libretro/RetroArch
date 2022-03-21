/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2015-2019 - Andrés Suárez (input remapping + other things)
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

#include <ctype.h>

#include <libretro.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_assert.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "file_path_special.h"
#include "configuration.h"
#include "content.h"
#include "config.def.h"
#include "config.features.h"
#include "input/input_keymaps.h"
#include "input/input_remapping.h"
#include "led/led_defines.h"
#include "defaults.h"
#include "core.h"
#include "paths.h"
#include "retroarch.h"
#include "verbosity.h"

#include "audio/audio_driver.h"
#include "record/record_driver.h"
#include "gfx/gfx_animation.h"

#include "tasks/task_content.h"
#include "tasks/tasks_internal.h"

#include "list_special.h"

#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#include "uwp/uwp_func.h"
#endif

#include "lakka.h"

#if defined(HAVE_LAKKA) || defined(HAVE_LIBNX)
#include "switch_performance_profiles.h"
#endif

enum video_driver_enum
{
   VIDEO_GL                 = 0,
   VIDEO_GL1,
   VIDEO_GL_CORE,
   VIDEO_VULKAN,
   VIDEO_METAL,
   VIDEO_DRM,
   VIDEO_XVIDEO,
   VIDEO_SDL,
   VIDEO_SDL2,
   VIDEO_SDL_DINGUX,
   VIDEO_SDL_RS90,
   VIDEO_EXT,
   VIDEO_WII,
   VIDEO_WIIU,
   VIDEO_XENON360,
   VIDEO_PSP1,
   VIDEO_VITA2D,
   VIDEO_PS2,
   VIDEO_CTR,
   VIDEO_SWITCH,
   VIDEO_D3D8,
   VIDEO_D3D9,
   VIDEO_D3D10,
   VIDEO_D3D11,
   VIDEO_D3D12,
   VIDEO_VG,
   VIDEO_OMAP,
   VIDEO_EXYNOS,
   VIDEO_SUNXI,
   VIDEO_DISPMANX,
   VIDEO_CACA,
   VIDEO_GDI,
   VIDEO_VGA,
   VIDEO_FPGA,
   VIDEO_RSX,
   VIDEO_NULL
};

enum audio_driver_enum
{
   AUDIO_RSOUND             = VIDEO_NULL + 1,
   AUDIO_AUDIOIO,
   AUDIO_OSS,
   AUDIO_ALSA,
   AUDIO_ALSATHREAD,
   AUDIO_TINYALSA,
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
   AUDIO_WASAPI,
   AUDIO_COREAUDIO,
   AUDIO_COREAUDIO3,
   AUDIO_PS3,
   AUDIO_XENON360,
   AUDIO_WII,
   AUDIO_WIIU,
   AUDIO_RWEBAUDIO,
   AUDIO_PSP,
   AUDIO_PS2,
   AUDIO_CTR,
   AUDIO_SWITCH,
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
   INPUT_SDL_DINGUX,
   INPUT_X,
   INPUT_WAYLAND,
   INPUT_DINPUT,
   INPUT_PS4,
   INPUT_PS3,
   INPUT_PSP,
   INPUT_PS2,
   INPUT_CTR,
   INPUT_SWITCH,
   INPUT_XENON360,
   INPUT_WII,
   INPUT_WIIU,
   INPUT_XINPUT,
   INPUT_UWP,
   INPUT_UDEV,
   INPUT_LINUXRAW,
   INPUT_COCOA,
   INPUT_QNX,
   INPUT_RWEBINPUT,
   INPUT_DOS,
   INPUT_WINRAW,
   INPUT_NULL
};

enum joypad_driver_enum
{
   JOYPAD_PS3               = INPUT_NULL + 1,
   JOYPAD_XINPUT,
   JOYPAD_GX,
   JOYPAD_WIIU,
   JOYPAD_XDK,
   JOYPAD_PS4,
   JOYPAD_PSP,
   JOYPAD_PS2,
   JOYPAD_CTR,
   JOYPAD_SWITCH,
   JOYPAD_DINPUT,
   JOYPAD_UDEV,
   JOYPAD_LINUXRAW,
   JOYPAD_ANDROID,
   JOYPAD_SDL,
   JOYPAD_SDL_DINGUX,
   JOYPAD_DOS,
   JOYPAD_HID,
   JOYPAD_QNX,
   JOYPAD_RWEBPAD,
   JOYPAD_MFI,
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

enum bluetooth_driver_enum
{
   BLUETOOTH_BLUETOOTHCTL          = CAMERA_NULL + 1,
   BLUETOOTH_BLUEZ,
   BLUETOOTH_NULL
};

enum wifi_driver_enum
{
   WIFI_CONNMANCTL          = BLUETOOTH_NULL + 1,
   WIFI_NMCLI,
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
   MENU_MATERIALUI,
   MENU_XMB,
   MENU_STRIPES,
   MENU_OZONE,
   MENU_NULL
};

enum record_driver_enum
{
   RECORD_FFMPEG            = MENU_NULL + 1,
   RECORD_NULL
};

enum midi_driver_enum
{
   MIDI_WINMM               = RECORD_NULL + 1,
   MIDI_ALSA,
   MIDI_NULL
};

#define DECLARE_BIND(base, bind, desc) { #base, desc, 0, bind, true }
#define DECLARE_META_BIND(level, base, bind, desc) { #base, desc, level, bind, true }

const struct input_bind_map input_config_bind_map[RARCH_BIND_LIST_END_NULL] = {
   DECLARE_BIND(b,                             RETRO_DEVICE_ID_JOYPAD_B,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B),
   DECLARE_BIND(y,                             RETRO_DEVICE_ID_JOYPAD_Y,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y),
   DECLARE_BIND(select,                        RETRO_DEVICE_ID_JOYPAD_SELECT,MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT),
   DECLARE_BIND(start,                         RETRO_DEVICE_ID_JOYPAD_START, MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START),
   DECLARE_BIND(up,                            RETRO_DEVICE_ID_JOYPAD_UP,    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP),
   DECLARE_BIND(down,                          RETRO_DEVICE_ID_JOYPAD_DOWN,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN),
   DECLARE_BIND(left,                          RETRO_DEVICE_ID_JOYPAD_LEFT,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT),
   DECLARE_BIND(right,                         RETRO_DEVICE_ID_JOYPAD_RIGHT, MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT),
   DECLARE_BIND(a,                             RETRO_DEVICE_ID_JOYPAD_A,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A),
   DECLARE_BIND(x,                             RETRO_DEVICE_ID_JOYPAD_X,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X),
   DECLARE_BIND(l,                             RETRO_DEVICE_ID_JOYPAD_L,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L),
   DECLARE_BIND(r,                             RETRO_DEVICE_ID_JOYPAD_R,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R),
   DECLARE_BIND(l2,                            RETRO_DEVICE_ID_JOYPAD_L2,    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2),
   DECLARE_BIND(r2,                            RETRO_DEVICE_ID_JOYPAD_R2,    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2),
   DECLARE_BIND(l3,                            RETRO_DEVICE_ID_JOYPAD_L3,    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3),
   DECLARE_BIND(r3,                            RETRO_DEVICE_ID_JOYPAD_R3,    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3),
   DECLARE_BIND(l_x_plus,                      RARCH_ANALOG_LEFT_X_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS),
   DECLARE_BIND(l_x_minus,                     RARCH_ANALOG_LEFT_X_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS),
   DECLARE_BIND(l_y_plus,                      RARCH_ANALOG_LEFT_Y_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS),
   DECLARE_BIND(l_y_minus,                     RARCH_ANALOG_LEFT_Y_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS),
   DECLARE_BIND(r_x_plus,                      RARCH_ANALOG_RIGHT_X_PLUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS),
   DECLARE_BIND(r_x_minus,                     RARCH_ANALOG_RIGHT_X_MINUS,   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS),
   DECLARE_BIND(r_y_plus,                      RARCH_ANALOG_RIGHT_Y_PLUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS),
   DECLARE_BIND(r_y_minus,                     RARCH_ANALOG_RIGHT_Y_MINUS,   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS),

   DECLARE_BIND(gun_trigger,                   RARCH_LIGHTGUN_TRIGGER,       MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER ),
   DECLARE_BIND(gun_offscreen_shot,            RARCH_LIGHTGUN_RELOAD,        MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD ),
   DECLARE_BIND(gun_aux_a,                     RARCH_LIGHTGUN_AUX_A,         MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A ),
   DECLARE_BIND(gun_aux_b,                     RARCH_LIGHTGUN_AUX_B,         MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B ),
   DECLARE_BIND(gun_aux_c,                     RARCH_LIGHTGUN_AUX_C,         MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C ),
   DECLARE_BIND(gun_start,                     RARCH_LIGHTGUN_START,         MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START ),
   DECLARE_BIND(gun_select,                    RARCH_LIGHTGUN_SELECT,        MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT ),
   DECLARE_BIND(gun_dpad_up,                   RARCH_LIGHTGUN_DPAD_UP,       MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP ),
   DECLARE_BIND(gun_dpad_down,                 RARCH_LIGHTGUN_DPAD_DOWN,     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN ),
   DECLARE_BIND(gun_dpad_left,                 RARCH_LIGHTGUN_DPAD_LEFT,     MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT ),
   DECLARE_BIND(gun_dpad_right,                RARCH_LIGHTGUN_DPAD_RIGHT,    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT ),

   DECLARE_BIND(turbo,                         RARCH_TURBO_ENABLE,           MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE),

   DECLARE_META_BIND(1, toggle_fast_forward,   RARCH_FAST_FORWARD_KEY,       MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY),
   DECLARE_META_BIND(2, hold_fast_forward,     RARCH_FAST_FORWARD_HOLD_KEY,  MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY),
   DECLARE_META_BIND(1, toggle_slowmotion,     RARCH_SLOWMOTION_KEY,         MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY),
   DECLARE_META_BIND(2, hold_slowmotion,       RARCH_SLOWMOTION_HOLD_KEY,    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY),
   DECLARE_META_BIND(2, toggle_vrr_runloop,    RARCH_VRR_RUNLOOP_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE),
   DECLARE_META_BIND(1, load_state,            RARCH_LOAD_STATE_KEY,         MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY),
   DECLARE_META_BIND(1, save_state,            RARCH_SAVE_STATE_KEY,         MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY),
   DECLARE_META_BIND(2, toggle_fullscreen,     RARCH_FULLSCREEN_TOGGLE_KEY,  MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY),
   DECLARE_META_BIND(2, close_content,         RARCH_CLOSE_CONTENT_KEY,      MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY),
#ifdef HAVE_LAKKA
   DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY,               MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY),
#else
   DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY,               MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY),
#endif
   DECLARE_META_BIND(2, state_slot_increase,   RARCH_STATE_SLOT_PLUS,        MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS),
   DECLARE_META_BIND(2, state_slot_decrease,   RARCH_STATE_SLOT_MINUS,       MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS),
   DECLARE_META_BIND(1, rewind,                RARCH_REWIND,                 MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND),
   DECLARE_META_BIND(2, movie_record_toggle,   RARCH_BSV_RECORD_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE),
   DECLARE_META_BIND(2, pause_toggle,          RARCH_PAUSE_TOGGLE,           MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE),
   DECLARE_META_BIND(2, frame_advance,         RARCH_FRAMEADVANCE,           MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE),
   DECLARE_META_BIND(2, reset,                 RARCH_RESET,                  MENU_ENUM_LABEL_VALUE_INPUT_META_RESET),
   DECLARE_META_BIND(2, shader_next,           RARCH_SHADER_NEXT,            MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT),
   DECLARE_META_BIND(2, shader_prev,           RARCH_SHADER_PREV,            MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV),
   DECLARE_META_BIND(2, cheat_index_plus,      RARCH_CHEAT_INDEX_PLUS,       MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS),
   DECLARE_META_BIND(2, cheat_index_minus,     RARCH_CHEAT_INDEX_MINUS,      MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS),
   DECLARE_META_BIND(2, cheat_toggle,          RARCH_CHEAT_TOGGLE,           MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE),
   DECLARE_META_BIND(2, screenshot,            RARCH_SCREENSHOT,             MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT),
   DECLARE_META_BIND(2, audio_mute,            RARCH_MUTE,                   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE),
   DECLARE_META_BIND(2, osk_toggle,            RARCH_OSK,                    MENU_ENUM_LABEL_VALUE_INPUT_META_OSK),
   DECLARE_META_BIND(2, fps_toggle,            RARCH_FPS_TOGGLE,             MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE),
   DECLARE_META_BIND(2, toggle_statistics,     RARCH_STATISTICS_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE),
   DECLARE_META_BIND(2, netplay_ping_toggle,   RARCH_NETPLAY_PING_TOGGLE,    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE),
   DECLARE_META_BIND(2, send_debug_info,       RARCH_SEND_DEBUG_INFO,        MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO),
   DECLARE_META_BIND(2, netplay_host_toggle,   RARCH_NETPLAY_HOST_TOGGLE,    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE),
   DECLARE_META_BIND(2, netplay_game_watch,    RARCH_NETPLAY_GAME_WATCH,     MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH),
   DECLARE_META_BIND(2, netplay_player_chat,   RARCH_NETPLAY_PLAYER_CHAT,    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT),
   DECLARE_META_BIND(2, netplay_fade_chat_toggle, RARCH_NETPLAY_FADE_CHAT_TOGGLE, MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE),
   DECLARE_META_BIND(2, enable_hotkey,         RARCH_ENABLE_HOTKEY,          MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY),
   DECLARE_META_BIND(2, volume_up,             RARCH_VOLUME_UP,              MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP),
   DECLARE_META_BIND(2, volume_down,           RARCH_VOLUME_DOWN,            MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN),
   DECLARE_META_BIND(2, overlay_next,          RARCH_OVERLAY_NEXT,           MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT),
   DECLARE_META_BIND(2, disk_eject_toggle,     RARCH_DISK_EJECT_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE),
   DECLARE_META_BIND(2, disk_next,             RARCH_DISK_NEXT,              MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT),
   DECLARE_META_BIND(2, disk_prev,             RARCH_DISK_PREV,              MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV),
   DECLARE_META_BIND(2, grab_mouse_toggle,     RARCH_GRAB_MOUSE_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE),
   DECLARE_META_BIND(2, game_focus_toggle,     RARCH_GAME_FOCUS_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE),
   DECLARE_META_BIND(2, desktop_menu_toggle,   RARCH_UI_COMPANION_TOGGLE,    MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE),
#ifdef HAVE_MENU
   DECLARE_META_BIND(1, menu_toggle,           RARCH_MENU_TOGGLE,            MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE),
#endif
   DECLARE_META_BIND(2, recording_toggle,      RARCH_RECORDING_TOGGLE,       MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE),
   DECLARE_META_BIND(2, streaming_toggle,      RARCH_STREAMING_TOGGLE,       MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE),
   DECLARE_META_BIND(2, runahead_toggle,       RARCH_RUNAHEAD_TOGGLE,        MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE),
   DECLARE_META_BIND(2, ai_service,            RARCH_AI_SERVICE,             MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE),
};

#if defined(HAVE_METAL)
/* iOS supports both the OpenGL and Metal video drivers; default to OpenGL since Metal support is preliminary */
#if defined(HAVE_COCOATOUCH) && defined(HAVE_OPENGL)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GL;
#else
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_METAL;
#endif
#elif defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
/* Lets default to D3D11 in UWP, even when its compiled with ANGLE, since ANGLE is just calling D3D anyway.*/
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D11;
#elif defined(HAVE_OPENGL1) && defined(_MSC_VER) && (_MSC_VER <= 1600)
/* On Windows XP and earlier, use gl1 by default
 * (regular opengl has compatibility issues with
 * obsolete hardware drivers...) */
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GL1;
#elif defined(HAVE_VITA2D)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_VITA2D;
#elif defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) || defined(HAVE_PSGL)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GL;
#elif defined(HAVE_OPENGL_CORE) && !defined(__HAIKU__)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GL_CORE;
#elif defined(HAVE_OPENGL1)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GL1;
#elif defined(GEKKO)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_WII;
#elif defined(WIIU)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_WIIU;
#elif defined(XENON)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_XENON360;
#elif defined(HAVE_D3D11)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D11;
#elif defined(HAVE_D3D12)
/* FIXME/WARNING: DX12 performance on Xbox is horrible for
 * some reason. For now, we will default to D3D11 when possible. */
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D12;
#elif defined(HAVE_D3D10)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D10;
#elif defined(HAVE_D3D9)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D9;
#elif defined(HAVE_D3D8)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_D3D8;
#elif defined(HAVE_VG)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_VG;
#elif defined(PSP)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_PSP1;
#elif defined(PS2)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_PS2;
#elif defined(_3DS)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_CTR;
#elif defined(SWITCH)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SWITCH;
#elif defined(HAVE_XVIDEO)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_XVIDEO;
#elif defined(HAVE_SDL) && !defined(HAVE_SDL_DINGUX)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SDL;
#elif defined(HAVE_SDL2)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SDL2;
#elif defined(HAVE_SDL_DINGUX)
#if defined(RS90) || defined(MIYOO)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SDL_RS90;
#else
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_SDL_DINGUX;
#endif
#elif defined(_WIN32) && !defined(_XBOX)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_GDI;
#elif defined(DJGPP)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_VGA;
#elif defined(HAVE_FPGA)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_FPGA;
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_EXT;
#elif defined(__PSL1GHT__)
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_RSX;
#else
static const enum video_driver_enum VIDEO_DEFAULT_DRIVER = VIDEO_NULL;
#endif

#if defined(XENON)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_XENON360;
#elif defined(GEKKO)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_WII;
#elif defined(WIIU)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_WIIU;
#elif defined(PSP) || defined(VITA) || defined(ORBIS)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_PSP;
#elif defined(PS2)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_PS2;
#elif defined(__PS3__)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_PS3;
#elif defined(_3DS)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_CTR;
#elif defined(SWITCH)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SWITCH;
#elif defined(DINGUX_BETA) && defined(HAVE_ALSA)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_ALSA;
#elif defined(DINGUX) && defined(HAVE_AL)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_AL;
#elif defined(HAVE_PULSE)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_PULSE;
#elif defined(HAVE_ALSA) && defined(HAVE_THREADS)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_ALSATHREAD;
#elif defined(HAVE_ALSA)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_ALSA;
#elif defined(HAVE_TINYALSA)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_TINYALSA;
#elif defined(HAVE_AUDIOIO)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_AUDIOIO;
#elif defined(HAVE_OSS)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_OSS;
#elif defined(HAVE_JACK)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_JACK;
#elif defined(HAVE_COREAUDIO3)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_COREAUDIO3;
#elif defined(HAVE_COREAUDIO)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_COREAUDIO;
#elif defined(HAVE_XAUDIO)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_XAUDIO;
#elif defined(HAVE_DSOUND)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_DSOUND;
#elif defined(HAVE_WASAPI)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_WASAPI;
#elif defined(HAVE_AL)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_AL;
#elif defined(HAVE_SL)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SL;
#elif defined(EMSCRIPTEN)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_RWEBAUDIO;
#elif defined(HAVE_SDL)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SDL;
#elif defined(HAVE_SDL2)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_SDL2;
#elif defined(HAVE_RSOUND)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_RSOUND;
#elif defined(HAVE_ROAR)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_ROAR;
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_EXT;
#else
static const enum audio_driver_enum AUDIO_DEFAULT_DRIVER = AUDIO_NULL;
#endif

#if defined(RS90) || defined(MIYOO)
static const enum audio_resampler_driver_enum AUDIO_DEFAULT_RESAMPLER_DRIVER = AUDIO_RESAMPLER_NEAREST;
#elif defined(PSP) || defined(EMSCRIPTEN)
static const enum audio_resampler_driver_enum AUDIO_DEFAULT_RESAMPLER_DRIVER = AUDIO_RESAMPLER_CC;
#else
static const enum audio_resampler_driver_enum AUDIO_DEFAULT_RESAMPLER_DRIVER = AUDIO_RESAMPLER_SINC;
#endif

#if defined(HAVE_FFMPEG)
static const enum record_driver_enum RECORD_DEFAULT_DRIVER = RECORD_FFMPEG;
#else
static const enum record_driver_enum RECORD_DEFAULT_DRIVER = RECORD_NULL;
#endif

#ifdef HAVE_WINMM
static const enum midi_driver_enum MIDI_DEFAULT_DRIVER = MIDI_WINMM;
#elif defined(HAVE_ALSA) && !defined(HAVE_HAKCHI) && !defined(HAVE_SEGAM) && !defined(DINGUX)
static const enum midi_driver_enum MIDI_DEFAULT_DRIVER = MIDI_ALSA;
#else
static const enum midi_driver_enum MIDI_DEFAULT_DRIVER = MIDI_NULL;
#endif

#if defined(HAVE_STEAM) && defined(__linux__) && defined(HAVE_SDL2)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL2;
#elif defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_UWP;
#elif defined(XENON)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_XENON360;
#elif defined(_XBOX360) || defined(_XBOX) || defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_XINPUT;
#elif defined(ANDROID)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_ANDROID;
#elif defined(EMSCRIPTEN) && defined(HAVE_SDL2)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL2;
#elif defined(WEBOS) && defined(HAVE_SDL2)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL2;
#elif defined(EMSCRIPTEN)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_RWEBINPUT;
#elif defined(_WIN32) && defined(HAVE_DINPUT)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_DINPUT;
#elif defined(_WIN32) && !defined(HAVE_DINPUT) && _WIN32_WINNT >= 0x0501
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_WINRAW;
#elif defined(PS2)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_PS2;
#elif defined(__PS3__)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_PS3;
#elif defined(ORBIS)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_PS4;
#elif defined(PSP) || defined(VITA)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_PSP;
#elif defined(_3DS)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_CTR;
#elif defined(SWITCH)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SWITCH;
#elif defined(GEKKO)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_WII;
#elif defined(WIIU)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_WIIU;
#elif defined(DINGUX) && defined(HAVE_SDL_DINGUX)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL_DINGUX;
#elif defined(HAVE_X11)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_X;
#elif defined(HAVE_UDEV)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_UDEV;
#elif defined(__linux__) && !defined(ANDROID)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_LINUXRAW;
#elif defined(HAVE_WAYLAND)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_WAYLAND;
#elif defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_COCOA;
#elif defined(__QNX__)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_QNX;
#elif defined(HAVE_SDL)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL;
#elif defined(HAVE_SDL2)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_SDL2;
#elif defined(DJGPP)
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_DOS;
#else
static const enum input_driver_enum INPUT_DEFAULT_DRIVER = INPUT_NULL;
#endif

#if defined(HAVE_STEAM) && defined(__linux__) && defined(HAVE_SDL2)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_SDL;
#elif defined(HAVE_XINPUT)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_XINPUT;
#elif defined(GEKKO)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_GX;
#elif defined(WIIU)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_WIIU;
#elif defined(WEBOS)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_SDL;
#elif defined(_XBOX)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_XDK;
#elif defined(PS2)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_PS2;
#elif defined(__PS3__) || defined(__PSL1GHT__)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_PS3;
#elif defined(ORBIS)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_PS4;
#elif defined(PSP) || defined(VITA)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_PSP;
#elif defined(_3DS)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_CTR;
#elif defined(SWITCH)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_SWITCH;
#elif defined(DINGUX) && defined(HAVE_SDL_DINGUX)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_SDL_DINGUX;
#elif defined(HAVE_DINPUT)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_DINPUT;
#elif defined(HAVE_UDEV)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_UDEV;
#elif defined(__linux) && !defined(ANDROID)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_LINUXRAW;
#elif defined(ANDROID)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_ANDROID;
#elif defined(HAVE_SDL) || defined(HAVE_SDL2)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_SDL;
#elif defined(DJGPP)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_DOS;
#elif defined(IOS)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_MFI;
#elif defined(HAVE_HID)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_HID;
#elif defined(__QNX__)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_QNX;
#elif defined(EMSCRIPTEN)
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_RWEBPAD;
#else
static const enum joypad_driver_enum JOYPAD_DEFAULT_DRIVER = JOYPAD_NULL;
#endif

#if defined(HAVE_V4L2)
static const enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_V4L2;
#elif defined(EMSCRIPTEN)
static const enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_RWEBCAM;
#elif defined(ANDROID)
static const enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_ANDROID;
#else
static const enum camera_driver_enum CAMERA_DEFAULT_DRIVER = CAMERA_NULL;
#endif

#if defined(HAVE_BLUETOOTH)
# if defined(HAVE_DBUS)
static const enum bluetooth_driver_enum BLUETOOTH_DEFAULT_DRIVER = BLUETOOTH_BLUEZ;
# else
static const enum bluetooth_driver_enum BLUETOOTH_DEFAULT_DRIVER = BLUETOOTH_BLUETOOTHCTL;
# endif
#else
static const enum bluetooth_driver_enum BLUETOOTH_DEFAULT_DRIVER = BLUETOOTH_NULL;
#endif

#if defined(HAVE_LAKKA)
static const enum wifi_driver_enum WIFI_DEFAULT_DRIVER = WIFI_CONNMANCTL;
#else
static const enum wifi_driver_enum WIFI_DEFAULT_DRIVER = WIFI_NULL;
#endif

#if defined(ANDROID)
static const enum location_driver_enum LOCATION_DEFAULT_DRIVER = LOCATION_ANDROID;
#else
static const enum location_driver_enum LOCATION_DEFAULT_DRIVER = LOCATION_NULL;
#endif

#if (defined(_3DS) || defined(DINGUX)) && defined(HAVE_RGUI)
static const enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_RGUI;
#elif defined(HAVE_MATERIALUI) && defined(RARCH_MOBILE)
static const enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_MATERIALUI;
#elif defined(HAVE_OZONE)
static const enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_OZONE;
#elif defined(HAVE_XMB) && !defined(_XBOX)
static const enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_XMB;
#elif defined(HAVE_RGUI)
static const enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_RGUI;
#else
static const enum menu_driver_enum MENU_DEFAULT_DRIVER = MENU_NULL;
#endif

/* All config related settings go here. */

struct config_bool_setting
{
   const char *ident;
   bool *ptr;
   enum rarch_override_setting override;
   bool def_enable;
   bool def;
   bool handle;
};

struct config_int_setting
{
   const char *ident;
   int *ptr;
   int def;
   enum rarch_override_setting override;
   bool def_enable;
   bool handle;
};

struct config_uint_setting
{
   const char *ident;
   unsigned *ptr;
   unsigned def;
   enum rarch_override_setting override;
   bool def_enable;
   bool handle;
};

struct config_size_setting
{
   const char *ident;
   size_t *ptr;
   size_t def;
   enum rarch_override_setting override;
   bool def_enable;
   bool handle;
};

struct config_float_setting
{
   const char *ident;
   float *ptr;
   float def;
   enum rarch_override_setting override;
   bool def_enable;
   bool handle;
};

struct config_array_setting
{
   const char *ident;
   const char *def;
   char *ptr;
   enum rarch_override_setting override;
   bool def_enable;
   bool handle;
};

struct config_path_setting
{
   const char *ident;
   char *ptr;
   char *def;
   bool def_enable;
   bool handle;
};


#define GENERAL_SETTING(key, configval, default_enable, default_setting, type, handle_setting) \
{ \
   tmp[count].ident      = key; \
   tmp[count].ptr        = configval; \
   tmp[count].def_enable = default_enable; \
   if (default_enable) \
      tmp[count].def     = default_setting; \
   tmp[count].handle   = handle_setting; \
   count++; \
}

#define SETTING_BOOL(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_bool_setting, handle_setting)

#define SETTING_FLOAT(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_float_setting, handle_setting)

#define SETTING_INT(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_int_setting, handle_setting)

#define SETTING_UINT(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_uint_setting, handle_setting)

#define SETTING_SIZE(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_size_setting, handle_setting)

#define SETTING_PATH(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_path_setting, handle_setting)

#define SETTING_ARRAY(key, configval, default_enable, default_setting, handle_setting) \
   GENERAL_SETTING(key, configval, default_enable, default_setting, struct config_array_setting, handle_setting)

#define SETTING_OVERRIDE(override_setting) \
   tmp[count-1].override = override_setting

/* Forward declarations */
#ifdef HAVE_CONFIGFILE
static void config_parse_file(global_t *global);
#endif

struct defaults g_defaults;

static settings_t *config_st = NULL;

settings_t *config_get_ptr(void)
{
   return config_st;
}

/**
 * config_get_default_audio:
 *
 * Gets default audio driver.
 *
 * Returns: Default audio driver.
 **/
const char *config_get_default_audio(void)
{
   enum audio_driver_enum default_driver = AUDIO_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case AUDIO_RSOUND:
         return "rsound";
      case AUDIO_AUDIOIO:
         return "audioio";
      case AUDIO_OSS:
         return "oss";
      case AUDIO_ALSA:
         return "alsa";
      case AUDIO_ALSATHREAD:
         return "alsathread";
      case AUDIO_TINYALSA:
         return "tinyalsa";
      case AUDIO_ROAR:
         return "roar";
      case AUDIO_COREAUDIO:
         return "coreaudio";
      case AUDIO_COREAUDIO3:
         return "coreaudio3";
      case AUDIO_AL:
         return "openal";
      case AUDIO_SL:
         return "opensl";
      case AUDIO_SDL:
         return "sdl";
      case AUDIO_SDL2:
         return "sdl2";
      case AUDIO_DSOUND:
         return "dsound";
      case AUDIO_WASAPI:
         return "wasapi";
      case AUDIO_XAUDIO:
         return "xaudio";
      case AUDIO_PULSE:
         return "pulse";
      case AUDIO_EXT:
         return "ext";
      case AUDIO_XENON360:
         return "xenon360";
      case AUDIO_PS3:
         return "ps3";
      case AUDIO_WII:
         return "gx";
      case AUDIO_WIIU:
         return "AX";
      case AUDIO_PSP:
#if defined(VITA)
         return "vita";
#elif defined(ORBIS)
         return "orbis";
#else
         return "psp";
#endif
      case AUDIO_PS2:
         return "ps2";
      case AUDIO_CTR:
         return "dsp";
      case AUDIO_SWITCH:
#if defined(HAVE_LIBNX)
         return "switch_audren_thread";
#else
         return "switch";
#endif
      case AUDIO_RWEBAUDIO:
         return "rwebaudio";
      case AUDIO_JACK:
         return "jack";
      case AUDIO_NULL:
         break;
   }

   return "null";
}

const char *config_get_default_record(void)
{
   enum record_driver_enum default_driver = RECORD_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case RECORD_FFMPEG:
         return "ffmpeg";
      case RECORD_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_audio_resampler:
 *
 * Gets default audio resampler driver.
 *
 * Returns: Default audio resampler driver.
 **/
const char *config_get_default_audio_resampler(void)
{
   enum audio_resampler_driver_enum default_driver = AUDIO_DEFAULT_RESAMPLER_DRIVER;

   switch (default_driver)
   {
      case AUDIO_RESAMPLER_CC:
         return "cc";
      case AUDIO_RESAMPLER_SINC:
         return "sinc";
      case AUDIO_RESAMPLER_NEAREST:
         return "nearest";
      case AUDIO_RESAMPLER_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_video:
 *
 * Gets default video driver.
 *
 * Returns: Default video driver.
 **/
const char *config_get_default_video(void)
{
   enum video_driver_enum default_driver = VIDEO_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case VIDEO_GL:
         return "gl";
      case VIDEO_GL1:
         return "gl1";
      case VIDEO_GL_CORE:
         return "glcore";
      case VIDEO_VULKAN:
         return "vulkan";
      case VIDEO_METAL:
         return "metal";
      case VIDEO_DRM:
         return "drm";
      case VIDEO_WII:
         return "gx";
      case VIDEO_WIIU:
         return "gx2";
      case VIDEO_XENON360:
         return "xenon360";
      case VIDEO_D3D8:
         return "d3d8";
      case VIDEO_D3D9:
         return "d3d9";
      case VIDEO_D3D10:
         return "d3d10";
      case VIDEO_D3D11:
         return "d3d11";
      case VIDEO_D3D12:
         return "d3d12";
      case VIDEO_PSP1:
         return "psp1";
      case VIDEO_PS2:
         return "ps2";
      case VIDEO_VITA2D:
         return "vita2d";
      case VIDEO_CTR:
         return "ctr";
      case VIDEO_SWITCH:
         return "switch";
      case VIDEO_XVIDEO:
         return "xvideo";
      case VIDEO_SDL_DINGUX:
         return "sdl_dingux";
      case VIDEO_SDL_RS90:
         return "sdl_rs90";
      case VIDEO_SDL:
         return "sdl";
      case VIDEO_SDL2:
         return "sdl2";
      case VIDEO_EXT:
         return "ext";
      case VIDEO_VG:
         return "vg";
      case VIDEO_OMAP:
         return "omap";
      case VIDEO_EXYNOS:
         return "exynos";
      case VIDEO_DISPMANX:
         return "dispmanx";
      case VIDEO_SUNXI:
         return "sunxi";
      case VIDEO_CACA:
         return "caca";
      case VIDEO_GDI:
         return "gdi";
      case VIDEO_VGA:
         return "vga";
      case VIDEO_FPGA:
         return "fpga";
      case VIDEO_RSX:
         return "rsx";
      case VIDEO_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_input:
 *
 * Gets default input driver.
 *
 * Returns: Default input driver.
 **/
const char *config_get_default_input(void)
{
   enum input_driver_enum default_driver = INPUT_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case INPUT_ANDROID:
         return "android";
      case INPUT_PS4:
         return "ps4";
      case INPUT_PS3:
         return "ps3";
      case INPUT_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case INPUT_PS2:
         return "ps2";
      case INPUT_CTR:
         return "ctr";
      case INPUT_SWITCH:
         return "switch";
      case INPUT_SDL:
         return "sdl";
      case INPUT_SDL2:
         return "sdl2";
      case INPUT_SDL_DINGUX:
         return "sdl_dingux";
      case INPUT_DINPUT:
         return "dinput";
      case INPUT_WINRAW:
         return "raw";
      case INPUT_X:
         return "x";
      case INPUT_WAYLAND:
         return "wayland";
      case INPUT_XENON360:
         return "xenon360";
      case INPUT_XINPUT:
         return "xinput";
      case INPUT_UWP:
         return "uwp";
      case INPUT_WII:
         return "gx";
      case INPUT_WIIU:
         return "wiiu";
      case INPUT_LINUXRAW:
         return "linuxraw";
      case INPUT_UDEV:
         return "udev";
      case INPUT_COCOA:
         return "cocoa";
      case INPUT_QNX:
          return "qnx_input";
      case INPUT_RWEBINPUT:
          return "rwebinput";
      case INPUT_DOS:
         return "dos";
      case INPUT_NULL:
          break;
   }

   return "null";
}

/**
 * config_get_default_joypad:
 *
 * Gets default input joypad driver.
 *
 * Returns: Default input joypad driver.
 **/
const char *config_get_default_joypad(void)
{
   enum joypad_driver_enum default_driver = JOYPAD_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case JOYPAD_PS4:
         return "ps4";
      case JOYPAD_PS3:
         return "ps3";
      case JOYPAD_XINPUT:
         return "xinput";
      case JOYPAD_GX:
         return "gx";
      case JOYPAD_WIIU:
         return "wiiu";
      case JOYPAD_XDK:
         return "xdk";
      case JOYPAD_PSP:
#ifdef VITA
         return "vita";
#else
         return "psp";
#endif
      case JOYPAD_PS2:
         return "ps2";
      case JOYPAD_CTR:
         return "ctr";
      case JOYPAD_SWITCH:
         return "switch";
      case JOYPAD_DINPUT:
         return "dinput";
      case JOYPAD_UDEV:
         return "udev";
      case JOYPAD_LINUXRAW:
         return "linuxraw";
      case JOYPAD_ANDROID:
         return "android";
      case JOYPAD_SDL:
#ifdef HAVE_SDL2
         return "sdl2";
#else
         return "sdl";
#endif
      case JOYPAD_SDL_DINGUX:
         return "sdl_dingux";
      case JOYPAD_HID:
         return "hid";
      case JOYPAD_QNX:
         return "qnx";
      case JOYPAD_RWEBPAD:
         return "rwebpad";
      case JOYPAD_DOS:
         return "dos";
      case JOYPAD_MFI:
         return "mfi";
      case JOYPAD_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_camera:
 *
 * Gets default camera driver.
 *
 * Returns: Default camera driver.
 **/
const char *config_get_default_camera(void)
{
   enum camera_driver_enum default_driver = CAMERA_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case CAMERA_V4L2:
         return "video4linux2";
      case CAMERA_RWEBCAM:
         return "rwebcam";
      case CAMERA_ANDROID:
         return "android";
      case CAMERA_AVFOUNDATION:
         return "avfoundation";
      case CAMERA_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_bluetooth:
 *
 * Gets default bluetooth driver.
 *
 * Returns: Default bluetooth driver.
 **/
const char *config_get_default_bluetooth(void)
{
   enum bluetooth_driver_enum default_driver = BLUETOOTH_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case BLUETOOTH_BLUETOOTHCTL:
         return "bluetoothctl";
      case BLUETOOTH_BLUEZ:
         return "bluez";
      case BLUETOOTH_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_wifi:
 *
 * Gets default wifi driver.
 *
 * Returns: Default wifi driver.
 **/
const char *config_get_default_wifi(void)
{
   enum wifi_driver_enum default_driver = WIFI_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case WIFI_CONNMANCTL:
         return "connmanctl";
      case WIFI_NMCLI:
         return "nmcli";
      case WIFI_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_led:
 *
 * Gets default led driver.
 *
 * Returns: Default led driver.
 **/
const char *config_get_default_led(void)
{
   return "null";
}

/**
 * config_get_default_location:
 *
 * Gets default location driver.
 *
 * Returns: Default location driver.
 **/
const char *config_get_default_location(void)
{
   enum location_driver_enum default_driver = LOCATION_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case LOCATION_ANDROID:
         return "android";
      case LOCATION_CORELOCATION:
         return "corelocation";
      case LOCATION_NULL:
         break;
   }

   return "null";
}

/**
 * config_get_default_menu:
 *
 * Gets default menu driver.
 *
 * Returns: Default menu driver.
 **/
const char *config_get_default_menu(void)
{
#ifdef HAVE_MENU
   enum menu_driver_enum default_driver = MENU_DEFAULT_DRIVER;

   if (!string_is_empty(g_defaults.settings_menu))
      return g_defaults.settings_menu;

   switch (default_driver)
   {
      case MENU_RGUI:
         return "rgui";
      case MENU_OZONE:
         return "ozone";
      case MENU_MATERIALUI:
         return "glui";
      case MENU_XMB:
         return "xmb";
      case MENU_STRIPES:
         return "stripes";
      case MENU_NULL:
         break;
   }
#endif

   return "null";
}

const char *config_get_default_midi(void)
{
   enum midi_driver_enum default_driver = MIDI_DEFAULT_DRIVER;

   switch (default_driver)
   {
      case MIDI_WINMM:
         return "winmm";
      case MIDI_ALSA:
         return "alsa";
      case MIDI_NULL:
         break;
   }

   return "null";
}

const char *config_get_midi_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MIDI_DRIVERS, NULL);
}

#ifdef HAVE_LAKKA
void config_set_timezone(char *timezone)
{
   setenv("TZ", timezone, 1);
   tzset();
}

const char *config_get_all_timezones(void)
{
   return char_list_new_special(STRING_LIST_TIMEZONES, NULL);
}

static void load_timezone(char *setting)
{
   char haystack[TIMEZONE_LENGTH+32];
   static char *needle = "TIMEZONE=";
   size_t needle_len = strlen(needle);

   RFILE *tzfp = filestream_open(LAKKA_TIMEZONE_PATH,
                       RETRO_VFS_FILE_ACCESS_READ,
                       RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (tzfp != NULL)
   {
      filestream_gets(tzfp, haystack, sizeof(haystack)-1);
      filestream_close(tzfp);

      char *start = strstr(haystack, needle);

      if (start != NULL)
         snprintf(setting, TIMEZONE_LENGTH, "%s", start + needle_len);
      else
         snprintf(setting, TIMEZONE_LENGTH, "%s", DEFAULT_TIMEZONE);
   }
   else
      snprintf(setting, TIMEZONE_LENGTH, "%s", DEFAULT_TIMEZONE);

   config_set_timezone(setting);
}
#endif

bool config_overlay_enable_default(void)
{
   if (g_defaults.overlay_set)
      return g_defaults.overlay_enable;
   return true;
}

static struct config_array_setting *populate_settings_array(settings_t *settings, int *size)
{
   unsigned count                        = 0;
   struct config_array_setting  *tmp    = (struct config_array_setting*)calloc(1, (*size + 1) * sizeof(struct config_array_setting));

   if (!tmp)
      return NULL;

   /* Arrays */
   SETTING_ARRAY("video_driver",             settings->arrays.video_driver,   false, NULL, true);
   SETTING_ARRAY("record_driver",            settings->arrays.record_driver,  false, NULL, true);
   SETTING_ARRAY("camera_driver",            settings->arrays.camera_driver,  false, NULL, true);
   SETTING_ARRAY("bluetooth_driver",         settings->arrays.bluetooth_driver, false, NULL, true);
   SETTING_ARRAY("wifi_driver",              settings->arrays.wifi_driver,    false, NULL, true);
   SETTING_ARRAY("location_driver",          settings->arrays.location_driver,false, NULL, true);
#ifdef HAVE_MENU
   SETTING_ARRAY("menu_driver",              settings->arrays.menu_driver,    false, NULL, true);
#endif
   SETTING_ARRAY("audio_device",             settings->arrays.audio_device,   false, NULL, true);
   SETTING_ARRAY("camera_device",            settings->arrays.camera_device,  false, NULL, true);
#ifdef HAVE_CHEEVOS
   SETTING_ARRAY("cheevos_custom_host",      settings->arrays.cheevos_custom_host, false, NULL, true);
   SETTING_ARRAY("cheevos_username",         settings->arrays.cheevos_username, false, NULL, true);
   SETTING_ARRAY("cheevos_password",         settings->arrays.cheevos_password, false, NULL, true);
   SETTING_ARRAY("cheevos_token",            settings->arrays.cheevos_token, false, NULL, true);
   SETTING_ARRAY("cheevos_leaderboards_enable", settings->arrays.cheevos_leaderboards_enable, true, "true", true);
#endif
   SETTING_ARRAY("video_context_driver",     settings->arrays.video_context_driver,   false, NULL, true);
   SETTING_ARRAY("audio_driver",             settings->arrays.audio_driver,           false, NULL, true);
   SETTING_ARRAY("audio_resampler",          settings->arrays.audio_resampler,        false, NULL, true);
   SETTING_ARRAY("input_driver",             settings->arrays.input_driver,           false, NULL, true);
   SETTING_ARRAY("input_joypad_driver",      settings->arrays.input_joypad_driver,    false, NULL, true);
   SETTING_ARRAY("input_keyboard_layout",    settings->arrays.input_keyboard_layout,  false, NULL, true);
   SETTING_ARRAY("bundle_assets_src_path",   settings->arrays.bundle_assets_src, false, NULL, true);
   SETTING_ARRAY("bundle_assets_dst_path",   settings->arrays.bundle_assets_dst, false, NULL, true);
   SETTING_ARRAY("bundle_assets_dst_path_subdir", settings->arrays.bundle_assets_dst_subdir, false, NULL, true);
   SETTING_ARRAY("led_driver",               settings->arrays.led_driver, false, NULL, true);
   SETTING_ARRAY("netplay_mitm_server",      settings->arrays.netplay_mitm_server, false, NULL, true);
   SETTING_ARRAY("midi_driver",              settings->arrays.midi_driver, false, NULL, true);
   SETTING_ARRAY("midi_input",               settings->arrays.midi_input, true, DEFAULT_MIDI_INPUT, true);
   SETTING_ARRAY("midi_output",              settings->arrays.midi_output, true, DEFAULT_MIDI_OUTPUT, true);
   SETTING_ARRAY("youtube_stream_key",       settings->arrays.youtube_stream_key, true, NULL, true);
   SETTING_ARRAY("twitch_stream_key",       settings->arrays.twitch_stream_key, true, NULL, true);
   SETTING_ARRAY("facebook_stream_key",      settings->arrays.facebook_stream_key, true, NULL, true);
   SETTING_ARRAY("discord_app_id",           settings->arrays.discord_app_id, true, DEFAULT_DISCORD_APP_ID, true);
   SETTING_ARRAY("ai_service_url",           settings->arrays.ai_service_url, true, DEFAULT_AI_SERVICE_URL, true);
   SETTING_ARRAY("crt_switch_timings",       settings->arrays.crt_switch_timings, false, NULL, true);
#ifdef HAVE_LAKKA
   SETTING_ARRAY("cpu_main_gov",             settings->arrays.cpu_main_gov, false, NULL, true);
   SETTING_ARRAY("cpu_menu_gov",             settings->arrays.cpu_menu_gov, false, NULL, true);
#endif

   *size = count;

   return tmp;
}

static struct config_path_setting *populate_settings_path(
      settings_t *settings, int *size)
{
   unsigned count = 0;
   recording_state_t *recording_st     = recording_state_get_ptr();
   struct config_path_setting  *tmp    = (struct config_path_setting*)calloc(1, (*size + 1) * sizeof(struct config_path_setting));

   if (!tmp)
      return NULL;

   /* Paths */
#ifdef HAVE_XMB
   SETTING_PATH("xmb_font",                   settings->paths.path_menu_xmb_font, false, NULL, true);
#endif
   SETTING_PATH("content_show_settings_password", settings->paths.menu_content_show_settings_password, false, NULL, true);
   SETTING_PATH("kiosk_mode_password",        settings->paths.kiosk_mode_password, false, NULL, true);
   SETTING_PATH("netplay_nickname",           settings->paths.username, false, NULL, true);
   SETTING_PATH("video_filter",               settings->paths.path_softfilter_plugin, false, NULL, true);
   SETTING_PATH("audio_dsp_plugin",           settings->paths.path_audio_dsp_plugin, false, NULL, true);
   SETTING_PATH("core_updater_buildbot_cores_url", settings->paths.network_buildbot_url, false, NULL, true);
   SETTING_PATH("core_updater_buildbot_assets_url", settings->paths.network_buildbot_assets_url, false, NULL, true);
#ifdef HAVE_NETWORKING
   SETTING_PATH("netplay_ip_address",         settings->paths.netplay_server, false, NULL, true);
   SETTING_PATH("netplay_custom_mitm_server", settings->paths.netplay_custom_mitm_server, false, NULL, true);
   SETTING_PATH("netplay_password",           settings->paths.netplay_password, false, NULL, true);
   SETTING_PATH("netplay_spectate_password",  settings->paths.netplay_spectate_password, false, NULL, true);
#endif
   SETTING_PATH("libretro_directory",
         settings->paths.directory_libretro, false, NULL, false);
   SETTING_PATH("core_options_path",
         settings->paths.path_core_options, false, NULL, true);
   SETTING_PATH("libretro_info_path",
         settings->paths.path_libretro_info, false, NULL, true);
   SETTING_PATH("content_database_path",
         settings->paths.path_content_database, false, NULL, true);
   SETTING_PATH("cheat_database_path",
         settings->paths.path_cheat_database, false, NULL, true);
#ifdef HAVE_MENU
   SETTING_PATH("menu_wallpaper",
         settings->paths.path_menu_wallpaper, false, NULL, true);
   SETTING_PATH("rgui_menu_theme_preset",
         settings->paths.path_rgui_theme_preset, false, NULL, true);
#endif
   SETTING_PATH("content_favorites_path",
         settings->paths.path_content_favorites, false, NULL, true);
   SETTING_PATH("content_history_path",
         settings->paths.path_content_history, false, NULL, true);
   SETTING_PATH("content_image_history_path",
         settings->paths.path_content_image_history, false, NULL, true);
   SETTING_PATH("content_music_history_path",
         settings->paths.path_content_music_history, false, NULL, true);
   SETTING_PATH("content_video_history_path",
         settings->paths.path_content_video_history, false, NULL, true);
#ifdef HAVE_OVERLAY
   SETTING_PATH("input_overlay",
         settings->paths.path_overlay, false, NULL, true);
#endif
#ifdef HAVE_VIDEO_LAYOUT
   SETTING_PATH("video_layout_path",
         settings->paths.path_video_layout, false, NULL, true);
   SETTING_PATH("video_layout_directory",
         settings->paths.directory_video_layout, true, NULL, true);
#endif
   SETTING_PATH("video_record_config",
         settings->paths.path_record_config, false, NULL, true);
   SETTING_PATH("video_stream_config",
         settings->paths.path_stream_config, false, NULL, true);
   SETTING_PATH("video_stream_url",
         settings->paths.path_stream_url, false, NULL, true);
   SETTING_PATH("video_font_path",
         settings->paths.path_font, false, NULL, true);
   SETTING_PATH("cursor_directory",
         settings->paths.directory_cursor, false, NULL, true);
   SETTING_PATH("screenshot_directory",
         settings->paths.directory_screenshot, true, NULL, true);
   SETTING_PATH("system_directory",
         settings->paths.directory_system, true, NULL, true);
   SETTING_PATH("cache_directory",
         settings->paths.directory_cache, false, NULL, true);
   SETTING_PATH("input_remapping_directory",
         settings->paths.directory_input_remapping, false, NULL, true);
   SETTING_PATH("resampler_directory",
         settings->paths.directory_resampler, false, NULL, true);
   SETTING_PATH("video_shader_dir",
         settings->paths.directory_video_shader, true, NULL, true);
   SETTING_PATH("video_filter_dir",
         settings->paths.directory_video_filter, true, NULL, true);
   SETTING_PATH("core_assets_directory",
         settings->paths.directory_core_assets, true, NULL, true);
   SETTING_PATH("assets_directory",
         settings->paths.directory_assets, true, NULL, true);
   SETTING_PATH("dynamic_wallpapers_directory",
         settings->paths.directory_dynamic_wallpapers, true, NULL, true);
   SETTING_PATH("thumbnails_directory",
         settings->paths.directory_thumbnails, true, NULL, true);
   SETTING_PATH("playlist_directory",
         settings->paths.directory_playlist, true, NULL, true);
   SETTING_PATH("content_favorites_directory",
         settings->paths.directory_content_favorites, true, NULL, true);
   SETTING_PATH("content_history_directory",
         settings->paths.directory_content_history, true, NULL, true);
   SETTING_PATH("content_image_history_directory",
         settings->paths.directory_content_image_history, true, NULL, true);
   SETTING_PATH("content_music_history_directory",
         settings->paths.directory_content_music_history, true, NULL, true);
   SETTING_PATH("content_video_directory",
         settings->paths.directory_content_video_history, true, NULL, true);
   SETTING_PATH("runtime_log_directory",
         settings->paths.directory_runtime_log, true, NULL, true);
   SETTING_PATH("joypad_autoconfig_dir",
         settings->paths.directory_autoconfig, false, NULL, true);
   SETTING_PATH("audio_filter_dir",
         settings->paths.directory_audio_filter, true, NULL, true);
   SETTING_PATH("savefile_directory",
         dir_get_ptr(RARCH_DIR_SAVEFILE), true, NULL, false);
   SETTING_PATH("savestate_directory",
         dir_get_ptr(RARCH_DIR_SAVESTATE), true, NULL, false);
#ifdef HAVE_MENU
   SETTING_PATH("rgui_browser_directory",
         settings->paths.directory_menu_content, true, NULL, true);
   SETTING_PATH("rgui_config_directory",
         settings->paths.directory_menu_config, true, NULL, true);
#endif
#ifdef HAVE_OVERLAY
   SETTING_PATH("overlay_directory",
         settings->paths.directory_overlay, true, NULL, true);
#endif
#ifdef HAVE_VIDEO_LAYOUT
   SETTING_PATH("video_layout_directory",
         settings->paths.directory_video_layout, true, NULL, true);
#endif
   SETTING_PATH(
         "screenshot_directory",
         settings->paths.directory_screenshot, true, NULL, false);

   SETTING_PATH("recording_output_directory",
         recording_st->output_dir, false, NULL, true);
   SETTING_PATH("recording_config_directory",
         recording_st->config_dir, false, NULL, true);

   SETTING_ARRAY("log_dir", settings->paths.log_dir, true, NULL, true);

   *size = count;

   return tmp;
}

static struct config_bool_setting *populate_settings_bool(
      settings_t *settings, int *size)
{
   struct config_bool_setting  *tmp    = (struct config_bool_setting*)calloc(1, (*size + 1) * sizeof(struct config_bool_setting));
   unsigned count                      = 0;

   SETTING_BOOL("accessibility_enable", &settings->bools.accessibility_enable, true, DEFAULT_ACCESSIBILITY_ENABLE, false);
   SETTING_BOOL("driver_switch_enable", &settings->bools.driver_switch_enable, true, DEFAULT_DRIVER_SWITCH_ENABLE, false);
   SETTING_BOOL("frame_time_counter_reset_after_fastforwarding", &settings->bools.frame_time_counter_reset_after_fastforwarding, true, false, false);
   SETTING_BOOL("frame_time_counter_reset_after_load_state", &settings->bools.frame_time_counter_reset_after_load_state, true, false, false);
   SETTING_BOOL("frame_time_counter_reset_after_save_state", &settings->bools.frame_time_counter_reset_after_save_state, true, false, false);
   SETTING_BOOL("crt_switch_resolution_use_custom_refresh_rate", &settings->bools.crt_switch_custom_refresh_enable, true, false, false);
   SETTING_BOOL("crt_switch_hires_menu", &settings->bools.crt_switch_hires_menu, true, false, true);
   SETTING_BOOL("ui_companion_start_on_boot",    &settings->bools.ui_companion_start_on_boot, true, ui_companion_start_on_boot, false);
   SETTING_BOOL("ui_companion_enable",           &settings->bools.ui_companion_enable, true, ui_companion_enable, false);
   SETTING_BOOL("ui_companion_toggle",           &settings->bools.ui_companion_toggle, false, ui_companion_toggle, false);
   SETTING_BOOL("desktop_menu_enable",           &settings->bools.desktop_menu_enable, true, DEFAULT_DESKTOP_MENU_ENABLE, false);
   SETTING_BOOL("video_gpu_record",              &settings->bools.video_gpu_record, true, DEFAULT_GPU_RECORD, false);
   SETTING_BOOL("input_remap_binds_enable",      &settings->bools.input_remap_binds_enable, true, true, false);
   SETTING_BOOL("all_users_control_menu",        &settings->bools.input_all_users_control_menu, true, DEFAULT_ALL_USERS_CONTROL_MENU, false);
   SETTING_BOOL("menu_swap_ok_cancel_buttons",   &settings->bools.input_menu_swap_ok_cancel_buttons, true, DEFAULT_MENU_SWAP_OK_CANCEL_BUTTONS, false);
#ifdef HAVE_NETWORKING
   SETTING_BOOL("netplay_show_only_connectable", &settings->bools.netplay_show_only_connectable, true, DEFAULT_NETPLAY_SHOW_ONLY_CONNECTABLE, false);
   SETTING_BOOL("netplay_public_announce",       &settings->bools.netplay_public_announce, true, DEFAULT_NETPLAY_PUBLIC_ANNOUNCE, false);
   SETTING_BOOL("netplay_start_as_spectator",    &settings->bools.netplay_start_as_spectator, false, netplay_start_as_spectator, false);
   SETTING_BOOL("netplay_fade_chat",             &settings->bools.netplay_fade_chat, true, netplay_fade_chat, false);
   SETTING_BOOL("netplay_allow_pausing",         &settings->bools.netplay_allow_pausing, true, netplay_allow_pausing, false);
   SETTING_BOOL("netplay_allow_slaves",          &settings->bools.netplay_allow_slaves, true, netplay_allow_slaves, false);
   SETTING_BOOL("netplay_require_slaves",        &settings->bools.netplay_require_slaves, true, netplay_require_slaves, false);
   SETTING_BOOL("netplay_stateless_mode",        &settings->bools.netplay_stateless_mode, true, netplay_stateless_mode, false);
   SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE);
   SETTING_BOOL("netplay_use_mitm_server",       &settings->bools.netplay_use_mitm_server, true, netplay_use_mitm_server, false);
   SETTING_BOOL("netplay_request_device_p1",     &settings->bools.netplay_request_devices[0], true, false, false);
   SETTING_BOOL("netplay_request_device_p2",     &settings->bools.netplay_request_devices[1], true, false, false);
   SETTING_BOOL("netplay_request_device_p3",     &settings->bools.netplay_request_devices[2], true, false, false);
   SETTING_BOOL("netplay_request_device_p4",     &settings->bools.netplay_request_devices[3], true, false, false);
   SETTING_BOOL("netplay_request_device_p5",     &settings->bools.netplay_request_devices[4], true, false, false);
   SETTING_BOOL("netplay_request_device_p6",     &settings->bools.netplay_request_devices[5], true, false, false);
   SETTING_BOOL("netplay_request_device_p7",     &settings->bools.netplay_request_devices[6], true, false, false);
   SETTING_BOOL("netplay_request_device_p8",     &settings->bools.netplay_request_devices[7], true, false, false);
   SETTING_BOOL("netplay_request_device_p9",     &settings->bools.netplay_request_devices[8], true, false, false);
   SETTING_BOOL("netplay_request_device_p10",    &settings->bools.netplay_request_devices[9], true, false, false);
   SETTING_BOOL("netplay_request_device_p11",    &settings->bools.netplay_request_devices[10], true, false, false);
   SETTING_BOOL("netplay_request_device_p12",    &settings->bools.netplay_request_devices[11], true, false, false);
   SETTING_BOOL("netplay_request_device_p13",    &settings->bools.netplay_request_devices[12], true, false, false);
   SETTING_BOOL("netplay_request_device_p14",    &settings->bools.netplay_request_devices[13], true, false, false);
   SETTING_BOOL("netplay_request_device_p15",    &settings->bools.netplay_request_devices[14], true, false, false);
   SETTING_BOOL("netplay_request_device_p16",    &settings->bools.netplay_request_devices[15], true, false, false);
   SETTING_BOOL("netplay_ping_show",             &settings->bools.netplay_ping_show, true, DEFAULT_NETPLAY_PING_SHOW, false);
   SETTING_BOOL("network_on_demand_thumbnails",  &settings->bools.network_on_demand_thumbnails, true, DEFAULT_NETWORK_ON_DEMAND_THUMBNAILS, false);
#endif
   SETTING_BOOL("input_descriptor_label_show",   &settings->bools.input_descriptor_label_show, true, input_descriptor_label_show, false);
   SETTING_BOOL("input_descriptor_hide_unbound", &settings->bools.input_descriptor_hide_unbound, true, input_descriptor_hide_unbound, false);
   SETTING_BOOL("load_dummy_on_core_shutdown",   &settings->bools.load_dummy_on_core_shutdown, true, DEFAULT_LOAD_DUMMY_ON_CORE_SHUTDOWN, false);
   SETTING_BOOL("check_firmware_before_loading", &settings->bools.check_firmware_before_loading, true, DEFAULT_CHECK_FIRMWARE_BEFORE_LOADING, false);
   SETTING_BOOL("core_option_category_enable",   &settings->bools.core_option_category_enable, true, DEFAULT_CORE_OPTION_CATEGORY_ENABLE, false);
#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   SETTING_BOOL("core_info_cache_enable",        &settings->bools.core_info_cache_enable, false, DEFAULT_CORE_INFO_CACHE_ENABLE, false);
#else
   SETTING_BOOL("core_info_cache_enable",        &settings->bools.core_info_cache_enable, true, DEFAULT_CORE_INFO_CACHE_ENABLE, false);
#endif
#ifndef HAVE_DYNAMIC
   SETTING_BOOL("always_reload_core_on_run_content", &settings->bools.always_reload_core_on_run_content, true, DEFAULT_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT, false);
#endif
   SETTING_BOOL("builtin_mediaplayer_enable",    &settings->bools.multimedia_builtin_mediaplayer_enable, true, DEFAULT_BUILTIN_MEDIAPLAYER_ENABLE, false);
   SETTING_BOOL("builtin_imageviewer_enable",    &settings->bools.multimedia_builtin_imageviewer_enable, true, DEFAULT_BUILTIN_IMAGEVIEWER_ENABLE, false);
   SETTING_BOOL("fps_show",                      &settings->bools.video_fps_show, true, DEFAULT_FPS_SHOW, false);
   SETTING_BOOL("statistics_show",               &settings->bools.video_statistics_show, true, DEFAULT_STATISTICS_SHOW, false);
   SETTING_BOOL("framecount_show",               &settings->bools.video_framecount_show, true, DEFAULT_FRAMECOUNT_SHOW, false);
   SETTING_BOOL("memory_show",                   &settings->bools.video_memory_show, true, DEFAULT_MEMORY_SHOW, false);
   SETTING_BOOL("ui_menubar_enable",             &settings->bools.ui_menubar_enable, true, DEFAULT_UI_MENUBAR_ENABLE, false);
   SETTING_BOOL("suspend_screensaver_enable",    &settings->bools.ui_suspend_screensaver_enable, true, true, false);
   SETTING_BOOL("rewind_enable",                 &settings->bools.rewind_enable, true, DEFAULT_REWIND_ENABLE, false);
   SETTING_BOOL("fastforward_frameskip",         &settings->bools.fastforward_frameskip, true, DEFAULT_FASTFORWARD_FRAMESKIP, false);
   SETTING_BOOL("vrr_runloop_enable",            &settings->bools.vrr_runloop_enable, true, DEFAULT_VRR_RUNLOOP_ENABLE, false);
   SETTING_BOOL("apply_cheats_after_toggle",     &settings->bools.apply_cheats_after_toggle, true, DEFAULT_APPLY_CHEATS_AFTER_TOGGLE, false);
   SETTING_BOOL("apply_cheats_after_load",       &settings->bools.apply_cheats_after_load, true, DEFAULT_APPLY_CHEATS_AFTER_LOAD, false);
   SETTING_BOOL("run_ahead_enabled",             &settings->bools.run_ahead_enabled, true, false, false);
   SETTING_BOOL("run_ahead_secondary_instance",  &settings->bools.run_ahead_secondary_instance, true, DEFAULT_RUN_AHEAD_SECONDARY_INSTANCE, false);
   SETTING_BOOL("run_ahead_hide_warnings",       &settings->bools.run_ahead_hide_warnings, true, DEFAULT_RUN_AHEAD_HIDE_WARNINGS, false);
   SETTING_BOOL("audio_sync",                    &settings->bools.audio_sync, true, DEFAULT_AUDIO_SYNC, false);
   SETTING_BOOL("video_shader_enable",           &settings->bools.video_shader_enable, true, DEFAULT_SHADER_ENABLE, false);
   SETTING_BOOL("video_shader_watch_files",      &settings->bools.video_shader_watch_files, true, DEFAULT_VIDEO_SHADER_WATCH_FILES, false);
   SETTING_BOOL("video_shader_remember_last_dir", &settings->bools.video_shader_remember_last_dir, true, DEFAULT_VIDEO_SHADER_REMEMBER_LAST_DIR, false);
   SETTING_BOOL("video_shader_preset_save_reference_enable",   &settings->bools.video_shader_preset_save_reference_enable, true, DEFAULT_VIDEO_SHADER_PRESET_SAVE_REFERENCE_ENABLE, false);

   /* Let implementation decide if automatic, or 1:1 PAR. */
   SETTING_BOOL("video_aspect_ratio_auto",       &settings->bools.video_aspect_ratio_auto, true, DEFAULT_ASPECT_RATIO_AUTO, false);

   SETTING_BOOL("video_allow_rotate",            &settings->bools.video_allow_rotate, true, DEFAULT_ALLOW_ROTATE, false);
   SETTING_BOOL("video_windowed_fullscreen",     &settings->bools.video_windowed_fullscreen, true, DEFAULT_WINDOWED_FULLSCREEN, false);
   SETTING_BOOL("video_crop_overscan",           &settings->bools.video_crop_overscan, true, DEFAULT_CROP_OVERSCAN, false);
   SETTING_BOOL("video_scale_integer",           &settings->bools.video_scale_integer, true, DEFAULT_SCALE_INTEGER, false);
   SETTING_BOOL("video_scale_integer_overscale", &settings->bools.video_scale_integer_overscale, true, DEFAULT_SCALE_INTEGER_OVERSCALE, false);
   SETTING_BOOL("video_smooth",                  &settings->bools.video_smooth, true, DEFAULT_VIDEO_SMOOTH, false);
   SETTING_BOOL("video_ctx_scaling",             &settings->bools.video_ctx_scaling, true, DEFAULT_VIDEO_CTX_SCALING, false);
   SETTING_BOOL("video_force_aspect",            &settings->bools.video_force_aspect, true, DEFAULT_FORCE_ASPECT, false);
   SETTING_BOOL("video_frame_delay_auto",        &settings->bools.video_frame_delay_auto, true, DEFAULT_FRAME_DELAY_AUTO, false);
#if defined(DINGUX)
   SETTING_BOOL("video_dingux_ipu_keep_aspect",  &settings->bools.video_dingux_ipu_keep_aspect, true, DEFAULT_DINGUX_IPU_KEEP_ASPECT, false);
#endif
   SETTING_BOOL("video_threaded",                video_driver_get_threaded(), true, DEFAULT_VIDEO_THREADED, false);
   SETTING_BOOL("video_shared_context",          &settings->bools.video_shared_context, true, DEFAULT_VIDEO_SHARED_CONTEXT, false);
   SETTING_BOOL("auto_screenshot_filename",      &settings->bools.auto_screenshot_filename, true, DEFAULT_AUTO_SCREENSHOT_FILENAME, false);
   SETTING_BOOL("video_force_srgb_disable",      &settings->bools.video_force_srgb_disable, true, false, false);
   SETTING_BOOL("video_fullscreen",              &settings->bools.video_fullscreen, true, DEFAULT_FULLSCREEN, false);
   SETTING_BOOL("video_hdr_enable",              &settings->bools.video_hdr_enable, true, DEFAULT_VIDEO_HDR_ENABLE, false);
   SETTING_BOOL("video_hdr_expand_gamut",        &settings->bools.video_hdr_expand_gamut, true, DEFAULT_VIDEO_HDR_EXPAND_GAMUT, false);
   SETTING_BOOL("bundle_assets_extract_enable",  &settings->bools.bundle_assets_extract_enable, true, DEFAULT_BUNDLE_ASSETS_EXTRACT_ENABLE, false);
   SETTING_BOOL("video_vsync",                   &settings->bools.video_vsync, true, DEFAULT_VSYNC, false);
   SETTING_BOOL("video_adaptive_vsync",          &settings->bools.video_adaptive_vsync, true, DEFAULT_ADAPTIVE_VSYNC, false);
   SETTING_BOOL("video_hard_sync",               &settings->bools.video_hard_sync, true, DEFAULT_HARD_SYNC, false);
   SETTING_BOOL("video_disable_composition",     &settings->bools.video_disable_composition, true, DEFAULT_DISABLE_COMPOSITION, false);
   SETTING_BOOL("pause_nonactive",               &settings->bools.pause_nonactive, true, DEFAULT_PAUSE_NONACTIVE, false);
   SETTING_BOOL("video_gpu_screenshot",          &settings->bools.video_gpu_screenshot, true, DEFAULT_GPU_SCREENSHOT, false);
   SETTING_BOOL("video_post_filter_record",      &settings->bools.video_post_filter_record, true, DEFAULT_POST_FILTER_RECORD, false);
   SETTING_BOOL("video_notch_write_over_enable", &settings->bools.video_notch_write_over_enable, true, DEFAULT_NOTCH_WRITE_OVER_ENABLE, false);
   SETTING_BOOL("keyboard_gamepad_enable",       &settings->bools.input_keyboard_gamepad_enable, true, true, false);
   SETTING_BOOL("core_set_supports_no_game_enable", &settings->bools.set_supports_no_game_enable, true, true, false);
   SETTING_BOOL("audio_enable",                  &settings->bools.audio_enable, true, DEFAULT_AUDIO_ENABLE, false);
   SETTING_BOOL("menu_enable_widgets",           &settings->bools.menu_enable_widgets, true, DEFAULT_MENU_ENABLE_WIDGETS, false);
   SETTING_BOOL("menu_show_load_content_animation", &settings->bools.menu_show_load_content_animation, true, DEFAULT_MENU_SHOW_LOAD_CONTENT_ANIMATION, false);
   SETTING_BOOL("notification_show_autoconfig", &settings->bools.notification_show_autoconfig, true, DEFAULT_NOTIFICATION_SHOW_AUTOCONFIG, false);
   SETTING_BOOL("notification_show_cheats_applied", &settings->bools.notification_show_cheats_applied, true, DEFAULT_NOTIFICATION_SHOW_CHEATS_APPLIED, false);
   SETTING_BOOL("notification_show_patch_applied", &settings->bools.notification_show_patch_applied, true, DEFAULT_NOTIFICATION_SHOW_PATCH_APPLIED, false);
   SETTING_BOOL("notification_show_remap_load", &settings->bools.notification_show_remap_load, true, DEFAULT_NOTIFICATION_SHOW_REMAP_LOAD, false);
   SETTING_BOOL("notification_show_config_override_load", &settings->bools.notification_show_config_override_load, true, DEFAULT_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD, false);
   SETTING_BOOL("notification_show_set_initial_disk", &settings->bools.notification_show_set_initial_disk, true, DEFAULT_NOTIFICATION_SHOW_SET_INITIAL_DISK, false);
   SETTING_BOOL("notification_show_fast_forward", &settings->bools.notification_show_fast_forward, true, DEFAULT_NOTIFICATION_SHOW_FAST_FORWARD, false);
#ifdef HAVE_SCREENSHOTS
   SETTING_BOOL("notification_show_screenshot", &settings->bools.notification_show_screenshot, true, DEFAULT_NOTIFICATION_SHOW_SCREENSHOT, false);
#endif
   SETTING_BOOL("notification_show_refresh_rate", &settings->bools.notification_show_refresh_rate, true, DEFAULT_NOTIFICATION_SHOW_REFRESH_RATE, false);
#ifdef HAVE_NETWORKING
   SETTING_BOOL("notification_show_netplay_extra", &settings->bools.notification_show_netplay_extra, true, DEFAULT_NOTIFICATION_SHOW_NETPLAY_EXTRA, false);
#endif
#ifdef HAVE_MENU
   SETTING_BOOL("notification_show_when_menu_is_alive", &settings->bools.notification_show_when_menu_is_alive, true, DEFAULT_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE, false);
#endif
   SETTING_BOOL("menu_widget_scale_auto",        &settings->bools.menu_widget_scale_auto, true, DEFAULT_MENU_WIDGET_SCALE_AUTO, false);
   SETTING_BOOL("audio_enable_menu",             &settings->bools.audio_enable_menu, true, audio_enable_menu, false);
   SETTING_BOOL("audio_enable_menu_ok",          &settings->bools.audio_enable_menu_ok, true, audio_enable_menu_ok, false);
   SETTING_BOOL("audio_enable_menu_cancel",      &settings->bools.audio_enable_menu_cancel, true, audio_enable_menu_cancel, false);
   SETTING_BOOL("audio_enable_menu_notice",      &settings->bools.audio_enable_menu_notice, true, audio_enable_menu_notice, false);
   SETTING_BOOL("audio_enable_menu_bgm",         &settings->bools.audio_enable_menu_bgm, true, audio_enable_menu_bgm, false);
   SETTING_BOOL("audio_mute_enable",             audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE), true, false, false);
#ifdef HAVE_AUDIOMIXER
   SETTING_BOOL("audio_mixer_mute_enable",       audio_get_bool_ptr(AUDIO_ACTION_MIXER_MUTE_ENABLE), true, false, false);
#endif
   SETTING_BOOL("audio_fastforward_mute",        &settings->bools.audio_fastforward_mute, true, DEFAULT_AUDIO_FASTFORWARD_MUTE, false);
   SETTING_BOOL("location_allow",                &settings->bools.location_allow, true, false, false);
   SETTING_BOOL("video_font_enable",             &settings->bools.video_font_enable, true, DEFAULT_FONT_ENABLE, false);
   SETTING_BOOL("core_updater_auto_extract_archive", &settings->bools.network_buildbot_auto_extract_archive, true, DEFAULT_NETWORK_BUILDBOT_AUTO_EXTRACT_ARCHIVE, false);
   SETTING_BOOL("core_updater_show_experimental_cores", &settings->bools.network_buildbot_show_experimental_cores, true, DEFAULT_NETWORK_BUILDBOT_SHOW_EXPERIMENTAL_CORES, false);
   SETTING_BOOL("core_updater_auto_backup",      &settings->bools.core_updater_auto_backup, true, DEFAULT_CORE_UPDATER_AUTO_BACKUP, false);
   SETTING_BOOL("camera_allow",                  &settings->bools.camera_allow, true, false, false);
   SETTING_BOOL("discord_allow",                 &settings->bools.discord_enable, true, false, false);
#if defined(VITA)
   SETTING_BOOL("input_backtouch_enable",        &settings->bools.input_backtouch_enable, false, false, false);
   SETTING_BOOL("input_backtouch_toggle",        &settings->bools.input_backtouch_toggle, false, false, false);
#endif
#if TARGET_OS_IPHONE
   SETTING_BOOL("small_keyboard_enable",         &settings->bools.input_small_keyboard_enable, true, false, false);
#endif
#ifdef GEKKO
   SETTING_BOOL("video_vfilter",                 &settings->bools.video_vfilter, true, DEFAULT_VIDEO_VFILTER, false);
#endif
#ifdef HAVE_THREADS
   SETTING_BOOL("threaded_data_runloop_enable",  &settings->bools.threaded_data_runloop_enable, true, DEFAULT_THREADED_DATA_RUNLOOP_ENABLE, false);
#endif
#ifdef HAVE_MENU
   SETTING_BOOL("menu_unified_controls",         &settings->bools.menu_unified_controls, true, false, false);
   SETTING_BOOL("menu_throttle_framerate",       &settings->bools.menu_throttle_framerate, true, true, false);
   SETTING_BOOL("menu_linear_filter",            &settings->bools.menu_linear_filter, true, DEFAULT_VIDEO_SMOOTH, false);
   SETTING_BOOL("menu_horizontal_animation",     &settings->bools.menu_horizontal_animation, true, DEFAULT_MENU_HORIZONTAL_ANIMATION, false);
   SETTING_BOOL("menu_pause_libretro",           &settings->bools.menu_pause_libretro, true, true, false);
   SETTING_BOOL("menu_savestate_resume",         &settings->bools.menu_savestate_resume, true, menu_savestate_resume, false);
   SETTING_BOOL("menu_insert_disk_resume",       &settings->bools.menu_insert_disk_resume, true, DEFAULT_MENU_INSERT_DISK_RESUME, false);
   SETTING_BOOL("menu_mouse_enable",             &settings->bools.menu_mouse_enable, true, DEFAULT_MOUSE_ENABLE, false);
   SETTING_BOOL("menu_pointer_enable",           &settings->bools.menu_pointer_enable, true, DEFAULT_POINTER_ENABLE, false);
   SETTING_BOOL("menu_timedate_enable",          &settings->bools.menu_timedate_enable, true, DEFAULT_MENU_TIMEDATE_ENABLE, false);
   SETTING_BOOL("menu_battery_level_enable",     &settings->bools.menu_battery_level_enable, true, true, false);
   SETTING_BOOL("menu_core_enable",              &settings->bools.menu_core_enable, true, true, false);
   SETTING_BOOL("menu_show_sublabels",           &settings->bools.menu_show_sublabels, true, menu_show_sublabels, false);
   SETTING_BOOL("menu_dynamic_wallpaper_enable", &settings->bools.menu_dynamic_wallpaper_enable, true, menu_dynamic_wallpaper_enable, false);
   SETTING_BOOL("menu_ticker_smooth",            &settings->bools.menu_ticker_smooth, true, DEFAULT_MENU_TICKER_SMOOTH, false);
   SETTING_BOOL("menu_scroll_fast",              &settings->bools.menu_scroll_fast, true, false, false);

   SETTING_BOOL("settings_show_drivers",          &settings->bools.settings_show_drivers, true, DEFAULT_SETTINGS_SHOW_DRIVERS, false);
   SETTING_BOOL("settings_show_video",            &settings->bools.settings_show_video, true, DEFAULT_SETTINGS_SHOW_VIDEO, false);
   SETTING_BOOL("settings_show_audio",            &settings->bools.settings_show_audio, true, DEFAULT_SETTINGS_SHOW_AUDIO, false);
   SETTING_BOOL("settings_show_input",            &settings->bools.settings_show_input, true, DEFAULT_SETTINGS_SHOW_INPUT, false);
   SETTING_BOOL("settings_show_latency",          &settings->bools.settings_show_latency, true, DEFAULT_SETTINGS_SHOW_LATENCY, false);
   SETTING_BOOL("settings_show_core",             &settings->bools.settings_show_core, true, DEFAULT_SETTINGS_SHOW_CORE, false);
   SETTING_BOOL("settings_show_configuration",    &settings->bools.settings_show_configuration, true, DEFAULT_SETTINGS_SHOW_CONFIGURATION, false);
   SETTING_BOOL("settings_show_saving",           &settings->bools.settings_show_saving, true, DEFAULT_SETTINGS_SHOW_SAVING, false);
   SETTING_BOOL("settings_show_logging",          &settings->bools.settings_show_logging, true, DEFAULT_SETTINGS_SHOW_LOGGING, false);
   SETTING_BOOL("settings_show_file_browser",     &settings->bools.settings_show_file_browser, true, DEFAULT_SETTINGS_SHOW_FILE_BROWSER, false);
   SETTING_BOOL("settings_show_frame_throttle",   &settings->bools.settings_show_frame_throttle, true, DEFAULT_SETTINGS_SHOW_FRAME_THROTTLE, false);
   SETTING_BOOL("settings_show_recording",        &settings->bools.settings_show_recording, true, DEFAULT_SETTINGS_SHOW_RECORDING, false);
   SETTING_BOOL("settings_show_onscreen_display", &settings->bools.settings_show_onscreen_display, true, DEFAULT_SETTINGS_SHOW_ONSCREEN_DISPLAY, false);
   SETTING_BOOL("settings_show_user_interface",   &settings->bools.settings_show_user_interface, true, DEFAULT_SETTINGS_SHOW_USER_INTERFACE, false);
   SETTING_BOOL("settings_show_ai_service",       &settings->bools.settings_show_ai_service, true, DEFAULT_SETTINGS_SHOW_AI_SERVICE, false);
   SETTING_BOOL("settings_show_accessibility",    &settings->bools.settings_show_accessibility, true, DEFAULT_SETTINGS_SHOW_ACCESSIBILITY, false);
   SETTING_BOOL("settings_show_power_management", &settings->bools.settings_show_power_management, true, DEFAULT_SETTINGS_SHOW_POWER_MANAGEMENT, false);
   SETTING_BOOL("settings_show_achievements",     &settings->bools.settings_show_achievements, true, DEFAULT_SETTINGS_SHOW_ACHIEVEMENTS, false);
   SETTING_BOOL("settings_show_network",          &settings->bools.settings_show_network, true, DEFAULT_SETTINGS_SHOW_NETWORK, false);
   SETTING_BOOL("settings_show_playlists",        &settings->bools.settings_show_playlists, true, DEFAULT_SETTINGS_SHOW_PLAYLISTS, false);
   SETTING_BOOL("settings_show_user",             &settings->bools.settings_show_user, true, DEFAULT_SETTINGS_SHOW_USER, false);
   SETTING_BOOL("settings_show_directory",        &settings->bools.settings_show_directory, true, DEFAULT_SETTINGS_SHOW_DIRECTORY, false);

   SETTING_BOOL("quick_menu_show_resume_content",             &settings->bools.quick_menu_show_resume_content, true, DEFAULT_QUICK_MENU_SHOW_RESUME_CONTENT, false);
   SETTING_BOOL("quick_menu_show_restart_content",            &settings->bools.quick_menu_show_restart_content, true, DEFAULT_QUICK_MENU_SHOW_RESTART_CONTENT, false);
   SETTING_BOOL("quick_menu_show_close_content",              &settings->bools.quick_menu_show_close_content, true, DEFAULT_QUICK_MENU_SHOW_CLOSE_CONTENT, false);
   SETTING_BOOL("quick_menu_show_recording",                  &settings->bools.quick_menu_show_recording, true, quick_menu_show_recording, false);
   SETTING_BOOL("quick_menu_show_streaming",                  &settings->bools.quick_menu_show_streaming, true, quick_menu_show_streaming, false);
   SETTING_BOOL("quick_menu_show_save_load_state",            &settings->bools.quick_menu_show_save_load_state, true, DEFAULT_QUICK_MENU_SHOW_SAVE_LOAD_STATE, false);
   SETTING_BOOL("quick_menu_show_take_screenshot",            &settings->bools.quick_menu_show_take_screenshot, true, DEFAULT_QUICK_MENU_SHOW_TAKE_SCREENSHOT, false);
   SETTING_BOOL("quick_menu_show_undo_save_load_state",       &settings->bools.quick_menu_show_undo_save_load_state, true, DEFAULT_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE, false);
   SETTING_BOOL("quick_menu_show_add_to_favorites",           &settings->bools.quick_menu_show_add_to_favorites, true, quick_menu_show_add_to_favorites, false);
   SETTING_BOOL("quick_menu_show_start_recording",            &settings->bools.quick_menu_show_start_recording, true, quick_menu_show_start_recording, false);
   SETTING_BOOL("quick_menu_show_start_streaming",            &settings->bools.quick_menu_show_start_streaming, true, quick_menu_show_start_streaming, false);
   SETTING_BOOL("quick_menu_show_set_core_association",       &settings->bools.quick_menu_show_set_core_association, true, quick_menu_show_set_core_association, false);
   SETTING_BOOL("quick_menu_show_reset_core_association",     &settings->bools.quick_menu_show_reset_core_association, true, quick_menu_show_reset_core_association, false);
   SETTING_BOOL("quick_menu_show_options",                    &settings->bools.quick_menu_show_options, true, quick_menu_show_options, false);
   SETTING_BOOL("quick_menu_show_core_options_flush",         &settings->bools.quick_menu_show_core_options_flush, true, DEFAULT_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH, false);
   SETTING_BOOL("quick_menu_show_controls",                   &settings->bools.quick_menu_show_controls, true, quick_menu_show_controls, false);
   SETTING_BOOL("quick_menu_show_cheats",                     &settings->bools.quick_menu_show_cheats, true, quick_menu_show_cheats, false);
   SETTING_BOOL("quick_menu_show_shaders",                    &settings->bools.quick_menu_show_shaders, true, quick_menu_show_shaders, false);
   SETTING_BOOL("quick_menu_show_save_core_overrides",        &settings->bools.quick_menu_show_save_core_overrides, true, quick_menu_show_save_core_overrides, false);
   SETTING_BOOL("quick_menu_show_save_game_overrides",        &settings->bools.quick_menu_show_save_game_overrides, true, quick_menu_show_save_game_overrides, false);
   SETTING_BOOL("quick_menu_show_save_content_dir_overrides", &settings->bools.quick_menu_show_save_content_dir_overrides, true, quick_menu_show_save_content_dir_overrides, false);
   SETTING_BOOL("quick_menu_show_information",                &settings->bools.quick_menu_show_information, true, quick_menu_show_information, false);
#ifdef HAVE_NETWORKING
   SETTING_BOOL("quick_menu_show_download_thumbnails",        &settings->bools.quick_menu_show_download_thumbnails, true, quick_menu_show_download_thumbnails, false);
#endif
   SETTING_BOOL("kiosk_mode_enable",             &settings->bools.kiosk_mode_enable, true, DEFAULT_KIOSK_MODE_ENABLE, false);
   SETTING_BOOL("menu_use_preferred_system_color_theme", &settings->bools.menu_use_preferred_system_color_theme, true, DEFAULT_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME, false);
   SETTING_BOOL("content_show_settings",         &settings->bools.menu_content_show_settings, true, content_show_settings, false);
   SETTING_BOOL("content_show_favorites",        &settings->bools.menu_content_show_favorites, true, content_show_favorites, false);
#ifdef HAVE_IMAGEVIEWER
   SETTING_BOOL("content_show_images",           &settings->bools.menu_content_show_images, true, content_show_images, false);
#endif
   SETTING_BOOL("content_show_music",            &settings->bools.menu_content_show_music, true, content_show_music, false);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   SETTING_BOOL("content_show_video",            &settings->bools.menu_content_show_video, true, content_show_video, false);
#endif
#ifdef HAVE_NETWORKING
   SETTING_BOOL("content_show_netplay",          &settings->bools.menu_content_show_netplay, true, content_show_netplay, false);
#endif
   SETTING_BOOL("content_show_history",          &settings->bools.menu_content_show_history, true, content_show_history, false);
   SETTING_BOOL("content_show_add",              &settings->bools.menu_content_show_add, true, DEFAULT_MENU_CONTENT_SHOW_ADD, false);
   SETTING_BOOL("content_show_playlists",        &settings->bools.menu_content_show_playlists, true, content_show_playlists, false);
#if defined(HAVE_LIBRETRODB)
   SETTING_BOOL("content_show_explore",          &settings->bools.menu_content_show_explore, true, DEFAULT_MENU_CONTENT_SHOW_EXPLORE, false);
#endif
   SETTING_BOOL("menu_show_load_core",           &settings->bools.menu_show_load_core, true, menu_show_load_core, false);
   SETTING_BOOL("menu_show_load_content",        &settings->bools.menu_show_load_content, true, menu_show_load_content, false);
#ifdef HAVE_CDROM
   SETTING_BOOL("menu_show_load_disc",           &settings->bools.menu_show_load_disc, true, menu_show_load_disc, false);
   SETTING_BOOL("menu_show_dump_disc",           &settings->bools.menu_show_dump_disc, true, menu_show_dump_disc, false);
#ifdef HAVE_LAKKA
   SETTING_BOOL("menu_show_eject_disc",          &settings->bools.menu_show_eject_disc, true, menu_show_eject_disc, false);
#endif /* HAVE_LAKKA */
#endif
   SETTING_BOOL("menu_show_information",         &settings->bools.menu_show_information, true, menu_show_information, false);
   SETTING_BOOL("menu_show_configurations",      &settings->bools.menu_show_configurations, true, menu_show_configurations, false);
   SETTING_BOOL("menu_show_latency",             &settings->bools.menu_show_latency, true, true, false);
   SETTING_BOOL("menu_show_rewind",              &settings->bools.menu_show_rewind, true, true, false);
   SETTING_BOOL("menu_show_overlays",            &settings->bools.menu_show_overlays, true, true, false);
#ifdef HAVE_VIDEO_LAYOUT
   SETTING_BOOL("menu_show_video_layout",        &settings->bools.menu_show_video_layout, true, true, false);
#endif
   SETTING_BOOL("menu_show_help",                &settings->bools.menu_show_help, true, menu_show_help, false);
   SETTING_BOOL("menu_show_quit_retroarch",      &settings->bools.menu_show_quit_retroarch, true, menu_show_quit_retroarch, false);
   SETTING_BOOL("menu_show_restart_retroarch",   &settings->bools.menu_show_restart_retroarch, true, menu_show_restart_retroarch, false);
   SETTING_BOOL("menu_show_reboot",              &settings->bools.menu_show_reboot, true, menu_show_reboot, false);
   SETTING_BOOL("menu_show_shutdown",            &settings->bools.menu_show_shutdown, true, menu_show_shutdown, false);
   SETTING_BOOL("menu_show_online_updater",      &settings->bools.menu_show_online_updater, true, menu_show_online_updater, false);
   SETTING_BOOL("menu_show_core_updater",        &settings->bools.menu_show_core_updater, true, menu_show_core_updater, false);
   SETTING_BOOL("menu_show_legacy_thumbnail_updater", &settings->bools.menu_show_legacy_thumbnail_updater, true, menu_show_legacy_thumbnail_updater, false);
#ifdef HAVE_MIST
   SETTING_BOOL("menu_show_core_manager_steam",  &settings->bools.menu_show_core_manager_steam, true, menu_show_core_manager_steam, false);
#endif
   SETTING_BOOL("filter_by_current_core",        &settings->bools.filter_by_current_core, true, DEFAULT_FILTER_BY_CURRENT_CORE, false);
   SETTING_BOOL("rgui_show_start_screen",        &settings->bools.menu_show_start_screen, false, false /* TODO */, false);
   SETTING_BOOL("menu_navigation_wraparound_enable", &settings->bools.menu_navigation_wraparound_enable, true, true, false);
   SETTING_BOOL("menu_navigation_browser_filter_supported_extensions_enable",
         &settings->bools.menu_navigation_browser_filter_supported_extensions_enable, true, true, false);
   SETTING_BOOL("menu_show_advanced_settings",  &settings->bools.menu_show_advanced_settings, true, DEFAULT_SHOW_ADVANCED_SETTINGS, false);
#ifdef HAVE_MATERIALUI
   SETTING_BOOL("materialui_icons_enable",       &settings->bools.menu_materialui_icons_enable, true, DEFAULT_MATERIALUI_ICONS_ENABLE, false);
   SETTING_BOOL("materialui_playlist_icons_enable", &settings->bools.menu_materialui_playlist_icons_enable, true, DEFAULT_MATERIALUI_PLAYLIST_ICONS_ENABLE, false);
   SETTING_BOOL("materialui_show_nav_bar",        &settings->bools.menu_materialui_show_nav_bar, true, DEFAULT_MATERIALUI_SHOW_NAV_BAR, false);
   SETTING_BOOL("materialui_auto_rotate_nav_bar", &settings->bools.menu_materialui_auto_rotate_nav_bar, true, DEFAULT_MATERIALUI_AUTO_ROTATE_NAV_BAR, false);
   SETTING_BOOL("materialui_dual_thumbnail_list_view_enable", &settings->bools.menu_materialui_dual_thumbnail_list_view_enable, true, DEFAULT_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE, false);
   SETTING_BOOL("materialui_thumbnail_background_enable", &settings->bools.menu_materialui_thumbnail_background_enable, true, DEFAULT_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE, false);
#endif
#ifdef HAVE_RGUI
   SETTING_BOOL("rgui_background_filler_thickness_enable", &settings->bools.menu_rgui_background_filler_thickness_enable, true, true, false);
   SETTING_BOOL("rgui_border_filler_thickness_enable",     &settings->bools.menu_rgui_border_filler_thickness_enable, true, true, false);
   SETTING_BOOL("rgui_border_filler_enable",               &settings->bools.menu_rgui_border_filler_enable, true, true, false);
   SETTING_BOOL("menu_rgui_transparency",                  &settings->bools.menu_rgui_transparency, true, DEFAULT_RGUI_TRANSPARENCY, false);
   SETTING_BOOL("menu_rgui_shadows",                       &settings->bools.menu_rgui_shadows, true, rgui_shadows, false);
   SETTING_BOOL("menu_rgui_full_width_layout",             &settings->bools.menu_rgui_full_width_layout, true, rgui_full_width_layout, false);
   SETTING_BOOL("rgui_inline_thumbnails",                  &settings->bools.menu_rgui_inline_thumbnails, true, rgui_inline_thumbnails, false);
   SETTING_BOOL("rgui_swap_thumbnails",                    &settings->bools.menu_rgui_swap_thumbnails, true, rgui_swap_thumbnails, false);
   SETTING_BOOL("rgui_extended_ascii",                     &settings->bools.menu_rgui_extended_ascii, true, rgui_extended_ascii, false);
   SETTING_BOOL("rgui_switch_icons",                       &settings->bools.menu_rgui_switch_icons, true, DEFAULT_RGUI_SWITCH_ICONS, false);
   SETTING_BOOL("rgui_particle_effect_screensaver",        &settings->bools.menu_rgui_particle_effect_screensaver, true, DEFAULT_RGUI_PARTICLE_EFFECT_SCREENSAVER, false);
#endif
#ifdef HAVE_XMB
   SETTING_BOOL("xmb_shadows_enable",            &settings->bools.menu_xmb_shadows_enable, true, DEFAULT_XMB_SHADOWS_ENABLE, false);
   SETTING_BOOL("xmb_vertical_thumbnails",       &settings->bools.menu_xmb_vertical_thumbnails, true, xmb_vertical_thumbnails, false);
#endif
#endif
#ifdef HAVE_CHEEVOS
   SETTING_BOOL("cheevos_enable",               &settings->bools.cheevos_enable, true, DEFAULT_CHEEVOS_ENABLE, false);
   SETTING_BOOL("cheevos_test_unofficial",      &settings->bools.cheevos_test_unofficial, true, false, false);
   SETTING_BOOL("cheevos_hardcore_mode_enable", &settings->bools.cheevos_hardcore_mode_enable, true, true, false);
   SETTING_BOOL("cheevos_challenge_indicators", &settings->bools.cheevos_challenge_indicators, true, true, false);
   SETTING_BOOL("cheevos_richpresence_enable",  &settings->bools.cheevos_richpresence_enable, true, true, false);
   SETTING_BOOL("cheevos_unlock_sound_enable",  &settings->bools.cheevos_unlock_sound_enable, true, false, false);
   SETTING_BOOL("cheevos_verbose_enable",       &settings->bools.cheevos_verbose_enable, true, true, false);
   SETTING_BOOL("cheevos_auto_screenshot",      &settings->bools.cheevos_auto_screenshot, true, false, false);
   SETTING_BOOL("cheevos_badges_enable",        &settings->bools.cheevos_badges_enable, true, false, false);
   SETTING_BOOL("cheevos_start_active",         &settings->bools.cheevos_start_active, true, false, false);
#endif
#ifdef HAVE_OVERLAY
   SETTING_BOOL("input_overlay_enable",         &settings->bools.input_overlay_enable, true, config_overlay_enable_default(), false);
   SETTING_BOOL("input_overlay_enable_autopreferred", &settings->bools.input_overlay_enable_autopreferred, true, DEFAULT_OVERLAY_ENABLE_AUTOPREFERRED, false);
   SETTING_BOOL("input_overlay_behind_menu",    &settings->bools.input_overlay_behind_menu, true, DEFAULT_OVERLAY_BEHIND_MENU, false);
   SETTING_BOOL("input_overlay_hide_in_menu",   &settings->bools.input_overlay_hide_in_menu, true, DEFAULT_OVERLAY_HIDE_IN_MENU, false);
   SETTING_BOOL("input_overlay_hide_when_gamepad_connected", &settings->bools.input_overlay_hide_when_gamepad_connected, true, DEFAULT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED, false);
   SETTING_BOOL("input_overlay_show_mouse_cursor",   &settings->bools.input_overlay_show_mouse_cursor, true, DEFAULT_OVERLAY_SHOW_MOUSE_CURSOR, false);
   SETTING_BOOL("input_overlay_auto_rotate",         &settings->bools.input_overlay_auto_rotate, true, DEFAULT_OVERLAY_AUTO_ROTATE, false);
   SETTING_BOOL("input_overlay_auto_scale",          &settings->bools.input_overlay_auto_scale, true, DEFAULT_INPUT_OVERLAY_AUTO_SCALE, false);
#endif
#ifdef HAVE_VIDEO_LAYOUT
   SETTING_BOOL("video_layout_enable",          &settings->bools.video_layout_enable, true, true, false);
#endif
#ifdef HAVE_COMMAND
   SETTING_BOOL("network_cmd_enable",           &settings->bools.network_cmd_enable, true, network_cmd_enable, false);
   SETTING_BOOL("stdin_cmd_enable",             &settings->bools.stdin_cmd_enable, true, stdin_cmd_enable, false);
#endif
#ifdef HAVE_NETWORKGAMEPAD
   SETTING_BOOL("network_remote_enable",        &settings->bools.network_remote_enable, false, false /* TODO */, false);
#endif
#ifdef HAVE_NETWORKING
   SETTING_BOOL("netplay_nat_traversal",        &settings->bools.netplay_nat_traversal, true, true, false);
#endif
   SETTING_BOOL("block_sram_overwrite",         &settings->bools.block_sram_overwrite, true, DEFAULT_BLOCK_SRAM_OVERWRITE, false);
   SETTING_BOOL("savestate_auto_index",         &settings->bools.savestate_auto_index, true, savestate_auto_index, false);
   SETTING_BOOL("savestate_auto_save",          &settings->bools.savestate_auto_save, true, savestate_auto_save, false);
   SETTING_BOOL("savestate_auto_load",          &settings->bools.savestate_auto_load, true, savestate_auto_load, false);
   SETTING_BOOL("savestate_thumbnail_enable",   &settings->bools.savestate_thumbnail_enable, true, savestate_thumbnail_enable, false);
   SETTING_BOOL("save_file_compression",        &settings->bools.save_file_compression, true, DEFAULT_SAVE_FILE_COMPRESSION, false);
   SETTING_BOOL("savestate_file_compression",   &settings->bools.savestate_file_compression, true, DEFAULT_SAVESTATE_FILE_COMPRESSION, false);
   SETTING_BOOL("history_list_enable",          &settings->bools.history_list_enable, true, DEFAULT_HISTORY_LIST_ENABLE, false);
   SETTING_BOOL("playlist_entry_rename",        &settings->bools.playlist_entry_rename, true, DEFAULT_PLAYLIST_ENTRY_RENAME, false);
   SETTING_BOOL("game_specific_options",        &settings->bools.game_specific_options, true, default_game_specific_options, false);
   SETTING_BOOL("auto_overrides_enable",        &settings->bools.auto_overrides_enable, true, default_auto_overrides_enable, false);
   SETTING_BOOL("auto_remaps_enable",           &settings->bools.auto_remaps_enable, true, default_auto_remaps_enable, false);
   SETTING_BOOL("global_core_options",          &settings->bools.global_core_options, true, default_global_core_options, false);
   SETTING_BOOL("auto_shaders_enable",          &settings->bools.auto_shaders_enable, true, default_auto_shaders_enable, false);
   SETTING_BOOL("scan_without_core_match",   &settings->bools.scan_without_core_match, true, DEFAULT_SCAN_WITHOUT_CORE_MATCH, false);
   SETTING_BOOL("sort_savefiles_enable",        &settings->bools.sort_savefiles_enable, true, default_sort_savefiles_enable, false);
   SETTING_BOOL("sort_savestates_enable",       &settings->bools.sort_savestates_enable, true, default_sort_savestates_enable, false);
   SETTING_BOOL("sort_savefiles_by_content_enable", &settings->bools.sort_savefiles_by_content_enable, true, default_sort_savefiles_by_content_enable, false);
   SETTING_BOOL("sort_savestates_by_content_enable", &settings->bools.sort_savestates_by_content_enable, true, default_sort_savestates_by_content_enable, false);
   SETTING_BOOL("sort_screenshots_by_content_enable", &settings->bools.sort_screenshots_by_content_enable, true, default_sort_screenshots_by_content_enable, false);
   SETTING_BOOL("config_save_on_exit",          &settings->bools.config_save_on_exit, true, DEFAULT_CONFIG_SAVE_ON_EXIT, false);
   SETTING_BOOL("show_hidden_files",            &settings->bools.show_hidden_files, true, DEFAULT_SHOW_HIDDEN_FILES, false);
   SETTING_BOOL("use_last_start_directory",     &settings->bools.use_last_start_directory, true, DEFAULT_USE_LAST_START_DIRECTORY, false);
   SETTING_BOOL("input_autodetect_enable",      &settings->bools.input_autodetect_enable, true, input_autodetect_enable, false);
   SETTING_BOOL("input_auto_mouse_grab",        &settings->bools.input_auto_mouse_grab, true, false, false);
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
   SETTING_BOOL("input_nowinkey_enable",        &settings->bools.input_nowinkey_enable, true, false, false);
#endif
   SETTING_BOOL("input_sensors_enable",         &settings->bools.input_sensors_enable, true, DEFAULT_INPUT_SENSORS_ENABLE, false);
   SETTING_BOOL("audio_rate_control",           &settings->bools.audio_rate_control, true, DEFAULT_RATE_CONTROL, false);
#ifdef HAVE_WASAPI
   SETTING_BOOL("audio_wasapi_exclusive_mode",  &settings->bools.audio_wasapi_exclusive_mode, true, DEFAULT_WASAPI_EXCLUSIVE_MODE, false);
   SETTING_BOOL("audio_wasapi_float_format",    &settings->bools.audio_wasapi_float_format, true, DEFAULT_WASAPI_FLOAT_FORMAT, false);
#endif

   SETTING_BOOL("savestates_in_content_dir",     &settings->bools.savestates_in_content_dir, true, default_savestates_in_content_dir, false);
   SETTING_BOOL("savefiles_in_content_dir",      &settings->bools.savefiles_in_content_dir, true, default_savefiles_in_content_dir, false);
   SETTING_BOOL("systemfiles_in_content_dir",    &settings->bools.systemfiles_in_content_dir, true, default_systemfiles_in_content_dir, false);
   SETTING_BOOL("screenshots_in_content_dir",    &settings->bools.screenshots_in_content_dir, true, default_screenshots_in_content_dir, false);

   SETTING_BOOL("video_msg_bgcolor_enable",      &settings->bools.video_msg_bgcolor_enable, true, message_bgcolor_enable, false);
   SETTING_BOOL("video_window_show_decorations", &settings->bools.video_window_show_decorations, true, DEFAULT_WINDOW_DECORATIONS, false);
   SETTING_BOOL("video_window_save_positions", &settings->bools.video_window_save_positions, true, DEFAULT_WINDOW_SAVE_POSITIONS, false);
   SETTING_BOOL("video_window_custom_size_enable", &settings->bools.video_window_custom_size_enable, true, DEFAULT_WINDOW_CUSTOM_SIZE_ENABLE, false);

   SETTING_BOOL("sustained_performance_mode",    &settings->bools.sustained_performance_mode, true, sustained_performance_mode, false);

#ifdef _3DS
   SETTING_BOOL("new3ds_speedup_enable",         &settings->bools.new3ds_speedup_enable, true, new3ds_speedup_enable, false);
   SETTING_BOOL("video_3ds_lcd_bottom",          &settings->bools.video_3ds_lcd_bottom, true, video_3ds_lcd_bottom, false);
#endif

#ifdef WIIU
   SETTING_BOOL("video_wiiu_prefer_drc",         &settings->bools.video_wiiu_prefer_drc, true, DEFAULT_WIIU_PREFER_DRC, false);
#endif

   SETTING_BOOL("playlist_use_old_format",       &settings->bools.playlist_use_old_format, true, DEFAULT_PLAYLIST_USE_OLD_FORMAT, false);
   SETTING_BOOL("playlist_compression",          &settings->bools.playlist_compression, true, DEFAULT_PLAYLIST_COMPRESSION, false);
   SETTING_BOOL("content_runtime_log",           &settings->bools.content_runtime_log, true, DEFAULT_CONTENT_RUNTIME_LOG, false);
   SETTING_BOOL("content_runtime_log_aggregate", &settings->bools.content_runtime_log_aggregate, true, DEFAULT_CONTENT_RUNTIME_LOG_AGGREGATE, false);
   SETTING_BOOL("playlist_show_sublabels",       &settings->bools.playlist_show_sublabels, true, DEFAULT_PLAYLIST_SHOW_SUBLABELS, false);
   SETTING_BOOL("playlist_show_entry_idx",       &settings->bools.playlist_show_entry_idx, true, DEFAULT_PLAYLIST_SHOW_ENTRY_IDX, false);
   SETTING_BOOL("playlist_sort_alphabetical",    &settings->bools.playlist_sort_alphabetical, true, DEFAULT_PLAYLIST_SORT_ALPHABETICAL, false);
   SETTING_BOOL("playlist_fuzzy_archive_match",  &settings->bools.playlist_fuzzy_archive_match, true, DEFAULT_PLAYLIST_FUZZY_ARCHIVE_MATCH, false);
   SETTING_BOOL("playlist_portable_paths",       &settings->bools.playlist_portable_paths, true, DEFAULT_PLAYLIST_PORTABLE_PATHS, false);

   SETTING_BOOL("quit_press_twice", &settings->bools.quit_press_twice, true, DEFAULT_QUIT_PRESS_TWICE, false);
   SETTING_BOOL("vibrate_on_keypress", &settings->bools.vibrate_on_keypress, true, vibrate_on_keypress, false);
   SETTING_BOOL("enable_device_vibration", &settings->bools.enable_device_vibration, true, enable_device_vibration, false);

#ifdef HAVE_OZONE
   SETTING_BOOL("ozone_collapse_sidebar",       &settings->bools.ozone_collapse_sidebar, true, DEFAULT_OZONE_COLLAPSE_SIDEBAR, false);
   SETTING_BOOL("ozone_truncate_playlist_name", &settings->bools.ozone_truncate_playlist_name, true, DEFAULT_OZONE_TRUNCATE_PLAYLIST_NAME, false);
   SETTING_BOOL("ozone_sort_after_truncate_playlist_name", &settings->bools.ozone_sort_after_truncate_playlist_name, true, DEFAULT_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME, false);
   SETTING_BOOL("ozone_scroll_content_metadata",&settings->bools.ozone_scroll_content_metadata, true, DEFAULT_OZONE_SCROLL_CONTENT_METADATA, false);
#endif
   SETTING_BOOL("log_to_file", &settings->bools.log_to_file, true, DEFAULT_LOG_TO_FILE, false);
   SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_LOG_TO_FILE);
   SETTING_BOOL("log_to_file_timestamp", &settings->bools.log_to_file_timestamp, true, DEFAULT_LOG_TO_FILE_TIMESTAMP, false);
   SETTING_BOOL("ai_service_enable",     &settings->bools.ai_service_enable, true, DEFAULT_AI_SERVICE_ENABLE, false);
   SETTING_BOOL("ai_service_pause",      &settings->bools.ai_service_pause, true, DEFAULT_AI_SERVICE_PAUSE, false);
   SETTING_BOOL("wifi_enabled",          &settings->bools.wifi_enabled, true, DEFAULT_WIFI_ENABLE, false);
   SETTING_BOOL("gamemode_enable",       &settings->bools.gamemode_enable, true, DEFAULT_GAMEMODE_ENABLE, false);

   *size = count;

   return tmp;
}

static struct config_float_setting *populate_settings_float(
      settings_t *settings, int *size)
{
   unsigned count = 0;
   struct config_float_setting  *tmp      = (struct config_float_setting*)calloc(1, (*size + 1) * sizeof(struct config_float_setting));

   if (!tmp)
      return NULL;

   SETTING_FLOAT("video_aspect_ratio",       &settings->floats.video_aspect_ratio, true, DEFAULT_ASPECT_RATIO, false);
   SETTING_FLOAT("video_scale",              &settings->floats.video_scale, false, 0.0f, false);
   SETTING_FLOAT("crt_video_refresh_rate",   &settings->floats.crt_video_refresh_rate, true, DEFAULT_CRT_REFRESH_RATE, false);
   SETTING_FLOAT("video_refresh_rate",       &settings->floats.video_refresh_rate, true, DEFAULT_REFRESH_RATE, false);
   SETTING_FLOAT("audio_rate_control_delta", audio_get_float_ptr(AUDIO_ACTION_RATE_CONTROL_DELTA), true, DEFAULT_RATE_CONTROL_DELTA, false);
   SETTING_FLOAT("audio_max_timing_skew",    &settings->floats.audio_max_timing_skew, true, DEFAULT_MAX_TIMING_SKEW, false);
   SETTING_FLOAT("audio_volume",             &settings->floats.audio_volume, true, DEFAULT_AUDIO_VOLUME, false);
#ifdef HAVE_AUDIOMIXER
   SETTING_FLOAT("audio_mixer_volume",       &settings->floats.audio_mixer_volume, true, DEFAULT_AUDIO_MIXER_VOLUME, false);
#endif
#ifdef HAVE_OVERLAY
   SETTING_FLOAT("input_overlay_opacity",                 &settings->floats.input_overlay_opacity, true, DEFAULT_INPUT_OVERLAY_OPACITY, false);
   SETTING_FLOAT("input_overlay_scale_landscape",         &settings->floats.input_overlay_scale_landscape, true, DEFAULT_INPUT_OVERLAY_SCALE_LANDSCAPE, false);
   SETTING_FLOAT("input_overlay_aspect_adjust_landscape", &settings->floats.input_overlay_aspect_adjust_landscape, true, DEFAULT_INPUT_OVERLAY_ASPECT_ADJUST_LANDSCAPE, false);
   SETTING_FLOAT("input_overlay_x_separation_landscape",  &settings->floats.input_overlay_x_separation_landscape, true, DEFAULT_INPUT_OVERLAY_X_SEPARATION_LANDSCAPE, false);
   SETTING_FLOAT("input_overlay_y_separation_landscape",  &settings->floats.input_overlay_y_separation_landscape, true, DEFAULT_INPUT_OVERLAY_Y_SEPARATION_LANDSCAPE, false);
   SETTING_FLOAT("input_overlay_x_offset_landscape",      &settings->floats.input_overlay_x_offset_landscape, true, DEFAULT_INPUT_OVERLAY_X_OFFSET_LANDSCAPE, false);
   SETTING_FLOAT("input_overlay_y_offset_landscape",      &settings->floats.input_overlay_y_offset_landscape, true, DEFAULT_INPUT_OVERLAY_Y_OFFSET_LANDSCAPE, false);
   SETTING_FLOAT("input_overlay_scale_portrait",          &settings->floats.input_overlay_scale_portrait, true, DEFAULT_INPUT_OVERLAY_SCALE_PORTRAIT, false);
   SETTING_FLOAT("input_overlay_aspect_adjust_portrait",  &settings->floats.input_overlay_aspect_adjust_portrait, true, DEFAULT_INPUT_OVERLAY_ASPECT_ADJUST_PORTRAIT, false);
   SETTING_FLOAT("input_overlay_x_separation_portrait",   &settings->floats.input_overlay_x_separation_portrait, true, DEFAULT_INPUT_OVERLAY_X_SEPARATION_PORTRAIT, false);
   SETTING_FLOAT("input_overlay_y_separation_portrait",   &settings->floats.input_overlay_y_separation_portrait, true, DEFAULT_INPUT_OVERLAY_Y_SEPARATION_PORTRAIT, false);
   SETTING_FLOAT("input_overlay_x_offset_portrait",       &settings->floats.input_overlay_x_offset_portrait, true, DEFAULT_INPUT_OVERLAY_X_OFFSET_PORTRAIT, false);
   SETTING_FLOAT("input_overlay_y_offset_portrait",       &settings->floats.input_overlay_y_offset_portrait, true, DEFAULT_INPUT_OVERLAY_Y_OFFSET_PORTRAIT, false);
#endif
#ifdef HAVE_MENU
   SETTING_FLOAT("menu_scale_factor",        &settings->floats.menu_scale_factor, true, DEFAULT_MENU_SCALE_FACTOR, false);
   SETTING_FLOAT("menu_widget_scale_factor", &settings->floats.menu_widget_scale_factor, true, DEFAULT_MENU_WIDGET_SCALE_FACTOR, false);
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   SETTING_FLOAT("menu_widget_scale_factor_windowed", &settings->floats.menu_widget_scale_factor_windowed, true, DEFAULT_MENU_WIDGET_SCALE_FACTOR_WINDOWED, false);
#endif
   SETTING_FLOAT("menu_wallpaper_opacity",   &settings->floats.menu_wallpaper_opacity, true, menu_wallpaper_opacity, false);
   SETTING_FLOAT("menu_framebuffer_opacity", &settings->floats.menu_framebuffer_opacity, true, menu_framebuffer_opacity, false);
   SETTING_FLOAT("menu_footer_opacity",      &settings->floats.menu_footer_opacity,    true, menu_footer_opacity, false);
   SETTING_FLOAT("menu_header_opacity",      &settings->floats.menu_header_opacity,    true, menu_header_opacity, false);
   SETTING_FLOAT("menu_ticker_speed",        &settings->floats.menu_ticker_speed,      true, menu_ticker_speed,   false);
   SETTING_FLOAT("rgui_particle_effect_speed", &settings->floats.menu_rgui_particle_effect_speed, true, DEFAULT_RGUI_PARTICLE_EFFECT_SPEED, false);
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
   SETTING_FLOAT("menu_screensaver_animation_speed", &settings->floats.menu_screensaver_animation_speed, true, DEFAULT_MENU_SCREENSAVER_ANIMATION_SPEED, false);
#endif
#ifdef HAVE_OZONE
   SETTING_FLOAT("ozone_thumbnail_scale_factor", &settings->floats.ozone_thumbnail_scale_factor, true, DEFAULT_OZONE_THUMBNAIL_SCALE_FACTOR, false);
#endif
#endif
   SETTING_FLOAT("video_message_pos_x",      &settings->floats.video_msg_pos_x,      true, message_pos_offset_x, false);
   SETTING_FLOAT("video_message_pos_y",      &settings->floats.video_msg_pos_y,      true, message_pos_offset_y, false);
   SETTING_FLOAT("video_font_size",          &settings->floats.video_font_size,      true, DEFAULT_FONT_SIZE, false);
   SETTING_FLOAT("fastforward_ratio",        &settings->floats.fastforward_ratio,    true, DEFAULT_FASTFORWARD_RATIO, false);
   SETTING_FLOAT("slowmotion_ratio",         &settings->floats.slowmotion_ratio,     true, DEFAULT_SLOWMOTION_RATIO, false);
   SETTING_FLOAT("input_axis_threshold",     &settings->floats.input_axis_threshold, true, DEFAULT_AXIS_THRESHOLD, false);
   SETTING_FLOAT("input_analog_deadzone",    &settings->floats.input_analog_deadzone, true, DEFAULT_ANALOG_DEADZONE, false);
   SETTING_FLOAT("input_analog_sensitivity",    &settings->floats.input_analog_sensitivity, true, DEFAULT_ANALOG_SENSITIVITY, false);
   SETTING_FLOAT("video_msg_bgcolor_opacity", &settings->floats.video_msg_bgcolor_opacity, true, message_bgcolor_opacity, false);

   SETTING_FLOAT("video_hdr_max_nits",          &settings->floats.video_hdr_max_nits, true, DEFAULT_VIDEO_HDR_MAX_NITS, false);
   SETTING_FLOAT("video_hdr_paper_white_nits",  &settings->floats.video_hdr_paper_white_nits, true, DEFAULT_VIDEO_HDR_PAPER_WHITE_NITS, false);
   SETTING_FLOAT("video_hdr_display_contrast",  &settings->floats.video_hdr_display_contrast, true, DEFAULT_VIDEO_HDR_CONTRAST, false);

   *size = count;

   return tmp;
}

static struct config_uint_setting *populate_settings_uint(
      settings_t *settings, int *size)
{
   unsigned count                     = 0;
   struct config_uint_setting  *tmp   = (struct config_uint_setting*)calloc(1, (*size + 1) * sizeof(struct config_uint_setting));

   if (!tmp)
      return NULL;

   SETTING_UINT("accessibility_narrator_speech_speed",  		         &settings->uints.accessibility_narrator_speech_speed, true, DEFAULT_ACCESSIBILITY_NARRATOR_SPEECH_SPEED, false);
#ifdef HAVE_NETWORKING
   SETTING_UINT("streaming_mode",  		         &settings->uints.streaming_mode, true, STREAMING_MODE_TWITCH, false);
#endif
   SETTING_UINT("screen_brightness",	  		&settings->uints.screen_brightness, true, DEFAULT_SCREEN_BRIGHTNESS, false);
   SETTING_UINT("crt_switch_resolution",  		&settings->uints.crt_switch_resolution, true, DEFAULT_CRT_SWITCH_RESOLUTION, false);
   SETTING_UINT("input_bind_timeout",           &settings->uints.input_bind_timeout,     true, input_bind_timeout, false);
   SETTING_UINT("input_bind_hold",              &settings->uints.input_bind_hold,        true, input_bind_hold, false);
   SETTING_UINT("input_turbo_period",           &settings->uints.input_turbo_period,     true, turbo_period, false);
   SETTING_UINT("input_duty_cycle",             &settings->uints.input_turbo_duty_cycle, true, turbo_duty_cycle, false);
   SETTING_UINT("input_turbo_mode",             &settings->uints.input_turbo_mode, true, turbo_mode, false);
   SETTING_UINT("input_turbo_default_button",   &settings->uints.input_turbo_default_button, true, turbo_default_btn, false);
   SETTING_UINT("input_max_users",              &settings->uints.input_max_users,          true, input_max_users, false);
   SETTING_UINT("fps_update_interval",          &settings->uints.fps_update_interval, true, DEFAULT_FPS_UPDATE_INTERVAL, false);
   SETTING_UINT("memory_update_interval",       &settings->uints.memory_update_interval, true, DEFAULT_MEMORY_UPDATE_INTERVAL, false);
   SETTING_UINT("input_menu_toggle_gamepad_combo", &settings->uints.input_menu_toggle_gamepad_combo, true, DEFAULT_MENU_TOGGLE_GAMEPAD_COMBO, false);
   SETTING_UINT("input_quit_gamepad_combo",     &settings->uints.input_quit_gamepad_combo, true, DEFAULT_QUIT_GAMEPAD_COMBO, false);
   SETTING_UINT("input_hotkey_block_delay",     &settings->uints.input_hotkey_block_delay, true, DEFAULT_INPUT_HOTKEY_BLOCK_DELAY, false);
#ifdef GEKKO
   SETTING_UINT("input_mouse_scale",            &settings->uints.input_mouse_scale, true, DEFAULT_MOUSE_SCALE, false);
#endif
   SETTING_UINT("input_touch_scale",            &settings->uints.input_touch_scale, true, DEFAULT_TOUCH_SCALE, false);
   SETTING_UINT("input_rumble_gain",            &settings->uints.input_rumble_gain, true, DEFAULT_RUMBLE_GAIN, false);
   SETTING_UINT("input_auto_game_focus",        &settings->uints.input_auto_game_focus, true, DEFAULT_INPUT_AUTO_GAME_FOCUS, false);
   SETTING_UINT("audio_latency",                &settings->uints.audio_latency, false, 0 /* TODO */, false);
   SETTING_UINT("audio_resampler_quality",      &settings->uints.audio_resampler_quality, true, audio_resampler_quality_level, false);
   SETTING_UINT("audio_block_frames",           &settings->uints.audio_block_frames, true, 0, false);
#ifdef ANDROID
   SETTING_UINT("input_block_timeout",           &settings->uints.input_block_timeout, true, 1, false);
#endif
   SETTING_UINT("rewind_granularity",           &settings->uints.rewind_granularity, true, DEFAULT_REWIND_GRANULARITY, false);
   SETTING_UINT("rewind_buffer_size_step",      &settings->uints.rewind_buffer_size_step, true, DEFAULT_REWIND_BUFFER_SIZE_STEP, false);
   SETTING_UINT("autosave_interval",            &settings->uints.autosave_interval,  true, DEFAULT_AUTOSAVE_INTERVAL, false);
   SETTING_UINT("savestate_max_keep",           &settings->uints.savestate_max_keep, true, DEFAULT_SAVESTATE_MAX_KEEP, false);
   SETTING_UINT("frontend_log_level",           &settings->uints.frontend_log_level, true, DEFAULT_FRONTEND_LOG_LEVEL, false);
   SETTING_UINT("libretro_log_level",           &settings->uints.libretro_log_level, true, DEFAULT_LIBRETRO_LOG_LEVEL, false);
   SETTING_UINT("keyboard_gamepad_mapping_type",&settings->uints.input_keyboard_gamepad_mapping_type, true, 1, false);
   SETTING_UINT("input_poll_type_behavior",     &settings->uints.input_poll_type_behavior, true, 2, false);
   SETTING_UINT("video_monitor_index",          &settings->uints.video_monitor_index, true, DEFAULT_MONITOR_INDEX, false);
#ifdef __WINRT__
   SETTING_UINT("video_fullscreen_x", &settings->uints.video_fullscreen_x, true, uwp_get_width(), false);
   SETTING_UINT("video_fullscreen_y", &settings->uints.video_fullscreen_y, true, uwp_get_height(), false);
#else
   SETTING_UINT("video_fullscreen_x", &settings->uints.video_fullscreen_x, true, DEFAULT_FULLSCREEN_X, false);
   SETTING_UINT("video_fullscreen_y", &settings->uints.video_fullscreen_y, true, DEFAULT_FULLSCREEN_Y, false);
#endif
   SETTING_UINT("video_window_opacity",         &settings->uints.video_window_opacity, true, DEFAULT_WINDOW_OPACITY, false);
#ifdef HAVE_VIDEO_LAYOUT
   SETTING_UINT("video_layout_selected_view",   &settings->uints.video_layout_selected_view, true, 0, false);
#endif
   SETTING_UINT("video_shader_delay",           &settings->uints.video_shader_delay, true, DEFAULT_SHADER_DELAY, false);
#ifdef HAVE_COMMAND
   SETTING_UINT("network_cmd_port",             &settings->uints.network_cmd_port,    true, network_cmd_port, false);
#endif
#ifdef HAVE_NETWORKGAMEPAD
   SETTING_UINT("network_remote_base_port",     &settings->uints.network_remote_base_port, true, network_remote_base_port, false);
#endif
#ifdef GEKKO
   SETTING_UINT("video_viwidth",                    &settings->uints.video_viwidth, true, DEFAULT_VIDEO_VI_WIDTH, false);
   SETTING_UINT("video_overscan_correction_top",    &settings->uints.video_overscan_correction_top, true, DEFAULT_VIDEO_OVERSCAN_CORRECTION_TOP, false);
   SETTING_UINT("video_overscan_correction_bottom", &settings->uints.video_overscan_correction_bottom, true, DEFAULT_VIDEO_OVERSCAN_CORRECTION_BOTTOM, false);
#endif
#ifdef HAVE_MENU
   SETTING_UINT("menu_thumbnails",              &settings->uints.gfx_thumbnails, true, gfx_thumbnails_default, false);
   SETTING_UINT("menu_left_thumbnails",         &settings->uints.menu_left_thumbnails, true, menu_left_thumbnails_default, false);
   SETTING_UINT("menu_thumbnail_upscale_threshold", &settings->uints.gfx_thumbnail_upscale_threshold, true, gfx_thumbnail_upscale_threshold, false);
   SETTING_UINT("menu_timedate_style",          &settings->uints.menu_timedate_style, true, DEFAULT_MENU_TIMEDATE_STYLE, false);
   SETTING_UINT("menu_timedate_date_separator", &settings->uints.menu_timedate_date_separator, true, DEFAULT_MENU_TIMEDATE_DATE_SEPARATOR, false);
   SETTING_UINT("menu_ticker_type",             &settings->uints.menu_ticker_type, true, DEFAULT_MENU_TICKER_TYPE, false);
   SETTING_UINT("menu_scroll_delay",            &settings->uints.menu_scroll_delay, true, DEFAULT_MENU_SCROLL_DELAY, false);
   SETTING_UINT("content_show_add_entry",       &settings->uints.menu_content_show_add_entry, true, DEFAULT_MENU_CONTENT_SHOW_ADD_ENTRY, false);
   SETTING_UINT("content_show_contentless_cores", &settings->uints.menu_content_show_contentless_cores, true, DEFAULT_MENU_CONTENT_SHOW_CONTENTLESS_CORES, false);
   SETTING_UINT("menu_screensaver_timeout",     &settings->uints.menu_screensaver_timeout, true, DEFAULT_MENU_SCREENSAVER_TIMEOUT, false);
#if defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)
   SETTING_UINT("menu_screensaver_animation",   &settings->uints.menu_screensaver_animation, true, DEFAULT_MENU_SCREENSAVER_ANIMATION, false);
#endif
#ifdef HAVE_RGUI
   SETTING_UINT("rgui_menu_color_theme",        &settings->uints.menu_rgui_color_theme, true, DEFAULT_RGUI_COLOR_THEME, false);
   SETTING_UINT("rgui_thumbnail_downscaler",    &settings->uints.menu_rgui_thumbnail_downscaler, true, rgui_thumbnail_downscaler, false);
   SETTING_UINT("rgui_thumbnail_delay",         &settings->uints.menu_rgui_thumbnail_delay, true, rgui_thumbnail_delay, false);
   SETTING_UINT("rgui_internal_upscale_level",  &settings->uints.menu_rgui_internal_upscale_level, true, rgui_internal_upscale_level, false);
   SETTING_UINT("rgui_aspect_ratio",            &settings->uints.menu_rgui_aspect_ratio, true, rgui_aspect, false);
   SETTING_UINT("rgui_aspect_ratio_lock",       &settings->uints.menu_rgui_aspect_ratio_lock, true, rgui_aspect_lock, false);
   SETTING_UINT("rgui_particle_effect",         &settings->uints.menu_rgui_particle_effect, true, rgui_particle_effect, false);
#endif
#ifdef HAVE_LIBNX
   SETTING_UINT("split_joycon_p1", &settings->uints.input_split_joycon[0], true, 0, false);
   SETTING_UINT("split_joycon_p2", &settings->uints.input_split_joycon[1], true, 0, false);
   SETTING_UINT("split_joycon_p3", &settings->uints.input_split_joycon[2], true, 0, false);
   SETTING_UINT("split_joycon_p4", &settings->uints.input_split_joycon[3], true, 0, false);
   SETTING_UINT("split_joycon_p5", &settings->uints.input_split_joycon[4], true, 0, false);
   SETTING_UINT("split_joycon_p6", &settings->uints.input_split_joycon[5], true, 0, false);
   SETTING_UINT("split_joycon_p7", &settings->uints.input_split_joycon[6], true, 0, false);
   SETTING_UINT("split_joycon_p8", &settings->uints.input_split_joycon[7], true, 0, false);
#endif
#ifdef HAVE_XMB
   SETTING_UINT("menu_xmb_animation_opening_main_menu",    &settings->uints.menu_xmb_animation_opening_main_menu, true, DEFAULT_XMB_ANIMATION, false);
   SETTING_UINT("menu_xmb_animation_horizontal_highlight", &settings->uints.menu_xmb_animation_horizontal_highlight, true, DEFAULT_XMB_ANIMATION, false);
   SETTING_UINT("menu_xmb_animation_move_up_down",         &settings->uints.menu_xmb_animation_move_up_down, true, DEFAULT_XMB_ANIMATION, false);
   SETTING_UINT("xmb_alpha_factor",             &settings->uints.menu_xmb_alpha_factor, true, xmb_alpha_factor, false);
   SETTING_UINT("xmb_layout",                   &settings->uints.menu_xmb_layout, true, xmb_menu_layout, false);
   SETTING_UINT("xmb_theme",                    &settings->uints.menu_xmb_theme, true, xmb_icon_theme, false);
   SETTING_UINT("xmb_menu_color_theme",         &settings->uints.menu_xmb_color_theme, true, xmb_theme, false);
   SETTING_UINT("menu_font_color_red",          &settings->uints.menu_font_color_red, true, menu_font_color_red, false);
   SETTING_UINT("menu_font_color_green",        &settings->uints.menu_font_color_green, true, menu_font_color_green, false);
   SETTING_UINT("menu_font_color_blue",         &settings->uints.menu_font_color_blue, true, menu_font_color_blue, false);
   SETTING_UINT("menu_xmb_thumbnail_scale_factor", &settings->uints.menu_xmb_thumbnail_scale_factor, true, xmb_thumbnail_scale_factor, false);
   SETTING_UINT("menu_xmb_vertical_fade_factor",&settings->uints.menu_xmb_vertical_fade_factor, true, DEFAULT_XMB_VERTICAL_FADE_FACTOR, false);
   SETTING_UINT("menu_xmb_title_margin",        &settings->uints.menu_xmb_title_margin, true, DEFAULT_XMB_TITLE_MARGIN, false);
#endif
   SETTING_UINT("materialui_menu_color_theme",  &settings->uints.menu_materialui_color_theme, true, DEFAULT_MATERIALUI_THEME, false);
   SETTING_UINT("materialui_menu_transition_animation", &settings->uints.menu_materialui_transition_animation, true, DEFAULT_MATERIALUI_TRANSITION_ANIM, false);
   SETTING_UINT("materialui_thumbnail_view_portrait", &settings->uints.menu_materialui_thumbnail_view_portrait, true, DEFAULT_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT, false);
   SETTING_UINT("materialui_thumbnail_view_landscape", &settings->uints.menu_materialui_thumbnail_view_landscape, true, DEFAULT_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE, false);
   SETTING_UINT("materialui_landscape_layout_optimization", &settings->uints.menu_materialui_landscape_layout_optimization, true, DEFAULT_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION, false);
   SETTING_UINT("menu_shader_pipeline",         &settings->uints.menu_xmb_shader_pipeline, true, DEFAULT_MENU_SHADER_PIPELINE, false);
#ifdef HAVE_OZONE
   SETTING_UINT("ozone_menu_color_theme",       &settings->uints.menu_ozone_color_theme, true, DEFAULT_OZONE_COLOR_THEME, false);
#endif
#endif
   SETTING_UINT("audio_out_rate",               &settings->uints.audio_output_sample_rate, true, DEFAULT_OUTPUT_RATE, false);
   SETTING_UINT("custom_viewport_width",        &settings->video_viewport_custom.width, false, 0 /* TODO */, false);
   SETTING_UINT("crt_switch_resolution_super",  &settings->uints.crt_switch_resolution_super,      true, DEFAULT_CRT_SWITCH_RESOLUTION_SUPER, false);
   SETTING_UINT("custom_viewport_height",       &settings->video_viewport_custom.height, false, 0 /* TODO */, false);
   SETTING_UINT("custom_viewport_x",            (unsigned*)&settings->video_viewport_custom.x, false, 0 /* TODO */, false);
   SETTING_UINT("custom_viewport_y",            (unsigned*)&settings->video_viewport_custom.y, false, 0 /* TODO */, false);
   SETTING_UINT("content_history_size",         &settings->uints.content_history_size,   true, default_content_history_size, false);
   SETTING_UINT("video_hard_sync_frames",       &settings->uints.video_hard_sync_frames, true, DEFAULT_HARD_SYNC_FRAMES, false);
   SETTING_UINT("video_frame_delay",            &settings->uints.video_frame_delay,      true, DEFAULT_FRAME_DELAY, false);
   SETTING_UINT("video_max_swapchain_images",   &settings->uints.video_max_swapchain_images, true, DEFAULT_MAX_SWAPCHAIN_IMAGES, false);
   SETTING_UINT("video_swap_interval",          &settings->uints.video_swap_interval, true, DEFAULT_SWAP_INTERVAL, false);
   SETTING_UINT("video_rotation",               &settings->uints.video_rotation, true, ORIENTATION_NORMAL, false);
   SETTING_UINT("screen_orientation",           &settings->uints.screen_orientation, true, ORIENTATION_NORMAL, false);
   SETTING_UINT("aspect_ratio_index",           &settings->uints.video_aspect_ratio_idx, true, DEFAULT_ASPECT_RATIO_IDX, false);
#ifdef HAVE_SCREENSHOTS
   SETTING_UINT("notification_show_screenshot_duration",    &settings->uints.notification_show_screenshot_duration, true, DEFAULT_NOTIFICATION_SHOW_SCREENSHOT_DURATION, false);
   SETTING_UINT("notification_show_screenshot_flash",       &settings->uints.notification_show_screenshot_flash, true, DEFAULT_NOTIFICATION_SHOW_SCREENSHOT_FLASH, false);
#endif
#ifdef HAVE_NETWORKING
   SETTING_UINT("netplay_ip_port",              &settings->uints.netplay_port,         true, RARCH_DEFAULT_PORT, false);
   SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT);
   SETTING_UINT("netplay_max_connections",      &settings->uints.netplay_max_connections, true, netplay_max_connections, false);
   SETTING_UINT("netplay_max_ping",             &settings->uints.netplay_max_ping, true, netplay_max_ping, false);
   SETTING_UINT("netplay_input_latency_frames_min",&settings->uints.netplay_input_latency_frames_min, true, 0, false);
   SETTING_UINT("netplay_input_latency_frames_range",&settings->uints.netplay_input_latency_frames_range, true, 0, false);
   SETTING_UINT("netplay_share_digital",        &settings->uints.netplay_share_digital, true, netplay_share_digital, false);
   SETTING_UINT("netplay_share_analog",         &settings->uints.netplay_share_analog,  true, netplay_share_analog, false);
#endif
#ifdef HAVE_LANGEXTRA
#ifdef VITA
   SETTING_UINT("user_language",                msg_hash_get_uint(MSG_HASH_USER_LANGUAGE), true, frontend_driver_get_user_language(), false);
#else
   SETTING_UINT("user_language",                msg_hash_get_uint(MSG_HASH_USER_LANGUAGE), true, DEFAULT_USER_LANGUAGE, false);
#endif
#endif
#ifndef __APPLE__
   SETTING_UINT("bundle_assets_extract_version_current", &settings->uints.bundle_assets_extract_version_current, true, 0, false);
#endif
   SETTING_UINT("bundle_assets_extract_last_version",    &settings->uints.bundle_assets_extract_last_version, true, 0, false);

#if defined(HAVE_OVERLAY)
   SETTING_UINT("input_overlay_show_inputs",      &settings->uints.input_overlay_show_inputs, true, DEFAULT_OVERLAY_SHOW_INPUTS, false);
   SETTING_UINT("input_overlay_show_inputs_port", &settings->uints.input_overlay_show_inputs_port, true, DEFAULT_OVERLAY_SHOW_INPUTS_PORT, false);
#endif

   SETTING_UINT("video_msg_bgcolor_red",        &settings->uints.video_msg_bgcolor_red, true, message_bgcolor_red, false);
   SETTING_UINT("video_msg_bgcolor_green",        &settings->uints.video_msg_bgcolor_green, true, message_bgcolor_green, false);
   SETTING_UINT("video_msg_bgcolor_blue",        &settings->uints.video_msg_bgcolor_blue, true, message_bgcolor_blue, false);

   SETTING_UINT("run_ahead_frames",           &settings->uints.run_ahead_frames, true, 1,  false);

   SETTING_UINT("midi_volume",                  &settings->uints.midi_volume, true, midi_volume, false);

   SETTING_UINT("video_stream_port",            &settings->uints.video_stream_port,    true, RARCH_STREAM_DEFAULT_PORT, false);
   SETTING_UINT("video_record_quality",            &settings->uints.video_record_quality,    true, RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY, false);
   SETTING_UINT("video_stream_quality",            &settings->uints.video_stream_quality,    true, RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY, false);
   SETTING_UINT("video_record_scale_factor",            &settings->uints.video_record_scale_factor,    true, 1, false);
   SETTING_UINT("video_stream_scale_factor",            &settings->uints.video_stream_scale_factor,    true, 1, false);
   SETTING_UINT("video_windowed_position_x",            &settings->uints.window_position_x,    true, 0, false);
   SETTING_UINT("video_windowed_position_y",            &settings->uints.window_position_y,    true, 0, false);
   SETTING_UINT("video_windowed_position_width",        &settings->uints.window_position_width,    true, DEFAULT_WINDOW_WIDTH, false);
   SETTING_UINT("video_windowed_position_height",       &settings->uints.window_position_height,    true, DEFAULT_WINDOW_HEIGHT, false);
   SETTING_UINT("video_window_auto_width_max",          &settings->uints.window_auto_width_max,    true, DEFAULT_WINDOW_AUTO_WIDTH_MAX, false);
   SETTING_UINT("video_window_auto_height_max",         &settings->uints.window_auto_height_max,    true, DEFAULT_WINDOW_AUTO_HEIGHT_MAX, false);
   SETTING_UINT("ai_service_mode",            &settings->uints.ai_service_mode,    true, DEFAULT_AI_SERVICE_MODE, false);
   SETTING_UINT("ai_service_target_lang",            &settings->uints.ai_service_target_lang,    true, 0, false);
   SETTING_UINT("ai_service_source_lang",            &settings->uints.ai_service_source_lang,    true, 0, false);

   SETTING_UINT("video_record_threads",            &settings->uints.video_record_threads,    true, DEFAULT_VIDEO_RECORD_THREADS, false);

#ifdef HAVE_LIBNX
   SETTING_UINT("libnx_overclock",  &settings->uints.libnx_overclock, true, SWITCH_DEFAULT_CPU_PROFILE, false);
#endif

#ifdef _3DS
   SETTING_UINT("video_3ds_display_mode",  &settings->uints.video_3ds_display_mode, true, video_3ds_display_mode, false);
#endif

#if defined(DINGUX)
   SETTING_UINT("video_dingux_ipu_filter_type", &settings->uints.video_dingux_ipu_filter_type, true, DEFAULT_DINGUX_IPU_FILTER_TYPE, false);
#if defined(DINGUX_BETA)
   SETTING_UINT("video_dingux_refresh_rate",    &settings->uints.video_dingux_refresh_rate, true, DEFAULT_DINGUX_REFRESH_RATE, false);
#endif
#if defined(RS90) || defined(MIYOO)
   SETTING_UINT("video_dingux_rs90_softfilter_type", &settings->uints.video_dingux_rs90_softfilter_type, true, DEFAULT_DINGUX_RS90_SOFTFILTER_TYPE, false);
#endif
#endif

#ifdef HAVE_MENU
   SETTING_UINT("playlist_entry_remove_enable",    &settings->uints.playlist_entry_remove_enable, true, DEFAULT_PLAYLIST_ENTRY_REMOVE_ENABLE, false);
   SETTING_UINT("playlist_show_inline_core_name",  &settings->uints.playlist_show_inline_core_name, true, DEFAULT_PLAYLIST_SHOW_INLINE_CORE_NAME, false);
   SETTING_UINT("playlist_show_history_icons",     &settings->uints.playlist_show_history_icons, true, DEFAULT_PLAYLIST_SHOW_HISTORY_ICONS, false);
   SETTING_UINT("playlist_sublabel_runtime_type",  &settings->uints.playlist_sublabel_runtime_type, true, DEFAULT_PLAYLIST_SUBLABEL_RUNTIME_TYPE, false);
   SETTING_UINT("playlist_sublabel_last_played_style", &settings->uints.playlist_sublabel_last_played_style, true, DEFAULT_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE, false);

   SETTING_UINT("quit_on_close_content",           &settings->uints.quit_on_close_content, true, DEFAULT_QUIT_ON_CLOSE_CONTENT, false);
#endif

   SETTING_UINT("core_updater_auto_backup_history_size", &settings->uints.core_updater_auto_backup_history_size, true, DEFAULT_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE, false);

   SETTING_UINT("video_black_frame_insertion",   &settings->uints.video_black_frame_insertion, true, DEFAULT_BLACK_FRAME_INSERTION, false);

#ifdef HAVE_LAKKA
   SETTING_UINT("cpu_scaling_mode",            &settings->uints.cpu_scaling_mode,    true,   0, false);
   SETTING_UINT("cpu_min_freq",                &settings->uints.cpu_min_freq,        true,   1, false);
   SETTING_UINT("cpu_max_freq",                &settings->uints.cpu_max_freq,        true, ~0U, false);
#endif

   *size = count;

   return tmp;
}

static struct config_size_setting *populate_settings_size(
      settings_t *settings, int *size)
{
   unsigned count                     = 0;
   struct config_size_setting  *tmp   = (struct config_size_setting*)calloc((*size + 1), sizeof(struct config_size_setting));

   if (!tmp)
      return NULL;

   SETTING_SIZE("rewind_buffer_size",           &settings->sizes.rewind_buffer_size, true, DEFAULT_REWIND_BUFFER_SIZE, false);

   *size = count;

   return tmp;
}

static struct config_int_setting *populate_settings_int(
      settings_t *settings, int *size)
{
   unsigned count                     = 0;
   struct config_int_setting  *tmp    = (struct config_int_setting*)calloc((*size + 1), sizeof(struct config_int_setting));

   if (!tmp)
      return NULL;

   SETTING_INT("state_slot",                   &settings->ints.state_slot, false, 0 /* TODO */, false);
#ifdef HAVE_NETWORKING
   SETTING_INT("netplay_check_frames",         &settings->ints.netplay_check_frames, true, netplay_check_frames, false);
   SETTING_OVERRIDE(RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES);
#endif
#ifdef HAVE_WASAPI
   SETTING_INT("audio_wasapi_sh_buffer_length", &settings->ints.audio_wasapi_sh_buffer_length, true, DEFAULT_WASAPI_SH_BUFFER_LENGTH, false);
#endif
   SETTING_INT("crt_switch_center_adjust",      &settings->ints.crt_switch_center_adjust, false, DEFAULT_CRT_SWITCH_CENTER_ADJUST, false);
   SETTING_INT("crt_switch_porch_adjust",      &settings->ints.crt_switch_porch_adjust, false, DEFAULT_CRT_SWITCH_PORCH_ADJUST, false);
#ifdef HAVE_VULKAN
   SETTING_INT("vulkan_gpu_index",              &settings->ints.vulkan_gpu_index, true, DEFAULT_VULKAN_GPU_INDEX, false);
#endif
#ifdef HAVE_D3D10
   SETTING_INT("d3d10_gpu_index",              &settings->ints.d3d10_gpu_index, true, DEFAULT_D3D10_GPU_INDEX, false);
#endif
#ifdef HAVE_D3D11
   SETTING_INT("d3d11_gpu_index",              &settings->ints.d3d11_gpu_index, true, DEFAULT_D3D11_GPU_INDEX, false);
#endif
#ifdef HAVE_D3D12
   SETTING_INT("d3d12_gpu_index",              &settings->ints.d3d12_gpu_index, true, DEFAULT_D3D12_GPU_INDEX, false);
#endif
#ifdef HAVE_WINDOW_OFFSET
   SETTING_INT("video_window_offset_x",        &settings->ints.video_window_offset_x, true, DEFAULT_WINDOW_OFFSET_X, false);
   SETTING_INT("video_window_offset_y",        &settings->ints.video_window_offset_y, true, DEFAULT_WINDOW_OFFSET_Y, false);
#endif
   SETTING_INT("content_favorites_size",       &settings->ints.content_favorites_size, true, default_content_favorites_size, false);

   *size = count;

   return tmp;
}

static void video_driver_default_settings(global_t *global)
{
   if (!global)
      return;

   global->console.screen.gamma_correction       = DEFAULT_GAMMA;
   global->console.flickerfilter_enable          = false;
   global->console.softfilter_enable             = false;

   global->console.screen.resolutions.current.id = 0;
}

/**
 * config_set_defaults:
 *
 * Set 'default' configuration values.
 **/
void config_set_defaults(void *data)
{
   unsigned i, j;
#ifdef HAVE_MENU
   static bool first_initialized   = true;
#endif
   global_t *global                = (global_t*)data;
   settings_t *settings            = config_st;
   recording_state_t *recording_st = recording_state_get_ptr();
   int bool_settings_size          = sizeof(settings->bools)   / sizeof(settings->bools.placeholder);
   int float_settings_size         = sizeof(settings->floats)  / sizeof(settings->floats.placeholder);
   int int_settings_size           = sizeof(settings->ints)    / sizeof(settings->ints.placeholder);
   int uint_settings_size          = sizeof(settings->uints)   / sizeof(settings->uints.placeholder);
   int size_settings_size          = sizeof(settings->sizes)   / sizeof(settings->sizes.placeholder);
   const char *def_video           = config_get_default_video();
   const char *def_audio           = config_get_default_audio();
   const char *def_audio_resampler = config_get_default_audio_resampler();
   const char *def_input           = config_get_default_input();
   const char *def_joypad          = config_get_default_joypad();
#ifdef HAVE_MENU
   const char *def_menu            = config_get_default_menu();
#endif
   const char *def_camera          = config_get_default_camera();
   const char *def_bluetooth       = config_get_default_bluetooth();
   const char *def_wifi            = config_get_default_wifi();
   const char *def_led             = config_get_default_led();
   const char *def_location        = config_get_default_location();
   const char *def_record          = config_get_default_record();
   const char *def_midi            = config_get_default_midi();
   const char *def_mitm            = DEFAULT_NETPLAY_MITM_SERVER;
   struct config_float_setting      *float_settings = populate_settings_float  (settings, &float_settings_size);
   struct config_bool_setting       *bool_settings  = populate_settings_bool  (settings, &bool_settings_size);
   struct config_int_setting        *int_settings   = populate_settings_int   (settings, &int_settings_size);
   struct config_uint_setting       *uint_settings  = populate_settings_uint   (settings, &uint_settings_size);
   struct config_size_setting       *size_settings  = populate_settings_size   (settings, &size_settings_size);

   if (bool_settings && (bool_settings_size > 0))
   {
      for (i = 0; i < (unsigned)bool_settings_size; i++)
      {
         if (bool_settings[i].def_enable)
            *bool_settings[i].ptr = bool_settings[i].def;
      }

      free(bool_settings);
   }

   if (int_settings && (int_settings_size > 0))
   {
      for (i = 0; i < (unsigned)int_settings_size; i++)
      {
         if (int_settings[i].def_enable)
            *int_settings[i].ptr = int_settings[i].def;
      }

      free(int_settings);
   }

   if (uint_settings && (uint_settings_size > 0))
   {
      for (i = 0; i < (unsigned)uint_settings_size; i++)
      {
         if (uint_settings[i].def_enable)
            *uint_settings[i].ptr = uint_settings[i].def;
      }

      free(uint_settings);
   }

   if (size_settings && (size_settings_size > 0))
   {
      for (i = 0; i < (unsigned)size_settings_size; i++)
      {
         if (size_settings[i].def_enable)
            *size_settings[i].ptr = size_settings[i].def;
      }

      free(size_settings);
   }

   if (float_settings && (float_settings_size > 0))
   {
      for (i = 0; i < (unsigned)float_settings_size; i++)
      {
         if (float_settings[i].def_enable)
            *float_settings[i].ptr = float_settings[i].def;
      }

      free(float_settings);
   }

   if (def_camera)
      configuration_set_string(settings,
            settings->arrays.camera_driver,
            def_camera);
   if (def_bluetooth)
      configuration_set_string(settings,
            settings->arrays.bluetooth_driver,
            def_bluetooth);
   if (def_wifi)
      configuration_set_string(settings,
            settings->arrays.wifi_driver,
            def_wifi);
   if (def_led)
      configuration_set_string(settings,
            settings->arrays.led_driver,
            def_led);
   if (def_location)
      configuration_set_string(settings,
            settings->arrays.location_driver,
            def_location);
   if (def_video)
      configuration_set_string(settings,
            settings->arrays.video_driver,
            def_video);
   if (def_audio)
      configuration_set_string(settings,
            settings->arrays.audio_driver,
            def_audio);
   if (def_audio_resampler)
      configuration_set_string(settings,
            settings->arrays.audio_resampler,
            def_audio_resampler);
   if (def_input)
      configuration_set_string(settings,
            settings->arrays.input_driver,
            def_input);
   if (def_joypad)
      configuration_set_string(settings,
            settings->arrays.input_joypad_driver,
            def_joypad);
   if (def_record)
      configuration_set_string(settings,
            settings->arrays.record_driver,
            def_record);
   if (def_midi)
      configuration_set_string(settings,
            settings->arrays.midi_driver,
            def_midi);
   if (def_mitm)
      configuration_set_string(settings,
            settings->arrays.netplay_mitm_server,
            def_mitm);
#ifdef HAVE_MENU
   if (def_menu)
      configuration_set_string(settings,
            settings->arrays.menu_driver,
            def_menu);
#ifdef HAVE_XMB
   *settings->paths.path_menu_xmb_font            = '\0';
#endif

   configuration_set_string(settings,
         settings->arrays.discord_app_id,
         DEFAULT_DISCORD_APP_ID);

   configuration_set_string(settings,
         settings->arrays.ai_service_url,
         DEFAULT_AI_SERVICE_URL);

#ifdef HAVE_CHEEVOS
   configuration_set_string(settings,
         settings->arrays.cheevos_leaderboards_enable,
         "true");
#endif

#ifdef HAVE_MATERIALUI
   if (g_defaults.menu_materialui_menu_color_theme_enable)
      settings->uints.menu_materialui_color_theme = g_defaults.menu_materialui_menu_color_theme;
#endif
#endif

   settings->floats.video_scale                = DEFAULT_SCALE;

   video_driver_set_threaded(DEFAULT_VIDEO_THREADED);

   settings->floats.video_msg_color_r          = ((message_color >> 16) & 0xff) / 255.0f;
   settings->floats.video_msg_color_g          = ((message_color >>  8) & 0xff) / 255.0f;
   settings->floats.video_msg_color_b          = ((message_color >>  0) & 0xff) / 255.0f;

   if (g_defaults.settings_video_refresh_rate > 0.0 &&
         g_defaults.settings_video_refresh_rate != DEFAULT_REFRESH_RATE)
      settings->floats.video_refresh_rate      = g_defaults.settings_video_refresh_rate;

   if (DEFAULT_AUDIO_DEVICE)
      configuration_set_string(settings,
            settings->arrays.audio_device,
            DEFAULT_AUDIO_DEVICE);

   if (!g_defaults.settings_out_latency)
      g_defaults.settings_out_latency          = DEFAULT_OUT_LATENCY;

   settings->uints.audio_latency               = g_defaults.settings_out_latency;

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, settings->floats.audio_volume);
#ifdef HAVE_AUDIOMIXER
   audio_set_float(AUDIO_ACTION_MIXER_VOLUME_GAIN, settings->floats.audio_mixer_volume);
#endif

#ifdef HAVE_LAKKA
   configuration_set_bool(settings,
         settings->bools.ssh_enable, filestream_exists(LAKKA_SSH_PATH));
   configuration_set_bool(settings,
         settings->bools.samba_enable, filestream_exists(LAKKA_SAMBA_PATH));
   configuration_set_bool(settings,
         settings->bools.bluetooth_enable, filestream_exists(LAKKA_BLUETOOTH_PATH));
   configuration_set_bool(settings, settings->bools.localap_enable, false);
   load_timezone(settings->arrays.timezone);
#endif

#ifdef HAVE_MENU
   if (first_initialized)
      configuration_set_bool(settings,
            settings->bools.menu_show_start_screen,
            DEFAULT_MENU_SHOW_START_SCREEN);
#endif

#ifdef HAVE_CHEEVOS
   *settings->arrays.cheevos_username                 = '\0';
   *settings->arrays.cheevos_password                 = '\0';
   *settings->arrays.cheevos_token                    = '\0';
#endif

   input_config_reset();
   input_remapping_deinit(false);
   input_remapping_set_defaults(false);

   /* Verify that binds are in proper order. */
   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         const struct retro_keybind *keyval = &input_config_binds[i][j];
         if (keyval->valid)
            retro_assert(j == keyval->id);
      }
   }

   configuration_set_string(settings,
         settings->paths.network_buildbot_url, DEFAULT_BUILDBOT_SERVER_URL);
   configuration_set_string(settings,
         settings->paths.network_buildbot_assets_url,
         DEFAULT_BUILDBOT_ASSETS_SERVER_URL);

   *settings->arrays.input_keyboard_layout                = '\0';

   for (i = 0; i < MAX_USERS; i++)
   {
      settings->uints.input_joypad_index[i] = i;
#ifdef SWITCH /* Switch prefered default dpad mode */
      settings->uints.input_analog_dpad_mode[i] = ANALOG_DPAD_LSTICK;
#else
      settings->uints.input_analog_dpad_mode[i] = ANALOG_DPAD_NONE;
#endif
      input_config_set_device(i, RETRO_DEVICE_JOYPAD);
      settings->uints.input_mouse_index[i] = i;
   }

   video_driver_reset_custom_viewport(settings);

   /* Make sure settings from other configs carry over into defaults
    * for another config. */
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
      dir_clear(RARCH_DIR_SAVEFILE);
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
      dir_clear(RARCH_DIR_SAVESTATE);

   *settings->paths.path_libretro_info = '\0';
   *settings->paths.directory_libretro = '\0';
   *settings->paths.directory_cursor = '\0';
   *settings->paths.directory_resampler = '\0';
   *settings->paths.directory_screenshot = '\0';
   *settings->paths.directory_system = '\0';
   *settings->paths.directory_cache = '\0';
   *settings->paths.directory_input_remapping = '\0';
   *settings->paths.directory_core_assets = '\0';
   *settings->paths.directory_assets = '\0';
   *settings->paths.directory_dynamic_wallpapers = '\0';
   *settings->paths.directory_thumbnails = '\0';
   *settings->paths.directory_playlist = '\0';
   *settings->paths.directory_content_favorites = '\0';
   *settings->paths.directory_content_history = '\0';
   *settings->paths.directory_content_image_history = '\0';
   *settings->paths.directory_content_music_history = '\0';
   *settings->paths.directory_content_video_history = '\0';
   *settings->paths.directory_runtime_log = '\0';
   *settings->paths.directory_autoconfig = '\0';
#ifdef HAVE_MENU
   *settings->paths.directory_menu_content = '\0';
   *settings->paths.directory_menu_config = '\0';
#endif
   *settings->paths.directory_video_shader = '\0';
   *settings->paths.directory_video_filter = '\0';
   *settings->paths.directory_audio_filter = '\0';

   retroarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   retroarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   retroarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);

   *recording_st->output_dir                     = '\0';
   *recording_st->config_dir                     = '\0';

   *settings->paths.path_core_options            = '\0';
   *settings->paths.path_content_favorites       = '\0';
   *settings->paths.path_content_history         = '\0';
   *settings->paths.path_content_image_history   = '\0';
   *settings->paths.path_content_music_history   = '\0';
   *settings->paths.path_content_video_history   = '\0';
   *settings->paths.path_cheat_settings          = '\0';
#if !defined(__APPLE__)
   *settings->arrays.bundle_assets_src           = '\0';
   *settings->arrays.bundle_assets_dst           = '\0';
   *settings->arrays.bundle_assets_dst_subdir    = '\0';
#endif
   *settings->paths.path_cheat_database    = '\0';
   *settings->paths.path_menu_wallpaper    = '\0';
   *settings->paths.path_rgui_theme_preset = '\0';
   *settings->paths.path_content_database  = '\0';
   *settings->paths.path_overlay           = '\0';
#ifdef HAVE_VIDEO_LAYOUT
   *settings->paths.path_video_layout      = '\0';
#endif
   *settings->paths.path_record_config     = '\0';
   *settings->paths.path_stream_config     = '\0';
   *settings->paths.path_stream_url        = '\0';
   *settings->paths.path_softfilter_plugin = '\0';

   *settings->paths.path_audio_dsp_plugin = '\0';

   *settings->paths.log_dir = '\0';

   video_driver_default_settings(global);

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_WALLPAPERS]))
      configuration_set_string(settings,
            settings->paths.directory_dynamic_wallpapers,
            g_defaults.dirs[DEFAULT_DIR_WALLPAPERS]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]))
      configuration_set_string(settings,
            settings->paths.directory_thumbnails,
            g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_REMAP]))
      configuration_set_string(settings,
            settings->paths.directory_input_remapping,
            g_defaults.dirs[DEFAULT_DIR_REMAP]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CACHE]))
      configuration_set_string(settings,
            settings->paths.directory_cache,
            g_defaults.dirs[DEFAULT_DIR_CACHE]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_ASSETS]))
      configuration_set_string(settings,
            settings->paths.directory_assets,
            g_defaults.dirs[DEFAULT_DIR_ASSETS]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]))
      configuration_set_string(settings,
            settings->paths.directory_core_assets,
            g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]))
      configuration_set_string(settings,
            settings->paths.directory_playlist,
            g_defaults.dirs[DEFAULT_DIR_PLAYLIST]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CONTENT_FAVORITES]))
      configuration_set_string(settings,
            settings->paths.directory_content_favorites,
            g_defaults.dirs[DEFAULT_DIR_CONTENT_FAVORITES]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]))
      configuration_set_string(settings,
            settings->paths.directory_content_history,
            g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CONTENT_IMAGE_HISTORY]))
      configuration_set_string(settings,
            settings->paths.directory_content_image_history,
            g_defaults.dirs[DEFAULT_DIR_CONTENT_IMAGE_HISTORY]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CONTENT_MUSIC_HISTORY]))
      configuration_set_string(settings,
            settings->paths.directory_content_music_history,
            g_defaults.dirs[DEFAULT_DIR_CONTENT_MUSIC_HISTORY]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CONTENT_VIDEO_HISTORY]))
      configuration_set_string(settings,
            settings->paths.directory_content_video_history,
            g_defaults.dirs[DEFAULT_DIR_CONTENT_VIDEO_HISTORY]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CORE]))
      fill_pathname_expand_special(settings->paths.directory_libretro,
            g_defaults.dirs[DEFAULT_DIR_CORE],
            sizeof(settings->paths.directory_libretro));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]))
      configuration_set_string(settings,
            settings->paths.directory_audio_filter,
            g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]))
      configuration_set_string(settings,
            settings->paths.directory_video_filter,
            g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SHADER]))
      fill_pathname_expand_special(settings->paths.directory_video_shader,
            g_defaults.dirs[DEFAULT_DIR_SHADER],
            sizeof(settings->paths.directory_video_shader));

   if (!string_is_empty(g_defaults.path_buildbot_server_url))
      configuration_set_string(settings,
            settings->paths.network_buildbot_url,
            g_defaults.path_buildbot_server_url);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_DATABASE]))
      configuration_set_string(settings,
            settings->paths.path_content_database,
            g_defaults.dirs[DEFAULT_DIR_DATABASE]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CURSOR]))
      configuration_set_string(settings,
            settings->paths.directory_cursor,
            g_defaults.dirs[DEFAULT_DIR_CURSOR]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CHEATS]))
      configuration_set_string(settings,
            settings->paths.path_cheat_database,
            g_defaults.dirs[DEFAULT_DIR_CHEATS]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]))
      fill_pathname_expand_special(settings->paths.path_libretro_info,
            g_defaults.dirs[DEFAULT_DIR_CORE_INFO],
            sizeof(settings->paths.path_libretro_info));
#ifdef HAVE_OVERLAY
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_OVERLAY]))
   {
      fill_pathname_expand_special(settings->paths.directory_overlay,
            g_defaults.dirs[DEFAULT_DIR_OVERLAY],
            sizeof(settings->paths.directory_overlay));
#ifdef RARCH_MOBILE
      if (string_is_empty(settings->paths.path_overlay))
      {
         fill_pathname_join(settings->paths.path_overlay,
               settings->paths.directory_overlay,
               FILE_PATH_DEFAULT_OVERLAY,
               sizeof(settings->paths.path_overlay));
      }
#endif
   }
#endif
#ifdef HAVE_VIDEO_LAYOUT
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT]))
   {
      fill_pathname_expand_special(settings->paths.directory_video_layout,
            g_defaults.dirs[DEFAULT_DIR_VIDEO_LAYOUT],
            sizeof(settings->paths.directory_video_layout));
   }
#endif

#ifdef HAVE_MENU
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]))
   {
      configuration_set_string(settings,
            settings->paths.directory_menu_config,
            g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]);
#if TARGET_OS_IPHONE
      {
         char config_file_path[PATH_MAX_LENGTH];

         config_file_path[0]           = '\0';

         fill_pathname_join(config_file_path,
               settings->paths.directory_menu_config,
               FILE_PATH_MAIN_CONFIG,
               sizeof(config_file_path));
         path_set(RARCH_PATH_CONFIG,
               config_file_path);
      }
#endif
   }

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT]))
      configuration_set_string(settings,
            settings->paths.directory_menu_content,
            g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT]);
#endif
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]))
      configuration_set_string(settings,
            settings->paths.directory_autoconfig,
            g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]);

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]))
      dir_set(RARCH_DIR_SAVESTATE, g_defaults.dirs[DEFAULT_DIR_SAVESTATE]);

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SRAM]))
      dir_set(RARCH_DIR_SAVEFILE, g_defaults.dirs[DEFAULT_DIR_SRAM]);

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SYSTEM]))
      configuration_set_string(settings,
            settings->paths.directory_system,
            g_defaults.dirs[DEFAULT_DIR_SYSTEM]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]))
      configuration_set_string(settings,
            settings->paths.directory_screenshot,
            g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_RESAMPLER]))
      configuration_set_string(settings,
            settings->paths.directory_resampler,
            g_defaults.dirs[DEFAULT_DIR_RESAMPLER]);
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_LOGS]))
      configuration_set_string(settings,
            settings->paths.log_dir,
            g_defaults.dirs[DEFAULT_DIR_LOGS]);

   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT]))
      fill_pathname_expand_special(recording_st->output_dir,
            g_defaults.dirs[DEFAULT_DIR_RECORD_OUTPUT],
            sizeof(recording_st->output_dir));
   if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG]))
      fill_pathname_expand_special(recording_st->config_dir,
            g_defaults.dirs[DEFAULT_DIR_RECORD_CONFIG],
            sizeof(recording_st->config_dir));

   if (!string_is_empty(g_defaults.path_config))
   {
      char temp_str[PATH_MAX_LENGTH];

      temp_str[0] = '\0';

      fill_pathname_expand_special(temp_str,
            g_defaults.path_config,
            sizeof(temp_str));
      path_set(RARCH_PATH_CONFIG, temp_str);
   }

   configuration_set_string(settings,
         settings->arrays.midi_input,
         DEFAULT_MIDI_INPUT);
   configuration_set_string(settings,
         settings->arrays.midi_output,
         DEFAULT_MIDI_OUTPUT);

#ifdef HAVE_CONFIGFILE
   /* Avoid reloading config on every content load */
   if (DEFAULT_BLOCK_CONFIG_READ)
      retroarch_ctl(RARCH_CTL_SET_BLOCK_CONFIG_READ, NULL);
   else
      retroarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);
#endif

#ifdef HAVE_MENU
   first_initialized = false;
#endif
}

/**
 * config_load:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_load(void *data)
{
   global_t *global = (global_t*)data;
   config_set_defaults(global);
#ifdef HAVE_CONFIGFILE
   config_parse_file(global);
#endif
}

#ifdef HAVE_CONFIGFILE
#if defined(HAVE_MENU) && defined(HAVE_RGUI)
static bool check_menu_driver_compatibility(settings_t *settings)
{
   char *video_driver   = settings->arrays.video_driver;
   char *menu_driver    = settings->arrays.menu_driver;

   if (  string_is_equal(menu_driver,  "rgui") ||
         string_is_equal(menu_driver,  "null") ||
         string_is_equal(video_driver, "null"))
      return true;

   /* TODO/FIXME - maintenance hazard */
   if (string_starts_with_size(video_driver, "d3d", STRLEN_CONST("d3d")))
      if (
            string_is_equal(video_driver, "d3d9")   ||
            string_is_equal(video_driver, "d3d10")  ||
            string_is_equal(video_driver, "d3d11")  ||
            string_is_equal(video_driver, "d3d12")
         )
      return true;
   if (string_starts_with_size(video_driver, "gl", STRLEN_CONST("gl")))
      if (
            string_is_equal(video_driver, "gl")     ||
            string_is_equal(video_driver, "gl1")    ||
            string_is_equal(video_driver, "glcore")
         )
         return true;
   if (
         string_is_equal(video_driver, "caca")   ||
         string_is_equal(video_driver, "gdi")    ||
         string_is_equal(video_driver, "gx2")    ||
         string_is_equal(video_driver, "vulkan") ||
         string_is_equal(video_driver, "metal")  ||
         string_is_equal(video_driver, "ctr")    ||
         string_is_equal(video_driver, "vita2d")
      )
      return true;

   return false;
}
#endif

/**
 * open_default_config_file
 *
 * Open a default config file. Platform-specific.
 *
 * Returns: handle to config file if found, otherwise NULL.
 **/
static config_file_t *open_default_config_file(void)
{
   char conf_path[PATH_MAX_LENGTH];
   char app_path[PATH_MAX_LENGTH];
   config_file_t *conf                    = NULL;

   #ifndef RARCH_CONSOLE
   char application_data[PATH_MAX_LENGTH];
   bool has_application_data              = false;
   application_data[0] = '\0';
   #endif

   conf_path[0] = app_path[0] = '\0';

#if defined(_WIN32) && !defined(_XBOX)
#if defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
   /* On UWP, the app install directory is not writable so use the writable LocalState dir instead */
   fill_pathname_home_dir(app_path, sizeof(app_path));
#else
   fill_pathname_application_dir(app_path, sizeof(app_path));
#endif
   fill_pathname_resolve_relative(conf_path, app_path,
         FILE_PATH_MAIN_CONFIG, sizeof(conf_path));

   conf = config_file_new_from_path_to_string(conf_path);

   if (!conf)
   {
      if (fill_pathname_application_data(application_data,
            sizeof(application_data)))
      {
         fill_pathname_join(conf_path, application_data,
               FILE_PATH_MAIN_CONFIG, sizeof(conf_path));
         conf = config_file_new_from_path_to_string(conf_path);
      }
   }

   if (!conf)
   {
      bool saved = false;

      /* Try to create a new config file. */
      conf = config_file_new_alloc();

      if (conf)
      {
         /* Since this is a clean config file, we can
          * safely use config_save_on_exit. */
         fill_pathname_resolve_relative(conf_path, app_path,
               FILE_PATH_MAIN_CONFIG, sizeof(conf_path));
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path, true);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("[Config]: Failed to create new config file in: \"%s\".\n",
               conf_path);
         goto error;
      }

      RARCH_WARN("[Config]: Created new config file in: \"%s\".\n", conf_path);
   }
#elif defined(OSX)
   if (!fill_pathname_application_data(application_data,
            sizeof(application_data)))
      goto error;

   /* Group config file with menu configs, remaps, etc: */
   strlcat(application_data, "/config", sizeof(application_data));

   path_mkdir(application_data);

   fill_pathname_join(conf_path, application_data,
         FILE_PATH_MAIN_CONFIG, sizeof(conf_path));
   conf = config_file_new_from_path_to_string(conf_path);

   if (!conf)
   {
      bool saved = false;
      conf       = config_file_new_alloc();

      if (conf)
      {
         config_set_bool(conf, "config_save_on_exit", true);
         saved = config_file_write(conf, conf_path, true);
      }

      if (!saved)
      {
         /* WARN here to make sure user has a good chance of seeing it. */
         RARCH_ERR("Failed to create new config file in: \"%s\".\n",
               conf_path);
         goto error;
      }

      RARCH_WARN("Created new config file in: \"%s\".\n", conf_path);
   }
#elif !defined(RARCH_CONSOLE)
   has_application_data =
      fill_pathname_application_data(application_data,
            sizeof(application_data));

   if (has_application_data)
   {
      fill_pathname_join(conf_path, application_data,
            FILE_PATH_MAIN_CONFIG, sizeof(conf_path));
      RARCH_LOG("[Config]: Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new_from_path_to_string(conf_path);
   }

   /* Fallback to $HOME/.retroarch.cfg. */
   if (!conf && getenv("HOME"))
   {
      fill_pathname_join(conf_path, getenv("HOME"),
            "." FILE_PATH_MAIN_CONFIG, sizeof(conf_path));
      RARCH_LOG("[Config]: Looking for config in: \"%s\".\n", conf_path);
      conf = config_file_new_from_path_to_string(conf_path);
   }

   if (!conf && has_application_data)
   {
      bool dir_created = false;
      char basedir[PATH_MAX_LENGTH];

      basedir[0]       = '\0';

      /* Try to create a new config file. */

      fill_pathname_basedir(basedir, application_data, sizeof(basedir));
      fill_pathname_join(conf_path, application_data,
            FILE_PATH_MAIN_CONFIG, sizeof(conf_path));

      dir_created = path_mkdir(basedir);

      if (dir_created)
      {
         char skeleton_conf[PATH_MAX_LENGTH];
         bool saved          = false;

         skeleton_conf[0] = '\0';

         /* Build a retroarch.cfg path from the
          * global config directory (/etc). */
         fill_pathname_join(skeleton_conf, GLOBAL_CONFIG_DIR,
            FILE_PATH_MAIN_CONFIG, sizeof(skeleton_conf));

         conf = config_file_new_from_path_to_string(skeleton_conf);
         if (conf)
            RARCH_WARN("[Config]: Using skeleton config \"%s\" as base for a new config file.\n", skeleton_conf);
         else
            conf = config_file_new_alloc();

         if (conf)
         {
            /* Since this is a clean config file, we can
             * safely use config_save_on_exit. */
            config_set_bool(conf, "config_save_on_exit", true);
            saved = config_file_write(conf, conf_path, true);
         }

         if (!saved)
         {
            /* WARN here to make sure user has a good chance of seeing it. */
            RARCH_ERR("[Config]: Failed to create new config file in: \"%s\".\n",
                  conf_path);
            goto error;
         }

         RARCH_WARN("[Config]: Created new config file in: \"%s\".\n",
               conf_path);
      }
   }
#endif

   if (!conf)
      goto error;

   path_set(RARCH_PATH_CONFIG, conf_path);

   return conf;

error:
   if (conf)
      config_file_free(conf);
   return NULL;
}

#ifdef RARCH_CONSOLE
static void video_driver_load_settings(global_t *global,
      config_file_t *conf)
{
   bool               tmp_bool = false;

   CONFIG_GET_INT_BASE(conf, global,
         console.screen.gamma_correction, "gamma_correction");

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
#endif

/**
 * config_load:
 * @path                : path to be read from.
 * @set_defaults        : set default values first before
 *                        reading the values from the config file
 *
 * Loads a config file and reads all the values into memory.
 *
 */
static bool config_load_file(global_t *global,
      const char *path, settings_t *settings)
{
   unsigned i;
   char tmp_str[PATH_MAX_LENGTH];
   unsigned tmp_uint                               = 0;
   bool tmp_bool                                   = false;
   static bool first_load                          = true;
   unsigned msg_color                              = 0;
   char *save                                      = NULL;
   char *override_username                         = NULL;
   const char *path_config                         = NULL;
   runloop_state_t *runloop_st                     = runloop_state_get_ptr();
   int bool_settings_size                          = sizeof(settings->bools)  / sizeof(settings->bools.placeholder);
   int float_settings_size                         = sizeof(settings->floats) / sizeof(settings->floats.placeholder);
   int int_settings_size                           = sizeof(settings->ints)   / sizeof(settings->ints.placeholder);
   int uint_settings_size                          = sizeof(settings->uints)  / sizeof(settings->uints.placeholder);
   int size_settings_size                          = sizeof(settings->sizes)  / sizeof(settings->sizes.placeholder);
   int array_settings_size                         = sizeof(settings->arrays) / sizeof(settings->arrays.placeholder);
   int path_settings_size                          = sizeof(settings->paths)  / sizeof(settings->paths.placeholder);
   struct config_bool_setting *bool_settings       = NULL;
   struct config_float_setting *float_settings     = NULL;
   struct config_int_setting *int_settings         = NULL;
   struct config_uint_setting *uint_settings       = NULL;
   struct config_size_setting *size_settings       = NULL;
   struct config_array_setting *array_settings     = NULL;
   struct config_path_setting *path_settings       = NULL;
   config_file_t *conf                             = path ? config_file_new_from_path_to_string(path) : open_default_config_file();

   tmp_str[0]                                      = '\0';

   if (!conf)
   {
      first_load = false;
      if (!path)
         return true;
      return false;
   }

   bool_settings                                   = populate_settings_bool  (settings, &bool_settings_size);
   float_settings                                  = populate_settings_float (settings, &float_settings_size);
   int_settings                                    = populate_settings_int   (settings, &int_settings_size);
   uint_settings                                   = populate_settings_uint  (settings, &uint_settings_size);
   size_settings                                   = populate_settings_size  (settings, &size_settings_size);
   array_settings                                  = populate_settings_array (settings, &array_settings_size);
   path_settings                                   = populate_settings_path  (settings, &path_settings_size);

   if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
   {
      /* Don't destroy append_config_path, store in temporary
       * variable. */
      char tmp_append_path[PATH_MAX_LENGTH];
      const char *extra_path = NULL;

      tmp_append_path[0] = '\0';

      strlcpy(tmp_append_path, path_get(RARCH_PATH_CONFIG_APPEND),
            sizeof(tmp_append_path));
      extra_path = strtok_r(tmp_append_path, "|", &save);

      while (extra_path)
      {
         bool result = config_append_file(conf, extra_path);

         RARCH_LOG("[Config]: Appending config \"%s\".\n", extra_path);

         if (!result)
            RARCH_ERR("[Config]: Failed to append config \"%s\".\n", extra_path);
         extra_path = strtok_r(NULL, "|", &save);
      }
   }

#if 0
   if (verbosity_is_enabled())
   {
      RARCH_LOG_OUTPUT("=== Config ===\n");
      config_file_dump_all(conf);
      RARCH_LOG_OUTPUT("=== Config end ===\n");
   }
#endif

   /* Overrides */

   if (retroarch_ctl(RARCH_CTL_HAS_SET_USERNAME, NULL))
      override_username = strdup(settings->paths.username);

   /* Boolean settings */

   for (i = 0; i < (unsigned)bool_settings_size; i++)
   {
      bool tmp = false;
      if (config_get_bool(conf, bool_settings[i].ident, &tmp))
         *bool_settings[i].ptr = tmp;
   }

#ifdef HAVE_NETWORKGAMEPAD
   for (i = 0; i < MAX_USERS; i++)
   {
      char tmp[64];

      tmp[0] = '\0';

      snprintf(tmp, sizeof(tmp), "network_remote_enable_user_p%u", i + 1);

      if (config_get_bool(conf, tmp, &tmp_bool))
         configuration_set_bool(settings,
               settings->bools.network_remote_enable_user[i], tmp_bool);
   }
#endif
   /* Set verbosity according to config only if the 'v' command line argument was not used
    * or if it is not the first config load. */
   if (config_get_bool(conf, "log_verbosity", &tmp_bool) &&
      (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL) ||
      !first_load))
   {
      if (tmp_bool)
         verbosity_enable();
      else
         verbosity_disable();
   }
   /* On first config load, make sure log_to_file is true if 'log-file' command line
    * argument was used. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL) &&
      first_load)
   {
      configuration_set_bool(settings,settings->bools.log_to_file,true);
   }
   if (config_get_uint(conf, "frontend_log_level", &tmp_uint))
   {
      verbosity_set_log_level(tmp_uint);
   }

   /* Integer settings */

   for (i = 0; i < (unsigned)int_settings_size; i++)
   {
      int tmp = 0;
      if (config_get_int(conf, int_settings[i].ident, &tmp))
         *int_settings[i].ptr = tmp;
   }

   for (i = 0; i < (unsigned)uint_settings_size; i++)
   {
      int tmp = 0;
      if (config_get_int(conf, uint_settings[i].ident, &tmp))
         *uint_settings[i].ptr = tmp;
   }

   for (i = 0; i < (unsigned)size_settings_size; i++)
   {
      size_t tmp = 0;
      if (config_get_size_t(conf, size_settings[i].ident, &tmp))
         *size_settings[i].ptr = tmp ;
      /* Special case for rewind_buffer_size - need to convert
       * low values to what they were
       * intended to be based on the default value in config.def.h
       * If the value is less than 10000 then multiple by 1MB because if
       * the retroarch.cfg
       * file contains rewind_buffer_size = "100",
       * then that ultimately gets interpreted as
       * 100MB, so ensure the internal values represent that.*/
      if (string_is_equal(size_settings[i].ident, "rewind_buffer_size"))
         if (*size_settings[i].ptr < 10000)
            *size_settings[i].ptr  = *size_settings[i].ptr * 1024 * 1024;
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      char buf[64];

      buf[0] = '\0';

      snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_joypad_index[i], buf);

      snprintf(buf, sizeof(buf), "input_player%u_analog_dpad_mode", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_analog_dpad_mode[i], buf);

      snprintf(buf, sizeof(buf), "input_player%u_mouse_index", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_mouse_index[i], buf);
   }

   /* LED map for use by the led driver */
   for (i = 0; i < MAX_LEDS; i++)
   {
      char buf[64];

      buf[0] = '\0';

      snprintf(buf, sizeof(buf), "led%u_map", i + 1);

      /* TODO/FIXME - change of sign - led_map is unsigned */
      settings->uints.led_map[i] = -1;

      CONFIG_GET_INT_BASE(conf, settings, uints.led_map[i], buf);
   }

   /* Hexadecimal settings  */

   if (config_get_hex(conf, "video_message_color", &msg_color))
   {
      settings->floats.video_msg_color_r = ((msg_color >> 16) & 0xff) / 255.0f;
      settings->floats.video_msg_color_g = ((msg_color >>  8) & 0xff) / 255.0f;
      settings->floats.video_msg_color_b = ((msg_color >>  0) & 0xff) / 255.0f;
   }

   /* Float settings */
   for (i = 0; i < (unsigned)float_settings_size; i++)
   {
      float tmp = 0.0f;
      if (config_get_float(conf, float_settings[i].ident, &tmp))
         *float_settings[i].ptr = tmp;
   }

   /* Array settings  */
   for (i = 0; i < (unsigned)array_settings_size; i++)
   {
      if (!array_settings[i].handle)
         continue;
      config_get_array(conf, array_settings[i].ident,
            array_settings[i].ptr, PATH_MAX_LENGTH);
   }

   /* Path settings  */
   for (i = 0; i < (unsigned)path_settings_size; i++)
   {
      if (!path_settings[i].handle)
         continue;
      if (config_get_path(conf, path_settings[i].ident, tmp_str, sizeof(tmp_str)))
         strlcpy(path_settings[i].ptr, tmp_str, PATH_MAX_LENGTH);
   }

   if (config_get_path(conf, "libretro_directory", tmp_str, sizeof(tmp_str)))
      configuration_set_string(settings,
            settings->paths.directory_libretro, tmp_str);

#ifdef RARCH_CONSOLE
   if (conf)
      video_driver_load_settings(global, conf);
#endif

   /* Post-settings load */

   if (retroarch_ctl(RARCH_CTL_HAS_SET_USERNAME, NULL) && override_username)
   {
      configuration_set_string(settings,
            settings->paths.username,
            override_username);
      free(override_username);
   }

   if (settings->uints.video_hard_sync_frames > 3)
      settings->uints.video_hard_sync_frames = 3;

   if (settings->uints.video_frame_delay > MAXIMUM_FRAME_DELAY)
      settings->uints.video_frame_delay = MAXIMUM_FRAME_DELAY;

   settings->uints.video_swap_interval = MAX(settings->uints.video_swap_interval, 1);
   settings->uints.video_swap_interval = MIN(settings->uints.video_swap_interval, 4);

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, settings->floats.audio_volume);
#ifdef HAVE_AUDIOMIXER
   audio_set_float(AUDIO_ACTION_MIXER_VOLUME_GAIN, settings->floats.audio_mixer_volume);
#endif

   path_config = path_get(RARCH_PATH_CONFIG);

   if (string_is_empty(settings->paths.path_content_favorites))
         strlcpy(settings->paths.directory_content_favorites, "default", sizeof(settings->paths.directory_content_favorites));

   if (string_is_empty(settings->paths.directory_content_favorites) || string_is_equal(settings->paths.directory_content_favorites, "default"))
         fill_pathname_resolve_relative(
               settings->paths.path_content_favorites,
               path_config,
               FILE_PATH_CONTENT_FAVORITES,
               sizeof(settings->paths.path_content_favorites));
   else
         fill_pathname_join(
               settings->paths.path_content_favorites,
               settings->paths.directory_content_favorites,
               FILE_PATH_CONTENT_FAVORITES,
               sizeof(settings->paths.path_content_favorites));

   if (string_is_empty(settings->paths.path_content_history))
         strlcpy(settings->paths.directory_content_history, "default", sizeof(settings->paths.directory_content_history));

   if (string_is_empty(settings->paths.directory_content_history) || string_is_equal(settings->paths.directory_content_history, "default"))
         fill_pathname_resolve_relative(
               settings->paths.path_content_history,
               path_config,
               FILE_PATH_CONTENT_HISTORY,
               sizeof(settings->paths.path_content_history));
   else
         fill_pathname_join(
               settings->paths.path_content_history,
               settings->paths.directory_content_history,
               FILE_PATH_CONTENT_HISTORY,
               sizeof(settings->paths.path_content_history));

   if (string_is_empty(settings->paths.path_content_image_history))
         strlcpy(settings->paths.directory_content_image_history, "default", sizeof(settings->paths.directory_content_image_history));

   if (string_is_empty(settings->paths.directory_content_image_history) || string_is_equal(settings->paths.directory_content_image_history, "default"))
         fill_pathname_resolve_relative(
               settings->paths.path_content_image_history,
               path_config,
               FILE_PATH_CONTENT_IMAGE_HISTORY,
               sizeof(settings->paths.path_content_image_history));
   else
         fill_pathname_join(
               settings->paths.path_content_image_history,
               settings->paths.directory_content_image_history,
               FILE_PATH_CONTENT_IMAGE_HISTORY,
               sizeof(settings->paths.path_content_image_history));

   if (string_is_empty(settings->paths.path_content_music_history))
         strlcpy(settings->paths.directory_content_music_history, "default", sizeof(settings->paths.directory_content_music_history));

   if (string_is_empty(settings->paths.directory_content_music_history) || string_is_equal(settings->paths.directory_content_music_history, "default"))
         fill_pathname_resolve_relative(
               settings->paths.path_content_music_history,
               path_config,
               FILE_PATH_CONTENT_MUSIC_HISTORY,
               sizeof(settings->paths.path_content_music_history));
   else
         fill_pathname_join(
               settings->paths.path_content_music_history,
               settings->paths.directory_content_music_history,
               FILE_PATH_CONTENT_MUSIC_HISTORY,
               sizeof(settings->paths.path_content_music_history));

   if (string_is_empty(settings->paths.path_content_video_history))
         strlcpy(settings->paths.directory_content_video_history, "default", sizeof(settings->paths.directory_content_video_history));

   if (string_is_empty(settings->paths.directory_content_video_history) || string_is_equal(settings->paths.directory_content_video_history, "default"))
         fill_pathname_resolve_relative(
               settings->paths.path_content_video_history,
               path_config,
               FILE_PATH_CONTENT_VIDEO_HISTORY,
               sizeof(settings->paths.path_content_video_history));
   else
         fill_pathname_join(
               settings->paths.path_content_video_history,
               settings->paths.directory_content_video_history,
               FILE_PATH_CONTENT_VIDEO_HISTORY,
               sizeof(settings->paths.path_content_video_history));

   if (!string_is_empty(settings->paths.directory_screenshot))
   {
      if (string_is_equal(settings->paths.directory_screenshot, "default"))
         *settings->paths.directory_screenshot = '\0';
      else if (!path_is_directory(settings->paths.directory_screenshot))
      {
         RARCH_WARN("[Config]: 'screenshot_directory' is not an existing directory, ignoring ...\n");
         *settings->paths.directory_screenshot = '\0';
      }
   }

#if defined(__APPLE__) && defined(OSX)
#if defined(__aarch64__)
   /* Wrong architecture, set it back to arm64 */
   if (string_is_equal(settings->paths.network_buildbot_url, "http://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/"))
       configuration_set_string(settings,
             settings->paths.network_buildbot_url, DEFAULT_BUILDBOT_SERVER_URL);
#elif defined(__x86_64__)
   /* Wrong architecture, set it back to x86_64 */
   if (string_is_equal(settings->paths.network_buildbot_url, "http://buildbot.libretro.com/nightly/apple/osx/arm64/latest/"))
       configuration_set_string(settings,
             settings->paths.network_buildbot_url, DEFAULT_BUILDBOT_SERVER_URL);
#endif
#endif

   if (string_is_equal(settings->paths.path_menu_wallpaper, "default"))
      *settings->paths.path_menu_wallpaper = '\0';
   if (string_is_equal(settings->paths.path_rgui_theme_preset, "default"))
      *settings->paths.path_rgui_theme_preset = '\0';
   if (string_is_equal(settings->paths.directory_video_shader, "default"))
      *settings->paths.directory_video_shader = '\0';
   if (string_is_equal(settings->paths.directory_video_filter, "default"))
      *settings->paths.directory_video_filter = '\0';
   if (string_is_equal(settings->paths.directory_audio_filter, "default"))
      *settings->paths.directory_audio_filter = '\0';
   if (string_is_equal(settings->paths.directory_core_assets, "default"))
      *settings->paths.directory_core_assets = '\0';
   if (string_is_equal(settings->paths.directory_assets, "default"))
      *settings->paths.directory_assets = '\0';
   if (string_is_equal(settings->paths.directory_dynamic_wallpapers, "default"))
      *settings->paths.directory_dynamic_wallpapers = '\0';
   if (string_is_equal(settings->paths.directory_thumbnails, "default"))
      *settings->paths.directory_thumbnails = '\0';
   if (string_is_equal(settings->paths.directory_playlist, "default"))
      *settings->paths.directory_playlist = '\0';
   if (string_is_equal(settings->paths.directory_content_favorites, "default"))
      *settings->paths.directory_content_favorites = '\0';
   if (string_is_equal(settings->paths.directory_content_history, "default"))
      *settings->paths.directory_content_history = '\0';
   if (string_is_equal(settings->paths.directory_content_image_history, "default"))
      *settings->paths.directory_content_image_history = '\0';
   if (string_is_equal(settings->paths.directory_content_music_history, "default"))
      *settings->paths.directory_content_music_history = '\0';
   if (string_is_equal(settings->paths.directory_content_video_history, "default"))
      *settings->paths.directory_content_video_history = '\0';
   if (string_is_equal(settings->paths.directory_runtime_log, "default"))
      *settings->paths.directory_runtime_log = '\0';
#ifdef HAVE_MENU
   if (string_is_equal(settings->paths.directory_menu_content, "default"))
      *settings->paths.directory_menu_content = '\0';
   if (string_is_equal(settings->paths.directory_menu_config, "default"))
      *settings->paths.directory_menu_config = '\0';
#endif
#ifdef HAVE_OVERLAY
   if (string_is_equal(settings->paths.directory_overlay, "default"))
      *settings->paths.directory_overlay = '\0';
#endif
#ifdef HAVE_VIDEO_LAYOUT
   if (string_is_equal(settings->paths.directory_video_layout, "default"))
      *settings->paths.directory_video_layout = '\0';
#endif
   if (string_is_equal(settings->paths.directory_system, "default"))
      *settings->paths.directory_system = '\0';

   /* Log directory is a special case, since it must contain
    * a valid path as soon as possible - if config file
    * value is 'default' must copy g_defaults.dirs[DEFAULT_DIR_LOGS]
    * directly... */
   if (string_is_equal(settings->paths.log_dir, "default"))
   {
      if (!string_is_empty(g_defaults.dirs[DEFAULT_DIR_LOGS]))
      {
         configuration_set_string(settings,
               settings->paths.log_dir,
               g_defaults.dirs[DEFAULT_DIR_LOGS]);
      }
      else
         *settings->paths.log_dir = '\0';
   }

   if (settings->floats.slowmotion_ratio < 1.0f)
      configuration_set_float(settings, settings->floats.slowmotion_ratio, 1.0f);

   /* Sanitize fastforward_ratio value - previously range was -1
    * and up (with 0 being skipped) */
   if (settings->floats.fastforward_ratio < 0.0f)
      configuration_set_float(settings, settings->floats.fastforward_ratio, 0.0f);

#ifdef HAVE_LAKKA
   configuration_set_bool(settings,
         settings->bools.ssh_enable, filestream_exists(LAKKA_SSH_PATH));
   configuration_set_bool(settings,
         settings->bools.samba_enable, filestream_exists(LAKKA_SAMBA_PATH));
   configuration_set_bool(settings,
         settings->bools.bluetooth_enable, filestream_exists(LAKKA_BLUETOOTH_PATH));
#endif

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         config_get_path(conf, "savefile_directory", tmp_str, sizeof(tmp_str)))
   {
      if (string_is_equal(tmp_str, "default"))
         dir_set(RARCH_DIR_SAVEFILE, g_defaults.dirs[DEFAULT_DIR_SRAM]);

      else if (path_is_directory(tmp_str))
      {
         dir_set(RARCH_DIR_SAVEFILE, tmp_str);

         strlcpy(runloop_st->name.savefile, tmp_str,
               sizeof(runloop_st->name.savefile));
         fill_pathname_dir(runloop_st->name.savefile,
               path_get(RARCH_PATH_BASENAME),
               FILE_PATH_SRM_EXTENSION,
               sizeof(runloop_st->name.savefile));
      }
      else
         RARCH_WARN("[Config]: 'savefile_directory' is not a directory, ignoring ...\n");
   }

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         config_get_path(conf, "savestate_directory", tmp_str, sizeof(tmp_str)))
   {
      if (string_is_equal(tmp_str, "default"))
         dir_set(RARCH_DIR_SAVESTATE, g_defaults.dirs[DEFAULT_DIR_SAVESTATE]);
      else if (path_is_directory(tmp_str))
      {
         dir_set(RARCH_DIR_SAVESTATE, tmp_str);

         strlcpy(runloop_st->name.savestate, tmp_str,
               sizeof(runloop_st->name.savestate));
         fill_pathname_dir(runloop_st->name.savestate,
               path_get(RARCH_PATH_BASENAME),
               ".state",
               sizeof(runloop_st->name.savestate));
      }
      else
         RARCH_WARN("[Config]: 'savestate_directory' is not a directory, ignoring ...\n");
   }

   config_read_keybinds_conf(conf);

#if defined(HAVE_MENU) && defined(HAVE_RGUI)
   if (!check_menu_driver_compatibility(settings))
      configuration_set_string(settings,
            settings->arrays.menu_driver, "rgui");
#endif

#ifdef HAVE_LIBNX
   /* Apply initial clocks */
   extern void libnx_apply_overclock();
   libnx_apply_overclock();
#endif

   frontend_driver_set_sustained_performance_mode(settings->bools.sustained_performance_mode);
   recording_driver_update_streaming_url();

   if (!config_entry_exists(conf, "user_language"))
      msg_hash_set_uint(MSG_HASH_USER_LANGUAGE, frontend_driver_get_user_language());

   if (frontend_driver_has_gamemode() &&
       !frontend_driver_set_gamemode(settings->bools.gamemode_enable) &&
       settings->bools.gamemode_enable)
   {
      RARCH_WARN("[Config]: GameMode unsupported - disabling...\n");
      configuration_set_bool(settings,
            settings->bools.gamemode_enable, false);
   }

   /* If this is the first run of an existing installation
    * after the independent favourites playlist size limit was
    * added, set the favourites limit according to the current
    * history playlist size limit. (Have to do this, otherwise
    * users with large custom history size limits may lose
    * favourites entries when updating RetroArch...) */
   if ( config_entry_exists(conf, "content_history_size") &&
       !config_entry_exists(conf, "content_favorites_size"))
   {
      if (settings->uints.content_history_size > 999)
         settings->ints.content_favorites_size = -1;
      else
         settings->ints.content_favorites_size = (int)settings->uints.content_history_size;
   }

   if (conf)
      config_file_free(conf);
   if (bool_settings)
      free(bool_settings);
   if (int_settings)
      free(int_settings);
   if (uint_settings)
      free(uint_settings);
   if (float_settings)
      free(float_settings);
   if (array_settings)
      free(array_settings);
   if (path_settings)
      free(path_settings);
   if (size_settings)
      free(size_settings);
   first_load = false;
   return true;
}

/**
 * config_load_override:
 *
 * Tries to append game-specific and core-specific configuration.
 * These settings will always have precedence, thus this feature
 * can be used to enforce overrides.
 *
 * This function only has an effect if a game-specific or core-specific
 * configuration file exists at respective locations.
 *
 * core-specific: $CONFIG_DIR/$CORE_NAME/$CORE_NAME.cfg
 * fallback:      $CURRENT_CFG_LOCATION/$CORE_NAME/$CORE_NAME.cfg
 *
 * game-specific: $CONFIG_DIR/$CORE_NAME/$ROM_NAME.cfg
 * fallback:      $CURRENT_CFG_LOCATION/$CORE_NAME/$GAME_NAME.cfg
 *
 * Returns: false if there was an error or no action was performed.
 *
 */
bool config_load_override(void *data)
{
   char core_path[PATH_MAX_LENGTH];
   char game_path[PATH_MAX_LENGTH];
   char content_path[PATH_MAX_LENGTH];
   char content_dir_name[PATH_MAX_LENGTH];
   char config_directory[PATH_MAX_LENGTH];
   bool should_append                     = false;
   rarch_system_info_t *system            = (rarch_system_info_t*)data;
   const char *core_name                  = system ?
      system->info.library_name : NULL;
   const char *rarch_path_basename        = path_get(RARCH_PATH_BASENAME);
   const char *game_name                  = NULL;
   settings_t *settings                   = config_st;
   bool has_content                       = !string_is_empty(rarch_path_basename);

   core_path[0]        = '\0';
   game_path[0]        = '\0';
   content_path[0]     = '\0';
   content_dir_name[0] = '\0';
   config_directory[0] = '\0';

   /* Cannot load an override if we have no core */
   if (string_is_empty(core_name))
      return false;

   /* Get base config directory */
   fill_pathname_application_special(config_directory,
         sizeof(config_directory),
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* Concatenate strings into full paths for core_path,
    * game_path, content_path */
   if (has_content)
   {
      fill_pathname_parent_dir_name(content_dir_name,
            rarch_path_basename, sizeof(content_dir_name));
      game_name = path_basename(rarch_path_basename);

      fill_pathname_join_special_ext(game_path,
            config_directory, core_name,
            game_name,
            ".cfg",
            sizeof(game_path));

      fill_pathname_join_special_ext(content_path,
         config_directory, core_name,
         content_dir_name,
         ".cfg",
         sizeof(content_path));
   }

   fill_pathname_join_special_ext(core_path,
         config_directory, core_name,
         core_name,
         ".cfg",
         sizeof(core_path));

   /* per-core overrides */
   /* Create a new config file from core_path */
   if (config_file_exists(core_path))
   {
      RARCH_LOG("[Overrides]: Core-specific overrides found at \"%s\".\n",
            core_path);

      path_set(RARCH_PATH_CONFIG_APPEND, core_path);

      should_append = true;
   }
   else
      RARCH_LOG("[Overrides]: No core-specific overrides found at \"%s\".\n",
            core_path);

   if (has_content)
   {
      /* per-content-dir overrides */
      /* Create a new config file from content_path */
      if (config_file_exists(content_path))
      {
         char temp_path[PATH_MAX_LENGTH];

         RARCH_LOG("[Overrides]: Content dir-specific overrides found at \"%s\".\n",
               content_path);

         if (should_append)
         {
            RARCH_LOG("[Overrides]: Content dir-specific overrides stacking on top of previous overrides.\n");
            snprintf(temp_path, sizeof(temp_path),
                  "%s|%s",
                  path_get(RARCH_PATH_CONFIG_APPEND),
                  content_path
                  );
         }
         else
         {
            temp_path[0]    = '\0';
            strlcpy(temp_path, content_path, sizeof(temp_path));
         }

         path_set(RARCH_PATH_CONFIG_APPEND, temp_path);

         should_append = true;
      }
      else
         RARCH_LOG("[Overrides]: No content-dir-specific overrides found at \"%s\".\n",
            content_path);

      /* per-game overrides */
      /* Create a new config file from game_path */
      if (config_file_exists(game_path))
      {
         char temp_path[PATH_MAX_LENGTH];

         RARCH_LOG("[Overrides]: Game-specific overrides found at \"%s\".\n",
               game_path);

         if (should_append)
         {
            RARCH_LOG("[Overrides]: Game-specific overrides stacking on top of previous overrides.\n");
            snprintf(temp_path, sizeof(temp_path),
                  "%s|%s",
                  path_get(RARCH_PATH_CONFIG_APPEND),
                  game_path
                  );
         }
         else
         {
            temp_path[0]    = '\0';
            strlcpy(temp_path, game_path, sizeof(temp_path));
         }

         path_set(RARCH_PATH_CONFIG_APPEND, temp_path);

         should_append = true;
      }
      else
         RARCH_LOG("[Overrides]: No game-specific overrides found at \"%s\".\n",
               game_path);
   }

   if (!should_append)
      return false;

   /* Re-load the configuration with any overrides
    * that might have been found */

   /* Toggle has_save_path to false so it resets */
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_SAVE_PATH,  NULL);

   if (!config_load_file(global_get_ptr(),
            path_get(RARCH_PATH_CONFIG), settings))
      return false;

   if (settings->bools.notification_show_config_override_load)
      runloop_msg_queue_push(msg_hash_to_str(MSG_CONFIG_OVERRIDE_LOADED),
            1, 100, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   /* Reset save paths. */
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);

   path_clear(RARCH_PATH_CONFIG_APPEND);

   return true;
}

/**
 * config_unload_override:
 *
 * Unloads configuration overrides if overrides are active.
 *
 *
 * Returns: false if there was an error.
 */
bool config_unload_override(void)
{
   path_clear(RARCH_PATH_CONFIG_APPEND);

   /* Toggle has_save_path to false so it resets */
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_SAVE_PATH,  NULL);

   if (!config_load_file(global_get_ptr(),
            path_get(RARCH_PATH_CONFIG), config_st))
      return false;

   RARCH_LOG("[Overrides]: Configuration overrides unloaded, original configuration restored.\n");

   /* Reset save paths */
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
   retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);

   return true;
}

/**
 * config_load_remap:
 *
 * Tries to append game-specific and core-specific remap files.
 *
 * This function only has an effect if a game-specific or core-specific
 * configuration file exists at respective locations.
 *
 * core-specific: $REMAP_DIR/$CORE_NAME/$CORE_NAME.cfg
 * game-specific: $REMAP_DIR/$CORE_NAME/$GAME_NAME.cfg
 *
 * Returns: false if there was an error or no action was performed.
 */
bool config_load_remap(const char *directory_input_remapping,
      void *data)
{
   char content_dir_name[PATH_MAX_LENGTH];
   /* final path for core-specific configuration (prefix+suffix) */
   char core_path[PATH_MAX_LENGTH];
   /* final path for game-specific configuration (prefix+suffix) */
   char game_path[PATH_MAX_LENGTH];
   /* final path for content-dir-specific configuration (prefix+suffix) */
   char content_path[PATH_MAX_LENGTH];
   config_file_t *new_conf                = NULL;
   rarch_system_info_t *system            = (rarch_system_info_t*)data;
   const char *core_name                  = system ? system->info.library_name : NULL;
   const char *rarch_path_basename        = path_get(RARCH_PATH_BASENAME);
   const char *game_name                  = NULL;
   bool has_content                       = !string_is_empty(rarch_path_basename);
   enum msg_hash_enums msg_remap_loaded   = MSG_GAME_REMAP_FILE_LOADED;
   settings_t *settings                   = config_st;
   bool notification_show_remap_load      = settings->bools.notification_show_remap_load;

   content_dir_name[0] = '\0';
   core_path[0]        = '\0';
   game_path[0]        = '\0';
   content_path[0]     = '\0';

   /* > Cannot load remaps if we have no core
    * > Cannot load remaps if remap directory is unset */
   if (string_is_empty(core_name) ||
       string_is_empty(directory_input_remapping))
      return false;

   RARCH_LOG("[Remaps]: Remap directory: \"%s\".\n", directory_input_remapping);

   /* Concatenate strings into full paths for core_path,
    * game_path, content_path */
   if (has_content)
   {
      fill_pathname_parent_dir_name(content_dir_name,
            rarch_path_basename, sizeof(content_dir_name));
      game_name = path_basename(rarch_path_basename);

      fill_pathname_join_special_ext(game_path,
            directory_input_remapping, core_name,
            game_name,
            FILE_PATH_REMAP_EXTENSION,
            sizeof(game_path));

      fill_pathname_join_special_ext(content_path,
            directory_input_remapping, core_name,
            content_dir_name,
            FILE_PATH_REMAP_EXTENSION,
            sizeof(content_path));
   }

   fill_pathname_join_special_ext(core_path,
         directory_input_remapping, core_name,
         core_name,
         FILE_PATH_REMAP_EXTENSION,
         sizeof(core_path));

   input_remapping_set_defaults(false);

   /* If a game remap file exists, load it. */
   if (has_content && (new_conf = config_file_new_from_path_to_string(game_path)))
   {
      bool ret = input_remapping_load_file(new_conf, game_path);
      config_file_free(new_conf);
      new_conf = NULL;
      RARCH_LOG("[Remaps]: Game-specific remap found at \"%s\".\n", game_path);
      if (ret)
      {
         retroarch_ctl(RARCH_CTL_SET_REMAPS_GAME_ACTIVE, NULL);
         /* msg_remap_loaded is set to MSG_GAME_REMAP_FILE_LOADED
          * by default - no need to change it here */
         goto success;
      }
   }

   /* If a content-dir remap file exists, load it. */
   if (has_content && (new_conf = config_file_new_from_path_to_string(content_path)))
   {
      bool ret = input_remapping_load_file(new_conf, content_path);
      config_file_free(new_conf);
      new_conf = NULL;
      RARCH_LOG("[Remaps]: Content-dir-specific remap found at \"%s\".\n", content_path);
      if (ret)
      {
         retroarch_ctl(RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE, NULL);
         msg_remap_loaded = MSG_DIRECTORY_REMAP_FILE_LOADED;
         goto success;
      }
   }

   /* If a core remap file exists, load it. */
   if ((new_conf = config_file_new_from_path_to_string(core_path)))
   {
      bool ret = input_remapping_load_file(new_conf, core_path);
      config_file_free(new_conf);
      new_conf = NULL;
      RARCH_LOG("[Remaps]: Core-specific remap found at \"%s\".\n", core_path);
      if (ret)
      {
         retroarch_ctl(RARCH_CTL_SET_REMAPS_CORE_ACTIVE, NULL);
         msg_remap_loaded = MSG_CORE_REMAP_FILE_LOADED;
         goto success;
      }
   }

   if (new_conf)
      config_file_free(new_conf);
   new_conf = NULL;

   return false;

success:
   if (notification_show_remap_load)
      runloop_msg_queue_push(
            msg_hash_to_str(msg_remap_loaded), 1, 100, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   return true;
}

/**
 * config_parse_file:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
static void config_parse_file(global_t *global)
{
   const char *config_path = path_get(RARCH_PATH_CONFIG);
   if (path_is_empty(RARCH_PATH_CONFIG))
   {
      RARCH_LOG("[Config]: Loading default config.\n");
   }
   else
       RARCH_LOG("[Config]: Loading config from: \"%s\".\n", config_path);

   {
      if (!config_load_file(global, config_path, config_st))
      {
         RARCH_ERR("[Config]: Couldn't find config at path: \"%s\".\n",
               config_path);
      }
   }
}

static void video_driver_save_settings(global_t *global, config_file_t *conf)
{
   config_set_int(conf, "gamma_correction",
         global->console.screen.gamma_correction);
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
         config_set_string(conf, key, "nul");
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
         config_set_string(conf, key, "nul");
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
            config_set_string(conf, key, "nul");
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
static void input_config_save_keybind(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind,
      bool save_empty)
{
   save_keybind_joykey (conf, prefix, base, bind, save_empty);
   save_keybind_axis   (conf, prefix, base, bind, save_empty);
   save_keybind_mbutton(conf, prefix, base, bind, save_empty);
}

const char *input_config_get_prefix(unsigned user, bool meta)
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
   if (meta)
   {
      if (user == 0)
         return "input";
      /* Don't bother with meta bind for anyone else than first user. */
      return NULL;
   }
   return bind_user_prefix[user];
}

/**
 * input_config_save_keybinds_user:
 * @conf               : pointer to config file object
 * @user               : user number
 *
 * Save the current keybinds of a user (@user) to the config file (@conf).
 */
static void input_config_save_keybinds_user(config_file_t *conf, unsigned user)
{
   unsigned i = 0;

   for (i = 0; input_config_bind_map_get_valid(i); i++)
   {
      char key[64];
      char btn[64];
      const struct input_bind_map *keybind =
         (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(i);
      bool meta                            = keybind ? keybind->meta : false;
      const char *prefix                   = input_config_get_prefix(user, meta);
      const struct retro_keybind *bind     = &input_config_binds[user][i];
      const char                 *base     = NULL;

      if (!prefix || !bind->valid || !keybind)
         continue;

      base                                 = keybind->base;
      key[0] = btn[0]                      = '\0';

      fill_pathname_join_delim(key, prefix, base, '_', sizeof(key));

      input_keymaps_translate_rk_to_str(bind->key, btn, sizeof(btn));
      config_set_string(conf, key, btn);

      input_config_save_keybind(conf, prefix, base, bind, true);
   }
}

/**
 * config_save_autoconf_profile:
 * @device_name       : Input device name
 * @user              : Controller number to save
 * Writes a controller autoconf file to disk.
 **/
bool config_save_autoconf_profile(const
      char *device_name, unsigned user)
{
   static const char* invalid_filename_chars[] = {
      /* https://support.microsoft.com/en-us/help/905231/information-about-the-characters-that-you-cannot-use-in-site-names--fo */
      "~", "#", "%", "&", "*", "{", "}", "\\", ":", "[", "]", "?", "/", "|", "\'", "\"",
      NULL
   };
   unsigned i;
   char buf[PATH_MAX_LENGTH];
   char autoconf_file[PATH_MAX_LENGTH];
   config_file_t *conf                  = NULL;
   int32_t pid_user                     = 0;
   int32_t vid_user                     = 0;
   bool ret                             = false;
   settings_t *settings                 = config_st;
   const char *autoconf_dir             = settings->paths.directory_autoconfig;
   const char *joypad_driver_fallback   = settings->arrays.input_joypad_driver;
   const char *joypad_driver            = NULL;
   char *sanitised_name                 = NULL;

   buf[0]                               = '\0';
   autoconf_file[0]                     = '\0';

   if (string_is_empty(device_name))
      goto end;

   /* Get currently set joypad driver */
   joypad_driver = input_config_get_device_joypad_driver(user);
   if (string_is_empty(joypad_driver))
   {
      /* This cannot happen, but if we reach this
       * point without a driver being set for the
       * current input device then use the value
       * from the settings struct as a fallback */
      joypad_driver = joypad_driver_fallback;

      if (string_is_empty(joypad_driver))
         goto end;
   }

   sanitised_name = strdup(device_name);

   /* Remove invalid filename characters from
    * input device name */
   for (i = 0; invalid_filename_chars[i]; i++)
   {
      for (;;)
      {
         char *tmp = strstr(sanitised_name,
               invalid_filename_chars[i]);

         if (tmp)
            *tmp = '_';
         else
            break;
      }
   }

   /* Generate autoconfig file path */
   fill_pathname_join(buf, autoconf_dir, joypad_driver, sizeof(buf));

   if (path_is_directory(buf))
      fill_pathname_join_concat(autoconf_file, buf,
            sanitised_name, ".cfg", sizeof(autoconf_file));
   else
      fill_pathname_join_concat(autoconf_file, autoconf_dir,
            sanitised_name, ".cfg", sizeof(autoconf_file));

   /* Open config file */
   conf = config_file_new_from_path_to_string(autoconf_file);

   if (!conf)
   {
      conf = config_file_new_alloc();

      if (!conf)
         goto end;
   }

   /* Update config file */
   config_set_string(conf, "input_driver",
         joypad_driver);
   config_set_string(conf, "input_device",
         input_config_get_device_name(user));

   pid_user = input_config_get_device_pid(user);
   vid_user = input_config_get_device_vid(user);

   if (pid_user && vid_user)
   {
      config_set_int(conf, "input_vendor_id",
            vid_user);
      config_set_int(conf, "input_product_id",
            pid_user);
   }

   for (i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      const struct retro_keybind *bind = &input_config_binds[user][i];
      if (bind->valid)
         input_config_save_keybind(
               conf, "input", input_config_bind_map_get_base(i),
               bind, false);
   }

   ret = config_file_write(conf, autoconf_file, false);

end:
   if (sanitised_name)
      free(sanitised_name);

   if (conf)
      config_file_free(conf);

   return ret;
}

/**
 * config_save_file:
 * @path            : Path that shall be written to.
 *
 * Writes a config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_file(const char *path)
{
   float msg_color;
   unsigned i                                        = 0;
   bool ret                                          = false;
   struct config_bool_setting     *bool_settings     = NULL;
   struct config_int_setting     *int_settings       = NULL;
   struct config_uint_setting     *uint_settings     = NULL;
   struct config_size_setting     *size_settings     = NULL;
   struct config_float_setting     *float_settings   = NULL;
   struct config_array_setting     *array_settings   = NULL;
   struct config_path_setting     *path_settings     = NULL;
   config_file_t                              *conf  = config_file_new_from_path_to_string(path);
   settings_t                              *settings = config_st;
   global_t *global                                  = global_get_ptr();
   int bool_settings_size                            = sizeof(settings->bools) / sizeof(settings->bools.placeholder);
   int float_settings_size                           = sizeof(settings->floats)/ sizeof(settings->floats.placeholder);
   int int_settings_size                             = sizeof(settings->ints)  / sizeof(settings->ints.placeholder);
   int uint_settings_size                            = sizeof(settings->uints) / sizeof(settings->uints.placeholder);
   int size_settings_size                            = sizeof(settings->sizes) / sizeof(settings->sizes.placeholder);
   int array_settings_size                           = sizeof(settings->arrays)/ sizeof(settings->arrays.placeholder);
   int path_settings_size                            = sizeof(settings->paths) / sizeof(settings->paths.placeholder);

   if (!conf)
      conf = config_file_new_alloc();

   if (!conf || retroarch_ctl(RARCH_CTL_IS_OVERRIDES_ACTIVE, NULL))
   {
      if (conf)
         config_file_free(conf);
      return false;
   }

   bool_settings   = populate_settings_bool  (settings, &bool_settings_size);
   int_settings    = populate_settings_int   (settings, &int_settings_size);
   uint_settings   = populate_settings_uint  (settings, &uint_settings_size);
   size_settings   = populate_settings_size  (settings, &size_settings_size);
   float_settings  = populate_settings_float (settings, &float_settings_size);
   array_settings  = populate_settings_array (settings, &array_settings_size);
   path_settings   = populate_settings_path  (settings, &path_settings_size);

   /* Path settings */
   if (path_settings && (path_settings_size > 0))
   {
      for (i = 0; i < (unsigned)path_settings_size; i++)
      {
         const char *value = path_settings[i].ptr;

         if (path_settings[i].def_enable && string_is_empty(path_settings[i].ptr))
            value = "default";

         config_set_path(conf, path_settings[i].ident, value);
      }

      free(path_settings);
   }

#ifdef HAVE_MENU
   config_set_path(conf, "xmb_font",
         !string_is_empty(settings->paths.path_menu_xmb_font)
         ? settings->paths.path_menu_xmb_font : "");
#endif

   /* String settings  */
   if (array_settings && (array_settings_size > 0))
   {
      for (i = 0; i < (unsigned)array_settings_size; i++)
         if (!array_settings[i].override ||
             !retroarch_override_setting_is_set(array_settings[i].override, NULL))
            config_set_string(conf,
                  array_settings[i].ident,
                  array_settings[i].ptr);

      free(array_settings);
   }

   /* Float settings  */
   if (float_settings && (float_settings_size > 0))
   {
      for (i = 0; i < (unsigned)float_settings_size; i++)
         if (!float_settings[i].override ||
             !retroarch_override_setting_is_set(float_settings[i].override, NULL))
            config_set_float(conf,
                  float_settings[i].ident,
                  *float_settings[i].ptr);

      free(float_settings);
   }

   /* Integer settings */
   if (int_settings && (int_settings_size > 0))
   {
      for (i = 0; i < (unsigned)int_settings_size; i++)
         if (!int_settings[i].override ||
             !retroarch_override_setting_is_set(int_settings[i].override, NULL))
            config_set_int(conf,
                  int_settings[i].ident,
                  *int_settings[i].ptr);

      free(int_settings);
   }

   if (uint_settings && (uint_settings_size > 0))
   {
      for (i = 0; i < (unsigned)uint_settings_size; i++)
         if (!uint_settings[i].override ||
             !retroarch_override_setting_is_set(uint_settings[i].override, NULL))
            config_set_int(conf,
                  uint_settings[i].ident,
                  *uint_settings[i].ptr);

      free(uint_settings);
   }

   if (size_settings && (size_settings_size > 0))
   {
      for (i = 0; i < (unsigned)size_settings_size; i++)
         if (!size_settings[i].override ||
             !retroarch_override_setting_is_set(size_settings[i].override, NULL))
            config_set_int(conf,
                  size_settings[i].ident,
                  (int)*size_settings[i].ptr);

      free(size_settings);
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      char cfg[64];

      cfg[0] = '\0';

      snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
      config_set_int(conf, cfg, settings->uints.input_device[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_joypad_index", i + 1);
      config_set_int(conf, cfg, settings->uints.input_joypad_index[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_analog_dpad_mode", i + 1);
      config_set_int(conf, cfg, settings->uints.input_analog_dpad_mode[i]);
      snprintf(cfg, sizeof(cfg), "input_player%u_mouse_index", i + 1);
      config_set_int(conf, cfg, settings->uints.input_mouse_index[i]);
   }

   /* Boolean settings */
   if (bool_settings && (bool_settings_size > 0))
   {
      for (i = 0; i < (unsigned)bool_settings_size; i++)
         if (!bool_settings[i].override ||
             !retroarch_override_setting_is_set(bool_settings[i].override, NULL))
            config_set_bool(conf, bool_settings[i].ident,
                  *bool_settings[i].ptr);

      free(bool_settings);
   }

#ifdef HAVE_NETWORKGAMEPAD
   for (i = 0; i < MAX_USERS; i++)
   {
      char tmp[64];

      tmp[0] = '\0';

      snprintf(tmp, sizeof(tmp), "network_remote_enable_user_p%u", i + 1);
      config_set_bool(conf, tmp, settings->bools.network_remote_enable_user[i]);
   }
#endif

   /* Verbosity isn't in bool_settings since it needs to be loaded differently */
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL))
      config_set_bool(conf, "log_verbosity",
            verbosity_is_enabled());
   config_set_bool(conf, "perfcnt_enable",
         retroarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL));

   msg_color = (((int)(settings->floats.video_msg_color_r * 255.0f) & 0xff) << 16) +
               (((int)(settings->floats.video_msg_color_g * 255.0f) & 0xff) <<  8) +
               (((int)(settings->floats.video_msg_color_b * 255.0f) & 0xff));

   /* Hexadecimal settings */
   config_set_hex(conf, "video_message_color", msg_color);

   if (conf)
      video_driver_save_settings(global, conf);

#ifdef HAVE_LAKKA
   if (settings->bools.ssh_enable)
      filestream_close(filestream_open(LAKKA_SSH_PATH,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE));
   else
      filestream_delete(LAKKA_SSH_PATH);
   if (settings->bools.samba_enable)
      filestream_close(filestream_open(LAKKA_SAMBA_PATH,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE));
   else
      filestream_delete(LAKKA_SAMBA_PATH);
   if (settings->bools.bluetooth_enable)
      filestream_close(filestream_open(LAKKA_BLUETOOTH_PATH,
               RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE));
   else
      filestream_delete(LAKKA_BLUETOOTH_PATH);
#endif

   for (i = 0; i < MAX_USERS; i++)
      input_config_save_keybinds_user(conf, i);

   ret = config_file_write(conf, path, true);
   config_file_free(conf);

   return ret;
}

/**
 * config_save_overrides:
 * @path            : Path that shall be written to.
 *
 * Writes a config file override to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_overrides(enum override_type type, void *data)
{
   int tmp_i                                   = 0;
   unsigned i                                  = 0;
   bool ret                                    = false;
   config_file_t *conf                         = NULL;
   settings_t *settings                        = NULL;
   struct config_bool_setting *bool_settings   = NULL;
   struct config_bool_setting *bool_overrides  = NULL;
   struct config_int_setting *int_settings     = NULL;
   struct config_uint_setting *uint_settings   = NULL;
   struct config_size_setting *size_settings   = NULL;
   struct config_int_setting *int_overrides    = NULL;
   struct config_uint_setting *uint_overrides  = NULL;
   struct config_size_setting *size_overrides  = NULL;
   struct config_float_setting *float_settings = NULL;
   struct config_float_setting *float_overrides= NULL;
   struct config_array_setting *array_settings = NULL;
   struct config_array_setting *array_overrides= NULL;
   struct config_path_setting *path_settings   = NULL;
   struct config_path_setting *path_overrides  = NULL;
   char config_directory[PATH_MAX_LENGTH];
   char override_directory[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char game_path[PATH_MAX_LENGTH];
   char content_path[PATH_MAX_LENGTH];
   char content_dir_name[PATH_MAX_LENGTH];
   settings_t *overrides                       = config_st;
   int bool_settings_size                      = sizeof(settings->bools)  / sizeof(settings->bools.placeholder);
   int float_settings_size                     = sizeof(settings->floats) / sizeof(settings->floats.placeholder);
   int int_settings_size                       = sizeof(settings->ints)   / sizeof(settings->ints.placeholder);
   int uint_settings_size                      = sizeof(settings->uints)  / sizeof(settings->uints.placeholder);
   int size_settings_size                      = sizeof(settings->sizes)  / sizeof(settings->sizes.placeholder);
   int array_settings_size                     = sizeof(settings->arrays) / sizeof(settings->arrays.placeholder);
   int path_settings_size                      = sizeof(settings->paths)  / sizeof(settings->paths.placeholder);
   rarch_system_info_t *system                 = (rarch_system_info_t*)data;
   const char *core_name                       = system ? system->info.library_name : NULL;
   const char *rarch_path_basename             = path_get(RARCH_PATH_BASENAME);
   const char *game_name                       = NULL;
   bool has_content                            = !string_is_empty(rarch_path_basename);

   config_directory[0]   = '\0';
   override_directory[0] = '\0';
   core_path[0]          = '\0';
   game_path[0]          = '\0';
   content_path[0]       = '\0';
   content_dir_name[0]   = '\0';

   /* > Cannot save an override if we have no core
    * > Cannot save a per-game or per-content-directory
    *   override if we have no content */
   if (string_is_empty(core_name) ||
       (!has_content && (type != OVERRIDE_CORE)))
      return false;

   settings = (settings_t*)calloc(1, sizeof(settings_t));
   conf     = config_file_new_alloc();

   /* Get base config directory */
   fill_pathname_application_special(config_directory,
         sizeof(config_directory),
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   fill_pathname_join(override_directory,
      config_directory, core_name,
      sizeof(override_directory));

   /* Ensure base config directory exists */
   if (!path_is_directory(override_directory))
      path_mkdir(override_directory);

   /* Load the original config file in memory */
   config_load_file(global_get_ptr(),
         path_get(RARCH_PATH_CONFIG), settings);

   bool_settings       = populate_settings_bool(settings,   &bool_settings_size);
   tmp_i               = sizeof(settings->bools) / sizeof(settings->bools.placeholder);
   bool_overrides      = populate_settings_bool(overrides,  &tmp_i);

   int_settings        = populate_settings_int(settings,    &int_settings_size);
   tmp_i               = sizeof(settings->ints) / sizeof(settings->ints.placeholder);
   int_overrides       = populate_settings_int (overrides,  &tmp_i);

   uint_settings       = populate_settings_uint(settings,    &uint_settings_size);
   tmp_i               = sizeof(settings->uints) / sizeof(settings->uints.placeholder);
   uint_overrides      = populate_settings_uint (overrides,  &tmp_i);

   size_settings       = populate_settings_size(settings,    &size_settings_size);
   tmp_i               = sizeof(settings->sizes) / sizeof(settings->sizes.placeholder);
   size_overrides      = populate_settings_size (overrides,  &tmp_i);

   float_settings      = populate_settings_float(settings,  &float_settings_size);
   tmp_i               = sizeof(settings->floats) / sizeof(settings->floats.placeholder);
   float_overrides     = populate_settings_float(overrides, &tmp_i);

   array_settings      = populate_settings_array(settings,  &array_settings_size);
   tmp_i               = sizeof(settings->arrays)   / sizeof(settings->arrays.placeholder);
   array_overrides     = populate_settings_array (overrides, &tmp_i);

   path_settings       = populate_settings_path(settings, &path_settings_size);
   tmp_i               = sizeof(settings->paths)   / sizeof(settings->paths.placeholder);
   path_overrides      = populate_settings_path (overrides, &tmp_i);

   RARCH_LOG("[Overrides]: Looking for changed settings... \n");

   if (conf)
   {
      for (i = 0; i < (unsigned)bool_settings_size; i++)
      {
         if ((*bool_settings[i].ptr) != (*bool_overrides[i].ptr))
            config_set_bool(conf, bool_overrides[i].ident,
                  (*bool_overrides[i].ptr));
      }
      for (i = 0; i < (unsigned)int_settings_size; i++)
      {
         if ((*int_settings[i].ptr) != (*int_overrides[i].ptr))
            config_set_int(conf, int_overrides[i].ident,
                  (*int_overrides[i].ptr));
      }
      for (i = 0; i < (unsigned)uint_settings_size; i++)
      {
         if ((*uint_settings[i].ptr) != (*uint_overrides[i].ptr))
            config_set_int(conf, uint_overrides[i].ident,
                  (*uint_overrides[i].ptr));
      }
      for (i = 0; i < (unsigned)size_settings_size; i++)
      {
         if ((*size_settings[i].ptr) != (*size_overrides[i].ptr))
            config_set_int(conf, size_overrides[i].ident,
                  (int)(*size_overrides[i].ptr));
      }
      for (i = 0; i < (unsigned)float_settings_size; i++)
      {
         if ((*float_settings[i].ptr) != (*float_overrides[i].ptr))
            config_set_float(conf, float_overrides[i].ident,
                  *float_overrides[i].ptr);
      }

      for (i = 0; i < (unsigned)array_settings_size; i++)
      {
         if (!string_is_equal(array_settings[i].ptr, array_overrides[i].ptr))
         {
#ifdef HAVE_CHEEVOS
            /* As authentication doesn't occur until after content is loaded,
             * the achievement authentication token might only exist in the
             * override set, and therefore differ from the master config set.
             * Storing the achievement authentication token in an override
             * is a recipe for disaster. If it expires and the user generates
             * a new token, then the override will be out of date and the
             * user will have to reauthenticate for each override (and also
             * remember to update each override). Also exclude the username
             * as it's directly tied to the token and password.
             */
            if (string_is_equal(array_settings[i].ident, "cheevos_token") ||
                string_is_equal(array_settings[i].ident, "cheevos_password") ||
                string_is_equal(array_settings[i].ident, "cheevos_username"))
               continue;
#endif
            config_set_string(conf, array_overrides[i].ident,
                  array_overrides[i].ptr);
         }
      }

      for (i = 0; i < (unsigned)path_settings_size; i++)
      {
         if (!string_is_equal(path_settings[i].ptr, path_overrides[i].ptr))
            config_set_path(conf, path_overrides[i].ident,
                  path_overrides[i].ptr);
      }

      for (i = 0; i < MAX_USERS; i++)
      {
         char cfg[64];

         cfg[0] = '\0';
         if (settings->uints.input_device[i]
               != overrides->uints.input_device[i])
         {
            snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
            config_set_int(conf, cfg, overrides->uints.input_device[i]);
         }

         if (settings->uints.input_joypad_index[i]
               != overrides->uints.input_joypad_index[i])
         {
            snprintf(cfg, sizeof(cfg), "input_player%u_joypad_index", i + 1);
            config_set_int(conf, cfg, overrides->uints.input_joypad_index[i]);
         }
      }

      ret = false;

      switch (type)
      {
         case OVERRIDE_CORE:
            fill_pathname_join_special_ext(core_path,
                  config_directory, core_name,
                  core_name,
                  FILE_PATH_CONFIG_EXTENSION,
                  sizeof(core_path));
            RARCH_LOG ("[Overrides]: Path \"%s\".\n", core_path);
            ret = config_file_write(conf, core_path, true);
            break;
         case OVERRIDE_GAME:
            game_name = path_basename(rarch_path_basename);
            fill_pathname_join_special_ext(game_path,
                  config_directory, core_name,
                  game_name,
                  FILE_PATH_CONFIG_EXTENSION,
                  sizeof(game_path));
            RARCH_LOG ("[Overrides]: Path \"%s\".\n", game_path);
            ret = config_file_write(conf, game_path, true);
            break;
         case OVERRIDE_CONTENT_DIR:
            fill_pathname_parent_dir_name(content_dir_name,
                  rarch_path_basename, sizeof(content_dir_name));
            fill_pathname_join_special_ext(content_path,
                  config_directory, core_name,
                  content_dir_name,
                  FILE_PATH_CONFIG_EXTENSION,
                  sizeof(content_path));
            RARCH_LOG ("[Overrides]: Path \"%s\".\n", content_path);
            ret = config_file_write(conf, content_path, true);
            break;
         case OVERRIDE_NONE:
         default:
            break;
      }

      config_file_free(conf);
   }

   if (bool_settings)
      free(bool_settings);
   if (bool_overrides)
      free(bool_overrides);
   if (int_settings)
      free(int_settings);
   if (uint_settings)
      free(uint_settings);
   if (size_settings)
      free(size_settings);
   if (int_overrides)
      free(int_overrides);
   if (uint_overrides)
      free(uint_overrides);
   if (float_settings)
      free(float_settings);
   if (float_overrides)
      free(float_overrides);
   if (array_settings)
      free(array_settings);
   if (array_overrides)
      free(array_overrides);
   if (path_settings)
      free(path_settings);
   if (path_overrides)
      free(path_overrides);
   if (size_overrides)
      free(size_overrides);
   free(settings);

   return ret;
}

/* Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly. */
bool config_replace(bool config_replace_save_on_exit, char *path)
{
   content_ctx_info_t content_info = {0};
   const char *rarch_path_config   = path_get(RARCH_PATH_CONFIG);

   /* If config file to be replaced is the same as the
    * current config file, exit. */
   if (string_is_equal(path, rarch_path_config))
      return false;

   if (config_replace_save_on_exit && !path_is_empty(RARCH_PATH_CONFIG))
      config_save_file(rarch_path_config);

   path_set(RARCH_PATH_CONFIG, path);

   retroarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);

   /* Load core in new (salamander) config. */
   path_clear(RARCH_PATH_CORE);

   return task_push_start_dummy_core(&content_info);
}

/**
 * input_remapping_load_file:
 * @data                     : Path to config file.
 *
 * Loads a remap file from disk to memory.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(void *data, const char *path)
{
   unsigned i, j;
   config_file_t *conf                              = (config_file_t*)data;
   settings_t *settings                             = config_st;
   runloop_state_t *runloop_st                      = runloop_state_get_ptr();
   char key_strings[RARCH_FIRST_CUSTOM_BIND + 8][8] = {
      "b", "y", "select", "start",
      "up", "down", "left", "right",
      "a", "x", "l", "r", "l2", "r2",
      "l3", "r3", "l_x+", "l_x-", "l_y+", "l_y-", "r_x+", "r_x-", "r_y+", "r_y-" };

   if (!conf || string_is_empty(path))
      return false;

   if (!string_is_empty(runloop_st->name.remapfile))
   {
      input_remapping_deinit(false);
      input_remapping_set_defaults(false);
   }
   runloop_st->name.remapfile = strdup(path);

   for (i = 0; i < MAX_USERS; i++)
   {
      char s1[32], s2[32], s3[32];

      s1[0] = '\0';
      s2[0] = '\0';
      s3[0] = '\0';

      snprintf(s1, sizeof(s1), "input_player%u_btn", i + 1);
      snprintf(s2, sizeof(s2), "input_player%u_key", i + 1);
      snprintf(s3, sizeof(s3), "input_player%u_stk", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 8; j++)
      {
         const char *key_string = key_strings[j];

         if (j < RARCH_FIRST_CUSTOM_BIND)
         {
            int btn_remap = -1;
            int key_remap = -1;
            char btn_ident[128];
            char key_ident[128];

            btn_ident[0] = key_ident[0] = '\0';

            fill_pathname_join_delim(btn_ident, s1,
                  key_string, '_', sizeof(btn_ident));
            fill_pathname_join_delim(key_ident, s2,
                  key_string, '_', sizeof(key_ident));

            if (config_get_int(conf, btn_ident, &btn_remap))
            {
               if (btn_remap == -1)
                  btn_remap = RARCH_UNMAPPED;

               configuration_set_uint(settings,
                     settings->uints.input_remap_ids[i][j], btn_remap);
            }

            if (!config_get_int(conf, key_ident, &key_remap))
               key_remap = RETROK_UNKNOWN;

            configuration_set_uint(settings,
                  settings->uints.input_keymapper_ids[i][j], key_remap);
         }
         else
         {
            char stk_ident[256];
            char key_ident[128];
            int stk_remap = -1;
            int key_remap = -1;

            stk_ident[0]  = '\0';
            key_ident[0]  = '\0';

            fill_pathname_join_delim(stk_ident, s3,
                  key_string, '_', sizeof(stk_ident));

            if (config_get_int(conf, stk_ident, &stk_remap))
            {
               if (stk_remap == -1)
                  stk_remap = RARCH_UNMAPPED;

               configuration_set_uint(settings,
                     settings->uints.input_remap_ids[i][j], stk_remap);
            }

            fill_pathname_join_delim(key_ident, s2,
                  key_string, '_', sizeof(key_ident));

            if (!config_get_int(conf, key_ident, &key_remap))
               key_remap = RETROK_UNKNOWN;

            configuration_set_uint(settings,
                  settings->uints.input_keymapper_ids[i][j], key_remap);
         }
      }

      snprintf(s1, sizeof(s1), "input_player%u_analog_dpad_mode", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_analog_dpad_mode[i], s1);

      snprintf(s1, sizeof(s1), "input_libretro_device_p%u", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_libretro_device[i], s1);

      snprintf(s1, sizeof(s1), "input_remap_port_p%u", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_remap_ports[i], s1);
   }

   input_remapping_update_port_map();

   /* Whenever a remap file is loaded, subsequent
    * changes to global remap-related parameters
    * must be reset at the next core deinitialisation */
   input_remapping_enable_global_config_restore();

   return true;
}

/**
 * input_remapping_save_file:
 * @path                     : Path to remapping file.
 *
 * Saves remapping values to file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save_file(const char *path)
{
   bool ret;
   unsigned i, j;
   char remap_file_dir[PATH_MAX_LENGTH];
   char key_strings[RARCH_FIRST_CUSTOM_BIND + 8][8] = {
      "b", "y", "select", "start",
      "up", "down", "left", "right",
      "a", "x", "l", "r", "l2", "r2",
      "l3", "r3", "l_x+", "l_x-", "l_y+", "l_y-", "r_x+", "r_x-", "r_y+", "r_y-" };
   const char      *remap_file = path;
   config_file_t         *conf = NULL;
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   settings_t        *settings = config_st;
   unsigned          max_users = settings->uints.input_max_users;

   remap_file_dir[0]           = '\0';

   if (string_is_empty(remap_file))
      return false;

   /* Create output directory, if required */
   strlcpy(remap_file_dir, remap_file, sizeof(remap_file_dir));
   path_parent_dir(remap_file_dir);

   if (!string_is_empty(remap_file_dir) &&
       !path_is_directory(remap_file_dir) &&
       !path_mkdir(remap_file_dir))
      return false;

   /* Attempt to load file */
   if (!(conf = config_file_new_from_path_to_string(remap_file)))
   {
      if (!(conf = config_file_new_alloc()))
         return false;
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      bool skip_port = true;
      char s1[32];
      char s2[32];
      char s3[32];

      s1[0] = '\0';
      s2[0] = '\0';
      s3[0] = '\0';

      /* We must include all mapped ports + all those
       * with an index less than max_users */
      if (i < max_users)
         skip_port = false;
      else
      {
         /* Check whether current port is mapped
          * to an input device */
         for (j = 0; j < max_users; j++)
         {
            if (i == settings->uints.input_remap_ports[j])
            {
               skip_port = false;
               break;
            }
         }
      }

      if (skip_port)
         continue;

      snprintf(s1, sizeof(s1), "input_player%u_btn", i + 1);
      snprintf(s2, sizeof(s2), "input_player%u_key", i + 1);
      snprintf(s3, sizeof(s3), "input_player%u_stk", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
      {
         char btn_ident[128];
         char key_ident[128];
         const char *key_string = key_strings[j];
         unsigned remap_id      = settings->uints.input_remap_ids[i][j];
         unsigned keymap_id     = settings->uints.input_keymapper_ids[i][j];

         btn_ident[0]           = '\0';
         key_ident[0]           = '\0';

         fill_pathname_join_delim(btn_ident, s1,
               key_string, '_', sizeof(btn_ident));
         fill_pathname_join_delim(key_ident, s2,
               key_string, '_', sizeof(key_ident));

         /* Only save modified button values */
         if (remap_id == j)
            config_unset(conf, btn_ident);
         else
         {
            if (remap_id == RARCH_UNMAPPED)
               config_set_int(conf, btn_ident, -1);
            else
               config_set_int(conf, btn_ident,
                     settings->uints.input_remap_ids[i][j]);
         }

         /* Only save non-empty keymapper values */
         if (keymap_id == RETROK_UNKNOWN)
            config_unset(conf, key_ident);
         else
            config_set_int(conf, key_ident,
                  settings->uints.input_keymapper_ids[i][j]);
      }

      for (j = RARCH_FIRST_CUSTOM_BIND; j < (RARCH_FIRST_CUSTOM_BIND + 8); j++)
      {
         char stk_ident[128];
         char key_ident[128];
         const char *key_string = key_strings[j];
         unsigned remap_id      = settings->uints.input_remap_ids[i][j];
         unsigned keymap_id     = settings->uints.input_keymapper_ids[i][j];

         stk_ident[0]           = '\0';
         key_ident[0]           = '\0';

         fill_pathname_join_delim(stk_ident, s3,
               key_string, '_', sizeof(stk_ident));
         fill_pathname_join_delim(key_ident, s2,
               key_string, '_', sizeof(key_ident));

         /* Only save modified button values */
         if (remap_id == j)
            config_unset(conf, stk_ident);
         else
         {
            if (remap_id == RARCH_UNMAPPED)
               config_set_int(conf, stk_ident, -1);
            else
               config_set_int(conf, stk_ident,
                     settings->uints.input_remap_ids[i][j]);
         }

         /* Only save non-empty keymapper values */
         if (keymap_id == RETROK_UNKNOWN)
            config_unset(conf, key_ident);
         else
            config_set_int(conf, key_ident,
                  settings->uints.input_keymapper_ids[i][j]);
      }

      snprintf(s1, sizeof(s1), "input_libretro_device_p%u", i + 1);
      config_set_int(conf, s1, input_config_get_device(i));

      snprintf(s1, sizeof(s1), "input_player%u_analog_dpad_mode", i + 1);
      config_set_int(conf, s1, settings->uints.input_analog_dpad_mode[i]);

      snprintf(s1, sizeof(s1), "input_remap_port_p%u", i + 1);
      config_set_int(conf, s1, settings->uints.input_remap_ports[i]);
   }

   ret = config_file_write(conf, remap_file, true);
   config_file_free(conf);

   /* Cache remap file path
    * > Must guard against the case where
    *   runloop_st->name.remapfile itself
    *   is passed to this function... */
   if (runloop_st->name.remapfile != remap_file)
   {
      if (runloop_st->name.remapfile)
         free(runloop_st->name.remapfile);
      runloop_st->name.remapfile = strdup(remap_file);
   }

   return ret;
}
#endif

#if !defined(HAVE_DYNAMIC)
/* Salamander config file contains a single
 * entry (libretro_path), which is linked to
 * RARCH_PATH_CORE
 * > Used to select which core to load
 *   when launching a salamander build */

static bool config_file_salamander_get_path(char *s, size_t len)
{
   const char *rarch_config_path = g_defaults.path_config;

   if (!string_is_empty(rarch_config_path))
      fill_pathname_resolve_relative(s,
            rarch_config_path,
            FILE_PATH_SALAMANDER_CONFIG,
            len);
   else
      strcpy_literal(s, FILE_PATH_SALAMANDER_CONFIG);

   return !string_is_empty(s);
}

void config_load_file_salamander(void)
{
   config_file_t *config = NULL;
   char config_path[PATH_MAX_LENGTH];
   char libretro_path[PATH_MAX_LENGTH];

   config_path[0]   = '\0';
   libretro_path[0] = '\0';

   /* Get config file path */
   if (!config_file_salamander_get_path(
         config_path, sizeof(config_path)))
      return;

   /* Open config file */
   config = config_file_new_from_path_to_string(config_path);

   if (!config)
      return;

   /* Read 'libretro_path' value and update
    * RARCH_PATH_CORE */
   RARCH_LOG("[Config]: Loading salamander config from: \"%s\".\n",
         config_path);

   if (config_get_path(config, "libretro_path",
         libretro_path, sizeof(libretro_path)) &&
       !string_is_empty(libretro_path) &&
       !string_is_equal(libretro_path, "builtin"))
      path_set(RARCH_PATH_CORE, libretro_path);

   config_file_free(config);
}

void config_save_file_salamander(void)
{
   config_file_t *config     = NULL;
   const char *libretro_path = path_get(RARCH_PATH_CORE);
   bool success              = false;
   char config_path[PATH_MAX_LENGTH];

   config_path[0] = '\0';

   if (string_is_empty(libretro_path) ||
       string_is_equal(libretro_path, "builtin"))
      return;

   /* Get config file path */
   if (!config_file_salamander_get_path(
         config_path, sizeof(config_path)))
      return;

   /* Open config file */
   config = config_file_new_from_path_to_string(config_path);

   if (!config)
      config = config_file_new_alloc();

   if (!config)
      goto end;

   /* Update config file */
   config_set_path(config, "libretro_path", libretro_path);

   /* Save config file
    * > Only one entry - no need to sort */
   success = config_file_write(config, config_path, false);

end:
   if (success)
      RARCH_LOG("[Config]: Saving salamander config to: \"%s\".\n",
            config_path);
   else
      RARCH_ERR("[Config]: Failed to create new salamander config file in: \"%s\".\n",
            config_path);

   if (config)
      config_file_free(config);
}
#endif

bool input_config_bind_map_get_valid(unsigned bind_index)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(bind_index);
   if (!keybind)
      return false;
   return keybind->valid;
}

unsigned input_config_bind_map_get_meta(unsigned bind_index)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(bind_index);
   if (!keybind)
      return 0;
   return keybind->meta;
}

const char *input_config_bind_map_get_base(unsigned bind_index)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(bind_index);
   if (!keybind)
      return NULL;
   return keybind->base;
}

const char *input_config_bind_map_get_desc(unsigned bind_index)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(bind_index);
   if (!keybind)
      return NULL;
   return msg_hash_to_str(keybind->desc);
}

uint8_t input_config_bind_map_get_retro_key(unsigned bind_index)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(bind_index);
   if (!keybind)
      return 0;
   return keybind->retro_key;
}

void input_config_reset_autoconfig_binds(unsigned port)
{
   unsigned i;

   if (port >= MAX_USERS)
      return;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_autoconf_binds[port][i].joykey  = NO_BTN;
      input_autoconf_binds[port][i].joyaxis = AXIS_NONE;

      if (input_autoconf_binds[port][i].joykey_label)
      {
         free(input_autoconf_binds[port][i].joykey_label);
         input_autoconf_binds[port][i].joykey_label = NULL;
      }

      if (input_autoconf_binds[port][i].joyaxis_label)
      {
         free(input_autoconf_binds[port][i].joyaxis_label);
         input_autoconf_binds[port][i].joyaxis_label = NULL;
      }
   }
}

void input_config_set_autoconfig_binds(unsigned port, void *data)
{
   unsigned i;
   config_file_t *config       = (config_file_t*)data;
   struct retro_keybind *binds = NULL;

   if ((port >= MAX_USERS) || !config)
      return;

   binds = input_autoconf_binds[port];

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      const struct input_bind_map *keybind =
         (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(i);
      if (keybind)
      {
         char str[256];
         const char *base = keybind->base;
         str[0]                     = '\0';

         fill_pathname_join_delim(str, "input", base,  '_', sizeof(str));

         input_config_parse_joy_button(str, config, "input", base, &binds[i]);
         input_config_parse_joy_axis  (str, config, "input", base, &binds[i]);
      }
   }
}

void input_config_parse_mouse_button(
      char *s,
      void *conf_data, const char *prefix,
      const char *btn, void *bind_data)
{
   int val;
   char tmp[64];
   char key[64];
   config_file_t *conf        = (config_file_t*)conf_data;
   struct retro_keybind *bind = (struct retro_keybind*)bind_data;

   tmp[0] = key[0]     = '\0';

   fill_pathname_join_delim(key, s, "mbtn", '_', sizeof(key));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
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

void input_config_parse_joy_axis(
      char *s,
      void *conf_data, const char *prefix,
      const char *axis, void *bind_data)
{
   char       tmp[64];
   char       key[64];
   char key_label[64];
   config_file_t *conf                     = (config_file_t*)conf_data;
   struct retro_keybind *bind              = (struct retro_keybind*)bind_data;
   struct config_entry_list *tmp_a         = NULL;

   tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(key, s,
         "axis", '_', sizeof(key));
   fill_pathname_join_delim(key_label, s,
         "axis_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      if (     tmp[0] == 'n'
            && tmp[1] == 'u'
            && tmp[2] == 'l'
            && tmp[3] == '\0'
         )
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

   tmp_a = config_get_entry(conf, key_label);

   if (tmp_a && (!string_is_empty(tmp_a->value)))
   {
      if (bind->joyaxis_label &&
            !string_is_empty(bind->joyaxis_label))
         free(bind->joyaxis_label);
      bind->joyaxis_label = strdup(tmp_a->value);
   }
}

static uint16_t input_config_parse_hat(const char *dir)
{
   if (     dir[0] == 'u'
         && dir[1] == 'p'
         && dir[2] == '\0'
      )
      return HAT_UP_MASK;
   else if (
            dir[0] == 'd'
         && dir[1] == 'o'
         && dir[2] == 'w'
         && dir[3] == 'n'
         && dir[4] == '\0'
         )
      return HAT_DOWN_MASK;
   else if (
            dir[0] == 'l'
         && dir[1] == 'e'
         && dir[2] == 'f'
         && dir[3] == 't'
         && dir[4] == '\0'
         )
      return HAT_LEFT_MASK;
   else if (
            dir[0] == 'r'
         && dir[1] == 'i'
         && dir[2] == 'g'
         && dir[3] == 'h'
         && dir[4] == 't'
         && dir[5] == '\0'
         )
      return HAT_RIGHT_MASK;

   return 0;
}

void input_config_parse_joy_button(
      char *s,
      void *data, const char *prefix,
      const char *btn, void *bind_data)
{
   char tmp[64];
   char key[64];
   char key_label[64];
   config_file_t *conf                     = (config_file_t*)data;
   struct retro_keybind *bind              = (struct retro_keybind*)bind_data;
   struct config_entry_list *tmp_a         = NULL;

   tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(key, s,
         "btn", '_', sizeof(key));
   fill_pathname_join_delim(key_label, s,
         "btn_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      btn = tmp;
      if (     btn[0] == 'n'
            && btn[1] == 'u'
            && btn[2] == 'l'
            && btn[3] == '\0'
         )
         bind->joykey = NO_BTN;
      else
      {
         if (*btn == 'h')
         {
            const char *str = btn + 1;
            /* Parse hat? */
            if (str && ISDIGIT((int)*str))
            {
               char        *dir = NULL;
               uint16_t     hat = strtoul(str, &dir, 0);
               uint16_t hat_dir = dir ? input_config_parse_hat(dir) : 0;
               if (hat_dir)
                  bind->joykey = HAT_MAP(hat, hat_dir);
            }
         }
         else
            bind->joykey = strtoull(tmp, NULL, 0);
      }
   }

   tmp_a = config_get_entry(conf, key_label);

   if (tmp_a && !string_is_empty(tmp_a->value))
   {
      if (!string_is_empty(bind->joykey_label))
         free(bind->joykey_label);

      bind->joykey_label = strdup(tmp_a->value);
   }
}

void rarch_config_deinit(void)
{
   if (config_st)
      free(config_st);
   config_st = NULL;
}

void rarch_config_init(void)
{
   if (config_st)
      return;
   config_st = (settings_t*)calloc(1, sizeof(settings_t));
}
