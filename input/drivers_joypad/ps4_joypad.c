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

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#define LERP(p, f, t) ((((p * 10) * (t * 10)) / (f * 10)) / 10)

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
   bool connected;
} ds_joypad_state;

/* TODO/FIXME - static globals */
static ds_joypad_state ds_joypad_states[PS4_MAX_ORBISPADS];
static uint64_t pad_state[PS4_MAX_ORBISPADS];
static int16_t analog_state[PS4_MAX_ORBISPADS][2][2];
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
   int result, handle;
   SceUserServiceLoginUserIdList user_id_list;

   num_players = 0;

   scePadInit();

   confPad     = orbisPadGetConf();
   result      = sceUserServiceGetLoginUserIdList(&user_id_list);

   if (result == 0)
   {
      unsigned i;
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
#if 0
               scePadGetHandle(user_id, SCE_PAD_PORT_TYPE_STANDARD, 0);

               ds_joypad_states[num_players].handle[1]    = scePadOpen(user_id, SCE_PAD_PORT_TYPE_SPECIAL, 0, NULL);
               if (ds_joypad_states[num_players].handle[1] == SCE_ORBISPAD_ERROR_ALREADY_OPENED)
                  ds_joypad_states[num_players].handle[1] =
                     scePadGetHandle(user_id, SCE_PAD_PORT_TYPE_SPECIAL, 0);

               ds_joypad_states[num_players].handle[2]    = scePadOpen(user_id, SCE_PAD_PORT_TYPE_REMOTE_CONTROL, 0, NULL);
               if (ds_joypad_states[num_players].handle[2] == SCE_ORBISPAD_ERROR_ALREADY_OPENED)
                  ds_joypad_states[num_players].handle[2] =
                     scePadGetHandle(user_id, SCE_PAD_PORT_TYPE_REMOTE_CONTROL, 0);
#endif

               if (  ds_joypad_states[num_players].handle[0] > 0 ||
                     ds_joypad_states[num_players].handle[1] > 0 ||
                     ds_joypad_states[num_players].handle[2] > 0)
               {
                  ds_joypad_states[num_players].connected = true;
                  ds_joypad_states[num_players].userId    = user_id;

                  input_autoconfigure_connect(
                        ps4_joypad_name(num_players),
                        NULL,
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
   return pad_state[port] & (UINT64_C(1) << joykey);
}

static int16_t ps4_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int val     = 0;
   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (joyaxis == AXIS_NONE || port >= PS4_MAX_ORBISPADS)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0:
         val = analog_state[port][0][0];
         break;
      case 1:
         val = analog_state[port][0][1];
         break;
      case 2:
         val = analog_state[port][1][0];
         break;
      case 3:
         val = analog_state[port][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static int16_t ps4_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx >= PS4_MAX_ORBISPADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      if (
               (uint16_t)joykey != NO_BTN
            && pad_state[port_idx] & (UINT64_C(1) << (uint16_t)joykey)
         )
         ret |= ( 1 << i);
   }

   return ret;
}

static void ps4_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
	if (port_num < PS4_MAX_ORBISPADS)
   {
		BITS_COPY16_PTR( state, pad_state[port_num] );
	}
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static void ps4_joypad_poll(void)
{
   unsigned player;
   ScePadData buttons;
   unsigned players_count = num_players;

   for (player = 0; player < SCE_USER_SERVICE_MAX_LOGIN_USERS; player++)
   {
      int ret;
      unsigned j, k;
      unsigned i  = player;

      if (ds_joypad_states[player].connected == false)
         continue;

      ret     = scePadReadState(ds_joypad_states[player].handle[0],&buttons);

      if (!~buttons.connected)
      {
         ds_joypad_states[player].connected = false;
         continue;
      }

      if (ret == 0)
      {
         int32_t state_tmp = buttons.buttons;
         pad_state[i] = 0;

         pad_state[i] |= (state_tmp & ORBISPAD_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_OPTIONS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_TOUCH_PAD) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_R1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_L1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         pad_state[i] |= (state_tmp & ORBISPAD_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(buttons.lx);
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(buttons.ly);
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(buttons.rx);
         analog_state[i][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(buttons.ry);
      }
      for (j = 0; j < 2; j++)
         for (k = 0; k < 2; k++)
            if (analog_state[i][j][k] == -0x8000)
               analog_state[i][j][k] = -0x7fff;
   }
}

static bool ps4_joypad_query_pad(unsigned pad)
{
   return pad < PS4_MAX_ORBISPADS && pad_state[pad];
}

static bool ps4_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
#if 0
   ScePadVibrationParam params;

   switch (effect)
   {
      case RETRO_RUMBLE_WEAK:
         params.smallMotor = LERP(strength, 0xffff, 0xff);
         break;
      case RETRO_RUMBLE_STRONG:
         params.largeMotor = LERP(strength, 0xffff, 0xff);
         break;
   }

   scePadSetVibration(ds_joypad_states[pad].handle[0], &params);

   return true;
#else
   return false;
#endif
}

static void ps4_joypad_destroy(void)
{
#if 0
   SceUserServiceUserId user_id;
   SceUserServiceLoginUserIdList user_id_list;
   if (sceUserServiceGetLoginUserIdList(&user_id_list) == 0)
   {
      unsigned i;
      for (i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++)
      {
         user_id = user_id_list.userId[i];
         if (user_id != SCE_USER_SERVICE_USER_ID_INVALID)
         {
            int handle = scePadGetHandle(user_id, SCE_PAD_PORT_TYPE_STANDARD, 0);
            if (handle > 0)
               scePadClose(handle);
            if ((handle = scePadGetHandle(user_id, SCE_PAD_PORT_TYPE_SPECIAL, 0))
                  > 0)
               scePadClose(handle);
            if ((handle = scePadGetHandle(user_id,
                        SCE_PAD_PORT_TYPE_REMOTE_CONTROL, 0)) > 0)
               scePadClose(handle);
         }
      }
   }
#endif
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
   NULL,
   ps4_joypad_name,
   "ps4",
};
