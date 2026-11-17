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

#include <stdint.h>

#include "../../config.def.h"

#include "../input_driver.h"
#include "../../tasks/tasks_internal.h"
#include "../include/input_pad_view.h"

typedef struct
{
   HANDLE id;
   /* the pad as its poll left it, lent to the readers (input_pad_view.h):
    * the RetroPad's buttons, the d-pad as hat 0, axes 0-3 the sticks */
   input_pad_view_t view;
   XINPUT_STATE xstate;
   bool connected;
} xinput_joypad_state;

/* TODO/FIXME - static globals */
static xinput_joypad_state g_xinput_states[DEFAULT_MAX_PADS];

static const char *xdk_joypad_name(unsigned pad)
{
   static const char* const XBOX_CONTROLLER_NAMES[4] =
   {
      "XInput Controller (User 1)",
      "XInput Controller (User 2)",
      "XInput Controller (User 3)",
      "XInput Controller (User 4)"
   };
   if (pad >= DEFAULT_MAX_PADS)
      return NULL;
   return XBOX_CONTROLLER_NAMES[pad];
}

static void xdk_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
         xdk_joypad_name(autoconf_pad),
         NULL, NULL,
         xdk_joypad.ident,
         autoconf_pad,
         0,
         0);
}

static void *xdk_joypad_init(void *data)
{
   XInitDevices(0, NULL);
   return (void*)-1;
}

/* Whether RetroPad button @id is down on @pad */
static bool xdk_joypad_button_state(const XINPUT_GAMEPAD *pad, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_JOYPAD_A:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_B] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      case RETRO_DEVICE_ID_JOYPAD_B:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_A] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      case RETRO_DEVICE_ID_JOYPAD_Y:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_X] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      case RETRO_DEVICE_ID_JOYPAD_X:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_Y] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      case RETRO_DEVICE_ID_JOYPAD_START:
         return (pad->wButtons & XINPUT_GAMEPAD_START) != 0;
      case RETRO_DEVICE_ID_JOYPAD_SELECT:
         return (pad->wButtons & XINPUT_GAMEPAD_BACK) != 0;
      case RETRO_DEVICE_ID_JOYPAD_L3:
         return (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
      case RETRO_DEVICE_ID_JOYPAD_R3:
         return (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
      case RETRO_DEVICE_ID_JOYPAD_L2:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_WHITE] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      case RETRO_DEVICE_ID_JOYPAD_R2:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_BLACK] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      case RETRO_DEVICE_ID_JOYPAD_L:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      case RETRO_DEVICE_ID_JOYPAD_R:
         return (pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] > XINPUT_GAMEPAD_MAX_CROSSTALK);
      default:
         break;
   }
   return false;
}

/* The view of what the poll read into @st->xstate */
static void xdk_joypad_make_view(xinput_joypad_state *st)
{
   unsigned i;
   XINPUT_GAMEPAD *pad = &st->xstate.Gamepad;
   input_pad_view_t *v = &st->view;
   uint32_t bits       = 0;
   int16_t sticks[4];

   for (i = 0; i < 16; i++)
      if (xdk_joypad_button_state(pad, i))
         bits |= (1u << i);
   v->buttons.data[0] = bits;
   v->hats            = (uint16_t)(
           ((pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)    ? 1 : 0)
         | ((pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  ? 2 : 0)
         | ((pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  ? 4 : 0)
         | ((pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 8 : 0));
   sticks[0]          = pad->sThumbLX;
   sticks[1]          = pad->sThumbLY;
   sticks[2]          = pad->sThumbRX;
   sticks[3]          = pad->sThumbRY;
   for (i = 0; i < 4; i++)
      v->axes[i]      = (sticks[i] == -32768) ? -32767 : sticks[i];
}

static int32_t xdk_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_button(&g_xinput_states[port].view, joykey);
}

static void xdk_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (port < DEFAULT_MAX_PADS)
      *state = g_xinput_states[port].view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t xdk_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_axis(&g_xinput_states[port].view, joyaxis);
}

static int16_t xdk_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   uint16_t port_idx = joypad_info->joy_idx;
   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_state(&g_xinput_states[port_idx].view,
         joypad_info, joykeys, joyaxes);
}

/* Read in the poll alone: the same until the next */
static const input_pad_view_t *xdk_joypad_view(unsigned port)
{
   return (port < DEFAULT_MAX_PADS) ? &g_xinput_states[port].view : NULL;
}

static void xdk_joypad_poll(void)
{
   unsigned port;
   DWORD dwInsertions, dwRemovals;

#ifdef __cplusplus
   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,
         reinterpret_cast<PDWORD>(&dwInsertions),
         reinterpret_cast<PDWORD>(&dwRemovals));
#else
   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD,
         (PDWORD)&dwInsertions,
         (PDWORD)&dwRemovals);
#endif

   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
      /* Handle removed devices. */
      if (dwRemovals & (1 << port))
      {
         /* if the controller was removed after
          * XGetDeviceChanges but before
          * XInputOpen, the device handle will be NULL. */
         if (g_xinput_states[port].id)
            XInputClose(g_xinput_states[port].id);

         g_xinput_states[port].id        = 0;
         g_xinput_states[port].connected = false;
         memset(&g_xinput_states[port].xstate, 0, sizeof(XINPUT_STATE));
         memset(&g_xinput_states[port].view, 0, sizeof(input_pad_view_t));

         input_autoconfigure_disconnect(port, xdk_joypad.ident);
      }

      /* Handle inserted devices. */
      if (dwInsertions & (1 << port))
      {
         XINPUT_POLLING_PARAMETERS m_pollingParameters;

         m_pollingParameters.fAutoPoll       = FALSE;
         m_pollingParameters.fInterruptOut   = TRUE;
         m_pollingParameters.bInputInterval  = 8;
         m_pollingParameters.bOutputInterval = 8;

         g_xinput_states[port].id            = XInputOpen(
               XDEVICE_TYPE_GAMEPAD, port,
               XDEVICE_NO_SLOT, &m_pollingParameters);

         xdk_joypad_autodetect_add(port);
      }

      if (!g_xinput_states[port].id)
         continue;

      /* Unreadable: the handle stays until the pad is removed. */
      if (     XInputPoll(g_xinput_states[port].id) != ERROR_SUCCESS
            || XInputGetState(g_xinput_states[port].id,
               &g_xinput_states[port].xstate) != ERROR_SUCCESS)
      {
         g_xinput_states[port].connected = false;
         memset(&g_xinput_states[port].xstate, 0, sizeof(XINPUT_STATE));
         memset(&g_xinput_states[port].view, 0, sizeof(input_pad_view_t));
         continue;
      }

      g_xinput_states[port].connected = true;
      xdk_joypad_make_view(&g_xinput_states[port]);
   }
}

static bool xdk_joypad_query_pad(unsigned pad)
{
   return pad < DEFAULT_MAX_PADS && g_xinput_states[pad].connected;
}

static void xdk_joypad_destroy(void)
{
   int i;
   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      if (g_xinput_states[i].id)
         XInputClose(g_xinput_states[i].id);
      memset(&g_xinput_states[i], 0, sizeof(xinput_joypad_state));
   }
}

input_device_driver_t xdk_joypad = {
   xdk_joypad_init,
   xdk_joypad_query_pad,
   xdk_joypad_destroy,
   xdk_joypad_button,
   xdk_joypad_state,
   xdk_joypad_get_buttons,
   xdk_joypad_axis,
   xdk_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   xdk_joypad_name,
   "xdk",
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   xdk_joypad_view
};
