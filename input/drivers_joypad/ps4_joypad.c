/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - CatalystG
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

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#include <orbis/libScePad.h>
#include <defines/ps4_defines.h>

#include "../input_driver.h"
#include "../include/input_pad_view.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"


#if defined(ORBIS)
#include <orbis/orbisPad.h>

OrbisPadConfig *confPad;

typedef struct SceUserServiceLoginUserIdList
{
   int32_t userId[SCE_USER_SERVICE_MAX_LOGIN_USERS];
} SceUserServiceLoginUserIdList;

int sceUserServiceGetLoginUserIdList(
      SceUserServiceLoginUserIdList* userIdList);
#endif

/*
 * Global var's
 */
typedef struct
{
   SceUserServiceUserId userId;
   int handle[PS4_MAX_PAD_PORT_TYPES];
   bool connected; /* the handle is open */
   bool reporting; /* the last read came from a connected pad */
} ds_joypad_state;

/* TODO/FIXME - static globals */
static ds_joypad_state ds_joypad_states[PS4_MAX_ORBISPADS];
/* each pad as its poll left it, lent to the readers (input_pad_view.h):
 * the buttons as RetroPad ids, axes 0-3 the sticks' x and y */
static input_pad_view_t ps4_pads[PS4_MAX_ORBISPADS];
static int16_t num_players = 0;

static INLINE int16_t convert_u8_to_s16(uint8_t val)
{
   if (val == 0)
      return -0x7fff;
   return val * 0x0101 - 0x8000;
}

static const char *ps4_joypad_name(unsigned pad)
{
   return "PS4 Controller";
}

static void *ps4_joypad_init(void *data)
{
   int result;
   SceUserServiceLoginUserIdList user_id_list;

   num_players = 0;

   scePadInit();

   confPad     = orbisPadGetConf();
   result      = sceUserServiceGetLoginUserIdList(&user_id_list);

   if (result == 0)
   {
      int i;
      for (i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++)
      {
         SceUserServiceUserId user_id = user_id_list.userId[i];

         if (user_id != SCE_USER_SERVICE_USER_ID_INVALID)
         {
            int index = 0;

            while (index < num_players)
            {
               ds_joypad_states[index].userId = user_id;
               index++;
            }

            if (index == num_players)
            {
               ds_joypad_states[num_players].handle[0] = scePadOpen(user_id, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
               if (ds_joypad_states[num_players].handle[0] == SCE_ORBISPAD_ERROR_ALREADY_OPENED)
                  ds_joypad_states[num_players].handle[0] = confPad->padHandle;

               if (  ds_joypad_states[num_players].handle[0] > 0 ||
                     ds_joypad_states[num_players].handle[1] > 0 ||
                     ds_joypad_states[num_players].handle[2] > 0)
               {
                  ds_joypad_states[num_players].connected = true;
                  ds_joypad_states[num_players].userId    = user_id;

                  input_autoconfigure_connect(
                        ps4_joypad_name(num_players),
                        NULL, NULL,
                        ps4_joypad.ident,
                        num_players,
                        0,
                        0);
                  num_players++;
               }
            }
         }
      }
   }

   return (void*)-1;
}

static int32_t ps4_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= PS4_MAX_ORBISPADS)
      return 0;
   return input_pad_view_button(&ps4_pads[port], joykey);
}

static int16_t ps4_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= PS4_MAX_ORBISPADS)
      return 0;
   return input_pad_view_axis(&ps4_pads[port], joyaxis);
}

static int16_t ps4_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   uint16_t port_idx = joypad_info->joy_idx;
   if (port_idx >= PS4_MAX_ORBISPADS)
      return 0;
   return input_pad_view_state(&ps4_pads[port_idx], joypad_info,
         joykeys, joyaxes);
}

static void ps4_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   if (port_num < PS4_MAX_ORBISPADS)
      *state = ps4_pads[port_num].buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

/* Read in the poll alone: the same until the next */
static const input_pad_view_t *ps4_joypad_view(unsigned port)
{
   return (port < PS4_MAX_ORBISPADS) ? &ps4_pads[port] : NULL;
}

static void ps4_joypad_poll(void)
{
   unsigned player;
   ScePadData buttons;

   for (player = 0; player < SCE_USER_SERVICE_MAX_LOGIN_USERS; player++)
   {
      int ret;
      unsigned j;
      input_pad_view_t *v = &ps4_pads[player];

      if (!ds_joypad_states[player].connected)
         continue;

      ret     = scePadReadState(ds_joypad_states[player].handle[0],&buttons);

      ds_joypad_states[player].reporting = (ret == 0 && buttons.connected);

      if (ds_joypad_states[player].reporting)
      {
         int32_t state_tmp = buttons.buttons;
         uint64_t bits     = 0;

         bits |= (state_tmp & ORBISPAD_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         bits |= (state_tmp & ORBISPAD_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         bits |= (state_tmp & ORBISPAD_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         bits |= (state_tmp & ORBISPAD_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         bits |= (state_tmp & ORBISPAD_OPTIONS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         bits |= (state_tmp & ORBISPAD_TOUCH_PAD) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         bits |= (state_tmp & ORBISPAD_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         bits |= (state_tmp & ORBISPAD_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         bits |= (state_tmp & ORBISPAD_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         bits |= (state_tmp & ORBISPAD_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         bits |= (state_tmp & ORBISPAD_R1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         bits |= (state_tmp & ORBISPAD_L1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         bits |= (state_tmp & ORBISPAD_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         bits |= (state_tmp & ORBISPAD_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         bits |= (state_tmp & ORBISPAD_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         bits |= (state_tmp & ORBISPAD_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         INPUT_PAD_VIEW_SET_BITS64(v, bits);
         v->axes[0] = convert_u8_to_s16(buttons.lx);
         v->axes[1] = convert_u8_to_s16(buttons.ly);
         v->axes[2] = convert_u8_to_s16(buttons.rx);
         v->axes[3] = convert_u8_to_s16(buttons.ry);
      }
      else
      {
         /* Unread or not reporting: the handle stays for when it does. */
         INPUT_PAD_VIEW_SET_BITS64(v, 0);
         for (j = 0; j < 4; j++)
            v->axes[j] = 0;
      }
      for (j = 0; j < 4; j++)
         if (v->axes[j] == -0x8000)
            v->axes[j] = -0x7fff;
   }
}

static bool ps4_joypad_query_pad(unsigned pad)
{
   return pad < PS4_MAX_ORBISPADS && ds_joypad_states[pad].reporting;
}

static bool ps4_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   return false;
}

static void ps4_joypad_destroy(void)
{
}

input_device_driver_t ps4_joypad = {
   ps4_joypad_init,
   ps4_joypad_query_pad,
   ps4_joypad_destroy,
   ps4_joypad_button,
   ps4_joypad_state,
   ps4_joypad_get_buttons,
   ps4_joypad_axis,
   ps4_joypad_poll,
   ps4_joypad_rumble,
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   ps4_joypad_name,
   "ps4",
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   ps4_joypad_view
};
