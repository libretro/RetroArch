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

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"

#include <userservice.h>
#include <pad.h>

#define PS4_MAX_ORBISPADS 16
#define SCE_USER_SERVICE_MAX_LOGIN_USERS 16
#define SCE_USER_SERVICE_USER_ID_INVALID 0xFFFFFFFF
#define SCE_ORBISPAD_ERROR_ALREADY_OPENED 0x80920004

#define	ORBISPAD_L3		      0x00000002
#define	ORBISPAD_R3		      0x00000004
#define	ORBISPAD_OPTIONS	   0x00000008
#define	ORBISPAD_UP		      0x00000010
#define	ORBISPAD_RIGHT		   0x00000020
#define	ORBISPAD_DOWN		   0x00000040
#define	ORBISPAD_LEFT		   0x00000080
#define	ORBISPAD_L2		      0x00000100
#define	ORBISPAD_R2		      0x00000200
#define	ORBISPAD_L1		      0x00000400
#define	ORBISPAD_R1		      0x00000800
#define	ORBISPAD_TRIANGLE	   0x00001000
#define	ORBISPAD_CIRCLE		0x00002000
#define	ORBISPAD_CROSS		   0x00004000
#define	ORBISPAD_SQUARE		0x00008000
#define	ORBISPAD_TOUCH_PAD	0x00100000
#define	ORBISPAD_INTERCEPTED	0x80000000

typedef struct SceUserServiceLoginUserIdList {
	int32_t userId[SCE_USER_SERVICE_MAX_LOGIN_USERS];
} SceUserServiceLoginUserIdList;

int sceUserServiceGetLoginUserIdList(SceUserServiceLoginUserIdList* userIdList);

/*
 * Global var's
 */
typedef struct
{
   SceUserServiceUserId userId;
   int handle;
   bool connected;
} ds_joypad_state;

static ds_joypad_state ds_joypad_states[PS4_MAX_ORBISPADS];
static uint64_t pad_state[PS4_MAX_ORBISPADS];
static int16_t analog_state[PS4_MAX_ORBISPADS][2][2];
static int16_t num_players = 0;

static const char *ps4_joypad_name(unsigned pad)
{
   return "PS4 Controller";
}

static bool ps4_joypad_init(void *data)
{
   int result;
   SceUserServiceLoginUserIdList userIdList;

   num_players = 0;

   scePadInit();

	result = sceUserServiceGetLoginUserIdList(&userIdList);

   RARCH_LOG("sceUserServiceGetLoginUserIdList %x ", result);

	if (result == 0)
	{
      unsigned i;
      for (i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++)
      {
         SceUserServiceUserId userId = userIdList.userId[i];

         RARCH_LOG("USER %d ID %x\n", i, userId);

         if (userId != SCE_USER_SERVICE_USER_ID_INVALID)
         {
            int index = 0;

            while (index < num_players)
            {
               ds_joypad_states[index].userId = userId;
               index++;
            }

            if (index == num_players)
            {
               ds_joypad_states[num_players].handle = scePadOpen(userId, 0, 0, NULL);
               RARCH_LOG("USER %x HANDLE %x\n", userId, ds_joypad_states[num_players].handle);
               if (ds_joypad_states[num_players].handle > 0)
               {
                  ds_joypad_states[num_players].connected = true;
                  ds_joypad_states[num_players].userId = userId;
                  RARCH_LOG("NEW PAD: num_players %x \n", num_players);

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

   return true;
}

static bool ps4_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= PS4_MAX_ORBISPADS)
      return false;
   return (pad_state[port_num] & (UINT64_C(1) << joykey));
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

static int16_t ps4_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   /* TODO/FIXME - implement */
   return 0;
}

static void ps4_joypad_poll(void)
{
   unsigned player;
   unsigned players_count = num_players;
   ScePadData buttons;

   for (player = 0; player < players_count; player++)
   {
      unsigned j, k;
      unsigned i  = player;
      unsigned p  = player;
      int ret     = scePadReadState(ds_joypad_states[player].handle,&buttons);

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
      }
   }

}

static bool ps4_joypad_query_pad(unsigned pad)
{
   return pad < PS4_MAX_ORBISPADS && pad_state[pad];
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
   ps4_joypad_get_buttons,
   ps4_joypad_axis,
   ps4_joypad_poll,
   ps4_joypad_rumble,
   ps4_joypad_name,
   "ps4",
};
