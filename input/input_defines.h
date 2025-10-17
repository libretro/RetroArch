/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __INPUT_DEFINES__H
#define __INPUT_DEFINES__H

#include <stdint.h>
#include <string.h>

#include <retro_common_api.h>

#define MAX_USERS                      16

#define MAX_INPUT_DEVICES              16

#define RARCH_MAX_KEYS                 155

#define RARCH_FIRST_CUSTOM_BIND        16
#define RARCH_FIRST_LIGHTGUN_BIND      RARCH_ANALOG_BIND_LIST_END
#define RARCH_FIRST_MISC_CUSTOM_BIND   RARCH_LIGHTGUN_BIND_LIST_END
#define RARCH_FIRST_META_KEY           RARCH_CUSTOM_BIND_LIST_END

#define RARCH_UNMAPPED                 1024

/* Specialized _MOUSE that targets the full screen regardless of viewport.
 */
#define RARCH_DEVICE_MOUSE_SCREEN      (RETRO_DEVICE_MOUSE | 0x10000)

/* Specialized _POINTER that targets the full screen regardless of viewport.
 * Should not be used by a libretro implementation as coordinates returned
 * make no sense.
 *
 * It is only used internally for overlays. */
#define RARCH_DEVICE_POINTER_SCREEN    (RETRO_DEVICE_POINTER | 0x10000)

#define RARCH_DEVICE_ID_POINTER_BACK   (RETRO_DEVICE_ID_POINTER_PRESSED | 0x10000)

/* libretro has 16 buttons from 0-15 (libretro.h)
 * Analog binds use RETRO_DEVICE_ANALOG, but we follow the same scheme
 * internally in RetroArch for simplicity, so they are mapped into [16, 23].
 */

#define AXIS_NEG(x)        (((uint32_t)(x) << 16) | 0xFFFFU)
#define AXIS_POS(x)        ((uint32_t)(x) | 0xFFFF0000UL)
#define AXIS_NONE          0xFFFFFFFFUL
#define AXIS_DIR_NONE      0xFFFFU

#define AXIS_NEG_GET(x)    (((uint32_t)(x) >> 16) & 0xFFFFU)
#define AXIS_POS_GET(x)    ((uint32_t)(x) & 0xFFFFU)

#define NO_BTN             0xFFFFU

#define HAT_UP_SHIFT       15
#define HAT_DOWN_SHIFT     14
#define HAT_LEFT_SHIFT     13
#define HAT_RIGHT_SHIFT    12
#define HAT_UP_MASK        (1 << HAT_UP_SHIFT)
#define HAT_DOWN_MASK      (1 << HAT_DOWN_SHIFT)
#define HAT_LEFT_MASK      (1 << HAT_LEFT_SHIFT)
#define HAT_RIGHT_MASK     (1 << HAT_RIGHT_SHIFT)
#define HAT_MAP(x, hat)    ((x & ((1 << 12) - 1)) | hat)

#define HAT_MASK           (HAT_UP_MASK | HAT_DOWN_MASK | HAT_LEFT_MASK | HAT_RIGHT_MASK)
#define GET_HAT_DIR(x)     (x & HAT_MASK)
#define GET_HAT(x)         (x & (~HAT_MASK))

#ifdef HAVE_BSV_MOVIE
#define BSV_MOVIE_IS_PLAYBACK_ON() (input_st->bsv_movie_state_handle && (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK) && !(input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_END))
#define BSV_MOVIE_IS_RECORDING() (input_st->bsv_movie_state_handle && (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING))

#endif

RETRO_BEGIN_DECLS

/* RetroArch specific bind IDs. */
enum
{
   /* Custom binds that extend the scope of RETRO_DEVICE_JOYPAD for
    * RetroArch specifically.
    * Analogs (RETRO_DEVICE_ANALOG) */
   RARCH_ANALOG_LEFT_X_PLUS = RARCH_FIRST_CUSTOM_BIND,
   RARCH_ANALOG_LEFT_X_MINUS,
   RARCH_ANALOG_LEFT_Y_PLUS,
   RARCH_ANALOG_LEFT_Y_MINUS,
   RARCH_ANALOG_RIGHT_X_PLUS,
   RARCH_ANALOG_RIGHT_X_MINUS,
   RARCH_ANALOG_RIGHT_Y_PLUS,
   RARCH_ANALOG_RIGHT_Y_MINUS,
   RARCH_ANALOG_BIND_LIST_END,

   /* Lightgun */
   RARCH_LIGHTGUN_TRIGGER = RARCH_FIRST_LIGHTGUN_BIND,
   RARCH_LIGHTGUN_RELOAD,
   RARCH_LIGHTGUN_AUX_A,
   RARCH_LIGHTGUN_AUX_B,
   RARCH_LIGHTGUN_AUX_C,
   RARCH_LIGHTGUN_START,
   RARCH_LIGHTGUN_SELECT,
   RARCH_LIGHTGUN_DPAD_UP,
   RARCH_LIGHTGUN_DPAD_DOWN,
   RARCH_LIGHTGUN_DPAD_LEFT,
   RARCH_LIGHTGUN_DPAD_RIGHT,
   RARCH_LIGHTGUN_BIND_LIST_END,

   /* Turbo */
   RARCH_TURBO_ENABLE = RARCH_FIRST_MISC_CUSTOM_BIND,

   RARCH_CUSTOM_BIND_LIST_END,

   /* Command binds. Not related to game input,
    * only usable for port 0. */
   RARCH_ENABLE_HOTKEY = RARCH_FIRST_META_KEY,
   RARCH_MENU_TOGGLE,
   RARCH_QUIT_KEY,
   RARCH_CLOSE_CONTENT_KEY,
   RARCH_RESET,
   RARCH_FAST_FORWARD_KEY,
   RARCH_FAST_FORWARD_HOLD_KEY,
   RARCH_SLOWMOTION_KEY,
   RARCH_SLOWMOTION_HOLD_KEY,
   RARCH_REWIND,
   RARCH_PAUSE_TOGGLE,
   RARCH_FRAMEADVANCE,

   RARCH_MUTE,
   RARCH_VOLUME_UP,
   RARCH_VOLUME_DOWN,

   RARCH_LOAD_STATE_KEY,
   RARCH_SAVE_STATE_KEY,
   RARCH_STATE_SLOT_PLUS,
   RARCH_STATE_SLOT_MINUS,

   RARCH_PLAY_REPLAY_KEY,
   RARCH_RECORD_REPLAY_KEY,
   RARCH_HALT_REPLAY_KEY,
   RARCH_SAVE_REPLAY_CHECKPOINT_KEY,
   RARCH_PREV_REPLAY_CHECKPOINT_KEY,
   RARCH_NEXT_REPLAY_CHECKPOINT_KEY,
   RARCH_REPLAY_SLOT_PLUS,
   RARCH_REPLAY_SLOT_MINUS,

   RARCH_DISK_EJECT_TOGGLE,
   RARCH_DISK_NEXT,
   RARCH_DISK_PREV,

   RARCH_SHADER_TOGGLE,
   RARCH_SHADER_HOLD,
   RARCH_SHADER_NEXT,
   RARCH_SHADER_PREV,

   RARCH_CHEAT_TOGGLE,
   RARCH_CHEAT_INDEX_PLUS,
   RARCH_CHEAT_INDEX_MINUS,

   RARCH_SCREENSHOT,
   RARCH_RECORDING_TOGGLE,
   RARCH_STREAMING_TOGGLE,

   RARCH_TURBO_FIRE_TOGGLE,
   RARCH_GRAB_MOUSE_TOGGLE,
   RARCH_GAME_FOCUS_TOGGLE,
   RARCH_FULLSCREEN_TOGGLE_KEY,
   RARCH_UI_COMPANION_TOGGLE,

   RARCH_VRR_RUNLOOP_TOGGLE,
   RARCH_RUNAHEAD_TOGGLE,
   RARCH_PREEMPT_TOGGLE,
   RARCH_FPS_TOGGLE,
   RARCH_STATISTICS_TOGGLE,
   RARCH_AI_SERVICE,

   RARCH_NETPLAY_PING_TOGGLE,
   RARCH_NETPLAY_HOST_TOGGLE,
   RARCH_NETPLAY_GAME_WATCH,
   RARCH_NETPLAY_PLAYER_CHAT,
   RARCH_NETPLAY_FADE_CHAT_TOGGLE,

   RARCH_OVERLAY_NEXT,
   RARCH_OSK,

   RARCH_BIND_LIST_END,

   RARCH_BIND_LIST_END_NULL
};

enum analog_dpad_mode
{
   ANALOG_DPAD_NONE = 0,
   ANALOG_DPAD_LSTICK,
   ANALOG_DPAD_RSTICK,
   ANALOG_DPAD_LSTICK_FORCED,
   ANALOG_DPAD_RSTICK_FORCED,
   ANALOG_DPAD_LAST
};

enum input_combo_type
{
   INPUT_COMBO_NONE = 0,
   INPUT_COMBO_DOWN_Y_L_R,
   INPUT_COMBO_L3_R3,
   INPUT_COMBO_L1_R1_START_SELECT,
   INPUT_COMBO_START_SELECT,
   INPUT_COMBO_L3_R,
   INPUT_COMBO_L_R,
   INPUT_COMBO_HOLD_START,
   INPUT_COMBO_HOLD_SELECT,
   INPUT_COMBO_DOWN_SELECT,
   INPUT_COMBO_L2_R2,
   INPUT_COMBO_LAST
};

enum input_turbo_mode
{
   INPUT_TURBO_MODE_CLASSIC = 0,
   INPUT_TURBO_MODE_CLASSIC_TOGGLE,
   INPUT_TURBO_MODE_SINGLEBUTTON,
   INPUT_TURBO_MODE_SINGLEBUTTON_HOLD,
   INPUT_TURBO_MODE_LAST
};

enum input_device_reservation_type
{
  INPUT_DEVICE_RESERVATION_NONE = 0,
  INPUT_DEVICE_RESERVATION_PREFERRED,
  INPUT_DEVICE_RESERVATION_RESERVED,
  INPUT_DEVICE_RESERVATION_LAST
};

RETRO_END_DECLS

#endif
