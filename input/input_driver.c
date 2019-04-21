/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <encodings/utf.h>
#include <string/stdstring.h>
#include <math/float_minmax.h>

#include <retro_assert.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_NETWORKGAMEPAD
#include "input_remote.h"
#endif

#ifdef HAVE_OVERLAY
#include "input_overlay.h"
#endif

#include "input_mapper.h"

#include "input_driver.h"
#include "input_keymaps.h"
#include "input_remapping.h"

#include "../config.def.keybinds.h"

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_input.h"
#include "../menu/widgets/menu_input_dialog.h"
#endif

#include "../msg_hash.h"
#include "../configuration.h"
#include "../file_path_special.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../movie.h"
#include "../list_special.h"
#include "../verbosity.h"
#include "../tasks/tasks_internal.h"
#include "../command.h"
#include "include/gamepad.h"

static pad_connection_listener_t *pad_connection_listener = NULL;

void set_connection_listener(pad_connection_listener_t *listener)
{
   pad_connection_listener = listener;
}

void fire_connection_listener(unsigned port, input_device_driver_t *driver)
{
   if(!pad_connection_listener)
      return;

   pad_connection_listener->connected(port, driver);
}

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

static uint16_t input_config_vid[MAX_USERS];
static uint16_t input_config_pid[MAX_USERS];

static char input_device_display_names[MAX_INPUT_DEVICES][64];
static char input_device_config_names [MAX_INPUT_DEVICES][64];
static char input_device_config_paths [MAX_INPUT_DEVICES][64];
char        input_device_names        [MAX_INPUT_DEVICES][64];

uint64_t lifecycle_state;
struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];
const struct retro_keybind *libretro_input_binds[MAX_USERS];

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
bool input_driver_flushing_input                  = false;
static float input_driver_axis_threshold          = 0.0f;
static unsigned input_driver_max_users            = 0;

#ifdef HAVE_HID
static const void *hid_data                       = NULL;
#endif

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

const input_driver_t *input_get_ptr(void)
{
   return current_input;
}

const input_driver_t **input_get_double_ptr(void)
{
   return &current_input;
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
void input_poll(void)
{
   size_t i;
   settings_t *settings           = config_get_ptr();
   uint8_t max_users              = (uint8_t)input_driver_max_users;

   current_input->poll(current_input_data);

   input_driver_turbo_btns.count++;

   for (i = 0; i < max_users; i++)
      input_driver_turbo_btns.frame_enable[i] = 0;

   if (input_driver_block_libretro_input)
      return;

   for (i = 0; i < max_users; i++)
   {
      if (libretro_input_binds[i][RARCH_TURBO_ENABLE].valid)
      {
         rarch_joypad_info_t joypad_info;
         joypad_info.axis_threshold = input_driver_axis_threshold;
         joypad_info.joy_idx        = settings->uints.input_joypad_map[i];
         joypad_info.auto_binds     = input_autoconf_binds[joypad_info.joy_idx];

         input_driver_turbo_btns.frame_enable[i] = current_input->input_state(
               current_input_data, joypad_info, libretro_input_binds,
               (unsigned)i, RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
      }
   }

#ifdef HAVE_OVERLAY
   if (overlay_ptr && input_overlay_is_alive(overlay_ptr))
      input_poll_overlay(
            overlay_ptr,
            settings->floats.input_overlay_opacity,
            settings->uints.input_analog_dpad_mode[0],
            input_driver_axis_threshold);
#endif

   if (settings->bools.input_remap_binds_enable && input_driver_mapper)
      input_mapper_poll(input_driver_mapper);

#ifdef HAVE_COMMAND
   if (input_driver_command)
      command_poll(input_driver_command);
#endif

#ifdef HAVE_NETWORKGAMEPAD
   if (input_driver_remote)
      input_remote_poll(input_driver_remote, max_users);
#endif
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
int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   int16_t res         = 0;
#ifdef HAVE_OVERLAY
   int16_t res_overlay = 0;
#endif

   /* used to reset input state of a button when the gamepad mapper
      is in action for that button*/
   bool reset_state    = false;

   device &= RETRO_DEVICE_MASK;

   if (bsv_movie_is_playback_on())
   {
      int16_t bsv_result;
      if (bsv_movie_get_input(&bsv_result))
         return bsv_result;

      bsv_movie_ctl(BSV_MOVIE_CTL_SET_END, NULL);
   }

   if (     !input_driver_flushing_input
         && !input_driver_block_libretro_input)
   {
      settings_t *settings = config_get_ptr();

      if (settings->bools.input_remap_binds_enable)
      {
         switch (device)
         {
            case RETRO_DEVICE_JOYPAD:
               if (id != settings->uints.input_remap_ids[port][id])
                  reset_state = true;
               break;
            case RETRO_DEVICE_ANALOG:
               if (idx < 2 && id < 2)
               {
                  unsigned offset = RARCH_FIRST_CUSTOM_BIND + (idx * 4) + (id * 2);
                  if (settings->uints.input_remap_ids[port][offset]   != offset)
                     reset_state = true;
                  if (settings->uints.input_remap_ids[port][offset+1] != (offset+1))
                     reset_state = true;
               }
               break;
         }
      }

#ifdef HAVE_OVERLAY
      if (overlay_ptr)
         input_state_overlay(overlay_ptr,
               &res_overlay, port, device, idx, id);
#endif

#ifdef HAVE_NETWORKGAMEPAD
      if (input_driver_remote)
         input_remote_state(&res, port, device, idx, id);
#endif

      if (((id < RARCH_FIRST_META_KEY) || (device == RETRO_DEVICE_KEYBOARD)))
      {
         bool bind_valid = libretro_input_binds[port] && libretro_input_binds[port][id].valid;

         if (bind_valid || device == RETRO_DEVICE_KEYBOARD)
         {
            rarch_joypad_info_t joypad_info;
            joypad_info.axis_threshold = input_driver_axis_threshold;
            joypad_info.joy_idx        = settings->uints.input_joypad_map[port];
            joypad_info.auto_binds     = input_autoconf_binds[joypad_info.joy_idx];

            if (!reset_state)
            {
               res = current_input->input_state(
                     current_input_data, joypad_info, libretro_input_binds, port, device, idx, id);

#ifdef HAVE_OVERLAY
               if (input_overlay_is_alive(overlay_ptr) && port == 0)
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

      /* Don't allow turbo for D-pad. */
      if (device == RETRO_DEVICE_JOYPAD && (id < RETRO_DEVICE_ID_JOYPAD_UP ||
               id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
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

   if (bsv_movie_is_playback_off())
      bsv_movie_ctl(BSV_MOVIE_CTL_SET_INPUT, &res);

   return res;
}

/**
 * state_tracker_update_input:
 *
 * Updates 16-bit input in same format as libretro API itself.
 **/
void state_tracker_update_input(uint16_t *input1, uint16_t *input2)
{
   unsigned i;
   const struct retro_keybind *binds[MAX_USERS];
   settings_t *settings = config_get_ptr();
   uint8_t max_users    = (uint8_t)input_driver_max_users;

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = input_config_binds[i];
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];
      binds[i]                            = input_config_binds[i];

      if (dpad_mode == ANALOG_DPAD_NONE)
         continue;

      input_push_analog_dpad(general_binds, dpad_mode);
      input_push_analog_dpad(auto_binds,    dpad_mode);
   }

   if (!input_driver_block_libretro_input)
   {
      rarch_joypad_info_t joypad_info;
      joypad_info.axis_threshold = input_driver_axis_threshold;

      for (i = 4; i < 16; i++)
      {
         unsigned id     = buttons[i - 4];

         if (binds[0][id].valid)
         {
            joypad_info.joy_idx        = settings->uints.input_joypad_map[0];
            joypad_info.auto_binds     = input_autoconf_binds[joypad_info.joy_idx];
            *input1 |= (current_input->input_state(current_input_data, joypad_info,
                     binds,
                     0, RETRO_DEVICE_JOYPAD, 0, id) ? 1 : 0) << i;
         }

         if (binds[1][id].valid)
         {
            joypad_info.joy_idx        = settings->uints.input_joypad_map[1];
            joypad_info.auto_binds     = input_autoconf_binds[joypad_info.joy_idx];
            *input2 |= (current_input->input_state(current_input_data, joypad_info,
                     binds,
                     1, RETRO_DEVICE_JOYPAD, 0, id) ? 1 : 0) << i;
         }
      }
   }

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = input_config_binds[i];
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];

      input_pop_analog_dpad(general_binds);
      input_pop_analog_dpad(auto_binds);
   }
}

static INLINE bool input_keys_pressed_iterate(unsigned i,
      input_bits_t* p_new_state)
{
   if ((i >= RARCH_FIRST_META_KEY) &&
         BIT64_GET(lifecycle_state, i)
      )
      return true;

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
   settings_t *settings           = config_get_ptr();
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
         new_val = 0x7fff;
      else if (new_val < -0x7fff)
         new_val = -0x7fff;

      return new_val;
   }

   return val;
}

#ifdef HAVE_MENU

/**
 * input_menu_keys_pressed:
 *
 * Grab an input sample for this frame. We exclude
 * keyboard input here.
 *
 * Returns: Input sample containing a mask of all pressed keys.
 */
void input_menu_keys_pressed(void *data, input_bits_t *p_new_state)
{
   unsigned i, port;
   rarch_joypad_info_t joypad_info;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   settings_t     *settings                     = (settings_t*)data;
   uint8_t max_users                            = (uint8_t)input_driver_max_users;
   uint8_t port_max                             =
      settings->bools.input_all_users_control_menu
      ? max_users : 1;

   joypad_info.joy_idx                          = 0;
   joypad_info.auto_binds                       = NULL;

   input_driver_block_libretro_input            = false;
   input_driver_block_hotkey                    = false;

   if (current_input->keyboard_mapping_is_blocked
         && current_input->keyboard_mapping_is_blocked(current_input_data))
      input_driver_block_hotkey = true;

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

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      bool bit_pressed    = false;

      if (
              (!input_driver_block_libretro_input && ((i < RARCH_FIRST_META_KEY)))
            || !input_driver_block_hotkey
         )
      {
         const input_device_driver_t *first = current_input->get_joypad_driver
            ? current_input->get_joypad_driver(current_input_data) : NULL;
         const input_device_driver_t *sec   = current_input->get_sec_joypad_driver
            ? current_input->get_sec_joypad_driver(current_input_data) : NULL;

         for (port = 0; port < port_max; port++)
         {
            uint16_t              joykey      = 0;
            uint32_t              joyaxis     = 0;
            const struct retro_keybind *mtkey = &input_config_binds[port][i];

            if (!mtkey->valid)
               continue;

            joypad_info.joy_idx               = settings->uints.input_joypad_map[port];
            joypad_info.auto_binds            = input_autoconf_binds[joypad_info.joy_idx];
            joypad_info.axis_threshold        = input_driver_axis_threshold;

            joykey                            = (input_config_binds[port][i].joykey != NO_BTN)
               ? input_config_binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
            joyaxis                           = (input_config_binds[port][i].joyaxis != AXIS_NONE)
               ? input_config_binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;

            if (sec)
            {
               if (joykey == NO_BTN || !sec->button(joypad_info.joy_idx, joykey))
               {
                  float    scaled_axis = sec->axis ? ((float)abs(input_joypad_axis(sec, joypad_info.joy_idx, joyaxis)) / 0x8000) : 0.0;
                  bit_pressed          = scaled_axis > joypad_info.axis_threshold;
               }
               else
                  bit_pressed          = true;
            }

            if (!bit_pressed && first)
            {
               if (joykey == NO_BTN || !first->button(joypad_info.joy_idx, joykey))
               {
                  float    scaled_axis = first->axis ? ((float)abs(input_joypad_axis(first, joypad_info.joy_idx, joyaxis)) / 0x8000) : 0.0;
                  bit_pressed          = scaled_axis > joypad_info.axis_threshold;
               }
               else
                  bit_pressed          = true;
            }

            if (bit_pressed)
               break;
         }
      }

      if (!bit_pressed)
         bit_pressed = input_keys_pressed_iterate(i, p_new_state);

      if (bit_pressed)
      {
         BIT256_SET_PTR(p_new_state, i);
      }
   }

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      input_pop_analog_dpad(auto_binds);
   }

   if (!menu_input_dialog_get_display_kb())
   {
      unsigned ids[18][2];
      const struct retro_keybind *quitkey = &input_config_binds[0][RARCH_QUIT_KEY];
      const struct retro_keybind *fskey   = &input_config_binds[0][RARCH_FULLSCREEN_TOGGLE_KEY];
      const struct retro_keybind *companionkey = &input_config_binds[0][RARCH_UI_COMPANION_TOGGLE];
      const struct retro_keybind *fpskey = &input_config_binds[0][RARCH_FPS_TOGGLE];
      const struct retro_keybind *debugkey = &input_config_binds[0][RARCH_SEND_DEBUG_INFO];
      const struct retro_keybind *netplaykey = &input_config_binds[0][RARCH_NETPLAY_HOST_TOGGLE];

      ids[0][0]  = RETROK_SPACE;
      ids[0][1]  = RETRO_DEVICE_ID_JOYPAD_START;
      ids[1][0]  = RETROK_SLASH;
      ids[1][1]  = RETRO_DEVICE_ID_JOYPAD_X;
      ids[2][0]  = RETROK_RSHIFT;
      ids[2][1]  = RETRO_DEVICE_ID_JOYPAD_SELECT;
      ids[3][0]  = RETROK_RIGHT;
      ids[3][1]  = RETRO_DEVICE_ID_JOYPAD_RIGHT;
      ids[4][0]  = RETROK_LEFT;
      ids[4][1]  = RETRO_DEVICE_ID_JOYPAD_LEFT;
      ids[5][0]  = RETROK_DOWN;
      ids[5][1]  = RETRO_DEVICE_ID_JOYPAD_DOWN;
      ids[6][0]  = RETROK_UP;
      ids[6][1]  = RETRO_DEVICE_ID_JOYPAD_UP;
      ids[7][0]  = RETROK_PAGEUP;
      ids[7][1]  = RETRO_DEVICE_ID_JOYPAD_L;
      ids[8][0]  = RETROK_PAGEDOWN;
      ids[8][1]  = RETRO_DEVICE_ID_JOYPAD_R;
      ids[9][0]  = quitkey->key;
      ids[9][1]  = RARCH_QUIT_KEY;
      ids[10][0] = fskey->key;
      ids[10][1] = RARCH_FULLSCREEN_TOGGLE_KEY;
      ids[11][0] = RETROK_BACKSPACE;
      ids[11][1] = RETRO_DEVICE_ID_JOYPAD_B;
      ids[12][0] = RETROK_RETURN;
      ids[12][1] = RETRO_DEVICE_ID_JOYPAD_A;
      ids[13][0] = RETROK_DELETE;
      ids[13][1] = RETRO_DEVICE_ID_JOYPAD_Y;
      ids[14][0] = companionkey->key;
      ids[14][1] = RARCH_UI_COMPANION_TOGGLE;
      ids[15][0] = fpskey->key;
      ids[15][1] = RARCH_FPS_TOGGLE;
      ids[16][0] = debugkey->key;
      ids[16][1] = RARCH_SEND_DEBUG_INFO;
      ids[17][0] = netplaykey->key;
      ids[17][1] = RARCH_NETPLAY_HOST_TOGGLE;

      if (settings->bools.input_menu_swap_ok_cancel_buttons)
      {
         ids[11][1] = RETRO_DEVICE_ID_JOYPAD_A;
         ids[12][1] = RETRO_DEVICE_ID_JOYPAD_B;
      }

      for (i = 0; i < 18; i++)
      {
         if (current_input->input_state(current_input_data,
                  joypad_info, binds, 0,
                  RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
            BIT256_SET_PTR(p_new_state, ids[i][1]);
      }
   }
}
#endif

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

/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 *
 * Returns: Input sample containing a mask of all pressed keys.
 */
void input_keys_pressed(void *data, input_bits_t *p_new_state)
{
   unsigned i;
   rarch_joypad_info_t joypad_info;
   settings_t              *settings            = (settings_t*)data;
   const struct retro_keybind *binds            = input_config_binds[0];
   const struct retro_keybind *binds_auto       = &input_autoconf_binds[0][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *binds_norm       = &binds[RARCH_ENABLE_HOTKEY];

   joypad_info.joy_idx                          = settings->uints.input_joypad_map[0];
   joypad_info.auto_binds                       = input_autoconf_binds[joypad_info.joy_idx];
   joypad_info.axis_threshold                   = input_driver_axis_threshold;

   input_driver_block_libretro_input            = false;
   input_driver_block_hotkey                    = false;

   if (     current_input->keyboard_mapping_is_blocked
         && current_input->keyboard_mapping_is_blocked(current_input_data))
      input_driver_block_hotkey = true;

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

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      bool bit_pressed = false;

      if (
            ((!input_driver_block_libretro_input && ((i < RARCH_FIRST_META_KEY)))
             || !input_driver_block_hotkey) &&
            binds[i].valid && current_input->input_state(current_input_data,
               joypad_info, &binds,
               0, RETRO_DEVICE_JOYPAD, 0, i)
         )
         bit_pressed = true;
      else if (input_keys_pressed_iterate(i, p_new_state))
         bit_pressed = true;

      if (bit_pressed)
      {
         BIT256_SET_PTR(p_new_state, i);
      }
   }
}

void input_get_state_for_port(void *data, unsigned port, input_bits_t *p_new_state)
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
      bool bit_pressed = false;
      int16_t val;
      val = input_joypad_analog(joypad_driver, joypad_info, port, RETRO_DEVICE_INDEX_ANALOG_BUTTON, i, libretro_input_binds[port]);

      if (input_driver_input_state(joypad_info, libretro_input_binds,
               port, RETRO_DEVICE_JOYPAD, 0, i) != 0)
         bit_pressed = true;

      if (bit_pressed)
         BIT256_SET_PTR(p_new_state, i);
      if (bit_pressed && val)
         p_new_state->analog_buttons[i] = val;
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

void **input_driver_get_data_ptr(void)
{
   return (void**)&current_input_data;
}

bool input_driver_has_capabilities(void)
{
   if (!current_input->get_capabilities || !current_input_data)
      return false;
   return true;
}

bool input_driver_init(void)
{
   if (current_input)
   {
      settings_t *settings    = config_get_ptr();
      current_input_data      = current_input->init(settings->arrays.input_joypad_driver);
   }

   if (!current_input_data)
      return false;
   return true;
}

void input_driver_deinit(void)
{
   if (current_input && current_input->free)
      current_input->free(current_input_data);
   current_input_data = NULL;
}

void input_driver_destroy_data(void)
{
   current_input_data = NULL;
}

void input_driver_destroy(void)
{
   input_driver_keyboard_linefeed_enable = false;
   input_driver_block_hotkey             = false;
   input_driver_block_libretro_input     = false;
   input_driver_nonblock_state           = false;
   input_driver_flushing_input           = false;
   memset(&input_driver_turbo_btns, 0, sizeof(turbo_buttons_t));
   current_input                         = NULL;
}

bool input_driver_grab_stdin(void)
{
   if (!current_input->grab_stdin)
      return false;
   return current_input->grab_stdin(current_input_data);
}

bool input_driver_keyboard_mapping_is_blocked(void)
{
   return current_input->keyboard_mapping_is_blocked(
         current_input_data);
}

bool input_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = config_get_ptr();

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

bool input_driver_is_nonblock_state(void)
{
   return input_driver_nonblock_state;
}

bool input_driver_init_command(void)
{
#ifdef HAVE_COMMAND
   settings_t *settings          = config_get_ptr();
   bool input_stdin_cmd_enable   = settings->bools.stdin_cmd_enable;
   bool input_network_cmd_enable = settings->bools.network_cmd_enable;
   bool grab_stdin               = input_driver_grab_stdin();

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
   settings_t *settings = config_get_ptr();

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
   settings_t *settings = config_get_ptr();

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

bool input_driver_is_data_ptr_same(void *data)
{
   return (current_input_data == data);
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
 * input_joypad_init_first:
 *
 * Finds first suitable joypad driver and initializes.
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_first(void *data)
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
   settings_t *settings = config_get_ptr();
   unsigned joy_idx     = settings->uints.input_joypad_map[port];

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

      input_conv_analog_id_to_bind_id(idx, ident, &ident_minus, &ident_plus);

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
   settings_t *settings = config_get_ptr();

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
   if(port >= MAX_USERS || !driver)
   {
      RARCH_ERR("[input]: input_pad_connect: bad parameters\n");
      return;
   }

   fire_connection_listener(port, driver);

   if(!input_autoconfigure_connect(driver->name(port), NULL, driver->ident,
          port, 0, 0))
      input_config_set_device_name(port, driver->name(port));
}

/**
 * input_conv_analog_id_to_bind_id:
 * @idx                     : Analog key index.
 *                            E.g.:
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @ident_minus             : Bind ID minus, will be set by function.
 * @ident_plus              : Bind ID plus,  will be set by function.
 *
 * Takes as input analog key identifiers and converts
 * them to corresponding bind IDs @ident_minus and @ident_plus.
 **/
void input_conv_analog_id_to_bind_id(unsigned idx, unsigned ident,
      unsigned *ident_minus, unsigned *ident_plus)
{
   switch ((idx << 1) | ident)
   {
      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_X:
         *ident_minus = RARCH_ANALOG_LEFT_X_MINUS;
         *ident_plus  = RARCH_ANALOG_LEFT_X_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
         *ident_minus = RARCH_ANALOG_LEFT_Y_MINUS;
         *ident_plus  = RARCH_ANALOG_LEFT_Y_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_X:
         *ident_minus = RARCH_ANALOG_RIGHT_X_MINUS;
         *ident_plus  = RARCH_ANALOG_RIGHT_X_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
         *ident_minus = RARCH_ANALOG_RIGHT_Y_MINUS;
         *ident_plus  = RARCH_ANALOG_RIGHT_Y_PLUS;
         break;
   }
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
   input_driver_keyboard_mapping_set_block(true);

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
      input_driver_keyboard_mapping_set_block(false);
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
      input_driver_keyboard_mapping_set_block(false);
   }
   else
   {
      retro_keyboard_event_t *key_event = NULL;
      rarch_ctl(RARCH_CTL_KEY_EVENT_GET, &key_event);

      if (key_event && *key_event)
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
         input_driver_keyboard_mapping_set_block(true);
         break;
      case RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS:
         g_keyboard_press_cb   = NULL;
         g_keyboard_press_data = NULL;
         input_driver_keyboard_mapping_set_block(false);
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
   settings_t *settings = config_get_ptr();
   bool label_show      = settings->bools.input_descriptor_label_show;

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
   settings_t *settings = config_get_ptr();

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
   settings_t *settings = config_get_ptr();
   return &settings->uints.input_libretro_device[port];
}

unsigned input_config_get_device(unsigned port)
{
   settings_t *settings = config_get_ptr();
   return settings->uints.input_libretro_device[port];
}

void input_config_set_device(unsigned port, unsigned id)
{
   settings_t *settings = config_get_ptr();

   if (settings)
      settings->uints.input_libretro_device[port] = id;
}


const struct retro_keybind *input_config_get_bind_auto(
      unsigned port, unsigned id)
{
   settings_t *settings = config_get_ptr();
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
